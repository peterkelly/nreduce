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

extern int genvar;

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

static void add_cells(cell ***cells, int *ncells, cell *c, int *alloc)
{
  int i;
  for (i = 0; i < (*ncells); i++)
    if ((*cells)[i] == c)
      return;

  if ((*ncells) == *alloc) {
    (*alloc) *= 2;
    (*cells) = (cell**)realloc((*cells),(*alloc)*sizeof(cell*));
  }
  (*cells)[(*ncells)++] = c;

  switch (celltype(c)) {
  case TYPE_IND:
    assert(0);
    break;
  case TYPE_APPLICATION:
  case TYPE_CONS:
    add_cells(cells,ncells,(cell*)c->field1,alloc);
    add_cells(cells,ncells,(cell*)c->field2,alloc);
    break;
  case TYPE_LAMBDA:
    add_cells(cells,ncells,(cell*)c->field2,alloc);
    break;
  case TYPE_BUILTIN:
  case TYPE_NIL:
  case TYPE_INT:
  case TYPE_DOUBLE:
  case TYPE_STRING:
  case TYPE_SYMBOL:
  case TYPE_SCREF:
    break;
  default:
    assert(0);
    break;
  }
}

static void find_graph_cells(cell ***cells, int *ncells, cell *root)
{
  int alloc = 4;
  *ncells = 0;
  *cells = (cell**)malloc(alloc*sizeof(cell*));
  add_cells(cells,ncells,root,&alloc);
}

static void find_shared(cell *c, cell **cells, int ncells, int *shared, int *nshared)
{
  if (c->tag & FLAG_PROCESSED) {
    int i;
    for (i = 0; i < ncells; i++) {
      if (c == cells[i]) {
        if (!shared[i]) {
          shared[i] = ++(*nshared);
          nshared++;
        }
        return;
      }
    }
    assert(0);
    return;
  }
  c->tag |= FLAG_PROCESSED;

  if ((TYPE_APPLICATION == celltype(c)) || (TYPE_CONS == celltype(c))) {
    find_shared((cell*)c->field1,cells,ncells,shared,nshared);
    find_shared((cell*)c->field2,cells,ncells,shared,nshared);
  }
}

static cell *make_varref(char *varname)
{
  cell *ref = alloc_cell2(TYPE_SYMBOL,strdup(varname),NULL);
  return ref;
}

static cell *copy_shared(cell *c, cell **cells, int *shared, cell **defbodies,
                  char **varnames, int *pos)
{
  int thispos = *pos;
  cell *copy;
  cell *ret;

  if (c->tag & FLAG_PROCESSED) {
    int i;
    for (i = 0; i < *pos; i++) {
      if (cells[i] == c) {
        assert(shared[i]);
        return make_varref(varnames[shared[i]-1]);
      }
    }
    assert(!"can't find copy");
    return NULL;
  }

  c->tag |= FLAG_PROCESSED;
  (*pos)++;

  copy = alloc_cell();

  if (shared[thispos]) {
    defbodies[shared[thispos]-1] = copy;
    ret = make_varref(varnames[shared[thispos]-1]);
  }
  else {
    ret = copy;
  }

  copy_cell(copy,c);

  if ((TYPE_APPLICATION == celltype(c)) || (TYPE_CONS == celltype(c))) {
    copy->field1 = copy_shared((cell*)c->field1,cells,shared,defbodies,varnames,pos);
    copy->field2 = copy_shared((cell*)c->field2,cells,shared,defbodies,varnames,pos);
  }

  return ret;
}

// Note: if you consider getting rid of this and just keeping the letrecs during all
// stages of the compilation process (i.e. no letrec substitution), there is another
// transformation necessary. See the bottom of IFPL p. 345 (358 in the pdf) which
// explains that the RS scheme can't handle deep letrecs, and thus all letrecs should
// be floated up to the top level of a supercombinator body.
cell *graph_to_letrec(cell *root)
{
  cell **cells = NULL;
  int ncells = 0;
  find_graph_cells(&cells,&ncells,root);

  int pos = 0;
  int *shared = (int*)alloca(ncells*sizeof(int));
  int nshared = 0;
  cell **defbodies;
  char **varnames;
  cell *letrec;
  cell **lnkptr;
  int i;

  cleargraph(root,FLAG_PROCESSED);
  memset(shared,0,ncells*sizeof(int));
  find_shared(root,cells,ncells,shared,&nshared);

  if (0 == nshared)
    return root;

  defbodies = (cell**)calloc(nshared,sizeof(cell*));
  varnames = (char**)calloc(nshared,sizeof(char*));
  for (i = 0; i < nshared; i++) {
    varnames[i] = (char*)malloc(20);
    sprintf(varnames[i],"L%d",genvar++);
  }

  cleargraph(root,FLAG_PROCESSED);

  letrec = alloc_cell();
  letrec->tag = TYPE_LETREC;
  letrec->field1 = NULL;
  letrec->field2 = copy_shared(root,cells,shared,defbodies,varnames,&pos);
  lnkptr = (cell**)&letrec->field1;

  for (i = 0; i < nshared; i++) {
    cell *def = alloc_cell();
    cell *lnk = alloc_cell();

    def->tag = TYPE_VARDEF;
    def->field1 = varnames[i];
    def->field2 = defbodies[i];

    lnk->tag = TYPE_VARLNK;
    lnk->field1 = def;
    lnk->field2 = NULL;
    *lnkptr = lnk;
    lnkptr = (cell**)&lnk->field2;
  }

  free(defbodies);
  free(varnames);

  return letrec;
}
