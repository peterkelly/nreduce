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

#define WORKER_C

#include "compiler/bytecode.h"
#include "src/nreduce.h"
#include "runtime.h"
#include "node.h"
#include "messages.h"
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
#include <signal.h>
#include <execinfo.h>

const char *msg_names[MSG_COUNT] = {
  "DONE",
  "FISH",
  "FETCH",
  "TRANSFER",
  "ACK",
  "MARKROOTS",
  "MARKENTRY",
  "SWEEP",
  "SWEEPACK",
  "UPDATE",
  "RESPOND",
  "SCHEDULE",
  "UPDATEREF",
  "STARTDISTGC",
  "NEWTASK",
  "NEWTASKRESP",
  "INITTASK",
  "INITTASKRESP",
  "STARTTASK",
  "STARTTASKRESP",
  "KILL",
  "IORESPONSE",
  "ENDPOINT_EXIT",
  "LINK",
  "UNLINK",
  "CONSOLE_LINE",
  "FIND_SUCCESSOR",
  "GOT_SUCCESSOR",
  "NOTIFY",
  "NOTIFY_REPLY",
  "STABILIZE",
  "GET_TABLE",
  "REPLY_TABLE",
  "FIND_ALL",
  "CHORD_STARTED",
  "START_CHORD",
  "DEBUG_START",
  "DEBUG_DONE",
  "ID_CHANGED",
  "JOINED",
};

endpoint *find_endpoint(node *n, int localid)
{
  endpoint *endpt;
  for (endpt = n->endpoints.first; endpt; endpt = endpt->next)
    if (endpt->epid.localid == localid)
      break;
  return endpt;
}

task *find_task(node *n, int localid)
{
  endpoint *endpt = find_endpoint(n,localid);
  if (NULL == endpt)
    return NULL;
  if (TASK_ENDPOINT != endpt->type)
    fatal("Request for endpoint %d that is not a task",localid);
  return (task*)endpt->data;
}

void socket_send(task *tsk, int destid, int tag, char *data, int size)
{
  node *n = (node*)tsk->commdata;
  #ifdef MSG_DEBUG
  msg_print(tsk,destid,tag,data,size);
  #endif
  node_send(n,tsk->endpt->epid.localid,tsk->idmap[destid],tag,data,size);
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
  assert(tsk->endpt->epid.localid == msg->hdr.destlocalid);
  free(msg);
  return from;
}

static void *btarray[1024];

static void sigabrt(int sig)
{
  char *str = "abort()\n";
  int size;
  write(STDERR_FILENO,str,strlen(str));

  size = backtrace(btarray,1024);
  backtrace_symbols_fd(btarray,size,STDERR_FILENO);
}

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
    assert(initial->alloc == tsk->maxstack);
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

static void send_ioresponse(node *n, connection *conn, int *ioidptr, int event)
{
  endpointid destid = conn->tsk->endpt->epid;
  int ioid = *ioidptr;
  int msg[2];

  node_log(n,LOG_DEBUG2,"worker: event %s, ioid %d",event_types[event],ioid);

  msg[0] = ioid;
  msg[1] = event;
  *ioidptr = 0;

  if (EVENT_DATA_READ == event) {
    int fullsize = 2*sizeof(int)+conn->recvbuf->nbytes;
    char *full = (char*)malloc(fullsize);
    memcpy(full,msg,2*sizeof(int));
    memcpy(&full[2*sizeof(int)],conn->recvbuf->data,conn->recvbuf->nbytes);
    conn->recvbuf->nbytes = 0;
    node_send_locked(n,destid.localid,destid,MSG_IORESPONSE,full,fullsize);
    free(full);
    conn->dontread = 1;
  }
  else {
    if ((EVENT_CONN_IOERROR == event) || (EVENT_CONN_CLOSED == event))
      assert(0 == conn->recvbuf->nbytes);
    node_send_locked(n,destid.localid,destid,MSG_IORESPONSE,&msg,2*sizeof(int));
  }
}

