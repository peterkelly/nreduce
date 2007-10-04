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
 * $Id: assembler.c 340 2006-10-01 07:05:53Z pmkelly $
 *
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "src/nreduce.h"
#include "assembler.h"
#include "compiler/source.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <assert.h>
#include <ctype.h>
#include <stdarg.h>
#include <math.h>

#define MABS    0
#define MDISP8  1
#define MDISP32 2
#define MREG    3

#define RM_SIB     4
#define RM_DISP32  5
#define NOINDEX    4

static int modrm(int mod, int reg, int rm)
{
  return ((mod << 6) | (reg << 3) | rm);
}

static int sib(int scale, int index, int base)
{
  return ((scale << 6) | (index << 3) | base);
}

#define byte(val) wbyte(&ptr,val)
static void wbyte(char **ptr, int val)
{
  *((*ptr)++) = (char)(val);
}

#define dword(val) wdword(&ptr,val)
static void wdword(char **ptr, int val)
{
  wbyte(ptr,(val & 0x000000FF) >> 0);
  wbyte(ptr,(val & 0x0000FF00) >> 8);
  wbyte(ptr,(val & 0x00FF0000) >> 16);
  wbyte(ptr,(val & 0xFF000000) >> 24);
}

static void rmem(char **ptr, int reg, x86_arg oper)
{
  int base = oper.val;
  int disp = oper.off;
  int index = oper.index;
  int scale = oper.scale;
  int rm = base;
  int usesib = ((ESP == base) || (NOINDEX != oper.index));
  if (usesib)
    rm = RM_SIB;

  if ((disp == 0) && (EBP != base)) {
    wbyte(ptr,modrm(MABS,reg,rm));
    if (usesib)
      wbyte(ptr,sib(scale,index,base));
  }
  else if ((disp <= 127) && (disp >= -128)) {
    wbyte(ptr,modrm(MDISP8,reg,rm));
    if (usesib)
      wbyte(ptr,sib(scale,index,base));
    wbyte(ptr,disp);
  }
  else {
    wbyte(ptr,modrm(MDISP32,reg,rm));
    if (usesib)
      wbyte(ptr,sib(scale,index,base));
    wdword(ptr,disp);
  }
}

static void mem(char **ptr, int reg, int addr)
{
  wbyte(ptr,modrm(0,reg,RM_DISP32));
  wdword(ptr,addr);
}


static void add_reg_imm(char **ptr, int reg, int imm)
{
  if (EAX == reg) {
    wbyte(ptr,0x05);
    wdword(ptr,imm);
  }
  else {
    wbyte(ptr,0x81);
    wbyte(ptr,modrm(MREG,0,reg));
    wdword(ptr,imm);
  }
}

#define TYPE_NONE   0
#define TYPE_IMM    1
#define TYPE_REG    2
#define TYPE_REGMEM 3
#define TYPE_MEM    4
#define TYPE_LABEL  5

x86_assembly *x86_assembly_new(void)
{
  x86_assembly *as = (x86_assembly*)calloc(1,sizeof(x86_assembly));
  as->labels = 1;
  as->alloc = 2;
  as->count = 0;
  as->instructions = (x86_instr*)malloc(as->alloc*sizeof(x86_instr));
  as->nextlabel = -1;
  return as;
}

x86_arg imm(int val1)
{
  x86_arg arg;
  arg.type = TYPE_IMM;
  arg.val = val1;
  arg.off = 0;
  arg.scale = 0;
  arg.index = NOINDEX;
  return arg;
}

x86_arg reg(int reg1)
{
  x86_arg arg;
  arg.type = TYPE_REG;
  arg.val = reg1;
  arg.off = 0;
  arg.scale = 0;
  arg.index = NOINDEX;
  return arg;
}

x86_arg regmem(int reg1, int off1)
{
  x86_arg arg;
  arg.type = TYPE_REGMEM;
  arg.val = reg1;
  arg.off = off1;
  arg.scale = 0;
  arg.index = NOINDEX;
  return arg;
}

x86_arg regmemscaled(int reg1, int off1, int scale1, int index1)
{
  x86_arg arg;
  arg.type = TYPE_REGMEM;
  arg.val = reg1;
  arg.off = off1;
  arg.scale = scale1;
  arg.index = index1;
  assert(NOINDEX != index1);
  return arg;
}

