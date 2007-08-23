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
 * $Id$
 *
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "compiler/bytecode.h"
#include "src/nreduce.h"
#include "runtime.h"
#include "network/node.h"
#include "network/messages.h"
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

#define DISTGC_DELAY 3000
//#define DEBUG_DISTGC

typedef struct gcarg {
  int ntasks;
  endpointid idmap[0];
} gcarg;

static void gc_thread(node *n, endpoint *endpt, void *arg)
{
  gcarg *ga = (gcarg*)arg;
  int done = 0;
  int i;
  int ingc = 0;
  int *count = (int*)calloc(ga->ntasks,sizeof(int));
  int mark_done = 0;
  int rem_startacks = 0;
  int rem_sweepacks = 0;
  int gciter = 0;

  #ifdef DEBUG_DISTGC
  printf("gc_thread\n");
  for (i = 0; i < ga->ntasks; i++)
    printf("gc_thread: idmap[%d] = "EPID_FORMAT"\n",i,EPID_ARGS(ga->idmap[i]));
  #endif

  for (i = 0; i < ga->ntasks; i++)
    endpoint_link(endpt,ga->idmap[i]);

  while (!done) {
    message *msg = endpoint_receive(endpt,ingc ? -1 : DISTGC_DELAY);
    if (NULL == msg) {
      startdistgc_msg sm;
      sm.gc = endpt->epid;
      sm.gciter = ++gciter;
      #ifdef DEBUG_DISTGC
      printf("Starting distributed garbage collection\n");
      #endif
      assert(!ingc);
      ingc = 1;

      for (i = 0; i < ga->ntasks; i++)
        endpoint_send(endpt,ga->idmap[i],MSG_STARTDISTGC,&sm,sizeof(sm));
      rem_startacks = ga->ntasks;

      continue;
    }
    switch (msg->hdr.tag) {
    case MSG_STARTDISTGCACK: {
      assert(ingc);
      assert(!mark_done);
      assert(0 < rem_startacks);
      rem_startacks--;
      if (0 == rem_startacks) {
        #ifdef DEBUG_DISTGC
        printf("All tasks have received STARTDISTGC\n");
        #endif
        memset(count,0,ga->ntasks*sizeof(int));

        for (i = 0; i < ga->ntasks; i++) {
          endpoint_send(endpt,ga->idmap[i],MSG_MARKROOTS,NULL,0);
          count[i]++;
        }
      }
      break;
    }
    case MSG_UPDATE: {
      update_msg *m = (update_msg*)msg->data;
      assert(sizeof(update_msg)+ga->ntasks*sizeof(int) == msg->hdr.size);
      assert(ingc);
      assert(!mark_done);
      assert(m->gciter == gciter);

      for (i = 0; i < ga->ntasks; i++)
        count[i] += m->counts[i];

      #ifdef DEBUG_DISTGC
      printf("after update (gciter %d) from "EPID_FORMAT":",m->gciter,EPID_ARGS(msg->hdr.source));
      for (i = 0; i < ga->ntasks; i++)
        printf(" %d",count[i]);
      printf("\n");
      #endif

      mark_done = 1;
      for (i = 0; i < ga->ntasks; i++)
        if (count[i])
          mark_done = 0;

      if (mark_done) {
        #ifdef DEBUG_DISTGC
        printf("Mark done\n");
        #endif
        for (i = 0; i < ga->ntasks; i++)
          endpoint_send(endpt,ga->idmap[i],MSG_SWEEP,NULL,0);
        rem_sweepacks = ga->ntasks;
      }
      break;
    }
    case MSG_SWEEPACK: {
      assert(ingc);
      assert(mark_done);
      assert(0 < rem_sweepacks);
      rem_sweepacks--;
      if (0 == rem_sweepacks) {
        #ifdef DEBUG_DISTGC
        printf("Distributed garbage collection completed\n");
        #endif
        ingc = 0;
        mark_done = 0;
        for (i = 0; i < ga->ntasks; i++) {
          assert(0 == count[i]);
        }
      }
      break;
    }
    case MSG_ENDPOINT_EXIT:
      done = 1;
      break;
    case MSG_KILL:
      done = 1;
      break;
    default:
      fatal("gc: unexpected message %s",msg_names[msg->hdr.tag]);
      break;
    }
    message_free(msg);
  }
  free(count);
  free(ga);
}

static void manager_startgc(node *n, endpoint *endpt, startgc_msg *m, endpointid source)
{
  gcarg *ga = (gcarg*)calloc(1,sizeof(gcarg)+m->count*sizeof(endpointid));
  ga->ntasks = m->count;
  memcpy(ga->idmap,m->idmap,m->count*sizeof(endpointid));
  node_add_thread(n,"gc",gc_thread,ga,NULL);
  endpoint_send(endpt,source,MSG_STARTGC_RESPONSE,NULL,0);
}

static void manager_get_tasks(node *n, endpoint *endpt, get_tasks_msg *m)
{
  get_tasks_response_msg *gtrm;
  int msize;
  int count = 0;
  int i = 0;
  endpoint *ep;

  lock_node(n);
  for (ep = n->endpoints.first; ep; ep = ep->next) {
    if (!strcmp(ep->type,"task"))
      count++;
  }

  msize = sizeof(get_tasks_response_msg)+count*sizeof(endpointid);
  gtrm = (get_tasks_response_msg*)calloc(1,msize);
  gtrm->count = count;

  for (ep = n->endpoints.first; ep; ep = ep->next) {
    if (!strcmp(ep->type,"task"))
      gtrm->tasks[i++] = ep->epid;
  }
  unlock_node(n);

  endpoint_send(endpt,m->sender,MSG_GET_TASKS_RESPONSE,gtrm,msize);
  free(gtrm);
}

