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

    c->type = CELL_REMOTEREF;
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
  for (f = q->first; f; f = f->next)
    count++;
  return count;
}

static int queue_contains_frame(frameq *q, frame *f)
{
  frame *c;
  for (c = q->first; c; c = c->next)
    if (c == f)
      return 1;
  return 0;
}

static int check_queue(frameq *q)
{
  frame *f;
  assert((!q->first && !q->last) || (q->first && q->last));
  assert(!q->last || !q->last->next);
  assert(!q->first || !q->first->prev);
  assert(q->size == queue_size(q));
  for (f = q->first; f; f = f->next) {
    assert((q->last == f) || (f->next->prev == f));
    assert((q->first == f) || (f->prev->next == f));
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

  llist_prepend(q,f);
}

void add_frame_queue_end(frameq *q, frame *f)
{
  #ifdef QUEUE_CHECKS
  assert(check_queue(q));
  assert(!queue_contains_frame(q,f));
  #endif

  llist_append(q,f);
}

void remove_frame_queue(frameq *q, frame *f)
{
  #ifdef QUEUE_CHECKS
  assert(check_queue(q));
  assert(queue_contains_frame(q,f));
  #endif

  llist_remove(q,f);

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
  assert(NULL == f->next);
  assert(NULL == f->prev);
  assert(CELL_FRAME == celltype(f->c));
  assert(f == (frame*)get_pntr(f->c->field1));
}

void run_frame(process *proc, frame *f)
{
  if (STATE_SPARKED == f->state) {
    proc->stats.sparksused++;
    remove_frame_queue(&proc->sparked,f);
  }

  if ((STATE_SPARKED == f->state) || (STATE_NEW == f->state)) {
    assert((0 == f->address) ||
           (OP_GLOBSTART == bc_instructions(proc->bcdata)[f->address].opcode));
    add_frame_queue(&proc->runnable,f);
    f->state = STATE_RUNNING;

    #ifdef PROFILING
    if (0 <= f->fno)
      proc->stats.fusage[f->fno]++;
    #endif
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

void set_error(process *proc, const char *format, ...)
{
  va_list ap;
  int len;
  frame *f = *proc->curfptr;
  const instruction *instr;
  va_start(ap,format);
  len = vsnprintf(NULL,0,format,ap);
  va_end(ap);

  assert(NULL == proc->error);
  proc->error = (char*)malloc(len+1);
  va_start(ap,format);
  len = vsnprintf(proc->error,len+1,format,ap);
  va_end(ap);

  instr = &bc_instructions(proc->bcdata)[f->address];
  proc->errorsl.fileno = instr->fileno;
  proc->errorsl.lineno = instr->lineno;

  (*proc->curfptr)->fno = -1;
  (*proc->curfptr)->address = ((bcheader*)proc->bcdata)->erroraddr-1;
  /* ensure it's at the front */
  remove_frame_queue(&proc->runnable,f);
  add_frame_queue(&proc->runnable,f);
}

void statistics(process *proc, FILE *f)
{
  int i;
  int total;
  bcheader *bch = (bcheader*)proc->bcdata;
  fprintf(f,"totalallocs = %d\n",proc->stats.totalallocs);
  fprintf(f,"ncollections = %d\n",proc->stats.ncollections);
  fprintf(f,"nscombappls = %d\n",proc->stats.nscombappls);
  fprintf(f,"nreductions = %d\n",proc->stats.nreductions);
  fprintf(f,"nsparks = %d\n",proc->stats.nsparks);

  total = 0;
  for (i = 0; i < OP_COUNT; i++)
    total += proc->stats.op_usage[i];

  for (i = 0; i < OP_COUNT; i++) {
    double pct = 100.0*(((double)proc->stats.op_usage[i])/((double)total));
    fprintf(f,"usage %-12s %-12d %.2f%%\n",opcodes[i],proc->stats.op_usage[i],pct);
  }


  fprintf(f,"usage total = %d\n",total);

  fprintf(f,"\n");
  for (i = 0; i < bch->nfunctions; i++)
    fprintf(f,"%-20s %d\n",bc_function_name(proc->bcdata,i),proc->stats.fusage[i]);
}

void dump_info(process *proc)
{
  block *bl;
  int i;
  frame *f;

  fprintf(proc->output,"Frames with things waiting on them:\n");
  fprintf(proc->output,"%-12s %-20s %-12s %-12s\n","frame*","function","frames","fetchers");
  fprintf(proc->output,"%-12s %-20s %-12s %-12s\n","------","--------","------","--------");

  for (bl = proc->blocks; bl; bl = bl->next) {
    for (i = 0; i < BLOCK_SIZE; i++) {
      cell *c = &bl->values[i];
      if (CELL_FRAME == c->type) {
        f = (frame*)get_pntr(c->field1);
        if (f->wq.frames || f->wq.fetchers) {
          const char *fname = bc_function_name(proc->bcdata,f->fno);
          int nframes = list_count(f->wq.frames);
          int nfetchers = list_count(f->wq.fetchers);
          fprintf(proc->output,"%-12p %-20s %-12d %-12d\n",
                  f,fname,nframes,nfetchers);
        }
      }
    }
  }

  fprintf(proc->output,"\n");
  fprintf(proc->output,"Runnable queue:\n");
  fprintf(proc->output,"%-12s %-20s %-12s %-12s %-16s\n",
          "frame*","function","frames","fetchers","state");
  fprintf(proc->output,"%-12s %-20s %-12s %-12s %-16s\n",
          "------","--------","------","--------","-------------");
  for (f = proc->runnable.first; f; f = f->next) {
    const char *fname = bc_function_name(proc->bcdata,f->fno);
    int nframes = list_count(f->wq.frames);
    int nfetchers = list_count(f->wq.fetchers);
    fprintf(proc->output,"%-12p %-20s %-12d %-12d %-16s\n",
            f,fname,nframes,nfetchers,frame_states[f->state]);
  }

}

void dump_globals(process *proc)
{
  int h;
  global *glo;

  fprintf(proc->output,"\n");
  fprintf(proc->output,"%-9s %-12s %-12s\n","Address","Type","Cell");
  fprintf(proc->output,"%-9s %-12s %-12s\n","-------","----","----");
  for (h = 0; h < GLOBAL_HASH_SIZE; h++) {
    for (glo = proc->pntrhash[h]; glo; glo = glo->pntrnext) {
      fprintf(proc->output,"%4d@%-4d %-12s %-12p\n",
              glo->addr.lid,glo->addr.pid,cell_types[pntrtype(glo->p)],
              is_pntr(glo->p) ? get_pntr(glo->p) : NULL);
    }
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
  globnilvalue->type = CELL_NIL;
  globnilvalue->flags |= FLAG_PINNED;

  make_pntr(proc->globnilpntr,globnilvalue);
  proc->globtruepntr = 1.0;

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

  assert(NULL == proc->strings);
  assert(0 == proc->nstrings);
  assert(0 < proc->groupsize);


  proc->nstrings = bch->nstrings;
  proc->strings = (pntr*)malloc(bch->nstrings*sizeof(pntr));
  for (i = 0; i < bch->nstrings; i++)
    proc->strings[i] = string_to_array(proc,bc_string(proc->bcdata,i));
  proc->stats.funcalls = (int*)calloc(bch->nfunctions,sizeof(int));
  proc->stats.usage = (int*)calloc(bch->nops,sizeof(int));
  proc->stats.framecompletions = (int*)calloc(bch->nfunctions,sizeof(int));
  proc->stats.sendcount = (int*)calloc(MSG_COUNT,sizeof(int));
  proc->stats.sendbytes = (int*)calloc(MSG_COUNT,sizeof(int));
  proc->stats.recvcount = (int*)calloc(MSG_COUNT,sizeof(int));
  proc->stats.recvbytes = (int*)calloc(MSG_COUNT,sizeof(int));
  proc->stats.fusage = (int*)calloc(bch->nfunctions,sizeof(int));
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
  free(proc->stats.fusage);
  free(proc->stats.usage);
  free(proc->stats.op_usage);
  free(proc->gcsent);
  free(proc->error);
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

