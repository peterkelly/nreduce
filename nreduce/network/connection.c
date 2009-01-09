/*
 * This file is part of the nreduce project
 * Copyright (C) 2006-2008 Peter Kelly (pmk@cs.adelaide.edu.au)
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
 * $Id: node.c 702 2008-01-21 02:54:05Z pmkelly $
 *
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

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
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/select.h>
#include <sys/time.h>
#include <time.h>
#include <fcntl.h>
#include <unistd.h>
#include <signal.h>

const char *connection_states[12] = {
  "START",
  "WAITING",
  "READY",
  "FAILED",
  "CONNECTING",
  "CONNECTED",
  "CONNECTED_DONE_WRITING",
  "ACCEPTED",
  "ACCEPTED_DONE_WRITING",
  "ACCEPTED_DONE_READING",
  "FINISHED",
  "END",
};

const char *connection_events[13] = {
  "AUTO",
  "REQUESTED",
  "SLOT_AVAILABLE",
  "ATTEMPT_FAILED",
  "ASYNC_STARTED",
  "ASYNC_FAILED",
  "ASYNC_OK",
  "FINWRITE",
  "FINREAD",
  "ERROR",
  "DELETE",
  "READ",
  "CLIENT_CONNECTED",
};

static void do_shutdown(node *n, connection *conn, int how, const char *howstr)
{
  if ((0 > shutdown(conn->sock,how)) && (ENOTCONN != errno)) {
    node_log(n,LOG_WARNING,"shutdown(%s) on %s:%d failed: %s",
             howstr,conn->hostname,conn->port,strerror(errno));
  }
}

static void add_connection_stats(node *n, connection *conn)
{
  list *l;
  netstats *ns = NULL;
  assert(NODE_ALREADY_LOCKED(n));

  for (l = n->p->stats; l; l = l->next) {
    netstats *tmp = (netstats*)l->data;
    if ((tmp->ip == conn->ip) && (tmp->port == conn->port)) {
      ns = tmp;
      break;
    }
  }

  if (NULL == ns) {
    ns = (netstats*)calloc(1,sizeof(netstats));
    ns->ip = conn->ip;
    ns->port = conn->port;
    list_push(&n->p->stats,ns);
  }

  ns->totalread += conn->totalread;
  ns->totalwritten += conn->totalwritten;
}

static void remove_connection(node *n, connection *conn)
{
  assert(NODE_ALREADY_LOCKED(n));
  if (!conn->isreg)
    node_log(n,LOG_INFO,"Removing connection %s",conn->hostname);
  add_connection_stats(n,conn);
  if (0 <= conn->sock)
    list_push(&n->p->toclose,(void*)conn->sock);
  connhash_remove(n,conn);
  if (CS_WAITING == conn->state) {
    llist_remove(&conn->si->waiting_connections,conn);
  }
  else {
    llist_remove(&n->p->connections,conn);
  }
  free(conn->hostname);
  if (NULL != conn->recvbuf)
    array_free(conn->recvbuf);
  if (NULL != conn->sendbuf)
    array_free(conn->sendbuf);

  /* Possible race condition: port is re-used before socket closed */
  if (0 <= conn->outport)
    portset_releaseport(&n->p->outports,conn->outport);

  free(conn);
}

static int initiate_connection(connection *conn)
{
  struct sockaddr_in addr;
  int r;

  assert(NODE_ALREADY_LOCKED(conn->n));

  addr.sin_family = AF_INET;
  addr.sin_port = htons(conn->port);
  memset(&addr.sin_zero,0,8);
  addr.sin_addr.s_addr = conn->ip;

  if (0 > (conn->sock = socket(AF_INET,SOCK_STREAM,0))) {
    conn->errn = errno;
    snprintf(conn->errmsg,ERRMSG_MAX,"socket: %s",strerror(errno));
    perror("socket");
    return -1;
  }

  if (0 > set_non_blocking(conn->sock)) {
    snprintf(conn->errmsg,ERRMSG_MAX,"cannot set non-blocking socket");
    return -1;
  }

  if (0 < conn->n->p->outports.max) {
    /* Choose our own outgoing port for the connection */
    int yes = 1;
    if (0 > setsockopt(conn->sock,SOL_SOCKET,SO_REUSEADDR,&yes,sizeof(int))) {
      snprintf(conn->errmsg,ERRMSG_MAX,"setsockopt SO_REUSEADDR: %s",strerror(errno));
      return -1;
    }

    if (0 > (conn->outport = bind_client_port(conn->n,conn->sock))) {
      snprintf(conn->errmsg,ERRMSG_MAX,"failed to bind client port");
      return -1;
    }
  }

  /* We expect the connect() call to not succeed immediately, because the socket is in
     non-blocking mode */
  r = connect(conn->sock,(struct sockaddr*)&addr,sizeof(struct sockaddr));
  assert(0 > r); /* we don't expect direct success */
  if (EINPROGRESS != errno) {
    conn->errn = errno;
    snprintf(conn->errmsg,ERRMSG_MAX,"connect: %s",strerror(errno));
    return -1;
  }

#ifdef DEBUG_SHORT_KEEPALIVE
  /* Set a small keepalive interval so connection timeouts can be tested easily */
  if (0 > set_keepalive(conn->n,conn->sock,2,conn->errmsg,ERRMSG_MAX))
    return -1;
#endif

  conn->outgoing = 1;
  node_notify(conn->n);
  return 0;
}

