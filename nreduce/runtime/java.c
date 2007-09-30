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
 * $Id: manager.c 608 2007-08-21 11:59:50Z pmkelly $
 *
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "compiler/bytecode.h"
#include "src/nreduce.h"
#include "runtime.h"
#include "network/node.h"
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

#define JBRIDGE_PORT    2001 /* FIXME: make this configurable */

typedef struct javacmd {
  int ioid;
  int oneway;
  endpointid epid;
} javacmd;

typedef struct javath {
  int jconnected;
  int jconnecting;
  array *jsendbuf;
  list *jcmds;
  socketid jcon;
  array *jresponse;
} javath;

int get_callinfo(task *tsk, pntr obj, pntr method, char **targetname, char **methodname)
{
  if (0 > array_to_string(method,methodname))
    return set_error(tsk,"jcall: method is not a string");

  if ((CELL_SYSOBJECT == pntrtype(obj)) && (SYSOBJECT_JAVA == psysobject(obj)->type)) {
    char *str = malloc(100);
    sprintf(str,"@%d",psysobject(obj)->jid.jid);
    *targetname = str;
  }
  else if ((CELL_AREF == pntrtype(obj)) || (CELL_CONS == pntrtype(obj))) {
    if (0 > array_to_string(obj,targetname))
      return set_error(tsk,"jcall: class name is not a string");
  }
  else {
    return set_error(tsk,"jcall: first arg must be a java object or class name");
  }
  return 1;
}

static int serialise_arg(task *tsk, array *arr, int argno, pntr val)
{
  char *str;
  char *escaped;
  sysobject *so;
  switch (pntrtype(val)) {
  case CELL_NUMBER:
    array_printf(arr," %f",pntrdouble(val));
    return 1;
  case CELL_CONS:
  case CELL_AREF:
    if (0 <= array_to_string(val,&str)) {
      escaped = escape(str);
      array_printf(arr," \"%s\"",escaped);
      free(escaped);
      free(str);
      return 1;
    }
    break;
  case CELL_NIL:
    array_printf(arr," nil");
    return 1;
  case CELL_SYSOBJECT:
    so = psysobject(val);
    if (SYSOBJECT_JAVA == so->type) {
      array_printf(arr," @%d",so->jid.jid);
      return 1;
    }
    break;
  }
  return set_error(tsk,"jcall: argument %d invalid (%s)",argno,cell_types[pntrtype(val)]);
}

int serialise_args(task *tsk, pntr args, array *arr)
{
  pntr *argvalues = NULL;
  int nargs;
  int i;

  if (0 > (nargs = flatten_list(args,&argvalues)))
    return set_error(tsk,"jcall: args is not a valid list");

  for (i = 0; i < nargs; i++)
    if (!serialise_arg(tsk,arr,i,argvalues[i]))
      break;

  free(argvalues);
  return (i == nargs);
}

pntr decode_java_response(task *tsk, const char *str, endpointid source)
{
  int len = strlen(str);
  if ((2 <= len) && ('"' == str[0]) && ('"' == str[len-1])) {
    char *sub = substring(str,1,len-1);
    char *unescaped = unescape(sub);
    pntr p = string_to_array(tsk,unescaped);
    free(unescaped);
    free(sub);
    return p;
  }
  else if (!strcmp(str,"nil")) {
    return tsk->globnilpntr;
  }
  else if (!strcmp(str,"false")) {
    return tsk->globnilpntr;
  }
  else if (!strcmp(str,"true")) {
    return tsk->globtruepntr;
  }
  else if ((1 < len) && ('@' == str[0])) {
    int id = atoi(str+1);
    sysobject *so = new_sysobject(tsk,SYSOBJECT_JAVA);
    pntr p;
    so->jid.managerid = source;
    so->jid.jid = id;
    make_pntr(p,so->c);
    return p;
  }
  else if (!strncmp(str,"error: ",7)) {
    set_error(tsk,"jcall: %s",&str[7]);
    return tsk->globnilpntr;
  }
  else {
    char *end = NULL;
    double d = strtod(str,&end);
    if (('\0' != *str) && ('\0' == *end)) {
      pntr p;
      set_pntrdouble(p,d);
      return p;
    }
    else {
      set_error(tsk,"jcall: invalid response");
      return tsk->globnilpntr;
    }
  }
}

void send_jcmd(endpoint *endpt, int ioid, int oneway, const char *data, int cmdlen)
{
  endpointid javaid = { ip: endpt->n->listenip, port: endpt->n->listenport, localid: JAVA_ID };
  int msglen = sizeof(jcmd_msg)+cmdlen;
  jcmd_msg *jcm = (jcmd_msg*)calloc(msglen,1);
  jcm->ioid = ioid;
  jcm->oneway = oneway;
  jcm->cmdlen = cmdlen;
  memcpy(&jcm->cmd,data,cmdlen);
  endpoint_send(endpt,javaid,MSG_JCMD,jcm,msglen);
  free(jcm);
}

