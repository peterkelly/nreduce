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

extern int evaldoaddr;

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
      p = resolve_pntr(get_pntr(p)->cmp1);
      nargs++;
    }

    if (TYPE_FRAME == pntrtype(p)) {
      return 0;
    }
    else {
      cap *cp;
      assert(TYPE_CAP == pntrtype(p));
      cp = (cap*)get_pntr(get_pntr(p)->cmp1);
      if (nargs+cp->count >= cp->arity)
        return 0;
    }
  }
  return 1;
}

int start_addr(gprogram *gp, int fno)
{
  return addressmap[fno];
}

int end_addr(gprogram *gp, int fno)
{
  int addr = addressmap[fno];
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
      printf("Current address %d out of range\n",curf->address);
      printf("Function: %s\n",function_name(curf->fno));
      printf("Function start address: %d\n",start_addr(gp,curf->fno));
      printf("Function end address: %d\n",end_addr(gp,curf->fno));
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

pntr *gstack = NULL;
int gstackcount = 0;

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

void execute(gprogram *gp)
{
  int stcount = 0;
  pntr *stdata = (pntr*)malloc(sizeof(pntr));
  int lines = -1;

  frame *curf = frame_alloc();
  curf->fno = -1;
  add_active_frame(curf);

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
    assert((0 > curf->fno) || (curf->alloc >= stacksizes[curf->fno]));
    assert((0 > curf->fno) || (stcount <= stacksizes[curf->fno]));
    #endif

    if (nallocs > COLLECT_THRESHOLD) {
      gstack = stdata;
      gstackcount = stcount;
/*       collect(); */
      rtcollect();
      gstack = NULL;
      gstackcount = 0;
    }

    #ifdef PROFILING
    op_usage[instr->opcode]++;
    instr->usage++;
    #endif

    switch (instr->opcode) {
    case OP_BEGIN:
      break;
    case OP_END:
      remove_active_frame(curf);
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
        rtvalue *c = get_pntr(p);
        frame *newf;
        newf = (frame*)get_pntr(c->cmp1);

        assert(NULL == newf->d);
        assert(OP_GLOBSTART == gp->ginstrs[newf->address].opcode);

        newf->d = curf;

        swapstack_in(curf,stdata,stcount);
        curf = newf;
        swapstack_out(curf,&stdata,&stcount);

        #ifdef PROFILING
        if (0 <= curf->fno)
          funcalls[curf->fno]++;
        #endif

        // curf->address--; /* so we process the GLOBSTART */
        add_active_frame(curf);
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
      assert(TYPE_FRAME == celltype(curf->c));
      curf->c->tag = TYPE_IND;
      curf->c->cmp1 = stdata[stcount-1];
      curf->c = NULL;

      old = curf;

      swapstack_in(curf,stdata,stcount);
      curf = old->d;
      swapstack_out(curf,&stdata,&stcount);

      remove_active_frame(old);
      assert(curf);
      break;
    }
    case OP_DO: {
      rtvalue *capcell;
      cap *cp;
      int s;
      int s1;
      int a1;
      assert(0 < stcount);
      assert(TYPE_CAP == pntrtype(stdata[stcount-1]));
      capcell = (rtvalue*)get_pntr(stdata[stcount-1]);
      stcount--;

      cp = (cap*)get_pntr(capcell->cmp1);
      s = stcount;
      s1 = cp->count;
      a1 = cp->arity;

      if (s+s1 < a1) {
        /* create a new CAP with the existing CAPs arguments and those from the current
           FRAME's stack */
        cap *newcp = cap_alloc(cp->arity,cp->address,cp->fno);
        int i;
        rtvalue *replace;
        frame *old;
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
        make_pntr(replace->cmp1,newcp);

        /* return to caller */
        old = curf;

        swapstack_in(curf,stdata,stcount);
        curf = old->d;
        swapstack_out(curf,&stdata,&stcount);

        remove_active_frame(old);
        assert(curf);
      }
      else if (s+s1 == a1) {
        int i;
        curf->address = cp->address;
        curf->fno = cp->fno;
        pntrstack_grow(&curf->alloc,&stdata,stacksizes[cp->fno]);
        for (i = 0; i < cp->count; i++)
          stdata[stcount++] = cp->data[i];
        // curf->address--; /* so we process the GLOBSTART */
        #ifdef PROFILING
        if (0 <= curf->fno)
          funcalls[curf->fno]++;
        #endif
      }
      else { /* s+s1 > a1 */
        frame *newf = frame_alloc();
        int i;
        int extra;
        newf->alloc = stacksizes[cp->fno];
        newf->data = (pntr*)malloc(newf->alloc*sizeof(pntr));
        newf->count = 0;
        newf->address = cp->address;
        newf->fno = cp->fno;
        extra = a1-s1;
        for (i = stcount-extra; i < stcount; i++)
          newf->data[newf->count++] = stdata[i];
        for (i = 0; i < cp->count; i++)
          newf->data[newf->count++] = cp->data[i];

        newf->c = alloc_rtvalue();
        newf->c->tag = TYPE_FRAME;
        make_pntr(newf->c->cmp1,newf);

        stcount -= extra;
        make_pntr(stdata[stcount],newf->c);
        stcount++;

        curf->address = evaldoaddr-1;
        curf->fno = -1;
      }

      break;
    }
    case OP_JFUN:
      curf->address = addressmap[instr->arg0];
      curf->fno = instr->arg0;
      pntrstack_grow(&curf->alloc,&stdata,stacksizes[curf->fno]);
      // curf->address--; /* so we process the GLOBSTART */
      #ifdef PROFILING
      if (0 <= curf->fno)
        funcalls[curf->fno]++;
      #endif
      break;
    case OP_JFALSE: {
      pntr test = stdata[stcount-1];
      assert(TYPE_IND != pntrtype(test));
      assert(TYPE_APPLICATION != pntrtype(test));
      assert(TYPE_CAP != pntrtype(test));
      assert(TYPE_FRAME != pntrtype(test));
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
      rtvalue *target;
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
      target->cmp1 = res;
      stdata[stcount-1-n] = res;
      stcount--;
      break;
    }
    case OP_ALLOC: {
      int i;
      for (i = 0; i < instr->arg0; i++) {
        rtvalue *hole = alloc_rtvalue();
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
      cap *c = cap_alloc(function_nargs(fno),addressmap[fno],fno);
      rtvalue *capv;
      c->data = (pntr*)malloc(n*sizeof(pntr));
      c->count = 0;
      for (i = stcount-n; i < stcount; i++)
        c->data[c->count++] = stdata[i];
      stcount -= n;

      capv = alloc_rtvalue();
      capv->tag = TYPE_CAP;
      make_pntr(capv->cmp1,c);
      make_pntr(stdata[stcount],capv);
      stcount++;
      break;
    }
    case OP_MKFRAME: {
      int fno = instr->arg0;
      int n = instr->arg1;
      rtvalue *newfcell;
      int i;
      frame *newf = frame_alloc();
      newf->alloc = stacksizes[fno];
      newf->data = (pntr*)malloc(newf->alloc*sizeof(pntr));
      newf->count = 0;

      newf->address = addressmap[fno];
      newf->fno = fno;
      newf->d = NULL;

      newfcell = alloc_rtvalue();
      newfcell->tag = TYPE_FRAME;
      make_pntr(newfcell->cmp1,newf);
      newf->c = newfcell;

      for (i = stcount-n; i < stcount; i++)
        newf->data[newf->count++] = stdata[i];
      stcount -= n;
      make_pntr(stdata[stcount],newfcell);
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
        assert(TYPE_CAP != pntrtype(stdata[stcount-1-i]));
        assert(TYPE_FRAME != pntrtype(stdata[stcount-1-i]));
      }

      for (i = 0; i < builtin_info[bif].nstrict; i++)
        assert(TYPE_IND != pntrtype(stdata[stcount-1-i]));

      builtin_info[bif].f(&stdata[stcount-nargs]);
      stcount -= (nargs-1);
      assert(!builtin_info[bif].reswhnf || (TYPE_IND != pntrtype(stdata[stcount-1])));
      break;
    }
    case OP_PUSHNIL:
      stdata[stcount++] = globnilpntr;
      break;
    case OP_PUSHNUMBER:
      stdata[stcount++] = make_number(*((double*)&instr->arg0));
      break;
    case OP_PUSHSTRING:
      make_pntr(stdata[stcount],((cell**)gp->stringmap->data)[instr->arg0]);
      stcount++;
      break;
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
