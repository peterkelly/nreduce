/*
 * This file is part of the NReduce project
 * Copyright (C) 2009-2010 Peter Kelly <kellypmk@gmail.com>
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

#ifndef _EVENTS_H
#define _EVENTS_H

#include "runtime.h"

#define EVENTS_VERSION              7

#define EV_TASK_START               0  /* done */
#define EV_TASK_END                 1  /* done */
#define EV_SEND_FISH                2  /* done */
#define EV_RECV_FISH                3  /* done */
#define EV_SEND_FETCH               4  /* done */
#define EV_RECV_FETCH               5  /* done */
//#define EV_SEND_ACK                 6
//#define EV_RECV_ACK                 7
//#define EV_SEND_MARKROOTS           8  /* leave - sent by gc */
#define EV_RECV_MARKROOTS           9  /* done */
#define EV_SEND_MARKENTRY           10 /* done */
#define EV_RECV_MARKENTRY           11 /* done */
//#define EV_SEND_SWEEP               12 /* leave - sent by gc */
#define EV_RECV_SWEEP               13 /* done */
#define EV_SEND_SWEEPACK            14 /* done */
//#define EV_RECV_SWEEPACK            15 /* leave - received by gc */
//#define EV_SEND_UPDATE              16
//#define EV_RECV_UPDATE              17
#define EV_SEND_RESPOND             18 /* done */
#define EV_RECV_RESPOND             19 /* done */
#define EV_SEND_SCHEDULE            20 /* done */
#define EV_RECV_SCHEDULE            21 /* done */
//#define EV_SEND_STARTDISTGC         22 /* leave - sent by gc */
#define EV_RECV_STARTDISTGC         23 /* done */
//#define EV_SEND_PAUSE               24 /* leave - sent by gc */
#define EV_RECV_PAUSE               25 /* done */
#define EV_SEND_GOTPAUSE            26 /* done */
#define EV_RECV_GOTPAUSE            27 /* done */
#define EV_SEND_PAUSEACK            28 /* done */
//#define EV_RECV_PAUSEACK            29 /* leave - received by gc */
//#define EV_SEND_RESUME              30 /* leave - sent by gc */
#define EV_RECV_RESUME              31 /* done */

#define EV_ADD_GLOBAL               32
#define EV_REMOVE_GLOBAL            33
#define EV_KEEP_GLOBAL              34
#define EV_PRESERVE_TARGET          35
#define EV_GOT_REMOTEREF            36
#define EV_GOT_OBJECT               37

#define EV_ADD_INFLIGHT             38
#define EV_REMOVE_INFLIGHT          39
#define EV_RESCUE_GLOBAL            40

#define EV_NONE                     41 /* done */
#define EV_COUNT                    42

#define FUN_INTERPRETER_FISH        0
#define FUN_HANDLE_INTERRUPT        1
#define FUN_START_FRAME_RUNNING     2
#define FUN_EVAL_REMOTEREF          3
#define FUN_RESUME_FETCHERS         4
#define FUN_INTERPRETER_FETCH       5
#define FUN_GET_PHYSICAL_ADDRESS    6
#define FUN_ADD_TARGET              7
#define FUN_COUNT                   8

#define TIMESTAMP_MAX 2147483647

typedef unsigned int timestamp_t;

typedef struct {
  int version;
  int key;
  int tid;
  int groupsize;
} __attribute__ ((__packed__)) events_header;

typedef struct {
  timestamp_t timestamp;
  unsigned char type;
  unsigned char tid;
} __attribute__ ((__packed__)) event;

typedef struct {
  event e;
} __attribute__ ((__packed__)) ev_task_start;

typedef struct {
  event e;
} __attribute__ ((__packed__)) ev_task_end;

typedef struct {
  event e;
  unsigned char to;
  unsigned char reqtsk;
  int age;
  unsigned char fun;
} __attribute__ ((__packed__)) ev_send_fish;

typedef struct {
  event e;
  unsigned char from;
  unsigned char reqtsk;
  int age;
} __attribute__ ((__packed__)) ev_recv_fish;

typedef struct {
  event e;
  unsigned char to;
  gaddr target;
  gaddr store;
  unsigned char fun;
} __attribute__ ((__packed__)) ev_send_fetch;

typedef struct {
  event e;
  unsigned char from;
  gaddr target;
  gaddr store;
} __attribute__ ((__packed__)) ev_recv_fetch;

typedef struct {
  event e;
} __attribute__ ((__packed__)) ev_recv_markroots;

typedef struct {
  event e;
  unsigned char to;
  gaddr addr;
} __attribute__ ((__packed__)) ev_send_markentry;

typedef struct {
  event e;
  unsigned char from;
  gaddr addr;
} __attribute__ ((__packed__)) ev_recv_markentry;

typedef struct {
  event e;
} __attribute__ ((__packed__)) ev_recv_sweep;

typedef struct {
  event e;
} __attribute__ ((__packed__)) ev_send_sweepack;

