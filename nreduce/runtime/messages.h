/*
 * This file is part of the nreduce project
 * Copyright (C) 2006-2009 Peter Kelly (pmk@cs.adelaide.edu.au)
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

#ifndef _MESSAGES_H
#define _MESSAGES_H

#include "network/node.h"

/* Distributed execution */
#define MSG_DONE                0
#define MSG_FISH                1
#define MSG_FETCH               2
#define MSG_ACK                 3
#define MSG_MARKROOTS           4
#define MSG_MARKENTRY           5
#define MSG_SWEEP               6
#define MSG_SWEEPACK            7
#define MSG_UPDATE              8
#define MSG_RESPOND             9
#define MSG_SCHEDULE            10
#define MSG_STARTDISTGC         11
#define MSG_PAUSE               12
#define MSG_GOTPAUSE            13
#define MSG_PAUSEACK            14
#define MSG_RESUME              15
#define MSG_CHECKALLREFS        16
#define MSG_CHECKREFS           17
#define MSG_HAVE_REPLICAS       18
#define MSG_DELETE_REPLICAS     19
#define MSG_HISTMAX             20

/* Process startup */
#define MSG_NEWTASK             100
#define MSG_NEWTASKRESP         101
#define MSG_INITTASK            102
#define MSG_INITTASKRESP        103
#define MSG_STARTTASK           104
#define MSG_STARTTASKRESP       105

/* Garbage collector startup */
#define MSG_STARTGC             200
#define MSG_STARTGC_RESPONSE    201
#define MSG_STARTDISTGCACK      202

/* Client */
#define MSG_GET_TASKS           300
#define MSG_GET_TASKS_RESPONSE  301
#define MSG_REPORT_ERROR        302
#define MSG_SHUTDOWN            303

/* Java */
#define MSG_JCMD                400
#define MSG_JCMD_RESPONSE       401

/* Chord */
#define MSG_FIND_SUCCESSOR      600
#define MSG_GOT_SUCCESSOR       601
#define MSG_GET_SUCCLIST        602
#define MSG_REPLY_SUCCLIST      603
#define MSG_STABILIZE           604

/* Chord test */
#define MSG_GET_TABLE           700
#define MSG_REPLY_TABLE         701
#define MSG_FIND_ALL            702
#define MSG_CHORD_STARTED       703
#define MSG_START_CHORD         704
#define MSG_DEBUG_START         705
#define MSG_DEBUG_DONE          706
#define MSG_ID_CHANGED          707
#define MSG_JOINED              708
#define MSG_INSERT              709
#define MSG_SET_NEXT            710

/* Monitoring */
#define MSG_GET_STATS           800
#define MSG_GET_STATS_RESPONSE  801

typedef struct newtask_msg {
  int tid;
  int groupsize;
  int bcsize;
  socketid out_sockid;
  int eventskey;
  int argc;
  char bcdata[0];
} __attribute__ ((__packed__)) newtask_msg;

typedef struct inittask_msg {
  int localid;
  int count;
  endpointid idmap[0];
} __attribute__ ((__packed__)) inittask_msg;

typedef struct {
  socketid sockid;
  int rc;
  int len;
  char data[0];
} __attribute__ ((__packed__)) report_error_msg;

typedef struct {
  int count;
  endpointid idmap[0];
} __attribute__ ((__packed__)) startgc_msg;

typedef struct {
  endpointid gc;
  int gciter;
} __attribute__ ((__packed__)) startdistgc_msg;

typedef struct {
  int gciter;
  int counts[0];
} __attribute__ ((__packed__)) update_msg;

typedef struct {
  endpointid sender;
} __attribute__ ((__packed__)) get_tasks_msg;

typedef struct {
  int count;
  endpointid tasks[0];
} __attribute__ ((__packed__)) get_tasks_response_msg;

typedef struct {
  int ioid;
  int oneway;
  int cmdlen;
  char cmd[0];
} __attribute__ ((__packed__)) jcmd_msg;

typedef struct {
  int ioid;
  int error;
  int len;
  char data[0];
} __attribute__ ((__packed__)) jcmd_response_msg;

typedef struct get_stats_msg {
  endpointid sender;
} __attribute__ ((__packed__)) get_stats_msg;

typedef struct get_stats_response_msg {
  endpointid epid;
  /* frames */
  int sparked;
  int running;
  int blocked;
  /* memory */
  int cells;
  int bytes;
  int alloc;
} __attribute__ ((__packed__)) get_stats_response_msg;

#endif
