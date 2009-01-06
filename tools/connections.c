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

int customports = 0;

/* Keeps track of ports we have previously used, to aid in selection of client-side ports
   when establishing outgoing connections. */
typedef struct portset {
  int min;
  int max;
  int upto;

  int size;
  int count;
  int *ports;
  int start;
  int end;
} portset;

portset usedports;

void portset_init(portset *pb, int min, int max)
{
  pb->min = min;
  pb->max = max;
  pb->upto = min;

  pb->size = max+1-min;
  pb->ports = (int*)calloc(pb->size,sizeof(int));
  pb->count = 0;
  pb->start = 0;
  pb->end = 0;
}

void portset_destroy(portset *pb)
{
  free(pb->ports);
}

/* Choose a port to use for an outgoing connection. If we have not yet used up all ports in the
   range, the next port is picked. Otherwise, we choose the least recently used port. */
int portset_allocport(portset *pb)
{
  assert(0 <= pb->count);
  if (pb->upto <= pb->max) {
    /* Pick a port that has not yet been used, in preference to an existing one (which may still
       be be in TIME_WAIT) */
    return pb->upto++;
  }
  else if (0 < pb->count) {
    /* Pick a previously used port. It may still be in TIME_WAIT, but by picking the least
       recently used, we maximise the amount of time connections can stay in TIME_WAIT. */
    int r = pb->ports[pb->start];
    pb->start = (pb->start+1) % pb->size;
    pb->count--;
    return r;
  }
  else {
    return -1;
  }
}

/* Indicate that we have finished using a port, and it can be re-used again for a subsequent
   connection. */
void portset_releaseport(portset *pb, int port)
{
  pb->ports[pb->end] = port;
  pb->end = (pb->end+1) % pb->size;
  pb->count++;
  assert(pb->count <= pb->size);
}

void bind_client_port(int sock, int *clientport)
{
  while (1) {

    *clientport = portset_allocport(&usedports);
    if (0 > *clientport) {
      fprintf(stderr,"No more ports available!\n");
      exit(-1);
    }

    struct sockaddr_in local_addr;
    local_addr.sin_family = AF_INET;
    local_addr.sin_port = htons(*clientport);
    local_addr.sin_addr.s_addr = INADDR_ANY;
    memset(&local_addr.sin_zero,0,8);

    if (0 == bind(sock,(struct sockaddr*)&local_addr,sizeof(struct sockaddr))) {
      /* Port successfully bound */
      break;
    }

    if (EADDRINUSE == errno) {
      /* Port in use by another application. We don't put this back in the port set, since we
         don't want it to be used again. */
      printf("bind: port %d already in use, trying with another\n",*clientport);
      *clientport = -1;
    }
    else {
      perror("bind");
      exit(-1);
    }
  }
}

int open_connection(const char *hostname, int port, int *clientport)
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

  if (customports)
    bind_client_port(sock,clientport);

  addr.sin_family = AF_INET;
  addr.sin_port = htons(port);
  addr.sin_addr = *((struct in_addr*)he->h_addr);
  memset(&addr.sin_zero,0,8);

  if (0 > (connect(sock,(struct sockaddr*)&addr,sizeof(struct sockaddr)))) {
    perror("connect");
    close(sock);
    exit(-1);
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
  memset(&usedports,0,sizeof(portset));

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
        portset_init(&usedports,from,to);
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
    int clientport = -1;

    sock = open_connection(hostname,port,&clientport);
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

    if (0 <= clientport)
      portset_releaseport(&usedports,clientport);
  }

  return 0;
}
