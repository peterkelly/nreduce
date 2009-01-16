/*
 * This file is part of the nreduce project
 * Copyright (C) 2006-2008 Peter Kelly (pmk@cs.adelaide.edu.au)
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
#include <unistd.h>
#include <fenv.h>
#include <sys/mman.h>

#ifdef HAVE_EXECINFO_H
#include <execinfo.h>
#endif

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
#define NATIVE_JEQ

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
void op_consn(task *tsk, frame *runnable, const instruction *instr);
void op_itemn(task *tsk, frame *runnable, const instruction *instr);
void op_jeq(task *tsk, frame *runnable, const instruction *instr);
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
  op_consn,
  op_itemn,
  op_jeq,
  op_invalid,
};

#ifdef HAVE_MCONTEXT_T_GREGS
#define UC_ESP(uc) (uc->uc_mcontext.gregs[REG_ESP])
#define UC_EBP(uc) (uc->uc_mcontext.gregs[REG_EBP])
#define UC_EIP(uc) (uc->uc_mcontext.gregs[REG_EIP])
#else
#define UC_ESP(uc) (uc->uc_mcontext->__ss.__esp)
#define UC_EBP(uc) (uc->uc_mcontext->__ss.__ebp)
#define UC_EIP(uc) (uc->uc_mcontext->__ss.__eip)
#endif

#define STACK_ALIGNMENT 16
#define NATIVE_ESP_OFFSET 8

#define PUSHAD_OFFSET_EAX 4
#define PUSHAD_OFFSET_ECX 8
#define PUSHAD_OFFSET_EDX 12
#define PUSHAD_OFFSET_EBX 16
#define PUSHAD_OFFSET_ESP 20
#define PUSHAD_OFFSET_EBP 24
#define PUSHAD_OFFSET_ESI 28
#define PUSHAD_OFFSET_EDI 32

#define STACK_PADDING(argbytes) (STACK_ALIGNMENT-((NATIVE_ESP_OFFSET+(argbytes))%STACK_ALIGNMENT))

#define BEGIN_CALL(argbytes) {               \
  int Ltemp = as->labels++;                  \
  int Ltempend = as->labels++;               \
  int _addstack = STACK_PADDING(argbytes);   \
  I_CALL(label(Ltemp));                      \
  I_JMP(label(Ltempend));                    \
  LABEL(Ltemp);                              \
  I_SUB(reg(ESP),imm(_addstack));            \
  I_PUSHAD();

#define END_CALL                             \
  I_POPAD();                                 \
  I_ADD(reg(ESP),imm(_addstack));            \
  I_RET();                                   \
  LABEL(Ltempend); }

#define PRINT_VALUE(str,val) \
      BEGIN_CALL(8);	     \
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
      BEGIN_CALL(4);			     \
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

#define FRAME_WAITLNK ((int)&((frame*)0)->waitlnk)
#define FRAME_WQ ((int)&((frame*)0)->wq)
#define FRAME_FREELNK ((int)&((frame*)0)->freelnk)
#define FRAME_POSTPONED ((int)&((frame*)0)->postponed)

#define FRAME_DATA ((int)&((frame*)0)->data[0])
#define FRAME_C ((int)&((frame*)0)->c)
#define FRAME_RETP ((int)&((frame*)0)->retp)
#define FRAME_INSTR ((int)&((frame*)0)->instr)
#define FRAME_RNEXT ((int)&((frame*)0)->rnext)
#define FRAME_STATE ((int)&((frame*)0)->state)
#define FRAME_RESUME ((int)&((frame*)0)->resume)
#define FRAME_SPREV ((int)&((frame*)0)->sprev)
#define FRAME_SNEXT ((int)&((frame*)0)->snext)
#define ARRAY_DATA ((int)&((array*)0)->data)
#define TASK_GLOBNILPNTR ((int)&((task*)0)->globnilpntr)
#define TASK_STRINGS ((int)&((task*)0)->strings)
#define TASK_CODE ((int)&((task*)0)->code)
#define TASK_FREEFRAME ((int)&((task*)0)->freeframe)
#define WAITQUEUE_FRAMES ((int)&((waitqueue*)0)->frames)
#define WAITQUEUE_FETCHERS ((int)&((waitqueue*)0)->fetchers)

#define INSTR_CODE ((int)&((instruction*)0)->code)

#define CARRAY_ELEMENTS ((int)&((carray*)0)->elements[0])
#define CARRAY_SIZE ((int)&((carray*)0)->size)
#define CARRAY_TAIL ((int)&((carray*)0)->tail)
#define CARRAY_WRAPPER ((int)&((carray*)0)->wrapper)
#define CARRAY_ELEMSIZE ((int)&((carray*)0)->elemsize)

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
    else if (i == tsk->trap_addr-tsk->code) {
      fprintf(f,"trap:\n");
    }
    else if (i == tsk->headerror_addr-tsk->code) {
      fprintf(f,"headerror:\n");
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
  else if (tsk->trap_addr == eip) {
    snprintf(str,100,"(trap)        ");
  }
  else {
    snprintf(str,100,"???????????????????");
  }
}

void function_below(char *name)
{
#ifndef HAVE_EXECINFO_H
  sprintf(name,"unknown");
#else
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
#endif
}

void print_instruction(task *tsk, int ebp, int addr, int opcode)
{
  printf("%4d %-20s\n",addr,opcodes[opcode]);
}

/*
 * SIGUSR1 is sent to the thread when there is some exceptional condition that needs to be dealt
 * with, such as the garbage collection threshold being reached (meaning the GC should be
 * invoked), or a message being received over the network. When this occurs, we break out of the
 * normal code execution stream and deal with the event in handle_interrupt(). By using a signal
 * handler to deal with these events, we avoid having to continuously poll to check if there
 * is something that needs done, which is the method used by the interpreter.
 * 
 * We cannot handle the event directly in the signal handler, because the task could be part way
 * through some important operation and the memory may not be in a consistent state. For example
 * the program could be performing a malloc operation or updating a 64-bit field (which requires
 * two instructions). Instead of handling it immediately, we modify an instruction that is due
 * to be executed shortly. The point at which the interrupt will be handled lies at the
 * boundary between the current bytecode instruction and the next.
 * 
 * To do this, we set the 5 bytes at the "breakpoint address" of the current instruction. This is
 * usually the start address of the next bytecode instruction, or the address of a jump which comes
 * right at the end of this one. Because there are some opcodes which have two possible exit
 * paths (notably JFALSE and EVAL), we must modify two instructions in some cases. Both are
 * replaced with a CALL whos destination is the interrupt handler hook. Because the CALL
 * instruction takes 5 bytes of memory, a bytecode instruction with two breakpoint addresses has
 * these at least 5 bytes apart (the compiler pads the code with NOPs where necessary).
 * 
 * The code modification used to be done by setting a single byte 0xCC at both places, which
 * corresponds to an INT 3 instruction that causes a SIGTRAP signal to be sent to the process.
 * However, a kernel bug in OS X causes the SIGTRAP signals to be occasionally missed. For this
 * reason we now use CALL instead.
 */
