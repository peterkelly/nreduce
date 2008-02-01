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
#include <curses.h>
#include <time.h>
#include <limits.h>
#include <netinet/tcp.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <sys/time.h>

#define BUF_MAX 65536
#define MAX_POLLFDS 1024
#define BACKLOG 250
#define DELAYMS 1000
#define MAXPERSERVER 3

#define IP_FORMAT "%u.%u.%u.%u"
#define IP_ARGS(ip) ((unsigned char*)&(ip))[0], \
                    ((unsigned char*)&(ip))[1], \
                    ((unsigned char*)&(ip))[2], \
                    ((unsigned char*)&(ip))[3]

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
  int *sent;
  int *received;
} relay;

typedef struct server {
  char *hostname;
  int port;
  int loadfd;
  int load;
  pid_t pid;
  int index;
  int connections;
  int sent;
  int received;
  struct sockaddr_in addr;
  struct server *prev;
  struct server *next;
} server;

typedef struct serverlist {
  server *first;
  server *last;
} serverlist;

typedef struct proxy {
  int connected;

  int clientfd;
  int serverfd;

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
int rows = 0;
int cols = 0;
char progdir[PATH_MAX];
char *progname = NULL;

void fatal(const char *format, ...)
{
  va_list ap;
  endwin();
  va_start(ap,format);
  vfprintf(stderr,format,ap);
  va_end(ap);
  fprintf(stderr,"\n");
  exit(-1);
}

void debug(const char *format, ...)
{
#if 0
  va_list ap;
  fprintf(stdout,"LOG: ");
  va_start(ap,format);
  vfprintf(stderr,format,ap);
  va_end(ap);
  fprintf(stdout,"\n");
#endif
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
      if (r->received)
        (*r->received) += n;
    }
    else { /* error; tell caller to close both connections */
      debug("%s ERROR read: %s",type,strerror(errno));
      return 1;
    }
  }
  if (FD_ISSET(r->destfd,writefds)) {
    assert(0 < r->size);
    n = write(r->destfd,r->data,r->size);
    if (0 <= n) { /* some or all outstanding data written */
      memmove(&r->data[0],&r->data[n],r->size-n);
      r->size -= n;
      if (r->sent)
        (*r->sent) += n;
    }
    else { /* error; tell caller to close both connections */
      debug("%s ERROR write: %s",type,strerror(errno));
      return 1;
    }
  }
  return 0; /* success */
}

void update_overall()
{
  int total = 0;
  int total_sent = 0;
  int total_received = 0;
  server *s;
  int linepos = 30;
  int linelen = cols-linepos;
  char *line = (char*)malloc(linelen+1);
  int nchars;
  char tmp[100];

  for (s = servers.first; s; s = s->next) {
    total += s->load;
    total_sent += s->sent;
    total_received += s->received;
  }
  nchars = (linelen*total)/(100*nservers);

  memset(line,' ',linelen);
  line[linelen] = '\0';
  mvaddstr(nservers+3,linepos,line);
  attron(A_REVERSE);
  line[nchars] = '\0';
  mvaddstr(nservers+3,linepos,line);
  attroff(A_REVERSE);

  snprintf(tmp,100,"Total sent:     %7d",total_sent/1024);
  mvaddstr(nservers+5,0,tmp);
  snprintf(tmp,100,"Total received: %7d",total_received/1024);
  mvaddstr(nservers+6,0,tmp);
}

void update(server *s)
{
  char tmp[100];
  int linepos = 30;
  int linelen = cols-linepos;
  char *line = (char*)malloc(linelen+1);
  int nchars = linelen*s->load/100;

  if ((s->load > 100) || (s->load < 0))
    fatal("%s: s->load = %d\n",s->hostname,s->load);
  assert(s->load <= 100);

  snprintf(tmp,100,"%-10s %4d %6d %6d",s->hostname,s->connections,s->sent/1024,s->received/1024);
  mvaddstr(s->index+2,0,tmp);

  memset(line,' ',linelen);
  line[linelen] = '\0';
  mvaddstr(s->index+2,linepos,line);
  attron(A_REVERSE);
  line[nchars] = '\0';
  mvaddstr(s->index+2,linepos,line);
  attroff(A_REVERSE);

  update_overall();

  move(0,0);
  refresh();
}

