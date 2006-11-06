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

#ifndef _NETWORK_H
#define _NETWORK_H

#include "src/nreduce.h"
#include "compiler/util.h"
#include "runtime.h"
#include <netdb.h>
#include <netinet/tcp.h>
#include <sys/socket.h>

#define LISTEN_BACKLOG 10
#define WORKER_PORT 2000

typedef struct connection {
  char *hostname;
  struct in_addr ip;
  int port;
  int sock;

  int connected;
  int readfd;
  array *recvbuf;
  array *sendbuf;

  int donewelcome;
  int isconsole;
  int toclose;

  struct connection *prev;
  struct connection *next;
} connection;

typedef struct connectionlist {
  connection *first;
  connection *last;
} connectionlist;

typedef struct socketcomm {
  tasklist tasks;
  connectionlist wlist;
  int listenfd;
  int listenport;
  int nextlocalid;
} socketcomm;

int connect_host(const char *hostname, int port);
int start_listening(struct in_addr ip, int port);
int start_listening_host(const char *host, int port);
int accept_connection(int sockfd);
int parse_address(const char *address, char **host, int *port);
array *read_hostnames(const char *hostsfile);
int fdsetflag(int fd, int flag, int on);
int fdsetblocking(int fd, int blocking);
int fdsetasync(int fd, int async);

/* console2 */

void console_process_received(socketcomm *sc, connection *conn);

#endif
