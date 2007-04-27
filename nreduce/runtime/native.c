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
 * $Id: assembler.c 340 2006-10-01 07:05:53Z pmkelly $
 *
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#define _GNU_SOURCE /* for ucontext_t registers and feenableexcept() */

#include "src/nreduce.h"
#include "compiler/source.h"
#include "compiler/bytecode.h"
#include "runtime.h"
#include "assembler.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <assert.h>
#include <ctype.h>
#include <stdarg.h>
#include <math.h>
#include <signal.h>
#include <ucontext.h>
#include <execinfo.h>
#include <unistd.h>
#include <fenv.h>

#define NATIVE_BEGIN
#define NATIVE_END
#define NATIVE_GLOBSTART
#define NATIVE_SPARK
#define NATIVE_RETURN
#define NATIVE_DO
#define NATIVE_JFUN
#define NATIVE_JFALSE
#define NATIVE_JUMP
#define NATIVE_PUSH
#define NATIVE_UPDATE
#define NATIVE_ALLOC
#define NATIVE_SQUEEZE
#define NATIVE_MKCAP
#define NATIVE_MKFRAME
#define NATIVE_BIF
#define NATIVE_PUSHNIL
#define NATIVE_PUSHNUMBER
#define NATIVE_PUSHSTRING
#define NATIVE_POP
#define NATIVE_ERROR
#define NATIVE_EVAL
#define NATIVE_CALL
#define NATIVE_JCMP

typedef void (op_fun)(task *tsk, frame *runnable, const instruction *instr);

/* functions in interpreter.c */
void op_begin(task *tsk, frame *runnable, const instruction *instr);
void op_end(task *tsk, frame *runnable, const instruction *instr);
void op_globstart(task *tsk, frame *runnable, const instruction *instr);
void op_spark(task *tsk, frame *runnable, const instruction *instr);
void op_eval(task *tsk, frame *runnable, const instruction *instr);
void op_return(task *tsk, frame *runnable, const instruction *instr);
void op_do(task *tsk, frame *runnable, const instruction *instr);
void op_jfun(task *tsk, frame *runnable, const instruction *instr);
void op_jfalse(task *tsk, frame *runnable, const instruction *instr);
void op_jump(task *tsk, frame *runnable, const instruction *instr);
void op_push(task *tsk, frame *runnable, const instruction *instr);
void op_update(task *tsk, frame *runnable, const instruction *instr);
void op_alloc(task *tsk, frame *runnable, const instruction *instr);
void op_squeeze(task *tsk, frame *runnable, const instruction *instr);
void op_mkcap(task *tsk, frame *runnable, const instruction *instr);
void op_mkframe(task *tsk, frame *runnable, const instruction *instr);
void op_bif(task *tsk, frame *runnable, const instruction *instr);
void op_pushnil(task *tsk, frame *runnable, const instruction *instr);
void op_pushnumber(task *tsk, frame *runnable, const instruction *instr);
void op_pushstring(task *tsk, frame *runnable, const instruction *instr);
void op_pop(task *tsk, frame *runnable, const instruction *instr);
void op_error(task *tsk, frame *runnable, const instruction *instr);
void op_call(task *tsk, frame *runnable, const instruction *instr);
void op_jcmp(task *tsk, frame *runnable, const instruction *instr);
void op_invalid(task *tsk, frame *runnable, const instruction *instr);
int handle_interrupt(task *tsk);

op_fun *op_handlers[OP_COUNT] = {
  op_begin,
  op_end,
  op_globstart,
  op_spark,
  op_return,
  op_do,
  op_jfun,
  op_jfalse,
  op_jump,
  op_push,
  op_update,
  op_alloc,
  op_squeeze,
  op_mkcap,
  op_mkframe,
  op_bif,
  op_pushnil,
  op_pushnumber,
  op_pushstring,
  op_pop,
  op_error,
  op_eval,
  op_call,
  op_jcmp,
  op_invalid,
};

#define PUSHAD_OFFSET_EAX 4
#define PUSHAD_OFFSET_ECX 8
#define PUSHAD_OFFSET_EDX 12
#define PUSHAD_OFFSET_EBX 16
#define PUSHAD_OFFSET_ESP 20
#define PUSHAD_OFFSET_EBP 24
#define PUSHAD_OFFSET_ESI 28
#define PUSHAD_OFFSET_EDI 32

#define BEGIN_CALL {                         \
  int Ltemp = as->labels++;                  \
  int Ltempend = as->labels++;               \
  I_CALL(label(Ltemp));                      \
  I_JMP(label(Ltempend));                    \
  LABEL(Ltemp);                              \
  I_PUSHAD();

#define END_CALL                             \
  I_POPAD();                                 \
  I_RET();                                   \
  LABEL(Ltempend); }

#define PRINT_VALUE(str,val) \
      BEGIN_CALL; \
      I_PUSH(val); \
      I_PUSH(imm((int)str));                 \
      I_MOV(reg(EAX),imm((int)print_value)); \
      I_CALL(reg(EAX)); \
      I_ADD(reg(ESP),imm(8)); \
      END_CALL;

#define PRINT_VALUE_NOCALL(str,val) \
      I_PUSHAD();                       \
      I_PUSH(val); \
      I_PUSH(imm((int)str));                 \
      I_MOV(reg(EAX),imm((int)print_value)); \
      I_CALL(reg(EAX)); \
      I_ADD(reg(ESP),imm(8)); \
      I_POPAD();

#define PRINT_MESSAGE(str) \
      BEGIN_CALL; \
      I_PUSH(imm((int)str));                 \
      I_MOV(reg(EAX),imm((int)print_message)); \
      I_CALL(reg(EAX)); \
      I_ADD(reg(ESP),imm(4)); \
      END_CALL;

#define PRINT_MESSAGE_NOCALL(str) \
      I_PUSHAD();                                 \
      I_PUSH(imm((int)str));                 \
      I_MOV(reg(EAX),imm((int)print_message)); \
      I_CALL(reg(EAX)); \
      I_ADD(reg(ESP),imm(4)); \
      I_POPAD();

#define CELL_FLAGS ((int)&((cell*)0)->flags)
#define CELL_TYPE ((int)&((cell*)0)->type)
#define CELL_FIELD1 ((int)&((cell*)0)->field1)
#define CELL_FIELD2 ((int)&((cell*)0)->field2)

#define field(struct,member) ((int)&((struct*)0)->member)
#define obj_field(reg,struct,member) regmem(reg,field(struct,member))
#define set_field_value(reg,struct,member,value) I_MOV(obj_field(reg,struct,member),value)

#define FRAME_WAITGLO ((int)&((frame*)0)->waitglo)
#define FRAME_WAITLNK ((int)&((frame*)0)->waitlnk)
#define FRAME_WQ ((int)&((frame*)0)->wq)
#define FRAME_FREELNK ((int)&((frame*)0)->freelnk)

#define FRAME_DATA ((int)&((frame*)0)->data[0])
#define FRAME_C ((int)&((frame*)0)->c)
#define FRAME_RETP ((int)&((frame*)0)->retp)
#define FRAME_INSTR ((int)&((frame*)0)->instr)
#define FRAME_RNEXT ((int)&((frame*)0)->rnext)
#define FRAME_STATE ((int)&((frame*)0)->state)
#define FRAME_RESUME ((int)&((frame*)0)->resume)
#define ARRAY_DATA ((int)&((array*)0)->data)
#define TASK_GLOBNILPNTR ((int)&((task*)0)->globnilpntr)
#define TASK_STRINGS ((int)&((task*)0)->strings)
#define TASK_CODE ((int)&((task*)0)->code)
#define TASK_FREEFRAME ((int)&((task*)0)->freeframe)
#define WAITQUEUE_FRAMES ((int)&((waitqueue*)0)->frames)
#define WAITQUEUE_FETCHERS ((int)&((waitqueue*)0)->fetchers)

void print_value(const char *str, void *p)
{
  printf("%s = %p\n",str,p);
}

void print_message(const char *str)
{
  printf("%s",str);
}

