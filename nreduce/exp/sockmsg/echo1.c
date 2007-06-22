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

int start_listening(int port)
{
  struct sockaddr_in local_addr;
  int yes = 1;
  int sock;
  check(0 <= (sock = socket(AF_INET,SOCK_STREAM,0)));
  check(0 == setsockopt(sock,SOL_SOCKET,SO_REUSEADDR,&yes,sizeof(int)));
  local_addr.sin_family = AF_INET;
  local_addr.sin_port = htons(port);
  local_addr.sin_addr.s_addr = INADDR_ANY;
  memset(&local_addr.sin_zero,0,8);
  check(0 == bind(sock,(struct sockaddr*)&local_addr,sizeof(struct sockaddr)));
  check(0 == listen(sock,1));
  return sock;
}

int accept_connection(int ls)
{
  int sock;
  int flags;
  int yes = 1;
  check(0 <= (sock = accept(ls,NULL,0)));
  check(-1 != (flags = fcntl(sock,F_GETFL)));
  check(0 == fcntl(sock,F_SETFL,flags|O_NONBLOCK));
  check(0 == setsockopt(sock,IPPROTO_TCP,TCP_NODELAY,&yes,sizeof(int)));
  printf("Got connection\n");
  return sock;
}

int main(int argc, char **argv)
{
  int port;
  int listensock;
  int sock;
  int bufsize;

  setbuf(stdout,NULL);
  signal(SIGPIPE,SIG_IGN);

  if (3 > argc) {
    fprintf(stderr,"Usage: echo1 <port> <bufsize>\n");
    return -1;
  }

  port = atoi(argv[1]);
  bufsize = atoi(argv[2]);

  check(0 <= (listensock = start_listening(port)));
  printf("ready\n");

  while (0 <= (sock = accept_connection(listensock))) {
    int dalloc = bufsize;
    int dsize = 0;
    char *data = malloc(bufsize);
    int reading = 1;

    while (reading || (0 < dsize)) {

      fd_set readfds;
      fd_set writefds;
      int n;

      FD_ZERO(&readfds);
      FD_ZERO(&writefds);
      if (reading)
        FD_SET(sock,&readfds);
      if (0 < dsize)
        FD_SET(sock,&writefds);

      check(1 <= select(sock+1,&readfds,&writefds,NULL,NULL));

      if (FD_ISSET(sock,&writefds)) {
        check (0 <= (n = write(sock,data,dsize)));
        memmove(&data[0],&data[n],dsize-n);
        dsize -= n;
      }

      if (FD_ISSET(sock,&readfds)) {
        check (0 <= (n = read(sock,&data[dsize],bufsize)));
        if (0 == n) {
          reading = 0;
        }
        else {
          dsize += n;
          if (dalloc < dsize+bufsize) {
            dalloc = dsize+bufsize;
            data = realloc(data,dalloc);
          }
        }
      }
    }

    check(0 == close(sock));
    free(data);
    printf("Connection closed\n");
  }

  return 0;
}
