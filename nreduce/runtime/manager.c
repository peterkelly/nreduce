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
      fatal("gc: unexpected message %d",msg->hdr.tag);
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
  endpointid *epids = NULL;
  int count = node_get_endpoints(n,"task",&epids);

  msize = sizeof(get_tasks_response_msg)+count*sizeof(endpointid);
  gtrm = (get_tasks_response_msg*)calloc(1,msize);
  gtrm->count = count;
  memcpy(gtrm->tasks,epids,count*sizeof(endpointid));

  endpoint_send(endpt,m->sender,MSG_GET_TASKS_RESPONSE,gtrm,msize);
  free(gtrm);
}

static void manager_thread(node *n, endpoint *endpt, void *arg)
{
  int done = 0;
  while (!done) {
    message *msg = endpoint_receive(endpt,-1);
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
    case MSG_KILL:
      node_log(n,LOG_INFO,"Manager received KILL");
      done = 1;
      break;
    default:
      fatal("manager: unexpected message %d",msg->hdr.tag);
      break;
    }
    message_free(msg);
  }
}

void start_manager(node *n)
{
  node_add_thread2(n,"manager",manager_thread,NULL,NULL,MANAGER_ID,0);
}
