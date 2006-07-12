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

#include "grammar.tab.h"
#include "gcode.h"
#include "nreduce.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <assert.h>
#include <ctype.h>
#include <stdarg.h>
#include <math.h>

extern cell **globcells;
extern int repl_histogram[NUM_CELLTYPES];

static int address = 0;
static int exec_stackbase = 0;

int get_fno_from_address(int address)
{
  int fno = 0;
  while ((fno < nfunctions) && (addressmap[fno] != address))
    fno++;
  assert(fno < nfunctions);
  return fno;
}

void trace_address(ginstr *program, int address, int nargs)
{
  int fno = get_fno_from_address(address);

  if (trace) {
    debug(0,"\n");
    if (NUM_BUILTINS > fno)
      debug(0,"0      Function %d = %s",fno,builtin_info[fno].name);
    else
      debug(0,"0      Function %d = %s",fno,get_scomb_index(fno-NUM_BUILTINS)->name);
    debug(0,", address %d, nargs = %d, stackbase = %d\n",
          address,nargs,stackbase);
  }
}

void insufficient_args(ginstr *program, int address, cell *target, int nargs)
{
  int fno = get_fno_from_address(address);
  int reqargs = program[address].arg1;
  if (NUM_BUILTINS > fno)
    fprintf(stderr,"Built-in function %s requires %d args; have only %d\n",
            builtin_info[fno].name,reqargs,nargs);
  else
    fprintf(stderr,"Supercombinator %s requires %d args; have only %d\n",
    get_scomb_index(fno-NUM_BUILTINS)->name,reqargs,nargs);
  exit(1);
}

int is_whnf(cell *c)
{
  c = resolve_ind(c);
  if ((TYPE_APPLICATION == celltype(c)) ||
      (TYPE_FUNCTION == celltype(c))) {
    int nargs = 0;
    while (TYPE_APPLICATION == celltype(c)) {
      c = resolve_ind((cell*)c->field1);
      nargs++;
    }
    if (TYPE_FUNCTION == celltype(c)) {
      if (nargs >= (int)c->field1)
        return 0;
    }
  }
  return 1;
}

void check_stack(gprogram *gp, int address)
{
  ginstr *instr = &gp->ginstrs[address];
  if (0 > instr->expcount)
    return;

  if (stackcount-exec_stackbase != instr->expcount) {
    printf("Instruction %d expects stack frame size %d, actually %d\n",
           address,instr->expcount,stackcount-exec_stackbase);
    abort();
  }

  int i;
  for (i = 0; i < instr->expcount; i++) {
    cell *c = stackat(exec_stackbase+i);
    int actualstatus = is_whnf(c);
    if (instr->expstatus[i] && !actualstatus) {
      printf("Instruction %d expects stack frame entry %d (%d) to be evald but it's "
             "actually a %s\n",address,i,exec_stackbase+i,cell_types[celltype(c)]);
      abort();
    }
  }
}

cell *unwind_part1()
{
  stackcount = stackbase+1;
  cell *c = stack[stackcount-1];
  c = resolve_ind(c);
  while (TYPE_APPLICATION == celltype(c)) {
    c = resolve_ind((cell*)c->field1);
    push(c);
  }
  return c;
}

int unwind_part2(cell *bottom)
{
  if (TYPE_FUNCTION != celltype(bottom))
    return 0;

  int argspos = stackcount-(int)bottom->field1;
  if (argspos < stackbase+1)
    return 0;

  int funaddr = (int)bottom->field2;
  int pos;
  assert(!is_whnf(stack[argspos-1]));
  for (pos = stackcount-1; pos >= argspos; pos--) {
    cell *argval = resolve_ind(stack[pos-1]);
    stack[pos] = resolve_ind((cell*)argval->field2);
  }

  stack[argspos-1] = resolve_ind(stack[argspos-1]);

  address = funaddr;
  exec_stackbase = argspos-1;
  return 1;
}

