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
#include "messages.h"
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
#include <sys/time.h>
#include <time.h>
#include <signal.h>

/* #define PARALLELISM_DEBUG */
/* #define FETCH_DEBUG */

#define FISH_DELAY_MS 250
#define GC_DELAY 2500
#define NSPARKS_REQUESTED 50

inline void op_begin(task *tsk, frame *runnable, const instruction *instr)
  __attribute__ ((always_inline));
inline void op_end(task *tsk, frame *runnable, const instruction *instr)
  __attribute__ ((always_inline));
inline void op_globstart(task *tsk, frame *runnable, const instruction *instr)
  __attribute__ ((always_inline));
inline void op_spark(task *tsk, frame *runnable, const instruction *instr)
  __attribute__ ((always_inline));
inline void op_return(task *tsk, frame *runnable, const instruction *instr)
  __attribute__ ((always_inline));
inline void op_do(task *tsk, frame *runnable, const instruction *instr)
  __attribute__ ((always_inline));
inline void op_jfun(task *tsk, frame *runnable, const instruction *instr)
  __attribute__ ((always_inline));
inline void op_jfalse(task *tsk, frame *runnable, const instruction *instr)
  __attribute__ ((always_inline));
inline void op_jump(task *tsk, frame *runnable, const instruction *instr)
  __attribute__ ((always_inline));
inline void op_push(task *tsk, frame *runnable, const instruction *instr)
  __attribute__ ((always_inline));
inline void op_update(task *tsk, frame *runnable, const instruction *instr)
  __attribute__ ((always_inline));
inline void op_alloc(task *tsk, frame *runnable, const instruction *instr)
  __attribute__ ((always_inline));
inline void op_squeeze(task *tsk, frame *runnable, const instruction *instr)
  __attribute__ ((always_inline));
inline void op_mkcap(task *tsk, frame *runnable, const instruction *instr)
  __attribute__ ((always_inline));
inline void op_mkframe(task *tsk, frame *runnable, const instruction *instr)
  __attribute__ ((always_inline));
inline void op_bif(task *tsk, frame *runnable, const instruction *instr)
  __attribute__ ((always_inline));
inline void op_pushnil(task *tsk, frame *runnable, const instruction *instr)
  __attribute__ ((always_inline));
inline void op_pushnumber(task *tsk, frame *runnable, const instruction *instr)
  __attribute__ ((always_inline));
inline void op_pushstring(task *tsk, frame *runnable, const instruction *instr)
  __attribute__ ((always_inline));
inline void op_pop(task *tsk, frame *runnable, const instruction *instr)
  __attribute__ ((always_inline));
inline void op_error(task *tsk, frame *runnable, const instruction *instr)
  __attribute__ ((always_inline));
inline void op_eval(task *tsk, frame *runnable, const instruction *instr)
  __attribute__ ((always_inline));
inline void op_call(task *tsk, frame *runnable, const instruction *instr)
  __attribute__ ((always_inline));

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

void print_task_sourceloc(task *tsk, FILE *f, sourceloc sl)
{
  if (0 <= sl.fileno)
    fprintf(f,"%s:%d: ",bc_string(tsk->bcdata,sl.fileno),sl.lineno);
}

void cap_error(task *tsk, pntr cappntr)
{
  assert(CELL_CAP == pntrtype(cappntr));
  cell *capval = get_pntr(cappntr);
  cap *c = (cap*)get_pntr(capval->field1);
  const char *name = bc_function_name(tsk->bcdata,c->fno);
  int nargs = c->arity;

  if (0 <= c->sl.fileno) {
    set_error(tsk,"Attempt to evaluate incomplete function application\n"
              "%s:%d: %s requires %d args, only have %d",
              bc_string(tsk->bcdata,c->sl.fileno),c->sl.lineno,
              name,nargs,c->count);
  }
  else {
    set_error(tsk,"Attempt to evaluate incomplete function application\n"
              "%s requires %d args, only have %d",
              name,nargs,c->count);
  }
}

static void constant_app_error(task *tsk, pntr cappntr, const instruction *op)
{
  sourceloc sl;
  sl.fileno = op->fileno;
  sl.lineno = op->lineno;
  print_task_sourceloc(tsk,stderr,sl);
  fprintf(stderr,CONSTANT_APP_MSG"\n");
}

void handle_error(task *tsk)
{
  print_task_sourceloc(tsk,stderr,tsk->errorsl);
  fprintf(stderr,"%s\n",tsk->error);
  *tsk->endpt->interruptptr = 1;
  tsk->done = 1;
}

static void add_waiter_frame(waitqueue *wq, frame *f)
{
  assert(NULL == f->waitlnk);
  f->waitlnk = wq->frames;
  wq->frames = f;
}

void resume_waiters(task *tsk, waitqueue *wq, pntr obj)
{
  while (wq->frames) {
    frame *f = wq->frames;
    wq->frames = f->waitlnk;
    f->waitglo = NULL;
    f->waitlnk = NULL;
    unblock_frame(tsk,f);
  }

  if (wq->fetchers) {
    list *l;
    obj = resolve_pntr(obj);
    for (l = wq->fetchers; l; l = l->next) {
      gaddr *ft = (gaddr*)l->data;
      assert(CELL_FRAME != pntrtype(obj));
      /*     fprintf(tsk->output,"1: responding with %s\n",cell_types[pntrtype(obj)]); */
      msg_fsend(tsk,ft->tid,MSG_RESPOND,"pa",obj,*ft);
    }
    list_free(wq->fetchers,free);
    wq->fetchers = NULL;
  }
}

void frame_return(task *tsk, frame *curf, pntr val)
{
  #ifdef SHOW_FRAME_COMPLETION
  if (0 <= tsk->tid) {
    int valtype = pntrtype(val);
    fprintf(tsk->output,"FRAME(%d) completed; result is a %s\n",
            frame_fno(tsk,curf),cell_types[valtype]);
  }
  #endif


  assert(!is_nullpntr(val));
  if (curf->c) {
    assert(NULL == curf->retp);
    assert(CELL_FRAME == celltype(curf->c));
    curf->c->type = CELL_IND;
    curf->c->field1 = val;
    curf->c = NULL;
  }
  else {
    assert(curf->retp);
    *(curf->retp) = val;
    curf->retp = NULL;
  }

  done_frame(tsk,curf);
  resume_waiters(tsk,&curf->wq,val);
  check_runnable(tsk);
  frame_free(tsk,curf);
}

static void schedule_frame(task *tsk, frame *f, int desttsk, array *msg)
{
  global *glo;
  global *refglo;
  pntr fcp;
  gaddr addr;
  gaddr refaddr;

  /* Remove the frame from the sparked queue */
  unspark_frame(tsk,f);

  /* Create remote reference which will point to the frame on desttsk once an ACK is sent back */

  assert(f->c);
  make_pntr(fcp,f->c);
  addr.tid = desttsk;
  addr.lid = -1;
  glo = add_global(tsk,addr,fcp);

  refaddr.tid = tsk->tid;
  refaddr.lid = tsk->nextlid++;
  refglo = add_global(tsk,refaddr,fcp);

  /* Transfer the frame to the destination, and tell it to notfy us of the global address it
     assigns to the frame (which we'll store in newref) */
  #ifdef PAREXEC_DEBUG
/*   fprintf(tsk->output,"Scheduling frame %s on task %d, newrefaddr = %d@%d\n", */
/*           function_name(tsk->gp,f->fno),desttsk,newrefaddr.lid,newrefaddr.tid); */
  #endif
  write_format(msg,tsk,"pa",glo->p,refglo->addr);

  f->c->type = CELL_REMOTEREF;
  make_pntr(f->c->field1,glo);

  /* Delete the local copy of the frame */
  f->c = NULL;
  frame_free(tsk,f);
}

