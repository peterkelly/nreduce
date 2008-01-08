/*
 * This file is part of the nreduce project
 * Copyright (C) 2006-2007 Peter Kelly (pmk@cs.adelaide.edu.au)
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

#define FISH_DELAY_MS 250
#define GC_DELAY 2500
#define NSPARKS_REQUESTED 1

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

void handle_error(task *tsk)
{
  array *buf = array_new(1,0);
  report_error_msg *rem;
  int len;
  int msglen;

  if (0 <= tsk->errorsl.fileno)
    array_printf(buf,"%s:%d: ",bc_string(tsk->bcdata,tsk->errorsl.fileno),tsk->errorsl.lineno);
  array_printf(buf,"%s",tsk->error);

  len = buf->nbytes;
  msglen = sizeof(report_error_msg)+len;
  rem = (report_error_msg*)malloc(msglen);
  rem->sockid = tsk->out_so->sockid;
  rem->rc = 1;
  rem->len = len;
  memcpy(rem->data,buf->data,len);
  endpoint_send(tsk->endpt,tsk->out_so->sockid.coordid,MSG_REPORT_ERROR,rem,msglen);
  free(rem);
  array_free(buf);

  tsk->endpt->interrupt = 1;
  tsk->done = 1;
}

void add_waiter_frame(waitqueue *wq, frame *f)
{
  assert(NULL == f->waitlnk);
  f->waitlnk = wq->frames;
  wq->frames = f;
}

void resume_fetchers(task *tsk, waitqueue *wq, pntr obj) /* Can be called from native code */
{
  list *l;
  obj = resolve_pntr(obj);
  for (l = wq->fetchers; l; l = l->next) {
    gaddr *ft = (gaddr*)l->data;
    assert(CELL_FRAME != pntrtype(obj));
    msg_fsend(tsk,ft->tid,MSG_RESPOND,"ap",*ft,obj);
  }
  list_free(wq->fetchers,free);
  wq->fetchers = NULL;
}

void resume_local_waiters(task *tsk, waitqueue *wq)
{
  while (wq->frames) {
    frame *f = wq->frames;
    assert((OP_EVAL == f->instr->opcode) || (OP_CALL == f->instr->opcode));
    wq->frames = f->waitlnk;
    f->waitglo = NULL;
    f->waitlnk = NULL;
    unblock_frame_toend(tsk,f);
  }
}

void resume_waiters(task *tsk, waitqueue *wq, pntr obj)
{
  resume_local_waiters(tsk,wq);

  if (wq->fetchers)
    resume_fetchers(tsk,wq,obj);
}

