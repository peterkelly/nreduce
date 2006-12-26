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

#define _GNU_SOURCE /* for asprintf */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "compiler/bytecode.h"
#include "src/nreduce.h"
#include "compiler/source.h"
#include "runtime.h"
#include "network.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <assert.h>
#include <ctype.h>
#include <stdarg.h>
#include <math.h>
#include <errno.h>
#include <sys/types.h>
#include <unistd.h>

/* #define PARALLELISM_DEBUG */
/* #define FETCH_DEBUG */

#define FISH_DELAY_MS 5000

void print_stack(FILE *f, pntr *stk, int size, int dir)
{
  int i;

  if (dir)
    i = size-1;
  else
    i = 0;


  while (1) {
    pntr p;
    int pos = dir ? (size-1-i) : i;

    if (dir && i < 0)
      break;

    if (!dir && (i >= size))
      break;

    p = resolve_pntr(stk[i]);
    if (CELL_IND == pntrtype(stk[i]))
      fprintf(f,"%2d: [i] %12s ",pos,snode_types[pntrtype(p)]);
    else
      fprintf(f,"%2d:     %12s ",pos,snode_types[pntrtype(p)]);
    print_pntr(f,p);
    fprintf(f,"\n");

    if (dir)
      i--;
    else
      i++;
  }
}

static void print_task_sourceloc(task *tsk, FILE *f, sourceloc sl)
{
  if (0 <= sl.fileno)
    fprintf(f,"%s:%d: ",bc_string(tsk->bcdata,sl.fileno),sl.lineno);
}

static void cap_error(task *tsk, pntr cappntr, const instruction *op)
{
  sourceloc sl;
  assert(CELL_CAP == pntrtype(cappntr));
  cell *capval = get_pntr(cappntr);
  cap *c = (cap*)get_pntr(capval->field1);
  const char *name = bc_function_name(tsk->bcdata,c->fno);
  int nargs = c->arity;

  sl.fileno = op->fileno;
  sl.lineno = op->lineno;
  print_task_sourceloc(tsk,stderr,sl);
  fprintf(stderr,"Attempt to evaluate incomplete function application\n");

  print_task_sourceloc(tsk,stderr,c->sl);
  fprintf(stderr,"%s requires %d args, only have %d\n",name,nargs,c->count);
  exit(1);
}

static void constant_app_error(task *tsk, pntr cappntr, const instruction *op)
{
  sourceloc sl;
  sl.fileno = op->fileno;
  sl.lineno = op->lineno;
  print_task_sourceloc(tsk,stderr,sl);
  fprintf(stderr,CONSTANT_APP_MSG"\n");
  /* FIXME: need to print the expression here */
  abort();
}

static void handle_error(task *tsk)
{
  print_task_sourceloc(tsk,stderr,tsk->errorsl);
  fprintf(stderr,"%s\n",tsk->error);
  exit(1);
}

static void resume_waiters(task *tsk, waitqueue *wq, pntr obj)
{
  list *l;

  if (!wq->frames && !wq->fetchers)
    return;

  obj = resolve_pntr(obj);

  for (l = wq->frames; l; l = l->next) {
    frame *f = (frame*)l->data;
    f->waitframe = NULL;
    f->waitglo = NULL;
    unblock_frame(tsk,f);
  }
  list_free(wq->frames,NULL);
  wq->frames = NULL;

  for (l = wq->fetchers; l; l = l->next) {
    gaddr *ft = (gaddr*)l->data;
    assert(CELL_FRAME != pntrtype(obj));
/*     fprintf(tsk->output,"1: responding with %s\n",cell_types[pntrtype(obj)]); */
    msg_fsend(tsk,ft->pid,MSG_RESPOND,"pa",obj,*ft);
  }
  list_free(wq->fetchers,free);
  wq->fetchers = NULL;
}

static void frame_return(task *tsk, frame *curf, pntr val)
{
  #ifdef SHOW_FRAME_COMPLETION
  if (0 <= tsk->pid) {
    int valtype = pntrtype(val);
    fprintf(tsk->output,"FRAME(%d) completed; result is a %s\n",
            curf->fno,cell_types[valtype]);
  }
  #endif


  assert(!is_nullpntr(val));
  assert(NULL != curf->c);
  assert(CELL_FRAME == celltype(curf->c));
  asprintf(&curf->c->msg,"frame_return: wq frames = %d, wq fetchers = %d",
           list_count(curf->wq.frames),list_count(curf->wq.fetchers));
  curf->c->indsource = 1;
  curf->c->type = CELL_IND;
  curf->c->field1 = val;
  curf->c = NULL;

  resume_waiters(tsk,&curf->wq,val);
  done_frame(tsk,curf);
  frame_dealloc(tsk,curf);
}

