/*
 * This file is part of the GridXSLT project
 * Copyright (C) 2006 Peter Kelly (pmk@cs.adelaide.edu.au)
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

#ifndef _GCODE_H
#define _GCODE_H

#include "nreduce.h"

#define OP_BEGIN         0
#define OP_END           1
#define OP_LAST          2
#define OP_GLOBSTART     3
#define OP_EVAL          4
#define OP_RETURN        5
#define OP_DO            6
#define OP_JFUN          7
#define OP_JFALSE        8
#define OP_JUMP          9
#define OP_PUSH          10
#define OP_UPDATE        11
#define OP_ALLOC         12
#define OP_SQUEEZE       13
#define OP_MKCAP         14
#define OP_MKFRAME       15
#define OP_BIF           16
#define OP_PUSHNIL       17
#define OP_PUSHNUMBER    18
#define OP_PUSHSTRING    19
#define OP_RESOLVE       20
#define OP_COUNT         21

#define GINSTR_OPCODE   ((int)&((ginstr*)0)->opcode)
#define GINSTR_ARG0     ((int)&((ginstr*)0)->arg0)
#define GINSTR_ARG1     ((int)&((ginstr*)0)->arg1)
#define GINSTR_CODEADDR ((int)&((ginstr*)0)->codeaddr)

#define CONSTANT_APP_MSG "constant cannot be applied to arguments"

typedef struct stackinfo {
  int alloc;
  int count;
  int *status;
  int invalid;
} stackinfo;

typedef struct ginstr {
  int opcode;
  int arg0;
  int arg1;
  sourceloc sl;
  int codeaddr;
  int expcount;
  int *expstatus;
} ginstr;

typedef struct funinfo {
  int address;
  int addressne;
  int arity;
  int stacksize;
  int name;
} funinfo;

typedef struct gprogram {
  ginstr *ginstrs;
  int alloc;
  int count;
  stackinfo *si;
  array *stringmap;
  funinfo *finfo;
  int nfunctions;
  char **fnames;
  int evaldoaddr;
  int cdepth;
} gprogram;

typedef struct bcheader {
  int nops;
  int nfunctions;
  int nstrings;
  int evaldoaddr;
} bcheader;

typedef struct gop {
  int opcode;
  int arg0;
  int arg1;
  int fileno;
  int lineno;
} gop;

gprogram *gprogram_new(void);
void gprogram_free(gprogram *gp);

char *get_function_name(int fno);
const char *function_name(gprogram *gp, int fno);
int function_nargs(int fno);
void print_ginstr(FILE *f, gprogram *gp, int address, ginstr *instr);
void print_program(gprogram *gp, int builtins);
void print_profiling(process *proc, gprogram *gp);
void compile(gprogram *gp);
void print_bytecode(FILE *f, char *bcdata, int bcsize);
void gen_bytecode(gprogram *gp, char **bcdata, int *bcsize);

const gop *bc_get_ops(const char *bcdata);
const funinfo *bc_get_funinfo(const char *bcdata);
const int *bc_get_stroffsets(const char *bcdata);
const char *bc_function_name(const char *bcdata, int fno);

/* gmachine */

void check_stack(process *proc, frame *curf, pntr *stackdata, int stackcount, gprogram *gp);
void add_pending_mark(process *proc, gaddr addr);
void spark(process *proc, frame *f);
void run(gprogram *gp, FILE *statsfile);

/* jit */

void jit_compile(ginstr *program, array *cpucode);
void jit_execute(ginstr *program);

#ifndef GCODE_C
extern const char *op_names[OP_COUNT];
#endif

#endif
