/*
 * This file is part of the nreduce project
 * Copyright (C) 2006-2007 Peter Kelly (pmk@cs.adelaide.edu.au)
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
 * $Id: node.c 610 2007-08-22 06:07:12Z pmkelly $
 *
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#define NODE_C

#include "src/nreduce.h"
#include "node.h"
#include "messages.h"
#include "netprivate.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <assert.h>
#include <ctype.h>
#include <stdarg.h>
#include <math.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/select.h>
#include <sys/time.h>
#include <time.h>
#include <fcntl.h>
#include <unistd.h>
#include <signal.h>

int set_non_blocking(int fd)
{
  int flags;
  if (0 > (flags = fcntl(fd,F_GETFL))) {
    perror("fcntl(F_GETFL)");
    return -1;
  }

  flags |= O_NONBLOCK;

  if (0 > fcntl(fd,F_SETFL,flags)) {
    perror("fcntl(F_SETFL)");
    return -1;
  }
  return 0;
}

char *lookup_hostname(node *n, in_addr_t addr)
{
  int alloc = 1024;
  char *buf = malloc(alloc);
  struct hostent *he = NULL;
  int herr = 0;
  char *res;

#ifdef HAVE_GETHOSTBYNAME_R
  struct hostent ret;
  while ((0 != gethostbyaddr_r(&addr,sizeof(in_addr_t),AF_INET,&ret,buf,alloc,&he,&herr)) &&
         (ERANGE == errno))
    buf = realloc(buf,alloc *= 2);
#else
  he = gethostbyaddr(&addr,sizeof(in_addr_t),AF_INET);
  herr = h_errno;
#endif

  if (NULL != he) {
    res = strdup(he->h_name);
  }
  else {
    unsigned char *c = (unsigned char*)&addr;
    char *hostname = (char*)malloc(100);
    sprintf(hostname,"%u.%u.%u.%u",c[0],c[1],c[2],c[3]);
    res = hostname;
  }

  free(buf);
  return res;
}

int lookup_address(node *n, const char *host, in_addr_t *out, int *h_errout)
{
  int alloc = 1024;
  char *buf = malloc(alloc);
  struct hostent *he = NULL;
  int herr = 0;
  int r = 0;

#ifdef HAVE_GETHOSTBYNAME_R
  struct hostent ret;
  while ((0 != gethostbyname_r(host,&ret,buf,alloc,&he,&herr)) &&
         (ERANGE == errno))
    buf = realloc(buf,alloc *= 2);
#else
  he = gethostbyname(host);
  herr = h_errno;
#endif

  if (NULL == he) {
    if (h_errout)
      *h_errout = herr;
    node_log(n,LOG_WARNING,"%s: %s",host,hstrerror(herr));
    r = -1;
  }
  else if (4 != he->h_length) {
    if (h_errout)
      *h_errout = HOST_NOT_FOUND;
    node_log(n,LOG_WARNING,"%s: invalid address type",host);
    r = -1;
  }
  else {
    *out = (*((struct in_addr*)he->h_addr)).s_addr;
  }

  free(buf);
  return r;
}

void determine_ip(node *n)
{
  in_addr_t addr = 0;
  char hostname[4097];
  FILE *hf = popen("hostname","r");
  int size;
  if (NULL == hf) {
    fprintf(stderr,
            "Could not determine IP address, because executing the hostname command "
            "failed (%s)\n",strerror(errno));
    exit(1);
  }
  size = fread(hostname,1,4096,hf);
  if (0 > hf) {
    fprintf(stderr,
            "Could not determine IP address, because reading output from the hostname "
            "command failed (%s)\n",strerror(errno));
    pclose(hf);
    exit(1);
  }
  pclose(hf);
  if ((0 < size) && ('\n' == hostname[size-1]))
    size--;
  hostname[size] = '\0';

  if (0 > lookup_address(n,hostname,&addr,NULL)) {
    fprintf(stderr,
            "Could not determine IP address, because the address \"%s\" "
            "returned by the hostname command could not be resolved (%s)\n",
            hostname,strerror(errno));
    exit(1);
  }
  n->listenip = addr;
}

#ifdef DEBUG_SHORT_KEEPALIVE
int set_keepalive(node *n, int sock, int s)

{
  int one = 1;
  if (0 > setsockopt(sock,IPPROTO_TCP,TCP_KEEPIDLE,&s,sizeof(int))) {
    node_log(n,LOG_ERROR,"setsockopt(TCP_KEEPIDLE): %s",strerror(errno));
    return -1;
  }

  if (0 > setsockopt(sock,IPPROTO_TCP,TCP_KEEPINTVL,&s,sizeof(int))) {
    node_log(n,LOG_ERROR,"setsockopt(TCP_KEEPINTVL): %s",strerror(errno));
    return -1;
  }

  if (0 > setsockopt(sock,IPPROTO_TCP,TCP_KEEPCNT,&one,sizeof(int))) {
    node_log(n,LOG_ERROR,"setsockopt(TCP_KEEPCNT): %s",strerror(errno));
    return -1;
  }

  if (0 > setsockopt(sock,SOL_SOCKET,SO_KEEPALIVE,&one,sizeof(int))) {
    node_log(n,LOG_ERROR,"setsockopt(SO_KEEPALIVE): %s",strerror(errno));
    return -1;
  }

  node_log(n,LOG_DEBUG2,"Set connection keepalive interval to %ds",s);

  return 0;
}
#endif
