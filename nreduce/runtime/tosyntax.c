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

typedef struct p2smapping {
  pntr p;
  snode *s;
  char *var;
} p2smapping;

typedef struct p2sdata {
  array *map;
  letrec *rec;
} p2sdata;

void p2s_add(p2sdata *d, pntr p, snode *s)
{
  p2smapping entry;
  memset(&entry,0,sizeof(p2smapping));
  entry.p = p;
  entry.s = s;
  array_append(d->map,&entry,sizeof(p2smapping));
}

p2smapping *p2s_lookup(p2sdata *d, pntr p)
{
  int i;
  for (i = 0; i < array_count(d->map); i++) {
    p2smapping *entry = (p2smapping*)array_at(d->map,i);
    if (pntrequal(p,entry->p))
      return entry;
  }
  return NULL;
}

snode *graph_to_syntax_r(source *src, p2sdata *d, pntr p)
{
  snode *s = NULL;
  cell *c;
  p2smapping *entry;

  p = resolve_pntr(p);

  switch (pntrtype(p)) {
  case CELL_NUMBER:
    s = snode_new(-1,-1);
    s->type = SNODE_NUMBER;
    s->num = p;
    return s;
  case CELL_NIL:
    s = snode_new(-1,-1);
    s->type = SNODE_NIL;
    return s;
  case CELL_SYMBOL:
    c = get_pntr(p);
    s = snode_new(-1,-1);
    s->type = SNODE_SYMBOL;
    s->name = strdup((char*)get_pntr(c->field1));
    return s;
  case CELL_BUILTIN:
    c = get_pntr(p);
    s = snode_new(-1,-1);
    s->type = SNODE_BUILTIN;
    s->bif = (int)get_pntr(c->field1);
    return s;
  case CELL_SCREF:
    c = get_pntr(p);
    s = snode_new(-1,-1);
    s->type = SNODE_SCREF;
    s->sc = (scomb*)get_pntr(c->field1);
    return s;
  default:
    break;
  }

  if (NULL != (entry = p2s_lookup(d,p))) {
    assert((NULL == entry->var) || (SNODE_SYMBOL == entry->s->type));
    if (NULL == entry->var) {
      letrec *rec;
      char *copy;

      entry->var = (char*)malloc(20);
      sprintf(entry->var,"_v%d",src->varno++);

      copy = strdup(entry->var);
      array_append(src->oldnames,&copy,sizeof(char*));

      rec = (letrec*)calloc(1,sizeof(letrec));
      rec->name = strdup(entry->var);
      rec->value = snode_new(-1,-1);
      rec->next = d->rec;
      d->rec = rec;

      memcpy(rec->value,entry->s,sizeof(snode));
      memset(entry->s,0,sizeof(snode));
      entry->s->type = SNODE_SYMBOL;
      entry->s->name = strdup(entry->var);
    }

    s = snode_new(-1,-1);
    s->type = SNODE_SYMBOL;
    s->name = strdup(entry->var);
    return s;
  }

  c = get_pntr(p);
  s = snode_new(-1,-1);
  p2s_add(d,p,s);

  switch (celltype(c)) {
  case CELL_APPLICATION: {
    snode *app = snode_new(-1,-1);

    s->type = SNODE_WRAP;
    s->target = app;

    app->type = SNODE_APPLICATION;
    app->left = graph_to_syntax_r(src,d,c->field1);
    app->right = graph_to_syntax_r(src,d,c->field2);
    break;
  }
  case CELL_CONS: {
    snode *bif = snode_new(-1,-1);
    snode *app1 = snode_new(-1,-1);
    snode *app2 = snode_new(-1,-1);

    s->type = SNODE_WRAP;
    s->target = app2;

    bif->type = SNODE_BUILTIN;
    bif->bif = B_CONS;

    app1->type = SNODE_APPLICATION;
    app1->left = bif;
    app1->right = graph_to_syntax_r(src,d,c->field1);

    app2->type = SNODE_APPLICATION;
    app2->left = app1;
    app2->right = graph_to_syntax_r(src,d,c->field2);
    break;
  }
  case CELL_AREF: {
    carray *arr = aref_array(p);
    if (1 == arr->elemsize) {
      pntr tail = resolve_pntr(arr->tail);
      snode *str = s;

      if (CELL_NIL != pntrtype(tail)) {
        snode *app2 = s;
        snode *app1 = snode_new(-1,-1);
        snode *fun = snode_new(-1,-1);

        str = snode_new(-1,-1);

        fun->type = SNODE_SCREF;
        fun->sc = get_scomb(src,"append");
        assert(fun->sc);

        app1->type = SNODE_APPLICATION;
        app1->left = fun;
        app1->right = str;

        app2->type = SNODE_APPLICATION;
        app2->left = app1;
        app2->right = graph_to_syntax_r(src,d,arr->tail);
      }

      str->type = SNODE_STRING;
      str->value = (char*)malloc(arr->size+1);
      memcpy(str->value,(char*)arr->elements,arr->size);
      str->value[arr->size] = '\0';
    }
    else {
      snode *app = s;
      int i;
      assert(0 < arr->size);
      for (i = 0; i < arr->size; i++) {
        snode *fun = snode_new(-1,-1);
        snode *a1 = snode_new(-1,-1);
        snode *newapp = snode_new(-1,-1);

        fun->type = SNODE_BUILTIN;
        fun->bif = B_CONS;

        a1->type = SNODE_APPLICATION;
        a1->left = fun;
        a1->right = graph_to_syntax_r(src,d,((pntr*)arr->elements)[i]);

        app->type = SNODE_APPLICATION;
        app->left = a1;
        app->right = newapp;

        app = newapp;
      }

      app->type = SNODE_WRAP;
      app->target = graph_to_syntax_r(src,d,arr->tail);
    }
    break;
  }
  default:
    abort();
    break;
  }

  return s;
}

snode *graph_to_syntax(source *src, pntr p)
{
  p2sdata data;
  snode *s;
  memset(&data,0,sizeof(p2sdata));
  data.map = array_new(sizeof(p2smapping));
  s = graph_to_syntax_r(src,&data,p);

  if (NULL != data.rec) {
    snode *letrec = snode_new(-1,-1);
    letrec->type = SNODE_LETREC;
    letrec->bindings = data.rec;
    letrec->body = s;
    s = letrec;
  }

  array_free(data.map);
  remove_wrappers(s);
  return s;
}

void print_graph(source *src, pntr p)
{
  snode *s = graph_to_syntax(src,p);
  print_code(src,s);
  snode_free(s);
}
