/*
 * This file is part of the nreduce project
 * Copyright (C) 2008-2009 Peter Kelly (pmk@cs.adelaide.edu.au)
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

int open_connection(const char *hostname, int port)
{
  int sockfd;
  struct hostent *he;
  struct sockaddr_in addr;

  if (NULL == (he = gethostbyname(hostname))) {
    herror("gethostbyname");
    return -1;
  }

  if (0 > (sockfd = socket(AF_INET,SOCK_STREAM,0))) {
    perror("socket");
    return -1;
  }

  addr.sin_family = AF_INET;
  addr.sin_port = htons(port);
  addr.sin_addr = *((struct in_addr*)he->h_addr);
  memset(&addr.sin_zero,0,8);

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
  int maxtries = -1;
  int ntries = 0;

  setbuf(stdout,NULL);

  if (3 > argc) {
    fprintf(stderr,"Usage: client <host> <port> [maxtries]\n");
    return -1;
  }

  hostname = argv[1];
  port = atoi(argv[2]);
  if (4 <= argc) {
    maxtries = atoi(argv[3]);
  }

  printf("maxtries = %d\n",maxtries);

  /* Repeatedly try to connect to the server. Once the connection succeeds, exit
     successfully. */

  while (1) {
    printf("Attempting connection to %s:%d... ",hostname,port);
    /* Open a connection to the server */
    if (0 <= (sock = open_connection(hostname,port))) {
      printf("ok\n");
      close(sock);
      exit(0);
    }

    ntries++;
    if ((0 > maxtries) || (ntries < maxtries)) {
      sleep(1);
    }
    else {
      printf("aborting\n");
      exit(1);
    }
  }

  return 0;
}
