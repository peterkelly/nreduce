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
 * $Id: super.c 276 2006-07-20 03:39:42Z pmkelly $
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

typedef struct binding {
  char *name;
  letrec *rec;
} binding;

binding *binding_new(const char *name, letrec *rec)
{
  binding *bnd = (binding*)calloc(1,sizeof(binding));
  bnd->name = strdup(name);
  bnd->rec = rec;
  return bnd;
}

void bindings_setcount(stack *boundvars, int count)
{
  while (count < boundvars->count) {
    binding *bnd = (binding*)boundvars->data[--boundvars->count];
    free(bnd->name);
    free(bnd);
  }
}

int binding_level(stack *boundvars, const char *sym)
{
  int i;
  for (i = boundvars->count-1; 0 <= i; i--) {
    binding *bnd = (binding*)boundvars->data[i];
    if (!strcmp(bnd->name,sym)) {
      if (bnd->rec && (TYPE_LAMBDA == celltype(bnd->rec->value)))
        return -1;
      else
        return i+1;
    }
  }

  return 0; /* top-level def */
}

void abstract(stack *boundvars, const char *sym, int level, cell **lambda, int *changed)
{
  int appdepth = 0;
  cell **reall = lambda;
  assert(lambda);
  while (TYPE_APPLICATION == celltype(*reall)) {
    reall = (cell**)&(*reall)->field1;
    appdepth++;
  }

  while ((TYPE_LAMBDA == celltype(*reall)) &&
         (level > binding_level(boundvars,(char*)(*reall)->field1))) {
    reall = (cell**)&(*reall)->field2;
    appdepth--;
  }
  if (strcmp(sym,(char*)(*reall)->field1)) {
    *reall = alloc_cell2(TYPE_LAMBDA,strdup(sym),*reall);
    *changed = 1;
  }
}

static void create_scombs(stack *boundvars, cell **k, letrec *inletrec, const char *prefix)
{
  switch (celltype(*k)) {
  case TYPE_APPLICATION:
    create_scombs(boundvars,(cell**)&((*k)->field1),NULL,prefix);
    create_scombs(boundvars,(cell**)&((*k)->field2),NULL,prefix);
    break;
  case TYPE_LETREC: {
    int oldcount = boundvars->count;
    letrec *rec;
    letrec **ptr;

    /* Add a binding entry for each definition, and in the process create an empty supercombinator
       for those which correspond to lambda abstractions. These should now have no free variables,
       and the recursive call to create_scombs() will fill in the supercombinator with the
       appropriate arguments and body. */
    for (rec = (letrec*)(*k)->field1; rec; rec = rec->next) {
      if (TYPE_LAMBDA == celltype(rec->value))
        rec->sc = add_scomb(rec->name);
      stack_push(boundvars,binding_new(rec->name,rec));
    }

    for (rec = (letrec*)(*k)->field1; rec; rec = rec->next)
      create_scombs(boundvars,&rec->value,rec,prefix);
    create_scombs(boundvars,(cell**)&((*k)->field2),NULL,prefix);

    /* Remove any definitions that have been converted into supercombinators (all references
       to them should have been replaced by this point */
    ptr = (letrec**)&(*k)->field1;
    while (*ptr) {
      if ((*ptr)->sc) {
        letrec *old = *ptr;
        *ptr = (*ptr)->next;
        free(old->name);
        free(old);
      }
      else {
        ptr = &(*ptr)->next;
      }
    }

    /* If we have no definitions left, simply replace the letrec with its body */
    if (NULL == (*k)->field1)
      *k = (cell*)(*k)->field2;

    bindings_setcount(boundvars,oldcount);
    break;
  }
  case TYPE_LAMBDA: {
    int oldcount = boundvars->count;
    scomb *newsc = inletrec ? inletrec->sc : add_scomb(prefix);
    cell **tmp;
    int argno;

    for (tmp = k; TYPE_LAMBDA == celltype(*tmp); tmp = (cell**)&(*tmp)->field2) {
      stack_push(boundvars,binding_new((char*)(*tmp)->field1,NULL));
      newsc->nargs++;
    }
    newsc->body = *tmp;
    newsc->argnames = (char**)malloc(newsc->nargs*sizeof(char*));
    argno = 0;
    for (tmp = k; TYPE_LAMBDA == celltype(*tmp); tmp = (cell**)&(*tmp)->field2)
      newsc->argnames[argno++] = strdup((char*)(*tmp)->field1);

    create_scombs(boundvars,tmp,NULL,newsc->name);
    *k = alloc_cell2(TYPE_SCREF,newsc,NULL);

    bindings_setcount(boundvars,oldcount);
    break;
  }
  case TYPE_SYMBOL: {
    char *sym = (char*)(*k)->field1;
    int i;
    for (i = boundvars->count-1; 0 <= i; i--) {
      binding *bnd = (binding*)boundvars->data[i];
      if (!strcmp(bnd->name,sym)) {
        if (bnd->rec && bnd->rec->sc)
          *k = alloc_cell2(TYPE_SCREF,bnd->rec->sc,NULL);
        break;
      }
    }
    break;
  }
  case TYPE_SCREF:
  case TYPE_BUILTIN:
  case TYPE_NIL:
  case TYPE_NUMBER:
  case TYPE_STRING:
    break;
  default:
    assert(0);
    break;
  }
}

