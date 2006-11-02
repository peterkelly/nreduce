#include <stdio.h>
#include <time.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <string.h>
#include <stdlib.h>

#define BUFSIZE 1024

void delay()
{
  struct timespec time;
  time.tv_sec = 0;
  time.tv_nsec = 100*1000*1000; /* 0.1 seconds */
  nanosleep(&time,NULL);
}

int get_size(const char *filename)
{
  struct stat statbuf;
  if (0 != stat(filename,&statbuf)) {
    perror("fstat");
    exit(1);
  }

  return statbuf.st_size;
}

int main(int argc, char **argv)
{
  int r;
  char buf[BUFSIZE];
  char *filename;
  int fd;
  int oldsize = -1;

  if (2 > argc) {
    fprintf(stderr,"Usage: nobuftail <filename>\n");
    return 1;
  }

  filename = argv[1];

  while (1) {
    int total = 0;

    if (0 > (fd = open(filename,O_RDONLY))) {
      perror(filename);
      return 1;
    }

    fcntl(fd,F_SETFD,fcntl(fd,F_GETFD) | O_NONBLOCK);

    while (1) {
      if (total > get_size(filename)) {
        char s[1024];
        sprintf(s,"file truncated\n");
        write(STDOUT_FILENO,s,strlen(s));
        break;
      }

      r = read(fd,buf,BUFSIZE);
/*       printf("r = %d\n",r); */

      if (0 < r) {
        write(STDOUT_FILENO,buf,r);
        total += r;
      }
      else if (0 == r) {
        delay();
      }
      else {
        delay();
      }
    }
    close(fd);
    oldsize = total;

    while (total == get_size(filename))
      delay();
    printf("Reloading file\n");
  }

  return 0;
}
