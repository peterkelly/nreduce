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

#include "engine.h"
#include "serialization.h"
#include "util/macros.h"
#include "util/debug.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

typedef struct df_activity df_activity;
typedef struct df_actdest df_actdest;
typedef struct df_state df_state;

struct df_actdest {
  df_activity *a;
  int p;
};

struct df_activity {
  df_instruction *instr;
  int remaining;
  df_value **values;
  df_actdest *outports;
  int actno;
  int refs;
  int fired;
};

struct df_state {
  df_program *program;
  list *activities;
  int actno;
  df_seqtype *intype;
  int trace;
  error_info *ei;
};

df_state *df_state_new(df_program *program, error_info *ei)
{
  df_state *state = (df_state*)calloc(1,sizeof(df_state));
  state->program = program;
  state->intype = df_seqtype_new_atomic(program->schema->globals->int_type);
  state->ei = ei;
  return state;
}

void df_state_free(df_state *state)
{
  df_seqtype_deref(state->intype);
  free(state);
}

void free_activity(df_state *state, df_activity *a)
{
  if (!a->fired) {
    int i;
    for (i = 0; i < a->instr->ninports; i++)
      if (NULL != a->values[i])
        df_value_deref(state->program->schema->globals,a->values[i]);
  }
  free(a->values);
  free(a->outports);
  free(a);
}

df_activity *df_activate_function(df_state *state, df_function *fun,
                                    df_activity *dest, int destp,
                                    df_activity **starts, int startslen)
{
  list *l;
  df_activity *start = NULL;
  int actno = state->actno++;
  int i;
  for (l = fun->instructions; l; l = l->next) {
    df_instruction *instr = (df_instruction*)l->data;
    df_activity *a;

    if (instr->internal)
      continue;

    a = (df_activity*)calloc(1,sizeof(df_activity));

    a->actno = actno;
    a->instr = instr;
    instr->act = a;
    list_push(&state->activities,a);
  }
  start = fun->start->act;
  start->refs++;

  assert(startslen == fun->nparams);
  for (i = 0; i < fun->nparams; i++) {
    starts[i] = fun->params[i].start->act;
    starts[i]->refs++;
  }

  for (l = fun->instructions; l; l = l->next) {
    df_instruction *instr = (df_instruction*)l->data;
    df_activity *a;
    int i;

    if (instr->internal)
      continue;

    a = instr->act;
    a->remaining = instr->ninports;
    a->outports = (df_actdest*)calloc(a->instr->noutports,sizeof(df_actdest));
    a->values = (df_value**)calloc(a->instr->ninports,sizeof(df_value*));

    if (OP_RETURN == instr->opcode) {
      a->outports[0].a = dest;
      a->outports[0].p = destp;
    }
    else {
      for (i = 0; i < instr->noutports; i++) {
        if (NULL == instr->outports[i].dest) {
          fprintf(stderr,"Instruction %s:%d has no destination assigned to output port %d\n",
                  fun->ident.name,instr->id,i);
        }
        assert(NULL != instr->outports[i].dest);
        a->outports[i].a = instr->outports[i].dest->act;
        a->outports[i].a->refs++;
        a->outports[i].p = instr->outports[i].destp;
      }
    }
  }
  for (l = fun->instructions; l; l = l->next)
    ((df_instruction*)l->data)->act = NULL;
  return start;
}

void df_set_input(df_activity *a, int p, df_value *v)
{
  a->values[p] = v;
  a->remaining--;
  a->refs--;
  assert(0 <= a->remaining);
  assert(0 <= a->refs);
}

void df_output_value(df_activity *source, int sourcep, df_value *v)
{
  df_activity *dest = source->outports[sourcep].a;
  int destp = source->outports[sourcep].p;
  assert(NULL == dest->values[destp]);
  df_set_input(dest,destp,v);
}

void df_print_actmsg(const char *prefix, df_activity *a)
{
  printf("%s [%d] %s.%d - %s\n",prefix,
         a->actno,a->instr->fun->ident.name,a->instr->id,df_opstr(a->instr->opcode));
}

void df_deref_activity(df_activity *a, int indent, df_activity *merge)
{
  int i;
  a->refs--;
  assert(0 <= a->refs);
  assert(OP_MERGE == merge->instr->opcode);
  if ((OP_RETURN == a->instr->opcode) || (merge == a))
    return;
  if (0 == a->refs) {
    for (i = 0; i < a->instr->noutports; i++)
      df_deref_activity(a->outports[i].a,indent+1,merge);
  }
}


