/*
 * This file is part of the NReduce project
 * Copyright (C) 2006-2010 Peter Kelly <kellypmk@gmail.com>
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
#include <unistd.h>

typedef struct console {
  node *n;
  endpoint *endpt;
  pthread_t thread;
  socketid sockid;
} console;

static int process_cmd(node *n, endpoint *endpt, int argc, char **argv, array *out)
{
  assert(NODE_UNLOCKED(n));
  if (0 == argc)
    return 0;
  if (!strcmp(argv[0],"connections") || !strcmp(argv[0],"c")) {
    connection *c2;
    lock_node(n);
    array_printf(out,"%-30s %-6s %-6s %-8s %-20s\n",
                 "Hostname","Port","Socket","Regular?","Owner");
    array_printf(out,"%-30s %-6s %-6s %-8s %-20s\n",
                 "--------","----","------","--------","--------------------");
    for (c2 = n->p->connections.first; c2; c2 = c2->next) {
      array_printf(out,"%-30s %-6d %-6d %-8s",
                   c2->hostname,c2->port,c2->sock,
                   c2->isreg ? "Yes" : "No");
      if (endpointid_isnull(&c2->owner))
        array_printf(out," (none)");
      else
        array_printf(out," "EPID_FORMAT,EPID_ARGS(c2->owner));
      array_printf(out,"\n");
    }
    unlock_node(n);
  }
  else if (!strcmp(argv[0],"listeners") || !strcmp(argv[0],"l")) {
    listener *l;
    array_printf(out,"%-7s %-4s %-20s\n","sid    ","port","owner");
    array_printf(out,"%-7s %-4s %-20s\n","-------","----","--------------------");
    lock_node(n);
    for (l = n->p->listeners.first; l; l = l->next) {
      array_printf(out,"%-7d %-4d",l->sockid.sid,l->port);
      if (endpointid_isnull(&l->owner))
        array_printf(out," (none)");
      else
        array_printf(out," "EPID_FORMAT,EPID_ARGS(l->owner));
      array_printf(out,"\n");
    }
    unlock_node(n);
    return 0;
  }
  /* FIXME: enable this again? (this command maybe isn't that useful anyway) */
#if 0
  else if (!strcmp(argv[0],"tasks") || !strcmp(argv[0],"t")) {
    endpoint *endpt;
    array_printf(out,
                 "%-7s %-3s %-9s %-6s %-10s\n","localid","pid","groupsize","bcsize","instructions");
    array_printf(out,
                 "%-7s %-3s %-9s %-6s %-10s\n","-------","---","---------","------","------------");
    lock_node(n);
    for (endpt = n->p->endpoints.first; endpt; endpt = endpt->next) {
      if (!strcmp(endpt->p->type,"task")) {
        task *tsk = (task*)endpt->p->data;
        array_printf(out,"%-7d %-3d %-9d %-6d %-12d\n",
                     endpt->epid.localid,tsk->tid,tsk->groupsize,tsk->bcsize,tsk->stats.ninstrs);
      }
    }
    unlock_node(n);
    return 0;
  }
#endif
  else if (!strcmp(argv[0],"threads") || !strcmp(argv[0],"r")) {
    endpoint *endpt;
    array_printf(out,"%-7s %-4s\n","localid","type");
    array_printf(out,"%-7s %-4s\n","-------","----");
    lock_node(n);
    for (endpt = n->p->endpoints.first; endpt; endpt = endpt->next)
      array_printf(out,"%-7d %s\n",endpt->epid.localid,endpt->p->type);
    unlock_node(n);
    return 0;
  }
  else if (!strcmp(argv[0],"kill") || !strcmp(argv[0],"k")) {
    if (2 > argc)  {
      array_printf(out,"Please specify a task to kill\n");
    }
    else {
      endpointid destid;
      destid.ip = n->listenip;
      destid.port = n->listenport;
      destid.localid = atoi(argv[1]);
      printf("killing %d\n",destid.localid);
      endpoint_send(endpt,destid,MSG_KILL,NULL,0);
      printf("after killing %d\n",destid.localid);
    }
    return 0;
  }
  else if (!strcmp(argv[0],"quit") || !strcmp(argv[0],"q") || !strcmp(argv[0],"exit")) {
    return 1;
  }
  else if (!strcmp(argv[0],"shutdown") || !strcmp(argv[0],"s")) {
    node_shutdown(n);
    return 0;
  }
  else if (!strcmp(argv[0],"help") || !strcmp(argv[0],"h") || !strcmp(argv[0],"?")) {
    array_printf(out,"connections       [c] - List all open connections\n");
    array_printf(out,"listeners         [l] - List all listening sockets\n");
    /* FIXME: see above */
/*     array_printf(out,"tasks             [t] - List tasks\n"); */
    array_printf(out,"threads           [r] - List threads\n");
    array_printf(out,"kill              [k] - Kill a task\n");
    array_printf(out,"quit (or exit)    [q] - Disconnect from debug console\n");
    array_printf(out,"shutdown          [s] - Shut down VM\n");
    array_printf(out,"help              [h] - Print this message\n");
  }
  else {
    array_printf(out,"Unknown command. Type \"help\" to list available commands.\n");
  }
  return 0;
}

static int process_line(node *n, endpoint *endpt, console *csl, const char *line)
{
  int argc;
  char **argv;
  array *out = array_new(1,0);
  int doclose;

  parse_cmdline(line,&argc,&argv);
  doclose = process_cmd(n,endpt,argc,argv,out);
  array_printf(out,"\n> ");
  free_args(argc,argv);

  if (!doclose)
    send_write(endpt,csl->sockid,1,out->data,out->nbytes);
  array_free(out);
  return doclose;
}

void console_thread(node *n, endpoint *endpt, void *arg)
{
  console *csl = (console*)calloc(1,sizeof(console));
  message *msg;
  int done = 0;
  array *input = array_new(1,0);

  csl->endpt = endpt;
  csl->sockid = *(socketid*)arg;
  free(arg);

  while (!done) {
    msg = endpoint_receive(csl->endpt,-1);
    switch (msg->tag) {
    case MSG_READ_RESPONSE: {
      read_response_msg *m = (read_response_msg*)msg->data;
      int start = 0;
      int c = 0;
      int doclose = 0;

      assert(sizeof(read_response_msg) <= msg->size);

      array_append(input,m->data,m->len);

      while ((c < input->nbytes) && !doclose) {
        if (4 == input->data[c]) { /* CTRL-D */
          doclose = 1;
          break;
        }
        if ('\n' == input->data[c]) {
          input->data[c] = '\0';
          if ((start < c) && ('\r' == input->data[c-1]))
            input->data[c-1] = '\0';
          doclose = process_line(n,endpt,csl,&input->data[start]);
          c++;
          start = c;
        }
        else {
          c++;
        }
      }

      array_remove_data(input,start);

      if (doclose) {
        send_write(endpt,csl->sockid,1,NULL,0);
        done = 1;
      }
      else if (0 == m->len) {
        done = 1;
      }
      else {
        send_read(endpt,csl->sockid,1);
      }
      break;
    }
    case MSG_CONNECTION_CLOSED:
      done = 1;
      break;
    case MSG_WRITE_RESPONSE:
      break;
    case MSG_CONSOLE_DATA: {
      break;
    }
    case MSG_KILL:
      done = 1;
      break;
    default:
      fatal("console: unexpected message %d",msg->tag);
      break;
    }
    message_free(msg);
  }

  free(input);
  free(csl);
}
