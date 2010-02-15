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

//#define CHECK_HEAP_INTEGRITY
//#define DONT_FREE_BLOCKS

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#define MEMORY_C

#include "src/nreduce.h"
#include "compiler/bytecode.h"
#include "compiler/source.h"
#include "runtime/runtime.h"
#include "messages.h"
#include "events.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <assert.h>
#include <ctype.h>
#include <stdarg.h>
#include <math.h>
#include <unistd.h>
#include <sys/time.h>

static void *add_to_oldgen(task *tsk, void *mem);

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
  "[OBJS]",
  "[ARRAY]",
  "[CAP]",
};

const char *sysobject_types[SYSOBJECT_COUNT] = {
  "FILE",
  "CONNECTION",
  "LISTENER",
  "JAVA",
};

const char *frame_states[5] = {
  "UNALLOCATED",
  "NEW",
  "SPARKED",
  "ACTIVE",
  "DONE",
};

#define check_valid_object(_c) {                 \
                        if (CELL_COUNT <= (_c)->type) \
                          printf("Not valid object: %p\n",(_c)); \
                        assert(CELL_COUNT > (_c)->type); }

#define REPLACE_PNTR(p) { if (check) { \
                            if (is_pntr(p)) { \
                              assert(REF_FLAGS != ((cell*)(p).data[0])->flags); \
                              assert(0 != ((cell*)(p).data[0])->type); \
                              check_valid_object(((cell*)(p).data[0])); \
                            } \
                         } else if (is_pntr(p)) { \
                             if (REF_FLAGS == (unsigned int)((cell*)(p).data[0])->flags) {     \
                               (p).data[0] = (unsigned int)((cell*)(p).data[0])->type; } \
                         } }

#define REPLACE_CELL(c) { if (check) { \
                            assert(REF_FLAGS != (c)->flags); \
                            assert(0 != (c)->type); \
                            check_valid_object((c));              \
                          } else if (REF_FLAGS == (c)->flags) {                    \
                            (c) = (cell*)(c)->type; }     \
                        }

#ifndef INLINE_RESOLVE_PNTR
pntr resolve_pntr(pntr p)
{
  while (CELL_IND == pntrtype(p))
    p = get_pntr(p)->field1;
  return p;
}
#endif

unsigned int object_size(void *ptr)
{
  check_valid_object(((header*)ptr));
  assert(REF_FLAGS != ((header*)ptr)->flags);
  if (((header*)ptr)->type < CELL_OBJS)
    return sizeof(cell);
  else
    return ((objheader*)ptr)->nbytes;
}

static void mark(task *tsk, pntr p, unsigned int bit, int depth);

static inline pntr resolve_copy_pntr(task *tsk, cell *refcell, int bit, pntr p2)
{
  pntr p = p2;
  int changed = 0;
  while (1) {
    cell *c;
    if (!is_pntr(p))
      break;
    c = get_pntr(p);
    if (CELL_IND == c->type)
      p = c->field1;
    else if (REF_FLAGS == c->flags)
      p.data[0] = (unsigned int)c->type;
    else
      break;
    changed = 1;
  }

  if (changed && (NULL != refcell) && (FLAG_DMB == bit))
    write_barrier_ifnew(tsk,refcell,p);

  return p;
}

void mark_global(task *tsk, global *glo, unsigned int bit, int depth)
{
  assert(glo);
/*   if (glo->flags & bit) */ // FIXME?
/*     return; */

  glo->flags |= bit;
  mark(tsk,glo->p,bit,depth);

  if ((FLAG_DMB == bit) && (0 <= glo->addr.lid) && (tsk->tid != glo->addr.tid))
    add_pending_mark(tsk,glo->addr);
}

static void mark_frame(task *tsk, frame *f, unsigned int bit, int depth)
{
  int i;
  int count = f->instr->expcount;
  if (f->c) {
    pntr p;
    make_pntr(p,f->c);
    assert(CELL_IND != f->c->type);
    mark(tsk,p,bit,depth);
  }
  for (i = 0; i < count; i++) {
    assert(!is_nullpntr(f->data[i]));
    f->data[i] = resolve_copy_pntr(tsk,f->c,bit,f->data[i]);
    mark(tsk,f->data[i],bit,depth);
  }
}

static void mark_cap(task *tsk, cap *cp, unsigned int bit, int depth)
{
  int i;
  pntr p;
  make_pntr(p,cp);
  mark(tsk,p,bit,depth);

  pntr pc;
  make_pntr(pc,cp->c);
  mark(tsk,pc,bit,depth);

  for (i = 0; i < cp->count; i++) {
    assert(!is_nullpntr(cp->data[i]));
    cp->data[i] = resolve_copy_pntr(tsk,cp->c,bit,cp->data[i]);
    mark(tsk,cp->data[i],bit,depth);
  }
}

static inline int has_been_copied(task *tsk, void *mem)
{
  return ((((header*)mem)->flags & FLAG_ALTSPACE) == tsk->altspace);
}

