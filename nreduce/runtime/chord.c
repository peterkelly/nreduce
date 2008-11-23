/*
 * This file is part of the nreduce project
 * Copyright (C) 2006-2008 Peter Kelly (pmk@cs.adelaide.edu.au)
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

#define RANDOM_IDS

#ifdef DEBUG_CHORD
#define CHORD_DEBUG(...) chord_debug(crd->self,__func__,__VA_ARGS__)

static void chord_debug(chordnode cn, const char *function, const char *format, ...)
{
  array *arr = array_new(1,0);
  char end = '\0';
  char str[100];
  char idstr[100];
  va_list ap;

  if (!strncmp(function,"chord_",strlen("chord_")))
    function += strlen("chord_");
  else
    function = "";

  sprintf(str,EPID_FORMAT,EPID_ARGS(cn.epid));
  sprintf(idstr,"#%d",cn.id);
  array_printf(arr,"%18s %6s %14s: ",str,idstr,function);
  va_start(ap,format);
  array_vprintf(arr,format,ap);
  va_end(ap);

  array_append(arr,&end,1);
  printf("%s\n",arr->data);
  array_free(arr);
}
#else
#define CHORD_DEBUG(...) ((void)0)
#endif

typedef struct {
  node *n;
  endpointid chord_epid;
  pthread_t thread;
  int stabilize_delay;
} stabilizer;

static void stabilizer_thread(node *n, endpoint *endpt, void *arg)
{
  stabilizer *stb = (stabilizer*)arg;
  message *msg;
  int done = 0;

  endpoint_link(endpt,stb->chord_epid);
  endpoint_send(endpt,stb->chord_epid,MSG_STABILIZE,NULL,0);

  while (!done) {
    int delay = stb->stabilize_delay/2 + rand()%stb->stabilize_delay;
    msg = endpoint_receive(endpt,delay);
    if (NULL == msg) {
      endpoint_send(endpt,stb->chord_epid,MSG_STABILIZE,NULL,0);
    }
    else {
      switch (msg->tag) {
      case MSG_ENDPOINT_EXIT: {
        endpoint_exit_msg *eem = (endpoint_exit_msg*)msg->data;
        assert(sizeof(endpoint_exit_msg) == msg->size);
        assert(endpointid_equals(&eem->epid,&stb->chord_epid));
        done = 1;
        break;
      }
      case MSG_KILL:
        done = 1;
        break;
      default:
        fatal("Stabilizer received invalid message: %d",msg->tag);
        break;
      }
      message_free(msg);
    }
  }

  free(stb);
}

static void start_stabilizer(node *n, endpointid chord_epid, int stabilize_delay)
{
  stabilizer *stb = (stabilizer*)calloc(1,sizeof(stabilizer));
  stb->n = n;
  stb->chord_epid = chord_epid;
  stb->stabilize_delay = stabilize_delay;
  node_add_thread(n,"stabilizer",stabilizer_thread,stb,NULL);
}

typedef struct {
  node *n;
  pthread_t thread;
  endpointid caller;
  endpointid initial;
  chordnode self;
  chordnode fingers[MBITS+1];
  chordnode successors[NSUCCESSORS+1];
  endpoint *endpt;
  int stabilize_delay;
} chord;

int chordnode_isnull(chordnode n)
{
  return ((INADDR_ANY == n.epid.ip) && (0 == n.epid.port));
}

static int inrange_open(chordid val, chordid min, chordid max)
{
  if (min < max)
    return ((val > min) && (val < max));
  else
    return ((val > min) || (val < max));
}

static int inrange_open_closed(chordid val, chordid min, chordid max)
{
  if (min < max)
    return ((val > min) && (val <= max));
  else
    return ((val > min) || (val <= max));
}

static chordnode closest_preceding_finger(chord *crd, chordid id)
{
  int findex;

  for (findex = MBITS; 1 <= findex; findex--)
    if (!chordnode_isnull(crd->fingers[findex]) &&
        inrange_open(crd->fingers[findex].id,crd->self.id,id))
      return crd->fingers[findex];

  return crd->self;
}

static int check_links(chord *crd)
{
  int count = MBITS-1;
  endpointid *epids = (endpointid*)malloc(count*sizeof(endpointid));
  int i;
  int r;
  for (i = 1; i < MBITS; i++)
    epids[i-1] = crd->fingers[i].epid;
  r = endpoint_check_links(crd->endpt,epids,count);
  free(epids);
  return r;
}

static int count_instances(chord *crd, endpointid epid)
{
  int i;
  int count = 0;
  for (i = 1; i < MBITS; i++) {
    if (!chordnode_isnull(crd->fingers[i]) &&
        endpointid_equals(&crd->fingers[i].epid,&epid))
      count++;
  }
  return count;
}

static void set_noderef(chord *crd, chordnode *ptr, chordnode cn)
{
  int oldinst = 0;
  int newinst = 0;
  if ((ptr->id == cn.id) && endpointid_equals(&ptr->epid,&cn.epid))
    return;

  if (!chordnode_isnull(*ptr))
    oldinst = count_instances(crd,ptr->epid);
  if (!chordnode_isnull(cn))
    newinst = count_instances(crd,cn.epid);

  if (1 == oldinst)
    endpoint_unlink(crd->endpt,ptr->epid);
  *ptr = cn;
  if (!chordnode_isnull(cn))
    endpoint_link(crd->endpt,cn.epid);
}

static void set_finger(chord *crd, int i, chordnode newfinger)
{
  set_noderef(crd,&crd->fingers[i],newfinger);
}

static void set_successor(chord *crd, chordnode newsuccessor)
{
  set_finger(crd,1,newsuccessor);
}

static void chord_clear_tables(chord *crd)
{
  int i;
  chordnode null_node;
  memset(&null_node,0,sizeof(chordnode));

  for (i = 1; i <= MBITS; i++)
    set_finger(crd,i,null_node);
  memset(&crd->successors,0,sizeof(crd->successors));
}

static void chord_notify_joined(chord *crd, chordnode newnode)
{
  if (0 != crd->caller.localid) {
    joined_msg jm;
    jm.cn = crd->self;
    endpoint_send(crd->endpt,crd->caller,MSG_JOINED,&jm,sizeof(jm));
  }
}

static void chord_join(chord *crd)
{
  assert((MBITS+1)*sizeof(chordnode) == sizeof(crd->fingers));
  assert((NSUCCESSORS+1)*sizeof(chordnode) == sizeof(crd->successors));

  chord_clear_tables(crd);

  if (0 == crd->initial.localid) {
    CHORD_DEBUG("no existing node");
    set_successor(crd,crd->self);
    chord_notify_joined(crd,crd->self);
  }
  else {
    find_successor_msg fsm;
    CHORD_DEBUG("using ndash ("EPID_FORMAT")",EPID_ARGS(crd->initial));
    fsm.id = crd->self.id;
    fsm.sender = crd->self.epid;
    fsm.hops = 0;
    fsm.payload = 1;
    endpoint_send(crd->endpt,crd->initial,MSG_FIND_SUCCESSOR,&fsm,sizeof(fsm));
  }
}

static void chord_find_successor(chord *crd, find_successor_msg *m)
{
  chordnode successor = crd->fingers[1];

  assert(!chordnode_isnull(successor));

  if (inrange_open_closed(m->id,crd->self.id,successor.id)) {
    if (1 == m->payload) {
      insert_msg im;
      im.predecessor = crd->self.epid;
      im.successor = successor;
      endpoint_send(crd->endpt,m->sender,MSG_INSERT,&im,sizeof(im));
    }
    else {
      got_successor_msg gsm;
      gsm.successor = successor;
      gsm.hops = m->hops;
      gsm.payload = m->payload;
      endpoint_send(crd->endpt,m->sender,MSG_GOT_SUCCESSOR,&gsm,sizeof(gsm));
    }
  }
  else {
    chordnode closest = closest_preceding_finger(crd,m->id);
    m->hops++;
    endpoint_send(crd->endpt,closest.epid,MSG_FIND_SUCCESSOR,m,sizeof(*m));
  }
}

static void duplicate_detected(chord *crd, chordnode existing)
{
  chordid oldid = crd->self.id;
  #ifdef RANDOM_IDS
  chordid newid = rand()%KEYSPACE;
  #else
  chordid newid = crd->self.id+1;
  #endif

  CHORD_DEBUG("duplicate; existing #%d ("EPID_FORMAT"); using new id #%d",
              existing.id,EPID_ARGS(existing.epid),newid);

  crd->self.id = newid;

  if (0 != crd->caller.localid) {
    id_changed_msg idcm;
    idcm.cn = crd->self;
    idcm.oldid = oldid;
    endpoint_send(crd->endpt,crd->caller,MSG_ID_CHANGED,&idcm,sizeof(idcm));
  }

  chord_join(crd);
}

static void chord_insert(chord *crd, insert_msg *m)
{
  if (m->successor.id == crd->self.id) {
    /* Node with our id already exists in the network; we have to pick a new one */
    duplicate_detected(crd,m->successor);
  }
  else {
    /* Initialize our successor and try to update the sender's successor pointer */
    set_next_msg snm;
    CHORD_DEBUG("successor %d ("EPID_FORMAT"), predecessor ("EPID_FORMAT")",
                m->successor.id,EPID_ARGS(m->successor.epid),EPID_ARGS(m->predecessor));
    set_successor(crd,m->successor);
    snm.new_successor = crd->self;
    snm.old_successor = m->successor;
    endpoint_send(crd->endpt,m->predecessor,MSG_SET_NEXT,&snm,sizeof(snm));
  }
}

