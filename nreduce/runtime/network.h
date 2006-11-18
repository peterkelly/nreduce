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
#define MANAGER_ID 9999

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
  endpointlist endpoints;
  connectionlist connections;
  int listenfd;
  int listenport;
  struct in_addr listenip;
  int havelistenip;
  int nextlocalid;
  pthread_t iothread;
  pthread_t managerthread;
  int ioready_writefd;
  int ioready_readfd;
  pthread_mutex_t lock;
  pthread_cond_t cond;
} socketcomm;

/* network */

int connect_host(const char *hostname, int port);
int start_listening(struct in_addr ip, int port);
int start_listening_host(const char *host, int port);
int accept_connection(int sockfd);
int parse_address(const char *address, char **host, int *port);
array *read_hostnames(const char *hostsfile);
int fdsetflag(int fd, int flag, int on);
int fdsetblocking(int fd, int blocking);
void print_ip(FILE *f, struct in_addr ip);
void print_endpointid(FILE *f, endpointid id);

/* worker */

connection *initiate_connection(socketcomm *sc, const char *hostname, int port);
void start_task_using_manager(socketcomm *sc, const char *bcdata, int bcsize,
                              endpointid *managerids, int count);
endpoint *find_endpoint(socketcomm *sc, int localid);
task *find_task(socketcomm *sc, int localid);
void socket_send_raw(socketcomm *sc, endpoint *endpt,
                     endpointid destendpointid, int tag, const void *data, int size);
void socket_send(task *tsk, int destid, int tag, char *data, int size);
int socket_recv(task *tsk, int *tag, char **data, int *size, int delayms);

task *add_task(socketcomm *sc, int pid, int groupsize, const char *bcdata, int bcsize, int localid);
void add_endpoint(socketcomm *sc, endpoint *endpt);
void remove_endpoint(socketcomm *sc, endpoint *endpt);

/* console2 */

void console_process_received(socketcomm *sc, connection *conn);

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

void start_manager(socketcomm *sc);

#endif
