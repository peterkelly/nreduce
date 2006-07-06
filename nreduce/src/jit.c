#include "grammar.tab.h"
#include "gcode.h"
#include "nreduce.h"
#include "assembler.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <assert.h>
#include <ctype.h>
#include <stdarg.h>
#include <math.h>

const char *reg_names[8] = { "EAX", "ECX", "EDX", "EBX", "ESP", "EBP", "ESI", "EDI" };

void print_cell(cell *c)
{
/*   printf("print_cell\n"); */
/*   printf("print_cell: c = %p\n",c); */
/*   printf("print_cell: celltype(c) = %d\n",celltype(c)); */
/*   printf("print_cell: c->field1 = %p\n",c->field1); */
/*   printf("print_cell: c->field2 = %p\n",c->field2); */
/*   print_code(c); */
/*   printf("\nprint_cell done\n"); */
  c = resolve_ind(c);

  if (trace)
    debug(0,"<============ ");

  if (TYPE_AREF == celltype(c)) {
    print_code(c);
    printf("\n");
    return;
  }

  assert(isvalue(c));
  switch (celltype(c)) {
  case TYPE_NIL:
    break;
  case TYPE_INT:
    printf("%d",(int)c->field1);
    break;
  case TYPE_DOUBLE:
    printf("%f",celldouble(c));
    break;
  case TYPE_STRING:
    printf("%s",(char*)c->field1);
    break;
  }
  if (trace)
    debug(0,"\n");
}

void print_reg(x86_assembly *as, char *msg, int r)
{
  I_PUSHAD();
  I_PUSH(imm((int)msg));
  I_MOV(reg(EAX),imm((int)printf));
  I_CALL(reg(EAX));
  I_ADD(reg(ESP),imm(4));
  I_POPAD();

  I_PUSHAD();
  I_PUSH(reg(r));
  I_PUSH(imm((int)reg_names[r]));
  I_PUSH(imm((int)" %s = 0x%x\n"));
  I_MOV(reg(EAX),imm((int)printf));
  I_CALL(reg(EAX));
  I_ADD(reg(ESP),imm(12));
  I_POPAD();
}

void print_msg(x86_assembly *as, char *format)
{
  I_PUSHAD();
  I_PUSH(imm((int)format));
  I_MOV(reg(EAX),imm((int)printf));
  I_CALL(reg(EAX));
  I_ADD(reg(ESP),imm(4));
  I_POPAD();
}

void print_reg_code(x86_assembly *as, char *msg, int r)
{
  print_msg(as,msg);

  I_PUSHAD();
  I_PUSH(reg(r));
  I_MOV(reg(EAX),imm((int)print_code));
  I_CALL(reg(EAX));
  I_ADD(reg(ESP),imm(4));
  I_POPAD();

  print_msg(as,"\n");
}

int resolve_ind_offset = -1;

void compile_resolve_ind(x86_assembly *as)
{
  int Lcheckind = as->labels++;
  int Ltrue = as->labels++;
  int Lfalse = as->labels++;

  LABEL(Lcheckind);
  I_MOV(reg(EAX),regmem(EBX,CELL_TAG));
  I_AND(reg(EAX),imm(TAG_MASK));
  I_CMP(reg(EAX),imm(TYPE_IND));

  I_JZ(label(Ltrue));
  I_JMP(label(Lfalse));

  LABEL(Ltrue);
  I_MOV(reg(EBX),regmem(EBX,CELL_FIELD1));

  I_JMP(label(Lcheckind));

  LABEL(Lfalse);

  I_RET();
}

ginstr *trace_program = NULL;
char *trace_stackbase = NULL;
void trace_instr(int instrno, char *stacktop)
{
  int stacksize = (trace_stackbase-stacktop)/4;
/*   printf("ginstr %d, stack size = %d\n",instrno,stacksize); */
  print_ginstr(instrno,trace_program+instrno,0);
  print_stack(-1,(cell**)stacktop,stacksize,1);
}

void compile_trace(x86_assembly *as, int instrno)
{
  I_PUSHAD();
  I_MOV(reg(EAX),reg(ESP));
  I_ADD(reg(EAX),imm(8*4));
  I_PUSH(reg(EAX));
  I_PUSH(imm(instrno));
  I_MOV(reg(EAX),imm((int)trace_instr));
  I_CALL(reg(EAX));
  I_ADD(reg(ESP),imm(8));
  I_POPAD();
}