static void make_app(cell **k, list *args)
{
  list *l;
  for (l = args; l; l = l->next) {
    char *argname = (char*)l->data;
    cell *varref = alloc_cell2(TYPE_SYMBOL,strdup(argname),NULL);
    *k = alloc_cell2(TYPE_APPLICATION,*k,varref);
  }
}

static void replace_usage(cell **k, const char *fun, cell *extra, list *args)
{
  switch (celltype(*k)) {
  case TYPE_APPLICATION:
    replace_usage((cell**)&((*k)->field1),fun,extra,args);
    replace_usage((cell**)&((*k)->field2),fun,extra,args);
    break;
  case TYPE_LETREC: {
    letrec *rec;
    for (rec = (letrec*)(*k)->field1; rec; rec = rec->next)
      replace_usage(&rec->value,fun,extra,args);
    replace_usage((cell**)&((*k)->field2),fun,extra,args);
    break;
  }
  case TYPE_LAMBDA:
    replace_usage((cell**)&((*k)->field2),fun,extra,args);
    break;
  case TYPE_SYMBOL: {
    char *sym = (char*)(*k)->field1;
    if (!strcmp(sym,fun))
      make_app(k,args);
    break;
  }
  case TYPE_SCREF:
  case TYPE_BUILTIN:
  case TYPE_NIL:
  case TYPE_NUMBER:
  case TYPE_STRING:
    break;
  default:
    assert(0);
    break;
  }
}

static void lift_r(stack *boundvars, cell **k, list *noabsvars, cell **lambda,
                   int *changed, letrec *inletrec)
{
  switch (celltype(*k)) {
  case TYPE_APPLICATION:
    lift_r(boundvars,(cell**)&((*k)->field1),noabsvars,lambda,changed,NULL);
    lift_r(boundvars,(cell**)&((*k)->field2),noabsvars,lambda,changed,NULL);
    break;
  case TYPE_LETREC: {
    int oldcount = boundvars->count;
    list *newnoabs = NULL;
    letrec *rec;
    list *l;
    for (rec = (letrec*)(*k)->field1; rec; rec = rec->next) {
      stack_push(boundvars,binding_new(rec->name,rec));
      list_push(&newnoabs,strdup(rec->name));
    }
    for (l = noabsvars; l; l = l->next)
      list_push(&newnoabs,strdup((char*)l->data));

    for (rec = (letrec*)(*k)->field1; rec; rec = rec->next) {
      int islambda = (TYPE_LAMBDA == celltype(rec->value));
      cell *oldval = rec->value;

      lift_r(boundvars,&rec->value,newnoabs,lambda,changed,rec);

      if (islambda && (oldval != rec->value)) {
        list *vars = NULL;
        cell *tmp;
        for (tmp = rec->value; tmp != oldval; tmp = (cell*)tmp->field2) {
          assert(TYPE_LAMBDA == celltype(tmp));
          list_append(&vars,tmp->field1);
        }
        replace_usage(k,rec->name,rec->value,vars);
        list_free(vars,NULL);
      }
    }
    lift_r(boundvars,(cell**)&((*k)->field2),newnoabs,lambda,changed,NULL);

    bindings_setcount(boundvars,oldcount);
    list_free(newnoabs,free);
    break;
  }
  case TYPE_LAMBDA: {
    int oldcount = boundvars->count;
    list *lambdavars = NULL;
    cell **tmp;
    cell *oldlambda;
    for (tmp = k; TYPE_LAMBDA == celltype(*tmp); tmp = (cell**)&(*tmp)->field2) {
      stack_push(boundvars,binding_new((char*)(*tmp)->field1,NULL));
      list_append(&lambdavars,strdup((char*)(*tmp)->field1));
    }

    oldlambda = *k;
    lift_r(boundvars,tmp,lambdavars,k,changed,NULL);
    if (!inletrec && (oldlambda != *k)) {
      list *args = NULL;
      for (tmp = k; *tmp != oldlambda; tmp = (cell**)&(*tmp)->field2)
        list_append(&args,(char*)(*tmp)->field1);
      make_app(k,args);
      list_free(args,NULL);
    }

    bindings_setcount(boundvars,oldcount);

    for (tmp = k; TYPE_APPLICATION == celltype(*tmp); tmp = (cell**)&(*tmp)->field1)
      lift_r(boundvars,(cell**)&(*tmp)->field2,noabsvars,lambda,changed,NULL);
    list_free(lambdavars,free);
    break;
  }
  case TYPE_SYMBOL: {
    char *sym = (char*)(*k)->field1;
    if (lambda && !list_contains_string(noabsvars,sym)) {
      int level = binding_level(boundvars,sym);
      if (0 <= level)
        abstract(boundvars,sym,level,lambda,changed);
    }
    break;
  }
  case TYPE_SCREF:
  case TYPE_BUILTIN:
  case TYPE_NIL:
  case TYPE_NUMBER:
  case TYPE_STRING:
    break;
  default:
    assert(0);
    break;
  }
}

