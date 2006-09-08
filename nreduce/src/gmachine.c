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

#include "grammar.tab.h"
#include "gcode.h"
#include "nreduce.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <assert.h>
#include <ctype.h>
#include <stdarg.h>
#include <math.h>
#include <errno.h>
#include <mpi.h>
#include <sys/types.h>
#include <unistd.h>

static int is_whnf(pntr p)
{
  if (!is_pntr(p))
    return 1;

  if (TYPE_FRAME == pntrtype(p))
    return 0;

  if (TYPE_CAP == pntrtype(p))
    return 1;

  if (TYPE_APPLICATION == pntrtype(p)) {
    int nargs = 0;
    while (TYPE_APPLICATION == pntrtype(p)) {
      p = resolve_pntr(get_pntr(p)->field1);
      nargs++;
    }

    if (TYPE_FRAME == pntrtype(p)) {
      return 0;
    }
    else {
      cap *cp;
      assert(TYPE_CAP == pntrtype(p));
      cp = (cap*)get_pntr(get_pntr(p)->field1);
      if (nargs+cp->count >= cp->arity)
        return 0;
    }
  }
  return 1;
}

static int start_addr(gprogram *gp, int fno)
{
  return gp->addressmap[fno];
}

static int end_addr(gprogram *gp, int fno)
{
  int addr = gp->addressmap[fno];
  while ((addr < gp->count) &&
         ((OP_GLOBSTART != gp->ginstrs[addr].opcode) ||
          (gp->ginstrs[addr].arg0 == fno)))
    addr++;
  return addr-1;
}

void check_stack(process *proc, frame *curf, pntr *stackdata, int stackcount, gprogram *gp)
{
  ginstr *instr;
  int i;
  if (0 <= curf->fno) {
    if ((curf->address < start_addr(gp,curf->fno)) ||
        (curf->address > end_addr(gp,curf->fno))) {
      printf("Current address %d out of range\n",curf->address);
      printf("Function: %s\n",function_name(proc->gp,curf->fno));
      printf("Function start address: %d\n",start_addr(gp,curf->fno));
      printf("Function end address: %d\n",end_addr(gp,curf->fno));
      abort();
    }
  }

  instr = &gp->ginstrs[curf->address];
  if (0 > instr->expcount)
    return;

  if (stackcount != instr->expcount) {
    printf("Instruction %d expects stack frame size %d, actually %d\n",
           curf->address,instr->expcount,stackcount);
    abort();
  }

  for (i = 0; i < instr->expcount; i++) {
    pntr p = resolve_pntr(stackdata[i]);
    int actualstatus = is_whnf(p);
    if (instr->expstatus[i] && !actualstatus) {
      printf("Instruction %d expects stack frame entry %d (%d) to be evald but it's "
             "actually a %s\n",curf->address,i,i,cell_types[pntrtype(p)]);
      abort();
    }
    if (instr->expstatus[i] && !is_pntr(p) && (TYPE_IND == pntrtype(stackdata[i]))) {
      if (OP_RESOLVE != gp->ginstrs[curf->address].opcode) {
        printf("Instruction %d expects stack frame entry %d (%d) to be evald (and it is) "
               "but the pointer in the stack is an IND\n",curf->address,i,i);
        abort();
      }
    }
  }
}

static void cap_error(process *proc, pntr cappntr, ginstr *instr)
{
  assert(TYPE_CAP == pntrtype(cappntr));
  cell *capval = get_pntr(cappntr);
  cap *c = (cap*)get_pntr(capval->field1);
  const char *name = function_name(proc->gp,c->fno);
  int nargs = function_nargs(c->fno);

  print_sourceloc(stderr,instr->sl);
  fprintf(stderr,"Attempt to evaluate incomplete function application\n");

  print_sourceloc(stderr,c->sl);
  fprintf(stderr,"%s requires %d args, only have %d\n",name,nargs,c->count);
  exit(1);
}

static void constant_app_error(pntr cappntr, ginstr *instr)
{
  print_sourceloc(stderr,instr->sl);
  fprintf(stderr,CONSTANT_APP_MSG"\n");
  print_pntr_tree(stderr,cappntr,0);
  exit(1);
}

static void resume_waiters(process *proc, waitqueue *wq, pntr obj)
{
  list *l;

  if (!wq->frames && !wq->fetchers)
    return;

  obj = resolve_pntr(obj);

  for (l = wq->frames; l; l = l->next) {
    frame *f = (frame*)l->data;
    f->waitframe = NULL;
    f->waitglo = NULL;
    unblock_frame(proc,f);
  }
  list_free(wq->frames,NULL);
  wq->frames = NULL;

  for (l = wq->fetchers; l; l = l->next) {
    gaddr *ft = (gaddr*)l->data;
    assert(TYPE_FRAME != pntrtype(obj));
/*     fprintf(proc->output,"1: responding with %s\n",cell_types[pntrtype(obj)]); */
    msg_fsend(proc,ft->pid,MSG_RESPOND,"pa",obj,*ft);
  }
  list_free(wq->fetchers,free);
  wq->fetchers = NULL;
}

static void frame_return(process *proc, frame *curf, pntr val)
{
  #ifdef SHOW_FRAME_COMPLETION
  if (0 <= proc->pid) {
    const char *name = function_name(proc->gp,curf->fno);
    int valtype = pntrtype(val);
    fprintf(proc->output,"FRAME(%s) completed; result is a %s\n",
            name,cell_types[valtype]);
  }
  #endif


  assert(!is_nullpntr(val));
  assert(0 < curf->count);
  assert(NULL != curf->c);
  assert(TYPE_FRAME == celltype(curf->c));
  curf->c->type = TYPE_IND;
  curf->c->field1 = val;
  curf->c = NULL;

  resume_waiters(proc,&curf->wq,val);
  done_frame(proc,curf);
  frame_dealloc(proc,curf);
}

