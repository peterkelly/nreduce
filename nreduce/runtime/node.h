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

#define DEFAULT_IOSIZE 65536

#define HOSTNAME_MAX 256
#define ERRMSG_MAX 256

//#define DEBUG_SHORT_KEEPALIVE

#define WELCOME_MESSAGE "Welcome to the nreduce 0.1 debug console. Enter commands below:\n\n> "

#define LOG_NONE        0
#define LOG_ERROR       1
#define LOG_WARNING     2
#define LOG_INFO        3
#define LOG_DEBUG1      4
#define LOG_DEBUG2      5
#define LOG_COUNT       6

#define MANAGER_ID      1
#define MAIN_CHORD_ID   2
#define FIRST_ID        3

#define WORKER_PORT 2000

struct node;
struct listener;
struct task;
struct gaddr;

#define EPID_FORMAT "%u.%u.%u.%u:%u/%u"
#define EPID_ARGS(epid) \
   ((unsigned char*)&(epid).ip)[0],                \
   ((unsigned char*)&(epid).ip)[1],                \
   ((unsigned char*)&(epid).ip)[2],                \
   ((unsigned char*)&(epid).ip)[3],                \
    (epid).port,\
    (epid).localid

typedef struct endpointid {
  in_addr_t ip;
  unsigned short port;
  unsigned int localid;
} endpointid;

typedef char endpointid_str[100];

typedef struct msgheader {
  endpointid source;
  unsigned int destlocalid;
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

struct endpoint;
struct node;

typedef void (*endpoint_threadfun)(struct node *n, struct endpoint *endpt, void *arg);

typedef struct endpoint {
  endpointid epid;
  messagelist mailbox;
  int checkmsg;
  int interrupt;
  struct endpoint *prev;
  struct endpoint *next;
  struct node *n;
  char *type;
  void *data;
  int closed;
  list *inlinks;
  list *outlinks;
  endpoint_threadfun fun;
  pthread_t thread;
  int signal;
  int rc;
} endpoint;

typedef struct endpointlist {
  endpoint *first;
  endpoint *last;
} endpointlist;

#define CONNECT_FRAMEADDR        0
#define READ_FRAMEADDR           1
#define WRITE_FRAMEADDR          2
#define FINWRITE_FRAMEADDR       3
#define LISTEN_FRAMEADDR         4
#define ACCEPT_FRAMEADDR         5
#define FRAMEADDR_COUNT          6

typedef struct {
  endpointid managerid;
  unsigned int sid;
} socketid;

typedef struct connection {
  socketid sockid;
  char *hostname;
  in_addr_t ip;
  int port;
  int sock;
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

  int canread;
  int canwrite;

  endpointid owner;

  struct connection *prev;
  struct connection *next;
  char errmsg[ERRMSG_MAX+1];
} connection;

typedef struct connectionlist {
  connection *first;
  connection *last;
} connectionlist;

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

typedef struct listenerlist {
  listener *first;
  listener *last;
} listenerlist;

#define NODE_ALREADY_LOCKED(_n) check_mutex_locked(&(_n)->lock)
#define NODE_UNLOCKED(_n) check_mutex_unlocked(&(_n)->lock)

typedef struct node {
  endpointlist endpoints;
  connectionlist connections;
  listenerlist listeners;
  listener *mainl;
  in_addr_t listenip;
  unsigned short listenport;
  unsigned int nextlocalid;
  unsigned int nextsid;
  pthread_t iothread;
  int ioready_writefd;
  int ioready_readfd;
  int notified;
  pthread_mutex_t lock;
  pthread_cond_t cond;
  int shutdown;
  FILE *logfile;
  int loglevel;
  pthread_cond_t closecond;
  pthread_mutex_t liblock;
  endpointid managerid;
  int iosize;
  int total_sent;
  int total_received;
  list *toclose;
} node;

/* node */

#define lock_node(_n) { lock_mutex(&(_n)->lock);
#define unlock_node(_n) unlock_mutex(&(_n)->lock); }

char *lookup_hostname(node *n, in_addr_t addr);
int lookup_address(node *n, const char *host, in_addr_t *out, int *h_errout);

node *node_start(int loglevel, int port);
void node_run(node *n);
void node_log(node *n, int level, const char *format, ...);
listener *node_listen(node *n, in_addr_t ip, int port, int notify, void *data,
                      int dontaccept, int ismain, endpointid *owner, char *errmsg, int errlen);
endpoint *find_endpoint(node *n, int localid);
void node_remove_listener(node *n, listener *l);
void node_start_iothread(node *n);
void node_close_endpoints(node *n);
void node_close_connections(node *n);
connection *node_connect_locked(node *n, const char *dest, in_addr_t destaddr,
                                int port, int othernode, char *errmsg, int errlen);
void node_shutdown(node *n);
void node_notify(node *n);
void done_writing(node *n, connection *conn);
void done_reading(node *n, connection *conn);

endpointid node_add_thread(node *n, const char *type, endpoint_threadfun fun, void *arg,
                           pthread_t *threadp);
endpointid node_add_thread2(node *n, const char *type, endpoint_threadfun fun, void *arg,
                            pthread_t *threadp, int localid, int stacksize);
void endpoint_link(endpoint *endpt, endpointid to);
void endpoint_unlink(endpoint *endpt, endpointid to);
void endpoint_interrupt(endpoint *endpt);
void endpoint_send(endpoint *endpt, endpointid dest, int tag, const void *data, int size);
message *endpoint_receive(endpoint *endpt, int delayms);
int endpointid_equals(const endpointid *e1, const endpointid *e2);
int endpointid_isnull(const endpointid *epid);
void print_endpointid(endpointid_str str, endpointid epid);

void message_free(message *msg);

int socketid_equals(const socketid *a, const socketid *b);
int socketid_isnull(const socketid *a);

/* console */

void console_thread(node *n, endpoint *endpt, void *arg);

/* manager */

void start_manager(node *n);

#ifndef NODE_C
extern const char *log_levels[LOG_COUNT];
#endif

#endif