typedef struct {
  event e;
  unsigned char to;
  gaddr store;
  unsigned char objtype;
  unsigned char fun;
} __attribute__ ((__packed__)) ev_send_respond;

typedef struct {
  event e;
  unsigned char from;
  gaddr store;
  unsigned char objtype;
} __attribute__ ((__packed__)) ev_recv_respond;

typedef struct {
  event e;
  unsigned char to;
  gaddr frameaddr;
  unsigned short fno;
  unsigned char local;
} __attribute__ ((__packed__)) ev_send_schedule;

typedef struct {
  event e;
  unsigned char from;
  gaddr frameaddr;
  unsigned short fno;
  unsigned char existing;
} __attribute__ ((__packed__)) ev_recv_schedule;

typedef struct {
  event e;
  int gciter;
} __attribute__ ((__packed__)) ev_recv_startdistgc;

typedef struct {
  event e;
} __attribute__ ((__packed__)) ev_recv_pause;

typedef struct {
  event e;
  unsigned char to;
} __attribute__ ((__packed__)) ev_send_gotpause;

typedef struct {
  event e;
  unsigned char from;
} __attribute__ ((__packed__)) ev_recv_gotpause;

typedef struct {
  event e;
} __attribute__ ((__packed__)) ev_send_pauseack;

typedef struct {
  event e;
} __attribute__ ((__packed__)) ev_recv_resume;

typedef struct {
  event e;
  gaddr addr;
  unsigned char objtype;
  unsigned char fun;
} __attribute__ ((__packed__)) ev_add_global;

typedef struct {
  event e;
  gaddr addr;
} __attribute__ ((__packed__)) ev_remove_global;

typedef struct {
  event e;
  gaddr addr;
} __attribute__ ((__packed__)) ev_keep_global;

typedef struct {
  event e;
  gaddr addr;
  unsigned char objtype;
} __attribute__ ((__packed__)) ev_preserve_target;

typedef struct {
  event e;
  gaddr addr;
  unsigned char objtype;
} __attribute__ ((__packed__)) ev_got_remoteref;

typedef struct {
  event e;
  gaddr addr;
  unsigned char objtype;
  unsigned char extype;
} __attribute__ ((__packed__)) ev_got_object;

typedef struct {
  event e;
  gaddr addr;
} __attribute__ ((__packed__)) ev_add_inflight;

typedef struct {
  event e;
  gaddr addr;
} __attribute__ ((__packed__)) ev_remove_inflight;

typedef struct {
  event e;
  gaddr addr;
} __attribute__ ((__packed__)) ev_rescue_global;

void event_task_start(task *tsk);
void event_task_end(task *tsk);
void event_send_fish(task *tsk, unsigned char to, unsigned char reqtsk, int age, unsigned char fun);
void event_recv_fish(task *tsk, unsigned char from, unsigned char reqtsk, int age);
void event_send_fetch(task *tsk, unsigned char to, gaddr target, gaddr store, unsigned char fun);
void event_recv_fetch(task *tsk, unsigned char from, gaddr target, gaddr store);
void event_recv_markroots(task *tsk);
void event_send_markentry(task *tsk, unsigned char to, gaddr addr);
void event_recv_markentry(task *tsk, unsigned char from, gaddr addr);
void event_recv_sweep(task *tsk);
void event_send_sweepack(task *tsk);
void event_send_respond(task *tsk, unsigned char to, gaddr store, unsigned char objtype,
                        unsigned char fun);
void event_recv_respond(task *tsk, unsigned char from, gaddr store, unsigned char objtype);
void event_send_schedule(task *tsk, unsigned char to, gaddr frameaddr, unsigned short fno,
                         unsigned char local);
void event_recv_schedule(task *tsk, unsigned char from, gaddr frameaddr, unsigned short fno,
                         unsigned char existing);
void event_recv_startdistgc(task *tsk, int gciter);
void event_recv_pause(task *tsk);
void event_send_gotpause(task *tsk, unsigned char to);
void event_recv_gotpause(task *tsk, unsigned char from);
void event_send_pauseack(task *tsk);
void event_recv_resume(task *tsk);

void event_add_global(task *tsk, gaddr addr, unsigned char objtype, unsigned char fun);
void event_remove_global(task *tsk, gaddr addr);
void event_keep_global(task *tsk, gaddr addr);
void event_preserve_target(task *tsk, gaddr addr, unsigned char objtype);
void event_got_remoteref(task *tsk, gaddr addr, unsigned char objtype);
void event_got_object(task *tsk, gaddr addr, unsigned char objtype, unsigned char extype);
void event_add_inflight(task *tsk, gaddr addr);
void event_remove_inflight(task *tsk, gaddr addr);
void event_rescue_global(task *tsk, gaddr addr);

void print_event(FILE *f, event *evt);

#ifndef _EVENTS_C
extern int event_sizes[EV_COUNT];
#endif

#endif