static void send_update(task *tsk)
{
  int i;
  int msglen = sizeof(update_msg)+tsk->groupsize*sizeof(int);
  update_msg *um = (update_msg*)malloc(msglen);
  assert(tsk->indistgc);
  assert(!endpointid_isnull(&tsk->gc));

  um->gciter = tsk->gciter;
  memcpy(um->counts,tsk->gcsent,tsk->groupsize*sizeof(int));

  node_send(tsk->n,tsk->endpt->epid.localid,tsk->gc,MSG_UPDATE,um,msglen);
  for (i = 0; i < tsk->groupsize; i++)
    tsk->gcsent[i] = 0;
  free(um);
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
  assert(0 <= addr.tid);
  assert(tsk->groupsize >= addr.tid);
  assert(tsk->inmark);
  array_append(tsk->distmarks[addr.tid],&addr,sizeof(gaddr));
}

static void send_mark_messages(task *tsk)
{
  int pid;
  for (pid = 0; pid < tsk->groupsize; pid++) {
    if (0 < array_count(tsk->distmarks[pid])) {
      int a;
      array *msg = write_start();
      int count = array_count(tsk->distmarks[pid]);
      for (a = 0; a < count; a++) {
        gaddr addr = array_item(tsk->distmarks[pid],a,gaddr);
        write_gaddr(msg,tsk,addr);
        tsk->gcsent[addr.tid]++;
      }
      msg_send(tsk,pid,MSG_MARKENTRY,msg->data,msg->nbytes);
      write_end(msg);

      tsk->distmarks[pid]->nbytes = 0;
    }
  }
}

static frame *retrieve_blocked_frame(task *tsk, int ioid)
{
  frame *f;
  assert(0 < ioid);
  assert(ioid < tsk->iocount);
  assert(tsk->ioframes[ioid].f);
  f = tsk->ioframes[ioid].f;
  tsk->ioframes[ioid].f = NULL;
  tsk->ioframes[ioid].freelnk = tsk->iofree;
  tsk->iofree = ioid;
  tsk->netpending--;
  unblock_frame_toend(tsk,f);
  check_runnable(tsk);
  return f;
}

static sysobject *get_frame_sysobject(frame *f)
{
  pntr objp;
  sysobject *so;
  assert(OP_BIF == f->instr->opcode);
  assert((B_OPENCON == f->instr->arg0) ||
         (B_READCON == f->instr->arg0) ||
         (B_PRINT == f->instr->arg0) ||
         (B_PRINTARRAY == f->instr->arg0) ||
         (B_PRINTEND == f->instr->arg0) ||
         (B_ACCEPT == f->instr->arg0) ||
         (B_STARTLISTEN == f->instr->arg0));
  assert(1 <= f->instr->expcount);
  objp = f->data[f->instr->expcount-1];
  assert(CELL_SYSOBJECT == pntrtype(objp));
  so = (sysobject*)get_pntr(get_pntr(objp)->field1);
  assert((SYSOBJECT_CONNECTION == so->type) || (SYSOBJECT_LISTENER == so->type));
  return so;
}

static void interpreter_listen_response(task *tsk, listen_response_msg *m)
{
  frame *f2 = retrieve_blocked_frame(tsk,m->ioid);
  sysobject *so = get_frame_sysobject(f2);
  assert(B_STARTLISTEN == f2->instr->arg0);
  assert(SYSOBJECT_LISTENER == so->type);

  assert(m->ioid == so->frameids[LISTEN_FRAMEADDR]);
  so->frameids[LISTEN_FRAMEADDR] = 0;

  if (m->error) {
    so->error = 1;
    memcpy(so->errmsg,m->errmsg,sizeof(so->errmsg));
  }

  so->sockid = m->sockid;
}

static void interpreter_accept_response(task *tsk, accept_response_msg *m)
{
  frame *f2 = retrieve_blocked_frame(tsk,m->ioid);
  sysobject *so = get_frame_sysobject(f2);
  sysobject *connso;
  cell *c;
  assert(B_ACCEPT == f2->instr->arg0);
  assert(SYSOBJECT_LISTENER == so->type);

  assert(m->ioid == so->frameids[ACCEPT_FRAMEADDR]);
  so->frameids[ACCEPT_FRAMEADDR] = 0;

  connso = new_sysobject(tsk,SYSOBJECT_CONNECTION,&c);
  connso->hostname = strdup(m->hostname);
  connso->port = m->port;
  connso->connected = 1;
  make_pntr(connso->listenerso,so->c);
  connso->sockid = m->sockid;
  assert(NULL == so->newso);
  so->newso = connso;
}

static void interpreter_connect_response(task *tsk, connect_response_msg *m)
{
  frame *f2 = retrieve_blocked_frame(tsk,m->ioid);
  sysobject *so = get_frame_sysobject(f2);
  assert(SYSOBJECT_CONNECTION == so->type);

  assert(B_OPENCON == f2->instr->arg0);
  assert((EVENT_CONN_ESTABLISHED == m->event) || (EVENT_CONN_FAILED == m->event));

  assert(m->ioid == so->frameids[CONNECT_FRAMEADDR]);
  so->frameids[CONNECT_FRAMEADDR] = 0;

  if (EVENT_CONN_ESTABLISHED == m->event) {
    so->connected = 1;
  }
  else {
    so->closed = 1;
    so->error = 1;
    memcpy(so->errmsg,m->errmsg,sizeof(so->errmsg));
  }

  assert(0 == so->sockid.sid);
  assert(!socketid_isnull(&m->sockid) || (EVENT_CONN_FAILED == m->event));
  so->sockid = m->sockid;
}

static void interpreter_read_response(task *tsk, read_response_msg *m)
{
  frame *f2 = retrieve_blocked_frame(tsk,m->ioid);
  sysobject *so = get_frame_sysobject(f2);
  assert(SYSOBJECT_CONNECTION == so->type);

  assert(B_READCON == f2->instr->arg0);
  assert((EVENT_DATA_READ == m->event) || (EVENT_DATA_READFINISHED == m->event));
  assert(0 == so->len);

  assert(m->ioid == so->frameids[READ_FRAMEADDR]);
  so->frameids[READ_FRAMEADDR] = 0;

  if (EVENT_DATA_READ == m->event) {
    assert(NULL == so->buf);
    assert(0 == so->len);
    so->len = m->len;
    so->buf = (char*)malloc(m->len);
    memcpy(so->buf,m->data,m->len);
  }
  else {
    assert(0 == m->len);
  }
}

static void interpreter_write_response(task *tsk, write_response_msg *m)
{
  frame *f2 = retrieve_blocked_frame(tsk,m->ioid);
  sysobject *so = get_frame_sysobject(f2);
  assert(SYSOBJECT_CONNECTION == so->type);
  assert((B_PRINT == f2->instr->arg0) || (B_PRINTARRAY == f2->instr->arg0));

  assert(m->ioid == so->frameids[WRITE_FRAMEADDR]);
  so->frameids[WRITE_FRAMEADDR] = 0;
}

static void interpreter_finwrite_response(task *tsk, finwrite_response_msg *m)
{
  frame *f2 = retrieve_blocked_frame(tsk,m->ioid);
  sysobject *so = get_frame_sysobject(f2);
  assert(SYSOBJECT_CONNECTION == so->type);
  assert(B_PRINTEND == f2->instr->arg0);

  assert(m->ioid == so->frameids[FINWRITE_FRAMEADDR]);
  so->frameids[FINWRITE_FRAMEADDR] = 0;
}