static void schedule_frame(process *proc, frame *f, int destproc, array *msg)
{
  global *glo;
  global *refglo;
  pntr fcp;
  gaddr addr;
  gaddr refaddr;

  /* Remove the frame from the runnable queue */
  unspark_frame(proc,f);

  /* Create remote reference which will point to the frame on destproc once an ACK is sent back */

  make_pntr(fcp,f->c);
  addr.pid = destproc;
  addr.lid = -1;
  glo = add_global(proc,addr,fcp);

  refaddr.pid = proc->pid;
  refaddr.lid = proc->nextlid++;
  refglo = add_global(proc,refaddr,fcp);

  /* Transfer the frame to the destination, and tell it to notfy us of the global address it
     assigns to the frame (which we'll store in newref) */
  #ifdef PAREXEC_DEBUG
/*   fprintf(proc->output,"Scheduling frame %s on process %d, newrefaddr = %d@%d\n", */
/*           function_name(proc->gp,f->fno),destproc,newrefaddr.lid,newrefaddr.pid); */
  #endif
  write_format(msg,proc,"pa",glo->p,refglo->addr);

  f->c->type = TYPE_REMOTEREF;
  make_pntr(f->c->field1,glo);

  /* Delete the local copy of the frame */
  f->c = NULL;
  frame_dealloc(proc,f);
}

static void send_update(process *proc)
{
  int homesite = proc->groupsize-1;
  array *wr = write_start();
  int i;
  for (i = 0; i < proc->groupsize-1; i++) {
    write_int(wr,proc->gcsent[i]);
    proc->gcsent[i] = 0;
  }
  msg_send(proc,homesite,MSG_UPDATE,wr->data,wr->nbytes);
  write_end(wr);
}

static void start_address_reading(process *proc, int from, int msgtype)
{
  proc->ackmsg = write_start();
  proc->ackmsgsize = proc->ackmsg->nbytes;
}

static void finish_address_reading(process *proc, int from, int msgtype)
{
  int haveaddrs = (proc->ackmsg->nbytes != proc->ackmsgsize);
  int ackcount = proc->naddrsread;

  write_end(proc->ackmsg);
  proc->ackmsg = NULL;
  proc->naddrsread = 0;
  proc->ackmsgsize = 0;


  if (haveaddrs) {
    assert(0 < ackcount);
    msg_fsend(proc,from,MSG_ACK,"iii",1,ackcount,msgtype);
  }
  else {
    assert(0 == ackcount);
  }
}

void add_pending_mark(process *proc, gaddr addr)
{
  assert(0 <= addr.pid);
  assert(proc->groupsize-1 >= addr.pid);
  assert(proc->inmark);
  array_append(proc->distmarks[addr.pid],&addr,sizeof(gaddr));
}

static void send_mark_messages(process *proc)
{
  int pid;
  for (pid = 0; pid < proc->groupsize; pid++) {
    if (0 < array_count(proc->distmarks[pid])) {
      int a;
      array *msg = write_start();
      for (a = 0; a < array_count(proc->distmarks[pid]); a++) {
        gaddr addr = array_item(proc->distmarks[pid],a,gaddr);
        write_gaddr(msg,proc,addr);
        proc->gcsent[addr.pid]++;
      }
      msg_send(proc,pid,MSG_MARKENTRY,msg->data,msg->nbytes);
      write_end(msg);

      proc->distmarks[pid]->nbytes = 0;
    }
  }
}

