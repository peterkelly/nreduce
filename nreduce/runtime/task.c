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
#include "compiler/source.h"
#include "runtime/runtime.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <assert.h>
#include <ctype.h>
#include <stdarg.h>
#include <math.h>
#include <errno.h>
#include <unistd.h>

global *pntrhash_lookup(task *tsk, pntr p)
{
  int h = hash(&p,sizeof(pntr));
  global *g = tsk->pntrhash[h];
  while (g && !pntrequal(g->p,p))
    g = g->pntrnext;
  return g;
}

global *addrhash_lookup(task *tsk, gaddr addr)
{
  int h = hash(&addr,sizeof(gaddr));
  global *g = tsk->addrhash[h];
  while (g && ((g->addr.pid != addr.pid) || (g->addr.lid != addr.lid)))
    g = g->addrnext;
  return g;
}

void pntrhash_add(task *tsk, global *glo)
{
  int h = hash(&glo->p,sizeof(pntr));
  assert(NULL == glo->pntrnext);
  glo->pntrnext = tsk->pntrhash[h];
  tsk->pntrhash[h] = glo;
}

void addrhash_add(task *tsk, global *glo)
{
  int h = hash(&glo->addr,sizeof(gaddr));
  assert(NULL == glo->addrnext);
  glo->addrnext = tsk->addrhash[h];
  tsk->addrhash[h] = glo;
}

void pntrhash_remove(task *tsk, global *glo)
{
  int h = hash(&glo->p,sizeof(pntr));
  global **ptr = &tsk->pntrhash[h];

  assert(glo);
  while (*ptr != glo) {
    assert(*ptr);
    ptr = &(*ptr)->pntrnext;
  }
  assert(*ptr);

  *ptr = glo->pntrnext;
  glo->pntrnext = NULL;
}

void addrhash_remove(task *tsk, global *glo)
{
  int h = hash(&glo->addr,sizeof(gaddr));
  global **ptr = &tsk->addrhash[h];

  assert(glo);
  while (*ptr != glo) {
    assert(*ptr);
    ptr = &(*ptr)->addrnext;
  }
  assert(*ptr);

  *ptr = glo->addrnext;
  glo->addrnext = NULL;
}

global *add_global(task *tsk, gaddr addr, pntr p)
{
  global *glo = (global*)calloc(1,sizeof(global));
  glo->addr = addr;
  glo->p = p;
  glo->flags = tsk->indistgc ? FLAG_NEW : 0;

  pntrhash_add(tsk,glo);
  addrhash_add(tsk,glo);
  return glo;
}

pntr global_lookup_existing(task *tsk, gaddr addr)
{
  global *glo;
  pntr p;
  if (NULL != (glo = addrhash_lookup(tsk,addr)))
    return glo->p;
  make_pntr(p,NULL);
  return p;
}

pntr global_lookup(task *tsk, gaddr addr, pntr val)
{
  cell *c;
  global *glo;

  if (NULL != (glo = addrhash_lookup(tsk,addr)))
    return glo->p;

  if (!is_nullpntr(val)) {
    glo = add_global(tsk,addr,val);
  }
  else {
    pntr p;
    c = alloc_cell(tsk);
    make_pntr(p,c);
    glo = add_global(tsk,addr,p);

    c->type = CELL_REMOTEREF;
    make_pntr(c->field1,glo);
  }
  return glo->p;
}

gaddr global_addressof(task *tsk, pntr p)
{
  global *glo;
  gaddr addr;

  /* If this object is registered in the global address map, return the address
     that it corresponds to */
  glo = pntrhash_lookup(tsk,p);
  if (glo && (0 <= glo->addr.lid))
    return glo->addr;

  /* It's a local object; give it a global address */
  addr.pid = tsk->pid;
  addr.lid = tsk->nextlid++;
  glo = add_global(tsk,addr,p);
  return glo->addr;
}

/* Obtain a global address for the specified local object. Differs from global_addressof()
   in that if p is a remoteref, then make_global() will return the address of the *actual
   reference*, not the thing that it points to */
