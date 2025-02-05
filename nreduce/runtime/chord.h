/*
 * This file is part of the NReduce project
 * Copyright (C) 2006-2010 Peter Kelly <kellypmk@gmail.com>
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

#include "network/node.h"

#define MBITS 8
#define KEYSPACE (1 << MBITS)
#define NSUCCESSORS MBITS

//#define DEBUG_CHORD
//#define CHORDTEST_TIMING

typedef unsigned int chordid;

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
  endpointid sender;
} get_table_msg;

typedef struct {
  chordnode cn;
  chordnode fingers[MBITS+1];
  chordnode successors[NSUCCESSORS+1];
  int linksok;
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
  endpointid predecessor;
  chordnode successor;
} insert_msg;

typedef struct {
  chordnode new_successor;
  chordnode old_successor;
} set_next_msg;

typedef struct {
  endpointid initial;
  endpointid caller;
  int stabilize_delay;
} start_chord_msg;

typedef struct {
  endpointid sender;
} get_succlist_msg;

typedef struct {
  endpointid sender;
  chordnode successors[NSUCCESSORS+1];
} reply_succlist_msg;

int chordnode_isnull(chordnode n);
void start_chord(node *n, int localid, endpointid initial, endpointid caller, int stabilize_delay);
void run_chordtest(int argc, char **argv);
