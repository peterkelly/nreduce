/*

Chat server

This program is based on multiserver.c. It allows multiple clients to connect
to the server and send data. The difference with this version is that it acts
as a "chat" server; any data receives from a client is written out to every
client (including the one that sent it). It could be used as a basic IRC-style
system allowing different users to communicate with each other. Any client need
only connect with telnet to get interactive access to the chat room.

One complication with the fact that we are sending data back out to clients
is that we can't just send it directly with write(); we need to buffer the
data instead. If one client sends a large amount of data, and some of the other
clients are on slow network links, then if we tried to write directly on a
blocking socket then the server may have to block waiting for the slow clients
to be ready for more data. Instead, we use non-blocking mode.

The kernel maintains an output buffer for each socket and when that is full,
the non-blocking write() that we use will return EAGAIN. This means that we
cannot send more data until the contents of the buffer has been sent over the
network and more room becomes available in the buffer.

To get around this problem, we maintain our own buffer for each client. This
contains the data that is waiting to be written to the client. It may be
considerably larger than the kernel's buffer, due to the fact that we might
have received a lot of data from some of the clients that we have not been able
to send out yet. Each time that select() indicates writability of a socket, we
try to write some of the data from this buffer out to the client. The portion
that is written is removed from our own buffer. There may be some left which we
have to hold on to until the socket is ready for more data to be written to it.

Whenever some data is received from one of the clients, that data is appended
to the output buffer of every other client, as well as the sending client
itself. The reason we need to maintain separate output buffers is that for each
connection there may be a different amount of data that has been written to it,
so we need to keep track of what data is remaining to be sent to each client.
The differences in the amount of data sent are affected in part by the speed of
a client's connection; for a local client it will typically be very fast to
transmit the data but for another client elsewhere on the Internet it may take
longer and thus data could remain in the buffer for a longer period of time.

The mechanism for keeping track of client connections is similar to that used
in multiserver.c. However, since we also want to keep track of a buffer for
each client, we use an array of struct client rather than just an array of
sockets. This structure has the socket number, output buffer, and also other
information associated with the client. The name of each client is kept in
this structure, and displayed in the tracing information output by the server
that logs the reads and writes that have occurred.

As this program is just to demonstrate the core concepts behind supporting
multiple clients, there is no sophistication in the functionality provided by
the chat server. It would be possible to add many features on top of this
structure, such as:
  - maintaining a username for each client and prepending it to each message
    sent out to the other clients
  - support for an arbitrary number of clients
  - administrative functions such as disconnecting specific users and remote
    shutdown of the server

The program is run as follows:

  ./chatserver <port>

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
#define MAXNAME 100

/* Structure used to represent information about a client. We store a fixed
   number (MAXCLIENTS) of these; the information is only valid if the connected
   field is non-zero. The out and size fields represrent the data and size of
   the output buffer, which we are waiting to send to the client. The name is
   a fixed size string containing the hostname (or IP) and port number which
   the client is connected from. */
struct client {
  int connected;
  int sock;
  char *out;
  int size;
  char name[MAXNAME];
};

/* Array of client objects representing both connected and free client slots */
struct client clients[MAXCLIENTS];

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
int accept_connection(int ls, char *name, int namelen)
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
    snprintf(name,namelen,"%u.%u.%u.%u:%d",
           addrbytes[0],addrbytes[1],addrbytes[2],addrbytes[3],port);
  else
    snprintf(name,namelen,"%s:%d",he->h_name,port);

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

/* Close a client socket and remove it from the list of active clients. Note
   that the parameter passed in here is the *index* of the client in the array,
   not the actual socket itself. This function is called both upon normal
   disconnection and when an error communicating with the client occurs. */
void remove_client(int i)
{
  clients[i].connected = 0;
  close(clients[i].sock);
  free(clients[i].out);
}

/* Accept a new client connection and add it to the list of active clients. If
   there are already the maximum number of clients connected, the new one will
   be disconnected. This also initialises the socket into non-blocking mode
   and stores various information about the client such as it's hostname/port
   in the client structure. */
void handle_new_connection(int listensock)
{
  int sock;
  int c;
  char name[MAXNAME];
  if (0 > (sock = accept_connection(listensock,name,MAXNAME)))
    return;

  for (c = 0; c < MAXCLIENTS; c++) {
    if (!clients[c].connected)
      break;
  }

  if (MAXCLIENTS == c) {
    close(sock); /* too many connections */
    return;
  }

  clients[c].connected = 1;
  clients[c].sock = sock;
  clients[c].out = NULL;
  clients[c].size = 0;
  memcpy(clients[c].name,name,MAXNAME);

  printf("Got connection from %s\n",clients[c].name);

  if (0 > set_non_blocking(clients[c].sock))
    remove_client(c);
}

