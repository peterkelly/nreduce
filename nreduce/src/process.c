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
 * $Id: gmachine.c 333 2006-08-24 05:00:25Z pmkelly $
 *
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "grammar.tab.h"
#include "gcode.h"
#include "nreduce.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <assert.h>
#include <ctype.h>
#include <stdarg.h>
#include <math.h>
#include <errno.h>

global *pntrhash_lookup(process *proc, pntr p)
{
  int h = hash(&p,sizeof(pntr));
  global *g = proc->pntrhash[h];
  while (g && !pntrequal(g->p,p))
    g = g->pntrnext;
  return g;
}

global *addrhash_lookup(process *proc, gaddr addr)
{
  int h = hash(&addr,sizeof(gaddr));
  global *g = proc->addrhash[h];
  while (g && ((g->addr.pid != addr.pid) || (g->addr.lid != addr.lid)))
    g = g->addrnext;
  return g;
}

void pntrhash_add(process *proc, global *glo)
{
  int h = hash(&glo->p,sizeof(pntr));
  assert(NULL == glo->pntrnext);
  glo->pntrnext = proc->pntrhash[h];
  proc->pntrhash[h] = glo;
}

void addrhash_add(process *proc, global *glo)
{
  int h = hash(&glo->addr,sizeof(gaddr));
  assert(NULL == glo->addrnext);
  glo->addrnext = proc->addrhash[h];
  proc->addrhash[h] = glo;
}

void pntrhash_remove(process *proc, global *glo)
{
  int h = hash(&glo->p,sizeof(pntr));
  global **ptr = &proc->pntrhash[h];

  assert(glo);
  while (*ptr != glo) {
    assert(*ptr);
    ptr = &(*ptr)->pntrnext;
  }
  assert(*ptr);

  *ptr = glo->pntrnext;
  glo->pntrnext = NULL;
}

void addrhash_remove(process *proc, global *glo)
{
  int h = hash(&glo->addr,sizeof(gaddr));
  global **ptr = &proc->addrhash[h];

  assert(glo);
  while (*ptr != glo) {
    assert(*ptr);
    ptr = &(*ptr)->addrnext;
  }
  assert(*ptr);

  *ptr = glo->addrnext;
  glo->addrnext = NULL;
}

global *add_global(process *proc, gaddr addr, pntr p)
{
  global *glo = (global*)calloc(1,sizeof(global));
  glo->addr = addr;
  glo->p = p;
  glo->flags = proc->indistgc ? FLAG_NEW : 0;

  pntrhash_add(proc,glo);
  addrhash_add(proc,glo);
  return glo;
}

pntr global_lookup_existing(process *proc, gaddr addr)
{
  global *glo;
  pntr p;
  if (NULL != (glo = addrhash_lookup(proc,addr)))
    return glo->p;
  make_pntr(p,NULL);
  return p;
}

pntr global_lookup(process *proc, gaddr addr, pntr val)
{
  cell *c;
  global *glo;

  if (NULL != (glo = addrhash_lookup(proc,addr)))
    return glo->p;

  if (!is_nullpntr(val)) {
    glo = add_global(proc,addr,val);
  }
  else {
    pntr p;
    c = alloc_cell(proc);
    make_pntr(p,c);
    glo = add_global(proc,addr,p);

    c->type = TYPE_REMOTEREF;
    make_pntr(c->field1,glo);
  }
  return glo->p;
}

gaddr global_addressof(process *proc, pntr p)
{
  global *glo;
  gaddr addr;

  /* If this object is registered in the global address map, return the address
     that it corresponds to */
  glo = pntrhash_lookup(proc,p);
  if (glo && (0 <= glo->addr.lid))
    return glo->addr;

  /* It's a local object; give it a global address */
  addr.pid = proc->pid;
  addr.lid = proc->nextlid++;
  glo = add_global(proc,addr,p);
  return glo->addr;
}

/* Obtain a global address for the specified local object. Differs from global_addressof()
   in that if p is a remoteref, then make_global() will return the address of the *actual
   reference*, not the thing that it points to */
global *make_global(process *proc, pntr p)
{
  global *glo;
  gaddr addr;

  glo = pntrhash_lookup(proc,p);
  if (glo && (glo->addr.pid == proc->pid))
    return glo;

  addr.pid = proc->pid;
  addr.lid = proc->nextlid++;
  glo = add_global(proc,addr,p);
  return glo;
}

void add_gaddr(list **l, gaddr addr)
{
  if ((-1 != addr.pid) || (-1 != addr.lid)) {
    gaddr *copy = (gaddr*)malloc(sizeof(gaddr));
    memcpy(copy,&addr,sizeof(gaddr));
    list_push(l,copy);
  }
}

/* note: we only remove the first instance */
void remove_gaddr(process *proc, list **l, gaddr addr)
{
  while (*l) {
    gaddr *item = (gaddr*)(*l)->data;
    if ((item->pid == addr.pid) && (item->lid == addr.lid)) {
      list *old = *l;
      *l = (*l)->next;
      free(old->data);
      free(old);
/*       fprintf(proc->output,"removed gaddr %d@%d\n",addr.lid,addr.pid); */
      return;
    }
    else {
      l = &((*l)->next);
    }
  }
  fprintf(proc->output,"gaddr %d@%d not found\n",addr.lid,addr.pid);
  fatal("gaddr not found");
}

