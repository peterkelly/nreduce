/*
 * This file is part of the GridXSLT project
 * Copyright (C) 2005 Peter Kelly (pmk@cs.adelaide.edu.au)
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
 */

#ifndef _UTIL_NETWORK_H
#define _UTIL_NETWORK_H

#define BUFSIZE 65536

#include "list.h"

typedef struct fdhandler fdhandler;
typedef struct eventman eventman;

typedef int (*fdreadhandler)(eventman *em, fdhandler *h);
typedef int (*fdwritehandler)(eventman *em, fdhandler *h);
typedef void (*fdcleanup)(eventman *em, fdhandler *h);

struct fdhandler {
  int fd;
  int reading;
  int writing;
  fdreadhandler readhandler;
  fdwritehandler writehandler;
  fdcleanup cleanup;
  void *data;
};

struct eventman {
  list *handlers;
  int stopped;
  void *data;
};

void eventman_add_handler(eventman *em, fdhandler *h);
void eventman_clear(eventman *em);
void eventman_loop(eventman *em);

int connect_host(const char *hostname, int port);
int start_server(int port);

#endif /* _UTIL_NETWORK_H */