static void interpreter_connection_event(task *tsk, connection_event_msg *m)
{
  sysobject *so = find_sysobject(tsk,&m->sockid);
  if (NULL != so) {
    so->closed = 1;
    so->error = (EVENT_CONN_IOERROR == m->event);
    memcpy(so->errmsg,m->errmsg,sizeof(so->errmsg));

    if (0 != so->frameids[READ_FRAMEADDR])
      retrieve_blocked_frame(tsk,so->frameids[READ_FRAMEADDR]);
    if (0 != so->frameids[WRITE_FRAMEADDR])
      retrieve_blocked_frame(tsk,so->frameids[WRITE_FRAMEADDR]);
  }
}

static int get_idmap_index(task *tsk, endpointid epid)
{
  int i;
  for (i = 0; i < tsk->groupsize; i++)
    if (endpointid_equals(&tsk->idmap[i],&epid))
      return i;
  return -1;
}

static void interpreter_fish(task *tsk, message *msg)
{
  int r = READER_OK;
  reader rd;
  frameblock *fb;
  int i;
  int reqtsk, age, nframes;
  int scheduled = 0;
  array *newmsg;
  int from = get_idmap_index(tsk,msg->hdr.source);
  int done = 0;

  assert(0 <= from);

  rd = read_start(msg->data,msg->hdr.size);

  start_address_reading(tsk,from,msg->hdr.tag);
  r = read_format(&rd,tsk,0,"iii.",&reqtsk,&age,&nframes);
  finish_address_reading(tsk,from,msg->hdr.tag);
  assert(0 == r);

  if (reqtsk == tsk->tid)
    return;

  newmsg = write_start();

  for (fb = tsk->frameblocks; fb && !done; fb = fb->next) {
    for (i = 0; (i < tsk->framesperblock) && !done; i++) {
      frame *f = ((frame*)&fb->mem[i*tsk->framesize]);
      if (STATE_SPARKED == f->state) {
        if (1 <= nframes--) {
          schedule_frame(tsk,f,reqtsk,newmsg);
          scheduled++;
        }
        else {
          done = 1;
        }
      }
    }
  }

  if (0 < scheduled)
    msg_send(tsk,reqtsk,MSG_SCHEDULE,newmsg->data,newmsg->nbytes);
  write_end(newmsg);

  #ifdef PARALLELISM_DEBUG
  /*     fprintf(tsk->output,"received FISH from task %d; scheduled %d\n",reqtsk,scheduled); */
  #endif

  if (!scheduled && (0 < (age)--)) {
    int dest;

    /*       dest = (tsk->tid+1) % tsk->groupsize; */
    do {
      dest = rand() % tsk->groupsize;
    } while (dest == tsk->tid);

    msg_fsend(tsk,dest,MSG_FISH,"iii",reqtsk,age,nframes);
  }
}

static void interpreter_fetch(task *tsk, message *msg)
{
  pntr obj;
  gaddr storeaddr;
  gaddr objaddr;
  int r = READER_OK;
  reader rd;
  int from = get_idmap_index(tsk,msg->hdr.source);
  assert(0 <= from);

  rd = read_start(msg->data,msg->hdr.size);

  /* FETCH (objaddr) (storeaddr)
     Request to send the requested object (or a copy of it) to another task

     (objaddr)   is the global address of the object *in this task* that is to be sent

     (storeaddr) is the global address of a remote reference *in the other task*, which
     points to the requested object. When we send the response, the other address
     will store the object in the p field of the relevant "global" structure. */

  start_address_reading(tsk,from,msg->hdr.tag);
  r = read_format(&rd,tsk,0,"aa.",&objaddr,&storeaddr);
  finish_address_reading(tsk,from,msg->hdr.tag);
  assert(0 == r);

  CHECK_EXPR(objaddr.tid == tsk->tid);
  CHECK_EXPR(storeaddr.tid == from);
  obj = global_lookup_existing(tsk,objaddr);

  if (is_nullpntr(obj)) {
    fprintf(tsk->output,"Request for deleted global %d@%d, from task %d\n",
            objaddr.lid,objaddr.tid,from);
  }

  CHECK_EXPR(!is_nullpntr(obj)); /* we should only be asked for a cell we actually have */
  obj = resolve_pntr(obj);

  #ifdef FETCH_DEBUG
  fprintf(tsk->output,"received FETCH %d@%d -> %d@%d",
          objaddr.lid,objaddr.tid,storeaddr.lid,storeaddr.tid);
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
    /* Remove the frame from the sparked queue */
    unspark_frame(tsk,f);

    /* Send the actual frame */
    msg_fsend(tsk,from,MSG_RESPOND,"pa",obj,storeaddr);

    /* Check that we don't already have a reference to the remote addr
       (FIXME: is this safe?) */
    /* FIXME: there is a crash here... only happens rarely */
    ref = global_lookup_existing(tsk,storeaddr);
    CHECK_EXPR(is_nullpntr(ref));

    /* Replace our copy of a frame to a reference to the remote object */
    assert(f->c);
    make_pntr(fcp,f->c);
    glo = add_global(tsk,storeaddr,fcp);

    f->c->type = CELL_REMOTEREF;
    make_pntr(f->c->field1,glo);
    f->c = NULL;
    frame_free(tsk,f);
  }
  else {
    #ifdef PAREXEC_DEBUG
    fprintf(tsk->output,
            "Responding to fetch: val %s, req task %d, storeaddr = %d@%d, objaddr = %d@%d\n",
            cell_types[pntrtype(obj)],from,storeaddr.lid,storeaddr.tid,objaddr.lid,objaddr.tid);
    #endif
    #ifdef FETCH_DEBUG
    fprintf(tsk->output,": case 4\n");
    #endif
    msg_fsend(tsk,from,MSG_RESPOND,"pa",obj,storeaddr);
  }
}

static void interpreter_respond(task *tsk, message *msg)
{
  pntr obj;
  pntr ref;
  gaddr storeaddr;
  cell *refcell;
  global *objglo;

  int r = READER_OK;
  reader rd;
  int from = get_idmap_index(tsk,msg->hdr.source);
  assert(0 <= from);

  rd = read_start(msg->data,msg->hdr.size);

  start_address_reading(tsk,from,msg->hdr.tag);
  r = read_format(&rd,tsk,0,"pa",&obj,&storeaddr);
  finish_address_reading(tsk,from,msg->hdr.tag);
  assert(0 == r);

  ref = global_lookup_existing(tsk,storeaddr);

  CHECK_EXPR(storeaddr.tid == tsk->tid);
  if (is_nullpntr(ref)) {
    fprintf(tsk->output,"Respond tried to store in deleted global %d@%d\n",
            storeaddr.lid,storeaddr.tid);
  }
  CHECK_EXPR(!is_nullpntr(ref));
  CHECK_EXPR(CELL_REMOTEREF == pntrtype(ref));

  objglo = pglobal(ref);
  CHECK_EXPR(objglo->fetching);
  objglo->fetching = 0;
  CHECK_EXPR(objglo->addr.tid == from);
  CHECK_EXPR(objglo->addr.lid >= 0);
  CHECK_EXPR(pntrequal(objglo->p,ref));

  refcell = get_pntr(ref);
  refcell->type = CELL_IND;
  refcell->field1 = obj;

  resume_waiters(tsk,&objglo->wq,obj);

  if (CELL_FRAME == pntrtype(obj))
    run_frame(tsk,pframe(obj));
  tsk->stats.fetches--;
}

