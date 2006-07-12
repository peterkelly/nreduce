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

void trace_address(stack *s, ginstr *program, int address, int nargs)
{
  int fno = get_fno_from_address(address);

  if (trace) {
    debug(0,"\n");
    if (NUM_BUILTINS > fno)
      debug(0,"0      Function %d = %s",fno,builtin_info[fno].name);
    else
      debug(0,"0      Function %d = %s",fno,get_scomb_index(fno-NUM_BUILTINS)->name);
    debug(0,", address %d, nargs = %d, stackbase = %d\n",
          address,nargs,s->base);
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

void check_stack(stack *s, gprogram *gp, int address)
{
  ginstr *instr = &gp->ginstrs[address];
  if (0 > instr->expcount)
    return;

  if (s->count-exec_stackbase != instr->expcount) {
    printf("Instruction %d expects stack frame size %d, actually %d\n",
           address,instr->expcount,s->count-exec_stackbase);
    abort();
  }

  int i;
  for (i = 0; i < instr->expcount; i++) {
    cell *c = stack_at(s,exec_stackbase+i);
    int actualstatus = is_whnf(c);
    if (instr->expstatus[i] && !actualstatus) {
      printf("Instruction %d expects stack frame entry %d (%d) to be evald but it's "
             "actually a %s\n",address,i,exec_stackbase+i,cell_types[celltype(c)]);
      abort();
    }
  }
}

cell *unwind_part1(stack *s)
{
  s->count = s->base+1;
  cell *c = s->data[s->count-1];
  c = resolve_ind(c);
  while (TYPE_APPLICATION == celltype(c)) {
    c = resolve_ind((cell*)c->field1);
    stack_push(s,c);
  }
  return c;
}

int unwind_part2(stack *s, cell *bottom)
{
  if (TYPE_FUNCTION != celltype(bottom))
    return 0;

  int argspos = s->count-(int)bottom->field1;
  if (argspos < s->base+1)
    return 0;

  int funaddr = (int)bottom->field2;
  int pos;
  assert(!is_whnf(s->data[argspos-1]));
  for (pos = s->count-1; pos >= argspos; pos--) {
    cell *argval = resolve_ind(s->data[pos-1]);
    s->data[pos] = resolve_ind((cell*)argval->field2);
  }

  s->data[argspos-1] = resolve_ind(s->data[argspos-1]);

  address = funaddr;
  exec_stackbase = argspos-1;
  return 1;
}

void do_return(stack *s)
{
  dumpentry *edx = &dumpstack[dumpcount-1];
  assert(is_whnf(s->data[s->count-1]));
  s->base = edx->stackbase;
  s->count = edx->stackcount;
  address = edx->address;
  exec_stackbase = edx->sb;
  dumpcount--;
  assert(0 <= dumpcount);
}

void do_mkap(stack *s, int n)
{
  for (n--; n >= 0; n--) {
    cell *left = s->data[s->count-1];
    cell *right = s->data[s->count-2];
    s->count--;
    s->data[s->count-1] = alloc_cell2(TYPE_APPLICATION,left,right);
  }
}

void do_update(stack *s, int n)
{
  cell *res;
  cell *target;
  assert(n < s->count);
  assert(n > 0);

  res = resolve_ind(s->data[s->count-1-n]);

  if (res->tag & FLAG_PINNED) {
    s->data[s->count-1-n] = alloc_cell();
    res = s->data[s->count-1-n];
  }

  assert(!(res->tag & FLAG_PINNED));
  free_cell_fields(res);
  target = res;
  repl_histogram[celltype(target)]++;

  target->tag = TYPE_IND;
  target->field1 = resolve_ind(s->data[s->count-1]);
  s->count--;
}

void execute(gprogram *gp)
{
  stack *s = stack_new();

  while (1) {
    ginstr *instr = &gp->ginstrs[address];

#ifdef STACK_MODEL_SANITY_CHECK
    check_stack(s,gp,address);
#endif

    if (trace) {
      print_ginstr(address,instr,0);
      print_stack(-1,s->data,s->count,0);
    }

    if (nallocs > COLLECT_THRESHOLD)
      collect();

    op_usage[instr->opcode]++;
    instr->usage++;

    switch (instr->opcode) {
    case OP_BEGIN:
      break;
    case OP_END:
      stack_free(s);
      return;
    case OP_GLOBSTART:
      break;
    case OP_EVAL: {
      cell *c;
      assert(0 <= s->count-1-instr->arg0);
      c = resolve_ind(s->data[s->count-1-instr->arg0]);
      if ((TYPE_APPLICATION == celltype(c)) ||
          (TYPE_FUNCTION == celltype(c))) {
        dumpentry *de = pushdump();
        ndumpallocs++;
        de->stackbase = s->base;
        de->stackcount = s->count;
        de->address = address;
        de->sb = exec_stackbase;
        stack_push(s,s->data[s->count-1-instr->arg0]);
        s->base = s->count-1;
        exec_stackbase = s->base;
      }
      else {
        break;
      }
      /* fall through */
    }
    case OP_UNWIND: {
      cell *ebx = unwind_part1(s);
      if (unwind_part2(s,ebx))
        break;

      /* fall through */
    }
    case OP_RETURN: {
      do_return(s);
      break;
    }
    case OP_PUSH: {
      assert(instr->arg0 < s->count);
      stack_push(s,resolve_ind(s->data[s->count-1-instr->arg0]));
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
        stack_push(s,alloc_cell2(TYPE_FUNCTION,src->field1,src->field2));
      }
      // If the function is a supercombinator and arg1 is non-zero, this means we
      // actually want a reference to the "no-eval" part of the supercombinator, i.e.
      // we skip the EVAL instructions at the start, under the assumption that the
      // arguments to be passed to it have already been evaluated
      else if ((fno >= NUM_BUILTINS) && instr->arg1) {
        cell *src = globcells[fno];
        assert(TYPE_FUNCTION == celltype(src));
        assert((int)src->field2 == addressmap[fno]);
        stack_push(s,alloc_cell2(TYPE_FUNCTION,src->field1,(void*)noevaladdressmap[fno]));
      }
      else {
        stack_push(s,globcells[fno]);
      }
      break;
    }
    case OP_MKAP:
      do_mkap(s,instr->arg0);
      break;
    case OP_UPDATE:
      do_update(s,instr->arg0);
      break;
    case OP_POP:
      s->count -= instr->arg0;
      assert(0 <= s->count);
      break;
    case OP_PRINT:
      print_cell(resolve_ind(s->data[s->count-1]));
      s->count--;
      break;
    case OP_BIF: {
      int i;
      assert(0 <= builtin_info[instr->arg0].nargs); /* should we support 0-arg bifs? */
      for (i = 0; i < builtin_info[instr->arg0].nstrict; i++) {
        s->data[s->count-1-i] = resolve_ind(s->data[s->count-1-i]); /* bifs expect it */
        assert(TYPE_APPLICATION != celltype(s->data[s->count-1-i]));
      }
      if (trace)
        debug(0,"  builtin %s\n",builtin_info[instr->arg0].name);
      builtin_info[instr->arg0].f(s);
      break;
    }
    case OP_JFALSE: {
      cell *test = resolve_ind(s->data[s->count-1]);
      assert(TYPE_APPLICATION != celltype(test));
      if (TYPE_NIL == celltype(test))
        address += instr->arg0-1;
      s->count--;
      break;
    }
    case OP_JUMP:
      address += instr->arg0-1;
      break;
    case OP_JEMPTY:
      if (0 == s->count)
        address += instr->arg0-1;
      break;
    case OP_ISTYPE:
      if (celltype(resolve_ind(s->data[s->count-1])) == instr->arg0)
        s->data[s->count-1] = globtrue;
      else
        s->data[s->count-1] = globnil;
      break;
    case OP_ALLOC: {
      int i;
      /* FIXME: perhaps we should make a HOLE type instead of using nil here... then it
         should be save to use globnil everywhere we normally use TYPE_NIL */
      nALLOCs++;
      for (i = 0; i < instr->arg0; i++) {
        stack_push(s,alloc_cell2(TYPE_NIL,NULL,NULL));
        nALLOCcells++;
      }
      break;
    }
    case OP_SQUEEZE: {
      assert(0 <= s->count-instr->arg0-instr->arg1);
      int base = s->count-instr->arg0-instr->arg1;
      int i;
      for (i = 0; i <= instr->arg0; i++)
        s->data[base+i] = s->data[base+i+instr->arg1];
      s->count -= instr->arg1;
      break;
    }
    case OP_DISPATCH: {
      cell *fun = resolve_ind(s->data[s->count-1]);
      if (TYPE_FUNCTION == celltype(fun)) {
        int nargs = (int)fun->field1;
        int addr = (int)fun->field2;
        assert(OP_GLOBSTART == gp->ginstrs[addr].opcode);

        if (nargs == 0) {
          ndisp0++;
        }
        else if (nargs == instr->arg0) {
          ndispexact++;
          s->count--;
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

      do_mkap(s,instr->arg0);
      do_update(s,1);

      cell *bottom = unwind_part1(s);
      if (unwind_part2(s,bottom))
        break;
      do_return(s);
      break;
    }
    case OP_PUSHNIL:
      stack_push(s,globnil);
      break;
    case OP_PUSHINT:
      stack_push(s,alloc_cell2(TYPE_INT,(void*)instr->arg0,NULL));
      break;
    case OP_PUSHDOUBLE:
      stack_push(s,alloc_cell2(TYPE_DOUBLE,(void*)instr->arg0,(void*)instr->arg1));
      break;
    case OP_PUSHSTRING:
      stack_push(s,((cell**)gp->stringmap->data)[instr->arg0]);
      break;
    default:
      assert(0);
      break;
    }

    address++;
  }
}
