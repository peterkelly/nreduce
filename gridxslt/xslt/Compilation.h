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

#ifndef _XSLT_COMPILE_H
#define _XSLT_COMPILE_H

#include "Statement.h"
#include "dataflow/Program.h"
#include "util/XMLUtils.h"

namespace GridXSLT {

class Compilation;
class Template;

class var_reference {
public:
  var_reference() : ref(NULL), top_outport(NULL), local_outport(NULL) { }
  Expression *ref;
  OutputPort *top_outport;
  OutputPort *local_outport;
};


class Compilation {
  DISABLE_COPY(Compilation)
public:
  Compilation();
  ~Compilation();

  void set_and_move_cursor(OutputPort **cursor, Instruction *destinstr, int destp,
                                Instruction *newpos, int newdestno);

  Instruction *specialop(Function *fun, const String &ns,
                              const String &name, int nargs, sourceloc sloc);
  void useVar(Function *fun, OutputPort *outport,
                       OutputPort **cursor, int need_gate, sourceloc sloc);
  OutputPort *resolveVar(Function *fun, VarRefExpr *varref);
  void compileString(Function *fun, const String &str,
                                OutputPort **cursor, sourceloc sloc);
  int haveVarReference(List<var_reference*> vars, OutputPort *top_outport);
  void initParamInstructions(Function *fun,
                                    List<SequenceType> &seqtypes, OutputPort **outports);
  int compileInnerFunction(Function *fun, SyntaxNode *sn,
                                 Instruction **mapout, OutputPort **cursor, sourceloc sloc);
  void compileConditional(Function *fun, OutputPort **cursor,
                                    OutputPort **condcur, OutputPort **truecur,
                                    OutputPort **falsecur, sourceloc sloc);
  int compileSequence(Function *fun, Statement *parent,
                             OutputPort **cursor);
  int compileFunctionContents(Function *fun, SyntaxNode *sn, OutputPort **cursor);
  int compileFunction(Statement *sn, Function *fun);
  int compileApplyFunction(Function **ifout, const char *mode);
  int compileApplyTemplates(Function *fun, OutputPort **cursor, const char *mode, sourceloc sloc,
                            Instruction **map);
  void compileOrderedTemplateList();
  int compileDefaultTemplate();
  int compile2();






  Error *m_ei;
  xslt_source *m_source;
  Program *m_program;

  Schema *m_schema;

  ManagedPtrList<Template*> m_templates;
  int m_nextanonid;
};

class Template {
public:
  Template();
  ~Template() ;

  Expression *pattern;
  Function *fun;
  bool builtin;
};

};

int xslt_compile(GridXSLT::Error *ei, GridXSLT::xslt_source *source, GridXSLT::Program **program);

#endif /* _XSLT_COMPILE_H */
