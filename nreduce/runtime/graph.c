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
#include "runtime.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <assert.h>
#include <ctype.h>
#include <stdarg.h>
#include <math.h>

typedef int (*graphfun)(cell *c, void *arg);

static int set_needclear(cell *c, void *arg)
{
  if (c->flags & FLAG_NEEDCLEAR)
    return 1;
  c->flags |= FLAG_NEEDCLEAR;
  return 0;
}

static int clear_flag(cell *c, void *arg)
{
  int flag = (int)arg;
  if (!(c->flags & FLAG_NEEDCLEAR))
    return 1;
  c->flags &= ~FLAG_NEEDCLEAR;
  c->flags &= ~flag;
  return 0;
}

static int set_flag(cell *c, void *arg)
{
  int flag = (int)arg;
  if (c->flags & flag)
    return 1;
  c->flags |= flag;
  return 0;
}

static void traverse(pntr p, graphfun fun, void *arg)
{
  cell *c;

  if (CELL_NUMBER == pntrtype(p))
    return;

  c = get_pntr(p);
  if (fun(c,arg))
    return;

  switch (pntrtype(p)) {
  case CELL_APPLICATION:
  case CELL_CONS:
    traverse(c->field1,fun,arg);
    traverse(c->field2,fun,arg);
    break;
  case CELL_AREF: {
    carray *arr = aref_array(p);
    int i;
    if (sizeof(pntr) == arr->elemsize)
      for (i = 0; i < arr->size; i++)
        traverse(((pntr*)arr->elements)[i],fun,arg);
    traverse(arr->tail,fun,arg);
    break;
  }
  case CELL_IND:
    traverse(c->field1,fun,arg);
    break;
  case CELL_FRAME:
  case CELL_CAP:
    abort(); /* don't need to handle this case */
    break;
  default:
    break;
  }
}

void clear_graph(pntr root, int flag)
{
  traverse(root,set_needclear,0);
  traverse(root,clear_flag,(void*)flag);
}

static int graph_setcontains_r(pntr p, pntr needle)
{
  cell *c;
  int contains = 0;

  if (CELL_NUMBER == pntrtype(p))
    return 0;

  c = get_pntr(p);

  if (c->flags & FLAG_TMP)
    return (c->flags & FLAG_SELECTED);
  c->flags |= FLAG_TMP;

  switch (pntrtype(p)) {
  case CELL_APPLICATION:
  case CELL_CONS: {
    int c1 = graph_setcontains_r(c->field1,needle);
    int c2 = graph_setcontains_r(c->field2,needle);
    contains = (c1 || c2);
    break;
  }
  case CELL_AREF: {
    carray *arr = aref_array(p);
    int i;
    if (sizeof(pntr) == arr->elemsize) {
      for (i = 0; i < arr->size; i++) {
        if (graph_setcontains_r(((pntr*)arr->elements)[i],needle))
          contains = 1;
      }
      if (graph_setcontains_r(arr->tail,needle))
        contains = 1;
    }
    break;
  }
  case CELL_IND:
    contains = graph_setcontains_r(c->field1,needle);
    break;
  case CELL_FRAME:
  case CELL_CAP:
    abort(); /* don't need to handle this case */
    break;
  default:
    break;
  }

  if (contains)
    c->flags |= FLAG_SELECTED;
  return contains;
}

/* Traverses the graph haystack, marking all cells that contain the cell needle with the
   flag FLAG_SELECTED. Uses FLAG_TMP. */
static void graph_setcontains(pntr haystack, pntr needle)
{
  clear_graph(haystack,FLAG_SELECTED);
  clear_graph(haystack,FLAG_TMP);

  if ((CELL_NUMBER == pntrtype(haystack)) || (CELL_NUMBER == pntrtype(needle)))
    return;

  if (CELL_NUMBER != pntrtype(needle)) {
    cell *c = get_pntr(needle);
    c->flags |= FLAG_SELECTED;
    c->flags |= FLAG_TMP;
  }

  if (graph_setcontains_r(haystack,needle))
    get_pntr(haystack)->flags |= FLAG_SELECTED;
}

/* FIXME: remove */
int graph_contains(pntr haystack, pntr needle)
{
  cell *c;
  if (CELL_NUMBER == pntrtype(needle))
    return 0;
  c = get_pntr(needle);
  c->flags &= ~FLAG_TMP;
  clear_graph(haystack,FLAG_TMP);
  traverse(haystack,set_flag,(void*)FLAG_TMP);
  return (c->flags & FLAG_TMP);
}

