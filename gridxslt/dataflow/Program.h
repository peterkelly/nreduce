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

#ifndef _DATAFLOW_PROGRAM_H
#define _DATAFLOW_PROGRAM_H

#include "xmlschema/XMLSchema.h"
#include "util/String.h"
#include "util/List.h"
#include "SequenceType.h"
#include "Serialization.h"
#include <stdio.h>

namespace GridXSLT {

class Instruction;
class Function;
class BuiltinFunction;
class Program;
class Environment;
class Activity;
typedef Value (gxfunction)(Environment *env, List<Value> &args);

enum OpCode {
  OP_CONST      = 0,
  OP_DUP        = 1,
  OP_SPLIT      = 2,
  OP_MERGE      = 3,
  OP_CALL       = 4,
  OP_MAP        = 5,
  OP_PASS       = 6,
  OP_SWALLOW    = 7,
  OP_RETURN     = 8,
  OP_PRINT      = 9,
  OP_GATE       = 10,
  OP_BUILTIN    = 11,
  OP_COUNT      = 12
};

#ifndef _DATAFLOW_PROGRAM_CPP
extern const char *OpCodeNames[GridXSLT::OP_COUNT];
#endif

class InputPort {
public:
  InputPort() : source(0), sourcep(0), from_special(false) { }
  ~InputPort() { }

  SequenceType st;
  Instruction *source;
  unsigned int sourcep;
  bool from_special;
};

class OutputPort {
public:
  OutputPort() : dest(0), destp(0), owner(0), portno(0) { }
  ~OutputPort() { }

  SequenceType st;
  Instruction *dest;
  unsigned int destp;

  Instruction *owner;
  unsigned int portno;
};

class Instruction {
  DISABLE_COPY(Instruction)
public:
  Instruction();
  ~Instruction();

  void freeInports();

  int m_opcode;
  int m_id;

  unsigned int m_ninports;
  InputPort *m_inports;

  unsigned int m_noutports;
  OutputPort *m_outports;

  Value m_cvalue;
  Function *m_cfun;
  BuiltinFunction *m_bif;

  Activity *m_act;
  Function *m_fun;
  int m_internal;

  int m_computed;

  String m_nametest;
  SequenceType m_seqtypetest;
  int m_axis;
  df_seroptions *m_seroptions;
  list *m_nsdefs;
  String m_str;
  sourceloc m_sloc;
};

class Parameter {
public:
  Parameter() : start(0), outport(0), varsource(0) { }
  ~Parameter() { }

  SequenceType st;
  Instruction *start;
  OutputPort *outport;
  OutputPort *varsource;
};

class Function {
  DISABLE_COPY(Function)
public:
  Function(unsigned int nparams);
  ~Function();

  void init(sourceloc sloc);

  Instruction *addInstruction(int opcode, sourceloc sloc);
  Instruction *addBuiltinInstruction(const NSName &ident, int nargs, sourceloc sloc);
  void deleteInstruction(Instruction *instr);
  void computeTypes();

  NSName m_ident;
  List<Instruction*> m_instructions;
  unsigned int m_nparams;
  Parameter *m_params;
  SequenceType m_rtype;
/*   Instruction *m_start; */
  Instruction *m_ret;
  int m_nextid;
  Instruction *m_mapseq;
  Program *m_program;
  int m_isapply;
  String m_mode;
  bool m_internal;
};

class Context {
public:
  Context();
  ~Context();

  Value item;
  int position;
  int size;
  bool havefocus;
  Function *tmpl;
  String mode;
  String group;
  String groupkey;
};

class Environment {
public:
  Environment();
  ~Environment();

  Context *ctxt;
  Error *ei;
  sourceloc sloc;
  list *space_decls;
  Instruction *instr;
};

struct FunctionDefinition {
  gxfunction *fun;
  String ns;
  String name;
  char *arguments;
  char *returns;
  bool context;
};

class BuiltinFunction {
  DISABLE_COPY(BuiltinFunction)
public:
  BuiltinFunction();
  ~BuiltinFunction();

  gxfunction *m_fun;
  NSName m_ident;
  int m_nargs;
  bool m_context;
  List<SequenceType> m_argtypes;
  SequenceType m_rettype;
};

class Program {
  DISABLE_COPY(Program)
public:
  Program(Schema *schema);
  ~Program();

  Function *addFunction(const NSName &ident, unsigned int nparams);
  Function *getFunction(const NSName &ident);
  void outputDot(FILE *f, bool types, bool internal, bool anonfuns);
  void outputDF(FILE *f, bool internal);

  List<Function*> m_functions;
  Schema *m_schema;
  list *m_space_decls;
  List<BuiltinFunction*> m_builtin_functions;

private:
  void getActualOutputPort(Instruction **dest, unsigned int *destp);
  void outputDotFunction(FILE *f, bool types, bool internal, bool anonfuns, Function *fun,
                         List<Function*> &printed, Instruction *retdest, unsigned int retdestp);
};

};

#endif // _DATAFLOW_PROGRAM_H