void dump_asm(task *tsk, const char *filename, array *cpucode)
{
  const instruction *program_ops = bc_instructions(tsk->bcdata);
  bcheader *bch = (bcheader*)tsk->bcdata;
  FILE *f;
  int i;
  int bcaddr = bch->nops-1;

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
    char x;
    if (bcaddr != tsk->cpu_to_bcaddr[i]) {
      bcaddr = tsk->cpu_to_bcaddr[i];
      fprintf(f,"bytecode%d_%s:\n",bcaddr,opcodes[program_ops[bcaddr].opcode]);
    }
    else if (i == tsk->interrupt_addr-tsk->code) {
      fprintf(f,"interrupt:\n");
    }
    else if (i == tsk->caperror_addr-tsk->code) {
      fprintf(f,"caperror:\n");
    }
    else if (i == tsk->argerror_addr-tsk->code) {
      fprintf(f,"argerror:\n");
    }

    x = ((char*)cpucode->data)[i] & 0xFF;
    fprintf(f,"\t.byte\t%d\n",x);
  }
  fclose(f);
}

void print_eip_bytecode(task *tsk, void *eip, char *str)
{
  int eip_addr = eip-tsk->code;
  const instruction *program_ops = bc_instructions(tsk->bcdata);
  if ((0 <= eip_addr) && (tsk->codesize > eip_addr)) {
    int addr = tsk->cpu_to_bcaddr[eip_addr];
    const instruction *instr = &program_ops[addr];
    if (OP_BIF == instr->opcode)
      snprintf(str,100,"%-6d %-12s",addr,builtin_info[instr->arg0].name);
    else
      snprintf(str,100,"%-6d %-12s",addr,opcodes[instr->opcode]);
  }
  else if (tsk->interrupt_addr == eip) {
    snprintf(str,100,"(interrupt)        ");
  }
  else {
    snprintf(str,100,"???????????????????");
  }
}

void function_below(char *name)
{
  int explicit_interrupt = 0;
  int pos;
  int skiplines;
  int fds[2];
  int start;
  char buf[1025];
  int r;

  /* Get a backtrace of the current call stack. Do it via a file descriptor to avoid calling
     malloc(), which the thread might be in at the moment */
  if (0 > pipe(fds)) {
    perror("pipe");
    exit(1);
  }
  void *btarray[10];
  int size = backtrace(btarray,10);
  backtrace_symbols_fd(btarray,size,fds[1]);
  r = read(fds[0],buf,1024);
  if (0 > r) {
    perror("read");
    exit(1);
  }
  buf[r] = '\0';
/*   printf("%s",buf); */
  close(fds[1]);
  close(fds[0]);

  if (strstr(buf,"endpoint_interrupt"))
    explicit_interrupt = 1;

  /* Skip first three lines on stack trace; four if we're in a call to endpoint_interrupt */
  pos = 0;
  skiplines = (explicit_interrupt ? 4 : 3);
  while ((pos < r) && (0 < skiplines)) {
    if ('\n' == buf[pos])
      skiplines--;
    pos++;
  }

  /* Skip to function name */
  while ((pos < r) && ('(' != buf[pos]))
    pos++;
  if (pos < r)
    pos++;
  start = pos;

  /* Terminate the string at the end of the current function name */
  while ((pos < r) && ('+' != buf[pos]))
    pos++;
  buf[pos] = '\0';

  /* Return the function name */
  if (explicit_interrupt)
    snprintf(name,1024,"EI/%s",&buf[start]);
  else
    snprintf(name,1024,"%s",&buf[start]);
}

void print_instruction(task *tsk, int ebp, int addr, int opcode)
{
  printf("%4d %-20s\n",addr,opcodes[opcode]);
}

/*
 * SIGUSR1 is sent to the thread when there is some exceptional condition that needs to be handled,
 * e.g. the allocation threshold has been reached (meaning GC is needed), or a message has arrived.
 * By handling it this way we avoid needing to poll explicitly for these events, which would slow
 * down execution. The signal is sent by endpoint_interrupt().
 *
 * We cannot handle the event directly within the signal handler because the program could be
 * part way through some important operation and the task may be in an inconsistent state. Instead,
 * we set a breakpoint in the native code that will occur at the end of the current bytecode
 * instruction, guaranteeing that the program state is consistent. When this happens, a SIGTRAP
 * will be sent to the thread, and the appropriate action cab be taken then.
 *
 * The breakpoint is set by replacing a single byte at the start of the instruction with 0xCC,
 * which corresponts to an INT 3. This can thus be used regardless of how many bytes the
 * instruction we want to break at uses. Note that this is the same technique as used by gdb,
 * which makes debugging the generated code difficult (but not impossible).
 */
void native_sigusr1(int sig, siginfo_t *ino, void *uc1)
{
  ucontext_t *uc = (ucontext_t*)uc1;
  task *tsk = pthread_getspecific(task_key);
  int stackoffset = tsk->normal_esp-(void*)uc->uc_mcontext.gregs[REG_ESP];
  const instruction *program_ops = bc_instructions(tsk->bcdata);
  void **peip;
  int offset;
  int bcaddr;

  assert(tsk);
  assert(pthread_self() == tsk->endpt->thread);
  tsk->interrupt_again = 0;

  if (tsk->trap_pending || tsk->native_finished)
    return;

  if (((void*)uc->uc_mcontext.gregs[REG_EIP] >= tsk->interrupt_addr) &&
      ((void*)uc->uc_mcontext.gregs[REG_EIP] < tsk->interrupt_end)) {
    /* At very first instruction of interrupt handler, before 0 has been pushed onto stack */
    tsk->interrupt_again = 1;
    return;
  }

  /* There are three possibilities for what the task could be doing right before the call.

    If it was executing the generated code for a bytecode insturction, then ESP will be equal to
    tsk->normal_esp (since the generated code doesn't change ESP except for when it makes a call
    to C code). */
  if (0 == stackoffset) { /* Native code */
    peip = (void**)&uc->uc_mcontext.gregs[REG_EIP];
  }
  else {

    /* If the task was executing C code, then the ESP value will be different, and the first 32
       bit value after tsk->normal_esp will contain the saved EIP that the code will jump back to
       after the call; this can be used to determine which bytecode instruction the code is
       executiong. */
    if (0 != *((void**)(tsk->normal_esp-4))) { /* C function */
      peip = (void**)(tsk->normal_esp-4);
    }

    /* If the ESP is not equal to tsk->normal_esp, but the 32 bit value at that spot is 0, this
       means we are in the interrupt handler. This case is simple; we simply need to set the
       interrupt_again flag so that handle_interrupt() will be called again. */
    else { /* Interrupt handler */
      tsk->interrupt_again = 1;
      return;
    }
  }

  /* At this point, *peip is the machine address that the native code is currently at. We need
     to figure out which bytecode instruction this corresponds to */

  assert(*peip >= tsk->code);
  assert(*peip < tsk->bcend_addr);

  offset = *peip-tsk->code;
  assert((0 <= offset) && (tsk->codesize > offset));
  bcaddr = tsk->cpu_to_bcaddr[offset];
  assert(OP_INVALID != program_ops[bcaddr].opcode);

  /* Set the breakpoint at the appropriate place for the current bytecode instruction. In the case
     of straight-line code, this will be the first machine instruction that comes after the end of
     the current bytecode instruction. In the case of native code that contains jumps, there may
     be two places we need to set the breakpoint. */
  assert(tsk->bpaddrs1[bcaddr]);
  if (tsk->bpaddrs1[bcaddr]) {
    assert(0 == tsk->bcold1);
    tsk->bcold1 = *tsk->bpaddrs1[bcaddr];
    *tsk->bpaddrs1[bcaddr] = 0xCC;
  }

  if (tsk->bpaddrs2[bcaddr]) {
    assert(0 == tsk->bcold2);
    tsk->bcold2 = *tsk->bpaddrs2[bcaddr];
    *tsk->bpaddrs2[bcaddr] = 0xCC;
  }

  tsk->trap_pending = 1;
  tsk->interrupt_bcaddr = bcaddr;
}

/*
 * Received when we hit a breakpoint that was previously set by native_sigusr1(). When this is
 * called, the native code is at a "safe" point at which we can execute handle_interrupt().
 *
 * Rather than executing it directly here though, we modify the return instruction pointer so that
 * when the thread exits the signal handler it will jump to the assembler code generated for the
 * interrupt handler. We save the old value of the EIP register in tsk->interrupt_return_eip, so
 * that the thread can jump back to the original position after the interrupt has been dealt with.
 *
 * FIXME: should we block SIGUSR1 when inside native_sigtrap() and native_sigfpe()?
 */
