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

#define MEMORY_C

#include "src/nreduce.h"
#include "compiler/bytecode.h"
#include "compiler/source.h"
#include "runtime/runtime.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <assert.h>
#include <ctype.h>
#include <stdarg.h>
#include <math.h>
#include <pthread.h>

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
  "ARRAY",
};

const char *msg_names[MSG_COUNT] = {
  "ISTATS",
  "ALLSTATS",
  "DUMP_INFO",
  "DONE",
  "PAUSE",
  "RESUME",
  "FISH",
  "FETCH",
  "TRANSFER",
  "ACK",
  "MARKROOTS",
  "MARKENTRY",
  "SWEEP",
  "SWEEPACK",
  "UPDATE",
  "RESPOND",
  "SCHEDULE",
  "UPDATEREF",
  "TEST",
  "STARTDISTGC",
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

static void mark(process *proc, pntr p, short bit);
static void mark_frame(process *proc, frame *f, short bit);

void mark_global(process *proc, global *glo, short bit)
{
  if (glo->flags & bit) {
    assert(!is_pntr(glo->p) || (get_pntr(glo->p)->flags & bit));
    return;
  }

  glo->flags |= bit;
  mark(proc,glo->p,bit);

  if ((FLAG_DMB == bit) && (0 <= glo->addr.lid) && (proc->pid != glo->addr.pid))
    add_pending_mark(proc,glo->addr);
}

static void mark_frame(process *proc, frame *f, short bit)
{
  int i;
  if (f->c) {
    pntr p;
    make_pntr(p,f->c);
    mark(proc,p,bit);
  }
  for (i = 0; i < f->count; i++) {
    if (!is_nullpntr(f->data[i])) {
      f->data[i] = resolve_pntr(f->data[i]);
      mark(proc,f->data[i],bit);
    }
  }
}

static void mark_cap(process *proc, cap *c, short bit)
{
  int i;
  for (i = 0; i < c->count; i++)
    if (!is_nullpntr(c->data[i]))
      mark(proc,c->data[i],bit);
}

static void mark(process *proc, pntr p, short bit)
{
  cell *c;
  assert(CELL_EMPTY != pntrtype(p));

  /* handle CONS and AREF specially - process the "spine" iteratively */
  while ((CELL_AREF == pntrtype(p)) || (CELL_CONS == pntrtype(p))) {
    c = get_pntr(p);
    if (c->flags & bit)
      return;
    c->flags |= bit;

    if (CELL_AREF == pntrtype(p)) {
      carray *arr = (carray*)get_pntr(c->field1);
      int i;
      assert(MAX_ARRAY_SIZE >= arr->size);
      if (sizeof(pntr) == arr->elemsize) {
        for (i = 0; i < arr->size; i++) {
          ((pntr*)arr->elements)[i] = resolve_pntr(((pntr*)arr->elements)[i]);
          mark(proc,((pntr*)arr->elements)[i],bit);
        }
      }
      arr->tail = resolve_pntr(arr->tail);
      p = arr->tail;
    }
    else {
      c->field1 = resolve_pntr(c->field1);
      c->field2 = resolve_pntr(c->field2);
      mark(proc,c->field1,bit);
      p = c->field2;
    }
  }

  /* handle other types */
  if (!is_pntr(p))
    return;

  c = get_pntr(p);
  if (c->flags & bit)
    return;

  c->flags |= bit;
  switch (pntrtype(p)) {
  case CELL_IND:
    mark(proc,c->field1,bit);
    break;
  case CELL_APPLICATION:
    c->field1 = resolve_pntr(c->field1);
    c->field2 = resolve_pntr(c->field2);
    mark(proc,c->field1,bit);
    mark(proc,c->field2,bit);
    break;
  case CELL_FRAME:
    mark_frame(proc,(frame*)get_pntr(c->field1),bit);
    break;
  case CELL_CAP:
    mark_cap(proc,(cap*)get_pntr(c->field1),bit);
    break;
  case CELL_REMOTEREF: {
    global *glo = (global*)get_pntr(c->field1);
    mark_global(proc,glo,bit);
    break;
  }
  case CELL_BUILTIN:
  case CELL_SCREF:
  case CELL_NIL:
  case CELL_NUMBER:
  case CELL_HOLE:
    break;
  default:
    abort();
    break;
  }
}

cell *alloc_cell(process *proc)
{
  cell *v;
  assert(proc);
  if (NULL == proc->freeptr) {
    block *bl = (block*)calloc(1,sizeof(block));
    int i;
    bl->next = proc->blocks;
    proc->blocks = bl;
    for (i = 0; i < BLOCK_SIZE-1; i++)
      make_pntr(bl->values[i].field1,&bl->values[i+1]);
    make_pntr(bl->values[i].field1,NULL);

    proc->freeptr = &bl->values[0];
  }
  v = proc->freeptr;
  v->flags = proc->indistgc ? FLAG_NEW : 0;
  proc->freeptr = (cell*)get_pntr(proc->freeptr->field1);
  proc->stats.nallocs++;
  proc->stats.totalallocs++;
  return v;
}

static void free_global(process *proc, global *glo)
{
  if (CELL_REMOTEREF == pntrtype(glo->p)) {
    cell *refcell = get_pntr(glo->p);
    global *target = (global*)get_pntr(refcell->field1);
    if (target == glo) {
      assert(!(refcell->flags & FLAG_MARKED));
      assert(!(refcell->flags & FLAG_DMB));
    }
  }

  free(glo);
}

void free_cell_fields(process *proc, cell *v)
{
  assert(CELL_EMPTY != v->type);
  switch (celltype(v)) {
  case CELL_AREF: {
    carray *arr = (carray*)get_pntr(v->field1);
    free(arr->elements);
    free(arr);
    break;
  }
  case CELL_FRAME: {
    frame *f = (frame*)get_pntr(v->field1);
    f->c = NULL;
    assert(proc->done || (STATE_DONE == f->state) || (STATE_NEW == f->state));
    frame_dealloc(proc,f);
    break;
  }
  case CELL_CAP: {
    cap *cp = (cap*)get_pntr(v->field1);
    cap_dealloc(cp);
    break;
  }
  case CELL_REMOTEREF:
    break;
  }
}

int count_alive(process *proc)
{
  block *bl;
  int i;
  int alive = 0;
  for (bl = proc->blocks; bl; bl = bl->next)
    for (i = 0; i < BLOCK_SIZE; i++)
      if (CELL_EMPTY != bl->values[i].type)
        alive++;
  return alive;
}

void clear_marks(process *proc, short bit)
{
  block *bl;
  int i;
  int h;
  global *glo;

  for (bl = proc->blocks; bl; bl = bl->next)
    for (i = 0; i < BLOCK_SIZE; i++)
      bl->values[i].flags &= ~bit;

  for (h = 0; h < GLOBAL_HASH_SIZE; h++)
    for (glo = proc->pntrhash[h]; glo; glo = glo->pntrnext)
      glo->flags &= ~bit;
}

void mark_roots(process *proc, short bit)
{
  int pid;
  int i;
  frame *f;
  list *l;
  int h;
  global *glo;

  proc->memdebug = 0;

  for (i = 0; i < proc->nstrings; i++)
    mark(proc,proc->strings[i],bit);

  if (proc->streamstack) {
    for (i = 0; i < proc->streamstack->count; i++)
      mark(proc,proc->streamstack->data[i],bit);
  }

  for (f = proc->sparked.first; f; f = f->qnext) {
    mark_frame(proc,f,bit);
    assert(NULL == f->wq.frames);
    assert(NULL == f->wq.fetchers);
  }

  for (f = proc->runnable.first; f; f = f->qnext) {
    mark_frame(proc,f,bit);
  }

  for (f = proc->blocked.first; f; f = f->qnext) {
    mark_frame(proc,f,bit);
  }

  /* mark any in-flight gaddrs that refer to objects in this process */
  for (l = proc->inflight; l; l = l->next) {
    gaddr *addr = (gaddr*)l->data;
    if ((0 <= addr->lid) && (addr->pid == proc->pid)) {
      glo = addrhash_lookup(proc,*addr);
      assert(glo);
      if (proc->memdebug) {
        fprintf(proc->output,"root: inflight local ref %d@%d\n",
                addr->lid,addr->pid);
      }
      mark_global(proc,glo,bit);
    }
  }

  for (pid = 0; pid < proc->groupsize; pid++) {
    for (i = 0; i < array_count(proc->inflight_addrs[pid]); i++) {
      gaddr addr = array_item(proc->inflight_addrs[pid],i,gaddr);
      if ((0 <= addr.lid) && (addr.pid == proc->pid)) {
        glo = addrhash_lookup(proc,addr);
        assert(glo);
        if (proc->memdebug) {
          fprintf(proc->output,"root: inflight local ref %d@%d\n",
                  addr.lid,addr.pid);
        }
        mark_global(proc,glo,bit);
      }
    }
  }

  /* mark any remote references that are currently being fetched (and any frames waiting
     on them */
  for (h = 0; h < GLOBAL_HASH_SIZE; h++)
    for (glo = proc->pntrhash[h]; glo; glo = glo->pntrnext)
      if (glo->fetching)
        mark_global(proc,glo,FLAG_MARKED);
}

void print_cells(process *proc)
{
  block *bl;
  int i;
  for (bl = proc->blocks; bl; bl = bl->next) {
    for (i = 0; i < BLOCK_SIZE; i++) {
      if (CELL_EMPTY != bl->values[i].type) {
        pntr p;
        make_pntr(p,&bl->values[i]);
        fprintf(proc->output,"remaining: ");
        print_pntr(proc->output,p);
        fprintf(proc->output,"\n");
      }
    }
  }
}

void sweep(process *proc)
{
  block *bl;
  int i;

  global **gptr;
  int h;
  global *glo;


  /* Treat all new globals and cells that were created during this garbage collection cycle
     as rools that also need to be marked */
  if (proc->indistgc) {
    for (h = 0; h < GLOBAL_HASH_SIZE; h++)
      for (glo = proc->pntrhash[h]; glo; glo = glo->pntrnext)
        if (glo->flags & FLAG_NEW)
          mark_global(proc,glo,FLAG_MARKED);

    for (bl = proc->blocks; bl; bl = bl->next) {
      for (i = 0; i < BLOCK_SIZE; i++) {
        if ((CELL_EMPTY != bl->values[i].type) && (bl->values[i].flags & FLAG_NEW)) {
          pntr p;
          make_pntr(p,&bl->values[i]);
          mark(proc,p,FLAG_MARKED);
        }
      }
    }
  }

  for (h = 0; h < GLOBAL_HASH_SIZE; h++) {
    gptr = &proc->pntrhash[h];
    while (*gptr) {
      int needed = 0;
      glo = *gptr;

      if ((glo->flags & FLAG_MARKED) || (glo->flags & FLAG_DMB)) {
        needed = 1;
      }
      else if (is_pntr(glo->p)) {
        cell *c = get_pntr(glo->p);
        checkcell(c);
        if ((c->flags & FLAG_MARKED) || (c->flags & FLAG_DMB))
          needed = 1;
      }

      if (!needed) {
        #ifdef COLLECTION_DEBUG
        fprintf(proc->output,"removing garbage global %d@%d\n",glo->addr.lid,glo->addr.pid);
        #endif
        *gptr = (*gptr)->pntrnext;

        addrhash_remove(proc,glo);
        free_global(proc,glo);
      }
      else {
        gptr = &(*gptr)->pntrnext;
      }
    }
  }

  for (bl = proc->blocks; bl; bl = bl->next) {
    for (i = 0; i < BLOCK_SIZE; i++) {
      if (!(bl->values[i].flags & FLAG_MARKED) &&
          !(bl->values[i].flags & FLAG_DMB) &&
          !(bl->values[i].flags & FLAG_PINNED) &&
          (CELL_EMPTY != bl->values[i].type)) {
        free_cell_fields(proc,&bl->values[i]);
        bl->values[i].type = CELL_EMPTY;

        /* comment out these two lines to avoid reallocating cells, to check invalid accesses */
        make_pntr(bl->values[i].field1,proc->freeptr);
        proc->freeptr = &bl->values[i];
      }
    }
  }
}

void local_collect(process *proc)
{
  int h;
  global *glo;

  /* clear */
  clear_marks(proc,FLAG_MARKED);

  /* mark */
  mark_roots(proc,FLAG_MARKED);

  /* treat any objects referenced from other processes as roots */
  for (h = 0; h < GLOBAL_HASH_SIZE; h++)
    for (glo = proc->pntrhash[h]; glo; glo = glo->pntrnext)
      if (proc->pid == glo->addr.pid)
        mark_global(proc,glo,FLAG_MARKED);

  /* sweep */
  sweep(proc);

  #ifdef COLLECTION_DEBUG
  fprintf(proc->output,"local_collect() finished: %d cells remaining\n",count_alive(proc));
  #endif
}

frame *frame_alloc(process *proc)
{
  frame *f = (frame*)calloc(1,sizeof(frame));
  f->fno = -1;

  return f;
}

void frame_dealloc(process *proc, frame *f)
{
  assert(proc->done || (NULL == f->c));
/*   assert((NULL == f->c) || CELL_FRAME != celltype(f->c)); */
  assert(proc->done || (STATE_DONE == f->state) || (STATE_NEW == f->state));
  assert(proc->done || (NULL == f->wq.frames));
  assert(proc->done || (NULL == f->wq.fetchers));
  list_free(f->wq.fetchers,free);
  list_free(f->wq.frames,NULL);
  if (f->data)
    free(f->data);
  f->data = NULL;
  free(f);
}

cap *cap_alloc(int arity, int address, int fno)
{
  cap *c = (cap*)calloc(1,sizeof(cap));
  c->arity = arity;
  c->address = address;
  c->fno = fno;
  assert(0 < c->arity); /* MKCAP should not be called for CAFs */
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
  return s;
}

void pntrstack_push(pntrstack *s, pntr p)
{
  if (s->count == s->alloc) {
    if (s->count >= STACK_LIMIT) {
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

void print_pntr(FILE *f, pntr p)
{
  switch (pntrtype(p)) {
  case CELL_NUMBER:
    print_double(f,p);
    break;
  case CELL_NIL:
    fprintf(f,"nil");
    break;
  case CELL_FRAME: {
    frame *fr = (frame*)get_pntr(get_pntr(p)->field1);
    fprintf(f,"frame(%d/%d)",fr->fno,fr->count);
    break;
  }
  case CELL_REMOTEREF: {
    global *glo = (global*)get_pntr(get_pntr(p)->field1);
    fprintf(f,"%d@%d",glo->addr.lid,glo->addr.pid);
    break;
  }
  case CELL_IND: {
    fprintf(f,"(%s ",cell_types[pntrtype(p)]);
    print_pntr(f,get_pntr(p)->field1);
    fprintf(f,")");
    break;
  }
  default:
    fprintf(f,"(%s)",cell_types[pntrtype(p)]);
    break;
  }
}

