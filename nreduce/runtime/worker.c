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
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>

void startlog()
{
  int logfd;
  int i;

  if (0 > (logfd = open("/tmp/nreduce.log",O_WRONLY|O_APPEND))) {
    perror("/tmp/nreduce.log");
    exit(1);
  }

  setbuf(stdout,NULL);
  setbuf(stderr,NULL);

  close(STDOUT_FILENO);
  close(STDERR_FILENO);

  dup2(logfd,STDOUT_FILENO);
  dup2(logfd,STDERR_FILENO);

  for (i = 0; i < 20; i++)
    printf("\n");
  printf("==================================================\n");
}

int wait_for_startup_notification(int msock)
{
  int r;
  unsigned char c;
  do {
    r = read(msock,&c,1);
  } while ((0 > r) && (EAGAIN == errno));
  if (0 > r) {
    fprintf(stderr,"read() from master: %s\n",strerror(errno));
    return -1;
  }
  return c;
}

int connect_workers(nodeinfo *ni, int myindex)
{
  /* connect to workers > myindex, accept connections from those < myindex */
  int i;
  int remaining = ni->nworkers-1;

  for (i = myindex+1; i < ni->nworkers; i++) {
    struct sockaddr_in addr;

    if (0 > (ni->workers[i].sock = socket(AF_INET,SOCK_STREAM,0))) {
      perror("socket");
      return -1;
    }

    if (0 > fdsetflag(ni->workers[i].sock,O_NONBLOCK,1))
      return -1;

    addr.sin_family = AF_INET;
    addr.sin_port = htons(WORKER_PORT);
    addr.sin_addr = ni->workers[i].ip;
    memset(&addr.sin_zero,0,8);

    if (0 == connect(ni->workers[i].sock,(struct sockaddr*)&addr,sizeof(struct sockaddr))) {
      ni->workers[i].connected = 1;
      printf("Connected (immediately) to %s\n",ni->workers[i].hostname);
      remaining--;
    }
    else if (EINPROGRESS != errno) {
      perror("connect");
      return -1;
    }
  }

  while (0 < remaining) {
    fd_set readfds;
    fd_set writefds;
    int highest = ni->listenfd;

    FD_ZERO(&readfds);
    FD_ZERO(&writefds);

    for (i = 0; i < myindex; i++)
      if (!ni->workers[i].connected)
        FD_SET(ni->listenfd,&readfds);

    for (i = myindex+1; i < ni->nworkers; i++) {
      if (!ni->workers[i].connected) {
        FD_SET(ni->workers[i].sock,&writefds);
        if (highest < ni->workers[i].sock)
          highest = ni->workers[i].sock;
      }
    }

    if (0 > select(highest+1,&readfds,&writefds,NULL,NULL)) {
      perror("select");
      return -1;
    }

    if (FD_ISSET(ni->listenfd,&readfds)) {
      struct sockaddr_in remote_addr;
      int sin_size = sizeof(struct sockaddr_in);
      int clientfd;
      if (0 > (clientfd = accept(ni->listenfd,(struct sockaddr*)&remote_addr,&sin_size))) {
        perror("accept");
        return -1;
      }


      for (i = 0; i < ni->nworkers; i++) {
        if (!memcmp(&ni->workers[i].ip,&remote_addr.sin_addr,sizeof(struct in_addr))) {
          ni->workers[i].sock = clientfd;
          ni->workers[i].connected = 1;
          printf("Got connection from %s\n",ni->workers[i].hostname);
          remaining--;
        }
      }
    }

    for (i = myindex+1; i < ni->nworkers; i++) {
      if (!ni->workers[i].connected && FD_ISSET(ni->workers[i].sock,&writefds)) {
        int err;
        int optlen = sizeof(int);

        if (0 > getsockopt(ni->workers[i].sock,SOL_SOCKET,SO_ERROR,&err,&optlen)) {
          perror("getsockopt");
          return -1;
        }

        if (0 == err) {
          ni->workers[i].connected = 1;
          printf("Connected to %s\n",ni->workers[i].hostname);
          remaining--;
        }
        else {
          printf("Connection to %s failed: %s\n",ni->workers[i].hostname,strerror(err));
          return -1;
        }
      }
    }

  }
  printf("All connections now established\n");

  return 0;
}

int worker(const char *hostsfile, const char *masteraddr)
{
  char *mhost;
  int mport;
  int msock;
  struct in_addr addr;
  nodeinfo *ni;
  int i;
  int myindex;
  printf("1");

  startlog();

  printf("Running as worker, hostsfile = %s, master = %s, pid = %d\n",
         hostsfile,masteraddr,getpid());

  if (NULL == (ni = nodeinfo_init(hostsfile)))
    return -1;

  if (0 > parse_address(masteraddr,&mhost,&mport)) {
    fprintf(stderr,"Invalid address: %s\n",masteraddr);
    return -1;
  }

  addr.s_addr = INADDR_ANY;
  if (0 > (ni->listenfd = start_listening(addr,WORKER_PORT))) {
    free(mhost);
    return -1;
  }

  if (0 > (msock = connect_host(mhost,mport))) {
    free(mhost);
    close(ni->listenfd);
    fprintf(stderr,"Can't connect to %s: %s\n",masteraddr,strerror(errno));
    return -1;
  }

  if (0 > (myindex = wait_for_startup_notification(msock)))
    return -1;

  printf("Got startup notification; i am worker %d (%s)\n",
         myindex,ni->workers[myindex].hostname);

  if (0 > connect_workers(ni,myindex))
    return -1;

  printf("Hosts:\n");
  for (i = 0; i < ni->nworkers; i++)
    printf("%s, sock %d\n",ni->workers[i].hostname,ni->workers[i].sock);

/*   write(msock,"Here I am",strlen("Here I am")); */

  free(mhost);
  close(ni->listenfd);
  return 0;
}
