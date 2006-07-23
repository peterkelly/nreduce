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

#include "grammar.tab.h"
#include "nreduce.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <assert.h>
#include <ctype.h>
#include <stdarg.h>
#include <math.h>

static int lookup(stack *bound, const char *sym, cell **val)
{
  *val = NULL;
  int pos;
  for (pos = bound->count-1; (0 <= pos); pos--) {
    cell *p = stack_at(bound,pos);
    if (TYPE_LAMBDA == celltype(p)) {
      if (!strcmp((char*)p->field1,sym))
        return 1;
    }
    else {
      assert(TYPE_LETREC == celltype(p));
      cell *lnk;
      for (lnk = (cell*)p->field1; lnk; lnk = (cell*)lnk->field2) {
        if (!strcmp(def_name(lnk),sym)) {
          *val = def_value(lnk);
          return 1;
        }
      }
    }
  }
  return 0;
}

static int sc_has_arg(scomb *sc, const char *name)
{
  int argno;
  for (argno = 0; argno < sc->nargs; argno++)
    if (!strcmp(name,sc->argnames[argno]))
      return 1;
  return 0;
}

static void sub_letrecs(stack *bound, cell **k, scomb *sc)
{
  if (!(*k) || ((*k)->tag & FLAG_PROCESSED))
    return;

  (*k)->tag |= FLAG_PROCESSED;

  switch (celltype(*k)) {
  case TYPE_APPLICATION:
  case TYPE_CONS:
    sub_letrecs(bound,(cell**)&(*k)->field1,sc);
    sub_letrecs(bound,(cell**)&(*k)->field2,sc);
    break;
  case TYPE_LAMBDA:
    stack_push(bound,*k);
    sub_letrecs(bound,(cell**)&(*k)->field2,sc);
    stack_pop(bound);
    break;
  case TYPE_LETREC: {
    stack_push(bound,*k);
    cell *lnk;
    for (lnk = letrec_defs(*k); lnk; lnk = (cell*)lnk->field2) {
      assert(TYPE_VARLNK == celltype(lnk));
      cell *def = (cell*)lnk->field1;
      assert(TYPE_VARDEF == celltype(def));
      sub_letrecs(bound,(cell**)&def->field2,sc);
    }
    sub_letrecs(bound,(cell**)&(*k)->field2,sc);
    *k = (cell*)(*k)->field2;
    stack_pop(bound);
    break;
  }
  case TYPE_SYMBOL: {
    cell *prevres = NULL;
    do {
      char *sym = (char*)(*k)->field1;
      cell *val = NULL;

      if (lookup(bound,sym,&val)) {
        if (NULL == val) /* bound by lambda; leave *c as a symbol */
          return;
        *k = val; /* and loop again */
      }
      else {
        if ((NULL != sc) && sc_has_arg(sc,sym))
          return;

        /* maybe it's a supercombinator */
        scomb *refsc = get_scomb(sym);
        if (NULL != refsc) {
          free_cell_fields(*k);
          (*k)->tag = TYPE_SCREF;
          (*k)->field1 = refsc;
          return;
        }

        /* maybe it's a builtin */
        int bif = get_builtin(sym);
        if (0 <= bif) {
          free_cell_fields(*k);
          (*k)->field1 = (void*)bif;
          (*k)->tag = (TYPE_BUILTIN | ((*k)->tag & ~TAG_MASK));
          return;
        }

        fprintf(stderr,"Variable not bound: %s\n",sym);
        exit(1);
      }

      if (prevres == *k) {
        fprintf(stderr,"Invalid letrec recursion: ");
        print_codef(stderr,*k);
        fprintf(stderr,"\n");
        exit(1);
      }
      prevres = *k;
    } while (TYPE_SYMBOL == celltype(*k));

    break;
  }
  case TYPE_SCREF:
  case TYPE_BUILTIN:
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

void letrecs_to_graph(cell **root, scomb *sc)
{
  cleargraph(*root,FLAG_PROCESSED);
  stack *bound = stack_new();
  sub_letrecs(bound,root,sc);
  stack_free(bound);
}
