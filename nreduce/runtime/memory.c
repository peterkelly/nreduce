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

#define MEMORY_C

#include "src/nreduce.h"
#include "compiler/bytecode.h"
#include "compiler/source.h"
#include "runtime/runtime.h"
#include "messages.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <assert.h>
#include <ctype.h>
#include <stdarg.h>
#include <math.h>
#include <unistd.h>
#include <sys/time.h>

unsigned char NULL_PNTR_BITS[8] = { 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0xF0, 0xFF };

const char *cell_types[CELL_COUNT] = {
  "EMPTY",
  "APPLICATION",
  "BUILTIN",
  "CONS",
  "REMOTEREF",
  "IND",
  "SCREF",
  "AREF",
  "HOLE",
  "FRAME",
  "CAP",
  "NIL",
  "NUMBER",
  "SYMBOL",
  "SYSOBJECT",
};

const char *sysobject_types[SYSOBJECT_COUNT] = {
  "FILE",
  "CONNECTION",
  "LISTENER",
  "JAVA",
};

const char *frame_states[5] = {
  "NEW",
  "SPARKED",
  "RUNNING",
  "BLOCKED",
  "DONE",
};

#ifndef INLINE_RESOLVE_PNTR
pntr resolve_pntr(pntr p)
{
  while (TYPE_IND == pntrtype(p))
    p = get_pntr(p)->field1;
  return p;
}
#endif

static void mark(task *tsk, pntr p, short bit);
static void mark_frame(task *tsk, frame *f, short bit);

void mark_global(task *tsk, global *glo, short bit)
{
  assert(glo);
  if (glo->flags & bit) {
    assert(!is_pntr(glo->p) || (get_pntr(glo->p)->flags & bit));
    return;
  }

  glo->flags |= bit;
  mark(tsk,glo->p,bit);

  if ((FLAG_DMB == bit) && (0 <= glo->addr.lid) && (tsk->tid != glo->addr.tid))
    add_pending_mark(tsk,glo->addr);
}

static void mark_frame(task *tsk, frame *f, short bit)
{
  int i;
  int count = f->instr->expcount;
  if (f->c) {
    pntr p;
    make_pntr(p,f->c);
    mark(tsk,p,bit);
  }
  for (i = 0; i < count; i++) {
    if (!is_nullpntr(f->data[i])) {
      f->data[i] = resolve_pntr(f->data[i]);
      mark(tsk,f->data[i],bit);
    }
  }
  /* FIXME: do we need to mark the objects on the wait queue here? The frames should be
     ok, since they should already be in the runnable set. But the fetchers might need
     to be marked */
}

static void mark_cap(task *tsk, cap *c, short bit)
{
  int i;
  for (i = 0; i < c->count; i++)
    if (!is_nullpntr(c->data[i]))
      mark(tsk,c->data[i],bit);
}

