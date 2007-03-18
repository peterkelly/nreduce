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
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <assert.h>
#include <ctype.h>
#include <stdarg.h>
#include <math.h>
#include <errno.h>
#include <unistd.h>

typedef struct console2 {
  node *n;
  endpoint *endpt;
  pthread_t thread;
} console2;

static int process_cmd(node *n, int argc, char **argv, array *out)
{
  assert(NODE_UNLOCKED(n));
  if (0 == argc)
    return 0;
  if (!strcmp(argv[0],"connections") || !strcmp(argv[0],"c")) {
    connection *c2;
    lock_node(n);
    array_printf(out,"%-30s %-6s %-6s %-8s\n","Hostname","Port","Socket","Console?");
    array_printf(out,"%-30s %-6s %-6s %-8s\n","--------","----","------","--------");
    for (c2 = n->connections.first; c2; c2 = c2->next)
      array_printf(out,"%-30s %-6d %-6d %-8s\n",
              c2->hostname,c2->port,c2->sock,
              c2->isconsole ? "Yes" : "No");
    unlock_node(n);
  }
  else if (!strcmp(argv[0],"open") || !strcmp(argv[0],"o")) {
    char *colon;
    if ((2 > argc) || (NULL == (colon = strchr(argv[1],':')))) {
      array_printf(out,"Usage: open hostname:port\n");
      return 0;
    }
    *colon = '\0';
    lock_node(n);
    node_connect_locked(n,argv[1],atoi(colon+1),1);
    unlock_node(n);
  }
  else if (!strcmp(argv[0],"run") || !strcmp(argv[0],"r")) {

    if (2 > argc) {
      array_printf(out,"Usage: run filename\n");
      return 0;
    }

    lock_node(n);
    if (0 != run_program(n,argv[1]))
      array_printf(out,"Could not run program\n");
    unlock_node(n);
  }
  else if (!strcmp(argv[0],"exit") || !strcmp(argv[0],"q") || !strcmp(argv[0],"quit")) {
    return 1;
  }
  else if (!strcmp(argv[0],"tasks") || !strcmp(argv[0],"t")) {
    endpoint *endpt;
    array_printf(out,
                 "%-7s %-3s %-9s %-6s %-10s\n","localid","pid","groupsize","bcsize","instructions");
    array_printf(out,
                 "%-7s %-3s %-9s %-6s %-10s\n","-------","---","---------","------","------------");
    lock_node(n);
    for (endpt = n->endpoints.first; endpt; endpt = endpt->next) {
      if (TASK_ENDPOINT == endpt->type) {
        task *tsk = (task*)endpt->data;
        array_printf(out,"%-7d %-3d %-9d %-6d %-12d\n",
                tsk->endpt->localid,tsk->pid,tsk->groupsize,tsk->bcsize,tsk->stats.ninstrs);
      }
    }
    unlock_node(n);
    return 0;
  }
  else if (!strcmp(argv[0],"kill") || !strcmp(argv[0],"kill")) {
    array_printf(out,"Not yet implemented\n");
    return 0;
  }
  else if (!strcmp(argv[0],"shutdown") || !strcmp(argv[0],"s")) {
    lock_node(n);
    node_shutdown_locked(n);
    unlock_node(n);
    return 0;
  }
  else if (!strcmp(argv[0],"help")) {
    array_printf(out,"connections   [c] - List all open connections\n");
    array_printf(out,"open          [o] - Open new connection\n");
    array_printf(out,"run           [r] - Run program\n");
    array_printf(out,"tasks         [t] - List tasks\n");
    array_printf(out,"kill          [k] - Kill a task\n");
    array_printf(out,"shutdown      [s] - Shut down VM\n");
    array_printf(out,"help          [h] - Print this message\n");
  }
  else {
    array_printf(out,"Unknown command. Type \"help\" to list available commands.\n");
  }
  return 0;
}

static void conn_disconnect(connection *conn)
{
  done_reading(conn->n,conn);
  conn->finwrite = 1;
  node_notify(conn->n);
}

