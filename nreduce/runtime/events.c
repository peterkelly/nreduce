/*
 * This file is part of the nreduce project
 * Copyright (C) 2009 Peter Kelly (pmk@cs.adelaide.edu.au)
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

#define _EVENTS_C

#include "events.h"
#include <unistd.h>

int event_sizes[EV_COUNT] = {
  sizeof(ev_task_start),
  sizeof(ev_task_end),
  sizeof(ev_send_fish),
  sizeof(ev_recv_fish),
  sizeof(ev_send_fetch),
  sizeof(ev_recv_fetch),
  0, /* sizeof(ev_send_ack), */
  0, /* sizeof(ev_recv_ack), */
  0, /* sizeof(ev_send_markroots), */
  sizeof(ev_recv_markroots),
  sizeof(ev_send_markentry),
  sizeof(ev_recv_markentry),
  0, /* sizeof(ev_send_sweep), */
  sizeof(ev_recv_sweep),
  sizeof(ev_send_sweepack),
  0, /* sizeof(ev_recv_sweepack), */
  0, /* sizeof(ev_send_update), */
  0, /* sizeof(ev_recv_update), */
  sizeof(ev_send_respond),
  sizeof(ev_recv_respond),
  sizeof(ev_send_schedule),
  sizeof(ev_recv_schedule),
  0, /* sizeof(ev_send_startdistgc), */
  sizeof(ev_recv_startdistgc),
  0, /* sizeof(ev_send_pause), */
  sizeof(ev_recv_pause),
  sizeof(ev_send_gotpause),
  sizeof(ev_recv_gotpause),
  sizeof(ev_send_pauseack),
  0, /* sizeof(ev_recv_pauseack), */
  0, /* sizeof(ev_send_resume), */
  sizeof(ev_recv_resume),
  sizeof(ev_add_global),
  sizeof(ev_remove_global),
  sizeof(ev_keep_global),
  sizeof(ev_preserve_target),
  sizeof(ev_got_remoteref),
  sizeof(ev_got_object),
  sizeof(ev_add_inflight),
  sizeof(ev_remove_inflight),
  sizeof(ev_rescue_global),
  0, /* sizeof(ev_none) */
};

const char *fun_names[FUN_COUNT] = {
  "interpreter_fish",
  "handle_interrupt",
  "start_frame_running",
  "eval_remoteref",
  "resume_fetchers",
  "interpreter_fetch",
  "get_physical_address",
  "add_target",
};

static void init_event(task *tsk, int type, event *ev)
{
  lock_mutex(&tsk->n->clock_lock);
  ev->timestamp = tsk->n->clock++;
  unlock_mutex(&tsk->n->clock_lock);
  ev->type = type;
  ev->tid = tsk->tid;
}

static void write_event(task *tsk, void *data, int size)
{
  if (0 == tsk->eventsfd)
    return;
  write(tsk->eventsfd,data,size);
}

void event_task_start(task *tsk)
{
  ev_task_start ev;
  init_event(tsk,EV_TASK_START,&ev.e);
  write_event(tsk,&ev,sizeof(ev));
}

void event_task_end(task *tsk)
{
  ev_task_end ev;
  init_event(tsk,EV_TASK_END,&ev.e);
  write_event(tsk,&ev,sizeof(ev));
}

void event_send_fish(task *tsk, unsigned char to, unsigned char reqtsk, int age, unsigned char fun)
{
  ev_send_fish ev;
  init_event(tsk,EV_SEND_FISH,&ev.e);
  ev.to = to;
  ev.reqtsk = reqtsk;
  ev.age = age;
  ev.fun = fun;
  write_event(tsk,&ev,sizeof(ev));
}

void event_recv_fish(task *tsk, unsigned char from, unsigned char reqtsk, int age)
{
  ev_recv_fish ev;
  init_event(tsk,EV_RECV_FISH,&ev.e);
  ev.from = from;
  ev.reqtsk = reqtsk;
  ev.age = age;
  write_event(tsk,&ev,sizeof(ev));
}

