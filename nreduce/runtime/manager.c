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
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>

#define DISTGC_DELAY 3000
//#define DEBUG_DISTGC

typedef struct javacmd {
  int ioid;
  int oneway;
  endpointid epid;
} javacmd;

typedef struct manager {
  int jconnected;
  int jconnecting;
  array *jsendbuf;
  list *jcmds;
  socketid jcon;
  array *jresponse;
} manager;

typedef struct gcarg {
  int ntasks;
  endpointid idmap[0];
} gcarg;

static void gc_thread(node *n, endpoint *endpt, void *arg)
{
  gcarg *ga = (gcarg*)arg;
  int done = 0;
  int i;
  int ingc = 0;
  int *count = (int*)calloc(ga->ntasks,sizeof(int));
  int mark_done = 0;
  int rem_startacks = 0;
  int rem_sweepacks = 0;
  int gciter = 0;

  #ifdef DEBUG_DISTGC
  printf("gc_thread\n");
  for (i = 0; i < ga->ntasks; i++)
    printf("gc_thread: idmap[%d] = "EPID_FORMAT"\n",i,EPID_ARGS(ga->idmap[i]));
  #endif

  for (i = 0; i < ga->ntasks; i++)
    endpoint_link(endpt,ga->idmap[i]);

  while (!done) {
    message *msg = endpoint_receive(endpt,ingc ? -1 : DISTGC_DELAY);
    if (NULL == msg) {
      startdistgc_msg sm;
      sm.gc = endpt->epid;
      sm.gciter = ++gciter;
      #ifdef DEBUG_DISTGC
      printf("Starting distributed garbage collection\n");
      #endif
      assert(!ingc);
      ingc = 1;

      for (i = 0; i < ga->ntasks; i++)
        endpoint_send(endpt,ga->idmap[i],MSG_STARTDISTGC,&sm,sizeof(sm));
      rem_startacks = ga->ntasks;

      continue;
    }
    switch (msg->hdr.tag) {
    case MSG_STARTDISTGCACK: {
      assert(ingc);
      assert(!mark_done);
      assert(0 < rem_startacks);
      rem_startacks--;
      if (0 == rem_startacks) {
        #ifdef DEBUG_DISTGC
        printf("All tasks have received STARTDISTGC\n");
        #endif
        memset(count,0,ga->ntasks*sizeof(int));

        for (i = 0; i < ga->ntasks; i++) {
          endpoint_send(endpt,ga->idmap[i],MSG_MARKROOTS,NULL,0);
          count[i]++;
        }
      }
      break;
    }
    case MSG_UPDATE: {
      update_msg *m = (update_msg*)msg->data;
      assert(sizeof(update_msg)+ga->ntasks*sizeof(int) == msg->hdr.size);
      assert(ingc);
      assert(!mark_done);
      assert(m->gciter == gciter);

      for (i = 0; i < ga->ntasks; i++)
        count[i] += m->counts[i];

      #ifdef DEBUG_DISTGC
      printf("after update (gciter %d) from "EPID_FORMAT":",m->gciter,EPID_ARGS(msg->hdr.source));
      for (i = 0; i < ga->ntasks; i++)
        printf(" %d",count[i]);
      printf("\n");
      #endif

      mark_done = 1;
      for (i = 0; i < ga->ntasks; i++)
        if (count[i])
          mark_done = 0;

      if (mark_done) {
        #ifdef DEBUG_DISTGC
        printf("Mark done\n");
        #endif
        for (i = 0; i < ga->ntasks; i++)
          endpoint_send(endpt,ga->idmap[i],MSG_SWEEP,NULL,0);
        rem_sweepacks = ga->ntasks;
      }
      break;
    }
    case MSG_SWEEPACK: {
      assert(ingc);
      assert(mark_done);
      assert(0 < rem_sweepacks);
      rem_sweepacks--;
      if (0 == rem_sweepacks) {
        #ifdef DEBUG_DISTGC
        printf("Distributed garbage collection completed\n");
        #endif
        ingc = 0;
        mark_done = 0;
        for (i = 0; i < ga->ntasks; i++) {
          assert(0 == count[i]);
        }
      }
      break;
    }
    case MSG_ENDPOINT_EXIT:
      done = 1;
      break;
    case MSG_KILL:
      done = 1;
      break;
    default:
      fatal("gc: unexpected message %s",msg_names[msg->hdr.tag]);
      break;
    }
    message_free(msg);
  }
  free(count);
  free(ga);
}