void lift(scomb *sc)
{
  stack *boundvars = stack_new();
  int changed;
  do {
    int i;
    changed = 0;
    for (i = 0; i < sc->nargs; i++)
      stack_push(boundvars,binding_new(sc->argnames[i],NULL));
    lift_r(boundvars,&sc->body,NULL,NULL,&changed,NULL);
    bindings_setcount(boundvars,0);

    if (!changed)
      break;
  } while (changed);

  create_scombs(boundvars,&sc->body,NULL,sc->name);
  stack_free(boundvars);
}

void find_vars(cell *c, int *pos, char **names, letrec *ignore)
{
  switch (celltype(c)) {
  case TYPE_APPLICATION:
    find_vars((cell*)c->field1,pos,names,ignore);
    find_vars((cell*)c->field2,pos,names,ignore);
    break;
  case TYPE_SYMBOL: {
    char *sym = (char*)c->field1;
    int i;
    letrec *rec;

    for (i = 0; i < *pos; i++)
      if (!strcmp(sym,names[i]))
        return;

    for (rec = ignore; rec; rec = rec->next)
      if (!strcmp(sym,rec->name))
        return;

    names[(*pos)++] = strdup(sym);
    break;
  }
  case TYPE_BUILTIN:
  case TYPE_SCREF:
  case TYPE_NIL:
  case TYPE_NUMBER:
  case TYPE_STRING:
    break;
  default:
    assert(0);
    break;
  }
}

void applift_r(cell **k, scomb *sc)
{
  switch (celltype(*k)) {
  case TYPE_APPLICATION: {
    cell *fun = *k;
    int nargs = 0;
    while (TYPE_APPLICATION == celltype(fun)) {
      fun = (cell*)fun->field1;
      nargs++;
    }
    if ((TYPE_SYMBOL == celltype(fun)) ||
         ((TYPE_SCREF == celltype(fun)) && (nargs > ((scomb*)fun->field1)->nargs)) ||
        ((TYPE_BUILTIN == celltype(fun)) && (nargs > builtin_info[(int)fun->field1].nargs))) {
      int maxvars;
      scomb *newsc;
      int i;

      if (*k == sc->body) {
        /* just do the arguments */
        cell *app;
        for (app = *k; TYPE_APPLICATION == celltype(app); app = (cell*)app->field1)
          applift_r((cell**)&app->field2,sc);
        break;
      }


      maxvars = sc->nargs;
      if (TYPE_LETREC == celltype(sc->body)) {
        letrec *rec;
        for (rec = (letrec*)sc->body->field1; rec; rec = rec->next)
          maxvars++;
      }

      newsc = add_scomb(sc->name);

      newsc->body = *k;
      newsc->nargs = 0;
      newsc->argnames = (char**)malloc(maxvars*sizeof(char*));
      find_vars(newsc->body,&newsc->nargs,newsc->argnames,NULL);
      assert(newsc->nargs <= maxvars);

      (*k) = alloc_cell2(TYPE_SCREF,newsc,NULL);
      for (i = 0; i < newsc->nargs; i++) {
        cell *varref = alloc_cell2(TYPE_SYMBOL,strdup(newsc->argnames[i]),NULL);
        cell *app = alloc_cell2(TYPE_APPLICATION,*k,varref);
        *k = app;
      }

      applift(newsc);
    }
    else {
      applift_r((cell**)&((*k)->field1),sc);
      applift_r((cell**)&((*k)->field2),sc);
    }
    break;
  }
  case TYPE_SYMBOL:
    break;
  case TYPE_LETREC: {
    letrec *rec;
    for (rec = (letrec*)(*k)->field1; rec; rec = rec->next)
      applift_r(&rec->value,sc);
    applift_r((cell**)&((*k)->field2),sc);
    break;
  }
  case TYPE_BUILTIN:
  case TYPE_SCREF:
  case TYPE_NIL:
  case TYPE_NUMBER:
  case TYPE_STRING:
    break;
  }
}

void applift(scomb *sc)
{
  applift_r(&sc->body,sc);
}
