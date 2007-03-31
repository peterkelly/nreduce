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
#include <sys/time.h>
#include <time.h>

#define MAX_NODES 512
#define NODE_COUNT 32
#define TEST_LOOKUPS_PER_NODE 256
#define DEBUG_DELAY 250
#define STABILIZE_DELAY 1000
#define DISRUPT_INTERVAL 10
#define KILL_COUNT 8
#define JOIN_COUNT 12

typedef struct {
  node *n;
  endpointid epid;
} debug_control;

static void *debug_control_thread(void *arg)
{
  debug_control *dbc = (debug_control*)arg;
  node *n = dbc->n;
  message *msg;
  int done = 0;
  endpoint *endpt = node_add_endpoint(n,0,DBC_ENDPOINT,NULL,endpoint_close_kill);
  int indebug = 0;

  while (!done) {
    msg = endpoint_next_message(endpt,indebug ? -1 : DEBUG_DELAY);
    if (NULL == msg) {
      assert(!indebug);
      node_send(n,endpt->epid.localid,dbc->epid,MSG_DEBUG_START,&endpt->epid,sizeof(endpointid));
      indebug = 1;
    }
    else {
      switch (msg->hdr.tag) {
      case MSG_DEBUG_DONE:
        assert(indebug);
        indebug = 0;
        break;
      case MSG_KILL:
        done = 1;
        break;
      default:
        fatal("debug_control received invalid message: %d",msg->hdr.tag);
        break;
      }
      message_free(msg);
    }
  }

  node_remove_endpoint(n,endpt);
  free(dbc);
  return NULL;
}

static chordnode start_one_chord(node *n, endpoint *endpt, chordnode ndash, endpointid managerid)
{
  int done = 0;
  chordnode cn;
  start_chord_msg scm;

  memset(&cn,0,sizeof(chordnode));

  scm.ndash = ndash;
  scm.caller = endpt->epid;
  scm.stabilize_delay = STABILIZE_DELAY;
  node_send(n,endpt->epid.localid,managerid,MSG_START_CHORD,&scm,sizeof(start_chord_msg));

  while (!done) {
    message *msg = endpoint_next_message(endpt,20000);
    if (!msg) {
      printf("timeout waiting for CHORD_STARTED message\n");
      abort();
    }
    switch (msg->hdr.tag) {
    case MSG_CHORD_STARTED: {
      chord_started_msg *m = (chord_started_msg*)msg->data;
      assert(sizeof(chord_started_msg) == msg->hdr.size);

      lock_node(n);
      endpoint_link(endpt,m->cn.epid);
      unlock_node(n);

      cn = m->cn;
      done = 1;
      break;
    }
    case MSG_KILL:
      /* FIXME */
      abort();
      break;
    default:
      fatal("start_one_chord received invalid message: %d",msg->hdr.tag);
      break;
    }
    message_free(msg);
  }

  return cn;
}

static chordid manual_hash(chordid id, chordnode *nodes, int ncount)
{
  int i;
  for (i = 0; i < ncount; i++) {
    if (id <= nodes[i].id)
      return nodes[i].id;
  }
  return nodes[0].id;
}

static int chordnode_cmp(const void *a, const void *b)
{
  const chordnode *ca = (const chordnode*)a;
  const chordnode *cb = (const chordnode*)b;
  return ca->id - cb->id;
}

typedef struct {
  node *n;
  endpoint *endpt;

  int indebug;

  int incorrect_nodes;
  int incorrect_successor;
  int incorrect_predecessor;
  int incorrect_fingers;
  int incorrect_succlist;
  int total_bad;
  int total_hops;
  int total_lookups;

  int cur_node;
  int cur_lookup;
  chordid lookup_id;

  endpointid caller;

  int ncount;
  struct timeval start;
  chordnode *nodes;
  int pending;
  int iterations;
  int pending_exits;
  int pending_joins;
  endpointid *managerids;
  int nmanagers;
} check;

static void check_abort(check *chk)
{
  if (chk->indebug) {
    node_send(chk->n,chk->endpt->epid.localid,chk->caller,MSG_DEBUG_DONE,NULL,0);
    chk->indebug = 0;
  }
}

static void add_node(node *n, endpoint *endpt, endpointid managerid, chordnode ndash)
{
  start_chord_msg scm;
  scm.ndash = ndash;
  scm.caller = endpt->epid;
  scm.stabilize_delay = STABILIZE_DELAY;
  node_send(n,endpt->epid.localid,managerid,MSG_START_CHORD,&scm,sizeof(start_chord_msg));
}