static connection *get_connection(node *n, socketid id)
{
  connection *conn;
  assert(NODE_ALREADY_LOCKED(n));
  for (conn = n->connections.first; conn; conn = conn->next)
    if (socketid_equals(&conn->sockid,&id))
      return conn;
  return NULL;
}

static listener *get_listener(node *n, socketid id)
{
  listener *l;
  assert(NODE_ALREADY_LOCKED(n));
  for (l = n->listeners.first; l; l = l->next)
    if (socketid_equals(&l->sockid,&id))
      return l;
  return NULL;
}

static void manager_listen(node *n, endpoint *endpt, listen_msg *m, endpointid source)
{
  listener *l;
  listen_response_msg lrm;

  endpoint_link(endpt,m->owner);

  l = node_listen(n,m->ip,m->port,1,NULL,1,0,&m->owner,lrm.errmsg,ERRMSG_MAX);
  lrm.errmsg[ERRMSG_MAX] = '\0';

  lrm.ioid = m->ioid;
  if (NULL != l) {
    lrm.error = 0;
    lrm.sockid = l->sockid;
  }
  else {
    lrm.error = 1;
    memset(&lrm.sockid,0,sizeof(socketid));
  }

  endpoint_send(endpt,source,MSG_LISTEN_RESPONSE,&lrm,sizeof(lrm));
}

static void manager_accept(node *n, endpoint *endpt, accept_msg *m, endpointid source)
{
  listener *l;

  lock_node(n);
  l = get_listener(n,m->sockid);
  assert(NULL != l);
  assert(0 == l->accept_frameid);
  l->accept_frameid = m->ioid;
  assert(l->dontaccept);
  l->dontaccept = 0;
  node_notify(n);
  unlock_node(n);
}

static void manager_connect(node *n, endpoint *endpt, connect_msg *m, endpointid source)
{
  connection *conn;
  connect_response_msg crm;

  endpoint_link(endpt,m->owner);

  lock_node(n);
  conn = node_connect_locked(n,m->hostname,INADDR_ANY,m->port,0,crm.errmsg,ERRMSG_MAX);
  if (conn) {
    assert(0 == conn->frameids[CONNECT_FRAMEADDR]);
    conn->frameids[CONNECT_FRAMEADDR] = m->ioid;
    conn->dontread = 1;
    conn->owner = m->owner;
  }
  unlock_node(n);

  if (NULL == conn) {
    crm.errmsg[ERRMSG_MAX] = '\0';
    crm.ioid = m->ioid;
    crm.error = 1;
    memset(&crm.sockid,0,sizeof(crm.sockid));
    endpoint_send(endpt,source,MSG_CONNECT_RESPONSE,&crm,sizeof(crm));
  }
}

static void manager_read(node *n, endpoint *endpt, read_msg *m)
{
  connection *conn;

  lock_node(n);
  /* If the connection doesn't exist any more, ignore the request - the caller is about to
     receive a CONNECTION_CLOSED MESSAGE */
  if (NULL != (conn = get_connection(n,m->sockid))) {
    if (!conn->canread)
      fatal("READ request for socket that has finished reading");
    assert(!conn->collected);
    assert(conn->dontread);
    assert(0 == conn->frameids[READ_FRAMEADDR]);

    conn->frameids[READ_FRAMEADDR] = m->ioid;
    conn->dontread = 0;
    node_notify(n);
  }
  unlock_node(n);
}

