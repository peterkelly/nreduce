/*
 * This file is part of the nreduce project
 * Copyright (C) 2006-2009 Peter Kelly (pmk@cs.adelaide.edu.au)
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

static void debug_control_thread(node *n, endpoint *endpt, void *arg)
{
  debug_control *dbc = (debug_control*)arg;
  message *msg;
  int done = 0;
  int indebug = 0;

  while (!done) {
    msg = endpoint_receive(endpt,indebug ? -1 : DEBUG_DELAY);
    if (NULL == msg) {
      assert(!indebug);
      endpoint_send(endpt,dbc->epid,MSG_DEBUG_START,&endpt->epid,sizeof(endpointid));
      indebug = 1;
    }
    else {
      switch (msg->tag) {
      case MSG_DEBUG_DONE:
        assert(indebug);
        indebug = 0;
        break;
      case MSG_KILL:
        fatal("Node shutdown during chord test");
        break;
      default:
        fatal("debug_control received invalid message: %d",msg->tag);
        break;
      }
      message_free(msg);
    }
  }

  free(dbc);
}

static chordnode start_one_chord(node *n, endpoint *endpt, endpointid initial, endpointid managerid)
{
  int done = 0;
  chordnode cn;
  start_chord_msg scm;

  memset(&cn,0,sizeof(chordnode));

  scm.initial = initial;
  scm.caller = endpt->epid;
  scm.stabilize_delay = STABILIZE_DELAY;
  endpoint_send(endpt,managerid,MSG_START_CHORD,&scm,sizeof(start_chord_msg));

  while (!done) {
    message *msg = endpoint_receive(endpt,20000);
    if (!msg) {
      fatal("Timeout waiting for CHORD_STARTED message");
    }
    switch (msg->tag) {
    case MSG_CHORD_STARTED: {
      chord_started_msg *m = (chord_started_msg*)msg->data;
      assert(sizeof(chord_started_msg) == msg->size);
      endpoint_link(endpt,m->cn.epid);
      cn = m->cn;
      done = 1;
      break;
    }
    case MSG_KILL:
      fatal("Node shutdown during chord test");
      break;
    default:
      fatal("start_one_chord received invalid message: %d",msg->tag);
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
  int incorrect_links;
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

  struct timeval debug_start;
  struct timeval node_start;
} check;

static void check_abort(check *chk)
{
  if (chk->indebug) {
    endpoint_send(chk->endpt,chk->caller,MSG_DEBUG_DONE,NULL,0);
    chk->indebug = 0;
  }
}

static void add_node(node *n, endpoint *endpt, endpointid managerid, endpointid initial)
{
  start_chord_msg scm;
  scm.initial = initial;
  scm.caller = endpt->epid;
  scm.stabilize_delay = STABILIZE_DELAY;
  endpoint_send(endpt,managerid,MSG_START_CHORD,&scm,sizeof(start_chord_msg));
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
  chk->incorrect_links = 0;
  chk->incorrect_fingers = 0;
  chk->incorrect_succlist = 0;
  chk->total_bad = 0;
  chk->total_hops = 0;
  chk->total_lookups = 0;

  gettimeofday(&chk->debug_start,NULL);

  if (0 < chk->pending_joins) {
    printf("skipping debug, due to %d pending joins\n",chk->pending_joins);
    check_abort(chk);
  }
  else if ((1 <= chk->iterations) && (0 == (chk->iterations % DISRUPT_INTERVAL))) {
    if (0 == (chk->iterations % (2*DISRUPT_INTERVAL))) {
      /* Add some new nodes */
      endpointid initial = chk->nodes[rand()%chk->ncount].epid;
      int i;
      for (i = 0; i < JOIN_COUNT; i++) {
        add_node(chk->n,chk->endpt,chk->managerids[rand()%chk->nmanagers],initial);
        chk->pending_joins++;
      }
    }
    else {
      /* Kill some existing nodes */
      int i;
      int j;
      int *killindices = (int*)calloc(KILL_COUNT,sizeof(int));
      for (i = 0; i < KILL_COUNT; i++) {
        int have;
        int index;
        do {
          have = 0;
          index = rand()%chk->ncount;

          for (j = 0; j < i; j++)
            if (killindices[j] == index)
              have = 1;
        } while (have);
        killindices[i] = index;
      }

      for (i = 0; i < KILL_COUNT; i++) {
        endpoint_send(chk->endpt,chk->nodes[killindices[i]].epid,MSG_KILL,NULL,0);
      }

      free(killindices);
    }

    chk->iterations++;
    check_abort(chk);
  }
  else {
    get_table_msg gtm;
    gtm.sender = chk->endpt->epid;
    gettimeofday(&chk->node_start,NULL);
    endpoint_send(chk->endpt,chk->nodes[0].epid,MSG_GET_TABLE,&gtm,sizeof(gtm));
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

      #ifdef CHORDTEST_TIMING
      struct timeval now;
      gettimeofday(&now,NULL);
      printf("check node %d took %d\n",chk->cur_node-1,timeval_diffms(chk->node_start,now));
      #endif

      gfm.sender = chk->endpt->epid;
      gettimeofday(&chk->node_start,NULL);
      endpoint_send(chk->endpt,chk->nodes[chk->cur_node].epid,MSG_GET_TABLE,
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

      #ifdef CHORDTEST_TIMING
      printf("check round took %d\n",timeval_diffms(chk->debug_start,now));
      #endif

      printf("%.3f %.6f %.6f %.6f %.6f %.6f %.6f %.6f %6d check %d\n",
             seconds,
             100.0*chk->incorrect_nodes/(double)chk->ncount,
             100.0*chk->incorrect_successor/(double)chk->ncount,
             100.0*chk->incorrect_links/(double)chk->ncount,
             100.0*chk->incorrect_fingers/(double)(MBITS*chk->ncount),
             100.0*chk->total_bad/(double)chk->total_lookups,
             chk->total_hops/(double)chk->total_lookups,
             100.0*chk->incorrect_succlist/(double)(NSUCCESSORS*chk->ncount),
             chk->ncount,
             chk->iterations);
      endpoint_send(chk->endpt,chk->caller,MSG_DEBUG_DONE,NULL,0);
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
    endpoint_send(chk->endpt,chk->nodes[chk->cur_node].epid,
              MSG_FIND_SUCCESSOR,&fsm,sizeof(find_successor_msg));
    chk->lookup_id = id;
  }
}