x86_arg absmem(int addr1)
{
  x86_arg arg;
  arg.type = TYPE_MEM;
  arg.val = addr1;
  arg.off = 0;
  arg.scale = 0;
  arg.index = NOINDEX;
  return arg;
}

x86_arg label(int l)
{
  x86_arg arg;
  arg.type = TYPE_LABEL;
  arg.val = l;
  arg.off = 0;
  arg.scale = 0;
  arg.index = NOINDEX;
  return arg;
}

x86_arg none(void)
{
  x86_arg arg;
  arg.type = TYPE_NONE;
  arg.val = 0;
  arg.off = 0;
  arg.scale = 0;
  arg.index = NOINDEX;
  return arg;
}


void x86_setnextlabel(x86_assembly *as, int label)
{
  assert(-1 == as->nextlabel);
  as->nextlabel = label;
}

void x86_addinstr(x86_assembly *as, int op, x86_arg dst, x86_arg src)
{
  if (as->alloc <= as->count) {
    as->alloc *= 2;
    as->instructions = (x86_instr*)realloc(as->instructions,as->alloc*sizeof(x86_instr));
  }
  memset(&as->instructions[as->count],0,sizeof(x86_instr));
  as->instructions[as->count].op = op;
  as->instructions[as->count].dst = dst;
  as->instructions[as->count].src = src;
  as->instructions[as->count].label = as->nextlabel;
  as->nextlabel = -1;
  as->count++;
}

void x86_assembly_free(x86_assembly *as)
{
  free(as->instructions);
  free(as->labeladdrs);
  free(as);
}