static void send_jcmd_response(endpoint *endpt, endpointid epid, int ioid, int error,
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

static void send_java_commands(node *n, javath *jth, endpoint *endpt)
{
  if (0 < jth->jsendbuf->nbytes) {
    char *data = jth->jsendbuf->data;
    int len = jth->jsendbuf->nbytes;
    send_write(endpt,jth->jcon,1,data,len);
    jth->jsendbuf->nbytes = 0;
  }
}

static void send_java_errors(node *n, javath *jth, endpoint *endpt)
{
  list *l;
  for (l = jth->jcmds; l; l = l->next) {
    javacmd *jc = l->data;
    send_jcmd_response(endpt,jc->epid,jc->ioid,1,NULL,0);
  }
  list_free(jth->jcmds,free);
  jth->jcmds = NULL;
  jth->jconnected = 0;
  jth->jconnecting = 0;
}

/* for notifying of connection to JVM */
static void java_connect_response(node *n, javath *jth, endpoint *endpt, connect_response_msg *m)
{
  jth->jconnecting = 0;

  if (m->error) {
    send_java_errors(n,jth,endpt);
  }
  else {
    jth->jcon = m->sockid;
    jth->jconnected = 1;
    send_java_commands(n,jth,endpt);
    send_read(endpt,jth->jcon,1);
  }
}

/* for notifying of error communicating with JVM */
static void java_connection_closed(node *n, javath *jth, endpoint *endpt,
                                      connection_event_msg *m)
{
  send_java_errors(n,jth,endpt);
}

static void java_read_response(node *n, javath *jth, endpoint *endpt, read_response_msg *m)
{
  int start = 0;
  int pos = 0;

  if (0 == m->len) {
    send_java_errors(n,jth,endpt);
    return;
  }

  array_append(jth->jresponse,m->data,m->len);
  while (pos < jth->jresponse->nbytes) {
    if ('\n' == jth->jresponse->data[pos]) {
      int len = pos-start;
      list *old = jth->jcmds;
      javacmd *jc;
      assert(jth->jcmds);
      jc = jth->jcmds->data;
      if (!jc->oneway)
        send_jcmd_response(endpt,jc->epid,jc->ioid,0,&jth->jresponse->data[start],len);
      jth->jcmds = jth->jcmds->next;
      free(jc);
      free(old);

      start = pos+1;
    }
    pos++;
  }
  array_remove_data(jth->jresponse,start);
  send_read(endpt,jth->jcon,1);
}

static void java_jcmd(node *n, javath *jth, endpoint *endpt, message *msg)
{
  jcmd_msg *m;
  javacmd *jc;
  char newline = '\n';
  assert(sizeof(jcmd_msg) <= msg->size);
  m = (jcmd_msg*)msg->data;
  assert(msg->size == sizeof(jcmd_msg)+m->cmdlen);

  array_append(jth->jsendbuf,m->cmd,m->cmdlen);
  array_append(jth->jsendbuf,&newline,1);

  jc = (javacmd*)calloc(sizeof(javacmd),1);
  jc->ioid = m->ioid;
  jc->oneway = m->oneway;
  jc->epid = msg->source;
  list_append(&jth->jcmds,jc);

  if (!jth->jconnected && !jth->jconnecting) {
    send_connect(endpt,endpt->n->iothid,"127.0.0.1",JBRIDGE_PORT,endpt->epid,1);
    jth->jconnecting = 1;
    return;
  }

  if (jth->jconnected)
    send_java_commands(n,jth,endpt);
}

void java_thread(node *n, endpoint *endpt, void *arg)
{
  int done = 0;
  javath jth;

  memset(&jth,0,sizeof(jth));
  jth.jresponse = array_new(1,0);
  jth.jsendbuf = array_new(1,0);

  while (!done) {
    message *msg = endpoint_receive(endpt,-1);
    switch (msg->tag) {
    case MSG_KILL:
      node_log(n,LOG_INFO,"Java thread received KILL");
      done = 1;
      break;
    case MSG_CONNECT_RESPONSE:
      assert(sizeof(connect_response_msg) == msg->size);
      java_connect_response(n,&jth,endpt,(connect_response_msg*)msg->data);
      break;
    case MSG_CONNECTION_CLOSED:
      assert(sizeof(connection_event_msg) == msg->size);
      java_connection_closed(n,&jth,endpt,(connection_event_msg*)msg->data);
      break;
    case MSG_READ_RESPONSE:
      assert(sizeof(read_response_msg) <= msg->size);
      java_read_response(n,&jth,endpt,(read_response_msg*)msg->data);
      break;
    case MSG_WRITE_RESPONSE:
      /* ignore */
      break;
    case MSG_JCMD:
      java_jcmd(n,&jth,endpt,msg);
      break;
    default:
      fatal("java thread: unexpected message %d",msg->tag);
      break;
    }
    message_free(msg);
  }

  array_free(jth.jresponse);
  array_free(jth.jsendbuf);
}
