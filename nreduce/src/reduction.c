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

static pntr instantiate_scomb_r(cell *source, stack *names, pntrstack *values)
{
  rtvalue *dest;
  switch (celltype(source)) {
  case TYPE_APPLICATION: {
    dest = alloc_rtvalue();
    dest->tag = TYPE_APPLICATION;
    dest->cmp1 = instantiate_scomb_r((cell*)source->field1,names,values);
    dest->cmp2 = instantiate_scomb_r((cell*)source->field2,names,values);
    return make_pntr(dest);
  }
  case TYPE_SYMBOL: {
    int pos;
    for (pos = names->count-1; 0 <= pos; pos--) {
      if (!strcmp((char*)names->data[pos],(char*)source->field1)) {
        dest = alloc_rtvalue();
        dest->tag = TYPE_IND;
        dest->cmp1 = values->data[pos];
        return make_pntr(dest);
      }
    }
    fprintf(stderr,"Unknown variable: %s\n",(char*)source->field1);
    exit(1);
    return make_pntr(NULL);
  }
  case TYPE_LETREC: {
    int oldcount = names->count;
    letrec *rec;
    int i;
    pntr res;
    for (rec = (letrec*)source->field1; rec; rec = rec->next) {
      rtvalue *hole = alloc_rtvalue();
      hole->tag = TYPE_HOLE;
      stack_push(names,(char*)rec->name);
      pntrstack_push(values,make_pntr(hole));
    }
    i = 0;
    for (rec = (letrec*)source->field1; rec; rec = rec->next) {
      res = instantiate_scomb_r(rec->value,names,values);
      assert(TYPE_HOLE == get_pntr(values->data[oldcount+i])->tag);
      get_pntr(values->data[oldcount+i])->tag = TYPE_IND;
      get_pntr(values->data[oldcount+i])->cmp1 = res;
      i++;
    }
    res = instantiate_scomb_r((cell*)source->field2,names,values);
    names->count = oldcount;
    values->count = oldcount;
    return res;
  }
  case TYPE_BUILTIN:
    dest = alloc_rtvalue();
    dest->tag = TYPE_BUILTIN;
    dest->cmp1 = make_pntr(source->field1);
    return make_pntr(dest);
  case TYPE_SCREF:
    dest = alloc_rtvalue();
    dest->tag = TYPE_SCREF;
    dest->cmp1 = make_pntr((scomb*)source->field1);
    return make_pntr(dest);
  case TYPE_NIL:
    return globnilpntr;
  case TYPE_NUMBER:
    return make_number(celldouble(source));
  case TYPE_STRING:
    dest = alloc_rtvalue();
    dest->tag = TYPE_STRING;
    dest->cmp1 = make_string(strdup((char*)source->field1));
    return make_pntr(dest);
  default:
    abort();
    break;
  }
}

static pntr instantiate_scomb(pntrstack *s, cell *source, scomb *sc)
{
  stack *names = stack_new();
  pntrstack *values = pntrstack_new();
  pntr res;

  int i;
  for (i = 0; i < sc->nargs; i++) {
    int pos = s->count-1-i;
    assert(0 <= pos);
    stack_push(names,(char*)sc->argnames[i]);
    pntrstack_push(values,pntrstack_at(s,pos));
  }

  res = instantiate_scomb_r(source,names,values);

  stack_free(names);
  pntrstack_free(values);
  return res;
}