static void chord_set_next(chord *crd, set_next_msg *m)
{
  chordnode successor = crd->fingers[1];
  if (!endpointid_equals(&m->old_successor.epid,&successor.epid)) {
    /* Another node joined since we sent this one the insert message; send another insert */
    insert_msg im;
    CHORD_DEBUG("mismatch: new %d ("EPID_FORMAT"), old %d ("EPID_FORMAT"); sending another insert",
                m->new_successor.id,EPID_ARGS(m->new_successor.epid),
                successor.id,EPID_ARGS(successor.epid));

    assert(m->old_successor.id != successor.id);
    assert(m->new_successor.id != crd->self.id);

    if (inrange_open_closed(m->new_successor.id,crd->self.id,successor.id)) {
      /* New node sits between me and my succcessor */
      im.predecessor = crd->self.epid;
      im.successor = successor;
    }
    else {
      /* New node comes after my successor */
      im.predecessor = successor.epid;
      im.successor = m->old_successor;
    }

    endpoint_send(crd->endpt,m->new_successor.epid,MSG_INSERT,&im,sizeof(im));
  }
  else {
    /* Successor pointers match; accept the new node into the network */
    CHORD_DEBUG("match: new %d ("EPID_FORMAT"), old %d ("EPID_FORMAT"); join successful",
                m->new_successor.id,EPID_ARGS(m->new_successor.epid),
                successor.id,EPID_ARGS(successor.epid));
    assert(m->old_successor.id == successor.id);
    set_successor(crd,m->new_successor);
    chord_notify_joined(crd,m->new_successor);
  }
}