static header *mark_refs(task *tsk, header *v, unsigned int bit, int depth)
{
  #ifdef DISABLE_ARRAYS
  assert(CELL_AREF != v->type);
  #endif
  cell *c = (cell*)v;
  switch (v->type) {
  case CELL_AREF:
    c->field1 = resolve_copy_pntr(tsk,c,bit,c->field1);
    assert(CELL_O_ARRAY == pntrtype(c->field1));
    mark(tsk,c->field1,bit,depth+1);
    c->field2 = resolve_copy_pntr(tsk,c,bit,c->field2);
    if (is_pntr(c->field2))
      return (header*)get_pntr(c->field2);
    break;
  case CELL_O_ARRAY: {
    carray *arr = (carray*)c;
    assert(MAX_ARRAY_SIZE >= arr->size);
    if (sizeof(pntr) == arr->elemsize) {
      int i;
      for (i = 0; i < arr->size; i++) {
        ((pntr*)arr->elements)[i] = resolve_copy_pntr(tsk,c,bit,((pntr*)arr->elements)[i]);
        mark(tsk,((pntr*)arr->elements)[i],bit,depth+1);
      }
    }
    break;
  }
  case CELL_CONS: {
    c->field1 = resolve_copy_pntr(tsk,c,bit,c->field1);
    c->field2 = resolve_copy_pntr(tsk,c,bit,c->field2);
    mark(tsk,c->field1,bit,depth+1);
    if (is_pntr(c->field2))
      return (header*)get_pntr(c->field2);
    break;
  }
  case CELL_IND:
    mark(tsk,c->field1,bit,depth+1);
    break;
  case CELL_APPLICATION:
    c->field1 = resolve_copy_pntr(tsk,c,bit,c->field1);
    c->field2 = resolve_copy_pntr(tsk,c,bit,c->field2);
    mark(tsk,c->field1,bit,depth+1);
    mark(tsk,c->field2,bit,depth+1);
    break;
  case CELL_FRAME:
    mark_frame(tsk,(frame*)get_pntr(c->field1),bit,depth+1);
    break;
  case CELL_CAP:
    mark(tsk,c->field1,bit,depth+1);
    c->field1 = resolve_copy_pntr(tsk,c,bit,c->field1);
    assert(CELL_O_CAP == pntrtype(c->field1));
    mark_cap(tsk,(cap*)get_pntr(c->field1),bit,depth+1);
    break;
  case CELL_REMOTEREF: {
    global *glo = (global*)get_pntr(c->field1);
    mark_global(tsk,glo,bit,depth+1);
    break;
  }
  case CELL_SYSOBJECT: {
    sysobject *so = (sysobject*)get_pntr(c->field1);
    so->marked = 1;
    if (!is_nullpntr(so->listenerso)) {
      so->listenerso = resolve_copy_pntr(tsk,c,bit,so->listenerso);
      mark(tsk,so->listenerso,bit,depth+1);
    }
    if (so->newso) {
      so->newso->p = resolve_copy_pntr(tsk,so->newso->c,bit,so->newso->p);
      mark(tsk,so->newso->p,bit,depth+1);
    }
    break;
  }
  case CELL_BUILTIN:
  case CELL_SCREF:
  case CELL_NIL:
  case CELL_NUMBER:
  case CELL_HOLE:
  case CELL_SYMBOL:
    break;
  case CELL_O_CAP:
    /* taken care of by the CAP cell */
    break;
  default:
    fatal("Invalid pntr type %d",c->type);
    break;
  }
  return NULL;
}

static void mark_or_copy(task *tsk, header *v, unsigned int bit, int depth)
{
  assert((FLAG_MARKED == bit) || (FLAG_DMB == bit));

  while (v && (REF_FLAGS != v->flags) && !(v->flags & bit)) {
    check_valid_object(v);

    if (FLAG_MARKED == bit) {
      v->flags |= bit;
      v = add_to_oldgen(tsk,v);
    }
    else {
      v->flags |= bit;
      global *target;
      pntr p;
      make_pntr(p,v);
      if (NULL != (target = targethash_lookup(tsk,p)))
        mark_global(tsk,target,bit,depth);
    }

    v = mark_refs(tsk,v,bit,depth);
  }
}

static void mark(task *tsk, pntr p, unsigned int bit, int depth)
{
  assert(CELL_EMPTY != pntrtype(p));
  assert(tsk->markstack);
  if (!is_pntr(p))
    return;
  if (depth > 100) {
    stack_push(tsk->markstack,get_pntr(p));
    return;
  }

  mark_or_copy(tsk,(header*)get_pntr(p),bit,depth);
}

void mark_start(task *tsk, unsigned int bit)
{
  assert(NULL == tsk->markstack);
  tsk->markstack = stack_new();
  tsk->markstack->limit = -1;
}

void mark_end(task *tsk, unsigned int bit)
{
  assert(tsk->markstack);
  while (0 < tsk->markstack->count) {
    void *c = tsk->markstack->data[--tsk->markstack->count];
    mark_or_copy(tsk,c,bit,0);
  }

  stack_free(tsk->markstack);
  tsk->markstack = NULL;
}

/* FIXME: setting FLAG_DMB on newly created objects is not sufficient to ensure they
   survive beyond the end of the current distributed garbage collection cycle. This is
   because object retention is no longer based on marks, but on them being referenced
   somewhere. We need to ensure that during a local collection, all objects that have
   were new in the current distributed collection cycle are copied. */
void *alloc_mem(task *tsk, unsigned int nbytes)
{
  header *h;
  assert(tsk);
  assert(0 == nbytes%8); /* enforce alignment for performance reasons */
  assert(nbytes <= (BLOCK_END-BLOCK_START));

  /* New allocation method - pointer increment */
  assert(((int)tsk->newgenoffset >= sizeof(block)) || (NULL != tsk->newgen));

  if (tsk->newgenoffset >= sizeof(block)-nbytes) {
    if (tsk->newgen) {
      /* Filled up the first block... request a minor collection */
      tsk->need_minor = 1;
      if (tsk->endpt) {
/*         fprintf(stderr,"first block full; requesting minor collection\n"); */
        endpoint_interrupt(tsk->endpt);
      }
    }

    block *bl = (block*)calloc(1,sizeof(block));
    bl->next = tsk->newgen;
    bl->used = tsk->newgenoffset;
    tsk->newgen = bl;
    tsk->newgenoffset = BLOCK_START;
  }
  else {
/*     fprintf(stderr,"allocated cell\n"); */
  }
  h = (header*)(tsk->newgenoffset+(unsigned int)tsk->newgen);
  tsk->newgenoffset += nbytes;

  h->flags = tsk->newcellflags;

  #ifdef OBJECT_LIFETIMES
  h->birth = tsk->total_bytes_allocated;
  tsk->total_bytes_allocated += nbytes;
  #endif
  #ifdef PROFILING
  tsk->stats.cell_allocs++;
  tsk->stats.total_bytes += nbytes;
  #endif
  return (char*)h;
}

