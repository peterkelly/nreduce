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

#ifndef _NODE_H
#define _NODE_H

#include "src/nreduce.h"
#include "compiler/util.h"
#include <netdb.h>
#include <netinet/tcp.h>
#include <sys/socket.h>

#define IOSIZE 65536

//#define DEBUG_SHORT_KEEPALIVE

#define WELCOME_MESSAGE "Welcome to the nreduce 0.1 debug console. Enter commands below:\n\n> "

#define LOG_NONE        0
#define LOG_ERROR       1
#define LOG_WARNING     2
#define LOG_INFO        3
#define LOG_DEBUG1      4
#define LOG_DEBUG2      5
#define LOG_COUNT       6

#define MANAGER_ID      9999

struct node;
struct listener;
struct task;
struct gaddr;

typedef struct endpointid {
  struct in_addr nodeip;
  unsigned short nodeport;
  unsigned short localid;
} endpointid;

typedef struct msgheader {
  endpointid source;
  int destlocalid;
  int size;
  int tag;
} msgheader;

typedef struct message {
  msgheader hdr;
  char *data;
  struct message *next;
  struct message *prev;
} message;

typedef struct messagelist {
  message *first;
  message *last; 
  pthread_mutex_t lock;
  pthread_cond_t cond;
} messagelist;

#define TASK_ENDPOINT 1
#define MANAGER_ENDPOINT 2
#define LAUNCHER_ENDPOINT 3

struct endpoint;

typedef void (*endpoint_closefun)(struct endpoint *endpt);

typedef struct endpoint {
  int localid;
  messagelist mailbox;
  int checkmsg;
  int *interruptptr;
  int tempinterrupt;
  struct endpoint *prev;
  struct endpoint *next;
  int type;
  void *data;
  int closed;
  endpoint_closefun closefun;
} endpoint;

typedef struct endpointlist {
  endpoint *first;
  endpoint *last;
} endpointlist;

#define CONNECT_FRAMEADDR 0
#define READ_FRAMEADDR    1
#define WRITE_FRAMEADDR   2
#define LISTEN_FRAMEADDR  3
#define ACCEPT_FRAMEADDR  4
#define FRAMEADDR_COUNT   5

typedef struct connection {
  char *hostname;
  struct in_addr ip;
  int port;
  int sock;
  struct listener *l;
  struct node *n;
  struct in_addr localip;

  int connected;
  int readfd;
  array *recvbuf;
  array *sendbuf;

  int donewelcome;
  int donehandshake;
  int isconsole;
  int isreg;
  int finwrite;
  int collected;
  void *data;
  struct task *tsk;
  int frameids[FRAMEADDR_COUNT];
  int dontread;
  int *status;
  int totalread;

  int canread;
  int canwrite;

  struct connection *prev;
  struct connection *next;
} connection;

typedef struct connectionlist {
  connection *first;
  connection *last;
} connectionlist;

typedef void (*node_callbackfun)(struct node *n, void *data, int event,
                                 connection *conn, endpoint *endpt);

typedef struct node_callback {
  node_callbackfun fun;
  void *data;
  struct node_callback *prev;
  struct node_callback *next;
} node_callback;

typedef struct node_callbacklist {
  node_callback *first;
  node_callback *last;
} node_callbacklist;

typedef struct listener {
  char *hostname;
  int port;
  int fd;
  node_callbackfun callback;
  void *data;
  int dontaccept;
  int accept_frameid;
  struct listener *prev;
  struct listener *next;
} listener;

typedef struct listenerlist {
  listener *first;
  listener *last;
} listenerlist;

#define NODE_ALREADY_LOCKED(_n) is_mutex_locked(&(_n)->lock)
#define NODE_UNLOCKED(_n) is_mutex_unlocked(&(_n)->lock)

typedef struct node {
  endpointlist endpoints;
  connectionlist connections;
  listenerlist listeners;
  listener *mainl;
  node_callbacklist callbacks;
  struct in_addr listenip;
  int havelistenip;
  unsigned short nextlocalid;
  pthread_t iothread;
  int ioready_writefd;
  int ioready_readfd;
  int notified;
  pthread_mutex_t lock;
  pthread_cond_t cond;
  int shutdown;
  int isworker; /* FIXME: shouldn't need this */
  FILE *logfile;
  int loglevel;
  pthread_cond_t closecond;
  pthread_mutex_t liblock;
} node;

#define EVENT_NONE                  0
#define EVENT_CONN_ESTABLISHED      1
#define EVENT_CONN_FAILED           2
#define EVENT_CONN_ACCEPTED         3
#define EVENT_CONN_CLOSED           4
#define EVENT_CONN_IOERROR          5
#define EVENT_HANDSHAKE_DONE        6
#define EVENT_HANDSHAKE_FAILED      7
#define EVENT_DATA_READ             8
#define EVENT_DATA_READFINISHED     9
#define EVENT_DATA_WRITTEN          10
#define EVENT_ENDPOINT_ADDITION     11
#define EVENT_ENDPOINT_REMOVAL      12
#define EVENT_SHUTDOWN              13
#define EVENT_COUNT                 14

/* node */

#define lock_node(_n) { lock_mutex(&(_n)->lock);
#define unlock_node(_n) unlock_mutex(&(_n)->lock); }

node *node_new(int loglevel);
void node_free(node *n);
void node_log(node *n, int level, const char *format, ...);
listener *node_listen(node *n, const char *host, int port, node_callbackfun callback, void *data,
                      int dontaccept);
void node_add_callback(node *n, node_callbackfun fun, void *data);
void node_remove_callback(node *n, node_callbackfun fun, void *data);
void node_remove_listener(node *n, listener *l);
void node_start_iothread(node *n);
void node_close_endpoints(node *n);
void node_close_connections(node *n);
connection *node_connect_locked(node *n, const char *dest, int port, int othernode);
void node_send_locked(node *n, int sourcelocalid, endpointid destendpointid,
                      int tag, const void *data, int size);
void node_send(node *n, int sourcelocalid, endpointid destendpointid,
               int tag, const void *data, int size);
void node_waitclose_locked(node *n, int localid);
void node_shutdown_locked(node *n);
void node_notify(node *n);
void connection_printf(connection *conn, const char *format, ...);
void done_writing(node *n, connection *conn);
void done_reading(node *n, connection *conn);

endpoint *node_add_endpoint(node *n, int localid, int type, void *data,
                            endpoint_closefun closefun);
void node_remove_endpoint(node *n, endpoint *endpt);
void endpoint_forceclose(endpoint *endpt);
void endpoint_add_message(endpoint *endpt, message *msg);
message *endpoint_next_message(endpoint *endpt, int delayms);

void message_free(message *msg);

/* console2 */

void console_process_received(node *n, connection *conn);

/* manager */

typedef struct newtask_msg {
  int pid;
  int groupsize;
  int bcsize;
  char bcdata[0];
} newtask_msg;

typedef struct inittask_msg {
  int localid;
  int count;
  endpointid idmap[0];
} inittask_msg;

void start_manager(node *n);

#ifndef NODE_C
extern const char *log_levels[LOG_COUNT];
extern const char *event_types[EVENT_COUNT];
#endif

#endif
