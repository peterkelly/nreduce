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
#define OP_GLOBSTART     2
#define OP_EVAL          3
#define OP_UNWIND        4
#define OP_RETURN        5
#define OP_PUSH          6
#define OP_PUSHGLOBAL    7
#define OP_MKAP          8
#define OP_UPDATE        9
#define OP_REPLACE       10
#define OP_POP           11
#define OP_LAST          12
#define OP_PRINT         13
#define OP_BIF           14
#define OP_JFALSE        15
#define OP_JUMP          16
#define OP_JEMPTY        17
#define OP_ISTYPE        18
#define OP_ALLOC         19
#define OP_SQUEEZE       20
#define OP_DISPATCH      21
#define OP_PUSHNIL       22
#define OP_PUSHINT       23
#define OP_PUSHDOUBLE    24
#define OP_PUSHSTRING    25
#define OP_COUNT         26

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
  int codeaddr;
/*   int codesize; */
/*   int jmppos; */
  int label;
  int stmtno;
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

#define DE_STACKBASE  ((int)&((dumpentry*)0)->stackbase)
#define DE_STACKCOUNT ((int)&((dumpentry*)0)->stackcount)
#define DE_ADDRESS    ((int)&((dumpentry*)0)->address)
#define DE_NEXT       ((int)&((dumpentry*)0)->next)

/* FIXME: For uniformity, we should really store this stuff on the main stack like x86.
   This would either require the allocation of cells to hold the integer values, or storing
   the values directly on the stack (i.e. not as cell pointers).

   The latter would require support for a stack containing different types of values, which is
   actually required anyway for some of the operations described in the latter optimisation
   sections of IFPL which operate directly on numerical values. */

typedef struct dumpentry {
  int stackbase;
  int stackcount;
  int address;
  int sb;
  struct dumpentry *next;
} dumpentry;

void print_ginstr(int address, ginstr *instr, int usage);
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
extern int *addressmap;
extern int *noevaladdressmap;
extern int nfunctions;
extern const char *op_names[OP_COUNT];
#endif

#endif
