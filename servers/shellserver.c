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

int main(int argc, char **argv)
{
  int port;
  int listenfd;
  struct sockaddr_in remote_addr;
  int sin_size = sizeof(struct sockaddr_in);
  int yes = 1;
  int clientfd;

  if (2 > argc) {
    fprintf(stderr,"Usage: shellserver <port>\n");
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

  close(STDOUT_FILENO);
  close(STDIN_FILENO);
  close(STDERR_FILENO);

  dup2(clientfd,STDOUT_FILENO);
  dup2(clientfd,STDIN_FILENO);
  dup2(clientfd,STDERR_FILENO);

  printf("You are now in the remote shell\n");

  if (0 > chdir("/home/peter/shell")) {
    perror("/home/peter/shell");
    exit(1);
  }
  execl("/bin/bash","/bin/bash","-i","-l",(char*)NULL);

  return 0;
}
