#define _GNU_SOURCE /* for asprintf */

#include <assert.h>
#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <netdb.h>
#include <signal.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <time.h>
#include <limits.h>
#include <netinet/tcp.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <sys/time.h>

#define BUF_MAX 65536
#define BACKLOG 250
#define MAXPERSERVER 3

FILE *logfile = NULL;
int nextproxyid = 0;

#define llist_append(_ll,_obj) {                          \
    assert(!(_obj)->prev && !(_obj)->next);               \
    if ((_ll)->last) {                                    \
      (_obj)->prev = (_ll)->last;                         \
      (_ll)->last->next = (_obj);                         \
      (_ll)->last = (_obj);                               \
    }                                                     \
    else {                                                \
      (_ll)->first = (_ll)->last = (_obj);                \
    }                                                     \
  }

#define llist_remove(_ll,_obj) {                          \
    assert((_obj)->prev || ((_ll)->first == (_obj)));     \
    assert((_obj)->next || ((_ll)->last == (_obj)));      \
    if ((_ll)->first == (_obj))                           \
      (_ll)->first = (_obj)->next;                        \
    if ((_ll)->last == (_obj))                            \
      (_ll)->last = (_obj)->prev;                         \
    if ((_obj)->next)                                     \
      (_obj)->next->prev = (_obj)->prev;                  \
    if ((_obj)->prev)                                     \
      (_obj)->prev->next = (_obj)->next;                  \
    (_obj)->next = NULL;                                  \
    (_obj)->prev = NULL;                                  \
  }

typedef struct buffer {
  char data[BUF_MAX];
  int size;
} buffer;

typedef struct relay {
  char data[BUF_MAX];
  int size;
  int sourcefd;
  int destfd;
  int eof;
  int type;
  struct proxy *p;
} relay;

#define RELAY_CLIENT_TO_SERVER 0
#define RELAY_SERVER_TO_CLIENT 1

typedef struct server {
  char *hostname;
  int port;
  int connections;
  struct sockaddr_in addr;
  struct server *prev;
  struct server *next;
} server;

typedef struct serverlist {
  server *first;
  server *last;
} serverlist;

typedef struct proxy {
  int id;
  int connected;

  int clientfd;
  int serverfd;
  int haderror;

  relay client_to_server;
  relay server_to_client;

  server *s;

  struct proxy *prev;
  struct proxy *next;
} proxy;

typedef struct proxylist {
  proxy *first;
  proxy *last;
} proxylist;

proxylist proxies = { first: NULL, last: NULL };
int nproxies = 0;
serverlist servers = { first: NULL, last: NULL };
int nservers = 0;
int listenfd = 0;

void fatal(const char *format, ...)
{
  va_list ap;
  va_start(ap,format);
  vfprintf(stderr,format,ap);
  va_end(ap);
  fprintf(stderr,"\n");
  exit(-1);
}

void debug(const char *format, ...)
{
  if (NULL == logfile)
    return;

  va_list ap;
  va_start(ap,format);
  vfprintf(logfile,format,ap);
  va_end(ap);
  fprintf(logfile,"\n");
}

void express_interest(relay *r, fd_set *readfds, fd_set *writefds)
{
  if ((0 == r->size) && !r->eof)
    FD_SET(r->sourcefd,readfds);
  if (0 < r->size)
    FD_SET(r->destfd,writefds);
}

int check_action(relay *r, fd_set *readfds, fd_set *writefds, const char *type)
{
  int n;
  if (FD_ISSET(r->sourcefd,readfds)) {
    assert((0 == r->size) && !r->eof);
    n = read(r->sourcefd,r->data,BUF_MAX);
    if (0 == n) { /* source finished sending */
      r->eof = 1;
      shutdown(r->sourcefd,SHUT_RD);
      shutdown(r->destfd,SHUT_WR);
    }
    else if (0 < n) { /* we have some more data */
      r->size = n;
    }
    else { /* error; tell caller to close both connections */
      debug("ERROR read (proxy %d, type %s): %s",r->p->id,type,strerror(errno));
      return 1;
    }
  }
  if (FD_ISSET(r->destfd,writefds)) {
    assert(0 < r->size);
    n = write(r->destfd,r->data,r->size);
    if (0 <= n) { /* some or all outstanding data written */
      memmove(&r->data[0],&r->data[n],r->size-n);
      r->size -= n;
    }
    else { /* error; tell caller to close both connections */
      debug("ERROR write (proxy %d, type %s): %s",r->p->id,type,strerror(errno));
      return 1;
    }
  }
  return 0; /* success */
}