static void interpreter_schedule(task *tsk, message *msg)
{
  pntr framep;
  gaddr tellsrc;
  global *frameglo;
  array *urmsg;
  int count = 0;

  int r = READER_OK;
  reader rd;
  int from = get_idmap_index(tsk,msg->hdr.source);
  assert(0 <= from);

  rd = read_start(msg->data,msg->hdr.size);

  urmsg = write_start();

  start_address_reading(tsk,from,msg->hdr.tag);
  while (rd.pos < rd.size) {
    r = read_format(&rd,tsk,0,"pa",&framep,&tellsrc);
    assert(0 == r);

    CHECK_EXPR(CELL_FRAME == pntrtype(framep));
    frameglo = make_global(tsk,framep);
    write_format(urmsg,tsk,"aa",tellsrc,frameglo->addr);

    run_frame(tsk,pframe(framep));

    count++;
  }
  finish_address_reading(tsk,from,msg->hdr.tag);

  msg_send(tsk,from,MSG_UPDATEREF,urmsg->data,urmsg->nbytes);
  write_end(urmsg);

  fprintf(tsk->output,"Got %d new frames\n",count);
  tsk->newfish = 1;

  #ifdef PARALLELISM_DEBUG
  fprintf(tsk->output,"done processing SCHEDULE: count = %d\n",count);
  #endif
}

static void interpreter_updateref(task *tsk, message *msg)
{
  pntr ref;
  gaddr refaddr;
  gaddr remoteaddr;
  global *glo;

  int r = READER_OK;
  reader rd;
  int from = get_idmap_index(tsk,msg->hdr.source);
  assert(0 <= from);

  rd = read_start(msg->data,msg->hdr.size);

  start_address_reading(tsk,from,msg->hdr.tag);
  while (rd.pos < rd.size) {
    r = read_format(&rd,tsk,0,"aa",&refaddr,&remoteaddr);
    assert(0 == r);

    ref = global_lookup_existing(tsk,refaddr);

    CHECK_EXPR(refaddr.tid == tsk->tid);
    CHECK_EXPR(!is_nullpntr(ref));
    CHECK_EXPR(CELL_REMOTEREF == pntrtype(ref));

    glo = pglobal(ref);
    CHECK_EXPR(glo->addr.tid == from);
    CHECK_EXPR(glo->addr.lid == -1);
    CHECK_EXPR(pntrequal(glo->p,ref));
    CHECK_EXPR(remoteaddr.tid == from);

    addrhash_remove(tsk,glo);
    glo->addr.lid = remoteaddr.lid;
    addrhash_add(tsk,glo);

    resume_waiters(tsk,&glo->wq,glo->p);
  }
  finish_address_reading(tsk,from,msg->hdr.tag);
}

static void interpreter_ack(task *tsk, message *msg)
{
  int count;
  int naddrs;
  int remove;
  int fromtype;
  int i;

  int r = READER_OK;
  reader rd;
  int from = get_idmap_index(tsk,msg->hdr.source);
  assert(0 <= from);

  rd = read_start(msg->data,msg->hdr.size);



  r = read_int(&rd,&count);
  assert(0 == r);
  r = read_int(&rd,&naddrs);
  assert(0 == r);
  r = read_int(&rd,&fromtype);
  assert(0 == r);
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
}

static void interpreter_startdistgc(task *tsk, startdistgc_msg *m)
{
/*   printf("tsk->indistgc = %d\n",tsk->indistgc); */
  CHECK_EXPR(!tsk->indistgc);
  tsk->indistgc = 1;
  tsk->gc = m->gc;
  tsk->gciter = m->gciter;
  clear_marks(tsk,FLAG_DMB);

  node_send(tsk->n,tsk->endpt->epid.localid,tsk->gc,MSG_STARTDISTGCACK,NULL,0);

  #ifdef DISTGC_DEBUG
  fprintf(tsk->output,"Started distributed garbage collection\n");
  #endif
}

static void interpreter_markroots(task *tsk)
{
/* An RMT (Root Marking Task) */
  list *l;
  int pid;
  int i;
  int h;
  global *glo;

  assert(tsk->indistgc);
  assert(!endpointid_isnull(&tsk->gc));

/*   printf("markroots\n"); */

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
    if ((0 <= addr->lid) && (tsk->tid != addr->tid))
      add_pending_mark(tsk,*addr);
  }

  for (pid = 0; pid < tsk->groupsize; pid++) {
    int count = array_count(tsk->inflight_addrs[pid]);
    for (i = 0; i < count; i++) {
      gaddr addr = array_item(tsk->inflight_addrs[pid],i,gaddr);
      if ((0 <= addr.lid) && (tsk->tid != addr.tid))
        add_pending_mark(tsk,addr);
    }
  }

  /* make sure that for all marked globals, the pointer is also marked */
  for (h = 0; h < GLOBAL_HASH_SIZE; h++)
    for (glo = tsk->pntrhash[h]; glo; glo = glo->pntrnext)
      if (glo->flags & FLAG_DMB)
        mark_global(tsk,glo,FLAG_DMB);


  tsk->gcsent[tsk->tid]--;
  send_mark_messages(tsk);
  send_update(tsk);
  tsk->inmark = 0;
}

static void interpreter_markentry(task *tsk, message *msg)
{
  /* A DAMT (Distributed Address Marking Task) */
  global *glo;
  gaddr addr;
  int pid;

  int r = READER_OK;
  reader rd;
  int tag = msg->hdr.tag;
  char *data = msg->data;
  int size = msg->hdr.size;
  int from = get_idmap_index(tsk,msg->hdr.source);
  assert(0 <= from);

  assert(tsk->indistgc);
  assert(!endpointid_isnull(&tsk->gc));

  rd = read_start(data,size);

  CHECK_EXPR(tsk->indistgc);

  /* sanity check: shouldn't have any pending mark messages at this point */
  for (pid = 0; pid < tsk->groupsize; pid++)
    assert(0 == array_count(tsk->distmarks[pid]));
  tsk->inmark = 1;

  start_address_reading(tsk,from,tag);
  while (rd.pos < rd.size) {
    r = read_gaddr(&rd,tsk,&addr);
    assert(0 == r);
    CHECK_EXPR(addr.tid == tsk->tid);
    glo = addrhash_lookup(tsk,addr);
    if (!glo) {
      fprintf(tsk->output,"Marking request for deleted global %d@%d\n",
              addr.lid,addr.tid);
    }
    CHECK_EXPR(glo);
    mark_global(tsk,glo,FLAG_DMB);
    tsk->gcsent[tsk->tid]--;
  }
  finish_address_reading(tsk,from,tag);

  send_mark_messages(tsk);
  send_update(tsk);
  tsk->inmark = 0;
}

static void interpreter_sweep(task *tsk, message *msg)
{
  reader rd;
  assert(tsk->indistgc);
  assert(!endpointid_isnull(&tsk->gc));

  rd = read_start(msg->data,msg->hdr.size);

  /* do sweep */
  tsk->memdebug = 1;
  node_send(tsk->n,tsk->endpt->epid.localid,tsk->gc,MSG_SWEEPACK,NULL,0);

  clear_marks(tsk,FLAG_MARKED);
  mark_roots(tsk,FLAG_MARKED);

  sweep(tsk,0);
  clear_marks(tsk,FLAG_NEW);

  tsk->memdebug = 0;
  tsk->indistgc = 0;
  #ifdef DISTGC_DEBUG
  fprintf(tsk->output,"Completed distributed garbage collection: %d cells remaining\n",
          count_alive(tsk));
  #endif
}