/* FIXME: we should allow multiple WRITE messages to be sent without the caller having to first
   process the WRITE_RESPONSE. This is useful for things like the echo test, where if it supplies
   an ioid (which is irrelevant in this case) greater than 0, we can sometimes get crashes.

   Ideally we should just remove the whole ioid thing altogether, and send the sockid back
   in the WRITE_RESPONSE message instead of the ioid. The code in builtins.c should be
   modified to associate a {sockid,operation} combination with a suspended frame, instead of an
   ioid. */
static void manager_write(node *n, endpoint *endpt, write_msg *m, endpointid source)
{
  connection *conn;
  int spaceleft = 0;

  lock_node(n);

  /* If the connection doesn't exist any more, ignore the request - the caller is about to
     receive a CONNECTION_CLOSED MESSAGE */
  if (NULL != (conn = get_connection(n,m->sockid))) {
    assert(!conn->collected);

    array_append(conn->sendbuf,m->data,m->len);

    assert(0 == conn->frameids[WRITE_FRAMEADDR]);

    /* The calling frame will block when it sends us the WRITE message. If the write buffer
       still has room left for more data, wake it up immediately. Otherwise, keep it blocked
       until some of the data has been written. */
    if (n->iosize > conn->sendbuf->nbytes)
      spaceleft = 1;
    else
      conn->frameids[WRITE_FRAMEADDR] = m->ioid;

    node_notify(n);
  }
  unlock_node(n);

  if (spaceleft) {
    write_response_msg wrm;
    wrm.ioid = m->ioid;
    endpoint_send(endpt,source,MSG_WRITE_RESPONSE,&wrm,sizeof(wrm));
  }
}

static void manager_finwrite(node *n, endpoint *endpt, finwrite_msg *m, endpointid source)
{
  connection *conn;

  /* If the connection doesn't exist any more, ignore the request - the caller is about to
     receive a CONNECTION_CLOSED MESSAGE */
  lock_node(n);
  conn = get_connection(n,m->sockid);
  if (conn) {
    assert(!conn->collected);
    conn->finwrite = 1;
    node_notify(n);
  }
  unlock_node(n);

  if (conn) {
    finwrite_response_msg frm;
    frm.ioid = m->ioid;
    endpoint_send(endpt,source,MSG_FINWRITE_RESPONSE,&frm,sizeof(frm));
  }
}

static void manager_delete_connection(node *n, endpoint *endpt,
                                      delete_connection_msg *m, endpointid source)
{
  connection *conn;

  lock_node(n);
  if (NULL != (conn = get_connection(n,m->sockid))) {
    conn->collected = 1;
    if (!conn->finwrite) {
      conn->finwrite = 1;
      node_notify(n);
    }
    done_reading(n,conn);
  }
  unlock_node(n);
}

static void manager_delete_listener(node *n, endpoint *endpt,
                                    delete_listener_msg *m, endpointid source)
{
  listener *l;

  lock_node(n);
  l = get_listener(n,m->sockid);
  if (NULL != l)
    node_remove_listener(n,l);
  unlock_node(n);
}

static void send_java_commands(node *n, manager *mgr, endpoint *endpt)
{
  /* FIXME: handle the case where the connection has been dropped */
  if (0 < mgr->jsendbuf->nbytes) {
    char *data = mgr->jsendbuf->data;
    int len = mgr->jsendbuf->nbytes;
    send_write(endpt,mgr->jcon.managerid,mgr->jcon,1,data,len);
    mgr->jsendbuf->nbytes = 0;
  }
}

/* for notifying of connection to JVM */
static void manager_connect_response(node *n, manager *mgr, endpoint *endpt, connect_response_msg *m)
{
  mgr->jconnecting = 0;

  if (m->error) {
    list *l;
    for (l = mgr->jcmds; l; l = l->next) {
      javacmd *jc = l->data;
      send_jcmd_response(endpt,jc->epid,jc->ioid,1,NULL,0);
    }
    list_free(mgr->jcmds,free);
  }
  else {
    mgr->jcon = m->sockid;
    mgr->jconnected = 1;
    send_java_commands(n,mgr,endpt);
    send_read(endpt,endpt->epid,mgr->jcon,1);
  }
}

