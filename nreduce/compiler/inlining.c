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
 * $Id: sinking.c 526 2007-04-15 06:02:13Z pmkelly $
 *
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "src/nreduce.h"
#include "runtime/runtime.h"
#include "source.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <assert.h>

static void replace_symbols(source *src, snode *s, char **oldsyms, char **newsyms, int count)
{
  switch (s->type) {
  case SNODE_APPLICATION:
    replace_symbols(src,s->left,oldsyms,newsyms,count);
    replace_symbols(src,s->right,oldsyms,newsyms,count);
    break;
  case SNODE_LETREC: {
    letrec *rec;
    for (rec = s->bindings; rec; rec = rec->next)
      replace_symbols(src,rec->value,oldsyms,newsyms,count);
    replace_symbols(src,s->body,oldsyms,newsyms,count);
    break;
  }
  case SNODE_SYMBOL: {
    int i;
    for (i = 0; i < count; i++) {
      if (!strcmp(s->name,oldsyms[i])) {
        free(s->name);
        s->name = strdup(newsyms[i]);
        return;
      }
    }
    break;
  }
  default:
    break;
  }
}

static void inline_r(source *src, snode *s, int *changed)
{
  switch (s->type) {
  case SNODE_APPLICATION: {
    snode *a = s;
    int nargs = 0;
    while (SNODE_APPLICATION == a->type) {
      a = a->left;
      nargs++;
    }
    if ((SNODE_SCREF == a->type) && (nargs == a->sc->nargs) && a->sc->caninline) {
      scomb *asc = a->sc;
      letrec *r = NULL;
      letrec **recptr = &r;
      int pos = nargs-1;
      char **newsyms = (char**)calloc(asc->nargs,sizeof(char*));
      a = s;

      while (SNODE_APPLICATION == a->type) {
        snode *left = a->left;

        (*recptr) = (letrec*)calloc(1,sizeof(letrec));
        (*recptr)->name = next_var(src,real_varname(src,asc->argnames[pos]));
        (*recptr)->value = a->right;
        (*recptr)->strict = asc->strictin ? asc->strictin[pos] : 0;
        newsyms[pos] = (*recptr)->name;
        recptr = &((*recptr)->next);

        if (a != s)
          free(a);
        a = left;
        pos--;
      }
      free(a);

      s->type = SNODE_LETREC;
      s->bindings = r;
      s->body = snode_copy(asc->body);

      replace_symbols(src,s->body,asc->argnames,newsyms,asc->nargs);

      free(newsyms);
      *changed = 1;
    }
    else {
      inline_r(src,s->left,changed);
      inline_r(src,s->right,changed);
    }
    break;
  }
  case SNODE_LETREC: {
    letrec *rec;
    for (rec = s->bindings; rec; rec = rec->next)
      inline_r(src,rec->value,changed);
    inline_r(src,s->body,changed);
    break;
  }
  case SNODE_SCREF:
    if ((0 == s->sc->nargs) && s->sc->caninline) {
      snode *tmp = snode_copy(s->sc->body);
      memcpy(s,tmp,sizeof(snode));
      free(tmp);
    }
    break;
  case SNODE_BUILTIN:
  case SNODE_SYMBOL:
  case SNODE_NIL:
  case SNODE_NUMBER:
  case SNODE_STRING:
    break;
  default:
    abort();
    break;
  }
}

static int is_simple(snode *s)
{
  return ((SNODE_BUILTIN == s->type) || 
          (SNODE_SYMBOL == s->type) || 
          (SNODE_SCREF == s->type) || 
          (SNODE_NIL == s->type) || 
          (SNODE_NUMBER == s->type) || 
          (SNODE_STRING == s->type));
}

void inlining(source *src)
{
  int scno;
  int sccount = array_count(src->scombs);
  int changed;
  for (scno = 0; scno < sccount; scno++) {
    scomb *sc = array_item(src->scombs,scno,scomb*);
    int nargs = 0;
    snode *a = sc->body;
    int ok = 1;
    while (SNODE_APPLICATION == a->type) {
      if (!is_simple(a->right))
        ok = 0;
      a = a->left;
      nargs++;
    }
    if ((SNODE_SCREF == a->type) && ok && (nargs == a->sc->nargs))
      sc->caninline = 1;
    if ((SNODE_BUILTIN == a->type) && ok && (nargs == builtin_info[a->bif].nargs))
      sc->caninline = 1;
    if ((SNODE_NUMBER == a->type) && ok && (0 == nargs))
      sc->caninline = 1;
    if ((SNODE_STRING == a->type) && ok && (0 == nargs))
      sc->caninline = 1;
    if ((SNODE_NIL == a->type) && ok && (0 == nargs))
      sc->caninline = 1;
    if ((SNODE_SCREF == a->type) && ok && (0 == nargs))
      sc->caninline = 1;
  }

  do {
    changed = 0;
    for (scno = 0; scno < sccount; scno++) {
      scomb *sc = array_item(src->scombs,scno,scomb*);
      inline_r(src,sc->body,&changed);
    }
  } while (changed);
}