void frame_return(task *tsk, frame *curf, pntr val)
{
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

void schedule_frame(task *tsk, frame *f, int desttsk, array *msg)
{
  pntr p;

  if (NULL == f->c) {
    f->c = alloc_cell(tsk);
    f->c->type = CELL_FRAME;
    make_pntr(f->c->field1,f);

    make_pntr(p,f->c);
    assert(f->retp);
    *(f->retp) = p;
    f->retp = NULL;
  }
  else {
    make_pntr(p,f->c);
  }

  /* Remove the frame from the sparked queue */
  if (STATE_SPARKED == f->state)
    unspark_frame(tsk,f);

  /* Get the physical address of the frame's cell, which is about to become a remote reference */
  gaddr refaddr = get_physical_address(tsk,p);

  /* We can't convert waiting frames to fetchers, since the target address is currently unknown.
     Wake them up instead, so they'll handle it as a normal remote reference. */
  resume_local_waiters(tsk,&f->wq);
  assert(NULL == f->wq.frames);

  /* Transfer the frame to the destination, and tell it to notfy us of the global address it
     assigns to the frame (which we'll store in the new target) */
  write_format(msg,tsk,"pa",p,refaddr);

  /* Create a target that will hold the address of the frame once it arrives, and convert
     the frame's cell into a remote reference that points to this target */
  gaddr blankaddr = { tid: -1, lid: -1 };
  f->c->type = CELL_REMOTEREF;
  make_pntr(f->c->field1,add_target(tsk,blankaddr,p));

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

  endpoint_send(tsk->endpt,tsk->gc,MSG_UPDATE,um,msglen);
  for (i = 0; i < tsk->groupsize; i++)
    tsk->gcsent[i] = 0;
  free(um);
}

static void start_address_reading(task *tsk, int from, int msgtype)
{
  assert(0 == tsk->naddrsread);
}

static void finish_address_reading(task *tsk, int from, int msgtype)
{
  if (0 < tsk->naddrsread)
    msg_fsend(tsk,from,MSG_ACK,"iii",1,tsk->naddrsread,msgtype);
  tsk->naddrsread = 0;
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
  assert(f);
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
         (B_STARTLISTEN == f->instr->arg0) ||
         (B_JCALL == f->instr->arg0) ||
         (B_JNEW == f->instr->arg0) ||
         (B_CONNPAIR == f->instr->arg0));
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
  assert(B_ACCEPT == f2->instr->arg0);
  assert(SYSOBJECT_LISTENER == so->type);

  assert(m->ioid == so->frameids[ACCEPT_FRAMEADDR]);
  so->frameids[ACCEPT_FRAMEADDR] = 0;

  connso = new_sysobject(tsk,SYSOBJECT_CONNECTION);
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
  assert(m->ioid == so->frameids[CONNECT_FRAMEADDR]);
  so->frameids[CONNECT_FRAMEADDR] = 0;

  if (m->error) {
    so->closed = 1;
    so->error = 1;
    memcpy(so->errmsg,m->errmsg,sizeof(so->errmsg));
  }
  else {
    so->connected = 1;
  }

  assert(0 == so->sockid.sid);
  assert(!socketid_isnull(&m->sockid) || m->error);
  so->sockid = m->sockid;
}

static void interpreter_connpair_response(task *tsk, connpair_response_msg *m)
{
  frame *f = retrieve_blocked_frame(tsk,m->ioid);
  assert(OP_BIF == f->instr->opcode);
  assert(B_CONNPAIR == f->instr->arg0);
  assert(1 <= f->instr->expcount);
  if (m->error)
    set_error(tsk,"connpair: %s",m->errmsg);
  else
    f->data[f->instr->expcount-1] = mkcons(tsk,socketid_string(tsk,m->a),
                                           socketid_string(tsk,m->b));
}

static void interpreter_read_response(task *tsk, read_response_msg *m)
{
  frame *f2 = retrieve_blocked_frame(tsk,m->ioid);
  sysobject *so = get_frame_sysobject(f2);
  assert(SYSOBJECT_CONNECTION == so->type);

  assert(B_READCON == f2->instr->arg0);
  assert(0 == so->len);

  assert(m->ioid == so->frameids[READ_FRAMEADDR]);
  so->frameids[READ_FRAMEADDR] = 0;

  so->len = m->len;
  so->buf = (char*)malloc(m->len);
  memcpy(so->buf,m->data,m->len);
}

static void interpreter_write_response(task *tsk, write_response_msg *m)
{
  frame *f2 = retrieve_blocked_frame(tsk,m->ioid);
  sysobject *so = get_frame_sysobject(f2);
  assert(SYSOBJECT_CONNECTION == so->type);
  assert((B_PRINT == f2->instr->arg0) ||
         (B_PRINTARRAY == f2->instr->arg0) ||
         (B_PRINTEND == f2->instr->arg0));

  assert(m->ioid == so->frameids[WRITE_FRAMEADDR]);
  so->frameids[WRITE_FRAMEADDR] = 0;
}

static void interpreter_connection_closed(task *tsk, connection_closed_msg *m)
{
  sysobject *so = find_sysobject(tsk,&m->sockid);
  if (NULL != so) {
    so->closed = 1;
    so->error = m->error;
    memcpy(so->errmsg,m->errmsg,sizeof(so->errmsg));

    if (0 != so->frameids[READ_FRAMEADDR])
      retrieve_blocked_frame(tsk,so->frameids[READ_FRAMEADDR]);
    if (0 != so->frameids[WRITE_FRAMEADDR])
      retrieve_blocked_frame(tsk,so->frameids[WRITE_FRAMEADDR]);
  }
}