int df_fire_activity(df_state *state, df_activity *a)
{
  xs_globals *g = state->program->schema->globals;
  if (state->trace)
    df_print_actmsg("Firing",a);

  switch (a->instr->opcode) {
  case OP_CONST:
    df_output_value(a,0,df_value_ref(a->instr->cvalue));
    df_value_deref(g,a->values[0]);
    break;
  case OP_DUP:
    df_output_value(a,0,df_value_ref(a->values[0]));
    df_output_value(a,1,df_value_ref(a->values[0]));
    df_value_deref(g,a->values[0]);
    break;
  case OP_SPLIT:
    if (0 == a->values[0]->value.b) {
      df_output_value(a,0,a->values[1]);
      df_deref_activity(a->outports[1].a,0,a->outports[2].a);
    }
    else {
      df_output_value(a,1,a->values[1]);
      df_deref_activity(a->outports[0].a,0,a->outports[2].a);
    }
    df_output_value(a,2,a->values[0]);
    break;
  case OP_MERGE:
    df_output_value(a,0,a->values[0]);
    df_value_deref(state->program->schema->globals,a->values[1]);
    break;
  case OP_CALL: {
    df_activity **acts = (df_activity**)alloca(a->instr->cfun->nparams*sizeof(df_activity*));
    df_activity *start;
    int i;
    memset(acts,0,a->instr->cfun->nparams*sizeof(df_activity*));
    start = df_activate_function(state,a->instr->cfun,a->outports[0].a,a->outports[0].p,
                                  acts,a->instr->cfun->nparams);
    for (i = 0; i < a->instr->cfun->nparams; i++) {
      assert(NULL != a->values[i]);
      assert(NULL != acts[i]);
      df_set_input(acts[i],0,a->values[i]);
    }

    if (0 == a->instr->cfun->nparams)
      df_set_input(start,0,a->values[0]);
    else
      df_set_input(start,0,df_value_ref(a->values[0]));

    break;
  }
  case OP_MAP: {
    df_activity **acts = (df_activity**)alloca(a->instr->cfun->nparams*sizeof(df_activity*));
    list *values = df_sequence_to_list(a->values[0]);
    list *l;
    int i;
    df_activity *ultimate_desta = a->outports[0].a;
    int ultimate_destp = a->outports[0].p;

    if (NULL == values) {
      df_seqtype *empty = df_seqtype_new(SEQTYPE_EMPTY);
      df_value *ev = df_value_new(empty);
      df_seqtype_deref(empty);
      df_output_value(a,0,ev);
    }

    for (l = values; l; l = l->next) {
      df_value *v = (df_value*)l->data;

      df_activity *start;

      df_activity *desta = ultimate_desta;
      int destp = ultimate_destp;

      if (l->next) {
        df_activity *seq = (df_activity*)calloc(1,sizeof(df_activity));
        list_push(&state->activities,seq);
        assert(NULL != a->instr->fun->mapseq);
        seq->instr = a->instr->fun->mapseq;
        seq->remaining = 2;
        seq->values = (df_value**)calloc(2,sizeof(df_value*));
        seq->outports = (df_actdest*)calloc(1,sizeof(df_actdest));
        seq->outports[0].a = ultimate_desta;
        seq->outports[0].p = ultimate_destp;
        seq->actno = a->actno;
        seq->refs = 2;

        desta = seq;
        destp = 0;
        ultimate_desta = seq;
        ultimate_destp = 1;
      }

      memset(acts,0,a->instr->cfun->nparams*sizeof(df_activity*));
      start = df_activate_function(state,a->instr->cfun,desta,destp,
                                    acts,a->instr->cfun->nparams);

      df_set_input(start,0,df_value_ref(v));
      for (i = 0; i < a->instr->cfun->nparams; i++) {
        assert(NULL != a->values[i+1]);
        assert(NULL != acts[i]);
        df_set_input(acts[i],0,df_value_ref(a->values[i+1]));
      }
    }

    for (i = 0; i < a->instr->ninports; i++)
      df_value_deref(g,a->values[i]);

    list_free(values,NULL);
    break;
  }
  case OP_PASS:
    df_output_value(a,0,a->values[0]);
    break;
  case OP_SWALLOW:
    df_value_deref(g,a->values[0]);
    break;
  case OP_RETURN:
    df_output_value(a,0,a->values[0]);
    break;
  case OP_PRINT: {

    if (SEQTYPE_EMPTY != a->values[0]->seqtype->type) {
      stringbuf *buf = stringbuf_new();
      df_seroptions *options = df_seroptions_new(nsname_temp(NULL,"xml"));

      if (0 != df_serialize(g,a->values[0],buf,options,state->ei)) {
        df_value_deref(g,a->values[0]);
        df_seroptions_free(options);
        return -1;
      }

      printf("%s",buf->data);

      stringbuf_free(buf);
      df_seroptions_free(options);
    }

    df_value_deref(g,a->values[0]);
    break;
  }
  case OP_GATE:
    df_output_value(a,0,a->values[0]);
    df_value_deref(g,a->values[1]);
    break;
  case OP_BUILTIN: {
    gxcontext ctxt;
    df_value *result;
    int i;
    memset(&ctxt,0,sizeof(gxcontext));
    ctxt.item = a->values[0];
    ctxt.position = 1;
    ctxt.size = 1;
    ctxt.g = g;
    ctxt.ei = state->ei;
    ctxt.sloc = a->instr->sloc;
    ctxt.space_decls = state->program->space_decls;
    ctxt.instr = a->instr;
    if (NULL == (result = a->instr->bif->fun(&ctxt,a->values)))
      return -1;
    df_output_value(a,0,result);
    for (i = 0; i < a->instr->bif->nargs; i++)
      df_value_deref(g,a->values[i]);
    break;
  }
  default:
    assert(!"invalid opcode");
    break;
  }

  return 0;
}

