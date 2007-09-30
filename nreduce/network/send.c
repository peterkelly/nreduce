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
 * $Id: manager.c 603 2007-08-14 02:44:16Z pmkelly $
 *
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "netprivate.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

void send_listen(endpoint *endpt, endpointid epid, in_addr_t ip, int port,
                 endpointid owner, int ioid)
{
  listen_msg lm;
  lm.ip = ip;
  lm.port = port;
  lm.owner = owner;
  lm.ioid = ioid;
  endpoint_send(endpt,epid,MSG_LISTEN,&lm,sizeof(lm));
}

void send_accept(endpoint *endpt, socketid sockid, int ioid)
{
  accept_msg am;
  am.sockid = sockid;
  am.ioid = ioid;
  endpoint_send(endpt,sockid.coordid,MSG_ACCEPT,&am,sizeof(am));
}

void send_connect(endpoint *endpt, endpointid epid,
                  const char *hostname, int port, endpointid owner, int ioid)
{
  connect_msg cm;
  snprintf(cm.hostname,HOSTNAME_MAX,hostname);
  cm.port = port;
  cm.owner = owner;
  cm.ioid = ioid;
  endpoint_send(endpt,epid,MSG_CONNECT,&cm,sizeof(cm));
}

void send_connpair(endpoint *endpt, endpointid epid, int ioid)
{
  connpair_msg pm;
  pm.ioid = ioid;
  endpoint_send(endpt,epid,MSG_CONNPAIR,&pm,sizeof(pm));
}

void send_read(endpoint *endpt, socketid sid, int ioid)
{
  read_msg rm;
  rm.sockid = sid;
  rm.ioid = ioid;
  endpoint_send(endpt,sid.coordid,MSG_READ,&rm,sizeof(rm));
}

void send_write(endpoint *endpt, socketid sockid, int ioid, const char *data, int len)
{
  int msglen = sizeof(write_msg)+len;
  write_msg *wm = (write_msg*)malloc(msglen);
  wm->sockid = sockid;
  wm->ioid = ioid;
  wm->len = len;
  memcpy(wm->data,data,len);
  endpoint_send(endpt,sockid.coordid,MSG_WRITE,wm,msglen);
  free(wm);
}

void send_delete_connection(endpoint *endpt, socketid sockid)
{
  delete_connection_msg dcm;
  dcm.sockid = sockid;
  endpoint_send(endpt,sockid.coordid,MSG_DELETE_CONNECTION,&dcm,sizeof(dcm));
}

void send_delete_listener(endpoint *endpt, socketid sockid)
{
  delete_listener_msg dlm;
  dlm.sockid = sockid;
  if (!socketid_isnull(&sockid))
    endpoint_send(endpt,sockid.coordid,MSG_DELETE_LISTENER,&dlm,sizeof(dlm));
}