static void mark(task *tsk, pntr p, short bit)
{
  cell *c;
  assert(CELL_EMPTY != pntrtype(p));
  #ifdef DISABLE_ARRAYS
  assert(CELL_AREF != pntrtype(p));
  #endif

  if (tsk->markstack) {
    if (is_pntr(p) && !(get_pntr(p)->flags & bit))
      pntrstack_push(tsk->markstack,p);
    return;
  }

  tsk->markstack = pntrstack_new();
  tsk->markstack->limit = -1;
  pntrstack_push(tsk->markstack,p);

  while (0 < tsk->markstack->count) {
    int cont = 0;
    global *target;

    p = tsk->markstack->data[--tsk->markstack->count];

    /* handle CONS and AREF specially - process the "spine" iteratively */
    while ((CELL_AREF == pntrtype(p)) || (CELL_CONS == pntrtype(p))) {
      c = get_pntr(p);
      if (c->flags & bit) {
        cont = 1;
        break;
      }
      c->flags |= bit;
      if ((FLAG_DMB == bit) && (NULL != (target = targethash_lookup(tsk,p))))
        mark_global(tsk,target,bit);

      if (CELL_AREF == pntrtype(p)) {
        carray *arr = (carray*)get_pntr(c->field1);
        int i;
        assert(MAX_ARRAY_SIZE >= arr->size);
        if (sizeof(pntr) == arr->elemsize) {
          for (i = 0; i < arr->size; i++) {
            ((pntr*)arr->elements)[i] = resolve_pntr(((pntr*)arr->elements)[i]);
            mark(tsk,((pntr*)arr->elements)[i],bit);
          }
        }
        arr->tail = resolve_pntr(arr->tail);
        p = arr->tail;
      }
      else {
        c->field1 = resolve_pntr(c->field1);
        c->field2 = resolve_pntr(c->field2);
        mark(tsk,c->field1,bit);
        p = c->field2;
      }
    }

    if (cont)
      continue;

    /* handle other types */
    if (!is_pntr(p))
      continue;

    c = get_pntr(p);
    if (c->flags & bit)
      continue;

    c->flags |= bit;
    if ((FLAG_DMB == bit) && (NULL != (target = targethash_lookup(tsk,p))))
      mark_global(tsk,target,bit);
    switch (pntrtype(p)) {
    case CELL_IND:
      mark(tsk,c->field1,bit);
      break;
    case CELL_APPLICATION:
      c->field1 = resolve_pntr(c->field1);
      c->field2 = resolve_pntr(c->field2);
      mark(tsk,c->field1,bit);
      mark(tsk,c->field2,bit);
      break;
    case CELL_FRAME:
      mark_frame(tsk,(frame*)get_pntr(c->field1),bit);
      break;
    case CELL_CAP:
      mark_cap(tsk,(cap*)get_pntr(c->field1),bit);
      break;
    case CELL_REMOTEREF: {
      global *glo = (global*)get_pntr(c->field1);
      mark_global(tsk,glo,bit);
      break;
    }
    case CELL_SYSOBJECT: {
      sysobject *so = (sysobject*)get_pntr(c->field1);
      if (!is_nullpntr(so->listenerso))
        mark(tsk,so->listenerso,bit);
      if (so->newso)
        mark(tsk,so->newso->p,bit);
      break;
    }
    case CELL_BUILTIN:
    case CELL_SCREF:
    case CELL_NIL:
    case CELL_NUMBER:
    case CELL_HOLE:
    case CELL_SYMBOL:
      break;
    default:
      fatal("Invalid pntr type %d",pntrtype(p));
      break;
    }
  }

  assert(tsk->markstack);
  pntrstack_free(tsk->markstack);
  tsk->markstack = NULL;
}

cell *alloc_cell(task *tsk)
{
  cell *v;
  assert(tsk);
  if ((cell*)1 == tsk->freeptr) { /* 64 bit pntrs use 1 in second byte for null */
    block *bl = (block*)calloc(1,sizeof(block));
    int i;
    bl->next = tsk->blocks;
    tsk->blocks = bl;
    for (i = 0; i < BLOCK_SIZE-1; i++)
      make_pntr(bl->values[i].field1,&bl->values[i+1]);
    bl->values[i].field1 = NULL_PNTR;

    tsk->freeptr = &bl->values[0];
  }
  v = tsk->freeptr;
  v->flags = tsk->newcellflags;
  tsk->freeptr = (cell*)get_pntr(tsk->freeptr->field1);
  tsk->alloc_bytes += sizeof(cell);
  #ifdef PROFILING
  tsk->stats.cell_allocs++;
  #endif
  if ((tsk->alloc_bytes >= COLLECT_THRESHOLD) && tsk->endpt)
    endpoint_interrupt(tsk->endpt);
  return v;
}

sysobject *new_sysobject(task *tsk, int type)
{
  sysobject *so = (sysobject*)calloc(1,sizeof(sysobject));
  so->type = type;
  so->ownertid = tsk->tid;
  so->c = alloc_cell(tsk);
  make_pntr(so->p,so->c);
  so->c->type = CELL_SYSOBJECT;
  make_pntr(so->c->field1,so);
  return so;
}

sysobject *find_sysobject(task *tsk, const socketid *sockid)
{
  block *bl;
  int i;
  for (bl = tsk->blocks; bl; bl = bl->next) {
    for (i = 0; i < BLOCK_SIZE; i++) {
      if (CELL_SYSOBJECT == bl->values[i].type) {
        cell *c = &bl->values[i];
        sysobject *so = (sysobject*)get_pntr(c->field1);
        if (socketid_equals(&so->sockid,sockid))
          return so;
      }
    }
  }
  return NULL;
}

void free_global(task *tsk, global *glo)
{
  list_free(glo->wq.fetchers,free);

  if (CELL_REMOTEREF == pntrtype(glo->p)) {
    cell *refcell = get_pntr(glo->p);
    global *target = (global*)get_pntr(refcell->field1);
    if (target == glo) {
      assert(tsk->done || !(refcell->flags & FLAG_MARKED));
      assert(tsk->done || !(refcell->flags & FLAG_DMB));
    }
  }

  free(glo);
}

