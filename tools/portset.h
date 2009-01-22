/*
 * This file is part of the nreduce project
 * Copyright (C) 2008-2009 Peter Kelly (pmk@cs.adelaide.edu.au)
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
 * $Id: runtime.h 849 2009-01-22 01:01:00Z pmkelly $
 *
 */

#ifndef _PORTSET_H
#define _PORTSET_H

/* Keeps track of ports we have previously used, to aid in selection of client-side ports
   when establishing outgoing connections. */
typedef struct portset {
  int min;
  int max;
  int upto;

  int size;
  int count;
  int *ports;
  int start;
  int end;
} portset;

void portset_init(portset *pb, int min, int max);
void portset_destroy(portset *pb);
int portset_allocport(portset *pb);
void portset_releaseport(portset *pb, int port);
void bind_client_port(int sock, portset *ps, int *clientport);

#endif
