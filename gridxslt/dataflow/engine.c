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

namespace GridXSLT {

class Activity;

struct df_actdest {
  Activity *a;
  int p;
};

class Activity {
public:
  Activity();
  ~Activity();

  Instruction *instr;
  int remaining;
  Value *values;
  df_actdest *outports;
  int actno;
  int refs;
  int fired;
};

class ExecutionState {
public:
  ExecutionState(Program *_program, Error *_ei);
  ~ExecutionState();

  Program *program;
  List<Activity*> activities;
  int actno;
  SequenceType intype;
  int trace;
  Error *ei;
};

};

using namespace GridXSLT;

Activity::Activity()
  : instr(NULL),
    remaining(0),
    values(NULL),
    outports(NULL),
    actno(0),
    refs(0),
    fired(0)
{
}

Activity::~Activity()
{
  delete [] values;
  free(outports);
}

ExecutionState::ExecutionState(Program *_program, Error *_ei)
  : program(_program), actno(0), trace(0), ei(_ei)
{
  intype = SequenceType(xs_g->int_type);
}

ExecutionState::~ExecutionState()
{
}

Activity *df_activate_function(ExecutionState *state, Function *fun,
                                    Activity *dest, int destp,
                                    Activity **starts, int startslen)
{
  Activity *start = NULL;
  int actno = state->actno++;
  int i;
  Iterator<Instruction*> it;
  for (it = fun->m_instructions; it.haveCurrent(); it++) {
    Instruction *instr = *it;
    Activity *a;

    if (instr->m_internal)
      continue;

    a = new Activity();

    a->actno = actno;
    a->instr = instr;
    instr->m_act = a;
    state->activities.push(a);
  }
  start = fun->m_start->m_act;
  start->refs++;

  assert(startslen == fun->m_nparams);
  for (i = 0; i < fun->m_nparams; i++) {
    starts[i] = fun->m_params[i].start->m_act;
    starts[i]->refs++;
  }

  for (it = fun->m_instructions; it.haveCurrent(); it++) {
    Instruction *instr = *it;
    Activity *a;
    int i;

    if (instr->m_internal)
      continue;

    a = instr->m_act;
    a->remaining = instr->m_ninports;
    a->outports = (df_actdest*)calloc(a->instr->m_noutports,sizeof(df_actdest));
    a->values = new Value[a->instr->m_ninports];

    if (OP_RETURN == instr->m_opcode) {
      a->outports[0].a = dest;
      a->outports[0].p = destp;
    }
    else {
      for (i = 0; i < instr->m_noutports; i++) {
        if (NULL == instr->m_outports[i].dest) {
          fmessage(stderr,"Instruction %*:%d has no destination assigned to output port %d\n",
                  &fun->m_ident.m_name,instr->m_id,i);
        }
        assert(NULL != instr->m_outports[i].dest);
        a->outports[i].a = instr->m_outports[i].dest->m_act;
        a->outports[i].a->refs++;
        a->outports[i].p = instr->m_outports[i].destp;
      }
    }
  }
  for (it = fun->m_instructions; it.haveCurrent(); it++)
    (*it)->m_act = NULL;
  return start;
}

void df_set_input(Activity *a, int p, const Value &v)
{
  a->values[p] = v;
  a->remaining--;
  a->refs--;
  assert(0 <= a->remaining);
  assert(0 <= a->refs);
}

void df_output_value(Activity *source, int sourcep, const Value &v)
{
  Activity *dest = source->outports[sourcep].a;
  int destp = source->outports[sourcep].p;
  assert(dest->values[destp].isNull());
  df_set_input(dest,destp,v);
}

void df_print_actmsg(const char *prefix, Activity *a)
{
  message("%s [%d] %*.%d - %s\n",prefix,
         a->actno,&a->instr->m_fun->m_ident.m_name,a->instr->m_id,OpCodeNames[a->instr->m_opcode]);
}

void df_deref_activity(Activity *a, int indent, Activity *merge)
{
  int i;
  a->refs--;
  assert(0 <= a->refs);
  assert(OP_MERGE == merge->instr->m_opcode);
  if ((OP_RETURN == a->instr->m_opcode) || (merge == a))
    return;
  if (0 == a->refs) {
    for (i = 0; i < a->instr->m_noutports; i++)
      df_deref_activity(a->outports[i].a,indent+1,merge);
  }
}


