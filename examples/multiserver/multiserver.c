/*

Server supporting multiple simultaneous client connections.

This is still a single threaded server, but uses a select() loop to check for
incoming data on multiple sockets. This allows multiple clients to be handled
from within the one main loop. On each loop iteration, the program blocks until
data from one or more clients is available, and then reads from each of those
clients.

This program provides similar functionality to server.c. Data is only read from
clients (no replies are sent), and all data received is printed to the terminal.

Non-blocking I/O is used on each of the sockets. This changes the behaviour
of read() so that if there is no data available on a given socket, then the
function will return immediately instead of blocking. This is necessary so that
the program can check for data on other sockets, since any given client may
potentially wait for a very long time before sending more data. It also
necessitates the use of select(), which is where the blocking occurs instead.
In this case the program blocks only until data is available from *any* of the
clients, rather than just a specific one.

The set of connected clients is stored in a fixed-size array. This allows up
to MAXCLIENTS to be connected at any one time, not counting any that may be
waiting in the connection queue. However, if more clients try to connect and
the array is already full then they will be turned away with an immediate socket
disconnection.

The clientsockets array holds the socket associated with each active client. A
client is considered connected if the socket is >= 0. For elements in the array
that do not represent a client connection, the value is set to -1. At the
beginning of the program, all elements in the array are initialised to -1. When
a new client connection request is retrieved the program will look through this
array for an empty slot and set the appropriate value to the new client's
socket. When a client disconnects, then that element in the array is cleared
and can be reused for a subsequent connection.

The program is run as follows:

  ./multiserver <port>

*/

#include <fcntl.h>
#include <netdb.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <errno.h>

#define BACKLOG 10
#define MAXCLIENTS 10

int clientsockets[MAXCLIENTS];

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

/* Accept a client connection. See server.c for more details */
int accept_connection(int ls)
{
  struct sockaddr_in rmtaddr;
  int sin_size = sizeof(struct sockaddr_in);
  int cs;
  int port;
  unsigned char *addrbytes;
  struct hostent *he;

  if (0 > (cs = accept(ls,(struct sockaddr*)&rmtaddr,&sin_size))) {
    perror("accept");
    return -1;
  }

  addrbytes = (unsigned char*)&rmtaddr.sin_addr;
  port = ntohs(rmtaddr.sin_port);

  he = gethostbyaddr(&rmtaddr.sin_addr,sizeof(struct in_addr),AF_INET);
  if (NULL == he)
    printf("Got connection from %u.%u.%u.%u:%d\n",
           addrbytes[0],addrbytes[1],addrbytes[2],addrbytes[3],port);
  else
    printf("Got connection from %s:%d\n",he->h_name,port);

  return cs;
}

/* Put the socket into non-blocking mode. This is done by using the fcntl()
   system call to set the O_NONBLOCK flag. Although in practice it's unlikely
   that these calls could return errors, we check the results just to be sure,
   at the cost of slightly more verbose code. */
int set_non_blocking(int sock)
{
  int flags;

  if (0 > (flags = fcntl(sock,F_GETFL))) {
    perror("fcntl(F_GETFL)");
    return -1;
  }

  flags |= O_NONBLOCK;

  if (0 > fcntl(sock,F_SETFL,flags)) {
    perror("fcntl(F_SETFL)");
    return -1;
  }

  return 0;
}

/* Close a client socket and remove it from the list of active clients. This
   is done by setting the appropriate value in the client list to -1 indicating
   that the slot is free. Note that the parameter passed in here is the *index*
   of the client in the array, not the actual socket itself. This function is
   called both upon normal disconnection and when an error communicating with
   the client occurs. */
void remove_client(int i)
{
  close(clientsockets[i]);
  clientsockets[i] = -1;
}

/* Accept a new client connection and add it to the list of active clients. If
   there are already the maximum number of clients connected, the new one will
   be disconnected. This also initialises the socket into non-blocking mode. */
void handle_new_connection(int listensock)
{
  int sock;
  int c;
  if (0 > (sock = accept_connection(listensock)))
    return;

  for (c = 0; c < MAXCLIENTS; c++) {
    if (0 > clientsockets[c])
      break;
  }

  if (MAXCLIENTS == c) {
    close(sock); /* too many connections */
    return;
  }

  clientsockets[c] = sock;

  /* Try to set non-blocking mode. If an error occurs, just disconnect the
     client and remove it from the list. The error will be reported but the
     server can safely keep running and other clients won't be affected. */
  if (0 > set_non_blocking(clientsockets[c]))
    remove_client(c);
}

int main(int argc, char **argv)
{
  int port;
  int listensock;
  int i;

  setbuf(stdout,NULL);

  if (2 > argc) {
    fprintf(stderr,"Usage: multiserver <port>\n");
    return -1;
  }

  port = atoi(argv[1]);
  if (0 > (listensock = start_listening(port)))
    return -1;

  for (i = 0; i < MAXCLIENTS; i++)
    clientsockets[i] = -1;

  /* Main loop: This handles all actions performed by the server; including
     accepting new connections, reading data from clients, and dealing with
     disconnections/errors. */
  printf("Waiting for connection...\n");
  while (1) {
    fd_set readfds;
    int highest = -1;

    /* Initialise the set of file descriptors we're interested in. Since
       we're only interested in new connections and incoming data, we only
       have a file descriptor set for reading.

       The sockets we include in the set are:
       - The listen socket (upon which we receive new connections)
       - The connection socket for each client (from which we read data)

       As we set these, we also record the highest-numbered socket that we
       encounter. This is needed by select() so that it knows how many elements
       are in the array. */
    FD_ZERO(&readfds);

    FD_SET(listensock,&readfds);
    if (highest < listensock)
      highest = listensock;

    for (i = 0; i < MAXCLIENTS; i++) {
      if (0 <= clientsockets[i]) {
        FD_SET(clientsockets[i],&readfds);
        if (highest < clientsockets[i])
          highest = clientsockets[i];
      }
    }

    /* Call select to block until we have a new connection and/or data
       available from one or more clients. */
    if (0 > select(highest+1,&readfds,NULL,NULL,NULL)) {
      perror("select");
      return -1;
    }

    /* Test to see if a new connection request has been made. If so, try to
       accept the connection. */
    if (FD_ISSET(listensock,&readfds))
      handle_new_connection(listensock);

    /* For each client, if the file descriptor set indicates that it's socket
       is readable, try to read some data from it. If an error occurs then
       we report the error and disconnect the client. If the client has
       disconnected normally then we just remove it from our list.

       Note that we do not check for the case here where read() returns
       EAGAIN. Normally when calling read (or similar function) on a
       non-blocking socket, this should be checked for, and not considered
       a "real" error since it just means there is no data available on that
       socket at this particular time. However, since select has indicated that
       there is data available on this socket, we assume that EAGAIN will not
       be returned. */
    for (i = 0; i < MAXCLIENTS; i++) {
      if ((0 <= clientsockets[i]) && FD_ISSET(clientsockets[i],&readfds)) {
        char buf[1024];
        int r = read(clientsockets[i],buf,1024);
        if (0 > r) {
          fprintf(stderr,"read from %d: %s\n",i,strerror(errno));
          remove_client(i);
        }
        else if (0 == r) {
          printf("Client closed connection\n");
          remove_client(i);
        }
        else {
          /* Data has been read ok... write it to standard output */
          write(STDOUT_FILENO,buf,r);
        }
      }
    }
  }

  return 0;
}
