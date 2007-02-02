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

/* network */

int connect_host(const char *hostname, int port);
int start_listening(struct in_addr ip, int port, int *outport);
int accept_connection(int sockfd);
int parse_address(const char *address, char **host, int *port);
array *read_hostnames(const char *hostsfile);
int fdsetflag(int fd, int flag, int on);
int fdsetblocking(int fd, int blocking);
void print_ip(FILE *f, struct in_addr ip);
void print_endpointid(FILE *f, endpointid id);

#endif
