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

#include "Network.h"
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>
#include <errno.h>
#include <netdb.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/select.h>
#include <netinet/in.h>
#include <netinet/tcp.h>

using namespace GridXSLT;

#define BACKLOG 10

void fdhandler_free(eventman *em, fdhandler *h)
{
  if (h->cleanup)
    h->cleanup(em,h);
  free(h);
}

void eventman_add_handler(eventman *em, fdhandler *h)
{
  list_append(&em->handlers,h);
}

void eventman_clear(eventman *em)
{
  list *l;
  for (l = em->handlers; l; l = l->next) {
    fdhandler *h = (fdhandler*)l->data;
    close(h->fd);
    fdhandler_free(em,h);
  }
  list_free(em->handlers,NULL);
}

void eventman_loop(eventman *em)
{
  while (!em->stopped) {
    fd_set readfds;
    fd_set writefds;
    list *l;
    list **lptr;
    int reading = 0;
    int writing = 0;
    int highest = -1;
    int r;

    FD_ZERO(&readfds);
    FD_ZERO(&writefds);

    for (l = em->handlers; l; l = l->next) {
      fdhandler *h = (fdhandler*)l->data;

      if (h->reading) {
        FD_SET(h->fd,&readfds);
        reading++;
      }

      if (h->writing) {
        FD_SET(h->fd,&writefds);
        writing++;
      }

      if ((-1 == highest) || ((h->reading || h->writing) && (highest < h->fd)))
        highest = h->fd;
    }

    if ((0 == reading) && (0 == writing))
      break;

    if (0 > (r = select(highest+1,&readfds,&writefds,NULL,NULL))) {
      perror("select");
      exit(1);
    }

    lptr = &em->handlers;
    while (*lptr) {
      fdhandler *h = (fdhandler*)(*lptr)->data;
      int doclose = 0;

      if (h->reading && FD_ISSET(h->fd,&readfds))
        doclose = h->readhandler(em,h);

      if (h->writing && FD_ISSET(h->fd,&writefds) && !doclose)
        doclose = h->writehandler(em,h);

      if (doclose) {
        list *old = *lptr;
        close(h->fd);
        fdhandler_free(em,h);
        *lptr = (*lptr)->next;
        free(old);
      }
      else {
        lptr = &((*lptr)->next);
      }
    }
  }
}











int connect_host(const char *hostname, int port)
{
  int sockfd;
  struct hostent *he;
  struct sockaddr_in addr;
  int yes = 1;

  if (0 == (he = gethostbyname(hostname))) {
    perror("gethostbyname");
    return -1;
  }

  if (-1 == (sockfd = socket(AF_INET,SOCK_STREAM,0))) {
    perror("socket");
    return -1;
  }

  addr.sin_family = AF_INET;
  addr.sin_port = htons(port);
  addr.sin_addr = *((struct in_addr*)he->h_addr);
  memset(&addr.sin_zero,0,8);

  if (-1 == (connect(sockfd,(struct sockaddr*)&addr,sizeof(struct sockaddr)))) {
    close(sockfd);
    perror("connect");
    return -1;
  }
  setsockopt(sockfd,SOL_TCP,TCP_NODELAY,&yes,sizeof(int));

  return sockfd;
}

int start_server(int port)
{
  int sockfd;
  int yes = 1;
  struct sockaddr_in local_addr;

  if (-1 == (sockfd = socket(AF_INET,SOCK_STREAM,0))) {
    perror("socket");
    return -1;
  }

  if (-1 == setsockopt(sockfd,SOL_SOCKET,SO_REUSEADDR,&yes,sizeof(int))) {
    perror("setsockopt");
    close(sockfd);
    return -1;
  }

  local_addr.sin_family = AF_INET;
  local_addr.sin_port = htons(port);
  local_addr.sin_addr.s_addr = INADDR_ANY;
  memset(&local_addr.sin_zero,0,8);

  if (-1 == bind(sockfd,(struct sockaddr*)&local_addr,sizeof(struct sockaddr))) {
    perror("bind");
    close(sockfd);
    return -1;
  }

  if (-1 == listen(sockfd,BACKLOG)) {
    perror("listen");
    close(sockfd);
    return -1;
  }

  return sockfd;
}

