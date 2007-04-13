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

static task *add_task(node *n, int pid, int groupsize, const char *bcdata, int bcsize)
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

static void listen_callback(struct node *n, void *data, int event,
                            connection *conn, endpoint *endpt)
{
  if (EVENT_CONN_ACCEPTED == event) {
    listener *l = conn->l;
    accept_response_msg arm;
    assert(!socketid_isnull(&l->sockid));
    assert(!endpointid_isnull(&l->owner));

    conn->dontread = 1;
    conn->owner = l->owner;
    conn->isreg = 1;

    /* send message */
    assert(0 != l->accept_frameid);
    arm.ioid = l->accept_frameid;
    arm.sockid = conn->sockid;
    snprintf(arm.hostname,HOSTNAME_MAX,"%s",conn->hostname);
    arm.hostname[HOSTNAME_MAX] = '\0';
    arm.port = conn->port;

    node_send_locked(n,MANAGER_ID,l->owner,MSG_ACCEPT_RESPONSE,&arm,sizeof(arm));

    l->accept_frameid = 0;
    assert(!l->dontaccept);
    l->dontaccept = 1;
    node_notify(n);
  }
}

static void manager_listen(node *n, endpoint *endpt, listen_msg *m, endpointid source)
{
  listener *l;
  listen_response_msg lrm;

  l = node_listen(n,m->ip,m->port,listen_callback,NULL,1,0,&m->owner,lrm.errmsg,ERRMSG_MAX);
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

  node_send(n,MANAGER_ID,source,MSG_LISTEN_RESPONSE,&lrm,sizeof(lrm));
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

  lock_node(n);
  conn = node_connect_locked(n,m->hostname,INADDR_ANY,m->port,0,crm.errmsg,ERRMSG_MAX);
  crm.errmsg[ERRMSG_MAX] = '\0';
  if (NULL == conn) {
    crm.ioid = m->ioid;
    crm.event = EVENT_CONN_FAILED;
    memset(&crm.sockid,0,sizeof(crm.sockid));

    node_send_locked(n,MANAGER_ID,source,MSG_CONNECT_RESPONSE,&crm,sizeof(crm));
  }
  else {
    assert(0 == conn->frameids[CONNECT_FRAMEADDR]);
    conn->frameids[CONNECT_FRAMEADDR] = m->ioid;
    conn->dontread = 1;
    conn->owner = m->owner;
  }
  unlock_node(n);
}

static void manager_read(node *n, endpoint *endpt, read_msg *m)
{
  connection *conn;

  lock_node(n);
  /* If the connection doesn't exist any more, ignore the request - the caller is about to
     receive a CONNECTION_EVENT MESSAGE */
  if (NULL != (conn = get_connection(n,m->sockid))) {
    assert(conn->canread);
    assert(!conn->collected);
    assert(conn->dontread);
    assert(0 == conn->frameids[READ_FRAMEADDR]);

    conn->frameids[READ_FRAMEADDR] = m->ioid;
    conn->dontread = 0;
    node_notify(n);
  }
  unlock_node(n);
}

static void manager_write(node *n, endpoint *endpt, write_msg *m, endpointid source)
{
  connection *conn;

  lock_node(n);

  /* If the connection doesn't exist any more, ignore the request - the caller is about to
     receive a CONNECTION_EVENT MESSAGE */
  if (NULL != (conn = get_connection(n,m->sockid))) {
    assert(!conn->collected);

    array_append(conn->sendbuf,m->data,m->len);

    assert(0 == conn->frameids[WRITE_FRAMEADDR]);

    /* The calling frame will block when it sends us the WRITE message. If the write buffer
       still has room left for more data, wake it up immediately. Otherwise, keep it blocked
       until some of the data has been written. */
    if (IOSIZE > conn->sendbuf->nbytes) {
      write_response_msg wrm;
      wrm.ioid = m->ioid;

      node_send_locked(n,MANAGER_ID,source,MSG_WRITE_RESPONSE,&wrm,sizeof(wrm));
    }
    else {
      conn->frameids[WRITE_FRAMEADDR] = m->ioid;
    }

    node_notify(n);
  }
  unlock_node(n);
}