static void interpreter_jcmd_response(task *tsk, jcmd_response_msg *m, endpointid source)
{
  frame *f2 = retrieve_blocked_frame(tsk,m->ioid);
  pntr obj;

  if (m->error) {
    if (NULL == tsk->error)
      set_error(tsk,"jcall: connection to JVM failed");
    obj = tsk->globnilpntr;
  }
  else {
    char *str = mkstring(m->data,m->len);
    obj = decode_java_response(tsk,str,source);
    free(str);
  }

  if (NULL == tsk->error) {
    assert((B_JCALL == f2->instr->arg0) || (B_JNEW == f2->instr->arg0));
    if (B_JCALL == f2->instr->arg0) {
      assert(3 <= f2->instr->expcount);
      f2->data[f2->instr->expcount-3] = obj;
    }
    else {
      assert(2 <= f2->instr->expcount);
      f2->data[f2->instr->expcount-2] = obj;
    }
  }
}

static void interpreter_get_stats(task *tsk, get_stats_msg *m)
{
  get_stats_response_msg gsrm;
  frameblock *fb;
  int i;
  int connections;
  int listeners;
  memset(&gsrm,0,sizeof(gsrm));
  gsrm.epid = tsk->endpt->epid;

  /* frame statistics */
  for (fb = tsk->frameblocks; fb; fb = fb->next) {
    for (i = 0; i < tsk->framesperblock; i++) {
      frame *f = ((frame*)&fb->mem[i*tsk->framesize]);
      switch (f->state) {
      case STATE_SPARKED: gsrm.sparked++; break;
      case STATE_RUNNING: gsrm.running++; break;
      case STATE_BLOCKED: gsrm.blocked++; break;
      default: break;
      }
    }
  }

  /* memory statistics */
  memusage(tsk,&gsrm.cells,&gsrm.bytes,&gsrm.alloc,&connections,&listeners);

  endpoint_send(tsk->endpt,m->sender,MSG_GET_STATS_RESPONSE,&gsrm,sizeof(gsrm));
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
  reader rd;
  frameblock *fb;
  int i;
  int reqtsk, age, nframes;
  int scheduled = 0;
  array *newmsg;
  int from = get_idmap_index(tsk,msg->source);
  int done = 0;

  assert(0 <= from);

  rd = read_start(tsk,msg->data,msg->size);

  start_address_reading(tsk,from,msg->tag);
  read_int(&rd,&reqtsk);
  read_int(&rd,&age);
  read_int(&rd,&nframes);
  read_end(&rd);
  finish_address_reading(tsk,from,msg->tag);

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
  gaddr targetaddr;
  reader rd;
  global *fetchglo;
  int from = get_idmap_index(tsk,msg->source);
  assert(0 <= from);

  rd = read_start(tsk,msg->data,msg->size);

  /* FETCH (targetaddr) (storeaddr)
     Request to send the requested object (or a copy of it) to another task

     (targetaddr)   is the physical address of the object *in this task* that is to be sent

     (storeaddr) is the physical address of a remote reference *in the other task*, which
     points to the requested object. When we send the response, the other address
     will store the object in the p field of the relevant "global" structure. */

  start_address_reading(tsk,from,msg->tag);
  read_gaddr(&rd,&targetaddr);
  read_gaddr(&rd,&storeaddr);
  read_end(&rd);
  finish_address_reading(tsk,from,msg->tag);

  assert(targetaddr.tid == tsk->tid);
  assert(storeaddr.tid == from);
  fetchglo = addrhash_lookup(tsk,targetaddr);

  if (NULL == fetchglo)
    fatal("Request for deleted global %d@%d, from task %d",targetaddr.lid,targetaddr.tid,from);

  obj = fetchglo->p;
  assert(!is_nullpntr(obj));
  obj = resolve_pntr(obj);

  if ((CELL_REMOTEREF == pntrtype(obj)) && (0 > pglobal(obj)->addr.lid)) {
    add_gaddr(&pglobal(obj)->wq.fetchers,storeaddr);
  }
  else if ((CELL_FRAME == pntrtype(obj)) &&
           ((STATE_RUNNING == pframe(obj)->state) || (STATE_BLOCKED == pframe(obj)->state))) {
    add_gaddr(&pframe(obj)->wq.fetchers,storeaddr);
  }
  else if ((CELL_FRAME == pntrtype(obj)) &&
           ((STATE_SPARKED == pframe(obj)->state) ||
            (STATE_NEW == pframe(obj)->state))) {
    frame *f = pframe(obj);
    pntr fcp;
    global *storeglo;
    assert(f->c);
    make_pntr(fcp,f->c);

    /* Remove the frame from the sparked queue */
    unspark_frame(tsk,f);

    /* Send the actual frame */
    msg_fsend(tsk,from,MSG_RESPOND,"ap",storeaddr,obj);

    if (NULL != (storeglo = addrhash_lookup(tsk,storeaddr))) {
      /* We could already have a target for this storeaddr if a reference has bene
         passed back to us due to another function that is running in this task */
      assert(CELL_REMOTEREF == pntrtype(storeglo->p));
      f->c->type = CELL_IND;
      f->c->field1 = storeglo->p;
    }
    else {
      /* No target global for the store address yet... create one and make the frame's
         cell a remote reference */
      storeglo = add_target(tsk,storeaddr,fcp);
      f->c->type = CELL_REMOTEREF;
      make_pntr(f->c->field1,storeglo);
    }

    f->c = NULL;
    frame_free(tsk,f);
  }
  else {
    msg_fsend(tsk,from,MSG_RESPOND,"ap",storeaddr,obj);
  }
}

