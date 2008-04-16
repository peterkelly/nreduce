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
 * $Id: renaming.c 603 2007-08-14 02:44:16Z pmkelly $
 *
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "src/nreduce.h"
#include "source.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <assert.h>
#include <ctype.h>
#include <stdarg.h>
#include <math.h>

typedef struct mapping {
  char *from;
  char *to;
} mapping;

static mapping *mapping_new(const char *from, const char *to)
{
  mapping *mp = (mapping*)calloc(1,sizeof(mapping));
  mp->from = strdup(from);
  mp->to = strdup(to);
  return mp;
}

static void mapping_free(mapping *mp)
{
  free(mp->from);
  free(mp->to);
  free(mp);
}

static void mappings_set_count(stack *mappings, int count)
{
  int i;
  for (i = count; i < mappings->count; i++)
    mapping_free((mapping*)mappings->data[i]);
  mappings->count = count;
}

char *next_var(source *src, const char *oldname)
{
  char *name = (char*)malloc(strlen(GENVAR_PREFIX)+20);
  char *copy;
  sprintf(name,"%s%d",GENVAR_PREFIX,src->varno++);

  copy = strdup(oldname);
  array_append(src->oldnames,&copy,sizeof(char*));

  return name;
}

static void rename_variables_r(source *src, snode *c, stack *mappings)
{
  switch (c->type) {
  case SNODE_APPLICATION:
    rename_variables_r(src,c->left,mappings);
    rename_variables_r(src,c->right,mappings);
    break;
  case SNODE_LAMBDA: {
    char *newname = next_var(src,c->name);
    int oldcount = mappings->count;
    stack_push(mappings,mapping_new(c->name,newname));
    free(c->name);
    c->name = newname;
    rename_variables_r(src,c->body,mappings);
    mappings_set_count(mappings,oldcount);
    break;
  }
  case SNODE_LETREC: {
    int oldcount = mappings->count;
    letrec *rec;

    for (rec = c->bindings; rec; rec = rec->next) {
      char *newname = next_var(src,rec->name);
      stack_push(mappings,mapping_new(rec->name,newname));
      free(rec->name);
      rec->name = newname;
    }

    for (rec = c->bindings; rec; rec = rec->next)
      rename_variables_r(src,rec->value,mappings);
    rename_variables_r(src,c->body,mappings);

    mappings_set_count(mappings,oldcount);
    break;
  }
  case SNODE_SYMBOL: {
    int i;
    for (i = mappings->count-1; 0 <= i; i--) {
      mapping *mp = (mapping*)mappings->data[i];
      if (!strcmp(mp->from,c->name)) {
        free(c->name);
        c->name = strdup(mp->to);
      }
    }
    break;
  }
  case SNODE_BUILTIN:
  case SNODE_SCREF:
  case SNODE_NIL:
  case SNODE_NUMBER:
  case SNODE_STRING:
//  case SNODE_EXTFUNC:
    break;
  default:
    abort();
    break;
  }
}

void rename_variables(source *src, scomb *sc)
{
  stack *mappings = stack_new();
  int i;

  if (NULL == src->oldnames)
    src->oldnames = array_new(sizeof(char*),0);

  for (i = 0; i < sc->nargs; i++) {
    char *newname = next_var(src,sc->argnames[i]);
    stack_push(mappings,mapping_new(sc->argnames[i],newname));
    free(sc->argnames[i]);
    sc->argnames[i] = newname;
  }

  rename_variables_r(src,sc->body,mappings);

  mappings_set_count(mappings,0);
  stack_free(mappings);
}