void native_sigtrap(int sig, siginfo_t *ino, void *uc1)
{
  ucontext_t *uc = (ucontext_t*)uc1;
  task *tsk = pthread_getspecific(task_key);
  bcheader *bch = (bcheader*)tsk->bcdata;
  const instruction *program_ops = bc_instructions(tsk->bcdata);
  int bcaddr;
  frame *curf = (void*)uc->uc_mcontext.gregs[REG_EBP];
  assert((void*)1 == *tsk->runptr);

  /* Check that we're actually expecting this signal, and that we were invoked from generated code
     (i.e. not from some other C function) */
  assert(tsk->trap_pending);
  assert(tsk->normal_esp == (void*)uc->uc_mcontext.gregs[REG_ESP]);

  bcaddr = tsk->interrupt_bcaddr;
  uc->uc_mcontext.gregs[REG_EIP]--;
  assert(((void*)uc->uc_mcontext.gregs[REG_EIP] == tsk->bpaddrs1[bcaddr]) ||
         ((void*)uc->uc_mcontext.gregs[REG_EIP] == tsk->bpaddrs2[bcaddr]));
  assert(uc->uc_mcontext.gregs[REG_EIP] >= (int)tsk->instraddrs[bcaddr]);
  assert((bcaddr == bch->nops-1) ||
         (uc->uc_mcontext.gregs[REG_EIP] <= (int)tsk->instraddrs[bcaddr+1]));

  /* Clear the breakpoint(s), by restoring the previous values that were at the start of the
     relevant machine instrucitons. These were temporarily set to 0xCC in native_sigusr1() */
  assert(tsk->bpaddrs1[bcaddr]);
  if (tsk->bpaddrs1[bcaddr]) {
    assert(0xCC == *tsk->bpaddrs1[bcaddr]);
    *tsk->bpaddrs1[bcaddr] = tsk->bcold1;
    tsk->bcold1 = 0;
  }

  if (tsk->bpaddrs2[bcaddr]) {
    assert(0xCC == *tsk->bpaddrs2[bcaddr]);
    *tsk->bpaddrs2[bcaddr] = tsk->bcold2;
    tsk->bcold2 = 0;
  }

  tsk->trap_pending = 0;

  /* Swap out - set the current frame's instruction pointer to the correct value according to
     the bytecode address that the native code is executing */
  if (curf)
    curf->instr = &program_ops[bcaddr+1];

  /* Save the return address and get the thread to jump to the interrupt/error handler upon return
     from this function. */
  tsk->interrupt_return_eip = (void*)uc->uc_mcontext.gregs[REG_EIP];
  uc->uc_mcontext.gregs[REG_EIP] = (int)tsk->interrupt_addr;
  tsk->interrupt_bcaddr = 0;
}

void native_sigfpe(int sig, siginfo_t *ino, void *uc1)
{
  ucontext_t *uc = (ucontext_t*)uc1;
  void *eip;
  int offset;
  int bcaddr;
  task *tsk = pthread_getspecific(task_key);
  const instruction *program_ops = bc_instructions(tsk->bcdata);
  int stackoffset = tsk->normal_esp-(void*)uc->uc_mcontext.gregs[REG_ESP];
  const instruction *instr;
  frame *curf;

  if ((void*)1 == *tsk->runptr)
    *tsk->runptr = (void*)uc->uc_mcontext.gregs[REG_EBP];
  curf = *tsk->runptr;

  if (0 == stackoffset) { /* Native code */
    eip = (void*)uc->uc_mcontext.gregs[REG_EIP];
  }
  else { /* C function */
    eip = *(void**)(tsk->normal_esp-4);
    assert(0 != eip); /* should not be in interrupt handler */
  }
  assert(eip >= tsk->code);
  assert(eip < tsk->bcend_addr);

  offset = eip-tsk->code;
  assert((0 <= offset) && (tsk->codesize > offset));
  bcaddr = tsk->cpu_to_bcaddr[offset];
  assert(OP_INVALID != program_ops[bcaddr].opcode);
  instr = &program_ops[bcaddr];

  curf->instr = instr;

  if ((OP_BIF == instr->opcode) || (OP_JCMP == instr->opcode)) {
    int bif = (OP_BIF == instr->opcode) ? instr->arg0 : instr->arg1;
    int set = 0;
    if ((0 == stackoffset) && (2 == builtin_info[bif].nargs)) {
      pntr *argstack = &curf->data[instr->expcount-2];
      if ((CELL_NUMBER != pntrtype(argstack[1])) || (CELL_NUMBER != pntrtype(argstack[0]))) {
        invalid_binary_args(tsk,argstack,bif);
        set = 1;
      }
    }
    else if ((0 == stackoffset) && (1 == builtin_info[bif].nargs)) {
      if (CELL_NUMBER != pntrtype(curf->data[instr->expcount-1])) {
        invalid_arg(tsk,curf->data[instr->expcount-1],bif,0,CELL_NUMBER);
        set = 1;
      }
    }
    if (!set)
      set_error(tsk,"%s: floating point exception",builtin_info[bif].name);
  }
  else {
    set_error(tsk,"floating point exception");
  }

  uc->uc_mcontext.gregs[REG_ESP] = (int)tsk->normal_esp;
  uc->uc_mcontext.gregs[REG_EIP] = (int)tsk->argerror_addr;
  tsk->native_finished = 1;
}

void native_sigsegv(int sig, siginfo_t *ino, void *uc1)
{
  ucontext_t *uc = (ucontext_t*)uc1;
  task *tsk = pthread_getspecific(task_key);
  printf("SIGSEGV: eip = %p, rel eip = 0x%x, ebp = %p\n",
         (void*)uc->uc_mcontext.gregs[REG_EIP],
         (void*)uc->uc_mcontext.gregs[REG_EIP]-tsk->code,
         (void*)uc->uc_mcontext.gregs[REG_EBP]);
  exit(1);
}

void native_arg_error(task *tsk)
{
  if (tsk->error) {
    print_task_sourceloc(tsk,stderr,tsk->errorsl);
    fprintf(stderr,"%s\n",tsk->error);
  }
  else {
    fprintf(stderr,"unknown error\n");
  }
}

void native_cap_error(task *tsk, pntr cappntr)
{
  cap_error(tsk,cappntr);
  handle_error(tsk);
}

void native_assert_equal(task *tsk, const char *str, void *a, void *b)
{
/*   printf("%s: a = %p, b = %p\n",str,a,b); */
  if (a != b)
    fatal("%s: a = %p, b = %p",str,a,b);
}

void asm_assert_equal(task *tsk, x86_assembly *as, const char *str, x86_arg a, x86_arg b)
{
#if 0
  BEGIN_CALL;
  I_PUSH(b);
  I_PUSH(a);
  I_PUSH(imm((int)str));
  I_PUSH(imm((int)tsk));
  I_MOV(reg(EAX),imm((int)native_assert_equal));
  I_CALL(reg(EAX));
  I_ADD(reg(ESP),imm(16));
  END_CALL;
#endif
}

void runnable_owner_task(task *tsk, x86_assembly *as)
{
  asm_assert_equal(tsk,as,"runnable_owner_task",absmem((int)tsk->runptr),imm(1));
  I_MOV(absmem((int)tsk->runptr),reg(EBP));
  I_MOV(reg(EBP),imm(1));
}

void runnable_owner_native_code(task *tsk, x86_assembly *as)
{
  asm_assert_equal(tsk,as,"runnable_owner_native_code",reg(EBP),imm(1));
  I_MOV(reg(EBP),absmem((int)tsk->runptr));
  I_MOV(absmem((int)tsk->runptr),imm(1));
}

