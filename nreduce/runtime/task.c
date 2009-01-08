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

#define TASK_C

#include "compiler/bytecode.h"
#include "src/nreduce.h"
#include "compiler/source.h"
#include "runtime/runtime.h"
#include "modules/modules.h"
#include "messages.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <assert.h>
#include <ctype.h>
#include <stdarg.h>
#include <math.h>
#include <errno.h>
#include <unistd.h>

extern const module_info *modules;

pthread_key_t task_key;
// FIXME: make sure these are set correctly
int engine_type = ENGINE_NATIVE;
int strict_evaluation = 1;

global *targethash_lookup(task *tsk, pntr p)
{
  int h = hash(&p,sizeof(pntr));
  global *g = tsk->targethash[h];
  while (g && !pntrequal(g->p,p))
    g = g->targetnext;
  return g;
}

global *physhash_lookup(task *tsk, pntr p)
{
  int h = hash(&p,sizeof(pntr));
  global *g = tsk->physhash[h];
  while (g && !pntrequal(g->p,p))
    g = g->physnext;
  return g;
}

global *addrhash_lookup(task *tsk, gaddr addr)
{
  int h = hash(&addr,sizeof(gaddr));
  global *g = tsk->addrhash[h];
  while (g && ((g->addr.tid != addr.tid) || (g->addr.lid != addr.lid)))
    g = g->addrnext;
  return g;
}

void targethash_add(task *tsk, global *glo)
{
  int h = hash(&glo->p,sizeof(pntr));
  assert(NULL == glo->targetnext);
  assert(NULL == targethash_lookup(tsk,glo->p));
  glo->targetnext = tsk->targethash[h];
  tsk->targethash[h] = glo;
}

void physhash_add(task *tsk, global *glo)
{
  int h = hash(&glo->p,sizeof(pntr));
  assert(NULL == glo->physnext);
  assert(NULL == physhash_lookup(tsk,glo->p));
  glo->physnext = tsk->physhash[h];
  tsk->physhash[h] = glo;
}

void addrhash_add(task *tsk, global *glo)
{
  int h = hash(&glo->addr,sizeof(gaddr));
  assert(NULL == glo->addrnext);
  assert(0 <= glo->addr.lid);
  assert(NULL == addrhash_lookup(tsk,glo->addr));
  glo->addrnext = tsk->addrhash[h];
  tsk->addrhash[h] = glo;
}

void targethash_remove(task *tsk, global *glo)
{
  int h = hash(&glo->p,sizeof(pntr));
  global **ptr = &tsk->targethash[h];

  assert(glo);
  while (*ptr && (*ptr != glo))
    ptr = &(*ptr)->targetnext;
  if (*ptr == glo) {
    *ptr = glo->targetnext;
    glo->targetnext = NULL;
  }
}

void physhash_remove(task *tsk, global *glo)
{
  int h = hash(&glo->p,sizeof(pntr));
  global **ptr = &tsk->physhash[h];

  assert(glo);
  while (*ptr && (*ptr != glo))
    ptr = &(*ptr)->physnext;
  if (*ptr == glo) {
    *ptr = glo->physnext;
    glo->physnext = NULL;
  }
}

void addrhash_remove(task *tsk, global *glo)
{
  int h = hash(&glo->addr,sizeof(gaddr));
  global **ptr = &tsk->addrhash[h];

  assert(glo);
  while (*ptr && (*ptr != glo))
    ptr = &(*ptr)->addrnext;
  if (*ptr == glo) {
    *ptr = glo->addrnext;
    glo->addrnext = NULL;
  }
}

/* Returns the physical address associated with the object p, creating a new GAT entry if needed */
gaddr get_physical_address(task *tsk, pntr p)
{
  global *glo = physhash_lookup(tsk,p);
  if (NULL == glo) {
    glo = (global*)calloc(1,sizeof(global));
    llist_append(&tsk->globals,glo);
    glo->addr.tid = tsk->tid;
    glo->addr.lid = tsk->nextlid++;
    node_log(tsk->n,LOG_DEBUG2,"Added new global with address %d@%d (get_physical_address)",
             glo->addr.lid,glo->addr.tid);
    glo->p = p;
    glo->flags = tsk->indistgc ? FLAG_NEW : 0;
    physhash_add(tsk,glo);
    addrhash_add(tsk,glo);
  }
  else {
    assert(glo->addr.tid == tsk->tid);
    assert(glo->addr.tid >= 0);
    assert(pntrequal(glo->p,p));
    assert(addrhash_lookup(tsk,glo->addr) == glo);
  }
  return glo->addr;
}