static task *find_task(node *n, int localid)
{
  endpoint *endpt = find_endpoint(n,localid);
  if (NULL == endpt)
    return NULL;
  if (strcmp(endpt->type,"task"))
    fatal("Request for endpoint %d that is not a task",localid);
  return (task*)endpt->data;
}

int taskman_handle(node *n, endpoint *endpt, message *msg, int final, void *arg)
{
  if (final) {
    return 1;
  }

  switch (msg->hdr.tag) {
  case MSG_NEWTASK: {
    newtask_msg *ntmsg;
    endpointid epid;
    array *args = array_new(sizeof(char*),0);
    char *str;
    char *start;
    if (sizeof(newtask_msg) > msg->hdr.size)
      fatal("NEWTASK: invalid message size");
    ntmsg = (newtask_msg*)msg->data;
    if (sizeof(newtask_msg)+ntmsg->bcsize > msg->hdr.size)
      fatal("NEWTASK: invalid bytecode size");

    str = ((char*)msg->data)+sizeof(newtask_msg)+ntmsg->bcsize;
    start = str;
    while (str < ((char*)msg->data)+msg->hdr.size) {
      if ('\0' == *str) {
        array_append(args,&start,sizeof(char*));
        start = str+1;
      }
      str++;
    }
    assert(array_count(args) == ntmsg->argc);

    node_log(n,LOG_INFO,"NEWTASK pid = %d, groupsize = %d, bcsize = %d",
             ntmsg->tid,ntmsg->groupsize,ntmsg->bcsize);

    task_new(ntmsg->tid,ntmsg->groupsize,ntmsg->bcdata,ntmsg->bcsize,args,n,
             ntmsg->out_sockid,&epid);

    endpoint_send(endpt,msg->hdr.source,MSG_NEWTASKRESP,
                  &epid.localid,sizeof(int));
    array_free(args);
    break;
  }
  case MSG_INITTASK: {
    inittask_msg *initmsg;
    task *newtsk;
    int i;
    int resp = 0;
    if (sizeof(inittask_msg) > msg->hdr.size)
      fatal("INITTASK: invalid message size");
    initmsg = (inittask_msg*)msg->data;
    if (sizeof(initmsg)+initmsg->count*sizeof(endpointid) > msg->hdr.size)
      fatal("INITTASK: invalid idmap size");

    node_log(n,LOG_INFO,"INITTASK: localid = %d, count = %d",initmsg->localid,initmsg->count);

    lock_node(n);
    newtsk = find_task(n,initmsg->localid);

    if (NULL == newtsk)
      fatal("INITTASK: task with localid %d does not exist",initmsg->localid);

    if (newtsk->haveidmap)
      fatal("INITTASK: task with localid %d already has an idmap",initmsg->localid);

    if (initmsg->count != newtsk->groupsize)
      fatal("INITTASK: idmap size does not match expected");
    memcpy(newtsk->idmap,initmsg->idmap,newtsk->groupsize*sizeof(endpointid));
    newtsk->haveidmap = 1;

    for (i = 0; i < newtsk->groupsize; i++)
      if (!endpointid_equals(&newtsk->endpt->epid,&initmsg->idmap[i]))
        endpoint_link_locked(newtsk->endpt,initmsg->idmap[i]);

    unlock_node(n);

    for (i = 0; i < newtsk->groupsize; i++) {
      endpointid_str str;
      print_endpointid(str,initmsg->idmap[i]);
      node_log(n,LOG_INFO,"INITTASK: idmap[%d] = %s",i,str);
    }

    endpoint_send(endpt,msg->hdr.source,MSG_INITTASKRESP,&resp,sizeof(int));
    break;
  }
  case MSG_STARTTASK: {
    task *newtsk;
    int resp = 0;
    int localid;
    char semdata = 0;
    if (sizeof(int) > msg->hdr.size)
      fatal("STARTTASK: invalid message size");
    localid = *(int*)msg->data;

    node_log(n,LOG_INFO,"STARTTASK: localid = %d",localid);

    lock_node(n);
    newtsk = find_task(n,localid);

    if (NULL == newtsk)
      fatal("STARTTASK: task with localid %d does not exist",localid);

    if (!newtsk->haveidmap)
      fatal("STARTTASK: task with localid %d does not have an idmap",localid);

    if (newtsk->started)
      fatal("STARTTASK: task with localid %d has already been started",localid);

    write(newtsk->startfds[1],&semdata,1);
    newtsk->started = 1;
    unlock_node(n);

    endpoint_send(endpt,msg->hdr.source,MSG_STARTTASKRESP,&resp,sizeof(int));
    break;
  }
  case MSG_START_CHORD: {
    start_chord_msg *m = (start_chord_msg*)msg->data;
    assert(sizeof(start_chord_msg) == msg->hdr.size);
    start_chord(n,0,m->initial,m->caller,m->stabilize_delay);
    break;
  }
  case MSG_STARTGC:
    assert(sizeof(startgc_msg) <= msg->hdr.size);
    manager_startgc(n,endpt,(startgc_msg*)msg->data,msg->hdr.source);
    break;
  case MSG_GET_TASKS:
    assert(sizeof(get_tasks_msg) == msg->hdr.size);
    manager_get_tasks(n,endpt,(get_tasks_msg*)msg->data);
    break;
  default:
    return 0;
  }

  return 1;
}
