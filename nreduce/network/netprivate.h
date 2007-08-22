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

listener *node_listen(node *n, in_addr_t ip, int port, int notify, void *data,
                      int dontaccept, int ismain, endpointid *owner, char *errmsg, int errlen);
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

void console_thread(node *n, endpoint *endpt, void *arg);

void start_manager(node *n, mgr_extfun ext, void *extarg);

#endif