global *add_target(task *tsk, gaddr addr, pntr p)
{
  global *glo;
  assert(addr.tid != tsk->tid);
  assert(NULL == addrhash_lookup(tsk,addr));
  assert(NULL == targethash_lookup(tsk,p));
  glo = (global*)calloc(1,sizeof(global));
  llist_append(&tsk->globals,glo);
  glo->addr = addr;
  node_log(tsk->n,LOG_DEBUG2,"Added new global with address %d@%d (add_target)",
           glo->addr.lid,glo->addr.tid);
  glo->p = p;
  glo->flags = tsk->indistgc ? FLAG_NEW : 0;
  /* Can't store targets with lid < 0 in addrhash, since there may be more than one of them */
  if (0 <= addr.lid)
    addrhash_add(tsk,glo);
  targethash_add(tsk,glo);
  return glo;
}

void add_gaddr(list **l, gaddr addr)
{
  if ((-1 != addr.tid) || (-1 != addr.lid)) {
    gaddr *copy = (gaddr*)malloc(sizeof(gaddr));
    memcpy(copy,&addr,sizeof(gaddr));
    list_push(l,copy);
  }
}

/* note: we only remove the first instance */
void remove_gaddr(task *tsk, list **l, gaddr addr)
{
  while (*l) {
    gaddr *item = (gaddr*)(*l)->data;
    if ((item->tid == addr.tid) && (item->lid == addr.lid)) {
      list *old = *l;
      *l = (*l)->next;
      free(old->data);
      free(old);
      return;
    }
    else {
      l = &((*l)->next);
    }
  }
  fatal("gaddr %d@%d not found",addr.lid,addr.tid);
}

void append_spark(task *tsk, frame *f)
{
  assert(NULL == f->sprev);
  assert(NULL == f->snext);

  frame *last = tsk->sparklist->sprev;
  f->snext = tsk->sparklist;
  f->sprev = last;
  last->snext = f;
  tsk->sparklist->sprev = f;
}

void prepend_spark(task *tsk, frame *f)
{
  assert(NULL == f->sprev);
  assert(NULL == f->snext);

  frame *first = tsk->sparklist->snext;
  f->sprev = tsk->sparklist;
  f->snext = first;
  first->sprev = f;
  tsk->sparklist->snext = f;
}

void remove_spark(task *tsk, frame *f)
{
  assert(NULL != f->sprev);
  assert(NULL != f->snext);
  assert(f->sprev->snext == f);
  assert(f->snext->sprev == f);

  frame *prev = f->sprev;
  frame *next = f->snext;
  prev->snext = next;
  next->sprev = prev;

  f->sprev = NULL;
  f->snext = NULL;
}

void check_sparks(task *tsk)
{
  frameblock *fb;
  int i;
  for (fb = tsk->frameblocks; fb; fb = fb->next) {
    for (i = 0; i < tsk->framesperblock; i++) {
      frame *f = ((frame*)&fb->mem[i*tsk->framesize]);
      if (f == tsk->sparklist)
        continue;
      if (STATE_SPARKED == f->state) {
        assert(NULL != f->sprev);
        assert(NULL != f->snext);
        assert(f->sprev->snext == f);
        assert(f->snext->sprev == f);
      }
      else {
        assert(NULL == f->sprev);
        assert(NULL == f->snext);
      }
    }
  }

  frame *f;
  for (f = tsk->sparklist->snext; f != tsk->sparklist; f = f->snext) {
    assert(STATE_SPARKED == f->state);
    assert(NULL != f->sprev);
    assert(NULL != f->snext);
    assert(f->sprev->snext == f);
    assert(f->snext->sprev == f);
  }


  /* Check that all "nolocal" sparked frames are at the end of the list */
  int in_nolocal = 0;
  for (f = tsk->sparklist->snext; f != tsk->sparklist; f = f->snext) {
    if (f->nolocal)
      in_nolocal = 1;
    if (in_nolocal)
      assert(f->nolocal);
  }
}

