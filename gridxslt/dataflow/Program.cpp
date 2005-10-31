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

#include "Program.h"
#include "util/macros.h"
#include "util/Namespace.h"
#include "util/debug.h"
#include <string.h>
#include <errno.h>
#include <ctype.h>
#include <libxml/parser.h>
#include <libxml/tree.h>

#define _DATAFLOW_PROGRAM_CPP

using namespace GridXSLT;

const char *GridXSLT::OpCodeNames[GridXSLT::OP_COUNT] = {
  "const",
  "dup",
  "split",
  "merge",
  "call",
  "map",
  "pass",
  "swallow",
  "return",
  "print",
  "gate",
  "builtin",
};

Instruction::Instruction()
  : m_opcode(0),
    m_id(0),
    m_ninports(0),
    m_inports(NULL),
    m_noutports(0),
    m_outports(NULL),
    m_cfun(NULL),
    m_bif(NULL),
    m_act(NULL),
    m_fun(NULL),
    m_internal(0),
    m_computed(0),
    m_axis(0),
    m_seroptions(NULL),
    m_nsdefs(NULL)
{
  m_sloc.uri = NULL;
  m_sloc.line = 0;
}

Instruction::~Instruction()
{
  freeInports();
  delete [] m_outports;

  if (NULL != m_seroptions)
    delete m_seroptions;

  list *l;
  for (l = m_nsdefs; l; l = l->next)
    delete static_cast<ns_def*>(l->data);
  list_free(m_nsdefs,NULL);
  sourceloc_free(m_sloc);
}

void Instruction::freeInports()
{
  delete [] m_inports;
}

void Instruction_compute_types(Instruction *instr)
{
  unsigned int i;

  instr->m_computed = 1;

  debug("Instruction_compute_types %* %d (%s)\n",
        &instr->m_fun->m_ident,instr->m_id,OpCodeNames[instr->m_opcode]);

  switch (instr->m_opcode) {
  case OP_CONST:
    break;
  case OP_DUP:
    instr->m_outports[0].st = instr->m_inports[0].st;
    instr->m_outports[1].st = instr->m_inports[0].st;
    break;
  case OP_SPLIT:
    instr->m_outports[0].st = instr->m_inports[1].st;
    instr->m_outports[1].st = instr->m_inports[1].st;
    instr->m_outports[2].st = instr->m_inports[0].st;
    break;
  case OP_MERGE: {
    /* FIXME: merge the two input types; if you know they're both going to be ints, then the
       result of the merge can be int. */
    SequenceType st = SequenceType::item();
    instr->m_outports[0].st = SequenceType::occurrence(st,OCCURS_ZERO_OR_MORE);
    break;
  }
  case OP_CALL:
    instr->m_outports[0].st = instr->m_cfun->m_rtype;
    break;
  case OP_MAP:
    /* FIXME: we can possibly be more specific here if we know the occurrence range of the
       input type, e.g. OCCURS_ONE_OR_MORE */
    /* FIXME: what if the return type of the function is a sequence type with an occurrence
       indicator, e.g. item()*? The result will be a nested occurrence indicator... is this ok? */
    instr->m_outports[0].st = SequenceType::occurrence(instr->m_cfun->m_rtype,OCCURS_ZERO_OR_MORE);
    break;
  case OP_PASS:
    instr->m_outports[0].st = instr->m_inports[0].st;
    break;
  case OP_SWALLOW:
    break;
  case OP_RETURN:
    return;
  case OP_PRINT:
    break;
  case OP_GATE:
    instr->m_outports[0].st = instr->m_inports[0].st;
    break;
  case OP_BUILTIN:
    break;
  default:
    ASSERT(!"invalid opcode");
    break;
  }

  for (i = 0; i < instr->m_noutports; i++) {
    OutputPort *op = &instr->m_outports[i];
    InputPort *ip = &op->dest->m_inports[op->destp];
    int remaining = 0;

    ASSERT(!op->st.isNull());

    if ((OP_BUILTIN == op->dest->m_opcode) || (OP_CALL == op->dest->m_opcode)) {
      if (ip->st.isNull()) {
        message("null sequence type!\n");
      }
      ASSERT(!ip->st.isNull());
    }
    else {
      if ((OP_MERGE != op->dest->m_opcode) || (ip->st.isNull())) {
        if (!ip->st.isNull()) {
          debug("%* instr %d inport %d [opcode %d] already has a type\n",
                &instr->m_fun->m_ident,op->dest->m_id,op->destp,op->dest->m_opcode);
        }
        ASSERT(ip->st.isNull());
        ip->st = op->st;

        for (unsigned int i = 0; i < op->dest->m_ninports; i++)
          if (op->dest->m_inports[i].st.isNull())
            remaining++;

      }
    }

    if ((0 == remaining) && !op->dest->m_computed)
      Instruction_compute_types(op->dest);
  }
}


