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

int is_whnf(pntr p)
{
  if (!is_pntr(p))
    return 1;

  if (TYPE_FRAME == pntrtype(p))
    return 0;

  if (TYPE_CAP == pntrtype(p))
    return 1;

  if (TYPE_APPLICATION == pntrtype(p)) {
    int nargs = 0;
    while (TYPE_APPLICATION == pntrtype(p)) {
      p = resolve_pntr(get_pntr(p)->field1);
      nargs++;
    }

    if (TYPE_FRAME == pntrtype(p)) {
      return 0;
    }
    else {
      cap *cp;
      assert(TYPE_CAP == pntrtype(p));
      cp = (cap*)get_pntr(get_pntr(p)->field1);
      if (nargs+cp->count >= cp->arity)
        return 0;
    }
  }
  return 1;
}

int start_addr(gprogram *gp, int fno)
{
  return gp->addressmap[fno];
}

int end_addr(gprogram *gp, int fno)
{
  int addr = gp->addressmap[fno];
  while ((addr < gp->count) &&
         ((OP_GLOBSTART != gp->ginstrs[addr].opcode) ||
          (gp->ginstrs[addr].arg0 == fno)))
    addr++;
  return addr-1;
}

void check_stack(frame *curf, pntr *stackdata, int stackcount, gprogram *gp)
{
  ginstr *instr;
  int i;
  if (0 <= curf->fno) {
    if ((curf->address < start_addr(gp,curf->fno)) ||
        (curf->address > end_addr(gp,curf->fno))) {
      char *name = get_function_name(curf->fno);
      printf("Current address %d out of range\n",curf->address);
      printf("Function: %s\n",name);
      printf("Function start address: %d\n",start_addr(gp,curf->fno));
      printf("Function end address: %d\n",end_addr(gp,curf->fno));
      free(name);
      abort();
    }
  }

  instr = &gp->ginstrs[curf->address];
  if (0 > instr->expcount)
    return;

  if (stackcount != instr->expcount) {
    printf("Instruction %d expects stack frame size %d, actually %d\n",
           curf->address,instr->expcount,stackcount);
    abort();
  }

  for (i = 0; i < instr->expcount; i++) {
    pntr p = resolve_pntr(stackdata[i]);
    int actualstatus = is_whnf(p);
    if (instr->expstatus[i] && !actualstatus) {
      printf("Instruction %d expects stack frame entry %d (%d) to be evald but it's "
             "actually a %s\n",curf->address,i,i,cell_types[pntrtype(p)]);
      abort();
    }
    if (instr->expstatus[i] && !is_pntr(p) && (TYPE_IND == pntrtype(stackdata[i]))) {
      if (OP_RESOLVE != gp->ginstrs[curf->address].opcode) {
        printf("Instruction %d expects stack frame entry %d (%d) to be evald (and it is) "
               "but the pointer in the stack is an IND\n",curf->address,i,i);
        abort();
      }
    }
  }
}

void swapstack_in(frame *curf, pntr *stdata, int stcount)
{
  assert(NULL == curf->data);
  assert(0 == curf->count);

  curf->data = stdata;
  curf->count = stcount;
}

void swapstack_out(frame *curf, pntr **stdata, int *stcount)
{
  *stdata = curf->data;
  *stcount = curf->count;

  curf->data = NULL;
  curf->count = 0;
}

void cap_error(pntr cappntr, ginstr *instr)
{
  cell *capval = get_pntr(cappntr);
  cap *c = (cap*)get_pntr(capval->field1);
  char *name = get_function_name(c->fno);

  print_sourceloc(stderr,instr->sl);
  fprintf(stderr,"Attempt to evaluate incomplete function application\n");

  print_sourceloc(stderr,c->sl);
  fprintf(stderr,"%s requires %d args, only have %d\n",
          name,function_nargs(c->fno),c->count);
  free(name);
  exit(1);
}

