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
  int usage;
} ginstr;

typedef struct gprogram {
  ginstr *ginstrs;
  int alloc;
  int count;
  stackinfo *si;
  array *stringmap;
} gprogram;

gprogram *gprogram_new();
void gprogram_free(gprogram *gp);

char *get_function_name(int fno);
int function_nargs(int fno);
void print_ginstr(gprogram *gp, int address, ginstr *instr, int usage);
void print_program(gprogram *gp, int builtins, int usage);
void print_profiling(gprogram *gp);
void compile(gprogram *gp);

/* gmachine */

void execute(gprogram *gp);

/* jit */

void jit_compile(ginstr *program, array *cpucode);
void jit_execute(ginstr *program);

#ifndef GCODE_C
extern int op_usage[OP_COUNT];
extern int *funcalls;
extern int *addressmap;
extern int *noevaladdressmap;
extern int *stacksizes;
extern int nfunctions;
extern const char *op_names[OP_COUNT];
#endif

#endif