void spark_frame(task *tsk, frame *f)
{
  assert(f != tsk->sparklist);
  if (STATE_NEW == f->state) {
    f->state = STATE_SPARKED;
    prepend_spark(tsk,f);
  }
}

void unspark_frame(task *tsk, frame *f)
{
  assert(f != tsk->sparklist);
  assert((STATE_SPARKED == f->state) || (STATE_NEW == f->state));
  if (STATE_SPARKED == f->state)
    remove_spark(tsk,f);
  f->state = STATE_NEW;
  assert((NULL == f->c) || (CELL_FRAME == celltype(f->c)));
  assert((NULL == f->c) || (f == (frame*)get_pntr(f->c->field1)));
}

void run_frame(task *tsk, frame *f)
{
  assert(f != tsk->sparklist);
  switch (f->state) {
  case STATE_SPARKED:
    remove_spark(tsk,f);
    /* fall through */
  case STATE_NEW:
    f->rnext = *tsk->runptr;
    f->state = STATE_ACTIVE;
    *tsk->runptr = f;

    #ifdef PROFILING
    if (0 <= frame_fno(tsk,f))
      tsk->stats.funcalls[frame_fno(tsk,f)]++;
    #endif
    break;
  default:
    break;
  }
}

/* Used when called from a function that may be invoked within handle_interrupt, since
   we can't change the current frame if one already exists */
void run_frame_after_first(task *tsk, frame *f)
{
  assert(f != tsk->sparklist);
  switch (f->state) {
  case STATE_SPARKED:
    remove_spark(tsk,f);
    /* fall through */
  case STATE_NEW: {
    if (*tsk->runptr) {
      f->rnext = (*tsk->runptr)->rnext;
      (*tsk->runptr)->rnext = f;
    }
    else {
      f->rnext = *tsk->runptr;
      *tsk->runptr = f;
    }
    f->state = STATE_ACTIVE;

    #ifdef PROFILING
    if (0 <= frame_fno(tsk,f))
      tsk->stats.funcalls[frame_fno(tsk,f)]++;
    #endif
    break;
  }
  default:
    break;
  }
}

void check_runnable(task *tsk)
{
  assert((void*)1 != *tsk->runptr);
  if (NULL == *tsk->runptr)
    endpoint_interrupt(tsk->endpt);
}

void block_frame(task *tsk, frame *f)
{
  frame *next;
  assert(STATE_ACTIVE == f->state);
  assert(f == *tsk->runptr);

  next = f->rnext;
  f->rnext = NULL;
  *tsk->runptr = next;
}

void unblock_frame(task *tsk, frame *f)
{
  assert(STATE_ACTIVE == f->state);
  assert(NULL == f->rnext);
  f->rnext = *tsk->runptr;
  *tsk->runptr = f;
}

/* Used when called from a function that may be invoked within handle_interrupt, since
   we can't change the current frame if one already exists */
void unblock_frame_after_first(task *tsk, frame *f)
{
  assert(STATE_ACTIVE == f->state);
  assert(NULL == f->rnext);

  if (*tsk->runptr) {
    f->rnext = (*tsk->runptr)->rnext;
    (*tsk->runptr)->rnext = f;
  }
  else {
    f->rnext = *tsk->runptr;
    *tsk->runptr = f;
  }
}

int frame_fno(task *tsk, frame *f)
{
  const instruction *program_ops = bc_instructions(tsk->bcdata);
  int bcaddr = f->instr-program_ops;
  return tsk->bcaddr_to_fno[bcaddr];
}

const char *frame_fname(task *tsk, frame *f)
{
  return bc_function_name(tsk->bcdata,frame_fno(tsk,f));
}

int set_error(task *tsk, const char *format, ...)
{
  va_list ap;
  int len;
  const instruction *instr;
  va_start(ap,format);
  len = vsnprintf(NULL,0,format,ap);
  va_end(ap);

  assert(NULL == tsk->error);
  tsk->error = (char*)malloc(len+1);
  va_start(ap,format);
  len = vsnprintf(tsk->error,len+1,format,ap);
  va_end(ap);

  if (NULL == *tsk->runptr) {
    /* reduction engine */
  }
  else {
    /* bytecode interpreter */
    frame *f = *tsk->runptr;
    instr = f->instr-1;
    tsk->errorsl.fileno = instr->fileno;
    tsk->errorsl.lineno = instr->lineno;

    (*tsk->runptr)->instr = bc_instructions(tsk->bcdata)+((bcheader*)tsk->bcdata)->erroraddr;
    tsk->endpt->rc = 1;
  }
  return 0;
}