static void interpreter_respond(task *tsk, message *msg)
{
  pntr obj;
  gaddr storeaddr;

  reader rd;
  int from = get_idmap_index(tsk,msg->source);
  assert(0 <= from);

  rd = read_start(tsk,msg->data,msg->size);
  start_address_reading(tsk,from,msg->tag);

  /* Read the store address */
  read_gaddr(&rd,&storeaddr);
  assert(storeaddr.tid == tsk->tid);
  assert(storeaddr.lid >= 0);

  /* Look up the local reference object, whose physical address is storeaddr */
  global *reference = addrhash_lookup(tsk,storeaddr);
  assert(reference);
  assert(reference->addr.tid == storeaddr.tid);
  assert(reference->addr.lid == storeaddr.lid);
  assert(!is_nullpntr(reference->p));
  assert(CELL_REMOTEREF == pntrtype(reference->p));
  assert(physhash_lookup(tsk,reference->p) == reference);

  /* Look up the target global that the reference points to */
  global *target = pglobal(reference->p);
  assert(target != reference);
  assert(target->addr.tid != tsk->tid);
  assert(target->fetching);
  target->fetching = 0;

  /* Read the object. read_pntr() will update the reference global with the received object. */
  read_pntr(&rd,&obj);
  read_end(&rd);
  finish_address_reading(tsk,from,msg->tag);

  global *objglo = targethash_lookup(tsk,obj);
  if (objglo && (target != objglo)) {
    assert(NULL == objglo->wq.frames);
    assert(NULL == objglo->wq.fetchers);
  }

  if (target != objglo) {
      /* The object we got back has a different physical address to what we requested. Make the
         original target an indirection cell that points to it. In this case there shouldn't
         be anything waiting on the new object. */
    assert(CELL_REMOTEREF == pntrtype(target->p));
    cell *refcell = get_pntr(target->p);
    refcell->type = CELL_IND;
    refcell->field1 = obj;
  }

  /* Respond to any FETCH requests for the reference that were made by other tasks */
  resume_waiters(tsk,&target->wq,obj);

  /* If the received object is a frame, start it running */
  if (CELL_FRAME == pntrtype(obj))
    run_frame_toend(tsk,pframe(obj));
}