Function::Function(unsigned int nparams)
  : m_ret(NULL),
    m_nextid(0),
    m_mapseq(NULL),
    m_program(NULL),
    m_isapply(0),
    m_internal(false)
{
  m_nparams = nparams;
  m_params = new Parameter[m_nparams];
}

Function::~Function()
{
  Iterator<Instruction*> it;
  for (it = m_instructions; it.haveCurrent(); it++)
    delete *it;

  delete [] m_params;
}

void Function::init(sourceloc sloc)
{
  ASSERT(NULL == m_ret);
  ASSERT(m_rtype.isNull());

  m_ret = addInstruction(OP_RETURN,sloc);
//  m_start = addInstruction(OP_PASS,sloc);
//  m_start->m_inports[0].st = SequenceType::item();
//  m_start->m_outports[0].dest = m_ret;
//  m_start->m_outports[0].destp = 0;

  m_rtype = SequenceType::itemstar();
}

Instruction *Function::addInstruction(int opcode, sourceloc sloc)
{
  Instruction *instr = new Instruction();
  unsigned int i;
  m_instructions.append(instr);
  instr->m_id = m_nextid++;
  instr->m_opcode = opcode;
  instr->m_fun = this;
  instr->m_sloc = sourceloc_copy(sloc);

  switch (opcode) {
  case OP_CONST:
    instr->m_ninports = 1;
    instr->m_noutports = 1;
    break;
  case OP_DUP:
    instr->m_ninports = 1;
    instr->m_noutports = 2;
    break;
  case OP_SPLIT:
    instr->m_ninports = 2;
    instr->m_noutports = 3;
    break;
  case OP_MERGE:
    instr->m_ninports = 2;
    instr->m_noutports = 1;
    break;
  case OP_CALL:
    instr->m_ninports = 1;
    instr->m_noutports = 1;
    break;
  case OP_MAP:
    instr->m_ninports = 1;
    instr->m_noutports = 1;
    break;
  case OP_PASS:
    instr->m_ninports = 1;
    instr->m_noutports = 1;
    break;
  case OP_SWALLOW:
    instr->m_ninports = 1;
    instr->m_noutports = 0;
    break;
  case OP_RETURN:
    instr->m_ninports = 1;
    instr->m_noutports = 1;
    break;
  case OP_PRINT:
    instr->m_ninports = 1;
    instr->m_noutports = 0;
    break;
  case OP_GATE:
    instr->m_ninports = 2;
    instr->m_noutports = 1;
    break;
  case OP_BUILTIN:
    instr->m_ninports = 0;
    instr->m_noutports = 1;
    break;
  default:
    ASSERT(!"invalid opcode");
    break;
  }

  if (0 != instr->m_ninports)
    instr->m_inports = new InputPort[instr->m_ninports];
  if (0 != instr->m_noutports)
    instr->m_outports = new OutputPort[instr->m_noutports];

  for (i = 0; i < instr->m_noutports; i++) {
    instr->m_outports[i].owner = instr;
    instr->m_outports[i].portno = i;
  }

  return instr;
}

