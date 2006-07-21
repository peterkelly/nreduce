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

extern int genvar;

typedef struct abstraction {
  struct abstraction *next;
  cell *body;
  char *name;
  cell *replacement;
  int level;
} abstraction;

static void abstraction_free(abstraction *abs)
{
  free(abs->name);
  free(abs);
}

static void add_abstraction(list **abslist, cell **k, int level)
{
  abstraction *a = (abstraction*)calloc(1,sizeof(abstraction));
  list **ptr = abslist;
  while (*ptr && (((abstraction*)((*ptr)->data))->level < level))
    ptr = &((*ptr)->next);
  list_push(ptr,a);

  if (TYPE_SYMBOL == celltype((*k))) {
    a->name = strdup((char*)(*k)->field1);
  }
  else if (TYPE_SCREF == celltype((*k))) {
    a->name = strdup(((scomb*)(*k)->field1)->name);
  }
  else {
    a->name = (char*)malloc(20);
    sprintf(a->name,"V%d",genvar++);
  }

  a->body = (*k);
  a->replacement = alloc_cell2(TYPE_SYMBOL,strdup(a->name),NULL);
  a->replacement->level = level;
  a->level = level;

  (*k) = a->replacement;
  cleargraph(a->body,FLAG_PROCESSED);
}

static int have_abstraction_name(list *abslist, const char *name)
{
  for (; abslist; abslist = abslist->next)
    if (!strcmp(((abstraction*)abslist->data)->name,name))
      return 1;
  return 0;
}

static void abstract(cell **k, list **abslist)
{
  if (!abslist || isvaluefun((*k)))
    return;

  /* skip duplicate variable abstraction */
  if ((TYPE_SYMBOL == celltype((*k))) && have_abstraction_name(*abslist,(char*)(*k)->field1))
    return;

  int nargs = 0;
  cell *c;
  for (c = *k; TYPE_APPLICATION == celltype(c); c = (cell*)c->field1)
    nargs++;

  /* Don't abstract partial applications of builtin functions */
  /* FIXME: can we do this for supercombinators also? They may not have their nargs
     computed yet... */
  if ((TYPE_BUILTIN == celltype(c)) && (nargs < builtin_info[(int)c->field1].nargs)) {
    for (c = *k; TYPE_APPLICATION == celltype(c); c = (cell*)c->field1)
      abstract((cell**)&c->field2,abslist);
    return;
  }

  assert(0 <= (*k)->level);
  add_abstraction(abslist,k,(*k)->level);
}

static int lift_r(stack *boundvars, cell **k, char *lambdavar, list **abslist,
                  int *level, char *prefix)
{
  int maxfree = 1;
  *level = 0;

  /* If we've already abstracted this cell, just substitute it for the variable name */
  if (NULL != abslist) {
    list *l;
    for (l = *abslist; l; l = l->next) {
      abstraction *a = (abstraction*)l->data;
      if (a->body == *k) {
        *k = a->replacement;
        return ((*k)->tag & FLAG_MAXFREE);
      }
    }
  }

  if ((*k)->tag & FLAG_PROCESSED)
    return ((*k)->tag & FLAG_MAXFREE);
  (*k)->tag |= FLAG_PROCESSED;

  switch (celltype(*k)) {
  case TYPE_APPLICATION: {
    int level1 = 0;
    int level2 = 0;
    int mf1 = lift_r(boundvars,(cell**)&((*k)->field1),lambdavar,abslist,&level1,prefix);
    int mf2 = lift_r(boundvars,(cell**)&((*k)->field2),lambdavar,abslist,&level2,prefix);
    if (level1 > level2)
      *level = level1;
    else
      *level = level2;

    maxfree = (mf1 && mf2);

    if (!maxfree) {
      if (mf1)
        abstract((cell**)&((*k)->field1),abslist);
      if (mf2)
        abstract((cell**)&((*k)->field2),abslist);
    }
    break;
  }
  case TYPE_LAMBDA: {
    scomb *sc = add_scomb(NULL,prefix);
    char *name = strdup((char*)(*k)->field1);
    list *newabs = NULL;

    (*k)->tag &= ~FLAG_MAXFREE;

    stack_push(boundvars,*k);
    if (lift_r(boundvars,(cell**)&((*k)->field2),name,&newabs,level,prefix))
      abstract((cell**)&(*k)->field2,&newabs);
    stack_pop(boundvars);
    *level = boundvars->count+1;

    sc->nargs = 1 + list_count(newabs);
    sc->argnames = (char**)malloc(sc->nargs*sizeof(char*));

    int argno = 0;
    list *l;
    for (l = newabs; l; l = l->next) {
      abstraction *a = (abstraction*)l->data;
      sc->argnames[argno++] = strdup(a->name);
    }
    sc->argnames[argno++] = strdup(name);

    sc->body = (cell*)(*k)->field2;

    free((*k)->field1);
    (*k)->tag = TYPE_SCREF;
    (*k)->field1 = sc;
    (*k)->field2 = NULL;

    for (l = newabs; l; l = l->next) {
      abstraction *a = (abstraction*)l->data;
      cell *fun = alloc_cell();
      copy_raw(fun,*k);
      (*k)->tag = TYPE_APPLICATION;
      (*k)->field1 = fun;
      (*k)->field2 = a->body;
    }

    maxfree = lift_r(boundvars,k,lambdavar,abslist,level,prefix);

    free(name);
    list_free(newabs,(void*)abstraction_free);
    break;
  }
  case TYPE_SYMBOL: {
    char *name = (char*)(*k)->field1;
    assert(NULL == get_scomb((char*)(*k)->field1));
    maxfree = ((NULL == lambdavar) || strcmp(name,lambdavar));
    int i;
    for (i = boundvars->count-1; 0 <= i; i--) {
      assert(TYPE_LAMBDA == celltype((cell*)boundvars->data[i]));
      if (!strcmp((char*)((cell*)boundvars->data[i])->field1,name)) {
        *level = i+1;
        break;
      }
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

  if (maxfree)
    (*k)->tag |= FLAG_MAXFREE;
  else
    (*k)->tag &= ~FLAG_MAXFREE;

  (*k)->level = *level;

  return maxfree;
}

void lift(scomb *sc)
{
  int level = 0;
  stack *boundvars = stack_new();

  cleargraph(sc->body,FLAG_PROCESSED);
  lift_r(boundvars,&sc->body,NULL,NULL,&level,sc->name);
  stack_free(boundvars);
}
