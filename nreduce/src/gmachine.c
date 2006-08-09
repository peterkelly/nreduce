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

extern int evaldoaddr;

int is_whnf(cell *c)
{
  c = resolve_ind(c);

  if (TYPE_FRAME == celltype(c))
    return 0;

  if (TYPE_CAP == celltype(c))
    return 1;

  if (TYPE_APPLICATION == celltype(c)) {
    int nargs = 0;
    while (TYPE_APPLICATION == celltype(c)) {
      c = resolve_ind((cell*)c->field1);
      nargs++;
    }

    if (TYPE_FRAME == celltype(c)) {
      return 0;
    }
    else {
      assert(TYPE_CAP == celltype(c));
      cap *cp = (cap*)c->field1;
      if (nargs+cp->s->count >= cp->arity)
        return 0;
    }
  }
  return 1;
}

void check_stack(frame *curf, gprogram *gp)
{
  ginstr *instr = &gp->ginstrs[curf->address];
  if (0 > instr->expcount)
    return;

  if (curf->s->count != instr->expcount) {
    printf("Instruction %d expects stack frame size %d, actually %d\n",
           curf->address,instr->expcount,curf->s->count);
    abort();
  }

  int i;
  for (i = 0; i < instr->expcount; i++) {
    cell *c = stack_at(curf->s,i);
    int actualstatus = is_whnf(c);
    if (instr->expstatus[i] && !actualstatus) {
      printf("Instruction %d expects stack frame entry %d (%d) to be evald but it's "
             "actually a %s\n",curf->address,i,i,cell_types[celltype(c)]);
      abort();
    }
  }
}