/* Update the native code EIP for the current runnable list head */
void swap_in(task *tsk, x86_assembly *as, int lab)
{
  const instruction *program_ops = bc_instructions(tsk->bcdata);
  int Lnotnull = as->labels++;
  int Ljump = as->labels++;

  int Lcheck = as->labels++;
  LABEL(Lcheck);
  /* Compute the address of the next instruction */

  I_CMP(reg(EBP),imm(0));
  I_JNE(label(Lnotnull));

  /* If we get here, we should have had an interrupt, and the jump target should be a breakpoint */
  I_MOV(reg(EAX),label(Lcheck));
  I_ADD(reg(EAX),absmem((int)&tsk->code));
  I_JMP(label(Ljump));

  LABEL(Lnotnull);

  I_MOV(reg(EAX),regmem(EBP,FRAME_INSTR)); // get current instruction
  I_MOV(regmem(EBP,FRAME_INSTR),imm(0)); // set frame's instruction to 0
  I_SUB(reg(EAX),imm((int)program_ops)); // compute position of instr relative to start

  I_MOV(reg(EDX),imm(0));
  I_MOV(reg(ECX),imm(sizeof(instruction)));
  I_IDIV(reg(ECX)); // compute bytecode address

  I_MOV(reg(EBX),imm((int)tsk->instraddrs));
  I_MOV(reg(EAX),regmemscaled(EBX,0,SCALE_4,EAX));

  /* Jump to the next instruction */
  if (lab)
    LABEL(lab);
  LABEL(Ljump);
  I_JMP(reg(EAX));
}

void asm_jump_if_number(task *tsk, x86_assembly *as, int pos, int lab)
{
  I_MOV(reg(EAX),regmem(EBP,FRAME_DATA+8*pos+4));
  I_AND(reg(EAX),imm(PNTR_MASK));
  I_CMP(reg(EAX),imm(PNTR_VALUE));
  I_JNE(label(lab));
}

void asm_check_runnable(task *tsk, x86_assembly *as)
{
  int Lno = as->labels++;
  // if (NULL == *tsk->runptr)
  //  endpoint_interrupt(tsk->endpt);
  asm_assert_equal(tsk,as,"asm_check_runnable",absmem((int)tsk->runptr),imm(1));

  I_CMP(reg(EBP),imm(0));
  I_JNE(label(Lno));

  BEGIN_CALL;
  I_PUSH(imm((int)tsk->endpt));
  I_MOV(reg(EAX),imm((int)endpoint_interrupt));
  I_CALL(reg(EAX));
  I_ADD(reg(ESP),imm(4));
  END_CALL;

  LABEL(Lno);
}

void asm_check_collect_needed(task *tsk, x86_assembly *as)
{
  // if ((tsk->alloc_bytes >= COLLECT_THRESHOLD) && tsk->endpt && tsk->endpt->interruptptr) {
  //   endpoint_interrupt(tsk->endpt);
  // }
  int Laftermemcheck = as->labels++;
  I_CMP(absmem((int)&tsk->alloc_bytes),imm(COLLECT_THRESHOLD));
  I_JL(label(Laftermemcheck));

  BEGIN_CALL;
  I_PUSH(imm((int)tsk->endpt));
  I_MOV(reg(EAX),imm((int)endpoint_interrupt));
  I_CALL(reg(EAX));
  I_ADD(reg(ESP),imm(4));
  END_CALL;

  LABEL(Laftermemcheck);
}

/*
 * Resolve the pointer at the specified position in the stack. After this code has run, the stack
 * position will contain either a number, or pointer to the actual cell (i.e. one with type not
 * equal to IND)
 *
 * After execution of this code, EBX is set to the type, and (if it's not a number), EAX points to
 * the cell
 *
 * EBP == current frame
 */
void asm_resolve_stack_pntr(task *tsk, x86_assembly *as, int pos)
{
  int Ldone = as->labels++;
  int Lwhile = as->labels++;
  int Lisnumber = as->labels++;

  /* Check if the value is a cell pointer or number */
  LABEL(Lwhile);
  {
    I_MOV(reg(EAX),regmem(EBP,FRAME_DATA+8*pos+4));
    I_AND(reg(EAX),imm(PNTR_MASK));
    I_CMP(reg(EAX),imm(PNTR_VALUE));
    I_JNE(label(Lisnumber));

    /* Check if the cell type is an IND */
    I_MOV(reg(EAX),regmem(EBP,FRAME_DATA+8*pos)); // EAX now points to the cell
    I_MOV(reg(EBX),regmem(EAX,CELL_TYPE));
    I_CMP(reg(EBX),imm(CELL_IND));
    I_JNE(label(Ldone));

    /* Replace the stack entry with the target of the pointer and loop again */
    {
      I_MOV(reg(ECX),regmem(EAX,CELL_FIELD1));
      I_MOV(reg(EDX),regmem(EAX,CELL_FIELD1+4));
      I_MOV(regmem(EBP,FRAME_DATA+8*pos),reg(ECX));
      I_MOV(regmem(EBP,FRAME_DATA+8*pos+4),reg(EDX));
      I_JMP(label(Lwhile));
    }
  }

  LABEL(Lisnumber);
  I_MOV(reg(EBX),imm(CELL_NUMBER));

  LABEL(Ldone);
}

/*
 * Native code implementation of frame_new(). This uses the first item from the linked list of
 * free frames if possible, but falls back to the C version there are no free frames and another
 * block needs to be allocated.
 *
 * After this code runs, EDI contains the address of the new frame
 */
void asm_frame_new(task *tsk, x86_assembly *as, int addalloc, int state)
{
  int Lhavefree = as->labels++;
  int Lframeallocated = as->labels++;

  if (addalloc) {
    // tsk->alloc_bytes += tsk->framesize+sizeof(cell);
    I_ADD(absmem((int)&tsk->alloc_bytes),imm(tsk->framesize+sizeof(cell)));
    asm_check_collect_needed(tsk,as);
  }

  // f = tsk->freeframe;
  I_MOV(reg(EDI),absmem((int)&tsk->freeframe));

  // if (NULL == f)
  I_CMP(reg(EDI),imm(0));
  I_JNE(label(Lhavefree));
  {
    /* Fall back to C version */
    BEGIN_CALL;
    I_PUSH(imm(addalloc));
    I_PUSH(imm((int)tsk));
    I_MOV(reg(EAX),imm((int)frame_new));
    I_CALL(reg(EAX));
    I_ADD(reg(ESP),imm(8));
    I_MOV(regmem(ESP,32-PUSHAD_OFFSET_EDI),reg(EAX));
    END_CALL;
    I_MOV(regmem(EDI,FRAME_STATE),imm(state));
    I_JMP(label(Lframeallocated));
  }

  LABEL(Lhavefree);
  {
    /* There is a frame available on the free list - use this */

    // tsk->freeframe = f->freelnk;
    I_MOV(reg(EAX),regmem(EDI,FRAME_FREELNK));
    I_MOV(absmem((int)&tsk->freeframe),reg(EAX));

    // f->c - to be initialised by caller
    // f->instr - to be initialised by caller
    // f->retp - to be initialised by caller

    // f->state = 0;
    // f->resume = 0;
    // f->freelnk = 0;
    I_MOV(regmem(EDI,FRAME_STATE),imm(state));
    I_MOV(regmem(EDI,FRAME_RESUME),imm(0));
    I_MOV(regmem(EDI,FRAME_FREELNK),imm(0));
  }
  LABEL(Lframeallocated);
}

/*
 * Initialise the stack of another frame with values from the current one. This is used when
 * EVALing or CALLing a frame. The op n values are copied from the current frame's stack. The
 * same order is maintained.
 *
 * EDI == new frame
 * EBP == current frame
 */
void asm_copy_stack(task *tsk, x86_assembly *as, int expcount, int fno, int n)
{
  int i;
  int nfc = 0;

  for (i = expcount-n; i < expcount; i++) {
    I_MOV(reg(EAX),regmem(EBP,FRAME_DATA+8*i));
    I_MOV(reg(EBX),regmem(EBP,FRAME_DATA+8*i+4));
    I_MOV(regmem(EDI,FRAME_DATA+8*nfc),reg(EAX));
    I_MOV(regmem(EDI,FRAME_DATA+8*nfc+4),reg(EBX));
    nfc++;
  }
}