static void check_reply_table(check *chk, reply_table_msg *rtm)
{
  if (!chk->indebug)
    return;
  chordnode exp_successor = chk->nodes[(chk->cur_node+1)%chk->ncount];
  int k;

  /* Check successor pointer */
  if (chordnode_isnull(rtm->fingers[1]) || (rtm->fingers[1].id != exp_successor.id))
    chk->incorrect_successor++;

  /* Check links */
  if (!rtm->linksok)
    chk->incorrect_links++;

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
  endpoint_link(chk->endpt,m->cn.epid);
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
  assert(0 <= chk->pending_joins);
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
    message *msg = endpoint_receive(endpt,6000);
    if (NULL == msg) {
      printf("debug_loop: timeout: node %d, lookup %d\n",chk->cur_node,chk->cur_lookup);
      abort();
    }
    switch (msg->tag) {
    case MSG_DEBUG_START:
      assert(sizeof(endpointid) == msg->size);
      check_debug_start(chk,(endpointid*)msg->data);
      break;
    case MSG_REPLY_TABLE:
      assert(sizeof(reply_table_msg) == msg->size);
      check_reply_table(chk,(reply_table_msg*)msg->data);
      break;
    case MSG_GOT_SUCCESSOR:
      assert(sizeof(got_successor_msg) == msg->size);
      check_got_successor(chk,(got_successor_msg*)msg->data);
      break;
    case MSG_ENDPOINT_EXIT:
      assert(sizeof(endpoint_exit_msg) == msg->size);
      check_endpoint_exit(chk,(endpoint_exit_msg*)msg->data);
      break;
    case MSG_CHORD_STARTED:
      assert(sizeof(chord_started_msg) == msg->size);
      check_chord_started(chk,(chord_started_msg*)msg->data);
      break;
    case MSG_ID_CHANGED:
      assert(sizeof(id_changed_msg) == msg->size);
      check_id_changed(chk,(id_changed_msg*)msg->data);
      break;
    case MSG_JOINED:
      assert(sizeof(joined_msg) == msg->size);
      check_joined(chk,(joined_msg*)msg->data);
      break;
    case MSG_KILL:
      fatal("Node shutdown during chord test");
      break;
    }
  }
  free(chk);
}

typedef struct {
  endpointid *managerids;
  int nmanagers;
} testchord_arg;

