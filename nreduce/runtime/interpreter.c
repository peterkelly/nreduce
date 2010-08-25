/*
 * This file is part of the NReduce project
 * Copyright (C) 2006-2010 Peter Kelly <kellypmk@gmail.com>
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
#include "events.h"
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
//#define STANDALONE_NOFRAMES_ERROR

extern int opt_postpone;
extern int opt_fishframes;
extern int opt_fishhalf;

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

void head_error(task *tsk)
{
  set_error(tsk,"Invalid argument passed to head");
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
    event_send_respond(tsk,ft->tid,*ft,pntrtype(obj),FUN_RESUME_FETCHERS);
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
    f->waitlnk = NULL;
    unblock_frame_after_first(tsk,f);
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
    cell_make_ind(tsk,curf->c,val);
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

static pntr ensure_frame_has_cell(task *tsk, frame *f)
{
  pntr p;

  /* If the frame does not have a cell associated with it (e.g. because it was created by a CALL
     instruction), then create a cell, and make the return pointer refer to this cell. We need a
     cell when scheduling a frame so that it can be converted into a remote reference on the
     local side. */
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
  return p;
}

void schedule_frame(task *tsk, frame *f, int desttsk, array *msg)
{
  pntr p = ensure_frame_has_cell(tsk,f);

  /* Remove the frame from the sparked queue */
  if (STATE_SPARKED == f->state)
    unspark_frame(tsk,f);

  /* We can't convert waiting frames to fetchers, since the target address may not be known at
     this point. Wake them up instead, so they'll handle it as a normal remote reference. */
  resume_local_waiters(tsk,&f->wq);
  assert(NULL == f->wq.frames); /* Is this valid? What if it was already running, and migrated? */

  /* There are two cases we need to deal with here:
     1. The frame was originally created by this task. It's "canonical" address is therefore
     the phyiscal address of the cell.
     2. The frame was originally created by another task, and was scheduled or migrated here. In
     this case, it will have an entry in the target hash table, and its canonical address is the
     target address associated with the cell. We refer to the local copy of the frame as the
     "replica" (even though it was moved, not copied). In order for other tasks (including the
     creator) to recognise it as the same frame, we must send this target address. */

  global *targetglo = targethash_lookup(tsk,p);
  if (NULL == targetglo) {
    /* Case 1: Frame was created by this task */
    gaddr refaddr = get_physical_address(tsk,p);
    write_format(msg,tsk,"ap",refaddr,p);

    /* Convert the cell into a remote reference. The global this points to is actually the
       global associated with the physical address of the cell. In this case the reference
       isn't actually remote, since the address is local. However, the fetching flag is set,
       so any attempt to access this object will cause the requesting frame to block. */
    global *glo = physhash_lookup(tsk,p);
    assert(NULL != glo);
    assert(glo->addr.lid == refaddr.lid);
    assert(glo->addr.tid == refaddr.tid);
    assert(glo->addr.tid == tsk->tid);
    assert(pntrequal(glo->p,p));

    f->c->type = CELL_REMOTEREF;
    make_pntr(f->c->field1,glo);
    glo->fetching = 1;

    event_send_schedule(tsk,desttsk,glo->addr,frame_fno(tsk,f),1);
  }
  else {
    /* Case 2: Frame was created by another task */
    write_format(msg,tsk,"ap",targetglo->addr,p);

    /* Convert the cell back to a remote reference, whose destination is the original address
       of the frame (which may be different to the destination where its being sent). */
    f->c->type = CELL_REMOTEREF;
    make_pntr(f->c->field1,targetglo);

    event_send_schedule(tsk,desttsk,targetglo->addr,frame_fno(tsk,f),0);
  }

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
        event_send_markentry(tsk,pid,addr);
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
  unblock_frame_after_first(tsk,f);
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
    so->error = 1;
    so->errn = m->errn;
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
    assert(socketid_equals(&so->sockid,&m->sockid));

    /* We should only receive MSG_CONNECTION_CLOSED if the connection previously
       succeeded. */
    assert(so->done_connect);
    assert(0 == so->frameids[CONNECT_FRAMEADDR]);

    so->error = m->error;
    memcpy(so->errmsg,m->errmsg,sizeof(so->errmsg));

    if (0 != so->frameids[READ_FRAMEADDR]) {
      retrieve_blocked_frame(tsk,so->frameids[READ_FRAMEADDR]);
      so->frameids[READ_FRAMEADDR] = 0;
    }
    if (0 != so->frameids[WRITE_FRAMEADDR]) {
      retrieve_blocked_frame(tsk,so->frameids[WRITE_FRAMEADDR]);
      so->frameids[WRITE_FRAMEADDR] = 0;
    }

    so->closed = 1;
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
/*   int connections; */
/*   int listeners; */
  memset(&gsrm,0,sizeof(gsrm));
  gsrm.epid = tsk->endpt->epid;

  /* frame statistics */
  for (fb = tsk->frameblocks; fb; fb = fb->next) {
    for (i = 0; i < tsk->framesperblock; i++) {
      frame *f = ((frame*)&fb->mem[i*tsk->framesize]);
      switch (f->state) {
      case STATE_SPARKED: gsrm.sparked++; break;
      case STATE_ACTIVE: gsrm.running++; break;
      default: break;
      }
    }
  }

  /* memory statistics */
  /* FIXME */
  assert(0);
/*   memusage(tsk,&gsrm.cells,&gsrm.bytes,&gsrm.alloc,&connections,&listeners); */

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

int count_sparks(task *tsk)
{
  int count = 0;
  frame *spark = tsk->sparklist->sprev;
  while (spark != tsk->sparklist) {
    spark = spark->sprev;
    count++;
  }
  return count;
}

static void interpreter_fish(task *tsk, message *msg)
{
  reader rd;
  int reqtsk;
  int age;
  int scheduled = 0;
  array *newmsg;
  int from = get_idmap_index(tsk,msg->source);

  assert(0 <= from);

  rd = read_start(tsk,msg->data,msg->size);

  start_address_reading(tsk,from,msg->tag);
  read_int(&rd,&reqtsk);
  read_int(&rd,&age);
  read_end(&rd);
  event_recv_fish(tsk,from,reqtsk,age);
  finish_address_reading(tsk,from,msg->tag);

  if (opt_fishhalf) {
    /* Schedule half of all sparks to the requestor */

    newmsg = write_start();
    if (reqtsk != tsk->tid) {
      int nsparks = count_sparks(tsk);
      int nframes = nsparks/2;
      while (0 < nframes) {
        frame *spark = tsk->sparklist->sprev;

        if (spark == tsk->sparklist)
          break; /* Spark list is empty */

        pntr p;
        assert(spark->c);
        make_pntr(p,spark->c);

        schedule_frame(tsk,spark,reqtsk,newmsg);
        scheduled++;
        nframes--;
      }
    }
  }
  else {
    /* Schedule a fixed number of sparks to the requestor */

    if (reqtsk == tsk->tid)
      return;

    newmsg = write_start();

    int nframes = opt_fishframes;
    while (0 < nframes) {
      frame *spark = tsk->sparklist->sprev;

      if (spark == tsk->sparklist)
        break; /* Spark list is empty */

      pntr p;
      assert(spark->c);
      make_pntr(p,spark->c);

      schedule_frame(tsk,spark,reqtsk,newmsg);
      scheduled++;
      nframes--;
    }
  }

  if (0 < scheduled) {
    msg_send(tsk,reqtsk,MSG_SCHEDULE,newmsg->data,newmsg->nbytes);
  }
  else if (0 < age--) {
    int dest;
    /*       dest = (tsk->tid+1) % tsk->groupsize; */
    do {
      dest = rand() % tsk->groupsize;
    } while (dest == tsk->tid);

    event_send_fish(tsk,dest,reqtsk,age,FUN_INTERPRETER_FISH);
    msg_fsend(tsk,dest,MSG_FISH,"ii",reqtsk,age);
  }

  write_end(newmsg);
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
  event_recv_fetch(tsk,from,targetaddr,storeaddr);
  finish_address_reading(tsk,from,msg->tag);

  assert(targetaddr.tid == tsk->tid);
  assert(storeaddr.tid == from);
  fetchglo = addrhash_lookup(tsk,targetaddr);

  if (NULL == fetchglo) {
    node_log(tsk->n,LOG_ERROR,"recv(%d->%d) FETCH targetaddr %d@%d storeaddr %d@%d (target deleted)",
             from,tsk->tid,targetaddr.lid,targetaddr.tid,storeaddr.lid,storeaddr.tid);
    abort();
  }

  obj = fetchglo->p;
  assert(!is_nullpntr(obj));
  obj = resolve_pntr(obj);

  if ((CELL_REMOTEREF == pntrtype(obj)) && (0 > pglobal(obj)->addr.lid)) {
    add_gaddr(&pglobal(obj)->wq.fetchers,storeaddr);
  }
  else if ((CELL_FRAME == pntrtype(obj)) &&
           (STATE_ACTIVE == pframe(obj)->state)) {
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
    event_send_respond(tsk,from,storeaddr,pntrtype(obj),FUN_INTERPRETER_FETCH);
    msg_fsend(tsk,from,MSG_RESPOND,"ap",storeaddr,obj);

    if (NULL != (storeglo = addrhash_lookup(tsk,storeaddr))) {
      /* We could already have a target for this storeaddr if a reference has bene
         passed back to us due to another function that is running in this task */
      assert(CELL_REMOTEREF == pntrtype(storeglo->p));
      cell_make_ind(tsk,f->c,storeglo->p);
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
    event_send_respond(tsk,from,storeaddr,pntrtype(obj),FUN_INTERPRETER_FETCH);
    msg_fsend(tsk,from,MSG_RESPOND,"ap",storeaddr,obj);
  }
}

static void transfer_waiters_and_fetchers(waitqueue *from, waitqueue *to)
{
  /* Transfer waiters */
  frame **waitptr = &to->frames;
  while (*waitptr)
    waitptr = &(*waitptr)->waitlnk;
  *waitptr = from->frames;
  from->frames = NULL;

  /* Transfer fetchers */
  list **lptr = &to->fetchers;
  while (*lptr)
    lptr = &(*lptr)->next;
  *lptr = from->fetchers;
  from->fetchers = NULL;
}

static void resolve_local_fetchers(task *tsk, frame *f)
{
  /* This function is called when we have just received a frame from another task.
     If there are any fetch requests pending on that frame which came from this
     task, then translate them into waiting frames. */

  /* First go through the list finding all fetchers from this node, and move them to
     a separate list (to avoid potential problems with concurrent modification) */
  list *to_transfer = NULL;
  list **lptr = &f->wq.fetchers;
  while (*lptr) {
    list *l = *lptr;
    gaddr *current = (gaddr*)l->data;
    if (current->tid == tsk->tid) {
      /* Add to pending list */
      list_push(&to_transfer,current);

      /* Remove fetcher from list */
      *lptr = (*lptr)->next;
      free(l);
    }
    else {
      lptr = &(*lptr)->next;
    }
  }

  /* Now transfer the local fetchers/waiters to the new frame */
  list *l;
  for (l = to_transfer; l; l = l->next) {
    gaddr *current = (gaddr*)l->data;
    global *glo = addrhash_lookup(tsk,*current);
    assert(NULL != glo);
    assert(glo->addr.lid == current->lid);
    assert(glo->addr.tid == current->tid);
    if (glo->fetching) {
      transfer_waiters_and_fetchers(&glo->wq,&f->wq);
      glo->fetching = 0;
    }
  }
  list_free(to_transfer,free);
  
}

void response_for_fetching_ref(task *tsk, global *target, pntr obj)
{
  frame *w;
  for (w = target->wq.frames; w; w = w->waitlnk)
    tsk->nfetching--;
  assert(0 <= tsk->nfetching);

  /* Respond to any FETCH requests for the reference that were made by other tasks */
  resume_waiters(tsk,&target->wq,obj);

  /* If the received object is a frame, start it running */
  if (CELL_FRAME == pntrtype(obj)) {
    resolve_local_fetchers(tsk,pframe(obj));
    run_frame_after_first(tsk,pframe(obj));
  }
}

static void interpreter_respond(task *tsk, message *msg)
{
  pntr obj;
  gaddr storeaddr;

  reader rd;
  int from = get_idmap_index(tsk,msg->source);
  assert(0 <= from);
  assert(from != tsk->tid);

  rd = read_start(tsk,msg->data,msg->size);
  start_address_reading(tsk,from,msg->tag);

  /* Read the store address */
  read_gaddr(&rd,&storeaddr);
  assert(storeaddr.tid == tsk->tid);
  assert(storeaddr.lid >= 0);

  /* Look up the local reference object, whose physical address is storeaddr */
  global *reference = addrhash_lookup(tsk,storeaddr);
  if (NULL == reference) {
    node_log(tsk->n,LOG_ERROR,"recv(%d->%d) RESPOND storeaddr %d@%d (does not exist)",
             from,tsk->tid,storeaddr.lid,storeaddr.tid);
  }
  assert(reference);
  assert(reference->addr.tid == storeaddr.tid);
  assert(reference->addr.lid == storeaddr.lid);
  assert(!is_nullpntr(reference->p));
  if (CELL_REMOTEREF != pntrtype(reference->p)) {

    /* If the object we're being told to store the response in is not a REMOTEREF cell, it means
       that we've already received this object from another node. This can happen in the case
       where an object is sent as part of a message which relates to another object, and this
       one happened to be included in it.

       In this situation I think it should be safe to just ignore the second copy, since we
       already have it. */
    node_log(tsk->n,LOG_WARNING,"recv(%d->%d) RESPOND storeaddr %d@%d - store not remoteref (%s)",
             from,tsk->tid,storeaddr.lid,storeaddr.tid,cell_types[pntrtype(reference->p)]);
    pntr dest = reference->p;
    if (CELL_IND == pntrtype(dest)) {
      dest = resolve_pntr(reference->p);
      node_log(tsk->n,LOG_WARNING,"target of IND is %s",cell_types[pntrtype(dest)]);
    }
    if (CELL_FRAME == pntrtype(dest)) {
      const char *fname = frame_fname(tsk,pframe(dest));
      node_log(tsk->n,LOG_WARNING,"FRAME %s",fname);
    }

    /* Read the object just so we can log the event */
    read_pntr(&rd,&obj);
    read_end(&rd);

    node_log(tsk->n,LOG_WARNING,"Received object is %s",cell_types[pntrtype(obj)]);

    finish_address_reading(tsk,from,msg->tag);

    event_recv_respond(tsk,from,storeaddr,pntrtype(obj));
    return;
  }
  assert(CELL_REMOTEREF == pntrtype(reference->p));
  assert(physhash_lookup(tsk,reference->p) == reference);

  /* Look up the target global that the reference points to */
  global *target = pglobal(reference->p);
  assert(target->fetching);
  target->fetching = 0;

  /* Read the object. read_pntr() will update the reference global with the received object. */
  read_pntr(&rd,&obj);
  read_end(&rd);
  finish_address_reading(tsk,from,msg->tag);

  event_recv_respond(tsk,from,storeaddr,pntrtype(obj));

  global *objglo = targethash_lookup(tsk,obj);
#if 0
  if (objglo && (target != objglo)) {
    assert(NULL == objglo->wq.frames); /* FIXME: assertion failure here */
    assert(NULL == objglo->wq.fetchers);
  }
#endif

  if (target != objglo) {
      /* The object we got back has a different physical address to what we requested. Make the
         original target an indirection cell that points to it. In this case there shouldn't
         be anything waiting on the new object. */
    assert(CELL_REMOTEREF == pntrtype(target->p));
    cell *refcell = get_pntr(target->p);
    cell_make_ind(tsk,refcell,obj);
  }

  response_for_fetching_ref(tsk,target,obj);
}

static void add_fetcher_unique(waitqueue *wq, gaddr srcaddr)
{
  int origfetcher = 0;
  list *l;
  for (l = wq->fetchers; l; l = l->next) {
    gaddr *ft = (gaddr*)l->data;
    if ((ft->tid == srcaddr.tid) && (ft->lid == srcaddr.lid))
      origfetcher = 1;
  }
  if (!origfetcher)
    add_gaddr(&wq->fetchers,srcaddr);
}

static void interpreter_schedule(task *tsk, message *msg)
{
  int from = get_idmap_index(tsk,msg->source);
  assert(0 <= from);

  start_address_reading(tsk,from,msg->tag);

  reader rd = read_start(tsk,msg->data,msg->size);
  while (rd.pos < rd.size) {
    /* Read the frame and originating address */
    gaddr srcaddr;
    pntr framep;
    read_gaddr(&rd,&srcaddr);

    global *existing = addrhash_lookup(tsk,srcaddr);
    int have_existing = 0;
    int existing_type = 0;
    if (NULL != existing) {
      have_existing = 1;
      existing_type = pntrtype(existing->p);
    }

    read_pntr(&rd,&framep);
    assert(CELL_FRAME == pntrtype(framep));
    frame *newf = pframe(framep);

    event_recv_schedule(tsk,from,srcaddr,frame_fno(tsk,newf),have_existing);

    /* Add the old address as a fetcher, so it will be given the result when the frame
       eventually returns. But *only* if it is not already listed as a fetcher. */
    add_fetcher_unique(&newf->wq,srcaddr);
    resolve_local_fetchers(tsk,newf);

    run_frame_after_first(tsk,newf);
  }
  read_end(&rd);
  finish_address_reading(tsk,from,msg->tag);

  tsk->newfish = 1;
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

  for (i = 0; i < remove; i++)
    event_remove_inflight(tsk,array_item(tsk->inflight_addrs[from],i,gaddr));

  array_remove_items(tsk->unack_msg_acount[from],count);
  array_remove_items(tsk->inflight_addrs[from],remove);
}

static void interpreter_startdistgc(task *tsk, startdistgc_msg *m)
{
  event_recv_startdistgc(tsk,m->gciter);
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
/*   clear_marks(tsk,FLAG_DMB); */

  endpoint_send(tsk->endpt,tsk->gc,MSG_STARTDISTGCACK,NULL,0);
}

static void interpreter_markroots(task *tsk)
{
/* An RMT (Root Marking Task) */
  int tid;

  event_recv_markroots(tsk);
  assert(tsk->indistgc);
  assert(!endpointid_isnull(&tsk->gc));

  assert(tsk->indistgc);

  /* sanity check: shouldn't have any pending mark messages at this point */
  for (tid = 0; tid < tsk->groupsize; tid++)
    assert(0 == array_count(tsk->distmarks[tid]));
  tsk->inmark = 1;

  distributed_collect_start(tsk);

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
  mark_start(tsk,FLAG_DMB);
  while (rd.pos < rd.size) {
    read_gaddr(&rd,&addr);
    assert(addr.tid == tsk->tid);
    event_recv_markentry(tsk,from,addr);
    glo = addrhash_lookup(tsk,addr);
    if (!glo) { /* FIXME: crash */
      node_log(tsk->n,LOG_ERROR,"Marking request for deleted global %d@%d",addr.lid,addr.tid);
      abort();
    }
    assert(glo);
    mark_global(tsk,glo,FLAG_DMB,0);
    tsk->gcsent[tsk->tid]--;
  }
  mark_end(tsk,FLAG_DMB);
  finish_address_reading(tsk,from,tag);

  send_mark_messages(tsk);
  send_update(tsk);
  tsk->inmark = 0;
}

static void interpreter_sweep(task *tsk, message *msg)
{
  event_recv_sweep(tsk);

  reader rd;
  assert(tsk->indistgc);
  assert(!endpointid_isnull(&tsk->gc));

  rd = read_start(tsk,msg->data,msg->size);

  /* do sweep */
  distributed_collect_end(tsk);

  tsk->indistgc = 0;
  tsk->newcellflags &= ~FLAG_NEW;

  event_send_sweepack(tsk);
  endpoint_send(tsk->endpt,tsk->gc,MSG_SWEEPACK,NULL,0);
}

static void maybe_pauseack(task *tsk)
{
  if (tsk->paused && (tsk->groupsize-1 == tsk->gotpause)) {
    event_send_pauseack(tsk);
    endpoint_send(tsk->endpt,tsk->gc,MSG_PAUSEACK,NULL,0);
  }
}

static void interpreter_pause(task *tsk, message *msg)
{
  event_recv_pause(tsk);
  assert(!tsk->paused);
  if (endpointid_isnull(&tsk->gc)) {
    tsk->gc = msg->source;
    endpoint_link(tsk->endpt,tsk->gc);
  }
  tsk->paused = 1;
  int i;
  for (i = 0; i < tsk->groupsize; i++) {
    if (i != tsk->tid) {
      event_send_gotpause(tsk,i);
      endpoint_send(tsk->endpt,tsk->idmap[i],MSG_GOTPAUSE,NULL,0);
    }
  }
  maybe_pauseack(tsk);
}

static void interpreter_gotpause(task *tsk, message *msg)
{
  int from = get_idmap_index(tsk,msg->source);
  event_recv_gotpause(tsk,from);
  tsk->gotpause++;
  assert(tsk->groupsize-1 >= tsk->gotpause);
  maybe_pauseack(tsk);
}

static void interpreter_resume(task *tsk)
{
  event_recv_resume(tsk);
  tsk->paused = 0;
  tsk->gotpause = 0;
}

static void interpreter_checkallrefs(task *tsk)
{
  send_checkrefs(tsk);
}

static void interpreter_checkrefs(task *tsk, message *msg)
{
  int from = get_idmap_index(tsk,msg->source);
  uint32_t pos = 0;
  int count = 0;
  int bad = 0;
  while (pos < msg->size) {
    assert(pos+sizeof(gaddr) <= msg->size);
    gaddr addr;
    memcpy(&addr,&msg->data[pos],sizeof(gaddr));
    pos += sizeof(gaddr);
    if (NULL == addrhash_lookup(tsk,addr)) {
      node_log(tsk->n,LOG_ERROR,"Task %d: Reference %d@%d from task %d is invalid",
               tsk->tid,addr.lid,addr.tid,from);
      bad++;
    }
    count++;
  }
  if (0 == bad) {
    node_log(tsk->n,LOG_DEBUG1,"Task %d: All %d refs from task %d are ok",
             tsk->tid,count,from);
  }
  else {
    node_log(tsk->n,LOG_ERROR,"Task %d: Out of %d refs from task %d, %d are bad",
             tsk->tid,count,from,bad);
  }
}

void add_known_replica(global *glo, int peer)
{
  list *l;

  /* Check that it doesn't already exist */
  for (l = glo->known_replicas; l; l = l->next) {
    int *other = (int*)l->data;
    assert(*other != peer);
  }

  /* Add it to the list */
  int *peercopy = (int*)malloc(sizeof(int));
  *peercopy = peer;
  list_push(&glo->known_replicas,peercopy);
}

static void interpreter_have_replicas(task *tsk, message *msg)
{
  int from = get_idmap_index(tsk,msg->source);

  array *deleted = array_new(1,0);

  assert(0 == (msg->size % sizeof(gaddr)));
  int count = msg->size/sizeof(gaddr);
  int i;
  for (i = 0; i < count; i++) {
    gaddr addr = ((gaddr*)msg->data)[i];;
    assert(addr.tid == tsk->tid);
    global *glo = addrhash_lookup(tsk,addr);
    if (NULL == glo) {
      /* We have already deleted this replica... send back a DELETE_REPLICA message immediately */
      array_append(deleted,&addr,sizeof(gaddr));
    }
    else {
      /* Master object still exists - add source to list of known replicas */
      add_known_replica(glo,from);
    }
  }

  if (0 < deleted->nbytes)
    msg_send(tsk,from,MSG_DELETE_REPLICAS,deleted->data,deleted->nbytes);
  array_free(deleted);
}

static void interpreter_delete_replicas(task *tsk, message *msg)
{
  int from = get_idmap_index(tsk,msg->source);

  assert(0 == (msg->size % sizeof(gaddr)));
  int count = msg->size/sizeof(gaddr);
  int i;
  for (i = 0; i < count; i++) {
    gaddr addr = ((gaddr*)msg->data)[i];;
    assert(addr.tid == from);
    global *glo = addrhash_lookup(tsk,addr);
    if (NULL == glo) {
      /* We have already deleted this replica; do nothing */
    }
    else {
      /* Replica still exists */
      glo->deleteme = 1;
    }
  }
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
  case MSG_PAUSE:
    interpreter_pause(tsk,msg);
    break;
  case MSG_GOTPAUSE:
    interpreter_gotpause(tsk,msg);
    break;
  case MSG_RESUME:
    interpreter_resume(tsk);
    break;
  case MSG_CHECKALLREFS:
    interpreter_checkallrefs(tsk);
    break;
  case MSG_CHECKREFS:
    interpreter_checkrefs(tsk,msg);
    break;
  case MSG_HAVE_REPLICAS:
    interpreter_have_replicas(tsk,msg);
    break;
  case MSG_DELETE_REPLICAS:
    interpreter_delete_replicas(tsk,msg);
    break;
  case MSG_COUNT_SPARKS:
    interpreter_count_sparks(tsk,msg);
    break;
  case MSG_DISTRIBUTE:
    interpreter_distribute(tsk,msg);
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

#ifdef STANDALONE_NOFRAMES_ERROR
static int find_frame(task *tsk, stack *s, frame *f, frame *lookingfor)
{
  frame *w;
  int i;
  for (i = 0; i < s->count; i++)
    if (s->data[i] == f)
      return 0;
  stack_push(s,f);
  for (w = f->wq.frames; w; w = w->waitlnk) {
    if ((w == lookingfor) || find_frame(tsk,s,w,lookingfor)) {
      stack_pop(s);
      return 1;
    }
  }
  stack_pop(s);
  return 0;
}

static int graph_trace(task *tsk, stack *s, frame *f, int depth)
{
  frame *w;
  int i;
  for (i = 0; i < s->count; i++)
    if (s->data[i] == f)
      return 0;

  for (i = 0; i < depth; i++)
    printf("  ");
  printf("%p (%s)\n",f,bc_function_name(tsk->bcdata,frame_fno(tsk,f)));

  stack_push(s,f);
  for (w = f->wq.frames; w; w = w->waitlnk)
    graph_trace(tsk,s,w,depth+1);
  stack_pop(s);
  return 0;
}

static void find_recursion(task *tsk)
{
  frameblock *fb;
  int i;
  stack *s = stack_new();
  for (fb = tsk->frameblocks; fb; fb = fb->next) {
    for (i = 0; i < tsk->framesperblock; i++) {
      frame *f = ((frame*)&fb->mem[i*tsk->framesize]);
      if (STATE_ACTIVE == f->state) { /* FIXME: check after running/blocked change */
        assert(0 == s->count);
        if (find_frame(tsk,s,f,f)) {
          printf("%p (%s): blocked on self\n",
                 f,bc_function_name(tsk->bcdata,frame_fno(tsk,f)));
          graph_trace(tsk,s,f,0);
          printf("\n");
        }
      }
    }
  }
  stack_free(s);
}
#endif

int find_spark(task *tsk)
{
  if (NULL != tsk->frameblocks) {
    /* Some frames in the spark list may have the postponed field set, indicating that they
       correspond to service requests to the local machine which should not be sparked locally
       if there are already the maximum number of service requests in the progress. The spark
       list is organised such that these are always placed *after* normal sparks that can
       be run locally. So we only need to check the head of the spark list. If it is marked
       postponed, this means that all frames in the spark list are marked nonlocal, and so we
       should only run the spark if the maximum no. of local requests are not already in
       progress.

       Normally, sparks are added to the front of the list. b_connect is the only place
       where a nonlocal spark can be added to the list, in which case it is placed at the end */

    frame *spark = tsk->sparklist->snext;
    if ((spark != tsk->sparklist) &&
        (!spark->postponed || ((MAX_LOCAL_CONNECTIONS > tsk->local_conns) &&
                               (MAX_TOTAL_CONNECTIONS > tsk->total_conns)))) {
      run_frame(tsk,spark);
      return 1;
    }
  }
  return 0;
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

  if (tsk->need_minor) {
    local_collect(tsk);

    list *l;
    for (l = tsk->wakeup_after_collect; l; l = l->next) {
      fprintf(stderr,"waking up blocked frame with ioid %d\n",*(int*)l->data);
      retrieve_blocked_frame(tsk,*(int*)l->data);
    }
    list_free(tsk->wakeup_after_collect,free);
    tsk->wakeup_after_collect = NULL;

    /* FIXME */
/*     if (getenv("GC_STATS")) { */
/*       int cells; */
/*       int bytes; */
/*       int alloc; */
/*       int connections; */
/*       int listeners; */
/*       memusage(tsk,&cells,&bytes,&alloc,&connections,&listeners); */
/*       printf("Collect: %d cells, %dk memory, %dk allocated, %d connections, %d listeners\n", */
/*              cells,bytes/1024,alloc/1024,connections,listeners); */
/*     } */

    gettimeofday(&now,NULL);
    tsk->nextgc = timeval_addms(now,GC_DELAY);
  }

  if (NULL != (msg = endpoint_receive(tsk->endpt,0))) {
    handle_message(tsk,msg);
    if (tsk->done)
      return 1;
    endpoint_interrupt(tsk->endpt); /* may be another one */
  }

  while (tsk->paused) {
    msg = endpoint_receive(tsk->endpt,-1);
    handle_message(tsk,msg);
    if (tsk->done)
      return 1;
  }

  if (NULL == *tsk->runptr) {
    int diffms;

#ifdef STANDALONE_NOFRAMES_ERROR
    /* Standalone: no runnable or blocked frames means we can exit. This situation should
       only ever arise in programs that are incorrect, due to a cell pointing to itself. */
    if ((1 == tsk->groupsize) && (0 == tsk->netpending)) {
      printf("Out of runnable frames!\n");
      find_recursion(tsk);
      return 1;
    }
#endif

/*     node_log(tsk->n,LOG_DEBUG1,"%d: no runnable frames; local_conns = %d, nfetching = %d", */
/*              tsk->tid,tsk->local_conns,tsk->nfetching); */

    assert(0 <= tsk->local_conns);

    /* We really have nothing to do; run a sparked frame if we have one, otherwise send
       out a work request */

    if (find_spark(tsk))
      return 0;

    gettimeofday(&now,NULL);
    diffms = timeval_diffms(now,tsk->nextfish);

    /* Only send a FISH request if there's been enough time passed since the last one,
       and if the local machine is not already busy with enough service requests. */
    if ((MAX_LOCAL_CONNECTIONS > tsk->local_conns) &&
        (tsk->newfish || (0 >= diffms)) &&
        (1 < tsk->groupsize)) {
      int dest;

      /* avoid sending another until after the sleep period */

      int delay = FISH_DELAY_MS + ((rand() % 1000) * FISH_DELAY_MS / 1000);
      tsk->nextfish = timeval_addms(tsk->nextfish,delay);

/*       dest = (tsk->tid+1) % tsk->groupsize; */
      do {
        dest = rand() % tsk->groupsize;
      } while (dest == tsk->tid);

      event_send_fish(tsk,dest,tsk->tid,tsk->groupsize,FUN_HANDLE_INTERRUPT);
      msg_fsend(tsk,dest,MSG_FISH,"ii",tsk->tid,tsk->groupsize);
      tsk->newfish = 0;
    }

    /* Check if we've waited for more than GC_DELAY ms since the last garbage collection cycle.
       If so, cause another garbage collection to be done. Even though there may not have been
       much memory allocation done during this time, there may be socket connections lying around
       that are no longer referenced and should therefore be cleaned up. */
    if (0 >= timeval_diffms(now,tsk->nextgc)) {
      tsk->need_minor = 1;
      tsk->need_major = 1;
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
    cap *newcp = cap_alloc(tsk,cp->arity,cp->address,cp->fno,
                           instr->expcount-1+cp->count);
    #ifdef PROFILING
    tsk->stats.caps[cp->fno]++;
    #endif
    newcp->sl = cp->sl;
    newcp->count = 0;
    for (i = 0; i < instr->expcount-1; i++)
      newcp->data[newcp->count++] = f2->data[i];
    for (i = 0; i < cp->count; i++)
      newcp->data[newcp->count++] = cp->data[i];

    /* replace the current FRAME with the new CAP */
    if (f2->c) {
      write_barrier(tsk,f2->c);
      f2->c->type = CELL_CAP;
      make_pntr(f2->c->field1,newcp);
      make_pntr(rep,f2->c);
      newcp->c = f2->c;
      f2->c = NULL;
    }
    else {
      cell *capc = alloc_cell(tsk);
      newcp->c = capc;
      capc->type = CELL_CAP;
      make_pntr(capc->field1,newcp);
      make_pntr(*f2->retp,capc);
      f2->retp = NULL;
      make_pntr(rep,capc);
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
    frame *newf = frame_new(tsk);
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
  cell_make_ind(tsk,target,res);
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
  cap *c = cap_alloc(tsk,program_finfo[fno].arity,program_finfo[fno].address,fno,n);
  #ifdef PROFILING
  tsk->stats.caps[fno]++;
  #endif
  c->sl.fileno = instr->fileno;
  c->sl.lineno = instr->lineno;
  c->count = 0;
  for (i = instr->expcount-n; i < instr->expcount; i++)
    c->data[c->count++] = runnable->data[i];

  capv = alloc_cell(tsk);
  c->c = capv;
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
  frame *newf = frame_new(tsk);
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

  if (!target->fetching && (0 <= target->addr.lid)) {
    assert(target->addr.tid != tsk->tid);
    assert(CELL_REMOTEREF == pntrtype(ref));
    gaddr storeaddr = get_physical_address(tsk,ref);
    assert(storeaddr.tid == tsk->tid);
    event_send_fetch(tsk,target->addr.tid,target->addr,storeaddr,FUN_EVAL_REMOTEREF);
    msg_fsend(tsk,target->addr.tid,MSG_FETCH,"aa",target->addr,storeaddr);
    target->fetching = 1;
  }
  assert(0 <= tsk->nfetching);
  tsk->nfetching++;
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

  newf = frame_new(tsk);
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
  pntr aref = create_array(tsk,sizeof(pntr),n-1);
  cell *refcell = get_pntr(aref);
  carray *arr = aref_array(aref);
  int i;

  assert(n-1 == arr->alloc);
  for (i = 0; i < n-1; i++)
    ((pntr*)arr->elements)[i] = runnable->data[instr->expcount-i-1];
  arr->size = n-1;
  refcell->field2 = runnable->data[instr->expcount-n];
  make_aref_pntr(runnable->data[instr->expcount-n],refcell,0);
}

/* Can be called from native code */
void make_item_frame(task *tsk, frame *runnable, int expcount, int pos)
{
  const instruction *program_ops = bc_instructions(tsk->bcdata);
  const funinfo *program_finfo = bc_funinfo(tsk->bcdata);
  const bcheader *bch = (const bcheader*)tsk->bcdata;
  cell *newfholder;
  frame *newf = frame_new(tsk);
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

  node_log(tsk->n,LOG_INFO,"opt_postpone = %d",opt_postpone);
  node_log(tsk->n,LOG_INFO,"opt_fishframes = %d",opt_fishframes);

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

  event_task_start(tsk);

  if (0 == tsk->tid) {
    frame *initial = frame_new(tsk);
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
    /* Clear floating point exceptions - seems to be needed on
       Linux for some reason, otherwise SIGFPEs get re-raised. */
    asm("fnclex");
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
  if (totalms == 0)
    totalms = 1;
  int gcms = tsk->gcms;
  int runms = totalms-gcms;
  double gcpct = 100.0*((double)gcms)/((double)totalms);
  double minorpct = 100.0*((double)tsk->minorms)/((double)totalms);
  double majorpct = 100.0*((double)tsk->majorms)/((double)totalms);

  node_log(tsk->n,LOG_INFO,"Task completed; total %d run %d gc %d gcpct %.2f%%"
           " minor %.2f%% major %.2f%%",
           totalms,runms,gcms,gcpct,minorpct,majorpct);
  #ifdef OBJECT_LIFETIMES
  int bucket;
  node_log(tsk->n,LOG_INFO,"Task ages: (mb/count)");
  for (bucket = 0; bucket < array_count(tsk->lifetimes); bucket++) {
    node_log(tsk->n,LOG_INFO,"%u %u",
             (bucket*LIFETIME_INCR)/1024/1024,((int*)tsk->lifetimes->data)[bucket]);
  }
  #endif
  #ifdef PROFILING
  if (ENGINE_INTERPRETER == engine_type)
    print_profile(tsk);
  #endif
  tsk->done = 1;
  event_task_end(tsk);
  task_free(tsk);
}
