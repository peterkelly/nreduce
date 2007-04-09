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

#define WORKER_STABILIZE_DELAY 5000

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
  "GET_SUCCLIST",
  "REPLY_SUCCLIST",
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
  "INSERT",
  "SET_NEXT",
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
    if (endpointid_equals(&tsk->idmap[i],&tid))
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
}

static void standalone_callback(struct node *n, void *data, int event,
                                connection *conn, endpoint *endpt)
{
  if ((EVENT_ENDPOINT_REMOVAL == event) && (TASK_ENDPOINT == endpt->type))
    node_shutdown_locked(n);
}

static node *worker_startup(int loglevel, int port)
{
  node *n = node_new(loglevel);
  unsigned char *ipbytes;

  n->isworker = 1;
  if (NULL == node_listen(n,n->listenip,port,NULL,NULL,0,1)) {
    node_free(n);
    return NULL;
  }

  node_add_callback(n,worker_callback,NULL);
  start_manager(n);
  node_start_iothread(n);

  ipbytes = (unsigned char*)&n->listenip;
  node_log(n,LOG_INFO,"Worker started, pid = %d, listening addr = %u.%u.%u.%u:%d",
           getpid(),ipbytes[0],ipbytes[1],ipbytes[2],ipbytes[3],n->listenport);
  return n;
}

int standalone(const char *bcdata, int bcsize)
{
  endpointid managerid;
  node *n;

  if (NULL == (n = worker_startup(LOG_ERROR,0)))
    return -1;

  node_add_callback(n,standalone_callback,NULL);

  managerid.ip = n->listenip;
  managerid.port = n->listenport;
  managerid.localid = MANAGER_ID;
  start_launcher(n,bcdata,bcsize,&managerid,1,NULL);

  if (0 != pthread_join(n->iothread,NULL))
    fatal("pthread_join: %s",strerror(errno));
  node_close_endpoints(n);
  node_close_connections(n);
  node_free(n);
  return 0;
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
    if (0 > lookup_address(n,host,&addr)) {
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

  if (0 != pthread_join(n->iothread,NULL))
    fatal("pthread_join: %s",strerror(errno));
  node_close_endpoints(n);
  node_close_connections(n);
  node_free(n);
  return 0;
}
