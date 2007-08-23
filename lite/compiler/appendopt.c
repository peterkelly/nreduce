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
#include <ctype.h>
#include <stdarg.h>
#include <math.h>

/* Optimize away usage of append where possible. See the paper titled "The concatenate vanishes"
   by Philip Wadler. */

snode *snode_copy(snode *s)
{
  snode *repl = snode_new(s->sl.fileno,s->sl.lineno);
  repl->type = s->type;
  repl->strict = s->strict;
  switch (s->type) {
  case SNODE_APPLICATION:
    repl->left = snode_copy(s->left);
    repl->right = snode_copy(s->right);
    break;
  case SNODE_BUILTIN:
    repl->bif = s->bif;
    break;
  case SNODE_SYMBOL:
    repl->name = strdup(s->name);
    break;
  case SNODE_LETREC: {
    letrec *rec;
    letrec **rptr = &repl->bindings;
    for (rec = s->bindings; rec; rec = rec->next) {
      (*rptr) = (letrec*)calloc(1,sizeof(letrec));
      (*rptr)->name = strdup(rec->name);
      (*rptr)->value = snode_copy(rec->value);
      (*rptr)->strict = rec->strict;
      rptr = &(*rptr)->next;
    }
    repl->body = snode_copy(s->body);
    break;
  }
  case SNODE_SCREF:
    repl->sc = s->sc;
    break;
  case SNODE_NIL:
    break;
  case SNODE_NUMBER:
    repl->num = s->num;
    break;
  case SNODE_STRING:
    repl->value = strdup(s->value);
    break;
  default:
    abort();
    break;
  }
  return repl;
}

static void mark_used(snode *s, int *used)
{
  switch (s->type) {
  case SNODE_APPLICATION:
    mark_used(s->left,used);
    mark_used(s->right,used);
    break;
  case SNODE_LETREC: {
    letrec *rec;
    for (rec = s->bindings; rec; rec = rec->next)
      mark_used(rec->value,used);
    mark_used(s->body,used);
    break;
  }
  case SNODE_SCREF:
    used[s->sc->index] = 1;
    break;
  default:
    break;
  }
}

static void make_append_version(source *src, scomb *sc)
{
  scomb *newsc = add_scomb(src,sc->name);
  snode *app1 = snode_new(sc->body->sl.fileno,sc->body->sl.lineno);
  snode *app2 = snode_new(sc->body->sl.fileno,sc->body->sl.lineno);
  snode *fun = snode_new(sc->body->sl.fileno,sc->body->sl.lineno);
  snode *sym = snode_new(sc->body->sl.fileno,sc->body->sl.lineno);
  char *restname = next_var(src,"rest");
  int i;

  newsc->nargs = sc->nargs+1;
  newsc->argnames = (char**)malloc(newsc->nargs*sizeof(char*));
  for (i = 0; i < sc->nargs; i++)
    newsc->argnames[i] = strdup(sc->argnames[i]);
  newsc->argnames[i] = strdup(restname);

  if (sc->strictin) {
    newsc->strictin = (int*)malloc(newsc->nargs*sizeof(int));
    for (i = 0; i < sc->nargs; i++)
      newsc->strictin[i] = sc->strictin[i];
    newsc->strictin[i] = 0;
  }

  sym->type = SNODE_SYMBOL;
  sym->name = strdup(restname);

  fun->type = SNODE_SCREF;
  fun->sc = get_scomb(src,"append");

  app1->type = SNODE_APPLICATION;
  app1->left = fun;
  app1->right = snode_copy(sc->body);

  app2->type = SNODE_APPLICATION;
  app2->left = app1;
  app2->right = sym;

  newsc->body = app2;
  newsc->doesappend = 1;
  sc->appendver = newsc;
  free(restname);
}