/* Append the specified data to each client's output buffer. This will
   subsequently be transmitted to the clients. As mentioned in the comments
   at the top of this file, we need to maintain a separate output buffer
   for each client, as they may receive data at different rates or times. */
void add_data(char *data, int size)
{
  int i;
  for (i = 0; i < MAXCLIENTS; i++) {

    if (!clients[i].connected)
      continue;

    /* Reallocate the array so that we have the right amount of memory for
       the data we want to add. This is simple but inneficient; ideally it
       we should be a bit more clever about only doing a reallocation when
       absolutely necessary. */
    clients[i].out = realloc(clients[i].out,clients[i].size+size);
    memcpy(&clients[i].out[clients[i].size],data,size);
    clients[i].size += size;
  }
}

int main(int argc, char **argv)
{
  int port;
  int listensock;

  setbuf(stdout,NULL);

  if (2 > argc) {
    fprintf(stderr,"Usage: chatserver <port>\n");
    return -1;
  }

  port = atoi(argv[1]);
  if (0 > (listensock = start_listening(port)))
    return -1;

  memset(&clients,0,MAXCLIENTS*sizeof(struct client));

  /* Main loop: This handles all actions performed by the server; including
     accepting new connections, reading/writing data from/to clients, and
     dealing with disconnections/errors. */
  printf("Waiting for connection...\n");
  while (1) {
    fd_set readfds;
    fd_set writefds;
    int highest = -1;
    int i;

    /* Initialise the set of file descriptors we're interested in. We have
       two sets: one for reading, and one for writing. The reading set contains
       the listening socket (upon which new connections are received), and the
       socket of each connected client. The writing set is only used for clients
       that have data in their output buffer that is waiting to be sent; we want
       to know as soon as these clients are ready to receive more data.

       As we set these, we also record the highest-numbered socket that we
       encounter. This is needed by select() so that it knows how many elements
       are in the array. */
    FD_ZERO(&readfds);
    FD_ZERO(&writefds);

    FD_SET(listensock,&readfds);
    if (highest < listensock)
      highest = listensock;

    for (i = 0; i < MAXCLIENTS; i++) {
      if (clients[i].connected) {
        FD_SET(clients[i].sock,&readfds);
        if (highest < clients[i].sock)
          highest = clients[i].sock;

        /* Only tell select() to check for writeability if there's data in the
           output buffer we want to send */
        if (0 < clients[i].size)
          FD_SET(clients[i].sock,&writefds);
      }
    }

    /* Call select to block until we have a new connection and/or one or more
       of the sockets is ready for reading/writing */
    if (0 > select(highest+1,&readfds,&writefds,NULL,NULL)) {
      perror("select");
      return -1;
    }

    /* Test to see if a new connection request has been made. If so, try to
       accept the connection. */
    if (FD_ISSET(listensock,&readfds))
      handle_new_connection(listensock);

    /* Iterate through each of the active clients and perform the appropriate
       I/O operations on each, where possible */
    for (i = 0; i < MAXCLIENTS; i++) {

      if (!clients[i].connected)
        continue;

      /* For each client, if the file descriptor set indicates that it's socket
         is readable, try to read some data from it. If an error occurs then
         we report the error and disconnect the client. If the client has
         disconnected normally then we just remove it from our list. */
      if (FD_ISSET(clients[i].sock,&readfds)) {
        char buf[1024];
        int r = read(clients[i].sock,buf,1024);
        if (0 > r) {
          fprintf(stderr,"read from %s: %s\n",clients[i].name,strerror(errno));
          remove_client(i);
        }
        else if (0 == r) {
          printf("Client closed connection\n");
          remove_client(i);
        }
        else {
          printf("read %d bytes from %s\n",r,clients[i].name);
          add_data(buf,r);
        }
      }

      /* Check to see if the client's socket is writable. This test will only
         succeed if we've specifically asked for this information above, which
         will only happen when there is some data in the output buffer for that
         client waiting to be written. If the client is ready to receive more
         data, we send a portion of the output buffer to it. The number of bytes
         that write() returns is removed from the front of the buffer.

         If the client is able to receive only some data that we tried to write
         but not all of it, the remaining data will remain in the buffer and be
         sent later on. */
      if (FD_ISSET(clients[i].sock,&writefds)) {
        int w = write(clients[i].sock,clients[i].out,clients[i].size);
        if (0 > w) {
          fprintf(stderr,"write to %s: %s\n",clients[i].name,strerror(errno));
          remove_client(i);
        }
        memmove(&clients[i].out[0],&clients[i].out[w],clients[i].size-w);
        clients[i].size -= w;
        printf("wrote %d bytes to %s\n",w,clients[i].name);
      }
    }
  }

  return 0;
}
