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

#include <netdb.h>
#include <pthread.h>

#define DEFAULT_IOSIZE 65536

#define HOSTNAME_MAX 256
#define ERRMSG_MAX 256

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

struct node;
struct endpoint;

#define IP_FORMAT "%u.%u.%u.%u"
#define IP_ARGS(ip) ((unsigned char*)&(ip))[0], \
                    ((unsigned char*)&(ip))[1], \
                    ((unsigned char*)&(ip))[2], \
                    ((unsigned char*)&(ip))[3]

#define EPID_FORMAT IP_FORMAT":%u/%u"
#define EPID_ARGS(epid) IP_ARGS((epid).ip),(epid).port,(epid).localid

typedef struct endpointid {
  in_addr_t ip;
  uint16_t port;
  uint32_t localid;
} __attribute__ ((__packed__)) endpointid;

typedef struct msgheader {
  uint32_t sourcelocalid;
  uint32_t destlocalid;
  uint32_t size1;
  uint32_t tag1;
} __attribute__ ((__packed__)) msgheader;

typedef struct message {
  struct message *next;
  struct message *prev;
  endpointid source;
  endpointid dest;
  uint32_t tag;
  uint32_t size;
  char data[0];
} message;

typedef void (*endpoint_threadfun)(struct node *n, struct endpoint *endpt, void *arg);

typedef struct endpoint {
  endpointid epid;
  int interrupt;
  struct node *n;
  int signal;
  int rc;
  struct endpoint *prev;
  struct endpoint *next;
  struct endpoint_private *p;
} endpoint;

#define CONNECT_FRAMEADDR        0
#define READ_FRAMEADDR           1
#define WRITE_FRAMEADDR          2
#define LISTEN_FRAMEADDR         3
#define ACCEPT_FRAMEADDR         4
#define CONNPAIR_FRAMEADDR       5
#define FRAMEADDR_COUNT          6

typedef struct {
  endpointid coordid;
  unsigned int sid;
} socketid;

typedef struct node {
  in_addr_t listenip;
  unsigned short listenport;
  endpointid iothid;
  int iosize;
  struct node_private *p;
} node;

/* node */

node *node_start(int loglevel, int port);
void node_run(node *n);
void node_log(node *n, int level, const char *format, ...);
void node_shutdown(node *n);

endpointid node_add_thread(node *n, const char *type, endpoint_threadfun fun, void *arg,
                           pthread_t *threadp);
endpointid node_add_thread2(node *n, const char *type, endpoint_threadfun fun, void *arg,
                            pthread_t *threadp, int localid, int stacksize);
void node_stats(node *n, int *regconnections, int *listeners);
int node_get_endpoints(node *n, const char *type, endpointid **epids);

int endpoint_check_links(endpoint *endpt, endpointid *epids, int count);
void endpoint_link(endpoint *endpt, endpointid to);
void endpoint_unlink(endpoint *endpt, endpointid to);
void endpoint_interrupt(endpoint *endpt);
void endpoint_send(endpoint *endpt, endpointid dest, uint32_t tag, const void *data, uint32_t size);
message *endpoint_receive(endpoint *endpt, int delayms);
int endpointid_equals(const endpointid *e1, const endpointid *e2);
int endpointid_isnull(const endpointid *epid);

void message_free(message *msg);

int socketid_equals(const socketid *a, const socketid *b);
int socketid_isnull(const socketid *a);

#ifndef NODE_C
extern const char *log_levels[LOG_COUNT];
#endif

/* Node */
#define MSG_KILL                2147483647
#define MSG_ENDPOINT_EXIT       2147483646
#define MSG_LINK                2147483645
#define MSG_UNLINK              2147483644

/* Regular connections */
#define MSG_LISTEN              2147483643
#define MSG_ACCEPT              2147483642
#define MSG_CONNECT             2147483641
#define MSG_CONNPAIR            2147483640
#define MSG_READ                2147483639
#define MSG_WRITE               2147483638
#define MSG_LISTEN_RESPONSE     2147483637
#define MSG_ACCEPT_RESPONSE     2147483636
#define MSG_CONNECT_RESPONSE    2147483635
#define MSG_CONNPAIR_RESPONSE   2147483634
#define MSG_READ_RESPONSE       2147483633
#define MSG_WRITE_RESPONSE      2147483632
#define MSG_CONNECTION_CLOSED   2147483631
#define MSG_DELETE_CONNECTION   2147483630
#define MSG_DELETE_LISTENER     2147483629

/* Console */
#define MSG_CONSOLE_DATA        2147483628

/* I/O messages */

typedef struct {
  in_addr_t ip;
  int port;
  endpointid owner;
  int ioid;
} __attribute__ ((__packed__)) listen_msg;

typedef struct {
  socketid sockid;
  int ioid;
} __attribute__ ((__packed__)) accept_msg;

typedef struct {
  char hostname[HOSTNAME_MAX+1];
  int port;
  endpointid owner;
  int ioid;
} __attribute__ ((__packed__)) connect_msg;

typedef struct {
  int ioid;
} __attribute__ ((__packed__)) connpair_msg;

typedef struct {
  socketid sockid;
  int ioid;
} __attribute__ ((__packed__)) read_msg;

typedef struct {
  socketid sockid;
  int ioid;
  int len;
  char data[0];
} __attribute__ ((__packed__)) write_msg;

typedef struct {
  int ioid;
  int error;
  char errmsg[ERRMSG_MAX+1];
  socketid sockid;
} __attribute__ ((__packed__)) listen_response_msg;

typedef struct {
  int ioid;
  socketid sockid;
  char hostname[HOSTNAME_MAX+1];
  int port;
} __attribute__ ((__packed__)) accept_response_msg;

typedef struct {
  int ioid;
  socketid sockid;
  int error;
  char errmsg[ERRMSG_MAX+1];
} __attribute__ ((__packed__)) connect_response_msg;

typedef struct {
  int ioid;
  socketid a;
  socketid b;
  int error;
  char errmsg[ERRMSG_MAX+1];
} __attribute__ ((__packed__)) connpair_response_msg;

typedef struct {
  int ioid;
  socketid sockid;
  int len;
  char data[0];
} __attribute__ ((__packed__)) read_response_msg;

typedef struct {
  int ioid;
} __attribute__ ((__packed__)) write_response_msg;

typedef struct {
  socketid sockid;
  int error;
  char errmsg[ERRMSG_MAX+1];
} __attribute__ ((__packed__)) connection_closed_msg;

typedef struct {
  socketid sockid;
} __attribute__ ((__packed__)) delete_connection_msg;

typedef struct {
  socketid sockid;
} __attribute__ ((__packed__)) delete_listener_msg;

typedef struct {
  endpointid epid;
  int reason;
} __attribute__ ((__packed__)) endpoint_exit_msg;

void send_listen(endpoint *endpt, endpointid epid, in_addr_t ip, int port,
                 endpointid owner, int ioid);
void send_accept(endpoint *endpt, socketid sockid, int ioid);
void send_connect(endpoint *endpt, endpointid epid,
                  const char *hostname, int port, endpointid owner, int ioid);
void send_connpair(endpoint *endpt, endpointid epid, int ioid);
void send_read(endpoint *endpt, socketid sid, int ioid);
void send_write(endpoint *endpt, socketid sockid, int ioid, const char *data, int len);
void send_delete_connection(endpoint *endpt, socketid sockid);
void send_delete_listener(endpoint *endpt, socketid sockid);

#endif