static void interpreter_schedule(task *tsk, message *msg)
{
  pntr framep;
  gaddr tellsrc;
  array *urmsg;
  int count = 0;

  reader rd;
  int from = get_idmap_index(tsk,msg->source);
  assert(0 <= from);

  rd = read_start(tsk,msg->data,msg->size);

  urmsg = write_start();

  start_address_reading(tsk,from,msg->tag);
  while (rd.pos < rd.size) {
    gaddr frameaddr;
    read_pntr(&rd,&framep);
    read_gaddr(&rd,&tellsrc);

    assert(CELL_FRAME == pntrtype(framep));

    frameaddr = get_physical_address(tsk,framep);
    write_format(urmsg,tsk,"aa",tellsrc,frameaddr);

    run_frame_toend(tsk,pframe(framep));

    count++;
  }
  read_end(&rd);
  finish_address_reading(tsk,from,msg->tag);

  msg_send(tsk,from,MSG_UPDATEREF,urmsg->data,urmsg->nbytes);
  write_end(urmsg);

  printf("Got %d new frames\n",count);
  tsk->newfish = 1;
}

static void interpreter_updateref(task *tsk, message *msg)
{
  gaddr refaddr;
  gaddr remoteaddr;

  reader rd;
  int from = get_idmap_index(tsk,msg->source);
  assert(0 <= from);

  /* FIXME: possible race condition:
     1. A SCHEDULEs frame to B
     2. B sends back UPDATEREF with the address, but message does not yet reach A
     3. frame migrates to C
     4. C sends back RESPOND with return value of frame, and it is processed b A
     5. UPDATEREF finally arrives from B; it's not longer a reference
        and we'll have an assertion failure */

  start_address_reading(tsk,from,msg->tag);
  rd = read_start(tsk,msg->data,msg->size);
  while (rd.pos < rd.size) {
    /* Read the reference address and target address */
    read_gaddr(&rd,&refaddr);
    read_gaddr(&rd,&remoteaddr);
    assert(refaddr.tid == tsk->tid);
    assert(refaddr.lid >= 0);
    assert(remoteaddr.tid == from);
    assert(remoteaddr.tid != tsk->tid);
    assert(remoteaddr.lid >= 0);

    /* Obtain the reference object to be upated */
    global *reference = addrhash_lookup(tsk,refaddr);
    assert(reference);
    assert(reference->addr.tid == refaddr.tid);
    assert(reference->addr.lid == refaddr.lid);
    assert(!is_nullpntr(reference->p));
    assert(CELL_REMOTEREF == pntrtype(reference->p));
    assert(physhash_lookup(tsk,reference->p) == reference);

    /* Get the target of this reference, which should currently have its lid set to -1 */
    global *target = pglobal(reference->p);
    assert(target != reference);
    assert(target->addr.tid == -1);
    assert(target->addr.lid == -1);
    assert(NULL == addrhash_lookup(tsk,target->addr)); /* since lid == -1 */

    /* Now the have the correct address of the target, assign it */
    target->addr = remoteaddr;
    addrhash_add(tsk,target);

    resume_waiters(tsk,&target->wq,reference->p);
  }
  read_end(&rd);
  finish_address_reading(tsk,from,msg->tag);
}

static void interpreter_ack(task *tsk, message *msg)
{
  int count;
  int naddrs;
  int remove;
  int fromtype;
  int i;

  reader rd;
  int from = get_idmap_index(tsk,msg->source);
  assert(0 <= from);

  rd = read_start(tsk,msg->data,msg->size);



  read_int(&rd,&count);
  read_int(&rd,&naddrs);
  read_int(&rd,&fromtype);
  assert(0 < naddrs);

  if (count > array_count(tsk->unack_msg_acount[from]))
    fatal("ACK: count = %d, array count = %d",count,array_count(tsk->unack_msg_acount[from]));

  assert(count <= array_count(tsk->unack_msg_acount[from]));

  remove = 0;
  for (i = 0; i < count; i++)
    remove += array_item(tsk->unack_msg_acount[from],i,int);


  /* temp */
  assert(1 == count);
  assert(naddrs == array_item(tsk->unack_msg_acount[from],0,int));


  assert(remove <= array_count(tsk->inflight_addrs[from]));

  array_remove_items(tsk->unack_msg_acount[from],count);
  array_remove_items(tsk->inflight_addrs[from],remove);
}