void native_sigusr1(int sig, siginfo_t *ino, void *uc1)
{
  ucontext_t *uc = (ucontext_t*)uc1;
  task *tsk = pthread_getspecific(task_key);
  int stackoffset = tsk->normal_esp-(void*)UC_ESP(uc);
  const instruction *program_ops = bc_instructions(tsk->bcdata);
  void *eip;
  int offset;
  int bcaddr;
  unsigned char zero5[5] = {0,0,0,0,0};
  int bp;

  assert(tsk);

  if (!tsk->usr1setup)
    return;

  if (tsk->trap_pending || tsk->native_finished || tsk->done)
    return;

  if (0 == stackoffset) { /* Native code */
    eip = (void*)UC_EIP(uc);
  }
  else { /* C function or trap handler */
    eip = *(void**)(tsk->normal_esp-4);
  }

  /* At this point, eip is the machine address that the native code is currently at. We need
     to figure out which bytecode instruction this corresponds to */

  assert(eip >= tsk->code);
  assert(eip < tsk->bcend_addr);

  offset = eip-tsk->code;
  assert((0 <= offset) && (tsk->codesize > offset));
  bcaddr = tsk->cpu_to_bcaddr[offset];
  assert(OP_INVALID != program_ops[bcaddr].opcode);

  /* Set the breakpoint at the appropriate place for the current bytecode instruction. In the case
     of straight-line code, this will be the first machine instruction that comes after the end of
     the current bytecode instruction. In the case of native code that contains jumps, there may
     be two places we need to set the breakpoint. */
  assert(tsk->bpaddrs[0][bcaddr]);

  for (bp = 0; bp <= 1; bp++) {
    if (tsk->bpaddrs[bp][bcaddr]) {
      int offset = (int)tsk->trap_addr-((int)tsk->bpaddrs[bp][bcaddr]+5);
      assert(!memcmp(tsk->bcbackup[bp],zero5,5));
      memcpy(tsk->bcbackup[bp],tsk->bpaddrs[bp][bcaddr],5);
      tsk->bpaddrs[bp][bcaddr][0] = 0xe8;
      memcpy(&tsk->bpaddrs[bp][bcaddr][1],&offset,4);
    }
  }

  assert(0 == tsk->trap_bcaddr);
  tsk->trap_pending = 1;
  tsk->trap_bcaddr = bcaddr;
}

/*
 * Received when we hit a breakpoint that was previously set by native_sigusr1(). When this is
 * called, the native code is at a "safe" point at which we can execute handle_interrupt().
 */
void handle_trap(task *tsk, void *eip, void *eax)
{
  bcheader *bch = (bcheader*)tsk->bcdata;
  const instruction *program_ops = bc_instructions(tsk->bcdata);
  int bcaddr;
  int bp;
  frame *curf = *tsk->runptr;
  assert((void*)1 != curf);
  assert((eip >= tsk->code) && (eip < tsk->code+tsk->codesize));

  /* Check that we're actually expecting this signal, and that we were invoked from generated code
     (i.e. not from some other C function) */
  assert(tsk->trap_pending);
  bcaddr = tsk->trap_bcaddr;

  assert(((void*)eip == tsk->bpaddrs[0][bcaddr]) ||
         ((void*)eip == tsk->bpaddrs[1][bcaddr]));
  assert((eip >= program_ops[bcaddr].code) || (0 == bcaddr));
  assert((bcaddr == bch->nops-1) ||
         (eip <= program_ops[bcaddr+1].code));

  /* Figure out which breakpoint address we came from, so we can tell if the code was executing
     swap_in when the breakpoint was triggered. This affects how we instruct the code to continue
     afterwards. */

  int found = 0;
  int from_swapin = 0;
  for (bp = 0; bp <= 1; bp++) {
    if (tsk->bpaddrs[bp][bcaddr] == eip) {
      found++;
      if (tsk->bpswapin[bp][bcaddr])
        from_swapin = 1;
    }
  }
  assert(1 == found); /* exactly one should match */

  /* Clear the breakpoint(s), by restoring the previous values that were at the start of the
     relevant machine instrucitons. These were temporarily set to a 5-byte CALL instruction
     in native_sigusr1() */
  assert(tsk->bpaddrs[0][bcaddr]);
  for (bp = 0; bp <= 1; bp++) {
    if (tsk->bpaddrs[bp][bcaddr]) {
      assert(0xe8 == *tsk->bpaddrs[bp][bcaddr]);
      memcpy(tsk->bpaddrs[bp][bcaddr],tsk->bcbackup[bp],5);
      memset(tsk->bcbackup[bp],0,5);
    }
  }

  tsk->trap_bcaddr = 0;
  tsk->trap_pending = 0;

  /* Make sure that if we have a current frame, it's instr pointer is set to NULL, because
     the native code instruction pointer is the authorative source for this information. */
  assert((NULL == curf) || (NULL == curf->instr));

  if (curf) {
    /* If we were called from swap_in, then the current frame's instruction is derived from the
       address that the native code is about to jump to after we've finished handling this trap.
       Otheriwse, it's simply the next instruction to be executed. */
    if (from_swapin) {
      int target_bcaddr = tsk->cpu_to_bcaddr[eax-tsk->code];
      curf->instr =  &program_ops[target_bcaddr];
    }
    else {
      curf->instr = &program_ops[bcaddr+1];
    }
  }

  /* Call the handle_interrupt() function in interpreter.c to do message processing, garbage
     collection etc. */
  handle_interrupt(tsk);

  curf = *tsk->runptr;
  if (curf) {
    assert(curf->instr);
    if (from_swapin) {
      /* If we were called during execution of swap_in, this means there is a possibility that
         the current frame may have changed. We need to arrange for the jump instruction at
         the end of swap_in to jump to this frame.

         Note that we do not do this for other instructions, as this may cause incorrect behaviour
         (e.g. if it is about to jump to a label). */
      tsk->interrupt_return_eip = curf->instr->code;
    }

    /* Set the current frame's instruction to NULL, since the native code instruction pointer is
       now the authorative source of information about where this frame is in the code. */
    curf->instr = NULL;
  }
}