void *realloc_mem(task *tsk, void *old, unsigned int nbytes)
{
  assert(0 <= object_size(old));
  assert(0 <= nbytes);
  assert(nbytes > object_size(old));
  char *new = alloc_mem(tsk,nbytes);
  unsigned int flags = ((header*)new)->flags;
  memcpy(new,old,object_size(old));
  ((header*)new)->flags = flags;
  if (((header*)old)->flags & FLAG_DMB)
    ((header*)new)->flags |= FLAG_DMB;
  if (((header*)old)->flags & FLAG_NEW)
    ((header*)new)->flags |= FLAG_NEW;
  return new;
}

cell *alloc_cell(task *tsk)
{
  return (cell*)alloc_mem(tsk,sizeof(cell));
}

sysobject *new_sysobject(task *tsk, int type)
{
  sysobject *so = (sysobject*)calloc(1,sizeof(sysobject));
  so->tsk = tsk;
  so->type = type;
  so->ownertid = tsk->tid;
  so->c = alloc_cell(tsk);
  make_pntr(so->p,so->c);
  so->c->type = CELL_SYSOBJECT;
  make_pntr(so->c->field1,so);
  llist_append(&tsk->sysobjects,so);
  tsk->nsysobjects++;
  return so;
}

static void sysobject_check_finished(sysobject *so)
{
  if (so->outgoing_connection &&
      so->done_connect && so->done_reading && so->done_writing) {
    if (so->local) {
      so->tsk->local_conns--;
      assert(0 <= so->tsk->local_conns);
    }
    so->tsk->total_conns--;
    assert(0 <= so->tsk->total_conns);
  }
}

void sysobject_done_connect(sysobject *so)
{
  if (so->done_connect)
    return;
  so->done_connect = 1;
  sysobject_check_finished(so);
}

void sysobject_done_reading(sysobject *so)
{
  if (so->done_reading)
    return;
  so->done_reading = 1;
  sysobject_check_finished(so);
}

void sysobject_done_writing(sysobject *so)
{
  if (so->done_writing)
    return;
  so->done_writing = 1;
  sysobject_check_finished(so);
}

void sysobject_delete_if_finished(sysobject *so)
{
  if (so->done_connect && so->done_reading && so->done_writing)
    free_sysobject(so->tsk,so);
}

sysobject *find_sysobject(task *tsk, const socketid *sockid)
{
  sysobject *so;
  for (so = tsk->sysobjects.first; so; so = so->next) {
    if (socketid_equals(&so->sockid,sockid)) {
      return so;
    }
  }
  return NULL;
}

static void remove_global(task *tsk, global *glo)
{
  event_remove_global(tsk,glo->addr);

  /* Remove from hash tables */
  addrhash_remove(tsk,glo);
  targethash_remove(tsk,glo);
  physhash_remove(tsk,glo);
  /* Remove from linked list of all globals */
  llist_remove(&tsk->globals,glo);
  /* Free fetchers */
  list_free(glo->wq.fetchers,free);

  /* Free memory */
  free(glo);
}

