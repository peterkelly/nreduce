#include <assert.h>
#include <fcntl.h>
#include <netdb.h>
#include <netinet/tcp.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/time.h>
#include <time.h>
#include <unistd.h>
#include "util.h"

#define BUFSIZE 1024

int main(int argc, char **argv)
{
  int port;
  int readsize;
  int delay;
  int listenfd;
  struct sockaddr_in remote_addr;
  int sin_size = sizeof(struct sockaddr_in);
  int yes = 1;
  int clientfd;
  char *buf;

  setbuf(stdout,NULL);

  if (4 > argc) {
    fprintf(stderr,"Usage: readserver <port> <readsize> <delay>\n");
    exit(1);
  }

  port = atoi(argv[1]);
  readsize = atoi(argv[2]);
  delay = atoi(argv[3]);
  if (0 > (listenfd = start_listening_host("0.0.0.0",port)))
    exit(1);

  buf = (char*)malloc(readsize+1);

  while (1) {
    if (0 > (clientfd = accept(listenfd,(struct sockaddr*)&remote_addr,&sin_size))) {
      perror("accept");
      exit(1);
    }

    if (0 > setsockopt(clientfd,SOL_TCP,TCP_NODELAY,&yes,sizeof(int))) {
      perror("setsockopt TCP_NODELAY");
      close(clientfd);
      break;
    }

    printf("Got connection\n");

    while (1) {
      struct timespec sleep;
      struct timeval now;
      int r = read(clientfd,buf,readsize);
      struct tm *st;
      if (0 > r) {
        perror("read");
        break;
      }
      if (0 == r) {
        printf("Client closed connection\n");
        break;
      }

      sleep.tv_sec = delay/1000;
      sleep.tv_nsec = (delay%1000)*1000000;
      nanosleep(&sleep,NULL);

      gettimeofday(&now,NULL);
      st = localtime(&now.tv_sec);
      printf("%02d:%02d:%02d.%03d: Read %d bytes\n",
             st->tm_hour,st->tm_min,st->tm_sec,(int)now.tv_usec/1000,r);
    }

    close(clientfd);
  }

  close(listenfd);
  free(buf);

  return 0;
}
