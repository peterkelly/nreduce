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
#include "nreduce.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <assert.h>
#include <ctype.h>
#include <stdarg.h>
#include <math.h>

int resolve_var(int oldcount, scomb *sc, cell **k)
{
  int varno;
  cell *var = *k;

  assert(TYPE_IND != celltype(var)); /* var comes from the scomb */

  if (TYPE_SYMBOL != celltype(var))
    return 0;
  varno = get_scomb_var(sc,(char*)var->field1);
/*   assert(NULL == var->field2); */

  if (0 <= varno) {
    int stackpos = oldcount-1-varno;
    assert(0 <= stackpos);
    *k = (cell*)stackat(stackpos);
    return 1;
  }
  return 0;
}

void inschild(int oldcount, scomb *sc, int base, cell **dt, cell *source)
{
  cell *oldsource;
  assert(TYPE_IND != celltype(source)); /* source comes from the scomb */

  oldsource = source;

  resolve_var(oldcount,sc,&source);
  if (oldsource != source) {
    (*dt) = source;
  }
  else if (source->tag & FLAG_PROCESSED) {
    int b;
    int found = 0;
    for (b = oldcount; b < base; b += 2) {
      cell *d = stackat(b);
      cell *s = stackat(b+1);
      assert(s->tag & FLAG_PROCESSED);
      if (s == source) {
        (*dt) = d;
        found = 1;
        break;
      }
    }
    assert(found);
  }
  else {
    (*dt) = alloc_cell();
    insert(source,base);
    insert((*dt),base);
  }
}

void instantiate_scomb(cell *dest, cell *source, scomb *sc)
{
  int oldcount = stackcount;
  int base = stackcount;
  assert(TYPE_IND != celltype(source)); /* source comes from the scomb */
  assert(TYPE_EMPTY != celltype(source));

  push(dest);
  push(source);
  assert(sc->cells);
  cleargraph(sc->body,FLAG_PROCESSED);
  while (base < stackcount) {
    int wasarg;

    dest = stackat(base++);

    assert(TYPE_IND != celltype(stack[base])); /* stack[base] comes from the scomb */
    wasarg = resolve_var(oldcount,sc,&stack[base]);
    source = stackat(base++);
    assert(TYPE_IND != celltype(source));

    source->tag |= FLAG_PROCESSED;

    if (wasarg) {
      free_cell_fields(dest);
      dest->tag = TYPE_IND;
      dest->field1 = source;
    }
    else {
      switch (celltype(source)) {
      case TYPE_APPLICATION:
      case TYPE_CONS:
        free_cell_fields(dest);
        dest->tag = celltype(source);

        inschild(oldcount,sc,base,(cell**)(&dest->field1),(cell*)source->field1);
        inschild(oldcount,sc,base,(cell**)(&dest->field2),(cell*)source->field2);

        break;
      case TYPE_LAMBDA:
        free_cell_fields(dest);
        dest->tag = TYPE_LAMBDA;
        dest->field1 = strdup((char*)source->field1);
        inschild(oldcount,sc,base,(cell**)(&dest->field2),(cell*)source->field2);
        break;
      case TYPE_BUILTIN:
      case TYPE_NIL:
      case TYPE_INT:
      case TYPE_DOUBLE:
      case TYPE_STRING:
      case TYPE_SYMBOL:
      case TYPE_SCREF:
        copy_cell(dest,source);
        break;
      default:
        printf("instantiate_scomb: unknown cell type %d\n",celltype(source));
        assert(0);
        break;
      }
    }
  }

  stackcount = oldcount;
}

void reduce()
{
  int reductions = 0;

  /* REPEAT */
  while (1) {
    int oldtop = stackcount;
    cell *target;
    cell *redex;

    nreductions++;


    /* FIXME: if we call collect() here then sometimes the redex gets collected */
    if (nallocs > COLLECT_THRESHOLD)
      collect();

    redex = stack[stackcount-1];
    reductions++;

    target = resolve_ind(redex);

    /* 1. Unwind the spine until something other than an application node is encountered. */
    push(target);

    while (TYPE_APPLICATION == celltype(target)) {
      target = resolve_ind((cell*)target->field1);
      push(target);
    }

    /* 2. Examine the cell  at the tip of the spine */
    switch (celltype(target)) {

    /* A variable. This should correspond to a supercombinator, and if it doesn't it means
       something has gone wrong. */
    case TYPE_SYMBOL:
      assert(!"variable encountered during reduction");
      break;
    case TYPE_SCREF: {
      scomb *sc = (scomb*)target->field1;

      int i;
      int destno;
      cell *dest;

      destno = stackcount-1-sc->nargs;
      dest = stackat(destno);

      nscombappls++;

      /* If there are not enough arguments to the supercombinator, we cannot instantiate it.
         The expression is in WHNF, so we can return. */
      if (stackcount-1-oldtop < sc->nargs) {
        stackcount = oldtop;
        return;
      }

      /* We have enough arguments present to instantiate the supercombinator */
      for (i = stackcount-1; i >= stackcount-sc->nargs; i--) {
        assert(i > oldtop);
        assert(TYPE_APPLICATION == celltype(stackat(i-1)));
        stack[i] = (cell*)stackat(i-1)->field2;
      }

      instantiate_scomb(dest,sc->body,sc);

      stackcount = oldtop;
      continue;
    }
    case TYPE_CONS:
    case TYPE_AREF:
    case TYPE_NIL:
    case TYPE_INT:
    case TYPE_DOUBLE:
    case TYPE_STRING:
      /* The item at the tip of the spine is a value; this means the expression is in WHNF.
         If there are one or more arguments "applied" to this value then it's considered an
         error, e.g. caused by an attempt to pass more arguments to a function than it requires. */
      if (1 < stackcount-oldtop) {
        printf("Attempt to apply %d arguments to a value: ",stackcount-oldtop-1);
        print_code(target);
        printf("\n");
        exit(1);
      }

      stackcount = oldtop;
      return;
      /* b. A built-in function. Check the number of arguments available. If there are too few
         arguments the expression is in WHNF so STOP. Otherwise evaluate any arguments required,
         execute the built-in function and overwrite the root of the redex with the result. */
    case TYPE_BUILTIN: {

      int bif = (int)target->field1;
      int reqargs;
      int strictargs;
      int i;
      assert(0 <= bif);
      assert(NUM_BUILTINS > bif);

      reqargs = builtin_info[bif].nargs;
      strictargs = builtin_info[bif].nstrict;

      if (stackcount-1 < reqargs + oldtop) {
        fprintf(stderr,"Built-in function %s requires %d args; have only %d\n",
                builtin_info[bif].name,reqargs,stackcount-1-oldtop);
        exit(1);
      }

      for (i = stackcount-1; i >= stackcount-reqargs; i--) {
        assert(i > oldtop);
        assert(TYPE_APPLICATION == celltype(stackat(i-1)));
        stack[i] = (cell*)stackat(i-1)->field2;
      }

      assert(strictargs <= reqargs);
      for (i = 0; i < strictargs; i++) {
        push(stack[stackcount-1-i]);
        reduce();
        stack[stackcount-1-i] = pop();
      }

      builtin_info[bif].f();

      /* UPDATE */

      stack[stackcount-1] = resolve_ind(stack[stackcount-1]);

      free_cell_fields(stack[stackcount-2]);
      stack[stackcount-2]->tag = TYPE_IND;
      stack[stackcount-2]->field1 = stack[stackcount-1];

      stackcount--;
      break;
    }
    default:
      assert(0);
      break;
    }

    stackcount = oldtop;
  }
  /* END */
}