static void schedule_frame(task *tsk, frame *f, int desttsk, array *msg)
{
  global *glo;
  global *refglo;
  pntr fcp;
  gaddr addr;
  gaddr refaddr;

  /* Remove the frame from the runnable queue */
  unspark_frame(tsk,f);

  /* Create remote reference which will point to the frame on desttsk once an ACK is sent back */

  make_pntr(fcp,f->c);
  addr.pid = desttsk;
  addr.lid = -1;
  glo = add_global(tsk,addr,fcp);

  refaddr.pid = tsk->pid;
  refaddr.lid = tsk->nextlid++;
  refglo = add_global(tsk,refaddr,fcp);

  /* Transfer the frame to the destination, and tell it to notfy us of the global address it
     assigns to the frame (which we'll store in newref) */
  #ifdef PAREXEC_DEBUG
/*   fprintf(tsk->output,"Scheduling frame %s on task %d, newrefaddr = %d@%d\n", */
/*           function_name(tsk->gp,f->fno),desttsk,newrefaddr.lid,newrefaddr.pid); */
  #endif
  write_format(msg,tsk,"pa",glo->p,refglo->addr);

  f->c->type = CELL_REMOTEREF;
  make_pntr(f->c->field1,glo);

  /* Delete the local copy of the frame */
  f->c = NULL;
  frame_dealloc(tsk,f);
}

static void send_update(task *tsk)
{
  int homesite = tsk->groupsize-1;
  array *wr = write_start();
  int i;
  for (i = 0; i < tsk->groupsize-1; i++) {
    write_int(wr,tsk->gcsent[i]);
    tsk->gcsent[i] = 0;
  }
  msg_send(tsk,homesite,MSG_UPDATE,wr->data,wr->nbytes);
  write_end(wr);
}

static void start_address_reading(task *tsk, int from, int msgtype)
{
  tsk->ackmsg = write_start();
  tsk->ackmsgsize = tsk->ackmsg->nbytes;
}

static void finish_address_reading(task *tsk, int from, int msgtype)
{
  int haveaddrs = (tsk->ackmsg->nbytes != tsk->ackmsgsize);
  int ackcount = tsk->naddrsread;

  write_end(tsk->ackmsg);
  tsk->ackmsg = NULL;
  tsk->naddrsread = 0;
  tsk->ackmsgsize = 0;


  if (haveaddrs) {
    assert(0 < ackcount);
    msg_fsend(tsk,from,MSG_ACK,"iii",1,ackcount,msgtype);
  }
  else {
    assert(0 == ackcount);
  }
}

void add_pending_mark(task *tsk, gaddr addr)
{
  assert(0 <= addr.pid);
  assert(tsk->groupsize-1 >= addr.pid);
  assert(tsk->inmark);
  array_append(tsk->distmarks[addr.pid],&addr,sizeof(gaddr));
}

static void send_mark_messages(task *tsk)
{
  int pid;
  for (pid = 0; pid < tsk->groupsize; pid++) {
    if (0 < array_count(tsk->distmarks[pid])) {
      int a;
      array *msg = write_start();
      for (a = 0; a < array_count(tsk->distmarks[pid]); a++) {
        gaddr addr = array_item(tsk->distmarks[pid],a,gaddr);
        write_gaddr(msg,tsk,addr);
        tsk->gcsent[addr.pid]++;
      }
      msg_send(tsk,pid,MSG_MARKENTRY,msg->data,msg->nbytes);
      write_end(msg);

      tsk->distmarks[pid]->nbytes = 0;
    }
  }
}

