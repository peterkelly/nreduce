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
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <assert.h>
#include <ctype.h>
#include <stdarg.h>
#include <math.h>
#include <errno.h>

extern const char *prelude;

static void cprintf(connection *conn, const char *format, ...)
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

static int get_managerids(socketcomm *sc, endpointid **managerids)
{
  int count = 1;
  int i = 0;
  connection *conn;

  if (!sc->havelistenip)
    fatal("I don't have my listen IP yet!");

  for (conn = sc->connections.first; conn; conn = conn->next)
    if (0 <= conn->port)
      count++;

  *managerids = (endpointid*)calloc(count,sizeof(endpointid));

  (*managerids)[i].nodeip = sc->listenip;
  (*managerids)[i].nodeport = sc->listenport;
  (*managerids)[i].localid = MANAGER_ID;
  i++;

  for (conn = sc->connections.first; conn; conn = conn->next) {
    if (0 <= conn->port) {
      (*managerids)[i].nodeip = conn->ip;
      (*managerids)[i].nodeport = conn->port;
      (*managerids)[i].localid = MANAGER_ID;
      i++;
    }
  }

  return count;
}


static int run_program(socketcomm *sc, const char *filename)
{
  int bcsize;
  char *bcdata;
  source *src;
  int count;
  endpointid *managerids;

  src = source_new();
  if (0 != source_parse_string(src,prelude,"prelude.l"))
    return -1;
  if (0 != source_parse_file(src,filename,""))
    return -1;
  if (0 != source_process(src,0,0))
    return -1;
  if (0 != source_compile(src,&bcdata,&bcsize))
    return -1;
  source_free(src);

  count = get_managerids(sc,&managerids);

  printf("Compiled\n");
  start_task_using_manager(sc,bcdata,bcsize,managerids,count);
  free(managerids);
  free(bcdata);
  return 0;
}

static void process_cmd(socketcomm *sc, connection *conn, int argc, char **argv)
{
  if (0 == argc)
    return;
  if (!strcmp(argv[0],"connections") || !strcmp(argv[0],"c")) {
    connection *c2;
    cprintf(conn,"%-30s %-6s %-6s %-8s\n","Hostname","Port","Socket","Console?");
    cprintf(conn,"%-30s %-6s %-6s %-8s\n","--------","----","------","--------");
    for (c2 = sc->connections.first; c2; c2 = c2->next)
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
    initiate_connection(sc,argv[1],atoi(colon+1));
  }
  else if (!strcmp(argv[0],"run") || !strcmp(argv[0],"r")) {

    if (2 > argc) {
      cprintf(conn,"Usage: run filename\n");
      return;
    }

    if (0 != run_program(sc,argv[1])) {
      cprintf(conn,"Could not run program\n");
    }

  }
  else if (!strcmp(argv[0],"exit") || !strcmp(argv[0],"q") || !strcmp(argv[0],"quit")) {
    conn_disconnect(conn);
    return;
  }
  else if (!strcmp(argv[0],"tasks") || !strcmp(argv[0],"t")) {
    endpoint *endpt;
    cprintf(conn,"%-3s %-9s %-6s\n","pid","groupsize","bcsize");
    cprintf(conn,"%-3s %-9s %-6s\n","---","---------","------");
    for (endpt = sc->endpoints.first; endpt; endpt = endpt->next) {
      if (TASK_ENDPOINT == endpt->type) {
        task *tsk = (task*)endpt->data;
        cprintf(conn,"%-3d %-9d %-6d\n",tsk->pid,tsk->groupsize,tsk->bcsize);
      }
    }
    return;
  }
  else if (!strcmp(argv[0],"shutdown") || !strcmp(argv[0],"s")) {
    endpoint *endpt;
    for (endpt = sc->endpoints.first; endpt; endpt = endpt->next)
      if (TASK_ENDPOINT == endpt->type)
        ((task*)endpt->data)->done = 1;
    sc->shutdown = 1;
    return;
  }
  else if (!strcmp(argv[0],"help")) {
    cprintf(conn,"connections   [c] - List all open connections\n");
    cprintf(conn,"open          [o] - Open new connection\n");
    cprintf(conn,"run           [r] - Run program\n");
    cprintf(conn,"tasks         [t] - List tasks\n");
    cprintf(conn,"shutdown      [s] - Shut down VM\n");
    cprintf(conn,"help          [h] - Print this message\n");
  }
  else {
    cprintf(conn,"Unknown command. Type \"help\" to list available commands.\n");
  }
}

static void process_line(socketcomm *sc, connection *conn, const char *line)
{
  int argc;
  char **argv;

  parse_cmdline(line,&argc,&argv);
  process_cmd(sc,conn,argc,argv);
  free_args(argc,argv);

  cprintf(conn,"\n> ");
}

void console_process_received(socketcomm *sc, connection *conn)
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
      process_line(sc,conn,&conn->recvbuf->data[start]);
      c++;
      start = c;
    }
    else {
      c++;
    }
  }

  array_remove_data(conn->recvbuf,start);
}