void do_return()
{
  dumpentry *edx = &dumpstack[dumpcount-1];
  assert(is_whnf(stack[stackcount-1]));
  stackbase = edx->stackbase;
  stackcount = edx->stackcount;
  address = edx->address;
  exec_stackbase = edx->sb;
  dumpcount--;
  assert(0 <= dumpcount);
}

void do_mkap(int n)
{
  for (n--; n >= 0; n--) {
    cell *left = stack[stackcount-1];
    cell *right = stack[stackcount-2];
    stackcount--;
    stack[stackcount-1] = alloc_cell2(TYPE_APPLICATION,left,right);
  }
}

void do_update(int n)
{
  cell *res;
  cell *target;
  assert(n < stackcount);
  assert(n > 0);

  res = resolve_ind(stack[stackcount-1-n]);

  if (res->tag & FLAG_PINNED) {
    stack[stackcount-1-n] = alloc_cell();
    res = stack[stackcount-1-n];
  }

  assert(!(res->tag & FLAG_PINNED));
  free_cell_fields(res);
  target = res;
  repl_histogram[celltype(target)]++;

  target->tag = TYPE_IND;
  target->field1 = resolve_ind(stack[stackcount-1]);
  stackcount--;
}

void execute(gprogram *gp)
{
  assert(0 == stackbase);
  while (1) {
    ginstr *instr = &gp->ginstrs[address];

#ifdef STACK_MODEL_SANITY_CHECK
    check_stack(gp,address);
#endif

    if (trace) {
      print_ginstr(address,instr,0);
      print_stack(-1,stack,stackcount,0);
    }

    if (nallocs > COLLECT_THRESHOLD)
      collect();

    op_usage[instr->opcode]++;
    instr->usage++;

    switch (instr->opcode) {
    case OP_BEGIN:
      break;
    case OP_END:
      return;
    case OP_GLOBSTART:
      break;
    case OP_EVAL: {
      cell *c;
      assert(0 <= stackcount-1-instr->arg0);
      c = resolve_ind(stack[stackcount-1-instr->arg0]);
      if ((TYPE_APPLICATION == celltype(c)) ||
          (TYPE_FUNCTION == celltype(c))) {
        dumpentry *de = pushdump();
        ndumpallocs++;
        de->stackbase = stackbase;
        de->stackcount = stackcount;
        de->address = address;
        de->sb = exec_stackbase;
        push(stack[stackcount-1-instr->arg0]);
        stackbase = stackcount-1;
        exec_stackbase = stackbase;
      }
      else {
        break;
      }
      /* fall through */
    }
    case OP_UNWIND: {
      cell *ebx = unwind_part1();
      if (unwind_part2(ebx))
        break;

      /* fall through */
    }
    case OP_RETURN: {
      do_return();
      break;
    }
    case OP_PUSH: {
      assert(instr->arg0 < stackcount);
      push(resolve_ind(stack[stackcount-1-instr->arg0]));
      break;
    }
    case OP_PUSHGLOBAL: {
      int fno = instr->arg0;

      // If the function is a supercombinator with 0 args (i.e. a CAF), then
      // push a *copy* of the cell. This avoids supercombinators which evaluate
      // to infinite (or large) lists from not ever being garbage collected.
      if ((fno >= NUM_BUILTINS) &&
          (0 == get_scomb_index(fno-NUM_BUILTINS)->nargs)) {
        cell *src = globcells[fno];
        assert(TYPE_FUNCTION == celltype(src));
        push(alloc_cell2(TYPE_FUNCTION,src->field1,src->field2));
      }
      // If the function is a supercombinator and arg1 is non-zero, this means we
      // actually want a reference to the "no-eval" part of the supercombinator, i.e.
      // we skip the EVAL instructions at the start, under the assumption that the
      // arguments to be passed to it have already been evaluated
      else if ((fno >= NUM_BUILTINS) && instr->arg1) {
        cell *src = globcells[fno];
        assert(TYPE_FUNCTION == celltype(src));
        assert((int)src->field2 == addressmap[fno]);
        push(alloc_cell2(TYPE_FUNCTION,src->field1,(void*)noevaladdressmap[fno]));
      }
      else {
        push(globcells[fno]);
      }
      break;
    }
    case OP_MKAP:
      do_mkap(instr->arg0);
      break;
    case OP_UPDATE:
      do_update(instr->arg0);
      break;
    case OP_POP:
      stackcount -= instr->arg0;
      assert(0 <= stackcount);
      break;
    case OP_PRINT:
      print_cell(resolve_ind(stack[stackcount-1]));
      stackcount--;
      break;
    case OP_BIF: {
      int i;
      assert(0 <= builtin_info[instr->arg0].nargs); /* should we support 0-arg bifs? */
      for (i = 0; i < builtin_info[instr->arg0].nstrict; i++) {
        stack[stackcount-1-i] = resolve_ind(stack[stackcount-1-i]); /* bifs expect it */
        assert(TYPE_APPLICATION != celltype(stack[stackcount-1-i]));
      }
      if (trace)
        debug(0,"  builtin %s\n",builtin_info[instr->arg0].name);
      builtin_info[instr->arg0].f();
      break;
    }
    case OP_JFALSE: {
      cell *test = resolve_ind(stack[stackcount-1]);
      assert(TYPE_APPLICATION != celltype(test));
      if (TYPE_NIL == celltype(test))
        address += instr->arg0-1;
      stackcount--;
      break;
    }
    case OP_JUMP:
      address += instr->arg0-1;
      break;
    case OP_JEMPTY:
      if (0 == stackcount)
        address += instr->arg0-1;
      break;
    case OP_ISTYPE:
      if (celltype(resolve_ind(stack[stackcount-1])) == instr->arg0)
        stack[stackcount-1] = globtrue;
      else
        stack[stackcount-1] = globnil;
      break;
    case OP_ALLOC: {
      int i;
      /* FIXME: perhaps we should make a HOLE type instead of using nil here... then it
         should be save to use globnil everywhere we normally use TYPE_NIL */
      nALLOCs++;
      for (i = 0; i < instr->arg0; i++) {
        push(alloc_cell2(TYPE_NIL,NULL,NULL));
        nALLOCcells++;
      }
      break;
    }
    case OP_SQUEEZE: {
      assert(0 <= stackcount-instr->arg0-instr->arg1);
      int base = stackcount-instr->arg0-instr->arg1;
      int i;
      for (i = 0; i <= instr->arg0; i++)
        stack[base+i] = stack[base+i+instr->arg1];
      stackcount -= instr->arg1;
      break;
    }
    case OP_DISPATCH: {
      cell *fun = resolve_ind(stack[stackcount-1]);
      if (TYPE_FUNCTION == celltype(fun)) {
        int nargs = (int)fun->field1;
        int addr = (int)fun->field2;
        assert(OP_GLOBSTART == gp->ginstrs[addr].opcode);

        if (nargs == 0) {
          ndisp0++;
        }
        else if (nargs == instr->arg0) {
          ndispexact++;
          stackcount--;
          address = (int)fun->field2;
          break;
        }
        else if (nargs < instr->arg0) {
          ndispless++;
        }
        else { // nargs > instr->arg0
          ndispgreater++;
        }
      }
      else {
        ndispother++;
      }

      do_mkap(instr->arg0);
      do_update(1);

      cell *bottom = unwind_part1();
      if (unwind_part2(bottom))
        break;
      do_return();
      break;
    }
    case OP_PUSHNIL:
      push(globnil);
      break;
    case OP_PUSHINT:
      push(alloc_cell2(TYPE_INT,(void*)instr->arg0,NULL));
      break;
    case OP_PUSHDOUBLE:
      push(alloc_cell2(TYPE_DOUBLE,(void*)instr->arg0,(void*)instr->arg1));
      break;
    case OP_PUSHSTRING:
      push(((cell**)gp->stringmap->data)[instr->arg0]);
      break;
    default:
      assert(0);
      break;
    }

    address++;
  }
}
