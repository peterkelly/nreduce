/*
 * This file is part of the GridXSLT project
 * Copyright (C) 2006 Peter Kelly (pmk@cs.adelaide.edu.au)
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
 * $Id: util.c 339 2006-09-21 01:18:34Z pmkelly $
 *
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "src/nreduce.h"
#include "compiler/gcode.h"
#include "network.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <assert.h>
#include <ctype.h>
#include <stdarg.h>
#include <math.h>
#include <netdb.h>
#include <netinet/tcp.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <unistd.h>
#include <errno.h>
#include <fcntl.h>

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
  addr.sin_addr = *((struct in_addr*)he->h_addr_list[0]);
  memset(&addr.sin_zero,0,8);

  if (-1 == (connect(sockfd,(struct sockaddr*)&addr,sizeof(struct sockaddr)))) {
    close(sockfd);
    perror("connect");
    return -1;
  }
  setsockopt(sockfd,SOL_TCP,TCP_NODELAY,&yes,sizeof(int));

  return sockfd;
}

int start_listening(struct in_addr ip, int port)
{
  int yes = 1;
  struct sockaddr_in local_addr;
  int listenfd;

  if (-1 == (listenfd = socket(AF_INET,SOCK_STREAM,0))) {
    perror("socket");
    return -1;
  }

  if (-1 == setsockopt(listenfd,SOL_SOCKET,SO_REUSEADDR,&yes,sizeof(int))) {
    perror("setsockopt");
    return -1;
  }

  local_addr.sin_family = AF_INET;
  local_addr.sin_port = htons(port);
  local_addr.sin_addr = ip;
  memset(&local_addr.sin_zero,0,8);

  if (-1 == bind(listenfd,(struct sockaddr*)&local_addr,sizeof(struct sockaddr))) {
    perror("bind");
    return -1;
  }

  if (-1 == listen(listenfd,LISTEN_BACKLOG)) {
    perror("listen");
    return -1;
  }

  return listenfd;
}

int start_listening_host(const char *host, int port)
{
  struct hostent *he = gethostbyname(host);
  struct in_addr ip;
  if (NULL == he) {
    fprintf(stderr,"%s: %s\n",host,hstrerror(h_errno));
    return -1;
  }

  if (4 != he->h_length) {
    fprintf(stderr,"%s: unknown address type (length %d)\n",host,he->h_length);
    return -1;
  }

  ip = *((struct in_addr*)he->h_addr_list[0]);
  return start_listening(ip,port);
}

int accept_connection(int sockfd)
{
  struct sockaddr_in remote_addr;
  int sin_size = sizeof(struct sockaddr_in);
  int clientfd;
  int yes = 1;
  if (-1 == (clientfd = accept(sockfd,(struct sockaddr*)&remote_addr,&sin_size))) {
    perror("accept");
    return -1;
  }
  setsockopt(clientfd,SOL_TCP,TCP_NODELAY,&yes,sizeof(int));
  return clientfd;
}

int parse_address(const char *address, char **host, int *port)
{
  const char *colon = strchr(address,':');
  if (NULL == colon)
    return -1;
  *host = (char*)malloc(colon-address+1);
  memcpy(*host,address,colon-address);
  (*host)[colon-address] = '\0';
  *port = atoi(colon+1);
  return 0;
}

array *read_hostnames(const char *hostsfile)
{
  array *hostnames;
  int start = 0;
  int pos = 0;
  array *filedata;
  FILE *f;
  int r;
  char buf[1024];

  if (NULL == (f = fopen(hostsfile,"r"))) {
    perror(hostsfile);
    return NULL;
  }

  filedata = array_new(1);
  while (0 < (r = fread(buf,1,1024,f)))
    array_append(filedata,buf,r);
  fclose(f);

  hostnames = array_new(sizeof(char*));
  while (1) {
    if ((pos == filedata->nbytes) ||
        ('\n' == filedata->data[pos])) {
      if (pos > start) {
        char *hostname = (char*)malloc(pos-start+1);
        memcpy(hostname,&filedata->data[start],pos-start);
        hostname[pos-start] = '\0';
        array_append(hostnames,&hostname,sizeof(char*));
      }
      start = pos+1;
    }
    if (pos == filedata->nbytes)
      break;
    pos++;
  }

  free(filedata);
  return hostnames;
}

nodeinfo *nodeinfo_init(const char *hostsfile)
{
  array *hostnames;
  int i;
  int r = 0;
  nodeinfo *ni;

  if (NULL == (hostnames = read_hostnames(hostsfile)))
    return NULL;

  ni = (nodeinfo*)calloc(1,sizeof(nodeinfo));
  ni->listenfd = -1;
  ni->nworkers = array_count(hostnames);
  ni->workers = calloc(ni->nworkers,sizeof(workerinfo));

  for (i = 0; (i < ni->nworkers) && (0 == r); i++) {
    ni->workers[i].hostname = array_item(hostnames,i,char*);
    ni->workers[i].ip.s_addr = 0;
    ni->workers[i].sock = -1;
    ni->workers[i].pid = -1;
    ni->workers[i].readfd = -1;
  }

  for (i = 0; (i < ni->nworkers) && (0 == r); i++) {
    struct hostent *he = gethostbyname(ni->workers[i].hostname);
    if (NULL == he) {
      fprintf(stderr,"%s: %s\n",ni->workers[i].hostname,hstrerror(h_errno));
      r = -1;
    }
    else if (4 != he->h_length) {
      fprintf(stderr,"%s: %s\n",ni->workers[i].hostname,hstrerror(h_errno));
      r = -1;
    }
    else {
      ni->workers[i].ip = *((struct in_addr*)he->h_addr_list[0]);
    }
  }

  if (0 != r) {
    nodeinfo_free(ni);
    return NULL;
  }

  return ni;
}

void nodeinfo_free(nodeinfo *ni)
{
  int i;
  for (i = 0; i < ni->nworkers; i++) {
    free(ni->workers[i].hostname);
    if (0 <= ni->workers[i].sock)
      close(ni->workers[i].sock);
    if (0 <= ni->workers[i].readfd)
      close(ni->workers[i].readfd);
  }
  if (0 <= ni->listenfd)
    close(ni->listenfd);
  free(ni->workers);
  free(ni);
}

int wait_for_connections(nodeinfo *ni)
{
  int remaining = ni->nworkers;
  while (0 < remaining) {
    struct sockaddr_in remote_addr;
    int sin_size = sizeof(struct sockaddr_in);
    int clientfd;
    int yes = 1;
    int i;
    int found = 0;


/*     printf("accept: before select\n"); */
/*     while (1) { */
/*       fd_set readfds; */
/*       fd_set writefds; */
/*       fd_set exceptfds; */
/*       int r; */