#define WAITING_STATE(state) \
  (CS_WAITING == (state))

#define OPENING_STATE(state) \
  ((CS_READY == (state)) || \
   (CS_CONNECTING == (state)) || \
   (CS_CONNECTED == (state)) || \
   (CS_CONNECTED_DONE_WRITING == (state)))

#define ACCEPTED_STATE(state) \
  ((CS_ACCEPTED == (state)) || \
   (CS_ACCEPTED_DONE_READING == (state)) || \
   (CS_ACCEPTED_DONE_WRITING == (state)))

static void enter_waiting(connection *conn)
{
  assert(0 <= conn->si->nwaiting);
  conn->si->nwaiting++;
  node_log(conn->n,LOG_DEBUG1,"nwaiting("IP_FORMAT") = %d",IP_ARGS(conn->ip),conn->si->nwaiting);
  assert(!conn->iswaiting);
  conn->iswaiting = 1;
  llist_remove(&conn->n->p->connections,conn);
  llist_append(&conn->si->waiting_connections,conn);
}

static void exit_waiting(connection *conn)
{
  conn->si->nwaiting--;
  node_log(conn->n,LOG_DEBUG1,"nwaiting("IP_FORMAT") = %d",IP_ARGS(conn->ip),conn->si->nwaiting);
  assert(0 <= conn->si->nwaiting);
  assert(conn->iswaiting);
  conn->iswaiting = 0;
  llist_remove(&conn->si->waiting_connections,conn);
  llist_append(&conn->n->p->connections,conn);
}

static void enter_opening(connection *conn)
{
  assert(0 <= conn->si->nopening);
  conn->si->nopening++;
  node_log(conn->n,LOG_DEBUG1,"nopening("IP_FORMAT") = %d",IP_ARGS(conn->ip),conn->si->nopening);
  assert(!conn->isopening);
  conn->isopening = 1;
}

static void exit_opening(connection *conn)
{
  conn->si->nopening--;
  node_log(conn->n,LOG_DEBUG1,"nopening("IP_FORMAT") = %d",IP_ARGS(conn->ip),conn->si->nopening);
  assert(0 <= conn->si->nopening);
  assert(conn->isopening);
  conn->isopening = 0;
}

static void enter_active(connection *conn)
{
  assert(0 <= conn->si->naccepted);
  conn->si->naccepted++;
  node_log(conn->n,LOG_DEBUG1,"naccepted("IP_FORMAT") = %d",IP_ARGS(conn->ip),conn->si->naccepted);
  assert(!conn->isaccepted);
  conn->isaccepted = 1;
}

static void exit_active(connection *conn)
{
  conn->si->naccepted--;
  node_log(conn->n,LOG_DEBUG1,"naccepted("IP_FORMAT") = %d",IP_ARGS(conn->ip),conn->si->naccepted);
  assert(0 <= conn->si->naccepted);
  assert(conn->isaccepted);
  conn->isaccepted = 0;
}

