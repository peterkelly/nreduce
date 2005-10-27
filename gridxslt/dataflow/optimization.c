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

using namespace GridXSLT;

void df_remove_redundant(Program *program, Function *fun)
{
  while (1) {
    int removed = 0;
    Iterator<Instruction*> it;
    for (it = fun->m_instructions; it.haveCurrent(); it++) {
      Instruction *instr = *it;
      if ((OP_SWALLOW == instr->m_opcode) &&
          (NULL != instr->m_inports[0].source) &&
          (OP_DUP == instr->m_inports[0].source->m_opcode) &&
          (NULL != instr->m_inports[0].source->m_inports[0].source)) {

        Instruction *dup = instr->m_inports[0].source;
        Instruction *source = dup->m_inports[0].source;
        int sourcep = dup->m_inports[0].sourcep;
        Instruction *dest;
        int destp;

/*         debugl("Redundant: swallow %s:%d, dup %s:%d", */
/*                fun->name,instr->m_id,fun->name,dup->id); */

        if (0 == instr->m_inports[0].sourcep) {
          dest = dup->m_outports[1].dest;
          destp = dup->m_outports[1].destp;
        }
        else {
          dest = dup->m_outports[0].dest;
          destp = dup->m_outports[0].destp;
        }

        source->m_outports[sourcep].dest = dest;
        source->m_outports[sourcep].destp = destp;
        dest->m_inports[destp].source = source;
        dest->m_inports[destp].sourcep = sourcep;
        fun->deleteInstruction(instr);
        fun->deleteInstruction(dup);
        removed = 1;
        break;
      }
    }
    df_check_portsmatch(program,fun);
    if (!removed)
      return;
  }
}