static void manager_read_response(node *n, manager *mgr, endpoint *endpt, read_response_msg *m)
{
  int start = 0;
  int pos = 0;
  array_append(mgr->jresponse,m->data,m->len);
  while (pos < mgr->jresponse->nbytes) {
    if ('\n' == mgr->jresponse->data[pos]) {
      int len = pos-start;
      list *old = mgr->jcmds;
      javacmd *jc;
      assert(mgr->jcmds);
      jc = mgr->jcmds->data;
      if (!jc->oneway)
        send_jcmd_response(endpt,jc->epid,jc->ioid,0,&mgr->jresponse->data[start],len);
      mgr->jcmds = mgr->jcmds->next;
      free(jc);
      free(old);

      start = pos+1;
    }
    pos++;
  }
  array_remove_data(mgr->jresponse,start);
  send_read(endpt,endpt->epid,mgr->jcon,1);
}

static void manager_jcmd(node *n, manager *mgr, endpoint *endpt, message *msg)
{
  jcmd_msg *m;
  javacmd *jc;
  char newline = '\n';
  assert(sizeof(jcmd_msg) <= msg->hdr.size);
  m = (jcmd_msg*)msg->data;
  assert(msg->hdr.size == sizeof(jcmd_msg)+m->cmdlen);

  array_append(mgr->jsendbuf,m->cmd,m->cmdlen);
  array_append(mgr->jsendbuf,&newline,1);

  jc = (javacmd*)calloc(sizeof(javacmd),1);
  jc->ioid = m->ioid;
  jc->oneway = m->oneway;
  jc->epid = msg->hdr.source;
  list_append(&mgr->jcmds,jc);

  if (!mgr->jconnected && !mgr->jconnecting) {
    connect_msg cm;
    snprintf(cm.hostname,HOSTNAME_MAX,"127.0.0.1");
    cm.port = JBRIDGE_PORT; /* FIXME: make this configurable */
    cm.owner = endpt->epid;
    cm.ioid = 1;
    endpoint_send(endpt,endpt->epid,MSG_CONNECT,&cm,sizeof(cm));
    return;
  }

  if (mgr->jconnected)
    send_java_commands(n,mgr,endpt);
}

static void manager_startgc(node *n, endpoint *endpt, startgc_msg *m, endpointid source)
{
  gcarg *ga = (gcarg*)calloc(1,sizeof(gcarg)+m->count*sizeof(endpointid));
  ga->ntasks = m->count;
  memcpy(ga->idmap,m->idmap,m->count*sizeof(endpointid));
  node_add_thread(n,"gc",gc_thread,ga,NULL);
  endpoint_send(endpt,source,MSG_STARTGC_RESPONSE,NULL,0);
}

static void manager_get_tasks(node *n, endpoint *endpt, get_tasks_msg *m)
{
  get_tasks_response_msg *gtrm;
  int msize;
  int count = 0;
  int i = 0;
  endpoint *ep;

  lock_node(n);
  for (ep = n->endpoints.first; ep; ep = ep->next) {
    if (!strcmp(ep->type,"task"))
      count++;
  }

  msize = sizeof(get_tasks_response_msg)+count*sizeof(endpointid);
  gtrm = (get_tasks_response_msg*)calloc(1,msize);
  gtrm->count = count;

  for (ep = n->endpoints.first; ep; ep = ep->next) {
    if (!strcmp(ep->type,"task"))
      gtrm->tasks[i++] = ep->epid;
  }
  unlock_node(n);

  endpoint_send(endpt,m->sender,MSG_GET_TASKS_RESPONSE,gtrm,msize);
  free(gtrm);
}

static task *find_task(node *n, int localid)
{
  endpoint *endpt = find_endpoint(n,localid);
  if (NULL == endpt)
    return NULL;
  if (strcmp(endpt->type,"task"))
    fatal("Request for endpoint %d that is not a task",localid);
  return (task*)endpt->data;
}

