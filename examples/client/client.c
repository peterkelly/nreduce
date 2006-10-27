/*

Client for one-way transmission of data to a server

This is a demonstration of probably the simplest possible client. All it does
is connect to a server, and transmit data read from standard input.

The reason for only supporting one way transfer of data to the server for this
example is that in order to be able to read data from the server *and* from
standard input, we would need to use a select() loop. This adds more complexity
and is thus covered in other examples. The main point of this program is to
demonstrate how to open a connection to a server.

Note: *All* system calls should be checked for errors and handled cleanly,
as there are various things that can go wrong when setting up and using
network connections. We chose to return a -1 error code from the functions
rather than exiting the program altogether. While it doesn't make any
difference for this small program, this enables the code to be reused in larger
programs that want to handle the errors appropriately and maybe keep running
or retry the operation.

The program is run as follows:

  ./client <host> <port>

The host may be either a name or IP address.

*/

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

int main(int argc, char **argv)
{
  char *hostname;
  int port;
  int sock;
  char buf[1024];
  int r;

  setbuf(stdout,NULL);

  if (3 > argc) {
    fprintf(stderr,"Usage: client <host> <port>\n");
    return -1;
  }

  hostname = argv[1];
  port = atoi(argv[2]);

  /* Open a connection to the server */
  if (0 > (sock = open_connection(hostname,port)))
    return -1;

  printf("Connected to server; enter data to send\n");

  /* Read data from standard input and send it to the server. Both the
     standard input file descriptor and the server socket are in blocking mode,
     so e.g. if the server is not yet ready to receive the data then the
     client will block until it is able to send more. This loop will terminate
     when standard input is complete (e.g. the user pressed CTRL-D on the
     terminal). */
  while (0 < (r = read(STDIN_FILENO,buf,1024)))
    write(sock,buf,r);

  /* Close the connection to the server and release resources associated with
     the socket. While this will be done automatically when we exit the program
     it's good practice to do it here anyway, as a larger program may keep
     running for long after the socket is no longer needed. */
  close(sock);

  return 0;
}
