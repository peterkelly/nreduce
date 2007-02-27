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
#include "source.h"
#include "runtime/runtime.h"
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

static binding *binding_new(const char *name, letrec *rec)
{
  binding *bnd = (binding*)calloc(1,sizeof(binding));
  bnd->name = strdup(name);
  bnd->rec = rec;
  return bnd;
}

static void bindings_setcount(stack *boundvars, int count)
{
  while (count < boundvars->count) {
    binding *bnd = (binding*)boundvars->data[--boundvars->count];
    free(bnd->name);
    free(bnd);
  }
}

static int binding_level(stack *boundvars, const char *sym)
{
  int i;
  for (i = boundvars->count-1; 0 <= i; i--) {
    binding *bnd = (binding*)boundvars->data[i];
    if (!strcmp(bnd->name,sym)) {
      if (bnd->rec && (SNODE_LAMBDA == bnd->rec->value->type))
        return -1;
      else
        return i+1;
    }
  }

  return 0; /* top-level def */
}

static void abstract(stack *boundvars, const char *sym, int level, snode **lambda, int *changed)
{
  int appdepth = 0;
  snode **reall = lambda;
  assert(lambda);
  while (SNODE_APPLICATION == (*reall)->type) {
    reall = &(*reall)->left;
    appdepth++;
  }

  while ((SNODE_LAMBDA == (*reall)->type) &&
         (level > binding_level(boundvars,(*reall)->name))) {
    reall = &(*reall)->body;
    appdepth--;
  }
  if (strcmp(sym,(*reall)->name)) {
    snode *body = *reall;
    *reall = snode_new(-1,-1);
    (*reall)->type = SNODE_LAMBDA;
    (*reall)->name = strdup(sym);
    (*reall)->body = body;
    (*reall)->sl = body->sl;
    *changed = 1;
  }
}

static void create_scombs(source *src, stack *boundvars, snode **k,
                          letrec *inletrec, const char *prefix)
{
  switch ((*k)->type) {
  case SNODE_APPLICATION:
    create_scombs(src,boundvars,&((*k)->left),NULL,prefix);
    create_scombs(src,boundvars,&((*k)->right),NULL,prefix);
    break;
  case SNODE_LETREC: {
    int oldcount = boundvars->count;
    letrec *rec;
    letrec **ptr;

    /* Add a binding entry for each definition, and in the process create an empty supercombinator
       for those which correspond to lambda abstractions. These should now have no free variables,
       and the recursive call to create_scombs() will fill in the supercombinator with the
       appropriate arguments and body. */
    for (rec = (*k)->bindings; rec; rec = rec->next) {
      if (SNODE_LAMBDA == rec->value->type) {
        rec->sc = add_scomb(src,rec->name);
        rec->sc->sl = rec->value->sl;
      }
      stack_push(boundvars,binding_new(rec->name,rec));
    }

    for (rec = (*k)->bindings; rec; rec = rec->next)
      create_scombs(src,boundvars,&rec->value,rec,prefix);
    create_scombs(src,boundvars,&((*k)->body),NULL,prefix);

    /* Remove any definitions that have been converted into supercombinators (all references
       to them should have been replaced by this point */
    ptr = &(*k)->bindings;
    while (*ptr) {
      if ((*ptr)->sc) {
        letrec *old = *ptr;
        *ptr = (*ptr)->next;
        assert(SNODE_SCREF == old->value->type);
        snode_free(old->value);
        free(old->name);
        free(old);
      }
      else {
        ptr = &(*ptr)->next;
      }
    }

    /* If we have no definitions left, simply replace the letrec with its body */
    if (NULL == (*k)->bindings) {
      snode *old = *k;
      *k = (*k)->body;
      old->body = NULL;
      snode_free(old);
    }

    bindings_setcount(boundvars,oldcount);
    break;
  }
  case SNODE_LAMBDA: {
    int oldcount = boundvars->count;
    snode **tmp;
    int argno;
    scomb *newsc;

    if (inletrec) {
      newsc = inletrec->sc;
      assert(newsc);
    }
    else {
      newsc = add_scomb(src,prefix);
      newsc->sl = (*k)->sl;
    }

    for (tmp = k; SNODE_LAMBDA == (*tmp)->type; tmp = &(*tmp)->body) {
      stack_push(boundvars,binding_new((*tmp)->name,NULL));
      newsc->nargs++;
    }
    newsc->body = *tmp;
    newsc->argnames = (char**)malloc(newsc->nargs*sizeof(char*));
    argno = 0;
    for (tmp = k; SNODE_LAMBDA == (*tmp)->type; tmp = &(*tmp)->body)
      newsc->argnames[argno++] = strdup((*tmp)->name);

    create_scombs(src,boundvars,tmp,NULL,newsc->name);

    *tmp = NULL;
    snode_free(*k);

    (*k) = snode_new(-1,-1);
    (*k)->type = SNODE_SCREF;
    (*k)->sc = newsc;
    (*k)->sl = newsc->body->sl;

    bindings_setcount(boundvars,oldcount);
    break;
  }
  case SNODE_SYMBOL: {
    char *sym = (*k)->name;
    sourceloc sl = (*k)->sl;
    int i;
    for (i = boundvars->count-1; 0 <= i; i--) {
      binding *bnd = (binding*)boundvars->data[i];
      if (!strcmp(bnd->name,sym)) {
        if (bnd->rec && bnd->rec->sc) {
          snode_free(*k);
          (*k) = snode_new(-1,-1);
          (*k)->type = SNODE_SCREF;
          (*k)->sc = bnd->rec->sc;
          (*k)->sl = sl;
        }
        break;
      }
    }
    break;
  }
  case SNODE_SCREF:
  case SNODE_BUILTIN:
  case SNODE_NIL:
  case SNODE_NUMBER:
  case SNODE_STRING:
    break;
  default:
    abort();
    break;
  }
}

