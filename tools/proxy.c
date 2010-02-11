#include <assert.h>
#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <netdb.h>
#include <signal.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <time.h>
#include <limits.h>
#include <pthread.h>
#include <netinet/tcp.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <sys/time.h>

#define BACKLOG 10

/* Open a connection to the server. This does a host lookup for the specified
   name if it's not just an IP address */
int open_connection(const char *hostname, int port)
{
  int sockfd;
  struct hostent *he;
  struct sockaddr_in addr;

  /* Look up the host addrses associated with this name. If hostnames is a
     string in the form x.x.x.x then it will just be treated as an IP address
     and no name resolution need to be performed. If it's a DNS name e.g.
     server.mycompany.com then this will be resolved to an IP address which
     can be retrieved from the reurned hostent structure. */
  if (NULL == (he = gethostbyname(hostname))) {
    herror("gethostbyname");
    return -1;
  }

  /* Create a socket to use for our connection with the server. */
  if (0 > (sockfd = socket(AF_INET,SOCK_STREAM,0))) {
    perror("socket");
    return -1;
  }

  /* Initialise the address structure for the server that we want to connect
     to. The port number has been supplied by the caller but needs to be
     converted into network byte order, which is done using htons(). The
     IP address is obtained from the hostent structure returned from the lookup
     performed above. */
  addr.sin_family = AF_INET;
  addr.sin_port = htons(port);
  addr.sin_addr = *((struct in_addr*)he->h_addr);
  memset(&addr.sin_zero,0,8);

  /* Attempt to open a connection to the server. If this fails for any reason,
     such as if the server is not running or the relevant IP address is
     inaccessible, we'll just return an error. Sockets are always supposed to
     have a value of >= 0, so we can use a negative value to indicate an error
     (as with the above operations). */
  if (0 > (connect(sockfd,(struct sockaddr*)&addr,sizeof(struct sockaddr)))) {
    close(sockfd);
    perror("connect");
    return -1;
  }

  return sockfd;
}

/* Create a listening socket. See server.c for more details */
int start_listening(int port)
{
  int yes = 1;
  struct sockaddr_in local_addr;
  int sock;

  if (0 > (sock = socket(AF_INET,SOCK_STREAM,0))) {
    perror("socket");
    return -1;
  }

  if (0 > setsockopt(sock,SOL_SOCKET,SO_REUSEADDR,&yes,sizeof(int))) {
    perror("setsockopt");
    close(sock);
    return -1;
  }

  local_addr.sin_family = AF_INET;
  local_addr.sin_port = htons(port);
  local_addr.sin_addr.s_addr = INADDR_ANY;
  memset(&local_addr.sin_zero,0,8);

  if (0 > bind(sock,(struct sockaddr*)&local_addr,sizeof(struct sockaddr))) {
    perror("bind");
    close(sock);
    return -1;
  }

  if (0 > listen(sock,BACKLOG)) {
    perror("listen");
    close(sock);
    return -1;
  }

  return sock;
}

int listen_port;
char *server;
int server_port;

typedef struct handler_arg {
  int clientfd;
  int connno;
} handler_arg;

void *handler_thread(void *arg)
{
  handler_arg *ha = (handler_arg*)arg;
  int clientfd = ha->clientfd;
  int connno = ha->connno;
  free(arg);

  /* connect to server */
  int serverfd = open_connection(server,server_port);
  if (0 > serverfd)
    exit(1);

  printf("%d: connected to server\n",connno);

  /* read all data */
  char *request = (char*)malloc(1024);
  int rsize = 0;
  int r;
  while (0 < (r = read(clientfd,&request[rsize],1024))) {
    rsize += r;
    request = (char*)realloc(request,rsize+1024);
  }
  if (0 > r) {
    printf("%d: client read error: %s\n",connno,strerror(errno));
    exit(1);
  }

  printf("%d: read %d total bytes from client\n",connno,rsize);

  /* send it to the other side */
  int sent = 0;
  while (sent < rsize) {
    int remaining = rsize - sent;
    int tosend = remaining > 1024 ? 1024 : remaining;
    int w = write(serverfd,&request[sent],tosend);
    if (0 > w) {
      printf("%d: server write error: %s\n",connno,strerror(errno));
      exit(1);
    }
    sent += w;
  }
  printf("%d: sent %d total bytes to server\n",connno,sent);

  /* read response from other side and send back to client */
  char buf[1024];
  int respsize = 0;
  while (0 < (r = read(serverfd,buf,1024))) {
    respsize += r;
    int w = write(clientfd,buf,r);
    if (0 > w) {
      printf("%d: client write error: %s\n",connno,strerror(errno));
      exit(1);
    }
    if (w != r) {
      printf("%d: wrote only %d of %d bytes to client\n",connno,w,r);
      exit(1);
    }
  }
  if (0 > r) {
    printf("%d: server read error: %s\n",connno,strerror(errno));
    exit(1);
  }

  close(serverfd);
  close(clientfd);
  printf("%d: completed; response contained %d bytes\n",connno,respsize);

  return NULL;
}

int main(int argc, char **argv)
{
  setbuf(stdout,NULL);

  if (4 > argc) {
    fprintf(stderr,"Usage: proxy <listen_port> <server> <server_port>");
    return 1;
  }

  listen_port = atoi(argv[1]);
  server = argv[2];
  server_port = atoi(argv[3]);

  int listenfd;
  if (0 > (listenfd = start_listening(listen_port)))
    return 1;
  int connno = 0;

  int clientfd;
  while (0 <= (clientfd = accept(listenfd,NULL,NULL))) {
    pthread_t thread;

    printf("%d: accepted connection from client\n",connno);

    handler_arg *ha = (handler_arg*)malloc(sizeof(handler_arg));
    ha->clientfd = clientfd;
    ha->connno = connno++;

    if (0 != pthread_create(&thread,NULL,handler_thread,ha)) {
      perror("pthread_create");
      return 1;
    }
    if (0 != pthread_detach(thread)) {
      perror("pthread_detach");
      return 1;
    }
  }
  perror("accept");


  return 1;
}
