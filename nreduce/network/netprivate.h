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
 * $Id: node.h 608 2007-08-21 11:59:50Z pmkelly $
 *
 */

#ifndef _NETPRIVATE_H
#define _NETPRIVATE_H

#define WELCOME_MESSAGE "Welcome to the nreduce 0.1 debug console. Enter commands below:\n\n> "
#define MSG_HEADER_SIZE sizeof(msgheader)
#define LISTEN_BACKLOG 10

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

#define NODE_ALREADY_LOCKED(_n) check_mutex_locked(&(_n)->lock)
#define NODE_UNLOCKED(_n) check_mutex_unlocked(&(_n)->lock)

listener *node_listen_locked(node *n, in_addr_t ip, int port, int notify, void *data,
                             int dontaccept, int ismain, endpointid *owner, char *errmsg,
                             int errlen);
void node_remove_listener(node *n, listener *l);
void node_start_iothread(node *n);
void node_close_endpoints(node *n);
void node_close_connections(node *n);
connection *node_connect_locked(node *n, const char *dest, in_addr_t destaddr,
                                int port, int othernode, char *errmsg, int errlen);
void node_handle_endpoint_exit(node *n, endpointid epid);
void node_notify(node *n);
void done_writing(node *n, connection *conn);
void done_reading(node *n, connection *conn);
void remove_connection(node *n, connection *conn);
void start_console(node *n, connection *conn);
void node_send_locked(node *n, unsigned int sourcelocalid, endpointid destendpointid,
                             int tag, const void *data, int size);
void got_message(node *n, const msgheader *hdr, const void *data);
int set_non_blocking(int fd);
connection *add_connection(node *n, const char *hostname, int sock, listener *l);
void endpoint_send_locked(endpoint *endpt, endpointid dest, int tag, const void *data, int size);

void console_thread(node *n, endpoint *endpt, void *arg);

/* iothread.c */

void handle_disconnection(node *n, connection *conn);

/* notify.c */

void notify_accept(node *n, connection *conn);
void notify_connect(node *n, connection *conn, int error);
void notify_read(node *n, connection *conn);
void notify_closed(node *n, connection *conn, int error);
void notify_write(node *n, connection *conn);

/* netutil.c */

int set_non_blocking(int fd);
char *lookup_hostname(node *n, in_addr_t addr);
int lookup_address(node *n, const char *host, in_addr_t *out, int *h_errout);
void determine_ip(node *n);

#ifdef DEBUG_SHORT_KEEPALIVE
int set_keepalive(node *n, int sock, int s)
#endif

#endif