static void interpreter_startdistgc(task *tsk, startdistgc_msg *m)
{
/*   printf("tsk->indistgc = %d\n",tsk->indistgc); */
  assert(!tsk->indistgc);
  tsk->indistgc = 1;
  tsk->newcellflags |= FLAG_NEW;
  assert(endpointid_isnull(&tsk->gc) || endpointid_equals(&tsk->gc,&m->gc));
  if (endpointid_isnull(&tsk->gc)) {
    tsk->gc = m->gc;
    endpoint_link(tsk->endpt,tsk->gc);
  }
  tsk->gciter = m->gciter;
  clear_marks(tsk,FLAG_DMB);

  endpoint_send(tsk->endpt,tsk->gc,MSG_STARTDISTGCACK,NULL,0);
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

  assert(tsk->indistgc);

  /* sanity check: shouldn't have any pending mark messages at this point */
  for (pid = 0; pid < tsk->groupsize; pid++)
    assert(0 == array_count(tsk->distmarks[pid]));
  tsk->inmark = 1;

  mark_roots(tsk,FLAG_DMB);

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
  for (h = 0; h < GLOBAL_HASH_SIZE; h++) {
    for (glo = tsk->targethash[h]; glo; glo = glo->targetnext)
      if (glo->flags & FLAG_DMB)
        mark_global(tsk,glo,FLAG_DMB);
    for (glo = tsk->physhash[h]; glo; glo = glo->physnext)
      if (glo->flags & FLAG_DMB)
        mark_global(tsk,glo,FLAG_DMB);
  }

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

  reader rd;
  int tag = msg->tag;
  char *data = msg->data;
  int size = msg->size;
  int from = get_idmap_index(tsk,msg->source);
  assert(0 <= from);

  assert(tsk->indistgc);
  assert(!endpointid_isnull(&tsk->gc));

  rd = read_start(tsk,data,size);

  assert(tsk->indistgc);

  /* sanity check: shouldn't have any pending mark messages at this point */
  for (pid = 0; pid < tsk->groupsize; pid++)
    assert(0 == array_count(tsk->distmarks[pid]));
  tsk->inmark = 1;

  start_address_reading(tsk,from,tag);
  while (rd.pos < rd.size) {
    read_gaddr(&rd,&addr);
    assert(addr.tid == tsk->tid);
    glo = addrhash_lookup(tsk,addr);
    if (!glo)
      fatal("Marking request for deleted global %d@%d",addr.lid,addr.tid);
    assert(glo);
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

  rd = read_start(tsk,msg->data,msg->size);

  /* do sweep */
  endpoint_send(tsk->endpt,tsk->gc,MSG_SWEEPACK,NULL,0);

  clear_marks(tsk,FLAG_MARKED);
  mark_roots(tsk,FLAG_MARKED);

  sweep(tsk,0);
  clear_marks(tsk,FLAG_NEW);

  tsk->indistgc = 0;
  tsk->newcellflags &= ~FLAG_NEW;
}

static void handle_message(task *tsk, message *msg)
{
  switch (msg->tag) {
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
    assert(sizeof(startdistgc_msg) == msg->size);
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
    assert(sizeof(listen_response_msg) == msg->size);
    interpreter_listen_response(tsk,(listen_response_msg*)msg->data);
    break;
  case MSG_ACCEPT_RESPONSE:
    assert(sizeof(accept_response_msg) == msg->size);
    interpreter_accept_response(tsk,(accept_response_msg*)msg->data);
    break;
  case MSG_CONNECT_RESPONSE:
    assert(sizeof(connect_response_msg) == msg->size);
    interpreter_connect_response(tsk,(connect_response_msg*)msg->data);
    break;
  case MSG_CONNPAIR_RESPONSE:
    assert(sizeof(connpair_response_msg) == msg->size);
    interpreter_connpair_response(tsk,(connpair_response_msg*)msg->data);
    break;
  case MSG_READ_RESPONSE:
    assert(sizeof(read_response_msg) <= msg->size);
    interpreter_read_response(tsk,(read_response_msg*)msg->data);
    break;
  case MSG_WRITE_RESPONSE:
    assert(sizeof(write_response_msg) == msg->size);
    interpreter_write_response(tsk,(write_response_msg*)msg->data);
    break;
  case MSG_CONNECTION_CLOSED:
    assert(sizeof(connection_closed_msg) == msg->size);
    interpreter_connection_closed(tsk,(connection_closed_msg*)msg->data);
    break;
  case MSG_JCMD_RESPONSE:
    assert(sizeof(jcmd_response_msg) <= msg->size);
    interpreter_jcmd_response(tsk,(jcmd_response_msg*)msg->data,msg->source);
    break;
  case MSG_GET_STATS:
    assert(sizeof(get_stats_msg) <= msg->size);
    interpreter_get_stats(tsk,(get_stats_msg*)msg->data);
    break;
  case MSG_KILL:
    node_log(tsk->n,LOG_INFO,"task: received KILL");
    tsk->done = 1;
    break;
  case MSG_ENDPOINT_EXIT: {
    endpoint_exit_msg *m = (endpoint_exit_msg*)msg->data;
    assert(sizeof(endpoint_exit_msg) == msg->size);
    node_log(tsk->n,LOG_INFO,"task: received ENDPOINT_EXIT for "EPID_FORMAT,EPID_ARGS(m->epid));
    tsk->done = 1;
    break;
  }
  default:
    fatal("interpreter: unexpected message %d",msg->tag);
    break;
  }
  message_free(msg);
}

/* Note: handle_interrupt() should never change the frame at the head of the runnable queue
   unless the queue is empty. This is because when handle_trap() returns, it will go back to
   an EIP which is within the code for the current frame. If the current frame changes, EBP
   will be updated to have its new value, and the program will be running the code for one frame
   with the EBP set to another. */
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

  if (NULL != (msg = endpoint_receive(tsk->endpt,0))) {
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

    msg = endpoint_receive(tsk->endpt,FISH_DELAY_MS);

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
    assert(CELL_FRAME != pntrtype(p));

    if (CELL_CAP != pntrtype(p)) {
      frame_return(tsk,f2,p);
      return;
    }
  }

  if (CELL_CAP != pntrtype(p)) {
    set_error(tsk,"%s",CONSTANT_APP_MSG);
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
  assert((STATE_DONE == f2->state) ||
         !builtin_info[bif].reswhnf ||
         (CELL_IND != pntrtype(f2->data[instr->expcount-nargs])) ||
         (0 != f2->resume));
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

void eval_remoteref(task *tsk, frame *f2, pntr ref) /* Can be called from native code */
{
  global *target = (global*)get_pntr(get_pntr(ref)->field1);
  assert(target->addr.tid != tsk->tid);

  if (!target->fetching && (0 <= target->addr.lid)) {
    assert(CELL_REMOTEREF == pntrtype(ref));
    gaddr storeaddr = get_physical_address(tsk,ref);
    assert(storeaddr.tid == tsk->tid);
    msg_fsend(tsk,target->addr.tid,MSG_FETCH,"aa",target->addr,storeaddr);
    target->fetching = 1;
  }
  f2->waitglo = target;
  add_waiter_frame(&target->wq,f2);
  f2->instr--;
  block_frame(tsk,f2);
  check_runnable(tsk);
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
    eval_remoteref(tsk,runnable,p);
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

inline void op_jeq(task *tsk, frame *runnable, const instruction *instr)
{
  pntr p = runnable->data[instr->expcount-1];

  if (CELL_NUMBER != pntrtype(p)) {
    int t = pntrtype(p);
    set_error(tsk,"==: incompatible argument: (%s)",cell_types[t]);
    return;
  }

  if (pntrdouble(p) == (double)instr->arg1)
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
    printf("interrupting\n");
    endpoint_interrupt(tsk->endpt);
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

static void idmap_setup(node *n, endpoint *endpt, task *tsk)
{
  while (1) {
    message *msg = endpoint_receive(tsk->endpt,-1);
    if (MSG_INITTASK == msg->tag) {
      inittask_msg *initmsg = (inittask_msg*)msg->data;
      int i;
      int resp = 0;
      assert(sizeof(inittask_msg) <= msg->size);
      assert(sizeof(initmsg)+initmsg->count*sizeof(endpointid) <= msg->size);

      node_log(n,LOG_INFO,"INITTASK: localid = %d, count = %d",initmsg->localid,initmsg->count);

      assert(!tsk->haveidmap);
      assert(initmsg->count == tsk->groupsize);
      memcpy(tsk->idmap,initmsg->idmap,tsk->groupsize*sizeof(endpointid));
      tsk->haveidmap = 1;

      for (i = 0; i < tsk->groupsize; i++)
        if (!endpointid_equals(&tsk->endpt->epid,&initmsg->idmap[i]))
          endpoint_link(tsk->endpt,initmsg->idmap[i]);

      for (i = 0; i < tsk->groupsize; i++)
        node_log(n,LOG_INFO,"INITTASK: idmap[%d] = "EPID_FORMAT,i,EPID_ARGS(initmsg->idmap[i]));

      endpoint_send(endpt,msg->source,MSG_INITTASKRESP,&resp,sizeof(int));
      message_free(msg);
    }
    else if (MSG_STARTTASK == msg->tag) {
      int resp = 0;
      endpoint_send(endpt,msg->source,MSG_STARTTASKRESP,&resp,sizeof(int));
      message_free(msg);
      break;
    }
    else {
      assert(tsk->haveidmap);
      handle_message(tsk,msg);
    }
  }
}

void interpreter_thread(node *n, endpoint *endpt, void *arg)
{
  task *tsk = (task*)arg;
  frame *runnable;
  const instruction *instr;
  char semdata = 0;
  struct timeval start;
  struct timeval end;
  #ifdef PROFILING
  const instruction *program_ops = bc_instructions(tsk->bcdata);
  #endif

  tsk->n = n;
  tsk->endpt = endpt;
  tsk->endpt->interrupt = 1;
  pthread_setspecific(task_key,tsk);
  endpoint_link(endpt,tsk->out_sockid.coordid);

  write(tsk->threadrunningfds[1],&semdata,1);

  idmap_setup(n,endpt,tsk);

  tsk->newfish = 1;

  gettimeofday(&tsk->nextfish,NULL);
  gettimeofday(&tsk->nextgc,NULL);
  tsk->nextgc = timeval_addms(tsk->nextgc,GC_DELAY);

  runnable = tsk->rtemp;
  tsk->rtemp = NULL;
  tsk->runptr = &runnable;

  gettimeofday(&start,NULL);

  if (0 == tsk->tid) {
    frame *initial = frame_new(tsk,1);
    initial->instr = bc_instructions(tsk->bcdata);
    assert(1 == initial->instr->expcount);
    initial->data[0] = tsk->argsp;
    tsk->argsp = tsk->globnilpntr;
    initial->c = alloc_cell(tsk);
    initial->c->type = CELL_FRAME;
    make_pntr(initial->c->field1,initial);
    run_frame(tsk,initial);
  }

  if (ENGINE_NATIVE == engine_type) {

    native_compile(tsk->bcdata,tsk->bcsize,tsk);

    tsk->endpt->signal = 1;

    if (getenv("INTERRUPTS")) {
      pthread_t sigthread;
      pthread_create(&sigthread,NULL,signal_thread,tsk);
    }

    pthread_kill(pthread_self(),SIGUSR1);
    tsk->usr1setup = 1;
    ((native_fun*)tsk->code)();;
  }
  else if (0 != setjmp(tsk->jbuf)) {
    /* skip any futher interpretation */
    handle_error(tsk);
  }
  else {

    while (1) {

      if (endpt->interrupt) {
        endpt->interrupt = 0;
        if (handle_interrupt(tsk))
          break;
        else
          continue;
      }

      assert(runnable);

      instr = runnable->instr;
      runnable->instr++;

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
      case OP_JEQ:
        op_jeq(tsk,runnable,instr);
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

  gettimeofday(&end,NULL);

  int totalms = timeval_diffms(start,end);
  int gcms = tsk->gcms;
  int runms = totalms-gcms;
  double gcpct = 100.0*((double)gcms)/((double)totalms);

  node_log(tsk->n,LOG_INFO,"Task completed; total %d run %d gc %d gcpct %.3f%",
           totalms,runms,gcms,gcpct);
  #ifdef PROFILING
  if (ENGINE_INTERPRETER == engine_type)
    print_profile(tsk);
  #endif
  tsk->done = 1;
  task_free(tsk);
}
