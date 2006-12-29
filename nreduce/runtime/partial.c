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

static int isevaluated(pntr p)
{
  return ((CELL_APPLICATION != pntrtype(p)) && (CELL_SYMBOL != pntrtype(p)));
}

static int isnil(pntr p)
{
  return (CELL_NIL == pntrtype(p));
}

static int istrue(pntr p)
{
  return (isevaluated(p) && !isnil(p));
}

static int makeind(task *tsk, pntr p, pntr dest, const char *msg)
{
  cell *c;
  p = resolve_pntr(p);
  assert(CELL_APPLICATION == pntrtype(p));
  c = get_pntr(p);
  c->type = CELL_IND;
  c->field1 = dest;
  trace_step(tsk,p,0,"%s",msg);
  return 1;
}

#define MAXARGS 3

int get_funargs(pntr app, pntr *args, int *nargs)
{
  pntr fun = resolve_pntr(app);
  pntr rev[MAXARGS];
  int i;
  *nargs = 0;
  while ((CELL_APPLICATION == pntrtype(fun)) && (*nargs < MAXARGS)) {
    rev[*nargs] = resolve_pntr(get_pntr(fun)->field2);
    fun = resolve_pntr(get_pntr(fun)->field1);
    (*nargs)++;
  }

  for (i = 0; i < *nargs; i++)
    args[i] = rev[(*nargs)-1-i];

  if (CELL_BUILTIN == pntrtype(fun)) {
    int bif = (int)get_pntr(get_pntr(fun)->field1);
    if (*nargs == builtin_info[bif].nargs)
      return bif;
  }

  return -1;
}

pntr makeif(task *tsk, pntr cond, pntr truebranch, pntr falsebranch)
{
  cell *app1 = alloc_cell(tsk);
  cell *app2 = alloc_cell(tsk);
  cell *app3 = alloc_cell(tsk);
  cell *fun = alloc_cell(tsk);
  pntr ret;

  fun->type = CELL_BUILTIN;
  make_pntr(fun->field1,B_IF);

  app1->type = CELL_APPLICATION;
  make_pntr(app1->field1,fun);
  app1->field2 = cond;

  app2->type = CELL_APPLICATION;
  make_pntr(app2->field1,app1);
  app2->field2 = truebranch;

  app3->type = CELL_APPLICATION;
  make_pntr(app3->field1,app2);
  app3->field2 = falsebranch;

  make_pntr(ret,app3);
  return ret;
}

int matches_r(scomb *sc, pntr s, pntr p, pntr *args,
              pntrset *tmp1, pntrset *tmp2, pntrset *sel)
{
  cell *c;
  cell *pc;

  s = resolve_pntr(s);
  p = resolve_pntr(p);

  if (CELL_SYMBOL == pntrtype(s)) {
    char *name = (char*)get_pntr(get_pntr(s)->field1);
    int argno = get_scomb_var(sc,name);
    assert(0 <= argno);
    if (NULL == get_pntr(args[argno])) {
      args[argno] = p;
      return 1;
    }
    else {
      return pntrequal(args[argno],p);
    }
  }

  if (pntrtype(s) != pntrtype(p))
    return 0;

  if (CELL_NUMBER == pntrtype(s))
    return (s == p);

  c = get_pntr(s);
  pc = get_pntr(p);

  if (pntrset_contains(tmp1,s) != pntrset_contains(tmp2,p))
    return 0;

  if (pntrset_contains(tmp1,s))
    return pntrset_contains(sel,s);

  pntrset_add(tmp1,s);
  pntrset_add(tmp2,p);

  switch (c->type) {
  case CELL_APPLICATION:
  case CELL_CONS:
    if (matches_r(sc,c->field1,pc->field1,args,tmp1,tmp2,sel) &&
        matches_r(sc,c->field2,pc->field2,args,tmp1,tmp2,sel)) {
      pntrset_add(sel,s);
      return 1;
    }
    break;
  case CELL_BUILTIN:
    if ((int)get_pntr(c->field1) == (int)get_pntr(pc->field1)) {
      pntrset_add(sel,s);
      return 1;
    }
    break;
  case CELL_SCREF:
    if (get_pntr(c->field1) == get_pntr(pc->field1)) {
      pntrset_add(sel,s);
      return 1;
    }
    break;
  case CELL_AREF: {
    carray *sarr = aref_array(s);
    int sindex = aref_index(s);
    carray *parr = aref_array(p);
    int pindex = aref_index(p);

    if ((sarr->size != parr->size) || (sarr->elemsize != parr->elemsize) || (sindex != pindex))
      return 0;

    if ((1 == sarr->elemsize) &&
        !memcmp(&((char*)sarr->elements)[sindex],
                &((char*)parr->elements)[pindex],
                sarr->size-sindex)) {
      if (matches_r(sc,sarr->tail,parr->tail,args,tmp1,tmp2,sel)) {
        pntrset_add(sel,s);
        return 1;
      }
    }

    /* We don't bother with the case where elemsize == sizeof(pntr), since it's unlikely
     to occur often. If it does happen it just means we'll miss a small optimisation. */
    break;
  }
  case CELL_NIL:
    pntrset_add(sel,s);
    return 1;
  default:
    abort();
    break;
  }
  return 0;
}

