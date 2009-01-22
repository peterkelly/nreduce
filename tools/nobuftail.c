/*
 * This file is part of the nreduce project
 * Copyright (C) 2006-2009 Peter Kelly (pmk@cs.adelaide.edu.au)
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 *
 * $Id: runtime.h 849 2009-01-22 01:01:00Z pmkelly $
 *
 */

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
