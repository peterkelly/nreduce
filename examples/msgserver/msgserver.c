/*

Message-based server

This program implements a simple server which reads data from client in terms
of complete messages, rather than just byte streams. It is similar to server.c
in other respects.

For many applications it is more useful to exchange data in terms of messages
rather than streams. A message consists of header information followed by a
sequence of bytes. The header is of a fixed size and includes, at minimum, the
number of bytes in the payload of the message, and possibly other information
as well. In this example this extra information is in the form of a "tag", an
integer value indicating the type of the message. Applications may use this to
determine how to interpret the message, or what action to take as a result of
receiving it; MPI provides a similar facility.

The main point of this example is to demonstrate how to transfer these messages
via a socket connection. Each time a read operation is performed, the
communications later implemented here inspects the data to determine if a
complete message has been received. The size field extracted from the header is
used to find out how much data the payload is supposed to contain. Only when
all of this data has been received will the message be delivered. "Delivery" in
this case means passing the message to the "application layer", which consists
of the handle_message() function. In a real application this function would be
replaced with something that sends the message to the main logic of the program.

Because a single call to read() may return only part of a message, we can't
necessarily do the delivery after just that call. Instead, we have to maintain
a receive buffer that stores all of the data we've read since the end of the
last message. There may be many calls to read() before this buffer contains all
of the data for a particular message. Once we determine that the receive buffer
contains all data from the message, it is extracted and sent to the application
layer.

It is also possible that multiple messages could be returned from a single
read() call, if they are small enough. These need to be processed individually.
The way that this and the previous situation is handled is by appending all data
returned from read() to the receive buffer, and then calling the
process_incoming() function to inspect this buffer and extract any completed
messages from it. This separates the logic needed to identify and process
messages from the code that receives the data over the network. To avoid
copying however, we read directly into the receive buffer, allocating more
memory for it if necessary beforehand.

This program only supports one connection at a time, so a single buffer is
sufficient to hold in-progress messages. A server which permitted multiple
simultaneous connections would need to have a separate receive buffer for each
connection.

The file msgclient.c contains a sample implementation of a client that sends
data in the required format, consisting of the size, tag, and actual data.

The program is run as follows:

  ./msgserver <port>

*/

#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <string.h>
#include <unistd.h>

#define BACKLOG 10
#define CHUNKSIZE 1024

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

void handle_message(int tag, int size, const char *data)
{
  printf("tag = %d, size = %d, data = \"",tag,size);
  write(STDOUT_FILENO,data,size);
  printf("\"\n");
}

#define HEADER_SIZE (2*sizeof(int))

struct {
  int alloc;
  int size;
  char *data;
} recvbuf;

/* Inspect the receive buffer and process any complete messages it contains */
void process_incoming()
{
  int start = 0;

  /* Keep looping until we find an incomplete message, or no more data after
     a complete message */
  while (HEADER_SIZE <= recvbuf.size-start) {
    int size = *(int*)&recvbuf.data[start];
    int tag = *(int*)&recvbuf.data[start+sizeof(int)];

    if (HEADER_SIZE+size > recvbuf.size-start)
      break; /* incomplete message */

    handle_message(tag,size,recvbuf.data+start+HEADER_SIZE);
    start += HEADER_SIZE+size;
  }

  /* If we've consumed any of the data in the buffer, then remove that
     from the front. The remaining data, if any, will be an incomplete
     message. */
  if (0 < start) {
    memmove(&recvbuf.data[0],&recvbuf.data[start],recvbuf.size-start);
    recvbuf.size -= start;
  }
}

int main(int argc, char **argv)
{
  int port;
  int listensock;
  int sock;

  setbuf(stdout,NULL);
  memset(&recvbuf,0,sizeof(recvbuf));

  if (2 > argc) {
    fprintf(stderr,"Usage: msgserver <port>\n");
    return -1;
  }

  port = atoi(argv[1]);
  if (0 > (listensock = start_listening(port)))
    return -1;

  printf("Waiting for connection...\n");
  while (0 <= (sock = accept_connection(listensock))) {

    /* Reset the buffer size to 0 in case a previous connection closed without
       completing the last message. */
    recvbuf.size = 0;

    while (1) {
      int r;

      /* Allocate more memory for the receive buffer, if necessary */
      if (recvbuf.alloc < recvbuf.size+CHUNKSIZE) {
        recvbuf.alloc = recvbuf.size+CHUNKSIZE;
        recvbuf.data = realloc(recvbuf.data,recvbuf.alloc);
      }

      /* Read the next chunk of data */
      r = read(sock,&recvbuf.data[recvbuf.size],CHUNKSIZE);
      if (0 > r) {
        /* Read error */
        perror("read");
        close(sock);
        break;
      }
      else if (0 == r) {
        /* Clean disconnect */
        if (0 < recvbuf.size)
          fprintf(stderr,"Client disconnected without completing message\n");
        close(sock);
        break;
      }
      else {
        /* We have some data... process all of the completed messages we've
           received so far. It is possible that the call to read may have
           obtained the data from a message that was incomplete the last
           time round, so this will be processed too. */
        recvbuf.size += r;
        process_incoming();
      }
    }
    printf("Client closed connection\n");
  }

  return 0;
}