void reduce(pntrstack *s)
{
  int reductions = 0;

  /* REPEAT */
  while (1) {
    int oldtop = s->count;
    pntr target;
    pntr redex;

    nreductions++;


    /* FIXME: if we call collect() here then sometimes the redex gets collected */
    if (nallocs > COLLECT_THRESHOLD)
      rtcollect();

    redex = s->data[s->count-1];
    reductions++;

    target = resolve_pntr(redex);

    /* 1. Unwind the spine until something other than an application node is encountered. */
    pntrstack_push(s,target);

    while (TYPE_APPLICATION == pntrtype(target)) {
      target = resolve_pntr(get_pntr(target)->cmp1);
      pntrstack_push(s,target);
    }

    /* 2. Examine the cell  at the tip of the spine */
    switch (pntrtype(target)) {

    /* A variable. This should correspond to a supercombinator, and if it doesn't it means
       something has gone wrong. */
    case TYPE_SYMBOL:
      assert(!"variable encountered during reduction");
      break;
    case TYPE_SCREF: {
      scomb *sc = (scomb*)get_pntr(get_pntr(target)->cmp1);

      int i;
      int destno;
      pntr dest;
      pntr res;

      destno = s->count-1-sc->nargs;
      dest = pntrstack_at(s,destno);

      nscombappls++;

      /* If there are not enough arguments to the supercombinator, we cannot instantiate it.
         The expression is in WHNF, so we can return. */
      if (s->count-1-oldtop < sc->nargs) {
        s->count = oldtop;
        return;
      }

      /* We have enough arguments present to instantiate the supercombinator */
      for (i = s->count-1; i >= s->count-sc->nargs; i--) {
        pntr arg;
        assert(i > oldtop);
        arg = pntrstack_at(s,i-1);
        assert(TYPE_APPLICATION == pntrtype(arg));
        s->data[i] = get_pntr(arg)->cmp2;
      }

      assert((TYPE_APPLICATION == pntrtype(dest)) ||
             (TYPE_SCREF == pntrtype(dest)));
      res = instantiate_scomb(s,sc->body,sc);
      get_pntr(dest)->tag = TYPE_IND;
      get_pntr(dest)->cmp1 = res;

      s->count = oldtop;
      continue;
    }
    case TYPE_CONS:
    case TYPE_AREF:
    case TYPE_NIL:
    case TYPE_NUMBER:
    case TYPE_STRING:
      /* The item at the tip of the spine is a value; this means the expression is in WHNF.
         If there are one or more arguments "applied" to this value then it's considered an
         error, e.g. caused by an attempt to pass more arguments to a function than it requires. */
      if (1 < s->count-oldtop) {
        printf("Attempt to apply %d arguments to a value: ",s->count-oldtop-1);
        print_pntr(target);
        printf("\n");
        exit(1);
      }

      s->count = oldtop;
      return;
      /* b. A built-in function. Check the number of arguments available. If there are too few
         arguments the expression is in WHNF so STOP. Otherwise evaluate any arguments required,
         execute the built-in function and overwrite the root of the redex with the result. */
    case TYPE_BUILTIN: {

      int bif = (int)get_pntr(get_pntr(target)->cmp1);
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
        pntr arg = pntrstack_at(s,i-1);
        assert(i > oldtop);
        assert(TYPE_APPLICATION == pntrtype(arg));
        s->data[i] = get_pntr(arg)->cmp2;
      }

      assert(strictargs <= reqargs);
      for (i = 0; i < strictargs; i++) {
        pntr val;
        pntrstack_push(s,s->data[s->count-1-i]);
        reduce(s);
        val = pntrstack_pop(s);
        s->data[s->count-1-i] = val;
      }

      for (i = 0; i < strictargs; i++)
        assert(TYPE_IND != pntrtype(s->data[s->count-1-i]));

      builtin_info[bif].f(&s->data[s->count-reqargs]);
      s->count -= (reqargs-1);

      /* UPDATE */

      s->data[s->count-1] = resolve_pntr(s->data[s->count-1]);

      free_rtvalue_fields(get_pntr(s->data[s->count-2]));
      get_pntr(s->data[s->count-2])->tag = TYPE_IND;
      get_pntr(s->data[s->count-2])->cmp1 = s->data[s->count-1];

      s->count--;
      break;
    }
    default:
      fprintf(stderr,"Encountered %s\n",cell_types[pntrtype(target)]);
      assert(0);
      break;
    }

    s->count = oldtop;
  }
  /* END */
}
