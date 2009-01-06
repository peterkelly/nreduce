#include <fcntl.h>
#include <errno.h>
#include <netdb.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/time.h>
#include <netinet/tcp.h>
#include <assert.h>

#define llist_prepend(_ll,_obj) {                         \
    assert(!(_obj)->prev && !(_obj)->next);               \
    if ((_ll)->first) {                                   \
      (_obj)->next = (_ll)->first;                        \
      (_ll)->first->prev = (_obj);                        \
      (_ll)->first = (_obj);                              \
    }                                                     \
    else {                                                \
      (_ll)->first = (_ll)->last = (_obj);                \
    }                                                     \
  }

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

typedef struct portinfo {
  int port;
  int used;
  struct portinfo *prev;
  struct portinfo *next;
} portinfo;

typedef struct portlist {
  portinfo *first;
  portinfo *last;
} portlist;

typedef struct portset {
  int min;
  int max;
  int upto;
  portlist used;
  portlist available;
} portset;

portset main_ps;

struct timeval timeval_diff(struct timeval from, struct timeval to)
{
  struct timeval diff;
  diff.tv_sec = to.tv_sec - from.tv_sec;
  diff.tv_usec = to.tv_usec - from.tv_usec;
  if (0 > diff.tv_usec) {
    diff.tv_sec -= 1;
    diff.tv_usec += 1000000;
  }
  return diff;
}

int timeval_diffms(struct timeval from, struct timeval to)
{
  struct timeval diff = timeval_diff(from,to);
  return diff.tv_sec*1000 + diff.tv_usec/1000;
}

void portset_init(portset *ps, int min, int max)
{
  memset(ps,0,sizeof(portset));
  ps->min = min;
  ps->max = max;
  ps->upto = min;
}

portinfo *portset_getport(portset *ps)
{
  if (ps->upto <= ps->max) {
    /* There is still at least one more port available in the specified port range.
       Use this in preference to a previously-used one, so we can maximise the duration
       for which previously-used ports can remain associated with connections in the
       TIME_WAIT state. */
    portinfo *pi = (portinfo*)calloc(1,sizeof(portinfo));
    pi->port = ps->upto++;
    pi->used = 1;
    llist_append(&ps->used,pi);
    return pi;
  }
  else if (ps->available.last) {
    /* We have previously used this port, but it is now released. Take the least
       recently used port and return it */
    portinfo *pi = ps->available.last;
    llist_remove(&ps->available,pi);
    llist_append(&ps->used,pi);
    assert(!pi->used);
    pi->used = 1;
    return pi;
  }
  else {
    return NULL;
  }
}

void portset_releaseport(portset *ps, portinfo *pi)
{
  assert(pi->used);
  pi->used = 0;
  llist_remove(&ps->used,pi);
  llist_prepend(&ps->available,pi);
}

/* Port has been found to be in use by another application. Remove it from the used
   list, but don't add it back to the avaiable list, because we don't want to try and
   use it again. */
void portset_portbusy(portset *ps, portinfo *pi)
{
  assert(pi->used);
  pi->used = 0;
  llist_remove(&ps->used,pi);
  free(pi);
}

int customports = 0;

int open_connection(const char *hostname, int port, portinfo **pi)
{
  int sock;
  struct hostent *he;
  struct sockaddr_in addr;

  if (NULL == (he = gethostbyname(hostname))) {
    herror("gethostbyname");
    exit(-1);
  }

  if (0 > (sock = socket(AF_INET,SOCK_STREAM,0))) {
    perror("socket");
    exit(-1);
  }

  int yes = 1;
  if (0 > setsockopt(sock,SOL_SOCKET,SO_REUSEADDR,&yes,sizeof(int))) {
    perror("setsockopt SO_REUSEADDR");
    exit(-1);
  }

  while (1) {

    if (customports) {
      *pi = portset_getport(&main_ps);
      if (NULL == *pi) {
        fprintf(stderr,"No more ports available!\n");
        exit(-1);
      }

      struct sockaddr_in local_addr;
      local_addr.sin_family = AF_INET;
      local_addr.sin_port = htons((*pi)->port);
      local_addr.sin_addr.s_addr = INADDR_ANY;
      memset(&local_addr.sin_zero,0,8);

      if (0 > bind(sock,(struct sockaddr*)&local_addr,sizeof(struct sockaddr))) {

        if (EADDRINUSE == errno) {
          printf("bind: port %d already in use, trying with another\n",(*pi)->port);
          portset_releaseport(&main_ps,*pi);
          *pi = NULL;
          continue;
        }
        else {
          perror("bind");
          exit(-1);
        }
      }
    }

    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    addr.sin_addr = *((struct in_addr*)he->h_addr);
    memset(&addr.sin_zero,0,8);

    if (0 > (connect(sock,(struct sockaddr*)&addr,sizeof(struct sockaddr)))) {
      perror("connect");
      close(sock);
      exit(-1);
    }
    else {
      break;
    }
  }

  return sock;
}

void read_write(int sock)
{
  int yes = 1;

  if (0 > setsockopt(sock,IPPROTO_TCP,TCP_NODELAY,&yes,sizeof(int))) {
    perror("setsockopt TCP_NODELAY");
    exit(-1);
  }

  char *input = "0 0";
  write(sock,input,strlen(input));
  shutdown(sock,SHUT_WR);

  char buf[1024];
  int r;
  while (0 < (r = read(sock,buf,1024))) {
  }
  if (0 > r) {
    perror("read");
    exit(-1);
  }
  shutdown(sock,SHUT_RD);
}

void usage()
{
  fprintf(stderr,"Usage: connections [-p from-to] <host> <port>\n");
  exit(-1);
}

int main(int argc, char **argv)
{
  char *hostname = NULL;
  char *portstr = NULL;
  int port;
  int sock;
  int nconnections = 0;

  setbuf(stdout,NULL);
  memset(&main_ps,0,sizeof(portset));

  struct timeval start;
  gettimeofday(&start,NULL);

  int argpos;
  for (argpos = 1; argpos < argc; argpos++) {
    if (!strcmp(argv[argpos],"-p")) {
      if (argpos+2 >= argc)
        usage();
      argpos++;
      int from;
      int to;
      if (2 == sscanf(argv[argpos],"%d-%d",&from,&to)) {
        customports = 1;
        portset_init(&main_ps,from,to);
      }
      else {
        fprintf(stderr,"Invalid port range: %s\n",argv[argpos]);
        exit(-1);
      }
    }
    else if (NULL == hostname) {
      hostname = argv[argpos];
    }
    else if (NULL == portstr) {
      portstr = argv[argpos];
    }
  }

  if ((NULL == hostname) || (NULL == portstr))
    usage();

  port = atoi(portstr);

  while (1) {
    portinfo *pi = NULL;

    sock = open_connection(hostname,port,&pi);
    nconnections++;

    read_write(sock);

    struct sockaddr_in new_addr;
    socklen_t new_size = sizeof(struct sockaddr);
    if (0 > getsockname(sock,(struct sockaddr*)&new_addr,&new_size)) {
      perror("getsockname");
      exit(-1);
    }

    struct timeval now;
    gettimeofday(&now,NULL);
    int ms = timeval_diffms(start,now);
    double rate = 1000.0*((double)nconnections)/ms;

    printf("# connections = %d, rate = %.3f c/s, port = %d\n",
           nconnections,rate,ntohs(new_addr.sin_port));

    close(sock);

    if (NULL != pi) {
      portset_releaseport(&main_ps,pi);
    }
  }

  return 0;
}
