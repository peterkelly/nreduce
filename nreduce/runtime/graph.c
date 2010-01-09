/*
 * This file is part of the nreduce project
 * Copyright (C) 2006-2009 Peter Kelly (pmk@cs.adelaide.edu.au)
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

#include "src/nreduce.h"
#include "runtime.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <assert.h>
#include <ctype.h>
#include <stdarg.h>
#include <math.h>

pntrset *pntrset_new()
{
  pntrset *ps = (pntrset*)calloc(1,sizeof(pntrset));
  ps->count = 1;
  ps->alloc = 1024;
  ps->data = (psentry*)malloc(ps->alloc*sizeof(psentry));
  ps->data[0].p = NULL_PNTR;
  ps->data[0].next = 0;
  return ps;
}

void pntrset_free(pntrset *ps)
{
  free(ps->data);
  free(ps);
}

void pntrset_add(pntrset *ps, pntr p)
{
  unsigned char h = phash(p);
  if (ps->count == ps->alloc) {
    ps->alloc *= 2;
    ps->data = (psentry*)realloc(ps->data,ps->alloc*sizeof(psentry));
  }
  ps->data[ps->count].p = p;
  ps->data[ps->count].next = ps->map[h];
  ps->map[h] = ps->count;
  ps->count++;
}

int pntrset_contains(pntrset *ps, pntr p)
{
  unsigned char h = phash(p);
  int n = ps->map[h];
  while (n && !pntrequal(ps->data[n].p,p))
    n = ps->data[n].next;
  return (n != 0);
}

void pntrset_clear(pntrset *ps)
{
  memset(&ps->map,0,sizeof(int)*256);
  ps->count = 1;
}

pntrmap *pntrmap_new()
{
  pntrmap *pm = (pntrmap*)calloc(1,sizeof(pntrmap));
  pm->count = 1;
  pm->alloc = 1024;
  pm->data = (pmentry*)calloc(pm->alloc,sizeof(pmentry));
  pm->data[0].p = NULL_PNTR;
  pm->data[0].next = 0;
  return pm;
}

void pntrmap_free(pntrmap *pm)
{
  free(pm->data);
  free(pm);
}

int pntrmap_add(pntrmap *pm, pntr p, snode *s, char *name, pntr val)
{
  unsigned char h = phash(p);
  int r = pm->count;
  if (pm->count == pm->alloc) {
    pm->alloc *= 2;
    pm->data = (pmentry*)realloc(pm->data,pm->alloc*sizeof(pmentry));
  }
  pm->data[pm->count].p = p;
  pm->data[pm->count].next = pm->map[h];
  pm->data[pm->count].s = s;
  pm->data[pm->count].name = name;
  pm->data[pm->count].val = val;
  pm->map[h] = pm->count;
  pm->count++;
  return r;
}

int pntrmap_lookup(pntrmap *pm, pntr p)
{
  unsigned char h = phash(p);
  int n = pm->map[h];
  while (n && !pntrequal(pm->data[n].p,p))
    n = pm->data[n].next;
  return n;
}
