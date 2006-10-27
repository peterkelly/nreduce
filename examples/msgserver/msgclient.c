/* Client program for message server - see msgserver.c */

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

#define CHUNKSIZE 200

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
  int i;

  setbuf(stdout,NULL);

  if (3 > argc) {
    fprintf(stderr,"Usage: msgclient <host> <port>\n");
    return -1;
  }

  hostname = argv[1];
  port = atoi(argv[2]);

  if (0 > (sock = open_connection(hostname,port)))
    return -1;

  for (i = 0; i < 60; i++) {
    int tag = i%5;
    char data[100];
    int size;

    sprintf(data,"Test message %d",i);
    size = strlen(data);

    write(sock,&size,sizeof(int));
    write(sock,&tag,sizeof(int));
    write(sock,data,size);

    printf("Sent message %d\n",i);
  }

  close(sock);

  return 0;
}
