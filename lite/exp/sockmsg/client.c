#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <netdb.h>
#include <netinet/tcp.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

#define check(e)  \
  if (!(e)) { fprintf(stderr,"%s:%u: %s\n%s\n",__FILE__,__LINE__,#e,strerror(errno)); exit(1); }
#define hcheck(e)  \
  if (!(e)) { fprintf(stderr,"%s:%u: %s\n%s\n",__FILE__,__LINE__,#e,hstrerror(h_errno)); exit(1); }

int open_connection(const char *hostname, int port)
{
  struct hostent *he;
  struct sockaddr_in addr;
  int sock;
  int yes = 1;
  int flags;
  hcheck(NULL != (he = gethostbyname(hostname)));
  check(0 <= (sock = socket(AF_INET,SOCK_STREAM,0)));
  addr.sin_family = AF_INET;
  addr.sin_port = htons(port);
  addr.sin_addr = *((struct in_addr*)he->h_addr);
  memset(&addr.sin_zero,0,8);
  check(0 == connect(sock,(struct sockaddr*)&addr,sizeof(struct sockaddr)));
  check(-1 != (flags = fcntl(sock,F_GETFL)));
  check(0 == fcntl(sock,F_SETFL,flags|O_NONBLOCK));
  check(0 == setsockopt(sock,IPPROTO_TCP,TCP_NODELAY,&yes,sizeof(int)));
  return sock;
}

int main(int argc, char **argv)
{
  char *hostname;
  int port;
  int bufsize;
  int sock;
  int insize = 0;
  int reading = 1;
  int receiving = 1;
  char *indata;
  int shut = 0;

  setbuf(stdout,NULL);
  signal(SIGPIPE,SIG_IGN);

  if (4 > argc) {
    fprintf(stderr,"Usage: client <hostname> <port> <bufsize>\n");
    return -1;
  }

  hostname = argv[1];
  port = atoi(argv[2]);
  bufsize = atoi(argv[3]);

  check (0 <= (sock = open_connection(hostname,port)));

  indata = malloc(bufsize);
  while (reading || receiving || (0 < insize)) {
    fd_set readfds;
    fd_set writefds;
    int n;

    FD_ZERO(&readfds);
    FD_ZERO(&writefds);
    FD_SET(sock,&readfds);

    if (0 < insize)
      FD_SET(sock,&writefds);
    else if (reading)
      FD_SET(STDIN_FILENO,&readfds);

    check(1 <= select(sock+1,&readfds,&writefds,NULL,NULL));

    if (FD_ISSET(sock,&writefds)) {
      check(0 <= (n = write(sock,indata,insize)));
      memmove(&indata[0],&indata[n],insize-n);
      insize -= n;
    }

    if (FD_ISSET(STDIN_FILENO,&readfds)) {
      assert(0 == insize);
      check(0 <= (n = read(STDIN_FILENO,indata,bufsize)));
      if (0 == n)
        reading = 0;
      else
        insize = n;
    }

    if (FD_ISSET(sock,&readfds)) {
      char buf[bufsize];
      check(0 <= (n = read(sock,buf,bufsize)));
      if (0 == n)
        receiving = 0;
      else
        fwrite(buf,1,n,stdout);
    }

    if (!reading && (0 == insize) && !shut) {
      check(0 == shutdown(sock,SHUT_WR));
      shut = 1;
    }
  }

  free(indata);
  close(sock);
  return 0;
}
