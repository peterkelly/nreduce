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

#ifdef DEBUG_CHORD
#define CHORD_DEBUG(...) chord_debug(crd->self,__func__,__VA_ARGS__)

static void chord_debug(chordnode cn, const char *function, const char *format, ...)
{
  array *arr = array_new(1,0);
  char end = '\0';
  endpointid_str str;
  char idstr[100];
  va_list ap;

  if (!strncmp(function,"chord_",strlen("chord_")))
    function += strlen("chord_");
  else
    function = "";

  print_endpointid(str,cn.epid);
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

  lock_node(n);
  endpoint_link(endpt,stb->chord_epid);
  unlock_node(n);

  node_send(n,endpt->epid.localid,stb->chord_epid,MSG_STABILIZE,NULL,0);

  while (!done) {
    int delay = stb->stabilize_delay/2 + rand()%stb->stabilize_delay;
    msg = endpoint_next_message(endpt,delay);
    if (NULL == msg) {
      node_send(n,endpt->epid.localid,stb->chord_epid,MSG_STABILIZE,NULL,0);
    }
    else {
      switch (msg->hdr.tag) {
      case MSG_ENDPOINT_EXIT: {
        assert(sizeof(endpointid) == msg->hdr.size);
        assert(endpointid_equals((endpointid*)msg->data,&stb->chord_epid));
        done = 1;
        break;
      }
      case MSG_KILL:
        done = 1;
        break;
      default:
        fatal("Stabilizer received invalid message: %d",msg->hdr.tag);
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
  node_add_thread(n,0,STABILIZER_ENDPOINT,0,stabilizer_thread,stb,NULL);
}

typedef struct {
  node *n;
  pthread_t thread;
  endpointid caller;
  endpointid initial;
  chordnode self;
  chordnode predecessor;
  chordnode fingers[MBITS+1];
  chordnode successors[NSUCCESSORS+1];
  endpoint *endpt;
  int stabilize_delay;
  int joined;
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

static int have_link(endpoint *endpt, endpointid epid)
{
  list *l;
  for (l = endpt->outlinks; l; l = l->next) {
    endpointid *linkid = (endpointid*)l->data;
    if (endpointid_equals(linkid,&epid))
      return 1;
  }
  return 0;
}

static int link_ok_to_have(chord *crd, endpointid epid)
{
  int i;
  if (!chordnode_isnull(crd->predecessor) &&
      endpointid_equals(&crd->predecessor.epid,&epid))
    return 1;
  for (i = 1; i < MBITS; i++) {
    if (!chordnode_isnull(crd->fingers[i]) &&
        endpointid_equals(&crd->fingers[i].epid,&epid))
      return 1;
  }
  return 0;
}

static int check_links(chord *crd)
{
  int i;
  list *l;
  int r = 1;
  lock_node(crd->n);
  if (!chordnode_isnull(crd->predecessor) && !have_link(crd->endpt,crd->predecessor.epid))
    r = 0;
  for (i = 1; i < MBITS; i++) {
    if (!chordnode_isnull(crd->fingers[i]) && !have_link(crd->endpt,crd->fingers[i].epid))
      r = 0;
  }
  for (l = crd->endpt->outlinks; l; l = l->next) {
    endpointid *linkid = (endpointid*)l->data;
    if (!link_ok_to_have(crd,*linkid))
      r = 0;
  }
  unlock_node(crd->n);
  return r;
}

static int count_instances(chord *crd, endpointid epid)
{
  int i;
  int count = 0;
  if (!chordnode_isnull(crd->predecessor) &&
      endpointid_equals(&crd->predecessor.epid,&epid))
    count++;
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

  lock_node(crd->n);
  if (1 == oldinst)
    endpoint_unlink(crd->endpt,ptr->epid);
  *ptr = cn;
  if ((0 == newinst) && !chordnode_isnull(cn))
    endpoint_link(crd->endpt,cn.epid);

  unlock_node(crd->n);
}

static void set_predecessor(chord *crd, chordnode newpredecessor)
{
  set_noderef(crd,&crd->predecessor,newpredecessor);
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

  set_predecessor(crd,null_node);
  for (i = 1; i <= MBITS; i++)
    set_finger(crd,i,null_node);
  memset(&crd->successors,0,sizeof(crd->successors));
}

static void chord_notify_joined(chord *crd)
{
  joined_msg jm;
  jm.cn = crd->self;
  node_send(crd->n,crd->endpt->epid.localid,crd->caller,MSG_JOINED,&jm,sizeof(jm));
  crd->joined = 1;
}

static void chord_join(chord *crd)
{
  node *n = crd->n;

  assert(sizeof(chordnode) == sizeof(crd->predecessor));
  assert((MBITS+1)*sizeof(chordnode) == sizeof(crd->fingers));
  assert((NSUCCESSORS+1)*sizeof(chordnode) == sizeof(crd->successors));

  chord_clear_tables(crd);

  if (0 == crd->initial.localid) {
    CHORD_DEBUG("no existing node");
    set_successor(crd,crd->self);
    chord_notify_joined(crd);
  }
  else {
    find_successor_msg fsm;
    CHORD_DEBUG("using ndash ("EPID_FORMAT")",EPID_ARGS(crd->initial));
    fsm.id = crd->self.id;
    fsm.sender = crd->self.epid;
    fsm.hops = 0;
    fsm.payload = 1;
    node_send(n,crd->self.epid.localid,crd->initial,MSG_FIND_SUCCESSOR,&fsm,sizeof(fsm));
  }
}

static void chord_find_successor(chord *crd, find_successor_msg *m)
{
  chordnode successor = crd->fingers[1];

  assert(crd->joined);
  assert(!chordnode_isnull(successor));

  if (inrange_open_closed(m->id,crd->self.id,successor.id)) {
    got_successor_msg gsm;
    gsm.successor = successor;
    gsm.hops = m->hops;
    gsm.payload = m->payload;
    node_send(crd->n,crd->self.epid.localid,m->sender,MSG_GOT_SUCCESSOR,&gsm,sizeof(gsm));
  }
  else {
    chordnode closest = closest_preceding_finger(crd,m->id);
    m->hops++;
    node_send(crd->n,crd->self.epid.localid,closest.epid,MSG_FIND_SUCCESSOR,m,sizeof(*m));
  }
}

static void chord_notify(chord *crd, notify_msg *m)
{
  chordnode successor = crd->fingers[1];
  notify_reply_msg nrm;
  int i;

  nrm.old_sucprec = crd->predecessor;

  if (successor.id == crd->self.id) {
    assert(endpointid_equals(&successor.epid,&crd->self.epid));
    set_successor(crd,m->ndash);
  }

  if (!chordnode_isnull(crd->predecessor) && (crd->predecessor.id == m->ndash.id) &&
      !endpointid_equals(&crd->predecessor.epid,&m->ndash.epid)) {
    CHORD_DEBUG("duplicate join attempt: #%d ("EPID_FORMAT" and "EPID_FORMAT")",
                crd->predecessor.id,EPID_ARGS(crd->predecessor.epid),EPID_ARGS(m->ndash.epid));
  }

  if (chordnode_isnull(crd->predecessor)) {
    CHORD_DEBUG("predecessor -> #%d",m->ndash.id);
    set_predecessor(crd,m->ndash);
  }
  else if (inrange_open(m->ndash.id,crd->predecessor.id,crd->self.id)) {
    CHORD_DEBUG("predecessor #%d -> #%d",crd->predecessor.id,m->ndash.id);
    node_send(crd->n,crd->self.epid.localid,crd->predecessor.epid,MSG_STABILIZE,NULL,0);
    set_predecessor(crd,m->ndash);
  }
  nrm.sucprec = crd->predecessor;

  nrm.successors[1] = crd->fingers[1];
  for (i = 2; i <= NSUCCESSORS; i++)
    nrm.successors[i] = crd->successors[i-1];

  node_send(crd->n,crd->self.epid.localid,m->ndash.epid,MSG_NOTIFY_REPLY,&nrm,sizeof(nrm));
}

static void duplicate_detected(chord *crd, chordnode existing)
{
  chordid oldid = crd->self.id;
  chordid newid = rand()%KEYSPACE;
  id_changed_msg idcm;

  CHORD_DEBUG("duplicate; existing #%d ("EPID_FORMAT"); using new id #%d",
              existing.id,EPID_ARGS(existing.epid),newid);
  assert(!crd->joined);

  crd->self.id = newid;

  idcm.cn = crd->self;
  idcm.oldid = oldid;
  node_send(crd->n,crd->endpt->epid.localid,crd->caller,MSG_ID_CHANGED,&idcm,sizeof(idcm));

  chord_join(crd);
}

static void chord_notify_reply(chord *crd, notify_reply_msg *m)
{
  if (!chordnode_isnull(m->sucprec)) {
    chordnode successor = crd->fingers[1];

    if (m->sucprec.id == crd->self.id) {
      if (!endpointid_equals(&m->sucprec.epid,&crd->self.epid)) {
        duplicate_detected(crd,m->sucprec);
        return;
      }
      else if (!crd->joined) {
        /* set the new predecessor */
        assert(chordnode_isnull(crd->predecessor));
        set_predecessor(crd,m->old_sucprec);

        CHORD_DEBUG("join successful; successor %d, sucprec #%d ("EPID_FORMAT")",
                    successor.id,m->sucprec.id,EPID_ARGS(m->sucprec.epid));
        CHORD_DEBUG("predecessor -> #%d",crd->predecessor);
        chord_notify_joined(crd);
      }
    }

    if (inrange_open(m->sucprec.id,crd->self.id,successor.id)) {
      notify_msg nm;
      CHORD_DEBUG("successor %d -> %d E (%d,%d)",
                  successor.id,m->sucprec.id,crd->self.id,successor.id);
      set_successor(crd,m->sucprec);
      nm.ndash = crd->self;
      node_send(crd->n,crd->self.epid.localid,m->sucprec.epid,MSG_NOTIFY,&nm,sizeof(nm));
    }
  }
  memcpy(&crd->successors,m->successors,sizeof(m->successors));
}

static void chord_got_successor(chord *crd, got_successor_msg *m)
{
  if (1 == m->payload) {
    CHORD_DEBUG("setting successor = #%d ("EPID_FORMAT")",
                m->successor.id,EPID_ARGS(m->successor.epid));
  }

  assert(!chordnode_isnull(m->successor));

  if (m->successor.id == crd->self.id) {
    if (!endpointid_equals(&m->successor.epid,&crd->self.epid))
      duplicate_detected(crd,m->successor);
    else
      assert(1 < m->payload);
  }

  if (1 == m->payload)
    node_send(crd->n,crd->endpt->epid.localid,crd->self.epid,MSG_STABILIZE,NULL,0);

  set_finger(crd,m->payload,m->successor);
}

static void chord_stabilize(chord *crd)
{
  chordnode successor = crd->fingers[1];
  notify_msg nm;

  if (chordnode_isnull(successor))
    return; /* still joining */

  /* Send a NOTIFY message to our current successor */
  nm.ndash = crd->self;
  node_send(crd->n,crd->self.epid.localid,successor.epid,MSG_NOTIFY,&nm,sizeof(nm));

  /* Update a random finger */
  if (crd->joined) {
    int k;
    find_successor_msg fsm;
    k = 2+rand()%(MBITS-2);
    fsm.id = crd->self.id+(int)pow(2,k-1);
    fsm.sender = crd->self.epid;
    fsm.hops = 0;
    fsm.payload = k;
    node_send(crd->n,crd->self.epid.localid,crd->self.epid,MSG_FIND_SUCCESSOR,&fsm,sizeof(fsm));
  }
}

static void chord_get_table(chord *crd, get_table_msg *m)
{
  reply_table_msg rtm;

  rtm.predecessor = crd->predecessor;

  memcpy(&rtm.fingers,&crd->fingers,sizeof(crd->fingers));
  memcpy(&rtm.successors,&crd->successors,sizeof(crd->successors));
  node_send(crd->n,crd->self.epid.localid,m->sender,MSG_REPLY_TABLE,&rtm,sizeof(rtm));
}

static void chord_endpoint_exit(chord *crd, endpoint_exit_msg *m)
{
  int i;
  chordnode null_node;
  memset(&null_node,0,sizeof(chordnode));
  CHORD_DEBUG("received ENDPOINT_EXIT for "
              EPID_FORMAT,EPID_ARGS(m->epid));

  if (endpointid_equals(&m->epid,&crd->predecessor.epid))
    set_predecessor(crd,null_node);

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
      abort();
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
  chord_started_msg csm;
  assert(crd->n);
  assert(NULL == crd->endpt);
  crd->endpt = endpt;

  crd->self.id = rand() % KEYSPACE;
  crd->self.epid = endpt->epid;

  csm.cn = crd->self;
  node_send(n,crd->self.epid.localid,crd->caller,MSG_CHORD_STARTED,&csm,sizeof(csm));

  start_stabilizer(n,crd->self.epid,crd->stabilize_delay);

  chord_join(crd);

  while (!done) {
    msg = endpoint_next_message(endpt,-1);
    switch (msg->hdr.tag) {
    case MSG_FIND_SUCCESSOR:
      assert(sizeof(find_successor_msg) == msg->hdr.size);
      chord_find_successor(crd,(find_successor_msg*)msg->data);
      break;
    case MSG_NOTIFY:
      assert(sizeof(notify_msg) == msg->hdr.size);
      chord_notify(crd,(notify_msg*)msg->data);
      break;
    case MSG_NOTIFY_REPLY:
      assert(sizeof(notify_reply_msg) == msg->hdr.size);
      chord_notify_reply(crd,(notify_reply_msg*)msg->data);
      break;
    case MSG_GOT_SUCCESSOR:
      assert(sizeof(got_successor_msg) == msg->hdr.size);
      chord_got_successor(crd,(got_successor_msg*)msg->data);
      break;
    case MSG_STABILIZE:
      assert(check_links(crd));
      chord_stabilize(crd);
      break;
    case MSG_GET_TABLE:
      assert(sizeof(get_table_msg) == msg->hdr.size);
      chord_get_table(crd,(get_table_msg*)msg->data);
      break;
    case MSG_FIND_ALL:
      break;
    case MSG_ENDPOINT_EXIT:
      assert(sizeof(endpoint_exit_msg) == msg->hdr.size);
      chord_endpoint_exit(crd,(endpoint_exit_msg*)msg->data);
      break;
    case MSG_KILL:
      done = 1;
      break;
    default:
      fatal("Chord received invalid message: %d",msg->hdr.tag);
      break;
    }
    message_free(msg);
  }

  free(crd);
}

void start_chord(node *n, endpointid initial, endpointid caller, int stabilize_delay)
{
  chord *crd = (chord*)calloc(1,sizeof(chord));
  crd->n = n;
  crd->initial = initial;
  crd->caller = caller;
  crd->stabilize_delay = stabilize_delay;
  node_add_thread(n,0,CHORD_ENDPOINT,32768,chord_thread,crd,NULL);
}