static void handle_message(task *tsk, message *msg)
{
  #ifdef MSG_DEBUG
  /* FIXME: this is currently broken due to the new IO response messages. Need to somehow
     record the format a message uses */
  if (MSG_IORESPONSE != tag) {
/*     fprintf(tsk->output,"[%d] ",list_count(tsk->inflight)); */
    fprintf(tsk->output,"%d => %s ",from,msg_names[tag]);
    r = print_data(tsk,data+rd.pos,size-rd.pos);
    assert(0 == r);
    fprintf(tsk->output," (%d bytes)\n",size);
    fprintf(tsk->output,"\n");
  }
  #endif

  switch (msg->hdr.tag) {
  case MSG_DONE:
    fprintf(tsk->output,"%d: done\n",tsk->tid);
    /* FIXME: check this works! */
    /* FIXME: do we need to signal another interrupt? */
    tsk->done = 1;
    break;
  case MSG_FISH:
    interpreter_fish(tsk,msg);
    break;
  case MSG_FETCH:
    interpreter_fetch(tsk,msg);
    break;
  case MSG_RESPOND:
    interpreter_respond(tsk,msg);
    break;
  case MSG_SCHEDULE:
    interpreter_schedule(tsk,msg);
    break;
  case MSG_UPDATEREF:
    interpreter_updateref(tsk,msg);
    break;
  case MSG_ACK:
    interpreter_ack(tsk,msg);
    break;
  case MSG_STARTDISTGC:
    assert(sizeof(startdistgc_msg) == msg->hdr.size);
    interpreter_startdistgc(tsk,(startdistgc_msg*)msg->data);
    break;
  case MSG_MARKROOTS:
    interpreter_markroots(tsk);
    break;
  case MSG_MARKENTRY:
    interpreter_markentry(tsk,msg);
    break;
  case MSG_SWEEP:
    interpreter_sweep(tsk,msg);
    break;
  case MSG_LISTEN_RESPONSE:
    assert(sizeof(listen_response_msg) == msg->hdr.size);
    interpreter_listen_response(tsk,(listen_response_msg*)msg->data);
    break;
  case MSG_ACCEPT_RESPONSE:
    assert(sizeof(accept_response_msg) == msg->hdr.size);
    interpreter_accept_response(tsk,(accept_response_msg*)msg->data);
    break;
  case MSG_CONNECT_RESPONSE:
    assert(sizeof(connect_response_msg) == msg->hdr.size);
    interpreter_connect_response(tsk,(connect_response_msg*)msg->data);
    break;
  case MSG_READ_RESPONSE:
    assert(sizeof(read_response_msg) <= msg->hdr.size);
    interpreter_read_response(tsk,(read_response_msg*)msg->data);
    break;
  case MSG_WRITE_RESPONSE:
    assert(sizeof(write_response_msg) == msg->hdr.size);
    interpreter_write_response(tsk,(write_response_msg*)msg->data);
    break;
  case MSG_FINWRITE_RESPONSE:
    assert(sizeof(finwrite_response_msg) == msg->hdr.size);
    interpreter_finwrite_response(tsk,(finwrite_response_msg*)msg->data);
    break;
  case MSG_CONNECTION_EVENT:
    assert(sizeof(connection_event_msg) == msg->hdr.size);
    interpreter_connection_event(tsk,(connection_event_msg*)msg->data);
    break;
  case MSG_KILL:
    /* FIXME: ensure that when a task is killed, all connections that is has open are closed. */
    node_log(tsk->n,LOG_INFO,"task: received KILL");
    tsk->done = 1;
    break;
  case MSG_ENDPOINT_EXIT: {
    endpoint_exit_msg *m = (endpoint_exit_msg*)msg->data;
    endpointid_str str;
    assert(sizeof(endpoint_exit_msg) == msg->hdr.size);
    print_endpointid(str,m->epid);
    node_log(tsk->n,LOG_INFO,"task: received ENDPOINT_EXIT for %s",str);
    tsk->done = 1;
    break;
  }
  default:
    fatal("interpreter: unexpected message %s",msg_names[msg->hdr.tag]);
    break;
  }
  message_free(msg);
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

int handle_interrupt(task *tsk)
{
  struct timeval now;
  message *msg;

  assert((void*)1 != *tsk->runptr);

  if (tsk->done)
    return 1;

  if (tsk->alloc_bytes >= COLLECT_THRESHOLD) {
    local_collect(tsk);

    if (getenv("GC_STATS")) {
      int cells;
      int bytes;
      int alloc;
      int connections;
      int listeners;
      memusage(tsk,&cells,&bytes,&alloc,&connections,&listeners);
      printf("Collect: %d cells, %dk memory, %dk allocated, %d connections, %d listeners\n",
             cells,bytes/1024,alloc/1024,connections,listeners);
    }

    gettimeofday(&now,NULL);
    tsk->nextgc = timeval_addms(now,GC_DELAY);
  }

  if (NULL != (msg = endpoint_next_message(tsk->endpt,0))) {
    handle_message(tsk,msg);
    endpoint_interrupt(tsk->endpt); /* may be another one */
  }

  if (NULL == *tsk->runptr) {
    int diffms;
    frameblock *fb;
    int i;

    for (fb = tsk->frameblocks; fb; fb = fb->next) {
      for (i = 0; i < tsk->framesperblock; i++) {
        frame *f = ((frame*)&fb->mem[i*tsk->framesize]);
        if (STATE_SPARKED == f->state) {
          run_frame(tsk,f);
          return 0;
        }
      }
    }

    if ((1 == tsk->groupsize) && (0 == tsk->netpending))
      return 1;

    gettimeofday(&now,NULL);
    diffms = timeval_diffms(now,tsk->nextfish);

    if ((tsk->newfish || (0 >= diffms)) && (1 < tsk->groupsize)) {
      int dest;
      #ifdef PARALLELISM_DEBUG
      fprintf(tsk->output,"sending FISH; outstanding fetches = %d\n",tsk->stats.fetches);
      #endif

      /* avoid sending another until after the sleep period */

      int delay = FISH_DELAY_MS + ((rand() % 1000) * FISH_DELAY_MS / 1000);
      tsk->nextfish = timeval_addms(tsk->nextfish,delay);

/*       dest = (tsk->tid+1) % tsk->groupsize; */
      do {
        dest = rand() % tsk->groupsize;
      } while (dest == tsk->tid);

      msg_fsend(tsk,dest,MSG_FISH,
                "iii",tsk->tid,tsk->groupsize,NSPARKS_REQUESTED);
      tsk->newfish = 0;
    }

    /* Check if we've waited for more than GC_DELAY ms since the last garbage collection cycle.
       If so, cause another garbage collection to be done. Even though there may not have been
       much memory allocation done during this time, there may be socket connections lying around
       that are no longer referenced and should therefore be cleaned up. */
    if (0 >= timeval_diffms(now,tsk->nextgc)) {
      tsk->alloc_bytes = COLLECT_THRESHOLD;
      endpoint_interrupt(tsk->endpt); /* may be another one */
    }

    msg = endpoint_next_message(tsk->endpt,FISH_DELAY_MS);

    #ifdef PARALLELISM_DEBUG
    if (0 > from)
      fprintf(tsk->output,"%d: no runnable; slept; fetches = %d\n",tsk->tid,tsk->stats.fetches);
    #endif

    if (NULL != msg)
      handle_message(tsk,msg);
    endpoint_interrupt(tsk->endpt); /* still no runnable frames */
    return 0;
  }

  return 0;
}

inline void op_begin(task *tsk, frame *runnable, const instruction *instr)
{
}

inline void op_end(task *tsk, frame *runnable, const instruction *instr)
{
  frame *f = runnable;
  done_frame(tsk,f);
  check_runnable(tsk);
  assert(f->c);
  f->c->type = CELL_NIL;
  f->c = NULL;
  frame_free(tsk,f);
}

inline void op_globstart(task *tsk, frame *runnable, const instruction *instr)
{
}

inline void op_spark(task *tsk, frame *runnable, const instruction *instr)
{
  pntr p = resolve_pntr(runnable->data[instr->arg0]);
  if (CELL_FRAME == pntrtype(p))
    spark_frame(tsk,pframe(p));
}

inline void op_return(task *tsk, frame *runnable, const instruction *instr)
{
  frame_return(tsk,runnable,runnable->data[instr->expcount-1]);
}

inline void op_do(task *tsk, frame *runnable, const instruction *instr)
{
  const instruction *program_ops = bc_instructions(tsk->bcdata);
  int evaldoaddr = ((bcheader*)tsk->bcdata)->evaldoaddr;
  cell *capholder;
  cap *cp;
  int s;
  int have;
  int arity;
  pntr p;
  frame *f2 = runnable;

  p = f2->data[instr->expcount-1];
  assert(CELL_IND != pntrtype(p));

  if (instr->arg0) {
    if (CELL_FRAME == pntrtype(p)) {
      /* FIXME: this code cannot be reached, because an EVAL is always placed before it
         (to avoid the argument to DO being an IND cell). Either remove the EVAL and do a
         resolve above, or get rid of this code (and transfer_waiters(), since this is
         the only place it is called). */
      frame *newf = (frame*)get_pntr(get_pntr(p)->field1);
      pntr val;

      /* Deactivate the current frame */
      transfer_waiters(&f2->wq,&newf->wq);
      assert(newf->c);
      make_pntr(val,newf->c);
      frame_return(tsk,f2,val);

      /* Run the new frame */
      run_frame(tsk,newf);
      return;
    }

    if (CELL_CAP != pntrtype(p)) {
      frame_return(tsk,f2,p);
      return;
    }
  }

  if (CELL_CAP != pntrtype(p)) {
    /* FIXME: test this with native code */
    constant_app_error(tsk,p,instr);
    endpoint_interrupt(tsk->endpt);
    tsk->done = 1;
    return;
  }
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
    cap *newcp = cap_alloc(tsk,cp->arity,cp->address,cp->fno);
    #ifdef PROFILING
    tsk->stats.caps[cp->fno]++;
    #endif
    newcp->sl = cp->sl;
    newcp->data = (pntr*)malloc((instr->expcount-1+cp->count)*sizeof(pntr));
    newcp->count = 0;
    for (i = 0; i < instr->expcount-1; i++)
      newcp->data[newcp->count++] = f2->data[i];
    for (i = 0; i < cp->count; i++)
      newcp->data[newcp->count++] = cp->data[i];

    /* replace the current FRAME with the new CAP */
    if (f2->c) {
      f2->c->type = CELL_CAP;
      make_pntr(f2->c->field1,newcp);
      make_pntr(rep,f2->c);
      f2->c = NULL;
    }
    else {
      cell *capc = alloc_cell(tsk);
      capc->type = CELL_CAP;
      make_pntr(capc->field1,newcp);
      make_pntr(*f2->retp,capc);
      f2->retp = NULL;
    }

    /* return to caller */
    done_frame(tsk,f2);
    resume_waiters(tsk,&f2->wq,rep);
    check_runnable(tsk);
    frame_free(tsk,f2);
    return;
  }
  else if (s+have == arity) {
    int i;
    int base = instr->expcount-1;
    f2->instr = program_ops+cp->address;

    assert(OP_GLOBSTART == f2->instr->opcode);
    f2->instr++;

    #ifdef PROFILING
    if (0 <= cp->fno)
      tsk->stats.funcalls[cp->fno]++;
    #endif
    for (i = 0; i < cp->count; i++)
      f2->data[base+i] = cp->data[i];
    // f2->instr--; /* so we process the GLOBSTART */
  }
  else { /* s+have > arity */
    int newcount = instr->expcount-1;
    frame *newf = frame_new(tsk,1);
    int i;
    int extra = arity-have;
    int nfc = 0;
    assert(newf->data);
    newf->instr = program_ops+cp->address;

    assert(OP_GLOBSTART == newf->instr->opcode);
    newf->instr++;

    #ifdef PROFILING
    tsk->stats.frames[cp->fno]++;
    #endif
    for (i = newcount-extra; i < newcount; i++)
      newf->data[nfc++] = f2->data[i];
    for (i = 0; i < cp->count; i++)
      newf->data[nfc++] = cp->data[i];

    newf->c = alloc_cell(tsk);
    newf->c->type = CELL_FRAME;
    make_pntr(newf->c->field1,newf);

    make_pntr(f2->data[newcount-extra],newf->c);
    newcount += 1-extra;

    f2->instr = program_ops+evaldoaddr;

    f2->instr += (newcount-1)*EVALDO_SEQUENCE_SIZE;
  }
}

