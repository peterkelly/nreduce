/*
 * This file is part of the NReduce project
 * Copyright (C) 2006-2010 Peter Kelly <kellypmk@gmail.com>
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

static pntrstack *pntrstack_new(void)
{
  pntrstack *s = (pntrstack*)calloc(1,sizeof(pntrstack));
  s->alloc = 1;
  s->count = 0;
  s->data = (pntr*)malloc(sizeof(pntr));
  s->limit = STACK_LIMIT;
  return s;
}

static void pntrstack_grow(int *alloc, pntr **data, int size)
{
  if (*alloc < size) {
    *alloc = size;
    *data = (pntr*)realloc(*data,(*alloc)*sizeof(pntr));
  }
}

static void pntrstack_push(pntrstack *s, pntr p)
{
  if (s->count == s->alloc) {
    if ((0 <= s->limit) && (s->count >= s->limit)) {
      fprintf(stderr,"Out of stack space\n");
      exit(1);
    }
    pntrstack_grow(&s->alloc,&s->data,s->alloc*2);
  }
  s->data[s->count++] = p;
}

static pntr pntrstack_at(pntrstack *s, int pos)
{
  assert(0 <= pos);
  assert(pos < s->count);
  return resolve_pntr(s->data[pos]);
}

static pntr pntrstack_pop(pntrstack *s)
{
  assert(0 < s->count);
  return resolve_pntr(s->data[--s->count]);
}

static void pntrstack_free(pntrstack *s)
{
  free(s->data);
  free(s);
}

static pntr makescref(task *tsk, scomb *sc)
{
  cell *c = alloc_cell(tsk);
  pntr p;
  c->type = CELL_SCREF;
  make_pntr(c->field1,sc);
  make_pntr(p,c);
  return p;
}

static pntr instantiate_scomb_r(task *tsk, scomb *sc, snode *source,
                                stack *names, pntrstack *values)
{
  cell *dest;
  pntr p;
  switch (source->type) {
  case SNODE_APPLICATION: {
    dest = alloc_cell(tsk);
    dest->type = CELL_APPLICATION;
    dest->field1 = instantiate_scomb_r(tsk,sc,source->left,names,values);
    dest->field2 = instantiate_scomb_r(tsk,sc,source->right,names,values);
    make_pntr(p,dest);
    return p;
  }
  case SNODE_SYMBOL: {
    int pos;
    for (pos = names->count-1; 0 <= pos; pos--) {
      if (!strcmp((char*)names->data[pos],source->name)) {
        dest = alloc_cell(tsk);
        dest->type = CELL_IND;
        dest->field1 = values->data[pos];
        make_pntr(p,dest);
        return p;
      }
    }
    fprintf(stderr,"Unknown variable: %s\n",source->name);
    exit(1);
    return NULL_PNTR;
  }
  case SNODE_LETREC: {
    int oldcount = names->count;
    letrec *rec;
    int i;
    pntr res;
    for (rec = source->bindings; rec; rec = rec->next) {
      cell *hole = alloc_cell(tsk);
      hole->type = CELL_HOLE;
      stack_push(names,(char*)rec->name);
      make_pntr(p,hole);
      pntrstack_push(values,p);
    }
    i = 0;
    for (rec = source->bindings; rec; rec = rec->next) {
      res = instantiate_scomb_r(tsk,sc,rec->value,names,values);
      assert(CELL_HOLE == get_pntr(values->data[oldcount+i])->type);
      cell_make_ind(tsk,get_pntr(values->data[oldcount+i]),res);
      i++;
    }
    res = instantiate_scomb_r(tsk,sc,source->body,names,values);
    names->count = oldcount;
    values->count = oldcount;
    return res;
  }
  case SNODE_BUILTIN:
    dest = alloc_cell(tsk);
    dest->type = CELL_BUILTIN;
    make_pntr(dest->field1,source->bif);
    make_pntr(p,dest);
    return p;
  case SNODE_SCREF: {
    return makescref(tsk,source->sc);
  }
  case SNODE_NIL:
    return tsk->globnilpntr;
  case SNODE_NUMBER: {
    pntr p;
    set_pntrdouble(p,source->num);
    return p;
  }
  case SNODE_STRING:
    return string_to_array(tsk,source->value);
  default:
    abort();
    break;
  }
}

pntr instantiate_scomb(task *tsk, pntrstack *s, scomb *sc)
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

  res = instantiate_scomb_r(tsk,sc,sc->body,names,values);

  stack_free(names);
  pntrstack_free(values);
  return res;
}

static void reduce_single(task *tsk, pntrstack *s, pntr p)
{
  pntrstack_push(s,p);
  reduce(tsk,s);
  pntrstack_pop(s);
}

void reduce(task *tsk, pntrstack *s)
{
  int reductions = 0;
  pntr redex = s->data[s->count-1];
  if (CELL_NUMBER != pntrtype(redex)) {
    if (get_pntr(redex)->flags & FLAG_REDUCED)
      return;
    get_pntr(redex)->flags |= FLAG_REDUCED;
  }

  assert(tsk);

  /* REPEAT */
  while (1) {
    int oldtop = s->count;
    pntr target;

    if (tsk->need_minor)
      local_collect(tsk);

    redex = s->data[s->count-1];
    reductions++;

    target = resolve_pntr(redex);

    trace_step(tsk,target,0,"Performing reduction");

    /* 1. Unwind the spine until something other than an application node is encountered. */
    pntrstack_push(s,target);

    while (CELL_APPLICATION == pntrtype(target)) {
      target = resolve_pntr(get_pntr(target)->field1);
      trace_step(tsk,target,0,"Unwound spine");
      pntrstack_push(s,target);
    }

    /* 2. Examine the cell at the tip of the spine */
    switch (pntrtype(target)) {

    case CELL_SCREF: {
      int i;
      int destno;
      pntr dest;
      pntr res;
      scomb *sc = (scomb*)get_pntr(get_pntr(target)->field1);

      /* If there are not enough arguments to the supercombinator, we cannot instantiate it.
         The expression is in WHNF, so we can return. */
      if (s->count-1-oldtop < sc->nargs) {
        /* TODO: maybe reduce the args that we do have? */
        s->count = oldtop;
        return;
      }

      destno = s->count-1-sc->nargs;
      dest = pntrstack_at(s,destno);

      /* We have enough arguments present to instantiate the supercombinator */
      for (i = s->count-1; i >= s->count-sc->nargs; i--) {
        pntr arg;
        assert(i > oldtop);
        arg = pntrstack_at(s,i-1);
        assert(CELL_APPLICATION == pntrtype(arg));
        s->data[i] = get_pntr(arg)->field2;
      }

      assert((CELL_APPLICATION == pntrtype(dest)) ||
             (CELL_SCREF == pntrtype(dest)));

      trace_step(tsk,redex,1,"Instantiating supercombinator %s",sc->name);
      res = instantiate_scomb(tsk,s,sc);
      cell_make_ind(tsk,get_pntr(dest),res);

      s->count = oldtop;
      continue;
    }
    case CELL_CONS:
    case CELL_AREF:
    case CELL_NIL:
    case CELL_NUMBER:
      /* The item at the tip of the spine is a value; this means the expression is in WHNF.
         If there are one or more arguments "applied" to this value then it's considered an
         error, e.g. caused by an attempt to pass more arguments to a function than it requires. */
      if (1 < s->count-oldtop) {
        printf("Attempt to apply %d arguments to a value\n",s->count-oldtop-1);
        exit(1);
      }

      s->count = oldtop;
      return;
    case CELL_SYMBOL:
      s->count = oldtop;
      return;
      /* b. A built-in function. Check the number of arguments available. If there are too few
         arguments the expression is in WHNF so STOP. Otherwise evaluate any arguments required,
         execute the built-in function and overwrite the root of the redex with the result. */
    case CELL_BUILTIN: {

      int bif = (int)get_pntr(get_pntr(target)->field1);
      int reqargs;
      int strictargs;
      int i;
      int strictok = 0;
      assert(0 <= bif);
      assert(NUM_BUILTINS > bif);

      reqargs = builtin_info[bif].nargs;
      strictargs = builtin_info[bif].nstrict;

      if (s->count-1 < reqargs + oldtop) {
        fprintf(stderr,"Built-in function %s requires %d args; have only %d\n",
                builtin_info[bif].name,reqargs,s->count-1-oldtop);
        exit(1);
      }

      /* Replace application cells on stack with the corresponding arguments */
      for (i = s->count-1; i >= s->count-reqargs; i--) {
        pntr arg = pntrstack_at(s,i-1);
        assert(i > oldtop);
        assert(CELL_APPLICATION == pntrtype(arg));
        s->data[i] = get_pntr(arg)->field2;
      }

      /* Reduce arguments */
      for (i = 0; i < strictargs; i++)
        reduce_single(tsk,s,s->data[s->count-1-i]);

      /* Ensure all stack items are values (not indirection cells) */
      for (i = 0; i < strictargs; i++)
        s->data[s->count-1-i] = resolve_pntr(s->data[s->count-1-i]);

      /* Are any strict arguments a SYMBOL? */
      for (i = 0; i < strictargs; i++) {
        pntr argval = resolve_pntr(s->data[s->count-1-i]);
        if ((CELL_SYMBOL != pntrtype(argval)) && (CELL_APPLICATION != pntrtype(argval))) {
          strictok++;
        }
        else {
          trace_step(tsk,argval,0,"Found argument to be irreducible");
          break;
        }
      }

      trace_step(tsk,redex,1,"Executing built-in function %s",builtin_info[bif].name);
      builtin_info[bif].f(tsk,&s->data[s->count-reqargs]);
      if (tsk->error)
        fatal("%s",tsk->error);
      s->count -= (reqargs-1);

      /* UPDATE */

      s->data[s->count-1] = resolve_pntr(s->data[s->count-1]);

      cell_make_ind(tsk,get_pntr(s->data[s->count-2]),s->data[s->count-1]);

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

static void stream(task *tsk, pntr lst)
{
  tsk->streamstack = pntrstack_new();
  pntrstack_push(tsk->streamstack,lst);
  while (0 < tsk->streamstack->count) {
    pntr p;
    reduce(tsk,tsk->streamstack);

    p = pntrstack_pop(tsk->streamstack);
    p = resolve_pntr(p);
    if (CELL_NIL == pntrtype(p)) {
      /* nothing */
    }
    else if (CELL_NUMBER == pntrtype(p)) {
      double d = pntrdouble(p);
      if ((d == floor(d)) && (0 < d) && (128 > d))
        printf("%c",(int)d);
      else
        printf("?");
    }
    else if (CELL_CONS == pntrtype(p)) {
      pntrstack_push(tsk->streamstack,get_pntr(p)->field2);
      pntrstack_push(tsk->streamstack,get_pntr(p)->field1);
    }
    else if (CELL_APPLICATION == pntrtype(p)) {
      fprintf(stderr,"Too many arguments applied to function\n");
      exit(1);
    }
    else if (CELL_AREF == pntrtype(p)) {
      carray *arr = aref_array(p);
      int index = aref_index(p);
      pntr tail = aref_tail(p);
      pntrstack_push(tsk->streamstack,tail);
      if (1 == arr->elemsize) {
        char *str = (char*)malloc(arr->size-index+1);
        memcpy(str,&((char*)arr->elements)[index],arr->size-index);
        str[arr->size-index] = '\0';
        printf("%s",str);
        free(str);
      }
      else if (sizeof(pntr) == arr->elemsize) {
        int i;
        for (i = arr->size-1; i >= index; i--)
          pntrstack_push(tsk->streamstack,((pntr*)arr->elements)[i]);
      }
      else {
        fatal("invalid array size");
      }
    }
    else {
      fprintf(stderr,"Bad cell type returned to printing mechanism: %s\n",cell_types[pntrtype(p)]);
      exit(1);
    }
  }
  pntrstack_free(tsk->streamstack);
  tsk->streamstack = NULL;
}

void run_reduction(source *src, char *trace_dir, int trace_type, array *args)
{
  scomb *mainsc;
  cell *app1;
  cell *app2;
  pntr rootp;
  socketid out_sockid;
  task *tsk;

  memset(&out_sockid,0,sizeof(out_sockid));
  tsk = task_new(0,0,NULL,0,args,NULL,out_sockid,NULL,0);

  debug_stage("Reduction engine");
  mainsc = get_scomb(src,"main");

  app1 = alloc_cell(tsk);
  app1->type = CELL_APPLICATION;
  app1->field1 = makescref(tsk,mainsc);
  app1->field2 = tsk->argsp;

  app2 = alloc_cell(tsk);
  app2->type = CELL_APPLICATION;
  make_pntr(app2->field1,app1);
  app2->field2 = tsk->globnilpntr;

  make_pntr(rootp,app2);
  tsk->argsp = tsk->globnilpntr;

  if (trace_dir) {
    tsk->tracing = 1;
    tsk->trace_src = src;
    tsk->trace_root = rootp;
    tsk->trace_dir = trace_dir;
    tsk->trace_type = trace_type;
  }

  stream(tsk,rootp);

  if (trace_dir)
    trace_step(tsk,rootp,0,"Done");

  printf("\n");

  task_free(tsk);
}
