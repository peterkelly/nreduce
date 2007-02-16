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

typedef struct replace_data {
  task *tsk;
  pntr old;
  pntr new;
  pntrmap *map;
} replace_data;

static void graph_replace_r(replace_data *rd, pntr p, pntr *repl)
{
  int n;
  pntr nullp;

  *repl = p;

  if (pntrequal(p,rd->old) || pntrequal(p,rd->new)) {
    *repl = rd->new;
    return;
  }

  switch (pntrtype(p)) {
  case CELL_BUILTIN:
  case CELL_SCREF:
  case CELL_NIL:
  case CELL_SYMBOL:
  case CELL_NUMBER:
    *repl = p;
    return;
  default:
    break;
  }

  if (0 < (n = pntrmap_lookup(rd->map,p))) {
    if (!is_nullpntr(rd->map->data[n].val)) {
      *repl = rd->map->data[n].val;
      return;
    }
    else {
      cell *newc = alloc_cell(rd->tsk);
      newc->type = CELL_IND;
      newc->field1 = 999;
      make_pntr(*repl,newc);
      rd->map->data[n].s = (snode*)1;
      rd->map->data[n].val = *repl;
      return;
    }
  }

  make_pntr(nullp,NULL);
  n = pntrmap_add(rd->map,p,NULL,NULL,nullp);

  switch (pntrtype(p)) {
  case CELL_APPLICATION: {
    pntr app = p;
    int nargs = 0;
    pntr *newargs;
    pntr newapp;
    int argno = 0;
    int different = 0;

    for (app = p; CELL_APPLICATION == pntrtype(app); app = resolve_pntr(get_pntr(app)->field1))
      nargs++;

    newargs = malloc(nargs*sizeof(pntr));

    for (app = p; CELL_APPLICATION == pntrtype(app); app = resolve_pntr(get_pntr(app)->field1)) {
      pntr arg = resolve_pntr(get_pntr(app)->field2);
      graph_replace_r(rd,arg,&newargs[argno]);
      if (!pntrequal(arg,newargs[argno]))
        different = 1;
      argno++;
    }

    graph_replace_r(rd,app,&newapp);
    if (!pntrequal(app,newapp))
      different = 1;

    if (different) {
      *repl = newapp;
      for (argno = nargs-1; argno >= 0; argno--) {
        cell *c = alloc_cell(rd->tsk);
        c->type = CELL_APPLICATION;
        c->field1 = *repl;
        c->field2 = newargs[argno];
        make_pntr(*repl,c);
      }
    }

    free(newargs);
    break;
  }
  case CELL_CONS: {
    cell *c = get_pntr(p);
    pntr r1;
    pntr r2;
    graph_replace_r(rd,c->field1,&r1);
    graph_replace_r(rd,c->field2,&r2);
    if (!pntrequal(r1,c->field1) || !pntrequal(r2,c->field2)) {
      cell *newc = alloc_cell(rd->tsk);
      newc->type = pntrtype(p);
      newc->field1 = r1;
      newc->field2 = r2;
      make_pntr(*repl,newc);
    }
    break;
  }
  case CELL_IND: {
    cell *c = get_pntr(p);
    pntr r1;
    graph_replace_r(rd,c->field1,&r1);
    if (!pntrequal(r1,c->field1)) {
      cell *newc = alloc_cell(rd->tsk);
      newc->type = CELL_IND;
      newc->field1 = r1;
      make_pntr(*repl,newc);
    }
    break;
  }
  case CELL_AREF: {
    carray *arr = aref_array(p);
    int index = aref_index(p);
    pntr rtail;
    if (sizeof(pntr) == arr->elemsize) {
      int different = 0;
      pntr *relems = (pntr*)malloc((arr->size-index)*sizeof(pntr));
      int i;
      for (i = index; i < arr->size; i++) {
        graph_replace_r(rd,((pntr*)arr->elements)[i],&relems[i-index]);
        if (!pntrequal(((pntr*)arr->elements)[i],relems[i-index]))
          different = 1;
      }
      graph_replace_r(rd,arr->tail,&rtail);
      if (!pntrequal(rtail,arr->tail))
        different = 1;
      if (different) {
        carray *newarr = carray_new(rd->tsk,sizeof(pntr),0,NULL,NULL);
        pntr refp;
        make_aref_pntr(refp,newarr->wrapper,0);
        carray_append(rd->tsk,&newarr,relems,arr->size-index,sizeof(pntr));
        newarr->tail = rtail;
        *repl = refp;
      }
      free(relems);
    }
    else {
      graph_replace_r(rd,arr->tail,&rtail);
      if (!pntrequal(rtail,arr->tail)) {
        carray *newarr = carray_new(rd->tsk,1,0,NULL,NULL);
        pntr refp;
        make_aref_pntr(refp,newarr->wrapper,0);
        carray_append(rd->tsk,&newarr,&((char*)arr->elements)[index],arr->size-index,1);
        newarr->tail = rtail;
        *repl = refp;
      }
    }
    break;
  }
  case CELL_SYSOBJECT:
    abort();
    break;
  default:
    abort();
    break;
  }

  if (rd->map->data[n].s) {
    pmentry *entry = &rd->map->data[n];
    assert(CELL_IND == pntrtype(entry->val));
    assert(999 == get_pntr(entry->val)->field1);
    get_pntr(entry->val)->field1 = *repl;
  }
  else {
    rd->map->data[n].val = *repl;
  }

  return;
}

