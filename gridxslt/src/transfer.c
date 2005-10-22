/*
 * This file is part of the GridXSLT project
 * Copyright (C) 2005 Peter Kelly (pmk@cs.adelaide.edu.au)
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
 */

#include "util/list.h"
#include "util/stringbuf.h"
#include "util/debug.h"
#include "util/network.h"
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>
#include <errno.h>
#include <netdb.h>
#include <stdarg.h>
#include <assert.h>
#include <ctype.h>
#include <dirent.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/time.h>
#include <sys/stat.h>
#include <time.h>
#include <netinet/tcp.h>
#include <libxml/uri.h>

int timediff(struct timeval start, struct timeval end)
{
  int sdiff = end.tv_sec - start.tv_sec;
  int usdiff = end.tv_usec - start.tv_usec;
  int msdiff;
  if (0 > usdiff) {
    sdiff--;
    usdiff += 1000000;
  }
  msdiff = usdiff/1000;
  return sdiff*1000 + msdiff;
}

int kbsec(int bytes, int ms)
{
  double kb = ((double)bytes)/1024.0;
  double s = ((double)ms)/1000.0;
  return (int)(kb/s);
}

void server(const char *filename, int max)
{
  int fd;
  int sock;
  struct sockaddr_in remote_addr;
  int sin_size = sizeof(struct sockaddr_in);
  int serverfd;
  char buf[BUFSIZE];
  int r;
  struct timeval start;
  struct timeval end;
  int bytes = 0;
  int ms;

  if (0 > (fd = open(filename,O_RDONLY))) {
    perror(filename);
    exit(1);
  }

  if (0 > (sock = start_server(1080)))
    exit(1);

  printf("Listening\n");

  if (-1 == (serverfd = accept(sock,(struct sockaddr*)&remote_addr,(socklen_t*)&sin_size))) {
    perror("accept");
    exit(1);
  }

  printf("Got connection\n");

  gettimeofday(&start,NULL);

  while (0 < (r = read(fd,buf,BUFSIZE))) {
    write(serverfd,buf,r);
    bytes += r;
/*     printf("%dkb\n",bytes/1024); */
    if ((0 <= max) && (bytes >= max*1024*1024))
      break;
  }

  gettimeofday(&end,NULL);

  ms = timediff(start,end);
  printf("Sent %d bytes in %dms, transfer rate = %dkb/sec\n",bytes,ms,kbsec(bytes,ms));

  close(serverfd);
  close(fd);
}

void client(const char *filename)
{
  int fd;
  int clientfd;
  int r;
  char buf[BUFSIZE];
  struct timeval start;
  struct timeval end;
  int bytes = 0;
  int ms;

  if (0 > (fd = open(filename,O_WRONLY|O_CREAT|O_TRUNC,0666))) {
    perror(filename);
    exit(1);
  }

  if (0 > (clientfd = connect_host("localhost",1080)))
    exit(1);

  printf("Got connection\n");

  gettimeofday(&start,NULL);

  while (0 < (r = read(clientfd,buf,BUFSIZE))) {
    write(fd,buf,r);
    bytes += r;
/*     printf("%dkb\n",bytes/1024); */
  }
  gettimeofday(&end,NULL);

  ms = timediff(start,end);
  printf("Received %d bytes in %dms, transfer rate = %dkb/sec\n",bytes,ms,kbsec(bytes,ms));

  close(clientfd);
  close(fd);
}

int main(int argc, char **argv)
{
  int max = -1;
  setbuf(stdout,NULL);

  if (3 > argc) {
    fprintf(stderr,"Usage: transfer <c|s> filename\n");
    exit(1);
  }

  if (4 <= argc)
    max = atoi(argv[3]);

  if (!strcmp(argv[1],"s")) {
    server(argv[2],max);
  }
  else if (!strcmp(argv[1],"c")) {
    client(argv[2]);
  }
  else {
    fprintf(stderr,"Must specify client or server!\n");
    exit(1);
  }

  return 0;
}