Instruction *Function::addBuiltinInstruction(const NSName &ident, int nargs, sourceloc sloc)
{
  BuiltinFunction *bif = NULL;
  Instruction *instr;
  int i;
  Iterator<BuiltinFunction*> it;
  for (it = m_program->m_builtin_functions; it.haveCurrent(); it++) {
    BuiltinFunction *b = *it;
    if ((b->m_ident == ident) && (b->m_nargs == nargs)) {
      bif = b;
      break;
    }
  }

  if (NULL == bif)
    return NULL;
  instr = addInstruction(OP_BUILTIN,sloc);

  instr->m_bif = bif;

  instr->freeInports();

  if (bif->m_context) {
    // Function requires context item - this goes to the first input port (0)
    instr->m_ninports = nargs+1;
    instr->m_inports = new InputPort[instr->m_ninports];
    instr->m_inports[0].st = SequenceType(xs_g->context_type);
    for (i = 0; i < nargs; i++)
      instr->m_inports[i+1].st = bif->m_argtypes[i];
  }
  else {
    // Function does not require context item - just have one input port for each parameter

    ASSERT(0 < nargs); // Functions with no arguments must take the context as input

    instr->m_ninports = nargs;
    instr->m_inports = new InputPort[instr->m_ninports];
    for (i = 0; i < nargs; i++)
      instr->m_inports[i].st = bif->m_argtypes[i];
  }

  ASSERT(1 == instr->m_noutports);
  instr->m_outports[0].st = bif->m_rettype;

  return instr;
}

void Function::deleteInstruction(Instruction *instr)
{
  for (Iterator<Instruction*> it = m_instructions; it.haveCurrent(); it++) {
    if (*it == instr) {
      it.remove();
      delete instr;
      return;
    }
  }
  ASSERT(!"instruction not found");
}

void Function::computeTypes()
{
  for (unsigned int i = 0; i < m_nparams; i++)
    Instruction_compute_types(m_params[i].start);
}

Context::Context()
  : position(0),
    size(0),
    havefocus(false),
    tmpl(NULL)
{
}

Context::~Context()
{
}

Environment::Environment()
  : ctxt(NULL),
    ei(NULL),
    space_decls(NULL),
    instr(NULL)
{
  sloc.uri = NULL;
  sloc.line = 0;
}

Environment::~Environment()
{
  delete ctxt;
}

BuiltinFunction::BuiltinFunction()
  : m_fun(NULL), m_nargs(0), m_context(false)
{
}

BuiltinFunction::~BuiltinFunction()
{
}

Program::Program(Schema *schema)
  : m_schema(schema), m_space_decls(NULL)
{
}

Program::~Program()
{
  for (Iterator<Function*> fit(m_functions); fit.haveCurrent(); fit++)
    delete *fit;

  list_free(m_space_decls,(list_d_t)space_decl_free);

  for (Iterator<BuiltinFunction*> bit = m_builtin_functions; bit.haveCurrent(); bit++)
    delete *bit;
}

Function *Program::addFunction(const NSName &ident, unsigned int nparams)
{
  Function *fun = new Function(nparams);
  m_functions.append(fun);
  fun->m_ident = ident;
  fun->m_program = this;

  /* Create sequence instruction (not actually part of the program, but used by the map operator
     to collate results of inner function calls) */


  fun->m_mapseq = fun->addBuiltinInstruction(NSName(SPECIAL_NAMESPACE,"sequence"),2,
                                             nosourceloc);
  fun->m_mapseq->m_internal = 1;

  return fun;
}

Function *Program::getFunction(const NSName &ident)
{
  for (Iterator<Function*> it(m_functions); it.haveCurrent(); it++) {
    if ((*it)->m_ident == ident)
      return *it;
  }
  return NULL;
}

static int has_portlabels(Instruction *instr)
{
  return ((OP_DUP != instr->m_opcode) && (OP_GATE != instr->m_opcode));
}

