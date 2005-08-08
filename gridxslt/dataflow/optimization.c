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

#include "optimization.h"
#include "validity.h"

void df_remove_redundant(df_state *state, df_function *fun)
{
  while (1) {
    list *l;
    int removed = 0;
    for (l = fun->instructions; l; l = l->next) {
      df_instruction *instr = (df_instruction*)l->data;
      if ((OP_SPECIAL_SWALLOW == instr->opcode) &&
          (NULL != instr->inports[0].source) &&
          (OP_SPECIAL_DUP == instr->inports[0].source->opcode) &&
          (NULL != instr->inports[0].source->inports[0].source)) {

        df_instruction *dup = instr->inports[0].source;
        df_instruction *source = dup->inports[0].source;
        int sourcep = dup->inports[0].sourcep;
        df_instruction *dest;
        int destp;

/*         debug("Redundant: swallow %s:%d, dup %s:%d", */
/*                fun->name,instr->id,fun->name,dup->id); */

        if (0 == instr->inports[0].sourcep) {
          dest = dup->outports[1].dest;
          destp = dup->outports[1].destp;
        }
        else {
          dest = dup->outports[0].dest;
          destp = dup->outports[0].destp;
        }

        source->outports[sourcep].dest = dest;
        source->outports[sourcep].destp = destp;
        dest->inports[destp].source = source;
        dest->inports[destp].sourcep = sourcep;
        df_delete_instruction(state,fun,instr);
        df_delete_instruction(state,fun,dup);
        removed = 1;
        break;
      }
    }
    df_check_portsmatch(state,fun);
    if (!removed)
      return;
  }
}