static int handle_message2(process *proc, int from, int tag, char *data, int size)
{
  int r = READER_OK;
  reader rd = read_start(data,size);

  if ((0 > tag) || (MSG_COUNT <= tag))
    return READER_INCORRECT_CONTENTS;

  proc->stats.recvcount[tag]++;
  proc->stats.recvbytes[tag] += size;

  #ifdef MSG_DEBUG
  fprintf(proc->output,"[%d] ",list_count(proc->inflight));
  fprintf(proc->output,"%d => %s ",from,msg_names[tag]);
  CHECK_READ(print_data(proc,data+rd.pos,size-rd.pos));
  fprintf(proc->output,"\n");
  #endif

  switch (tag) {
  case MSG_ISTATS: {
    array *wr = write_start();
    int i;
    int h;
    global *glo;
    int globals = 0;

    write_int(wr,proc->stats.totalallocs);

    for (h = 0; h < GLOBAL_HASH_SIZE; h++)
      for (glo = proc->pntrhash[h]; glo; glo = glo->pntrnext)
        globals++;
    write_int(wr,globals);

    for (i = 0; i < OP_COUNT; i++)
      write_int(wr,proc->stats.op_usage[i]);
    for (i = 0; i < MSG_COUNT; i++)
      write_int(wr,proc->stats.sendcount[i]);
    for (i = 0; i < MSG_COUNT; i++)
      write_int(wr,proc->stats.sendbytes[i]);
    for (i = 0; i < MSG_COUNT; i++)
      write_int(wr,proc->stats.recvcount[i]);
    for (i = 0; i < MSG_COUNT; i++)
      write_int(wr,proc->stats.recvbytes[i]);
    msg_send(proc,from,MSG_ISTATS,wr->data,wr->nbytes);
    write_end(wr);
    break;
  }
  case MSG_ALLSTATS: {
    int totalallocs;
    int *usage;
    int op;
    array *wr;

    if (READER_OK != (r = read_int(&rd,&totalallocs)))
      return r;

    usage = (int*)calloc(OP_COUNT,sizeof(int));
    for (op = 0; op < OP_COUNT; op++) {
      if (READER_OK != (r = read_int(&rd,&usage[op])))
        return r;
    }

    if (READER_OK != r) {
      free(usage);
      return r;
    }

    totalallocs += proc->stats.totalallocs;
    for (op = 0; op < OP_COUNT; op++)
      usage[op] += proc->stats.op_usage[op];

    wr = write_start();
    write_int(wr,totalallocs);
    for (op = 0; op < OP_COUNT; op++)
      write_int(wr,usage[op]);
    msg_send(proc,(proc->pid+1)%proc->groupsize,MSG_ALLSTATS,wr->data,wr->nbytes);
    write_end(wr);
    break;
  }
  case MSG_DONE:
    proc->done = 1;
    fprintf(proc->output,"%d: done\n",proc->pid);
    break;
  case MSG_PAUSE: {
    msg_send(proc,(proc->pid+1)%proc->groupsize,MSG_PAUSE,data,size);
    proc->paused++;
    fprintf(proc->output,"%d: paused\n",proc->pid);
    break;
  }
  case MSG_RESUME:
    msg_send(proc,(proc->pid+1)%proc->groupsize,MSG_RESUME,data,size);
    fprintf(proc->output,"%d: resumed\n",proc->pid);
    proc->paused--;
    break;
  case MSG_FISH: {
    int reqproc, age, nframes;
    int scheduled = 0;
    array *msg;

    start_address_reading(proc,from,tag);
    r = read_format(&rd,proc,0,"iii.",&reqproc,&age,&nframes);
    finish_address_reading(proc,from,tag);
    CHECK_READ(r);

    if (reqproc == proc->pid)
      break;

    msg = write_start();
    while ((1 <= nframes--) && (1 <= proc->sparked.size)) {
      schedule_frame(proc,proc->sparked.last,reqproc,msg);
      scheduled++;
    }
    msg_send(proc,reqproc,MSG_SCHEDULE,msg->data,msg->nbytes);
    write_end(msg);

    if (!scheduled && (0 < (age)--))
      msg_fsend(proc,(proc->pid+1) % (proc->groupsize-1),MSG_FISH,"iii",reqproc,age,nframes);
    break;
  }
  case MSG_FETCH: {
    /* FETCH (objaddr) (storeaddr)
       Request to send the requested object (or a copy of it) to another process

       (objaddr)   is the global address of the object *in this process* that is to be sent

       (storeaddr) is the global address of a remote reference *in the other process*, which
                   points to the requested object. When we send the response, the other address
                   will store the object in the p field of the relevant "global" structure. */
    pntr obj;
    gaddr storeaddr;
    gaddr objaddr;

    start_address_reading(proc,from,tag);
    r = read_format(&rd,proc,0,"aa.",&objaddr,&storeaddr);
    finish_address_reading(proc,from,tag);
    CHECK_READ(r);

    CHECK_EXPR(objaddr.pid == proc->pid);
    CHECK_EXPR(storeaddr.pid == from);
    obj = global_lookup_existing(proc,objaddr);

    if (is_nullpntr(obj)) {
      fprintf(proc->output,"Request for deleted global %d@%d, from process %d\n",
              objaddr.lid,objaddr.pid,from);
    }

    CHECK_EXPR(!is_nullpntr(obj)); /* we should only be asked for a cell we actually have */
    obj = resolve_pntr(obj);

    if ((TYPE_REMOTEREF == pntrtype(obj)) && (0 > pglobal(obj)->addr.lid))
      add_gaddr(&pglobal(obj)->wq.fetchers,storeaddr);
    else if ((TYPE_FRAME == pntrtype(obj)) &&
             ((STATE_RUNNING == pframe(obj)->state) || (STATE_BLOCKED == pframe(obj)->state)))
      add_gaddr(&pframe(obj)->wq.fetchers,storeaddr);
    else if ((TYPE_FRAME == pntrtype(obj)) &&
             ((STATE_SPARKED == pframe(obj)->state) ||
              (STATE_NEW == pframe(obj)->state))) {
      frame *f = pframe(obj);
      pntr ref;
      global *glo;
      pntr fcp;

      /* Remove the frame from the runnable queue */
      unspark_frame(proc,f);

      /* Send the actual frame */
      msg_fsend(proc,from,MSG_RESPOND,"pa",obj,storeaddr);

      /* Check that we don't already have a reference to the remote addr
         (FIXME: is this safe?) */
      ref = global_lookup_existing(proc,storeaddr);
      CHECK_EXPR(is_nullpntr(ref));

      /* Replace our copy of a frame to a reference to the remote object */
      make_pntr(fcp,f->c);
      glo = add_global(proc,storeaddr,fcp);

      f->c->type = TYPE_REMOTEREF;
      make_pntr(f->c->field1,glo);
      f->c = NULL;
      frame_dealloc(proc,f);
    }
    else {
      #ifdef PAREXEC_DEBUG
      fprintf(proc->output,
              "Responding to fetch: val %s, req process %d, storeaddr = %d@%d, objaddr = %d@%d\n",
              cell_types[pntrtype(obj)],from,storeaddr.lid,storeaddr.pid,objaddr.lid,objaddr.pid);
      #endif
      msg_fsend(proc,from,MSG_RESPOND,"pa",obj,storeaddr);
    }
    break;
  }
  case MSG_RESPOND: {
    pntr obj;
    pntr ref;
    gaddr storeaddr;
    cell *refcell;
    global *objglo;

    start_address_reading(proc,from,tag);
    r = read_format(&rd,proc,0,"pa",&obj,&storeaddr);
    finish_address_reading(proc,from,tag);
    CHECK_READ(r);

    ref = global_lookup_existing(proc,storeaddr);

    CHECK_EXPR(storeaddr.pid == proc->pid);
    if (is_nullpntr(ref)) {
      fprintf(proc->output,"Respond tried to store in deleted global %d@%d\n",
              storeaddr.lid,storeaddr.pid);
    }
    CHECK_EXPR(!is_nullpntr(ref));
    CHECK_EXPR(TYPE_REMOTEREF == pntrtype(ref));

    objglo = pglobal(ref);
    CHECK_EXPR(objglo->fetching);
    objglo->fetching = 0;
    CHECK_EXPR(objglo->addr.pid == from);
    CHECK_EXPR(objglo->addr.lid >= 0);
    CHECK_EXPR(pntrequal(objglo->p,ref));

    refcell = get_pntr(ref);
    refcell->type = TYPE_IND;
    refcell->field1 = obj;

    resume_waiters(proc,&objglo->wq,obj);

    if (TYPE_FRAME == pntrtype(obj))
      run_frame(proc,pframe(obj));
    break;
  }
  case MSG_SCHEDULE: {
    pntr framep;
    gaddr tellsrc;
    global *frameglo;
    array *urmsg;

    urmsg = write_start();

    start_address_reading(proc,from,tag);
    while (rd.pos < rd.size) {
      r = read_format(&rd,proc,0,"pa",&framep,&tellsrc);
      CHECK_READ(r);

      CHECK_EXPR(TYPE_FRAME == pntrtype(framep));
      frameglo = make_global(proc,framep);
      write_format(urmsg,proc,"aa",tellsrc,frameglo->addr);

      run_frame(proc,pframe(framep));
    }
    finish_address_reading(proc,from,tag);

    msg_send(proc,from,MSG_UPDATEREF,urmsg->data,urmsg->nbytes);
    write_end(urmsg);

    break;
  }
  case MSG_UPDATEREF: {
    pntr ref;
    gaddr refaddr;
    gaddr remoteaddr;
    global *glo;

    start_address_reading(proc,from,tag);
    while (rd.pos < rd.size) {
      r = read_format(&rd,proc,0,"aa",&refaddr,&remoteaddr);
      CHECK_READ(r);

      ref = global_lookup_existing(proc,refaddr);

      CHECK_EXPR(refaddr.pid == proc->pid);
      CHECK_EXPR(!is_nullpntr(ref));
      CHECK_EXPR(TYPE_REMOTEREF == pntrtype(ref));

      glo = pglobal(ref);
      CHECK_EXPR(glo->addr.pid == from);
      CHECK_EXPR(glo->addr.lid == -1);
      CHECK_EXPR(pntrequal(glo->p,ref));
      CHECK_EXPR(remoteaddr.pid == from);

      addrhash_remove(proc,glo);
      glo->addr.lid = remoteaddr.lid;
      addrhash_add(proc,glo);
    }
    finish_address_reading(proc,from,tag);
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
    CHECK_EXPR(count <= array_count(proc->unack_msg_acount[from]));

    remove = 0;
    for (i = 0; i < count; i++)
      remove += array_item(proc->unack_msg_acount[from],i,int);


    /* temp */
    assert(1 == count);
    assert(naddrs == array_item(proc->unack_msg_acount[from],0,int));


    CHECK_EXPR(remove <= array_count(proc->inflight_addrs[from]));

    array_remove_items(proc->unack_msg_acount[from],count);
    array_remove_items(proc->inflight_addrs[from],remove);
    break;
  }
  case MSG_STARTDISTGC: {
    CHECK_EXPR(!proc->indistgc);
    proc->indistgc = 1;
    clear_marks(proc,FLAG_DMB);
    msg_send(proc,(proc->pid+1)%proc->groupsize,MSG_STARTDISTGC,data,size);
    #ifdef DISTGC_DEBUG
    fprintf(proc->output,"Started distributed garbage collection\n");
    #endif
    break;
  }
  case MSG_MARKROOTS: { /* An RMT (Root Marking Task) */
    list *l;
    int pid;
    int i;
    int h;
    global *glo;

    CHECK_EXPR(proc->indistgc);

    /* sanity check: shouldn't have any pending mark messages at this point */
    for (pid = 0; pid < proc->groupsize; pid++)
      assert(0 == array_count(proc->distmarks[pid]));
    proc->inmark = 1;

    proc->memdebug = 1;
    mark_roots(proc,FLAG_DMB);
    proc->memdebug = 0;

    /* mark any in-flight gaddrs that refer to remote objects */
    for (l = proc->inflight; l; l = l->next) {
      gaddr *addr = (gaddr*)l->data;
      if ((0 <= addr->lid) && (proc->pid != addr->pid))
        add_pending_mark(proc,*addr);
    }

    for (pid = 0; pid < proc->groupsize; pid++) {
      for (i = 0; i < array_count(proc->inflight_addrs[pid]); i++) {
        gaddr addr = array_item(proc->inflight_addrs[pid],i,gaddr);
        if ((0 <= addr.lid) && (proc->pid != addr.pid))
          add_pending_mark(proc,addr);
      }
    }

    /* make sure that for all marked globals, the pointer is also marked */
    for (h = 0; h < GLOBAL_HASH_SIZE; h++)
      for (glo = proc->pntrhash[h]; glo; glo = glo->pntrnext)
        if (glo->flags & FLAG_DMB)
          mark_global(proc,glo,FLAG_DMB);


    proc->gcsent[proc->pid]--;
    send_mark_messages(proc);
    send_update(proc);
    proc->inmark = 0;
    break;
  }
  case MSG_MARKENTRY: { /* A DAMT (Distributed Address Marking Task) */
    global *glo;
    gaddr addr;
    int pid;
    CHECK_EXPR(proc->indistgc);

    /* sanity check: shouldn't have any pending mark messages at this point */
    for (pid = 0; pid < proc->groupsize; pid++)
      assert(0 == array_count(proc->distmarks[pid]));
    proc->inmark = 1;

    start_address_reading(proc,from,tag);
    while (rd.pos < rd.size) {
      CHECK_READ(read_gaddr(&rd,proc,&addr));
      CHECK_EXPR(addr.pid == proc->pid);
      glo = addrhash_lookup(proc,addr);
      if (!glo) {
        fprintf(proc->output,"Marking request for deleted global %d@%d\n",
                addr.lid,addr.pid);
      }
      CHECK_EXPR(glo);
      mark_global(proc,glo,FLAG_DMB);
      proc->gcsent[proc->pid]--;
    }
    finish_address_reading(proc,from,tag);

    send_mark_messages(proc);
    send_update(proc);
    proc->inmark = 0;
    break;
  }
  case MSG_SWEEP: {
    CHECK_EXPR(proc->indistgc);

    /* do sweep */
    proc->memdebug = 1;
    msg_fsend(proc,from,MSG_SWEEPACK,"");

    clear_marks(proc,FLAG_MARKED);
    mark_roots(proc,FLAG_MARKED);

    sweep(proc);
    clear_marks(proc,FLAG_NEW);

    proc->memdebug = 0;
    proc->indistgc = 0;
    #ifdef DISTGC_DEBUG
    fprintf(proc->output,"Completed distributed garbage collection: %d cells remaining\n",
            count_alive(proc));
    #endif
    break;
  }
  case MSG_TEST: {
    int remoterefs = 0;
    int resolved = 0;
    int entries = 0;
    int globals = 0;
    int h;
    int pid;
    global *glo;
    msg_send(proc,(proc->pid+1)%proc->groupsize,MSG_TEST,data,size);
    local_collect(proc);
    fprintf(proc->output,"In-flight addresses: %d\n",list_count(proc->inflight));

    for (h = 0; h < GLOBAL_HASH_SIZE; h++) {
      for (glo = proc->pntrhash[h]; glo; glo = glo->pntrnext) {
        globals++;
        if (glo->addr.pid == proc->pid) {
          entries++;
        }
        else if ((TYPE_REMOTEREF == pntrtype(glo->p)) &&
                 ((global*)get_pntr(get_pntr(glo->p)->field1) == glo)) {
          remoterefs++;
        }
        else {
          resolved++;
        }
      }
    }

    fprintf(proc->output,"Globals: %d\n",globals);
    fprintf(proc->output,"Remote references: %d\n",remoterefs);
    fprintf(proc->output,"Resolved references: %d\n",resolved);
    fprintf(proc->output,"Entry items: %d\n",entries);

    for (pid = 0; pid < proc->groupsize; pid++) {
      fprintf(proc->output,"inflight_addrs[%d] = %-6d     unack_msg_acount[%d] = %-6d\n",
              pid,array_count(proc->inflight_addrs[pid]),
              pid,array_count(proc->unack_msg_acount[pid]));
    }

    dump_globals(proc);
    break;
  }
  case MSG_DUMP_INFO:
    msg_send(proc,(proc->pid+1)%proc->groupsize,MSG_DUMP_INFO,data,size);
    dump_info(proc);
    print_cells(proc);
    break;
  default:
    fatal("unknown message");
    break;
  }
  free(data);
  return r;
}

