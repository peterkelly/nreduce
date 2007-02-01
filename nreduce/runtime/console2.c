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
#include "network.h"
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

void cprintf(connection *conn, const char *format, ...)
{
  va_list ap;
  va_start(ap,format);
  array_vprintf(conn->sendbuf,format,ap);
  va_end(ap);
}

static void conn_disconnect(connection *conn)
{
  conn->toclose = 1;
  cprintf(conn,"Disconnecting\n");
}

static void node_shutdown(node *n)
{
  char c = 0;
  n->shutdown = 1;
  write(n->ioready_writefd,&c,1);
}

static void process_cmd(node *n, connection *conn, int argc, char **argv)
{
  if (0 == argc)
    return;
  if (!strcmp(argv[0],"connections") || !strcmp(argv[0],"c")) {
    connection *c2;
    cprintf(conn,"%-30s %-6s %-6s %-8s\n","Hostname","Port","Socket","Console?");
    cprintf(conn,"%-30s %-6s %-6s %-8s\n","--------","----","------","--------");
    for (c2 = n->connections.first; c2; c2 = c2->next)
      cprintf(conn,"%-30s %-6d %-6d %-8s\n",
              c2->hostname,c2->port,c2->sock,
              c2->isconsole ? "Yes" : "No");
  }
  else if (!strcmp(argv[0],"open") || !strcmp(argv[0],"o")) {
    char *colon;
    if ((2 > argc) || (NULL == (colon = strchr(argv[1],':')))) {
      cprintf(conn,"Usage: open hostname:port\n");
      return;
    }
    *colon = '\0';
    node_connect(n,argv[1],atoi(colon+1));
  }
  else if (!strcmp(argv[0],"run") || !strcmp(argv[0],"r")) {

    if (2 > argc) {
      cprintf(conn,"Usage: run filename\n");
      return;
    }

    if (0 != run_program(n,argv[1])) {
      cprintf(conn,"Could not run program\n");
    }

  }
  else if (!strcmp(argv[0],"exit") || !strcmp(argv[0],"q") || !strcmp(argv[0],"quit")) {
    conn_disconnect(conn);
    return;
  }
  else if (!strcmp(argv[0],"tasks") || !strcmp(argv[0],"t")) {
    endpoint *endpt;
    cprintf(conn,"%-7s %-3s %-9s %-6s %-10s\n","localid","pid","groupsize","bcsize","instructions");
    cprintf(conn,"%-7s %-3s %-9s %-6s %-10s\n","-------","---","---------","------","------------");
    for (endpt = n->endpoints.first; endpt; endpt = endpt->next) {
      if (TASK_ENDPOINT == endpt->type) {
        task *tsk = (task*)endpt->data;
        int ninstrs = 0;
        int i;
        for (i = 0; i < OP_COUNT; i++)
          ninstrs += tsk->stats.op_usage[i];
        cprintf(conn,"%-7d %-3d %-9d %-6d %-12d\n",
                tsk->endpt->localid,tsk->pid,tsk->groupsize,tsk->bcsize,ninstrs);
      }
    }
    return;
  }
  else if (!strcmp(argv[0],"kill") || !strcmp(argv[0],"kill")) {
    printf("Not yet implemented\n");
    return;
  }
  else if (!strcmp(argv[0],"shutdown") || !strcmp(argv[0],"s")) {
/*     endpoint *endpt; */
/*     for (endpt = n->endpoints.first; endpt; endpt = endpt->next) */
/*       if (TASK_ENDPOINT == endpt->type) */
/*         ((task*)endpt->data)->done = 1; */
/*     n->shutdown = 1; */
    node_shutdown(n);
    return;
  }
  else if (!strcmp(argv[0],"help")) {
    cprintf(conn,"connections   [c] - List all open connections\n");
    cprintf(conn,"open          [o] - Open new connection\n");
    cprintf(conn,"run           [r] - Run program\n");
    cprintf(conn,"tasks         [t] - List tasks\n");
    cprintf(conn,"kill          [k] - Kill a task\n");
    cprintf(conn,"shutdown      [s] - Shut down VM\n");
    cprintf(conn,"help          [h] - Print this message\n");
  }
  else {
    cprintf(conn,"Unknown command. Type \"help\" to list available commands.\n");
  }
}

static void process_line(node *n, connection *conn, const char *line)
{
  int argc;
  char **argv;

  parse_cmdline(line,&argc,&argv);
  process_cmd(n,conn,argc,argv);
  free_args(argc,argv);

  cprintf(conn,"\n> ");
}

void console_process_received(node *n, connection *conn)
{
  int start = 0;
  int c = 0;

  while (c < conn->recvbuf->nbytes) {

    if (4 == conn->recvbuf->data[c]) { /* CTRL-D */
      printf("Got CTRL-D from %s\n",conn->hostname);
      conn_disconnect(conn);
      break;
    }

    if ('\n' == conn->recvbuf->data[c]) {
      conn->recvbuf->data[c] = '\0';
      if ((c > start) && ('\r' == conn->recvbuf->data[c-1]))
        conn->recvbuf->data[c-1] = '\0';
      process_line(n,conn,&conn->recvbuf->data[start]);
      c++;
      start = c;
    }
    else {
      c++;
    }
  }

  array_remove_data(conn->recvbuf,start);
}
