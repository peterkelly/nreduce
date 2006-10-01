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

#include "src/nreduce.h"
#include "compiler/source.h"
#include "runtime/runtime.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <assert.h>
#include <ctype.h>
#include <stdarg.h>
#include <math.h>

static pntr instantiate_scomb_r(process *proc, snode *source, stack *names, pntrstack *values)
{
  cell *dest;
  pntr p;
  switch (snodetype(source)) {
  case TYPE_APPLICATION: {
    dest = alloc_cell(proc);
    dest->type = TYPE_APPLICATION;
    dest->field1 = instantiate_scomb_r(proc,source->left,names,values);
    dest->field2 = instantiate_scomb_r(proc,source->right,names,values);
    make_pntr(p,dest);
    return p;
  }
  case TYPE_SYMBOL: {
    int pos;
    for (pos = names->count-1; 0 <= pos; pos--) {
      if (!strcmp((char*)names->data[pos],source->name)) {
        dest = alloc_cell(proc);
        dest->type = TYPE_IND;
        dest->field1 = values->data[pos];
        make_pntr(p,dest);
        return p;
      }
    }
    fprintf(stderr,"Unknown variable: %s\n",source->name);
    exit(1);
    make_pntr(p,NULL);
    return p;
  }
  case TYPE_LETREC: {
    int oldcount = names->count;
    letrec *rec;
    int i;
    pntr res;
    for (rec = source->bindings; rec; rec = rec->next) {
      cell *hole = alloc_cell(proc);
      hole->type = TYPE_HOLE;
      stack_push(names,(char*)rec->name);
      make_pntr(p,hole);
      pntrstack_push(values,p);
    }
    i = 0;
    for (rec = source->bindings; rec; rec = rec->next) {
      res = instantiate_scomb_r(proc,rec->value,names,values);
      assert(TYPE_HOLE == get_pntr(values->data[oldcount+i])->type);
      get_pntr(values->data[oldcount+i])->type = TYPE_IND;
      get_pntr(values->data[oldcount+i])->field1 = res;
      i++;
    }
    res = instantiate_scomb_r(proc,source->body,names,values);
    names->count = oldcount;
    values->count = oldcount;
    return res;
  }
  case TYPE_BUILTIN:
    dest = alloc_cell(proc);
    dest->type = TYPE_BUILTIN;
    make_pntr(dest->field1,source->bif);
    make_pntr(p,dest);
    return p;
  case TYPE_SCREF:
    dest = alloc_cell(proc);
    dest->type = TYPE_SCREF;
    make_pntr(dest->field1,source->sc);
    make_pntr(p,dest);
    return p;
  case TYPE_NIL:
    return proc->globnilpntr;
  case TYPE_NUMBER:
    return make_number(source->num);
  case TYPE_STRING:
    dest = alloc_cell(proc);
    dest->type = TYPE_STRING;
    make_pntr(dest->field1,strdup(source->value));
    make_pntr(p,dest);
    return p;
  default:
    abort();
    break;
  }
}

static pntr instantiate_scomb(process *proc, pntrstack *s, snode *source, scomb *sc)
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

  res = instantiate_scomb_r(proc,source,names,values);

  stack_free(names);
  pntrstack_free(values);
  return res;
}