/*       FD_ZERO(&readfds); */
/*       FD_ZERO(&writefds); */
/*       FD_ZERO(&exceptfds); */

/*       FD_SET(ni->listenfd,&readfds); */
/*       FD_SET(ni->listenfd,&writefds); */
/*       FD_SET(ni->listenfd,&exceptfds); */

/*       r = select(ni->listenfd+1,&readfds,&writefds,&exceptfds,NULL); */
/*       printf("select() returned %d\n",r); */

/*       if (0 <= r) { */
/*         if (FD_ISSET(ni->listenfd,&readfds)) */
/*           printf("read is set\n"); */
/*         if (FD_ISSET(ni->listenfd,&writefds)) */
/*           printf("write is set\n"); */
/*         if (FD_ISSET(ni->listenfd,&exceptfds)) */
/*           printf("except is set\n"); */
/*       } */
/*     } */
/*     printf("accept: after select\n"); */



    if (0 > (clientfd = accept(ni->listenfd,(struct sockaddr*)&remote_addr,&sin_size))) {
      perror("accept");
      return -1;
    }
    setsockopt(clientfd,SOL_TCP,TCP_NODELAY,&yes,sizeof(int));

    for (i = 0; i < ni->nworkers; i++) {
      if (!memcmp(&ni->workers[i].ip,&remote_addr.sin_addr,sizeof(struct in_addr))) {

        if (0 <= ni->workers[i].sock) {
          fprintf(stderr,"Got connection from %s but it's already connected!\n",
                  ni->workers[i].hostname);
          return -1;
        }
        else {
          ni->workers[i].sock = clientfd;
          printf("Got connection from %s\n",ni->workers[i].hostname);
          found = 1;
          remaining--;
        }
      }
    }

    if (!found) {
      unsigned char *ip = (unsigned char*)&remote_addr.sin_addr;
      fprintf(stderr,"Got connection from unknown host %d.%d.%d.%d\n",
              ip[0],ip[1],ip[2],ip[3]);
      return -1;
    }
  }
  printf("all connected!\n");
  return 0;
}

int fdsetflag(int fd, int flag, int on)
{
  int flags;
  if (0 > (flags = fcntl(fd,F_GETFL))) {
    perror("fcntl(F_GETFL)");
    return -1;
  }

  if (on)
    flags |= flag;
  else
    flags &= ~flag;

  if (0 > fcntl(fd,F_SETFL,flags)) {
    perror("fcntl(F_SETFL)");
    return -1;
  }
  return 0;
}
