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

int open_connection(const char *hostname, int port)
{
  int sockfd;
  struct hostent *he;
  struct sockaddr_in addr;

  if (NULL == (he = gethostbyname(hostname))) {
    herror("gethostbyname");
    return -1;
  }

  if (0 > (sockfd = socket(AF_INET,SOCK_STREAM,0))) {
    perror("socket");
    return -1;
  }

  addr.sin_family = AF_INET;
  addr.sin_port = htons(port);
  addr.sin_addr = *((struct in_addr*)he->h_addr);
  memset(&addr.sin_zero,0,8);

  if (0 > (connect(sockfd,(struct sockaddr*)&addr,sizeof(struct sockaddr)))) {
    close(sockfd);
    perror("connect");
    return -1;
  }

  return sockfd;
}

int main(int argc, char **argv)
{
  char *hostname;
  int port;
  int sock;
  int maxtries = -1;
  int ntries = 0;

  setbuf(stdout,NULL);

  if (3 > argc) {
    fprintf(stderr,"Usage: client <host> <port> [maxtries]\n");
    return -1;
  }

  hostname = argv[1];
  port = atoi(argv[2]);
  if (4 <= argc) {
    maxtries = atoi(argv[3]);
  }

  printf("maxtries = %d\n",maxtries);

  /* Repeatedly try to connect to the server. Once the connection succeeds, exit
     successfully. */

  while (1) {
    printf("Attempting connection to %s:%d... ",hostname,port);
    /* Open a connection to the server */
    if (0 <= (sock = open_connection(hostname,port))) {
      printf("ok\n");
      close(sock);
      exit(0);
    }

    ntries++;
    if ((0 > maxtries) || (ntries < maxtries)) {
      sleep(1);
    }
    else {
      printf("aborting\n");
      exit(1);
    }
  }

  return 0;
}
