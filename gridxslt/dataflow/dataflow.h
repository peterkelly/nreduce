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

#ifndef _DATAFLOW_DATAFLOW_H
#define _DATAFLOW_DATAFLOW_H

#include "xmlschema/xmlschema.h"
#include "util/stringbuf.h"
#include "util/list.h"
#include "sequencetype.h"
#include "serialization.h"
#include <stdio.h>

#define OP_CONST              0
#define OP_DUP                1
#define OP_SPLIT              2
#define OP_MERGE              3
#define OP_CALL               4
#define OP_MAP                5
#define OP_PASS               6
#define OP_SWALLOW            7
#define OP_RETURN             8
#define OP_PRINT              9
#define OP_GATE               10
#define OP_BUILTIN            11
#define OP_COUNT              12

typedef struct df_inport df_inport;
typedef struct df_outport df_outport;
typedef struct df_instruction df_instruction;
typedef struct df_parameter df_parameter;
typedef struct df_function df_function;
typedef struct gxcontext gxcontext;
typedef struct gxenvironment gxenvironment;
typedef struct gxfunctiondef gxfunctiondef;
typedef struct df_builtin_function df_builtin_function;
typedef struct df_program df_program;

struct df_inport {
  seqtype *st;
  df_instruction *source;
  int sourcep;
  int from_special;
};

struct df_outport {
  seqtype *st;
  df_instruction *dest;
  int destp;

  df_instruction *owner;
  int portno;
};

struct df_instruction {
  int opcode;
  int id;

  int ninports;
  df_inport *inports;

  int noutports;
  df_outport *outports;

  value *cvalue;
/*   seqtype *ctype; */
  df_function *cfun;
  df_builtin_function *bif;

  struct df_activity *act;
  df_function *fun;
  int internal;

  int computed;

  char *nametest;
  seqtype *seqtypetest;
  int axis;
  df_seroptions *seroptions;
  list *nsdefs;
  char *str;
  sourceloc sloc;
};

struct df_parameter {
  seqtype *st;
  df_instruction *start;
  df_outport *outport;
  df_outport *varsource;
};

struct df_function {
  nsname ident;
  list *instructions;
  int nparams;
  df_parameter *params;
  seqtype *rtype;
  df_instruction *start;
  df_instruction *ret;
  int nextid;
  df_instruction *mapseq;
  df_program *program;
  int isapply;
  char *mode;
};

void df_compute_types(df_function *fun);
int df_check_is_atomic_type(seqtype *st, xs_type *type);
int df_check_derived_atomic_type(value *v, xs_type *type);

void df_init_function(df_program *program, df_function *fun, sourceloc sloc);
df_function *df_new_function(df_program *program, const nsname ident);
df_function *df_lookup_function(df_program *program, const nsname ident);
void df_free_function(df_function *fun);

df_instruction *df_add_instruction(df_program *program, df_function *fun, int opcode,
                                   sourceloc sloc);
df_instruction *df_add_builtin_instruction(df_program *program, df_function *fun,
                                           const nsname ident, int nargs, sourceloc sloc);

void df_delete_instruction(df_program *program, df_function *fun, df_instruction *instr);
void df_free_instruction_inports(df_instruction *instr);
void df_free_instruction(df_instruction *instr);

typedef value* (gxfunction)(gxenvironment *ctxt, value **args);

struct gxcontext {
  value *item;
  int position;
  int size;
  int havefocus;
  df_function *tmpl;
  char *mode;
  char *group;
  char *groupkey;
};

gxcontext *df_create_context(value *item, int position, int size, int havefocus,
                              gxcontext *prev);
void df_free_context(gxcontext *ctxt);

struct gxenvironment {
  gxcontext *ctxt;
  error_info *ei;
  sourceloc sloc;
  list *space_decls;
  df_instruction *instr;
};

struct gxfunctiondef {
  gxfunction *fun;
  char *ns;
  char *name;
  char *arguments;
  char *returns;
};

struct df_builtin_function {
  gxfunction *fun;
  nsname ident;
  int nargs;
  seqtype **argtypes;
  seqtype *rettype;
};

struct df_program {
  list *functions;
  xs_schema *schema;
  list *space_decls;
  list *builtin_functions;
};

const char *df_opstr(int opcode);

void df_module_init(gxfunctiondef *fundefs);

df_program *df_program_new(xs_schema *schema);
void df_program_free(df_program *program);

void df_output_dot(df_program *program, FILE *f, int types);
void df_output_df(df_program *program, FILE *f);

#ifndef _DATAFLOW_DATAFLOW_C
extern const char *df_special_op_names[OP_COUNT];
#endif

#endif /* _DATAFLOW_DATAFLOW_H */