void add_proxy(int clientfd, int serverfd, server *s)
{
  proxy *p = (proxy*)calloc(1,sizeof(proxy));
  p->connected = 0;
  p->clientfd = clientfd;
  p->serverfd = serverfd;
  p->client_to_server.sourcefd = clientfd;
  p->client_to_server.destfd = serverfd;
  p->client_to_server.received = NULL;
  p->client_to_server.sent = &s->sent;
  p->server_to_client.sourcefd = serverfd;
  p->server_to_client.destfd = clientfd;
  p->server_to_client.received = &s->received;
  p->server_to_client.sent = NULL;
  p->s = s;
  llist_append(&proxies,p);
  nproxies++;
}

void remove_proxy(proxy *p)
{
  debug("remove_proxy");
  p->s->connections--;
  update(p->s);
  llist_remove(&proxies,p);
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

#if 0
  /* Choose based on load */
  int r;
  int total_load = 0;
  int cur = 0;
  server *s;
  for (s = servers.first; s; s = s->next) {
    total_load += (101-s->load);
  }
  r = random()%total_load;
  for (s = servers.first; s; s = s->next) {
    cur += (101-s->load);
    if (r <= cur)
      return s;
  }
  assert(0);

  return servers.first;
#else
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
#endif
}

void main_loop()
{
  while (1) {
    proxy *p;
    server *s;
    fd_set readfds;
    fd_set writefds;
    int highest = 0;
    proxy *next;

    FD_ZERO(&readfds);
    FD_ZERO(&writefds);

    for (s = servers.first; s; s = s->next) {
      if (0 <= s->loadfd)
        FD_SET(s->loadfd,&readfds);
      if (highest < s->loadfd)
        highest = s->loadfd;
    }

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

    for (s = servers.first; s; s = s->next) {
      if ((0 <= s->loadfd) && FD_ISSET(s->loadfd,&readfds)) {
        char c;
        int r = read(s->loadfd,&c,1);
        if (1 != r)
          fatal("read from %s %s",s->hostname,strerror(errno));
        s->load = c;
        debug("%s load = %d",s->hostname,s->load);
        update(s);
      }
    }

    for (p = proxies.first; p; p = next) {
      next = p->next;

      if (p->connected) {
        if (check_action(&p->client_to_server,&readfds,&writefds,"c2s") ||
            check_action(&p->server_to_client,&readfds,&writefds,"s2c") ||
            (p->client_to_server.eof && p->server_to_client.eof)) {
          remove_proxy(p);
        }
      }
      else if (FD_ISSET(p->serverfd,&writefds)) {
        int err;
        socklen_t optlen = sizeof(int);
        if (0 > getsockopt(p->serverfd,SOL_SOCKET,SO_ERROR,&err,&optlen))
          fatal("getsockopt SO_ERROR: %s",strerror(errno));
        else if (0 != err)
          fatal("connect: %s",strerror(err));
        else
          p->connected = 1;
        debug("Connected to server");
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
        fatal("connect: %s",strerror(errno));

      debug("Initiated connection; clientfd = %d, serverfd = %d",clientfd,serverfd);
      add_proxy(clientfd,serverfd,s);
      s->connections++;
      update(s);
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

int run_process(pid_t *pid, const char *hostname, const char *cmd)
{
  int fds[2];

  if (0 > pipe(fds))
    fatal("pipe: %s",strerror(errno));

  *pid = fork();
  if (0 > *pid) {
    fatal("fork: %s",strerror(errno));
  }
  else if (0 == *pid) {
    /* child */
    char *args[4];
    args[0] = "rsh";
    args[1] = (char*)hostname;
    args[2] = (char*)cmd;
    args[3] = NULL;

    dup2(fds[1],STDOUT_FILENO);
    dup2(fds[1],STDERR_FILENO);
    close(fds[0]);

    execvp(args[0],args);
    fatal("Can't execute %s: %s",args[0],strerror(errno));
  }
  else {
    /* parent */
    close(fds[1]);
    return fds[0];
  }
  return -1;
}

void add_server(const char *hostname, int port)
{
  server *s = (server*)calloc(1,sizeof(server));
  struct hostent *he;
  char *cmd;

  asprintf(&cmd,"cd %s && %s SLAVE",progdir,progname);

  if (NULL == (he = gethostbyname(hostname)))
    fatal("gethostbyname: %s",hstrerror(h_errno));

  s->hostname = strdup(hostname);
  s->port = port;
  s->load = -1;
  s->index = nservers;

  /* FIXME: don't hardcode path name */
  s->loadfd = run_process(&s->pid,hostname,cmd);
  s->addr.sin_family = AF_INET;
  s->addr.sin_port = htons(port);
  s->addr.sin_addr = *((struct in_addr*)he->h_addr);
  memset(&s->addr.sin_zero,0,8);

  llist_append(&servers,s);
  nservers++;
  free(cmd);
}

void init_servers(const char *filename)
{
  FILE *f;
  char name[257];
  if (NULL == (f = fopen(filename,"r")))
    fatal("filename: %s",strerror(errno));
  while (1 == fscanf(f,"%256s",name)) {
    char *colon = strchr(name,':');
    if (NULL == colon)
      fatal("%s: invalid server");
    *colon = '\0';
    add_server(name,atoi(colon+1));
  }
  fclose(f);
}

int getload()
{
  static unsigned long long int oldtotal = 0;
  static unsigned long long int oldidle = 0;
  static unsigned long long int load = 0;

  FILE *f;
  int count;
  unsigned long long int user = 0;
  unsigned long long int nice = 0;
  unsigned long long int system = 0;
  unsigned long long int idle = 0;
  unsigned long long int iowait = 0;
  unsigned long long int irq = 0;
  unsigned long long int softirq = 0;
  unsigned long long int total;
  unsigned long long int totaldiff;
  unsigned long long int idlediff;

  if (NULL == (f = fopen("/proc/stat","r")))
    return -1;

  count = fscanf(f,"cpu %llu %llu %llu %llu %llu %llu %llu",
                 &user,&nice,&system,&idle,&iowait,&irq,&softirq);
  fclose(f);

  if ((4 != count) && (7 != count))
    return -1;

  total = user+nice+system+idle+iowait+irq+softirq;

  if (0 > total) {
    oldtotal = total;
    oldidle = idle;
    return -1;
  }

  if ((total < oldtotal) || (idle < oldidle))
    return load; /* happens sometimes; rounding error in kernel? */

  totaldiff = total-oldtotal;
  idlediff = idle-oldidle;

  oldtotal = total;
  oldidle = idle;

  if (0 == totaldiff)
    return -1;

  load = 100-100*idlediff/totaldiff;
  return (int)load;
}

void slave()
{
  while (1) {
    int load = getload();
    char c = (char)load;
    struct timespec time;

    write(STDOUT_FILENO,&c,1);

    time.tv_sec = DELAYMS/1000;
    time.tv_nsec = (DELAYMS%1000)*1000000;
    nanosleep(&time,NULL);

    load++;
  }
}

void finish()
{
  endwin();
/*   fclose(logfile); */
  exit(0);
}

void setup_screen()
{
  signal(SIGINT,finish);

  initscr();
  keypad(stdscr,1);
  nonl();
  cbreak();
  noecho();
  curs_set(0);
  getmaxyx(stdscr,rows,cols);
}

int main(int argc, char **argv)
{
  struct timeval now;
  gettimeofday(&now,NULL);
  srandom(now.tv_usec);
  setbuf(stdout,NULL);

  signal(SIGPIPE,SIG_IGN);

  if ((2 == argc) && !strcmp("SLAVE",argv[1])) {
    slave();
    return 0;
  }

  if (3 > argc) {
    fprintf(stderr,"Usage: loadbal <port> <servers>\n");
    exit(-1);
  }

  progname = argv[0];
  if (NULL == getcwd(progdir,PATH_MAX))
    fatal("getcwd: %s",strerror(errno));

  init_servers(argv[2]);

  listenfd = start_listening(atoi(argv[1]));
  printf("started listening\n");
  setup_screen();
  mvaddstr(0,0,"Host       #con   Sent  Recvd Load");
  mvaddstr(nservers+3,0,"Average");
  main_loop();

  return 0;
}
