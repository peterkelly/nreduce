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
 * $Id: interpreter.c 923 2010-04-07 12:30:46Z pmkelly $
 *
 */

#define _GNU_SOURCE /* for asprintf */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "compiler/bytecode.h"
#include "src/nreduce.h"
#include "compiler/source.h"
#include "runtime.h"
#include "messages.h"
#include "events.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <assert.h>
#include <ctype.h>
#include <stdarg.h>
#include <math.h>
#include <errno.h>
#include <sys/types.h>
#include <unistd.h>
#include <sys/time.h>
#include <time.h>
#include <signal.h>

#define SCHEDULER_DELAY 250
#define SCHEDULER_TOLERANCE 0.0

void interpreter_count_sparks(task *tsk, message *msg)
{
  int response[2];
  response[0] = tsk->tid;
  response[1] = count_sparks(tsk);
  node_log(tsk->n,LOG_INFO,"COUNT_SPARKS: i have %d sparks",response[1]);
  endpoint_send(tsk->endpt,msg->source,MSG_NUM_SPARKS,response,2*sizeof(int));
}

void interpreter_distribute(task *tsk, message *msg)
{
  assert(tsk->groupsize*sizeof(int) == msg->size);
  int *dist = (int*)msg->data;
  int to;
  for (to = 0; to < tsk->groupsize; to++) {
    if (dist[to] > 0) {
      node_log(tsk->n,LOG_INFO,"DISTRIBUTE1: %d -> %d : %d sparks",tsk->tid,to,dist[to]);

      array *newmsg = write_start();
      int found = 0;
      while (dist[to] > 0) {
        frame *spark = tsk->sparklist->sprev;

        if (spark == tsk->sparklist)
          break; /* Spark list is empty */

        pntr p;
        assert(spark->c);
        make_pntr(p,spark->c);

        schedule_frame(tsk,spark,to,newmsg);
        found++;
        dist[to]--;
      }
      if (found > 0)
        msg_send(tsk,to,MSG_SCHEDULE,newmsg->data,newmsg->nbytes);
      write_end(newmsg);
      node_log(tsk->n,LOG_INFO,"DISTRIBUTE2: %d -> %d : %d sparks, actual %d",
               tsk->tid,to,dist[to],found);
    }
  }
}

void scheduler(int n, int *nsparks, int **transfer, double tolerance)
{
  int below[n];
  int above[n];
  int nbelow = 0;
  int nabove = 0;
  int i;

  /* Calculate the total number of sparks that each node should have */
  int total = 0;
  for (i = 0; i < n; i++)
    total += nsparks[i];
  int average = total/n;
  int least = (int)(average*(1.0-tolerance));
  int most = (int)(average*(1.0+tolerance));

  /* Split the nodes into below and above bins */
  for (i = 0; i < n; i++) {
    if (nsparks[i] < least)
      below[nbelow++] = i;
    if (nsparks[i] > most)
      above[nabove++] = i;
  }

  /* Distribute the surplus sparks from the above nodes among the below nodes */
  int h = 0;
  int l = 0;
  for (l = 0; l < nbelow; l++) {

    while ((h < nabove) && (nsparks[below[l]] < least)) {
      int want = least - nsparks[below[l]];
      int have = nsparks[above[h]] - most;
      int move = min(want,have);
      transfer[above[h]][below[l]] = move;
      nsparks[below[l]] += move;
      nsparks[above[h]] -= move;

      if (nsparks[above[h]] <= most)
        h++;
    }
  }
}

void do_schedule(node *n, endpoint *endpt, gcarg *ga, int *nsparks)
{
  int *transfer[ga->ntasks];
  int i;
  for (i = 0; i < ga->ntasks; i++)
    transfer[i] = (int*)calloc(ga->ntasks,sizeof(int));

  /* Determine which tasks must send frames to wich other tasks */
  scheduler(ga->ntasks,nsparks,transfer,SCHEDULER_TOLERANCE);

  /* Send the DISTRIBUTE messages */
  int from;
  for (from = 0; from < ga->ntasks; from++) {
    int to;

    /* Does this task need to distribute any frames */
    int count = 0;
    for (to = 0; to < ga->ntasks; to++) {
      if (transfer[from][to] > 0) {
        node_log(n,LOG_INFO,"SCHEDULER: %d -> %d : %d sparks\n",from,to,transfer[from][to]);
        count++;
      }
    }

    if (count > 0) {
      endpoint_send(endpt,ga->idmap[from],MSG_DISTRIBUTE,transfer[from],ga->ntasks*sizeof(int));
    }
  }

  for (i = 0; i < ga->ntasks; i++)
    free(transfer[i]);
}

void scheduler_thread(node *n, endpoint *endpt, void *arg)
{
  gcarg *ga = (gcarg*)arg;
  int nsparks[ga->ntasks];
  int have[ga->ntasks];
  int remaining = 0;

  int i;
  for (i = 0; i < ga->ntasks; i++)
    endpoint_link(endpt,ga->idmap[i]);

  int done = 0;
  while (!done) {
    message *msg = endpoint_receive(endpt,remaining > 0 ? -1 : SCHEDULER_DELAY);
    if (NULL == msg) {
      assert(0 == remaining);

      node_log(n,LOG_INFO,"SCHEDULER: sending COUNT_SPARKS");

      for (i = 0; i < ga->ntasks; i++) {
        nsparks[i] = 0;
        have[i] = 0;
        endpoint_send(endpt,ga->idmap[i],MSG_COUNT_SPARKS,NULL,0);
      }

      remaining = ga->ntasks;

      continue;
    }
    switch (msg->tag) {
    case MSG_NUM_SPARKS: {

      assert(2*sizeof(int) == msg->size);
      int tid = ((int*)msg->data)[0];
      int sparks = ((int*)msg->data)[1];

      assert(remaining > 0);
      assert(0 <= tid);
      assert(tid < ga->ntasks);
      assert(!have[tid]);
      assert(sparks >= 0);
      nsparks[tid] = sparks;
      remaining--;

      node_log(n,LOG_INFO,"SCHEDULER: NUM_SPARKS %d = %d - remaining = %d",
               tid,sparks,remaining);

      if (0 == remaining)
        do_schedule(n,endpt,ga,nsparks);
      break;
    }
    case MSG_ENDPOINT_EXIT:
    case MSG_KILL:
      done = 1;
      break;
    }
  }
}

void scheduler_test()
{
  int n = 32;
  int *transfer[n];
  int orig[n];
  int nsparks[n];

  struct timeval now;
  gettimeofday(&now,NULL);
  srand(now.tv_usec);
/*   srand(1); */
  
  int i;
  for (i = 0; i < n; i++)
    transfer[i] = calloc(n,sizeof(int));

  for (i = 0; i < n; i++) {
    orig[i] = nsparks[i] = rand()%1024;
  }

  scheduler(n,nsparks,transfer,0.5);

  printf("%-4s %-12s %-12s\n","Node","Original","Post schedule");
  for (i = 0; i < n; i++) {
    printf("%-4d %-12d %-12d\n",i,orig[i],nsparks[i]);
  }

  int from;
  int to;
  for (from = 0; from < n; from++) {
    for (to = 0; to < n; to++) {
      if (transfer[from][to] > 0)
        printf("%d -> %d : %d sparks\n",from,to,transfer[from][to]);
    }
  }
}
