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
  return array_item(src->scarr,index,scomb*);
}

scomb *get_scomb(source *src, const char *name)
{
  int scno;
  for (scno = 0; scno < array_count(src->scarr); scno++) {
    scomb *sc = array_item(src->scarr,scno,scomb*);
    if (!strcmp(sc->name,name))
      return sc;
  }
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

  array_append(src->scarr,&sc,sizeof(scomb*));

  sc->sl.fileno = -1;
  sc->sl.lineno = -1;

  free(name);
  return sc;
}

void scomb_free(scomb *sc)
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