void Program::getActualOutputPort(Instruction **dest, unsigned int *destp)
{
  if (OP_MAP == (*dest)->m_opcode) {
    if (0 < (*destp)) {
      (*destp)--;
      ASSERT((*destp) < (*dest)->m_cfun->m_nparams);
      (*dest) = (*dest)->m_cfun->m_params[(*destp)].start;
      ASSERT(NULL != (*dest));
    }
  }
}

void Program::outputDotFunction(FILE *f, bool types, bool internal, bool anonfuns, Function *fun,
                                List<Function*> &printed, Instruction *retdest,
                                unsigned int retdestp)
{
    if (!internal && fun->m_internal)
      return;

    if (printed.contains(fun))
      return;

    printed.append(fun);

    unsigned int i;

    fmessage(f,"  subgraph cluster%p {\n",fun);
    fmessage(f,"    color=black;\n");
    fmessage(f,"    label=\"%*\";\n",&fun->m_ident.m_name);

    fmessage(f,"  subgraph clusterz%p {\n",fun);
    fmessage(f,"    label=\"\";\n");
    fmessage(f,"    color=white;\n");

    for (i = 0; i < fun->m_nparams; i++) {
      fmessage(f,"    %*p%d [label=\"[param %d]\",color=white];\n",&fun->m_ident.m_name,i,i);
      if (NULL != fun->m_params[i].start)
        fmessage(f,"    %*p%d -> %*%d;\n",
                &fun->m_ident.m_name,i,&fun->m_ident.m_name,fun->m_params[i].start->m_id);
    }
    if (!anonfuns)
      fmessage(f,"    %*input;\n",&fun->m_ident.m_name);
    fmessage(f,"  }\n");

    fmessage(f,"  subgraph clusterx%p {\n",fun);
    fmessage(f,"    label=\"\";\n");
    fmessage(f,"    color=white;\n");

    Iterator<Instruction*> iit;
    for (iit = fun->m_instructions; iit.haveCurrent(); iit++) {
      Instruction *instr = *iit;
      unsigned int i;

      if (fun->m_mapseq == instr)
        continue;

      if (!anonfuns && (OP_MAP == instr->m_opcode)) {
        outputDotFunction(f,types,internal,anonfuns,instr->m_cfun,printed,
                          instr->m_outports[0].dest,instr->m_outports[0].destp);
      }

      if (OP_DUP == instr->m_opcode) {
        fmessage(f,"    %*%d [label=\"\",style=filled,height=0.2,width=0.2,"
                "fixedsize=true,color=\"#808080\",shape=circle];\n",&
                fun->m_ident.m_name,instr->m_id);
      }
      else if (OP_GATE == instr->m_opcode) {
        fmessage(f,"    %*%d [label=\"\",height=0.4,width=0.4,style=solid,"
                "fixedsize=true,color=\"#808080\",shape=triangle];\n",
                &fun->m_ident.m_name,instr->m_id);
      }
      else {

        const char *extra = "";

        fmessage(f,"    %*%d [label=\"",&fun->m_ident.m_name,instr->m_id);

        if (types) {
          fmessage(f,"{{");

          for (i = 0; i < instr->m_ninports; i++) {
            if (!instr->m_inports[i].st.isNull()) {
              StringBuffer buf;
              instr->m_inports[i].st.printFS(buf,xs_g->namespaces);
              if (0 < i)
                fmessage(f,"|<i%d>%*",i,&buf);
              else
                fmessage(f,"<i%d>%*",i,&buf);
            }
          }

          fmessage(f,"}|{");
        }

        fmessage(f,"%d|",instr->m_id);

        if (OP_CONST == instr->m_opcode) {
          instr->m_cvalue.fprint(f);
          extra = ",fillcolor=\"#FF8080\"";
        }
        else if (OP_CALL == instr->m_opcode) {
          fmessage(f,"call(%*)",&instr->m_cfun->m_ident.m_name);
          extra = ",fillcolor=\"#C0FFC0\"";
        }
        else if (OP_MAP == instr->m_opcode) {
          fmessage(f,"map(%*)",&instr->m_cfun->m_ident.m_name);
          extra = ",fillcolor=\"#FFFFC0\"";
        }
        else {

          if ((OP_BUILTIN == instr->m_opcode) &&
              (instr->m_bif->m_ident == NSName(SPECIAL_NAMESPACE,"select"))) {
            fmessage(f,"select: ");
            if (!instr->m_nametest.isNull()) {
              fmessage(f,"%*",&instr->m_nametest);
            }
            else {
              StringBuffer buf;
              /* FIXME: need to use the namespace map of the syntax tree node */
              instr->m_seqtypetest.printXPath(buf,xs_g->namespaces);
              fmessage(f,"%*",&buf);
            }
/*             fmessage(f,"\\n[%s]",df_axis_types[instr->m_axis]); */
          }
          else if (OP_BUILTIN == instr->m_opcode) {
            fmessage(f,"%*",&instr->m_bif->m_ident.m_name);
          }
          else {
            fmessage(f,"%s",OpCodeNames[instr->m_opcode]);
          }
          extra = ",fillcolor=\"#80E0FF\"";
        }

        if (types) {
          fmessage(f,"}|{");

          for (i = 0; i < instr->m_noutports; i++) {
            if (!instr->m_outports[i].st.isNull()) {
              StringBuffer buf;
              instr->m_outports[i].st.printFS(buf,xs_g->namespaces);
              if (0 < i)
                fmessage(f,"|<o%d>%*",i,&buf);
              else
                fmessage(f,"<o%d>%*",i,&buf);
            }
          }

          fmessage(f,"}}");
        }

        fmessage(f,"\"%s];\n",extra);
      }

      for (i = 0; i < instr->m_noutports; i++) {

        Instruction *dest = instr->m_outports[i].dest;
        unsigned int destp = instr->m_outports[i].destp;

        if ((OP_RETURN == instr->m_opcode) && (NULL != retdest)) {
          dest = retdest;
          destp = retdestp;
        }

        if (!anonfuns && (OP_MAP == instr->m_opcode))
          continue;

        if (NULL != dest) {

          if (types && has_portlabels(instr))
            fmessage(f,"    %*%d:o%d",&fun->m_ident.m_name,instr->m_id,i);
          else
            fmessage(f,"    %*%d",&fun->m_ident.m_name,instr->m_id);

          fmessage(f," -> ");



          if (!anonfuns && (0 == destp) && (OP_MAP == dest->m_opcode)) {
            fmessage(f," %*input [label=\"",&dest->m_cfun->m_ident.m_name);
          }
          else {
            if (!anonfuns)
              getActualOutputPort(&dest,&destp);

            if (types && has_portlabels(dest))
              fmessage(f," %*%d:i%d [label=\"",&dest->m_fun->m_ident.m_name,dest->m_id,destp);
            else
              fmessage(f," %*%d [label=\"",&dest->m_fun->m_ident.m_name,dest->m_id);
          }

/*           if ((OP_SPLIT == instr->m_opcode) && (0 == i)) */
/*             fmessage(f,"F"); */
/*           else if ((OP_SPLIT == instr->m_opcode) && (1 == i)) */
/*             fmessage(f,"T"); */

          fmessage(f,"\"");

          if (OP_DUP == dest->m_opcode)
            fmessage(f,",color=\"#808080\",arrowhead=none");
          else if (OP_DUP == instr->m_opcode)
            fmessage(f,",color=\"#808080\"");
          else if ((2 == i) &&
                   (OP_SPLIT == instr->m_opcode) &&
                   (OP_MERGE == dest->m_opcode))
            fmessage(f,",style=dashed");

          fmessage(f,"];\n");
        }
      }
    }

    fmessage(f,"  }\n");
    fmessage(f,"  }\n");
}

