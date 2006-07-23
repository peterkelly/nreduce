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

#define SUPER_C

#include "grammar.tab.h"
#include "nreduce.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <assert.h>
#include <ctype.h>
#include <stdarg.h>
#include <math.h>

scomb *scombs = NULL;
scomb **lastsc = &scombs;

int genvar = 0;
int nscombs = 0;

scomb *get_scomb_index(int index)
{
  scomb *sc = scombs;
  while (0 < index) {
    index--;
    sc = sc->next;
  }
  return sc;
}

scomb *get_scomb(const char *name)
{
  scomb *sc;
  for (sc = scombs; sc; sc = sc->next)
    if (!strcmp(sc->name,name))
      return sc;
  return NULL;
}

int get_scomb_var(scomb *sc, const char *name)
{
  int i;
  for (i = 0; i < sc->nargs; i++)
    if (sc->argnames[i] && !strcmp(sc->argnames[i],name))
      return i;
  return -1;
}

void check_r(cell *c, scomb *sc)
{
  if (c->tag & FLAG_PROCESSED)
    return;
  c->tag |= FLAG_PROCESSED;

  switch (celltype(c)) {
  case TYPE_IND:
    assert(0);
    break;
  case TYPE_APPLICATION:
  case TYPE_CONS:
    check_r((cell*)c->field1,sc);
    check_r((cell*)c->field2,sc);
    break;
  case TYPE_LAMBDA:
    assert(!"lambdas should be removed by this point");
    break;
  case TYPE_SYMBOL: {
    int found = 0;
    int i;
    for (i = 0; (i < sc->nargs) && !found; i++) {
      if (!strcmp(sc->argnames[i],(char*)c->field1))
        found = 1;
    }
    if (!found) {
      fprintf(stderr,"Supercombinator %s contains free variable %s\n",sc->name,(char*)c->field1);
      assert(0);
    }
    break;
  }
  case TYPE_SCREF:
  case TYPE_BUILTIN:
  case TYPE_NIL:
  case TYPE_INT:
  case TYPE_DOUBLE:
  case TYPE_STRING:
    break;
  }
}

void check_scombs()
{
  scomb *sc;
  for (sc = scombs; sc; sc = sc->next) {
    cleargraph(sc->body,FLAG_PROCESSED);
    check_r(sc->body,sc);
  }
  /* FIXME: check that cells are not shared between supercombinators... each should
     have its own copy */
}

int sc_has_cell(scomb *sc, cell *c)
{
  int i;
  for (i = 0; i < sc->ncells; i++)
    if (sc->cells[i] == c)
      return 1;
  return 0;
}

