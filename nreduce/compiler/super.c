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
  scomb *sc = src->scombs;
  while (0 < index) {
    index--;
    sc = sc->next;
  }
  return sc;
}

scomb *get_scomb(source *src, const char *name)
{
  scomb *sc;
  for (sc = src->scombs; sc; sc = sc->next)
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

scomb *add_scomb(source *src, const char *name1)
{
  char *name = strdup(name1);
  char *hashpos = strchr(name,'#');
  scomb *sc;
  int num;

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

  *src->lastsc = sc;
  src->lastsc = &sc->next;

  sc->sl.fileno = -1;
  sc->sl.lineno = -1;

  free(name);
  return sc;
}

static void scomb_free(scomb *sc)
{
  int i;
  snode_free(sc->body);
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

/* Note: this is not actually used at the moment */
void fix_partial_applications(source *src)
{
  scomb *sc;
  for (sc = src->scombs; sc; sc = sc->next) {
    snode *c = sc->body;
    int nargs = 0;
    scomb *other;
    while (TYPE_APPLICATION == snodetype(c)) {
      c = c->left;
      nargs++;
    }
    if (TYPE_SCREF == snodetype(c)) {
      other = c->sc;
      if (nargs < other->nargs) {
        int oldargs = sc->nargs;
        int extra = other->nargs - nargs;
        int i;
        sc->nargs += extra;
        sc->argnames = (char**)realloc(sc->argnames,sc->nargs*sizeof(char*));
        for (i = oldargs; i < sc->nargs; i++) {
          snode *fun = sc->body;
          snode *arg = snode_new(-1,-1);

          sc->argnames[i] = (char*)malloc(20);
          sprintf(sc->argnames[i],"N%d",src->genvar++);

          arg->tag = TYPE_SYMBOL;
          arg->name = strdup(sc->argnames[i]);

          sc->body = snode_new(-1,-1);
          sc->body->tag = TYPE_APPLICATION;
          sc->body->left = fun;
          sc->body->right = arg;
        }
      }
    }
  }
}

static void shared_error(source *src, snode ***nodes, int *nnodes, snode *shared)
{
  int i = 0;
  scomb *sc;
  fprintf(stderr,"Shared node: ");
  print_codef(src,stderr,shared);
  fprintf(stderr,"\n");
  fprintf(stderr,"Present in supercombinators:\n");
  for (sc = src->scombs; sc; sc = sc->next) {
    int j;
    for (j = 0; j < nnodes[i]; j++) {
      if (nodes[i][j] == shared)
        fprintf(stderr,"%s\n",sc->name);
    }
    i++;
  }
  abort();
}

void check_scombs_nosharing(source *src)
{
  int count = 0;
  scomb *sc;
  snode ***nodes;
  int *nnodes;
  int i;

  for (sc = src->scombs; sc; sc = sc->next)
    count++;

  nodes = (snode***)calloc(count,sizeof(snode**));
  nnodes = (int*)calloc(count,sizeof(snode**));
  i = 0;
  for (sc = src->scombs; sc; sc = sc->next) {
    find_snodes(&nodes[i],&nnodes[i],sc->body);
    i++;
  }

  /* FIXME: avoid use of FLAG_PROCESSED here; store the "found bit" in a
     temporary array. This is the only plcae in the code FLAG_PROCESSED is still
     used, so it can be removed after that. */

  i = 0;
  for (sc = src->scombs; sc; sc = sc->next) {
    int j;
    for (j = 0; j < nnodes[i]; j++)
      nodes[i][j]->tag &= ~FLAG_PROCESSED;
    i++;
  }

  i = 0;
  for (sc = src->scombs; sc; sc = sc->next) {
    int j;
    for (j = 0; j < nnodes[i]; j++) {
      if (nodes[i][j]->tag & FLAG_PROCESSED)
        shared_error(src,nodes,nnodes,nodes[i][j]);
      nodes[i][j]->tag |= FLAG_PROCESSED;
    }
    i++;
  }

  for (i = 0; i < count; i++)
    free(nodes[i]);
  free(nodes);
  free(nnodes);
}