static void check_debug_start(check *chk, endpointid *caller)
{
  assert(!chk->indebug);
  chk->indebug = 1;
  memcpy(&chk->caller,caller,sizeof(endpointid));
  chk->cur_node = 0;
  chk->cur_lookup = 0;

  chk->incorrect_nodes = 0;
  chk->incorrect_successor = 0;
  chk->incorrect_predecessor = 0;
  chk->incorrect_fingers = 0;
  chk->incorrect_succlist = 0;
  chk->total_bad = 0;
  chk->total_hops = 0;
  chk->total_lookups = 0;

  if (0 < chk->pending_joins) {
    printf("skipping debug, due to %d pending joins\n",chk->pending_joins);
    check_abort(chk);
  }
  else if ((1 <= chk->iterations) && (0 == (chk->iterations % DISRUPT_INTERVAL))) {
    int i;
    int j;
    int *killindices = (int*)calloc(KILL_COUNT,sizeof(int));
    chordnode initial = chk->nodes[rand()%chk->ncount];
    for (i = 0; i < KILL_COUNT; i++) {
      int have;
      int index;
      do {
        have = 0;
        index = rand()%chk->ncount;

        for (j = 0; j < i; j++)
          if (killindices[j] == index)
            have = 1;

        if (endpointid_equals(&initial.epid,&chk->nodes[index].epid))
          have = 1;

      } while (have);
      killindices[i] = index;
    }

    for (i = 0; i < KILL_COUNT; i++) {
      node_send(chk->n,chk->endpt->epid.localid,chk->nodes[killindices[i]].epid,MSG_KILL,NULL,0);
    }

    free(killindices);

    for (i = 0; i < JOIN_COUNT; i++) {
      add_node(chk->n,chk->endpt,chk->managerids[rand()%chk->nmanagers],initial);
      chk->pending_joins++;
    }

    chk->iterations++;
    check_abort(chk);
  }
  else {
    get_table_msg gtm;
    gtm.sender = chk->endpt->epid;
    node_send(chk->n,chk->endpt->epid.localid,chk->nodes[0].epid,MSG_GET_TABLE,&gtm,sizeof(gtm));
  }
}

static void check_check_next(check *chk)
{
  assert(chk->indebug);
  if (chk->cur_lookup >= TEST_LOOKUPS_PER_NODE) {
    chk->cur_lookup = 0;
    chk->cur_node++;

    if (chk->cur_node < chk->ncount) {
      get_table_msg gfm;
      gfm.sender = chk->endpt->epid;
      node_send(chk->n,chk->endpt->epid.localid,chk->nodes[chk->cur_node].epid,MSG_GET_TABLE,
                &gfm,sizeof(get_table_msg));
    }
    else {
      /* Print lookup statistics */
      struct timeval now;
      struct timeval diff;
      double seconds;
      gettimeofday(&now,NULL);
      diff = timeval_diff(chk->start,now);
      seconds = (double)diff.tv_sec + ((double)diff.tv_usec)/1000000.0;

      printf("%.3f %.6f %.6f %.6f %.6f %.6f %.6f %.6f %6d check %d\n",
             seconds,
             100.0*chk->incorrect_nodes/(double)chk->ncount,
             100.0*chk->incorrect_successor/(double)chk->ncount,
             100.0*chk->incorrect_predecessor/(double)chk->ncount,
             100.0*chk->incorrect_fingers/(double)(MBITS*chk->ncount),
             100.0*chk->total_bad/(double)chk->total_lookups,
             chk->total_hops/(double)chk->total_lookups,
             100.0*chk->incorrect_succlist/(double)(NSUCCESSORS*chk->ncount),
             chk->ncount,
             chk->iterations);
      node_send(chk->n,chk->endpt->epid.localid,chk->caller,MSG_DEBUG_DONE,NULL,0);
      chk->indebug = 0;
      chk->iterations++;
    }
  }
  else {
    chordid id = rand()%KEYSPACE;
    find_successor_msg fsm;
    fsm.id = id;
    fsm.sender = chk->endpt->epid;
    fsm.hops = 0;
    fsm.payload = 0;
    node_send(chk->n,chk->endpt->epid.localid,chk->nodes[chk->cur_node].epid,
              MSG_FIND_SUCCESSOR,&fsm,sizeof(find_successor_msg));
    chk->lookup_id = id;
  }
}