static void manager_finwrite(node *n, endpoint *endpt, finwrite_msg *m, endpointid source)
{
  connection *conn;

  lock_node(n);

  /* If the connection doesn't exist any more, ignore the request - the caller is about to
     receive a CONNECTION_EVENT MESSAGE */
  if (NULL != (conn = get_connection(n,m->sockid))) {
    finwrite_response_msg frm;
    assert(!conn->collected);
    conn->finwrite = 1;
    node_notify(n);

    frm.ioid = m->ioid;

    node_send_locked(n,MANAGER_ID,source,MSG_FINWRITE_RESPONSE,&frm,sizeof(frm));
  }
  unlock_node(n);
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
  unlock_node(n);

  if (NULL != l)
    node_remove_listener(n,l);
}

static void manager_thread(node *n, endpoint *endpt, void *arg)
{
  message *msg;
  int done = 0;

  while (!done) {
    msg = endpoint_next_message(endpt,-1);
    switch (msg->hdr.tag) {
    case MSG_NEWTASK: {
      newtask_msg *ntmsg;
      task *newtsk;
      if (sizeof(newtask_msg) > msg->hdr.size)
        fatal("NEWTASK: invalid message size");
      ntmsg = (newtask_msg*)msg->data;
      if (sizeof(newtask_msg)+ntmsg->bcsize > msg->hdr.size)
        fatal("NEWTASK: invalid bytecode size");

      node_log(n,LOG_INFO,"NEWTASK pid = %d, groupsize = %d, bcsize = %d",
               ntmsg->tid,ntmsg->groupsize,ntmsg->bcsize);

      newtsk = add_task(n,ntmsg->tid,ntmsg->groupsize,ntmsg->bcdata,ntmsg->bcsize);
      node_send(n,endpt->epid.localid,msg->hdr.source,MSG_NEWTASKRESP,
                &newtsk->endpt->epid.localid,sizeof(int));
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
          endpoint_link(newtsk->endpt,initmsg->idmap[i]);

      unlock_node(n);

      for (i = 0; i < newtsk->groupsize; i++) {
        endpointid_str str;
        print_endpointid(str,initmsg->idmap[i]);
        node_log(n,LOG_INFO,"INITTASK: idmap[%d] = %s",i,str);
      }

      node_send(n,endpt->epid.localid,msg->hdr.source,MSG_INITTASKRESP,&resp,sizeof(int));
      break;
    }
    case MSG_STARTTASK: {
      task *newtsk;
      int resp = 0;
      int localid;
      char semdata;
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

      node_send(n,endpt->epid.localid,msg->hdr.source,MSG_STARTTASKRESP,&resp,sizeof(int));
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
      assert(sizeof(finwrite_msg) <= msg->hdr.size);
      manager_finwrite(n,endpt,(finwrite_msg*)msg->data,msg->hdr.source);
      break;
    case MSG_DELETE_CONNECTION:
      assert(sizeof(delete_connection_msg) <= msg->hdr.size);
      manager_delete_connection(n,endpt,(delete_connection_msg*)msg->data,msg->hdr.source);
      break;
    case MSG_DELETE_LISTENER:
      assert(sizeof(delete_listener_msg) <= msg->hdr.size);
      manager_delete_listener(n,endpt,(delete_listener_msg*)msg->data,msg->hdr.source);
      break;
    default:
      fatal("Manager received invalid message: %d",msg->hdr.tag);
      break;
    }
    message_free(msg);
  }
}

void start_manager(node *n)
{
  node_add_thread(n,MANAGER_ID,MANAGER_ENDPOINT,0,manager_thread,NULL,NULL);
}