static void handle_message(process *proc, int from, int tag, char *data, int size)
{
  int r = handle_message2(proc,from,tag,data,size);
  assert(READER_OK == r);
}

static void execute(process *proc, gprogram *gp)
{
  char *msgdata;
  int msgsize;
  int from;
  int tag;
  int pastend = 0;
  int fishdue = 1;
  int nextcollect = COLLECT_INTERVAL;

  while (!proc->done) {
    ginstr *instr;
    frame *curf;

    if ((0 < proc->paused) || (NULL == proc->runnable.first)) {

      if (0 == proc->paused) {
        struct timeval before;
        struct timeval after;
        struct timeval diff;
        struct timespec timeout;

        if ((NULL == proc->runnable.first) && (NULL != proc->sparked.first)) {
          run_frame(proc,proc->sparked.first);
          continue;
        }

        if (1 == proc->groupsize)
          return;

        if (fishdue) {
          msg_fsend(proc,(proc->pid+1) % (proc->groupsize-1),MSG_FISH,
                    "iii",proc->pid,10,50);
          fishdue = 0; /* don't send another until after the sleep period */
        }

        gettimeofday(&before,NULL);
        timeout.tv_sec = before.tv_sec + 1;
        timeout.tv_nsec = before.tv_usec * 1000;

        from = msg_recvbt(proc,&tag,&msgdata,&msgsize,&timeout);

        gettimeofday(&after,NULL);
        diff = timeval_diff(before,after);

/*         if (0 > from) { */
        if ((1 <= diff.tv_sec) || (500000 <= diff.tv_usec)) {
          fprintf(proc->output,"%d: no runnable frames; slept\n",proc->pid);
          fishdue = 1;
        }
        else {
/*           fprintf(proc->output,"%d: no runnable frames\n",proc->pid); */
        }
      }
      else {
        from = msg_recvb(proc,&tag,&msgdata,&msgsize);
        assert(0 <= from);
      }
      if (0 <= from)
        handle_message(proc,from,tag,msgdata,msgsize);
      continue;
    }
    else {
      if (0 <= (from = msg_recv(proc,&tag,&msgdata,&msgsize)))
        handle_message(proc,from,tag,msgdata,msgsize);
    }

    assert(proc->runnable.first);

    curf = proc->runnable.first;
    instr = &gp->ginstrs[curf->address];

    if (pastend) {
      printf(", curf %p %s",curf,function_name(proc->gp,curf->fno));
      if (curf->c)
        printf(", curf->c %s",cell_types[celltype(curf->c)]);
      printf(", address %d",curf->address);
      printf(", opcode %s",op_names[instr->opcode]);
    }


    #ifdef EXECUTION_TRACE
    if (trace) {
      print_ginstr(proc->output,gp,curf->address,instr);
      print_stack(proc->output,curf->data,curf->count,0);
    }
    #endif

    #ifdef STACK_MODEL_SANITY_CHECK
    check_stack(proc,curf,curf->data,curf->count,gp);
    assert(curf->count <= curf->alloc);
    assert((0 > curf->fno) || (curf->alloc >= gp->stacksizes[curf->fno]));
    assert((0 > curf->fno) || (curf->count <= gp->stacksizes[curf->fno]));
    #endif

    if (0 == nextcollect--) {
      local_collect(proc);
      nextcollect = COLLECT_INTERVAL;
    }

    #ifdef PROFILING
    proc->stats.op_usage[instr->opcode]++;
    proc->stats.usage[curf->address]++;
    #endif

    switch (instr->opcode) {
    case OP_BEGIN:
      break;
    case OP_END:
      done_frame(proc,curf);
      curf->c->type = TYPE_NIL;
      curf->c = NULL;
      frame_dealloc(proc,curf);
/*       proc->done = 1; */
      pastend = 1;
      continue;
    case OP_LAST:
      break;
    case OP_GLOBSTART:
      break;
    case OP_EVAL: {
      pntr p;
      assert(0 <= curf->count-1-instr->arg0);
      curf->data[curf->count-1-instr->arg0] = resolve_pntr(curf->data[curf->count-1-instr->arg0]);
      p = curf->data[curf->count-1-instr->arg0];

      if (TYPE_FRAME == pntrtype(p)) {
        frame *newf = (frame*)get_pntr(get_pntr(p)->field1);
        run_frame(proc,newf);
        curf->waitframe = newf;
        list_push(&newf->wq.frames,curf);
        block_frame(proc,curf);
        continue;
      }
      else if (TYPE_REMOTEREF == pntrtype(p)) {
        global *target = (global*)get_pntr(get_pntr(p)->field1);
        assert(target->addr.pid != proc->pid);

        if (!target->fetching && (0 <= target->addr.lid)) {
          global *refglo = make_global(proc,p);
          assert(refglo->addr.pid == proc->pid);
          msg_fsend(proc,target->addr.pid,MSG_FETCH,"aa",target->addr,refglo->addr);
          target->fetching = 1;
        }
        curf->waitglo = target;
        list_push(&target->wq.frames,curf);
        block_frame(proc,curf);
        continue;
      }
      else {
        assert(OP_RESOLVE == gp->ginstrs[curf->address+1].opcode);
        curf->address++; // skip RESOLVE
      }
      break;
    }
    case OP_RETURN:
      frame_return(proc,curf,curf->data[curf->count-1]);
      continue;
    case OP_DO: {
      cell *capholder;
      cap *cp;
      int s;
      int s1;
      int a1;
      pntr p;
      assert(0 < curf->count);

      p = curf->data[curf->count-1];
      p = resolve_pntr(p); /* FIXME: temp: is this necessary? superlife needs it */
      assert(TYPE_IND != pntrtype(p));

      if (instr->arg0) {
        if (TYPE_FRAME == pntrtype(p)) {
          frame *newf = (frame*)get_pntr(get_pntr(p)->field1);
          pntr val;
          run_frame(proc,newf);

          /* Deactivate the current frame */
          transfer_waiters(&curf->wq,&newf->wq);
          make_pntr(val,newf->c);
          frame_return(proc,curf,val);
          continue;
        }

        if (TYPE_CAP != pntrtype(p)) {
          frame_return(proc,curf,p);
          continue;
        }
      }

      if (TYPE_CAP != pntrtype(p))
        constant_app_error(p,instr);
      capholder = (cell*)get_pntr(p);
      curf->count--;

      cp = (cap*)get_pntr(capholder->field1);
      s = curf->count;
      s1 = cp->count;
      a1 = cp->arity;

      if (s+s1 < a1) {
        /* create a new CAP with the existing CAPs arguments and those from the current
           FRAME's stack */
        int i;
        pntr rep;
        cap *newcp = cap_alloc(cp->arity,cp->address,cp->fno);
        newcp->sl = cp->sl;
        newcp->data = (pntr*)malloc((curf->count+cp->count)*sizeof(pntr));
        newcp->count = 0;
        for (i = 0; i < curf->count; i++)
          newcp->data[newcp->count++] = curf->data[i];
        for (i = 0; i < cp->count; i++)
          newcp->data[newcp->count++] = cp->data[i];

        /* replace the current FRAME with the new CAP */
        curf->c->type = TYPE_CAP;
        make_pntr(curf->c->field1,newcp);
        make_pntr(rep,curf->c);
        curf->c = NULL;

        /* return to caller */
        resume_waiters(proc,&curf->wq,rep);
        done_frame(proc,curf);
        frame_dealloc(proc,curf);
        continue;
      }
      else if (s+s1 == a1) {
        int i;
        curf->address = cp->address;
        curf->fno = cp->fno;
        pntrstack_grow(&curf->alloc,&curf->data,gp->stacksizes[cp->fno]);
        for (i = 0; i < cp->count; i++)
          curf->data[curf->count++] = cp->data[i];
        // curf->address--; /* so we process the GLOBSTART */
      }
      else { /* s+s1 > a1 */
        frame *newf = frame_alloc(proc);
        int i;
        int extra;
        newf->alloc = gp->stacksizes[cp->fno];
        newf->data = (pntr*)malloc(newf->alloc*sizeof(pntr));
        newf->count = 0;
        newf->address = cp->address;
        newf->fno = cp->fno;
        extra = a1-s1;
        for (i = curf->count-extra; i < curf->count; i++)
          newf->data[newf->count++] = curf->data[i];
        for (i = 0; i < cp->count; i++)
          newf->data[newf->count++] = cp->data[i];

        newf->c = alloc_cell(proc);
        newf->c->type = TYPE_FRAME;
        make_pntr(newf->c->field1,newf);

        curf->count -= extra;
        make_pntr(curf->data[curf->count],newf->c);
        curf->count++;

        curf->address = gp->evaldoaddr-1;
        curf->fno = -1;
      }

      break;
    }
    case OP_JFUN:
      curf->address = gp->addressmap[instr->arg0];
      curf->fno = instr->arg0;
      pntrstack_grow(&curf->alloc,&curf->data,gp->stacksizes[curf->fno]);
      // curf->address--; /* so we process the GLOBSTART */
      break;
    case OP_JFALSE: {
      pntr test = curf->data[curf->count-1];
      assert(TYPE_IND != pntrtype(test));
      assert(TYPE_APPLICATION != pntrtype(test));
      assert(TYPE_FRAME != pntrtype(test));

      if (TYPE_CAP == pntrtype(test))
        cap_error(proc,test,instr);

      if (TYPE_NIL == pntrtype(test))
        curf->address += instr->arg0-1;
      curf->count--;
      break;
    }
    case OP_JUMP:
      curf->address += instr->arg0-1;
      break;
    case OP_PUSH: {
      assert(instr->arg0 < curf->count);
      curf->data[curf->count] = curf->data[curf->count-1-instr->arg0];
      curf->count++;
      break;
    }
    case OP_UPDATE: {
      int n = instr->arg0;
      pntr targetp;
      cell *target;
      pntr res;
      assert(n < curf->count);
      assert(n > 0);

      targetp = curf->data[curf->count-1-n];
      assert(TYPE_HOLE == pntrtype(targetp));
      target = get_pntr(targetp);

      res = resolve_pntr(curf->data[curf->count-1]);
      if (pntrequal(targetp,res)) {
        fprintf(stderr,"Attempt to update cell with itself\n");
        exit(1);
      }
      target->type = TYPE_IND;
      target->field1 = res;
      curf->data[curf->count-1-n] = res;
      curf->count--;
      break;
    }
    case OP_ALLOC: {
      int i;
      for (i = 0; i < instr->arg0; i++) {
        cell *hole = alloc_cell(proc);
        hole->type = TYPE_HOLE;
        make_pntr(curf->data[curf->count],hole);
        curf->count++;
      }
      break;
    }
    case OP_SQUEEZE: {
      int count = instr->arg0;
      int remove = instr->arg1;
      int base = curf->count-count-remove;
      int i;
      assert(0 <= base);
      for (i = 0; i < count; i++)
        curf->data[base+i] = curf->data[base+i+remove];
      curf->count -= remove;
      break;
    }
    case OP_MKCAP: {
      int fno = instr->arg0;
      int n = instr->arg1;
      int i;
      cell *capv;
      cap *c = cap_alloc(function_nargs(fno),gp->addressmap[fno],fno);
      c->sl = instr->sl;
      c->data = (pntr*)malloc(n*sizeof(pntr));
      c->count = 0;
      for (i = curf->count-n; i < curf->count; i++)
        c->data[c->count++] = curf->data[i];
      curf->count -= n;

      capv = alloc_cell(proc);
      capv->type = TYPE_CAP;
      make_pntr(capv->field1,c);
      make_pntr(curf->data[curf->count],capv);
      curf->count++;
      break;
    }
    case OP_MKFRAME: {
      int fno = instr->arg0;
      int n = instr->arg1;
      cell *newfholder;
      int i;
      frame *newf = frame_alloc(proc);
      newf->alloc = gp->stacksizes[fno];
      newf->data = (pntr*)malloc(newf->alloc*sizeof(pntr));
      newf->count = 0;

      newf->address = gp->addressmap[fno];
      newf->fno = fno;

      newfholder = alloc_cell(proc);
      newfholder->type = TYPE_FRAME;
      make_pntr(newfholder->field1,newf);
      newf->c = newfholder;

      for (i = curf->count-n; i < curf->count; i++)
        newf->data[newf->count++] = curf->data[i];
      curf->count -= n;
      make_pntr(curf->data[curf->count],newfholder);
      curf->count++;
      break;
    }
    case OP_BIF: {
      int bif = instr->arg0;
      int nargs = builtin_info[bif].nargs;
      int i;
      assert(0 <= builtin_info[bif].nargs); /* should we support 0-arg bifs? */
      for (i = 0; i < builtin_info[bif].nstrict; i++) {
        assert(TYPE_APPLICATION != pntrtype(curf->data[curf->count-1-i]));
        assert(TYPE_FRAME != pntrtype(curf->data[curf->count-1-i]));

        if (TYPE_CAP == pntrtype(curf->data[curf->count-1-i]))
          cap_error(proc,curf->data[curf->count-1-i],instr);
      }

      for (i = 0; i < builtin_info[bif].nstrict; i++)
        assert(TYPE_IND != pntrtype(curf->data[curf->count-1-i]));

      builtin_info[bif].f(proc,&curf->data[curf->count-nargs]);
      curf->count -= (nargs-1);
      assert(!builtin_info[bif].reswhnf || (TYPE_IND != pntrtype(curf->data[curf->count-1])));
      break;
    }
    case OP_PUSHNIL:
      curf->data[curf->count++] = proc->globnilpntr;
      break;
    case OP_PUSHNUMBER:
      curf->data[curf->count++] = make_number(*((double*)&instr->arg0));
      break;
    case OP_PUSHSTRING: {
      curf->data[curf->count] = proc->strings[instr->arg0];
      curf->count++;
      break;
    }
    case OP_RESOLVE:
      curf->data[curf->count-1-instr->arg0] = resolve_pntr(curf->data[curf->count-1-instr->arg0]);
      assert(TYPE_REMOTEREF != pntrtype(curf->data[curf->count-1-instr->arg0]));
      break;
    default:
      abort();
      break;
    }

    curf->address++;
  }

  if (1 < proc->groupsize) {
    fprintf(proc->output,"%d: finished execution, waiting for shutdown\n",proc->pid);
    while (!proc->done) {
      if (0 <= (from = msg_recvb(proc,&tag,&msgdata,&msgsize)))
        handle_message(proc,from,tag,msgdata,msgsize);
    }
  }

}