static pntr instantiate_abstract(task *tsk, scomb *sc)
{
  int i;
  pntrstack *stk = pntrstack_new();
  pntr ins;
  for (i = sc->nargs-1; 0 <= i; i--) {
    pntr argp;
    cell *sym = alloc_cell(tsk);
    char *copy = strdup(sc->argnames[i]);
    sym->type = CELL_SYMBOL;
    make_pntr(sym->field1,copy);
    make_pntr(argp,sym);
    pntrstack_push(stk,argp);
  }
  ins = instantiate_scomb(tsk,stk,sc->body,sc);
  pntrstack_free(stk);
  return ins;
}

int matches(task *tsk, scomb *sc, pntr p, pntr *args)
{
  pntrset_clear(tsk->partial_tmp1);
  pntrset_clear(tsk->partial_tmp2);
  pntrset_clear(tsk->partial_sel);
  return matches_r(sc,tsk->partial_scp,p,args,
                   tsk->partial_tmp1,tsk->partial_tmp2,tsk->partial_sel);
}

int match_repl(task *tsk, scomb *sc, pntr p)
{
  int i;
  pntr *args = (pntr*)malloc(sc->nargs*sizeof(pntr));
  int res = 0;
  for (i = 0; i < sc->nargs; i++)
    make_pntr(args[i],NULL);
  if (matches(tsk,sc,p,args)) {
    int all = 1;
    int same = 0;
    for (i = 0; i < sc->nargs; i++) {
      if (NULL == get_pntr(args[i])) {
        all = 0;
      }
      else {
        args[i] = resolve_pntr(args[i]);
        if ((CELL_SYMBOL == pntrtype(args[i])) &&
            !strcmp((char*)get_pntr(get_pntr(args[i])->field1),sc->argnames[i])) {
          same++;
        }
      }
    }
    if (all && (same != sc->nargs)) {
      cell *funcell;
      pntr fun;

      funcell = alloc_cell(tsk);
      funcell->type = CELL_SCREF;
      make_pntr(funcell->field1,sc);
      make_pntr(fun,funcell);

      for (i = 0; i < sc->nargs; i++) {
        cell *app = alloc_cell(tsk);
        app->type = CELL_APPLICATION;
        app->field1 = fun;
        app->field2 = args[i];
        make_pntr(fun,app);
      }
      makeind(tsk,p,fun,"Applied scomb match\n");
      res = 1;
    }
  }
  free(args);

  return res;
}