void free_sysobject(task *tsk, sysobject *so)
{
  tsk->nsysobjects--;
  llist_remove(&tsk->sysobjects,so);

  assert(NULL != so->c);
  assert(CELL_SYSOBJECT == so->c->type);
  so->c->type = CELL_IND;
  so->c->field1 = tsk->globnilpntr;

  if (so->ownertid == tsk->tid) {
    switch (so->type) {
    case SYSOBJECT_FILE:
      close(so->fd);
      break;
    case SYSOBJECT_CONNECTION: {
      sysobject_done_connect(so);
      sysobject_done_reading(so);
      sysobject_done_writing(so);
      if (!socketid_isnull(&so->sockid) && !so->from_network)
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
      if (!socketid_isnull(&so->sockid) && !so->from_network)
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

static void mark_inflight_local_addresses(task *tsk, unsigned int bit, int justglobal)
{
  list *l;
  int tid;
  int i;
  /* mark any in-flight gaddrs that refer to objects in this task */
  for (l = tsk->inflight; l; l = l->next) {
    gaddr *addr = (gaddr*)l->data;
    assert(0 <= addr->lid);
    if (addr->tid == tsk->tid) {
      global *glo = addrhash_lookup(tsk,*addr);
      if (NULL == glo) {
        node_log(tsk->n,LOG_ERROR,"Marking inflight local address (case 1) %d@%d: no such global",
                 addr->lid,addr->tid);
      }
      assert(glo);
      if (justglobal)
        glo->flags |= bit;
      else
        mark_global(tsk,glo,bit,0);
    }
  }

  for (tid = 0; tid < tsk->groupsize; tid++) {
    int count = array_count(tsk->inflight_addrs[tid]);
    for (i = 0; i < count; i++) {
      gaddr addr = array_item(tsk->inflight_addrs[tid],i,gaddr);
      assert(0 <= addr.lid);
      if (addr.tid == tsk->tid) {
        global *glo = addrhash_lookup(tsk,addr);
        if (NULL == glo) {
          node_log(tsk->n,LOG_ERROR,"Marking inflight local address (case 2) %d@%d: no such global",
                   addr.lid,addr.tid);
        }
        assert(glo);
        if (justglobal)
          glo->flags |= bit;
        else
          mark_global(tsk,glo,bit,0);
      }
    }
  }
}

static void mark_inflight_remote_addresses(task *tsk)
{
  list *l;
  int tid;
  int i;
  /* mark any in-flight gaddrs that refer to remote objects */
  for (l = tsk->inflight; l; l = l->next) {
    gaddr *addr = (gaddr*)l->data;
    if ((0 <= addr->lid) && (tsk->tid != addr->tid))
      add_pending_mark(tsk,*addr);
  }

  for (tid = 0; tid < tsk->groupsize; tid++) {
    int count = array_count(tsk->inflight_addrs[tid]);
    for (i = 0; i < count; i++) {
      gaddr addr = array_item(tsk->inflight_addrs[tid],i,gaddr);
      if ((0 <= addr.lid) && (tsk->tid != addr.tid))
        add_pending_mark(tsk,addr);
    }
  }
}

static void mark_active_frames(task *tsk, unsigned int bit)
{
  int i;
  frameblock *fb;
  /* Note: we don't need to mark sparked frames explicitly, since a frame will only be
     sparked if it is definitely needed by a program, which means there will be a
     reference to it from somewhere else. */
  for (fb = tsk->frameblocks; fb; fb = fb->next) {
    for (i = 0; i < tsk->framesperblock; i++) {
      frame *f = ((frame*)&fb->mem[i*tsk->framesize]);
      if ((STATE_ACTIVE == f->state) || (STATE_SPARKED == f->state))
        mark_frame(tsk,f,bit,0);
    }
  }
}

static void mark_misc_roots(task *tsk, unsigned int bit)
{
  int i;
  for (i = 0; i < tsk->nstrings; i++)
    mark(tsk,tsk->strings[i],bit,0);

  mark(tsk,tsk->argsp,bit,0);

  if (tsk->streamstack) {
    for (i = 0; i < tsk->streamstack->count; i++) {
      tsk->streamstack->data[i] = resolve_copy_pntr(tsk,NULL,bit,tsk->streamstack->data[i]);
      mark(tsk,tsk->streamstack->data[i],bit,0);
    }
  }

  if (tsk->tracing) {
    tsk->trace_root = resolve_copy_pntr(tsk,NULL,bit,tsk->trace_root);
    mark(tsk,tsk->trace_root,bit,0);
  }

  if (tsk->out_so) {
    tsk->out_so->p = resolve_copy_pntr(tsk,NULL,bit,tsk->out_so->p);
    mark(tsk,tsk->out_so->p,bit,0);
  }

  sysobject *so;
  for (so = tsk->sysobjects.first; so; so = so->next) {
    mark(tsk,so->p,bit,0);
  }
}

static void mark_roots(task *tsk, unsigned int bit)
{
  mark(tsk,tsk->globnilpntr,bit,0);
  mark(tsk,tsk->globtruepntr,bit,0);
  mark_misc_roots(tsk,bit);
  mark_active_frames(tsk,bit);
  mark_inflight_local_addresses(tsk,bit,0);
  mark_frame(tsk,tsk->sparklist,bit,0);

  /* Mark all globals that are being fetched but would not otherwise be marked
     (e.g. scheduled frames) */
  int h;
  global *glo;
  for (h = 0; h < GLOBAL_HASH_SIZE; h++) {
    for (glo = tsk->targethash[h]; glo; glo = glo->targetnext) {
      if (glo->fetching)
        mark_global(tsk,glo,bit,0);
    }
  }
}

static void mark_mature_remoteref_globals(task *tsk, unsigned int bit)
{
  int h;
  global *glo;
  for (h = 0; h < GLOBAL_HASH_SIZE; h++) {
    for (glo = tsk->addrhash[h]; glo; glo = glo->addrnext) {
      if (is_pntr(glo->p)) {
        header *v = (header*)get_pntr(glo->p);
        if (REF_FLAGS == v->flags)
          v = (header*)v->type;
        if (has_been_copied(tsk,v))
          mark_global(tsk,glo,bit,0);
      }
    }
  }
}

static void mark_incoming_global_refs(task *tsk, unsigned int bit)
{
  global *glo;
  /* treat any objects referenced from other tasks as roots */
  for (glo = tsk->globals.first; glo; glo = glo->next) {
    if (glo->addr.tid == tsk->tid) {
      mark_global(tsk,glo,bit,0);
    }
  }
}

static void clear_global_marks(task *tsk, unsigned int bit)
{
  global *glo;
  for (glo = tsk->globals.first; glo; glo = glo->next)
    glo->flags &= ~bit;
}

static void preserve_targets(task *tsk)
{
  /* Mark all targets whose local replicas are marked. This prevents us from losing the
     association between the local replica of an object and it's canonical address.
     This is particularly important for sysobjects, since other parts of the program
     depend on a sysobject who's owner tid is elsewhere always have a correct target. */

  global *glo;
  for (glo = tsk->globals.first; glo; glo = glo->next) {
    if (!(glo->flags & FLAG_MARKED)) {
      int check = 0;
      REPLACE_PNTR(glo->p);
      check_valid_object(((cell*)(glo->p).data[0]));
      if (is_pntr(glo->p) && ((cell*)(glo->p).data[0])->flags & FLAG_MARKED) {
        event_preserve_target(tsk,glo->addr,pntrtype(glo->p));
        glo->flags |= FLAG_MARKED;
      }
    }
  }
}

static void sweep_globals(task *tsk)
{
  global *glo;
  global *next;

  for (glo = tsk->globals.first; glo; glo = next) {
    next = glo->next;
    if (!(glo->flags & FLAG_MARKED))
      remove_global(tsk,glo);
  }
}

static void clear_marks(task *tsk, unsigned int bit)
{
  block *bl;
  global *glo;

  for (bl = tsk->oldgen; bl; bl = bl->next) {
    unsigned int off;
    for (off = BLOCK_START; off < BLOCK_END; off += object_size(off+(char*)bl)) {
      header *h = (header*)(off+(char*)bl);
      assert(REF_FLAGS != h->flags);
      check_valid_object(h);
      h->flags &= ~bit;
    }
  }
  /* This should only be called after a major collection, so the new generation
     should be empty */
  assert(NULL == tsk->newgen);

  for (glo = tsk->globals.first; glo; glo = glo->next)
    glo->flags &= ~bit;
}

static void *add_to_oldgen(task *tsk, void *mem)
{
  assert(REF_FLAGS != ((header*)mem)->flags);
  check_valid_object(((header*)mem));
  assert((0 == tsk->altspace) || (FLAG_ALTSPACE == tsk->altspace));

  if (has_been_copied(tsk,mem))
    return mem;

  unsigned int size = object_size(mem);

  if (tsk->oldgenoffset >= sizeof(block)-size) {
    block *bl = (block*)calloc(1,sizeof(block));
    bl->next = tsk->oldgen;
    bl->used = tsk->oldgenoffset;
    tsk->oldgen = bl;
    tsk->oldgenoffset = BLOCK_START;
    tsk->need_major = 1;
  }
  assert(tsk->oldgenoffset+object_size(mem) <= sizeof(block));

  char *newmem = (char*)(((char*)tsk->oldgen)+tsk->oldgenoffset);
  if (((header*)mem)->type < CELL_OBJS)
    *((cell*)newmem) = *((cell*)mem);
  else
    memcpy(newmem,mem,size);
  tsk->oldgenbytes += size;
  tsk->oldgenoffset += size;

  ((header*)mem)->type = (unsigned int)newmem;
  ((header*)mem)->flags = REF_FLAGS;

  if (tsk->altspace)
    ((header*)newmem)->flags |= FLAG_ALTSPACE;
  else
    ((header*)newmem)->flags &= ~FLAG_ALTSPACE;
  ((header*)newmem)->flags |= FLAG_MATURE;

  return (char*)newmem;
}

static void replace_refs_cell(task *tsk, cell *c, int check)
{
  switch (c->type) {
  case CELL_APPLICATION:
    REPLACE_PNTR(c->field1);
    REPLACE_PNTR(c->field2);
    break;
  case CELL_CONS:
    REPLACE_PNTR(c->field1);
    REPLACE_PNTR(c->field2);
    break;
  case CELL_IND:
    REPLACE_PNTR(c->field1);
    break;
  case CELL_AREF: {
    REPLACE_PNTR(c->field1);
    REPLACE_PNTR(c->field2);
    break;
  }
  case CELL_O_ARRAY: {
    carray *carr = (carray*)c;
    if (sizeof(pntr) == carr->elemsize) {
      int i;
      for (i = 0; i < carr->size; i++)
        REPLACE_PNTR(((pntr*)carr->elements)[i]);
    }
    break;
  }
  case CELL_FRAME: {
    frame *f = (frame*)get_pntr(c->field1);
    int i;
    for (i = 0; i < f->instr->expcount; i++)
      REPLACE_PNTR(f->data[i]);
    REPLACE_CELL(f->c);
    assert(c == f->c);
    break;
  }
  case CELL_CAP: {
    REPLACE_PNTR(c->field1);
    cap *cp = (cap*)get_pntr(c->field1);
    int i;
    for (i = 0; i < cp->count; i++)
      REPLACE_PNTR(cp->data[i]);
    REPLACE_CELL(cp->c);
    assert(c == cp->c);
    break;
  }
  case CELL_SYSOBJECT: {
    sysobject *so = (sysobject*)get_pntr(c->field1);
    REPLACE_CELL(so->c);
    assert(c == so->c);
    if (is_pntr(so->listenerso) && !is_nullpntr(so->listenerso))
      REPLACE_PNTR(so->listenerso);
    if (is_pntr(so->p) && !is_nullpntr(so->p))
      REPLACE_PNTR(so->p);
  }
  case CELL_REMOTEREF: {
    global *glo = (global*)get_pntr(c->field1);
    REPLACE_PNTR(glo->p);
    break;
  }
  }
}

static void replace_refs_globals(task *tsk, int check)
{
  int h;
  global *glo;
  if (check) {
    /* Just verification - REPLACE_PNTR only does a check here */
    for (h = 0; h < GLOBAL_HASH_SIZE; h++) {
      for (glo = tsk->physhash[h]; glo; glo = glo->physnext)
        REPLACE_PNTR(glo->p);
      for (glo = tsk->targethash[h]; glo; glo = glo->targetnext)
        REPLACE_PNTR(glo->p);
    }
  }
  else {
    /* Note that we need to rebuild the target and physical hash tables here,
       sine the pointers have changed */
    global *next;
    global **oldphyshash = tsk->physhash;
    global **oldtargethash = tsk->targethash;
    tsk->physhash = (global**)calloc(GLOBAL_HASH_SIZE,sizeof(global*));
    tsk->targethash = (global**)calloc(GLOBAL_HASH_SIZE,sizeof(global*));

    for (h = 0; h < GLOBAL_HASH_SIZE; h++) {
      for (glo = oldphyshash[h]; glo; glo = next) {
        next = glo->physnext;
        glo->physnext = NULL;
        REPLACE_PNTR(glo->p);
        physhash_add(tsk,glo);
      }
    }

    for (h = 0; h < GLOBAL_HASH_SIZE; h++) {
      for (glo = oldtargethash[h]; glo; glo = next) {
        next = glo->targetnext;
        glo->targetnext = NULL;
        REPLACE_PNTR(glo->p);
        targethash_add(tsk,glo);
      }
    }

    free(oldphyshash);
    free(oldtargethash);
  }
}

static void replace_refs_other(task *tsk, int check)
{
  frameblock *fb;
  int di;
  int blocks = 0;
  for (fb = tsk->frameblocks; fb; fb = fb->next) {
    blocks++;
    int fi;
    for (fi = 0; fi < tsk->framesperblock; fi++) {
      frame *f = ((frame*)&fb->mem[fi*tsk->framesize]);
      if ((STATE_ACTIVE == f->state) || (STATE_SPARKED == f->state)) {
        int i;
        for (i = 0; i < f->instr->expcount; i++)
          REPLACE_PNTR(f->data[i]);
        if (f->c)
          REPLACE_CELL(f->c);
      }
    }
  }

  REPLACE_PNTR(tsk->globnilpntr);
  REPLACE_PNTR(tsk->globtruepntr);
  REPLACE_PNTR(tsk->argsp);
  for (di = 0; di < tsk->nstrings; di++)
    REPLACE_PNTR(tsk->strings[di]);
  REPLACE_PNTR(tsk->trace_root);

  int rsetsize = array_count(tsk->remembered);
  cell **rsetdata = (cell**)tsk->remembered->data;
  int i;
  for (i = 0; i < rsetsize; i++)
    REPLACE_CELL(rsetdata[i]);

  if (tsk->streamstack) {
    for (di = 0; di < tsk->streamstack->count; di++)
      REPLACE_PNTR(tsk->streamstack->data[di]);
  }

  replace_refs_globals(tsk,check);
}

static void replace_refs(task *tsk, int check)
{
  block *bl;
  for (bl = tsk->oldgen; bl; bl = bl->next) {
    unsigned int off;
    for (off = BLOCK_START; off < BLOCK_END; off += object_size(off+(char*)bl)) {
      header *h = (header*)(off+(char*)bl);
      replace_refs_cell(tsk,(cell*)h,check);
    }
  }

  replace_refs_other(tsk,check);
}

static void init_oldgen(task *tsk)
{
  if (NULL == tsk->oldgen) {
    tsk->oldgen = (block*)calloc(1,sizeof(block));
    tsk->oldgenoffset = BLOCK_START;
  }
}

void free_blocks(block *bl)
{
  while (bl) {
    block *next = bl->next;
#ifdef DONT_FREE_BLOCKS
    memset(bl,0,sizeof(block));
#else
    free(bl);
#endif
    bl = next;
  }
}

void write_barrier(task *tsk, cell *c)
{
  if (!(c->flags & FLAG_INRSET) && (c->flags & FLAG_MATURE)) {
/*       fprintf(stderr,"Adding %p to remembered set\n",c); */
      array_append(tsk->remembered,&c,sizeof(cell*));
      c->flags |= FLAG_INRSET;
    }
}

/* Can be called from native code */
void write_barrier_ifnew(task *tsk, cell *c, pntr dest)
{
  if (is_pntr(dest) && !(get_pntr(dest)->flags & FLAG_MATURE))
    write_barrier(tsk,c);
}

void cell_make_ind(task *tsk, cell *c, pntr dest)
{
  write_barrier_ifnew(tsk,c,dest);
  c->type = CELL_IND;
  c->field1 = dest;
}

static void sweep_frames(task *tsk)
{
  frameblock *fb;
  int i;
  int keep = 0;
  int remove = 0;
  for (fb = tsk->frameblocks; fb; fb = fb->next) {
    for (i = 0; i < tsk->framesperblock; i++) {
      frame *f = ((frame*)&fb->mem[i*tsk->framesize]);
      if ((STATE_SPARKED == f->state) ||
          (STATE_ACTIVE == f->state)) {
        keep++;
      }
      else if (STATE_NEW == f->state) {
        assert(NULL != f->c);
        assert(NULL == f->retp);
        assert(CELL_EMPTY != f->c->type);
        assert(CELL_FRAME == f->c->type);
        if (f->c->flags & FLAG_MARKED) {
          keep++;
        }
        else {
          remove++;
          f->c = NULL;
          f->retp = NULL;
          frame_free(tsk,f);
        }
      }
    }
  }
}

static block *copy_heap_start(task *tsk)
{
  block *prevgen = tsk->oldgen;
  tsk->oldgen = NULL;
  tsk->oldgenbytes = 0;
  init_oldgen(tsk);
  if (tsk->altspace) {
    tsk->altspace = 0;
    tsk->newcellflags |= FLAG_ALTSPACE;
  }
  else {
    tsk->altspace = FLAG_ALTSPACE;
    tsk->newcellflags &= ~FLAG_ALTSPACE;
  }
  return prevgen;
}

static void copy_heap_finish(task *tsk, block *prevgen)
{
  replace_refs(tsk,0);
  sweep_frames(tsk);
  update_lifetimes(tsk,prevgen);
  free_blocks(prevgen);
}

static void mark_remembered_set(task *tsk)
{
  int rsetsize = array_count(tsk->remembered);
  cell **rsetdata = (cell**)tsk->remembered->data;
  int i;
  for (i = 0; i < rsetsize; i++) {
    pntr p;
    make_pntr(p,rsetdata[i]);
    assert(REF_FLAGS != rsetdata[i]->flags);
    check_valid_object(rsetdata[i]);
    rsetdata[i]->flags &= ~FLAG_MARKED;
    mark(tsk,p,FLAG_MARKED,0);
  }
}

static void clear_remembered_set(task *tsk)
{
  int rsetsize = array_count(tsk->remembered);
  cell **rsetdata = (cell**)tsk->remembered->data;
  int i;
  for (i = 0; i < rsetsize; i++) {
    assert(REF_FLAGS != rsetdata[i]->flags);
    assert(rsetdata[i]->flags & FLAG_INRSET);
    rsetdata[i]->flags &= ~FLAG_INRSET;
  }
  tsk->remembered->nbytes = 0;
}

static void begin_promotion(task *tsk, block **prevstart, unsigned int *prevoffset)
{
  init_oldgen(tsk);
  *prevstart = tsk->oldgen;
  *prevoffset = tsk->oldgenoffset;
  #ifdef CHECK_HEAP_INTEGRITY
  check_remembered_set(tsk);
  #endif
}

static void replace_refs_for_newgen(task *tsk, block *prevstart, unsigned int prevoffset)
{
  block *bl;
  assert(prevstart);

  bl = tsk->oldgen;
  unsigned int endoffset = tsk->oldgenoffset;

  unsigned int offset;

  while (bl != prevstart) {
    offset = BLOCK_START;
    while (offset < endoffset) {
      cell *c = (cell*)(((char*)bl)+offset);
      replace_refs_cell(tsk,c,0);
      offset += object_size(c);
    }
    bl = bl->next;
    endoffset = sizeof(block);
  }

  offset = prevoffset;
  while (offset < endoffset) {
    cell *c = (cell*)(((char*)bl)+offset);
    replace_refs_cell(tsk,c,0);
    offset += object_size(c);
  }

  int rsetsize = array_count(tsk->remembered);
  cell **rsetdata = (cell**)tsk->remembered->data;
  int i;
  for (i = 0; i < rsetsize; i++) {
    replace_refs_cell(tsk,rsetdata[i],0);
  }

  replace_refs_other(tsk,0);
}

static void reset_newgen(task *tsk)
{
  free_blocks(tsk->newgen);
  tsk->newgen = NULL;
  tsk->newgenoffset = sizeof(block);
}

static void finish_promotion(task *tsk, block *prevstart, unsigned int prevoffset)
{
  replace_refs_for_newgen(tsk,prevstart,prevoffset);
  sweep_frames(tsk);

  clear_remembered_set(tsk);
  update_lifetimes(tsk,tsk->newgen);
  reset_newgen(tsk);
  #ifdef CHECK_HEAP_INTEGRITY
  replace_refs(tsk,1); /* just check */
  #endif
}

void sweep_sysobjects(task *tsk, int all)
{
  sysobject *so;
  int deleted = 0;
  int kept = 0;
  so = tsk->sysobjects.first;
  while (so) {
    sysobject *next = so->next;
    if (!all && so->marked) {
      so->marked = 0;
      kept++;
    }
    else {
      free_sysobject(tsk,so);
      deleted++;
    }
    so = next;
  }
}

void force_major_collection(task *tsk)
{
  tsk->need_minor = 1;
  tsk->need_major = 1;
  tsk->skipremaining = 0;
  endpoint_interrupt(tsk->endpt);
}

/* When a local collection is performed during a distributed collection cycle, all objects
   that have the FLAG_NEW bit set must be copied. */
static void mark_new_objects(task *tsk, block *from)
{
  if (!tsk->indistgc)
    return;
  block *bl;
  for (bl = from; bl; bl = bl->next) {
    unsigned int off = BLOCK_START;
    while (off < BLOCK_END) {
      header *h = (header*)(off+(char*)bl);
      if ((REF_FLAGS != h->flags) && (h->flags & FLAG_NEW)) {
        pntr p;
        make_pntr(p,h);
        mark(tsk,p,FLAG_MARKED,0);
      }
      if (REF_FLAGS == h->flags)
        h = (header*)h->type;
      off += object_size(h);
    }
  }

  /* Treat all new globals and cells that were created during this distributed garbage
     collection cycle as roots that also need to be marked */
  global *glo;
  for (glo = tsk->globals.first; glo; glo = glo->next)
    if (glo->flags & FLAG_NEW)
      mark_global(tsk,glo,FLAG_MARKED,0);
}

void local_collect(task *tsk)
{
/*   fprintf(stderr,"local_collect\n"); */
  int prev_oldgenbytes = tsk->oldgenbytes;

  /* Minor collection cycle */
  int rsetsize = array_count(tsk->remembered);
  int prev_newgenbytes = calc_newgenbytes(tsk);
  struct timeval start;
  struct timeval end;
  block *prevstart;
  unsigned int prevoffset;

  #ifdef CHECK_HEAP_INTEGRITY
  check_all_refs_in_eithergen(tsk);
  #endif

  gettimeofday(&start,NULL);

  begin_promotion(tsk,&prevstart,&prevoffset);

  /* Mark phase */
  clear_global_marks(tsk,FLAG_MARKED);
  mark_start(tsk,FLAG_MARKED);
  mark_roots(tsk,FLAG_MARKED);
  mark_incoming_global_refs(tsk,FLAG_MARKED);
  mark_mature_remoteref_globals(tsk,FLAG_MARKED);
  mark_remembered_set(tsk);
  mark_new_objects(tsk,tsk->newgen);
  mark_end(tsk,FLAG_MARKED);

  #ifdef CHECK_HEAP_INTEGRITY
  assert(check_oldgen_valid(tsk));
  #endif

  /* Copy phase */
  preserve_targets(tsk);
  sweep_globals(tsk);
  finish_promotion(tsk,prevstart,prevoffset);
  #ifdef CHECK_HEAP_INTEGRITY
  check_all_refs_in_oldgen(tsk);
  #endif

  tsk->need_minor = 0;
  gettimeofday(&end,NULL);

  int copiedbytes = tsk->oldgenbytes-prev_oldgenbytes;
  int survived = 0;
  if (prev_newgenbytes > 0)
    survived = (int)((100.0*(double)copiedbytes)/((double)prev_newgenbytes));

  node_log(tsk->n,LOG_INFO,"minor: %dms; %dkb of %dkb survived (%d%%); rset %d, oldgen now %dkb",
           timeval_diffms(start,end),
           copiedbytes/1024,
           prev_newgenbytes/1024,
           survived,
           rsetsize,
           tsk->oldgenbytes/1024);
  tsk->minorms += timeval_diffms(start,end);
  tsk->gcms += timeval_diffms(start,end);

  /* Major collection cycle */
  if (tsk->need_major) {
    if (tsk->skipremaining > 0) {
      tsk->skipremaining--;
      node_log(tsk->n,LOG_INFO,"Skipping major collection (%d skips remaining)",
               tsk->skipremaining);
    }
    else {

      assert(NULL == tsk->newgen);
      prev_oldgenbytes = tsk->oldgenbytes;
      gettimeofday(&start,NULL);
      /* Mark phase */
      clear_marks(tsk,FLAG_MARKED);

      block *prevgen = copy_heap_start(tsk);

      mark_start(tsk,FLAG_MARKED);
      mark_roots(tsk,FLAG_MARKED);
      mark_incoming_global_refs(tsk,FLAG_MARKED);
      mark_new_objects(tsk,prevgen);
      mark_end(tsk,FLAG_MARKED);

      /* Copy phase */
      preserve_targets(tsk);
      sweep_globals(tsk);
      copy_heap_finish(tsk,prevgen);
      #ifdef CHECK_HEAP_INTEGRITY
      check_all_refs_in_oldgen(tsk);
#if 0
      int new_inds = count_inds(tsk);
      fprintf(stderr,"new_inds = %d\n",new_inds);
      assert(0 == new_inds);
#endif
      #endif
      sweep_sysobjects(tsk,0);

      double survived = ((double)tsk->oldgenbytes)/((double)prev_oldgenbytes);

      /* Each time we have a major collection and at least 90% of the objects
         survive, increment the number of major collections that are skipped
         (up to a maximum of 8).
         If less than 90% survived, we reset this number to 0. */
      if (survived >= 0.9) {
        if (tsk->skipmajors < 4) {
          tsk->skipmajors++;
          node_log(tsk->n,LOG_INFO,"More than 90%% survived in a major collection; setting "
                   "skipmajors to %d",tsk->skipmajors);
        }
      }
      else {
        tsk->skipmajors = 0;
      }

      /* Skip the next n major collections */
      tsk->skipremaining = tsk->skipmajors;

      gettimeofday(&end,NULL);
      node_log(tsk->n,LOG_INFO,"MAJOR: %dms; %dkb of %dkb survived (%d%%)",
               timeval_diffms(start,end),
               tsk->oldgenbytes/1024,
               prev_oldgenbytes/1024,
               (int)(100.0*survived));
      tsk->majorms += timeval_diffms(start,end);
      tsk->gcms += timeval_diffms(start,end);
    }
    tsk->need_major = 0;
  }

  /* End */
  tsk->stats.gcs++;
}

void distributed_collect_start(task *tsk)
{
  /* Note: All marks that are done with FLAG_DMB do not cause a copy to occur, but instead
     the flag to be set, and any remote references encountered to have a MARKENTRY message
     sent to them. */


  force_major_collection(tsk);

  /* First do a local collection to make sure the remembered set and new generation are
     empty. Might not be strictly necessary but can simplify things if we can rely on
     this assumption. */
  local_collect(tsk);

  tsk->indcstart = 1;
  mark_start(tsk,FLAG_DMB);

  /* Clear the FLAG_DMB bit on all globals. When distributed collection ends, we will delete
     any globals that have neither FLAG_DMB or FLAG_MARKED set on them. */
/*BAD:    clear_global_marks(tsk,FLAG_DMB); */

  /* Mark roots (active frames etc.) */
  mark_roots(tsk,FLAG_DMB);

  /* Mark any remote addresses that are contained within messages that have yet to be
     acknowledged by their destination. */
  mark_inflight_remote_addresses(tsk);


  mark_end(tsk,FLAG_DMB);
  tsk->indcstart = 0;
}

static void sweep_incoming_references(task *tsk)
{
  global *glo;
  global *next;

  for (glo = tsk->globals.first; glo; glo = next) {
    next = glo->next;

    if (glo->addr.tid == tsk->tid) {
      assert(0 <= glo->addr.lid);

      if (!(glo->flags & FLAG_DMB) && !(glo->flags & FLAG_NEW)) {

        assert(!is_pntr(glo->p) || (get_pntr(glo->p)->flags & FLAG_MATURE));
        if (CELL_REMOTEREF == pntrtype(glo->p)) {
          global *other = pglobal(glo->p);
          assert(pntrequal(other->p,glo->p));
          if (other->addr.tid == tsk->tid) {
            /* We have a "remote reference" that points to a target on the local node, e.g. a
               frame that has been migrated (in which case the remoteref will be fetching). */
            continue;
          }
        }

        if ((CELL_REMOTEREF == pntrtype(glo->p)) && pglobal(glo->p)->fetching)
          event_keep_global(tsk,glo->addr);
        else if (is_pntr(glo->p) &&
                 ((get_pntr(glo->p)->flags & FLAG_DMB) ||
                  (get_pntr(glo->p)->flags & FLAG_NEW)))
          event_rescue_global(tsk,glo->addr);
        else
          remove_global(tsk,glo);
      }
    }
  }
}

static void reset_dmb(task *tsk)
{
  assert(NULL == tsk->newgen);
  block *bl;
  for (bl = tsk->oldgen; bl; bl = bl->next) {
    unsigned int off;
    for (off = BLOCK_START; off < BLOCK_END; off += object_size(off+(char*)bl)) {
      header *h = (header*)(off+(char*)bl);
      h->flags &= ~FLAG_DMB;
      h->flags &= ~FLAG_NEW;
    }
  }

  global *glo;
  for (glo = tsk->globals.first; glo; glo = glo->next) {
    glo->flags &= ~FLAG_DMB;
    glo->flags &= ~FLAG_NEW;
  }
}

void distributed_collect_end(task *tsk)
{
#ifdef DEBUG_DISTRIBUTION
  node_log(tsk->n,LOG_INFO,"distributed_collect_end");
  /* Force a major collection, so we can determine how much memory we're about to save */
  force_major_collection(tsk);
  local_collect(tsk);
  int oldmem = tsk->oldgenbytes;
#endif

  /* Get rid of any incoming global references that don't have the distributed mark bit set */
  mark_inflight_local_addresses(tsk,FLAG_DMB,1);
  sweep_incoming_references(tsk);

  /* Force a major collection */
  force_major_collection(tsk);
  local_collect(tsk);

  /* Reset the DMB on all objects in the heap */
  reset_dmb(tsk);

  clear_marks(tsk,FLAG_NEW);

#ifdef DEBUG_DISTRIBUTION
  int newmem = tsk->oldgenbytes;
  node_log(tsk->n,LOG_INFO,
           "Distributed garbage collection end: old %dkb new %dkb saved %dkb (%d bytes)",
           oldmem/1024,newmem/1024,(oldmem-newmem)/1024,oldmem-newmem);
#endif
}

cap *cap_alloc(task *tsk, int arity, int address, int fno, unsigned int datasize)
{
  unsigned int sizereq = sizeof(cap)+datasize*sizeof(pntr);
  if (0 != sizereq%8)
    sizereq += 8-(sizereq%8);
  cap *c = (cap*)alloc_mem(tsk,sizereq);
  c->type = CELL_O_CAP;
  c->nbytes = sizereq;
  c->arity = arity;
  c->address = address;
  c->fno = fno;
  assert(0 < c->arity); /* MKCAP should not be called for CAFs */

  #ifdef PROFILING
  tsk->stats.cap_allocs++;
  #endif

  return c;
}

frame *frame_new(task *tsk) /* Can be called from native code */
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

  f = tsk->freeframe;
  tsk->freeframe = f->freelnk;

  f->c = 0; /* should be set by caller */
  f->instr = 0; /* should be set by caller */

  f->state = STATE_NEW;
  f->resume = 0;
  f->freelnk = 0;
  f->retp = NULL;
  f->postponed = 0;
  f->sprev = NULL;
  f->snext = NULL;

  assert(NULL == f->wq.frames);
  assert(NULL == f->wq.fetchers);
  assert(NULL == f->waitlnk);
  assert(NULL == f->rnext);

  #ifdef PROFILING
  tsk->stats.frame_allocs++;
  #endif

  return f;
}
