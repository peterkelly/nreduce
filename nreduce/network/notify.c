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
 * $Id: manager.c 610 2007-08-22 06:07:12Z pmkelly $
 *
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "compiler/bytecode.h"
#include "src/nreduce.h"
#include "node.h"
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

void notify_accept(node *n, connection *conn)
{
  listener *l = conn->l;
  accept_response_msg arm;

  if (!l->notify)
    return;

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

  node_send_locked(n,IO_ID,l->owner,MSG_ACCEPT_RESPONSE,&arm,sizeof(arm));

  l->accept_frameid = 0;
  assert(!l->dontaccept);
  l->dontaccept = 1;
  node_notify(n);
}

/* FIXME: for all regular connections, perhaps we should assert that we have a frameid set */
void notify_connect(node *n, connection *conn, int error)
{
  if (0 < conn->frameids[CONNECT_FRAMEADDR]) {
    connect_response_msg crm;

    crm.ioid = conn->frameids[CONNECT_FRAMEADDR];
    crm.error = error;
    if (error)
      memset(&crm.sockid,0,sizeof(crm.sockid));
    else
      crm.sockid = conn->sockid;
    memcpy(crm.errmsg,conn->errmsg,sizeof(crm.errmsg));

    conn->frameids[CONNECT_FRAMEADDR] = 0;

    node_send_locked(n,IO_ID,conn->owner,MSG_CONNECT_RESPONSE,&crm,sizeof(crm));
  }

  if (error)
    handle_disconnection(n,conn);
}

void notify_read(node *n, connection *conn)
{
  if (conn->isreg && (0 < conn->frameids[READ_FRAMEADDR])) {
    int rrmlen = sizeof(read_response_msg)+conn->recvbuf->nbytes;
    read_response_msg *rrm = (read_response_msg*)malloc(rrmlen);

    rrm->ioid = conn->frameids[READ_FRAMEADDR];
    rrm->sockid = conn->sockid;
    rrm->len = conn->recvbuf->nbytes;
    memcpy(rrm->data,conn->recvbuf->data,conn->recvbuf->nbytes);

    node_send_locked(n,IO_ID,conn->owner,MSG_READ_RESPONSE,rrm,rrmlen);

    conn->frameids[READ_FRAMEADDR] = 0;
    conn->recvbuf->nbytes = 0;
    conn->dontread = 1;
    free(rrm);
  }
}

void notify_closed(node *n, connection *conn, int error)
{
  if (0 != conn->owner.localid) {
    connection_event_msg cem;
    cem.sockid = conn->sockid;
    cem.error = error;
    memcpy(cem.errmsg,conn->errmsg,sizeof(cem.errmsg));
    node_send_locked(n,IO_ID,conn->owner,MSG_CONNECTION_CLOSED,&cem,sizeof(cem));
  }
  handle_disconnection(n,conn);
}

void notify_write(node *n, connection *conn)
{
  if ((0 < conn->frameids[WRITE_FRAMEADDR]) && (n->iosize > conn->sendbuf->nbytes)) {
    write_response_msg wrm;
    wrm.ioid = conn->frameids[WRITE_FRAMEADDR];
    conn->frameids[WRITE_FRAMEADDR] = 0;
    node_send_locked(n,IO_ID,conn->owner,MSG_WRITE_RESPONSE,&wrm,sizeof(wrm));
  }
}