task *task_new(int tid, int groupsize, const char *bcdata, int bcsize, array *args, node *n,
               socketid out_sockid, endpointid *epid)
{
  task *tsk = (task*)calloc(1,sizeof(task));
  cell *globnilvalue;
  int i;
  bcheader *bch;
  const funinfo *finfo;
  int fno;
  int cur;
  int maxstack = 0;
  int argc = array_count(args);

  if (epid)
    memset(epid,0,sizeof(*epid));

  tsk->n = n;
  tsk->runptr = &tsk->rtemp;
  tsk->freeptr = (cell*)1;
  tsk->oldgenoffset = BLOCK_START;
  tsk->newgenoffset = sizeof(block);
  tsk->altspace = FLAG_ALTSPACE;
  tsk->lifetimes = array_new(sizeof(unsigned int),0);

  if (0 > pipe(tsk->threadrunningfds))
    fatal("pipe: %s",strerror(errno));

  globnilvalue = alloc_cell(tsk);
  globnilvalue->type = CELL_NIL;

  make_pntr(tsk->globnilpntr,globnilvalue);
  set_pntrdouble(tsk->globtruepntr,1.0);

  tsk->targethash = (global**)calloc(GLOBAL_HASH_SIZE,sizeof(global*));
  tsk->physhash = (global**)calloc(GLOBAL_HASH_SIZE,sizeof(global*));
  tsk->addrhash = (global**)calloc(GLOBAL_HASH_SIZE,sizeof(global*));
  tsk->idmap = (endpointid*)calloc(groupsize,sizeof(endpointid));

  tsk->ioalloc = 1;
  tsk->iocount = 1;
  tsk->ioframes = (ioframe*)calloc(tsk->ioalloc,sizeof(ioframe));
  tsk->iofree = 0;

  tsk->tid = tid;
  tsk->groupsize = groupsize;

  tsk->argsp = tsk->globnilpntr;
  if (0 < argc) {
    carray *arr = carray_new(tsk,sizeof(pntr),0,NULL,NULL);
    make_pntr(tsk->argsp,arr->wrapper);
    for (i = 0; i < argc; i++) {
      pntr p = string_to_array(tsk,array_item(args,i,char*));
      carray_append(tsk,&arr,&p,1,sizeof(pntr));
    }
  }

  if (NULL == bcdata)
    return tsk; /* no bytecode; we must be using the reduction engine */

  tsk->bcdata = (char*)malloc(bcsize);
  memcpy(tsk->bcdata,bcdata,bcsize);
  tsk->bcsize = bcsize;

  bch = (bcheader*)tsk->bcdata;
  finfo = bc_funinfo(tsk->bcdata);
  for (fno = 0; fno < bch->nfunctions; fno++)
    if (maxstack < finfo[fno].stacksize)
      maxstack = finfo[fno].stacksize;

  tsk->bcaddr_to_fno = (int*)malloc(bch->nops*sizeof(int));
  for (i = 0; i < bch->nops; i++)
    tsk->bcaddr_to_fno[i] = -1;
  for (fno = 0; fno < bch->nfunctions; fno++)
    tsk->bcaddr_to_fno[finfo[fno].address] = fno;
  cur = -1;
  for (i = 0; i < bch->nops; i++) {
    if (0 <= tsk->bcaddr_to_fno[i])
      cur = tsk->bcaddr_to_fno[i];
    tsk->bcaddr_to_fno[i] = cur;
  }

  tsk->framesize = sizeof(frame)+maxstack*sizeof(pntr);
  tsk->framesperblock = FRAMEBLOCK_SIZE/tsk->framesize;
  tsk->remembered = array_new(sizeof(cell*),1024);

  assert(NULL == tsk->strings);
  assert(0 == tsk->nstrings);
  assert(0 < tsk->groupsize);

  tsk->nstrings = bch->nstrings;
  tsk->strings = (pntr*)malloc(bch->nstrings*sizeof(pntr));
  for (i = 0; i < bch->nstrings; i++)
    tsk->strings[i] = string_to_array(tsk,bc_string(tsk->bcdata,i));
  tsk->stats.funcalls = (int*)calloc(bch->nfunctions,sizeof(int));
  tsk->stats.frames = (int*)calloc(bch->nfunctions,sizeof(int));
  tsk->stats.caps = (int*)calloc(bch->nfunctions,sizeof(int));
  tsk->stats.usage = (int*)calloc(bch->nops,sizeof(int));
  tsk->gcsent = (int*)calloc(tsk->groupsize,sizeof(int));
  tsk->distmarks = (array**)calloc(tsk->groupsize,sizeof(array*));

  tsk->inflight_addrs = (array**)calloc(tsk->groupsize,sizeof(array*));
  tsk->unack_msg_acount = (array**)calloc(tsk->groupsize,sizeof(array*));
  for (i = 0; i < tsk->groupsize; i++) {
    tsk->inflight_addrs[i] = array_new(sizeof(gaddr),0);
    tsk->unack_msg_acount[i] = array_new(sizeof(int),0);
    tsk->distmarks[i] = array_new(sizeof(gaddr),0);
  }

  assert(!socketid_isnull(&out_sockid));
  tsk->out_sockid = out_sockid;
  tsk->out_so = new_sysobject(tsk,SYSOBJECT_CONNECTION);
  tsk->out_so->hostname = strdup("client");
  tsk->out_so->port = -1;
  tsk->out_so->sockid = tsk->out_sockid;
  tsk->out_so->connected = 1;

  if (n) {
    char semdata = 0;
    *epid = node_add_thread(n,"task",interpreter_thread,tsk,NULL);
    read(tsk->threadrunningfds[0],&semdata,1);
    close(tsk->threadrunningfds[0]);
    close(tsk->threadrunningfds[1]);
    tsk->threadrunningfds[0] = -1;
    tsk->threadrunningfds[1] = -1;
  }

  /* Create sentinel node for spark list */
  tsk->sparklist = frame_new(tsk);
  tsk->sparklist->snext = tsk->sparklist;
  tsk->sparklist->sprev = tsk->sparklist;
  tsk->sparklist->instr = bc_instructions(tsk->bcdata);
  tsk->sparklist->c = alloc_cell(tsk);
  tsk->sparklist->c->type = CELL_FRAME;
  make_pntr(tsk->sparklist->c->field1,tsk->sparklist);

  return tsk;
}