char *run_bif(int bif, char *esp)
{
  int nargs = builtin_info[bif].nargs;
  int i;
  int oldcount;
  char *newesp;
  assert(0 == stackcount);

  if (trace)
    debug(0,"  builtin %s\n",builtin_info[bif].name);

  for (i = nargs-1; i >= 0; i--) {
    cell **cp = (cell**)(esp+i*4);
    push(resolve_ind(*cp));
  }
  oldcount = stackcount;
  builtin_info[bif].f();
  newesp = esp + (oldcount-stackcount)*4;
  for (i = 0; i < stackcount; i++) {
    cell **cp = (cell**)(newesp+i*4);
    *cp = stack[i];
  }
  stackcount = 0;
  return newesp;
}

char *code_start = NULL;

void jit_compile(ginstr *program, array *cpucode)
{
  ginstr *instr = NULL;
  x86_assembly *as = x86_assembly_new();
  int Lstart = as->labels++;
  int nginstrs = 0;
  int Lresolveind = as->labels++;
  int resolve_ind_stmtno = -1;
  trace_program = program;

  I_PUSH(reg(EBP));
  I_MOV(reg(EBP),reg(ESP));
  I_MOV(absmem((int)&trace_stackbase),reg(EBP));
  I_MOV(absmem((int)&collect_ebp),reg(EBP));
  I_JMP(label(Lstart));
  LABEL(Lresolveind);
  resolve_ind_stmtno = as->count;
  compile_resolve_ind(as);
  LABEL(Lstart);

  instr = &program[0];
  while (OP_LAST != instr->opcode) {
    nginstrs++;
    instr->label = as->labels++;
    instr++;
  }

  instr = &program[0];
  while (OP_LAST != instr->opcode) {
    int instrno = instr-program;
    int Levalend = -1;
    int Laftereval = -1;
    int Ljunwend = -1;

    LABEL(instr->label);

    instr->stmtno = as->count;

    if (trace)
      compile_trace(as,instr-program);

    switch (instr->opcode) {
    case OP_BEGIN:
      I_NOP();
      break;
    case OP_END:
      print_msg(as,"\n");
      I_PUSH(imm(0));
      I_MOV(reg(EAX),imm((int)exit));
      I_CALL(reg(EAX));
      break;
    case OP_GLOBSTART:
      I_NOP();
      break;
    case OP_EVAL: {
      int Ldoeval = as->labels++;
      Laftereval = as->labels++;

      I_MOV(reg(EBX),regmem(ESP,instr->arg0*4));
      I_CALL(label(Lresolveind));

      I_MOV(reg(EAX),regmem(EBX,CELL_TAG));
      I_AND(reg(EAX),imm(TAG_MASK));
      I_CMP(reg(EAX),imm(TYPE_APPLICATION));     /*  if ((TYPE_APPLICATION == celltype(top())) || */
      I_JZ(label(Ldoeval));
      I_CMP(reg(EAX),imm(TYPE_FUNCTION));        /*      (TYPE_FUNCTION == celltype(top()))) { */
      I_JZ(label(Ldoeval));
      I_JMP(label(Laftereval));

      LABEL(Ldoeval);
      I_MOV(reg(EAX),imm((int)pushdump));        /*    dumpentry *de = pushdump(); */
      I_CALL(reg(EAX));
      I_MOV(reg(EBX),reg(EAX));
      I_MOV(regmem(EBX,DE_STACKBASE),reg(EBP));  /*    de->stackbase = stackbase; */
      I_MOV(regmem(EBX,DE_STACKCOUNT),reg(ESP)); /*    de->stackcount = stackcount; */
      I_MOV(reg(EAX),absmem((int)&code_start));  /*    de->address = address; */
      I_ADD(reg(EAX),imm(Laftereval));
      as->instructions[as->count-1].isgetnextaddr = 1;
      I_MOV(regmem(EBX,DE_ADDRESS),reg(EAX));
      I_PUSH(regmem(ESP,instr->arg0*4));         /*    push(stack[stackcount-1]); */
      I_MOV(reg(EBP),reg(ESP));                  /*    stackbase = stackcount-1; */
                                                 /*  } */
                                                 /*  else { */
                                                 /*    break; */
                                                 /*  } */
      /* fall through */
    }
    case OP_UNWIND: {
      int Ljdontcol = as->labels++;
      int Lalstart = as->labels++;
      int Lalend = as->labels++;
      int Ljwhnf = as->labels++;
      int Ladjstart = as->labels++;
      int Ladjend = as->labels++;
      int Lnotfun = as->labels++;
      Ljunwend = as->labels++;

      I_MOV(reg(EAX),absmem((int)&nallocs));    /* if (nallocs > COLLECT_THRESHOLD) */
      I_CMP(reg(EAX),imm(COLLECT_THRESHOLD));
      I_JLE(label(Ljdontcol));
      I_MOV(absmem((int)&collect_esp),reg(ESP));
      I_PUSHAD();
      I_MOV(reg(EAX),imm((int)collect));        /*   collect(); */
      I_CALL(reg(EAX));
      I_POPAD();
      LABEL(Ljdontcol);

      /* main part of UNWIND */

      I_MOV(reg(ESP),reg(EBP));                    /* ESP = EBP; */
      I_MOV(reg(EBX),regmem(ESP,0));               /* EBX = stack[ESP]; */
      I_CALL(label(Lresolveind));                  /* EBX = resolve_ind(EBX); */

      LABEL(Lalstart);

      I_MOV(reg(EAX),regmem(EBX,CELL_TAG));        /* while (TYPE_APPLICATION == celltype(EBX)) { */
      I_AND(reg(EAX),imm(TAG_MASK));
      I_CMP(reg(EAX),imm(TYPE_APPLICATION));
      I_JNE(label(Lalend));

      I_MOV(reg(EBX),regmem(EBX,CELL_FIELD1));     /*   EBX = (cell*)EBX->field1; */
      I_CALL(label(Lresolveind));                  /*   EBX = resolve_ind(EBX); */
      I_PUSH(reg(EBX));                            /*   push(EBX); */
      I_JMP(label(Lalstart));                      /* } */
      LABEL(Lalend);

      I_MOV(reg(EAX),regmem(EBX,CELL_TAG));        /* if (TYPE_FUNCTION == celltype(EBX)) { */
      I_AND(reg(EAX),imm(TAG_MASK));
      I_CMP(reg(EAX),imm(TYPE_FUNCTION));
      I_JNE(label(Lnotfun));

      I_MOV(reg(ESI),reg(ESP));                    /*   int ESI = ESP+(int)EBX->field1; */
      I_MOV(reg(EAX),regmem(EBX,CELL_FIELD1));
      I_IMUL(reg(EAX),imm(4));
      I_ADD(reg(ESI),reg(EAX));

      I_CMP(reg(ESI),reg(EBP));                    /*   if (ESI <= EBP) { */
      I_JG(label(Lnotfun));

      I_MOV(reg(EDI),regmem(EBX,CELL_FIELD2));     /*     int EDI = (int)EBX->field2; */
      I_MOV(reg(EDX),reg(ESP));                    /*     EDX = ESP; */

      LABEL(Ladjstart);

      I_CMP(reg(EDX),reg(ESI));                    /*     while (EDX < ESI) { */
      I_JGE(label(Ladjend));
      I_MOV(reg(EBX),regmem(EDX,4));               /*       EBX = stack[EDX+1]; */
      I_CALL(label(Lresolveind));                  /*       EBX = resolve_ind(EBX); */
      I_MOV(reg(EBX),regmem(EBX,CELL_FIELD2));     /*       EBX = (cell*)EBX->field2; */
      I_CALL(label(Lresolveind));                  /*       EBX = resolve_ind(EBX); */
      I_MOV(reg(EAX),reg(EBX));                    /*       stack[EDX] = EBX */

      I_MOV(regmem(EDX,0),reg(EAX));
      I_ADD(reg(EDX),imm(4));                      /*       EDX++; */
      I_JMP(label(Ladjstart));
      LABEL(Ladjend);                              /*     } */

      I_MOV(reg(EBX),regmem(EDX,0));               /*     EBX = stack[EDX]; */
      I_CALL(label(Lresolveind));                  /*     EBX = resolve_ind(EBX); */
      I_MOV(regmem(EDX,0),reg(EBX));               /*     stack[EDX] = EBX; */

      I_IMUL(reg(EDI),imm(sizeof(ginstr)));        /*     address = EDI; */
      I_ADD(reg(EDI),imm((int)program));
      I_MOV(reg(EDI),regmem(EDI,GINSTR_CODEADDR));
      I_JMP(reg(EDI));
                                                   /*     break; */
                                                   /*   } */
      LABEL(Lnotfun);                              /* } */

      LABEL(Ljwhnf);                               /* else { */

      /* fall through */
    }
    case OP_RETURN:

      I_MOV(reg(EAX),absmem((int)&dumpstack));     /* edx = &dumpstack[dumpcount-1]; */
      I_MOV(reg(EDX),absmem((int)&dumpcount));
      I_SUB(reg(EDX),imm(1));
      I_IMUL(reg(EDX),imm(sizeof(dumpentry)));
      I_ADD(reg(EDX),absmem((int)&dumpstack));
      I_MOV(reg(EBP),regmem(EDX,DE_STACKBASE));    /* stackbase = edx->stackbase; */
      I_MOV(reg(ESP),regmem(EDX,DE_STACKCOUNT));   /* stackcount = edx->stackcount; */
      I_MOV(reg(ESI),regmem(EDX,DE_ADDRESS));      /* address = edx->address; */
      I_SUB(absmem((int)&dumpcount),imm(1));
      I_MOV(reg(EBX),regmem(ESP,0));               /* ebx = stack[stackcount-1]; */
      I_CALL(label(Lresolveind));                  /* ebx = resolve_ind(ebx); */

      if (trace)
        print_reg(as,"unwind WHNF: jumping",ESI);
      I_JMP(reg(ESI));

      if (0 <= Ljunwend)
        LABEL(Ljunwend);

      if (0 <= Laftereval)
        LABEL(Laftereval);

      if (0 <= Levalend)
        LABEL(Levalend);

      break;
    case OP_PUSH:
      I_MOV(reg(EBX),regmem(ESP,instr->arg0*4));     /* res = stack[stackcount-1-instr->arg0]; */
      I_CALL(label(Lresolveind));                    /* res = resolve_ind(res); */
      I_PUSH(reg(EBX));
      break;
    case OP_PUSHGLOBAL:
      // FIXME: out of date
      I_MOV(reg(ECX),imm((int)instr->arg0));         /* src = (cell*)instr->arg0; */

      I_MOV(reg(EAX),imm((int)alloc_cell));          /* c = alloc_cell(); */
      I_CALL(reg(EAX));
      I_MOV(reg(EBX),reg(EAX));

      I_MOV(regmem(EBX,CELL_TAG),imm(TYPE_FUNCTION));/* c->tag = TYPE_FUNCTION; */
      I_MOV(reg(EAX),regmem(ECX,CELL_FIELD1));       /* c->field2 = src->field1; */
      I_MOV(regmem(EBX,CELL_FIELD1),reg(EAX));
      I_MOV(reg(EAX),regmem(ECX,CELL_FIELD2));       /* c->field2 = src->field2; */
      I_MOV(regmem(EBX,CELL_FIELD2),reg(EAX));

      I_PUSH(reg(EBX));                              /* push(c); */
      break;
    case OP_MKAP: {

      int Lloop = as->labels++;
      int Ldone = as->labels++;

      I_MOV(reg(ECX),imm(instr->arg0-1));

      LABEL(Lloop);
      I_CMP(reg(ECX),imm(0));
      I_JL(label(Ldone));
      I_SUB(reg(ECX),imm(1));

      I_MOV(reg(EAX),imm((int)alloc_cell));          /* cell *c = alloc_cell();        [EAX=c] */
      I_PUSH(reg(ECX));
      I_CALL(reg(EAX));
      I_POP(reg(ECX));
      I_MOV(regmem(EAX,CELL_TAG),imm(TYPE_APPLICATION));/* c->tag = TYPE_APPLICATION;          */
      I_MOV(reg(EBX),regmem(ESP,0));                 /* c->field1 = stack[stackcount-1];       */
      I_MOV(regmem(EAX,CELL_FIELD1),reg(EBX));
      I_MOV(reg(EBX),regmem(ESP,4));                 /* c->field2 = stack[stackcount-2];       */
      I_MOV(regmem(EAX,CELL_FIELD2),reg(EBX));
      I_ADD(reg(ESP),imm(4));                        /* stackcount--;                          */
      I_MOV(regmem(ESP,0),reg(EAX));                 /* stack[stackcount-1] = c;               */

      I_JMP(label(Lloop));

      LABEL(Ldone);

      break;
    }
    case OP_UPDATE: {
      I_MOV(reg(EBX),regmem(ESP,instr->arg0*4));     /* res = stack[stackcount-1-instr->arg0]; */
      I_CALL(label(Lresolveind));                    /* res = resolve_ind(res); */

      I_PUSH(reg(EBX));                              /* free_cell_fields(res); */
      I_MOV(reg(EAX),imm((int)free_cell_fields));
      I_ADD(reg(ESP),imm(4));

      I_MOV(reg(ECX),reg(EBX));                      /* target = res; */
      I_MOV(regmem(ECX,CELL_TAG),imm(TYPE_IND));     /* target->tag = TYPE_IND; */

      I_MOV(reg(EBX),regmem(ESP,0));                 /* res = stack[stackcount-1]; */
      I_CALL(label(Lresolveind));                    /* res = resolve_ind(res); */
      I_MOV(regmem(ECX,CELL_FIELD1),reg(EBX));       /* target->field1 = res; */

      I_ADD(reg(ESP),imm(4));                        /* stackcount--; */
      break;
    }
    case OP_POP:
      I_ADD(reg(ESP),imm(instr->arg0*4));
      break;
    case OP_PRINT: {

      I_MOV(reg(EBX),regmem(ESP,0));
      I_CALL(label(Lresolveind));

      I_MOV(reg(EAX),imm((int)print_cell));
      I_CALL(reg(EAX));
      I_ADD(reg(ESP),imm(4));
      break;
    }
    case OP_BIF:
      I_PUSH(reg(ESP));
      I_PUSH(imm(instr->arg0));
      I_MOV(reg(EAX),imm((int)run_bif));
      I_CALL(reg(EAX));
      I_MOV(reg(ESP),reg(EAX));
      break;
    case OP_JFALSE: {

      I_MOV(reg(EBX),regmem(ESP,0));
      I_ADD(reg(ESP),imm(4));

      I_CALL(label(Lresolveind));
      I_MOV(reg(EAX),regmem(EBX,CELL_TAG));
      I_AND(reg(EAX),imm(TAG_MASK));
      I_CMP(reg(EAX),imm(TYPE_NIL));

      assert(0 <= instrno+instr->arg0);
      assert(instrno+instr->arg0 < nginstrs);
      I_JZ(label(program[instrno+instr->arg0].label));
      break;
    }
    case OP_JUMP:
      assert(0 <= instrno+instr->arg0);
      assert(instrno+instr->arg0 < nginstrs);
      I_JMP(label(program[instrno+instr->arg0].label));
      break;
    case OP_JEMPTY:
      I_CMP(reg(ESP),reg(EBP));
      assert(0 <= instrno+instr->arg0);
      assert(instrno+instr->arg0 < nginstrs);
      I_JZ(label(program[instrno+instr->arg0].label));
      break;
    case OP_ISTYPE: {
      int Ltrue = as->labels++;
      int Lend = as->labels++;

      I_MOV(reg(EBX),regmem(ESP,0));            /* c = stack[stackcount-1]; */
      I_CALL(label(Lresolveind));               /* c = resolve_ind(c); */

      I_MOV(reg(EAX),regmem(EBX,0));            /* type = c->tag; */
      I_AND(reg(EAX),imm(TAG_MASK));            /* type &= TAG_MASK; */
      I_CMP(reg(EAX),imm(instr->arg0));         /* if (type == instr->arg0) */
      I_JZ(label(Ltrue));

      /* false branch */
      I_MOV(regmem(ESP,0),imm((int)globnil));
      I_JMP(label(Lend));

      /* true branch */
      LABEL(Ltrue);
      I_MOV(regmem(ESP,0),imm((int)globtrue));

      /* end */
      LABEL(Lend);
      break;
    }
    case OP_ANNOTATE:
      /* FIXME */
      I_NOP();
      break;
    case OP_ALLOC: {
      int Lloopstart = as->labels++;
      int Lend = as->labels++;

      I_MOV(reg(ECX),imm(0));                           /* i = 0; */

      LABEL(Lloopstart);                                /* while (i < instr->arg0) { */
      I_CMP(reg(ECX),imm(instr->arg0));
      I_JGE(label(Lend));

      /* loop body */
      I_PUSH(reg(ECX));                                 /*   c = alloc_cell(); */
      I_MOV(reg(EAX),imm((int)alloc_cell));
      I_CALL(reg(EAX));
      I_POP(reg(ECX));

      I_MOV(regmem(EAX,CELL_TAG),imm(TYPE_NIL));        /*   c->tag = TYPE_NIL; */
      I_PUSH(reg(EAX));                                 /*   push(c); */

      I_ADD(reg(ECX),imm(1));                           /*   i++; */
      I_JMP(label(Lloopstart));                         /* } */

      /* end of loop */
      LABEL(Lend);
      break;
    }
    default:
      assert(0);
      break;
    }

    instr++;
  }

  I_MOV(reg(ESP),reg(EBP));
  I_POP(reg(EBP));
  I_RET();

  x86_assemble(as,cpucode);

  for (instr = &program[0]; OP_LAST != instr->opcode; instr++)
    instr->codeaddr = as->instructions[instr->stmtno].addr;

  resolve_ind_offset = as->instructions[resolve_ind_stmtno].addr;

  x86_assembly_free(as);
}