int df_fire_activity(ExecutionState *state, Activity *a)
{
  int i;
  if (state->trace)
    df_print_actmsg("Firing",a);

  for (i = 0; i < a->instr->m_ninports; i++) {
    if (!a->values[i].type().isDerivedFrom(a->instr->m_inports[i].st)) {
      StringBuffer buf1;
      StringBuffer buf2;
      a->values[i].type().printFS(buf1,xs_g->namespaces);
      a->instr->m_inports[i].st.printFS(buf2,xs_g->namespaces);
      error(state->ei,a->instr->m_sloc.uri,a->instr->m_sloc.line,String::null(),
            "Instruction %*:%d: Sequence type mismatch: %* does not match %*",
            &a->instr->m_fun->m_ident,a->instr->m_id,&buf1,&buf2);
      return -1;
    }
  }

  switch (a->instr->m_opcode) {
  case OP_CONST:
    df_output_value(a,0,a->instr->m_cvalue);
    break;
  case OP_DUP:
    df_output_value(a,0,a->values[0]);
    df_output_value(a,1,a->values[0]);
    break;
  case OP_SPLIT:
    assert(a->values[0].isDerivedFrom(xs_g->boolean_type));
    if (!a->values[0].asBool()) {
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
    break;
  case OP_CALL: {
    Activity **acts = (Activity**)alloca(a->instr->m_cfun->m_nparams*sizeof(Activity*));
    Activity *start;
    int i;
    memset(acts,0,a->instr->m_cfun->m_nparams*sizeof(Activity*));
    start = df_activate_function(state,a->instr->m_cfun,a->outports[0].a,a->outports[0].p,
                                  acts,a->instr->m_cfun->m_nparams);
    for (i = 0; i < a->instr->m_cfun->m_nparams; i++) {
      assert(!a->values[i].isNull());
      assert(NULL != acts[i]);
      df_set_input(acts[i],0,a->values[i]);
    }

    if (0 == a->instr->m_cfun->m_nparams)
      df_set_input(start,0,a->values[0]);
    else
      df_set_input(start,0,a->values[0]);

    break;
  }
  case OP_MAP: {
    Activity **acts = (Activity**)alloca(a->instr->m_cfun->m_nparams*sizeof(Activity*));
    List<Value> values = a->values[0].sequenceToList();
    int i;
    Activity *ultimate_desta = a->outports[0].a;
    int ultimate_destp = a->outports[0].p;

    if (values.isEmpty()) {
      SequenceType empty = SequenceType(SEQTYPE_EMPTY);
      df_output_value(a,0,Value(empty));
    }

    Iterator<Value> it;
    for (it = values; it.haveCurrent(); it++) {
      Value v = *it;

      Activity *start;

      Activity *desta = ultimate_desta;
      int destp = ultimate_destp;

      if (it.haveNext()) {
        Activity *seq = new Activity();
        state->activities.push(seq);
        assert(NULL != a->instr->m_fun->m_mapseq);
        seq->instr = a->instr->m_fun->m_mapseq;
        seq->remaining = 2;
        seq->values = new Value[2];
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

      memset(acts,0,a->instr->m_cfun->m_nparams*sizeof(Activity*));
      start = df_activate_function(state,a->instr->m_cfun,desta,destp,
                                    acts,a->instr->m_cfun->m_nparams);

      df_set_input(start,0,v);
      for (i = 0; i < a->instr->m_cfun->m_nparams; i++) {
        assert(!a->values[i+1].isNull());
        assert(NULL != acts[i]);
        df_set_input(acts[i],0,a->values[i+1]);
      }
    }

    break;
  }
  case OP_PASS:
    df_output_value(a,0,a->values[0]);
    break;
  case OP_SWALLOW:
    break;
  case OP_RETURN:
    df_output_value(a,0,a->values[0]);
    break;
  case OP_PRINT: {

    if (SEQTYPE_EMPTY != a->values[0].type().type()) {
      stringbuf *buf = stringbuf_new();
      df_seroptions *options = new df_seroptions(NSName(String::null(),"xml"));

      if (0 != df_serialize(a->values[0],buf,options,state->ei)) {
        delete options;
        return -1;
      }

      message("%s",buf->data);

      stringbuf_free(buf);
      delete options;
    }

    break;
  }
  case OP_GATE:
    df_output_value(a,0,a->values[0]);
    break;
  case OP_BUILTIN: {
    Environment env;
    Value result;
    int i;
    /* FIXME: what if a->values[0] is a sequence? */
    env.ctxt = new Context();
    env.ctxt->item = a->values[0];
    env.ctxt->position = 1;
    env.ctxt->size = 1;
    env.ctxt->havefocus = 1;
    env.ei = state->ei;
    env.sloc = a->instr->m_sloc;
    env.space_decls = state->program->m_space_decls;
    env.instr = a->instr;

    List<Value> values;
    for (i = 0; i < a->instr->m_ninports; i++)
      values.append(a->values[i]);

    result = a->instr->m_bif->m_fun(&env,values);
    if (result.isNull())
      return -1;
    df_output_value(a,0,result);
    break;
  }
  default:
    assert(!"invalid opcode");
    break;
  }

  return 0;
}

int df_execute_network(ExecutionState *state)
{
  int found;
  int err = 0;
  Iterator<Activity*> it;
  do {
    it = state->activities;
    found = 0;
    while (it.haveCurrent()) {
      Activity *a = *it;

      if ((0 == a->remaining) && !a->fired) {
        if (0 != df_fire_activity(state,a))
          err = 1;
        else
          a->fired = 1;
        found = 1;
      }

      else if (0 == a->refs) {
        /* just remove it */
        found = 1;
        delete a;
        it.remove();
        break;
      }
      it++;
    }
  } while (found && !err);

  if (err) {
    for (it = state->activities; it.haveCurrent(); it++)
      delete *it;
  }
  else if (!state->activities.isEmpty()) {
    fmessage(stderr,"The following activities remain outstanding:\n");
    for (it = state->activities; it.haveCurrent(); it++) {
      Activity *a = *it;
      fmessage(stderr,"[%d] %*.%d - %s\n",
             a->actno,&a->instr->m_fun->m_ident.m_name,
             a->instr->m_id,OpCodeNames[a->instr->m_opcode]);
    }
  }

  if (err)
    return -1;
  else
    return 0;
}

int df_execute(Program *program, int trace, Error *ei, const Value &context)
{
  Function *mainfun;
  Function *init;
  Activity *print;
  Activity *start;
  ExecutionState *state = NULL;
  SequenceType st;
  int r;

  mainfun = program->getFunction(NSName(GX_NAMESPACE,"main"));
  if (NULL == mainfun)
    mainfun = program->getFunction(NSName(String::null(),"main"));
  if (NULL == mainfun)
    mainfun = program->getFunction(NSName(String::null(),"default"));

  if (NULL == mainfun) {
    fmessage(stderr,"No main function defined\n");
    return -1;
  }

  init = new Function();
  init->m_ident = NSName(String::null(),"_init");
  init->m_program = program;
  init->m_start = init->addInstruction(OP_PRINT,nosourceloc);

  SequenceType item = SequenceType::item();
  st = SequenceType::occurrence(item,OCCURS_ZERO_OR_MORE);
  init->m_start->m_inports[0].st = st;

  init->m_rtype = SequenceType(xs_g->complex_ur_type);

  state = new ExecutionState(program,ei);
  state->trace = trace;

  print = new Activity();
  state->activities.push(print);
  print->instr = init->m_start;
  print->values = new Value[1];
  print->remaining = 1;
  print->refs++;

  /* FIXME: support main functions with >0 params (where do we get the values of these from?) */
  assert(0 == mainfun->m_nparams);

  start = df_activate_function(state,mainfun,print,0,NULL,0);

  if (context.isNull())
    df_set_input(start,0,Value(0));
  else
    df_set_input(start,0,context);

  r = df_execute_network(state);

  delete state;
  delete init;

  return r;
}