static void free_sysobject(task *tsk, sysobject *so)
{
  if (so->ownertid == tsk->tid) {
    switch (so->type) {
    case SYSOBJECT_FILE:
      close(so->fd);
      break;
    case SYSOBJECT_CONNECTION: {
      if (!socketid_isnull(&so->sockid))
        send_delete_connection(tsk->endpt,so->sockid);
      if (!tsk->done) {
        assert(0 == so->frameids[CONNECT_FRAMEADDR]);
        assert(0 == so->frameids[READ_FRAMEADDR]);
        assert(0 == so->frameids[WRITE_FRAMEADDR]);
        assert(0 == so->frameids[LISTEN_FRAMEADDR]);
        assert(0 == so->frameids[ACCEPT_FRAMEADDR]);
      }
      break;
    }
    case SYSOBJECT_LISTENER: {
      if (!socketid_isnull(&so->sockid))
        send_delete_listener(tsk->endpt,so->sockid);
      break;
    }
    case SYSOBJECT_JAVA: {
      /* This could be improved by releasing all garbage java objects in a single message */
      array *arr = array_new(1,0);
      array_printf(arr,"release @%d",so->jid.jid);
      send_jcmd(tsk->endpt,1,1,arr->data,arr->nbytes);
      array_free(arr);
      break;
    }
    default:
      fatal("Invalid sysobject type %d",so->type);
      break;
    }
  }
  free(so->hostname);
  free(so->buf);
  free(so);
}

void free_cell_fields(task *tsk, cell *v)
{
  assert(CELL_EMPTY != v->type);
  switch (celltype(v)) {
  case CELL_AREF: {
    free(get_pntr(v->field1));
    break;
  }
  case CELL_FRAME: {
    frame *f = (frame*)get_pntr(v->field1);
    f->c = NULL;
    assert(tsk->done || (STATE_DONE == f->state) || (STATE_NEW == f->state));
    assert(tsk->done || (NULL == f->retp));
    frame_free(tsk,f);
    break;
  }
  case CELL_CAP: {
    cap *cp = (cap*)get_pntr(v->field1);
    cap_dealloc(cp);
    break;
  }
  case CELL_REMOTEREF:
    break;
  case CELL_SYSOBJECT: {
    sysobject *so = (sysobject*)get_pntr(v->field1);
    free_sysobject(tsk,so);
    break;
  }
  case CELL_SYMBOL:
    free((char*)get_pntr(v->field1));
    break;
  }
}

void clear_marks(task *tsk, short bit)
{
  block *bl;
  int i;
  global *glo;

  for (bl = tsk->blocks; bl; bl = bl->next)
    for (i = 0; i < BLOCK_SIZE; i++)
      bl->values[i].flags &= ~bit;

  for (glo = tsk->globals.first; glo; glo = glo->next)
    glo->flags &= ~bit;
}

void mark_roots(task *tsk, short bit)
{
  int pid;
  int i;
/*   frame *f; */
  list *l;
  int h;
  global *glo;
  frameblock *fb;

  for (i = 0; i < tsk->nstrings; i++)
    mark(tsk,tsk->strings[i],bit);

  mark(tsk,tsk->argsp,bit);

  if (tsk->streamstack) {
    for (i = 0; i < tsk->streamstack->count; i++)
      mark(tsk,tsk->streamstack->data[i],bit);
  }

  if (tsk->tracing)
    mark(tsk,tsk->trace_root,bit);

  if (tsk->out_so)
    mark(tsk,tsk->out_so->p,bit);

  for (fb = tsk->frameblocks; fb; fb = fb->next) {
    for (i = 0; i < tsk->framesperblock; i++) {
      frame *f = ((frame*)&fb->mem[i*tsk->framesize]);
      if ((STATE_SPARKED == f->state) || (STATE_RUNNING == f->state) || (STATE_BLOCKED == f->state))
        mark_frame(tsk,f,bit);
    }
  }

  /* mark any in-flight gaddrs that refer to objects in this task */
  for (l = tsk->inflight; l; l = l->next) {
    gaddr *addr = (gaddr*)l->data;
    if ((0 <= addr->lid) && (addr->tid == tsk->tid)) {
      glo = addrhash_lookup(tsk,*addr);
      assert(glo);
      mark_global(tsk,glo,bit);
    }
  }

  for (pid = 0; pid < tsk->groupsize; pid++) {
    int count = array_count(tsk->inflight_addrs[pid]);
    for (i = 0; i < count; i++) {
      gaddr addr = array_item(tsk->inflight_addrs[pid],i,gaddr);
      if ((0 <= addr.lid) && (addr.tid == tsk->tid)) {
        glo = addrhash_lookup(tsk,addr);
        assert(glo);
        mark_global(tsk,glo,bit);
      }
    }
  }

  /* mark any remote references that are currently being fetched (and any frames waiting
     on them */
  for (h = 0; h < GLOBAL_HASH_SIZE; h++)
    for (glo = tsk->targethash[h]; glo; glo = glo->targetnext)
      if (glo->fetching)
        mark_global(tsk,glo,bit);
}

