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

#define SUPER_C

#include "src/nreduce.h"
#include "runtime/runtime.h"
#include "source.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <assert.h>
#include <ctype.h>
#include <stdarg.h>
#include <math.h>

scomb *get_scomb_index(source *src, int index)
{
  return array_item(src->scombs,index,scomb*);
}

scomb *get_scomb(source *src, const char *name)
{
  int h = hash(name,strlen(name));
  scomb *sc = src->schash[h];
  while (sc && strcmp(sc->name,name))
    sc = sc->hashnext;
  return sc;
}

int get_scomb_var(scomb *sc, const char *name)
{
  int i;
  for (i = 0; i < sc->nargs; i++)
    if (sc->argnames[i] && !strcmp(sc->argnames[i],name))
      return i;
  return -1;
}

scomb *add_scomb(source *src, const char *name1)
{
  char *name = strdup(name1);
  char *hashpos = strchr(name,'#');
  scomb *sc;
  int num;
  int h;

  if (hashpos)
    *hashpos = '\0';

  sc = (scomb*)calloc(1,sizeof(scomb));
  num = 0;
  while (1) {
    sc->name = (char*)malloc(strlen(name)+20);
    if (0 < num)
      sprintf(sc->name,"%s#%d",name,num);
    else
      sprintf(sc->name,"%s",name);
    if (NULL == get_scomb(src,sc->name))
      break;
    free(sc->name);
    num++;
  }

  sc->index = array_count(src->scombs);
  array_append(src->scombs,&sc,sizeof(scomb*));

  sc->sl.fileno = -1;
  sc->sl.lineno = -1;

  h = hash(sc->name,strlen(sc->name));
  sc->hashnext = src->schash[h];
  src->schash[h] = sc;

  free(name);
  return sc;
}

void scomb_free(scomb *sc)
{
  int i;
  snode_free(sc->body);
  free(sc->name);
  free(sc->modname);
  for (i = 0; i < sc->nargs; i++)
    free(sc->argnames[i]);
  free(sc->argnames);

  free(sc->strictin);
  free(sc);
}

/* Detects whether the the specified supercombinator could potentially be applied to some
   arguments - either through direct usage, or via a variable */
static int sc_maybe_applied(snode *s, scomb *sc, int *examined)
{
  switch (s->type) {
  case SNODE_APPLICATION: {
    snode *app = s;
    int nargs = 0;
    while (SNODE_APPLICATION == app->type) {
      nargs++;
      app = app->left;
      while (SNODE_WRAP == app->type)
        app = app->target;
    }
    if (((SNODE_BUILTIN == app->type) &&
         (nargs <= builtin_info[app->bif].nargs)) ||
        ((SNODE_SCREF == app->type) &&
         (nargs <= app->sc->nargs))) {
      /* Application of a built-in function or supercombinator to the right number or fewer
         args - this is safe */
      return (sc_maybe_applied(s->left,sc,examined) ||
              sc_maybe_applied(s->right,sc,examined));
    }
    /* Application of a variable to some args, or application of the result of another
       fucntion call to some args; this could potentially be recursive */
    return 1;
  }
  case SNODE_LAMBDA:
    return sc_maybe_applied(s->body,sc,examined);
  case SNODE_LETREC: {
    letrec *rec;
    for (rec = s->bindings; rec; rec = rec->next)
      if (sc_maybe_applied(rec->value,sc,examined))
        return 1;
    return sc_maybe_applied(s->body,sc,examined);
  }
  case SNODE_SCREF:
    if (s->sc == sc)
      return 1;
    if (examined[s->sc->index])
      return 0;
    examined[s->sc->index] = 1;
    return sc_maybe_applied(s->sc->body,sc,examined);
  case SNODE_WRAP:
    return sc_maybe_applied(s->target,sc,examined);
  default:
    break;
  }
  return 0;
}

void detect_nonrecursion(source *src)
{
  int i;
  int count = array_count(src->scombs);
  int *examined = (int*)malloc(count*sizeof(int));

  for (i = 0; i < count; i++) {
    scomb *sc = array_item(src->scombs,i,scomb*);
    memset(examined,0,count*sizeof(int));
    sc->nonrecursive = !sc_maybe_applied(sc->body,sc,examined);
  }
  free(examined);
}
