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
 * $Id$
 *
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "src/nreduce.h"
#include "compiler/bytecode.h"
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
  if (0 > setsockopt(sockfd,SOL_TCP,TCP_NODELAY,&yes,sizeof(int))) {
    perror("setsockopt TCP_NODELAY");
    return -1;
  }

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

int fdsetblocking(int fd, int blocking)
{
  return fdsetflag(fd,O_NONBLOCK,!blocking);
}

void print_ip(FILE *f, struct in_addr ip)
{
  unsigned char *addrbytes = (unsigned char*)&ip.s_addr;
  fprintf(f,"%u.%u.%u.%u",addrbytes[0],addrbytes[1],addrbytes[2],addrbytes[3]);
}

void print_taskid(FILE *f, taskid id)
{
  print_ip(f,id.nodeip);
  fprintf(f,":%d %d",id.nodeport,id.localid);
}
