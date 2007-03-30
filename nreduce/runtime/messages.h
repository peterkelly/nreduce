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

#ifndef _MESSAGES_H
#define _MESSAGES_H

#include "runtime.h"

#define MSG_DONE                0
#define MSG_FISH                1
#define MSG_FETCH               2
#define MSG_TRANSFER            3
#define MSG_ACK                 4
#define MSG_MARKROOTS           5
#define MSG_MARKENTRY           6
#define MSG_SWEEP               7
#define MSG_SWEEPACK            8
#define MSG_UPDATE              9
#define MSG_RESPOND             10
#define MSG_SCHEDULE            11
#define MSG_UPDATEREF           12
#define MSG_STARTDISTGC         13

#define MSG_NEWTASK             14
#define MSG_NEWTASKRESP         15
#define MSG_INITTASK            16
#define MSG_INITTASKRESP        17
#define MSG_STARTTASK           18
#define MSG_STARTTASKRESP       19
#define MSG_KILL                20

#define MSG_IORESPONSE          21

#define MSG_ENDPOINT_EXIT       22
#define MSG_LINK                23
#define MSG_UNLINK              24

#define MSG_CONSOLE_LINE        25

#define MSG_FIND_SUCCESSOR      26
#define MSG_GOT_SUCCESSOR       27
#define MSG_NOTIFY              28
#define MSG_NOTIFY_REPLY        29
#define MSG_STABILIZE           30

#define MSG_GET_TABLE           31
#define MSG_REPLY_TABLE         32
#define MSG_FIND_ALL            33
#define MSG_CHORD_STARTED       34
#define MSG_START_CHORD         35
#define MSG_DEBUG_START         36
#define MSG_DEBUG_DONE          37
#define MSG_ID_CHANGED          38
#define MSG_JOINED              39

#define MSG_COUNT               40

#ifndef WORKER_C
extern const char *msg_names[MSG_COUNT];
#endif

typedef struct {
  endpointid epid;
} endpoint_exit_msg;

#endif
