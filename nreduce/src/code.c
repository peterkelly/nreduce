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

cell *sub_letrecs(stack *s, cell *c, scomb *sc)
{
  cell *res = c;

  if (!c || (c->tag & FLAG_PROCESSED))
    return res;

  c->tag |= FLAG_PROCESSED;

  stack_push(s,c);
  switch (celltype(c)) {
  case TYPE_IND:
    assert(0);
    break;
  case TYPE_EMPTY:
    break;
  case TYPE_APPLICATION:
    c->field1 = sub_letrecs(s,(cell*)c->field1,sc);
    c->field2 = sub_letrecs(s,(cell*)c->field2,sc);
    break;
  case TYPE_LAMBDA:
    c->field2 = sub_letrecs(s,(cell*)c->field2,sc);
    break;
  case TYPE_BUILTIN:
    break;
  case TYPE_CONS:
    c->field1 = sub_letrecs(s,(cell*)c->field1,sc);
    c->field2 = sub_letrecs(s,(cell*)c->field2,sc);
    break;
  case TYPE_NIL:
    break;
  case TYPE_INT:
    break;
  case TYPE_DOUBLE:
    break;
  case TYPE_STRING:
    break;
  case TYPE_SYMBOL: {
    int chain = 0;
    int bletrec;

    cell *prevres = NULL;
    do {
      int st = s->count-2;
      int found = 0;

      bletrec = 0;

      while ((0 <= st) && !found) {
        cell *p = stack_at(s,st);
        if (TYPE_LAMBDA == celltype(p)) {
          if (!strcmp((char*)p->field1,(char*)res->field1))
            found = 1;
        }
        else if (TYPE_LETREC == celltype(p)) {
          cell *lnk;
          char *refname = (char*)res->field1;
          for (lnk = (cell*)p->field1; lnk && !found; lnk = (cell*)lnk->field2) {
            assert(TYPE_VARLNK == celltype(lnk));
            cell *def = (cell*)lnk->field1;
            assert(TYPE_VARDEF == celltype(def));
            if (!strcmp((char*)def->field1,refname)) {
              res = (cell*)def->field2;

              found = 1;
              bletrec = 1;
            }
          }
        }
        st--;
      }
      if (!found && (NULL != sc)) {
        int argno;
        for (argno = 0; argno < sc->nargs; argno++)
          if (!strcmp((char*)res->field1,sc->argnames[argno]))
            found = 1;
      }
      if (!found) {
        if (NULL != get_scomb((char*)res->field1))
          break;

        /* maybe it's a builtin */
        int i;
        for (i = 0; i < NUM_BUILTINS; i++) {
          if (!strcmp((char*)c->field1,builtin_info[i].name)) {
            free((char*)c->field1);
            c->field1 = (void*)i;
            c->tag = (TYPE_BUILTIN | (c->tag & ~TAG_MASK));
            found = 1;
            break;
          }
        }

        if (found)
          break;

        fprintf(stderr,"Variable not bound: %s\n",(char*)res->field1);

  /* FIXME: disable this to support supercombinators */
        exit(1);
      }
      chain++;

      if (prevres == res) {
        fprintf(stderr,"Invalid letrec recursion: ");
        print_codef(stderr,c);
        fprintf(stderr,"\n");
        exit(1);
      }
      prevres = res;
    } while ((TYPE_SYMBOL == celltype(res)) && bletrec);

    break;
  }
  case TYPE_SCREF:
    break;
  case TYPE_LETREC: {
    cell *lnk;
    for (lnk = (cell*)c->field1; lnk; lnk = (cell*)lnk->field2) {
      cell *def = (cell*)lnk->field1;
      sub_letrecs(s,(cell*)def->field2,sc);
    }
    res = sub_letrecs(s,(cell*)c->field2,sc);
    break;
  }
  case TYPE_VARDEF:
    assert(0);
    break;
  case TYPE_VARLNK:
    assert(0);
    break;
  }
  stack_pop(s);
  return res;
}

cell *suball_letrecs(cell *root, scomb *sc)
{
  /* FIXME: check variable usage to make sure all variables are bound in the letrec
     or above.
     How should be handle situations like ths:
       letrec
          foo (+ x 1),
          x 4,
          bar: (!x.* foo 2)
     -- what is the x in foo bound to? I think it should be the letrec x (not bar's !x)
     -- solution: rename variables to give them unique names before calling suball_letrecs()
     Same issue for:
     !x.letrec foo (+ x 1)
               x 4
        in * foo 2
  */
  cleargraph(root,FLAG_PROCESSED);
  stack *s = stack_new();
  root = sub_letrecs(s,root,sc);
  stack_free(s);
  return root;
}
