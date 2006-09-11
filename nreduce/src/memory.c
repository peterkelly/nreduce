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

#include "grammar.tab.h"
#include "nreduce.h"
#include "gcode.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <assert.h>
#include <ctype.h>
#include <stdarg.h>
#include <math.h>
#include <pthread.h>

int trace = 0;

array *lexstring = NULL;
array *oldnames = NULL;
array *parsedfiles = NULL;

int _pntrtype(pntr p) { return pntrtype(p); }
const char *_pntrtname(pntr p) { return cell_types[pntrtype(p)]; }
global *_pglobal(pntr p) { return pglobal(p); }
frame *_pframe(pntr p) { return pframe(p); }
pntr _make_pntr(cell *c) { pntr p; make_pntr(p,c); return p; }

#ifndef INLINE_PTRFUNS
int is_pntr(pntr p)
{
  return (*(((unsigned int*)&p)+1) == 0xFFFFFFF1);
}

cell *get_pntr(pntr p)
{
  assert(is_pntr(p));
  return (cell*)(*((unsigned int*)&p));
}

int is_string(pntr p)
{
  return (*(((unsigned int*)&p)+1) == 0xFFFFFFF2);
}

char *get_string(pntr p)
{
  assert(is_string(p));
  return (char*)(*((unsigned int*)&p));
}

int pntrequal(pntr a, pntr b)
{
  return 
    ((*(((unsigned int*)&(a))+0) == *(((unsigned int*)&(b))+0)) &&
     (*(((unsigned int*)&(a))+1) == *(((unsigned int*)&(b))+1)));
}

int is_nullpntr(pntr p)
{
  return (is_pntr(p) && (NULL == get_pntr(p)));
}

#endif

#ifndef INLINE_RESOLVE_PNTR
pntr resolve_pntr(pntr p)
{
  while (TYPE_IND == pntrtype(p))
    p = get_pntr(p)->field1;
  return p;
}
#endif

#ifndef UNBOXED_NUMBERS
pntr make_number(double d)
{
  cell *v = alloc_cell(NULL);
  pntr p;
  v->type = TYPE_NUMBER;
  v->field1 = d;
  make_pntr(p,v);
  return p;
}
#endif