void add_cells(scomb *sc, cell *c, int *alloc)
{
  if (sc_has_cell(sc,c))
    return;

  if (sc->ncells == *alloc) {
    (*alloc) *= 2;
    sc->cells = (cell**)realloc(sc->cells,(*alloc)*sizeof(cell*));
  }
  sc->cells[sc->ncells++] = c;

  switch (celltype(c)) {
  case TYPE_IND:
    assert(0);
    break;
  case TYPE_APPLICATION:
  case TYPE_CONS:
    add_cells(sc,(cell*)c->field1,alloc);
    add_cells(sc,(cell*)c->field2,alloc);
    break;
  case TYPE_LAMBDA:
    add_cells(sc,(cell*)c->field2,alloc);
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

void scomb_calc_cells(scomb *sc)
{
  assert(NULL == sc->cells);

  int alloc = 4;
  sc->ncells = 0;
  sc->cells = (cell**)malloc(alloc*sizeof(cell*));
  add_cells(sc,sc->body,&alloc);
}

void check_scombs_nosharing()
{
#ifdef CHECK_FOR_SUPERCOMBINATOR_SHARED_CELLS
  scomb *sc;
  clearflag(FLAG_PROCESSED);
  for (sc = scombs; sc; sc = sc->next) {
    int i;
    for (i = 0; i < sc->ncells; i++) {
      cell *c = sc->cells[i];
      if (c->tag & FLAG_PROCESSED) {
        scomb *clasher;
        printf("Supercombinator %s contains cell %p shared with another "
                "supercombinator\n",sc->name,c);

        for (clasher = scombs; clasher; clasher = clasher->next) {
          int j;
          for (j = 0; j < clasher->ncells; j++) {
            if (clasher->cells[j] == c) {
              printf("\n");
              printf("%s\n",clasher->name);
              print(clasher->body);
              break;
            }
          }
        }

        exit(1);
      }
      c->tag |= FLAG_PROCESSED;
    }
  }
#endif
}

int scomb_count()
{
  int count = 0;
  scomb *sc;
  for (sc = scombs; sc; sc = sc->next)
    count++;
  return count;
}

scomb *add_scomb(char *name, char *prefix)
{
  scomb *sc = (scomb*)calloc(1,sizeof(scomb));

  if (NULL != name) {
    sc->name = strdup(name);
    nscombs++;
  }
  else {
    sc->name = (char*)malloc(strlen(prefix)+20);
    sprintf(sc->name,"%s__%d",prefix,nscombs++);
    assert(NULL == get_scomb(sc->name));
  }

  *lastsc = sc;
  lastsc = &sc->next;

  return sc;
}

void scomb_free_list(scomb **list)
{
  while (*list) {
    scomb *sc = *list;
    *list = sc->next;
    scomb_free(sc);
  }
}

void scomb_free(scomb *sc)
{
  int i;
  free(sc->name);
  for (i = 0; i < sc->nargs; i++)
    free(sc->argnames[i]);
  free(sc->argnames);

  free(sc->strictin);
  free(sc->cells);
  free(sc);
}

void find_shared(cell *c, scomb *sc, int *shared, int *nshared)
{
  if (c->tag & FLAG_PROCESSED) {
    int i;
    for (i = 0; i < sc->ncells; i++) {
      if (c == sc->cells[i]) {
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
    find_shared((cell*)c->field1,sc,shared,nshared);
    find_shared((cell*)c->field2,sc,shared,nshared);
  }
}

cell *make_varref(char *varname)
{
  cell *ref = alloc_cell();
  ref->tag = TYPE_SYMBOL;
  ref->field1 = strdup(varname);
  ref->field2 = NULL;
  return ref;
}

cell *copy_shared(cell *c, scomb *sc, int *shared, cell **defbodies,
                  char **varnames, int *pos)
{
  int thispos = *pos;
  cell *copy;
  cell *ret;

  if (c->tag & FLAG_PROCESSED) {
    int i;
    for (i = 0; i < *pos; i++) {
      if (sc->cells[i] == c) {
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
    copy->field1 = copy_shared((cell*)c->field1,sc,shared,defbodies,varnames,pos);
    copy->field2 = copy_shared((cell*)c->field2,sc,shared,defbodies,varnames,pos);
  }

  return ret;
}

// Note: if you consider getting rid of this and just keeping the letrecs during all
// stages of the compilation process (i.e. no letrec substitution), there is another
// transformation necessary. See the bottom of IFPL p. 345 (358 in the pdf) which
// explains that the RS scheme can't handle deep letrecs, and thus all letrecs should
// be floated up to the top level of a supercombinator body.
cell *super_to_letrec(scomb *sc)
{
  int pos = 0;
  cell *root;
  int *shared = (int*)alloca(sc->ncells*sizeof(int));
  int nshared = 0;
  cell **defbodies;
  char **varnames;
  cell *letrec;
  cell **lnkptr;
  int i;

  cleargraph(sc->body,FLAG_PROCESSED);
  memset(shared,0,sc->ncells*sizeof(int));
  find_shared(sc->body,sc,shared,&nshared);

  if (0 == nshared)
    return sc->body;

  defbodies = (cell**)calloc(nshared,sizeof(cell*));
  varnames = (char**)calloc(nshared,sizeof(char*));
  for (i = 0; i < nshared; i++) {
    varnames[i] = (char*)malloc(20);
    sprintf(varnames[i],"L%d",genvar++);
  }

  cleargraph(sc->body,FLAG_PROCESSED);
  root = copy_shared(sc->body,sc,shared,defbodies,varnames,&pos);

  letrec = alloc_cell();
  letrec->tag = TYPE_LETREC;
  letrec->field1 = NULL;
  letrec->field2 = root;
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

void varused(cell *c, char *name, int *used)
{
  if (c->tag & FLAG_PROCESSED)
    return;
  c->tag |= FLAG_PROCESSED;

  assert(TYPE_IND != celltype(c));
  assert(TYPE_LAMBDA != celltype(c));
  if ((TYPE_SYMBOL == celltype(c)) && !strcmp((char*)c->field1,name))
    *used = 1;
  if ((TYPE_APPLICATION == celltype(c)) || (TYPE_CONS == celltype(c))) {
    varused((cell*)c->field1,name,used);
    varused((cell*)c->field2,name,used);
  }
}

void remove_redundant_scombs()
{
#ifdef REMOVE_REDUNDANT_SUPERCOMBINATORS
  scomb *sc;
  printf("Removing redundant arguments\n");

  for (sc = scombs; sc; sc = sc->next) {
    while ((0 < sc->nargs) &&
           (TYPE_APPLICATION == celltype(sc->body)) &&
           (TYPE_SYMBOL == celltype((cell*)sc->body->field2)) &&
           (!strcmp(sc->argnames[sc->nargs-1],(char*)((cell*)sc->body->field2)->field1))) {
      int used = 0;
      clearflag(FLAG_PROCESSED);
      varused((cell*)sc->body->field1,sc->argnames[sc->nargs-1],&used);

      if (used) {
        printf("%s: NOT removing %s\n",sc->name,sc->argnames[sc->nargs-1]);
        break;
      }
      else {
        printf("%s: removing %s\n",sc->name,sc->argnames[sc->nargs-1]);
        /* FIXME: we can only remove the parameter if it isn't used elsewhere in the body! */
        free(sc->argnames[sc->nargs-1]);
        sc->nargs--;
        sc->body = (cell*)sc->body->field1;
      }
    }
  }

  printf("Removing redundant supercombinators\n");
#endif
}

void fix_partial_applications()
{
#if 1
/*   printf("Fixing partial applications\n"); */
  scomb *sc;
  for (sc = scombs; sc; sc = sc->next) {
    cell *c = sc->body;
    int nargs = 0;
    scomb *other;
    while (TYPE_APPLICATION == celltype(c)) {
      c = (cell*)c->field1;
      nargs++;
    }
    if (TYPE_SCREF == celltype(c)) {
      other = (scomb*)c->field1;
      if (nargs < other->nargs) {
        int oldargs = sc->nargs;
        int extra = other->nargs - nargs;
        int i;
        sc->nargs += extra;
        sc->argnames = (char**)realloc(sc->argnames,sc->nargs*sizeof(char*));
        for (i = oldargs; i < sc->nargs; i++) {
          cell *fun = sc->body;
          cell *arg = alloc_cell();

          sc->argnames[i] = (char*)malloc(20);
          sprintf(sc->argnames[i],"N%d",genvar++);

          arg->tag = TYPE_SYMBOL;
          arg->field1 = strdup(sc->argnames[i]);
          arg->field2 = NULL;

          sc->body = alloc_cell();
          sc->body->tag = TYPE_APPLICATION;
          sc->body->field1 = fun;
          sc->body->field2 = arg;
        }
      }
    }
  }
/*   printf("Done fixing partial applications\n"); */
#endif
}