static void chord_get_succlist(chord *crd, get_succlist_msg *m)
{
  reply_succlist_msg rsm;
  int i;

  rsm.sender = crd->self.epid;
  rsm.successors[1] = crd->fingers[1];
  for (i = 2; i <= NSUCCESSORS; i++)
    rsm.successors[i] = crd->successors[i-1];

  endpoint_send(crd->endpt,m->sender,MSG_REPLY_SUCCLIST,&rsm,sizeof(rsm));
}

static void chord_reply_succlist(chord *crd, reply_succlist_msg *m)
{
  /* Ignore it if our successor has changed since the request */
  if (!endpointid_equals(&m->sender,&crd->fingers[1].epid))
    return;

  memcpy(&crd->successors,m->successors,sizeof(m->successors));
}

static void chord_got_successor(chord *crd, got_successor_msg *m)
{
  assert(1 < m->payload);
  assert(!chordnode_isnull(m->successor));
  assert((m->successor.id != crd->self.id) ||
         endpointid_equals(&m->successor.epid,&crd->self.epid));

  set_finger(crd,m->payload,m->successor);
}

static void chord_stabilize(chord *crd)
{
  chordnode successor = crd->fingers[1];
  get_succlist_msg gsm;

  if (chordnode_isnull(successor))
    return; /* still joining */

  gsm.sender = crd->self.epid;
  endpoint_send(crd->endpt,successor.epid,MSG_GET_SUCCLIST,&gsm,sizeof(gsm));

  /* Update a random finger */
  int k;
  find_successor_msg fsm;
  k = 2+rand()%(MBITS-2);
  fsm.id = crd->self.id+(int)pow(2,k-1);
  fsm.sender = crd->self.epid;
  fsm.hops = 0;
  fsm.payload = k;
  endpoint_send(crd->endpt,crd->self.epid,MSG_FIND_SUCCESSOR,&fsm,sizeof(fsm));
}

static void chord_get_table(chord *crd, get_table_msg *m)
{
  reply_table_msg rtm;

  rtm.cn = crd->self;
  memcpy(&rtm.fingers,&crd->fingers,sizeof(crd->fingers));
  memcpy(&rtm.successors,&crd->successors,sizeof(crd->successors));
  rtm.linksok = check_links(crd);
  endpoint_send(crd->endpt,m->sender,MSG_REPLY_TABLE,&rtm,sizeof(rtm));
}