typedef struct pmentry {
  pntr key;
  pntr value;
  struct pmentry *next;
} pmentry;

typedef struct pntrmap {
  pmentry *entries[GLOBAL_HASH_SIZE];
} pntrmap;

pntrmap *pntrmap_new()
{
  pntrmap *pm = (pntrmap*)calloc(1,sizeof(pntrmap));
  return pm;
}

void pntrmap_free(pntrmap *pm)
{
  int h;
  for (h = 0; h < GLOBAL_HASH_SIZE; h++) {
    pmentry *e = pm->entries[h];
    while (e) {
      pmentry *next = e->next;
      free(e);
      e = next;
    }
  }
  free(pm);
}

int pntrmap_lookup(pntrmap *pm, pntr key, pntr *value)
{
  int h = hash(&key,sizeof(pntr));
  pmentry *e;
  for (e = pm->entries[h]; e; e = e->next) {
    if (pntrequal(key,e->key)) {
      *value = e->value;
      return 1;
    }
  }
  return 0;
}

void pntrmap_add(pntrmap *pm, pntr key, pntr value)
{
  int h = hash(&key,sizeof(pntr));
  pmentry *e = (pmentry*)calloc(1,sizeof(pmentry));
  e->key = key;
  e->value = value;
  e->next = pm->entries[h];
  pm->entries[h] = e;
}

static pntr graph_replace_r(task *tsk, pntrmap *pm, pntr p, pntr old, pntr new)
{
  cell *c;
  pntr repl = p;

  if (CELL_NUMBER == pntrtype(p))
    return p;

  if (!(get_pntr(p)->flags & FLAG_SELECTED))
    return p;

  if (pntrequal(p,old))
    return new;

  if (pntrequal(p,new))
    return new;

  c = get_pntr(p);
  if (c->flags & FLAG_PROCESSED) {
    int have = pntrmap_lookup(pm,p,&repl);
    assert(have);
    return repl;
  }
  c->flags |= FLAG_PROCESSED;

  switch (pntrtype(p)) {
  case CELL_APPLICATION:
  case CELL_CONS: {
    cell *r = alloc_cell(tsk);
    r->type = pntrtype(p);
    r->field1 = graph_replace_r(tsk,pm,c->field1,old,new);
    r->field2 = graph_replace_r(tsk,pm,c->field2,old,new);
    make_pntr(repl,r);
    break;
  }
  case CELL_AREF: {
    assert(!"FIXME: implement");
    break;
  }
  case CELL_IND: {
    cell *r = alloc_cell(tsk);
    r->type = pntrtype(p);
    r->field1 = graph_replace_r(tsk,pm,c->field1,old,new);
    make_pntr(repl,r);
    break;
  }
  case CELL_FRAME:
  case CELL_CAP:
    abort(); /* don't need to handle this case */
    break;
  case CELL_BUILTIN:
  case CELL_SCREF:
  case CELL_NIL: {
    cell *r = alloc_cell(tsk);
    r->type = pntrtype(p);
    r->field1 = c->field1;
    make_pntr(repl,r);
    break;
  }
  case CELL_SYMBOL: {
    cell *r = alloc_cell(tsk);
    char *name = strdup((char*)get_pntr(c->field1));
    r->type = CELL_SYMBOL;
    make_pntr(r->field1,name);
    make_pntr(repl,r);
    break;
  }
  default:
    break;
  }

  pntrmap_add(pm,p,repl);

  return repl;
}

static void replace_hilight(FILE *f, pntr p, pntr arg)
{
  cell *c;
  if (CELL_NUMBER == pntrtype(p))
    return;
  c = get_pntr(p);
  if (pntrequal(p,arg))
    fprintf(f,"style=filled,fillcolor=blue,fontcolor=white,");
  else if (c->flags & FLAG_SELECTED)
    fprintf(f,"style=filled,fillcolor=green,");
}

void dot_replace(pntr root, pntr old)
{
  static int count = 0;
  graph_setcontains(root,old);
  dot_graph("replace",count++,root,1,replace_hilight,old,NULL,0);
}

pntr graph_replace(task *tsk, pntr root, pntr old, pntr new)
{
  pntrmap *pm = pntrmap_new();
  pntr res;

  graph_setcontains(root,old);
  clear_graph(root,FLAG_PROCESSED);
  res = graph_replace_r(tsk,pm,root,old,new);
  pntrmap_free(pm);
  return res;
}