static void check_reply_table(check *chk, reply_table_msg *rtm)
{
  if (!chk->indebug)
    return;
  chordnode exp_successor = chk->nodes[(chk->cur_node+1)%chk->ncount];
  chordnode exp_predecessor = chk->nodes[(chk->cur_node+chk->ncount-1)%chk->ncount];
  int k;

  /* Check successor pointer */
  if (chordnode_isnull(rtm->fingers[1]) || (rtm->fingers[1].id != exp_successor.id))
    chk->incorrect_successor++;

  /* Check predecessor */
  if (chordnode_isnull(rtm->predecessor) || (rtm->predecessor.id != exp_predecessor.id))
    chk->incorrect_predecessor++;

  /* Check fingers (including successor) */
  for (k = 1; k <= MBITS; k++) {
    chordid finger_k_start = chk->nodes[chk->cur_node].id+(int)pow(2,k-1);
    chordid exp_fid = manual_hash(finger_k_start,chk->nodes,chk->ncount);
    if (chordnode_isnull(rtm->fingers[k]) || (rtm->fingers[k].id != exp_fid))
      chk->incorrect_fingers++;
  }

  /* Check successor list */
  for (k = 1; k <= NSUCCESSORS; k++) {
    chordnode exp_successor = chk->nodes[(chk->cur_node+1+k)%chk->ncount];
    if (chordnode_isnull(rtm->successors[k]) || (rtm->successors[k].id != exp_successor.id))
      chk->incorrect_succlist++;
  }

  chk->cur_lookup = 0;
  check_check_next(chk);
}

static void check_got_successor(check *chk, got_successor_msg *m)
{
  if (!chk->indebug)
    return;

  assert(chk->indebug);
  chordid expected = manual_hash(chk->lookup_id,chk->nodes,chk->ncount);
  chk->total_hops += m->hops;
  if (m->successor.id != expected)
    chk->total_bad++;
  chk->total_lookups++;

  chk->cur_lookup++;
  check_check_next(chk);
}

static void check_endpoint_exit(check *chk, endpoint_exit_msg *m)
{
  endpointid exited = m->epid;
  int i;
  int nodeno = -1;

  for (i = 0; i < chk->ncount; i++) {
    if (endpointid_equals(&chk->nodes[i].epid,&exited))
      nodeno = i;
  }

  printf("debug_loop: got endpoint_exit: endpoint "EPID_FORMAT", nodeno %d\n",
         EPID_ARGS(exited),nodeno);

  assert(0 <= nodeno);
  memmove(&chk->nodes[nodeno],&chk->nodes[nodeno+1],(chk->ncount-nodeno-1)*sizeof(chordnode));
  chk->ncount--;

  check_abort(chk);
}

static void check_chord_started(check *chk, chord_started_msg *m)
{
  assert(chk->ncount+1 < MAX_NODES);
  chk->nodes[chk->ncount] = m->cn;
  chk->ncount++;
  qsort(chk->nodes,chk->ncount,sizeof(chordnode),chordnode_cmp);

  lock_node(chk->n);
  endpoint_link(chk->endpt,m->cn.epid);
  unlock_node(chk->n);

  printf("chk chord_started %d: now have %d nodes\n",m->cn.id,chk->ncount);

  check_abort(chk);
}

static void check_id_changed(check *chk, id_changed_msg *m)
{
  int i;
  for (i = 0; i < chk->ncount; i++) {
    if (endpointid_equals(&chk->nodes[i].epid,&m->cn.epid)) {
      printf(EPID_FORMAT" changed id %d -> %d\n",EPID_ARGS(m->cn.epid),m->oldid,m->cn.id);
      assert(chk->nodes[i].id == m->oldid);
      chk->nodes[i].id = m->cn.id;
      qsort(chk->nodes,chk->ncount,sizeof(chordnode),chordnode_cmp);
      check_abort(chk);
      return;
    }
  }
  fatal("received ID_CHANGED for unknown endpointid "EPID_FORMAT,EPID_ARGS(m->cn.epid));
}

static void check_joined(check *chk, joined_msg *m)
{
  chk->pending_joins--;
  printf("#%d ("EPID_FORMAT") has joined the network; %d pending\n",
         m->cn.id,EPID_ARGS(m->cn.epid),chk->pending_joins);
  check_abort(chk);
}

