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

#include "fsm.h"
#include <stdio.h>
#include <assert.h>
#include <stdlib.h>
#include <math.h>
#include <string.h>
#include <signal.h>
#include <unistd.h>
#include <argp.h>
#include <sys/types.h>

typedef struct dfsm_transition dfsm_transition;

/**
 * Represents a transition that will be added to a deterministic finite state machine. The
 * tostates is a list of states in the non-deterministic machine that correspond to the
 * transition. When this is added to the deterministic machine, it will become a transition object
 * with "to" set to a deterministic machine state corresponding to the list in "tostates".
 *
 * This intermediate representation is needed because the deterministic machine state that the
 * transition will point to can only be decided once all of the tostates are known; it is only
 * at this point that we can decide whether we need to create a new deterministic machine state or
 * use an existing one.
 */
struct dfsm_transition {
  void *input;
  list *tostates;
  int accept;
};

fsm *fsm_new(fsm_transition_input_equals input_equals, fsm_transition_input_print input_print)
{
  fsm *f = (fsm*)calloc(1,sizeof(fsm));
  f->input_equals = input_equals;
  f->input_print = input_print;
  return f;
}

fsm_transition *add_transition(fsm_state *from, fsm_state *to, void *input,
                               int min, int max, int toid, int toidrel)
{
  fsm_transition *tr = (fsm_transition*)calloc(1,sizeof(fsm_transition));
  tr->to = to;
  tr->input = input;
  tr->min = min;
  tr->max = max;
  tr->toid = toid;
  tr->toidrel = toidrel;
  if (NULL == input)
    tr->epsilon = 1;
  list_append(&from->transitions,tr);
  return tr;
}

fsm_state *fsm_add_state(fsm *f)
{
  fsm_state *st = (fsm_state*)calloc(1,sizeof(fsm_state));
  st->num = f->nextnum++;
  st->count = 1;
  list_append(&f->states,st);
  return st;
}

void print_statelist(FILE *f, list *l)
{
  list *nl;
  for (nl = l; nl; nl = nl->next) {
    fprintf(f,"%d",((fsm_state*)nl->data)->num);
    if (nl->next)
      fprintf(f,",");
  }
}

void fsm_optimize_transitions(fsm *f)
{
  list *sl;
  for (sl = f->states; sl; sl = sl->next) {
    fsm_state *st = (fsm_state*)sl->data;
    list *tl;
    fsm_transition *tr;
    fsm_transition *trnext;
    int trno = 0;

    tl = st->transitions;
    while (tl) {
      tr = (fsm_transition*)tl->data;
      if (tl->next) {
        trnext = (fsm_transition*)tl->next->data;
        if ((tr->max+1 == trnext->min) && (0 > trnext->toid) &&
            (tr->min == tr->max) && (0 <= tr->toid)) {
          int rel = tr->toid - tr->min;
          if (rel == trnext->toidrel) {
            printf("state %d: merging transitions %d and %d\n",st->num,trno,trno+1);
            tr->max = trnext->max;
            tr->toid = -1;
            tr->toidrel = rel;
            tl->next = tl->next->next;
            /* FIXME: free old transition and list item */
          }
        }
      }
      tl = tl->next;
      trno++;
    }
  }
}

void fsm_dump(fsm *f)
{
  list *sl;
  for (sl = f->states; sl; sl = sl->next) {
    fsm_state *st = (fsm_state*)sl->data;
    list *tl;

    printf("state %d: %d-%d\n",st->num,0,st->count-1);
    for (tl = st->transitions; tl; tl = tl->next) {
      fsm_transition *tr = (fsm_transition*)tl->data;

      printf("  transition ");

      if (tr->input)
        f->input_print(stdout,tr->input);
      else
        fprintf(stdout," ? ");

      if (tr->min == tr->max)
        printf(" %d   ",tr->min);
      else
        printf(" %d-%d ",tr->min,tr->max);

      if (0 > tr->toid)
        printf("-> %d:+%d\n",tr->to->num,tr->toidrel);
      else
        printf("-> %d:%d\n",tr->to->num,tr->toid);
    }
  }
}

