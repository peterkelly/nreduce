#include "portset.h"

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
portset outports;

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

  if (customports) {
    int yes = 1;
    if (0 > setsockopt(sock,SOL_SOCKET,SO_REUSEADDR,&yes,sizeof(int))) {
      perror("setsockopt SO_REUSEADDR");
      exit(-1);
    }
    bind_client_port(sock,&outports,clientport);
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
  memset(&outports,0,sizeof(portset));

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
        portset_init(&outports,from,to);
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
      portset_releaseport(&outports,clientport);
  }

  return 0;
}
