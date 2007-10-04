/*
 * This file is part of the nreduce project
 * Copyright (C) 2006-2007 Peter Kelly (pmk@cs.adelaide.edu.au)
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

#ifndef _BYTECODE_H
#define _BYTECODE_H

#include "src/nreduce.h"
#include "source.h"

#define OP_BEGIN         0
#define OP_END           1
#define OP_GLOBSTART     2
#define OP_SPARK         3
#define OP_RETURN        4
#define OP_DO            5
#define OP_JFUN          6
#define OP_JFALSE        7
#define OP_JUMP          8
#define OP_PUSH          9
#define OP_UPDATE        10
#define OP_ALLOC         11
#define OP_SQUEEZE       12
#define OP_MKCAP         13
#define OP_MKFRAME       14
#define OP_BIF           15
#define OP_PUSHNIL       16
#define OP_PUSHNUMBER    17
#define OP_PUSHSTRING    18
#define OP_POP           19
#define OP_ERROR         20
#define OP_EVAL          21
#define OP_CALL          22
#define OP_JCMP          23
#define OP_CONSN         24
#define OP_ITEMN         25
#define OP_JEQ           26
#define OP_INVALID       27
#define OP_COUNT         28

#define CONSTANT_APP_MSG "constant cannot be applied to arguments"
#define EVALDO_SEQUENCE_SIZE 2

typedef struct instruction {
  int opcode;
  int arg0;
  int arg1;
  int fileno;
  int lineno;
  int expcount;
} instruction;

typedef struct funinfo {
  int address;
  int addressne;
  int addressed;
  int arity;
  int stacksize;
  int name;
} funinfo;

typedef struct bcheader {
  char signature[16];
  int nops;
  int nfunctions;
  int nstrings;
  int evaldoaddr;
  int erroraddr;
  int itemfno;
} bcheader;

void compile(source *src, char **bcdata, int *bcsize);

void bc_print(const char *bcdata, FILE *f, source *src, int builtins, int *usage);
const instruction *bc_instructions(const char *bcdata);
const funinfo *bc_funinfo(const char *bcdata);
const char *bc_string(const char *bcdata, int sno);
const char *bc_function_name(const char *bcdata, int fno);

#ifndef BYTECODE_C
extern const char *opcodes[OP_COUNT];
#endif

#endif
