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

void resolve_refs_r(cell *c, stack *bound)
{
  switch (celltype(c)) {
  case TYPE_APPLICATION:
    resolve_refs_r((cell*)c->field1,bound);
    resolve_refs_r((cell*)c->field2,bound);
    break;
  case TYPE_LAMBDA:
    stack_push(bound,(char*)c->field1);
    resolve_refs_r((cell*)c->field2,bound);
    stack_pop(bound);
    break;
  case TYPE_SYMBOL: {
    char *sym = (char*)c->field1;
    int found = 0;
    int i;
    for (i = 0; (i < bound->count) && !found; i++)
      if (!strcmp((char*)bound->data[i],sym))
        found = 1;
    if (!found) {
      scomb *sc;
      int bif;

      if (NULL != (sc = get_scomb(sym))) {
        free(c->field1);
        c->tag = TYPE_SCREF;
        c->field1 = sc;
      }
      else if (0 <= (bif = get_builtin(sym))) {
        free(c->field1);
        c->tag = TYPE_BUILTIN;
        c->field1 = (void*)bif;
      }
      else {
        fprintf(stderr,"Unbound variable: %s\n",sym);
        exit(1);
      }
    }
    break;
  }
  case TYPE_LETREC: {
    letrec *rec;
    int oldcount = bound->count;
    for (rec = (letrec*)c->field1; rec; rec = rec->next)
      stack_push(bound,rec->name);
    for (rec = (letrec*)c->field1; rec; rec = rec->next)
      resolve_refs_r(rec->value,bound);
    resolve_refs_r((cell*)c->field2,bound);
    bound->count = oldcount;
    break;
  }
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

void resolve_refs(scomb *sc)
{
  stack *bound = stack_new();
  int i;
  for (i = 0; i < sc->nargs; i++)
    stack_push(bound,sc->argnames[i]);
  resolve_refs_r(sc->body,bound);
  stack_free(bound);
}
