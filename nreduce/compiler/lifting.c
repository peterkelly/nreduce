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
      if (bnd->rec && (TYPE_LAMBDA == snodetype(bnd->rec->value)))
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
  while (TYPE_APPLICATION == snodetype(*reall)) {
    reall = &(*reall)->left;
    appdepth++;
  }

  while ((TYPE_LAMBDA == snodetype(*reall)) &&
         (level > binding_level(boundvars,(*reall)->name))) {
    reall = &(*reall)->body;
    appdepth--;
  }
  if (strcmp(sym,(*reall)->name)) {
    snode *body = *reall;
    *reall = snode_new(-1,-1);
    (*reall)->tag = TYPE_LAMBDA;
    (*reall)->name = strdup(sym);
    (*reall)->body = body;
    (*reall)->sl = body->sl;
    *changed = 1;
  }
}

static void create_scombs(source *src, stack *boundvars, snode **k,
                          letrec *inletrec, const char *prefix)
{
  switch (snodetype(*k)) {
  case TYPE_APPLICATION:
    create_scombs(src,boundvars,&((*k)->left),NULL,prefix);
    create_scombs(src,boundvars,&((*k)->right),NULL,prefix);
    break;
  case TYPE_LETREC: {
    int oldcount = boundvars->count;
    letrec *rec;
    letrec **ptr;

    /* Add a binding entry for each definition, and in the process create an empty supercombinator
       for those which correspond to lambda abstractions. These should now have no free variables,
       and the recursive call to create_scombs() will fill in the supercombinator with the
       appropriate arguments and body. */
    for (rec = (*k)->bindings; rec; rec = rec->next) {
      if (TYPE_LAMBDA == snodetype(rec->value)) {
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
        assert(TYPE_SCREF == snodetype(old->value));
        snode_free(old->value);
        free(old->name);
        free(old);
      }
      else {
        ptr = &(*ptr)->next;
      }
    }

    /* If we have no definitions left, simply replace the letrec with its body */
    if (NULL == (*k)->bindings)
      *k = (*k)->body;

    bindings_setcount(boundvars,oldcount);
    break;
  }
  case TYPE_LAMBDA: {
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

    for (tmp = k; TYPE_LAMBDA == snodetype(*tmp); tmp = &(*tmp)->body) {
      stack_push(boundvars,binding_new((*tmp)->name,NULL));
      newsc->nargs++;
    }
    newsc->body = *tmp;
    newsc->argnames = (char**)malloc(newsc->nargs*sizeof(char*));
    argno = 0;
    for (tmp = k; TYPE_LAMBDA == snodetype(*tmp); tmp = &(*tmp)->body)
      newsc->argnames[argno++] = strdup((*tmp)->name);

    create_scombs(src,boundvars,tmp,NULL,newsc->name);

    *tmp = NULL;
    snode_free(*k);

    (*k) = snode_new(-1,-1);
    (*k)->tag = TYPE_SCREF;
    (*k)->sc = newsc;
    (*k)->sl = newsc->body->sl;

    bindings_setcount(boundvars,oldcount);
    break;
  }
  case TYPE_SYMBOL: {
    char *sym = (*k)->name;
    sourceloc sl = (*k)->sl;
    int i;
    for (i = boundvars->count-1; 0 <= i; i--) {
      binding *bnd = (binding*)boundvars->data[i];
      if (!strcmp(bnd->name,sym)) {
        if (bnd->rec && bnd->rec->sc) {
          snode_free(*k);
          (*k) = snode_new(-1,-1);
          (*k)->tag = TYPE_SCREF;
          (*k)->sc = bnd->rec->sc;
          (*k)->sl = sl;
        }
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
    varref->tag = TYPE_SYMBOL;
    varref->name = strdup(argname);
    varref->sl = left->sl;
    (*k) = snode_new(-1,-1);
    (*k)->tag = TYPE_APPLICATION;
    (*k)->left = left;
    (*k)->right = varref;
    (*k)->sl = left->sl;
  }
}

static void replace_usage(snode **k, const char *fun, snode *extra, list *args)
{
  switch (snodetype(*k)) {
  case TYPE_APPLICATION:
    replace_usage(&((*k)->left),fun,extra,args);
    replace_usage(&((*k)->right),fun,extra,args);
    break;
  case TYPE_LETREC: {
    letrec *rec;
    for (rec = (*k)->bindings; rec; rec = rec->next)
      replace_usage(&rec->value,fun,extra,args);
    replace_usage(&((*k)->body),fun,extra,args);
    break;
  }
  case TYPE_LAMBDA:
    replace_usage(&((*k)->body),fun,extra,args);
    break;
  case TYPE_SYMBOL: {
    char *sym = (*k)->name;
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
    abort();
    break;
  }
}

static void lift_r(stack *boundvars, snode **k, list *noabsvars, snode **lambda,
                   int *changed, letrec *inletrec)
{
  switch (snodetype(*k)) {
  case TYPE_APPLICATION:
    lift_r(boundvars,&((*k)->left),noabsvars,lambda,changed,NULL);
    lift_r(boundvars,&((*k)->right),noabsvars,lambda,changed,NULL);
    break;
  case TYPE_LETREC: {
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
      int islambda = (TYPE_LAMBDA == snodetype(rec->value));
      snode *oldval = rec->value;

      lift_r(boundvars,&rec->value,newnoabs,lambda,changed,rec);

      if (islambda && (oldval != rec->value)) {
        list *vars = NULL;
        snode *tmp;
        for (tmp = rec->value; tmp != oldval; tmp = tmp->body) {
          assert(TYPE_LAMBDA == snodetype(tmp));
          list_append(&vars,tmp->name);
        }
        replace_usage(k,rec->name,rec->value,vars);
        list_free(vars,NULL);
      }
    }
    lift_r(boundvars,&((*k)->body),newnoabs,lambda,changed,NULL);

    bindings_setcount(boundvars,oldcount);
    list_free(newnoabs,free);
    break;
  }
  case TYPE_LAMBDA: {
    int oldcount = boundvars->count;
    list *lambdavars = NULL;
    snode **tmp;
    snode *oldlambda;
    for (tmp = k; TYPE_LAMBDA == snodetype(*tmp); tmp = &(*tmp)->body) {
      stack_push(boundvars,binding_new((*tmp)->name,NULL));
      list_append(&lambdavars,strdup((*tmp)->name));
    }

    oldlambda = *k;
    lift_r(boundvars,tmp,lambdavars,k,changed,NULL);
    if (!inletrec && (oldlambda != *k)) {
      list *args = NULL;
      for (tmp = k; *tmp != oldlambda; tmp = &(*tmp)->body)
        list_append(&args,(*tmp)->name);
      make_app(k,args);
      list_free(args,NULL);
    }

    bindings_setcount(boundvars,oldcount);

    for (tmp = k; TYPE_APPLICATION == snodetype(*tmp); tmp = &(*tmp)->left)
      lift_r(boundvars,&(*tmp)->right,noabsvars,lambda,changed,NULL);
    list_free(lambdavars,free);
    break;
  }
  case TYPE_SYMBOL: {
    char *sym = (*k)->name;
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
    abort();
    break;
  }
}

void lift(source *src, scomb *sc)
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

  create_scombs(src,boundvars,&sc->body,NULL,sc->name);
  stack_free(boundvars);
}

static void find_vars(snode *c, list **names, stack *ignore)
{
  switch (snodetype(c)) {
  case TYPE_APPLICATION:
    find_vars(c->left,names,ignore);
    find_vars(c->right,names,ignore);
    break;
  case TYPE_SYMBOL: {
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
  case TYPE_BUILTIN:
  case TYPE_SCREF:
  case TYPE_NIL:
  case TYPE_NUMBER:
  case TYPE_STRING:
    break;
  case TYPE_LETREC: {
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

static void applift_r(source *src, snode **k, scomb *sc)
{
  switch (snodetype(*k)) {
  case TYPE_APPLICATION: {
    snode *fun = *k;
    int nargs = 0;
    while (TYPE_APPLICATION == snodetype(fun)) {
      fun = fun->left;
      nargs++;
    }
    if ((TYPE_SYMBOL == snodetype(fun)) ||
        ((TYPE_SCREF == snodetype(fun)) && (nargs > fun->sc->nargs)) ||
        ((TYPE_BUILTIN == snodetype(fun)) && (nargs > builtin_info[fun->bif].nargs))) {
      scomb *newsc;
      int i;
      list *vars = NULL;
      list *l;
      stack *ignore;

      if (*k == sc->body) {
        /* just do the arguments */
        snode *app;
        for (app = *k; TYPE_APPLICATION == snodetype(app); app = app->left)
          applift_r(src,&app->right,sc);
        break;
      }


      newsc = add_scomb(src,sc->name);

      newsc->body = *k;
      newsc->sl = (*k)->sl;

      ignore = stack_new();
      find_vars(newsc->body,&vars,ignore);
      stack_free(ignore);


      newsc->nargs = list_count(vars);
      newsc->argnames = (char**)malloc(newsc->nargs*sizeof(char*));
      i = 0;
      for (l = vars; l; l = l->next)
        newsc->argnames[i++] = (char*)l->data;
      list_free(vars,NULL);

      (*k) = snode_new(-1,-1);
      (*k)->tag = TYPE_SCREF;
      (*k)->sc = newsc;
      (*k)->sl = newsc->body->sl;
      for (i = 0; i < newsc->nargs; i++) {
        snode *varref;
        snode *app;
        varref = snode_new(-1,-1);
        varref->tag = TYPE_SYMBOL;
        varref->name = strdup(newsc->argnames[i]);
        varref->sl = (*k)->sl;
        app = snode_new(-1,-1);
        app->tag = TYPE_APPLICATION;
        app->left = *k;
        app->right = varref;
        app->sl = (*k)->sl;
        *k = app;
      }

      applift(src,newsc);
    }
    else {
      applift_r(src,&((*k)->left),sc);
      applift_r(src,&((*k)->right),sc);
    }
    break;
  }
  case TYPE_SYMBOL:
    break;
  case TYPE_LETREC: {
    letrec *rec;
    for (rec = (*k)->bindings; rec; rec = rec->next)
      applift_r(src,&rec->value,sc);
    applift_r(src,&((*k)->body),sc);
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

void applift(source *src, scomb *sc)
{
  applift_r(src,&sc->body,sc);
}