void initmem(void)
{
}

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
  for (i = 0; i < f->count; i++)
    if (!is_nullpntr(f->data[i]))
      mark(proc,f->data[i],bit);
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
  assert(TYPE_EMPTY != pntrtype(p));
  if (!is_pntr(p))
    return;
  c = get_pntr(p);
  if (c->flags & bit)
    return;
  c->flags |= bit;
  switch (pntrtype(p)) {
  case TYPE_IND:
    mark(proc,c->field1,bit);
    break;
  case TYPE_APPLICATION:
  case TYPE_CONS:
    c->field1 = resolve_pntr(c->field1);
    c->field2 = resolve_pntr(c->field2);
    mark(proc,c->field1,bit);
    mark(proc,c->field2,bit);
    break;
  case TYPE_AREF: {
    cell *arrholder = get_pntr(c->field1);
    carray *arr = (carray*)get_pntr(arrholder->field1);
    int index = (int)get_pntr(c->field2);
    assert(index < arr->size);
    assert(get_pntr(arr->refs[index]) == c);

    mark(proc,c->field1,bit);
    break;
  }
  case TYPE_ARRAY: {
    carray *arr = (carray*)get_pntr(c->field1);
    int i;
    for (i = 0; i < arr->size; i++) {
      mark(proc,arr->elements[i],bit);
      if (!is_nullpntr(arr->refs[i]))
        mark(proc,arr->refs[i],bit);
    }
    mark(proc,arr->tail,bit);
    break;
  }
  case TYPE_FRAME:
    mark_frame(proc,(frame*)get_pntr(c->field1),bit);
    break;
  case TYPE_CAP:
    mark_cap(proc,(cap*)get_pntr(c->field1),bit);
    break;
  case TYPE_REMOTEREF: {
    global *glo = (global*)get_pntr(c->field1);
    mark_global(proc,glo,bit);
    break;
  }
  case TYPE_BUILTIN:
  case TYPE_SCREF:
  case TYPE_NIL:
  case TYPE_NUMBER:
  case TYPE_STRING:
  case TYPE_HOLE:
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
  if (TYPE_REMOTEREF == pntrtype(glo->p)) {
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
  assert(TYPE_EMPTY != v->type);
  switch (celltype(v)) {
  case TYPE_STRING:
    free(get_string(v->field1));
    break;
  case TYPE_ARRAY: {
    carray *arr = (carray*)get_pntr(v->field1);
    free(arr->elements);
    free(arr->refs);
    free(arr);
    break;
  }
  case TYPE_FRAME: {
    frame *f = (frame*)get_pntr(v->field1);
    f->c = NULL;
    assert(proc->done || (STATE_DONE == f->state) || (STATE_NEW == f->state));
    frame_dealloc(proc,f);
    break;
  }
  case TYPE_CAP: {
    cap *cp = (cap*)get_pntr(v->field1);
    cap_dealloc(cp);
    break;
  }
  case TYPE_REMOTEREF:
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
      if (TYPE_EMPTY != bl->values[i].type)
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

  if (proc->streamstack) {
    for (i = 0; i < proc->streamstack->count; i++) {
      if (proc->memdebug) {
        fprintf(proc->output,"root: streamstack[%d]\n",i);
      }
      mark(proc,proc->streamstack->data[i],bit);
    }
  }

  for (f = proc->sparked.first; f; f = f->qnext) {
    mark_frame(proc,f,bit);
    assert(NULL == f->wq.frames);
    assert(NULL == f->wq.fetchers);
  }

  for (f = proc->runnable.first; f; f = f->qnext)
    mark_frame(proc,f,bit);

  for (f = proc->blocked.first; f; f = f->qnext)
    mark_frame(proc,f,bit);

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
      if (TYPE_EMPTY != bl->values[i].type) {
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
        if ((TYPE_EMPTY != bl->values[i].type) && (bl->values[i].flags & FLAG_NEW)) {
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
          (TYPE_EMPTY != bl->values[i].type)) {
        free_cell_fields(proc,&bl->values[i]);
        bl->values[i].type = TYPE_EMPTY;

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
/*   assert((NULL == f->c) || TYPE_FRAME != celltype(f->c)); */
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

void cleanup(void)
{
  int i;
  scomb_free_list(&scombs);

  if (lexstring)
    array_free(lexstring);
  if (oldnames) {
    for (i = 0; i < array_count(oldnames); i++)
      free(array_item(oldnames,i,char*));
    array_free(oldnames);
  }
  if (parsedfiles) {
    for (i = 0; i < array_count(parsedfiles); i++)
      free(array_item(parsedfiles,i,char*));
    array_free(parsedfiles);
  }
}

process *process_new(void)
{
  process *proc = (process*)calloc(1,sizeof(process));
  cell *globnilvalue;

  pthread_mutex_init(&proc->msglock,NULL);
  pthread_cond_init(&proc->msgcond,NULL);

  proc->stats.op_usage = (int*)calloc(OP_COUNT,sizeof(int));

  globnilvalue = alloc_cell(proc);
  globnilvalue->type = TYPE_NIL;
  globnilvalue->flags |= FLAG_PINNED;

  make_pntr(proc->globnilpntr,globnilvalue);
  proc->globtruepntr = make_number(1.0);

  if (is_pntr(proc->globtruepntr))
    get_pntr(proc->globtruepntr)->flags |= FLAG_PINNED;

  proc->pntrhash = (global**)calloc(GLOBAL_HASH_SIZE,sizeof(global*));
  proc->addrhash = (global**)calloc(GLOBAL_HASH_SIZE,sizeof(global*));

  return proc;
}

void process_init(process *proc)
{
  int i;
  bcheader *bch = (bcheader*)proc->bcdata;
  int count = bch->nstrings;
  int *stroffsets = bc_get_stroffsets(proc->bcdata);

  assert(NULL == proc->strings);
  assert(0 == proc->nstrings);
  assert(0 < proc->groupsize);


  proc->nstrings = count;
  proc->strings = (pntr*)malloc(count*sizeof(pntr));
  for (i = 0; i < count; i++) {
    cell *val = alloc_cell(proc);
    char *copy = strdup(proc->bcdata+stroffsets[i]);
    val->type = TYPE_STRING;
    val->flags |= FLAG_PINNED;
    make_string(val->field1,copy);
    make_pntr(proc->strings[i],val);
  }
  proc->stats.funcalls = (int*)calloc(bch->nfunctions,sizeof(int));
  proc->stats.usage = (int*)calloc(bch->nops,sizeof(int));
  proc->stats.framecompletions = (int*)calloc(bch->nfunctions,sizeof(int));
  proc->stats.sendcount = (int*)calloc(MSG_COUNT,sizeof(int));
  proc->stats.sendbytes = (int*)calloc(MSG_COUNT,sizeof(int));
  proc->stats.recvcount = (int*)calloc(MSG_COUNT,sizeof(int));
  proc->stats.recvbytes = (int*)calloc(MSG_COUNT,sizeof(int));
  proc->gcsent = (int*)calloc(proc->groupsize,sizeof(int));
  proc->distmarks = (array**)calloc(proc->groupsize,sizeof(array*));

  proc->inflight_addrs = (array**)calloc(proc->groupsize,sizeof(array*));
  proc->unack_msg_acount = (array**)calloc(proc->groupsize,sizeof(array*));
  for (i = 0; i < proc->groupsize; i++) {
    proc->inflight_addrs[i] = array_new(sizeof(gaddr));
    proc->unack_msg_acount[i] = array_new(sizeof(int));
    proc->distmarks[i] = array_new(sizeof(gaddr));
  }
}

static void message_free(message *msg)
{
  free(msg->data);
  free(msg);
}

void process_free(process *proc)
{
  int i;
  block *bl;
  int h;

  for (bl = proc->blocks; bl; bl = bl->next)
    for (i = 0; i < BLOCK_SIZE; i++)
      bl->values[i].flags &= ~(FLAG_PINNED | FLAG_MARKED | FLAG_DMB);

/*   local_collect(proc); */
  sweep(proc);

  bl = proc->blocks;
  while (bl) {
    block *next = bl->next;
    free(bl);
    bl = next;
  }

  for (h = 0; h < GLOBAL_HASH_SIZE; h++) {
    global *glo = proc->pntrhash[h];
    while (glo) {
      global *next = glo->pntrnext;
      free(glo);
      glo = next;
    }
  }

  for (i = 0; i < proc->groupsize; i++) {
    array_free(proc->inflight_addrs[i]);
    array_free(proc->unack_msg_acount[i]);
    array_free(proc->distmarks[i]);
  }

  free(proc->strings);
  free(proc->stats.funcalls);
  free(proc->stats.framecompletions);
  free(proc->stats.sendcount);
  free(proc->stats.sendbytes);
  free(proc->stats.recvcount);
  free(proc->stats.recvbytes);
  free(proc->stats.usage);
  free(proc->stats.op_usage);
  free(proc->gcsent);
  free(proc->distmarks);
  free(proc->pntrhash);
  free(proc->addrhash);

  free(proc->inflight_addrs);
  free(proc->unack_msg_acount);

  list_free(proc->msgqueue,(list_d_t)message_free);
  pthread_mutex_destroy(&proc->msglock);
  pthread_cond_destroy(&proc->msgcond);
  free(proc);
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
  case TYPE_NUMBER:
    print_double(f,p);
    break;
  case TYPE_STRING: {
    char *str = get_string(get_pntr(p)->field1);
    fprintf(f,"\"%s\"",str);
    break;
  }
  case TYPE_NIL:
    fprintf(f,"nil");
    break;
  case TYPE_FRAME: {
    frame *fr = (frame*)get_pntr(get_pntr(p)->field1);
    char *name = get_function_name(fr->fno);
    fprintf(f,"frame(%s/%d)",name,fr->count);
    free(name);
    break;
  }
  case TYPE_REMOTEREF: {
    global *glo = (global*)get_pntr(get_pntr(p)->field1);
    fprintf(f,"%d@%d",glo->addr.lid,glo->addr.pid);
    break;
  }
  case TYPE_IND: {
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

