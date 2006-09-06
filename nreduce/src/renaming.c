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
 * $Id: letrec.c 284 2006-07-25 07:09:57Z pmkelly $
 *
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "grammar.tab.h"
#include "nreduce.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <assert.h>
#include <ctype.h>
#include <stdarg.h>
#include <math.h>

static int varno = 0;

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

static char *next_var(const char *oldname)
{
  char *name = (char*)malloc(20);
  char *copy;
  sprintf(name,"_v%d",varno++);

  copy = strdup(oldname);
  array_append(oldnames,&copy,sizeof(char*));

  return name;
}

static void rename_variables_r(snode *c, stack *mappings)
{
  switch (snodetype(c)) {
  case TYPE_APPLICATION:
  case TYPE_CONS:
    rename_variables_r(c->left,mappings);
    rename_variables_r(c->right,mappings);
    break;
  case TYPE_LAMBDA: {
    char *newname = next_var(c->name);
    int oldcount = mappings->count;
    stack_push(mappings,mapping_new(c->name,newname));
    free(c->name);
    c->name = newname;
    rename_variables_r(c->body,mappings);
    mappings_set_count(mappings,oldcount);
    break;
  }
  case TYPE_LETREC: {
    int oldcount = mappings->count;
    letrec *rec;

    for (rec = c->bindings; rec; rec = rec->next) {
      char *newname = next_var(rec->name);
      stack_push(mappings,mapping_new(rec->name,newname));
      free(rec->name);
      rec->name = newname;
    }

    for (rec = c->bindings; rec; rec = rec->next)
      rename_variables_r(rec->value,mappings);
    rename_variables_r(c->body,mappings);

    mappings_set_count(mappings,oldcount);
    break;
  }
  case TYPE_SYMBOL: {
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
  case TYPE_BUILTIN:
  case TYPE_SCREF:
  case TYPE_NIL:
  case TYPE_NUMBER:
  case TYPE_STRING:
    break;
  default:
    abort();
    break;
  }
}

void rename_variables(scomb *sc)
{
  stack *mappings = stack_new();
  int i;

  if (NULL == oldnames)
    oldnames = array_new();

  for (i = 0; i < sc->nargs; i++)
    stack_push(mappings,mapping_new(sc->argnames[i],sc->argnames[i]));

  rename_variables_r(sc->body,mappings);

  mappings_set_count(mappings,0);
  stack_free(mappings);
}
