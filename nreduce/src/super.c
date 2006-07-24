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

void scomb_free(scomb *sc)
{
  int i;
  free(sc->name);
  for (i = 0; i < sc->nargs; i++)
    free(sc->argnames[i]);
  free(sc->argnames);

  free(sc->strictin);
  free(sc);
}

void scomb_free_list(scomb **list)
{
  while (*list) {
    scomb *sc = *list;
    *list = sc->next;
    scomb_free(sc);
  }
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
      cleargraph(sc->body,FLAG_PROCESSED);
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
}

void shared_error(cell ***cells, int *ncells, cell *shared)
{
  fprintf(stderr,"Shared cell: ");
  print_codef(stderr,shared);
  fprintf(stderr,"\n");
  fprintf(stderr,"Present in supercombinators:\n");
  int i = 0;
  scomb *sc;
  for (sc = scombs; sc; sc = sc->next) {
    int j;
    for (j = 0; j < ncells[i]; j++) {
      if (cells[i][j] == shared)
        fprintf(stderr,"%s\n",sc->name);
    }
    i++;
  }
  abort();
}

void check_scombs_nosharing()
{
  int count = 0;
  scomb *sc;
  for (sc = scombs; sc; sc = sc->next)
    count++;

  cell ***cells = (cell***)calloc(count,sizeof(cell**));
  int *ncells = (int*)calloc(count,sizeof(cell**));
  int i = 0;
  for (sc = scombs; sc; sc = sc->next) {
    find_graph_cells(&cells[i],&ncells[i],sc->body);
    i++;
  }

  i = 0;
  for (sc = scombs; sc; sc = sc->next) {
    int j;
    for (j = 0; j < ncells[i]; j++)
      cells[i][j]->tag &= ~FLAG_PROCESSED;
    i++;
  }

  i = 0;
  for (sc = scombs; sc; sc = sc->next) {
    int j;
    for (j = 0; j < ncells[i]; j++) {
      /* we allow globnil to be shared - could it still cause problems? */
      if ((cells[i][j]->tag & FLAG_PROCESSED) && (cells[i][j] != globnil)) {
        shared_error(cells,ncells,cells[i][j]);
      }
      cells[i][j]->tag |= FLAG_PROCESSED;
    }
    i++;
  }

  for (i = 0; i < count; i++)
    free(cells[i]);
  free(cells);
  free(ncells);
}