void execute(process *proc, gprogram *gp)
{
  int stcount = 0;
  pntr *stdata = (pntr*)malloc(sizeof(pntr));
  int lines = -1;

  frame *curf = frame_alloc(proc);
  curf->fno = -1;
  add_active_frame(proc,curf);

  curf->alloc = 1;

  if (getenv("LINES"))
    lines = atoi(getenv("LINES"));

  while (1) {
    int done = 0;
    ginstr *instr = &gp->ginstrs[curf->address];

    #ifdef EXECUTION_TRACE
    int printed = 0;
    if (trace) {
      printf("[d %d] ",frame_depth(curf));
      print_ginstr(gp,curf->address,instr,0);
      print_stack(stdata,stcount,0);
      printed += stcount+1;
    }
    #endif

    #ifdef STACK_MODEL_SANITY_CHECK
    check_stack(curf,stdata,stcount,gp);
    assert(stcount <= curf->alloc);
    assert((0 > curf->fno) || (curf->alloc >= gp->stacksizes[curf->fno]));
    assert((0 > curf->fno) || (stcount <= gp->stacksizes[curf->fno]));
    #endif

    if (proc->nallocs > COLLECT_THRESHOLD) {
      proc->gstack = stdata;
      proc->gstackcount = stcount;
      collect(proc);
      proc->gstack = NULL;
      proc->gstackcount = 0;
    }

    #ifdef PROFILING
    proc->op_usage[instr->opcode]++;
    proc->usage[curf->address]++;
    #endif

    switch (instr->opcode) {
    case OP_BEGIN:
      break;
    case OP_END:
      remove_active_frame(proc,curf);
      free(stdata);
      curf = NULL;
      done = 1;
      break;
    case OP_LAST:
      break;
    case OP_GLOBSTART:
      break;
    case OP_EVAL: {
      pntr p;
      assert(0 <= stcount-1-instr->arg0);
      stdata[stcount-1-instr->arg0] = resolve_pntr(stdata[stcount-1-instr->arg0]);
      p = stdata[stcount-1-instr->arg0];


      if (TYPE_FRAME == pntrtype(p)) {
        cell *c = get_pntr(p);
        frame *newf;
        newf = (frame*)get_pntr(c->field1);

        assert(NULL == newf->d);
        assert(OP_GLOBSTART == gp->ginstrs[newf->address].opcode);

        newf->d = curf;

        swapstack_in(curf,stdata,stcount);
        curf = newf;
        swapstack_out(curf,&stdata,&stcount);

        #ifdef PROFILING
        if (0 <= curf->fno)
          proc->funcalls[curf->fno]++;
        #endif

        // curf->address--; /* so we process the GLOBSTART */
        add_active_frame(proc,curf);
      }
      else {
        assert(OP_RESOLVE == gp->ginstrs[curf->address+1].opcode);
        curf->address++; // skip RESOLVE
      }
      break;
    }
    case OP_RETURN: {
      frame *old;
      assert(0 < stcount);
      assert(NULL != curf->c);
      assert(TYPE_FRAME == snodetype(curf->c));
      curf->c->tag = TYPE_IND;
      curf->c->field1 = stdata[stcount-1];
      curf->c = NULL;

      old = curf;

      swapstack_in(curf,stdata,stcount);
      curf = old->d;
      swapstack_out(curf,&stdata,&stcount);

      remove_active_frame(proc,old);
      assert(curf);
      break;
    }
    case OP_DO: {
      cell *capholder;
      cap *cp;
      int s;
      int s1;
      int a1;
      assert(0 < stcount);
      assert(TYPE_CAP == pntrtype(stdata[stcount-1]));
      capholder = (cell*)get_pntr(stdata[stcount-1]);
      stcount--;

      cp = (cap*)get_pntr(capholder->field1);
      s = stcount;
      s1 = cp->count;
      a1 = cp->arity;

      if (s+s1 < a1) {
        /* create a new CAP with the existing CAPs arguments and those from the current
           FRAME's stack */
        int i;
        cell *replace;
        frame *old;
        cap *newcp = cap_alloc(cp->arity,cp->address,cp->fno);
        newcp->sl = cp->sl;
        newcp->data = (pntr*)malloc((stcount+cp->count)*sizeof(pntr));
        newcp->count = 0;
        for (i = 0; i < stcount; i++)
          newcp->data[newcp->count++] = stdata[i];
        for (i = 0; i < cp->count; i++)
          newcp->data[newcp->count++] = cp->data[i];

        /* replace the current FRAME with the new CAP */
        replace = curf->c;
        curf->c = NULL;
        replace->tag = TYPE_CAP;
        make_pntr(replace->field1,newcp);

        /* return to caller */
        old = curf;

        swapstack_in(curf,stdata,stcount);
        curf = old->d;
        swapstack_out(curf,&stdata,&stcount);

        remove_active_frame(proc,old);
        assert(curf);
      }
      else if (s+s1 == a1) {
        int i;
        curf->address = cp->address;
        curf->fno = cp->fno;
        pntrstack_grow(&curf->alloc,&stdata,gp->stacksizes[cp->fno]);
        for (i = 0; i < cp->count; i++)
          stdata[stcount++] = cp->data[i];
        // curf->address--; /* so we process the GLOBSTART */
        #ifdef PROFILING
        if (0 <= curf->fno)
          proc->funcalls[curf->fno]++;
        #endif
      }
      else { /* s+s1 > a1 */
        frame *newf = frame_alloc(proc);
        int i;
        int extra;
        newf->alloc = gp->stacksizes[cp->fno];
        newf->data = (pntr*)malloc(newf->alloc*sizeof(pntr));
        newf->count = 0;
        newf->address = cp->address;
        newf->fno = cp->fno;
        extra = a1-s1;
        for (i = stcount-extra; i < stcount; i++)
          newf->data[newf->count++] = stdata[i];
        for (i = 0; i < cp->count; i++)
          newf->data[newf->count++] = cp->data[i];

        newf->c = alloc_cell(proc);
        newf->c->tag = TYPE_FRAME;
        make_pntr(newf->c->field1,newf);

        stcount -= extra;
        make_pntr(stdata[stcount],newf->c);
        stcount++;

        curf->address = gp->evaldoaddr-1;
        curf->fno = -1;
      }

      break;
    }
    case OP_JFUN:
      curf->address = gp->addressmap[instr->arg0];
      curf->fno = instr->arg0;
      pntrstack_grow(&curf->alloc,&stdata,gp->stacksizes[curf->fno]);
      // curf->address--; /* so we process the GLOBSTART */
      #ifdef PROFILING
      if (0 <= curf->fno)
        proc->funcalls[curf->fno]++;
      #endif
      break;
    case OP_JFALSE: {
      pntr test = stdata[stcount-1];
      assert(TYPE_IND != pntrtype(test));
      assert(TYPE_APPLICATION != pntrtype(test));
      assert(TYPE_FRAME != pntrtype(test));

      if (TYPE_CAP == pntrtype(test))
        cap_error(test,instr);

      if (TYPE_NIL == pntrtype(test))
        curf->address += instr->arg0-1;
      stcount--;
      break;
    }
    case OP_JUMP:
      curf->address += instr->arg0-1;
      break;
    case OP_PUSH: {
      assert(instr->arg0 < stcount);
      stdata[stcount] = stdata[stcount-1-instr->arg0];
      stcount++;
      break;
    }
    case OP_UPDATE: {
      int n = instr->arg0;
      pntr targetp;
      cell *target;
      pntr res;
      assert(n < stcount);
      assert(n > 0);

      targetp = stdata[stcount-1-n];
      assert(TYPE_HOLE == pntrtype(targetp));
      target = get_pntr(targetp);

      res = resolve_pntr(stdata[stcount-1]);
      if (pntrequal(targetp,res)) {
        fprintf(stderr,"Attempt to update cell with itself\n");
        exit(1);
      }
      target->tag = TYPE_IND;
      target->field1 = res;
      stdata[stcount-1-n] = res;
      stcount--;
      break;
    }
    case OP_ALLOC: {
      int i;
      for (i = 0; i < instr->arg0; i++) {
        cell *hole = alloc_cell(proc);
        hole->tag = TYPE_HOLE;
        make_pntr(stdata[stcount],hole);
        stcount++;
      }
      break;
    }
    case OP_SQUEEZE: {
      int count = instr->arg0;
      int remove = instr->arg1;
      int base = stcount-count-remove;
      int i;
      assert(0 <= base);
      for (i = 0; i < count; i++)
        stdata[base+i] = stdata[base+i+remove];
      stcount -= remove;
      break;
    }
    case OP_MKCAP: {
      int fno = instr->arg0;
      int n = instr->arg1;
      int i;
      cell *capv;
      cap *c = cap_alloc(function_nargs(fno),gp->addressmap[fno],fno);
      c->sl = instr->sl;
      c->data = (pntr*)malloc(n*sizeof(pntr));
      c->count = 0;
      for (i = stcount-n; i < stcount; i++)
        c->data[c->count++] = stdata[i];
      stcount -= n;

      capv = alloc_cell(proc);
      capv->tag = TYPE_CAP;
      make_pntr(capv->field1,c);
      make_pntr(stdata[stcount],capv);
      stcount++;
      break;
    }
    case OP_MKFRAME: {
      int fno = instr->arg0;
      int n = instr->arg1;
      cell *newfholder;
      int i;
      frame *newf = frame_alloc(proc);
      newf->alloc = gp->stacksizes[fno];
      newf->data = (pntr*)malloc(newf->alloc*sizeof(pntr));
      newf->count = 0;

      newf->address = gp->addressmap[fno];
      newf->fno = fno;
      newf->d = NULL;

      newfholder = alloc_cell(proc);
      newfholder->tag = TYPE_FRAME;
      make_pntr(newfholder->field1,newf);
      newf->c = newfholder;

      for (i = stcount-n; i < stcount; i++)
        newf->data[newf->count++] = stdata[i];
      stcount -= n;
      make_pntr(stdata[stcount],newfholder);
      stcount++;
      break;
    }
    case OP_BIF: {
      int bif = instr->arg0;
      int nargs = builtin_info[bif].nargs;
      int i;
      assert(0 <= builtin_info[bif].nargs); /* should we support 0-arg bifs? */
      for (i = 0; i < builtin_info[bif].nstrict; i++) {
        assert(TYPE_APPLICATION != pntrtype(stdata[stcount-1-i]));
        assert(TYPE_FRAME != pntrtype(stdata[stcount-1-i]));

        if (TYPE_CAP == pntrtype(stdata[stcount-1-i]))
          cap_error(stdata[stcount-1-i],instr);
      }

      for (i = 0; i < builtin_info[bif].nstrict; i++)
        assert(TYPE_IND != pntrtype(stdata[stcount-1-i]));

      builtin_info[bif].f(proc,&stdata[stcount-nargs]);
      stcount -= (nargs-1);
      assert(!builtin_info[bif].reswhnf || (TYPE_IND != pntrtype(stdata[stcount-1])));
      break;
    }
    case OP_PUSHNIL:
      stdata[stcount++] = proc->globnilpntr;
      break;
    case OP_PUSHNUMBER:
      stdata[stcount++] = make_number(*((double*)&instr->arg0));
      break;
    case OP_PUSHSTRING: {
      stdata[stcount] = proc->strings[instr->arg0];
      stcount++;
      break;
    }
    case OP_RESOLVE:
      stdata[stcount-1-instr->arg0] = resolve_pntr(stdata[stcount-1-instr->arg0]);
      break;
    default:
      assert(0);
      break;
    }

    #ifdef EXECUTION_TRACE
    if (trace) {
      int ln;
      for (ln = printed; ln < lines-1; ln++)
        printf("\n");
    }
    #endif

    if (done)
      break;

    curf->address++;
  }
}