void fsm_print_simple(FILE *dotfile, fsm *f)
{
  list *sl;
  fprintf(dotfile,"  subgraph clustercomp {\n");
  fprintf(dotfile,"    label=\"Compressed form\";\n");

  for (sl = f->states; sl; sl = sl->next) {
    fsm_state *st = (fsm_state*)sl->data;
    list *tl;
    fprintf(dotfile,"    comp%d [label=\"%d\"];\n",st->num,st->num);

    for (tl = st->transitions; tl; tl = tl->next) {
      fsm_transition *tr = (fsm_transition*)tl->data;
      fprintf(dotfile,"    comp%d -> comp%d [label=\"",st->num,tr->to->num);

      
      if (tr->epsilon) {
/*         fprintf(dotfile,"?"); */
      }
      else {
        f->input_print(dotfile,tr->input);
      }

      if (NULL != st->transitions->next) {
        /* Multiple transitions; print details */
        if (tr->min == tr->max)
          fprintf(dotfile,"\\ns = %d",tr->min);
        else
          fprintf(dotfile,"\\n%d <= s <= %d",tr->min,tr->max);

        if (0 <= tr->toid) {
          if ((tr->min != tr->max) || (tr->toid != tr->min))
            fprintf(dotfile,"\\ns := %d",tr->toid);
        }
        else if (0 < tr->toidrel) {
          fprintf(dotfile,"\\ns := s + %d",tr->toidrel);
        }
        else if (0 > tr->toidrel) {
          fprintf(dotfile,"\\ns := s - %d",-tr->toidrel);
        }

      }

      fprintf(dotfile,"\"");

      if (tr->epsilon)
        fprintf(dotfile,",style=dashed];\n");
      else
        fprintf(dotfile,"];\n");
    }

  }

  fprintf(dotfile,"  }\n");
}

void fsm_print(FILE *dotfile, fsm *f, int stage)
{
  list *sl;
  fprintf(dotfile,"  subgraph cluster%d {\n",stage);
  fprintf(dotfile,"    label=\"Stage %d\";\n",stage);
  for (sl = f->states; sl; sl = sl->next) {
    fsm_state *st = (fsm_state*)sl->data;
    list *tl;
    int id;

    for (id = 0; id < st->count; id++) {

      if (st->ndfsm_states) {
        fprintf(dotfile,"    sx%dx%dx%d [label=\"",stage,st->num,id);
        print_statelist(dotfile,st->ndfsm_states);
      }
      else {
        fprintf(dotfile,"    sx%dx%dx%d [label=\"%d:%d",stage,st->num,id,st->num,id);
      }
      fprintf(dotfile,"\"");
      if (st->accept)
        fprintf(dotfile,"color=red");
      fprintf(dotfile,"];\n");

      for (tl = st->transitions; tl; tl = tl->next) {
        fsm_transition *tr = (fsm_transition*)tl->data;

        if (id >= tr->min && id <= tr->max) {

          int destid = tr->toid;
          if (0 > destid)
            destid = id + tr->toidrel;

          fprintf(dotfile,"    sx%dx%dx%d -> sx%dx%dx%d [label=\"",
                 stage,st->num,id,stage,tr->to->num,destid);

          if (tr->epsilon) {
    /*         fprintf(dotfile,"?"); */
          }
          else {
            f->input_print(dotfile,tr->input);
          }
          if (tr->epsilon)
            fprintf(dotfile,"\",style=dashed];\n");
          else
            fprintf(dotfile,"\"];\n");
        }
      }
    }
  }
  fprintf(dotfile,"  }\n");
}

void free_state(fsm_state *s)
{
  list_free(s->transitions,free);
  list_free(s->ndfsm_states,NULL);
  free(s);
}

void fsm_free(fsm *f)
{
  list_free(f->states,(void*)free_state);
  free(f);
}

/**
 * Replace all instances of multiple transitions from a given state with the same input token
 * with an extra state for each transition and an epsilon transition from the from state to the
 * new state
 */