#ifndef USE_MPI
static void *execthread(void *param)
{
  process *proc = (process*)param;

  if ((1 < proc->groupsize) && (proc->pid == proc->groupsize-1)) {
    console(proc);
  }
  else {
    execute(proc,proc->gp);
    fprintf(proc->output,"\n");
  }
  return NULL;
}
#endif

void run(gprogram *gp)
{
#ifdef USE_MPI


  /* FIXME: many places assume the number of gmachines processes is 1 less than proc->groupsize,
     because of the console. Need to allow the console to be present as a message
     sender/receiver, but not considered for things like distributed garbage collection. */

  process *proc;
  int rank;
  int size;
  char filename[100];
  MPI_Comm_rank(MPI_COMM_WORLD,&rank);
  MPI_Comm_size(MPI_COMM_WORLD,&size);





  proc = process_new();
  proc->pid = rank;
  proc->groupsize = size;
  proc->gp = gp;

/*   proc->output = stdout; */
  sprintf(filename,"output.%d",rank);
  proc->output = fopen(filename,"w");
  if (NULL == proc->output) {
    perror(filename);
    exit(1);
  }
  fprintf(proc->output,"%d: in run(), pid is %d\n",rank,getpid());

  MPI_Barrier(MPI_COMM_WORLD);
  fprintf(proc->output,"Past first barrier\n");
  MPI_Barrier(MPI_COMM_WORLD);
  fprintf(proc->output,"Past second barrier\n");


  int r;

  int val = 40+rank;
  if (rank == size-1)
    r = MPI_Send(&val,1,MPI_INT,0,0,MPI_COMM_WORLD);
  else
    r = MPI_Send(&val,1,MPI_INT,rank+1,0,MPI_COMM_WORLD);
  fprintf(proc->output,"MPI_Send returned %d\n",r);

  int rdval;
  MPI_Status status;
  r = MPI_Recv(&rdval,1,MPI_INT,MPI_ANY_SOURCE,MPI_ANY_TAG,MPI_COMM_WORLD,&status);
  fprintf(proc->output,"MPI_Recv returned %d\n",r);
  fprintf(proc->output,"rdval = %d\n",rdval);

  MPI_Barrier(MPI_COMM_WORLD);
  fprintf(proc->output,"Past third barrier\n");


  process_init(proc,gp);

  if (0 == rank) {
    frame *initial = frame_alloc(proc);
    initial->address = 0;
    initial->fno = -1;
    initial->data = (pntr*)malloc(sizeof(pntr));
    initial->count = 0;
    initial->alloc = 1;
    initial->c = alloc_cell(proc);
    initial->c->type = TYPE_FRAME;
    make_pntr(initial->c->field1,initial);
    run_frame(proc,initial);
  }

  fprintf(proc->output,"Before fourth barrier\n");
  MPI_Barrier(MPI_COMM_WORLD);
  fprintf(proc->output,"Past fourth barrier\n");

  fprintf(proc->output,"%d: before execute()\n",rank);
  execute(proc,proc->gp);
  fprintf(proc->output,"\n");
  fprintf(proc->output,"%d: after execute()\n",rank);

#else
  group grp;
  int i;
  pthread_t *threads;
  frame *initial;

  grp.nprocs = 5;
  grp.procs = (process**)calloc(grp.nprocs,sizeof(process*));

  printf("Initializing processes\n");
  for (i = 0; i < grp.nprocs; i++) {
    char filename[100];

    grp.procs[i] = process_new();
    grp.procs[i]->pid = i;
    grp.procs[i]->groupsize = grp.nprocs;
    grp.procs[i]->grp = &grp;
    grp.procs[i]->gp = gp;
    process_init(grp.procs[i],gp);

    if (1 == grp.nprocs) {
      grp.procs[i]->output = stdout;
    }
    else {
      sprintf(filename,"output.%d",i);
      grp.procs[i]->output = fopen(filename,"w");
      if (NULL == grp.procs[i]->output) {
        perror(filename);
        exit(1);
      }
      setbuf(grp.procs[i]->output,NULL);
    }
  }

  initial = frame_alloc(grp.procs[0]);
  initial->address = 0;
  initial->fno = -1;
  initial->data = (pntr*)malloc(sizeof(pntr));
  initial->count = 0;
  initial->alloc = 1;
  initial->c = alloc_cell(grp.procs[0]);
  initial->c->type = TYPE_FRAME;
  make_pntr(initial->c->field1,initial);
  run_frame(grp.procs[0],initial);

  printf("Creating threads\n");
  threads = (pthread_t*)malloc(grp.nprocs*sizeof(pthread_t));
  for (i = 0; i < grp.nprocs; i++) {
    if (0 != pthread_create(&threads[i],NULL,execthread,grp.procs[i])) {
      perror("pthread_create");
      exit(1);
    }
  }

  printf("Executing program\n");
  for (i = 0; i < grp.nprocs; i++)
    pthread_join(threads[i],NULL);
  free(threads);

  printf("Cleaning up\n");
  for (i = 0; i < grp.nprocs; i++) {
    if (1 < grp.nprocs)
      fclose(grp.procs[i]->output);
    process_free(grp.procs[i]);
  }
  free(grp.procs);

  printf("Done!\n");
#endif
}