static void check_loop(node *n, endpoint *endpt, chordnode *nodes, int ncount2,
                         struct timeval start, int pending_joins,
                         endpointid *managerids, int nmanagers)
{
  int done = 0;
  check *chk = (check*)calloc(1,sizeof(check));
  chk->n = n;
  chk->endpt = endpt;

  memset(&chk->caller,0,sizeof(endpointid));
  chk->ncount = ncount2;
  chk->start = start;
  chk->nodes = nodes;
  chk->pending_joins = pending_joins;
  chk->managerids = managerids;
  chk->nmanagers = nmanagers;

  while (!done) {
    message *msg = endpoint_next_message(endpt,6000);
    if (NULL == msg) {
      printf("debug_loop: timeout: node %d, lookup %d\n",chk->cur_node,chk->cur_lookup);
      abort();
    }
    switch (msg->hdr.tag) {
    case MSG_DEBUG_START:
      assert(sizeof(endpointid) == msg->hdr.size);
      check_debug_start(chk,(endpointid*)msg->data);
      break;
    case MSG_REPLY_TABLE:
      assert(sizeof(reply_table_msg) == msg->hdr.size);
      check_reply_table(chk,(reply_table_msg*)msg->data);
      break;
    case MSG_GOT_SUCCESSOR:
      assert(sizeof(got_successor_msg) == msg->hdr.size);
      check_got_successor(chk,(got_successor_msg*)msg->data);
      break;
    case MSG_ENDPOINT_EXIT:
      assert(sizeof(endpoint_exit_msg) == msg->hdr.size);
      check_endpoint_exit(chk,(endpoint_exit_msg*)msg->data);
      break;
    case MSG_CHORD_STARTED:
      assert(sizeof(chord_started_msg) == msg->hdr.size);
      check_chord_started(chk,(chord_started_msg*)msg->data);
      break;
    case MSG_ID_CHANGED:
      assert(sizeof(id_changed_msg) == msg->hdr.size);
      check_id_changed(chk,(id_changed_msg*)msg->data);
      break;
    case MSG_JOINED:
      assert(sizeof(joined_msg) == msg->hdr.size);
      check_joined(chk,(joined_msg*)msg->data);
      break;
    case MSG_KILL:
      done = 1;
      break;
    }
  }
  free(chk);
}

static void delay(int ms)
{
  struct timespec ts;
  ts.tv_sec = ms/1000;
  ts.tv_nsec = (ms%1000)*1000000;
  nanosleep(&ts,NULL);
}

static void testchord(node *n, endpointid *managerids, int nmanagers)
{
  chordnode nodes[MAX_NODES];
  int i;
  endpoint *endpt = node_add_endpoint(n,0,TEST_ENDPOINT,NULL,endpoint_close_kill);
  struct timeval start;
  int ncount = 0;
  debug_control dbc;
  pthread_t dbc_thread;
  chordnode null_node;

  printf("managers:\n");
  for (i = 0; i < nmanagers; i++)
    printf(EPID_FORMAT"\n",EPID_ARGS(managerids[i]));

  memset(&nodes,0,MAX_NODES*sizeof(chordnode));
  memset(&null_node,0,sizeof(chordnode));

  delay(250); /* wait for manager to start; FIXME: find a more robust solution */

  nodes[0] = start_one_chord(n,endpt,null_node,managerids[rand()%nmanagers]);
  ncount = 1;
  printf("initial node = #%d ("EPID_FORMAT")\n",nodes[0].id,EPID_ARGS(nodes[0].epid));

  for (i = 1; i < NODE_COUNT; i++)
    add_node(n,endpt,managerids[rand()%nmanagers],nodes[0]);

  gettimeofday(&start,NULL);

  dbc.n = n;
  dbc.epid = endpt->epid;
  if (0 != pthread_create(&dbc_thread,NULL,debug_control_thread,&dbc))
    fatal("pthread_create: %s",strerror(errno));

  check_loop(n,endpt,nodes,ncount,start,NODE_COUNT,managerids,nmanagers);
  node_remove_endpoint(n,endpt);
}

void run_chordtest(int argc, char **argv)
{
  node *n = node_new(LOG_ERROR);

  if (NULL == node_listen(n,n->listenip,0,NULL,NULL,0,1))
    exit(1);

  start_manager(n);
  node_start_iothread(n);

  if (0 == argc) {
    /* Standalone mode; run the test locally */
    endpointid managerid;
    managerid.ip = n->listenip;
    managerid.port = n->listenport;
    managerid.localid = MANAGER_ID;
    testchord(n,&managerid,1);
  }
  else {
    /* Client mode; run the chord test on a set of remote nodes specified in the file */
    endpointid *managerids;
    int nmanagers;
    if (0 != read_managers(n,argv[0],&managerids,&nmanagers))
      exit(1);
    testchord(n,managerids,nmanagers);
    free(managerids);
  }

  if (0 != pthread_join(n->iothread,NULL))
    fatal("pthread_join: %s",strerror(errno));
  node_close_endpoints(n);
  node_close_connections(n);
  node_free(n);
}