void reduce(process *proc, pntrstack *s)
{
  int reductions = 0;

  assert(proc);

  /* REPEAT */
  while (1) {
    int oldtop = s->count;
    pntr target;
    pntr redex;

    proc->stats.nreductions++;


    /* FIXME: if we call collect() here then sometimes the redex gets collected */
    if (proc->stats.nallocs > COLLECT_THRESHOLD)
      local_collect(proc);

    redex = s->data[s->count-1];
    reductions++;

    target = resolve_pntr(redex);

    /* 1. Unwind the spine until something other than an application node is encountered. */
    pntrstack_push(s,target);

    while (TYPE_APPLICATION == pntrtype(target)) {
      target = resolve_pntr(get_pntr(target)->field1);
      pntrstack_push(s,target);
    }

    /* 2. Examine the cell at the tip of the spine */
    switch (pntrtype(target)) {

    /* A variable. This should correspond to a supercombinator, and if it doesn't it means
       something has gone wrong. */
    case TYPE_SYMBOL:
      fatal("variable encountered during reduction");
      break;
    case TYPE_SCREF: {
      scomb *sc = (scomb*)get_pntr(get_pntr(target)->field1);

      int i;
      int destno;
      pntr dest;
      pntr res;

      destno = s->count-1-sc->nargs;
      dest = pntrstack_at(s,destno);

      proc->stats.nscombappls++;

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
        s->data[i] = get_pntr(arg)->field2;
      }

      assert((TYPE_APPLICATION == pntrtype(dest)) ||
             (TYPE_SCREF == pntrtype(dest)));
      res = instantiate_scomb(proc,s,sc->body,sc);
      get_pntr(dest)->type = TYPE_IND;
      get_pntr(dest)->field1 = res;

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
        print_pntr(stdout,target);
        printf("\n");
        exit(1);
      }

      s->count = oldtop;
      return;
      /* b. A built-in function. Check the number of arguments available. If there are too few
         arguments the expression is in WHNF so STOP. Otherwise evaluate any arguments required,
         execute the built-in function and overwrite the root of the redex with the result. */
    case TYPE_BUILTIN: {

      int bif = (int)get_pntr(get_pntr(target)->field1);
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
        s->data[i] = get_pntr(arg)->field2;
      }

      assert(strictargs <= reqargs);
      for (i = 0; i < strictargs; i++) {
        pntr val;
        pntrstack_push(s,s->data[s->count-1-i]);
        reduce(proc,s);
        val = pntrstack_pop(s);
        s->data[s->count-1-i] = val;
      }

      for (i = 0; i < strictargs; i++)
        assert(TYPE_IND != pntrtype(s->data[s->count-1-i]));

      builtin_info[bif].f(proc,&s->data[s->count-reqargs]);
      if (proc->error) {
        abort();
      }
      s->count -= (reqargs-1);

      /* UPDATE */

      s->data[s->count-1] = resolve_pntr(s->data[s->count-1]);

      free_cell_fields(proc,get_pntr(s->data[s->count-2]));
      get_pntr(s->data[s->count-2])->type = TYPE_IND;
      get_pntr(s->data[s->count-2])->field1 = s->data[s->count-1];

      s->count--;
      break;
    }
    default:
      fprintf(stderr,"Encountered %s\n",cell_types[pntrtype(target)]);
      abort();
      break;
    }

    s->count = oldtop;
  }
  /* END */
}

static void stream(process *proc, pntr lst)
{
  proc->streamstack = pntrstack_new();
  pntrstack_push(proc->streamstack,lst);
  while (0 < proc->streamstack->count) {
    pntr p;
    reduce(proc,proc->streamstack);

    p = pntrstack_pop(proc->streamstack);
    p = resolve_pntr(p);
    if (TYPE_NIL == pntrtype(p)) {
      /* nothing */
    }
    else if (TYPE_NUMBER == pntrtype(p)) {
      printf("%f",pntrdouble(p));
    }
    else if (TYPE_STRING == pntrtype(p)) {
      printf("%s",(char*)get_pntr(get_pntr(p)->field1));
    }
    else if (TYPE_CONS == pntrtype(p)) {
      pntrstack_push(proc->streamstack,get_pntr(p)->field2);
      pntrstack_push(proc->streamstack,get_pntr(p)->field1);
    }
    else if (TYPE_APPLICATION == pntrtype(p)) {
      fprintf(stderr,"Too many arguments applied to function\n");
      exit(1);
    }
    else {
      fprintf(stderr,"Bad cell type returned to printing mechanism: %s\n",cell_types[pntrtype(p)]);
      exit(1);
    }
  }
  pntrstack_free(proc->streamstack);
  proc->streamstack = NULL;
}

void run_reduction(source *src, FILE *stats)
{
  scomb *mainsc;
  cell *root;
  pntr rootp;
  process *proc = process_new();
#ifdef TIMING
  struct timeval start;
  struct timeval end;
  int ms;
#endif

  debug_stage("Reduction engine");
  mainsc = get_scomb(src,"main");

  root = alloc_cell(proc);
  root->type = TYPE_SCREF;
  make_pntr(root->field1,mainsc);
  make_pntr(rootp,root);

#ifdef TIMING
  gettimeofday(&start,NULL);
#endif
  stream(proc,rootp);
#ifdef TIMING
  gettimeofday(&end,NULL);
  ms = (end.tv_sec - start.tv_sec)*1000 +
       (end.tv_usec - start.tv_usec)/1000;
  if (stats)
    fprintf(stats,"Execution time: %.3fs\n",((double)ms)/1000.0);
#endif

  printf("\n");

  if (stats)
    statistics(proc,stats);

  process_free(proc);
}