static int handle_message2(task *tsk, int from, int tag, char *data, int size)
{
  int r = READER_OK;
  reader rd = read_start(data,size);

  if ((0 > tag) || (MSG_COUNT <= tag))
    return READER_INCORRECT_CONTENTS;

  tsk->stats.recvcount[tag]++;
  tsk->stats.recvbytes[tag] += size;

  #ifdef MSG_DEBUG
/*   fprintf(tsk->output,"[%d] ",list_count(tsk->inflight)); */
  fprintf(tsk->output,"%d => %s ",from,msg_names[tag]);
  CHECK_READ(print_data(tsk,data+rd.pos,size-rd.pos));
  fprintf(tsk->output," (%d bytes)\n",size);
  fprintf(tsk->output,"\n");
  #endif

  switch (tag) {
  case MSG_DONE:
    tsk->done = 1;
    fprintf(tsk->output,"%d: done\n",tsk->pid);
    break;
  case MSG_FISH: {
    int reqtsk, age, nframes;
    int scheduled = 0;
    array *msg;

    start_address_reading(tsk,from,tag);
    r = read_format(&rd,tsk,0,"iii.",&reqtsk,&age,&nframes);
    finish_address_reading(tsk,from,tag);
    CHECK_READ(r);

    if (reqtsk == tsk->pid)
      break;

    msg = write_start();
    while ((NULL != tsk->sparked.first) && (1 <= nframes--)) {
      schedule_frame(tsk,tsk->sparked.last,reqtsk,msg);
      scheduled++;
    }
    if (0 < scheduled)
      msg_send(tsk,reqtsk,MSG_SCHEDULE,msg->data,msg->nbytes);
    write_end(msg);

    #ifdef PARALLELISM_DEBUG
/*     fprintf(tsk->output,"received FISH from task %d; scheduled %d\n",reqtsk,scheduled); */
    #endif

    if (!scheduled && (0 < (age)--))
      msg_fsend(tsk,(tsk->pid+1) % (tsk->groupsize-1),MSG_FISH,"iii",reqtsk,age,nframes);
    break;
  }
  case MSG_FETCH: {
    /* FETCH (objaddr) (storeaddr)
       Request to send the requested object (or a copy of it) to another task

       (objaddr)   is the global address of the object *in this task* that is to be sent

       (storeaddr) is the global address of a remote reference *in the other task*, which
                   points to the requested object. When we send the response, the other address
                   will store the object in the p field of the relevant "global" structure. */
    pntr obj;
    gaddr storeaddr;
    gaddr objaddr;

    start_address_reading(tsk,from,tag);
    r = read_format(&rd,tsk,0,"aa.",&objaddr,&storeaddr);
    finish_address_reading(tsk,from,tag);
    CHECK_READ(r);

    CHECK_EXPR(objaddr.pid == tsk->pid);
    CHECK_EXPR(storeaddr.pid == from);
    obj = global_lookup_existing(tsk,objaddr);

    if (is_nullpntr(obj)) {
      fprintf(tsk->output,"Request for deleted global %d@%d, from task %d\n",
              objaddr.lid,objaddr.pid,from);
    }

    CHECK_EXPR(!is_nullpntr(obj)); /* we should only be asked for a cell we actually have */
    obj = resolve_pntr(obj);

    #ifdef FETCH_DEBUG
    fprintf(tsk->output,"received FETCH %d@%d -> %d@%d",
            objaddr.lid,objaddr.pid,storeaddr.lid,storeaddr.pid);
    #endif

    if ((CELL_REMOTEREF == pntrtype(obj)) && (0 > pglobal(obj)->addr.lid)) {
      #ifdef FETCH_DEBUG
      fprintf(tsk->output,": case 1\n");
      #endif
      add_gaddr(&pglobal(obj)->wq.fetchers,storeaddr);
    }
    else if ((CELL_FRAME == pntrtype(obj)) &&
             ((STATE_RUNNING == pframe(obj)->state) || (STATE_BLOCKED == pframe(obj)->state))) {
      #ifdef FETCH_DEBUG
      fprintf(tsk->output,": case 2\n");
      #endif
      add_gaddr(&pframe(obj)->wq.fetchers,storeaddr);
    }
    else if ((CELL_FRAME == pntrtype(obj)) &&
             ((STATE_SPARKED == pframe(obj)->state) ||
              (STATE_NEW == pframe(obj)->state))) {
      frame *f = pframe(obj);
      pntr ref;
      global *glo;
      pntr fcp;

      #ifdef FETCH_DEBUG
      fprintf(tsk->output,": case 3\n");
      #endif
      /* Remove the frame from the runnable queue */
      unspark_frame(tsk,f);

      /* Send the actual frame */
      msg_fsend(tsk,from,MSG_RESPOND,"pa",obj,storeaddr);

      /* Check that we don't already have a reference to the remote addr
         (FIXME: is this safe?) */
      ref = global_lookup_existing(tsk,storeaddr);
      CHECK_EXPR(is_nullpntr(ref));

      /* Replace our copy of a frame to a reference to the remote object */
      make_pntr(fcp,f->c);
      glo = add_global(tsk,storeaddr,fcp);

      f->c->type = CELL_REMOTEREF;
      make_pntr(f->c->field1,glo);
      f->c = NULL;
      frame_dealloc(tsk,f);
    }
    else {
      #ifdef PAREXEC_DEBUG
      fprintf(tsk->output,
              "Responding to fetch: val %s, req task %d, storeaddr = %d@%d, objaddr = %d@%d\n",
              cell_types[pntrtype(obj)],from,storeaddr.lid,storeaddr.pid,objaddr.lid,objaddr.pid);
      #endif
      #ifdef FETCH_DEBUG
      fprintf(tsk->output,": case 4\n");
      #endif
      msg_fsend(tsk,from,MSG_RESPOND,"pa",obj,storeaddr);
    }
    break;
  }
  case MSG_RESPOND: {
    pntr obj;
    pntr ref;
    gaddr storeaddr;
    cell *refcell;
    global *objglo;

    start_address_reading(tsk,from,tag);
    r = read_format(&rd,tsk,0,"pa",&obj,&storeaddr);
    finish_address_reading(tsk,from,tag);
    CHECK_READ(r);

    ref = global_lookup_existing(tsk,storeaddr);

    CHECK_EXPR(storeaddr.pid == tsk->pid);
    if (is_nullpntr(ref)) {
      fprintf(tsk->output,"Respond tried to store in deleted global %d@%d\n",
              storeaddr.lid,storeaddr.pid);
    }
    CHECK_EXPR(!is_nullpntr(ref));
    CHECK_EXPR(CELL_REMOTEREF == pntrtype(ref));

    objglo = pglobal(ref);
    CHECK_EXPR(objglo->fetching);
    objglo->fetching = 0;
    CHECK_EXPR(objglo->addr.pid == from);
    CHECK_EXPR(objglo->addr.lid >= 0);
    CHECK_EXPR(pntrequal(objglo->p,ref));

    refcell = get_pntr(ref);
    refcell->indsource = 2;
    refcell->type = CELL_IND;
    refcell->field1 = obj;

    resume_waiters(tsk,&objglo->wq,obj);

    if (CELL_FRAME == pntrtype(obj))
      run_frame(tsk,pframe(obj));
    tsk->stats.fetches--;
    break;
  }
  case MSG_SCHEDULE: {
    pntr framep;
    gaddr tellsrc;
    global *frameglo;
    array *urmsg;
    int count = 0;

/*     fprintf(tsk->output,"processing SCHEDULE\n"); */

    urmsg = write_start();

    start_address_reading(tsk,from,tag);
    while (rd.pos < rd.size) {
      r = read_format(&rd,tsk,0,"pa",&framep,&tellsrc);
      CHECK_READ(r);

      CHECK_EXPR(CELL_FRAME == pntrtype(framep));
/*       fprintf(tsk->output,"got frame\n"); */
      frameglo = make_global(tsk,framep);
      write_format(urmsg,tsk,"aa",tellsrc,frameglo->addr);

      spark_frame(tsk,pframe(framep)); /* FIXME: temp; just to count sparks */
      run_frame(tsk,pframe(framep));


/*       { */
/*         frame *f = pframe(framep); */
/*         fprintf(tsk->output,"schedule: frame %d (%s)\n", */
/*                 f->fno,bc_function_name(tsk->bcdata,f->fno)); */
/*       } */

      count++;
      tsk->stats.sparks++;
    }
    finish_address_reading(tsk,from,tag);

    msg_send(tsk,from,MSG_UPDATEREF,urmsg->data,urmsg->nbytes);
    write_end(urmsg);

    fprintf(tsk->output,"Got %d new frames\n",count);
    tsk->newfish = 1;

    #ifdef PARALLELISM_DEBUG
    fprintf(tsk->output,"done processing SCHEDULE: count = %d\n",count);
    #endif

    break;
  }
  case MSG_UPDATEREF: {
    pntr ref;
    gaddr refaddr;
    gaddr remoteaddr;
    global *glo;

    start_address_reading(tsk,from,tag);
    while (rd.pos < rd.size) {
      r = read_format(&rd,tsk,0,"aa",&refaddr,&remoteaddr);
      CHECK_READ(r);

      ref = global_lookup_existing(tsk,refaddr);

      CHECK_EXPR(refaddr.pid == tsk->pid);
      CHECK_EXPR(!is_nullpntr(ref));
      CHECK_EXPR(CELL_REMOTEREF == pntrtype(ref));

      glo = pglobal(ref);
      CHECK_EXPR(glo->addr.pid == from);
      CHECK_EXPR(glo->addr.lid == -1);
      CHECK_EXPR(pntrequal(glo->p,ref));
      CHECK_EXPR(remoteaddr.pid == from);

      addrhash_remove(tsk,glo);
      glo->addr.lid = remoteaddr.lid;
      addrhash_add(tsk,glo);

      resume_waiters(tsk,&glo->wq,glo->p);
    }
    finish_address_reading(tsk,from,tag);
    break;
  }
  case MSG_ACK: {
    int count;
    int naddrs;
    int remove;
    int fromtype;
    int i;
    CHECK_READ(read_int(&rd,&count));
    CHECK_READ(read_int(&rd,&naddrs));
    CHECK_READ(read_int(&rd,&fromtype));
    assert(0 < naddrs);

    if (count > array_count(tsk->unack_msg_acount[from])) {
      fprintf(tsk->output,"ACK: count = %d, array count = %d\n",
              count,array_count(tsk->unack_msg_acount[from]));
    }

    CHECK_EXPR(count <= array_count(tsk->unack_msg_acount[from]));

    remove = 0;
    for (i = 0; i < count; i++)
      remove += array_item(tsk->unack_msg_acount[from],i,int);


    /* temp */
    assert(1 == count);
    assert(naddrs == array_item(tsk->unack_msg_acount[from],0,int));


    CHECK_EXPR(remove <= array_count(tsk->inflight_addrs[from]));

    array_remove_items(tsk->unack_msg_acount[from],count);
    array_remove_items(tsk->inflight_addrs[from],remove);
    break;
  }
  case MSG_STARTDISTGC: {
    CHECK_EXPR(!tsk->indistgc);
    tsk->indistgc = 1;
    clear_marks(tsk,FLAG_DMB);
    msg_send(tsk,(tsk->pid+1)%tsk->groupsize,MSG_STARTDISTGC,data,size);
    #ifdef DISTGC_DEBUG
    fprintf(tsk->output,"Started distributed garbage collection\n");
    #endif
    break;
  }
  case MSG_MARKROOTS: { /* An RMT (Root Marking Task) */
    list *l;
    int pid;
    int i;
    int h;
    global *glo;

    CHECK_EXPR(tsk->indistgc);

    /* sanity check: shouldn't have any pending mark messages at this point */
    for (pid = 0; pid < tsk->groupsize; pid++)
      assert(0 == array_count(tsk->distmarks[pid]));
    tsk->inmark = 1;

    tsk->memdebug = 1;
    mark_roots(tsk,FLAG_DMB);
    tsk->memdebug = 0;

    /* mark any in-flight gaddrs that refer to remote objects */
    for (l = tsk->inflight; l; l = l->next) {
      gaddr *addr = (gaddr*)l->data;
      if ((0 <= addr->lid) && (tsk->pid != addr->pid))
        add_pending_mark(tsk,*addr);
    }

    for (pid = 0; pid < tsk->groupsize; pid++) {
      for (i = 0; i < array_count(tsk->inflight_addrs[pid]); i++) {
        gaddr addr = array_item(tsk->inflight_addrs[pid],i,gaddr);
        if ((0 <= addr.lid) && (tsk->pid != addr.pid))
          add_pending_mark(tsk,addr);
      }
    }

    /* make sure that for all marked globals, the pointer is also marked */
    for (h = 0; h < GLOBAL_HASH_SIZE; h++)
      for (glo = tsk->pntrhash[h]; glo; glo = glo->pntrnext)
        if (glo->flags & FLAG_DMB)
          mark_global(tsk,glo,FLAG_DMB);


    tsk->gcsent[tsk->pid]--;
    send_mark_messages(tsk);
    send_update(tsk);
    tsk->inmark = 0;
    break;
  }
  case MSG_MARKENTRY: { /* A DAMT (Distributed Address Marking Task) */
    global *glo;
    gaddr addr;
    int pid;
    CHECK_EXPR(tsk->indistgc);

    /* sanity check: shouldn't have any pending mark messages at this point */
    for (pid = 0; pid < tsk->groupsize; pid++)
      assert(0 == array_count(tsk->distmarks[pid]));
    tsk->inmark = 1;

    start_address_reading(tsk,from,tag);
    while (rd.pos < rd.size) {
      CHECK_READ(read_gaddr(&rd,tsk,&addr));
      CHECK_EXPR(addr.pid == tsk->pid);
      glo = addrhash_lookup(tsk,addr);
      if (!glo) {
        fprintf(tsk->output,"Marking request for deleted global %d@%d\n",
                addr.lid,addr.pid);
      }
      CHECK_EXPR(glo);
      mark_global(tsk,glo,FLAG_DMB);
      tsk->gcsent[tsk->pid]--;
    }
    finish_address_reading(tsk,from,tag);

    send_mark_messages(tsk);
    send_update(tsk);
    tsk->inmark = 0;
    break;
  }
  case MSG_SWEEP: {
    CHECK_EXPR(tsk->indistgc);

    /* do sweep */
    tsk->memdebug = 1;
    msg_fsend(tsk,from,MSG_SWEEPACK,"");

    clear_marks(tsk,FLAG_MARKED);
    mark_roots(tsk,FLAG_MARKED);

    sweep(tsk);
    clear_marks(tsk,FLAG_NEW);

    tsk->memdebug = 0;
    tsk->indistgc = 0;
    #ifdef DISTGC_DEBUG
    fprintf(tsk->output,"Completed distributed garbage collection: %d cells remaining\n",
            count_alive(tsk));
    #endif
    break;
  }
  default:
    fatal("unknown message");
    break;
  }
  free(data);
  return r;
}

