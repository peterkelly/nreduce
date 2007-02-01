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

#define WELCOME_MESSAGE "Welcome to the nreduce 0.1 debug console. Enter commands below:\n\n> "

struct node;
struct listener;

typedef struct endpointid {
  struct in_addr nodeip;
  short nodeport;
  short localid;
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

typedef struct endpoint {
  int localid;
  messagelist mailbox;
  int checkmsg;
  int *interruptptr;
  struct endpoint *prev;
  struct endpoint *next;
  int type;
  void *data;
  int closed;
} endpoint;

typedef struct endpointlist {
  endpoint *first;
  endpoint *last;
} endpointlist;

typedef struct connection {
  char *hostname;
  struct in_addr ip;
  int port;
  int sock;
  struct listener *l;
  struct node *n;

  int connected;
  int readfd;
  array *recvbuf;
  array *sendbuf;

  int donewelcome;
  int isconsole;
  int isreg;
  int toclose;
  void *data;

  struct connection *prev;
  struct connection *next;
} connection;

typedef struct connectionlist {
  connection *first;
  connection *last;
} connectionlist;

typedef void (*node_callbackfun)(struct node *n, void *data, int event, connection *conn);

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
  int port;
  int fd;
  node_callbackfun callback;
  void *data;
  struct listener *prev;
  struct listener *next;
} listener;

typedef struct listenerlist {
  listener *first;
  listener *last;
} listenerlist;

typedef struct node {
  endpointlist endpoints;
  connectionlist connections;
  listenerlist listeners;
  listener *mainl;
  node_callbacklist callbacks;
  int listenport;
  struct in_addr listenip;
  int havelistenip;
  int nextlocalid;
  pthread_t iothread;
  pthread_t managerthread;
  endpoint *managerendpt;
  int ioready_writefd;
  int ioready_readfd;
  pthread_mutex_t lock;
  pthread_cond_t cond;
  int shutdown;
} node;

#define EVENT_CONN_ESTABLISHED      0
#define EVENT_CONN_FAILED           1
#define EVENT_CONN_ACCEPTED         2
#define EVENT_CONN_CLOSED           3
#define EVENT_CONN_IOERROR          4
#define EVENT_DATA                  5
#define EVENT_SHUTDOWN              6

/* node */

node *node_new();
void node_free(node *n);
listener *node_listen(node *n, const char *host, int port, node_callbackfun callback, void *data);
void node_add_callback(node *n, node_callbackfun fun, void *data);
void node_remove_callback(node *n, node_callbackfun fun, void *data);
listener *node_add_listener(node *n, int port, int fd, node_callbackfun callback, void *data);
void node_remove_listener(node *n, listener *l);
void node_start_iothread(node *n);
void node_close_endpoints(node *n);
void node_close_connections(node *n);
connection *node_connect(node *n, const char *hostname, int port);
void node_send(node *n, endpoint *endpt, endpointid destendpointid,
               int tag, const void *data, int size);

endpoint *node_add_endpoint(node *n, int localid, int type, void *data);
void node_remove_endpoint(node *n, endpoint *endpt);
void endpoint_forceclose(endpoint *endpt);
void endpoint_add_message(endpoint *endpt, message *msg);
message *endpoint_next_message(endpoint *endpt, int delayms);

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

#endif