global *make_global(task *tsk, pntr p)
{
  global *glo;
  gaddr addr;

  glo = pntrhash_lookup(tsk,p);
  if (glo && (glo->addr.pid == tsk->pid))
    return glo;

  addr.pid = tsk->pid;
  addr.lid = tsk->nextlid++;
  glo = add_global(tsk,addr,p);
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
void remove_gaddr(task *tsk, list **l, gaddr addr)
{
  while (*l) {
    gaddr *item = (gaddr*)(*l)->data;
    if ((item->pid == addr.pid) && (item->lid == addr.lid)) {
      list *old = *l;
      *l = (*l)->next;
      free(old->data);
      free(old);
/*       fprintf(tsk->output,"removed gaddr %d@%d\n",addr.lid,addr.pid); */
      return;
    }
    else {
      l = &((*l)->next);
    }
  }
  fprintf(tsk->output,"gaddr %d@%d not found\n",addr.lid,addr.pid);
  fatal("gaddr not found");
}

void transfer_waiters(waitqueue *from, waitqueue *to)
{
  frame **ptr = &to->frames;
  list **lptr;
  while (*ptr)
    ptr = &(*ptr)->waitlnk;
  *ptr = from->frames;
  from->frames = NULL;

  lptr = &to->fetchers;
  while (*lptr)
    lptr = &(*lptr)->next;
  *lptr = from->fetchers;
  from->fetchers = NULL;
}

void spark_frame(task *tsk, frame *f)
{
  if (STATE_NEW == f->state) {
    add_frame_queue_end(&tsk->sparked,f);
    f->state = STATE_SPARKED;
    tsk->stats.nsparks++;
  }
}

void unspark_frame(task *tsk, frame *f)
{
  assert((STATE_SPARKED == f->state) || (STATE_NEW == f->state));
  if (STATE_SPARKED == f->state)
    remove_frame_queue(&tsk->sparked,f);
  f->state = STATE_NEW;
  assert(NULL == f->wq.frames);
  assert(NULL == f->wq.fetchers);
  assert(NULL == f->next);
  assert(NULL == f->prev);
  assert(CELL_FRAME == celltype(f->c));
  assert(f == (frame*)get_pntr(f->c->field1));
}

void run_frame(task *tsk, frame *f)
{
  if (STATE_SPARKED == f->state) {
    tsk->stats.sparksused++;
    remove_frame_queue(&tsk->sparked,f);
  }

  if ((STATE_SPARKED == f->state) || (STATE_NEW == f->state)) {
    assert((bc_instructions(tsk->bcdata) == f->instr) ||
           (OP_GLOBSTART == f->instr->opcode));
    add_frame_queue(&tsk->active,f);
    f->rnext = *tsk->runptr;
    *tsk->runptr = f;
    f->state = STATE_RUNNING;

    #ifdef PROFILING
    if (0 <= f->fno)
      tsk->stats.fusage[f->fno]++;
    #endif
  }
}

void check_runnable(task *tsk)
{
  if (NULL == *tsk->runptr)
    *tsk->endpt->interruptptr = 1;
}

void block_frame(task *tsk, frame *f)
{
  assert(STATE_RUNNING == f->state);
  assert(f == *tsk->runptr);

  *tsk->runptr = f->rnext;
  f->rnext = NULL;

  f->state = STATE_BLOCKED;
}

void unblock_frame(task *tsk, frame *f)
{
  assert(STATE_BLOCKED == f->state);
  assert(NULL == f->rnext);
  f->rnext = *tsk->runptr;
  *tsk->runptr = f;
  f->state = STATE_RUNNING;
}

void set_error(task *tsk, const char *format, ...)
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

    (*tsk->runptr)->fno = -1;
    (*tsk->runptr)->instr = bc_instructions(tsk->bcdata)+((bcheader*)tsk->bcdata)->erroraddr;
  }
}

void statistics(task *tsk, FILE *f)
{
  int i;
  int total;
  bcheader *bch = (bcheader*)tsk->bcdata;
  fprintf(f,"totalallocs = %d\n",tsk->stats.totalallocs);
  fprintf(f,"ncollections = %d\n",tsk->stats.ncollections);
  fprintf(f,"nscombappls = %d\n",tsk->stats.nscombappls);
  fprintf(f,"nreductions = %d\n",tsk->stats.nreductions);
  fprintf(f,"nsparks = %d\n",tsk->stats.nsparks);

  total = 0;
  for (i = 0; i < OP_COUNT; i++)
    total += tsk->stats.op_usage[i];

  for (i = 0; i < OP_COUNT; i++) {
    double pct = 100.0*(((double)tsk->stats.op_usage[i])/((double)total));
    fprintf(f,"usage %-12s %-12d %.2f%%\n",opcodes[i],tsk->stats.op_usage[i],pct);
  }


  fprintf(f,"usage total = %d\n",total);

  fprintf(f,"\n");
  for (i = 0; i < bch->nfunctions; i++)
    fprintf(f,"%-20s %d\n",bc_function_name(tsk->bcdata,i),tsk->stats.fusage[i]);
}

