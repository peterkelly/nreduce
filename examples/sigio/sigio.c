/*

Demonstration of asynchronous I/O using signals

Signaled I/O is useful when your program needs to spend most of it's time doing
compute-intensive tasks but also needs to respond to I/O events. If it's
inconvenient to do regular polling as part of the computation loop, then you
can arrange for your program to receive the SIGIO signal whenever I/O is
available, and then handle the I/O stuff within the signal handler. This allows
the main computation routine to proceed in ignorance of the fact that I/O is
even happening.

When a signal handler is called, a new frame is added to the stack and the
signal handler function is executed within that frame. This can occur at
literally any point during computation, so it's necessary to be very careful
about accessing or manipulating any data structures that may be in an
inconsistent state. However, if the data manipulated by your computation routine
is entirely separate from that used by the I/O routines, then it's safe to do
whatever you need to in the signal handler.

This program performs the same functionality as server.c; it accepts a
connection from a single client at a time, reads some data from it, and when
the client disconnects it waits for a new connection. The difference however
is that while it's waiting for a connection or more data to become available,
it spends its time finding prime numbers. The prime number routine is totally
unaware that the process is also acting as a server.

To accomplish this, the listening socket and all client sockets are set up
in asynchronous mode. This tells the kernel that when data becomes available
on these sockets it should send a SIGIO to the process. Inside the signal
handler, the usual things are done to accept connections or perform a read.
The client sockets are also used in non-blocking mode, so if some data has
been read but there is no more yet available, the program can go back to
computing prime numbers until another SIGIO is received.

The program is run as follows:

  ./sigio <port>

*/

#include <fcntl.h>
#include <errno.h>
#include <netdb.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/types.h>

#define BACKLOG 10
#define START_THRESHOLD 2000000

int clientsock = -1;
int listensock = -1;

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

/* The prime number computation routine. It doesn't actually make any difference
   to the I/O stuff what happens here, as long as they don't (carelessly) share
   any data structures. This routine is designed to keep the CPU busy, and it
   prints output occasionally when it finds a prime. The algorithm is a
   deliberately inefficient way of finding primes, and the reason for starting
   at the high threshold is so that the output produced from here only appeares
   occasionally. Depending in your CPU speed, you may need to adjust the
   threshold so that you can see the SIGIO output in between the prime numbers
   appearing. */
void findprimes()
{
  int i = START_THRESHOLD;
  while (1) {
    int j;
    int ndivisors = 0;
    for (j = 2; j < i; j++)
      if (0 == (i % j))
        ndivisors++;
    if (0 == ndivisors)
      printf("%d is prime\n",i);
    i++;
  }
}

/* Put the socket into non-blocking mode. This is done by using the fcntl()
   system call to set the O_NONBLOCK flag. Although in practice it's unlikely
   that these calls could return errors, we check the results just to be sure,
   at the cost of slightly more verbose code. */
int set_non_blocking(int fd)
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

/* Put the socket into asynchronous mode. As with set_non_blocking(), this sets
   a flag using fcntl(). However it also makes an additional fcntl() call to
   set the "owner" of the socket (i.e. this process), which the kernal will
   notify when data becomes available. */
int set_async(int fd)
{
  int flags;
  if (0 > (flags = fcntl(fd,F_GETFL))) {
    perror("fcntl(F_GETFL)");
    return -1;
  }

  flags |= O_ASYNC;

  if (0 > fcntl(fd,F_SETFL,flags)) {
    perror("fcntl(F_SETFL)");
    return -1;
  }

  if (0 > fcntl(fd,F_SETOWN,getpid())) {
    perror("fcntl(F_SETOWN)");
    return -1;
  }

  return 0;
}

/* SIGIO handler. This is called whenever the SIGIO signal is sent to the
   process, and is responsible for dealing with any pending I/O operations.
   Note that this could be called at *any* point during the computation routine,
   however in this case we don't share any data between the two parts of the
   program so we can safely do I/O here.

   Note: One potential thing to watch out for is the use of the standard output
   stream, which by default is buffered. Potentially there could be problems if
   the computation routine was party way through printing to the buffer and
   we also tried to write to it here. But buffering is turned off for standard
   output by the setbuf() call in main(), so it should not be a problem in this
   case - the worst thing that could happen is that the output from the SIGIO
   handler may appearn party way through the output from the computation
   routine. */
void sigio(int sig)
{
  printf("\nSIGIO\n");

  if (0 > clientsock) {
    /* If there is no client socket currently it means we're waiting for
       a connection. The SIGIO must have been sent due to a connection request
       being received for the socket. */
    if (0 > (clientsock = accept_connection(listensock)))
      return;
    if ((0 > set_async(clientsock)) || (0 > set_non_blocking(clientsock))) {
      close(clientsock);
      clientsock = -1;
      printf("\n");
      return;
    }
  }

  if (0 <= clientsock) {
    /* Now that we have a client socket, read all the data we can from it.
       Eventually we'll get to a point where there's no more data available
       (EAGAIN will be returned) and we can return from the signal handler.
       The program will go back to the computation routine. */
    int r;
    char buf[1024];
    while (0 < (r = read(clientsock,buf,1024)))
      write(STDOUT_FILENO,buf,r);
    if (0 > r) {
      if (EAGAIN == errno) {
        /* No more data available yet */
        /* Note: it may initially seem that there is a race condition where
           I/O could become available right at this point, before we return.
           However, in this case another SIGIO will be delivered and this
           function will be called again straight away after returning */
        printf("\n");
        return;
      }
      else {
        /* An error occurred reading from the client... disconnect the
           client but keep the server running. Computation will continue
           until we get another connection request, or if there is already
           one pending then the signal handler will be invoked again after
           we return. */
        perror("read");
        close(clientsock);
        clientsock = -1;
        printf("\n");
        return;
      }
    }
    printf("Client closed connection\n");
    close(clientsock);
    clientsock = -1;
  }

  printf("\n");
}

int main(int argc, char **argv)
{
  int port;

  /* Turn off standard output buffering. This means that any calls to printf()
     and friends will get written straight through to the underlying file
     descriptor, avoiding the possibility of the buffer being accessed in an
     inconsistent state due to the signal handler being called while the
     computation routine is inside printf().

     I usually do this anyway so if the program crashes, the last output
     just before the crash will be visible (i.e. not still in the buffer).  */
  setbuf(stdout,NULL);

  if (2 > argc) {
    fprintf(stderr,"Usage: sigio <port>\n");
    return -1;
  }

  /* Set up the SIGIO handler */
  signal(SIGIO,sigio);

  /* Start listening for client connections (but don't accept any yet) */
  port = atoi(argv[1]);
  if (0 > (listensock = start_listening(port)))
    return -1;

  /* Set up the listening socket for asynchronous I/O. We'll get a SIGIO as
     soon as a client tries to connect. */
  if (0 > set_async(listensock))
    return -1;

  /* Call the main computation routine. In this example it executes
     indefinitely, so this function won't return. But the program will jump
     into the SIGIO handler as I/O events occur. */
  findprimes();

  return 0;
}