proxy *add_proxy(int clientfd, int serverfd, server *s)
{
  proxy *p = (proxy*)calloc(1,sizeof(proxy));
  p->id = nextproxyid++;
  p->connected = 0;
  p->clientfd = clientfd;
  p->serverfd = serverfd;
  p->client_to_server.sourcefd = clientfd;
  p->client_to_server.destfd = serverfd;
  p->client_to_server.type = RELAY_CLIENT_TO_SERVER;
  p->client_to_server.p = p;
  p->server_to_client.sourcefd = serverfd;
  p->server_to_client.destfd = clientfd;
  p->server_to_client.type = RELAY_SERVER_TO_CLIENT;
  p->server_to_client.p = p;
  p->s = s;
  llist_append(&proxies,p);
  nproxies++;
  return p;
}

void remove_proxy(proxy *p)
{
  debug("remove_proxy %d",p->id);
  p->s->connections--;
  llist_remove(&proxies,p);

  if (p->haderror) {
    /* abortive close */
    struct linger li;
    li.l_onoff = 1;
    li.l_linger = 0;
    if (0 > setsockopt(p->clientfd,SOL_SOCKET,SO_LINGER,&li,sizeof(li)))
      fatal("setsockopt SOL_SOCKET: %s",strerror(errno));
    if (0 > setsockopt(p->serverfd,SOL_SOCKET,SO_LINGER,&li,sizeof(li)))
      fatal("setsockopt SOL_SOCKET: %s",strerror(errno));
  }

  close(p->clientfd);
  close(p->serverfd);
  free(p);
  nproxies--;
}

void set_non_blocking(int fd)
{
  int flags;
  if (0 > (flags = fcntl(fd,F_GETFL)))
    fatal("fcntl(F_GETFL): %s",strerror(errno));
  flags |= O_NONBLOCK;
  if (0 > fcntl(fd,F_SETFL,flags))
    fatal("fcntl(F_SETFL): %s",strerror(errno));
}

server *select_server()
{
  assert(NULL != servers.first);
  /* Choose randomly from the servers which have the min. no of connections */
  int r;
  server *s;
  int nwithmin = 0;
  int cur = 0;
  int min = servers.first->connections;
  int max = servers.first->connections;
  for (s = servers.first->next; s; s = s->next) {
    if (min > s->connections)
      min = s->connections;
    if (max < s->connections)
      max = s->connections;
  }
  for (s = servers.first; s; s = s->next) {
    if (s->connections == min)
      nwithmin++;
  }
  r = random()%nwithmin;
  for (s = servers.first; s; s = s->next) {
    if (s->connections == min) {
      if (cur == r)
        return s;
      cur++;
    }
  }
  assert(0);

  return servers.first;
}

void main_loop()
{
  while (1) {
    proxy *p;
    fd_set readfds;
    fd_set writefds;
    int highest = 0;
    proxy *next;

    FD_ZERO(&readfds);
    FD_ZERO(&writefds);

    for (p = proxies.first; p; p = p->next) {
      if (p->connected) {
        express_interest(&p->client_to_server,&readfds,&writefds);
        express_interest(&p->server_to_client,&readfds,&writefds);
      }
      else {
        FD_SET(p->serverfd,&writefds);
      }

      if (highest < p->clientfd)
        highest = p->clientfd;
      if (highest < p->serverfd)
        highest = p->serverfd;
    }

    if (nservers*MAXPERSERVER > nproxies) {
      FD_SET(listenfd,&readfds);
      if (highest < listenfd)
        highest = listenfd;
    }

    if (0 > select(highest+1,&readfds,&writefds,NULL,NULL))
      fatal("select: %s",strerror(errno));

    for (p = proxies.first; p; p = next) {
      next = p->next;

      if (p->connected) {
        if (check_action(&p->client_to_server,&readfds,&writefds,"c2s") ||
            check_action(&p->server_to_client,&readfds,&writefds,"s2c")) {
          p->haderror = 1;
          remove_proxy(p);
        }
        else if (p->client_to_server.eof && p->server_to_client.eof) {
          remove_proxy(p);
        }
      }
      else if (FD_ISSET(p->serverfd,&writefds)) {
        int err;
        socklen_t optlen = sizeof(int);
        if (0 > getsockopt(p->serverfd,SOL_SOCKET,SO_ERROR,&err,&optlen))
          fatal("getsockopt SO_ERROR: %s",strerror(errno));
        else if (0 != err)
          fatal("connect %s: %s",p->s->hostname,strerror(err));
        else
          p->connected = 1;
        debug("Connected to server (proxy %d)",p->id);
      }
    }

    if (FD_ISSET(listenfd,&readfds)) {
      server *s = select_server();
      struct sockaddr_in addr = s->addr;
      int clientfd;
      int serverfd;
      int r;
      debug("Client connected");

      if (0 > (clientfd = accept(listenfd,NULL,0)))
        fatal("accept: %s",strerror(errno));

      if (0 > (serverfd = socket(AF_INET,SOCK_STREAM,0)))
        fatal("socket: %s",strerror(errno));

      set_non_blocking(clientfd);
      set_non_blocking(serverfd);

      /* Initiate asynchronous connection */
      r = connect(serverfd,(struct sockaddr*)&addr,sizeof(struct sockaddr));
      assert(0 > r);
      if (EINPROGRESS != errno)
        fatal("connect %s: %s",p->s->hostname,strerror(errno));

      proxy *p = add_proxy(clientfd,serverfd,s);
      debug("Initiated connection to %s; clientfd = %d, serverfd = %d (proxy %d)",
            s->hostname,clientfd,serverfd,p->id);
      s->connections++;
    }
  }
}

