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
#include "node.h"
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
#include <execinfo.h>

endpoint *find_endpoint(node *n, int localid)
{
  endpoint *endpt;
  for (endpt = n->endpoints.first; endpt; endpt = endpt->next)
    if (endpt->localid == localid)
      break;
  return endpt;
}

task *find_task(node *n, int localid)
{
  endpoint *endpt = find_endpoint(n,localid);
  if (NULL == endpt)
    return NULL;
  if (TASK_ENDPOINT != endpt->type)
    fatal("Request for endpoint %d that is not a task\n",localid);
  return (task*)endpt->data;
}

void socket_send(task *tsk, int destid, int tag, char *data, int size)
{
  node *n = (node*)tsk->commdata;
  #ifdef MSG_DEBUG
  msg_print(tsk,destid,tag,data,size);
  #endif

  if (destid == tsk->pid)
    fatal("Attempt to send message (with tag %d) to task on same host\n",msg_names[tag]);

  node_send(n,tsk->endpt,tsk->idmap[destid],tag,data,size);
}

static int get_idmap_index(task *tsk, endpointid tid)
{
  int i;
  for (i = 0; i < tsk->groupsize; i++)
    if (!memcmp(&tsk->idmap[i],&tid,sizeof(endpointid)))
      return i;
  return -1;
}

int socket_recv(task *tsk, int *tag, char **data, int *size, int delayms)
{
  message *msg = endpoint_next_message(tsk->endpt,delayms);
  int from;
  if (NULL == msg)
    return -1;

  *tag = msg->hdr.tag;
  *data = msg->data;
  *size = msg->hdr.size;
  from = get_idmap_index(tsk,msg->hdr.source);
  assert(0 <= from);
  assert(tsk->endpt->localid == msg->hdr.destlocalid);
  free(msg);
  return from;
}

static void sigabrt(int sig)
{
  char *str = "abort()\n";
  write(STDERR_FILENO,str,strlen(str));
}

static void *btarray[1024];

static void sigsegv(int sig)
{
  char *str = "Segmentation fault\n";
  int size;
  write(STDERR_FILENO,str,strlen(str));

  size = backtrace(btarray,1024);
  backtrace_symbols_fd(btarray,size,STDERR_FILENO);

  signal(sig,SIG_DFL);
  kill(getpid(),sig);
}

static void exited()
{
  char *str = "exited\n";
  write(STDOUT_FILENO,str,strlen(str));
}

task *add_task(node *n, int pid, int groupsize, const char *bcdata, int bcsize)
{
  task *tsk = task_new(pid,groupsize,bcdata,bcsize,n);

  tsk->commdata = n;
  tsk->output = stdout;

  if ((0 == pid) && (NULL != bcdata)) {
    frame *initial = frame_new(tsk);
    initial->instr = bc_instructions(tsk->bcdata);
    initial->fno = -1;
    initial->data = (pntr*)malloc(sizeof(pntr));
    initial->alloc = 1;
    initial->c = alloc_cell(tsk);
    initial->c->type = CELL_FRAME;
    make_pntr(initial->c->field1,initial);
    run_frame(tsk,initial);
  }

  return tsk;
}

int worker(const char *host, int port)
{
  node *n;
  int listenfd;

  signal(SIGABRT,sigabrt);
  signal(SIGSEGV,sigsegv);
  atexit(exited);

  n = node_new();

  if (NULL == (n->mainl = node_listen(n,host,port,NULL,NULL)))
    return -1;
  n->listenport = port;

  start_manager(n);

  node_start_iothread(n);

  printf("Worker started, pid = %d, listening addr = %s:%d\n",
         getpid(),host ? host : "0.0.0.0",port);

  if (0 > wrap_pthread_join(n->iothread,NULL))
    fatal("pthread_join: %s",strerror(errno));

  endpoint_forceclose(n->managerendpt);
  if (0 > wrap_pthread_join(n->managerthread,NULL))
    fatal("pthread_join: %s",strerror(errno));
  printf("ioloop thread done\n");


  node_close_endpoints(n);
  node_close_connections(n);
  node_free(n);

  close(listenfd);
  return 0;
}
