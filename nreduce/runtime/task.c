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
    assert((0 == f->address) ||
           (OP_GLOBSTART == bc_instructions(tsk->bcdata)[f->address].opcode));
    add_frame_queue(&tsk->runnable,f);
    f->state = STATE_RUNNING;

    #ifdef PROFILING
    if (0 <= f->fno)
      tsk->stats.fusage[f->fno]++;
    #endif
  }
}

void block_frame(task *tsk, frame *f)
{
  assert(STATE_RUNNING == f->state);
  remove_frame_queue(&tsk->runnable,f);
  add_frame_queue(&tsk->blocked,f);
  f->state = STATE_BLOCKED;
}

void unblock_frame(task *tsk, frame *f)
{
  assert(STATE_BLOCKED == f->state);
  remove_frame_queue(&tsk->blocked,f);
  add_frame_queue(&tsk->runnable,f);
  f->state = STATE_RUNNING;
}

void done_frame(task *tsk, frame *f)
{
  assert(STATE_RUNNING == f->state);
  remove_frame_queue(&tsk->runnable,f);
  f->state = STATE_DONE;
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

  if (NULL == tsk->curfptr) {
    /* reduction engine */
  }
  else {
    /* bytecode interpreter */
    frame *f = *tsk->curfptr;
    instr = &bc_instructions(tsk->bcdata)[f->address];
    tsk->errorsl.fileno = instr->fileno;
    tsk->errorsl.lineno = instr->lineno;

    (*tsk->curfptr)->fno = -1;
    (*tsk->curfptr)->address = ((bcheader*)tsk->bcdata)->erroraddr-1;
    /* ensure it's at the front */
    remove_frame_queue(&tsk->runnable,f);
    add_frame_queue(&tsk->runnable,f);
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
          int nframes = list_count(f->wq.frames);
          int nfetchers = list_count(f->wq.fetchers);
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
  for (f = tsk->runnable.first; f; f = f->next) {
    const char *fname = bc_function_name(tsk->bcdata,f->fno);
    int nframes = list_count(f->wq.frames);
    int nfetchers = list_count(f->wq.fetchers);
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

task *task_new(int pid, int groupsize, const char *bcdata, int bcsize)
{
  task *tsk = (task*)calloc(1,sizeof(task));
  cell *globnilvalue;
  int i;
  bcheader *bch;

  pthread_mutex_init(&tsk->mailbox.lock,NULL);
  pthread_cond_init(&tsk->mailbox.cond,NULL);

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
  tsk->idmap = (taskid*)calloc(groupsize,sizeof(taskid));

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

  return tsk;
}

void task_free(task *tsk)
{
  int i;
  block *bl;
  int h;

  for (bl = tsk->blocks; bl; bl = bl->next)
    for (i = 0; i < BLOCK_SIZE; i++)
      bl->values[i].flags &= ~(FLAG_PINNED | FLAG_MARKED | FLAG_DMB);

/*   local_collect(tsk); */
  sweep(tsk);

  bl = tsk->blocks;
  while (bl) {
    block *next = bl->next;
    free(bl);
    bl = next;
  }

  for (h = 0; h < GLOBAL_HASH_SIZE; h++) {
    global *glo = tsk->pntrhash[h];
    while (glo) {
      global *next = glo->pntrnext;
      free(glo);
      glo = next;
    }
  }

  free(tsk->idmap);

  for (i = 0; i < tsk->groupsize; i++) {
    array_free(tsk->inflight_addrs[i]);
    array_free(tsk->unack_msg_acount[i]);
    array_free(tsk->distmarks[i]);
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
  free(tsk);

  pthread_mutex_destroy(&tsk->mailbox.lock);
  pthread_cond_destroy(&tsk->mailbox.cond);
}

void task_add_message(task *tsk, message *msg)
{
  pthread_mutex_lock(&tsk->mailbox.lock);
  llist_append(&tsk->mailbox,msg);
  tsk->checkmsg = 1;
  pthread_cond_broadcast(&tsk->mailbox.cond);
  pthread_mutex_unlock(&tsk->mailbox.lock);
}

message *task_next_message(task *tsk, int delayms)
{
  message *msg;
  pthread_mutex_lock(&tsk->mailbox.lock);

  if ((NULL == tsk->mailbox.first) && (0 != delayms)) {
    if (0 > delayms) {
      pthread_cond_wait(&tsk->mailbox.cond,&tsk->mailbox.lock);
    }
    else {
      struct timeval now;
      struct timespec abstime;
      gettimeofday(&now,NULL);

      now.tv_sec += delayms/1000;
      now.tv_usec += (delayms%1000)*1000;
      if (now.tv_usec >= 1000000) {
        now.tv_usec -= 1000000;
        now.tv_sec++;
      }
      abstime.tv_sec = now.tv_sec;
      abstime.tv_nsec = now.tv_usec * 1000;
      pthread_cond_timedwait(&tsk->mailbox.cond,&tsk->mailbox.lock,&abstime);
    }
  }

  msg = tsk->mailbox.first;
  if (NULL != msg)
    llist_remove(&tsk->mailbox,msg);
  tsk->checkmsg = (NULL != tsk->mailbox.first);
  pthread_mutex_unlock(&tsk->mailbox.lock);
  return msg;
}
