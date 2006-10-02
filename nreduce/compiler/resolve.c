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

static void resolve_refs_r(source *src, snode *c, stack *bound)
{
  switch (c->type) {
  case SNODE_APPLICATION:
    resolve_refs_r(src,c->left,bound);
    resolve_refs_r(src,c->right,bound);
    break;
  case SNODE_LAMBDA:
    stack_push(bound,c->name);
    resolve_refs_r(src,c->body,bound);
    bound->count--;
    break;
  case SNODE_SYMBOL: {
    char *sym = c->name;
    int found = 0;
    int i;
    for (i = 0; (i < bound->count) && !found; i++)
      if (!strcmp((char*)bound->data[i],sym))
        found = 1;
    if (!found) {
      scomb *sc;
      int bif;

      if (NULL != (sc = get_scomb(src,sym))) {
        free(c->name);
        c->name = NULL;
        c->type = SNODE_SCREF;
        c->sc = sc;
      }
      else if (0 <= (bif = get_builtin(sym))) {
        free(c->name);
        c->name = NULL;
        c->type = SNODE_BUILTIN;
        c->bif = bif;
      }
      else {
        print_sourceloc(src,stderr,c->sl);
        fprintf(stderr,"Unbound variable: %s\n",sym);
        exit(1);
      }
    }
    break;
  }
  case SNODE_LETREC: {
    letrec *rec;
    int oldcount = bound->count;
    for (rec = c->bindings; rec; rec = rec->next)
      stack_push(bound,rec->name);
    for (rec = c->bindings; rec; rec = rec->next)
      resolve_refs_r(src,rec->value,bound);
    resolve_refs_r(src,c->body,bound);
    bound->count = oldcount;
    break;
  }
  case SNODE_NIL:
  case SNODE_NUMBER:
  case SNODE_STRING:
    break;
  default:
    abort();
    break;
  }
}

void resolve_refs(source *src, scomb *sc)
{
  stack *bound = stack_new();
  int i;
  for (i = 0; i < sc->nargs; i++)
    stack_push(bound,sc->argnames[i]);
  resolve_refs_r(src,sc->body,bound);
  stack_free(bound);
}