static void chord_endpoint_exit(chord *crd, endpoint_exit_msg *m)
{
  int i;
  chordnode null_node;
  memset(&null_node,0,sizeof(chordnode));
  CHORD_DEBUG("received ENDPOINT_EXIT for "
              EPID_FORMAT,EPID_ARGS(m->epid));

  if (endpointid_equals(&m->epid,&crd->fingers[1].epid)) {
    if (!chordnode_isnull(crd->successors[1])) {
      CHORD_DEBUG("successor %d failed! replacement %d",crd->fingers[1].id,crd->successors[1].id);
      set_finger(crd,1,crd->successors[1]);
      memmove(&crd->successors[1],&crd->successors[2],(NSUCCESSORS-1)*sizeof(chordnode));
    }
    else {
      CHORD_DEBUG("successor %d failed! no spare successors!",crd->fingers[1].id);
      /* If we get here, we can no longer be part of the network, and must rejoin */
      printf("*********** #%d IS OUT OF NETWORK ************\n",crd->self.id);
      exit(1);
    }
  }

  for (i = 2; i <= MBITS; i++) {
    if (endpointid_equals(&m->epid,&crd->fingers[i].epid))
      set_finger(crd,i,null_node);
  }
}

static void chord_thread(node *n, endpoint *endpt, void *arg)
{
  chord *crd = (chord*)arg;
  message *msg;
  int done = 0;
  assert(crd->n);
  assert(NULL == crd->endpt);
  crd->endpt = endpt;

  #ifdef RANDOM_IDS
  crd->self.id = rand()%KEYSPACE;
  #else
  crd->self.id = 4;
  #endif
  crd->self.epid = endpt->epid;

  if (0 != crd->caller.localid) {
    chord_started_msg csm;
    csm.cn = crd->self;
    endpoint_send(crd->endpt,crd->caller,MSG_CHORD_STARTED,&csm,sizeof(csm));
  }

  start_stabilizer(n,crd->self.epid,crd->stabilize_delay);

  chord_join(crd);

  while (!done) {
    msg = endpoint_receive(endpt,-1);
    switch (msg->tag) {
    case MSG_FIND_SUCCESSOR:
      assert(sizeof(find_successor_msg) == msg->size);
      chord_find_successor(crd,(find_successor_msg*)msg->data);
      break;
    case MSG_INSERT:
      assert(sizeof(insert_msg) == msg->size);
      chord_insert(crd,(insert_msg*)msg->data);
      break;
    case MSG_SET_NEXT:
      assert(sizeof(set_next_msg) == msg->size);
      chord_set_next(crd,(set_next_msg*)msg->data);
      break;
    case MSG_GET_SUCCLIST:
      assert(sizeof(get_succlist_msg) == msg->size);
      chord_get_succlist(crd,(get_succlist_msg*)msg->data);
      break;
    case MSG_REPLY_SUCCLIST:
      assert(sizeof(reply_succlist_msg) == msg->size);
      chord_reply_succlist(crd,(reply_succlist_msg*)msg->data);
      break;
    case MSG_GOT_SUCCESSOR:
      assert(sizeof(got_successor_msg) == msg->size);
      chord_got_successor(crd,(got_successor_msg*)msg->data);
      break;
    case MSG_STABILIZE:
      chord_stabilize(crd);
      break;
    case MSG_GET_TABLE:
      assert(sizeof(get_table_msg) == msg->size);
      chord_get_table(crd,(get_table_msg*)msg->data);
      break;
    case MSG_FIND_ALL:
      break;
    case MSG_ENDPOINT_EXIT:
      assert(sizeof(endpoint_exit_msg) == msg->size);
      chord_endpoint_exit(crd,(endpoint_exit_msg*)msg->data);
      break;
    case MSG_KILL:
      done = 1;
      break;
    default:
      fatal("Chord received invalid message: %d",msg->tag);
      break;
    }
    message_free(msg);
  }

  free(crd);
}

void start_chord(node *n, int localid, endpointid initial, endpointid caller, int stabilize_delay)
{
  chord *crd = (chord*)calloc(1,sizeof(chord));
  crd->n = n;
  crd->initial = initial;
  crd->caller = caller;
  crd->stabilize_delay = stabilize_delay;
  node_add_thread2(n,"chord",chord_thread,crd,NULL,localid,262144);
}
