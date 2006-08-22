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
    reall = &(*reall)->left;
    appdepth++;
  }

  while ((TYPE_LAMBDA == celltype(*reall)) &&
         (level > binding_level(boundvars,(*reall)->name))) {
    reall = &(*reall)->body;
    appdepth--;
  }
  if (strcmp(sym,(*reall)->name)) {
    cell *body = *reall;
    *reall = alloc_cell();
    (*reall)->tag = TYPE_LAMBDA;
    (*reall)->name = strdup(sym);
    (*reall)->body = body;
    (*reall)->sl = body->sl;
    *changed = 1;
  }
}

static void create_scombs(stack *boundvars, cell **k, letrec *inletrec, const char *prefix)
{
  switch (celltype(*k)) {
  case TYPE_APPLICATION:
    create_scombs(boundvars,&((*k)->left),NULL,prefix);
    create_scombs(boundvars,&((*k)->right),NULL,prefix);
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
      if (TYPE_LAMBDA == celltype(rec->value)) {
        rec->sc = add_scomb(rec->name);
        rec->sc->sl = rec->value->sl;
      }
      stack_push(boundvars,binding_new(rec->name,rec));
    }

    for (rec = (*k)->bindings; rec; rec = rec->next)
      create_scombs(boundvars,&rec->value,rec,prefix);
    create_scombs(boundvars,&((*k)->body),NULL,prefix);

    /* Remove any definitions that have been converted into supercombinators (all references
       to them should have been replaced by this point */
    ptr = &(*k)->bindings;
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
    if (NULL == (*k)->bindings)
      *k = (cell*)(*k)->body;

    bindings_setcount(boundvars,oldcount);
    break;
  }
  case TYPE_LAMBDA: {
    int oldcount = boundvars->count;
    cell **tmp;
    int argno;
    scomb *newsc;

    if (inletrec) {
      newsc = inletrec->sc;
      assert(newsc);
    }
    else {
      newsc = add_scomb(prefix);
      newsc->sl = (*k)->sl;
    }

    for (tmp = k; TYPE_LAMBDA == celltype(*tmp); tmp = &(*tmp)->body) {
      stack_push(boundvars,binding_new((*tmp)->name,NULL));
      newsc->nargs++;
    }
    newsc->body = *tmp;
    newsc->argnames = (char**)malloc(newsc->nargs*sizeof(char*));
    argno = 0;
    for (tmp = k; TYPE_LAMBDA == celltype(*tmp); tmp = &(*tmp)->body)
      newsc->argnames[argno++] = strdup((*tmp)->name);

    create_scombs(boundvars,tmp,NULL,newsc->name);
    (*k) = alloc_cell();
    (*k)->tag = TYPE_SCREF;
    (*k)->sc = newsc;
    (*k)->sl = newsc->body->sl;

    bindings_setcount(boundvars,oldcount);
    break;
  }
  case TYPE_SYMBOL: {
    cell *ref = *k;
    char *sym = ref->name;
    int i;
    for (i = boundvars->count-1; 0 <= i; i--) {
      binding *bnd = (binding*)boundvars->data[i];
      if (!strcmp(bnd->name,sym)) {
        if (bnd->rec && bnd->rec->sc) {
          (*k) = alloc_cell();
          (*k)->tag = TYPE_SCREF;
          (*k)->sc = bnd->rec->sc;
          (*k)->sl = ref->sl;
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
    assert(0);
    break;
  }
}

static void make_app(cell **k, list *args)
{
  list *l;
  for (l = args; l; l = l->next) {
    char *argname = (char*)l->data;
    cell *varref = alloc_cell();
    cell *left = *k;
    varref->tag = TYPE_SYMBOL;
    varref->name = strdup(argname);
    varref->sl = left->sl;
    (*k) = alloc_cell();
    (*k)->tag = TYPE_APPLICATION;
    (*k)->left = left;
    (*k)->right = varref;
    (*k)->sl = left->sl;
  }
}

static void replace_usage(cell **k, const char *fun, cell *extra, list *args)
{
  switch (celltype(*k)) {
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
    assert(0);
    break;
  }
}

static void lift_r(stack *boundvars, cell **k, list *noabsvars, cell **lambda,
                   int *changed, letrec *inletrec)
{
  switch (celltype(*k)) {
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
      int islambda = (TYPE_LAMBDA == celltype(rec->value));
      cell *oldval = rec->value;

      lift_r(boundvars,&rec->value,newnoabs,lambda,changed,rec);

      if (islambda && (oldval != rec->value)) {
        list *vars = NULL;
        cell *tmp;
        for (tmp = rec->value; tmp != oldval; tmp = tmp->body) {
          assert(TYPE_LAMBDA == celltype(tmp));
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
    cell **tmp;
    cell *oldlambda;
    for (tmp = k; TYPE_LAMBDA == celltype(*tmp); tmp = &(*tmp)->body) {
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

    for (tmp = k; TYPE_APPLICATION == celltype(*tmp); tmp = &(*tmp)->left)
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
    find_vars(c->left,pos,names,ignore);
    find_vars(c->right,pos,names,ignore);
    break;
  case TYPE_SYMBOL: {
    char *sym = c->name;
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
      fun = fun->left;
      nargs++;
    }
    if ((TYPE_SYMBOL == celltype(fun)) ||
        ((TYPE_SCREF == celltype(fun)) && (nargs > fun->sc->nargs)) ||
        ((TYPE_BUILTIN == celltype(fun)) && (nargs > builtin_info[fun->bif].nargs))) {
      int maxvars;
      scomb *newsc;
      int i;

      if (*k == sc->body) {
        /* just do the arguments */
        cell *app;
        for (app = *k; TYPE_APPLICATION == celltype(app); app = app->left)
          applift_r(&app->right,sc);
        break;
      }


      maxvars = sc->nargs;
      if (TYPE_LETREC == celltype(sc->body)) {
        letrec *rec;
        for (rec = sc->body->bindings; rec; rec = rec->next)
          maxvars++;
      }

      newsc = add_scomb(sc->name);

      newsc->body = *k;
      newsc->sl = (*k)->sl;
      newsc->nargs = 0;
      newsc->argnames = (char**)malloc(maxvars*sizeof(char*));
      find_vars(newsc->body,&newsc->nargs,newsc->argnames,NULL);
      assert(newsc->nargs <= maxvars);

      (*k) = alloc_cell();
      (*k)->tag = TYPE_SCREF;
      (*k)->sc = newsc;
      (*k)->sl = newsc->body->sl;
      for (i = 0; i < newsc->nargs; i++) {
        cell *varref;
        cell *app;
        varref = alloc_cell();
        varref->tag = TYPE_SYMBOL;
        varref->name = strdup(newsc->argnames[i]);
        varref->sl = (*k)->sl;
        app = alloc_cell();
        app->tag = TYPE_APPLICATION;
        app->left = *k;
        app->right = varref;
        app->sl = (*k)->sl;
        *k = app;
      }

      applift(newsc);
    }
    else {
      applift_r(&((*k)->left),sc);
      applift_r(&((*k)->right),sc);
    }
    break;
  }
  case TYPE_SYMBOL:
    break;
  case TYPE_LETREC: {
    letrec *rec;
    for (rec = (*k)->bindings; rec; rec = rec->next)
      applift_r(&rec->value,sc);
    applift_r(&((*k)->body),sc);
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
