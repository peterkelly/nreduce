#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <netdb.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

#define BACKLOG 10
#define MAXNAME 100
#define CHUNKSIZE 1024
#define M 24
#define M2 (1 << M)

#define RETRY(expression)                                 \
    ({ long int __result;                                 \
       do __result = (long int) (expression);             \
       while (__result == -1L && errno == EINTR);         \
       __result; })

#define llist_append(_ll,_obj) do {                       \
    assert(!(_obj)->prev && !(_obj)->next);               \
    if ((_ll)->last) {                                    \
      (_obj)->prev = (_ll)->last;                         \
      (_ll)->last->next = (_obj);                         \
      (_ll)->last = (_obj);                               \
    }                                                     \
    else {                                                \
      (_ll)->first = (_ll)->last = (_obj);                \
    }                                                     \
  }  while(0)

#define llist_remove(_ll,_obj) do {                       \
    assert((_obj)->prev || ((_ll)->first == (_obj)));     \
    assert((_obj)->next || ((_ll)->last == (_obj)));      \
    if ((_ll)->first == (_obj))                           \
      (_ll)->first = (_obj)->next;                        \
    if ((_ll)->last == (_obj))                            \
      (_ll)->last = (_obj)->prev;                         \
    if ((_obj)->next)                                     \
      (_obj)->next->prev = (_obj)->prev;                  \
    if ((_obj)->prev)                                     \
      (_obj)->prev->next = (_obj)->next;                  \
    (_obj)->next = NULL;                                  \
    (_obj)->prev = NULL;                                  \
  } while(0)

typedef struct buffer {
  char *data;
  int alloc;
  int size;
} buffer;

typedef struct address {
  struct in_addr ip;
  int port;
} address;

typedef struct connection {
  int sock;
  int connecting;
  char name[MAXNAME];
  address addr;
  buffer recvbuf;
  buffer sendbuf;
  struct connection *prev;
  struct connection *next;
} connection;

typedef struct connectionlist {
  connection *first;
  connection *last;
} connectionlist;

connectionlist connections;
int listensock = -1;

