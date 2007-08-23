/*
 * This file is part of the nreduce project
 * Copyright (C) 2006-2007 Peter Kelly (pmk@cs.adelaide.edu.au)
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

typedef struct p2sdata {
  letrec *rec;
  pntrmap *pm;
} p2sdata;

snode *graph_to_syntax_r(source *src, p2sdata *d, pntr p)
{
  snode *s = NULL;
  cell *c;
  int eno;
  pntr zero;
  set_pntrdouble(zero,0);

  p = resolve_pntr(p);

  switch (pntrtype(p)) {
  case CELL_NUMBER:
    s = snode_new(-1,-1);
    s->type = SNODE_NUMBER;
    s->num = pntrdouble(p);
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

  if (0 < (eno = pntrmap_lookup(d->pm,p))) {
    pmentry *entry = &d->pm->data[eno];
    assert((NULL == entry->name) || (SNODE_SYMBOL == entry->s->type));
    if (NULL == entry->name) {
      letrec *rec;
      char *copy;

      entry->name = (char*)malloc(20);
      sprintf(entry->name,"_v%d",src->varno++);

      copy = strdup(entry->name);
      array_append(src->oldnames,&copy,sizeof(char*));

      rec = (letrec*)calloc(1,sizeof(letrec));
      rec->name = strdup(entry->name);
      rec->value = snode_new(-1,-1);
      rec->next = d->rec;
      d->rec = rec;

      memcpy(rec->value,entry->s,sizeof(snode));
      memset(entry->s,0,sizeof(snode));
      entry->s->type = SNODE_SYMBOL;
      entry->s->name = strdup(entry->name);
    }

    s = snode_new(-1,-1);
    s->type = SNODE_SYMBOL;
    s->name = strdup(entry->name);
    return s;
  }

  c = get_pntr(p);
  s = snode_new(-1,-1);
  pntrmap_add(d->pm,p,s,NULL,zero);

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
    int index = aref_index(p);
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
      str->value = (char*)malloc(arr->size-index+1);
      memcpy(str->value,&((char*)arr->elements)[index],arr->size-index);
      str->value[arr->size-index] = '\0';
    }
    else {
      snode *app = s;
      int i;
      assert(index < arr->size);
      for (i = index; i < arr->size; i++) {
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
  case CELL_SYSOBJECT:
    abort();
    break;
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
  int i;
  memset(&data,0,sizeof(p2sdata));
  data.pm = pntrmap_new();
  s = graph_to_syntax_r(src,&data,p);

  if (NULL != data.rec) {
    snode *letrec = snode_new(-1,-1);
    letrec->type = SNODE_LETREC;
    letrec->bindings = data.rec;
    letrec->body = s;
    s = letrec;
  }

  for (i = 0; i < data.pm->count; i++)
    free(data.pm->data[i].name);
  pntrmap_free(data.pm);

  remove_wrappers(s);
  return s;
}

void print_graph(source *src, pntr p)
{
  snode *s = graph_to_syntax(src,p);
  print_code(src,s);
  snode_free(s);
}
