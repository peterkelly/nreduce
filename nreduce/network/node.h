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

#ifndef _NODE_H
#define _NODE_H

#include "src/nreduce.h"
#include <netdb.h>
#include <netinet/tcp.h>
#include <sys/socket.h>

#define DEFAULT_IOSIZE 65536

#define HOSTNAME_MAX 256
#define ERRMSG_MAX 256

//#define DEBUG_SHORT_KEEPALIVE

#define LOG_NONE        0
#define LOG_ERROR       1
#define LOG_WARNING     2
#define LOG_INFO        3
#define LOG_DEBUG1      4
#define LOG_DEBUG2      5
#define LOG_COUNT       6

#define IO_ID           1
#define MANAGER_ID      2
#define MAIN_CHORD_ID   3
#define JAVA_ID         4
#define FIRST_ID        5

#define WORKER_PORT     2000
#define JBRIDGE_PORT    2001 /* FIXME: make this configurable */

struct node;
struct connection;
struct listener;
struct endpoint;

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

typedef void (*endpoint_threadfun)(struct node *n, struct endpoint *endpt, void *arg);
typedef int (*mgr_extfun)(struct node *n, struct endpoint *endpt, message *msg,
                          int final, void *arg);

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
  endpointid coordid;
  unsigned int sid;
} socketid;

typedef struct {
  endpointid managerid;
  unsigned int jid;
} javaid;

typedef struct connectionlist {
  struct connection *first;
  struct connection *last;
} connectionlist;

typedef struct listenerlist {
  struct listener *first;
  struct listener *last;
} listenerlist;

typedef struct node {
  endpointlist endpoints;
  connectionlist connections;
  listenerlist listeners;
  struct listener *mainl;
  in_addr_t listenip;
  unsigned short listenport;
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
  endpointid managerid;
  endpointid iothid;
  int iosize;
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
endpoint *find_endpoint(node *n, int localid);
void node_shutdown(node *n);

endpointid node_add_thread(node *n, const char *type, endpoint_threadfun fun, void *arg,
                           pthread_t *threadp);
endpointid node_add_thread2(node *n, const char *type, endpoint_threadfun fun, void *arg,
                            pthread_t *threadp, int localid, int stacksize);
void node_stats(node *n, int *regconnections, int *listeners);
void endpoint_link_locked(endpoint *endpt, endpointid to);
void endpoint_unlink_locked(endpoint *endpt, endpointid to);
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

#ifndef NODE_C
extern const char *log_levels[LOG_COUNT];
#endif

#endif
