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
#include "modules/modules.h"
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
      tsk->stats.funcalls[f->fno]++;
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

void unblock_frame_toend(task *tsk, frame *f)
{
  frame **fp;
  assert(STATE_BLOCKED == f->state);
  assert(NULL == f->rnext);
  fp = tsk->runptr;
  while (*fp)
    fp = &((*fp)->rnext);
  *fp = f;
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
  const funinfo *finfo;
  int fno;

  tsk->n = n;
  if (n)
    tsk->endpt = node_add_endpoint(n,0,TASK_ENDPOINT,tsk);
  tsk->runptr = &tsk->rtemp;

  sem_init(&tsk->startsem,0,0);

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
  finfo = bc_funinfo(tsk->bcdata);
  for (fno = 0; fno < bch->nfunctions; fno++)
    if (tsk->maxstack < finfo[fno].stacksize)
      tsk->maxstack = finfo[fno].stacksize;

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
  tsk->stats.sendcount = (int*)calloc(MSG_COUNT,sizeof(int));
  tsk->stats.sendbytes = (int*)calloc(MSG_COUNT,sizeof(int));
  tsk->stats.recvcount = (int*)calloc(MSG_COUNT,sizeof(int));
  tsk->stats.recvbytes = (int*)calloc(MSG_COUNT,sizeof(int));
  tsk->gcsent = (int*)calloc(tsk->groupsize,sizeof(int));
  tsk->distmarks = (array**)calloc(tsk->groupsize,sizeof(array*));

  tsk->inflight_addrs = (array**)calloc(tsk->groupsize,sizeof(array*));
  tsk->unack_msg_acount = (array**)calloc(tsk->groupsize,sizeof(array*));
  for (i = 0; i < tsk->groupsize; i++) {
    tsk->inflight_addrs[i] = array_new(sizeof(gaddr),0);
    tsk->unack_msg_acount[i] = array_new(sizeof(int),0);
    tsk->distmarks[i] = array_new(sizeof(gaddr),0);
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

  while (tsk->frameblocks) {
    frameblock *next = tsk->frameblocks->next;
    free(tsk->frameblocks);
    tsk->frameblocks = next;
  }

  free(tsk->strings);
  free(tsk->stats.sendcount);
  free(tsk->stats.sendbytes);
  free(tsk->stats.recvcount);
  free(tsk->stats.recvbytes);
  free(tsk->stats.funcalls);
  free(tsk->stats.frames);
  free(tsk->stats.caps);
  free(tsk->stats.usage);
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

static array *read_file(const char *filename)
{
  FILE *f;
  int r;
  array *buf;
  if (NULL == (f = fopen(filename,"r"))) {
    perror(filename);
    return NULL;
  }

  buf = array_new(1,0);
  while (1) {
    array_mkroom(buf,1024);
    r = fread(&buf->data[buf->nbytes],1,1024,f);
    if (0 >= r)
      break;
    buf->nbytes += r;
  }

  fclose(f);
  return buf;
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

    if (0 == usage)
      break;

    fno = fusage[i].fno;
    fprintf(f,"%8d %6.2f%% %8d %8d %8d %s\n",usage,pct,
            tsk->stats.funcalls[fno],tsk->stats.frames[fno],tsk->stats.caps[fno],
            bc_function_name(tsk->bcdata,fno));
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