void event_send_fetch(task *tsk, unsigned char to, gaddr target, gaddr store, unsigned char fun)
{
  ev_send_fetch ev;
  init_event(tsk,EV_SEND_FETCH,&ev.e);
  ev.to = to;
  ev.target = target;
  ev.store = store;
  ev.fun = fun;
  write_event(tsk,&ev,sizeof(ev));
}

void event_recv_fetch(task *tsk, unsigned char from, gaddr target, gaddr store)
{
  ev_recv_fetch ev;
  init_event(tsk,EV_RECV_FETCH,&ev.e);
  ev.from = from;
  ev.target = target;
  ev.store = store;
  write_event(tsk,&ev,sizeof(ev));
}

void event_recv_markroots(task *tsk)
{
  ev_recv_markroots ev;
  init_event(tsk,EV_RECV_MARKROOTS,&ev.e);
  write_event(tsk,&ev,sizeof(ev));
}

void event_send_markentry(task *tsk, unsigned char to, gaddr addr)
{
  ev_send_markentry ev;
  init_event(tsk,EV_SEND_MARKENTRY,&ev.e);
  ev.to = to;
  ev.addr = addr;
  write_event(tsk,&ev,sizeof(ev));
}

void event_recv_markentry(task *tsk, unsigned char from, gaddr addr)
{
  ev_recv_markentry ev;
  init_event(tsk,EV_RECV_MARKENTRY,&ev.e);
  ev.from = from;
  ev.addr = addr;
  write_event(tsk,&ev,sizeof(ev));
}

void event_recv_sweep(task *tsk)
{
  ev_recv_sweep ev;
  init_event(tsk,EV_RECV_SWEEP,&ev.e);
  write_event(tsk,&ev,sizeof(ev));
}

void event_send_sweepack(task *tsk)
{
  ev_send_sweepack ev;
  init_event(tsk,EV_SEND_SWEEPACK,&ev.e);
  write_event(tsk,&ev,sizeof(ev));
}

void event_send_respond(task *tsk, unsigned char to, gaddr store, unsigned char objtype,
                        unsigned char fun)
{
  ev_send_respond ev;
  init_event(tsk,EV_SEND_RESPOND,&ev.e);
  ev.to = to;
  ev.store = store;
  ev.objtype = objtype;
  ev.fun = fun;
  write_event(tsk,&ev,sizeof(ev));
}

void event_recv_respond(task *tsk, unsigned char from, gaddr store, unsigned char objtype)
{
  ev_recv_respond ev;
  init_event(tsk,EV_RECV_RESPOND,&ev.e);
  ev.from = from;
  ev.store = store;
  ev.objtype = objtype;
  write_event(tsk,&ev,sizeof(ev));
}

void event_send_schedule(task *tsk, unsigned char to, gaddr frameaddr, unsigned short fno,
                         unsigned char local)
{
  ev_send_schedule ev;
  init_event(tsk,EV_SEND_SCHEDULE,&ev.e);
  ev.to = to;
  ev.frameaddr = frameaddr;
  ev.fno = fno;
  ev.local = local;
  write_event(tsk,&ev,sizeof(ev));
}

void event_recv_schedule(task *tsk, unsigned char from, gaddr frameaddr, unsigned short fno,
                         unsigned char existing)
{
  ev_recv_schedule ev;
  init_event(tsk,EV_RECV_SCHEDULE,&ev.e);
  ev.from = from;
  ev.frameaddr = frameaddr;
  ev.fno = fno;
  ev.existing = existing;
  write_event(tsk,&ev,sizeof(ev));
}

void event_recv_startdistgc(task *tsk, int gciter)
{
  ev_recv_startdistgc ev;
  init_event(tsk,EV_RECV_STARTDISTGC,&ev.e);
  ev.gciter = gciter;
  write_event(tsk,&ev,sizeof(ev));
}

void event_recv_pause(task *tsk)
{
  ev_recv_pause ev;
  init_event(tsk,EV_RECV_PAUSE,&ev.e);
  write_event(tsk,&ev,sizeof(ev));
}