void native_compile(char *bcdata, int bcsize, array *cpucode, task *tsk)
{
  x86_assembly *as = x86_assembly_new();
  const instruction *program_ops = bc_instructions(bcdata);
  const instruction *instr;
  int addr;
  bcheader *bch = (bcheader*)bcdata;
  int *tempaddrs = (int*)calloc(bch->nops,sizeof(int)); // FIXME: free these
  int Lfinished = as->labels++;
  int Linterrupt = as->labels++;
  int Lcaperror = as->labels++;
  int Largerror = as->labels++;
  int Lbcend = as->labels++;
  int *instrlabels;
  int i;
  tsk->bplabels1 = (int*)calloc(bch->nops,sizeof(int)); // FIXME: free these
  tsk->bplabels2 = (int*)calloc(bch->nops,sizeof(int)); // FIXME: free these
  tsk->instraddrs = (void**)calloc(bch->nops,sizeof(int));
  tsk->bpaddrs1 = (unsigned char**)calloc(bch->nops,sizeof(int));
  tsk->bpaddrs2 = (unsigned char**)calloc(bch->nops,sizeof(int));

  instrlabels = (int*)malloc(bch->nops*sizeof(int));
  for (i = 0; i < bch->nops; i++)
    instrlabels[i] = as->labels++;

  for (addr = 0; addr < bch->nops; addr++) {
    tsk->bplabels1[addr] = as->labels++;
    tsk->bplabels2[addr] = as->labels++;
  }

  I_PUSHAD();
  I_MOV(absmem((int)&tsk->normal_esp),reg(ESP));

  /* basically runnable_owner_native_code but without the assert */
  I_MOV(reg(EBP),absmem((int)tsk->runptr));
  I_MOV(absmem((int)tsk->runptr),imm(1));

  swap_in(tsk,as,0); /* initialize EBP and jump to first instruction */

  // EBP: always points to current frame

  for (addr = 0; addr < bch->nops; addr++) {
    instr = &program_ops[addr];

    tempaddrs[addr] = as->count;
    LABEL(instrlabels[addr]);

    if (getenv("TRACE")) {
      /* Print out message for this instruction */
      BEGIN_CALL;
      I_PUSH(imm(instr->opcode));
      I_PUSH(imm(instr-program_ops));
      I_PUSH(reg(EBP));
      I_PUSH(imm((int)tsk));
      I_MOV(reg(EAX),imm((int)print_instruction));
      I_CALL(reg(EAX));
      I_ADD(reg(ESP),imm(16));
      END_CALL;
    }

    switch (instr->opcode) {
    case OP_END:
      I_MOV(absmem((int)&tsk->native_finished),imm(1));
      LABEL(tsk->bplabels1[addr]);
      I_JMP(label(Lfinished));
      break;
    #ifdef NATIVE_BEGIN
    case OP_BEGIN:
      /* NOOP */
      LABEL(tsk->bplabels1[addr]);
      break;
    #endif
    #ifdef NATIVE_GLOBSTART
    case OP_GLOBSTART:
      /* NOOP */
      LABEL(tsk->bplabels1[addr]);
      break;
    #endif
    #ifdef NATIVE_SPARK
    case OP_SPARK: {
      int Ldone = as->labels++;
      int pos = instr->arg0;

      // RUNNABLE->data[INSTR->arg0] = resolve_pntr(RUNNABLE->data[INSTR->arg0]);
      asm_resolve_stack_pntr(tsk,as,pos);
      // now EBX == type, EAX == cell pointer (if not number)
      I_CMP(reg(EBX),imm(CELL_FRAME));
      I_JNE(label(Ldone));

      /* If we get here, the cell is a frame. Move the frame pointer to EAX. */
      I_MOV(reg(EAX),regmem(EAX,CELL_FIELD1));

      /* Check the frame state and change it to SPARKED if necessary */
      I_MOV(reg(EBX),regmem(EAX,FRAME_STATE));
      I_CMP(reg(EBX),imm(STATE_NEW));
      I_JNE(label(Ldone));
      I_MOV(regmem(EAX,FRAME_STATE),imm(STATE_SPARKED));
      LABEL(Ldone);
      LABEL(tsk->bplabels1[addr]);
      break;
    }
    #endif
    #ifdef NATIVE_RETURN
    case OP_RETURN: {
      int Lvalueset = as->labels++;
      int Lretp = as->labels++;

      I_MOV(reg(ECX),regmem(EBP,FRAME_C));
      I_CMP(reg(ECX),imm(0));
      I_JZ(label(Lretp));

      /* Have cell - change it to an IND reference to the actual result */

      // curf->c->type = CELL_IND;
      I_MOV(regmem(ECX,CELL_TYPE),imm(CELL_IND));

      // curf->c->field1 = val;
      I_MOV(reg(EAX),regmem(EBP,FRAME_DATA+8*(instr->expcount-1)));
      I_MOV(reg(EBX),regmem(EBP,FRAME_DATA+8*(instr->expcount-1)+4));
      I_MOV(regmem(ECX,CELL_FIELD1),reg(EAX));
      I_MOV(regmem(ECX,CELL_FIELD1+4),reg(EBX));

      // curf->c = NULL;
      I_MOV(regmem(EBP,FRAME_C),imm(0));
      I_JMP(label(Lvalueset));

      LABEL(Lretp);

      /* Don't have cell - set the appropriate stack entry in the calling frame */
      I_MOV(reg(ECX),regmem(EBP,FRAME_RETP));

      // *(curf->retp) = val;
      I_MOV(reg(EAX),regmem(EBP,FRAME_DATA+8*(instr->expcount-1)));
      I_MOV(reg(EBX),regmem(EBP,FRAME_DATA+8*(instr->expcount-1)+4));
      I_MOV(regmem(ECX,0),reg(EAX));
      I_MOV(regmem(ECX,4),reg(EBX));

      // curf->retp = NULL;
      I_MOV(regmem(EBP,FRAME_RETP),imm(0));

      LABEL(Lvalueset);

      /* Remove frame from run queue and put it back on the free list */

      I_MOV(reg(EDI),reg(EBP));

      // done_frame()
      {
        // *tsk->runptr = (_f)->rnext;
        I_MOV(reg(EAX),regmem(EBP,FRAME_RNEXT));
        I_MOV(reg(EBP),reg(EAX));

        // (_f)->rnext = NULL;
        I_MOV(regmem(EDI,FRAME_RNEXT),imm(0));

        // (_f)->state = STATE_DONE;
        I_MOV(regmem(EDI,FRAME_STATE),imm(STATE_DONE));
      }

      // resume_waiters()
      {
        // EDI: curf

        // while (wq->frames) {
        //   frame *f = wq->frames;
        //   wq->frames = f->waitlnk;
        //   f->waitglo = NULL;
        //   f->waitlnk = NULL;
        //   unblock_frame(tsk,f);
        // }

        int Lwhile = as->labels++;
        int Ldone = as->labels++;
        LABEL(Lwhile);
        {
          I_MOV(reg(EBX),regmem(EDI,FRAME_WQ+WAITQUEUE_FRAMES));
          I_CMP(reg(EBX),imm(0));
          I_JZ(label(Ldone));

          I_MOV(reg(EAX),regmem(EBX,FRAME_WAITLNK));
          I_MOV(regmem(EDI,FRAME_WQ+WAITQUEUE_FRAMES),reg(EAX));
          I_MOV(regmem(EBX,FRAME_WAITGLO),imm(0));
          I_MOV(regmem(EBX,FRAME_WAITLNK),imm(0));

          // f->rnext = *tsk->runptr;
          I_MOV(regmem(EBX,FRAME_RNEXT),reg(EBP));
          // *tsk->runptr = f;
          I_MOV(reg(EBP),reg(EBX));
          // f->state = STATE_RUNNING;
          I_MOV(regmem(EBX,FRAME_STATE),imm(STATE_RUNNING));

          I_JMP(label(Lwhile));
        }

        LABEL(Ldone);

        /* FIXME: resume fetchers */
      }

      // check_runnable()
      asm_check_runnable(tsk,as);


      // frame_free()
      {
        int Ldonefetchers = as->labels++;
        // if (_f->wq.fetchers)
        //   list_free(_f->wq.fetchers,free);
        I_CMP(regmem(EDI,FRAME_WQ+WAITQUEUE_FETCHERS),imm(0));
        I_JZ(label(Ldonefetchers));

        BEGIN_CALL;
        I_PUSH(imm((int)free));
        I_PUSH(regmem(EDI,FRAME_WQ+WAITQUEUE_FETCHERS));
        I_MOV(reg(EAX),imm((int)list_free)); /* FIXME: test this code branch (parallel only) */
        I_CALL(reg(EAX));
        I_ADD(reg(ESP),imm(8));
        END_CALL;

        LABEL(Ldonefetchers);
        // _f->freelnk = tsk->freeframe;
        I_MOV(reg(EAX),absmem((int)&tsk->freeframe));
        I_MOV(regmem(EDI,FRAME_FREELNK),reg(EAX));

        // tsk->freeframe = _f;
        I_MOV(absmem((int)&tsk->freeframe),reg(EDI));
      }

      swap_in(tsk,as,tsk->bplabels1[addr]);
      break;
    }
    #endif
    #ifdef NATIVE_DO
/*     case OP_DO: */
    #endif
    #ifdef NATIVE_JFUN
    case OP_JFUN: {
      const funinfo *program_finfo = bc_funinfo(tsk->bcdata);
      int newfno = instr->arg0;
      const funinfo *fi = &program_finfo[newfno];
      LABEL(tsk->bplabels1[addr]);
      switch (instr->arg1) {
      case 2:  I_JMP(label(instrlabels[fi->addressed])); break;
      case 1:  I_JMP(label(instrlabels[fi->addressne])); break;
      default: I_JMP(label(instrlabels[fi->address]));  break;
      }
      break;
    }
    #endif
    #ifdef NATIVE_JFALSE
    case OP_JFALSE: {
      int Ltrue = as->labels++;
      int Lnotcap = as->labels++;
      int pos = instr->expcount-1;

      /* Check if it's a pointer */
      asm_jump_if_number(tsk,as,pos,Ltrue); // uses EAX

      /* Check cell type */
      I_MOV(reg(EAX),regmem(EBP,FRAME_DATA+8*pos)); // EAX now points to the cell
      I_MOV(reg(EBX),regmem(EAX,CELL_TYPE));

      /* CAP - report error */
      I_CMP(reg(EBX),imm(CELL_CAP)); // FIXME: problem if signal is received here
      I_JNE(label(Lnotcap));

      I_MOV(regmem(EBP,FRAME_INSTR),imm((int)(instr+1)));
      I_LEA(reg(EAX),regmem(EBP,FRAME_DATA+8*(instr->expcount-1)));
      I_JMP(label(Lcaperror));

      LABEL(Lnotcap);

      /* NIL - jump to target */
      I_CMP(reg(EBX),imm(CELL_NIL));
      LABEL(tsk->bplabels1[addr]);
      I_JZ(label(instrlabels[addr+instr->arg0]));

      LABEL(Ltrue);
      LABEL(tsk->bplabels2[addr]);
      break;
    }
    #endif
    #ifdef NATIVE_JUMP
    case OP_JUMP:
      LABEL(tsk->bplabels1[addr]);
      I_JMP(label(instrlabels[addr+instr->arg0]));
      break;
    #endif
    #ifdef NATIVE_PUSH
    case OP_PUSH:
      I_MOV(reg(EAX),regmem(EBP,FRAME_DATA+8*instr->arg0));
      I_MOV(reg(EBX),regmem(EBP,FRAME_DATA+8*instr->arg0+4));
      I_MOV(regmem(EBP,FRAME_DATA+8*instr->expcount),reg(EAX));
      I_MOV(regmem(EBP,FRAME_DATA+8*instr->expcount+4),reg(EBX));
      LABEL(tsk->bplabels1[addr]);
      break;
    #endif
    #ifdef NATIVE_UPDATE
/*     case OP_UPDATE: */
    #endif
    #ifdef NATIVE_ALLOC
/*     case OP_ALLOC: */
    #endif
    #ifdef NATIVE_SQUEEZE
    case OP_SQUEEZE: {
      int count = instr->arg0;
      int remove = instr->arg1;
      int base = instr->expcount-count-remove;
      int i;
      for (i = 0; i < count; i++) {
        I_MOV(reg(EAX),regmem(EBP,FRAME_DATA+8*(base+i+remove)));
        I_MOV(reg(EBX),regmem(EBP,FRAME_DATA+8*(base+i+remove)+4));
        I_MOV(regmem(EBP,FRAME_DATA+8*(base+i)),reg(EAX));
        I_MOV(regmem(EBP,FRAME_DATA+8*(base+i)+4),reg(EBX));
      }
      LABEL(tsk->bplabels1[addr]);
      break;
    }
    #endif
    #ifdef NATIVE_MKCAP
/*     case OP_MKCAP: */
    #endif
    #ifdef NATIVE_MKFRAME
    case OP_MKFRAME: {
      const instruction *program_ops = bc_instructions(tsk->bcdata);
      const funinfo *program_finfo = bc_funinfo(tsk->bcdata);
      int fno = instr->arg0;
      int n = instr->arg1;

      asm_frame_new(tsk,as,1,STATE_NEW);

      // newf->retp = NULL;
      I_MOV(regmem(EDI,FRAME_RETP),imm(0));

      // newfholder = alloc_cell(tsk);

      // if (NULL == tsk->ptr)
      int Lhavefreecell = as->labels++;
      int Lcellallocated = as->labels++;
      I_CMP(absmem((int)&tsk->freeptr),imm(1));
      I_JNE(label(Lhavefreecell));
      /* Fall back to C version */

      /* Note: an extra sizeof(cell) will get added to alloc_bytes here, but it doesn't affect
         program correctness (the next collection will just happen very slightly sooner) */

      BEGIN_CALL;
      I_PUSH(imm((int)tsk));
      I_MOV(reg(EAX),imm((int)alloc_cell));
      I_CALL(reg(EAX));
      I_ADD(reg(ESP),imm(4));
      I_MOV(regmem(ESP,32-PUSHAD_OFFSET_ESI),reg(EAX));
      END_CALL;
      I_JMP(label(Lcellallocated));

      LABEL(Lhavefreecell);

      // v = tsk->freeptr;
      I_MOV(reg(ESI),absmem((int)&tsk->freeptr));

      // v->flags = tsk->indistgc ? FLAG_NEW : 0;
      /* FIXME: set the flags properly. They get overwritten when we set the cell type; need
         to write the 16 bit parts of the value separately, or do a combined MOV with the
         both high and low values set appropriately */

      // tsk->freeptr = (cell*)get_pntr(v->field1);
      I_MOV(reg(EAX),regmem(ESI,CELL_FIELD1));
      I_MOV(absmem(((int)&tsk->freeptr)),reg(EAX));
      LABEL(Lcellallocated);

      // newfholder->type = CELL_FRAME;
      /* FIXME: we need to make this a 16-bit move, because the current version overwrites
         the flags field (copying all 32 bits). This is only strictly necessary for parallel
         mode since in standalone mode the flags are initialised to 0 anyway */
      I_MOV(regmem(ESI,CELL_TYPE),imm(CELL_FRAME));

      // make_pntr(newfholder->field1,newf);
      I_MOV(regmem(ESI,CELL_FIELD1),reg(EDI));
      I_MOV(regmem(ESI,CELL_FIELD1+4),imm(PNTR_VALUE));

      // newf->c = newfholder;
      I_MOV(regmem(EDI,FRAME_C),reg(ESI));

      // newf->instr = &program_ops[program_finfo[fno].address+1];
      I_MOV(regmem(EDI,FRAME_INSTR),imm((int)(program_ops+program_finfo[fno].address+1)));

      /* Transfer the arguments to the new frame's data area */
      asm_copy_stack(tsk,as,instr->expcount,fno,n);

      I_MOV(regmem(EBP,FRAME_DATA+8*(instr->expcount-n)),reg(ESI));
      I_MOV(regmem(EBP,FRAME_DATA+8*(instr->expcount-n)+4),imm(PNTR_VALUE));
      LABEL(tsk->bplabels1[addr]);
      break;
    }
    #endif
    #ifdef NATIVE_BIF
    case OP_BIF: {
      int bif = instr->arg0;
      int nargs = builtin_info[bif].nargs;

      switch (bif) {
        /* Functions which accept 2 numeric arguments */
      case B_ADD:
      case B_SUBTRACT:
      case B_MULTIPLY:
      case B_DIVIDE: {
        I_FLD_64(regmem(EBP,FRAME_DATA+8*(instr->expcount-1)));
        switch (bif) {
        case B_ADD:      I_FADD(regmem(EBP,FRAME_DATA+8*(instr->expcount-2))); break;
        case B_SUBTRACT: I_FSUB(regmem(EBP,FRAME_DATA+8*(instr->expcount-2))); break;
        case B_MULTIPLY: I_FMUL(regmem(EBP,FRAME_DATA+8*(instr->expcount-2))); break;
        case B_DIVIDE:   I_FDIV(regmem(EBP,FRAME_DATA+8*(instr->expcount-2))); break;
        }
        I_FSTP_64(regmem(EBP,FRAME_DATA+8*(instr->expcount-2)));
        LABEL(tsk->bplabels1[addr]);
        break;
      }
      case B_EQ:
      case B_NE:
      case B_LT:
      case B_LE:
      case B_GT:
      case B_GE: {
        int Ltrue = as->labels++;
        int Lend = as->labels++;
        double one = 1;
        int *trueval = (int*)&one;
        int *falseval = (int*)&tsk->globnilpntr;

        I_FLD_64(regmem(EBP,FRAME_DATA+8*(instr->expcount-2)));
        I_FLD_64(regmem(EBP,FRAME_DATA+8*(instr->expcount-1)));
        I_FUCOMPP();
        I_FNSTSW_AX();
        I_SAHF();
        switch (bif) {
        case B_EQ: I_JZ(label(Ltrue)); break;
        case B_NE: I_JNE(label(Ltrue)); break;
        case B_LT: I_JNAE(label(Ltrue)); break;
        case B_LE: I_JNA(label(Ltrue)); break;
        case B_GT: I_JNBE(label(Ltrue)); break;
        case B_GE: I_JNB(label(Ltrue)); break;
        }

        I_MOV(regmem(EBP,FRAME_DATA+8*(instr->expcount-2)),imm(falseval[0]));
        I_MOV(regmem(EBP,FRAME_DATA+8*(instr->expcount-2)+4),imm(falseval[1]));
        I_JMP(label(Lend));

        LABEL(Ltrue);
        I_MOV(regmem(EBP,FRAME_DATA+8*(instr->expcount-2)),imm(trueval[0]));
        I_MOV(regmem(EBP,FRAME_DATA+8*(instr->expcount-2)+4),imm(trueval[1]));

        LABEL(Lend);
        LABEL(tsk->bplabels1[addr]);
        break;
      }
      case B_SQRT:
        I_FLD_64(regmem(EBP,FRAME_DATA+8*(instr->expcount-1)));
        I_FSQRT();
        I_FSTP_64(regmem(EBP,FRAME_DATA+8*(instr->expcount-1)));
        LABEL(tsk->bplabels1[addr]);
        break;
      default:
        I_MOV(regmem(EBP,FRAME_INSTR),imm((int)(instr+1)));
        I_LEA(reg(ECX),regmem(EBP,FRAME_DATA+8*(instr->expcount-nargs)));
        runnable_owner_task(tsk,as);

        BEGIN_CALL;
        I_PUSH(reg(ECX));
        I_PUSH(imm((int)tsk));
        I_MOV(reg(EAX),imm((int)builtin_info[bif].f));
        I_CALL(reg(EAX));
        I_ADD(reg(ESP),imm(8));
        END_CALL;

        runnable_owner_native_code(tsk,as);
        swap_in(tsk,as,tsk->bplabels1[addr]);
        break;
      }
      break;
    }
    #endif
    #ifdef NATIVE_PUSHNIL
    case OP_PUSHNIL: {
      // runnable->data[instr->expcount] = tsk->globnilpntr;
      int *p = (int*)&tsk->globnilpntr;
      I_MOV(regmem(EBP,FRAME_DATA+8*instr->expcount),imm(p[0]));
      I_MOV(regmem(EBP,FRAME_DATA+8*instr->expcount+4),imm(p[1]));
      LABEL(tsk->bplabels1[addr]);
      break;
    }
    #endif
    #ifdef NATIVE_PUSHNUMBER
    case OP_PUSHNUMBER: {
      // runnable->data[instr->expcount] = *((double*)&instr->arg0);
      I_MOV(regmem(EBP,FRAME_DATA+8*instr->expcount),imm(instr->arg0));
      I_MOV(regmem(EBP,FRAME_DATA+8*instr->expcount+4),imm(instr->arg1));
      LABEL(tsk->bplabels1[addr]);
      break;
    }
    #endif
    #ifdef NATIVE_PUSHSTRING
    case OP_PUSHSTRING: {
      // runnable->data[instr->expcount] = tsk->strings[instr->arg0];
      int *p = (int*)&tsk->strings[instr->arg0];
      I_MOV(regmem(EBP,FRAME_DATA+8*instr->expcount),imm(p[0]));
      I_MOV(regmem(EBP,FRAME_DATA+8*instr->expcount+4),imm(p[1]));
      LABEL(tsk->bplabels1[addr]);
      break;
    }
    #endif
    #ifdef NATIVE_POP
    case OP_POP:
      /* NOOP */
      LABEL(tsk->bplabels1[addr]);
      break;
    #endif
    //#ifdef NATIVE_ERROR
    case OP_ERROR:
      I_MOV(absmem((int)&tsk->native_finished),imm(1));
      LABEL(tsk->bplabels1[addr]);
      I_JMP(label(Largerror));
      break;
    //#endif
    #ifdef NATIVE_EVAL
    case OP_EVAL: {
      int Ldone = as->labels++;
      int pos = instr->arg0;

      // RUNNABLE->data[INSTR->arg0] = resolve_pntr(RUNNABLE->data[INSTR->arg0]);
      asm_resolve_stack_pntr(tsk,as,pos); // uses EAX, EBX, ECX, ECX
      // now EBX == type, EAX == cell pointer (if not number)
      I_CMP(reg(EBX),imm(CELL_FRAME));
      I_JNE(label(Ldone));

      /* If we get here, the cell is a frame */

      // EDI = new frame
      // EBP = current frame (as always)

      // frame *newf = (frame*)get_pntr(get_pntr(p)->field1);
      I_MOV(reg(EDI),regmem(EAX,CELL_FIELD1));

      // frame *curf = RUNNABLE;
      // curf->instr--;
      I_MOV(regmem(EBP,FRAME_INSTR),imm((int)instr));

      // curf->waitlnk = newf->wq.frames;
      I_MOV(reg(EAX),regmem(EDI,FRAME_WQ+WAITQUEUE_FRAMES));
      I_MOV(regmem(EBP,FRAME_WAITLNK),reg(EAX));
      // newf->wq.frames = curf;
      I_MOV(regmem(EDI,FRAME_WQ+WAITQUEUE_FRAMES),reg(EBP));

      // block_frame(tsk,curf);
      I_MOV(reg(EAX),regmem(EBP,FRAME_RNEXT));
      I_MOV(regmem(EBP,FRAME_RNEXT),imm(0));
      I_MOV(regmem(EBP,FRAME_STATE),imm(STATE_BLOCKED));
      I_MOV(reg(EBP),reg(EAX));

      // run_frame(tsk,newf);
      int Lafterrun = as->labels++;
      int Ldorun = as->labels++;

      I_CMP(regmem(EDI,FRAME_STATE),imm(STATE_SPARKED));
      I_JZ(label(Ldorun));
      I_CMP(regmem(EDI,FRAME_STATE),imm(STATE_NEW));
      I_JZ(label(Ldorun));
      I_JMP(label(Lafterrun));

      LABEL(Ldorun);

      I_MOV(regmem(EDI,FRAME_RNEXT),reg(EBP));
      I_MOV(regmem(EDI,FRAME_STATE),imm(STATE_RUNNING));
      I_MOV(reg(EBP),reg(EDI));

      LABEL(Lafterrun);

      // check_runnable(tsk);
      asm_check_runnable(tsk,as);

      swap_in(tsk,as,tsk->bplabels1[addr]);

      /* FIXME: handle remote references */
      LABEL(Ldone);
      LABEL(tsk->bplabels2[addr]);
      break;
    }
    #endif
    #ifdef NATIVE_CALL
    case OP_CALL: {
      const funinfo *program_finfo = bc_funinfo(tsk->bcdata);
      int fno = instr->arg0;
      int n = instr->arg1;

      // newf = frame_new(tsk,0);
      asm_frame_new(tsk,as,0,STATE_RUNNING);
      // EDI == frame pointer

      // newf->c = 0;
      I_MOV(regmem(EDI,FRAME_C),imm(0));

      // newf->instr = NULL (since we will jump directly to it)
      I_MOV(regmem(EDI,FRAME_INSTR),imm(0));

      // newf->retp = &f2->data[INSTR->expcount-n];
      I_LEA(reg(EAX),regmem(EBP,FRAME_DATA+8*(instr->expcount-n)));
      I_MOV(regmem(EDI,FRAME_RETP),reg(EAX));

      /* Transfer the arguments to the new frame's data area */
      asm_copy_stack(tsk,as,instr->expcount,fno,n);

      /* Update the curren't frames instruction pointer to refer to the next instruction
         after this one. */
      I_MOV(regmem(EBP,FRAME_INSTR),imm((int)(instr+1)));

      // curf->waitlnk = newf->wq.frames;
      I_MOV(reg(EAX),regmem(EDI,FRAME_WQ+WAITQUEUE_FRAMES));
      I_MOV(regmem(EBP,FRAME_WAITLNK),reg(EAX));
      // newf->wq.frames = curf;
      I_MOV(regmem(EDI,FRAME_WQ+WAITQUEUE_FRAMES),reg(EBP));

      /* But the current frame into the blocked state and remove it from the run queue.
         Add the new frame on to the run queue. Note: the state of the new frame was
         already initialised to STATE_NEW when we created the frame above. */
      I_MOV(reg(EAX),regmem(EBP,FRAME_RNEXT));
      I_MOV(regmem(EBP,FRAME_RNEXT),imm(0));
      I_MOV(regmem(EBP,FRAME_STATE),imm(STATE_BLOCKED));
      I_MOV(regmem(EDI,FRAME_RNEXT),reg(EAX));

      /* Jump directly to the new frame, instead of calling swap_in() */
      I_MOV(reg(EBP),reg(EDI));
      LABEL(tsk->bplabels1[addr]);
      I_JMP(label(instrlabels[program_finfo[fno].address+1]));
      break;
    }
    #endif
    #ifdef NATIVE_JCMP
    case OP_JCMP: {
      int bif = instr->arg1;
      I_FLD_64(regmem(EBP,FRAME_DATA+8*(instr->expcount-2)));
      I_FLD_64(regmem(EBP,FRAME_DATA+8*(instr->expcount-1)));
      I_FUCOMPP();
      I_FNSTSW_AX();
      I_SAHF();
      LABEL(tsk->bplabels1[addr]);
      switch (bif) {
      case B_EQ: I_JNE(label(instrlabels[addr+instr->arg0])); break;
      case B_NE: I_JZ(label(instrlabels[addr+instr->arg0])); break;
      case B_LT: I_JNB(label(instrlabels[addr+instr->arg0])); break;
      case B_LE: I_JNBE(label(instrlabels[addr+instr->arg0])); break;
      case B_GT: I_JNA(label(instrlabels[addr+instr->arg0])); break;
      case B_GE: I_JNAE(label(instrlabels[addr+instr->arg0])); break;
      default: abort(); break;
      }
      break;
    }
    #endif
    case OP_INVALID:
      PRINT_MESSAGE("INVALID INSTRUCTION\n");
      I_MOV(absmem((int)&tsk->native_finished),imm(1));
      LABEL(tsk->bplabels1[addr]);
      I_JMP(label(Lfinished));
      break;
    default: {
      I_MOV(regmem(EBP,FRAME_INSTR),imm((int)(instr+1)));
      runnable_owner_task(tsk,as);

      /* Call the appropriate op_* function */
      BEGIN_CALL;
      I_PUSH(imm((int)(instr)));
      I_PUSH(absmem((int)tsk->runptr));
      I_PUSH(imm((int)tsk));
      I_MOV(reg(EAX),imm((int)op_handlers[instr->opcode]));
      I_CALL(reg(EAX));
      I_ADD(reg(ESP),imm(12));
      END_CALL;

      runnable_owner_native_code(tsk,as);
      swap_in(tsk,as,tsk->bplabels1[addr]);
      break;
    }
    }
  }

  LABEL(Lbcend);
  LABEL(Lfinished);
  I_POPAD();
  I_RET();





  /************** Interrupt ************/
  LABEL(Linterrupt);

  /* Push a 0 onto the stack directly on top of normal_esp. If an interrupt signal is sent
     while we are in this function, sigusr1() will detect that this value (the saved eip)
     is 0, and realise that we are already in the interrupt handler. If this happens, it will
     set interrupt_again to 1. */
  I_PUSH(imm(0));
  I_PUSHF();
  runnable_owner_task(tsk,as);
  I_PUSHAD();

  /* Handle the interrupt. This will try again if another interrupt was received during
     the process (in which interrupt_again will be set to true).

     do {
       interrupt_again = 0;
       handle_interrupt(tsk);
     } while (interrupt_again); */

  int Linterruptstart = as->labels++;
  int Linterruptend = as->labels++;
  LABEL(Linterruptstart);
  I_MOV(absmem((int)&tsk->interrupt_again),imm(0));

  I_MOV(absmem((int)tsk->endpt->interruptptr),imm(0));

  I_PUSHAD();
  I_PUSH(imm((int)tsk));
  I_MOV(reg(EAX),imm((int)handle_interrupt));
  I_CALL(reg(EAX));
  I_ADD(reg(ESP),imm(4));
  I_POPAD();

  I_CMP(absmem((int)&tsk->interrupt_again),imm(0));
  I_JZ(label(Linterruptend));
  I_JMP(label(Linterruptstart));

  LABEL(Linterruptend);

  I_POPAD();
  runnable_owner_native_code(tsk,as);
  I_POPF();
  I_MOV(reg(ESP),absmem((int)&tsk->normal_esp));
  I_JMP(absmem((int)&tsk->interrupt_return_eip));

  /************** cap_error ************/
  LABEL(Lcaperror);
  // Caller has set EAX = pntr* referring to offending value
  runnable_owner_task(tsk,as);
  BEGIN_CALL;
  I_PUSH(regmem(EAX,4));
  I_PUSH(regmem(EAX,0));
  I_PUSH(imm((int)tsk));
  I_MOV(reg(EAX),imm((int)native_cap_error));
  I_CALL(reg(EAX));
  I_ADD(reg(ESP),imm(12));
  END_CALL;
  I_JMP(label(Lfinished));

  /************** arg_error ************/
  LABEL(Largerror);
  BEGIN_CALL;
  I_PUSH(imm((int)tsk));
  I_MOV(reg(EAX),imm((int)native_arg_error));
  I_CALL(reg(EAX));
  I_ADD(reg(ESP),imm(4));
  END_CALL;
  I_JMP(label(Lfinished));

  /* Done compiling; now assemble */
  x86_assemble(as,cpucode);
  tsk->cpu_to_bcaddr = (int*)calloc(cpucode->nbytes,sizeof(int));
  int last = 0;

  for (addr = 0; addr < bch->nops; addr++) {
    int cpuaddr = as->instructions[tempaddrs[addr]].addr;
    int bplabel1 = tsk->bplabels1[addr];
    int bplabel2 = tsk->bplabels2[addr];
    tsk->instraddrs[addr] = (void*)(((char*)cpucode->data)+cpuaddr);

    assert(as->labeladdrs[bplabel1]);
    tsk->bpaddrs1[addr] = ((unsigned char*)cpucode->data)+as->labeladdrs[bplabel1];
    if (as->labeladdrs[bplabel2])
      tsk->bpaddrs2[addr] = ((unsigned char*)cpucode->data)+as->labeladdrs[bplabel2];

    if (0 == addr) {
      for (i = last; i < cpuaddr; i++)
        tsk->cpu_to_bcaddr[i] = bch->nops-1; /* INVALID instruction */
    }
    else {
      for (i = last; i < cpuaddr; i++)
        tsk->cpu_to_bcaddr[i] = addr-1;
    }
    last = cpuaddr;
  }
  for (i = last; i < cpucode->nbytes; i++)
    tsk->cpu_to_bcaddr[i] = addr-1;

  tsk->bcend_addr = cpucode->data+as->labeladdrs[Lbcend];
  tsk->interrupt_addr = cpucode->data+as->labeladdrs[Linterrupt];
  tsk->caperror_addr = cpucode->data+as->labeladdrs[Lcaperror];
  tsk->argerror_addr = cpucode->data+as->labeladdrs[Largerror];

  tsk->interrupt_end = tsk->caperror_addr;
  tsk->caperror_end = tsk->argerror_addr;
  tsk->argerror_end = cpucode->data+cpucode->nbytes;

  tsk->code = cpucode->data;
  tsk->codesize = cpucode->nbytes;

/*   dump_asm(tsk,"output.s",cpucode); */
  free(instrlabels);
  x86_assembly_free(as);
}
