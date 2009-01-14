/*
 * This file is part of the nreduce project
 * Copyright (C) 2006-2008 Peter Kelly (pmk@cs.adelaide.edu.au)
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

#ifndef _NETPRIVATE_H
#define _NETPRIVATE_H

#include "node.h"
#include "util.h"
#include <netdb.h>
#include <netinet/tcp.h>
#include <sys/socket.h>

#define CONNECTION_HASH_SIZE 7919

//#define DEBUG_SHORT_KEEPALIVE

#define WELCOME_MESSAGE "Welcome to the nreduce 0.1 debug console. Enter commands below:\n\n> "
#define MSG_HEADER_SIZE sizeof(msgheader)
#define LISTEN_BACKLOG 10

#define MAX_OPENING 3

typedef struct messagelist {
  message *first;
  message *last; 
  pthread_mutex_t lock;
  pthread_cond_t cond;
} messagelist;

typedef struct endpoint_private {
  messagelist mailbox;
  pthread_t thread;
  char *type;
  void *data;
  int closed;
  list *inlinks;
  list *outlinks;
  endpoint_threadfun fun;
} endpoint_private;

typedef struct netstats {
  in_addr_t ip;
  uint16_t port;
  int totalread;
  int totalwritten;
} netstats;

typedef struct connection {
  socketid sockid;
  char *hostname;
  in_addr_t ip;
  int port;
  int sock;
  int outport;
  struct listener *l;
  struct node *n;

  int connected;
  array *recvbuf;
  array *sendbuf;

  int outgoing;
  int donewelcome;
  int donehandshake;
  int isreg;
  int finwrite;
  int collected;
  void *data;
  int frameids[FRAMEADDR_COUNT];
  int dontread;
  int totalread;
  int totalwritten;

  endpointid owner;

  struct connection *prev;
  struct connection *next;
  char errmsg[ERRMSG_MAX+1];
  int errn;
  int iswaiting;
  int isopening;
  int isaccepted;
  int state;
  int haderror;
  struct serverinfo *si;
  struct connection *hashnext;
} connection;

typedef struct listener {
  socketid sockid;
  in_addr_t ip;
  int port;
  int fd;
  void *data;
  int dontaccept;
  int accept_frameid;
  struct listener *prev;
  struct listener *next;
  endpointid owner;
  int notify;
} listener;

typedef struct endpointlist {
  endpoint *first;
  endpoint *last;
} endpointlist;

typedef struct connectionlist {
  struct connection *first;
  struct connection *last;
} connectionlist;

typedef struct listenerlist {
  listener *first;
  listener *last;
} listenerlist;

typedef struct serverinfo {
  int nwaiting;
  int nopening;
  int naccepted;
  in_addr_t ip;
  connectionlist waiting_connections;
} serverinfo;

/* Keeps track of ports we have previously used, to aid in selection of client-side ports
   when establishing outgoing connections. */
typedef struct portset {
  int min;
  int max;
  int upto;

  int size;
  int count;
  int *ports;
  int start;
  int end;
} portset;

typedef struct node_private {
  struct listener *mainl;
  endpointlist endpoints;
  connectionlist connections;
  listenerlist listeners;
  unsigned int nextlocalid;
  unsigned int nextsid;
  pthread_t iothread;
  int ioready_writefd;
  int ioready_readfd;
  int notified;
  pthread_mutex_t lock;
  int shutdown;
  FILE *logfile;
  int loglevel;
  pthread_cond_t closecond;
  list *toclose;
  list *stats;
  list *servers;
  connection *connhash[CONNECTION_HASH_SIZE];
  portset outports;
} node_private;

#define lock_node(_n) { lock_mutex(&(_n)->p->lock);
#define unlock_node(_n) unlock_mutex(&(_n)->p->lock); }

#define NODE_ALREADY_LOCKED(_n) check_mutex_locked(&(_n)->p->lock)
#define NODE_UNLOCKED(_n) check_mutex_unlocked(&(_n)->p->lock)

listener *node_listen_locked(node *n, in_addr_t ip, int port, int notify, void *data,
                             int dontaccept, int ismain, endpointid *owner, char *errmsg,
                             int errlen);
void node_remove_listener(node *n, listener *l);
void node_close_pending(node *n);
void node_start_iothread(node *n);
void node_close_endpoints(node *n);
void node_close_connections(node *n);
void node_handle_endpoint_exit(node *n, endpoint_exit_msg *m);
void node_notify(node *n);
void connect_pending(node *n);
void start_console(node *n, connection *conn);
void node_send_locked(node *n, uint32_t sourcelocalid, endpointid destendpointid,
                      uint32_t tag, const void *data, uint32_t size);
void got_message(node *n, const msgheader *hdr, endpointid source,
                 uint32_t tag, uint32_t size, const void *data);
void create_connection_buffers(connection *conn);
connection *add_connection(node *n, const char *hostname, in_addr_t ip, listener *l);
void endpoint_link_locked(endpoint *endpt, endpointid to);
void endpoint_unlink_locked(endpoint *endpt, endpointid to);
void endpoint_send_locked(endpoint *endpt, endpointid dest, int tag, const void *data, int size);

void console_thread(node *n, endpoint *endpt, void *arg);

serverinfo *get_serverinfo(node *n, in_addr_t ip);
connection *connhash_lookup(node *n, socketid sockid);
void connhash_add(node *n, connection *conn);
void connhash_remove(node *n, connection *conn);

/* iothread.c */

void handle_disconnection(node *n, connection *conn);

/* notify.c */

void notify_accept(node *n, connection *conn);
void notify_connect(node *n, connection *conn, int error);
void notify_read(node *n, connection *conn);
void notify_closed(node *n, connection *conn, int error);
void notify_write(node *n, connection *conn);

/* connection.c */

#define CS_MIN                      100
#define CS_START                    100
#define CS_WAITING                  101
#define CS_READY                    102
#define CS_FAILED                   103
#define CS_CONNECTING               104
#define CS_CONNECTED                105
#define CS_CONNECTED_DONE_WRITING   106
#define CS_ACCEPTED                 107
#define CS_ACCEPTED_DONE_WRITING    108
#define CS_ACCEPTED_DONE_READING    109
#define CS_FINISHED                 110
#define CS_END                      111

#define CE_MIN                      200
#define CE_AUTO                     200
#define CE_REQUESTED                201
#define CE_SLOT_AVAILABLE           202
#define CE_ATTEMPT_FAILED           203
#define CE_ASYNC_STARTED            204
#define CE_ASYNC_FAILED             205
#define CE_ASYNC_OK                 206
#define CE_FINWRITE                 207
#define CE_FINREAD                  208
#define CE_ERROR                    209
#define CE_DELETE                   210
#define CE_READ                     211
#define CE_CLIENT_CONNECTED         212

#define CANREAD(conn) \
  ((CS_CONNECTED == (conn)->state) || \
   (CS_CONNECTED_DONE_WRITING == (conn)->state) || \
   (CS_ACCEPTED == (conn)->state) || \
   (CS_ACCEPTED_DONE_WRITING == (conn)->state))

#define CANWRITE(conn) \
  ((CS_CONNECTED == (conn)->state) || \
   (CS_ACCEPTED == (conn)->state) || \
   (CS_ACCEPTED_DONE_READING == (conn)->state))

void connection_fsm(connection *conn, int event);

void portset_init(portset *pb, int min, int max);
void portset_destroy(portset *pb);
int portset_allocport(portset *pb);
void portset_releaseport(portset *pb, int port);
int bind_client_port(node *n, int sock);

#endif