static void make_app(snode **k, list *args)
{
  list *l;
  for (l = args; l; l = l->next) {
    char *argname = (char*)l->data;
    snode *varref = snode_new(-1,-1);
    snode *left = *k;
    varref->type = SNODE_SYMBOL;
    varref->name = strdup(argname);
    varref->sl = left->sl;
    (*k) = snode_new(-1,-1);
    (*k)->type = SNODE_APPLICATION;
    (*k)->left = left;
    (*k)->right = varref;
    (*k)->sl = left->sl;
  }
}

/* Iterate through a syntax tree, replacing any instance of the symbol fun with an application
of that function to the list of arguments. The args list should be a list of strings; symbols
will be created for each of these. */
static void replace_usage(snode **k, const char *fun, list *args)
{
  switch ((*k)->type) {
  case SNODE_APPLICATION:
    replace_usage(&((*k)->left),fun,args);
    replace_usage(&((*k)->right),fun,args);
    break;
  case SNODE_LETREC: {
    letrec *rec;
    for (rec = (*k)->bindings; rec; rec = rec->next)
      replace_usage(&rec->value,fun,args);
    replace_usage(&((*k)->body),fun,args);
    break;
  }
  case SNODE_LAMBDA:
    replace_usage(&((*k)->body),fun,args);
    break;
  case SNODE_SYMBOL: {
    char *sym = (*k)->name;
    if (!strcmp(sym,fun))
      make_app(k,args);
    break;
  }
  case SNODE_SCREF:
  case SNODE_BUILTIN:
  case SNODE_NIL:
  case SNODE_NUMBER:
  case SNODE_STRING:
    break;
  default:
    abort();
    break;
  }
}

static void capture(stack *boundvars, snode **k, list *noabsvars, snode **lambda,
                    int *changed, letrec *inletrec)
{
  switch ((*k)->type) {
  case SNODE_APPLICATION:
    capture(boundvars,&((*k)->left),noabsvars,lambda,changed,NULL);
    capture(boundvars,&((*k)->right),noabsvars,lambda,changed,NULL);
    break;
  case SNODE_LETREC: {
    int oldcount = boundvars->count;
    list *newnoabs = NULL;
    letrec *rec;
    list *l;
    for (rec = (*k)->bindings; rec; rec = rec->next) {
      stack_push(boundvars,binding_new(rec->name,rec));
      list_push(&newnoabs,strdup(rec->name));
    }
    for (l = noabsvars; l; l = l->next)
      list_push(&newnoabs,strdup((char*)l->data));

    for (rec = (*k)->bindings; rec; rec = rec->next) {
      int islambda = (SNODE_LAMBDA == rec->value->type);
      snode *oldval = rec->value;

      capture(boundvars,&rec->value,newnoabs,lambda,changed,rec);

      if (islambda && (oldval != rec->value)) {
        list *vars = NULL;
        snode *tmp;
        for (tmp = rec->value; tmp != oldval; tmp = tmp->body) {
          assert(SNODE_LAMBDA == tmp->type);
          list_append(&vars,tmp->name);
        }
        replace_usage(k,rec->name,vars);
        list_free(vars,NULL);
      }
    }
    capture(boundvars,&((*k)->body),newnoabs,lambda,changed,NULL);

    bindings_setcount(boundvars,oldcount);
    list_free(newnoabs,free);
    break;
  }
  case SNODE_LAMBDA: {
    int oldcount = boundvars->count;
    list *lambdavars = NULL;
    snode **tmp;
    snode *oldlambda;
    for (tmp = k; SNODE_LAMBDA == (*tmp)->type; tmp = &(*tmp)->body) {
      stack_push(boundvars,binding_new((*tmp)->name,NULL));
      list_append(&lambdavars,strdup((*tmp)->name));
    }

    oldlambda = *k;
    capture(boundvars,tmp,lambdavars,k,changed,NULL);
    if (!inletrec && (oldlambda != *k)) {
      list *args = NULL;
      for (tmp = k; *tmp != oldlambda; tmp = &(*tmp)->body)
        list_append(&args,(*tmp)->name);
      make_app(k,args);
      list_free(args,NULL);
    }

    bindings_setcount(boundvars,oldcount);

    for (tmp = k; SNODE_APPLICATION == (*tmp)->type; tmp = &(*tmp)->left)
      capture(boundvars,&(*tmp)->right,noabsvars,lambda,changed,NULL);
    list_free(lambdavars,free);
    break;
  }
  case SNODE_SYMBOL: {
    char *sym = (*k)->name;
    if (lambda && !list_contains_string(noabsvars,sym)) {
      int level = binding_level(boundvars,sym);
      if (0 <= level)
        abstract(boundvars,sym,level,lambda,changed);
    }
    break;
  }
  case SNODE_SCREF:
  case SNODE_BUILTIN:
  case SNODE_NIL:
  case SNODE_NUMBER:
  case SNODE_STRING:
    break;
  default:
    abort();
    break;
  }
}