int setnonblock(int fd)
{
  int flags;
  if (0 > (flags = fcntl(fd,F_GETFL))) {
    perror("fcntl(F_GETFL)");
    return -1;
  }

  flags |= O_NONBLOCK;

  if (0 > fcntl(fd,F_SETFL,flags)) {
    perror("fcntl(F_SETFL)");
    return -1;
  }

  return 0;
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

void handle_new_connection()
{
  struct sockaddr_in rmtaddr;
  int sin_size = sizeof(struct sockaddr_in);
  int sock;
  int port;
  unsigned char *addrbytes;
  struct hostent *he;
  connection *conn;

  if (0 > (sock = accept(listensock,(struct sockaddr*)&rmtaddr,&sin_size))) {
    perror("accept");
    return;
  }

  if (0 > setnonblock(sock)) {
    close(sock);
    return;
  }

  conn = (connection*)calloc(1,sizeof(connection));
  conn->sock = sock;

  addrbytes = (unsigned char*)&rmtaddr.sin_addr;
  port = ntohs(rmtaddr.sin_port);

  he = gethostbyaddr(&rmtaddr.sin_addr,sizeof(struct in_addr),AF_INET);
  if (NULL == he)
    snprintf(conn->name,MAXNAME,"%u.%u.%u.%u:%d",
           addrbytes[0],addrbytes[1],addrbytes[2],addrbytes[3],port);
  else
    snprintf(conn->name,MAXNAME,"%s:%d",he->h_name,port);

  conn->addr.ip = rmtaddr.sin_addr;
  conn->addr.port = -1; /* client's listen port; we don't know this yet */

  printf("Got connection from %s\n",conn->name);

  llist_append(&connections,conn);

  return;
}

void remove_connection(connection *conn)
{
  llist_remove(&connections,conn);
  close(conn->sock);
  free(conn->recvbuf.data);
  free(conn->sendbuf.data);
  free(conn);
}

void connect_done(connection *conn)
{
  int err;
  int optlen = sizeof(int);

  /* Get the return value of the asynchronous connect() call */
  if (0 > getsockopt(conn->sock,SOL_SOCKET,SO_ERROR,&err,&optlen)) {
    fprintf(stderr,"Connection to %s failed: getsockopt(SO_ERROR): %s\n",
           conn->name,strerror(err));
    remove_connection(conn);
    return;
  }

  /* Did the connection succeed? */
  if (0 != err) {
    fprintf(stderr,"Connection to %s failed: %s\n",conn->name,strerror(err));
    remove_connection(conn);
    return;
  }

  printf("Connection to %s completed asynchronously\n",conn->name);
  conn->connecting = 0;

  /* FIXME: send our port to the peer */
}

void buffer_allocto(buffer *buf, int alloc)
{
  if (buf->alloc < alloc) {
    buf->alloc = alloc;
    buf->data = realloc(buf->data,buf->alloc);
  }
}

void buffer_append(buffer *buf, const void *data, int size)
{
  if (buf->alloc < buf->size+size) {
    buf->alloc = buf->size+size;
    buf->data = realloc(buf->data,buf->alloc);
  }
  memmove(&buf->data[buf->size],data,size);
  buf->size += size;
}

void buffer_remove(buffer *buf, int count)
{
  memmove(&buf->data[0],&buf->data[count],buf->size-count);
  buf->size -= count;
}

int open_connection(const char *hostname, int port)
{
  int sockfd;
  struct hostent *he;
  struct sockaddr_in addr;
  connection *conn;
  int r;

  if (NULL == (he = gethostbyname(hostname))) {
    herror("gethostbyname");
    return -1;
  }

  if (0 > (sockfd = socket(AF_INET,SOCK_STREAM,0))) {
    perror("socket");
    return -1;
  }

  if (0 > setnonblock(sockfd)) {
    close(sockfd);
    return -1;
  }

  addr.sin_family = AF_INET;
  addr.sin_port = htons(port);
  addr.sin_addr = *((struct in_addr*)he->h_addr);
  memset(&addr.sin_zero,0,8);

  printf("Opening connection to %s:%d\n",hostname,port);
  r = connect(sockfd,(struct sockaddr*)&addr,sizeof(struct sockaddr));
  if ((0 > r) && (EINPROGRESS != errno)) {
    close(sockfd);
    perror("connect");
    return -1;
  }

  conn = (connection*)calloc(1,sizeof(connection));
  conn->sock = sockfd;
  conn->connecting = (0 > r);
  snprintf(conn->name,MAXNAME,"%s:%d",hostname,port);
  conn->addr.ip = addr.sin_addr;
  conn->addr.port = port;

  buffer_append(&conn->sendbuf,&port,sizeof(int));

  llist_append(&connections,conn);

  if (!conn->connecting)
    printf("Connection to %s:%d completed immediately\n",hostname,port);

  return 0;
}

#define HEADER_SIZE (2*sizeof(int))

void handle_message(int tag, int size, const char *data)
{
  printf("tag = %d, size = %d, data = \"",tag,size);
  write(STDOUT_FILENO,data,size);
  printf("\"\n");
}

void process_incoming(connection *conn)
{
  int start = 0;

  if (0 > conn->addr.port) {
    if (sizeof(int) > conn->recvbuf.size)
      return;
    conn->addr.port = *(int*)conn->recvbuf.data;
    start = sizeof(int);

    printf("Client %s: connection port %d\n",conn->name,conn->addr.port);
  }

  while (HEADER_SIZE <= conn->recvbuf.size-start) {
    int size = *(int*)&conn->recvbuf.data[start];
    int tag = *(int*)&conn->recvbuf.data[start+sizeof(int)];

    if (HEADER_SIZE+size > conn->recvbuf.size-start)
      break; /* incomplete message */

    handle_message(tag,size,conn->recvbuf.data+start+HEADER_SIZE);
    printf("%s: message tag = %d, size = %d\n",conn->name,tag,size);
    start += HEADER_SIZE+size;
  }

  if (0 < start) {
    memmove(&conn->recvbuf.data[0],&conn->recvbuf.data[start],
            conn->recvbuf.size-start);
    conn->recvbuf.size -= start;
  }
}

#define FIND_SUCCESSOR 20

typedef struct finger {
  address addr;
  int start;
  int node;
} finger;

void join(connection *conn)
{
}

int nodeloop()
{
  while (1) {
    fd_set readfds;
    fd_set writefds;
    int highest = listensock;
    connection *conn;
    connection *next;

    FD_ZERO(&readfds);
    FD_ZERO(&writefds);

    FD_SET(listensock,&readfds);

    for (conn = connections.first; conn; conn = conn->next) {
      if (highest < conn->sock)
        highest = conn->sock;

      if (conn->connecting) {
        FD_SET(conn->sock,&writefds);
      }
      else {
        FD_SET(conn->sock,&readfds);
        if (0 < conn->sendbuf.size)
          FD_SET(conn->sock,&writefds);
      }
    }

    if (0 > select(highest+1,&readfds,&writefds,NULL,NULL)) {
      if (EINTR == errno)
        continue;
      perror("select");
      return -1;
    }

    if (FD_ISSET(listensock,&readfds))
      handle_new_connection();

    for (conn = connections.first; conn; conn = next) {
      next = conn->next;

      if (FD_ISSET(conn->sock,&readfds)) {
        int r;
        buffer_allocto(&conn->recvbuf,conn->recvbuf.size+CHUNKSIZE);
        r = RETRY(read(conn->sock,&conn->recvbuf.data[conn->recvbuf.size],
                       CHUNKSIZE));
        if (0 > r) {
          fprintf(stderr,"read from %s: %s\n",conn->name,strerror(errno));
          remove_connection(conn);
          continue;
        }
        else if (0 == r) {
          fprintf(stderr,"%s closed connection\n",conn->name);
          remove_connection(conn);
          continue;
        }
        else {
          printf("read %d bytes from %s\n",r,conn->name);
          conn->recvbuf.size += r;
          process_incoming(conn);
        }
      }
      if (FD_ISSET(conn->sock,&writefds)) {
        if (conn->connecting) {
          connect_done(conn);
        }
        else {
          int w = write(conn->sock,conn->sendbuf.data,conn->sendbuf.size);
          if (0 > w) {
            fprintf(stderr,"write to %s: %s\n",conn->name,strerror(errno));
            remove_connection(conn);
            continue;
          }
          memmove(&conn->sendbuf.data[0],&conn->sendbuf.data[w],
                  conn->sendbuf.size-w);
          conn->sendbuf.size -= w;
        }
      }
    }
  }
}

void sigusr1(int sig)
{
  printf("SIGUSR1\n");
  open_connection("pmk1",5678);
}

int main(int argc, char **argv)
{
  int port;
  setbuf(stdout,NULL);
  memset(&connections,0,sizeof(connectionlist));

  signal(SIGUSR1,sigusr1);

  if (2 > argc) {
    fprintf(stderr,"Usage: chord <port>\n");
    return -1;
  }

  port = atoi(argv[1]);
  if (0 > (listensock = start_listening(port)))
    return -1;

  if (4 <= argc)
    open_connection(argv[2],atoi(argv[3]));

  return nodeloop();
}
