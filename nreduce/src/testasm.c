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

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "assembler.h"
#include <stdlib.h>

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
  fprintf(f,"\t.size\t_start,%d\n",cpucode->size);
  fprintf(f,"_start:\n");

  for (i = 0; i < cpucode->size; i++) {
    char x = ((char*)cpucode->data)[i] & 0xFF;
    fprintf(f,"\t.byte\t%d\n",x);
  }
  fclose(f);
}

void testasm(array *cpucode)
{
  x86_assembly *as = x86_assembly_new();

  I_PUSH(imm(12));
  I_PUSH(imm(123456789));
  I_PUSH(absmem(12));
  I_PUSH(absmem(123456789));

  I_PUSH(reg(EAX));
  I_PUSH(reg(ECX));
  I_PUSH(reg(EDX));
  I_PUSH(reg(EBX));
  I_PUSH(reg(ESP));
  I_PUSH(reg(EBP));
  I_PUSH(reg(ESI));
  I_PUSH(reg(EDI));

  I_PUSH(regmem(EAX,12));
  I_PUSH(regmem(ECX,12));
  I_PUSH(regmem(EDX,12));
  I_PUSH(regmem(EBX,12));
  I_PUSH(regmem(ESP,12));
  I_PUSH(regmem(EBP,12));
  I_PUSH(regmem(ESI,12));
  I_PUSH(regmem(EDI,12));

  I_PUSH(regmem(EAX,123456789));
  I_PUSH(regmem(ECX,123456789));
  I_PUSH(regmem(EDX,123456789));
  I_PUSH(regmem(EBX,123456789));
  I_PUSH(regmem(ESP,123456789));
  I_PUSH(regmem(EBP,123456789));
  I_PUSH(regmem(ESI,123456789));
  I_PUSH(regmem(EDI,123456789));

  I_POP(absmem(12));
  I_POP(absmem(123456789));

  I_POP(reg(EAX));
  I_POP(reg(ECX));
  I_POP(reg(EDX));
  I_POP(reg(EBX));
  I_POP(reg(ESP));
  I_POP(reg(EBP));
  I_POP(reg(ESI));
  I_POP(reg(EDI));

  I_POP(regmem(EAX,12));
  I_POP(regmem(ECX,12));
  I_POP(regmem(EDX,12));
  I_POP(regmem(EBX,12));
  I_POP(regmem(ESP,12));
  I_POP(regmem(EBP,12));
  I_POP(regmem(ESI,12));
  I_POP(regmem(EDI,12));

  I_POP(regmem(EAX,123456789));
  I_POP(regmem(ECX,123456789));
  I_POP(regmem(EDX,123456789));
  I_POP(regmem(EBX,123456789));
  I_POP(regmem(ESP,123456789));
  I_POP(regmem(EBP,123456789));
  I_POP(regmem(ESI,123456789));
  I_POP(regmem(EDI,123456789));

  I_MOV(reg(EBX),reg(EAX));
  I_MOV(reg(EBX),reg(ECX));
  I_MOV(reg(EBX),reg(EDX));
  I_MOV(reg(EBX),reg(EBX));
  I_MOV(reg(EBX),reg(ESP));
  I_MOV(reg(EBX),reg(EBP));
  I_MOV(reg(EBX),reg(ESI));
  I_MOV(reg(EBX),reg(EDI));

  I_MOV(reg(EAX),reg(EBX));
  I_MOV(reg(ECX),reg(EBX));
  I_MOV(reg(EDX),reg(EBX));
  I_MOV(reg(EBX),reg(EBX));
  I_MOV(reg(ESP),reg(EBX));
  I_MOV(reg(EBP),reg(EBX));
  I_MOV(reg(ESI),reg(EBX));
  I_MOV(reg(EDI),reg(EBX));

  I_MOV(reg(EAX),absmem(12));
  I_MOV(reg(ECX),absmem(12));
  I_MOV(reg(EDX),absmem(12));
  I_MOV(reg(EBX),absmem(12));
  I_MOV(reg(ESP),absmem(12));
  I_MOV(reg(EBP),absmem(12));
  I_MOV(reg(ESI),absmem(12));
  I_MOV(reg(EDI),absmem(12));

  I_MOV(reg(EAX),absmem(123456789));
  I_MOV(reg(ECX),absmem(123456789));
  I_MOV(reg(EDX),absmem(123456789));
  I_MOV(reg(EBX),absmem(123456789));
  I_MOV(reg(ESP),absmem(123456789));
  I_MOV(reg(EBP),absmem(123456789));
  I_MOV(reg(ESI),absmem(123456789));
  I_MOV(reg(EDI),absmem(123456789));

  I_MOV(reg(EBX),regmem(EAX,12));
  I_MOV(reg(EBX),regmem(ECX,12));
  I_MOV(reg(EBX),regmem(EDX,12));
  I_MOV(reg(EBX),regmem(EBX,12));
  I_MOV(reg(EBX),regmem(ESP,12));
  I_MOV(reg(EBX),regmem(EBP,12));
  I_MOV(reg(EBX),regmem(ESI,12));
  I_MOV(reg(EBX),regmem(EDI,12));

  I_MOV(reg(EBX),regmem(EAX,123456789));
  I_MOV(reg(EBX),regmem(ECX,123456789));
  I_MOV(reg(EBX),regmem(EDX,123456789));
  I_MOV(reg(EBX),regmem(EBX,123456789));
  I_MOV(reg(EBX),regmem(ESP,123456789));
  I_MOV(reg(EBX),regmem(EBP,123456789));
  I_MOV(reg(EBX),regmem(ESI,123456789));
  I_MOV(reg(EBX),regmem(EDI,123456789));

  I_MOV(reg(EAX),regmem(EBX,12));
  I_MOV(reg(ECX),regmem(EBX,12));
  I_MOV(reg(EDX),regmem(EBX,12));
  I_MOV(reg(EBX),regmem(EBX,12));
  I_MOV(reg(ESP),regmem(EBX,12));
  I_MOV(reg(EBP),regmem(EBX,12));
  I_MOV(reg(ESI),regmem(EBX,12));
  I_MOV(reg(EDI),regmem(EBX,12));

  I_MOV(reg(EAX),regmem(EBX,123456789));
  I_MOV(reg(ECX),regmem(EBX,123456789));
  I_MOV(reg(EDX),regmem(EBX,123456789));
  I_MOV(reg(EBX),regmem(EBX,123456789));
  I_MOV(reg(ESP),regmem(EBX,123456789));
  I_MOV(reg(EBP),regmem(EBX,123456789));
  I_MOV(reg(ESI),regmem(EBX,123456789));
  I_MOV(reg(EDI),regmem(EBX,123456789));

  I_MOV(reg(EAX),imm(12));
  I_MOV(reg(ECX),imm(12));
  I_MOV(reg(EDX),imm(12));
  I_MOV(reg(EBX),imm(12));
  I_MOV(reg(ESP),imm(12));
  I_MOV(reg(EBP),imm(12));
  I_MOV(reg(ESI),imm(12));
  I_MOV(reg(EDI),imm(12));

  I_MOV(reg(EAX),imm(123456789));
  I_MOV(reg(ECX),imm(123456789));
  I_MOV(reg(EDX),imm(123456789));
  I_MOV(reg(EBX),imm(123456789));
  I_MOV(reg(ESP),imm(123456789));
  I_MOV(reg(EBP),imm(123456789));
  I_MOV(reg(ESI),imm(123456789));
  I_MOV(reg(EDI),imm(123456789));

  I_MOV(absmem(12),reg(EAX));
  I_MOV(absmem(12),reg(ECX));
  I_MOV(absmem(12),reg(EDX));
  I_MOV(absmem(12),reg(EBX));
  I_MOV(absmem(12),reg(ESP));
  I_MOV(absmem(12),reg(EBP));
  I_MOV(absmem(12),reg(ESI));
  I_MOV(absmem(12),reg(EDI));

  I_MOV(absmem(123456789),reg(EAX));
  I_MOV(absmem(123456789),reg(ECX));
  I_MOV(absmem(123456789),reg(EDX));
  I_MOV(absmem(123456789),reg(EBX));
  I_MOV(absmem(123456789),reg(ESP));
  I_MOV(absmem(123456789),reg(EBP));
  I_MOV(absmem(123456789),reg(ESI));
  I_MOV(absmem(123456789),reg(EDI));

  I_MOV(regmem(EBX,12),reg(EAX));
  I_MOV(regmem(EBX,12),reg(ECX));
  I_MOV(regmem(EBX,12),reg(EDX));
  I_MOV(regmem(EBX,12),reg(EBX));
  I_MOV(regmem(EBX,12),reg(ESP));
  I_MOV(regmem(EBX,12),reg(EBP));
  I_MOV(regmem(EBX,12),reg(ESI));
  I_MOV(regmem(EBX,12),reg(EDI));

  I_MOV(regmem(EBX,123456789),reg(EAX));
  I_MOV(regmem(EBX,123456789),reg(ECX));
  I_MOV(regmem(EBX,123456789),reg(EDX));
  I_MOV(regmem(EBX,123456789),reg(EBX));
  I_MOV(regmem(EBX,123456789),reg(ESP));
  I_MOV(regmem(EBX,123456789),reg(EBP));
  I_MOV(regmem(EBX,123456789),reg(ESI));
  I_MOV(regmem(EBX,123456789),reg(EDI));

  I_MOV(regmem(EAX,12),reg(EBX));
  I_MOV(regmem(ECX,12),reg(EBX));
  I_MOV(regmem(EDX,12),reg(EBX));
  I_MOV(regmem(EBX,12),reg(EBX));
  I_MOV(regmem(ESP,12),reg(EBX));
  I_MOV(regmem(EBP,12),reg(EBX));
  I_MOV(regmem(ESI,12),reg(EBX));
  I_MOV(regmem(EDI,12),reg(EBX));

  I_MOV(regmem(EAX,12),imm(99));
  I_MOV(regmem(ECX,12),imm(99));
  I_MOV(regmem(EDX,12),imm(99));
  I_MOV(regmem(EBX,12),imm(99));
  I_MOV(regmem(ESP,12),imm(99));
  I_MOV(regmem(EBP,12),imm(99));
  I_MOV(regmem(ESI,12),imm(99));
  I_MOV(regmem(EDI,12),imm(99));

  I_MOV(regmem(EAX,123456789),imm(99));
  I_MOV(regmem(ECX,123456789),imm(99));
  I_MOV(regmem(EDX,123456789),imm(99));
  I_MOV(regmem(EBX,123456789),imm(99));
  I_MOV(regmem(ESP,123456789),imm(99));
  I_MOV(regmem(EBP,123456789),imm(99));
  I_MOV(regmem(ESI,123456789),imm(99));
  I_MOV(regmem(EDI,123456789),imm(99));

  I_MOV(reg(EBX),regmem(EAX,0));
  I_MOV(reg(EBX),regmem(ECX,0));
  I_MOV(reg(EBX),regmem(EDX,0));
  I_MOV(reg(EBX),regmem(EBX,0));
  I_MOV(reg(EBX),regmem(ESP,0));
  I_MOV(reg(EBX),regmem(EBP,0));
  I_MOV(reg(EBX),regmem(ESI,0));
  I_MOV(reg(EBX),regmem(EDI,0));

  I_CMP(reg(EBX),reg(EAX));
  I_CMP(reg(EBX),reg(ECX));
  I_CMP(reg(EBX),reg(EDX));
  I_CMP(reg(EBX),reg(EBX));
  I_CMP(reg(EBX),reg(ESP));
  I_CMP(reg(EBX),reg(EBP));
  I_CMP(reg(EBX),reg(ESI));
  I_CMP(reg(EBX),reg(EDI));

  I_CMP(reg(EAX),reg(EBX));
  I_CMP(reg(ECX),reg(EBX));
  I_CMP(reg(EDX),reg(EBX));
  I_CMP(reg(EBX),reg(EBX));
  I_CMP(reg(ESP),reg(EBX));
  I_CMP(reg(EBP),reg(EBX));
  I_CMP(reg(ESI),reg(EBX));
  I_CMP(reg(EDI),reg(EBX));

  I_CMP(reg(EAX),imm(99));
  I_CMP(reg(ECX),imm(99));
  I_CMP(reg(EDX),imm(99));
  I_CMP(reg(EBX),imm(99));
  I_CMP(reg(ESP),imm(99));
  I_CMP(reg(EBP),imm(99));
  I_CMP(reg(ESI),imm(99));
  I_CMP(reg(EDI),imm(99));

  I_CMP(absmem(12),imm(99));
  I_CMP(absmem(123456789),imm(99));

  I_CMP(regmem(EAX,12),imm(99));
  I_CMP(regmem(ECX,12),imm(99));
  I_CMP(regmem(EDX,12),imm(99));
  I_CMP(regmem(EBX,12),imm(99));
  I_CMP(regmem(ESP,12),imm(99));
  I_CMP(regmem(EBP,12),imm(99));
  I_CMP(regmem(ESI,12),imm(99));
  I_CMP(regmem(EDI,12),imm(99));

  I_CMP(regmem(EAX,123456789),imm(99));
  I_CMP(regmem(ECX,123456789),imm(99));
  I_CMP(regmem(EDX,123456789),imm(99));
  I_CMP(regmem(EBX,123456789),imm(99));
  I_CMP(regmem(ESP,123456789),imm(99));
  I_CMP(regmem(EBP,123456789),imm(99));
  I_CMP(regmem(ESI,123456789),imm(99));
  I_CMP(regmem(EDI,123456789),imm(99));

  I_CMP(reg(EBX),regmem(EAX,0));
  I_CMP(reg(EBX),regmem(ECX,0));
  I_CMP(reg(EBX),regmem(EDX,0));
  I_CMP(reg(EBX),regmem(EBX,0));
  I_CMP(reg(EBX),regmem(ESP,0));
  I_CMP(reg(EBX),regmem(EBP,0));
  I_CMP(reg(EBX),regmem(ESI,0));
  I_CMP(reg(EBX),regmem(EDI,0));

  I_CMP(reg(EAX),regmem(EBX,0));
  I_CMP(reg(ECX),regmem(EBX,0));
  I_CMP(reg(EDX),regmem(EBX,0));
  I_CMP(reg(EBX),regmem(EBX,0));
  I_CMP(reg(ESP),regmem(EBX,0));
  I_CMP(reg(EBP),regmem(EBX,0));
  I_CMP(reg(ESI),regmem(EBX,0));
  I_CMP(reg(EDI),regmem(EBX,0));

  I_CMP(reg(EBX),regmem(EAX,12));
  I_CMP(reg(EBX),regmem(ECX,12));
  I_CMP(reg(EBX),regmem(EDX,12));
  I_CMP(reg(EBX),regmem(EBX,12));
  I_CMP(reg(EBX),regmem(ESP,12));
  I_CMP(reg(EBX),regmem(EBP,12));
  I_CMP(reg(EBX),regmem(ESI,12));
  I_CMP(reg(EBX),regmem(EDI,12));

  I_CMP(reg(EAX),regmem(EBX,12));
  I_CMP(reg(ECX),regmem(EBX,12));
  I_CMP(reg(EDX),regmem(EBX,12));
  I_CMP(reg(EBX),regmem(EBX,12));
  I_CMP(reg(ESP),regmem(EBX,12));
  I_CMP(reg(EBP),regmem(EBX,12));
  I_CMP(reg(ESI),regmem(EBX,12));
  I_CMP(reg(EDI),regmem(EBX,12));

  I_CMP(reg(EBX),regmem(EAX,123456789));
  I_CMP(reg(EBX),regmem(ECX,123456789));
  I_CMP(reg(EBX),regmem(EDX,123456789));
  I_CMP(reg(EBX),regmem(EBX,123456789));
  I_CMP(reg(EBX),regmem(ESP,123456789));
  I_CMP(reg(EBX),regmem(EBP,123456789));
  I_CMP(reg(EBX),regmem(ESI,123456789));
  I_CMP(reg(EBX),regmem(EDI,123456789));

  I_CMP(reg(EAX),regmem(EBX,123456789));
  I_CMP(reg(ECX),regmem(EBX,123456789));
  I_CMP(reg(EDX),regmem(EBX,123456789));
  I_CMP(reg(EBX),regmem(EBX,123456789));
  I_CMP(reg(ESP),regmem(EBX,123456789));
  I_CMP(reg(EBP),regmem(EBX,123456789));
  I_CMP(reg(ESI),regmem(EBX,123456789));
  I_CMP(reg(EDI),regmem(EBX,123456789));

  I_CMP(reg(EAX),absmem(12));
  I_CMP(reg(ECX),absmem(12));
  I_CMP(reg(EDX),absmem(12));
  I_CMP(reg(EBX),absmem(12));
  I_CMP(reg(ESP),absmem(12));
  I_CMP(reg(EBP),absmem(12));
  I_CMP(reg(ESI),absmem(12));
  I_CMP(reg(EDI),absmem(12));

  I_CMP(reg(EAX),absmem(123456789));
  I_CMP(reg(ECX),absmem(123456789));
  I_CMP(reg(EDX),absmem(123456789));
  I_CMP(reg(EBX),absmem(123456789));
  I_CMP(reg(ESP),absmem(123456789));
  I_CMP(reg(EBP),absmem(123456789));
  I_CMP(reg(ESI),absmem(123456789));
  I_CMP(reg(EDI),absmem(123456789));

  I_JMP(reg(EAX));
  I_JMP(reg(ECX));
  I_JMP(reg(EDX));
  I_JMP(reg(EBX));
  I_JMP(reg(ESP));
  I_JMP(reg(EBP));
  I_JMP(reg(ESI));
  I_JMP(reg(EDI));

  I_JMP(regmem(EAX,0));
  I_JMP(regmem(ECX,0));
  I_JMP(regmem(EDX,0));
  I_JMP(regmem(EBX,0));
  I_JMP(regmem(ESP,0));
  I_JMP(regmem(EBP,0));
  I_JMP(regmem(ESI,0));
  I_JMP(regmem(EDI,0));

  I_JMP(regmem(EAX,12));
  I_JMP(regmem(ECX,12));
  I_JMP(regmem(EDX,12));
  I_JMP(regmem(EBX,12));
  I_JMP(regmem(ESP,12));
  I_JMP(regmem(EBP,12));
  I_JMP(regmem(ESI,12));
  I_JMP(regmem(EDI,12));

  I_JMP(regmem(EAX,123456789));
  I_JMP(regmem(ECX,123456789));
  I_JMP(regmem(EDX,123456789));
  I_JMP(regmem(EBX,123456789));
  I_JMP(regmem(ESP,123456789));
  I_JMP(regmem(EBP,123456789));
  I_JMP(regmem(ESI,123456789));
  I_JMP(regmem(EDI,123456789));

/*   I1(X86_JZ,imm(12)); */
/*   I1(X86_JG,imm(12)); */
/*   I1(X86_JGE,imm(12)); */
/*   I1(X86_JL,imm(12)); */
/*   I1(X86_JLE,imm(12)); */
  /* FIXME: tests for jb and friends */

  I_RET();

  I_CALL(reg(EAX));
  I_CALL(reg(ECX));
  I_CALL(reg(EDX));
  I_CALL(reg(EBX));
  I_CALL(reg(ESP));
  I_CALL(reg(EBP));
  I_CALL(reg(ESI));
  I_CALL(reg(EDI));

  I_CALL(regmem(EAX,0));
  I_CALL(regmem(ECX,0));
  I_CALL(regmem(EDX,0));
  I_CALL(regmem(EBX,0));
  I_CALL(regmem(ESP,0));
  I_CALL(regmem(EBP,0));
  I_CALL(regmem(ESI,0));
  I_CALL(regmem(EDI,0));

  I_CALL(regmem(EAX,12));
  I_CALL(regmem(ECX,12));
  I_CALL(regmem(EDX,12));
  I_CALL(regmem(EBX,12));
  I_CALL(regmem(ESP,12));
  I_CALL(regmem(EBP,12));
  I_CALL(regmem(ESI,12));
  I_CALL(regmem(EDI,12));

  I_CALL(regmem(EAX,123456789));
  I_CALL(regmem(ECX,123456789));
  I_CALL(regmem(EDX,123456789));
  I_CALL(regmem(EBX,123456789));
  I_CALL(regmem(ESP,123456789));
  I_CALL(regmem(EBP,123456789));
  I_CALL(regmem(ESI,123456789));
  I_CALL(regmem(EDI,123456789));

  I_CALL(absmem(12));
  I_CALL(absmem(123456789));

  I_ADD(reg(EAX),imm(12));
  I_ADD(reg(ECX),imm(12));
  I_ADD(reg(EDX),imm(12));
  I_ADD(reg(EBX),imm(12));
  I_ADD(reg(ESP),imm(12));
  I_ADD(reg(EBP),imm(12));
  I_ADD(reg(ESI),imm(12));
  I_ADD(reg(EDI),imm(12));

  I_ADD(reg(EBX),reg(EAX));
  I_ADD(reg(EBX),reg(ECX));
  I_ADD(reg(EBX),reg(EDX));
  I_ADD(reg(EBX),reg(EBX));
  I_ADD(reg(EBX),reg(ESP));
  I_ADD(reg(EBX),reg(EBP));
  I_ADD(reg(EBX),reg(ESI));
  I_ADD(reg(EBX),reg(EDI));

  I_ADD(reg(EAX),reg(EBX));
  I_ADD(reg(ECX),reg(EBX));
  I_ADD(reg(EDX),reg(EBX));
  I_ADD(reg(EBX),reg(EBX));
  I_ADD(reg(ESP),reg(EBX));
  I_ADD(reg(EBP),reg(EBX));
  I_ADD(reg(ESI),reg(EBX));
  I_ADD(reg(EDI),reg(EBX));

  I_ADD(reg(EBX),regmem(EAX,0));
  I_ADD(reg(EBX),regmem(ECX,0));
  I_ADD(reg(EBX),regmem(EDX,0));
  I_ADD(reg(EBX),regmem(EBX,0));
  I_ADD(reg(EBX),regmem(ESP,0));
  I_ADD(reg(EBX),regmem(EBP,0));
  I_ADD(reg(EBX),regmem(ESI,0));
  I_ADD(reg(EBX),regmem(EDI,0));

  I_ADD(reg(EBX),regmem(EAX,12));
  I_ADD(reg(EBX),regmem(ECX,12));
  I_ADD(reg(EBX),regmem(EDX,12));
  I_ADD(reg(EBX),regmem(EBX,12));
  I_ADD(reg(EBX),regmem(ESP,12));
  I_ADD(reg(EBX),regmem(EBP,12));
  I_ADD(reg(EBX),regmem(ESI,12));
  I_ADD(reg(EBX),regmem(EDI,12));

  I_ADD(reg(EBX),regmem(EAX,123456789));
  I_ADD(reg(EBX),regmem(ECX,123456789));
  I_ADD(reg(EBX),regmem(EDX,123456789));
  I_ADD(reg(EBX),regmem(EBX,123456789));
  I_ADD(reg(EBX),regmem(ESP,123456789));
  I_ADD(reg(EBX),regmem(EBP,123456789));
  I_ADD(reg(EBX),regmem(ESI,123456789));
  I_ADD(reg(EBX),regmem(EDI,123456789));

  I_ADD(reg(EAX),regmem(EBX,0));
  I_ADD(reg(ECX),regmem(EBX,0));
  I_ADD(reg(EDX),regmem(EBX,0));
  I_ADD(reg(EBX),regmem(EBX,0));
  I_ADD(reg(ESP),regmem(EBX,0));
  I_ADD(reg(EBP),regmem(EBX,0));
  I_ADD(reg(ESI),regmem(EBX,0));
  I_ADD(reg(EDI),regmem(EBX,0));

  I_ADD(reg(EAX),regmem(EBX,12));
  I_ADD(reg(ECX),regmem(EBX,12));
  I_ADD(reg(EDX),regmem(EBX,12));
  I_ADD(reg(EBX),regmem(EBX,12));
  I_ADD(reg(ESP),regmem(EBX,12));
  I_ADD(reg(EBP),regmem(EBX,12));
  I_ADD(reg(ESI),regmem(EBX,12));
  I_ADD(reg(EDI),regmem(EBX,12));

  I_ADD(reg(EAX),regmem(EBX,123456789));
  I_ADD(reg(ECX),regmem(EBX,123456789));
  I_ADD(reg(EDX),regmem(EBX,123456789));
  I_ADD(reg(EBX),regmem(EBX,123456789));
  I_ADD(reg(ESP),regmem(EBX,123456789));
  I_ADD(reg(EBP),regmem(EBX,123456789));
  I_ADD(reg(ESI),regmem(EBX,123456789));
  I_ADD(reg(EDI),regmem(EBX,123456789));

  I_ADD(reg(EAX),absmem(12));
  I_ADD(reg(ECX),absmem(12));
  I_ADD(reg(EDX),absmem(12));
  I_ADD(reg(EBX),absmem(12));
  I_ADD(reg(ESP),absmem(12));
  I_ADD(reg(EBP),absmem(12));
  I_ADD(reg(ESI),absmem(12));
  I_ADD(reg(EDI),absmem(12));

  I_ADD(reg(EAX),absmem(123456789));
  I_ADD(reg(ECX),absmem(123456789));
  I_ADD(reg(EDX),absmem(123456789));
  I_ADD(reg(EBX),absmem(123456789));
  I_ADD(reg(ESP),absmem(123456789));
  I_ADD(reg(EBP),absmem(123456789));
  I_ADD(reg(ESI),absmem(123456789));
  I_ADD(reg(EDI),absmem(123456789));

  I_ADD(regmem(EAX,0),imm(99));
  I_ADD(regmem(ECX,0),imm(99));
  I_ADD(regmem(EDX,0),imm(99));
  I_ADD(regmem(EBX,0),imm(99));
  I_ADD(regmem(ESP,0),imm(99));
  I_ADD(regmem(EBP,0),imm(99));
  I_ADD(regmem(ESI,0),imm(99));
  I_ADD(regmem(EDI,0),imm(99));

  I_ADD(regmem(EAX,12),imm(99));
  I_ADD(regmem(ECX,12),imm(99));
  I_ADD(regmem(EDX,12),imm(99));
  I_ADD(regmem(EBX,12),imm(99));
  I_ADD(regmem(ESP,12),imm(99));
  I_ADD(regmem(EBP,12),imm(99));
  I_ADD(regmem(ESI,12),imm(99));
  I_ADD(regmem(EDI,12),imm(99));

  I_ADD(regmem(EAX,123456789),imm(99));
  I_ADD(regmem(ECX,123456789),imm(99));
  I_ADD(regmem(EDX,123456789),imm(99));
  I_ADD(regmem(EBX,123456789),imm(99));
  I_ADD(regmem(ESP,123456789),imm(99));
  I_ADD(regmem(EBP,123456789),imm(99));
  I_ADD(regmem(ESI,123456789),imm(99));
  I_ADD(regmem(EDI,123456789),imm(99));

  I_ADD(absmem(12),imm(99));
  I_ADD(absmem(123456789),imm(99));

  I_SUB(reg(EAX),imm(12));
  I_SUB(reg(ECX),imm(12));
  I_SUB(reg(EDX),imm(12));
  I_SUB(reg(EBX),imm(12));
  I_SUB(reg(ESP),imm(12));
  I_SUB(reg(EBP),imm(12));
  I_SUB(reg(ESI),imm(12));
  I_SUB(reg(EDI),imm(12));

  I_SUB(reg(EBX),reg(EAX));
  I_SUB(reg(EBX),reg(ECX));
  I_SUB(reg(EBX),reg(EDX));
  I_SUB(reg(EBX),reg(EBX));
  I_SUB(reg(EBX),reg(ESP));
  I_SUB(reg(EBX),reg(EBP));
  I_SUB(reg(EBX),reg(ESI));
  I_SUB(reg(EBX),reg(EDI));

  I_SUB(reg(EAX),reg(EBX));
  I_SUB(reg(ECX),reg(EBX));
  I_SUB(reg(EDX),reg(EBX));
  I_SUB(reg(EBX),reg(EBX));
  I_SUB(reg(ESP),reg(EBX));
  I_SUB(reg(EBP),reg(EBX));
  I_SUB(reg(ESI),reg(EBX));
  I_SUB(reg(EDI),reg(EBX));

  I_SUB(reg(EBX),regmem(EAX,0));
  I_SUB(reg(EBX),regmem(ECX,0));
  I_SUB(reg(EBX),regmem(EDX,0));
  I_SUB(reg(EBX),regmem(EBX,0));
  I_SUB(reg(EBX),regmem(ESP,0));
  I_SUB(reg(EBX),regmem(EBP,0));
  I_SUB(reg(EBX),regmem(ESI,0));
  I_SUB(reg(EBX),regmem(EDI,0));

  I_SUB(reg(EBX),regmem(EAX,12));
  I_SUB(reg(EBX),regmem(ECX,12));
  I_SUB(reg(EBX),regmem(EDX,12));
  I_SUB(reg(EBX),regmem(EBX,12));
  I_SUB(reg(EBX),regmem(ESP,12));
  I_SUB(reg(EBX),regmem(EBP,12));
  I_SUB(reg(EBX),regmem(ESI,12));
  I_SUB(reg(EBX),regmem(EDI,12));

  I_SUB(reg(EBX),regmem(EAX,123456789));
  I_SUB(reg(EBX),regmem(ECX,123456789));
  I_SUB(reg(EBX),regmem(EDX,123456789));
  I_SUB(reg(EBX),regmem(EBX,123456789));
  I_SUB(reg(EBX),regmem(ESP,123456789));
  I_SUB(reg(EBX),regmem(EBP,123456789));
  I_SUB(reg(EBX),regmem(ESI,123456789));
  I_SUB(reg(EBX),regmem(EDI,123456789));

  I_SUB(reg(EAX),regmem(EBX,0));
  I_SUB(reg(ECX),regmem(EBX,0));
  I_SUB(reg(EDX),regmem(EBX,0));
  I_SUB(reg(EBX),regmem(EBX,0));
  I_SUB(reg(ESP),regmem(EBX,0));
  I_SUB(reg(EBP),regmem(EBX,0));
  I_SUB(reg(ESI),regmem(EBX,0));
  I_SUB(reg(EDI),regmem(EBX,0));

  I_SUB(reg(EAX),regmem(EBX,12));
  I_SUB(reg(ECX),regmem(EBX,12));
  I_SUB(reg(EDX),regmem(EBX,12));
  I_SUB(reg(EBX),regmem(EBX,12));
  I_SUB(reg(ESP),regmem(EBX,12));
  I_SUB(reg(EBP),regmem(EBX,12));
  I_SUB(reg(ESI),regmem(EBX,12));
  I_SUB(reg(EDI),regmem(EBX,12));

  I_SUB(reg(EAX),regmem(EBX,123456789));
  I_SUB(reg(ECX),regmem(EBX,123456789));
  I_SUB(reg(EDX),regmem(EBX,123456789));
  I_SUB(reg(EBX),regmem(EBX,123456789));
  I_SUB(reg(ESP),regmem(EBX,123456789));
  I_SUB(reg(EBP),regmem(EBX,123456789));
  I_SUB(reg(ESI),regmem(EBX,123456789));
  I_SUB(reg(EDI),regmem(EBX,123456789));

  I_SUB(reg(EAX),absmem(12));
  I_SUB(reg(ECX),absmem(12));
  I_SUB(reg(EDX),absmem(12));
  I_SUB(reg(EBX),absmem(12));
  I_SUB(reg(ESP),absmem(12));
  I_SUB(reg(EBP),absmem(12));
  I_SUB(reg(ESI),absmem(12));
  I_SUB(reg(EDI),absmem(12));

  I_SUB(reg(EAX),absmem(123456789));
  I_SUB(reg(ECX),absmem(123456789));
  I_SUB(reg(EDX),absmem(123456789));
  I_SUB(reg(EBX),absmem(123456789));
  I_SUB(reg(ESP),absmem(123456789));
  I_SUB(reg(EBP),absmem(123456789));
  I_SUB(reg(ESI),absmem(123456789));
  I_SUB(reg(EDI),absmem(123456789));

  I_SUB(regmem(EAX,0),imm(99));
  I_SUB(regmem(ECX,0),imm(99));
  I_SUB(regmem(EDX,0),imm(99));
  I_SUB(regmem(EBX,0),imm(99));
  I_SUB(regmem(ESP,0),imm(99));
  I_SUB(regmem(EBP,0),imm(99));
  I_SUB(regmem(ESI,0),imm(99));
  I_SUB(regmem(EDI,0),imm(99));

  I_SUB(regmem(EAX,12),imm(99));
  I_SUB(regmem(ECX,12),imm(99));
  I_SUB(regmem(EDX,12),imm(99));
  I_SUB(regmem(EBX,12),imm(99));
  I_SUB(regmem(ESP,12),imm(99));
  I_SUB(regmem(EBP,12),imm(99));
  I_SUB(regmem(ESI,12),imm(99));
  I_SUB(regmem(EDI,12),imm(99));

  I_SUB(regmem(EAX,123456789),imm(99));
  I_SUB(regmem(ECX,123456789),imm(99));
  I_SUB(regmem(EDX,123456789),imm(99));
  I_SUB(regmem(EBX,123456789),imm(99));
  I_SUB(regmem(ESP,123456789),imm(99));
  I_SUB(regmem(EBP,123456789),imm(99));
  I_SUB(regmem(ESI,123456789),imm(99));
  I_SUB(regmem(EDI,123456789),imm(99));

  I_SUB(absmem(12),imm(99));
  I_SUB(absmem(123456789),imm(99));

  I_AND(reg(EAX),imm(12));
  I_AND(reg(ECX),imm(12));
  I_AND(reg(EDX),imm(12));
  I_AND(reg(EBX),imm(12));
  I_AND(reg(ESP),imm(12));
  I_AND(reg(EBP),imm(12));
  I_AND(reg(ESI),imm(12));
  I_AND(reg(EDI),imm(12));

  I_AND(reg(EBX),reg(EAX));
  I_AND(reg(EBX),reg(ECX));
  I_AND(reg(EBX),reg(EDX));
  I_AND(reg(EBX),reg(EBX));
  I_AND(reg(EBX),reg(ESP));
  I_AND(reg(EBX),reg(EBP));
  I_AND(reg(EBX),reg(ESI));
  I_AND(reg(EBX),reg(EDI));

  I_AND(reg(EAX),reg(EBX));
  I_AND(reg(ECX),reg(EBX));
  I_AND(reg(EDX),reg(EBX));
  I_AND(reg(EBX),reg(EBX));
  I_AND(reg(ESP),reg(EBX));
  I_AND(reg(EBP),reg(EBX));
  I_AND(reg(ESI),reg(EBX));
  I_AND(reg(EDI),reg(EBX));

  I_AND(reg(EBX),regmem(EAX,0));
  I_AND(reg(EBX),regmem(ECX,0));
  I_AND(reg(EBX),regmem(EDX,0));
  I_AND(reg(EBX),regmem(EBX,0));
  I_AND(reg(EBX),regmem(ESP,0));
  I_AND(reg(EBX),regmem(EBP,0));
  I_AND(reg(EBX),regmem(ESI,0));
  I_AND(reg(EBX),regmem(EDI,0));

  I_AND(reg(EBX),regmem(EAX,12));
  I_AND(reg(EBX),regmem(ECX,12));
  I_AND(reg(EBX),regmem(EDX,12));
  I_AND(reg(EBX),regmem(EBX,12));
  I_AND(reg(EBX),regmem(ESP,12));
  I_AND(reg(EBX),regmem(EBP,12));
  I_AND(reg(EBX),regmem(ESI,12));
  I_AND(reg(EBX),regmem(EDI,12));

  I_AND(reg(EBX),regmem(EAX,123456789));
  I_AND(reg(EBX),regmem(ECX,123456789));
  I_AND(reg(EBX),regmem(EDX,123456789));
  I_AND(reg(EBX),regmem(EBX,123456789));
  I_AND(reg(EBX),regmem(ESP,123456789));
  I_AND(reg(EBX),regmem(EBP,123456789));
  I_AND(reg(EBX),regmem(ESI,123456789));
  I_AND(reg(EBX),regmem(EDI,123456789));

  I_AND(reg(EAX),regmem(EBX,0));
  I_AND(reg(ECX),regmem(EBX,0));
  I_AND(reg(EDX),regmem(EBX,0));
  I_AND(reg(EBX),regmem(EBX,0));
  I_AND(reg(ESP),regmem(EBX,0));
  I_AND(reg(EBP),regmem(EBX,0));
  I_AND(reg(ESI),regmem(EBX,0));
  I_AND(reg(EDI),regmem(EBX,0));

  I_AND(reg(EAX),regmem(EBX,12));
  I_AND(reg(ECX),regmem(EBX,12));
  I_AND(reg(EDX),regmem(EBX,12));
  I_AND(reg(EBX),regmem(EBX,12));
  I_AND(reg(ESP),regmem(EBX,12));
  I_AND(reg(EBP),regmem(EBX,12));
  I_AND(reg(ESI),regmem(EBX,12));
  I_AND(reg(EDI),regmem(EBX,12));

  I_AND(reg(EAX),regmem(EBX,123456789));
  I_AND(reg(ECX),regmem(EBX,123456789));
  I_AND(reg(EDX),regmem(EBX,123456789));
  I_AND(reg(EBX),regmem(EBX,123456789));
  I_AND(reg(ESP),regmem(EBX,123456789));
  I_AND(reg(EBP),regmem(EBX,123456789));
  I_AND(reg(ESI),regmem(EBX,123456789));
  I_AND(reg(EDI),regmem(EBX,123456789));

  I_AND(reg(EAX),absmem(12));
  I_AND(reg(ECX),absmem(12));
  I_AND(reg(EDX),absmem(12));
  I_AND(reg(EBX),absmem(12));
  I_AND(reg(ESP),absmem(12));
  I_AND(reg(EBP),absmem(12));
  I_AND(reg(ESI),absmem(12));
  I_AND(reg(EDI),absmem(12));

  I_AND(reg(EAX),absmem(123456789));
  I_AND(reg(ECX),absmem(123456789));
  I_AND(reg(EDX),absmem(123456789));
  I_AND(reg(EBX),absmem(123456789));
  I_AND(reg(ESP),absmem(123456789));
  I_AND(reg(EBP),absmem(123456789));
  I_AND(reg(ESI),absmem(123456789));
  I_AND(reg(EDI),absmem(123456789));

  I_AND(regmem(EAX,0),imm(99));
  I_AND(regmem(ECX,0),imm(99));
  I_AND(regmem(EDX,0),imm(99));
  I_AND(regmem(EBX,0),imm(99));
  I_AND(regmem(ESP,0),imm(99));
  I_AND(regmem(EBP,0),imm(99));
  I_AND(regmem(ESI,0),imm(99));
  I_AND(regmem(EDI,0),imm(99));

  I_AND(regmem(EAX,12),imm(99));
  I_AND(regmem(ECX,12),imm(99));
  I_AND(regmem(EDX,12),imm(99));
  I_AND(regmem(EBX,12),imm(99));
  I_AND(regmem(ESP,12),imm(99));
  I_AND(regmem(EBP,12),imm(99));
  I_AND(regmem(ESI,12),imm(99));
  I_AND(regmem(EDI,12),imm(99));

  I_AND(regmem(EAX,123456789),imm(99));
  I_AND(regmem(ECX,123456789),imm(99));
  I_AND(regmem(EDX,123456789),imm(99));
  I_AND(regmem(EBX,123456789),imm(99));
  I_AND(regmem(ESP,123456789),imm(99));
  I_AND(regmem(EBP,123456789),imm(99));
  I_AND(regmem(ESI,123456789),imm(99));
  I_AND(regmem(EDI,123456789),imm(99));

  I_AND(absmem(12),imm(99));
  I_AND(absmem(123456789),imm(99));

  I_OR(reg(EAX),imm(12));
  I_OR(reg(ECX),imm(12));
  I_OR(reg(EDX),imm(12));
  I_OR(reg(EBX),imm(12));
  I_OR(reg(ESP),imm(12));
  I_OR(reg(EBP),imm(12));
  I_OR(reg(ESI),imm(12));
  I_OR(reg(EDI),imm(12));

  I_OR(reg(EBX),reg(EAX));
  I_OR(reg(EBX),reg(ECX));
  I_OR(reg(EBX),reg(EDX));
  I_OR(reg(EBX),reg(EBX));
  I_OR(reg(EBX),reg(ESP));
  I_OR(reg(EBX),reg(EBP));
  I_OR(reg(EBX),reg(ESI));
  I_OR(reg(EBX),reg(EDI));

  I_OR(reg(EAX),reg(EBX));
  I_OR(reg(ECX),reg(EBX));
  I_OR(reg(EDX),reg(EBX));
  I_OR(reg(EBX),reg(EBX));
  I_OR(reg(ESP),reg(EBX));
  I_OR(reg(EBP),reg(EBX));
  I_OR(reg(ESI),reg(EBX));
  I_OR(reg(EDI),reg(EBX));

  I_OR(reg(EBX),regmem(EAX,0));
  I_OR(reg(EBX),regmem(ECX,0));
  I_OR(reg(EBX),regmem(EDX,0));
  I_OR(reg(EBX),regmem(EBX,0));
  I_OR(reg(EBX),regmem(ESP,0));
  I_OR(reg(EBX),regmem(EBP,0));
  I_OR(reg(EBX),regmem(ESI,0));
  I_OR(reg(EBX),regmem(EDI,0));

  I_OR(reg(EBX),regmem(EAX,12));
  I_OR(reg(EBX),regmem(ECX,12));
  I_OR(reg(EBX),regmem(EDX,12));
  I_OR(reg(EBX),regmem(EBX,12));
  I_OR(reg(EBX),regmem(ESP,12));
  I_OR(reg(EBX),regmem(EBP,12));
  I_OR(reg(EBX),regmem(ESI,12));
  I_OR(reg(EBX),regmem(EDI,12));

  I_OR(reg(EBX),regmem(EAX,123456789));
  I_OR(reg(EBX),regmem(ECX,123456789));
  I_OR(reg(EBX),regmem(EDX,123456789));
  I_OR(reg(EBX),regmem(EBX,123456789));
  I_OR(reg(EBX),regmem(ESP,123456789));
  I_OR(reg(EBX),regmem(EBP,123456789));
  I_OR(reg(EBX),regmem(ESI,123456789));
  I_OR(reg(EBX),regmem(EDI,123456789));

  I_OR(reg(EAX),regmem(EBX,0));
  I_OR(reg(ECX),regmem(EBX,0));
  I_OR(reg(EDX),regmem(EBX,0));
  I_OR(reg(EBX),regmem(EBX,0));
  I_OR(reg(ESP),regmem(EBX,0));
  I_OR(reg(EBP),regmem(EBX,0));
  I_OR(reg(ESI),regmem(EBX,0));
  I_OR(reg(EDI),regmem(EBX,0));

  I_OR(reg(EAX),regmem(EBX,12));
  I_OR(reg(ECX),regmem(EBX,12));
  I_OR(reg(EDX),regmem(EBX,12));
  I_OR(reg(EBX),regmem(EBX,12));
  I_OR(reg(ESP),regmem(EBX,12));
  I_OR(reg(EBP),regmem(EBX,12));
  I_OR(reg(ESI),regmem(EBX,12));
  I_OR(reg(EDI),regmem(EBX,12));

  I_OR(reg(EAX),regmem(EBX,123456789));
  I_OR(reg(ECX),regmem(EBX,123456789));
  I_OR(reg(EDX),regmem(EBX,123456789));
  I_OR(reg(EBX),regmem(EBX,123456789));
  I_OR(reg(ESP),regmem(EBX,123456789));
  I_OR(reg(EBP),regmem(EBX,123456789));
  I_OR(reg(ESI),regmem(EBX,123456789));
  I_OR(reg(EDI),regmem(EBX,123456789));

  I_OR(reg(EAX),absmem(12));
  I_OR(reg(ECX),absmem(12));
  I_OR(reg(EDX),absmem(12));
  I_OR(reg(EBX),absmem(12));
  I_OR(reg(ESP),absmem(12));
  I_OR(reg(EBP),absmem(12));
  I_OR(reg(ESI),absmem(12));
  I_OR(reg(EDI),absmem(12));

  I_OR(reg(EAX),absmem(123456789));
  I_OR(reg(ECX),absmem(123456789));
  I_OR(reg(EDX),absmem(123456789));
  I_OR(reg(EBX),absmem(123456789));
  I_OR(reg(ESP),absmem(123456789));
  I_OR(reg(EBP),absmem(123456789));
  I_OR(reg(ESI),absmem(123456789));
  I_OR(reg(EDI),absmem(123456789));

  I_OR(regmem(EAX,0),imm(99));
  I_OR(regmem(ECX,0),imm(99));
  I_OR(regmem(EDX,0),imm(99));
  I_OR(regmem(EBX,0),imm(99));
  I_OR(regmem(ESP,0),imm(99));
  I_OR(regmem(EBP,0),imm(99));
  I_OR(regmem(ESI,0),imm(99));
  I_OR(regmem(EDI,0),imm(99));

  I_OR(regmem(EAX,12),imm(99));
  I_OR(regmem(ECX,12),imm(99));
  I_OR(regmem(EDX,12),imm(99));
  I_OR(regmem(EBX,12),imm(99));
  I_OR(regmem(ESP,12),imm(99));
  I_OR(regmem(EBP,12),imm(99));
  I_OR(regmem(ESI,12),imm(99));
  I_OR(regmem(EDI,12),imm(99));

  I_OR(regmem(EAX,123456789),imm(99));
  I_OR(regmem(ECX,123456789),imm(99));
  I_OR(regmem(EDX,123456789),imm(99));
  I_OR(regmem(EBX,123456789),imm(99));
  I_OR(regmem(ESP,123456789),imm(99));
  I_OR(regmem(EBP,123456789),imm(99));
  I_OR(regmem(ESI,123456789),imm(99));
  I_OR(regmem(EDI,123456789),imm(99));

  I_OR(absmem(12),imm(99));
  I_OR(absmem(123456789),imm(99));

  I_XOR(reg(EAX),imm(12));
  I_XOR(reg(ECX),imm(12));
  I_XOR(reg(EDX),imm(12));
  I_XOR(reg(EBX),imm(12));
  I_XOR(reg(ESP),imm(12));
  I_XOR(reg(EBP),imm(12));
  I_XOR(reg(ESI),imm(12));
  I_XOR(reg(EDI),imm(12));

  I_XOR(reg(EBX),reg(EAX));
  I_XOR(reg(EBX),reg(ECX));
  I_XOR(reg(EBX),reg(EDX));
  I_XOR(reg(EBX),reg(EBX));
  I_XOR(reg(EBX),reg(ESP));
  I_XOR(reg(EBX),reg(EBP));
  I_XOR(reg(EBX),reg(ESI));
  I_XOR(reg(EBX),reg(EDI));

  I_XOR(reg(EAX),reg(EBX));
  I_XOR(reg(ECX),reg(EBX));
  I_XOR(reg(EDX),reg(EBX));
  I_XOR(reg(EBX),reg(EBX));
  I_XOR(reg(ESP),reg(EBX));
  I_XOR(reg(EBP),reg(EBX));
  I_XOR(reg(ESI),reg(EBX));
  I_XOR(reg(EDI),reg(EBX));

  I_XOR(reg(EBX),regmem(EAX,0));
  I_XOR(reg(EBX),regmem(ECX,0));
  I_XOR(reg(EBX),regmem(EDX,0));
  I_XOR(reg(EBX),regmem(EBX,0));
  I_XOR(reg(EBX),regmem(ESP,0));
  I_XOR(reg(EBX),regmem(EBP,0));
  I_XOR(reg(EBX),regmem(ESI,0));
  I_XOR(reg(EBX),regmem(EDI,0));

  I_XOR(reg(EBX),regmem(EAX,12));
  I_XOR(reg(EBX),regmem(ECX,12));
  I_XOR(reg(EBX),regmem(EDX,12));
  I_XOR(reg(EBX),regmem(EBX,12));
  I_XOR(reg(EBX),regmem(ESP,12));
  I_XOR(reg(EBX),regmem(EBP,12));
  I_XOR(reg(EBX),regmem(ESI,12));
  I_XOR(reg(EBX),regmem(EDI,12));

  I_XOR(reg(EBX),regmem(EAX,123456789));
  I_XOR(reg(EBX),regmem(ECX,123456789));
  I_XOR(reg(EBX),regmem(EDX,123456789));
  I_XOR(reg(EBX),regmem(EBX,123456789));
  I_XOR(reg(EBX),regmem(ESP,123456789));
  I_XOR(reg(EBX),regmem(EBP,123456789));
  I_XOR(reg(EBX),regmem(ESI,123456789));
  I_XOR(reg(EBX),regmem(EDI,123456789));

  I_XOR(reg(EAX),regmem(EBX,0));
  I_XOR(reg(ECX),regmem(EBX,0));
  I_XOR(reg(EDX),regmem(EBX,0));
  I_XOR(reg(EBX),regmem(EBX,0));
  I_XOR(reg(ESP),regmem(EBX,0));
  I_XOR(reg(EBP),regmem(EBX,0));
  I_XOR(reg(ESI),regmem(EBX,0));
  I_XOR(reg(EDI),regmem(EBX,0));

  I_XOR(reg(EAX),regmem(EBX,12));
  I_XOR(reg(ECX),regmem(EBX,12));
  I_XOR(reg(EDX),regmem(EBX,12));
  I_XOR(reg(EBX),regmem(EBX,12));
  I_XOR(reg(ESP),regmem(EBX,12));
  I_XOR(reg(EBP),regmem(EBX,12));
  I_XOR(reg(ESI),regmem(EBX,12));
  I_XOR(reg(EDI),regmem(EBX,12));

  I_XOR(reg(EAX),regmem(EBX,123456789));
  I_XOR(reg(ECX),regmem(EBX,123456789));
  I_XOR(reg(EDX),regmem(EBX,123456789));
  I_XOR(reg(EBX),regmem(EBX,123456789));
  I_XOR(reg(ESP),regmem(EBX,123456789));
  I_XOR(reg(EBP),regmem(EBX,123456789));
  I_XOR(reg(ESI),regmem(EBX,123456789));
  I_XOR(reg(EDI),regmem(EBX,123456789));

  I_XOR(reg(EAX),absmem(12));
  I_XOR(reg(ECX),absmem(12));
  I_XOR(reg(EDX),absmem(12));
  I_XOR(reg(EBX),absmem(12));
  I_XOR(reg(ESP),absmem(12));
  I_XOR(reg(EBP),absmem(12));
  I_XOR(reg(ESI),absmem(12));
  I_XOR(reg(EDI),absmem(12));

  I_XOR(reg(EAX),absmem(123456789));
  I_XOR(reg(ECX),absmem(123456789));
  I_XOR(reg(EDX),absmem(123456789));
  I_XOR(reg(EBX),absmem(123456789));
  I_XOR(reg(ESP),absmem(123456789));
  I_XOR(reg(EBP),absmem(123456789));
  I_XOR(reg(ESI),absmem(123456789));
  I_XOR(reg(EDI),absmem(123456789));

  I_XOR(regmem(EAX,0),imm(99));
  I_XOR(regmem(ECX,0),imm(99));
  I_XOR(regmem(EDX,0),imm(99));
  I_XOR(regmem(EBX,0),imm(99));
  I_XOR(regmem(ESP,0),imm(99));
  I_XOR(regmem(EBP,0),imm(99));
  I_XOR(regmem(ESI,0),imm(99));
  I_XOR(regmem(EDI,0),imm(99));

  I_XOR(regmem(EAX,12),imm(99));
  I_XOR(regmem(ECX,12),imm(99));
  I_XOR(regmem(EDX,12),imm(99));
  I_XOR(regmem(EBX,12),imm(99));
  I_XOR(regmem(ESP,12),imm(99));
  I_XOR(regmem(EBP,12),imm(99));
  I_XOR(regmem(ESI,12),imm(99));
  I_XOR(regmem(EDI,12),imm(99));

  I_XOR(regmem(EAX,123456789),imm(99));
  I_XOR(regmem(ECX,123456789),imm(99));
  I_XOR(regmem(EDX,123456789),imm(99));
  I_XOR(regmem(EBX,123456789),imm(99));
  I_XOR(regmem(ESP,123456789),imm(99));
  I_XOR(regmem(EBP,123456789),imm(99));
  I_XOR(regmem(ESI,123456789),imm(99));
  I_XOR(regmem(EDI,123456789),imm(99));

  I_XOR(absmem(12),imm(99));
  I_XOR(absmem(123456789),imm(99));

  I_NOT(reg(EAX));
  I_NOT(reg(ECX));
  I_NOT(reg(EDX));
  I_NOT(reg(EBX));
  I_NOT(reg(ESP));
  I_NOT(reg(EBP));
  I_NOT(reg(ESI));
  I_NOT(reg(EDI));

  I_NOT(regmem(EAX,0));
  I_NOT(regmem(ECX,0));
  I_NOT(regmem(EDX,0));
  I_NOT(regmem(EBX,0));
  I_NOT(regmem(ESP,0));
  I_NOT(regmem(EBP,0));
  I_NOT(regmem(ESI,0));
  I_NOT(regmem(EDI,0));

  I_NOT(regmem(EAX,12));
  I_NOT(regmem(ECX,12));
  I_NOT(regmem(EDX,12));
  I_NOT(regmem(EBX,12));
  I_NOT(regmem(ESP,12));
  I_NOT(regmem(EBP,12));
  I_NOT(regmem(ESI,12));
  I_NOT(regmem(EDI,12));

  I_NOT(regmem(EAX,123456789));
  I_NOT(regmem(ECX,123456789));
  I_NOT(regmem(EDX,123456789));
  I_NOT(regmem(EBX,123456789));
  I_NOT(regmem(ESP,123456789));
  I_NOT(regmem(EBP,123456789));
  I_NOT(regmem(ESI,123456789));
  I_NOT(regmem(EDI,123456789));

  I_NOT(absmem(12));
  I_NOT(absmem(123456789));

  I_MOV(reg(ECX),regmemscaled(EBX,12,SCALE_1,EAX));
  I_MOV(reg(ECX),regmemscaled(EBX,12,SCALE_1,ECX));
  I_MOV(reg(ECX),regmemscaled(EBX,12,SCALE_1,EDX));
  I_MOV(reg(ECX),regmemscaled(EBX,12,SCALE_1,EBX));
  I_MOV(reg(ECX),regmemscaled(EBX,12,SCALE_1,EBP));
  I_MOV(reg(ECX),regmemscaled(EBX,12,SCALE_1,ESI));
  I_MOV(reg(ECX),regmemscaled(EBX,12,SCALE_1,EDI));

  I_MOV(reg(ECX),regmemscaled(EAX,12,SCALE_1,EBX));
  I_MOV(reg(ECX),regmemscaled(ECX,12,SCALE_1,EBX));
  I_MOV(reg(ECX),regmemscaled(EDX,12,SCALE_1,EBX));
  I_MOV(reg(ECX),regmemscaled(EBX,12,SCALE_1,EBX));
  I_MOV(reg(ECX),regmemscaled(ESP,12,SCALE_1,EBX));
  I_MOV(reg(ECX),regmemscaled(EBP,12,SCALE_1,EBX));
  I_MOV(reg(ECX),regmemscaled(ESI,12,SCALE_1,EBX));
  I_MOV(reg(ECX),regmemscaled(EDI,12,SCALE_1,EBX));

  I_MOV(reg(ECX),regmemscaled(EBX,12,SCALE_2,EAX));
  I_MOV(reg(ECX),regmemscaled(EBX,12,SCALE_2,ECX));
  I_MOV(reg(ECX),regmemscaled(EBX,12,SCALE_2,EDX));
  I_MOV(reg(ECX),regmemscaled(EBX,12,SCALE_2,EBX));
  I_MOV(reg(ECX),regmemscaled(EBX,12,SCALE_2,EBP));
  I_MOV(reg(ECX),regmemscaled(EBX,12,SCALE_2,ESI));
  I_MOV(reg(ECX),regmemscaled(EBX,12,SCALE_2,EDI));

  I_MOV(reg(ECX),regmemscaled(EAX,12,SCALE_2,EBX));
  I_MOV(reg(ECX),regmemscaled(ECX,12,SCALE_2,EBX));
  I_MOV(reg(ECX),regmemscaled(EDX,12,SCALE_2,EBX));
  I_MOV(reg(ECX),regmemscaled(EBX,12,SCALE_2,EBX));
  I_MOV(reg(ECX),regmemscaled(ESP,12,SCALE_2,EBX));
  I_MOV(reg(ECX),regmemscaled(EBP,12,SCALE_2,EBX));
  I_MOV(reg(ECX),regmemscaled(ESI,12,SCALE_2,EBX));
  I_MOV(reg(ECX),regmemscaled(EDI,12,SCALE_2,EBX));

  I_MOV(reg(ECX),regmemscaled(EBX,12,SCALE_4,EAX));
  I_MOV(reg(ECX),regmemscaled(EBX,12,SCALE_4,ECX));
  I_MOV(reg(ECX),regmemscaled(EBX,12,SCALE_4,EDX));
  I_MOV(reg(ECX),regmemscaled(EBX,12,SCALE_4,EBX));
  I_MOV(reg(ECX),regmemscaled(EBX,12,SCALE_4,EBP));
  I_MOV(reg(ECX),regmemscaled(EBX,12,SCALE_4,ESI));
  I_MOV(reg(ECX),regmemscaled(EBX,12,SCALE_4,EDI));

  I_MOV(reg(ECX),regmemscaled(EAX,12,SCALE_4,EBX));
  I_MOV(reg(ECX),regmemscaled(ECX,12,SCALE_4,EBX));
  I_MOV(reg(ECX),regmemscaled(EDX,12,SCALE_4,EBX));
  I_MOV(reg(ECX),regmemscaled(EBX,12,SCALE_4,EBX));
  I_MOV(reg(ECX),regmemscaled(ESP,12,SCALE_4,EBX));
  I_MOV(reg(ECX),regmemscaled(EBP,12,SCALE_4,EBX));
  I_MOV(reg(ECX),regmemscaled(ESI,12,SCALE_4,EBX));
  I_MOV(reg(ECX),regmemscaled(EDI,12,SCALE_4,EBX));

  I_MOV(reg(ECX),regmemscaled(EBX,12,SCALE_8,EAX));
  I_MOV(reg(ECX),regmemscaled(EBX,12,SCALE_8,ECX));
  I_MOV(reg(ECX),regmemscaled(EBX,12,SCALE_8,EDX));
  I_MOV(reg(ECX),regmemscaled(EBX,12,SCALE_8,EBX));
  I_MOV(reg(ECX),regmemscaled(EBX,12,SCALE_8,EBP));
  I_MOV(reg(ECX),regmemscaled(EBX,12,SCALE_8,ESI));
  I_MOV(reg(ECX),regmemscaled(EBX,12,SCALE_8,EDI));

  I_MOV(reg(ECX),regmemscaled(EAX,12,SCALE_8,EBX));
  I_MOV(reg(ECX),regmemscaled(ECX,12,SCALE_8,EBX));
  I_MOV(reg(ECX),regmemscaled(EDX,12,SCALE_8,EBX));
  I_MOV(reg(ECX),regmemscaled(EBX,12,SCALE_8,EBX));
  I_MOV(reg(ECX),regmemscaled(ESP,12,SCALE_8,EBX));
  I_MOV(reg(ECX),regmemscaled(EBP,12,SCALE_8,EBX));
  I_MOV(reg(ECX),regmemscaled(ESI,12,SCALE_8,EBX));
  I_MOV(reg(ECX),regmemscaled(EDI,12,SCALE_8,EBX));

  I_IMUL(reg(EAX),imm(12));
  I_IMUL(reg(ECX),imm(12));
  I_IMUL(reg(EDX),imm(12));
  I_IMUL(reg(EBX),imm(12));
  I_IMUL(reg(ESP),imm(12));
  I_IMUL(reg(EBP),imm(12));
  I_IMUL(reg(ESI),imm(12));
  I_IMUL(reg(EDI),imm(12));

  I_IMUL(reg(EAX),imm(123456789));
  I_IMUL(reg(ECX),imm(123456789));
  I_IMUL(reg(EDX),imm(123456789));
  I_IMUL(reg(EBX),imm(123456789));
  I_IMUL(reg(ESP),imm(123456789));
  I_IMUL(reg(EBP),imm(123456789));
  I_IMUL(reg(ESI),imm(123456789));
  I_IMUL(reg(EDI),imm(123456789));

  I_IMUL(reg(EBX),reg(EAX));
  I_IMUL(reg(EBX),reg(ECX));
  I_IMUL(reg(EBX),reg(EDX));
  I_IMUL(reg(EBX),reg(EBX));
  I_IMUL(reg(EBX),reg(ESP));
  I_IMUL(reg(EBX),reg(EBP));
  I_IMUL(reg(EBX),reg(ESI));
  I_IMUL(reg(EBX),reg(EDI));

  I_IMUL(reg(EAX),reg(EBX));
  I_IMUL(reg(ECX),reg(EBX));
  I_IMUL(reg(EDX),reg(EBX));
  I_IMUL(reg(EBX),reg(EBX));
  I_IMUL(reg(ESP),reg(EBX));
  I_IMUL(reg(EBP),reg(EBX));
  I_IMUL(reg(ESI),reg(EBX));
  I_IMUL(reg(EDI),reg(EBX));

  I_IMUL(reg(EBX),regmem(EAX,12));
  I_IMUL(reg(EBX),regmem(ECX,12));
  I_IMUL(reg(EBX),regmem(EDX,12));
  I_IMUL(reg(EBX),regmem(EBX,12));
  I_IMUL(reg(EBX),regmem(ESP,12));
  I_IMUL(reg(EBX),regmem(EBP,12));
  I_IMUL(reg(EBX),regmem(ESI,12));
  I_IMUL(reg(EBX),regmem(EDI,12));

  I_IMUL(reg(EAX),regmem(EBX,12));
  I_IMUL(reg(ECX),regmem(EBX,12));
  I_IMUL(reg(EDX),regmem(EBX,12));
  I_IMUL(reg(EBX),regmem(EBX,12));
  I_IMUL(reg(ESP),regmem(EBX,12));
  I_IMUL(reg(EBP),regmem(EBX,12));
  I_IMUL(reg(ESI),regmem(EBX,12));
  I_IMUL(reg(EDI),regmem(EBX,12));

  I_IMUL(reg(EAX),absmem(12));
  I_IMUL(reg(ECX),absmem(12));
  I_IMUL(reg(EDX),absmem(12));
  I_IMUL(reg(EBX),absmem(12));
  I_IMUL(reg(ESP),absmem(12));
  I_IMUL(reg(EBP),absmem(12));
  I_IMUL(reg(ESI),absmem(12));
  I_IMUL(reg(EDI),absmem(12));

  I_IMUL(reg(EAX),absmem(123456789));
  I_IMUL(reg(ECX),absmem(123456789));
  I_IMUL(reg(EDX),absmem(123456789));
  I_IMUL(reg(EBX),absmem(123456789));
  I_IMUL(reg(ESP),absmem(123456789));
  I_IMUL(reg(EBP),absmem(123456789));
  I_IMUL(reg(ESI),absmem(123456789));
  I_IMUL(reg(EDI),absmem(123456789));

  x86_assemble(as,cpucode);
  x86_assembly_free(as);
}

int main()
{
  array *cpucode = array_new();
  setbuf(stdout,NULL);
  testasm(cpucode);
  write_asm_raw("compiled.s",cpucode);
/*   printf("Write compiled.s\n"); */
  array_free(cpucode);
  return 0;
}
