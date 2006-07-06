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

#ifndef _ASSEMBLER_H
#define _ASSEMBLER_H

#include "nreduce.h"

#define EAX 0
#define ECX 1
#define EDX 2
#define EBX 3
#define ESP 4
#define EBP 5
#define ESI 6
#define EDI 7

#define SCALE_1     0
#define SCALE_2     1
#define SCALE_4     2
#define SCALE_8     3

#define X86_PUSH    0
#define X86_POP     1
#define X86_MOV     2
#define X86_CMP     3
#define X86_JMP     4
#define X86_JZ      5
#define X86_JNE     6
#define X86_JG      7
#define X86_JGE     8
#define X86_JL      9
#define X86_JLE     10
#define X86_JNB     11
#define X86_JNBE    12
#define X86_JNA     13
#define X86_JNAE    14
#define X86_RET     15
#define X86_LEAVE   16
#define X86_CALL    17
#define X86_ADD     18
#define X86_SUB     19
#define X86_AND     20
#define X86_OR      21
#define X86_XOR     22
#define X86_NOT     23
#define X86_NOP     24
#define X86_PUSHAD  25
#define X86_POPAD   26
#define X86_IMUL    27
#define X86_NONE    28
#define NUM_X86     29

#define I0(op)            x86_addinstr(as,(op),none(),none())
#define I1(op,dst)        x86_addinstr(as,(op),(dst),none())
#define I2(op,dst,src)    x86_addinstr(as,(op),(dst),(src))
#define LABEL(lbl)        { x86_setnextlabel(as,lbl); I0(X86_NONE); }

#define I_PUSH(dst)       I1(X86_PUSH,dst)
#define I_POP(dst)        I1(X86_POP,dst)
#define I_MOV(dst,src)    I2(X86_MOV,dst,src)
#define I_CMP(dst,src)    I2(X86_CMP,dst,src)
#define I_JMP(dst)        I1(X86_JMP,dst)
#define I_JZ(dst)         I1(X86_JZ,dst)
#define I_JNE(dst)        I1(X86_JNE,dst)
#define I_JG(dst)         I1(X86_JG,dst)
#define I_JGE(dst)        I1(X86_JGE,dst)
#define I_JL(dst)         I1(X86_JL,dst)
#define I_JLE(dst)        I1(X86_JLE,dst)
#define I_JNB(dst)        I1(X86_JNB,dst)
#define I_JNBE(dst)       I1(X86_JNBE,dst)
#define I_JNA(dst)        I1(X86_JNA,dst)
#define I_JNAE(dst)       I1(X86_JNAE,dst)
#define I_RET()           I0(X86_RET)
#define I_LEAVE()         I0(X86_LEAVE)
#define I_CALL(dst)       I1(X86_CALL,dst)
#define I_ADD(dst,src)    I2(X86_ADD,dst,src)
#define I_SUB(dst,src)    I2(X86_SUB,dst,src)
#define I_AND(dst,src)    I2(X86_AND,dst,src)
#define I_OR(dst,src)     I2(X86_OR,dst,src)
#define I_XOR(dst,src)    I2(X86_XOR,dst,src)
#define I_NOT(dst)        I1(X86_NOT,dst)
#define I_NOP()           I0(X86_NOP)
#define I_PUSHAD()        I0(X86_PUSHAD)
#define I_POPAD()         I0(X86_POPAD)
#define I_IMUL(dst,src)   I2(X86_IMUL,dst,src)

typedef struct x86_arg {
  int type;
  int val;
  int off;
  int scale;
  int index;
} x86_arg;

typedef struct x86_instr {
  int op;
  x86_arg dst;
  x86_arg src;
  int label;
  int addr;
  int isgetnextaddr;
} x86_instr;

typedef struct x86_assembly {
  int labels;
  x86_instr *instructions;
  int alloc;
  int count;
  int nextlabel;
} x86_assembly;

x86_assembly *x86_assembly_new();
x86_arg imm(int val1);
x86_arg reg(int reg1);
x86_arg regmem(int reg1, int off1);
x86_arg regmemscaled(int reg1, int off1, int scale1, int index1);
x86_arg absmem(int addr1);
x86_arg label(int l);
x86_arg none();
void x86_setnextlabel(x86_assembly *as, int label);
void x86_addinstr(x86_assembly *as, int op, x86_arg dst, x86_arg src);
void x86_assembly_free(x86_assembly *as);
void x86_assemble(x86_assembly *as, array *cpucode);

void testasm(array *cpucode);

#endif