void dump_info(task *tsk)
{
  block *bl;
  int i;
  frame *f;

  fprintf(tsk->output,"Frames with things waiting on them:\n");
  fprintf(tsk->output,"%-12s %-20s %-12s %-12s\n","frame*","function","frames","fetchers");
  fprintf(tsk->output,"%-12s %-20s %-12s %-12s\n","------","--------","------","--------");

  for (bl = tsk->blocks; bl; bl = bl->next) {
    for (i = 0; i < BLOCK_SIZE; i++) {
      cell *c = &bl->values[i];
      if (CELL_FRAME == c->type) {
        f = (frame*)get_pntr(c->field1);
        if (f->wq.frames || f->wq.fetchers) {
          const char *fname = bc_function_name(tsk->bcdata,f->fno);
          int nfetchers = list_count(f->wq.fetchers);
          int nframes = 0;
          frame *f2;

          for (f2 = f->wq.frames; f2; f2 = f2->waitlnk)
            nframes++;

          fprintf(tsk->output,"%-12p %-20s %-12d %-12d\n",
                  f,fname,nframes,nfetchers);
        }
      }
    }
  }

  fprintf(tsk->output,"\n");
  fprintf(tsk->output,"Runnable queue:\n");
  fprintf(tsk->output,"%-12s %-20s %-12s %-12s %-16s\n",
          "frame*","function","frames","fetchers","state");
  fprintf(tsk->output,"%-12s %-20s %-12s %-12s %-16s\n",
          "------","--------","------","--------","-------------");
  for (f = *tsk->runptr; f; f = f->rnext) {
    const char *fname = bc_function_name(tsk->bcdata,f->fno);
    int nfetchers = list_count(f->wq.fetchers);
    int nframes = 0;
    frame *f2;

    for (f2 = f->wq.frames; f2; f2 = f2->waitlnk)
      nframes++;

    fprintf(tsk->output,"%-12p %-20s %-12d %-12d %-16s\n",
            f,fname,nframes,nfetchers,frame_states[f->state]);
  }

}

void dump_globals(task *tsk)
{
  int h;
  global *glo;

  fprintf(tsk->output,"\n");
  fprintf(tsk->output,"%-9s %-12s %-12s\n","Address","Type","Cell");
  fprintf(tsk->output,"%-9s %-12s %-12s\n","-------","----","----");
  for (h = 0; h < GLOBAL_HASH_SIZE; h++) {
    for (glo = tsk->pntrhash[h]; glo; glo = glo->pntrnext) {
      fprintf(tsk->output,"%4d@%-4d %-12s %-12p\n",
              glo->addr.lid,glo->addr.pid,cell_types[pntrtype(glo->p)],
              is_pntr(glo->p) ? get_pntr(glo->p) : NULL);
    }
  }
}

