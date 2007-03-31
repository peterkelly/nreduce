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

#include "node.h"

#define MBITS 8
#define KEYSPACE (1 << MBITS)
#define NSUCCESSORS MBITS

//#define DEBUG_CHORD

typedef int chordid;

typedef struct {
  chordid id;
  endpointid epid;
} chordnode;

typedef struct {
  chordid id;
  endpointid sender;
  int hops;
  int payload;
} find_successor_msg;

typedef struct {
  chordnode successor;
  int hops;
  int payload;
} got_successor_msg;

typedef struct {
  chordnode ndash;
} notify_msg;

typedef struct {
  endpointid sender;
} stabilize2_msg;

typedef struct {
  chordnode sucprec;
} stabilize3_msg;

typedef struct {
  endpointid sender;
} get_table_msg;

typedef struct {
  chordnode predecessor;
  chordnode fingers[MBITS+1];
  chordnode successors[NSUCCESSORS+1];
} reply_table_msg;

typedef struct {
  chordnode cn;
} chord_started_msg;

typedef struct {
  chordnode cn;
  chordid oldid;
} id_changed_msg;

typedef struct {
  chordnode cn;
} joined_msg;

typedef struct {
  chordnode old_sucprec;
  chordnode sucprec;
  chordnode successors[NSUCCESSORS+1];
} notify_reply_msg;

typedef struct {
  chordnode ndash;
  endpointid caller;
  int stabilize_delay;
} start_chord_msg;

int chordnode_isnull(chordnode n);
void start_chord(node *n, chordnode ndash, endpointid caller, int stabilize_delay);
void run_chordtest(int argc, char **argv);