static int apply_one(task *tsk, pntr p)
{
  int nargs;
  pntr args[MAXARGS];
  int bif;
  int ninnerargs;
  pntr innerargs[MAXARGS];
  int innerbif;

  p = resolve_pntr(p);

  if (CELL_NUMBER == pntrtype(p))
    return 0;

  if ((pntrtype(tsk->partial_scp) == pntrtype(p)) &&
      match_repl(tsk,tsk->partial_sc,p))
    return 1;

  if (0 > (bif = get_funargs(p,args,&nargs)))
    return 0;

  innerbif = get_funargs(args[0],innerargs,&ninnerargs);

  // Rule 1: (and x nil) -> nil
  if ((B_AND == bif) && isnil(args[1]))
    return makeind(tsk,p,tsk->globnilpntr,"Applied (and x nil) -> nil");

  // Rule 2: (and x true) -> x
  if ((B_AND == bif) && istrue(args[1]))
    return makeind(tsk,p,args[0],"Applied (and x true) -> x");

  // Rule 3: (and nil x) -> nil
  if ((B_AND == bif) && isnil(args[0]))
    return makeind(tsk,p,tsk->globnilpntr,"Applied (and nil x) -> nil");

  // Rule 4: (and true x) -> x
  if ((B_AND == bif) && istrue(args[0]))
    return makeind(tsk,p,args[1],"Applied (and true x) -> x");

  // Rule 5: (or x nil) -> x
  if ((B_OR == bif) && isnil(args[1]))
    return makeind(tsk,p,args[0],"Applied (or x nil) -> x");

  // Rule 6: (or x true) -> true
  if ((B_OR == bif) && istrue(args[1]))
    return makeind(tsk,p,tsk->globtruepntr,"Applied (or x true) -> true");

  // Rule 7: (or nil x) -> x
  if ((B_OR == bif) && isnil(args[0]))
    return makeind(tsk,p,args[1],"Applied (or nil x) -> x");

  // Rule 8: (or true x) -> true
  if ((B_OR == bif) && istrue(args[0]))
    return makeind(tsk,p,tsk->globtruepntr,"Applied (or true x) -> true");

  // Rule 9: (if nil a b) -> b
  if ((B_IF == bif) && isnil(args[0]))
    return makeind(tsk,p,args[2],"Applied (if nil a b) -> b");

  // Rule 10: (if true a b) -> a
  if ((B_IF == bif) && istrue(args[0]))
    return makeind(tsk,p,args[1],"Applied (if true a b) -> a"); /* FIXME: test this */

  // Rule 11: (head (CONS a b)) -> a
  if ((B_HEAD == bif) && (CELL_CONS == pntrtype(args[0])))
    return makeind(tsk,p,get_pntr(args[0])->field1,"Applied (head (CONS a b)) -> a");

  // Rule 12: (tail (CONS a b)) -> b
  if ((B_TAIL == bif) && (CELL_CONS == pntrtype(args[0])))
    return makeind(tsk,p,get_pntr(args[0])->field2,"Applied (tail (CONS a b)) -> b");

  // Rule 13: (head (cons a b)) -> a
  if ((B_HEAD == bif) && (B_CONS == innerbif) && (2 == ninnerargs))
    return makeind(tsk,p,innerargs[0],"Applied (head (cons a b)) -> a");

  // Rule 14: (tail (cons a b)) -> b
  if ((B_TAIL == bif) && (B_CONS == innerbif) && (2 == ninnerargs))
    return makeind(tsk,p,innerargs[1],"Applied (tail (cons a b)) -> b");

  // Rule 15: (if (restrue ...) a b) -> a
  if ((B_IF == bif) && builtin_info[innerbif].restrue)
    return makeind(tsk,p,args[1],"Applied (if (restrue ...) a b) -> a");

  // Rule 16: (if (if a b c) d e) -> (if a (if b d e) (if c d e))
  if ((B_IF == bif) && (B_IF == innerbif)) {
    pntr x = args[0];
    pntr a = innerargs[0];
    pntr b = innerargs[1];
    pntr c = innerargs[2];
    pntr d = args[1];
    pntr e = args[2];
    pntr trueif = makeif(tsk,b,d,e);
    pntr falseif = makeif(tsk,c,d,e);
    pntr trueifrepl = graph_replace(tsk,trueif,x,b);
    pntr falseifrepl = graph_replace(tsk,falseif,x,c);
    pntr res = makeif(tsk,a,trueifrepl,falseifrepl);
    return makeind(tsk,p,res,"Applied (if (if a b c) d e) -> (if a (if b d e) (if c d e))");
  }

  return 0;
}

static int apply_rules_r(task *tsk, pntr p, pntrset *done)
{
  cell *c;
  int applied = 0;

  p = resolve_pntr(p);

  if (CELL_NUMBER == pntrtype(p))
    return 0;

  c = get_pntr(p);


  if (pntrset_contains(done,p))
    return 0;

  pntrset_add(done,p);

  // Apply rules to this node
  if (apply_one(tsk,p))
    applied = 1;


  // Apply to arguments
  p = resolve_pntr(p);
  while ((CELL_APPLICATION == pntrtype(p)) || (CELL_CONS == pntrtype(p))) {
    if (apply_rules_r(tsk,get_pntr(p)->field2,done))
      applied = 1;
    p = resolve_pntr(get_pntr(p)->field1);
  }
  if (apply_rules_r(tsk,p,done))
    applied = 1;

  return applied;
}