task *task_new(int pid, int groupsize, const char *bcdata, int bcsize, node *n)
{
  task *tsk = (task*)calloc(1,sizeof(task));
  cell *globnilvalue;
  int i;
  bcheader *bch;

  tsk->n = n;
  if (n)
    tsk->endpt = node_add_endpoint(n,0,TASK_ENDPOINT,tsk);
  tsk->runptr = &tsk->rtemp;

  sem_init(&tsk->startsem,0,0);

  tsk->stats.op_usage = (int*)calloc(OP_COUNT,sizeof(int));

  globnilvalue = alloc_cell(tsk);
  globnilvalue->type = CELL_NIL;
  globnilvalue->flags |= FLAG_PINNED;

  make_pntr(tsk->globnilpntr,globnilvalue);
  tsk->globtruepntr = 1.0;

  if (is_pntr(tsk->globtruepntr))
    get_pntr(tsk->globtruepntr)->flags |= FLAG_PINNED;

  tsk->pntrhash = (global**)calloc(GLOBAL_HASH_SIZE,sizeof(global*));
  tsk->addrhash = (global**)calloc(GLOBAL_HASH_SIZE,sizeof(global*));
  tsk->idmap = (endpointid*)calloc(groupsize,sizeof(endpointid));

  tsk->ioalloc = 1;
  tsk->iocount = 1;
  tsk->ioframes = (ioframe*)calloc(tsk->ioalloc,sizeof(ioframe));
  tsk->iofree = 0;

  tsk->pid = pid;
  tsk->groupsize = groupsize;
  if (NULL == bcdata)
    return tsk; /* no bytecode; we must be using the reduction engine */

  tsk->bcdata = (char*)malloc(bcsize);
  memcpy(tsk->bcdata,bcdata,bcsize);
  tsk->bcsize = bcsize;

  bch = (bcheader*)tsk->bcdata;

  assert(NULL == tsk->strings);
  assert(0 == tsk->nstrings);
  assert(0 < tsk->groupsize);

  tsk->nstrings = bch->nstrings;
  tsk->strings = (pntr*)malloc(bch->nstrings*sizeof(pntr));
  for (i = 0; i < bch->nstrings; i++)
    tsk->strings[i] = string_to_array(tsk,bc_string(tsk->bcdata,i));
  tsk->stats.funcalls = (int*)calloc(bch->nfunctions,sizeof(int));
  tsk->stats.usage = (int*)calloc(bch->nops,sizeof(int));
  tsk->stats.framecompletions = (int*)calloc(bch->nfunctions,sizeof(int));
  tsk->stats.sendcount = (int*)calloc(MSG_COUNT,sizeof(int));
  tsk->stats.sendbytes = (int*)calloc(MSG_COUNT,sizeof(int));
  tsk->stats.recvcount = (int*)calloc(MSG_COUNT,sizeof(int));
  tsk->stats.recvbytes = (int*)calloc(MSG_COUNT,sizeof(int));
  tsk->stats.fusage = (int*)calloc(bch->nfunctions,sizeof(int));
  tsk->gcsent = (int*)calloc(tsk->groupsize,sizeof(int));
  tsk->distmarks = (array**)calloc(tsk->groupsize,sizeof(array*));

  tsk->inflight_addrs = (array**)calloc(tsk->groupsize,sizeof(array*));
  tsk->unack_msg_acount = (array**)calloc(tsk->groupsize,sizeof(array*));
  for (i = 0; i < tsk->groupsize; i++) {
    tsk->inflight_addrs[i] = array_new(sizeof(gaddr));
    tsk->unack_msg_acount[i] = array_new(sizeof(int));
    tsk->distmarks[i] = array_new(sizeof(gaddr));
  }

  if (n) {
    if (0 > pthread_create(&tsk->thread,NULL,(void*)execute,tsk))
      fatal("pthread_create: %s",strerror(errno));
    if (0 > pthread_detach(tsk->thread))
      fatal("pthread_detach: %s",strerror(errno));
  }

  return tsk;
}

void task_free(task *tsk)
{
  int i;
  block *bl;
  int h;
  node *n = tsk->n;
  endpoint *endpt = tsk->endpt;

  sweep(tsk,1);

  for (h = 0; h < GLOBAL_HASH_SIZE; h++) {
    global *glo = tsk->pntrhash[h];
    while (glo) {
      global *next = glo->pntrnext;
      free_global(tsk,glo);
      glo = next;
    }
  }

  bl = tsk->blocks;
  while (bl) {
    block *next = bl->next;
    free(bl);
    bl = next;
  }

  free(tsk->idmap);
  free(tsk->ioframes);

  for (i = 0; i < tsk->groupsize; i++) {
    array_free(tsk->inflight_addrs[i]);
    array_free(tsk->unack_msg_acount[i]);
    array_free(tsk->distmarks[i]);
  }

  while (tsk->freeframe) {
    frame *next = tsk->freeframe->freelnk;
    free(tsk->freeframe->data);
    free(tsk->freeframe);
    tsk->freeframe = next;
  }

  free(tsk->strings);
  free(tsk->stats.funcalls);
  free(tsk->stats.framecompletions);
  free(tsk->stats.sendcount);
  free(tsk->stats.sendbytes);
  free(tsk->stats.recvcount);
  free(tsk->stats.recvbytes);
  free(tsk->stats.fusage);
  free(tsk->stats.usage);
  free(tsk->stats.op_usage);
  free(tsk->gcsent);
  free(tsk->error);
  free(tsk->distmarks);
  free(tsk->pntrhash);
  free(tsk->addrhash);

  free(tsk->inflight_addrs);
  free(tsk->unack_msg_acount);

  free(tsk->bcdata);

  sem_destroy(&tsk->startsem);

  free(tsk);

  if (n)
    node_remove_endpoint(n,endpt);
}

void task_kill_locked(task *tsk)
{
  /* FIXME: ensure that when a task is killed, all connections that is has open are closed. */
  tsk->done = 1;
  *tsk->endpt->interruptptr = 1;
  node_waitclose_locked(tsk->n,tsk->endpt->localid);
}