int start_listening(int port)
{
  int yes = 1;
  struct sockaddr_in local_addr;
  int sock;

  if (0 > (sock = socket(AF_INET,SOCK_STREAM,0)))
    fatal("socket: %s",strerror(errno));

  if (0 > setsockopt(sock,SOL_SOCKET,SO_REUSEADDR,&yes,sizeof(int)))
    fatal("setsockopt SO_REUSEADDR: %s",strerror(errno));

  local_addr.sin_family = AF_INET;
  local_addr.sin_port = htons(port);
  local_addr.sin_addr.s_addr = INADDR_ANY;
  memset(&local_addr.sin_zero,0,8);

  if (0 > bind(sock,(struct sockaddr*)&local_addr,sizeof(struct sockaddr)))
    fatal("bind: %s",strerror(errno));

  if (0 > listen(sock,BACKLOG))
    fatal("listen: %s",strerror(errno));

  return sock;
}

void add_server(const char *hostname, int port)
{
  server *s = (server*)calloc(1,sizeof(server));
  struct hostent *he;

  if (NULL == (he = gethostbyname(hostname)))
    fatal("gethostbyname: %s",hstrerror(h_errno));

  s->hostname = strdup(hostname);
  s->port = port;

  s->addr.sin_family = AF_INET;
  s->addr.sin_port = htons(port);
  s->addr.sin_addr = *((struct in_addr*)he->h_addr);
  memset(&s->addr.sin_zero,0,8);

  llist_append(&servers,s);
  nservers++;
}

void init_servers(const char *filename)
{
  FILE *f;
  char name[257];
  if (NULL == (f = fopen(filename,"r")))
    fatal("%s: %s",filename,strerror(errno));
  while (1 == fscanf(f,"%256s",name)) {
    char *colon = strchr(name,':');
    if (NULL == colon)
      fatal("%s: invalid server");
    *colon = '\0';
    add_server(name,atoi(colon+1));
  }
  fclose(f);
}

void usage()
{
  fprintf(stderr,"Usage: loadbal [OPTION] <port> <servers>\n");
  fprintf(stderr,"  -l FILENAME      Log status info to FILENAME\n");
  exit(-1);
}

int main(int argc, char **argv)
{
  struct timeval now;
  gettimeofday(&now,NULL);
  srandom(now.tv_usec);
  setbuf(stdout,NULL);

  signal(SIGPIPE,SIG_IGN);

  char *portstr = NULL;
  char *servers = NULL;
  char *logfilename = NULL;

  int argpos;
  for (argpos = 1; argpos < argc; argpos++) {
    if (!strcmp(argv[argpos],"-l")) {
      if (argpos+1 >= argc)
        usage();
      logfilename = argv[++argpos];
    }
    else {
      if (NULL == portstr)
        portstr = argv[argpos];
      else if (NULL == servers)
        servers = argv[argpos];
    }
  }

  if ((NULL == portstr) || (NULL == servers))
    usage();

  if (NULL != logfilename) {
    if (!strcmp(logfilename,"-"))
      logfile = stdout;
    else
      logfile = fopen(logfilename,"w");
    setbuf(logfile,NULL);
    if (NULL == logfile) {
      perror(logfilename);
      exit(1);
    }
  }

  init_servers(servers);

  listenfd = start_listening(atoi(portstr));
  main_loop();

  if (NULL != logfile)
    fclose(logfile);

  return 0;
}