void lift(source *src, scomb *sc)
{
  stack *boundvars = stack_new();
  int changed;

  /* Phase 1: Variable capture */

  do {
    int i;
    changed = 0;
    for (i = 0; i < sc->nargs; i++)
      stack_push(boundvars,binding_new(sc->argnames[i],NULL));
    capture(boundvars,&sc->body,NULL,NULL,&changed,NULL);
    bindings_setcount(boundvars,0);

    if (!changed)
      break;
  } while (changed);

  /* Phase 2: Supercombinator creation */

  create_scombs(src,boundvars,&sc->body,NULL,sc->name);
  stack_free(boundvars);
}

static void find_vars(snode *c, list **names, stack *ignore)
{
  switch (c->type) {
  case SNODE_APPLICATION:
    find_vars(c->left,names,ignore);
    find_vars(c->right,names,ignore);
    break;
  case SNODE_SYMBOL: {
    char *sym = c->name;
    int i;

    if (list_contains_string(*names,sym))
        return;

    for (i = 0; i < ignore->count; i++)
      if (!strcmp(sym,(char*)ignore->data[i]))
        return;

    list_push(names,strdup(sym));
    break;
  }
  case SNODE_BUILTIN:
  case SNODE_SCREF:
  case SNODE_NIL:
  case SNODE_NUMBER:
  case SNODE_STRING:
    break;
  case SNODE_LETREC: {
    int oldcount = ignore->count;
    letrec *rec;
    for (rec = c->bindings; rec; rec = rec->next)
      stack_push(ignore,rec->name);
    for (rec = c->bindings; rec; rec = rec->next)
      find_vars(rec->value,names,ignore);
    find_vars(c->body,names,ignore);
    ignore->count = oldcount;
    break;
  }
  default:
    abort();
    break;
  }
}

static scomb *lift_expr(source *src, snode **k, scomb *sc)
{
  scomb *newsc;
  stack *ignore;
  int i;
  list *vars = NULL;
  list *l;
  newsc = add_scomb(src,sc->name);

  newsc->body = *k;
  newsc->sl = (*k)->sl;

  ignore = stack_new();
  find_vars(newsc->body,&vars,ignore);
  stack_free(ignore);

  newsc->nargs = list_count(vars);
  newsc->argnames = (char**)malloc(newsc->nargs*sizeof(char*));
  i = newsc->nargs-1;
  for (l = vars; l; l = l->next)
    newsc->argnames[i--] = (char*)l->data;
  list_free(vars,NULL);

  (*k) = snode_new(-1,-1);
  (*k)->type = SNODE_SCREF;
  (*k)->sc = newsc;
  (*k)->sl = newsc->body->sl;
  for (i = 0; i < newsc->nargs; i++) {
    snode *varref;
    snode *app;
    varref = snode_new(-1,-1);
    varref->type = SNODE_SYMBOL;
    varref->name = strdup(newsc->argnames[i]);
    varref->sl = (*k)->sl;
    app = snode_new(-1,-1);
    app->type = SNODE_APPLICATION;
    app->left = *k;
    app->right = varref;
    app->sl = (*k)->sl;
    *k = app;
  }
  return newsc;
}

