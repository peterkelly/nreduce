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
 * $Id: gcode.h 339 2006-09-21 01:18:34Z pmkelly $
 *
 */

#ifndef _NETWORK_H
#define _NETWORK_H

#include "src/nreduce.h"
#include <netdb.h>
#include <netinet/tcp.h>
#include <sys/socket.h>

#define LISTEN_BACKLOG 10
#define WORKER_PORT 2000

typedef struct workerinfo {
  char *hostname;
  struct in_addr ip;
  int sock;

  int connected;
  pid_t pid;
  int readfd;
} workerinfo;

typedef struct nodeinfo {
  workerinfo *workers;
  int nworkers;
  int listenfd;
} nodeinfo;

int connect_host(const char *hostname, int port);
int start_listening(struct in_addr ip, int port);
int start_listening_host(const char *host, int port);
int accept_connection(int sockfd);
int parse_address(const char *address, char **host, int *port);
nodeinfo *nodeinfo_init(const char *hostsfile);
void nodeinfo_free(nodeinfo *ni);
int wait_for_connections(nodeinfo *ni);
int fdsetflag(int fd, int flag, int on);

#endif