void Program::outputDot(FILE *f, bool types, bool internal, bool anonfuns)
{
  fmessage(f,"digraph {\n");
/*   fmessage(f,"  rankdir=LR;\n"); */
  fmessage(f,"  nodesep=0.5;\n");
  fmessage(f,"  ranksep=0.3;\n");
  fmessage(f,"  node[shape=record,fontsize=12,fontname=\"Arial\",style=filled];\n");

  List<Function*> printed;

  for (Iterator<Function*> fit(m_functions); fit.haveCurrent(); fit++) {
    Function *fun = *fit;
    outputDotFunction(f,types,internal,anonfuns,fun,printed,NULL,0);
  }
  fmessage(f,"}\n");
}

void Program::outputDF(FILE *f, bool internal)
{
  StringBuffer buf;
  for (Iterator<Function*> it(m_functions); it.haveCurrent(); it++) {
    Function *fun = *it;
    unsigned int i;

    if (!internal && fun->m_internal)
      continue;

    fmessage(f,"function %* {\n",&fun->m_ident.m_name);
    ASSERT(!fun->m_rtype.isNull());
    buf.clear();
    fun->m_rtype.printFS(buf,xs_g->namespaces);
    fmessage(f,"  return %*\n",&buf);

    for (i = 0; i < fun->m_nparams; i++) {
      buf.clear();
      fun->m_params[i].st.printFS(buf,xs_g->namespaces);
      fmessage(f,"  param %d start %d type %*\n",i,fun->m_params[i].start->m_id,&buf);
    }

    Iterator<Instruction*> iit;
    for (iit = fun->m_instructions; iit.haveCurrent(); iit++) {
      Instruction *instr = *iit;

      if (fun->m_mapseq == instr)
        continue;

      fmessage(f,"  %-5d %-20s",instr->m_id,OpCodeNames[instr->m_opcode]);
      if (OP_RETURN != instr->m_opcode) {
        for (i = 0; i < instr->m_noutports; i++) {
          if (0 < i)
            fmessage(f," ");
          ASSERT(NULL != instr->m_outports[i].dest);
          fmessage(f,"%d:%d",instr->m_outports[i].dest->m_id,instr->m_outports[i].destp);
        }
      }
      if (OP_BUILTIN == instr->m_opcode) {
        fmessage(f," [%*]",&instr->m_bif->m_ident);
      }

      if (OP_CONST == instr->m_opcode) {
        ASSERT(SEQTYPE_ITEM == instr->m_cvalue.type().type());
        ASSERT(ITEM_ATOMIC == instr->m_cvalue.type().itemType()->m_kind);

        if (instr->m_cvalue.type().itemType()->m_type == xs_g->string_type) {
          String escaped = escape_str(instr->m_cvalue.asString());
          fmessage(f," = \"%*\"",&escaped);
        }
        else if (instr->m_cvalue.type().itemType()->m_type == xs_g->int_type) {
          fmessage(f," = %d",instr->m_cvalue.asInt());
        }
        else {
          ASSERT(!"unknown constant type");
        }
      }

      fmessage(f,"\n");

      for (i = 0; i < instr->m_ninports; i++) {
        if (instr->m_inports[i].st.isNull()) {
          fmessage(f,"    in  %d: none\n",i);
        }
        else {
          StringBuffer buf2;
          instr->m_inports[i].st.printFS(buf2,xs_g->namespaces);
          fmessage(f,"    in  %d: %*\n",i,&buf2);
        }
      }

      for (i = 0; i < instr->m_noutports; i++) {
        if (instr->m_outports[i].st.isNull()) {
          fmessage(f,"    out %d: none\n",i);
        }
        else {
          StringBuffer buf2;
          instr->m_outports[i].st.printFS(buf2,xs_g->namespaces);
          fmessage(f,"    out %d: %*\n",i,&buf2);
        }
      }

    }
    fmessage(f,"}\n");
    if (it.haveNext())
      fmessage(f,"\n");
  }
}
