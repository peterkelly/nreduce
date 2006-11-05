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

static void wprintf(workerinfo *wkr, const char *format, ...)
{
  va_list ap;
  va_start(ap,format);
  array_vprintf(wkr->sendbuf,format,ap);
  va_end(ap);
}

static void worker_disconnect(workerinfo *wkr)
{
  wkr->toclose = 1;
  wprintf(wkr,"Disconnecting\n");
}

static void process_line(socketcomm *sc, workerinfo *wkr, const char *line)
{
  if (!strcmp(line,"workers")) {
    workerinfo *w2;
    wprintf(wkr,"%-30s %-6s %-6s %-6s %-8s\n","Hostname","Port","Socket","ID","Console?");
    wprintf(wkr,"%-30s %-6s %-6s %-6s %-8s\n","--------","----","------","--","--------");
    for (w2 = sc->wlist.first; w2; w2 = w2->next)
      wprintf(wkr,"%-30s %-6d %-6d %-6d %-8s\n",
              w2->hostname,-1,w2->sock,w2->id,
              w2->isconsole ? "Yes" : "No");
  }
  else if (!strcmp(line,"exit") || !strcmp(line,"q") || !strcmp(line,"quit")) {
    worker_disconnect(wkr);
    return;
  }
  else if (!strcmp(line,"help")) {
    wprintf(wkr,"workers     - List all connected workers\n");
    wprintf(wkr,"help        - Print this message\n");
  }
  else {
    wprintf(wkr,"Unknown command. Type \"help\" to list available commands.\n");
  }

  wprintf(wkr,"\n> ");
}

void console_process_received(socketcomm *sc, workerinfo *wkr)
{
  int start = 0;
  int c = 0;

  while (c < wkr->recvbuf->nbytes) {

    if (4 == wkr->recvbuf->data[c]) { /* CTRL-D */
      worker_disconnect(wkr);
      break;
    }

    if ('\n' == wkr->recvbuf->data[c]) {
      wkr->recvbuf->data[c] = '\0';
      if ((c > start) && ('\r' == wkr->recvbuf->data[c-1]))
        wkr->recvbuf->data[c-1] = '\0';
      process_line(sc,wkr,&wkr->recvbuf->data[start]);
      c++;
      start = c;
    }
    else {
      c++;
    }
  }

  array_remove_data(wkr->recvbuf,start);
}