int df_execute_network(df_state *state)
{
  int found;
  int err = 0;
  do {
    list **listptr = &state->activities;
    found = 0;
    while (NULL != *listptr) {
      list *l = *listptr;
      df_activity *a = (df_activity*)((*listptr)->data);

      if ((0 == a->remaining) && !a->fired) {
        if (0 != df_fire_activity(state,a))
          err = 1;
        else
          a->fired = 1;
        found = 1;
      }

      else if (0 == a->refs) {
        /* just remove it */
        *listptr = (*listptr)->next;
        found = 1;
        free_activity(state,a);
        free(l);
        break;
      }
      listptr = &(*listptr)->next;
    }
  } while (found && !err);

  if (err) {
    list *l;
    for (l = state->activities; l; l = l->next) {
      df_activity *a = (df_activity*)l->data;
      free_activity(state,a);
    }
    list_free(state->activities,NULL);
  }
  else if (NULL != state->activities) {
    list *l;
    fprintf(stderr,"The following activities remain outstanding:\n");
    for (l = state->activities; l; l = l->next) {
      df_activity *a = (df_activity*)l->data;
      fprintf(stderr,"[%d] %s.%d - %s\n",
             a->actno,a->instr->fun->ident.name,a->instr->id,df_opstr(a->instr->opcode));
    }
  }

  if (err)
    return -1;
  else
    return 0;
}

int df_execute(df_program *program, int trace, error_info *ei, df_value *context)
{
  df_function *mainfun;
  df_function *init;
  df_activity *print;
  df_activity *start;
  df_state *state = NULL;
  int r;

  mainfun = df_lookup_function(program,nsname_temp(GX_NAMESPACE,"main"));
  if (NULL == mainfun)
    mainfun = df_lookup_function(program,nsname_temp(NULL,"main"));
  if (NULL == mainfun)
    mainfun = df_lookup_function(program,nsname_temp(NULL,"default"));

  if (NULL == mainfun) {
    fprintf(stderr,"No main function defined\n");
    return -1;
  }

  init = (df_function*)calloc(1,sizeof(df_function));
  init->ident = nsname_new(NULL,"_init");
  init->program = program;
  init->start = df_add_instruction(program,init,OP_PRINT,nosourceloc);
  init->rtype = df_seqtype_new_atomic(program->schema->globals->complex_ur_type);

  state = df_state_new(program,ei);
  state->trace = trace;

  print = (df_activity*)calloc(1,sizeof(df_activity));
  list_push(&state->activities,print);
  print->instr = init->start;
  print->values = (df_value**)calloc(1,sizeof(df_value*));
  print->remaining = 1;
  print->refs++;

  /* FIXME: support main functions with >0 params (where do we get the values of these from?) */
  assert(0 == mainfun->nparams);

  start = df_activate_function(state,mainfun,print,0,NULL,0);

  if (NULL == context) {
    context = df_value_new(state->intype);
    context->value.i = 0;
  }
  df_set_input(start,0,context);

  r = df_execute_network(state);

  df_state_free(state);

  df_free_function(init);

  return r;
}