static void connection_state(connection *conn, int newstate)
{
  if (WAITING_STATE(newstate) && !WAITING_STATE(conn->state))
    enter_waiting(conn);
  else if (!WAITING_STATE(newstate) && WAITING_STATE(conn->state))
    exit_waiting(conn);

  if (OPENING_STATE(newstate) && !OPENING_STATE(conn->state))
    enter_opening(conn);
  else if (!OPENING_STATE(newstate) && OPENING_STATE(conn->state))
    exit_opening(conn);

  if (ACCEPTED_STATE(newstate) && !ACCEPTED_STATE(conn->state))
    enter_active(conn);
  else if (!ACCEPTED_STATE(newstate) && ACCEPTED_STATE(conn->state))
    exit_active(conn);

  if (CS_CONNECTED == newstate)
    create_connection_buffers(conn);

  conn->state = newstate;
  connection_fsm(conn,CE_AUTO);
}

void connection_fsm(connection *conn, int event)
{
/*   printf("connection %d: state %s event %s\n",conn->sockid.sid, */
/*          connection_states[conn->state-CS_MIN],connection_events[event-CE_MIN]); */

  assert(NODE_ALREADY_LOCKED(conn->n));

  switch (conn->state) {

  case CS_START:
    switch (event) {
    case CE_REQUESTED:
      connection_state(conn,CS_WAITING);
      return;
    case CE_DELETE:
      connection_state(conn,CS_END);
      return;
    case CE_CLIENT_CONNECTED:
      connection_state(conn,CS_CONNECTED);
      return;
    }
    break;

  case CS_WAITING:
    switch (event) {
    case CE_SLOT_AVAILABLE:
      connection_state(conn,CS_READY);
      if (0 <= initiate_connection(conn))
        connection_fsm(conn,CE_ASYNC_STARTED);
      else
        connection_fsm(conn,CE_ATTEMPT_FAILED);
      return;
    case CE_DELETE:
      connection_state(conn,CS_END);
      return;
    }
    break;

  case CS_READY:
    switch (event) {
    case CE_ASYNC_STARTED:
      connection_state(conn,CS_CONNECTING);
      return;
    case CE_ATTEMPT_FAILED:
      connection_state(conn,CS_FAILED);
      return;
    case CE_DELETE:
      connection_state(conn,CS_END);
      return;
    }
    break;

  case CS_FAILED:
    switch (event) {
    case CE_AUTO:
      notify_connect(conn->n,conn,1);
      connection_state(conn,CS_END);
      return;
    }
    break;

  case CS_CONNECTING:
    switch (event) {
    case CE_ASYNC_OK:
      connection_state(conn,CS_CONNECTED);
      notify_connect(conn->n,conn,0);
      return;
    case CE_ASYNC_FAILED:
      connection_state(conn,CS_FAILED);
      return;
    case CE_DELETE:
      connection_state(conn,CS_END);
      return;
    }
    break;

  case CS_CONNECTED:
    switch (event) {
    case CE_ERROR:
      conn->haderror = 1;
      connection_state(conn,CS_FINISHED);
      return;
    case CE_DELETE:
      connection_state(conn,CS_FINISHED);
      return;
    case CE_READ:
      connection_state(conn,CS_ACCEPTED);
      return;
    case CE_FINREAD:
      do_shutdown(conn->n,conn,SHUT_RD,"SHUT_RD");
      connection_state(conn,CS_ACCEPTED_DONE_READING);
      return;
    case CE_FINWRITE:
      do_shutdown(conn->n,conn,SHUT_WR,"SHUT_WR");
      connection_state(conn,CS_CONNECTED_DONE_WRITING);
      return;
    }
    break;

  case CS_CONNECTED_DONE_WRITING:
    switch (event) {
    case CE_ERROR:
      conn->haderror = 1;
      connection_state(conn,CS_FINISHED);
      return;
    case CE_DELETE:
      connection_state(conn,CS_FINISHED);
      return;
    case CE_FINREAD:
      do_shutdown(conn->n,conn,SHUT_RD,"SHUT_RD");
      connection_state(conn,CS_FINISHED);
      return;
    case CE_FINWRITE:
      /* ignore */
      return;
    case CE_READ:
      connection_state(conn,CS_ACCEPTED_DONE_WRITING);
      return;
    }
    break;

  case CS_ACCEPTED:
    switch (event) {
    case CE_ERROR:
      conn->haderror = 1;
      connection_state(conn,CS_FINISHED);
      return;
    case CE_DELETE:
      connection_state(conn,CS_FINISHED);
      return;
    case CE_READ:
      /* ignore */
      return;
    case CE_FINREAD:
      do_shutdown(conn->n,conn,SHUT_RD,"SHUT_RD");
      connection_state(conn,CS_ACCEPTED_DONE_READING);
      return;
    case CE_FINWRITE:
      do_shutdown(conn->n,conn,SHUT_WR,"SHUT_WR");
      connection_state(conn,CS_ACCEPTED_DONE_WRITING);
      return;
    }
    break;

  case CS_ACCEPTED_DONE_WRITING:
    switch (event) {
    case CE_ERROR:
      conn->haderror = 1;
      connection_state(conn,CS_FINISHED);
      return;
    case CE_DELETE:
      connection_state(conn,CS_FINISHED);
      return;
    case CE_FINWRITE:
      /* ignore */
      return;
    case CE_READ:
      /* ignore */
      return;
    case CE_FINREAD:
      do_shutdown(conn->n,conn,SHUT_RD,"SHUT_RD");
      connection_state(conn,CS_FINISHED);
      return;
    }
    break;

  case CS_ACCEPTED_DONE_READING:
    switch (event) {
    case CE_ERROR:
      conn->haderror = 1;
      connection_state(conn,CS_FINISHED);
      return;
    case CE_DELETE:
      connection_state(conn,CS_FINISHED);
      return;
    case CE_FINREAD:
      /* ignore */
      return;
    case CE_FINWRITE:
      do_shutdown(conn->n,conn,SHUT_WR,"SHUT_WR");
      connection_state(conn,CS_FINISHED);
      return;
    }
    break;

  case CS_FINISHED:
    switch (event) {
    case CE_AUTO:
      notify_closed(conn->n,conn,conn->haderror);
      connection_state(conn,CS_END);
      return;
    }
    break;

  case CS_END:
    switch (event) {
    case CE_AUTO:
      remove_connection(conn->n,conn);
      return;
    }
    break;

  default:
    fatal("Invalid state %d",conn->state);
  }

  if (CE_AUTO != event)
    fatal("Invalid event %d in state %d",event,conn->state);
}