inline void op_jfun(task *tsk, frame *runnable, const instruction *instr)
{
  const instruction *program_ops = bc_instructions(tsk->bcdata);
  const funinfo *program_finfo = bc_funinfo(tsk->bcdata);
  int fno = instr->arg0;
  switch (instr->arg1) {
  case 2:
    runnable->instr = program_ops+program_finfo[fno].addressed;
    break;
  case 1:
    runnable->instr = program_ops+program_finfo[fno].addressne;
    break;
  default:
    runnable->instr = program_ops+program_finfo[fno].address;

    assert(OP_GLOBSTART == runnable->instr->opcode);
    runnable->instr++;
    break;
  }
  #ifdef PROFILING
  if (0 <= frame_fno(tsk,runnable))
    tsk->stats.funcalls[frame_fno(tsk,runnable)]++;
  #endif
  // runnable->instr--; /* so we process the GLOBSTART */
}

inline void op_jfalse(task *tsk, frame *runnable, const instruction *instr)
{
  pntr test = runnable->data[instr->expcount-1];
  assert(CELL_IND != pntrtype(test));
  assert(CELL_APPLICATION != pntrtype(test));
  assert(CELL_FRAME != pntrtype(test));

  if (CELL_CAP == pntrtype(test)) {
    cap_error(tsk,test);
    return;
  }

  if (CELL_NIL == pntrtype(test))
    runnable->instr += instr->arg0-1;
}

inline void op_jump(task *tsk, frame *runnable, const instruction *instr)
{
  runnable->instr += instr->arg0-1;
}

inline void op_push(task *tsk, frame *runnable, const instruction *instr)
{
  runnable->data[instr->expcount] = runnable->data[instr->arg0];
}

inline void op_update(task *tsk, frame *runnable, const instruction *instr)
{
  pntr targetp;
  cell *target;
  pntr res;

  targetp = runnable->data[instr->arg0];
  assert(CELL_HOLE == pntrtype(targetp));
  target = get_pntr(targetp);

  res = resolve_pntr(runnable->data[instr->expcount-1]);
  if (pntrequal(targetp,res)) {
    fprintf(stderr,"Attempt to update cell with itself\n");
    exit(1);
  }
  target->type = CELL_IND;
  target->field1 = res;
  runnable->data[instr->arg0] = res;
}

inline void op_alloc(task *tsk, frame *runnable, const instruction *instr)
{
  int i;
  for (i = 0; i < instr->arg0; i++) {
    cell *hole = alloc_cell(tsk);
    hole->type = CELL_HOLE;
    make_pntr(runnable->data[instr->expcount+i],hole);
  }
}

inline void op_squeeze(task *tsk, frame *runnable, const instruction *instr)
{
  int count = instr->arg0;
  int remove = instr->arg1;
  int base = instr->expcount-count-remove;
  int i;
  for (i = 0; i < count; i++)
    runnable->data[base+i] = runnable->data[base+i+remove];
}

inline void op_mkcap(task *tsk, frame *runnable, const instruction *instr)
{
  const funinfo *program_finfo = bc_funinfo(tsk->bcdata);
  int fno = instr->arg0;
  int n = instr->arg1;
  int i;
  cell *capv;
  cap *c = cap_alloc(tsk,program_finfo[fno].arity,program_finfo[fno].address,fno);
  #ifdef PROFILING
  tsk->stats.caps[fno]++;
  #endif
  c->sl.fileno = instr->fileno;
  c->sl.lineno = instr->lineno;
  c->data = (pntr*)malloc(n*sizeof(pntr));
  c->count = 0;
  for (i = instr->expcount-n; i < instr->expcount; i++)
    c->data[c->count++] = runnable->data[i];

  capv = alloc_cell(tsk);
  capv->type = CELL_CAP;
  make_pntr(capv->field1,c);
  make_pntr(runnable->data[instr->expcount-n],capv);
}

