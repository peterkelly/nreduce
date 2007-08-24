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
 * $Id$
 *
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#define WORKER_C

#include "compiler/bytecode.h"
#include "src/nreduce.h"
#include "runtime.h"
#include "network/node.h"
#include "network/messages.h"
#include "chord.h"
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
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <time.h>
#include <fcntl.h>
#include <unistd.h>

#define WORKER_STABILIZE_DELAY 5000

static node *worker_startup(int loglevel, int port)
{
  node *n = node_start(loglevel,port);
  if (n) {
    unsigned char *ipbytes;
    ipbytes = (unsigned char*)&n->listenip;
    node_log(n,LOG_INFO,"Worker started, pid = %d, listening addr = %u.%u.%u.%u:%d",
             getpid(),ipbytes[0],ipbytes[1],ipbytes[2],ipbytes[3],n->listenport);
    start_manager(n);
    node_add_thread2(n,"java",java_thread,NULL,NULL,JAVA_ID,0);
  }
  return n;
}

int standalone(const char *bcdata, int bcsize, int argc, const char **argv)
{
  endpointid managerid;
  socketid out_sockid;
  node *n;
  pthread_t thread;
  output_arg oa;

  if (NULL == (n = worker_startup(LOG_ERROR,0)))
    return -1;

  managerid.ip = n->listenip;
  managerid.port = n->listenport;
  managerid.localid = MANAGER_ID;

  memset(&oa,0,sizeof(oa));
  out_sockid.coordid = node_add_thread(n,"output",output_thread,&oa,&thread);
  out_sockid.sid = 1;

  start_launcher(n,bcdata,bcsize,&managerid,1,NULL,out_sockid,argc,argv);

  if (0 != pthread_join(thread,NULL))
    fatal("pthread_join: %s",strerror(errno));

  node_shutdown(n);

  node_run(n);
  return oa.rc;
}

int string_to_mainchordid(node *n, const char *str, endpointid *out)
{
  endpointid initial;
  memset(&initial,0,sizeof(endpointid));
  if (str) {
    char *host = NULL;
    int port = 0;
    in_addr_t addr = 0;

    if (NULL == strchr(str,':')) {
      host = strdup(str);
      port = WORKER_PORT;
    }
    else if (0 > parse_address(str,&host,&port)) {
      fprintf(stderr,"Invalid address: %s\n",str);
      return -1;
    }
    if (0 > lookup_address(n,host,&addr,NULL)) {
      fprintf(stderr,"Host lookup failed: %s\n",host);
      free(host);
      return -1;
    }
    initial.ip = addr;
    initial.port = port;
    initial.localid = MAIN_CHORD_ID;
    free(host);
  }
  *out = initial;
  return 0;
}

int worker(int port, const char *initial_str)
{
  node *n;
  endpointid initial;
  endpointid caller;

  if (NULL == (n = worker_startup(LOG_INFO,port)))
    return -1;

  if (0 > string_to_mainchordid(n,initial_str,&initial))
    exit(1);

  memset(&caller,0,sizeof(endpointid));
  start_chord(n,MAIN_CHORD_ID,initial,caller,WORKER_STABILIZE_DELAY);

  node_run(n);
  return 0;
}
