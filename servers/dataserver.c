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

#define MAX_FILESERVERS 100
#define BUFSIZE 16384

typedef struct fileserver {
  int port;
  char *filename;
  int listenfd;
} fileserver;

typedef struct connection {
  int fd;
  int server;
  int filefd;
  char buf[BUFSIZE];
  int bufused;
  int connno;
  struct connection *prev;
  struct connection *next;
} connection;

typedef struct connectionlist {
  connection *first;
  connection *last;
} connectionlist;

fileserver servers[MAX_FILESERVERS];
int nservers = 0;
connectionlist connections;
int conncount = 0;

void read_config(const char *filename)
{
  FILE *f;

  if (NULL == (f = fopen(filename,"r"))) {
    perror(filename);
    exit(1);
  }

  while ((MAX_FILESERVERS > nservers+1) &&
         (2 == fscanf(f,"%d %as",&servers[nservers].port,&servers[nservers].filename)))
    nservers++;

  if (0 == nservers) {
    fprintf(stderr,"No servers!\n");
    exit(1);
  }

  fclose(f);
}

void handle_new_connection(int i)
{
  struct sockaddr_in remote_addr;
  int sin_size = sizeof(struct sockaddr_in);
  int yes = 1;
  connection *c = (connection*)calloc(1,sizeof(connection));

  if (0 > (c->fd = accept(servers[i].listenfd,(struct sockaddr*)&remote_addr,&sin_size))) {
    perror("accept");
    exit(1);
  }

  if (0 > fdsetblocking(c->fd,0))
    exit(1);

  if (0 > setsockopt(c->fd,SOL_TCP,TCP_NODELAY,&yes,sizeof(int))) {
    perror("setsockopt TCP_NODELAY");
    exit(1);
  }

  if (0 > (c->filefd = open(servers[i].filename,O_RDONLY))) {
    perror(servers[i].filename);
    exit(1);
  }

  c->connno = conncount++;
  llist_append(&connections,c);

  printf("Got connection on port %d (%s)\n",servers[i].port,servers[i].filename);
}

void close_connection(connection *c)
{
  close(c->filefd);
  close(c->fd);
  llist_remove(&connections,c);
  free(c);
}

void handle_write(connection *c)
{
  int w;

  assert(0 <= c->bufused);
  if (0 == c->bufused) {
    c->bufused = read(c->filefd,c->buf,BUFSIZE);
    if (0 > c->bufused) {
      perror("fread");
      exit(1);
    }

    if (0 == c->bufused) {
      printf("Finished sending %s to fd %d\n",servers[c->server].filename,c->fd);
      close_connection(c);
      return;
    }
  }

  if (0 > (w = write(c->fd,c->buf,c->bufused))) {
    perror("write");
    close_connection(c);
    return;
  }
  printf("wrote %d bytes to connection %d\n",w,c->connno);

  if (w < c->bufused)
    memmove(&c->buf[0],&c->buf[w],c->bufused-w);
  c->bufused -= w;
}

void main_loop()
{
  while (1) {
    fd_set readfds;
    fd_set writefds;
    int i;
    int highest = -1;
    connection *c;

    FD_ZERO(&readfds);
    FD_ZERO(&writefds);

    for (i = 0; i < nservers; i++) {
      FD_SET(servers[i].listenfd,&readfds);
      if (highest < servers[i].listenfd)
        highest = servers[i].listenfd;
    }

    for (c = connections.first; c; c = c->next) {
      FD_SET(c->fd,&writefds);
      if (highest < c->fd)
        highest = c->fd;
    }

    if (0 > select(highest+1,&readfds,&writefds,NULL,NULL)) {
      perror("select");
      exit(1);
    }

    for (i = 0; i < nservers; i++) {
      if (FD_ISSET(servers[i].listenfd,&readfds))
        handle_new_connection(i);
    }

    c = connections.first;
    while (c) {
      connection *next = c->next;
      if (FD_ISSET(c->fd,&writefds)) {
        handle_write(c);
      }
      c = next;
    }
  }
}

int main(int argc, char **argv)
{
  int i;
  setbuf(stdout,NULL);

  memset(&connections,0,sizeof(connectionlist));

  if (2 > argc) {
    fprintf(stderr,"Usage: dataserver <configfile>\n");
    return 1;
  }

  read_config(argv[1]);

  for (i = 0; i < nservers; i++) {
    struct in_addr addr;
    addr.s_addr = INADDR_ANY;
    servers[i].listenfd = start_listening(addr,servers[i].port);
    printf("port=%d, filename=%s, listenfd=%d\n",
           servers[i].port,servers[i].filename,servers[i].listenfd);
  }

  main_loop();

  return 0;
}
