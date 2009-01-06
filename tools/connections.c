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

int open_connection(const char *hostname, int port)
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

  addr.sin_family = AF_INET;
  addr.sin_port = htons(port);
  addr.sin_addr = *((struct in_addr*)he->h_addr);
  memset(&addr.sin_zero,0,8);

  if (0 > (connect(sock,(struct sockaddr*)&addr,sizeof(struct sockaddr)))) {
    close(sock);
    perror("connect");
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

  /* Send request */
  char *input = "0 0";
  write(sock,input,strlen(input));
  shutdown(sock,SHUT_WR);

  /* Get response */
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

int main(int argc, char **argv)
{
  char *hostname;
  int port;
  int sock;
  int nconnections = 0;

  setbuf(stdout,NULL);

  struct timeval start;
  gettimeofday(&start,NULL);

  if (3 > argc) {
    fprintf(stderr,"Usage: connections <host> <port>\n");
    return -1;
  }

  hostname = argv[1];
  port = atoi(argv[2]);

  while (1) {

    sock = open_connection(hostname,port);
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
  }

  return 0;
}