inline void op_mkframe(task *tsk, frame *runnable, const instruction *instr)
{
  const instruction *program_ops = bc_instructions(tsk->bcdata);
  const funinfo *program_finfo = bc_funinfo(tsk->bcdata);
  int fno = instr->arg0;
  int n = instr->arg1;
  cell *newfholder;
  int i;
  frame *newf = frame_new(tsk,1);
  int nfc = 0;

  assert(OP_GLOBSTART == program_ops[program_finfo[fno].address].opcode);

  #ifdef PROFILING
  tsk->stats.frames[fno]++;
  #endif

  newfholder = alloc_cell(tsk);
  newfholder->type = CELL_FRAME;
  make_pntr(newfholder->field1,newf);
  newf->c = newfholder;

  newf->instr = &program_ops[program_finfo[fno].address+1];
  for (i = instr->expcount-n; i < instr->expcount; i++)
    newf->data[nfc++] = runnable->data[i];
  make_pntr(runnable->data[instr->expcount-n],newfholder);
}

inline void op_bif(task *tsk, frame *runnable, const instruction *instr)
{
  int bif = instr->arg0;
  int nargs = builtin_info[bif].nargs;
  int i;
  #ifndef NDEBUG
  frame *f2 = runnable;
  for (i = 0; i < builtin_info[bif].nstrict; i++) {
    assert(CELL_APPLICATION != pntrtype(runnable->data[instr->expcount-1-i]));
    assert(CELL_FRAME != pntrtype(runnable->data[instr->expcount-1-i]));
    assert(CELL_REMOTEREF != pntrtype(runnable->data[instr->expcount-1-i]));
  }

  for (i = 0; i < builtin_info[bif].nstrict; i++)
    assert(CELL_IND != pntrtype(runnable->data[instr->expcount-1-i]));
  #endif

  builtin_info[bif].f(tsk,&runnable->data[instr->expcount-nargs]);

  #ifndef NDEBUG
  assert(!builtin_info[bif].reswhnf ||
         (CELL_IND != pntrtype(f2->data[instr->expcount-nargs])));
  #endif
}

inline void op_pushnil(task *tsk, frame *runnable, const instruction *instr)
{
  runnable->data[instr->expcount] = tsk->globnilpntr;
}

inline void op_pushnumber(task *tsk, frame *runnable, const instruction *instr)
{
  runnable->data[instr->expcount] = *((pntr*)&instr->arg0);
}

inline void op_pushstring(task *tsk, frame *runnable, const instruction *instr)
{
  runnable->data[instr->expcount] = tsk->strings[instr->arg0];
}

inline void op_pop(task *tsk, frame *runnable, const instruction *instr)
{
}

inline void op_error(task *tsk, frame *runnable, const instruction *instr)
{
  handle_error(tsk);
}

inline void op_eval(task *tsk, frame *runnable, const instruction *instr)
{
  pntr p;
  runnable->data[instr->arg0] = resolve_pntr(runnable->data[instr->arg0]);
  p = runnable->data[instr->arg0];

  if (CELL_FRAME == pntrtype(p)) {
    frame *newf = (frame*)get_pntr(get_pntr(p)->field1);
    frame *f2 = runnable;
    f2->instr--;

    add_waiter_frame(&newf->wq,f2);
    block_frame(tsk,f2);
    run_frame(tsk,newf);
    check_runnable(tsk);
    return;
  }
  else if (CELL_REMOTEREF == pntrtype(p)) {
    global *target = (global*)get_pntr(get_pntr(p)->field1);
    frame *f2 = runnable;
    assert(target->addr.tid != tsk->tid);

    if (!target->fetching && (0 <= target->addr.lid)) {
      global *refglo = make_global(tsk,p);
      assert(refglo->addr.tid == tsk->tid);
      msg_fsend(tsk,target->addr.tid,MSG_FETCH,"aa",target->addr,refglo->addr);

      #ifdef FETCH_DEBUG
      fprintf(tsk->output,"sent FETCH %d@%d -> %d@%d\n",
              target->addr.lid,target->addr.tid,refglo->addr.lid,refglo->addr.tid);
      #endif

      tsk->stats.fetches++;
      target->fetching = 1;
    }
    f2->waitglo = target;
    add_waiter_frame(&target->wq,f2);
    f2->instr--;
    block_frame(tsk,f2);
    check_runnable(tsk);
    return;
  }
}

inline void op_call(task *tsk, frame *runnable, const instruction *instr)
{
  const instruction *program_ops = bc_instructions(tsk->bcdata);
  const funinfo *program_finfo = bc_funinfo(tsk->bcdata);
  frame *f2 = runnable;
  int fno = instr->arg0;
  int n = instr->arg1;
  frame *newf;
  int nfc = 0;
  int i;

  assert(OP_GLOBSTART == program_ops[program_finfo[fno].address].opcode);

  newf = frame_new(tsk,0);
  newf->instr = &program_ops[program_finfo[fno].address+1];
  newf->retp = &f2->data[instr->expcount-n];

  for (i = instr->expcount-n; i < instr->expcount; i++)
    newf->data[nfc++] = runnable->data[i];

  add_waiter_frame(&newf->wq,f2);
  block_frame(tsk,f2);
  run_frame(tsk,newf);

  #ifdef PROFILING
  tsk->stats.frames[fno]++;
  #endif
}

inline void op_jcmp(task *tsk, frame *runnable, const instruction *instr)
{
  pntr p1 = runnable->data[instr->expcount-1];
  pntr p2 = runnable->data[instr->expcount-2];
  double a;
  double b;
  int cmp = 0;
  int bif = instr->arg1;

  if ((CELL_NUMBER != pntrtype(p1)) || (CELL_NUMBER != pntrtype(p2))) {
    int t1 = pntrtype(p1);
    int t2 = pntrtype(p2);
    set_error(tsk,"%s: incompatible arguments: (%s,%s)",
              builtin_info[bif].name,cell_types[t1],cell_types[t2]);
    return;
  }

  a = pntrdouble(p1);
  b = pntrdouble(p2);

  switch (bif) {
  case B_EQ: cmp = (a == b); break;
  case B_NE: cmp = (a != b); break;
  case B_LT: cmp = (a < b); break;
  case B_LE: cmp = (a <= b); break;
  case B_GT: cmp = (a > b); break;
  case B_GE: cmp = (a >= b); break;
  default: abort(); break;
  }

  if (!cmp)
    runnable->instr += instr->arg0-1;
}

inline void op_consn(task *tsk, frame *runnable, const instruction *instr)
{
  int n = instr->arg0;
  carray *arr = carray_new(tsk,sizeof(pntr),n-1,NULL,NULL);
  int i;

  assert(n-1 == arr->alloc);
  for (i = 0; i < n-1; i++)
    ((pntr*)arr->elements)[i] = runnable->data[instr->expcount-i-1];
  arr->size = n-1;
  arr->tail = runnable->data[instr->expcount-n];
  make_aref_pntr(runnable->data[instr->expcount-n],arr->wrapper,0);

}

void make_item_frame(task *tsk, frame *runnable, int expcount, int pos)
{
  const instruction *program_ops = bc_instructions(tsk->bcdata);
  const funinfo *program_finfo = bc_funinfo(tsk->bcdata);
  const bcheader *bch = (const bcheader*)tsk->bcdata;
  cell *newfholder;
  frame *newf = frame_new(tsk,1);
  double index = pos;

  assert(OP_GLOBSTART == program_ops[program_finfo[bch->itemfno].address].opcode);

  #ifdef PROFILING
  tsk->stats.frames[bch->itemfno]++;
  #endif

  newfholder = alloc_cell(tsk);
  newfholder->type = CELL_FRAME;
  make_pntr(newfholder->field1,newf);
  newf->c = newfholder;

  newf->instr = &program_ops[program_finfo[bch->itemfno].address+1];
  newf->data[0] = runnable->data[expcount-1];
  newf->data[1] = *((pntr*)&index);

  make_pntr(runnable->data[expcount-1],newfholder);
}

