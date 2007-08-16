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

#define MESSAGES_C

#include "messages.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

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
  "CONSOLE_DATA",
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
  "CONNECTION_CLOSED",
  "FINWRITE_RESPONSE",
  "DELETE_CONNECTION",
  "DELETE_LISTENER",
  "STARTGC",
  "STARTGC_RESPONSE",
  "STARTDISTGCACK",
  "GET_TASKS",
  "GET_TASKS_RESPONSE",
  "REPORT_ERROR",
  "PING",
  "PONG",
  "JCMD",
  "JCMD_RESPONSE",
};

void send_read(endpoint *endpt, endpointid epid, socketid sid, int ioid)
{
  read_msg rm;
  rm.sockid = sid;
  rm.ioid = ioid;
  endpoint_send(endpt,epid,MSG_READ,&rm,sizeof(rm));
}

void send_write(endpoint *endpt, endpointid epid, socketid sockid, int ioid,
                const char *data, int len)
{
  int msglen = sizeof(write_msg)+len;
  write_msg *wm = (write_msg*)malloc(msglen);
  wm->sockid = sockid;
  wm->ioid = ioid;
  wm->len = len;
  memcpy(wm->data,data,len);
  endpoint_send(endpt,epid,MSG_WRITE,wm,msglen);
  free(wm);
}

void send_jcmd(endpoint *endpt, int ioid, int oneway, const char *data, int cmdlen)
{
  int msglen = sizeof(jcmd_msg)+cmdlen;
  jcmd_msg *jcm = (jcmd_msg*)calloc(msglen,1);
  jcm->ioid = ioid;
  jcm->oneway = oneway;
  jcm->cmdlen = cmdlen;
  memcpy(&jcm->cmd,data,cmdlen);
  endpoint_send(endpt,endpt->n->managerid,MSG_JCMD,jcm,msglen);
  free(jcm);
}

void send_jcmd_response(endpoint *endpt, endpointid epid, int ioid, int error,
                         const char *data, int len)
{
  int msglen = sizeof(jcmd_response_msg)+len;
  jcmd_response_msg *resp = (jcmd_response_msg*)calloc(msglen,1);
  resp->ioid = ioid;
  resp->error = error;
  resp->len = len;
  memcpy(&resp->data,data,len);
  endpoint_send(endpt,epid,MSG_JCMD_RESPONSE,resp,msglen);
  free(resp);
}