void portset_init(portset *pb, int min, int max)
{
  pb->min = min;
  pb->max = max;
  pb->upto = min;

  pb->size = max+1-min;
  pb->ports = (int*)calloc(pb->size,sizeof(int));
  pb->count = 0;
  pb->start = 0;
  pb->end = 0;
}

void portset_destroy(portset *pb)
{
  free(pb->ports);
}

/* Choose a port to use for an outgoing connection. If we have not yet used up all ports in the
   range, the next port is picked. Otherwise, we choose the least recently used port. */
int portset_allocport(portset *pb)
{
  assert(0 <= pb->count);
  if (pb->upto <= pb->max) {
    /* Pick a port that has not yet been used, in preference to an existing one (which may still
       be be in TIME_WAIT) */
    return pb->upto++;
  }
  else if (0 < pb->count) {
    /* Pick a previously used port. It may still be in TIME_WAIT, but by picking the least
       recently used, we maximise the amount of time connections can stay in TIME_WAIT. */
    int r = pb->ports[pb->start];
    pb->start = (pb->start+1) % pb->size;
    pb->count--;
    return r;
  }
  else {
    return -1;
  }
}

/* Indicate that we have finished using a port, and it can be re-used again for a subsequent
   connection. */
void portset_releaseport(portset *pb, int port)
{
  pb->ports[pb->end] = port;
  pb->end = (pb->end+1) % pb->size;
  pb->count++;
  assert(pb->count <= pb->size);
}

int bind_client_port(node *n, int sock)
{
  while (1) {

    int clientport = portset_allocport(&n->p->outports);
    if (0 > clientport) {
      node_log(n,LOG_ERROR,"Outgoing ports exhausted");
      return -1;
    }

    struct sockaddr_in local_addr;
    local_addr.sin_family = AF_INET;
    local_addr.sin_port = htons(clientport);
    local_addr.sin_addr.s_addr = INADDR_ANY;
    memset(&local_addr.sin_zero,0,8);

    if (0 == bind(sock,(struct sockaddr*)&local_addr,sizeof(struct sockaddr))) {
      /* Port successfully bound */
      return clientport;
    }

    if (EADDRINUSE == errno) {
      /* Port in use by another application. We don't put this back in the port set, since we
         don't want it to be used again. */
    }
    else {
      node_log(n,LOG_ERROR,"bind: %s",strerror(errno));
      return -1;
    }
  }
}
