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

#include "grammar.tab.h"
#include "nreduce.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <assert.h>
#include <ctype.h>
#include <stdarg.h>
#include <math.h>

extern list *all_letrecs;

typedef struct abstraction {
  char *name;
  int level;
  struct abstraction *next;
} abstraction;

static void abstract(const char *name, int level, abstraction **abslist)
{
  abstraction *a;
  /* skip duplicate variable abstraction */
  for (a = *abslist; a; a = a->next)
    if (!strcmp(a->name,name))
      return;

  a = (abstraction*)calloc(1,sizeof(abstraction));
  a->name = strdup(name);
  a->level = level;

  abstraction **ptr = abslist;
  while (*ptr && ((*ptr)->level < level))
    ptr = &((*ptr)->next);

  a->next = *ptr;
  *ptr = a;
}

static void lift_r(stack *boundvars, cell **k, abstraction **abslist, list *noabsvars, char *prefix)
{
  if ((*k)->tag & FLAG_PROCESSED)
    return;
  (*k)->tag |= FLAG_PROCESSED;

  switch (celltype(*k)) {
  case TYPE_APPLICATION: {
    lift_r(boundvars,(cell**)&((*k)->field1),abslist,noabsvars,prefix);
    lift_r(boundvars,(cell**)&((*k)->field2),abslist,noabsvars,prefix);
    break;
  }
  case TYPE_LAMBDA: {
    list *l;
    letrec *rec;
    char *name = NULL;
    for (l = all_letrecs; l && !name; l = l->next)
      for (rec = (letrec*)l->data; rec && !name; rec = rec->next)
        if ((rec->value == *k) && (NULL == get_scomb(rec->name)))
          name = rec->name;
    if (!name)
      name = prefix;

    scomb *sc = add_scomb(name);
    int oldcount = boundvars->count;
    cell **body = k;
    list *lambdavars = NULL;
    abstraction *newabs = NULL;
    abstraction *a;
    while (TYPE_LAMBDA == celltype(*body)) {
      stack_push(boundvars,strdup((char*)(*body)->field1));
      list_append(&lambdavars,strdup((char*)(*body)->field1));
      body = (cell**)&(*body)->field2;
    }
    sc->body = *body;

    lift_r(boundvars,body,&newabs,lambdavars,name);

    while (oldcount < boundvars->count)
      free(boundvars->data[--boundvars->count]);

    sc->nargs = 0;
    for (a = newabs; a; a = a->next)
      sc->nargs++;
    sc->nargs += list_count(lambdavars);
    sc->argnames = (char**)malloc(sc->nargs*sizeof(char*));
    int i = 0;
    for (a = newabs; a; a = a->next)
      sc->argnames[i++] = strdup(a->name);
    for (l = lambdavars; l; l = l->next)
      sc->argnames[i++] = strdup((char*)l->data);

    free((*k)->field1);
    (*k)->tag = TYPE_SCREF;
    (*k)->field1 = sc;
    (*k)->field2 = NULL;

    a = newabs;
    while (a) {
      abstraction *next = a->next;
      cell *fun = alloc_cell();
      copy_raw(fun,*k);
      (*k)->tag = TYPE_APPLICATION;
      (*k)->field1 = fun;
      (*k)->field2 = alloc_cell2(TYPE_SYMBOL,a->name,NULL);
      free(a);
      a = next;
    }

    list_free(lambdavars,free);

    lift_r(boundvars,k,abslist,noabsvars,prefix);
    break;
  }
  case TYPE_SYMBOL: {
    char *sym = (char*)(*k)->field1;
    if (abslist && !list_contains_string(noabsvars,sym)) {
      int i;
      int level = 0;
      for (i = boundvars->count-1; 0 <= i; i--) {
        if (!strcmp((char*)boundvars->data[i],sym)) {
          level = i+1;
          break;
        }
      }
      abstract(sym,level,abslist);
    }
    break;
  }
  case TYPE_SCREF:
  case TYPE_BUILTIN:
  case TYPE_NIL:
  case TYPE_INT:
  case TYPE_DOUBLE:
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

  cleargraph(sc->body,FLAG_PROCESSED);
  lift_r(boundvars,&sc->body,NULL,NULL,sc->name);
  stack_free(boundvars);
}

void find_vars_r(cell *c, int *pos, char **names)
{
  if (c->tag & FLAG_PROCESSED)
    return;
  c->tag |= FLAG_PROCESSED;

  switch (celltype(c)) {
  case TYPE_APPLICATION:
    find_vars_r((cell*)c->field1,pos,names);
    find_vars_r((cell*)c->field2,pos,names);
    break;
  case TYPE_SYMBOL: {
    char *sym = (char*)c->field1;
    int i;
    for (i = 0; i < *pos; i++)
      if (!strcmp(sym,names[i]))
        return;
    names[(*pos)++] = strdup(sym);
    break;
  }
  case TYPE_BUILTIN:
  case TYPE_SCREF:
  case TYPE_NIL:
  case TYPE_INT:
  case TYPE_DOUBLE:
  case TYPE_STRING:
    break;
  default:
    assert(0);
    break;
  }
}

void find_vars(cell *c, int *pos, char **names)
{
  cleargraph(c,FLAG_PROCESSED);
  find_vars_r(c,pos,names);
}

void applift_r(cell **k, scomb *sc)
{
  if ((*k)->tag & FLAG_PROCESSED)
    return;
  (*k)->tag |= FLAG_PROCESSED;

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
      scomb *newsc = add_scomb(sc->name);
      newsc->body = copy_graph(*k);
      newsc->nargs = 0;
      newsc->argnames = (char**)malloc(sc->nargs*sizeof(char*));
      find_vars(newsc->body,&newsc->nargs,newsc->argnames);
      assert(newsc->nargs <= sc->nargs);

      (*k) = alloc_cell2(TYPE_SCREF,newsc,NULL);
      int i;
      for (i = 0; i < newsc->nargs; i++) {
        cell *varref = alloc_cell2(TYPE_SYMBOL,strdup(newsc->argnames[i]),NULL);
        cell *app = alloc_cell2(TYPE_APPLICATION,*k,varref);
        *k = app;
      }
    }
    else {
      applift_r((cell**)&((*k)->field1),sc);
      applift_r((cell**)&((*k)->field2),sc);
    }
    break;
  }
  case TYPE_SYMBOL:
    break;
  case TYPE_BUILTIN:
  case TYPE_SCREF:
  case TYPE_NIL:
  case TYPE_INT:
  case TYPE_DOUBLE:
  case TYPE_STRING:
    break;
  }
}

void applift(scomb *sc)
{
  cleargraph(sc->body,FLAG_PROCESSED);
  applift_r(&sc->body,sc);
}