static void handle_message(task *tsk, int from, int tag, char *data, int size)
{
  int r = handle_message2(tsk,from,tag,data,size);
  assert(READER_OK == r);
}

#ifdef PARALLELISM_DEBUG
static int frameq_count(frameq *fq)
{
  int count = 0;
  frame *f = fq->first;
  while (f) {
    count++;
    f = f->next;
  }
  return count;
}
#endif

void *execute(task *tsk)
{
  char *msgdata;
  int msgsize;
  int from;
  int tag;
#ifdef PARALLELISM
  struct timeval lastfish;
#endif
/*   int nextcollect = COLLECT_INTERVAL; */
/*   int nops = ((bcheader*)tsk->bcdata)->nops; */
/*   int nfunctions = ((bcheader*)tsk->bcdata)->nfunctions; */
/*   int nstrings = ((bcheader*)tsk->bcdata)->nstrings; */
  int evaldoaddr = ((bcheader*)tsk->bcdata)->evaldoaddr;
  const instruction *program_ops = bc_instructions(tsk->bcdata);
  const funinfo *program_finfo = bc_funinfo(tsk->bcdata);
  frame *curf;
  const instruction *instr;
  int mycount = 0;
  static int oldused = 0;

  tsk->newfish = 1;

  lastfish.tv_sec = 0;
  lastfish.tv_usec = 0;

  tsk->curfptr = &curf;

  while (!tsk->done) {

#ifdef PARALLELISM
    if ((NULL == tsk->runnable.first) || (0 < tsk->paused)) {

      if (oldused < tsk->stats.sparksused) {
        #ifdef PARALLELISM_DEBUG
        fprintf(tsk->output,"done %d sparks; need some more; outstanding fetches = %d\n",
               tsk->stats.sparksused,tsk->stats.fetches);
        #endif
        oldused = tsk->stats.sparksused;
      }

      if (0 == tsk->paused) {
        struct timeval now;
        int diffms;

        if ((NULL == tsk->runnable.first) && (NULL != tsk->sparked.first)) {
          run_frame(tsk,tsk->sparked.first);
          continue;
        }

        if (1 == tsk->groupsize)
          return NULL;

        gettimeofday(&now,NULL);
        diffms = timeval_diffms(lastfish,now);

/*         fprintf(tsk->output,"diffms = %d, ratio = %f\n",diffms, */
/*                 ((double)diffms)/((double)FISH_DELAY_MS)); */

        if (tsk->newfish || ((double)diffms)/((double)FISH_DELAY_MS) > 0.9) {
          #ifdef PARALLELISM_DEBUG
          fprintf(tsk->output,
                  "sending FISH; outstanding fetches = %d, sparks = %d, "
                  "sparksused = %d, blocked = %d\n",
                  tsk->stats.fetches,tsk->stats.sparks,tsk->stats.sparksused,
                  frameq_count(&tsk->blocked));
          #endif
          lastfish = now; /* avoid sending another until after the sleep period */
          msg_fsend(tsk,(tsk->pid+1) % (tsk->groupsize-1),MSG_FISH,
                    "iii",tsk->pid,10,50);
          tsk->newfish = 0;
        }

        from = msg_recv(tsk,&tag,&msgdata,&msgsize,FISH_DELAY_MS);

        #ifdef PARALLELISM_DEBUG
        /* DEBUG */
        if (0 > from) {
          fprintf(tsk->output,
                  "%d: no runnable frames; slept; mycount = %d, sparks = %d, sparksused = %d"
                  ", fetches = %d\n",
                  tsk->pid,mycount,tsk->stats.sparks,tsk->stats.sparksused,tsk->stats.fetches);
        }
        /* END DEBUG */
        #endif
      }
      else {
        from = msg_recv(tsk,&tag,&msgdata,&msgsize,-1);
        assert(0 <= from);
      }
      if (0 <= from)
        handle_message(tsk,from,tag,msgdata,msgsize);
      continue;
    }
    else if (tsk->endpt->checkmsg) {
      if (0 <= (from = msg_recv(tsk,&tag,&msgdata,&msgsize,0)))
        handle_message(tsk,from,tag,msgdata,msgsize);
    }
    assert(tsk->runnable.first);
#else
    if (NULL == tsk->runnable.first)
      return;
#endif


    curf = tsk->runnable.first;
    instr = &program_ops[curf->address];
    mycount++;

    #ifdef EXECUTION_TRACE
    if (compileinfo) {
/*       print_ginstr(tsk->output,gp,curf->address,instr); */
      fprintf(tsk->output,"%-6d %s\n",curf->address,opcodes[instr->opcode]);
/*       print_stack(tsk->output,curf->data,instr->expcount,0); */
    }
    #endif

/*     if (0 == nextcollect--) { */
    if (tsk->stats.nallocs >= COLLECT_THRESHOLD) {
/*       fprintf(tsk->output,"Garbage collecting\n"); */
      local_collect(tsk);
/*       nextcollect = COLLECT_INTERVAL; */
    }

    #ifdef PROFILING
    tsk->stats.op_usage[instr->opcode]++;
    tsk->stats.usage[curf->address]++;
    #endif

    switch (instr->opcode) {
    case OP_BEGIN:
      break;
    case OP_END:
      done_frame(tsk,curf);
      curf->c->type = CELL_NIL;
      curf->c = NULL;
      frame_dealloc(tsk,curf);
/*       fprintf(tsk->output,"END\n"); */
/*       tsk->done = 1; */
      continue;
    case OP_GLOBSTART:
      break;
    case OP_EVAL: {
      pntr p;
      curf->data[instr->arg0] =
        resolve_pntr(curf->data[instr->arg0]);
      p = curf->data[instr->arg0];

      if (CELL_FRAME == pntrtype(p)) {
        frame *newf = (frame*)get_pntr(get_pntr(p)->field1);
        run_frame(tsk,newf);
        curf->waitframe = newf;
        list_push(&newf->wq.frames,curf);
        block_frame(tsk,curf);
        continue;
      }
      else if (CELL_REMOTEREF == pntrtype(p)) {
        global *target = (global*)get_pntr(get_pntr(p)->field1);
        assert(target->addr.pid != tsk->pid);

        if (!target->fetching && (0 <= target->addr.lid)) {
          global *refglo = make_global(tsk,p);
          assert(refglo->addr.pid == tsk->pid);
          msg_fsend(tsk,target->addr.pid,MSG_FETCH,"aa",target->addr,refglo->addr);

          #ifdef FETCH_DEBUG
          fprintf(tsk->output,"sent FETCH %d@%d -> %d@%d\n",
                  target->addr.lid,target->addr.pid,refglo->addr.lid,refglo->addr.pid);
          #endif

          tsk->stats.fetches++;
          target->fetching = 1;
        }
        else if (!target->fetching) {
          cell *c = get_pntr(p);
          asprintf(&c->msg,"EVAL called on me but lid unknown\n");
        }
        curf->waitglo = target;
        list_push(&target->wq.frames,curf);
        block_frame(tsk,curf);
        continue;
      }
      else {
        assert(OP_RESOLVE == program_ops[curf->address+1].opcode);
        curf->address++; // skip RESOLVE
      }
      break;
    }
    case OP_RETURN:
      frame_return(tsk,curf,curf->data[instr->expcount-1]);
      continue;
    case OP_DO: {
      cell *capholder;
      cap *cp;
      int s;
      int have;
      int arity;
      pntr p;

      p = curf->data[instr->expcount-1];
      assert(CELL_IND != pntrtype(p));

      if (instr->arg0) {
        if (CELL_FRAME == pntrtype(p)) {
          frame *newf = (frame*)get_pntr(get_pntr(p)->field1);
          pntr val;
          run_frame(tsk,newf);

          /* Deactivate the current frame */
          transfer_waiters(&curf->wq,&newf->wq);
          make_pntr(val,newf->c);
          frame_return(tsk,curf,val);
          continue;
        }

        if (CELL_CAP != pntrtype(p)) {
          frame_return(tsk,curf,p);
          continue;
        }
      }

      if (CELL_CAP != pntrtype(p))
        constant_app_error(tsk,p,instr);
      capholder = (cell*)get_pntr(p);

      cp = (cap*)get_pntr(capholder->field1);
      s = instr->expcount-1;
      have = cp->count;
      arity = cp->arity;

      if (s+have < arity) {
        /* create a new CAP with the existing CAPs arguments and those from the current
           FRAME's stack */
        int i;
        pntr rep;
        cap *newcp = cap_alloc(cp->arity,cp->address,cp->fno);
        newcp->sl = cp->sl;
        newcp->data = (pntr*)malloc((instr->expcount-1+cp->count)*sizeof(pntr));
        newcp->count = 0;
        for (i = 0; i < instr->expcount-1; i++)
          newcp->data[newcp->count++] = curf->data[i];
        for (i = 0; i < cp->count; i++)
          newcp->data[newcp->count++] = cp->data[i];

        /* replace the current FRAME with the new CAP */
        curf->c->type = CELL_CAP;
        make_pntr(curf->c->field1,newcp);
        make_pntr(rep,curf->c);
        curf->c = NULL;

        /* return to caller */
        resume_waiters(tsk,&curf->wq,rep);
        done_frame(tsk,curf);
        frame_dealloc(tsk,curf);
        continue;
      }
      else if (s+have == arity) {
        int i;
        int base = instr->expcount-1;
        curf->address = cp->address;
        curf->fno = cp->fno;
        pntrstack_grow(&curf->alloc,&curf->data,program_finfo[cp->fno].stacksize);
        for (i = 0; i < cp->count; i++)
          curf->data[base+i] = cp->data[i];
        // curf->address--; /* so we process the GLOBSTART */
      }
      else { /* s+have > arity */
        int newcount = instr->expcount-1;
        frame *newf = frame_alloc(tsk);
        int i;
        int extra = arity-have;
        int nfc = 0;
        newf->alloc = program_finfo[cp->fno].stacksize;
        newf->data = (pntr*)malloc(newf->alloc*sizeof(pntr));
        newf->address = cp->address;
        newf->fno = cp->fno;
        for (i = newcount-extra; i < newcount; i++)
          newf->data[nfc++] = curf->data[i];
        for (i = 0; i < cp->count; i++)
          newf->data[nfc++] = cp->data[i];

        newf->c = alloc_cell(tsk);
        newf->c->type = CELL_FRAME;
        make_pntr(newf->c->field1,newf);

        make_pntr(curf->data[newcount-extra],newf->c);
        newcount += 1-extra;


        curf->address = evaldoaddr-1;
        curf->fno = -1;

        curf->address += (newcount-1)*EVALDO_SEQUENCE_SIZE;
      }

      break;
    }
    case OP_JFUN:
      if (instr->arg1)
        curf->address = program_finfo[instr->arg0].addressne;
      else
        curf->address = program_finfo[instr->arg0].address;
      curf->fno = instr->arg0;
      pntrstack_grow(&curf->alloc,&curf->data,program_finfo[curf->fno].stacksize);
      // curf->address--; /* so we process the GLOBSTART */
      break;
    case OP_JFALSE: {
      pntr test = curf->data[instr->expcount-1];
      assert(CELL_IND != pntrtype(test));
      assert(CELL_APPLICATION != pntrtype(test));
      assert(CELL_FRAME != pntrtype(test));

      if (CELL_CAP == pntrtype(test))
        cap_error(tsk,test,instr);

      if (CELL_NIL == pntrtype(test))
        curf->address += instr->arg0-1;
      break;
    }
    case OP_JUMP:
      curf->address += instr->arg0-1;
      break;
    case OP_PUSH:
      curf->data[instr->expcount] = curf->data[instr->arg0];
      break;
    case OP_POP:
      break;
    case OP_UPDATE: {
      pntr targetp;
      cell *target;
      pntr res;

      targetp = curf->data[instr->arg0];
      assert(CELL_HOLE == pntrtype(targetp));
      target = get_pntr(targetp);

      res = resolve_pntr(curf->data[instr->expcount-1]);
      if (pntrequal(targetp,res)) {
        fprintf(stderr,"Attempt to update cell with itself\n");
        exit(1);
      }
      target->indsource = 3;
      target->type = CELL_IND;
      target->field1 = res;
      curf->data[instr->arg0] = res;
      break;
    }
    case OP_ALLOC: {
      int i;
      for (i = 0; i < instr->arg0; i++) {
        cell *hole = alloc_cell(tsk);
        hole->type = CELL_HOLE;
        make_pntr(curf->data[instr->expcount+i],hole);
      }
      break;
    }
    case OP_SQUEEZE: {
      int count = instr->arg0;
      int remove = instr->arg1;
      int base = instr->expcount-count-remove;
      int i;
      for (i = 0; i < count; i++)
        curf->data[base+i] = curf->data[base+i+remove];
      break;
    }
    case OP_MKCAP: {
      int fno = instr->arg0;
      int n = instr->arg1;
      int i;
      cell *capv;
      cap *c = cap_alloc(program_finfo[fno].arity,program_finfo[fno].address,fno);
      c->sl.fileno = instr->fileno;
      c->sl.lineno = instr->lineno;
      c->data = (pntr*)malloc(n*sizeof(pntr));
      c->count = 0;
      for (i = instr->expcount-n; i < instr->expcount; i++)
        c->data[c->count++] = curf->data[i];

      capv = alloc_cell(tsk);
      capv->type = CELL_CAP;
      make_pntr(capv->field1,c);
      make_pntr(curf->data[instr->expcount-n],capv);
      break;
    }
    case OP_MKFRAME: {
      int fno = instr->arg0;
      int n = instr->arg1;
      cell *newfholder;
      int i;
      frame *newf = frame_alloc(tsk);
      int nfc = 0;
      newf->alloc = program_finfo[fno].stacksize;
      newf->data = (pntr*)malloc(newf->alloc*sizeof(pntr));

      newf->address = program_finfo[fno].address;
      newf->fno = fno;

      newfholder = alloc_cell(tsk);
      newfholder->type = CELL_FRAME;
      make_pntr(newfholder->field1,newf);
      newf->c = newfholder;

      for (i = instr->expcount-n; i < instr->expcount; i++)
        newf->data[nfc++] = curf->data[i];
      make_pntr(curf->data[instr->expcount-n],newfholder);
      break;
    }
    case OP_BIF: {
      int bif = instr->arg0;
      int nargs = builtin_info[bif].nargs;
      int i;
      #ifndef NDEBUG
      for (i = 0; i < builtin_info[bif].nstrict; i++) {
        assert(CELL_APPLICATION != pntrtype(curf->data[instr->expcount-1-i]));
        assert(CELL_FRAME != pntrtype(curf->data[instr->expcount-1-i]));

        if (CELL_CAP == pntrtype(curf->data[instr->expcount-1-i]))
          cap_error(tsk,curf->data[instr->expcount-1-i],instr);
      }

      for (i = 0; i < builtin_info[bif].nstrict; i++)
        assert(CELL_IND != pntrtype(curf->data[instr->expcount-1-i]));
      #endif

      builtin_info[bif].f(tsk,&curf->data[instr->expcount-nargs]);

      assert(!builtin_info[bif].reswhnf ||
             (CELL_IND != pntrtype(curf->data[instr->expcount-nargs])));
      break;
    }
    case OP_PUSHNIL:
      curf->data[instr->expcount] = tsk->globnilpntr;
      break;
    case OP_PUSHNUMBER:
      curf->data[instr->expcount] = *((double*)&instr->arg0);
      break;
    case OP_PUSHSTRING: {
      curf->data[instr->expcount] = tsk->strings[instr->arg0];
      break;
    }
    case OP_RESOLVE:
      curf->data[instr->arg0] =
        resolve_pntr(curf->data[instr->arg0]);
      assert(CELL_REMOTEREF != pntrtype(curf->data[instr->arg0]));
      break;
    case OP_ERROR:
      handle_error(tsk);
      break;
    default:
      abort();
      break;
    }

    curf->address++;
  }

  if (1 < tsk->groupsize) {
    fprintf(tsk->output,"%d: finished execution, waiting for shutdown\n",tsk->pid);
    while (!tsk->done) {
      if (0 <= (from = msg_recv(tsk,&tag,&msgdata,&msgsize,-1)))
        handle_message(tsk,from,tag,msgdata,msgsize);
    }
  }

  return NULL;
}

void run(const char *bcdata, int bcsize, FILE *statsfile, int *usage)
{
  frame *initial;
  task *tsk;

  tsk = task_new(0,1,bcdata,bcsize,0);
  tsk->output = stdout;

  initial = frame_alloc(tsk);
  initial->address = 0;
  initial->fno = -1;
  initial->data = (pntr*)malloc(sizeof(pntr));
  initial->alloc = 1;
  initial->c = alloc_cell(tsk);
  initial->c->type = CELL_FRAME;
  make_pntr(initial->c->field1,initial);
  run_frame(tsk,initial);

  execute(tsk);

  if (NULL != statsfile)
    statistics(tsk,statsfile);

  if (NULL != usage) {
    const bcheader *bch = (const bcheader*)bcdata;
    memcpy(usage,tsk->stats.usage,bch->nops*sizeof(int));
  }

  task_free(tsk);
/*   printf("Done!\n"); */
}

