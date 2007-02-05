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
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
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

typedef struct worker_data {
  int standalone;
} worker_data;

static void worker_callback(struct node *n, void *data, int event,
                            connection *conn, endpoint *endpt)
{
  worker_data *wd = (worker_data*)data;
  if (((EVENT_CONN_IOERROR == event) || (EVENT_CONN_CLOSED == event)) &&
      !conn->isconsole && !conn->isreg) {
    endpoint *endpt;
    endpoint *mgrendpt = NULL;

    for (endpt = n->endpoints.first; endpt; endpt = endpt->next) {
      if (MANAGER_ENDPOINT == endpt->type) {
        assert(NULL == mgrendpt);
        mgrendpt = endpt;
      }
    }
    assert(mgrendpt);

    for (endpt = n->endpoints.first; endpt; endpt = endpt->next) {
      task *tsk;
      message *msg;
      int kill = 0;
      int i;

      if (TASK_ENDPOINT != endpt->type)
        continue;

      tsk = (task*)endpt->data;
      if (tsk->haveidmap) {
        for (i = 0; i < tsk->groupsize; i++)
          if ((tsk->idmap[i].nodeport == conn->port) &&
              !memcmp(&tsk->idmap[i].nodeip,&conn->ip,sizeof(struct in_addr)))
            kill = 1;
      }
      if (!kill)
        continue;

      msg = (message*)calloc(1,sizeof(message));
      msg->hdr.source.nodeip = n->listenip;
      msg->hdr.source.nodeport = n->mainl->port;
      msg->hdr.source.localid = 0;
      msg->hdr.destlocalid = MANAGER_ID;
      msg->hdr.size = sizeof(int);
      msg->hdr.tag = MSG_KILLTASK;
      msg->data = (char*)malloc(sizeof(int));
      memcpy(msg->data,&endpt->localid,sizeof(int));
      endpoint_add_message(mgrendpt,msg);
      node_log(n,LOG_WARNING,"Killing task %d due to node IO error",endpt->localid);
    }
  }
  else if ((EVENT_ENDPOINT_REMOVAL == event) && wd->standalone) {
    if (TASK_ENDPOINT == endpt->type) {
      node_shutdown_locked(n);
    }
  }
}

int worker(const char *host, int port, const char *bcdata, int bcsize)
{
  node *n;
  worker_data wd;

  signal(SIGABRT,sigabrt);
  signal(SIGSEGV,sigsegv);

  memset(&wd,0,sizeof(worker_data));

  n = node_new();
  n->isworker = 1;
  if (bcdata) {
    wd.standalone = 1;
    n->loglevel = LOG_ERROR;
  }

  if (NULL == (n->mainl = node_listen(n,host,port,NULL,NULL)))
    return -1;

  start_manager(n);

  node_add_callback(n,worker_callback,&wd);
  node_start_iothread(n);

  node_log(n,LOG_INFO,"Worker started, pid = %d, listening addr = %s:%d",
           getpid(),host ? host : "0.0.0.0",port);

  if (bcdata) {
    endpointid managerid;
    managerid.nodeip.s_addr = inet_addr("127.0.0.1");
    managerid.nodeport = n->mainl->port;
    managerid.localid = MANAGER_ID;
    start_launcher(n,bcdata,bcsize,&managerid,1);
  }

  if (0 > pthread_join(n->iothread,NULL))
    fatal("pthread_join: %s",strerror(errno));

  endpoint_forceclose(n->managerendpt);
  if (0 > pthread_join(n->managerthread,NULL))
    fatal("pthread_join: %s",strerror(errno));

  node_close_endpoints(n);
  node_close_connections(n);
  node_remove_callback(n,worker_callback,&wd);
  node_free(n);

  return 0;
}
