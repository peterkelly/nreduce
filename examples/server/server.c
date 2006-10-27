/*

Basic single-threaded server

This program listens on a specified port for connections. When a client
connects, the server reads continuously from the client until it has finished
sending data. The data read is output to the server's terminal.

Only one client connection is permitted at a time. If one is currently open,
and another client attempts to connect, then it will sit in the queue waiting
for the first to complete it's data transmission and disconnect. The maximum
number of pending client connections that can be outstanding at any given point
in time is determined by BACKLOG. Platforms differ in what they do when the
queue reaches this size, but in general clients will get some sort of error
if they try to connect and the queue is full.

For the sake of simplicity on the client, we do not send back any replies. This
means that the client can safely just do one thing at a time (send data) without
having to use a select() loop to cater for reads and writes. See client/client.c
for the corresponding client-side code.

The main point of this example is to demonstrate how to create a listening
socket and accept connections. Most of the logic for this is in the
start_listening() function, which initialises the socket so that accept() can
subsequently be used with it.

Note: *All* system calls should be checked for errors and handled cleanly,
as there are various things that can go wrong when setting up and using
network connections. We chose to return a -1 error code from the functions
rather than exiting the program altogether. While it doesn't make any
difference for this small program, this enables the code to be reused in larger
programs that want to handle the errors appropriately and maybe keep running
or retry the operation.

The program is run as follows:

  ./server <port>

*/

#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <string.h>
#include <unistd.h>

#define BACKLOG 1

/* Create a socket and put it into listening mode so that clients can connect
   to it */
int start_listening(int port)
{
  int yes = 1;
  struct sockaddr_in local_addr;
  int sock;

  /* Create the socket object */
  if (0 > (sock = socket(AF_INET,SOCK_STREAM,0))) {
    perror("socket");
    return -1;
  }

  /* Tell the socket to reuse the address we want to bind to if it's in a
     TIME_WAIT STATE. This can occur if another process was previously
     listening on the same address/port, and is no longer doing so but the
     port has not been properly released yet. When testing a server like this
     in such a manner that it is being started and stopped often, not using
     this option will result in "bind: address already in use" errors. */
  if (0 > setsockopt(sock,SOL_SOCKET,SO_REUSEADDR,&yes,sizeof(int))) {
    perror("setsockopt");
    close(sock);
    return -1;
  }

  /* Set the protocol, address, and port number that we want to listen on.
     It is possible to listen just on a specific IP address (e.g. corresponding
     to a particular network interface), but in most cases you want to be able
     to handle connections on any of this machine's IPs; for this we use
     INADDR_ANY. The port number must be converted to network byte order, which
     is why htons() is used. */
  local_addr.sin_family = AF_INET;
  local_addr.sin_port = htons(port);
  local_addr.sin_addr.s_addr = INADDR_ANY;
  memset(&local_addr.sin_zero,0,8);

  /* Bind the socket we've just created to the specified address. This creates
     an association between the two so that when we put the socket in listening
     mode, connection requests to the relevant address can be picked up by the
     socket. */
  if (0 > bind(sock,(struct sockaddr*)&local_addr,sizeof(struct sockaddr))) {
    perror("bind");
    close(sock);
    return -1;
  }

  /* Put the socket into listening mode. This means that we can accept
     connections on it; this is done later using accept(). */
  if (0 > listen(sock,BACKLOG)) {
    perror("listen");
    close(sock);
    return -1;
  }

  return sock;
}

/* Accept a new client connection from a listening socket */
int accept_connection(int ls)
{
  struct sockaddr_in rmtaddr;
  int sin_size = sizeof(struct sockaddr_in);
  int cs;
  int port;
  unsigned char *addrbytes;
  struct hostent *he;

  /* Get the next connection from the queue of outstanding requests. If there
     are no connection requests yet, this will block until one arrives.
     This is in fact all we need to do to get the connection; the next bit
     just reports information about the client's requests and isn't strictly
     necessary. */
  if (0 > (cs = accept(ls,(struct sockaddr*)&rmtaddr,&sin_size))) {
    perror("accept");
    return -1;
  }


  /* Convert the address and port into formats more suitable for printing */
  addrbytes = (unsigned char*)&rmtaddr.sin_addr;
  port = ntohs(rmtaddr.sin_port);

  /* Do a reverse name lookup for the client. If this succeeds, we print
     the client's hostname, otherwise we just display it's IP. We ignore
     lookup errors here. */
  he = gethostbyaddr(&rmtaddr.sin_addr,sizeof(struct in_addr),AF_INET);
  if (NULL == he)
    printf("Got connection from %u.%u.%u.%u:%d\n",
           addrbytes[0],addrbytes[1],addrbytes[2],addrbytes[3],port);
  else
    printf("Got connection from %s:%d\n",he->h_name,port);

  return cs;
}

int main(int argc, char **argv)
{
  int port;
  int listensock;
  int sock;

  setbuf(stdout,NULL);

  if (2 > argc) {
    fprintf(stderr,"Usage: server <port>\n");
    return -1;
  }

  port = atoi(argv[1]);

  /* Create the listening socket */
  if (0 > (listensock = start_listening(port)))
    return -1;

  /* Repeatedly accept connections from clients */
  printf("Waiting for connection...\n");
  while (0 <= (sock = accept_connection(listensock))) {
    int r;
    char buf[1024];

    /* Read all data from the client until they have finished sending data
       or an error occurs. The socket is in blocking mode so this will cause
       write() to wait until data is available before returning. We ignore
       errors here. */
    while (0 < (r = read(sock,buf,1024)))
      write(STDOUT_FILENO,buf,r);

    /* No more data available (r == 0) or an error occurred (r == -1). Either
       case is considered to be the end of our conversation with the client. */
    printf("Client closed connection\n");
    close(sock);
  }

  return 0;
}
