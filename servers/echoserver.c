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
#include <unistd.h>
#include "util.h"

#define BUFSIZE 1024

int main(int argc, char **argv)
{
  int port;
  int listenfd;
  struct sockaddr_in remote_addr;
  int sin_size = sizeof(struct sockaddr_in);
  int yes = 1;
  int clientfd;

  if (2 > argc) {
    fprintf(stderr,"Usage: echoserver <port>\n");
    exit(1);
  }

  port = atoi(argv[1]);
  if (0 > (listenfd = start_listening_host("127.0.0.1",port)))
    exit(1);

  if (0 > (clientfd = accept(listenfd,(struct sockaddr*)&remote_addr,&sin_size))) {
    perror("accept");
    exit(1);
  }

  if (0 > setsockopt(clientfd,SOL_TCP,TCP_NODELAY,&yes,sizeof(int))) {
    perror("setsockopt TCP_NODELAY");
    exit(1);
  }

  printf("Got connection\n");

  while (1) {
    char buf[BUFSIZE+1];
    int r = read(clientfd,buf,BUFSIZE);
    int w;
    if (0 > r) {
      perror("read");
      exit(1);
    }
    if (0 == r) {
      printf("Client closed connection\n");
      break;
    }
    buf[r] = '\0';
    printf("%s",buf);
    w = write(clientfd,buf,r);
    if (0 > w) {
      perror("write");
      exit(1);
    }
    if (w < r) {
      fprintf(stderr,"Could not write all data (only %d bytes of %d)\n",w,r);
      exit(1);
    }
  }

  close(clientfd);
  close(listenfd);

  return 0;
}
