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
#include "node.h"
#include "messages.h"
#include "netprivate.h"
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

typedef struct manager {
  mgr_extfun ext;
  void *extarg;
} manager;

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

static void manager_thread(node *n, endpoint *endpt, void *arg)
{
  int done = 0;
  manager *mgr = (manager*)arg;

  while (!done) {
    message *msg = endpoint_receive(endpt,-1);
    switch (msg->hdr.tag) {
    case MSG_ENDPOINT_EXIT: {
      endpoint_exit_msg *m = (endpoint_exit_msg*)msg->data;
      assert(sizeof(endpoint_exit_msg) == msg->hdr.size);
      node_handle_endpoint_exit(n,m->epid);
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
    default:
      if ((NULL == mgr->ext) || !mgr->ext(n,endpt,msg,0,mgr->extarg))
        fatal("manager: unexpected message %s",msg_names[msg->hdr.tag]);
      break;
    }
    message_free(msg);
  }

  if (mgr->ext)
    mgr->ext(n,endpt,NULL,1,mgr->extarg);

  free(mgr);
}

void start_manager(node *n, mgr_extfun ext, void *extarg)
{
  manager *mgr = (manager*)calloc(1,sizeof(manager));
  mgr->ext = ext;
  mgr->extarg = extarg;
  node_add_thread2(n,"manager",manager_thread,mgr,NULL,MANAGER_ID,0);
}