void x86_assemble(x86_assembly *as, array *cpucode)
{
  char *ptr;
  char *instart;
  int pos;
  int maxinstrsize = 1024;
  int *labeladdrs = as->labeladdrs = (int*)calloc(as->labels,sizeof(int));

  /* First pass: Compile all instructions, and record label references as 0 */
  for (pos = 0; pos < as->count; pos++) {
    x86_instr *instr = &as->instructions[pos];
    x86_arg dst = instr->dst;
    int dtype = dst.type;
    int dval = dst.val;
/*     int doff = dst.off; */
    x86_arg src = instr->src;
    int stype = src.type;
    int sval = src.val;
/*     int soff = src.off; */

    array_mkroom(cpucode,maxinstrsize);
    ptr = ((char*)cpucode->data)+cpucode->nbytes;
    instart = ptr;

    if (0 <= instr->label) {
      assert(instr->label < as->labels);
      labeladdrs[instr->label] = ptr-(char*)cpucode->data;
    }
    instr->addr = ptr-(char*)cpucode->data;

    switch (instr->op) {
    case X86_PUSH:
      if (TYPE_IMM == dtype) {
        byte(0x68);
        dword(dval);
      }
      else if (TYPE_REG == dtype) {
        byte(0x50+(dval));
      }
      else if (TYPE_REGMEM == dtype) {
        byte(0xFF);
        rmem(&ptr,6,dst);
      }
      else if (TYPE_MEM == dtype) {
        byte(0xFF);
        mem(&ptr,6,dval);
      }
      else {
        fatal("PUSH: invalid destination");
      }
      break;
    case X86_POP:
      if (TYPE_REG == dtype) {
        byte(0x58+(dval));
      }
      else if (TYPE_REGMEM == dtype) {
        byte(0x8F);
        rmem(&ptr,0,dst);
      }
      else if (TYPE_MEM == dtype) {
        byte(0x8F);
        mem(&ptr,0,dval);
      }
      else {
        fatal("POP: invalid destination");
      }
      break;
    case X86_MOV:
      if ((TYPE_REG == dtype) && (TYPE_REG == stype)) {
        byte(0x89);
        byte(modrm(MREG,sval,dval));
      }
      else if ((TYPE_MEM == dtype) && (TYPE_REG == stype)) {
        if (EAX == sval) {
          byte(0xA3);
          dword(dval);
        }
        else {
          byte(0x89);
          byte(modrm(MABS,sval,RM_DISP32));
          dword(dval);
        }
      }
      else if ((TYPE_REGMEM == dtype) && (TYPE_REG == stype)) {
        byte(0x89);
        rmem(&ptr,sval,dst);
      }
      else if ((TYPE_REGMEM == dtype) && (TYPE_IMM == stype)) {
        byte(0xC7);
        rmem(&ptr,0,dst);
        dword(sval);
      }
      else if ((TYPE_REG == dtype) && (TYPE_MEM == stype)) {
        if (EAX == dval) {
          byte(0xA1);
          dword(sval);
        }
        else {
          byte(0x8B);
          mem(&ptr,dval,sval);
        }
      }
      else if ((TYPE_REG == dtype) && (TYPE_REGMEM == stype)) {
        byte(0x8B);
        rmem(&ptr,dval,src);
      }
      else if ((TYPE_REG == dtype) && (TYPE_IMM == stype)) {
        byte(0xB8+(dval));
        dword(sval);
      }
      else if ((TYPE_REG == dtype) && (TYPE_LABEL == stype)) {
        byte(0xB8+(dval));
        dword(0);
      }
      else if ((TYPE_MEM == dtype) && (TYPE_IMM == stype)) {
        byte(0xC7);
        mem(&ptr,0,dval);
        dword(sval);
      }
      else {
        fatal("MOV: invalid source/destination");
      }
      break;
    case X86_CMP:
      if ((TYPE_REG == dtype) && (TYPE_REG == stype)) {
        byte(0x39);
        byte(modrm(MREG,sval,dval));
      }
      else if ((TYPE_REG == dtype) && (TYPE_IMM == stype)) {
        if (EAX == dval) {
          byte(0x3D);
          dword(sval);
        }
        else {
          byte(0x81);
          byte(modrm(MREG,7,dval));
          dword(sval);
        }
      }
      else if ((TYPE_REGMEM == dtype) && (TYPE_IMM == stype)) {
        byte(0x81);
        rmem(&ptr,7,dst);
        dword(sval);
      }
      else if ((TYPE_MEM == dtype) && (TYPE_IMM == stype)) {
        byte(0x81);
        byte(modrm(MABS,7,RM_DISP32));
        dword(dval);
        dword(sval);
      }
      else if ((TYPE_REG == dtype) && (TYPE_REGMEM == stype)) {
        byte(0x3B);
        rmem(&ptr,dval,src);
      }
      else if ((TYPE_REG == dtype) && (TYPE_MEM == stype)) {
        byte(0x3B);
        byte(modrm(MABS,dval,RM_DISP32));
        dword(sval);
      }
      else {
        fatal("CMP: invalid source/destination");
      }
      break;
    case X86_JMP:
      if (TYPE_LABEL == dtype) {
        byte(0xE9);
        dword(0);
      }
      else if (TYPE_REG == dtype) {
        byte(0xFF);
        byte(modrm(MREG,4,dval));
      }
      else if (TYPE_REGMEM == dtype) {
        byte(0xFF);
        rmem(&ptr,4,dst);
      }
      else if (TYPE_MEM == dtype) {
        byte(0xFF);
        mem(&ptr,4,dval);
      }
      else {
        fatal("JMP: invalid destination");
      }
      break;
    case X86_JZ:
      assert(TYPE_LABEL == dtype);
      ptr += 6;
      break;
    case X86_JNE:
      assert(TYPE_LABEL == dtype);
      ptr += 6;
      break;
    case X86_JG:
      assert(TYPE_LABEL == dtype);
      ptr += 6;
      break;
    case X86_JGE:
      assert(TYPE_LABEL == dtype);
      ptr += 6;
      break;
    case X86_JL:
      assert(TYPE_LABEL == dtype);
      ptr += 6;
      break;
    case X86_JLE:
      assert(TYPE_LABEL == dtype);
      ptr += 6;
      break;
    case X86_JNB:
      assert(TYPE_LABEL == dtype);
      ptr += 6;
      break;
    case X86_JNBE:
      assert(TYPE_LABEL == dtype);
      ptr += 6;
      break;
    case X86_JNA:
      assert(TYPE_LABEL == dtype);
      ptr += 6;
      break;
    case X86_JNAE:
      assert(TYPE_LABEL == dtype);
      ptr += 6;
      break;
    case X86_RET:
      byte(0xC3);
      break;
    case X86_LEAVE:
      byte(0xC9);
      break;
    case X86_CALL:
      if (TYPE_LABEL == dtype) {
        byte(0xE8);
        dword(0);
      }
      else if (TYPE_REG == dtype) {
        byte(0xFF);
        byte(modrm(MREG,2,dval));
      }
      else if (TYPE_REGMEM == dtype) {
        byte(0xFF);
        rmem(&ptr,2,dst);
      }
      else if (TYPE_MEM == dtype) {
        byte(0xFF);
        mem(&ptr,2,dval);
      }
      else {
        fatal("CALL: invalid destination");
      }
      break;
    case X86_ADD:
      if ((TYPE_REG == dtype) && (TYPE_IMM == stype)) {
        add_reg_imm(&ptr,dval,sval);
      }
      else if ((TYPE_REG == dtype) && (TYPE_REG == stype)) {
        byte(0x01);
        byte(modrm(MREG,sval,dval));
      }
      else if ((TYPE_REG == dtype) && (TYPE_REGMEM == stype)) {
        byte(0x03);
        rmem(&ptr,dval,src);
      }
      else if ((TYPE_REG == dtype) && (TYPE_MEM == stype)) {
        byte(0x03);
        mem(&ptr,dval,sval);
      }
      else if ((TYPE_REGMEM == dtype) && (TYPE_IMM == stype)) {
        byte(0x81);
        rmem(&ptr,0,dst);
        dword(sval);
      }
      else if ((TYPE_MEM == dtype) && (TYPE_IMM == stype)) {
        byte(0x81);
        mem(&ptr,0,dval);
        dword(sval);
      }
      else {
        fatal("ADD: invalid source/destination");
      }
      break;
    case X86_SUB:
      if ((TYPE_REG == dtype) && (TYPE_IMM == stype)) {
        if (EAX == dval) {
          byte(0x2D);
          dword(sval);
        }
        else {
          byte(0x81);
          byte(modrm(MREG,5,dval));
          dword(sval);
        }
      }
      else if ((TYPE_REG == dtype) && (TYPE_REG == stype)) {
        byte(0x29);
        byte(modrm(MREG,sval,dval));
      }
      else if ((TYPE_REG == dtype) && (TYPE_REGMEM == stype)) {
        byte(0x2B);
        rmem(&ptr,dval,src);
      }
      else if ((TYPE_REG == dtype) && (TYPE_MEM == stype)) {
        byte(0x2B);
        mem(&ptr,dval,sval);
      }
      else if ((TYPE_REGMEM == dtype) && (TYPE_IMM == stype)) {
        byte(0x81);
        rmem(&ptr,5,dst);
        dword(sval);
      }
      else if ((TYPE_MEM == dtype) && (TYPE_IMM == stype)) {
        byte(0x81);
        mem(&ptr,5,dval);
        dword(sval);
      }
      else {
        fatal("SUB: invalid source/destination");
      }
      break;
    case X86_AND:
      if ((TYPE_REG == dtype) && (TYPE_IMM == stype)) {
        if (EAX == dval) {
          byte(0x25);
          dword(sval);
        }
        else {
          byte(0x81);
          byte(modrm(MREG,4,dval));
          dword(sval);
        }
      }
      else if ((TYPE_REG == dtype) && (TYPE_REG == stype)) {
        byte(0x21);
        byte(modrm(MREG,sval,dval));
      }
      else if ((TYPE_REG == dtype) && (TYPE_REGMEM == stype)) {
        byte(0x23);
        rmem(&ptr,dval,src);
      }
      else if ((TYPE_REG == dtype) && (TYPE_MEM == stype)) {
        byte(0x23);
        mem(&ptr,dval,sval);
      }
      else if ((TYPE_REGMEM == dtype) && (TYPE_IMM == stype)) {
        byte(0x81);
        rmem(&ptr,4,dst);
        dword(sval);
      }
      else if ((TYPE_MEM == dtype) && (TYPE_IMM == stype)) {
        byte(0x81);
        mem(&ptr,4,dval);
        dword(sval);
      }
      else {
        fatal("AND: invalid source/destination");
      }
      break;
    case X86_OR:
      if ((TYPE_REG == dtype) && (TYPE_IMM == stype)) {
        if (EAX == dval) {
          byte(0x0D);
          dword(sval);
        }
        else {
          byte(0x81);
          byte(modrm(MREG,1,dval));
          dword(sval);
        }
      }
      else if ((TYPE_REG == dtype) && (TYPE_REG == stype)) {
        byte(0x09);
        byte(modrm(MREG,sval,dval));
      }
      else if ((TYPE_REG == dtype) && (TYPE_REGMEM == stype)) {
        byte(0x0B);
        rmem(&ptr,dval,src);
      }
      else if ((TYPE_REG == dtype) && (TYPE_MEM == stype)) {
        byte(0x0B);
        mem(&ptr,dval,sval);
      }
      else if ((TYPE_REGMEM == dtype) && (TYPE_IMM == stype)) {
        byte(0x81);
        rmem(&ptr,1,dst);
        dword(sval);
      }
      else if ((TYPE_MEM == dtype) && (TYPE_IMM == stype)) {
        byte(0x81);
        mem(&ptr,1,dval);
        dword(sval);
      }
      else {
        fatal("OR: invalid source/destination");
      }
      break;
    case X86_XOR:
      if ((TYPE_REG == dtype) && (TYPE_IMM == stype)) {
        if (EAX == dval) {
          byte(0x35);
          dword(sval);
        }
        else {
          byte(0x81);
          byte(modrm(MREG,6,dval));
          dword(sval);
        }
      }
      else if ((TYPE_REG == dtype) && (TYPE_REG == stype)) {
        byte(0x31);
        byte(modrm(MREG,sval,dval));
      }
      else if ((TYPE_REG == dtype) && (TYPE_REGMEM == stype)) {
        byte(0x33);
        rmem(&ptr,dval,src);
      }
      else if ((TYPE_REG == dtype) && (TYPE_MEM == stype)) {
        byte(0x33);
        mem(&ptr,dval,sval);
      }
      else if ((TYPE_REGMEM == dtype) && (TYPE_IMM == stype)) {
        byte(0x81);
        rmem(&ptr,6,dst);
        dword(sval);
      }
      else if ((TYPE_MEM == dtype) && (TYPE_IMM == stype)) {
        byte(0x81);
        mem(&ptr,6,dval);
        dword(sval);
      }
      else {
        fatal("XOR: invalid source/destination");
      }
      break;
    case X86_NOT:
      if (TYPE_REG == dtype) {
        byte(0xF7);
        byte(modrm(MREG,2,dval));
      }
      else if (TYPE_REGMEM == dtype) {
        byte(0xF7);
        rmem(&ptr,2,dst);
      }
      else if (TYPE_MEM == dtype) {
        byte(0xF7);
        mem(&ptr,2,dval);
      }
      else {
        fatal("NOT: invalid destination");
      }
      break;
    case X86_NOP:
      byte(0x90);
      break;
    case X86_PUSHAD:
      byte(0x60);
      break;
    case X86_POPAD:
      byte(0x61);
      break;
    case X86_IMUL:
      if ((TYPE_REG == dtype) && (TYPE_IMM == stype)) {
        byte(0x69);
        byte(modrm(MREG,dval,dval));
        dword(sval);
      }
      else if ((TYPE_REG == dtype) && (TYPE_REG == stype)) {
        byte(0x0F);
        byte(0xAF);
        byte(modrm(MREG,dval,sval));
      }
      else if ((TYPE_REG == dtype) && (TYPE_REGMEM == stype)) {
        byte(0x0F);
        byte(0xAF);
        rmem(&ptr,dval,src);
      }
      else if ((TYPE_REG == dtype) && (TYPE_MEM == stype)) {
        byte(0x0F);
        byte(0xAF);
        mem(&ptr,dval,sval);
      }
      break;
    case X86_IDIV:
      if (TYPE_REG == dtype) {
        byte(0xF7);
        byte(modrm(MREG,7,dval));
      }
      else if (TYPE_REGMEM == dtype) {
        byte(0xF7);
        rmem(&ptr,7,dst);
      }
      else if (TYPE_MEM == dtype) {
        byte(0xF7);
        mem(&ptr,7,dval);
      }
      else {
        fatal("IDIV: invalid destination");
      }
      break;
    case X86_LEA:
      if ((TYPE_REG == dtype) && (TYPE_REGMEM == stype)) {
        byte(0x8d);
        rmem(&ptr,dval,src);
      }
      else if ((TYPE_REG == dtype) && (TYPE_MEM == stype)) {
        byte(0x8d);
        mem(&ptr,dval,sval);
      }
      else {
        fatal("LEA: invalid source/destination");
      }
      break;
    case X86_FLD_64:
      if (TYPE_REG == dtype) {
        byte(0xD9);
        byte(0xC0+dval);
      }
      else if (TYPE_REGMEM == dtype) {
        byte(0xDD);
        rmem(&ptr,0,dst);
      }
      else if (TYPE_MEM == dtype) {
        byte(0xDD);
        mem(&ptr,0,dval);
      }
      else {
        fatal("FLD64: invalid destination");
      }
      break;
    case X86_FST_64:
      if (TYPE_REG == dtype) {
        byte(0xDD);
        byte(0xD0+dval);
      }
      else if (TYPE_REGMEM == dtype) {
        byte(0xDD);
        rmem(&ptr,2,dst);
      }
      else if (TYPE_MEM == dtype) {
        byte(0xDD);
        mem(&ptr,2,dval);
      }
      else {
        fatal("FST64: invalid destination");
      }
      break;
    case X86_FSTP_64:
      if (TYPE_REG == dtype) {
        byte(0xDD);
        byte(0xD8+dval);
      }
      else if (TYPE_REGMEM == dtype) {
        byte(0xDD);
        rmem(&ptr,3,dst);
      }
      else if (TYPE_MEM == dtype) {
        byte(0xDD);
        mem(&ptr,3,dval);
      }
      else {
        fatal("FSTP64: invalid destination");
      }
      break;
    case X86_FILD:
      if (TYPE_REGMEM == dtype) {
        byte(0xDB);
        rmem(&ptr,0,dst);
      }
      else if (TYPE_MEM == dtype) {
        byte(0xDB);
        mem(&ptr,0,dval);
      }
      else {
        fatal("FLD64: invalid destination");
      }
      break;
    case X86_FADD:
      if (TYPE_REG == dtype) {
        byte(0xD8);
        byte(0xC0+dval);
      }
      else if (TYPE_REGMEM == dtype) {
        byte(0xDC);
        rmem(&ptr,0,dst);
      }
      else if (TYPE_MEM == dtype) {
        byte(0xDC);
        mem(&ptr,0,dval);
      }
      else {
        fatal("FADD: invalid destination");
      }
      break;
    case X86_FSUB:
      if (TYPE_REG == dtype) {
        byte(0xD8);
        byte(0xE0+dval);
      }
      else if (TYPE_REGMEM == dtype) {
        byte(0xDC);
        rmem(&ptr,4,dst);
      }
      else if (TYPE_MEM == dtype) {
        byte(0xDC);
        mem(&ptr,4,dval);
      }
      else {
        fatal("FSUB: invalid destination");
      }
      break;
    case X86_FMUL:
      if (TYPE_REG == dtype) {
        byte(0xD8);
        byte(0xC8+dval);
      }
      else if (TYPE_REGMEM == dtype) {
        byte(0xDC);
        rmem(&ptr,1,dst);
      }
      else if (TYPE_MEM == dtype) {
        byte(0xDC);
        mem(&ptr,1,dval);
      }
      else {
        fatal("FMUL: invalid destination");
      }
      break;
    case X86_FDIV:
      if (TYPE_REG == dtype) {
        byte(0xD8);
        byte(0xF0+dval);
      }
      else if (TYPE_REGMEM == dtype) {
        byte(0xDC);
        rmem(&ptr,6,dst);
      }
      else if (TYPE_MEM == dtype) {
        byte(0xDC);
        mem(&ptr,6,dval);
      }
      else {
        fatal("FDIV: invalid destination");
      }
      break;
    case X86_FUCOMPP:
      byte(0xDA);
      byte(0xE9);
      break;
    case X86_FNSTSW_AX:
      byte(0xDF);
      byte(0xE0);
      break;
    case X86_SAHF:
      byte(0x9E);
      break;
    case X86_FSQRT:
      byte(0xD9);
      byte(0xFA);
      break;
    case X86_INT3:
      byte(0xCC);
      break;
    case X86_PUSHF:
      byte(0x9C);
      break;
    case X86_POPF:
      byte(0x9D);
      break;
    case X86_NONE:
      break;
    default:
      abort();
      break;
    }

    assert(maxinstrsize >= ptr-instart);
    cpucode->nbytes += ptr-instart;
  }

  /* Second pass: Recompile all jump and call instructions with the correct dest addrs
     based on the resolved label references gained from the first pass */
  for (pos = 0; pos < as->count; pos++) {
    x86_instr *instr = &as->instructions[pos];
    int dtype = instr->dst.type;
    int dval = instr->dst.val;
    int stype = instr->src.type;
    int sval = instr->src.val;
    ptr = (char*)cpucode->data+instr->addr;
    switch (instr->op) {
    case X86_ADD:
      if (instr->isgetnextaddr) {
        assert(TYPE_REG == dtype);
        assert(TYPE_IMM == stype);
        add_reg_imm(&ptr,dval,labeladdrs[sval]);
      }
      break;
    case X86_MOV:
      if ((TYPE_REG == dtype) && (TYPE_LABEL == stype)) {
        byte(0xB8+(dval));
        dword(labeladdrs[sval]);
      }
      break;
    case X86_JMP:
      if (TYPE_LABEL == dtype) {
        assert((0 <= dval) && (dval < as->labels));
        byte(0xE9);
/*         printf("jmp: label addr %d = %x, rel addr = %x\n",dval,labeladdrs[dval], */
/*                labeladdrs[dval]-instr->addr-5); */
        dword(labeladdrs[dval]-instr->addr-5);
      }
      break;
    case X86_JZ:
      assert((0 <= dval) && (dval < as->labels));
      byte(0x0F);
      byte(0x84);
      dword(labeladdrs[dval]-instr->addr-6);
      break;
    case X86_JNE:
      assert((0 <= dval) && (dval < as->labels));
      byte(0x0F);
      byte(0x85);
      dword(labeladdrs[dval]-instr->addr-6);
      break;
    case X86_JG:
      assert((0 <= dval) && (dval < as->labels));
      byte(0x0F);
      byte(0x8F);
      dword(labeladdrs[dval]-instr->addr-6);
      break;
    case X86_JGE:
      assert((0 <= dval) && (dval < as->labels));
      byte(0x0F);
      byte(0x8D);
      dword(labeladdrs[dval]-instr->addr-6);
      break;
    case X86_JL:
      assert((0 <= dval) && (dval < as->labels));
      byte(0x0F);
      byte(0x8C);
      dword(labeladdrs[dval]-instr->addr-6);
      break;
    case X86_JLE:
      assert((0 <= dval) && (dval < as->labels));
      byte(0x0F);
      byte(0x8E);
      dword(labeladdrs[dval]-instr->addr-6);
      break;
    case X86_JNB:
      assert((0 <= dval) && (dval < as->labels));
      byte(0x0F);
      byte(0x83);
      dword(labeladdrs[dval]-instr->addr-6);
      break;
    case X86_JNBE:
      assert((0 <= dval) && (dval < as->labels));
      byte(0x0F);
      byte(0x87);
      dword(labeladdrs[dval]-instr->addr-6);
      break;
    case X86_JNA:
      assert((0 <= dval) && (dval < as->labels));
      byte(0x0F);
      byte(0x86);
      dword(labeladdrs[dval]-instr->addr-6);
      break;
    case X86_JNAE:
      assert((0 <= dval) && (dval < as->labels));
      byte(0x0F);
      byte(0x82);
      dword(labeladdrs[dval]-instr->addr-6);
      break;
    case X86_CALL:
      if (TYPE_LABEL == dtype) {
        assert((0 <= dval) && (dval < as->labels));
        byte(0xE8);
        dword(labeladdrs[dval]-instr->addr-5);
      }
      break;
    }
  }
}

void write_asm_raw(const char *filename, array *cpucode)
{
  FILE *f;
  int i;

  if (NULL == (f = fopen(filename,"w"))) {
    perror(filename);
    exit(1);
  }

  fprintf(f,"\t.file \"compiled.c\"\n");
  fprintf(f,".globl _start\n");
  fprintf(f,"\t.section\t.rodata\n");
  fprintf(f,"\t.type\t_start,@object\n");
  fprintf(f,"\t.size\t_start,%d\n",cpucode->nbytes);
  fprintf(f,"_start:\n");

  for (i = 0; i < cpucode->nbytes; i++) {
    char x = ((char*)cpucode->data)[i] & 0xFF;
    fprintf(f,"\t.byte\t%d\n",x);
  }
  fclose(f);
}