static void worker_callback(struct node *n, void *data, int event,
                            connection *conn, endpoint *endpt)
{
  worker_data *wd = (worker_data*)data;

  if (conn && conn->status &&
      ((EVENT_CONN_IOERROR == event) || (EVENT_CONN_CLOSED == event)))
    *conn->status = event;

  if (conn && (0 < conn->frameids[CONNECT_FRAMEADDR]) &&
      ((EVENT_CONN_ESTABLISHED == event) ||
       (EVENT_CONN_FAILED == event)) )
    send_ioresponse(n,conn,&conn->frameids[CONNECT_FRAMEADDR],event);

  if (conn && (0 < conn->frameids[READ_FRAMEADDR]) &&
      ((EVENT_DATA_READ == event) ||
       (EVENT_DATA_READFINISHED == event) ||
       (EVENT_CONN_IOERROR == event) ||
       (EVENT_CONN_CLOSED == event)))
    send_ioresponse(n,conn,&conn->frameids[READ_FRAMEADDR],event);

  if (conn && (0 < conn->frameids[WRITE_FRAMEADDR]) &&
      ((EVENT_DATA_WRITTEN == event) ||
       (EVENT_CONN_IOERROR == event) ||
       (EVENT_CONN_CLOSED == event)))
    send_ioresponse(n,conn,&conn->frameids[WRITE_FRAMEADDR],event);

  if (conn && (0 < conn->frameids[ACCEPT_FRAMEADDR]) &&
      (EVENT_CONN_ACCEPTED == event))
    send_ioresponse(n,conn,&conn->frameids[ACCEPT_FRAMEADDR],event);

  if (((EVENT_CONN_IOERROR == event) ||
       (EVENT_CONN_CLOSED == event) ||
       (EVENT_CONN_FAILED == event) ||
       (EVENT_DATA_READFINISHED == event)) &&
      !conn->isconsole && !conn->isreg) {
    printf("worker: %s\n",event_types[event]);

    /* A connection to another node has failed. Search for links that referenced endpoints
       on the other node, and sent an ENDPOINT_EXIT to the local endpoint in each case. */
    endpoint *endpt;
    for (endpt = n->endpoints.first; endpt; endpt = endpt->next) {
      int max = list_count(endpt->outlinks);
      endpointid *exited = (endpointid*)malloc(max*sizeof(endpointid));
      int count = 0;
      list *l;
      int i;

      /* We must grab the list of exited endpoints before calling node_send_locked(), because it
         will call through to endpoint_add_message() which will modify the endpoint list to remove
         the links, so we can't do this while traversing the list. */
      for (l = endpt->outlinks; l; l = l->next) {
        endpointid *epid = (endpointid*)l->data;
        if ((epid->ip == conn->ip) && (epid->port == conn->port))
          exited[count++] = *epid;
      }

      /* Now we've found the relevant endpoints, send ENDPOINT_EXIT for each */
      for (i = 0; i < count; i++) {
        endpoint_exit_msg msg;
        msg.epid = exited[i];
        node_send_locked(n,endpt->epid.localid,endpt->epid,MSG_ENDPOINT_EXIT,
                         &msg,sizeof(endpoint_exit_msg));
      }

      free(exited);
    }
  }
  if ((EVENT_ENDPOINT_REMOVAL == event) && wd->standalone) {
    if (TASK_ENDPOINT == endpt->type) {
      node_shutdown_locked(n);
    }
  }
}

int worker(const char *host, int port, const char *bcdata, int bcsize, const char *chordtest)
{
  node *n;
  worker_data wd;
  unsigned char *ipbytes;

  signal(SIGABRT,sigabrt);
  signal(SIGSEGV,sigsegv);
  signal(SIGPIPE,SIG_IGN);

  memset(&wd,0,sizeof(worker_data));

  if (bcdata) {
    wd.standalone = 1;
    n = node_new(LOG_ERROR);
  }
  else {
    n = node_new(LOG_INFO);
  }

  n->isworker = 1;

  if (NULL == node_listen(n,n->listenip,port,NULL,NULL,0,1)) {
    node_free(n);
    return -1;
  }

  node_add_callback(n,worker_callback,&wd);
  start_manager(n);
  node_start_iothread(n);

  ipbytes = (unsigned char*)&n->listenip;
  node_log(n,LOG_INFO,"Worker started, pid = %d, listening addr = %u.%u.%u.%u:%d",
           getpid(),ipbytes[0],ipbytes[1],ipbytes[2],ipbytes[3],n->listenport);

  /* FIXME: this is kind of ugly... find a neater way to run the chord test that doesn't
     involve an extra argument to worker() */
  if (chordtest) {
    if (!strcmp(chordtest,"")) {
      /* no nodes file; run standalone test */
      endpointid managerid;
      managerid.ip = n->listenip;
      managerid.port = n->listenport;
      managerid.localid = MANAGER_ID;
      testchord(n,&managerid,1);
    }
    else {
      /* test against nodes specified in file */
      endpointid *managerids;
      int nmanagers;
      if (0 != read_managers(n,chordtest,&managerids,&nmanagers))
        exit(1);
      testchord(n,managerids,nmanagers);
      free(managerids);
    }
  }

  if (bcdata) {
    endpointid managerid;
    managerid.ip = n->listenip;
    managerid.port = n->listenport;
    managerid.localid = MANAGER_ID;
    start_launcher(n,bcdata,bcsize,&managerid,1);
  }

  if (0 != pthread_join(n->iothread,NULL))
    fatal("pthread_join: %s",strerror(errno));

  node_close_endpoints(n);
  node_close_connections(n);
  node_remove_callback(n,worker_callback,&wd);
  node_free(n);

  return 0;
}
