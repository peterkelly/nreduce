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

#include "validity.h"
#include "util/debug.h"
#include <assert.h>

using namespace GridXSLT;

void df_check_portsmatch(Program *program, Function *fun)
{
  Iterator<Instruction*> it;
  for (it = fun->m_instructions; it.haveCurrent(); it++) {
    Instruction *instr = *it;
    int i;
    for (i = 0; i < instr->m_ninports; i++) {
      if (NULL != instr->m_inports[i].source) {
        OutputPort *outport = &instr->m_inports[i].source->m_outports[instr->m_inports[i].sourcep];
        assert(outport->dest == instr);
        assert(outport->destp == i);
      }
    }
    for (i = 0; i < instr->m_noutports; i++) {
      if ((NULL != instr->m_outports[i].dest) &&
          (OP_MERGE != instr->m_outports[i].dest->m_opcode)) {
        InputPort *inport = &instr->m_outports[i].dest->m_inports[instr->m_outports[i].destp];


/*         if (inport->source != instr) { */
/*           debugl("%s:%d.%d (%s) points to %s:%d.%d (%s), but the destination points back to " */
/*                  "%s:%d.%d (%s)", */
/*                  fun->name,instr->m_id,i,OpCodeNames[instr->m_opcode], */
/*                  fun->name,instr->m_outports[i].dest->id,instr->m_outports[i].destp, */
/*                  OpCodeNames[instr->m_outports[i].dest->opcode], */
/*                  fun->name,inport->source->id,inport->sourcep,OpCodeNames[inport->source->opcode]); */
/*         } */

        assert(inport->source == instr);
        assert(inport->sourcep == i);
      }
    }
  }
}

int df_check_function_connected(Function *fun)
{
/*   debug("df_check_function_connected: %s\n",fun->ident.name); */

  /* Check output ports first and set the source of the input ports they point to */
  Iterator<Instruction*> it;
  for (it = fun->m_instructions; it.haveCurrent(); it++) {
    Instruction *instr = *it;
    int i;
    if (instr->m_internal)
      continue;
    for (i = 0; i < instr->m_noutports; i++) {


      if ((NULL == instr->m_outports[i].dest) &&
          (OP_RETURN != instr->m_opcode)) {
        fmessage(stderr,"Output port %*:%d.%d (of %p %s) is not connected\n",
                &fun->m_ident.m_name,instr->m_id,i,instr,OpCodeNames[instr->m_opcode]);
        return -1;
      }
      if (NULL != instr->m_outports[i].dest) {
        Instruction *dest = instr->m_outports[i].dest;
        int destp = instr->m_outports[i].destp;


/*         debug("Output port %s:%d.%d (of %p %s) -> input port %s:%d.%d (of %p %s)\n", */
/*               fun->ident.name,instr->m_id,i,instr,OpCodeNames[instr->m_opcode],fun->ident.name, */
/*               dest->id,destp,dest,OpCodeNames[dest->opcode]); */


        if (instr->m_outports[i].dest->m_fun != instr->m_fun) {
          fmessage(stderr,"Output port %*:%d.%d (of %p %s) is connected to different function: "
                  "input port %*:%d.%d (of %p %s)\n",&fun->m_ident.m_name,instr->m_id,i,instr,
                  OpCodeNames[instr->m_opcode],&dest->m_fun->m_ident.m_name,dest->m_id,destp,dest,
                  OpCodeNames[dest->m_opcode]);
          return -1;
        }


        if (destp >= dest->m_ninports) {
          fmessage(stderr,"Output port %*:%d.%d (of %p %s) is connected to non-existant "
                  "input port %*:%d.%d (of %p %s)\n",&fun->m_ident.m_name,instr->m_id,i,instr,
                  OpCodeNames[instr->m_opcode],&dest->m_fun->m_ident.m_name,dest->m_id,destp,dest,
                  OpCodeNames[dest->m_opcode]);
          return -1;
        }
        else {
          InputPort *inport = &dest->m_inports[destp];
          inport->source = instr;
          inport->sourcep = i;
        }
      }
    }
  }

  /* Now check input ports */
  for (it = fun->m_instructions; it.haveCurrent(); it++) {
    Instruction *instr = *it;
    int i;

    /* don't check inputs for parameters or start instruction */
    int isparam = 0;

    if (instr->m_internal)
      continue;

    if (instr == fun->m_start)
      continue;
    for (i = 0; i < fun->m_nparams; i++)
      if (instr == fun->m_params[i].start)
        isparam = 1;
    if (isparam)
      continue;

    for (i = 0; i < instr->m_ninports; i++) {
      if ((NULL == instr->m_inports[i].source) && !instr->m_inports[i].from_special) {
        fmessage(stderr,"Input port %*:%d.%d (of %p %s) is not connected\n",
                &fun->m_ident.m_name,instr->m_id,i,instr,OpCodeNames[instr->m_opcode]);
        return -1;
      }
    }
  }
  /* FIXME: check for loops */
  return 0;
}