pntr graph_replace(task *tsk, pntr root, pntr old, pntr new)
{
  replace_data rd;
  pntr res;
  rd.tsk = tsk;
  rd.old = old;
  rd.new = new;
  rd.map = pntrmap_new();

  graph_replace_r(&rd,root,&res);

  pntrmap_free(rd.map);
  return res;
}

void graph_size_r(pntr p, pntrset *visited, int *size)
{
  cell *c;

  p = resolve_pntr(p);

  if (CELL_NUMBER == pntrtype(p)) {
    (*size)++;
    return;
  }

  if (pntrset_contains(visited,p))
    return;
  pntrset_add(visited,p);
  c = get_pntr(p);

  (*size)++;

  switch (pntrtype(p)) {
  case CELL_APPLICATION:
  case CELL_CONS:
    graph_size_r(c->field1,visited,size);
    graph_size_r(c->field2,visited,size);
    break;
  case CELL_AREF: {
    carray *arr = aref_array(p);
    int index = aref_index(p);
    int i;
    if (sizeof(pntr) == arr->elemsize)
      for (i = index; i < arr->size; i++)
        graph_size_r(((pntr*)arr->elements)[i],visited,size);
    graph_size_r(arr->tail,visited,size);
    break;
  }
  }
}

int graph_size(pntr root)
{
  int size = 0;
  pntrset *visited = pntrset_new();
  graph_size_r(root,visited,&size);
  pntrset_free(visited);
  return size;
}

pntrset *pntrset_new()
{
  pntrset *ps = (pntrset*)calloc(1,sizeof(pntrset));
  ps->count = 1;
  ps->alloc = 1024;
  ps->data = (psentry*)malloc(ps->alloc*sizeof(psentry));
  make_pntr(ps->data[0].p,NULL);
  ps->data[0].next = 0;
  return ps;
}

void pntrset_free(pntrset *ps)
{
  free(ps->data);
  free(ps);
}

void pntrset_add(pntrset *ps, pntr p)
{
  unsigned char h = phash(p);
  if (ps->count == ps->alloc) {
    ps->alloc *= 2;
    ps->data = (psentry*)realloc(ps->data,ps->alloc*sizeof(psentry));
  }
  ps->data[ps->count].p = p;
  ps->data[ps->count].next = ps->map[h];
  ps->map[h] = ps->count;
  ps->count++;
}

int pntrset_contains(pntrset *ps, pntr p)
{
  unsigned char h = phash(p);
  int n = ps->map[h];
  while (n && !pntrequal(ps->data[n].p,p))
    n = ps->data[n].next;
  return (n != 0);
}

void pntrset_clear(pntrset *ps)
{
  memset(&ps->map,0,sizeof(int)*256);
  ps->count = 1;
}

pntrmap *pntrmap_new()
{
  pntrmap *pm = (pntrmap*)calloc(1,sizeof(pntrmap));
  pm->count = 1;
  pm->alloc = 1024;
  pm->data = (pmentry*)calloc(pm->alloc,sizeof(pmentry));
  make_pntr(pm->data[0].p,NULL);
  pm->data[0].next = 0;
  return pm;
}

void pntrmap_free(pntrmap *pm)
{
  free(pm->data);
  free(pm);
}

int pntrmap_add(pntrmap *pm, pntr p, snode *s, char *name, pntr val)
{
  unsigned char h = phash(p);
  int r = pm->count;
  if (pm->count == pm->alloc) {
    pm->alloc *= 2;
    pm->data = (pmentry*)realloc(pm->data,pm->alloc*sizeof(pmentry));
  }
  pm->data[pm->count].p = p;
  pm->data[pm->count].next = pm->map[h];
  pm->data[pm->count].s = s;
  pm->data[pm->count].name = name;
  pm->data[pm->count].val = val;
  pm->map[h] = pm->count;
  pm->count++;
  return r;
}

int pntrmap_lookup(pntrmap *pm, pntr p)
{
  unsigned char h = phash(p);
  int n = pm->map[h];
  while (n && !pntrequal(pm->data[n].p,p))
    n = pm->data[n].next;
  return n;
}
