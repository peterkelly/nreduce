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
#include "util/network.h"
#include "util/String.h"
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
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/time.h>
#include <netinet/tcp.h>

using namespace GridXSLT;

#define WRITESIZE 1024
#define READSIZE 64

int main(int argc, char **argv)
{
  int listenfd;
  int clientfd;
  int serverfd = -1;
  int wtotal = 0;
  int rtotal = 0;
  int index = 0;

  setbuf(stdout,NULL);

  if (0 > (listenfd = start_server(1080)))
    exit(1);

  message("listenfd = %d\n",listenfd);

  message("before connect\n");
  clientfd = connect_host("localhost",1080);
  message("after connect, clientfd = %d\n",clientfd);
  fcntl(clientfd,F_SETFL,fcntl(clientfd,F_GETFL)|O_NONBLOCK);

  while (1) {
    fd_set readfds;
    fd_set writefds;
    int highest;
    int r;

    FD_ZERO(&readfds);
    FD_ZERO(&writefds);

    FD_SET(clientfd,&readfds);
    highest = clientfd;

    FD_SET(listenfd,&readfds);
    if (highest < listenfd)
      highest = listenfd;

    if (0 <= serverfd) {
      FD_SET(serverfd,&writefds);
      if (highest < serverfd)
        highest = serverfd;
    }

    if (0 > (r = select(highest+1,&readfds,&writefds,NULL,NULL))) {
      perror("select");
      exit(1);
    }

    if (FD_ISSET(listenfd,&readfds)) {
      if (0 <= serverfd) {
        fmessage(stderr,"Already have a connection\n");
        exit(1);
      }
      struct sockaddr_in remote_addr;
      int sin_size = sizeof(struct sockaddr_in);
      if (-1 == (serverfd = accept(listenfd,(struct sockaddr*)&remote_addr,(socklen_t*)&sin_size))) {
        perror("accept");
        return -1;
      }
      fcntl(serverfd,F_SETFL,fcntl(serverfd,F_GETFL)|O_NONBLOCK);

      message("Got connection\n");
    }

    if (FD_ISSET(serverfd,&writefds)) {
      char buf[WRITESIZE];
      int wr = write(serverfd,buf,WRITESIZE);
      wtotal += wr;
/*       message("Wrote %d bytes, wtotal = %d, difference = %d\n",wr,wtotal,wtotal-rtotal); */
      message("%d %d\n",index++,wtotal-rtotal);
    }

    if (FD_ISSET(clientfd,&readfds)) {
      char buf[READSIZE];
      int rr = read(clientfd,buf,READSIZE);
      rtotal += rr;
      message("%d %d\n",index++,wtotal-rtotal);
/*       if (0 > rr) { */
/*         message("client read: %d %s (EAGAIN=%d)\n",errno,strerror(errno),EAGAIN); */
/*       } */
/*       else { */
/*         message("Read %d bytes, rtotal = %d, difference = %d\n",rr,rtotal,wtotal-rtotal); */
/*       } */
    }

    if (10000 <= index)
      exit(0);
  }

  return 0;
}