int apply_rules(task *tsk, pntr p)
{
  pntrset_clear(tsk->partial_applied);
  return apply_rules_r(tsk,p,tsk->partial_applied);
}

void reduce_args(source *src, task *tsk, pntr p)
{
  p = resolve_pntr(p);
  while (CELL_APPLICATION == pntrtype(p)) {
    pntr arg = resolve_pntr(get_pntr(p)->field2);
    pntrstack_push(tsk->streamstack,arg);
    reduce(tsk,tsk->streamstack);
    p = resolve_pntr(get_pntr(p)->field1);
  }
}

void reset_scused(source *src)
{
  int i;
  int count = array_count(src->scombs);
  for (i = 0; i < count; i++)
    array_item(src->scombs,i,scomb*)->used = 0;
}

snode *run_partial(source *src, scomb *sc, char *trace_dir, int trace_type)
{
  cell *root;
  pntr rootp;
  task *tsk = task_new(0,0,NULL,0,0);
  pntr p;
  int i;
  snode *s;

  tsk->partial = 1;
  tsk->partial_sc = sc;
  tsk->partial_scp = instantiate_abstract(tsk,sc);

  tsk->partial_tmp1 = pntrset_new();
  tsk->partial_tmp2 = pntrset_new();
  tsk->partial_sel = pntrset_new();
  tsk->partial_applied = pntrset_new();

  debug_stage("Partial evaluation");

  root = alloc_cell(tsk);
  root->type = CELL_SCREF;
  make_pntr(root->field1,sc);
  make_pntr(rootp,root);

  for (i = 0; i < sc->nargs; i++) {
    cell *arg = alloc_cell(tsk);
    cell *app = alloc_cell(tsk);
    char *s = strdup(sc->argnames[i]);

    arg->type = CELL_SYMBOL;
    make_pntr(arg->field1,s);

    app->type = CELL_APPLICATION;
    app->field1 = rootp;
    make_pntr(app->field2,arg);
    make_pntr(rootp,app);
  }

  tsk->streamstack = pntrstack_new();
  reset_scused(src);

  if (trace_dir) {
    tsk->tracing = 1;
    tsk->trace_src = src;
    tsk->trace_root = rootp;
    tsk->trace_dir = trace_dir;
    tsk->trace_type = trace_type;
  }

  pntrstack_push(tsk->streamstack,rootp);
  reduce(tsk,tsk->streamstack);
  p = resolve_pntr(pntrstack_top(tsk->streamstack));
  /* Note: we leave the values on the stack so they will be garbage collected */

  if (trace_dir)
    tsk->trace_root = p;

  if (CELL_CONS == pntrtype(p)) {
    cell *c = get_pntr(p);
    pntr head = c->field1;
    pntr tail = c->field2;

    pntrstack_push(tsk->streamstack,head);
    reduce(tsk,tsk->streamstack);
    head = resolve_pntr(pntrstack_top(tsk->streamstack));

    reduce_args(src,tsk,tail);
    tail = resolve_pntr(tail);
  }
  while (apply_rules(tsk,p)) {
    /* apply again */
  }

  pntrstack_free(tsk->streamstack);

  if (trace_dir)
    trace_step(tsk,p,0,"Done");

  s = graph_to_syntax(src,p);

  pntrset_free(tsk->partial_tmp1);
  pntrset_free(tsk->partial_tmp2);
  pntrset_free(tsk->partial_sel);
  pntrset_free(tsk->partial_applied);

  task_free(tsk);

  return s;
}

void rename_oldnames(source *src)
{
  int count = array_count(src->oldnames);
  int i;
  int varno = 0;
  for (i = count-1; 0 <= i; i--) {
    char *name = (char*)malloc(strlen(GENVAR_PREFIX)+20);
    sprintf(name,"%s%d",GENVAR_PREFIX,varno++);
    free(((char**)src->oldnames->data)[i]);
    ((char**)src->oldnames->data)[i] = name;
  }
}

void debug_partial(source *src, const char *name, char *trace_dir, int trace_type)
{
  scomb *sc;
  snode *s;

  if (NULL == (sc = get_scomb(src,name))) {
    fprintf(stderr,"No such supercombinator: %s\n",name);
    exit(1);
  }

  s = run_partial(src,sc,trace_dir,trace_type);
  snode_free(sc->body);
  sc->body = s;
  rename_oldnames(src);
  print_scomb_code(src,stdout,sc);
  printf("\n");
}