void task_free(task *tsk)
{
  int i;

  sweep_sysobjects(tsk,1);

#if 0
  int h;
  /* FIXME */
  sweep(tsk,1);

  /* Make sure all globals are deleted; this should be handled by sweep */
  for (h = 0; h < GLOBAL_HASH_SIZE; h++) {
    assert(NULL == tsk->addrhash[h]);
    assert(NULL == tsk->targethash[h]);
    assert(NULL == tsk->physhash[h]);
  }
  assert(NULL == tsk->globals.first);
  assert(NULL == tsk->globals.last);
#endif

  free_blocks(tsk->oldgen);
  free_blocks(tsk->newgen);

  free(tsk->idmap);
  free(tsk->ioframes);

  for (i = 0; i < tsk->groupsize; i++) {
    array_free(tsk->inflight_addrs[i]);
    array_free(tsk->unack_msg_acount[i]);
    array_free(tsk->distmarks[i]);
  }

  while (tsk->frameblocks) {
    frameblock *next = tsk->frameblocks->next;
    free(tsk->frameblocks);
    tsk->frameblocks = next;
  }

  if (tsk->remembered)
    array_free(tsk->remembered);

  list_free(tsk->wakeup_after_collect,free);
  free(tsk->lifetimes);
  free(tsk->strings);
  free(tsk->stats.funcalls);
  free(tsk->stats.frames);
  free(tsk->stats.caps);
  free(tsk->stats.usage);
  free(tsk->gcsent);
  free(tsk->error);
  free(tsk->distmarks);
  free(tsk->targethash);
  free(tsk->physhash);
  free(tsk->addrhash);

  free(tsk->inflight_addrs);
  free(tsk->unack_msg_acount);

  free(tsk->code);
  free(tsk->bcaddr_to_fno);
  free(tsk->bcdata);

  free(tsk->bpswapin[0]);
  free(tsk->bpswapin[1]);
  free(tsk->bpaddrs[0]);
  free(tsk->bpaddrs[1]);
  free(tsk->cpu_to_bcaddr);

  free(tsk);
}

