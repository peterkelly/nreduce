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

/**
 * Re-order letrec expressions so that the bindings are listed in order of their occurrance,
 * based on a pre-order traversal of the expression's body.
 *
 * This enables more efficient compilation by allowing expressions in some cases to be evaluated
 * directly, rather than being delayed. e.g. consider the following code:
 *
 * (let ((c (+ b 4))
 *       (b (+ a 3))
 *       (a (+ 1 2)))
 *      (* c 5)))
 *
 * The compiled G-code for this cannot evaluate c directly, because b and a have not yet been
 * evaluated (and will thus resolve to a cell with type HOLE). It must therefore create a delayed
 * evaluation of + using MKFRAME. The situation changes however if the expression is changed to the
 * following:
 *
 * let ((a (+ 1 2))
 *      (b (+ a 3))
 *      (c (+ b 4)))
 *     (* c 5)
 *
 * In this new version, a can be computed directly because it does not depend on any unevaluated
 * variables, and likewise for b and c. This results in more efficient code since we no longer need
 * to allocate memory to hold the delayed expressions.
 *
 * Note that the problem cannot be solved for all cases, however, since the letrec could be a
 * true graph:
 *
 * let ((a (cons 1 c))
 *      (b (cons a 3))
 *      (c (cons b 4)))
 *     (* c 5)
 *
 * This expression will be left as-is.
 */
static void reorder_letrecs_r(snode *c, list **used, stack *bound)
{
  switch (snodetype(c)) {
  case TYPE_APPLICATION:
    reorder_letrecs_r(c->left,used,bound);
    reorder_letrecs_r(c->right,used,bound);
    break;
  case TYPE_LETREC: {
    letrec *newrecs;
    letrec **newptr;
    list **lptr;
    letrec *rec;

    stack_push(bound,c->bindings);

    reorder_letrecs_r(c->body,used,bound);

    /* Create a new linked list of letrecs my moving the old entries one by one as we
       encounter them in the list of used variables. If there are any letrecs left in the
       old list at the end of this process, it means they are not used (and can thus be
       safely removed) */

    newrecs = NULL;
    newptr = &newrecs;

    lptr = used;
    while (*lptr) {
      char *usedname = (char*)(*lptr)->data;
      int found = 0;
      letrec **recptr;
      for (recptr = &c->bindings; *recptr; recptr = &(*recptr)->next) {
        char *recname = (char*)(*recptr)->name;
        if (!strcmp(recname,usedname)) {
          rec = *recptr;
          /* remove letrec from old list */
          *recptr = rec->next;
          rec->next = NULL;
          /* add letrec to new list */
          *newptr = rec;
          newptr = &((*newptr)->next);

          found = 1;
          break;
        }
      }

      if (found) {
        /* remove variable from used list */
        list *old = *lptr;
        *lptr = (*lptr)->next;
        free(old);
      }
      else {
        lptr = &(*lptr)->next;
      }
    }

    /* Remove any letrec bindings in the old list */
    rec = c->bindings;
    while (rec) {
      letrec *next = rec->next;
      free_letrec(rec);
      rec = next;
    }

    c->bindings = newrecs;

    bound->count--;
    break;
  }
  case TYPE_SYMBOL: {
    int i;
    letrec *rec;

    /* Encountered a symbol - add it to the beginning of the list of variables used, but
       *only* if we have not encountered it (otherwise we could have infinite recursion) */
    if (list_contains_string(*used,c->name))
      break;

    list_push(used,c->name);

    for (i = bound->count-1; 0 <= i; i--) {
      for (rec = (letrec*)bound->data[i]; rec; rec = rec->next) {
        if (!strcmp(rec->name,c->name)) {
          reorder_letrecs_r(rec->value,used,bound);
          return;
        }
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
  }
}

void reorder_letrecs(snode *c)
{
  list *used = NULL;
  stack *bound = stack_new();
  reorder_letrecs_r(c,&used,bound);
  stack_free(bound);
  list_free(used,NULL);
}
