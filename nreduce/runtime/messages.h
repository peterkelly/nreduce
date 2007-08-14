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

#define MSG_ENDPOINT_EXIT       21
#define MSG_LINK                22
#define MSG_UNLINK              23

#define MSG_CONSOLE_DATA        24

#define MSG_FIND_SUCCESSOR      25
#define MSG_GOT_SUCCESSOR       26
#define MSG_GET_SUCCLIST        27
#define MSG_REPLY_SUCCLIST      28
#define MSG_STABILIZE           29

#define MSG_GET_TABLE           30
#define MSG_REPLY_TABLE         31
#define MSG_FIND_ALL            32
#define MSG_CHORD_STARTED       33
#define MSG_START_CHORD         34
#define MSG_DEBUG_START         35
#define MSG_DEBUG_DONE          36
#define MSG_ID_CHANGED          37
#define MSG_JOINED              38
#define MSG_INSERT              39
#define MSG_SET_NEXT            40

#define MSG_LISTEN              41
#define MSG_ACCEPT              42
#define MSG_CONNECT             43
#define MSG_READ                44
#define MSG_WRITE               45
#define MSG_FINWRITE            46
#define MSG_LISTEN_RESPONSE     47
#define MSG_ACCEPT_RESPONSE     48
#define MSG_CONNECT_RESPONSE    49
#define MSG_READ_RESPONSE       50
#define MSG_WRITE_RESPONSE      51
#define MSG_CONNECTION_CLOSED   52
#define MSG_FINWRITE_RESPONSE   53
#define MSG_DELETE_CONNECTION   54
#define MSG_DELETE_LISTENER     55

#define MSG_STARTGC             56
#define MSG_STARTGC_RESPONSE    57
#define MSG_STARTDISTGCACK      58

#define MSG_GET_TASKS           59
#define MSG_GET_TASKS_RESPONSE  60

#define MSG_REPORT_ERROR        61
#define MSG_PING                62
#define MSG_PONG                63

#define MSG_COUNT               64

#ifndef WORKER_C
extern const char *msg_names[MSG_COUNT];
#endif

typedef struct newtask_msg {
  int tid;
  int groupsize;
  int bcsize;
  socketid out_sockid;
  int argc;
  char bcdata[0];
} newtask_msg;

typedef struct inittask_msg {
  int localid;
  int count;
  endpointid idmap[0];
} inittask_msg;

typedef struct {
  endpointid epid;
} endpoint_exit_msg;

typedef struct {
  in_addr_t ip;
  int port;
  endpointid owner;
  int ioid;
} listen_msg;

typedef struct {
  socketid sockid;
  int ioid;
} accept_msg;

typedef struct {
  char hostname[HOSTNAME_MAX+1];
  int port;
  endpointid owner;
  int ioid;
} connect_msg;

typedef struct {
  socketid sockid;
  int ioid;
} read_msg;

typedef struct {
  socketid sockid;
  int ioid;
  int len;
  char data[0];
} write_msg;

typedef struct {
  socketid sockid;
  int rc;
  int len;
  char data[0];
} report_error_msg;

typedef struct {
  socketid sockid;
  int ioid;
} finwrite_msg;

typedef struct {
  int ioid;
  int error;
  char errmsg[ERRMSG_MAX+1];
  socketid sockid;
} listen_response_msg;

typedef struct {
  int ioid;
  socketid sockid;
  char hostname[HOSTNAME_MAX+1];
  int port;
} accept_response_msg;

typedef struct {
  int ioid;
  socketid sockid;
  int error;
  char errmsg[ERRMSG_MAX+1];
} connect_response_msg;

typedef struct {
  int ioid;
  socketid sockid;
  int len;
  char data[0];
} read_response_msg;

typedef struct {
  int ioid;
} write_response_msg;

typedef struct {
  int ioid;
} finwrite_response_msg;

typedef struct {
  socketid sockid;
  int error;
  char errmsg[ERRMSG_MAX+1];
} connection_event_msg;

typedef struct {
  socketid sockid;
} delete_connection_msg;

typedef struct {
  socketid sockid;
} delete_listener_msg;

typedef struct {
  int count;
  endpointid idmap[0];
} startgc_msg;

typedef struct {
  endpointid gc;
  int gciter;
} startdistgc_msg;

typedef struct {
  int gciter;
  int counts[0];
} update_msg;

typedef struct {
  endpointid sender;
} get_tasks_msg;

typedef struct {
  int count;
  endpointid tasks[0];
} get_tasks_response_msg;

#endif