void print_pntr(task *tsk, array *arr, pntr p, int depth)
{
  p = resolve_pntr(p);
  switch (pntrtype(p)) {
  case CELL_AREF: {
    char *str = NULL;
    if (0 <= array_to_string(p,&str)) {
      char *esc = escape(str);
      array_printf(arr,"\"%s\"",esc);
      free(esc);
      free(str);
    }
    else {
      array_printf(arr,"%s",cell_types[pntrtype(p)]);
    }
    break;
  }
  case CELL_NUMBER: {
    char tmp[100];
    format_double(tmp,100,pntrdouble(p));
    array_printf(arr,"%s",tmp);
    break;
  }
  case CELL_FRAME: {
    frame *f = pframe(p);
    int fno = frame_fno(tsk,f);
    if (depth > 0) {
      int i;
      if (0 < f->instr->expcount)
        array_printf(arr,"(");
      array_printf(arr,"%s",bc_function_name(tsk->bcdata,fno));
      for (i = f->instr->expcount-1; i >= 0; i--) {
        array_printf(arr," ");
        print_pntr(tsk,arr,f->data[i],depth-1);
      }
      if (0 < f->instr->expcount)
        array_printf(arr,")");
    }
    else {
      array_printf(arr,"(%s ...)",bc_function_name(tsk->bcdata,fno));
    }
    break;
  }
  case CELL_CAP: {
    cell *capval = get_pntr(p);
    cap *cp = (cap*)get_pntr(capval->field1);
    int i;
    if (0 < cp->count)
      array_printf(arr,"(");
    array_printf(arr,"%s",bc_function_name(tsk->bcdata,cp->fno));
    for (i = cp->count-1; i >= 0; i--) {
      array_printf(arr," ");
      print_pntr(tsk,arr,cp->data[i],depth-1);
    }
    if (0 < cp->count)
      array_printf(arr,")");
    break;
  }
  case CELL_REMOTEREF: {
    global *target = pglobal(p);
    array_printf(arr,"%d@%d",target->addr.lid,target->addr.tid);
    break;
  }
  case CELL_CONS: {
    if (depth > 0) {
      cell *c = get_pntr(p);
      array_printf(arr,"(cons ");
      print_pntr(tsk,arr,c->field1,depth-1);
      array_printf(arr," ");
      print_pntr(tsk,arr,c->field2,depth-1);
      array_printf(arr,")");
    }
    else {
      array_printf(arr,"CONS");
    }
    break;
  }
  case CELL_NIL:
    array_printf(arr,"nil");
    break;
  default:
    array_printf(arr,"%s",cell_types[pntrtype(p)]);
    break;
  }
}

char *pntr_to_string(task *tsk, pntr p)
{
  array *arr = array_new(1,0);
  char *ret;
  print_pntr(tsk,arr,p,1);
  ret = strdup(arr->data);
  array_free(arr);
  return ret;
}

static array *get_lines(const char *data, int len)
{
  int pos;
  int start;
  array *lines;
  lines = array_new(sizeof(char*),0);

  start = 0;
  for (pos = 0; pos <= len; pos++) {
    if ((pos == len) || ('\n' == data[pos])) {
      char *line = malloc(pos-start+1);
      memcpy(line,&data[start],pos-start);
      line[pos-start] = '\0';
      array_append(lines,&line,sizeof(char*));
      start = pos+1;
    }
  }

  return lines;
}

static array *get_file_lines(const char *filename)
{
  array *buf;
  array *lines;

  if (NULL == (buf = read_file(filename)))
    return NULL;

  lines = get_lines(buf->data,buf->nbytes);
  array_free(buf);
  return lines;
}

typedef struct usage_info {
  int usage;
  int fno;
} usage_info;

int usage_info_compar(const void *a, const void *b)
{
  const usage_info *ua = (const usage_info*)a;
  const usage_info *ub = (const usage_info*)b;
  return (ub->usage - ua->usage);
}