static void manager_thread(node *n, endpoint *endpt, void *arg)
{
  message *msg;
  int done = 0;
  manager mgr;

  memset(&mgr,0,sizeof(mgr));
  mgr.jresponse = array_new(1,0);
  mgr.jsendbuf = array_new(1,0);

  while (!done) {
    msg = endpoint_receive(endpt,-1);
    switch (msg->hdr.tag) {
    case MSG_NEWTASK: {
      newtask_msg *ntmsg;
      endpointid epid;
      array *args = array_new(sizeof(char*),0);
      char *str;
      char *start;
      if (sizeof(newtask_msg) > msg->hdr.size)
        fatal("NEWTASK: invalid message size");
      ntmsg = (newtask_msg*)msg->data;
      if (sizeof(newtask_msg)+ntmsg->bcsize > msg->hdr.size)
        fatal("NEWTASK: invalid bytecode size");

      str = ((char*)msg->data)+sizeof(newtask_msg)+ntmsg->bcsize;
      start = str;
      while (str < ((char*)msg->data)+msg->hdr.size) {
        if ('\0' == *str) {
          array_append(args,&start,sizeof(char*));
          start = str+1;
        }
        str++;
      }
      assert(array_count(args) == ntmsg->argc);

      node_log(n,LOG_INFO,"NEWTASK pid = %d, groupsize = %d, bcsize = %d",
               ntmsg->tid,ntmsg->groupsize,ntmsg->bcsize);

      task_new(ntmsg->tid,ntmsg->groupsize,ntmsg->bcdata,ntmsg->bcsize,args,n,
               ntmsg->out_sockid,&epid);

      endpoint_send(endpt,msg->hdr.source,MSG_NEWTASKRESP,
                &epid.localid,sizeof(int));
      array_free(args);
      break;
    }
    case MSG_INITTASK: {
      inittask_msg *initmsg;
      task *newtsk;
      int i;
      int resp = 0;
      if (sizeof(inittask_msg) > msg->hdr.size)
        fatal("INITTASK: invalid message size");
      initmsg = (inittask_msg*)msg->data;
      if (sizeof(initmsg)+initmsg->count*sizeof(endpointid) > msg->hdr.size)
        fatal("INITTASK: invalid idmap size");

      node_log(n,LOG_INFO,"INITTASK: localid = %d, count = %d",initmsg->localid,initmsg->count);

      lock_node(n);
      newtsk = find_task(n,initmsg->localid);

      if (NULL == newtsk)
        fatal("INITTASK: task with localid %d does not exist",initmsg->localid);

      if (newtsk->haveidmap)
        fatal("INITTASK: task with localid %d already has an idmap",initmsg->localid);

      if (initmsg->count != newtsk->groupsize)
        fatal("INITTASK: idmap size does not match expected");
      memcpy(newtsk->idmap,initmsg->idmap,newtsk->groupsize*sizeof(endpointid));
      newtsk->haveidmap = 1;

      for (i = 0; i < newtsk->groupsize; i++)
        if (!endpointid_equals(&newtsk->endpt->epid,&initmsg->idmap[i]))
          endpoint_link_locked(newtsk->endpt,initmsg->idmap[i]);

      unlock_node(n);

      for (i = 0; i < newtsk->groupsize; i++) {
        endpointid_str str;
        print_endpointid(str,initmsg->idmap[i]);
        node_log(n,LOG_INFO,"INITTASK: idmap[%d] = %s",i,str);
      }

      endpoint_send(endpt,msg->hdr.source,MSG_INITTASKRESP,&resp,sizeof(int));
      break;
    }
    case MSG_STARTTASK: {
      task *newtsk;
      int resp = 0;
      int localid;
      char semdata = 0;
      if (sizeof(int) > msg->hdr.size)
        fatal("STARTTASK: invalid message size");
      localid = *(int*)msg->data;

      node_log(n,LOG_INFO,"STARTTASK: localid = %d",localid);

      lock_node(n);
      newtsk = find_task(n,localid);

      if (NULL == newtsk)
        fatal("STARTTASK: task with localid %d does not exist",localid);

      if (!newtsk->haveidmap)
        fatal("STARTTASK: task with localid %d does not have an idmap",localid);

      if (newtsk->started)
        fatal("STARTTASK: task with localid %d has already been started",localid);

      write(newtsk->startfds[1],&semdata,1);
      newtsk->started = 1;
      unlock_node(n);

      endpoint_send(endpt,msg->hdr.source,MSG_STARTTASKRESP,&resp,sizeof(int));
      break;
    }
    case MSG_ENDPOINT_EXIT: {
      endpoint_exit_msg *m = (endpoint_exit_msg*)msg->data;
      assert(sizeof(endpoint_exit_msg) == msg->hdr.size);
      node_handle_endpoint_exit(n,m->epid);
      break;
    }
    case MSG_START_CHORD: {
      start_chord_msg *m = (start_chord_msg*)msg->data;
      assert(sizeof(start_chord_msg) == msg->hdr.size);
      start_chord(n,0,m->initial,m->caller,m->stabilize_delay);
      break;
    }
    case MSG_KILL:
      node_log(n,LOG_INFO,"Manager received KILL");
      done = 1;
      break;
    case MSG_LISTEN:
      assert(sizeof(listen_msg) == msg->hdr.size);
      manager_listen(n,endpt,(listen_msg*)msg->data,msg->hdr.source);
      break;
    case MSG_ACCEPT:
      assert(sizeof(accept_msg) == msg->hdr.size);
      manager_accept(n,endpt,(accept_msg*)msg->data,msg->hdr.source);
      break;
    case MSG_CONNECT:
      assert(sizeof(connect_msg) == msg->hdr.size);
      manager_connect(n,endpt,(connect_msg*)msg->data,msg->hdr.source);
      break;
    case MSG_READ:
      assert(sizeof(read_msg) == msg->hdr.size);
      manager_read(n,endpt,(read_msg*)msg->data);
      break;
    case MSG_WRITE:
      assert(sizeof(write_msg) <= msg->hdr.size);
      manager_write(n,endpt,(write_msg*)msg->data,msg->hdr.source);
      break;
    case MSG_FINWRITE:
      assert(sizeof(finwrite_msg) == msg->hdr.size);
      manager_finwrite(n,endpt,(finwrite_msg*)msg->data,msg->hdr.source);
      break;
    case MSG_DELETE_CONNECTION:
      assert(sizeof(delete_connection_msg) == msg->hdr.size);
      manager_delete_connection(n,endpt,(delete_connection_msg*)msg->data,msg->hdr.source);
      break;
    case MSG_DELETE_LISTENER:
      assert(sizeof(delete_listener_msg) == msg->hdr.size);
      manager_delete_listener(n,endpt,(delete_listener_msg*)msg->data,msg->hdr.source);
      break;
    case MSG_CONNECT_RESPONSE:
      assert(sizeof(connect_response_msg) == msg->hdr.size);
      manager_connect_response(n,&mgr,endpt,(connect_response_msg*)msg->data);
      break;
      /* FIXME: also need to support CONNECTION_CLOSED */
    case MSG_READ_RESPONSE:
      assert(sizeof(read_response_msg) <= msg->hdr.size);
      manager_read_response(n,&mgr,endpt,(read_response_msg*)msg->data);
      break;
    case MSG_WRITE_RESPONSE:
      /* ignore */
      break;
    case MSG_JCMD:
      manager_jcmd(n,&mgr,endpt,msg);
      break;
    case MSG_STARTGC:
      assert(sizeof(startgc_msg) <= msg->hdr.size);
      manager_startgc(n,endpt,(startgc_msg*)msg->data,msg->hdr.source);
      break;
    case MSG_GET_TASKS:
      assert(sizeof(get_tasks_msg) == msg->hdr.size);
      manager_get_tasks(n,endpt,(get_tasks_msg*)msg->data);
      break;
    default:
      fatal("manager: unexpected message %s",msg_names[msg->hdr.tag]);
      break;
    }
    message_free(msg);
  }

  array_free(mgr.jresponse);
  array_free(mgr.jsendbuf);
}

void start_manager(node *n)
{
  node_add_thread2(n,"manager",manager_thread,NULL,NULL,MANAGER_ID,0);
}
