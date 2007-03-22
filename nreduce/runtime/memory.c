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
#include <unistd.h>

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
};

const char *msg_names[MSG_COUNT] = {
  "DONE",
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
  "STARTDISTGC",
  "NEWTASK",
  "NEWTASKRESP",
  "INITTASK",
  "INITTASKRESP",
  "STARTTASK",
  "STARTTASKRESP",
  "KILL",
  "IORESPONSE",
  "CONSOLE_LINE",
  "CONSOLE_CLOSE",
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

cell *pntrcell(pntr p)
{
  return (cell*)get_pntr(p);
}

global *pntrglobal(pntr p)
{
  return (global*)get_pntr(p);
}

frame *pntrframe(pntr p)
{
  return (frame*)get_pntr(p);
}

sysobject *pntrso(pntr p)
{
  return (sysobject*)get_pntr(p);
}

const char *pntrtypename(pntr p)
{
  return cell_types[pntrtype(p)];
}

static void mark(task *tsk, pntr p, short bit);
static void mark_frame(task *tsk, frame *f, short bit);

void mark_global(task *tsk, global *glo, short bit)
{
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

    p = tsk->markstack->data[--tsk->markstack->count];

    /* handle CONS and AREF specially - process the "spine" iteratively */
    while ((CELL_AREF == pntrtype(p)) || (CELL_CONS == pntrtype(p))) {
      c = get_pntr(p);
      if (c->flags & bit) {
        cont = 1;
        break;
      }
      c->flags |= bit;

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
      abort();
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
  if (NULL == tsk->freeptr) {
    block *bl = (block*)calloc(1,sizeof(block));
    int i;
    bl->next = tsk->blocks;
    tsk->blocks = bl;
    for (i = 0; i < BLOCK_SIZE-1; i++)
      make_pntr(bl->values[i].field1,&bl->values[i+1]);
    make_pntr(bl->values[i].field1,NULL);

    tsk->freeptr = &bl->values[0];
  }
  v = tsk->freeptr;
  v->flags = tsk->indistgc ? FLAG_NEW : 0;
  tsk->freeptr = (cell*)get_pntr(tsk->freeptr->field1);
  tsk->alloc_bytes += sizeof(cell);
  #ifdef PROFILING
  tsk->stats.cell_allocs++;
  #endif
  if ((tsk->alloc_bytes >= COLLECT_THRESHOLD) && tsk->endpt && tsk->endpt->interruptptr)
    *tsk->endpt->interruptptr = 1;
  return v;
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
  switch (so->type) {
  case SYSOBJECT_FILE:
    close(so->fd);
    break;
  case SYSOBJECT_CONNECTION: {
    connection *conn;

    lock_node(tsk->n);
    if (0 == so_lookup_connection(tsk,so,&conn)) {

      if (!tsk->done) {
        assert(0 == conn->frameids[CONNECT_FRAMEADDR]);
        assert(0 == conn->frameids[READ_FRAMEADDR]);
        assert(0 == conn->frameids[WRITE_FRAMEADDR]);
        assert(0 == conn->frameids[LISTEN_FRAMEADDR]);
        assert(0 == conn->frameids[ACCEPT_FRAMEADDR]);
        assert(conn->dontread);
      }

      conn->status = NULL;
      conn->collected = 1;

      if (!conn->finwrite) {
        conn->finwrite = 1;
        node_notify(tsk->n);
      }

      done_reading(tsk->n,conn);
    }
    unlock_node(tsk->n);

    free(so->hostname);
    free(so->buf);
    break;
  }
  case SYSOBJECT_LISTENER:
    if (so->newso)
      free_sysobject(tsk,so->newso);
    node_remove_listener(tsk->n,so->l);
    free(so->hostname);
    break;
  default:
    abort();
    break;
  }
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

int count_alive(task *tsk)
{
  block *bl;
  int i;
  int alive = 0;
  for (bl = tsk->blocks; bl; bl = bl->next)
    for (i = 0; i < BLOCK_SIZE; i++)
      if (CELL_EMPTY != bl->values[i].type)
        alive++;
  return alive;
}

void clear_marks(task *tsk, short bit)
{
  block *bl;
  int i;
  int h;
  global *glo;

  for (bl = tsk->blocks; bl; bl = bl->next)
    for (i = 0; i < BLOCK_SIZE; i++)
      bl->values[i].flags &= ~bit;

  for (h = 0; h < GLOBAL_HASH_SIZE; h++)
    for (glo = tsk->pntrhash[h]; glo; glo = glo->pntrnext)
      glo->flags &= ~bit;
}

void mark_roots(task *tsk, short bit)
{
  int pid;
  int i;
  frame *f;
  list *l;
  int h;
  global *glo;

  tsk->memdebug = 0;

  for (i = 0; i < tsk->nstrings; i++)
    mark(tsk,tsk->strings[i],bit);

  if (tsk->streamstack) {
    for (i = 0; i < tsk->streamstack->count; i++)
      mark(tsk,tsk->streamstack->data[i],bit);
  }

  if (tsk->tracing)
    mark(tsk,tsk->trace_root,bit);

  if (tsk->partial)
    mark(tsk,tsk->partial_scp,bit);

  for (f = tsk->sparked.first; f; f = f->next) {
    mark_frame(tsk,f,bit);
    assert(NULL == f->wq.frames);
    assert(NULL == f->wq.fetchers);
  }

  for (f = tsk->active.first; f; f = f->next)
    mark_frame(tsk,f,bit);

  /* mark any in-flight gaddrs that refer to objects in this task */
  for (l = tsk->inflight; l; l = l->next) {
    gaddr *addr = (gaddr*)l->data;
    if ((0 <= addr->lid) && (addr->tid == tsk->tid)) {
      glo = addrhash_lookup(tsk,*addr);
      assert(glo);
      if (tsk->memdebug) {
        fprintf(tsk->output,"root: inflight local ref %d@%d\n",
                addr->lid,addr->tid);
      }
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
        if (tsk->memdebug) {
          fprintf(tsk->output,"root: inflight local ref %d@%d\n",
                  addr.lid,addr.tid);
        }
        mark_global(tsk,glo,bit);
      }
    }
  }

  /* mark any remote references that are currently being fetched (and any frames waiting
     on them */
  for (h = 0; h < GLOBAL_HASH_SIZE; h++)
    for (glo = tsk->pntrhash[h]; glo; glo = glo->pntrnext)
      if (glo->fetching)
        mark_global(tsk,glo,FLAG_MARKED);
}

void print_cells(task *tsk)
{
  block *bl;
  int i;
  for (bl = tsk->blocks; bl; bl = bl->next) {
    for (i = 0; i < BLOCK_SIZE; i++) {
      if (CELL_EMPTY != bl->values[i].type) {
        pntr p;
        make_pntr(p,&bl->values[i]);
        fprintf(tsk->output,"remaining: ");
        print_pntr(tsk->output,p);
        fprintf(tsk->output,"\n");
      }
    }
  }
}

void sweep(task *tsk, int all)
{
  block *bl;
  int i;

  global **gptr;
  int h;
  global *glo;


  /* Treat all new globals and cells that were created during this garbage collection cycle
     as rools that also need to be marked */
  if (tsk->indistgc) {
    for (h = 0; h < GLOBAL_HASH_SIZE; h++)
      for (glo = tsk->pntrhash[h]; glo; glo = glo->pntrnext)
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

  for (h = 0; h < GLOBAL_HASH_SIZE; h++) {
    gptr = &tsk->pntrhash[h];
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
        fprintf(tsk->output,"removing garbage global %d@%d\n",glo->addr.lid,glo->addr.tid);
        #endif
        *gptr = (*gptr)->pntrnext;

        addrhash_remove(tsk,glo);
        free_global(tsk,glo);
      }
      else {
        gptr = &(*gptr)->pntrnext;
      }
    }
  }

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

void local_collect(task *tsk)
{
  int h;
  global *glo;

  #ifdef PROFILING
  tsk->stats.gcs++;
  #endif

  tsk->alloc_bytes = 0;

  /* clear */
  clear_marks(tsk,FLAG_MARKED);

  /* mark */
  mark_roots(tsk,FLAG_MARKED);

  /* treat any objects referenced from other tasks as roots */
  for (h = 0; h < GLOBAL_HASH_SIZE; h++)
    for (glo = tsk->pntrhash[h]; glo; glo = glo->pntrnext)
      if (tsk->tid == glo->addr.tid)
        mark_global(tsk,glo,FLAG_MARKED);

  /* sweep */
  sweep(tsk,0);

  #ifdef COLLECTION_DEBUG
  fprintf(tsk->output,"local_collect() finished: %d cells remaining\n",count_alive(tsk));
  #endif
}

void memusage(task *tsk, int *cells, int *bytes, int *alloc, int *conns)
{
  block *bl;
  int i;

  *cells = 0;
  *bytes = 0;
  *conns = 0;
  *alloc = 0;

  lock_node(tsk->n);
  for (bl = tsk->blocks; bl; bl = bl->next) {
    for (i = 0; i < BLOCK_SIZE; i++) {
      cell *c = &bl->values[i];
      if (CELL_AREF == c->type) {
        carray *arr = (carray*)get_pntr(c->field1);
        (*bytes) += arr->size*arr->elemsize;
        (*alloc) += arr->alloc*arr->elemsize;
      }
      else if (CELL_SYSOBJECT == c->type) {
        sysobject *so = (sysobject*)get_pntr(c->field1);
        if (SYSOBJECT_CONNECTION == so->type) {
          if (0 == so->status)
            (*conns)++;
        }
      }
      if (CELL_EMPTY != c->type) {
        (*bytes) += sizeof(cell);
        (*cells)++;
      }
    }
    (*alloc) += sizeof(block);
  }
  unlock_node(tsk->n);
}

frame *frame_new(task *tsk)
{
  frame *f;
  int framesize = sizeof(frame)+tsk->maxstack*sizeof(pntr);
  if (NULL == tsk->freeframe) {
    frameblock *fb = (frameblock*)calloc(1,sizeof(frameblock));
    int i;
    int framesperblock = FRAMEBLOCK_SIZE/framesize;
    fb->next = tsk->frameblocks;
    tsk->frameblocks = fb;

    for (i = 0; i < framesperblock-1; i++)
      ((frame*)&fb->mem[i*framesize])->freelnk = ((frame*)&fb->mem[(i+1)*framesize]);
    ((frame*)&fb->mem[i*framesize])->freelnk = NULL;

    tsk->freeframe = (frame*)fb->mem;
  }

  f = tsk->freeframe;
  tsk->freeframe = f->freelnk;
  tsk->alloc_bytes += framesize;

  if ((tsk->alloc_bytes >= COLLECT_THRESHOLD) && tsk->endpt && tsk->endpt->interruptptr)
    *tsk->endpt->interruptptr = 1;

  assert(!f->used);
  memset(f,0,framesize);
  f->used = 1;
  f->alloc = tsk->maxstack;
  assert(f->data == (pntr*)(((char*)f)+sizeof(frame)));

  f->fno = -1;

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

void print_pntr(FILE *f, pntr p)
{
  p = resolve_pntr(p);
  switch (pntrtype(p)) {
  case CELL_NUMBER:
    print_double(f,p);
    break;
  case CELL_NIL:
    fprintf(f,"nil");
    break;
  case CELL_SCREF: {
    scomb *sc = (scomb*)get_pntr(get_pntr(p)->field1);
    fprintf(f,"%s",sc->name);
    break;
  }
  case CELL_FRAME: {
    frame *fr = (frame*)get_pntr(get_pntr(p)->field1);
    fprintf(f,"frame(%d)",fr->fno);
    break;
  }
  case CELL_REMOTEREF: {
    global *glo = (global*)get_pntr(get_pntr(p)->field1);
    fprintf(f,"%d@%d",glo->addr.lid,glo->addr.tid);
    break;
  }
  case CELL_IND: {
    fprintf(f,"(%s ",cell_types[pntrtype(p)]);
    print_pntr(f,get_pntr(p)->field1);
    fprintf(f,")");
    break;
  }
  case CELL_APPLICATION: {
    print_pntr(f,get_pntr(p)->field1);
    fprintf(f," ");
    print_pntr(f,get_pntr(p)->field2);
    break;
  }
  case CELL_BUILTIN: {
    int bif = (int)get_pntr(get_pntr(p)->field1);
    fprintf(f,"%s",builtin_info[bif].name);
    break;
  }
  default:
    fprintf(f,"(%s)",cell_types[pntrtype(p)]);
    break;
  }
}

