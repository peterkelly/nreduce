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

#include "compiler/bytecode.h"
#include "src/nreduce.h"
#include "network.h"
#include "runtime.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <assert.h>
#include <ctype.h>
#include <stdarg.h>
#include <math.h>
#include <errno.h>
#include <sys/types.h>
#include <unistd.h>
#include <signal.h>
#include <sys/wait.h>

pid_t launch_worker(struct in_addr ip, const char *dir, const char *cmd, const char *hostsfile,
                    const char *mhost, int mport,
                    int *outfd)
{
  int fds[2];
  pid_t pid;

  if (0 > pipe(fds)) {
    perror("pipe");
    return -1;
  }

  pid = fork();

  if (-1 == pid) {
    /* fork failed */
    perror("fork");
    close(fds[0]);
    close(fds[1]);
    return -1;
  }
  else if (0 == pid) {
    /* child process */
    char *ipstr;
    char *rshcmd;
    char *args[6];
    unsigned char *ipaddr = (char*)&ip.s_addr;
    int len;

    len = snprintf(NULL,0,"%d.%d.%d.%d",ipaddr[0],ipaddr[1],ipaddr[1],ipaddr[1]);
    ipstr = (char*)malloc(len+1);
    sprintf(ipstr,"%d.%d.%d.%d",ipaddr[0],ipaddr[1],ipaddr[2],ipaddr[3]);

    len = snprintf(NULL,0,"cd %s && %s -w %s:%d",dir,cmd,mhost,mport);
    rshcmd = (char*)malloc(len+1);
    sprintf(rshcmd,"cd %s && %s -w %s %s:%d",dir,cmd,hostsfile,mhost,mport);

    args[0] = "rsh";
    args[1] = ipstr;
    args[2] = rshcmd;
    args[3] = NULL;

    dup2(fds[1],STDOUT_FILENO);
    dup2(fds[1],STDERR_FILENO);
    close(fds[0]);

    execvp(args[0],args);
    printf("Can't execute %s: %s\n",args[0],strerror(errno));
    exit(-1);
  }
  else {
    close(fds[1]);
    *outfd = fds[0];
  }

  return pid;
}

int wait_for_startup(nodeinfo *ni)
{
  int count = ni->nworkers;
  int *done = (int*)calloc(count,sizeof(int));

  while (1) {
    int i;
    int highest = -1;
    int active = 0;
    fd_set fds;

    FD_ZERO(&fds);

    for (i = 0; i < count; i++) {
      int fd = ni->workers[i].readfd;
      if (!done[i]) {
        FD_SET(fd,&fds);
        if (highest < fd)
          highest = fd;
        active++;
      }
    }

    if (0 == active)
      break;

    if (0 > select(highest+1,&fds,NULL,NULL,NULL)) {
      perror("select");
      free(done);
      return -1;
    }

    for (i = 0; i < count; i++) {
      int fd = ni->workers[i].readfd;
      if (!done[i] && FD_ISSET(fd,&fds)) {
        char *whost = ni->workers[i].hostname;
        char c;
        int r = read(fd,&c,1);
        if (0 > r) {
          fprintf(stderr,"read() from %s: %s\n",whost,strerror(errno));
          return -1;
        }
        else if (0 == r) {
          fprintf(stderr,"read() from %s: no data\n",whost);
          return -1;
        }
        else if ('1' != c) {
          printf("%s: %c",whost,c);
          while (0 < (r = read(fd,&c,1)))
            printf("%c",c);
          return -1;
        }
        else {
          printf("%s: process started\n",whost);
        }
        done[i] = 1;
      }
    }
  }

  return 0;
}

int notify_startup(nodeinfo *ni)
{
  int i;
  for (i = 0; i < ni->nworkers; i++) {
    unsigned char c = i;
    int r;
    do {
      r = write(ni->workers[i].sock,&c,1);
    } while ((0 > r) && (EAGAIN == errno));
    if (0 > r) {
      fprintf(stderr,"write() to %s: %s\n",ni->workers[i].hostname,strerror(errno));
      return -1;
    }
  }
  return 0;
}

int master(const char *hostsfile, const char *myaddr, const char *filename, const char *cmd)
{
  char *host;
  int port;
  int r = 0;
  int i;
  nodeinfo *ni;
  char *dir;

  if (NULL == (dir = getcwd_alloc())) {
    perror("getcwd");
    return -1;
  }

  printf("Running as master: %s/%s\n",hostsfile,myaddr);
  if (0 != parse_address(myaddr,&host,&port)) {
    fprintf(stderr,"Invalid address: %s\n",myaddr);
    return -1;
  }
  printf("host = %s\n",host);
  printf("port = %d\n",port);

  if (NULL == (ni = nodeinfo_init(hostsfile)))
    r = -1;

  if (0 == r) {
    ni->listenfd = start_listening_host(host,port);
    if (0 > ni->listenfd)
      r = -1;
  }

  if (0 == r) {
    for (i = 0; (i < ni->nworkers) && (0 == r); i++) {
      ni->workers[i].pid = launch_worker(ni->workers[i].ip,
                                         dir,cmd,hostsfile,host,port,
                                         &ni->workers[i].readfd);
      if (0 > ni->workers[i].pid)
        r = -1;
    }
  }

  if (0 == r)
    r = wait_for_startup(ni);

  if (0 == r) {
    printf("Waiting for connections...\n");
    r = wait_for_connections(ni);
  }


  if (0 == r) {
    printf("All workers connected; sending notifications...\n");
    r = notify_startup(ni);
  }

  if (0 == r)
    printf("Notifications sent\n");

  free(dir);
  free(host);
  if (ni)
    nodeinfo_free(ni);
  return r;
}