static int global_needed(global *glo)
{
  if ((glo->flags & FLAG_MARKED) || (glo->flags & FLAG_DMB)) {
    return 1;
  }
  else if (is_pntr(glo->p)) {
    cell *c = checkcell(get_pntr(glo->p));
    if ((c->flags & FLAG_MARKED) || (c->flags & FLAG_DMB))
      return 1;
  }
  return 0;
}

static void sweep_globals(task *tsk, int all)
{
  global *glo;
  global *next;

  for (glo = tsk->globals.first; glo; glo = next) {
    next = glo->next;
    if (all || !global_needed(glo)) {
      /* Remove from hash tables */
      addrhash_remove(tsk,glo);
      targethash_remove(tsk,glo);
      physhash_remove(tsk,glo);
      /* Remove from linked list of all globals */
      llist_remove(&tsk->globals,glo);
      /* Free memory */
      free_global(tsk,glo);
    }
  }
}

static void sweep_cells(task *tsk, int all)
{
  block *bl;
  int i;
  if (all) {
    for (bl = tsk->blocks; bl; bl = bl->next)
      for (i = 0; i < BLOCK_SIZE; i++)
        if (CELL_EMPTY != bl->values[i].type)
          free_cell_fields(tsk,&bl->values[i]);
  }
  else {
    for (bl = tsk->blocks; bl; bl = bl->next) {
      for (i = 0; i < BLOCK_SIZE; i++) {
        if (!(bl->values[i].flags & FLAG_MARKED) &&
            !(bl->values[i].flags & FLAG_DMB) &&
            !(bl->values[i].flags & FLAG_PINNED) &&
            (CELL_EMPTY != bl->values[i].type)) {
          free_cell_fields(tsk,&bl->values[i]);
          bl->values[i].type = CELL_EMPTY;

          /* comment out these two lines to avoid reallocating cells, to check invalid accesses */
          make_pntr(bl->values[i].field1,tsk->freeptr);
          tsk->freeptr = &bl->values[i];
        }
      }
    }
  }
}

void sweep(task *tsk, int all)
{
  global *glo;
  block *bl;
  int i;

  /* Treat all new globals and cells that were created during this garbage collection cycle
     as rools that also need to be marked */
  if (tsk->indistgc) {
    for (glo = tsk->globals.first; glo; glo = glo->next)
      if (glo->flags & FLAG_NEW)
        mark_global(tsk,glo,FLAG_MARKED);

    for (bl = tsk->blocks; bl; bl = bl->next) {
      for (i = 0; i < BLOCK_SIZE; i++) {
        if ((CELL_EMPTY != bl->values[i].type) && (bl->values[i].flags & FLAG_NEW)) {
          pntr p;
          make_pntr(p,&bl->values[i]);
          mark(tsk,p,FLAG_MARKED);
        }
      }
    }
  }

  sweep_globals(tsk,all);
  sweep_cells(tsk,all);
}

void local_collect(task *tsk)
{
  int h;
  global *glo;
  struct timeval start;
  struct timeval end;
  int ms;

  gettimeofday(&start,NULL);

  #ifdef PROFILING
  tsk->stats.gcs++;
  #endif

  tsk->alloc_bytes = 0;

  /* clear */
  clear_marks(tsk,FLAG_MARKED);

  /* mark */
  mark_roots(tsk,FLAG_MARKED);

  /* treat any objects referenced from other tasks as roots */
  for (h = 0; h < GLOBAL_HASH_SIZE; h++) {
    for (glo = tsk->physhash[h]; glo; glo = glo->physnext) {
      assert(glo->addr.tid == tsk->tid);
      assert(glo->addr.lid >= 0);
      mark_global(tsk,glo,FLAG_MARKED);
    }
  }

  /* sweep */
  sweep(tsk,0);

  gettimeofday(&end,NULL);
  ms = timeval_diffms(start,end);
  node_log(tsk->n,LOG_INFO,"Garbage collection took %dms",ms);
  tsk->gcms += ms;
}