void event_send_gotpause(task *tsk, unsigned char to)
{
  ev_send_gotpause ev;
  init_event(tsk,EV_SEND_GOTPAUSE,&ev.e);
  ev.to = to;
  write_event(tsk,&ev,sizeof(ev));
}

void event_recv_gotpause(task *tsk, unsigned char from)
{
  ev_recv_gotpause ev;
  init_event(tsk,EV_RECV_GOTPAUSE,&ev.e);
  ev.from = from;
  write_event(tsk,&ev,sizeof(ev));
}

void event_send_pauseack(task *tsk)
{
  ev_send_pauseack ev;
  init_event(tsk,EV_SEND_PAUSEACK,&ev.e);
  write_event(tsk,&ev,sizeof(ev));
}

void event_recv_resume(task *tsk)
{
  ev_recv_resume ev;
  init_event(tsk,EV_RECV_RESUME,&ev.e);
  write_event(tsk,&ev,sizeof(ev));
}

void event_add_global(task *tsk, gaddr addr, unsigned char objtype, unsigned char fun)
{
  ev_add_global ev;
  init_event(tsk,EV_ADD_GLOBAL,&ev.e);
  ev.addr = addr;
  ev.objtype = objtype;
  ev.fun = fun;
  write_event(tsk,&ev,sizeof(ev));
}

void event_remove_global(task *tsk, gaddr addr)
{
  ev_remove_global ev;
  init_event(tsk,EV_REMOVE_GLOBAL,&ev.e);
  ev.addr = addr;
  write_event(tsk,&ev,sizeof(ev));
}

void event_keep_global(task *tsk, gaddr addr)
{
  ev_keep_global ev;
  init_event(tsk,EV_KEEP_GLOBAL,&ev.e);
  ev.addr = addr;
  write_event(tsk,&ev,sizeof(ev));
}

void event_preserve_target(task *tsk, gaddr addr, unsigned char objtype)
{
  ev_preserve_target ev;
  init_event(tsk,EV_PRESERVE_TARGET,&ev.e);
  ev.addr = addr;
  ev.objtype = objtype;
  write_event(tsk,&ev,sizeof(ev));
}

void event_got_remoteref(task *tsk, gaddr addr, unsigned char objtype)
{
  ev_got_remoteref ev;
  init_event(tsk,EV_GOT_REMOTEREF,&ev.e);
  ev.addr = addr;
  ev.objtype = objtype;
  write_event(tsk,&ev,sizeof(ev));
}

void event_got_object(task *tsk, gaddr addr, unsigned char objtype, unsigned char extype)
{
  ev_got_object ev;
  init_event(tsk,EV_GOT_OBJECT,&ev.e);
  ev.addr = addr;
  ev.objtype = objtype;
  ev.extype = extype;
  write_event(tsk,&ev,sizeof(ev));
}

void event_add_inflight(task *tsk, gaddr addr)
{
  ev_add_inflight ev;
  init_event(tsk,EV_ADD_INFLIGHT,&ev.e);
  ev.addr = addr;
  write_event(tsk,&ev,sizeof(ev));
}

void event_remove_inflight(task *tsk, gaddr addr)
{
  ev_remove_inflight ev;
  init_event(tsk,EV_REMOVE_INFLIGHT,&ev.e);
  ev.addr = addr;
  write_event(tsk,&ev,sizeof(ev));
}

void event_rescue_global(task *tsk, gaddr addr)
{
  ev_rescue_global ev;
  init_event(tsk,EV_RESCUE_GLOBAL,&ev.e);
  ev.addr = addr;
  write_event(tsk,&ev,sizeof(ev));
}

