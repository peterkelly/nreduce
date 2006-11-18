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

#define _GNU_SOURCE /* for asprintf */

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

extern const char *prelude;

typedef struct workerinfo {
  char *hostname;
  struct in_addr ip;
  int readfd;
} workerinfo;

static void lookup_hostnames(int nworkers, workerinfo *workers)
{
  int i;
  for (i = 0; i < nworkers; i++) {
    struct hostent *he = gethostbyname(workers[i].hostname);
    if (NULL == he)
      fatal("%s: %s\n",workers[i].hostname,hstrerror(h_errno));

    if (4 != he->h_length)
      fatal("%s: %s\n",workers[i].hostname,hstrerror(h_errno));

    workers[i].ip = *((struct in_addr*)he->h_addr_list[0]);
  }
}

static pid_t launch_worker(struct in_addr ip, const char *dir, const char *cmd,
                           const char *hostsfile, const char *mhost, int mport,
                           int *outfd, int listenfd)
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
    char *ipstr = NULL;
    char *rshcmd = NULL;
    char *args[6];
    unsigned char *ipaddr = (char*)&ip.s_addr;

    close(listenfd);

    asprintf(&ipstr,"%d.%d.%d.%d",ipaddr[0],ipaddr[1],ipaddr[2],ipaddr[3]);
    printf("ipstr = %s\n",ipstr);

    asprintf(&rshcmd,"cd %s && %s -w",dir,cmd);
    printf("rshcmd = %s\n",rshcmd);

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

static int wait_for_startup(int nworkers, workerinfo *workers)
{
  int count = nworkers;
  int *done = (int*)calloc(count,sizeof(int));

  while (1) {
    int i;
    int highest = -1;
    int active = 0;
    fd_set fds;

    FD_ZERO(&fds);

    for (i = 0; i < count; i++) {
      int fd = workers[i].readfd;
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
      int fd = workers[i].readfd;
      if (!done[i] && FD_ISSET(fd,&fds)) {
        char *whost = workers[i].hostname;
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

int master(const char *hostsfile, const char *myaddr, const char *filename, const char *cmd)
{
  char *host;
  int port;
  int r = 0;
  char *dir;
  source *src;
  int bcsize;
  char *bcdata = NULL;
  int listenfd = -1;
  array *hostnames = NULL;
  int i;
  int nworkers;
  workerinfo *workers;

  src = source_new();
  if (0 != source_parse_string(src,prelude,"prelude.l"))
    return -1;
  if (0 != source_parse_file(src,filename,""))
    return -1;
  if (0 != source_process(src))
    return -1;
  if (0 != source_compile(src,&bcdata,&bcsize))
    return -1;
  source_free(src);

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

  if (NULL == (hostnames = read_hostnames(hostsfile)))
    exit(-1);

  nworkers = array_count(hostnames);
  workers = calloc(nworkers,sizeof(workerinfo));

  for (i = 0; (i < nworkers) && (0 == r); i++) {
    workers[i].hostname = strdup(array_item(hostnames,i,char*));
    workers[i].ip.s_addr = 0;
    workers[i].readfd = -1;
  }

  lookup_hostnames(nworkers,workers);

  if (0 > (listenfd = start_listening_host(host,port)))
    exit(-1);

  for (i = 0; (i < nworkers) && (0 == r); i++) {
    if (0 > launch_worker(workers[i].ip,
                          dir,cmd,hostsfile,host,port,
                          &workers[i].readfd,listenfd))
      exit(-1);
  }

  if (0 != wait_for_startup(nworkers,workers))
    exit(-1);

  printf("Closing connections\n");
  for (i = 0; i < nworkers; i++) {
    close(workers[i].readfd);
  }

  free(dir);
  free(host);
  free(bcdata);
  if (0 <= listenfd)
    close(listenfd);
  printf("exiting\n");
  exit(r);
  return r;
}
