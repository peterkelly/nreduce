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
#include "nreduce.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <assert.h>
#include <ctype.h>
#include <stdarg.h>
#include <math.h>

static void instantiate_scomb_r(cell *dest, cell *source, stack *names, stack *values)
{
  switch (celltype(source)) {
  case TYPE_APPLICATION:
    dest->tag = TYPE_APPLICATION;
    dest->field1 = alloc_cell();
    dest->field2 = alloc_cell();
    instantiate_scomb_r((cell*)dest->field1,(cell*)source->field1,names,values);
    instantiate_scomb_r((cell*)dest->field2,(cell*)source->field2,names,values);
    break;
  case TYPE_SYMBOL: {
    int pos;
    for (pos = names->count-1; 0 <= pos; pos--) {
      if (!strcmp((char*)names->data[pos],(char*)source->field1)) {
        dest->tag = TYPE_IND;
        dest->field1 = (cell*)values->data[pos];
        return;
      }
    }
    fprintf(stderr,"Unknown variable: %s\n",(char*)source->field1);
    break;
  }
  case TYPE_LETREC: {
    int oldcount = names->count;
    letrec *rec;
    int i;
    for (rec = (letrec*)source->field1; rec; rec = rec->next) {
      stack_push(names,(char*)rec->name);
      stack_push(values,alloc_cell());
    }
    i = 0;
    for (rec = (letrec*)source->field1; rec; rec = rec->next) {
      cell *val = (cell*)values->data[oldcount+i];
      instantiate_scomb_r(val,rec->value,names,values);
      i++;
    }
    instantiate_scomb_r(dest,(cell*)source->field2,names,values);
    names->count = oldcount;
    values->count = oldcount;
    break;
  }
  case TYPE_BUILTIN:
  case TYPE_SCREF:
  case TYPE_NIL:
  case TYPE_INT:
  case TYPE_DOUBLE:
  case TYPE_STRING:
    copy_cell(dest,source);
    break;
  }
}

static void instantiate_scomb(stack *s, cell *dest, cell *source, scomb *sc)
{
  stack *names = stack_new();
  stack *values = stack_new();

  int i;
  for (i = 0; i < sc->nargs; i++) {
    int pos = s->count-1-i;
    assert(0 <= pos);
    stack_push(names,(char*)sc->argnames[i]);
    stack_push(values,(cell*)stack_at(s,pos));
  }

  instantiate_scomb_r(dest,source,names,values);

  stack_free(names);
  stack_free(values);
}

void reduce(stack *s)
{
  int reductions = 0;

  /* REPEAT */
  while (1) {
    int oldtop = s->count;
    cell *target;
    cell *redex;

    nreductions++;


    /* FIXME: if we call collect() here then sometimes the redex gets collected */
    if (nallocs > COLLECT_THRESHOLD)
      collect();

    redex = (cell*)s->data[s->count-1];
    reductions++;

    target = resolve_ind(redex);

    /* 1. Unwind the spine until something other than an application node is encountered. */
    stack_push(s,target);

    while (TYPE_APPLICATION == celltype(target)) {
      target = resolve_ind((cell*)target->field1);
      stack_push(s,target);
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

      destno = s->count-1-sc->nargs;
      dest = stack_at(s,destno);

      nscombappls++;

      /* If there are not enough arguments to the supercombinator, we cannot instantiate it.
         The expression is in WHNF, so we can return. */
      if (s->count-1-oldtop < sc->nargs) {
        s->count = oldtop;
        return;
      }

      /* We have enough arguments present to instantiate the supercombinator */
      for (i = s->count-1; i >= s->count-sc->nargs; i--) {
        assert(i > oldtop);
        assert(TYPE_APPLICATION == celltype((cell*)stack_at(s,i-1)));
        s->data[i] = (cell*)((cell*)stack_at(s,i-1))->field2;
      }

      instantiate_scomb(s,dest,sc->body,sc);

      s->count = oldtop;
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
      if (1 < s->count-oldtop) {
        printf("Attempt to apply %d arguments to a value: ",s->count-oldtop-1);
        print_code(target);
        printf("\n");
        exit(1);
      }

      s->count = oldtop;
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

      if (s->count-1 < reqargs + oldtop) {
        fprintf(stderr,"Built-in function %s requires %d args; have only %d\n",
                builtin_info[bif].name,reqargs,s->count-1-oldtop);
        exit(1);
      }

      for (i = s->count-1; i >= s->count-reqargs; i--) {
        assert(i > oldtop);
        assert(TYPE_APPLICATION == celltype((cell*)stack_at(s,i-1)));
        s->data[i] = (cell*)((cell*)stack_at(s,i-1))->field2;
      }

      assert(strictargs <= reqargs);
      for (i = 0; i < strictargs; i++) {
        stack_push(s,s->data[s->count-1-i]);
        reduce(s);
        s->data[s->count-1-i] = stack_pop(s);
      }

      for (i = 0; i < strictargs; i++)
        assert(TYPE_IND != celltype((cell*)s->data[s->count-1-i]));

      builtin_info[bif].f((cell**)&s->data[s->count-reqargs]);
      s->count -= (reqargs-1);

      /* UPDATE */

      s->data[s->count-1] = resolve_ind((cell*)s->data[s->count-1]);

      free_cell_fields((cell*)s->data[s->count-2]);
      ((cell*)s->data[s->count-2])->tag = TYPE_IND;
      ((cell*)s->data[s->count-2])->field1 = s->data[s->count-1];

      s->count--;
      break;
    }
    default:
      fprintf(stderr,"Encountered %s\n",cell_types[celltype(target)]);
      assert(0);
      break;
    }

    s->count = oldtop;
  }
  /* END */
}
