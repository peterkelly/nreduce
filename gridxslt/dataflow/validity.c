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

#include "validity.h"
#include <assert.h>

void df_check_portsmatch(df_state *state, df_function *fun)
{
  list *l;
  for (l = fun->instructions; l; l = l->next) {
    df_instruction *instr = (df_instruction*)l->data;
    int i;
    for (i = 0; i < instr->ninports; i++) {
      if (NULL != instr->inports[i].source) {
        df_outport *outport = &instr->inports[i].source->outports[instr->inports[i].sourcep];
        assert(outport->dest == instr);
        assert(outport->destp == i);
      }
    }
    for (i = 0; i < instr->noutports; i++) {
      if ((NULL != instr->outports[i].dest) &&
          (OP_SPECIAL_MERGE != instr->outports[i].dest->opcode)) {
        df_inport *inport = &instr->outports[i].dest->inports[instr->outports[i].destp];


/*         if (inport->source != instr) { */
/*           debug("%s:%d.%d (%s) points to %s:%d.%d (%s), but the destination points back to " */
/*                  "%s:%d.%d (%s)", */
/*                  fun->name,instr->id,i,df_opstr(instr->opcode), */
/*                  fun->name,instr->outports[i].dest->id,instr->outports[i].destp, */
/*                  df_opstr(instr->outports[i].dest->opcode), */
/*                  fun->name,inport->source->id,inport->sourcep,df_opstr(inport->source->opcode)); */
/*         } */

        assert(inport->source == instr);
        assert(inport->sourcep == i);
      }
    }
  }
}

int df_check_function_connected(df_function *fun)
{
  list *l;

  /* Check output ports first and set the source of the input ports they point to */
  for (l = fun->instructions; l; l = l->next) {
    df_instruction *instr = (df_instruction*)l->data;
    int i;
    if (instr->internal)
      continue;
    for (i = 0; i < instr->noutports; i++) {
      if ((NULL == instr->outports[i].dest) &&
          (OP_SPECIAL_RETURN != instr->opcode)) {
        fprintf(stderr,"Output port %s:%d.%d (of %s) is not connected\n",fun->name,instr->id,i,
                df_opstr(instr->opcode));
        return -1;
      }
      if (NULL != instr->outports[i].dest) {
        df_instruction *dest = instr->outports[i].dest;
        int destp = instr->outports[i].destp;
        df_inport *inport = &dest->inports[destp];
        inport->source = instr;
        inport->sourcep = i;
      }
    }
  }

  /* Now check input ports */
  for (l = fun->instructions; l; l = l->next) {
    df_instruction *instr = (df_instruction*)l->data;
    int i;

    /* don't check inputs for parameters or start instruction */
    int isparam = 0;

    if (instr->internal)
      continue;

    if (instr == fun->start)
      continue;
    for (i = 0; i < fun->nparams; i++)
      if (instr == fun->params[i].start)
        isparam = 1;
    if (isparam)
      continue;

    for (i = 0; i < instr->ninports; i++) {
      if ((NULL == instr->inports[i].source) && !instr->inports[i].from_special) {
        fprintf(stderr,"Input port %s:%d.%d (of %s) is not connected\n",fun->name,instr->id,i,
                df_opstr(instr->opcode));
        return -1;
      }
    }
  }
  /* FIXME: check for loops */
  return 0;
}