static void testchord_thread(node *n, endpoint *endpt, void *arg)
{
  testchord_arg *tca = (testchord_arg*)arg;
  chordnode nodes[MAX_NODES];
  int i;
  struct timeval start;
  int ncount = 0;
  debug_control dbc;
  endpointid null_epid;

  printf("managers:\n");
  for (i = 0; i < tca->nmanagers; i++)
    printf(EPID_FORMAT"\n",EPID_ARGS(tca->managerids[i]));

  memset(&nodes,0,MAX_NODES*sizeof(chordnode));
  memset(&null_epid,0,sizeof(endpointid));

  nodes[0] = start_one_chord(n,endpt,null_epid,tca->managerids[rand()%tca->nmanagers]);
  ncount = 1;
  printf("initial node = #%d ("EPID_FORMAT")\n",nodes[0].id,EPID_ARGS(nodes[0].epid));

  for (i = 1; i < NODE_COUNT; i++)
    add_node(n,endpt,tca->managerids[rand()%tca->nmanagers],nodes[0].epid);

  gettimeofday(&start,NULL);

  dbc.n = n;
  dbc.epid = endpt->epid;
  node_add_thread(n,"debug_control",debug_control_thread,&dbc,NULL);

  check_loop(n,endpt,nodes,ncount,start,NODE_COUNT,tca->managerids,tca->nmanagers);
}

static void testchord(node *n, endpointid *managerids, int nmanagers)
{
  pthread_t thread;
  testchord_arg *tca = (testchord_arg*)calloc(1,sizeof(testchord_arg));
  tca->managerids = managerids;
  tca->nmanagers = nmanagers;
  node_add_thread(n,"testchord",testchord_thread,tca,&thread);
  if (0 != pthread_join(thread,NULL))
    fatal("pthread_join: %s",strerror(errno));
}

static array *read_nodes(const char *hostsfile)
{
  array *nodenames;
  int start = 0;
  int pos = 0;
  array *filedata;
  FILE *f;
  int r;
  char buf[1024];

  if (NULL == (f = fopen(hostsfile,"r"))) {
    perror(hostsfile);
    return NULL;
  }

  filedata = array_new(1,0);
  while (0 < (r = fread(buf,1,1024,f)))
    array_append(filedata,buf,r);
  fclose(f);

  nodenames = array_new(sizeof(char*),0);
  while (1) {
    if ((pos == filedata->nbytes) ||
        ('\n' == filedata->data[pos])) {
      if (pos > start) {
        char *line = (char*)malloc(pos-start+1);
        memcpy(line,&filedata->data[start],pos-start);
        line[pos-start] = '\0';
        array_append(nodenames,&line,sizeof(char*));
      }
      start = pos+1;
    }
    if (pos == filedata->nbytes)
      break;
    pos++;
  }

  free(filedata);
  return nodenames;
}

static int read_managers(node *n, const char *nodesfile, endpointid **outids, int *outcount)
{
  endpointid *managerids;
  int count;
  array *nodes = read_nodes(nodesfile);
  int i;

  *outids = NULL;
  *outcount = 0;

  if (NULL == nodes)
    return -1;

  count = array_count(nodes);
  managerids = (endpointid*)calloc(count,sizeof(endpointid));

  for (i = 0; i < count; i++) {
    char *nodename = array_item(nodes,i,char*);
    char *host = NULL;
    int port = 0;

    if (NULL == strchr(nodename,':')) {
      host = strdup(nodename);
      port = WORKER_PORT;
    }
    else if (0 > parse_address(nodename,&host,&port)) {
      fprintf(stderr,"Invalid node address: %s\n",nodename);
      array_free(nodes);
      free(managerids);
      return -1;
    }
    managerids[i].port = port;
    managerids[i].localid = MANAGER_ID;

    if (0 > lookup_address(host,&managerids[i].ip,NULL)) {
      fprintf(stderr,"%s: hostname lookup failed\n",host);
      array_free(nodes);
      free(managerids);
      return -1;
    }
    free(host);
  }

  *outids = managerids;
  *outcount = count;

  array_free(nodes);
  return 0;
}

void run_chordtest(int argc, char **argv)
{
  node *n = node_start(LOG_ERROR,0);
  if (NULL == n)
    exit(1);

  if (0 == argc) {
    /* Standalone mode; run the test locally */
    endpointid managerid;
    managerid.ip = n->listenip;
    managerid.port = n->listenport;
    managerid.localid = MANAGER_ID;
    start_manager(n);
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

  node_run(n);
}