static void process_line(node *n, console2 *csl, const char *line)
{
  int argc;
  char **argv;
  array *out = array_new(1,0);
  int doclose;
  connection *conn;

  parse_cmdline(line,&argc,&argv);
  doclose = process_cmd(n,argc,argv,out);
  array_printf(out,"\n> ");
  free_args(argc,argv);

  lock_node(n);

  for (conn = n->connections.first; conn; conn = conn->next) {

    if (conn->console_endpt != csl->endpt)
      continue;

    if (doclose) {
      array_printf(out,"Disconnecting\n");
      array_append(conn->sendbuf,out->data,out->nbytes);
      conn_disconnect(conn);
    }
    else {
      array_append(conn->sendbuf,out->data,out->nbytes);
      node_notify(n);
    }
  }

  unlock_node(n);
  array_free(out);
}

void console_process_received(node *n, connection *conn)
{
  int start = 0;
  int c = 0;

  while (c < conn->recvbuf->nbytes) {

    if (4 == conn->recvbuf->data[c]) { /* CTRL-D */
      printf("Got CTRL-D from %s\n",conn->hostname);
      array_printf(conn->sendbuf,"Disconnecting\n");
      node_notify(conn->n);
      conn_disconnect(conn);
      break;
    }

    if ('\n' == conn->recvbuf->data[c]) {
      endpointid destid;
      assert(conn->console_endpt);
      destid.nodeip = n->listenip;
      destid.nodeport = n->mainl->port;
      destid.localid = conn->console_endpt->localid;
      conn->recvbuf->data[c] = '\0';
      node_send_locked(n,conn->console_endpt->localid,destid,MSG_CONSOLE_LINE,
                       &conn->recvbuf->data[start],c+1-start);
      c++;
      start = c;
    }
    else {
      c++;
    }
  }

  array_remove_data(conn->recvbuf,start);
}

static int console_handle_message(node *n, endpoint *endpt, message *msg)
{
  console2 *csl = (console2*)endpt->data;
  switch (msg->hdr.tag) {
  case MSG_CONSOLE_LINE:
    process_line(n,csl,msg->data);
    break;
  case MSG_CONSOLE_CLOSE:
    printf("CONSOLE_CLOSE\n");
    endpoint_forceclose(endpt);
    break;
  }
  return 0;
}

static void console_endpoint_close(endpoint *endpt)
{
  console2 *csl = (console2*)endpt->data;
  node *n = csl->n;
  pthread_t thread = csl->thread;

  unlock_mutex(&n->lock);
  endpoint_forceclose(endpt);
  if (0 > pthread_join(thread,NULL))
    fatal("pthread_join: %s",strerror(errno));
  lock_mutex(&n->lock);
}

void *console_thread(void *arg)
{
  console2 *csl = (console2*)arg;
  node *n = csl->n;
  message *msg;

  printf("Console thread starting\n");

  while (NULL != (msg = endpoint_next_message(csl->endpt,-1))) {
    int r = console_handle_message(n,csl->endpt,msg);
    assert(0 == r);
    message_free(msg);
  }

  node_remove_endpoint(n,csl->endpt);
  free(csl);
  printf("Console thread finished\n");

  return NULL;
}

void start_console(node *n, connection *conn)
{
  console2 *csl = (console2*)calloc(1,sizeof(console2));
  printf("start_console\n");
  conn->isconsole = 1;
  csl->n = n;
  csl->endpt = node_add_endpoint_locked(n,0,CONSOLE_ENDPOINT,csl,console_endpoint_close);
  conn->console_endpt = csl->endpt;
  if (0 > pthread_create(&csl->thread,NULL,console_thread,csl))
    fatal("pthread_create: %s",strerror(errno));
}

void close_console(node *n, connection *conn)
{
  endpointid destid;
  printf("close_console\n");
  assert(NODE_ALREADY_LOCKED(n));
  assert(conn->console_endpt);
  destid.nodeip = n->listenip;
  destid.nodeport = n->mainl->port;
  destid.localid = conn->console_endpt->localid;
  node_send_locked(n,conn->console_endpt->localid,destid,MSG_CONSOLE_CLOSE,NULL,0);
}