void print_event(FILE *f, event *evt)
{
  fprintf(f,"%08u %2d ",evt->timestamp,evt->tid);
  switch (evt->type) {
  case EV_TASK_START:
    fprintf(f,"Task %d start",evt->tid);
    break;
  case EV_TASK_END:
    fprintf(f,"Task %d end",evt->tid);
    break;
  case EV_SEND_FISH: {
    ev_send_fish *ev = (ev_send_fish*)evt;
    fprintf(f,"send(%d->%d) FISH reqtsk=%d age=%d fun=%s",
            ev->e.tid,ev->to,ev->reqtsk,ev->age,fun_names[ev->fun]);
    break;
  }
  case EV_RECV_FISH: {
    ev_recv_fish *ev = (ev_recv_fish*)evt;
    fprintf(f,"recv(%d->%d) FISH reqtsk=%d age=%d",
            ev->from,ev->e.tid,ev->reqtsk,ev->age);
    break;
  }
  case EV_SEND_FETCH: {
    ev_send_fetch *ev = (ev_send_fetch*)evt;
    fprintf(f,"send(%d->%d) FETCH target=%d@%d store=%d@%d fun=%s",
            ev->e.tid,ev->to,
            ev->target.lid,ev->target.tid,
            ev->store.lid,ev->store.tid,
            fun_names[ev->fun]);
    break;
  }
  case EV_RECV_FETCH: {
    ev_recv_fetch *ev = (ev_recv_fetch*)evt;
    fprintf(f,"recv(%d->%d) FETCH target=%d@%d store=%d@%d",
            ev->from,ev->e.tid,
            ev->target.lid,ev->target.tid,
            ev->store.lid,ev->store.tid);
    break;
  }
  case EV_RECV_MARKROOTS: {
    fprintf(f,"recv(gc->%d) MARKROOTS",evt->tid);
    break;
  }
  case EV_SEND_MARKENTRY: {
    ev_send_markentry *ev = (ev_send_markentry*)evt;
    fprintf(f,"send(%d->%d) MARKENTRY addr=%d@%d",
            ev->e.tid,ev->to,
            ev->addr.lid,ev->addr.tid);
    break;
  }
  case EV_RECV_MARKENTRY: {
    ev_recv_markentry *ev = (ev_recv_markentry*)evt;
    fprintf(f,"recv(%d->%d) MARKENTRY addr=%d@%d",
            ev->from,ev->e.tid,
            ev->addr.lid,ev->addr.tid);
    break;
  }
  case EV_RECV_SWEEP: {
    fprintf(f,"recv(gc->%d) SWEEP",evt->tid);
    break;
  }
  case EV_SEND_SWEEPACK: {
    fprintf(f,"send(%d->gc) SWEEPACK",evt->tid);
    break;
  }
  case EV_SEND_RESPOND: {
    ev_send_respond *ev = (ev_send_respond*)evt;
    fprintf(f,"send(%d->%d) RESPOND store=%d@%d objtype=%s fun=%s",
            ev->e.tid,ev->to,
            ev->store.lid,ev->store.tid,
            cell_types[ev->objtype],
            fun_names[ev->fun]);
    break;
  }
  case EV_RECV_RESPOND: {
    ev_recv_respond *ev = (ev_recv_respond*)evt;
    fprintf(f,"recv(%d->%d) RESPOND store=%d@%d objtype=%s",
            ev->from,ev->e.tid,
            ev->store.lid,ev->store.tid,
            cell_types[ev->objtype]);
    break;
  }
  case EV_SEND_SCHEDULE: {
    ev_send_schedule *ev = (ev_send_schedule*)evt;
    fprintf(f,"send(%d->%d) SCHEDULE frameaddr=%d@%d fno=%d local=%d",
            ev->e.tid,ev->to,
            ev->frameaddr.lid,ev->frameaddr.tid,
            ev->fno,
            ev->local);
    break;
  }
  case EV_RECV_SCHEDULE: {
    ev_recv_schedule *ev = (ev_recv_schedule*)evt;
    fprintf(f,"recv(%d->%d) SCHEDULE frameaddr=%d@%d fno=%d existing=%d",
            ev->from,ev->e.tid,
            ev->frameaddr.lid,ev->frameaddr.tid,
            ev->fno,
            ev->existing);
    break;
  }
  case EV_RECV_STARTDISTGC: {
    ev_recv_startdistgc *ev = (ev_recv_startdistgc*)evt;
    fprintf(f,"recv(gc->%d) STARTDISTGC gciter=%d",
            ev->e.tid,
            ev->gciter);
    break;
  }
  case EV_RECV_PAUSE: {
    fprintf(f,"recv(gc->%d) PAUSE",evt->tid);
    break;
  }
  case EV_SEND_GOTPAUSE: {
    ev_send_gotpause *ev = (ev_send_gotpause*)evt;
    fprintf(f,"send(%d->%d) GOTPAUSE",ev->e.tid,ev->to);
    break;
  }
  case EV_RECV_GOTPAUSE: {
    ev_recv_gotpause *ev = (ev_recv_gotpause*)evt;
    fprintf(f,"recv(%d->%d) GOTPAUSE",ev->from,ev->e.tid);
    break;
  }
  case EV_SEND_PAUSEACK: {
    fprintf(f,"send(%d->gc) PAUSEACK",evt->tid);
    break;
  }
  case EV_RECV_RESUME: {
    fprintf(f,"recv(gc->%d) RESUME",evt->tid);
    break;
  }
  case EV_ADD_GLOBAL: {
    ev_add_global *ev = (ev_add_global*)evt;
    fprintf(f,"ADD_GLOBAL addr=%d@%d objtype=%s fun=%s",
            ev->addr.lid,ev->addr.tid,cell_types[ev->objtype],fun_names[ev->fun]);
    break;
  }
  case EV_REMOVE_GLOBAL: {
    ev_remove_global *ev = (ev_remove_global*)evt;
    fprintf(f,"REMOVE_GLOBAL addr=%d@%d",
            ev->addr.lid,ev->addr.tid);
    break;
  }
  case EV_KEEP_GLOBAL: {
    ev_keep_global *ev = (ev_keep_global*)evt;
    fprintf(f,"KEEP_GLOBAL addr=%d@%d",
            ev->addr.lid,ev->addr.tid);
    break;
  }
  case EV_PRESERVE_TARGET: {
    ev_preserve_target *ev = (ev_preserve_target*)evt;
    fprintf(f,"PRESERVE_TARGET addr=%d@%d objtype=%s",
            ev->addr.lid,ev->addr.tid,cell_types[ev->objtype]);
    break;
  }
  case EV_GOT_REMOTEREF: {
    ev_got_remoteref *ev = (ev_got_remoteref*)evt;
    if (CELL_EMPTY != ev->objtype) {
      fprintf(f,"GOT_REMOTEREF addr=%d@%d (existing objtype=%s)",
              ev->addr.lid,ev->addr.tid,cell_types[ev->objtype]);
    }
    else {
      fprintf(f,"GOT_REMOTEREF addr=%d@%d (no existing object)",
              ev->addr.lid,ev->addr.tid);
    }
    break;
  }
  case EV_GOT_OBJECT: {
    ev_got_object *ev = (ev_got_object*)evt;
    fprintf(f,"GOT_OBJECT addr=%d@%d objtype=%s",
            ev->addr.lid,ev->addr.tid,cell_types[ev->objtype]);
    if (CELL_EMPTY != ev->extype)
      fprintf(f," extype=%s",cell_types[ev->extype]);
    break;
  }
  case EV_ADD_INFLIGHT: {
    ev_add_inflight *ev = (ev_add_inflight*)evt;
    fprintf(f,"ADD_INFLIGHT addr=%d@%d",
            ev->addr.lid,ev->addr.tid);
    break;
  }
  case EV_REMOVE_INFLIGHT: {
    ev_remove_inflight *ev = (ev_remove_inflight*)evt;
    fprintf(f,"REMOVE_INFLIGHT addr=%d@%d",
            ev->addr.lid,ev->addr.tid);
    break;
  }
  case EV_RESCUE_GLOBAL: {
    ev_rescue_global *ev = (ev_rescue_global*)evt;
    fprintf(f,"RESCUE_GLOBAL addr=%d@%d",
            ev->addr.lid,ev->addr.tid);
    break;
  }
  default:
    assert(!"unimplemented event type");
    break;
  }
  fprintf(f,"\n");
}