void native_sigfpe(int sig, siginfo_t *ino, void *uc1)
{
  ucontext_t *uc = (ucontext_t*)uc1;
  void *eip;
  int offset;
  int bcaddr;
  task *tsk = pthread_getspecific(task_key);
  const instruction *program_ops = bc_instructions(tsk->bcdata);
  int stackoffset = tsk->normal_esp-(void*)UC_ESP(uc);
  const instruction *instr;
  frame *curf;

  if ((void*)1 == *tsk->runptr)
    *tsk->runptr = (void*)UC_EBP(uc);
  curf = *tsk->runptr;

  if (0 == stackoffset) { /* Native code */
    eip = (void*)UC_EIP(uc);
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

  UC_ESP(uc) = (int)tsk->normal_esp;
  UC_EIP(uc) = (int)tsk->argerror_addr;
  tsk->native_finished = 1;
}

void native_sigsegv(int sig, siginfo_t *ino, void *uc1)
{
  ucontext_t *uc = (ucontext_t*)uc1;
  task *tsk = pthread_getspecific(task_key);
  printf("SIGSEGV: eip = %p, rel eip = 0x%x, ebp = %p\n",
         (void*)UC_EIP(uc),
         (void*)UC_EIP(uc)-tsk->code,
         (void*)UC_EBP(uc));
  exit(1);
}

void native_arg_error(task *tsk)
{
  tsk->native_finished = 1;
  handle_error(tsk);
}

void native_head_error(task *tsk, pntr cappntr)
{
  tsk->native_finished = 1;
  head_error(tsk);
  handle_error(tsk);
}

void native_cap_error(task *tsk, pntr cappntr)
{
  tsk->native_finished = 1;
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
  BEGIN_CALL(16);
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

void swap_in_error(task *tsk)
{
  fatal("swap_in: no runnable frames, and trap not (correctly) set");
}

/* Update the native code EIP for the current runnable list head */
void swap_in(task *tsk, x86_assembly *as, int **bplabels, int curaddr)
{
  int Lnotnull = as->labels++;
  int Ljump = as->labels++;

  int Lcheck = as->labels++;
  int Lhavetrap = as->labels++;
  LABEL(Lcheck);
  /* Compute the address of the next instruction */

  I_CMP(reg(EBP),imm(0));
  I_JNE(label(Lnotnull));

  /* If we get here, we should have had an interrupt, and the jump target should be a breakpoint */
  I_CMP(absmem((int)&tsk->trap_pending),imm(0));
  I_JNE(label(Lhavetrap));

  BEGIN_CALL(4);
  I_PUSH(imm((int)tsk));
  I_MOV(reg(EAX),imm((int)swap_in_error));
  I_CALL(reg(EAX));
  I_ADD(reg(ESP),imm(4));
  END_CALL;

  LABEL(Lhavetrap);
  I_MOV(reg(EAX),label(Lcheck));
  I_ADD(reg(EAX),absmem((int)&tsk->code));
  I_JMP(label(Ljump));

  LABEL(Lnotnull);

  I_MOV(reg(EAX),regmem(EBP,FRAME_INSTR)); // get current instruction
  I_MOV(regmem(EBP,FRAME_INSTR),imm(0)); // set frame's instruction to 0
  I_MOV(reg(EAX),regmem(EAX,INSTR_CODE)); // get the code address for the current instruction

  /* Jump to the next instruction */
  tsk->bpswapin[0][curaddr] = 1;
  LABEL(bplabels[0][curaddr]);
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

  BEGIN_CALL(4);
  I_PUSH(imm((int)tsk->endpt));
  I_MOV(reg(EAX),imm((int)endpoint_interrupt));
  I_CALL(reg(EAX));
  I_ADD(reg(ESP),imm(4));
  END_CALL;

  LABEL(Lno);
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

void asm_alloc_cell_to_esi(task *tsk, x86_assembly *as)
{
  // if (NULL == tsk->ptr)
  int Lhavefreecell = as->labels++;
  int Lcellallocated = as->labels++;

  I_MOV(reg(ESI),absmem((int)&tsk->newgenoffset));
  I_CMP(reg(ESI),imm(sizeof(block)-sizeof(cell)));
  I_JL(label(Lhavefreecell));

  /* Fall back to C version */

  BEGIN_CALL(4);
  I_PUSH(imm((int)tsk));
  I_MOV(reg(EAX),imm((int)alloc_cell));
  I_CALL(reg(EAX));
  I_ADD(reg(ESP),imm(4));
  I_MOV(regmem(ESP,32-PUSHAD_OFFSET_ESI),reg(EAX));
  END_CALL;
  I_JMP(label(Lcellallocated));

  /* Allocate cell directly here; ESI points to newgenoffset from above */

  LABEL(Lhavefreecell);

  // newgenoffset += sizeof(cell)
  I_LEA(reg(EAX),regmem(ESI,sizeof(cell)));
  I_MOV(absmem((int)&tsk->newgenoffset),reg(EAX));

  // get correct cell position
  I_ADD(reg(ESI),absmem((int)&tsk->newgen));

  // Set flags on new cell
  I_MOV(reg(EAX),absmem((int)&tsk->newcellflags));
  I_MOV(regmem(ESI,CELL_FLAGS),reg(EAX));

  LABEL(Lcellallocated);
}

/*
 * Native code implementation of frame_new(). This uses the first item from the linked list of
 * free frames if possible, but falls back to the C version there are no free frames and another
 * block needs to be allocated.
 *
 * After this code runs, EDI contains the address of the new frame
 */
void asm_frame_new(task *tsk, x86_assembly *as, int state)
{
  int Lhavefree = as->labels++;
  int Lframeallocated = as->labels++;

  // f = tsk->freeframe;
  I_MOV(reg(EDI),absmem((int)&tsk->freeframe));

  // if (NULL == f)
  I_CMP(reg(EDI),imm(0));
  I_JNE(label(Lhavefree));
  {
    /* Fall back to C version */
    BEGIN_CALL(4);
    I_PUSH(imm((int)tsk));
    I_MOV(reg(EAX),imm((int)frame_new));
    I_CALL(reg(EAX));
    I_ADD(reg(ESP),imm(4));
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
    // f->postponed = 0;
    // f->sprev = NULL;
    // f->snext = NULL;
    I_MOV(regmem(EDI,FRAME_STATE),imm(state));
    I_MOV(regmem(EDI,FRAME_RESUME),imm(0));
    I_MOV(regmem(EDI,FRAME_FREELNK),imm(0));
    I_MOV(regmem(EDI,FRAME_POSTPONED),imm(0));
    I_MOV(regmem(EDI,FRAME_SPREV),imm(0));
    I_MOV(regmem(EDI,FRAME_SNEXT),imm(0));
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

void native_compile(char *bcdata, int bcsize, task *tsk)
{
  x86_assembly *as = x86_assembly_new();
  instruction *program_ops = (instruction*)&tsk->bcdata[sizeof(bcheader)];
  const instruction *instr;
  int addr;
  bcheader *bch = (bcheader*)bcdata;
  int *tempaddrs = (int*)calloc(bch->nops,sizeof(int));
  int Lfinished = as->labels++;
  int Ltrap = as->labels++;
  int Lstillactive = as->labels++;
  int Lcaperror = as->labels++;
  int Lheaderror = as->labels++;
  int Largerror = as->labels++;
  int Lbcend = as->labels++;
  int *instrlabels;
  int i;
  int *bplabels[2];
  array *cpucode;
  bplabels[0] = (int*)calloc(bch->nops,sizeof(int));
  bplabels[1] = (int*)calloc(bch->nops,sizeof(int));
  tsk->bpaddrs[0] = (unsigned char**)calloc(bch->nops,sizeof(int));
  tsk->bpaddrs[1] = (unsigned char**)calloc(bch->nops,sizeof(int));
  tsk->bpswapin[0] = (unsigned char*)calloc(bch->nops,sizeof(unsigned char));
  tsk->bpswapin[1] = (unsigned char*)calloc(bch->nops,sizeof(unsigned char));
  instrlabels = (int*)malloc(bch->nops*sizeof(int));
  for (i = 0; i < bch->nops; i++)
    instrlabels[i] = as->labels++;

  for (addr = 0; addr < bch->nops; addr++) {
    bplabels[0][addr] = as->labels++;
    bplabels[1][addr] = as->labels++;
  }

  I_PUSHAD();
  I_MOV(absmem((int)&tsk->normal_esp),reg(ESP));

  /* basically runnable_owner_native_code but without the assert */
  I_MOV(reg(EBP),absmem((int)tsk->runptr));
  I_MOV(absmem((int)tsk->runptr),imm(1));

  asm_check_runnable(tsk,as);
  assert(OP_BEGIN == program_ops[0].opcode);
  swap_in(tsk,as,bplabels,0); /* initialize EBP and jump to first instruction */
  I_NOP(); /* pad to 5 bytes between labels */
  I_NOP();
  I_NOP();

  // EBP: always points to current frame

  for (addr = 0; addr < bch->nops; addr++) {
    instr = &program_ops[addr];

    tempaddrs[addr] = as->count;
    LABEL(instrlabels[addr]);

    if (getenv("TRACE")) {
      /* Print out message for this instruction */
      BEGIN_CALL(16);
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
      LABEL(bplabels[0][addr]);
      I_JMP(label(Lfinished));
      break;
    #ifdef NATIVE_BEGIN
    case OP_BEGIN:
      /* NOOP */
      break;
    #endif
    #ifdef NATIVE_GLOBSTART
    case OP_GLOBSTART:
      /* NOOP */
      LABEL(bplabels[0][addr]);
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
      I_CMP(regmem(EAX,FRAME_STATE),imm(STATE_NEW));
      I_JNE(label(Ldone));

      I_MOV(regmem(EAX,FRAME_STATE),imm(STATE_SPARKED));

      /* prepend_spark */

      /* Note: since tsk->sparklist never changes, we can use absolute memory
         addresses here to access its fields */

      // EAX = frame being sparked
      // EBX = last == tsk->sparklist->sprev;

      // frame *first = tsk->sparklist->snext;
      I_MOV(reg(EBX),absmem(((int)tsk->sparklist)+FRAME_SNEXT));
      // f->sprev = tsk->sparklist;
      I_MOV(regmem(EAX,FRAME_SPREV),imm((int)tsk->sparklist));
      // f->snext = first;
      I_MOV(regmem(EAX,FRAME_SNEXT),reg(EBX));
      // first->sprev = f;
      I_MOV(regmem(EBX,FRAME_SPREV),reg(EAX));
      // tsk->sparklist->snext = f;
      I_MOV(absmem(((int)tsk->sparklist)+FRAME_SNEXT),reg(EAX));

      LABEL(Ldone);
      LABEL(bplabels[0][addr]);
      break;
    }
    #endif
    #ifdef NATIVE_RETURN
    case OP_RETURN: {
      int Lvalueset = as->labels++;
      int Lretp = as->labels++;
      int Lafter_write_barrier = as->labels++;

      I_MOV(reg(ECX),regmem(EBP,FRAME_C));
      I_CMP(reg(ECX),imm(0));
      I_JZ(label(Lretp));

      /* Write barrier */
      I_MOV(reg(EAX),regmem(ECX,CELL_FLAGS));
      I_AND(reg(EAX),imm(FLAG_MATURE));
      I_CMP(reg(EAX),imm(0));
      I_JZ(label(Lafter_write_barrier));

      BEGIN_CALL(16);
      I_PUSH(regmem(EBP,FRAME_DATA+8*(instr->expcount-1)+4));
      I_PUSH(regmem(EBP,FRAME_DATA+8*(instr->expcount-1)));
      I_PUSH(reg(ECX));
      I_PUSH(imm((int)tsk));
      I_MOV(reg(EAX),imm((int)write_barrier_ifnew));
      I_CALL(reg(EAX));
      I_ADD(reg(ESP),imm(16));
      END_CALL;

      LABEL(Lafter_write_barrier);

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
        //   f->waitlnk = NULL;
        //   unblock_frame(tsk,f);
        // }

        int Lwhile = as->labels++;
        int Ldone = as->labels++;
        int Lresumedfetchers = as->labels++;
        I_MOV(reg(ESI),regmem(EDI,FRAME_WQ+WAITQUEUE_FRAMES));
        LABEL(Lwhile);
        {
          I_CMP(reg(ESI),imm(0));
          I_JZ(label(Ldone));

          I_MOV(reg(EAX),regmem(ESI,FRAME_WAITLNK));
          I_MOV(regmem(ESI,FRAME_WAITLNK),imm(0));

          // f->rnext = *tsk->runptr;
          I_MOV(regmem(ESI,FRAME_RNEXT),reg(EBP));
          // *tsk->runptr = f;
          I_MOV(reg(EBP),reg(ESI));
          I_MOV(reg(ESI),reg(EAX));
          I_JMP(label(Lwhile));
        }
        LABEL(Ldone);

        I_MOV(regmem(EDI,FRAME_WQ+WAITQUEUE_FRAMES),imm(0));

        /* resume fetchers */
        I_CMP(regmem(EDI,FRAME_WQ+WAITQUEUE_FETCHERS),imm(0));
        I_JZ(label(Lresumedfetchers));

        BEGIN_CALL(16);
        I_PUSH(regmem(EDI,FRAME_DATA+8*(instr->expcount-1)+4));
        I_PUSH(regmem(EDI,FRAME_DATA+8*(instr->expcount-1)));
        I_LEA(reg(EAX),regmem(EDI,FRAME_WQ));
        I_PUSH(reg(EAX));
        I_PUSH(imm((int)tsk));
        I_MOV(reg(EAX),imm((int)resume_fetchers));
        I_CALL(reg(EAX));
        I_ADD(reg(ESP),imm(16));
        END_CALL;

        LABEL(Lresumedfetchers);
      }

      // check_runnable()
      asm_check_runnable(tsk,as);


      // frame_free()
      {
        // _f->freelnk = tsk->freeframe;
        I_MOV(reg(EAX),absmem((int)&tsk->freeframe));
        I_MOV(regmem(EDI,FRAME_FREELNK),reg(EAX));

        // tsk->freeframe = _f;
        I_MOV(absmem((int)&tsk->freeframe),reg(EDI));
      }

      swap_in(tsk,as,bplabels,addr);
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
      LABEL(bplabels[0][addr]);
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
      I_CMP(reg(EBX),imm(CELL_CAP));
      I_JNE(label(Lnotcap));

      I_MOV(regmem(EBP,FRAME_INSTR),imm((int)(instr+1)));
      I_LEA(reg(EAX),regmem(EBP,FRAME_DATA+8*(instr->expcount-1)));
      I_JMP(label(Lcaperror));

      LABEL(Lnotcap);

      /* NIL - jump to target */
      I_CMP(reg(EBX),imm(CELL_NIL));
      LABEL(bplabels[0][addr]);
      I_JZ(label(instrlabels[addr+instr->arg0]));

      LABEL(Ltrue);
      LABEL(bplabels[1][addr]);
      break;
    }
    #endif
    #ifdef NATIVE_JUMP
    case OP_JUMP:
      LABEL(bplabels[0][addr]);
      I_JMP(label(instrlabels[addr+instr->arg0]));
      break;
    #endif
    #ifdef NATIVE_PUSH
    case OP_PUSH:
      I_MOV(reg(EAX),regmem(EBP,FRAME_DATA+8*instr->arg0));
      I_MOV(reg(EBX),regmem(EBP,FRAME_DATA+8*instr->arg0+4));
      I_MOV(regmem(EBP,FRAME_DATA+8*instr->expcount),reg(EAX));
      I_MOV(regmem(EBP,FRAME_DATA+8*instr->expcount+4),reg(EBX));
      LABEL(bplabels[0][addr]);
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
      LABEL(bplabels[0][addr]);
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

      asm_frame_new(tsk,as,STATE_NEW);

      // newf->retp = NULL;
      I_MOV(regmem(EDI,FRAME_RETP),imm(0));

      // newfholder = alloc_cell(tsk);

      asm_alloc_cell_to_esi(tsk,as);

      // newfholder->type = CELL_FRAME;
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
      LABEL(bplabels[0][addr]);
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
        LABEL(bplabels[0][addr]);
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

        I_MOV(reg(EAX),absmem((int)&tsk->globnilpntr.data[0]));
        I_MOV(reg(EBX),absmem((int)&tsk->globnilpntr.data[1]));
        I_MOV(regmem(EBP,FRAME_DATA+8*(instr->expcount-2)),reg(EAX));
        I_MOV(regmem(EBP,FRAME_DATA+8*(instr->expcount-2)+4),reg(EBX));
        I_JMP(label(Lend));

        LABEL(Ltrue);
        I_MOV(regmem(EBP,FRAME_DATA+8*(instr->expcount-2)),imm(trueval[0]));
        I_MOV(regmem(EBP,FRAME_DATA+8*(instr->expcount-2)+4),imm(trueval[1]));

        LABEL(Lend);
        LABEL(bplabels[0][addr]);
        break;
      }
      case B_SQRT:
        I_FLD_64(regmem(EBP,FRAME_DATA+8*(instr->expcount-1)));
        I_FSQRT();
        I_FSTP_64(regmem(EBP,FRAME_DATA+8*(instr->expcount-1)));
        LABEL(bplabels[0][addr]);
        break;
      case B_CONS:
        asm_alloc_cell_to_esi(tsk,as);
        I_MOV(regmem(ESI,CELL_TYPE),imm(CELL_CONS));

        I_MOV(reg(EAX),regmem(EBP,FRAME_DATA+8*(instr->expcount-1)));
        I_MOV(reg(EBX),regmem(EBP,FRAME_DATA+8*(instr->expcount-1)+4));
        I_MOV(regmem(ESI,CELL_FIELD1),reg(EAX));
        I_MOV(regmem(ESI,CELL_FIELD1+4),reg(EBX));

        I_MOV(reg(EAX),regmem(EBP,FRAME_DATA+8*(instr->expcount-2)));
        I_MOV(reg(EBX),regmem(EBP,FRAME_DATA+8*(instr->expcount-2)+4));
        I_MOV(regmem(ESI,CELL_FIELD2),reg(EAX));
        I_MOV(regmem(ESI,CELL_FIELD2+4),reg(EBX));

        I_MOV(regmem(EBP,FRAME_DATA+8*(instr->expcount-2)),reg(ESI));
        I_MOV(regmem(EBP,FRAME_DATA+8*(instr->expcount-2)+4),imm(PNTR_VALUE));

        LABEL(bplabels[0][addr]);
        break;
      case B_HEAD: {
        int Linvalid = as->labels++;
        int Lend = as->labels++;
        int Lnotcons = as->labels++;
        asm_jump_if_number(tsk,as,instr->expcount-1,Linvalid);
        I_MOV(reg(EAX),regmem(EBP,FRAME_DATA+8*(instr->expcount-1))); // EAX now points to the cell
        I_MOV(reg(EBX),regmem(EAX,CELL_TYPE)); // EBX = cell type

        I_CMP(reg(EBX),imm(CELL_CONS));
        I_JNE(label(Lnotcons));

        /* cons cell */
        I_MOV(reg(ECX),regmem(EAX,CELL_FIELD1));
        I_MOV(reg(EDX),regmem(EAX,CELL_FIELD1+4));

        I_MOV(regmem(EBP,FRAME_DATA+8*(instr->expcount-1)),reg(ECX));
        I_MOV(regmem(EBP,FRAME_DATA+8*(instr->expcount-1)+4),reg(EDX));
        I_JMP(label(Lend));

        LABEL(Lnotcons);
        I_CMP(reg(EBX),imm(CELL_AREF));
        I_JNE(label(Linvalid));

        /* array reference */

        // EBX = array
        I_MOV(reg(ECX),regmem(EAX,CELL_FIELD1));
        // EDX = index
        I_MOV(reg(EDX),regmem(EBP,FRAME_DATA+8*(instr->expcount-1)+4));
        I_AND(reg(EDX),imm(INDEX_MASK));

        I_CMP(regmem(ECX,CARRAY_ELEMSIZE),imm(sizeof(pntr)));
        int Lcharelem = as->labels++;
        I_JNE(label(Lcharelem));
        /* pntr elements */

        I_MOV(reg(EAX),regmemscaled(ECX,CARRAY_ELEMENTS,SCALE_8,EDX));
        I_MOV(reg(EBX),regmemscaled(ECX,CARRAY_ELEMENTS+4,SCALE_8,EDX));

        I_MOV(regmem(EBP,FRAME_DATA+8*(instr->expcount-1)),reg(EAX));
        I_MOV(regmem(EBP,FRAME_DATA+8*(instr->expcount-1)+4),reg(EBX));

        I_JMP(label(Lend));

        /* char elements */
        LABEL(Lcharelem);

        // -3 is to avoid going beyond end of allocated memory
        I_MOV(reg(EAX),regmemscaled(ECX,CARRAY_ELEMENTS-3,SCALE_1,EDX));
        I_SHR(reg(EAX),imm(24));
        I_MOV(absmem((int)&tsk->itransfer),reg(EAX));
        I_FILD(absmem((int)&tsk->itransfer));

        I_FSTP_64(regmem(EBP,FRAME_DATA+8*(instr->expcount-1)));

        I_JMP(label(Lend));

        LABEL(Linvalid);
        I_MOV(regmem(EBP,FRAME_INSTR),imm((int)(instr+1)));
        I_JMP(label(Lheaderror));

        LABEL(Lend);
        LABEL(bplabels[0][addr]);
        break;
      }
      default:
        I_MOV(regmem(EBP,FRAME_INSTR),imm((int)(instr+1)));
        I_LEA(reg(ECX),regmem(EBP,FRAME_DATA+8*(instr->expcount-nargs)));
        runnable_owner_task(tsk,as);

        BEGIN_CALL(8);
        I_PUSH(reg(ECX));
        I_PUSH(imm((int)tsk));
        I_MOV(reg(EAX),imm((int)builtin_info[bif].f));
        I_CALL(reg(EAX));
        I_ADD(reg(ESP),imm(8));
        END_CALL;

        runnable_owner_native_code(tsk,as);
        swap_in(tsk,as,bplabels,addr);
        break;
      }
      break;
    }
    #endif
    #ifdef NATIVE_PUSHNIL
    case OP_PUSHNIL: {
      // runnable->data[instr->expcount] = tsk->globnilpntr;
      I_MOV(reg(EAX),absmem((int)&tsk->globnilpntr.data[0]));
      I_MOV(reg(EBX),absmem((int)&tsk->globnilpntr.data[1]));
      I_MOV(regmem(EBP,FRAME_DATA+8*instr->expcount),reg(EAX));
      I_MOV(regmem(EBP,FRAME_DATA+8*instr->expcount+4),reg(EBX));
      LABEL(bplabels[0][addr]);
      break;
    }
    #endif
    #ifdef NATIVE_PUSHNUMBER
    case OP_PUSHNUMBER: {
      // runnable->data[instr->expcount] = *((double*)&instr->arg0);
      I_MOV(regmem(EBP,FRAME_DATA+8*instr->expcount),imm(instr->arg0));
      I_MOV(regmem(EBP,FRAME_DATA+8*instr->expcount+4),imm(instr->arg1));
      LABEL(bplabels[0][addr]);
      break;
    }
    #endif
    #ifdef NATIVE_PUSHSTRING
    case OP_PUSHSTRING: {
      // runnable->data[instr->expcount] = tsk->strings[instr->arg0];
      I_MOV(reg(EAX),absmem((int)&tsk->strings[instr->arg0].data[0]));
      I_MOV(reg(EBX),absmem((int)&tsk->strings[instr->arg0].data[1]));
      I_MOV(regmem(EBP,FRAME_DATA+8*instr->expcount),reg(EAX));
      I_MOV(regmem(EBP,FRAME_DATA+8*instr->expcount+4),reg(EBX));
      LABEL(bplabels[0][addr]);
      break;
    }
    #endif
    #ifdef NATIVE_POP
    case OP_POP:
      /* NOOP */
      LABEL(bplabels[0][addr]);
      break;
    #endif
    //#ifdef NATIVE_ERROR
    case OP_ERROR:
      I_MOV(absmem((int)&tsk->native_finished),imm(1));
      LABEL(bplabels[0][addr]);
      I_JMP(label(Largerror));
      break;
    //#endif
    #ifdef NATIVE_EVAL
    case OP_EVAL: {
      int Ldone = as->labels++;
      int Lswapin = as->labels++;
      int Lcheckref = as->labels++;
      int pos = instr->arg0;

      // RUNNABLE->data[INSTR->arg0] = resolve_pntr(RUNNABLE->data[INSTR->arg0]);
      asm_resolve_stack_pntr(tsk,as,pos); // uses EAX, EBX, ECX, ECX
      // now EBX == type, EAX == cell pointer (if not number)
      I_CMP(reg(EBX),imm(CELL_NUMBER));
      I_JZ(label(Ldone));
      I_CMP(reg(EBX),imm(CELL_FRAME));
      I_JNE(label(Lcheckref));

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
      I_MOV(reg(EBP),reg(EAX));

      // run_frame(tsk,newf);
      int Lafterrun = as->labels++;
      I_CMP(regmem(EDI,FRAME_STATE),imm(STATE_ACTIVE));
      I_JZ(label(Lafterrun));

#ifndef DISABLE_SPARKS
      int Lnotsparked = as->labels++;
      I_CMP(regmem(EDI,FRAME_STATE),imm(STATE_SPARKED));
      I_JNE(label(Lnotsparked));

      /* remove_spark */

      // EDI = frame being unsparked
      // EAX = previous frame
      // EBX = next frame

      // frame *prev = f->sprev;
      I_MOV(reg(EAX),regmem(EDI,FRAME_SPREV));
      // frame *next = f->snext;
      I_MOV(reg(EBX),regmem(EDI,FRAME_SNEXT));
      // prev->snext = next;
      I_MOV(regmem(EAX,FRAME_SNEXT),reg(EBX));
      // next->sprev = prev;
      I_MOV(regmem(EBX,FRAME_SPREV),reg(EAX));

      // f->sprev = NULL;
      I_MOV(regmem(EDI,FRAME_SPREV),imm(0));
      // f->snext = NULL;
      I_MOV(regmem(EDI,FRAME_SNEXT),imm(0));

      LABEL(Lnotsparked);
#endif

      I_MOV(regmem(EDI,FRAME_RNEXT),reg(EBP));
      I_MOV(regmem(EDI,FRAME_STATE),imm(STATE_ACTIVE));
      I_MOV(reg(EBP),reg(EDI));

      LABEL(Lafterrun);

      // check_runnable(tsk);
      asm_check_runnable(tsk,as);
      I_JMP(label(Lswapin));

      LABEL(Lcheckref);
      I_CMP(reg(EBX),imm(CELL_REMOTEREF));
      I_JNE(label(Ldone));

      I_MOV(regmem(EBP,FRAME_INSTR),imm((int)(instr+1)));
      I_MOV(reg(EDI),reg(EBP));

      runnable_owner_task(tsk,as);

      BEGIN_CALL(16);
      I_PUSH(regmem(EDI,FRAME_DATA+8*pos+4));
      I_PUSH(regmem(EDI,FRAME_DATA+8*pos));
      I_PUSH(reg(EDI));
      I_PUSH(imm((int)tsk));
      I_MOV(reg(EAX),imm((int)eval_remoteref));
      I_CALL(reg(EAX));
      I_ADD(reg(ESP),imm(16));
      END_CALL;

      runnable_owner_native_code(tsk,as);
      asm_check_runnable(tsk,as);

      LABEL(Lswapin);
      swap_in(tsk,as,bplabels,addr);
      I_NOP(); /* pad to 5 bytes between labels */
      I_NOP();
      I_NOP();

      LABEL(Ldone);
      LABEL(bplabels[1][addr]);
      break;
    }
    #endif
    #ifdef NATIVE_CALL
    case OP_CALL: {
      const funinfo *program_finfo = bc_funinfo(tsk->bcdata);
      int fno = instr->arg0;
      int n = instr->arg1;

      // newf = frame_new(tsk);
      asm_frame_new(tsk,as,STATE_ACTIVE);
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

      /* Update the current frames instruction pointer to refer to the next instruction
         after this one. */
      I_MOV(regmem(EBP,FRAME_INSTR),imm((int)(instr+1)));

      // curf->waitlnk = newf->wq.frames;
      I_MOV(reg(EAX),regmem(EDI,FRAME_WQ+WAITQUEUE_FRAMES));
      I_MOV(regmem(EBP,FRAME_WAITLNK),reg(EAX));
      // newf->wq.frames = curf;
      I_MOV(regmem(EDI,FRAME_WQ+WAITQUEUE_FRAMES),reg(EBP));

      /* Put the current frame into the blocked state and remove it from the run queue.
         Add the new frame on to the run queue. Note: the state of the new frame was
         already initialised to STATE_NEW when we created the frame above. */
      I_MOV(reg(EAX),regmem(EBP,FRAME_RNEXT));
      I_MOV(regmem(EBP,FRAME_RNEXT),imm(0));
      I_MOV(regmem(EDI,FRAME_RNEXT),reg(EAX));

      /* Jump directly to the new frame, instead of calling swap_in() */
      I_MOV(reg(EBP),reg(EDI));
      LABEL(bplabels[0][addr]);
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
      LABEL(bplabels[0][addr]);
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
    #ifdef NATIVE_JEQ
    case OP_JEQ: {
      I_FILD(absmem((int)&(instr->arg1)));
      I_FLD_64(regmem(EBP,FRAME_DATA+8*(instr->expcount-1)));
      I_FUCOMPP();
      I_FNSTSW_AX();
      I_SAHF();
      LABEL(bplabels[0][addr]);
      I_JZ(label(instrlabels[addr+instr->arg0]));
      break;
    }
    #endif
    case OP_CONSN: {
      int n = instr->arg0;
      int i;

      // carray *arr = carray_new(tsk,sizeof(pntr),n-1,NULL,NULL);
      // EDI: arr
      BEGIN_CALL(20);
      I_PUSH(imm(0));
      I_PUSH(imm(0));
      I_PUSH(imm(n-1));
      I_PUSH(imm(sizeof(pntr)));
      I_PUSH(imm((int)tsk));
      I_MOV(reg(EAX),imm((int)carray_new));
      I_CALL(reg(EAX));
      I_ADD(reg(ESP),imm(20));
      I_MOV(regmem(ESP,32-PUSHAD_OFFSET_EDI),reg(EAX));
      END_CALL;

      for (i = 0; i < n-1; i++) {
        // ((pntr*)arr->elements)[i] = runnable->data[instr->expcount-i-1];
        I_MOV(reg(EAX),regmem(EBP,FRAME_DATA+8*(instr->expcount-i-1)));
        I_MOV(reg(EBX),regmem(EBP,FRAME_DATA+8*(instr->expcount-i-1)+4));
        I_MOV(regmem(EDI,CARRAY_ELEMENTS+8*i),reg(EAX));
        I_MOV(regmem(EDI,CARRAY_ELEMENTS+8*i+4),reg(EBX));
      }

      // arr->size = n-1;
      I_MOV(regmem(EDI,CARRAY_SIZE),imm(n-1));

      // arr->tail = runnable->data[instr->expcount-n];
      I_MOV(reg(EAX),regmem(EBP,FRAME_DATA+8*(instr->expcount-n)));
      I_MOV(reg(EBX),regmem(EBP,FRAME_DATA+8*(instr->expcount-n)+4));
      I_MOV(regmem(EDI,CARRAY_TAIL),reg(EAX));
      I_MOV(regmem(EDI,CARRAY_TAIL+4),reg(EBX));

      // make_aref_pntr(runnable->data[instr->expcount-n],arr->wrapper,0);
      I_MOV(reg(EAX),regmem(EDI,CARRAY_WRAPPER));
      I_MOV(regmem(EBP,FRAME_DATA+8*(instr->expcount-n)),reg(EAX));
      I_MOV(regmem(EBP,FRAME_DATA+8*(instr->expcount-n)+4),imm(PNTR_VALUE));

      LABEL(bplabels[0][addr]);
      break;
    }
    case OP_ITEMN: {
      int Lfallback = as->labels++;
      int Ldone = as->labels++;
      int pos = instr->arg0;

      /* Check if it's a pointer */
      asm_jump_if_number(tsk,as,instr->expcount-1,Lfallback); // uses EAX

      // if (0 == index)
      {
        I_MOV(reg(EBX),regmem(EBP,FRAME_DATA+8*(instr->expcount-1)+4));
        I_AND(reg(EBX),imm(INDEX_MASK));
        I_CMP(reg(EBX),imm(0));
        I_JNE(label(Lfallback));

        I_MOV(reg(EAX),regmem(EBP,FRAME_DATA+8*(instr->expcount-1)));
        I_CMP(regmem(EAX,CELL_TYPE),imm(CELL_AREF));
        I_JNE(label(Lfallback));

        I_MOV(reg(EAX),regmem(EAX,CELL_FIELD1));

        // if (sizeof(pntr) == arr->elemsize)
        I_CMP(regmem(EAX,CARRAY_ELEMSIZE),imm(sizeof(pntr)));
        I_JNE(label(Lfallback));
        {
          // if (pos < arr->size)
          {
            I_ADD(reg(EBX),imm(pos));
            I_CMP(reg(EBX),regmem(EAX,CARRAY_SIZE));
            I_JGE(label(Lfallback));

            I_MOV(reg(ECX),regmemscaled(EAX,CARRAY_ELEMENTS,SCALE_8,EBX));
            I_MOV(reg(EDX),regmemscaled(EAX,CARRAY_ELEMENTS+4,SCALE_8,EBX));
            I_MOV(regmem(EBP,FRAME_DATA+8*(instr->expcount-1)),reg(ECX));
            I_MOV(regmem(EBP,FRAME_DATA+8*(instr->expcount-1)+4),reg(EDX));
            I_JMP(label(Ldone));
          }
        }
      }

      LABEL(Lfallback);

      BEGIN_CALL(16);
      I_PUSH(imm(pos));
      I_PUSH(imm(instr->expcount));
      I_PUSH(reg(EBP));
      I_PUSH(imm((int)tsk));
      I_MOV(reg(EAX),imm((int)make_item_frame));
      I_CALL(reg(EAX));
      I_ADD(reg(ESP),imm(16));
      END_CALL;

      LABEL(Ldone);
      LABEL(bplabels[0][addr]);
      break;
    }
    case OP_INVALID:
      PRINT_MESSAGE("INVALID INSTRUCTION\n");
      I_MOV(absmem((int)&tsk->native_finished),imm(1));
      LABEL(bplabels[0][addr]);
      I_JMP(label(Lfinished));
      break;
    default: {
      I_MOV(regmem(EBP,FRAME_INSTR),imm((int)(instr+1)));
      runnable_owner_task(tsk,as);

      /* Call the appropriate op_* function */
      BEGIN_CALL(12);
      I_PUSH(imm((int)(instr)));
      I_PUSH(absmem((int)tsk->runptr));
      I_PUSH(imm((int)tsk));
      I_MOV(reg(EAX),imm((int)op_handlers[instr->opcode]));
      I_CALL(reg(EAX));
      I_ADD(reg(ESP),imm(12));
      END_CALL;

      runnable_owner_native_code(tsk,as);
      swap_in(tsk,as,bplabels,addr);
      break;
    }
    }
  }

  LABEL(Lbcend);
  LABEL(Lfinished);
  I_POPAD();
  I_RET();

  /************** Trap ************/
  LABEL(Ltrap);
  I_PUSHF();
  runnable_owner_task(tsk,as);
  I_PUSHAD();

  /* Decrement the saved EIP by 5 bytes to put it back where the temporary CALL starts */
  I_MOV(reg(EAX),absmem((int)&tsk->normal_esp));
  I_SUB(regmem(EAX,-4),imm(5));

  /* Get EIP */
  I_MOV(reg(EAX),regmem(EAX,-4));
  /* Get EAX (== jump target if we came from swap_in) */
  I_MOV(reg(EBX),regmem(ESP,32-PUSHAD_OFFSET_EAX));

  BEGIN_CALL(20); /* note: 8 extra bytes because of EIP pushed by caller and the PUSHF above */
  I_PUSH(reg(EBX));
  I_PUSH(reg(EAX));
  I_PUSH(imm((int)tsk));
  I_MOV(reg(EAX),imm((int)handle_trap));
  I_CALL(reg(EAX));
  I_ADD(reg(ESP),imm(12));
  END_CALL;

  I_CMP(absmem((int)&tsk->done),imm(0));
  I_JZ(label(Lstillactive));

  I_POPAD();
  runnable_owner_native_code(tsk,as);
  I_POPF();
  I_ADD(reg(ESP),imm(4));
  I_JMP(label(Lbcend));

  LABEL(Lstillactive);

  /* Cause the JMP at the end of swap_in to jump to the interrupt return eip */
  I_MOV(reg(EAX),absmem((int)&tsk->interrupt_return_eip));
  I_MOV(regmem(ESP,32-PUSHAD_OFFSET_EAX),reg(EAX));

  I_POPAD();
  runnable_owner_native_code(tsk,as);
  I_POPF();
  I_RET();

  /************** head_error ************/
  LABEL(Lheaderror);
  runnable_owner_task(tsk,as);
  BEGIN_CALL(4);
  I_PUSH(imm((int)tsk));
  I_MOV(reg(EAX),imm((int)native_head_error));
  I_CALL(reg(EAX));
  I_ADD(reg(ESP),imm(4));
  END_CALL;
  I_JMP(label(Lfinished));

  /************** cap_error ************/
  LABEL(Lcaperror);
  // Caller has set EAX = pntr* referring to offending value
  runnable_owner_task(tsk,as);
  BEGIN_CALL(12);
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
  BEGIN_CALL(4);
  I_PUSH(imm((int)tsk));
  I_MOV(reg(EAX),imm((int)native_arg_error));
  I_CALL(reg(EAX));
  I_ADD(reg(ESP),imm(4));
  END_CALL;
  I_JMP(label(Lfinished));

  /* Done compiling; now assemble */
  cpucode = array_new(1,0);
  x86_assemble(as,cpucode);
/*   dump_asm(tsk,"output.s",cpucode); */

  tsk->codesize = cpucode->nbytes;
  tsk->code = (char*)valloc(tsk->codesize);
  memcpy(tsk->code,cpucode->data,tsk->codesize);
  mprotect(tsk->code,tsk->codesize,PROT_READ|PROT_WRITE|PROT_EXEC);
  array_free(cpucode);

  tsk->cpu_to_bcaddr = (int*)calloc(tsk->codesize,sizeof(int));
  int last = 0;

  for (addr = 0; addr < bch->nops; addr++) {
    int cpuaddr = as->instructions[tempaddrs[addr]].addr;
    int bplabel1 = bplabels[0][addr];
    int bplabel2 = bplabels[1][addr];
    program_ops[addr].code = (void*)(((char*)tsk->code)+cpuaddr);

    assert(as->labeladdrs[bplabel1]);
    tsk->bpaddrs[0][addr] = ((unsigned char*)tsk->code)+as->labeladdrs[bplabel1];
    if (as->labeladdrs[bplabel2]) {
      tsk->bpaddrs[1][addr] = ((unsigned char*)tsk->code)+as->labeladdrs[bplabel2];

      /* ensure sufficient space between breakpoint addresses to insert CALL */
      assert(5 <= abs(as->labeladdrs[bplabel2]-as->labeladdrs[bplabel1]));
    }

    if (0 == addr) {
      for (i = last; i < cpuaddr; i++)
        tsk->cpu_to_bcaddr[i] = 0;
    }
    else {
      for (i = last; i < cpuaddr; i++)
        tsk->cpu_to_bcaddr[i] = addr-1;
    }
    last = cpuaddr;
  }
  for (i = last; i < tsk->codesize; i++)
    tsk->cpu_to_bcaddr[i] = addr-1;

  tsk->bcend_addr = tsk->code+as->labeladdrs[Lbcend];
  tsk->trap_addr = tsk->code+as->labeladdrs[Ltrap];
  tsk->headerror_addr = tsk->code+as->labeladdrs[Lheaderror];
  tsk->caperror_addr = tsk->code+as->labeladdrs[Lcaperror];
  tsk->argerror_addr = tsk->code+as->labeladdrs[Largerror];

  free(tempaddrs);
  free(bplabels[0]);
  free(bplabels[1]);
  free(instrlabels);
  x86_assembly_free(as);
}