static void applift_r(source *src, snode **k, scomb *sc)
{
  switch ((*k)->type) {
  case SNODE_APPLICATION: {
    snode *app = *k;
    snode **ptr = &app;
    int nargs = 0;
    while (SNODE_APPLICATION == (*ptr)->type) {
      ptr = &((*ptr)->left);
      nargs++;
    }
    if (SNODE_LETREC == (*ptr)->type) {
      snode *letrec = *ptr;
      *k = letrec;
      *ptr = letrec->body;
      letrec->body = app;
      applift_r(src,k,sc);
    }
    else if ((SNODE_SYMBOL == (*ptr)->type) ||
             ((SNODE_SCREF == (*ptr)->type) && (nargs > (*ptr)->sc->nargs)) ||
             ((SNODE_BUILTIN == (*ptr)->type) && (nargs > builtin_info[(*ptr)->bif].nargs))) {

      scomb *newsc;

      if (*k == sc->body) {
        /* just do the arguments */
        snode *app;
        for (app = *k; SNODE_APPLICATION == app->type; app = app->left)
          applift_r(src,&app->right,sc);
        break;
      }

      newsc = lift_expr(src,k,sc);

      applift(src,newsc);
    }
    else {
      applift_r(src,&((*k)->left),sc);
      applift_r(src,&((*k)->right),sc);
    }
    break;
  }
  case SNODE_SYMBOL:
    break;
  case SNODE_LETREC: {
    letrec *rec;
    for (rec = (*k)->bindings; rec; rec = rec->next)
      applift_r(src,&rec->value,sc);
    applift_r(src,&((*k)->body),sc);
    break;
  }
  case SNODE_BUILTIN:
  case SNODE_SCREF:
  case SNODE_NIL:
  case SNODE_NUMBER:
  case SNODE_STRING:
    break;
  }
}

void applift(source *src, scomb *sc)
{
  applift_r(src,&sc->body,sc);
}

static void nonstrict_lift_r(source *src, snode **k, scomb *sc);

/**
 * We have encountered an expression which is not in a strict context. Determine if it is a
 * complex expression that should be transformed into its own supercombinator, so only one MKFRAME
 * for the whole expression is necessary.
 */
static void lift_if_complex(source *src, snode **k, scomb *sc)
{
  snode *expr = *k;
  int nargs = 0;
  int nappargs = 0;
  while (SNODE_APPLICATION == expr->type) {
    nargs++;
    if (SNODE_APPLICATION == expr->right->type)
      nappargs++;
    expr = expr->left;
  }

  if (0 < nappargs) {
    if ((SNODE_BUILTIN == expr->type) && (B_CONS == expr->bif)) {
      /* A call to cons... this will be treated specially by the bytecode compiler and execute
         directly. Inspect the arguments though */
      nonstrict_lift_r(src,k,sc);
    }
    else {
      scomb *newsc = lift_expr(src,k,sc);
      int changed = 0;
      newsc->strictin = (int*)calloc(newsc->nargs,sizeof(int));
      check_strictness(newsc,&changed);
      newsc->nospark = 1;
      nonstrict_lift_r(src,&newsc->body,newsc);
    }
  }
}

/**
 * Find expressions in the source tree which will result in suspended evaluations at runtime
 * (via MKFRAME). If they are a complex expression which will require a MKFRAME instruction - i.e.
 * any expression other than an application of a function to variables or constants, then move
 * this expression into a separate supercombinator and replace it with a reference to that
 * supercombinator.
 *
 * This minimises the amount of overhead introduced by lazy evaluation.
 */
static void nonstrict_lift_r(source *src, snode **k, scomb *sc)
{
  switch ((*k)->type) {
  case SNODE_APPLICATION: {
    int strict = (*k)->strict;
    snode *expr = *k;
    while (SNODE_APPLICATION == expr->type)
      expr = expr->left;

    /* Skip this optimisation for if and seq; the compiler handles these specially */
    if ((SNODE_BUILTIN == expr->type) && (B_IF == expr->bif))
      strict = 1;
    if ((SNODE_BUILTIN == expr->type) && (B_SEQ == expr->bif))
      strict = 1;

    if (!strict)
      lift_if_complex(src,&(*k)->right,sc);
    else
      nonstrict_lift_r(src,&(*k)->right,sc);

    nonstrict_lift_r(src,&(*k)->left,sc);
    break;
  }
  case SNODE_SYMBOL:
    break;
  case SNODE_LETREC: {
    letrec *rec;
    for (rec = (*k)->bindings; rec; rec = rec->next)
      nonstrict_lift_r(src,&rec->value,sc);
    nonstrict_lift_r(src,&((*k)->body),sc);
    break;
  }
  case SNODE_BUILTIN:
  case SNODE_SCREF:
  case SNODE_NIL:
  case SNODE_NUMBER:
  case SNODE_STRING:
    break;
  }
}

void nonstrict_lift(source *src, scomb *sc)
{
  nonstrict_lift_r(src,&sc->body,sc);
}