#ifdef QUEUE_CHECKS
static int queue_size(frameq *q)
{
  int count = 0;
  frame *f;
  for (f = q->first; f; f = f->qnext)
    count++;
  return count;
}

static int queue_contains_frame(frameq *q, frame *f)
{
  frame *c;
  for (c = q->first; c; c = c->qnext)
    if (c == f)
      return 1;
  return 0;
}

static int check_queue(frameq *q)
{
  frame *f;
  assert((!q->first && !q->last) || (q->first && q->last));
  assert(!q->last || !q->last->qnext);
  assert(!q->first || !q->first->qprev);
  assert(q->size == queue_size(q));
  for (f = q->first; f; f = f->qnext) {
    assert((q->last == f) || (f->qnext->qprev == f));
    assert((q->first == f) || (f->qprev->qnext == f));
  }
  return 1;
}
#endif

void add_frame_queue(frameq *q, frame *f)
{
  #ifdef QUEUE_CHECKS
  assert(check_queue(q));
  assert(!queue_contains_frame(q,f));
  #endif
  assert(!f->qprev && !f->qnext);

  if (q->first) {
    f->qnext = q->first;
    q->first->qprev = f;
    q->first = f;
  }
  else {
    q->first = q->last = f;
  }
  q->size++;
}

void add_frame_queue_end(frameq *q, frame *f)
{
  #ifdef QUEUE_CHECKS
  assert(check_queue(q));
  assert(!queue_contains_frame(q,f));
  #endif
  assert(!f->qprev && !f->qnext);

  if (q->last) {
    f->qprev = q->last;
    q->last->qnext = f;
    q->last = f;
  }
  else {
    q->first = q->last = f;
  }
  q->size++;
}

void remove_frame_queue(frameq *q, frame *f)
{
  #ifdef QUEUE_CHECKS
  assert(check_queue(q));
  assert(queue_contains_frame(q,f));
  #endif
  assert(f->qprev || (q->first == f));
  assert(f->qnext || (q->last == f));

  if (q->first == f)
    q->first = f->qnext;
  if (q->last == f)
    q->last = f->qprev;

  if (f->qnext)
    f->qnext->qprev = f->qprev;
  if (f->qprev)
    f->qprev->qnext = f->qnext;

  f->qnext = NULL;
  f->qprev = NULL;
  q->size--;

  #ifdef QUEUE_CHECKS
  assert(!queue_contains_frame(q,f));
  #endif
}

void transfer_waiters(waitqueue *from, waitqueue *to)
{
  list **ptr = &to->frames;
  while (*ptr)
    ptr = &(*ptr)->next;
  *ptr = from->frames;
  from->frames = NULL;

  ptr = &to->fetchers;
  while (*ptr)
    ptr = &(*ptr)->next;
  *ptr = from->fetchers;
  from->fetchers = NULL;
}

void spark_frame(process *proc, frame *f)
{
  if (STATE_NEW == f->state) {
    add_frame_queue_end(&proc->sparked,f);
    f->state = STATE_SPARKED;
    proc->stats.nsparks++;
  }
}

void unspark_frame(process *proc, frame *f)
{
  assert((STATE_SPARKED == f->state) || (STATE_NEW == f->state));
  if (STATE_SPARKED == f->state)
    remove_frame_queue(&proc->sparked,f);
  f->state = STATE_NEW;
  assert(NULL == f->wq.frames);
  assert(NULL == f->wq.fetchers);
  assert(NULL == f->qnext);
  assert(NULL == f->qprev);
  assert(TYPE_FRAME == celltype(f->c));
  assert(f == (frame*)get_pntr(f->c->field1));
}

void run_frame(process *proc, frame *f)
{
  if (STATE_SPARKED == f->state)
    remove_frame_queue(&proc->sparked,f);

  if ((STATE_SPARKED == f->state) || (STATE_NEW == f->state)) {
    assert((0 == f->address) ||
           (OP_GLOBSTART == bc_get_ops(proc->bcdata)[f->address].opcode));
    add_frame_queue(&proc->runnable,f);
    f->state = STATE_RUNNING;
  }
}

void block_frame(process *proc, frame *f)
{
  assert(STATE_RUNNING == f->state);
  remove_frame_queue(&proc->runnable,f);
  add_frame_queue(&proc->blocked,f);
  f->state = STATE_BLOCKED;
}

void unblock_frame(process *proc, frame *f)
{
  assert(STATE_BLOCKED == f->state);
  remove_frame_queue(&proc->blocked,f);
  add_frame_queue(&proc->runnable,f);
  f->state = STATE_RUNNING;
}

void done_frame(process *proc, frame *f)
{
  assert(STATE_RUNNING == f->state);
  remove_frame_queue(&proc->runnable,f);
  f->state = STATE_DONE;
}