inline void op_itemn(task *tsk, frame *runnable, const instruction *instr)
{
  int pos = instr->arg0;
  pntr p;
  runnable->data[instr->expcount-1] = resolve_pntr(runnable->data[instr->expcount-1]);
  p = runnable->data[instr->expcount-1];

  if (CELL_AREF == pntrtype(p)) {
    carray *arr = aref_array(p);
    int index = aref_index(p);
    if ((sizeof(pntr) == arr->elemsize) && (index+pos < arr->size)) {
      runnable->data[instr->expcount-1] = ((pntr*)arr->elements)[index+pos];
      return;
    }
  }

  make_item_frame(tsk,runnable,instr->expcount,pos);
}

inline void op_invalid(task *tsk, frame *runnable, const instruction *instr)
{
  fatal("Invalid instruction");
}

typedef void (native_fun)();

void *signal_thread(void *arg)
{
  task *tsk = (task*)arg;
  struct timespec ts;
  int million = 1000000;
  while(1) {
    ts.tv_sec = 0;
    ts.tv_nsec = 10*million;
    nanosleep(&ts,NULL);
    printf("sending sigusr1\n");
    pthread_kill(tsk->endpt->thread,SIGUSR1);\
  }
  return NULL;
}

void interpreter_sigfpe(int sig, siginfo_t *ino, void *uc1)
{
  task *tsk = pthread_getspecific(task_key);
  frame *f = *tsk->runptr;
  const instruction *instr = f->instr-1;

  if ((OP_BIF == instr->opcode) || (OP_JCMP == instr->opcode)) {
    int bif = (OP_BIF == instr->opcode) ? instr->arg0 : instr->arg1;
    pntr *argstack = &f->data[instr->expcount-2];
    if ((2 == builtin_info[bif].nargs) &&
        ((CELL_NUMBER != pntrtype(argstack[1])) || (CELL_NUMBER != pntrtype(argstack[0]))))
      set_error(tsk,"%s: incompatible arguments: (%s,%s)",
                builtin_info[bif].name,
                cell_types[pntrtype(argstack[1])],cell_types[pntrtype(argstack[0])]);
    else
      set_error(tsk,"%s: floating point exception",builtin_info[bif].name);
  }
  else {
    set_error(tsk,"floating point exception");
  }

  longjmp(tsk->jbuf,1);
}

void interpreter_thread(node *n, endpoint *endpt, void *arg)
{
  task *tsk = (task*)arg;
  frame *runnable;
  const instruction *instr;
  int interrupt = 1;
  char semdata = 0;
  #ifdef PROFILING
  const instruction *program_ops = bc_instructions(tsk->bcdata);
  #endif

  tsk->n = n;
  tsk->endpt = endpt;
  tsk->thread = pthread_self();
  pthread_setspecific(task_key,tsk);

  write(tsk->threadrunningfds[1],&semdata,1);

  read(tsk->startfds[0],&semdata,1);
  close(tsk->startfds[0]);
  close(tsk->startfds[1]);
  tsk->startfds[0] = -1;
  tsk->startfds[1] = -1;

  tsk->newfish = 1;
  tsk->endpt->interruptptr = &interrupt;

  gettimeofday(&tsk->nextfish,NULL);
  gettimeofday(&tsk->nextgc,NULL);
  tsk->nextgc = timeval_addms(tsk->nextgc,GC_DELAY);

  runnable = tsk->rtemp;
  tsk->rtemp = NULL;
  tsk->runptr = &runnable;

  if (ENGINE_NATIVE == engine_type) {
    struct timeval start;
    struct timeval end;

    array *cpucode = array_new(1,0);
    gettimeofday(&start,NULL);
    native_compile(tsk->bcdata,tsk->bcsize,cpucode,tsk);
    gettimeofday(&end,NULL);

    tsk->endpt->signal = 1;

    if (getenv("INTERRUPTS")) {
      pthread_t sigthread;
      pthread_create(&sigthread,NULL,signal_thread,tsk);
    }

    pthread_kill(tsk->thread,SIGUSR1);
    tsk->usr1setup = 1;
    ((native_fun*)tsk->code)();;
    array_free(cpucode);
  }
  else if (0 != setjmp(tsk->jbuf)) {
    /* skip any futher interpretation */
    handle_error(tsk);
  }
  else {

    while (1) {

      if (interrupt) {
        interrupt = 0;
        if (handle_interrupt(tsk))
          break;
        else
          continue;
      }

      assert(runnable);

      instr = runnable->instr;
      runnable->instr++;

#ifdef EXECUTION_TRACE
      if (compileinfo) {
        /*       print_ginstr(tsk->output,gp,runnable->address,instr); */
        fprintf(tsk->output,"%-6d %s\n",instr-program_ops,opcodes[instr->opcode]);
        /*       print_stack(tsk->output,runnable->data,instr->expcount,0); */
      }
#endif

#ifdef PROFILING 
      tsk->stats.ninstrs++;
      tsk->stats.usage[runnable->instr-program_ops-1]++;
#endif

      switch (instr->opcode) {
      case OP_BEGIN:
        op_begin(tsk,runnable,instr);
        break;
      case OP_END:
        op_end(tsk,runnable,instr);
        break;
      case OP_GLOBSTART:
        abort();
        break;
      case OP_SPARK:
        op_spark(tsk,runnable,instr);
        break;
      case OP_EVAL:
        op_eval(tsk,runnable,instr);
        break;
      case OP_RETURN:
        op_return(tsk,runnable,instr);
        break;
      case OP_DO:
        op_do(tsk,runnable,instr);
        break;
      case OP_JFUN:
        op_jfun(tsk,runnable,instr);
        break;
      case OP_JFALSE:
        op_jfalse(tsk,runnable,instr);
        break;
      case OP_JUMP:
        op_jump(tsk,runnable,instr);
        break;
      case OP_PUSH:
        op_push(tsk,runnable,instr);
        break;
      case OP_POP:
        op_pop(tsk,runnable,instr);
        break;
      case OP_UPDATE:
        op_update(tsk,runnable,instr);
        break;
      case OP_ALLOC:
        op_alloc(tsk,runnable,instr);
        break;
      case OP_SQUEEZE:
        op_squeeze(tsk,runnable,instr);
        break;
      case OP_MKCAP:
        op_mkcap(tsk,runnable,instr);
        break;
      case OP_MKFRAME:
        op_mkframe(tsk,runnable,instr);
        break;
      case OP_BIF:
        op_bif(tsk,runnable,instr);
        break;
      case OP_PUSHNIL:
        op_pushnil(tsk,runnable,instr);
        break;
      case OP_PUSHNUMBER:
        op_pushnumber(tsk,runnable,instr);
        break;
      case OP_PUSHSTRING:
        op_pushstring(tsk,runnable,instr);
        break;
      case OP_ERROR:
        op_error(tsk,runnable,instr);
        break;
      case OP_CALL:
        op_call(tsk,runnable,instr);
        break;
      case OP_JCMP:
        op_jcmp(tsk,runnable,instr);
        break;
      case OP_CONSN:
        op_consn(tsk,runnable,instr);
        break;
      case OP_ITEMN:
        op_itemn(tsk,runnable,instr);
        break;
      case OP_INVALID:
        op_invalid(tsk,runnable,instr);
        break;
      default:
        abort();
        break;
      }
    }
  }

  node_log(tsk->n,LOG_INFO,"Task completed");
  #ifdef PROFILING
  if (ENGINE_INTERPRETER == engine_type)
    print_profile(tsk);
  #endif
  tsk->done = 1;
  task_free(tsk);
}
