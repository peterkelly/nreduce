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
 * $Id$
 *
 */

#ifndef _FSM_H
#define _FSM_H

#include "util/list.h"
#include <stdio.h>

typedef struct fsm_transition fsm_transition;
typedef struct fsm_state fsm_state;
typedef struct fsm fsm;

typedef int (*fsm_transition_input_equals)(void *a, void *b);
typedef void (*fsm_transition_input_print)(FILE *f, void *input);

struct fsm_transition {
  /** If true, this transition is an epsilon transition, i.e. the finite state machine can
      non-deterministicly choose to follow this without receiving an input token */
  int epsilon;
  /** Input token which causes this transition */
  void *input;
  /** Destination state */
  fsm_state *to;
  int min;
  int max;
  int toid;
  int toidrel;
};

struct fsm_state {
  /** State number */
  int num;
  /** Transitions originating from this state */
  list *transitions;
  /** If this state is in a deterministic finite state machine generated from a non-deterministic
      machine, this lists the corresponding states in the non-deterministic machine */
  list *ndfsm_states;
  /** Whether or not this state accepts (i.e. constitutes a valid token sequence if it the
      processing ends in this state) */
  int accept;
  int count;
};

struct fsm {
  /** Number to be assigned to the next state that is created */
  int nextnum;
  /** Initial state */
  fsm_state *start;
  /** List of states */
  list *states;
  /** User-supplied function for checking the equality of two input tokens */
  fsm_transition_input_equals input_equals;
  /** User-supplied function for printing the textual representation of an input token to an
      output file */
  fsm_transition_input_print input_print;
};

fsm *fsm_new(fsm_transition_input_equals input_equals, fsm_transition_input_print input_print);
fsm_transition *add_transition(fsm_state *from, fsm_state *to, void *input,
                               int min, int max, int toid, int toidrel);
fsm_state *fsm_add_state(fsm *f);
void fsm_print(FILE *dotfile, fsm *f, int stage);
void fsm_free(fsm *f);
void fsm_expand_duplicates(fsm *f);
void fsm_determinise(fsm *f, fsm *df);
void fsm_expand_and_determinise(fsm *f, fsm *df, FILE *outfile);

#endif /* _FSM_H */