void fsm_expand_duplicates(fsm *f)
{
  list *sl;
  for (sl = f->states; sl; sl = sl->next) {
    fsm_state *st = (fsm_state*)sl->data;
    int found_duplicate;
    do {
      list *tl1;
      list *duplicates = NULL;
      found_duplicate = 0;

      /* Look through all the transitions for duplicates. Once we find a duplicate, stop looking
         for duplicates with other input tokens, and just look for duplicates with the input token
         of the first duplicate found. */
      for (tl1 = st->transitions; tl1 && !found_duplicate; tl1 = tl1->next) {
        fsm_transition *tr1 = (fsm_transition*)tl1->data;
        list *tl2;

        if (tr1->epsilon)
          continue;

        for (tl2 = tl1->next; tl2; tl2 = tl2->next) {
          fsm_transition *tr2 = (fsm_transition*)tl2->data;
          if (!tr1->epsilon && !tr2->epsilon && f->input_equals(tr1->input,tr2->input)) {
            found_duplicate = 1;
            list_push(&duplicates,tr2);
          }
        }

        /* Add new states and epsilon transitions for each duplicate */
        if (found_duplicate) {
          list *dl;
          list_push(&duplicates,tr1);
          for (dl = duplicates; dl; dl = dl->next) {
            fsm_transition *dtr = (fsm_transition*)dl->data;
            fsm_transition *newtr;
            fsm_state *news = fsm_add_state(f);
            newtr = add_transition(news,dtr->to,dtr->input,0,0,0,0);
            dtr->to = news;
            dtr->input = NULL;
            dtr->epsilon = 1;
          }
          list_free(duplicates,NULL);
        }
      }

      /* Repeat the process until there are no groups of non-epsilon transitions from this state
         with the same input token */
    } while (found_duplicate);
  }
}

/**
 * Compare two state lists for equality. This is defined as being true if and only if both lists
 * have the same size, and contain the same states in each position.
 */
int statelists_equal(list *a, list *b)
{
  while (1) {
    if (!a && !b)
      return 1;
    if (!a || !b)
      return 0;
    if (((fsm_state*)a->data)->num != ((fsm_state*)b->data)->num)
      return 0;
    a = a->next;
    b = b->next;
  }
}

/**
 * Add a state to a list in the correct position based on it's numerical ordering
 */
void add_sorted(list **l, fsm_state *to)
{
  list **tostates_ptr = l;
  while (*tostates_ptr && (to->num > ((fsm_state*)(*tostates_ptr)->data)->num))
    tostates_ptr = &((*tostates_ptr)->next);
  list_push(tostates_ptr,to);
}

/**
 * Add the given state and all other states reachable via epsilon transitions to a list
 */
void add_reachable(dfsm_transition *d, fsm_state *st)
{
  list *l;
  add_sorted(&d->tostates,st);
  if (st->accept)
    d->accept = 1;
  for (l = st->transitions; l; l = l->next) {
    fsm_transition *tr = (fsm_transition*)l->data;
    if (tr->epsilon)
      add_reachable(d,tr->to);
  }
}

/**
 * Add a new (input,state) pair to the object representing a transition to be added to the
 * deterministic finite state machine. The state is a state from the non-deterministic machine.
 */
void add_dfsm_transition(fsm *df, list **dfsmtls, void *input, fsm_state *to)
{
  dfsm_transition *d;
  list *l;
  assert(input);
  for (l = *dfsmtls; l; l = l->next) {
    dfsm_transition *d = (dfsm_transition*)l->data;
    if (df->input_equals(input,d->input)) {
      add_reachable(d,to);
      return;
    }
  }
  d = (dfsm_transition*)calloc(1,sizeof(dfsm_transition));
  d->input = input;
  add_reachable(d,to);
  list_push(dfsmtls,d);
}

/**
 * Retrieve a state from a deterministic finite state machine corresponding to a list of
 * states in the nondeterministic machine. The list must be sorted in order of state no.
 */
fsm_state *get_dfsm_state_from_nfstatelist(fsm *df, list *nfstatelist)
{
  list *l;
  for (l = df->states; l; l = l->next) {
    fsm_state *dfsm_state = (fsm_state*)l->data;
    if (statelists_equal(dfsm_state->ndfsm_states,nfstatelist))
      return dfsm_state;
  }
  return NULL;
}