static void appreplace_r(source *src, snode *s, int lappend, int *changed)
{
  switch (s->type) {
  case SNODE_APPLICATION: {
    if ((SNODE_SCREF == s->left->type) && !strcmp(s->left->sc->name,"append")) {
      appreplace_r(src,s->left,0,changed);
      appreplace_r(src,s->right,1,changed);
    }
    else {
      appreplace_r(src,s->left,lappend,changed);
      appreplace_r(src,s->right,0,changed);
    }

    if ((SNODE_APPLICATION == s->type) &&
        (SNODE_APPLICATION == s->left->type) &&
        (SNODE_SCREF == s->left->left->type) &&
        !strcmp(s->left->left->sc->name,"append")) {
      snode *app2 = s->left;
      snode *appendref = s->left->left;
      snode *first = app2->right;
      snode *second = s->right;
      snode *a = first;
      int nargs = 0;

      if (SNODE_LETREC == first->type) {
        /* (append (letrec ... in x) y) -> (letrec ... in (append x y)) */
        snode *tmp = (snode*)malloc(sizeof(snode));
        memcpy(tmp,s,sizeof(snode));
        memcpy(s,first,sizeof(snode));
        memcpy(first,tmp,sizeof(snode));

        assert(SNODE_APPLICATION == first->type);
        app2->right = s->body;
        s->body = first;
        *changed = 1;

        free(tmp);
        return;
      }

      while (SNODE_APPLICATION == a->type) {
        a = a->left;
        nargs++;
      }
      if ((SNODE_NIL == a->type) && (0 == nargs)) {
        /* Rule 1: (append nil x) -> x */
        memcpy(s,second,sizeof(snode));
        free(app2);
        free(first);
        free(appendref);
        free(second);
        *changed = 1;
      }
      else if ((SNODE_BUILTIN == a->type) && (B_CONS == a->bif) && (2 == nargs)) {
        /* Rule 2: (append (cons x y) z) -> (cons x (append y z)) */
        memcpy(s,first,sizeof(snode));
        app2->right = s->right;
        s->right = first;
        first->left = app2;
        first->right = second;
        *changed = 1;          
      }
      else if ((SNODE_SCREF == a->type) && !strcmp(a->sc->name,"append") && (2 == nargs)) {
        /* Rule 3: (append (append x y) z) -> (append x (append y z)) */
        /* (same mutation as for rule 2) */
        memcpy(s,first,sizeof(snode));
        app2->right = s->right;
        s->right = first;
        first->left = app2;
        first->right = second;
        *changed = 1;          
      }
      else if ((SNODE_BUILTIN == a->type) && (B_IF == a->bif) && (3 == nargs)) {
        /* Rule 4:
           (append (if a b c) d) -> (if a (append b d) (append c d))
           or:
           (append (if a b c) d) -> (letrec
                                       first = d
                                     in
                                       (if a (append b first) (append c first)))
             (if d is not a symbol) */
        snode *trueap1 = snode_new(a->sl.fileno,a->sl.lineno);
        snode *trueap2 = snode_new(a->sl.fileno,a->sl.lineno);
        snode *trueappend = snode_new(a->sl.fileno,a->sl.lineno);
        snode *falseap1 = snode_new(a->sl.fileno,a->sl.lineno);
        snode *falseap2 = snode_new(a->sl.fileno,a->sl.lineno);
        snode *falseappend = snode_new(a->sl.fileno,a->sl.lineno);
        snode *truesym = snode_new(a->sl.fileno,a->sl.lineno);
        snode *falsesym = snode_new(a->sl.fileno,a->sl.lineno);
        char *firstsym = next_var(src,"first");

        /* true branch */

        trueappend->type = SNODE_SCREF;
        trueappend->sc = get_scomb(src,"append");

        truesym->type = SNODE_SYMBOL;
        if (SNODE_SYMBOL == second->type)
          truesym->name = strdup(second->name);
        else
          truesym->name = strdup(firstsym);

        trueap1->type = SNODE_APPLICATION;
        trueap1->left = trueappend;
        trueap1->right = first->left->right;

        trueap2->type = SNODE_APPLICATION;
        trueap2->left = trueap1;
        trueap2->right = truesym;

        /* false branch */

        falseappend->type = SNODE_SCREF;
        falseappend->sc = get_scomb(src,"append");

        falsesym->type = SNODE_SYMBOL;
        if (SNODE_SYMBOL == second->type)
          falsesym->name = strdup(second->name);
        else
          falsesym->name = strdup(firstsym);

        falseap1->type = SNODE_APPLICATION;
        falseap1->left = falseappend;
        falseap1->right = first->right;

        falseap2->type = SNODE_APPLICATION;
        falseap2->left = falseap1;
        falseap2->right = falsesym;

        first->left->right = trueap2;
        first->right = falseap2;

        if (SNODE_SYMBOL == second->type) {
          memcpy(s,first,sizeof(snode));
          free(first);
          snode_free(second);
        }
        else {
          s->type = SNODE_LETREC;
          s->body = first;
          s->bindings = (letrec*)calloc(1,sizeof(letrec));
          s->bindings->name = strdup(firstsym);
          s->bindings->value = second;
        }
        *changed = 1;

        free(app2);
        free(appendref);
        free(firstsym);
      }
      else if ((SNODE_SCREF == a->type) && strcmp(a->sc->name,"append")) {
        if (!a->sc->doesappend) {
          if (NULL == a->sc->appendver)
            make_append_version(src,a->sc);
          assert(a->sc->appendver);
          assert(a->sc->appendver->doesappend);

          /* Rule *: (append (f x1 ... xn) y) -> (f' y x1 ... xn) */
          s->left = first;
          s->right = second;
          a->sc = a->sc->appendver;

          free(app2);
          free(appendref);
          *changed = 1;
        }
        else {
          memcpy(s,first,sizeof(snode));
          first->right = second;
          first->left = app2;
          app2->right = s->right;
          s->right = first;
        }
      }
    }
    break;
  }
  case SNODE_LETREC: {
    letrec *rec;
    for (rec = s->bindings; rec; rec = rec->next)
      appreplace_r(src,rec->value,0,changed);
    appreplace_r(src,s->body,lappend,changed);
    break;
  }
  case SNODE_SCREF:
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

void appendopt(source *src)
{
  int scno;
  int sccount = array_count(src->scombs);
  int *used;
  array *newscombs;

  for (scno = 0; scno < array_count(src->scombs); scno++) {
    scomb *sc = array_item(src->scombs,scno,scomb*);
    int changed;
    do {
      changed = 0;
      appreplace_r(src,sc->body,0,&changed);
    } while (changed);
  }
  sccount = array_count(src->scombs);

  used = (int*)calloc(sccount,sizeof(int));
  for (scno = 0; scno < sccount; scno++) {
    scomb *sc = array_item(src->scombs,scno,scomb*);
    mark_used(sc->body,used);
  }
  newscombs = array_new(sizeof(scomb*),0);
  for (scno = 0; scno < sccount; scno++) {
    scomb *sc = array_item(src->scombs,scno,scomb*);
    if (NULL == sc)
      continue;
    if ((NULL == sc->appendver) || used[sc->index] || !strcmp(sc->name,"__start")) {
      sc->index = array_count(newscombs);
      array_append(newscombs,&sc,sizeof(scomb*));
    }
    else if (sc->appendver) {
      free(sc->appendver->name);
      sc->appendver->name = strdup(sc->name);

      ((scomb**)src->scombs->data)[sc->appendver->index] = NULL;
      sc->appendver->index = array_count(newscombs);
      array_append(newscombs,&sc->appendver,sizeof(scomb*));
      scomb_free(sc);
    }
    else {
      scomb_free(sc);
    }
  }
  array_free(src->scombs);
  src->scombs = newscombs;
  free(used);

  schash_rebuild(src);
}