void memusage(task *tsk, int *cells, int *bytes, int *alloc, int *connections, int *listeners)
{
  block *bl;
  int i;

  *cells = 0;
  *bytes = 0;
  *connections = 0;
  *listeners = 0;
  *alloc = 0;

  node_stats(tsk->n,connections,listeners);

  for (bl = tsk->blocks; bl; bl = bl->next) {
    for (i = 0; i < BLOCK_SIZE; i++) {
      cell *c = &bl->values[i];
      if (CELL_AREF == c->type) {
        carray *arr = (carray*)get_pntr(c->field1);
        (*bytes) += arr->size*arr->elemsize;
        (*alloc) += arr->alloc*arr->elemsize;
      }
      if (CELL_EMPTY != c->type) {
        (*bytes) += sizeof(cell);
        (*cells)++;
      }
    }
    (*alloc) += sizeof(block);
  }
}

frame *frame_new(task *tsk, int addalloc) /* Can be called from native code */
{
  frame *f;
  if (NULL == tsk->freeframe) {
    frameblock *fb = (frameblock*)calloc(1,sizeof(frameblock));
    int i;
    fb->next = tsk->frameblocks;
    tsk->frameblocks = fb;

    for (i = 0; i < tsk->framesperblock-1; i++)
      ((frame*)&fb->mem[i*tsk->framesize])->freelnk = ((frame*)&fb->mem[(i+1)*tsk->framesize]);
    ((frame*)&fb->mem[i*tsk->framesize])->freelnk = NULL;

    tsk->freeframe = (frame*)fb->mem;
  }

  if (addalloc)
    tsk->alloc_bytes += tsk->framesize;

  if ((tsk->alloc_bytes >= COLLECT_THRESHOLD) && tsk->endpt)
    endpoint_interrupt(tsk->endpt);

  f = tsk->freeframe;
  tsk->freeframe = f->freelnk;

  f->c = 0; /* should be set by caller */
  f->instr = 0; /* should be set by caller */

  f->state = 0;
  f->resume = 0;
  f->freelnk = 0;
  f->retp = NULL;

  assert(NULL == f->wq.frames);
  assert(NULL == f->wq.fetchers);
  assert(NULL == f->waitlnk);
  assert(NULL == f->rnext);
  assert(NULL == f->waitglo);

  #ifdef PROFILING
  tsk->stats.frame_allocs++;
  #endif

  return f;
}

cap *cap_alloc(task *tsk, int arity, int address, int fno)
{
  cap *c = (cap*)calloc(1,sizeof(cap));
  c->arity = arity;
  c->address = address;
  c->fno = fno;
  assert(0 < c->arity); /* MKCAP should not be called for CAFs */

  #ifdef PROFILING
  tsk->stats.cap_allocs++;
  #endif

  return c;
}

void cap_dealloc(cap *c)
{
  if (c->data)
    free(c->data);
  free(c);
}

pntrstack *pntrstack_new(void)
{
  pntrstack *s = (pntrstack*)calloc(1,sizeof(pntrstack));
  s->alloc = 1;
  s->count = 0;
  s->data = (pntr*)malloc(sizeof(pntr));
  s->limit = STACK_LIMIT;
  return s;
}

void pntrstack_push(pntrstack *s, pntr p)
{
  if (s->count == s->alloc) {
    if ((0 <= s->limit) && (s->count >= s->limit)) {
      fprintf(stderr,"Out of stack space\n");
      exit(1);
    }
    pntrstack_grow(&s->alloc,&s->data,s->alloc*2);
  }
  s->data[s->count++] = p;
}

pntr pntrstack_at(pntrstack *s, int pos)
{
  assert(0 <= pos);
  assert(pos < s->count);
  return resolve_pntr(s->data[pos]);
}

pntr pntrstack_pop(pntrstack *s)
{
  assert(0 < s->count);
  return resolve_pntr(s->data[--s->count]);
}

pntr pntrstack_top(pntrstack *s)
{
  assert(0 < s->count);
  return resolve_pntr(s->data[s->count]);
}

void pntrstack_free(pntrstack *s)
{
  free(s->data);
  free(s);
}

void pntrstack_grow(int *alloc, pntr **data, int size)
{
  if (*alloc < size) {
    *alloc = size;
    *data = (pntr*)realloc(*data,(*alloc)*sizeof(pntr));
  }
}