void print_profile(task *tsk)
{
  const bcheader *bch = (const bcheader*)tsk->bcdata;
  const instruction *instructions = bc_instructions(tsk->bcdata);
  FILE *f;
  int addr;
  int maxfileno = -1;
  int fileno;
  const char **filenames;
  array **lines;
  int *nlines;
  int **lineusage;
  int nofileusage = 0;
  int totalusage = 0;
  usage_info opusage[OP_COUNT];
  usage_info bifusage[NUM_BUILTINS];
  usage_info *fusage = (usage_info*)calloc(bch->nfunctions,sizeof(usage_info));
  int fno = 0;
  int i;

  memset(opusage,0,OP_COUNT*sizeof(usage_info));
  memset(bifusage,0,NUM_BUILTINS*sizeof(usage_info));

  if (NULL == (f = fopen(PROFILE_FILENAME,"w"))) {
    perror(PROFILE_FILENAME);
    exit(1);
  }

  for (addr = 0; addr < bch->nops; addr++)
    if (maxfileno < instructions[addr].fileno)
      maxfileno = instructions[addr].fileno;
  assert(maxfileno <= 1024);

  filenames = (const char**)calloc(maxfileno+1,sizeof(const char *));
  lines = (array**)calloc(maxfileno+1,sizeof(array*));
  nlines = (int*)calloc(maxfileno+1,sizeof(int));
  lineusage = (int**)calloc(maxfileno+1,sizeof(int*));
  for (fileno = 0; fileno <= maxfileno; fileno++) {
    filenames[fileno] = bc_string(tsk->bcdata,fileno);

    if (!strncmp(filenames[fileno],
                 MODULE_FILENAME_PREFIX,
                 strlen(MODULE_FILENAME_PREFIX))) {
      const module_info *mod;
      int found = 0;
      for (mod = modules; mod->name && !found; mod++) {
        if (!strcmp(mod->filename,filenames[fileno])) {
          lines[fileno] = get_lines(mod->source,strlen(mod->source));
          found = 1;
          break;
        }
      }
      assert(found);
    }
    else {
      lines[fileno] = get_file_lines(filenames[fileno]);
    }

    if (NULL == lines[fileno])
      exit(1);
    nlines[fileno] = array_count(lines[fileno]);
    lineusage[fileno] = (int*)calloc(nlines[fileno]+1,sizeof(int));
  }

  for (addr = 0; addr < bch->nops; addr++) {
    int fileno = instructions[addr].fileno;
    int lineno = instructions[addr].lineno;

    if (OP_GLOBSTART == instructions[addr].opcode) {
      fno = instructions[addr].arg0;
      assert(0 <= fno);
    }
    else if (addr >= bch->evaldoaddr) {
      fno = -1;
    }

    if (0 <= fno)
      fusage[fno].usage += tsk->stats.usage[addr];

    if (0 <= fileno) {
      assert(0 <= lineno);
      assert(lineno < nlines[fileno]);
      lineusage[fileno][lineno] += tsk->stats.usage[addr];
    }
    else if ((0 > fno) || (NUM_BUILTINS <= fno)) {
      nofileusage += tsk->stats.usage[addr];
    }
    totalusage += tsk->stats.usage[addr];
  }

  assert(totalusage == tsk->stats.ninstrs);

  for (fno = 0; fno < bch->nfunctions; fno++)
    fusage[fno].fno = fno;

  qsort(fusage,bch->nfunctions,sizeof(usage_info),usage_info_compar);

  fprintf(f,"================================================================================\n");
  fprintf(f,"Overall statistics\n");
  fprintf(f,"================================================================================\n");
  fprintf(f,"\n");
  fprintf(f,"Instructions           %d\n",totalusage);
  fprintf(f,"Cell allocations       %d\n",tsk->stats.cell_allocs);
  fprintf(f,"Total bytes allocated  %d\n",tsk->stats.total_bytes);
  fprintf(f,"Array allocations      %d\n",tsk->stats.array_allocs);
  fprintf(f,"Array resizes          %d\n",tsk->stats.array_resizes);
  fprintf(f,"FRAME allocations      %d\n",tsk->stats.frame_allocs);
  fprintf(f,"CAP allocations        %d\n",tsk->stats.cap_allocs);
  fprintf(f,"Garbage collections    %d\n",tsk->stats.gcs);

  fprintf(f,"\n");
  fprintf(f,"================================================================================\n");
  fprintf(f,"Function usage\n");
  fprintf(f,"================================================================================\n");
  fprintf(f,"\n");
  fprintf(f,"%8s %7s %8s %8s %8s %s\n","Instrs","%Instrs","Calls","FRAMEs","CAPs","Function name");
  fprintf(f,"%8s %7s %8s %8s %8s %s\n","------","-------","-----","------","----","-------------");
  for (i = 0; i < bch->nfunctions; i++) {
    int usage = fusage[i].usage;
    double proportion = ((double)usage)/((double)totalusage);
    double pct = 100.0*proportion;
    int thisfno = fusage[i].fno;
    int funcalls = tsk->stats.funcalls[thisfno];
    int frames = tsk->stats.frames[thisfno];
    int caps = tsk->stats.caps[thisfno];

    if ((0 < usage) || (0 < funcalls) || (0 < frames) || (0 < caps)) {
      fprintf(f,"%8d %6.2f%% %8d %8d %8d %s\n",
              usage,pct,funcalls,frames,caps,bc_function_name(tsk->bcdata,thisfno));
    }
  }

  fprintf(f,"\n");
  fprintf(f,"================================================================================\n");
  fprintf(f,"Opcode usage\n");
  fprintf(f,"================================================================================\n");
  fprintf(f,"\n");

  for (i = 0; i < OP_COUNT; i++)
    opusage[i].fno = i;

  for (addr = 0; addr < bch->nops; addr++)
    opusage[instructions[addr].opcode].usage += tsk->stats.usage[addr];

  qsort(opusage,OP_COUNT,sizeof(usage_info),usage_info_compar);

  for (i = 0; i < OP_COUNT; i++) {
    int usage = opusage[i].usage;
    int opcode = opusage[i].fno;
    double proportion = ((double)usage)/((double)totalusage);
    double pct = 100.0*proportion;
    fprintf(f,"%8d %6.2f%% %s\n",usage,pct,opcodes[opcode]);
  }

  fprintf(f,"\n");
  fprintf(f,"================================================================================\n");
  fprintf(f,"Built-in function usage\n");
  fprintf(f,"================================================================================\n");
  fprintf(f,"\n");

  for (i = 0; i < NUM_BUILTINS; i++)
    bifusage[i].fno = i;

  for (addr = 0; addr < bch->nops; addr++)
    if (OP_BIF == instructions[addr].opcode)
      bifusage[instructions[addr].arg0].usage += tsk->stats.usage[addr];

  qsort(bifusage,NUM_BUILTINS,sizeof(usage_info),usage_info_compar);

  for (i = 0; i < NUM_BUILTINS; i++) {
    int usage = bifusage[i].usage;
    int bif = bifusage[i].fno;
    double proportion = ((double)usage)/((double)totalusage);
    double pct = 100.0*proportion;
    if (0 == usage)
      break;
    fprintf(f,"%8d %6.2f%% %s\n",usage,pct,builtin_info[bif].name);
  }

  for (fileno = 0; fileno <= maxfileno; fileno++) {
    int lineno;
    fprintf(f,"\n");
    fprintf(f,"================================================================================\n");
    fprintf(f,"Source file: %s\n",filenames[fileno]);
    fprintf(f,"================================================================================\n");
    fprintf(f,"\n");
    for (lineno = 0; lineno < nlines[fileno]; lineno++) {
      int usage = lineusage[fileno][lineno+1];
      double proportion = ((double)usage)/((double)totalusage);
      double pct = 100.0*proportion;
      char *line = array_item(lines[fileno],lineno,char*);

      if (0 < usage)
        fprintf(f,"%8d %6.2f%% %s\n",usage,pct,line);
      else
        fprintf(f,"                 %s\n",line);
    }
  }
  if (0 < nofileusage) {
    double proportion = ((double)nofileusage)/((double)totalusage);
    double pct = 100.0*proportion;
    fprintf(f,"\n");
    fprintf(f,"Instruction executions not associated with a file: %d (%.2f%%)\n",nofileusage,pct);
  }

  for (fileno = 0; fileno <= maxfileno; fileno++) {
    for (i = 0; i < nlines[fileno]; i++)
      free(array_item(lines[fileno],i,char*));
    array_free(lines[fileno]);
    free(lineusage[fileno]);
  }
  free(filenames);
  free(lines);
  free(nlines);
  free(lineusage);
  free(fusage);

  fclose(f);

  printf("Profile written to %s\n",PROFILE_FILENAME);
}
