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
 * $Id: resolve.c 603 2007-08-14 02:44:16Z pmkelly $
 *
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "src/nreduce.h"
#include "source.h"
#include "runtime/runtime.h"
#include "runtime/builtins.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <assert.h>
#include <ctype.h>
#include <stdarg.h>
#include <math.h>

static void resolve_refs_r(source *src, snode *c, stack *bound, list **unbound,
                           const char *modname)
{
  switch (c->type) {
  case SNODE_APPLICATION:
    resolve_refs_r(src,c->left,bound,unbound,modname);
    resolve_refs_r(src,c->right,bound,unbound,modname);
    break;
  case SNODE_LAMBDA:
    stack_push(bound,c->name);
    resolve_refs_r(src,c->body,bound,unbound,modname);
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
      scomb *sc = NULL;
      int bif;
      char *scname;

      if (modname && (NULL == strstr(sym,"::"))) {
        scname = (char*)malloc(strlen(modname)+2+strlen(sym)+1);
        sprintf(scname,"%s::%s",modname,sym);
        sc = get_scomb(src,scname);
        free(scname);
      }

      if (NULL == sc)
        sc = get_scomb(src,sym);

      if (NULL != sc) {
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
      else if ( (bif = get_extfunc(sym)) >= 0){
      	free(c->name);
      	c->name = NULL;
      	c->type = SNODE_BUILTIN;
      	c->bif =  bif + MAX_BUILTINS;
      }
      else {
        unboundvar *ubv = (unboundvar*)calloc(1,sizeof(unboundvar));
        ubv->sl = c->sl;
        ubv->name = strdup(sym);
        list_append(unbound,ubv);
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
      resolve_refs_r(src,rec->value,bound,unbound,modname);
    resolve_refs_r(src,c->body,bound,unbound,modname);
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

void resolve_refs(source *src, scomb *sc, list **unbound)
{
  stack *bound = stack_new();
  int i;
  for (i = 0; i < sc->nargs; i++)
    stack_push(bound,sc->argnames[i]);
  resolve_refs_r(src,sc->body,bound,unbound,sc->modname);
  stack_free(bound);
}
