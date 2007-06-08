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
  "LISTEN",
  "ACCEPT",
  "CONNECT",
  "READ",
  "WRITE",
  "FINWRITE",
  "LISTEN_RESPONSE",
  "ACCEPT_RESPONSE",
  "CONNECT_RESPONSE",
  "READ_RESPONSE",
  "WRITE_RESPONSE",
  "CONNECTION_EVENT",
  "FINWRITE_RESPONSE",
  "DELETE_CONNECTION",
  "DELETE_LISTENER",
  "STARTGC",
  "STARTGC_RESPONSE",
  "STARTDISTGCACK",
  "GET_TASKS",
  "GET_TASKS_RESPONSE",
};

endpoint *find_endpoint(node *n, int localid)
{
  endpoint *endpt;
  for (endpt = n->endpoints.first; endpt; endpt = endpt->next)
    if (endpt->epid.localid == localid)
      break;
  return endpt;
}

void socket_send(task *tsk, int destid, int tag, char *data, int size)
{
  #ifdef MSG_DEBUG
  msg_print(tsk,destid,tag,data,size);
  #endif
  node_send(tsk->n,tsk->endpt->epid.localid,tsk->idmap[destid],tag,data,size);
}

static void worker_callback(struct node *n, void *data, int event,
                            connection *conn, endpoint *endpt)
{
  if (conn && (0 < conn->frameids[CONNECT_FRAMEADDR]) &&
      ((EVENT_CONN_ESTABLISHED == event) ||
       (EVENT_CONN_FAILED == event)) ) {
    connect_response_msg crm;

    crm.ioid = conn->frameids[CONNECT_FRAMEADDR];
    crm.event = event;
    if (EVENT_CONN_ESTABLISHED == event)
      crm.sockid = conn->sockid;
    else
      memset(&crm.sockid,0,sizeof(crm.sockid));
    memcpy(crm.errmsg,conn->errmsg,sizeof(crm.errmsg));

    conn->frameids[CONNECT_FRAMEADDR] = 0;

    node_send_locked(n,MANAGER_ID,conn->owner,MSG_CONNECT_RESPONSE,&crm,sizeof(crm));
  }

  if (conn && (0 < conn->frameids[READ_FRAMEADDR]) &&
      ((EVENT_DATA_READ == event) ||
       (EVENT_DATA_READFINISHED == event))) {


    int rrmlen = sizeof(read_response_msg)+conn->recvbuf->nbytes;
    read_response_msg *rrm = (read_response_msg*)malloc(rrmlen);

    rrm->ioid = conn->frameids[READ_FRAMEADDR];
    rrm->event = event;
    rrm->sockid = conn->sockid;
    rrm->len = conn->recvbuf->nbytes;
    memcpy(rrm->data,conn->recvbuf->data,conn->recvbuf->nbytes);

    node_send_locked(n,MANAGER_ID,conn->owner,MSG_READ_RESPONSE,rrm,rrmlen);

    conn->frameids[READ_FRAMEADDR] = 0;
    conn->recvbuf->nbytes = 0;
    conn->dontread = 1;
    free(rrm);
  }

  if (conn && (0 != conn->owner.localid) &&
      ((EVENT_CONN_IOERROR == event) || (EVENT_CONN_CLOSED == event))) {
    connection_event_msg cem;
    cem.sockid = conn->sockid;
    cem.event = event;
    memcpy(cem.errmsg,conn->errmsg,sizeof(cem.errmsg));
    node_send_locked(n,MANAGER_ID,conn->owner,MSG_CONNECTION_EVENT,&cem,sizeof(cem));
  }

  if (conn && (0 < conn->frameids[WRITE_FRAMEADDR]) &&
      (EVENT_DATA_WRITTEN == event) && (IOSIZE > conn->sendbuf->nbytes)) {
    write_response_msg wrm;
    wrm.ioid = conn->frameids[WRITE_FRAMEADDR];
    conn->frameids[WRITE_FRAMEADDR] = 0;
    node_send_locked(n,MANAGER_ID,conn->owner,MSG_WRITE_RESPONSE,&wrm,sizeof(wrm));
  }
}

static void standalone_callback(struct node *n, void *data, int event,
                                connection *conn, endpoint *endpt)
{
  if ((EVENT_ENDPOINT_REMOVAL == event) && (TASK_ENDPOINT == endpt->type)) {
    int *rc = (int*)data;
    *rc = endpt->rc;
    node_shutdown_locked(n);
  }
}

static node *worker_startup(int loglevel, int port)
{
  node *n = node_new(loglevel);
  unsigned char *ipbytes;

  if (NULL == node_listen(n,n->listenip,port,NULL,NULL,0,1,NULL,NULL,0)) {
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
  socketid out_sockid;
  node *n;
  int rc = 0;

  if (NULL == (n = worker_startup(LOG_ERROR,0)))
    return -1;

  node_add_callback(n,standalone_callback,&rc);

  managerid.ip = n->listenip;
  managerid.port = n->listenport;
  managerid.localid = MANAGER_ID;
  memset(&out_sockid,0,sizeof(out_sockid));
  start_launcher(n,bcdata,bcsize,&managerid,1,NULL,out_sockid);

  if (0 != pthread_join(n->iothread,NULL))
    fatal("pthread_join: %s",strerror(errno));
  node_close_endpoints(n);
  node_close_connections(n);
  node_free(n);
  return rc;
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

  if (0 != pthread_join(n->iothread,NULL))
    fatal("pthread_join: %s",strerror(errno));
  node_close_endpoints(n);
  node_close_connections(n);
  node_free(n);
  return 0;
}
