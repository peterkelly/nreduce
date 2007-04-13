#include <assert.h>
#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <math.h>
#include <netdb.h>
#include <netinet/tcp.h>
#include <pthread.h>
#include <signal.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>

int main(int argc, char **argv)
{
  char *hostname;
  int port;
  int bufsize;
  int delay;
  struct hostent *he;
  struct sockaddr_in addr;
  char *initial = "GET /home/peter/trace HTTP/1.0\r\n\r\n";
  int sock;
  int r;
  char *buf;

  setbuf(stdout,NULL);

  if (5 > argc) {
    fprintf(stderr,"Usage: slowget <hostname> <port> <bufsize> <delay>\n");
    exit(-1);
  }

  hostname = argv[1];
  port = atoi(argv[2]);
  bufsize = atoi(argv[3]);
  delay = atoi(argv[4]);

  if (NULL == (he = gethostbyname(hostname))) {
    perror("gethostbyname");
    exit(-1);
  }

  addr.sin_family = AF_INET;
  addr.sin_port = htons(port);
  addr.sin_addr.s_addr = (*((struct in_addr*)he->h_addr)).s_addr;
  memset(&addr.sin_zero,0,8);

  if (0 > (sock = socket(AF_INET,SOCK_STREAM,0))) {
    perror("socket");
    exit(-1);
  }

  if (0 > connect(sock,(struct sockaddr*)&addr,sizeof(struct sockaddr))) {
    perror("connect");
    exit(-1);
  }

  r = write(sock,initial,strlen(initial));
  if (0 > r) {
    perror("write");
    exit(1);
  }
  else if (strlen(initial) != r) {
    printf("Tried to write %d bytes, but write() returned %d\n",strlen(initial),r);
    exit(1);
  }

  buf = (char*)malloc(bufsize+1);
  while (0 < (r = read(sock,buf,bufsize))) {
    struct timespec time;
    buf[r] = '\0';
    printf("%s",buf);
    time.tv_sec = delay/1000;
    time.tv_nsec = (delay%1000)*1000000;
    nanosleep(&time,NULL);
  }
  if (0 > r) {
    perror("read");
    exit(1);
  }
  free(buf);

  return 0;
}