void execute(gprogram *gp)
{
  frame *curf = frame_alloc(-1);
  add_active_frame(curf);

  int lines = -1;
  if (getenv("LINES"))
    lines = atoi(getenv("LINES"));

  while (1) {
    int done = 0;
    ginstr *instr = &gp->ginstrs[curf->address];

/*     int depth = 0; */
/*     frame *tmp; */
/*     for (tmp = curf; tmp; tmp = tmp->d) */
/*       depth++; */
/*     if (depth > maxdepth) { */
/*       maxdepth = depth; */

/*       printf("new maxdepth: %d\n",depth); */
/*       int i = depth; */
/*       for (tmp = curf; tmp; tmp = tmp->d) { */
/*         printf("%d: %s\n",i,function_name(tmp->fno)); */
/*         i--; */
/*       } */
/*       printf("\n"); */
/*     } */

    int printed = 0;
    if (trace) {
      printf("[d %d] ",frame_depth(curf));
      print_ginstr(gp,curf->address,instr,0);
      print_stack((cell**)curf->s->data,curf->s->count,0);
      printed += curf->s->count+1;
    }

#ifdef STACK_MODEL_SANITY_CHECK
    check_stack(curf,gp);
#endif

    if (nallocs > COLLECT_THRESHOLD)
      collect();

    op_usage[instr->opcode]++;
    instr->usage++;

    switch (instr->opcode) {
    case OP_BEGIN:
      break;
    case OP_END:
      remove_active_frame(curf);
      curf = NULL;
      done = 1;
      break;
    case OP_LAST:
      break;
    case OP_GLOBSTART:
      break;
    case OP_EVAL: {
      cell *c;
      assert(0 <= curf->s->count-1-instr->arg0);
      c = resolve_ind(curf->s->data[curf->s->count-1-instr->arg0]);

      if (TYPE_FRAME == celltype(c)) {
        frame *newf = (frame*)c->field1;

        assert(NULL == newf->d);
        assert(OP_GLOBSTART == gp->ginstrs[newf->address].opcode);

        newf->d = curf;
        curf = newf;
        // curf->address--; /* so we process the GLOBSTART */
        add_active_frame(curf);
        #ifdef EXTRA_TRACE
        if (trace) {
          printf("EVAL(%d): entering new frame\n",instr->arg0);
          printed++;
        }
        #endif
      }
      break;
    }
    case OP_RETURN: {
      assert(0 < curf->s->count);
      assert(NULL != curf->c);
      assert(TYPE_FRAME == celltype(curf->c));
      curf->c->tag = TYPE_IND;
      curf->c->field1 = curf->s->data[curf->s->count-1];
      curf->c = NULL;

      frame *old = curf;
      curf = old->d;
      remove_active_frame(old);
      assert(curf);
      #ifdef EXTRA_TRACE
      if (trace) {
        printf("RETURN: exiting old frame\n");
        printed++;
      }
      #endif
      break;
    }
    case OP_DO: {
      assert(0 < curf->s->count);
      cell *capcell = resolve_ind((cell*)curf->s->data[curf->s->count-1]);
      curf->s->count--;

      assert(TYPE_CAP == celltype(capcell));

      cap *cp = (cap*)capcell->field1;
      int s = curf->s->count;
      int s1 = cp->s->count;
      int a1 = cp->arity;

      if (s+s1 < a1) {
        #ifdef EXTRA_TRACE
        if (trace) {
          printf("DO: %d+%d < %d\n",s,s1,a1);
          printed++;
        }
        #endif

        /* create a new CAP with the existing CAPs arguments and those from the current
           FRAME's stack */
        cap *newcp = cap_alloc();
        newcp->arity = cp->arity;
        newcp->address = cp->address;
        newcp->fno = cp->fno;
        int i;
        for (i = 0; i < curf->s->count; i++)
          stack_push(newcp->s,curf->s->data[i]);
        for (i = 0; i < cp->s->count; i++)
          stack_push(newcp->s,cp->s->data[i]);

        /* replace the current FRAME with the new CAP */
        cell *replace = curf->c;
        curf->c = NULL;
        replace->tag = TYPE_CAP;
        replace->field1 = newcp;

        /* return to caller */
        frame *old = curf;
        curf = old->d;
        remove_active_frame(old);
        assert(curf);
        #ifdef EXTRA_TRACE
        if (trace) {
          printf("DO: exiting old frame\n");
          printed++;
        }
        #endif
      }
      else if (s+s1 == a1) {
        #ifdef EXTRA_TRACE
        if (trace) {
          printf("DO: %d+%d == %d\n",s,s1,a1);
          printed++;
        }
        #endif

        int i;
        for (i = 0; i < cp->s->count; i++)
          stack_push(curf->s,cp->s->data[i]);
        curf->address = cp->address;
        curf->fno = cp->fno;
        // curf->address--; /* so we process the GLOBSTART */

        #ifdef EXTRA_TRACE
        if (trace) {
          printf("DO: jumping to function %s\n",function_name(curf->fno));
          printed++;
        }
        #endif
      }
      else { /* s+s1 > a1 */
        #ifdef EXTRA_TRACE
        if (trace) {
          printf("DO: %d+%d > %d\n",s,s1,a1);
          printed++;
        }
        #endif

        frame *newf = frame_alloc(cp->fno);
        newf->address = cp->address;
        int i;
        int extra = a1-s1;
        for (i = curf->s->count-extra; i < curf->s->count; i++)
          stack_push(newf->s,curf->s->data[i]);
        for (i = 0; i < cp->s->count; i++)
          stack_push(newf->s,cp->s->data[i]);

        newf->c = alloc_cell2(TYPE_FRAME,newf,NULL);

        curf->s->count -= extra;
        stack_push(curf->s,newf->c);

        curf->address = evaldoaddr-1;
      }

      break;
    }
    case OP_JFUN:
      curf->address = addressmap[instr->arg0];
      // curf->address--; /* so we process the GLOBSTART */
      break;
    case OP_JFALSE: {
      cell *test = resolve_ind(curf->s->data[curf->s->count-1]);
      assert(TYPE_APPLICATION != celltype(test));
      assert(TYPE_CAP != celltype(test));
      assert(TYPE_FRAME != celltype(test));
      if (TYPE_NIL == celltype(test))
        curf->address += instr->arg0-1;
      curf->s->count--;
      break;
    }
    case OP_JUMP:
      curf->address += instr->arg0-1;
      break;
    case OP_JEMPTY:
      if (0 == curf->s->count)
        curf->address += instr->arg0-1;
      break;
    case OP_PUSH: {
      assert(instr->arg0 < curf->s->count);
      stack_push(curf->s,resolve_ind(curf->s->data[curf->s->count-1-instr->arg0]));
      break;
    }
    case OP_POP:
      curf->s->count -= instr->arg0;
      assert(0 <= curf->s->count);
      break;
    case OP_UPDATE: {
      int n = instr->arg0;
      cell *target;
      assert(n < curf->s->count);
      assert(n > 0);

      target = resolve_ind(curf->s->data[curf->s->count-1-n]);

      /* FIXME: this check can probably just become TYPE_HOLE once the (v,g) scheme is
         in place, as I think UPDATE is only used for letrecs */
      assert((TYPE_APPLICATION == celltype(target)) ||
             (TYPE_FRAME == celltype(target)) ||
             (TYPE_HOLE == celltype(target)));
      assert(!(target->tag & FLAG_PINNED));

      if (TYPE_FRAME == celltype(target))
        ((frame*)target->field1)->c = NULL;

      cell *res = resolve_ind(curf->s->data[curf->s->count-1]);
      if (target == res) {
        fprintf(stderr,"Attempt to update cell with itself\n");
        exit(1);
      }
      target->tag = TYPE_IND;
      target->field1 = res;
      curf->s->count--;
      break;
    }
    case OP_REPLACE: {
      int n = instr->arg0;
      assert(n < curf->s->count);
      assert(n > 0);
      curf->s->data[curf->s->count-1-n] = curf->s->data[curf->s->count-1];
      curf->s->count--;
      break;
    }
    case OP_ALLOC: {
      int i;
      nALLOCs++;
      for (i = 0; i < instr->arg0; i++) {
        stack_push(curf->s,alloc_cell2(TYPE_HOLE,NULL,NULL));
        nALLOCcells++;
      }
      break;
    }
    case OP_SQUEEZE: {
      int count = instr->arg0;
      int remove = instr->arg1;
      int base = curf->s->count-count-remove;
      assert(0 <= base);
      int i;
      for (i = 0; i < count; i++)
        curf->s->data[base+i] = curf->s->data[base+i+remove];
      curf->s->count -= remove;
      break;
    }
    case OP_MKCAP: {
      int fno = instr->arg0;
      int n = instr->arg1;

      cap *c = cap_alloc();
      c->address = addressmap[fno];
      if (NUM_BUILTINS > fno)
        c->arity = builtin_info[fno].nargs;
      else
        c->arity = get_scomb_index(fno-NUM_BUILTINS)->nargs;
      assert(0 < c->arity); /* MKCAP should not be called for CAFs */
      c->fno = fno;

      int i;
      for (i = curf->s->count-n; i < curf->s->count; i++)
        stack_push(c->s,curf->s->data[i]);
      curf->s->count -= n;

      cell *capcell = alloc_cell2(TYPE_CAP,c,NULL);
      stack_push(curf->s,capcell);
      break;
    }
    case OP_MKFRAME: {
      int fno = instr->arg0;
      int n = instr->arg1;

      frame *newf = frame_alloc(fno);
      newf->address = addressmap[fno];
      newf->d = NULL;

      cell *newfcell = alloc_cell2(TYPE_FRAME,newf,NULL);
      newf->c = newfcell;

      int i;
      for (i = curf->s->count-n; i < curf->s->count; i++)
        stack_push(newf->s,curf->s->data[i]);;
      curf->s->count -= n;
      stack_push(curf->s,newfcell);
      break;
    }
    case OP_BIF: {
      int bif = instr->arg0;
      int nargs = builtin_info[bif].nargs;
      int i;
      assert(0 <= builtin_info[bif].nargs); /* should we support 0-arg bifs? */
      for (i = 0; i < builtin_info[bif].nstrict; i++) {
        curf->s->data[curf->s->count-1-i] = resolve_ind(curf->s->data[curf->s->count-1-i]); /* bifs expect it */
        assert(TYPE_APPLICATION != celltype((cell*)curf->s->data[curf->s->count-1-i]));
        assert(TYPE_CAP != celltype((cell*)curf->s->data[curf->s->count-1-i]));
        assert(TYPE_FRAME != celltype((cell*)curf->s->data[curf->s->count-1-i]));
      }

      #ifdef EXTRA_TRACE
      if (trace) {
        printf("builtin %s\n",builtin_info[bif].name);
        printed++;
      }
      #endif


      for (i = 0; i < nargs; i++)
        assert(TYPE_IND != celltype((cell*)curf->s->data[curf->s->count-1-i]));

      builtin_info[bif].f((cell**)(&curf->s->data[curf->s->count-nargs]));
      curf->s->count -= (nargs-1);
      break;
    }
    case OP_PUSHNIL:
      stack_push(curf->s,globnil);
      break;
    case OP_PUSHINT:
      stack_push(curf->s,alloc_cell2(TYPE_INT,(void*)instr->arg0,NULL));
      break;
    case OP_PUSHDOUBLE:
      stack_push(curf->s,alloc_cell2(TYPE_DOUBLE,(void*)instr->arg0,(void*)instr->arg1));
      break;
    case OP_PUSHSTRING:
      stack_push(curf->s,((cell**)gp->stringmap->data)[instr->arg0]);
      break;
    default:
      assert(0);
      break;
    }

    #ifdef EXTRA_TRACE
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