/**
 * Given a state in the deterministic finite state machine, add a set of transitions corresponding
 * to the set of transitions possible from the set of non-deterministic machine states that the
 * "from" state represents. New states in the deterministic machine will be created where
 * necessary.
 */
void add_dfsm_transitions_for_dfsm_state(fsm *df, fsm_state *from)
{
  list *nfal;
  list *tl;
  list *dfsm_transitions = NULL;

  for (nfal = from->ndfsm_states; nfal; nfal = nfal->next) {
    fsm_state *nfast = (fsm_state*)nfal->data;
    list *nfatl;
    for (nfatl = nfast->transitions; nfatl; nfatl = nfatl->next) {
      fsm_transition *nfatr = (fsm_transition*)nfatl->data;
      if (nfatr->epsilon)
        continue;
      add_dfsm_transition(df,&dfsm_transitions,nfatr->input,nfatr->to);
    }
  }

  for (tl = dfsm_transitions; tl; tl = tl->next) {
    dfsm_transition *dfsmtr = (dfsm_transition*)tl->data;
    fsm_state *dest_dfsm_state;
    if (NULL == (dest_dfsm_state = get_dfsm_state_from_nfstatelist(df,dfsmtr->tostates))) {
      dest_dfsm_state = fsm_add_state(df);
      dest_dfsm_state->ndfsm_states = dfsmtr->tostates;
    }
    else {
      list_free(dfsmtr->tostates,NULL);
    }
    add_transition(from,dest_dfsm_state,dfsmtr->input,0,0,0,0);
    if (dfsmtr->accept)
      dest_dfsm_state->accept = 1;
  }
  list_free(dfsm_transitions,free);
}

/**
 * Add the first state to a deterministic finite state machine. This consists of the start state
 * of the non-deterministic machine, plus all those states in the non-deterministic machine which
 * are reachable by epsilon transitions from the start state (i.e. all states that the
 * non-deterministic machine could be in without having yet received an input token).
 */
void add_first_dfsm_state(fsm *f, fsm *df)
{
  fsm_state *dfsm_state = fsm_add_state(df);
  list *l;
  list_push(&dfsm_state->ndfsm_states,f->start);
  for (l = f->start->transitions; l; l = l->next) {
    fsm_transition *tr = (fsm_transition*)l->data;
    if (tr->epsilon)
      list_push(&dfsm_state->ndfsm_states,tr->to);
  }
}

/**
 * Create a deterministic finite state machine from a non-deterministic machine. The resulting
 * deterministic machine will have one state for every possibly combination of simultaneous states
 * in the non-deterministic machine.
 */
void fsm_determinise(fsm *f, fsm *df)
{
  list *dfl;
  add_first_dfsm_state(f,df);
  assert(df->states);
  df->start = (fsm_state*)df->states->data;
  for (dfl = df->states; dfl; dfl = dfl->next) {
    add_dfsm_transitions_for_dfsm_state(df,(fsm_state*)dfl->data);
  }
}

void fsm_expand_and_determinise(fsm *f, fsm *df, FILE *outfile)
{
  if (outfile) {
    fprintf(outfile,"digraph {\n");
/*     fprintf(outfile,"  nodesep=2.0;\n"); */
    fprintf(outfile,"  rankdir=\"LR\";\n");

    printf("============= BEFORE TRANSITION OPTIMIZATION =============\n");
    fsm_dump(f);
    fsm_optimize_transitions(f);
    printf("============= AFTER TRANSITION OPTIMIZATION ==============\n");
    fsm_dump(f);

    fsm_print_simple(outfile,f);
    fsm_print(outfile,f,1);
  }
/*   fsm_expand_duplicates(f); */
/*   if (outfile) */
/*     fsm_print(outfile,f,2); */
/*   fsm_determinise(f,df); */
  if (outfile) {
/*     fsm_print(outfile,df,3); */
    fprintf(outfile,"}\n");
  }
}
