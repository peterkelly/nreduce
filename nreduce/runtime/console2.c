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
 * $Id: console.c 358 2006-11-01 03:52:33Z pmkelly $
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

static void process_line(socketcomm *sc, connection *conn, const char *line)
{
  if (!strcmp(line,"connections")) {
    connection *c2;
    cprintf(conn,"%-30s %-6s %-6s %-8s\n","Hostname","Port","Socket","Console?");
    cprintf(conn,"%-30s %-6s %-6s %-8s\n","--------","----","------","--------");
    for (c2 = sc->wlist.first; c2; c2 = c2->next)
      cprintf(conn,"%-30s %-6d %-6d %-8s\n",
              c2->hostname,c2->port,c2->sock,
              c2->isconsole ? "Yes" : "No");
  }
  else if (!strcmp(line,"exit") || !strcmp(line,"q") || !strcmp(line,"quit")) {
    conn_disconnect(conn);
    return;
  }
  else if (!strcmp(line,"help")) {
    cprintf(conn,"connections   - List all open connections\n");
    cprintf(conn,"help          - Print this message\n");
  }
  else {
    cprintf(conn,"Unknown command. Type \"help\" to list available commands.\n");
  }

  cprintf(conn,"\n> ");
}

void console_process_received(socketcomm *sc, connection *conn)
{
  int start = 0;
  int c = 0;

  while (c < conn->recvbuf->nbytes) {

    if (4 == conn->recvbuf->data[c]) { /* CTRL-D */
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
