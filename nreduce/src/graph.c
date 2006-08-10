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

static void setneedclear_r(cell *c)
{
  if (c->tag & FLAG_NEEDCLEAR)
    return;
  c->tag |= FLAG_NEEDCLEAR;

  switch (celltype(c)) {
  case TYPE_APPLICATION:
  case TYPE_CONS:
    setneedclear_r((cell*)c->field1);
    setneedclear_r((cell*)c->field2);
    break;
  case TYPE_LAMBDA:
    setneedclear_r((cell*)c->field2);
    break;
  case TYPE_IND:
    setneedclear_r((cell*)c->field1);
    break;
  case TYPE_LETREC: {
    letrec *rec;
    for (rec = (letrec*)c->field1; rec; rec = rec->next)
      setneedclear_r(rec->value);
    setneedclear_r((cell*)c->field2);
    break;
  }
  }
}

static void cleargraph_r(cell *c, int flag)
{
  if (!(c->tag & FLAG_NEEDCLEAR))
    return;
  c->tag &= ~FLAG_NEEDCLEAR;
  c->tag &= ~flag;

  switch (celltype(c)) {
  case TYPE_APPLICATION:
  case TYPE_CONS:
    cleargraph_r((cell*)c->field1,flag);
    cleargraph_r((cell*)c->field2,flag);
    break;
  case TYPE_LAMBDA:
    cleargraph_r((cell*)c->field2,flag);
    break;
  case TYPE_IND:
    cleargraph_r((cell*)c->field1,flag);
    break;
  case TYPE_LETREC: {
    letrec *rec;
    for (rec = (letrec*)c->field1; rec; rec = rec->next)
      cleargraph_r(rec->value,flag);
    cleargraph_r((cell*)c->field2,flag);
    break;
  }
  }
}

void cleargraph(cell *root, int flag)
{
  setneedclear_r(root);
  cleargraph_r(root,flag);
}

static void add_cells(cell ***cells, int *ncells, cell *c, int *alloc)
{
  if ((*ncells) == *alloc) {
    (*alloc) *= 2;
    (*cells) = (cell**)realloc((*cells),(*alloc)*sizeof(cell*));
  }
  (*cells)[(*ncells)++] = c;

  switch (celltype(c)) {
  case TYPE_APPLICATION:
  case TYPE_CONS:
    add_cells(cells,ncells,(cell*)c->field1,alloc);
    add_cells(cells,ncells,(cell*)c->field2,alloc);
    break;
  case TYPE_LAMBDA:
    add_cells(cells,ncells,(cell*)c->field2,alloc);
    break;
  case TYPE_LETREC: {
    letrec *rec;
    for (rec = (letrec*)c->field1; rec; rec = rec->next)
      add_cells(cells,ncells,rec->value,alloc);
    add_cells(cells,ncells,(cell*)c->field2,alloc);
    break;
  }
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

void find_graph_cells(cell ***cells, int *ncells, cell *root)
{
  int alloc = 4;
  *ncells = 0;
  *cells = (cell**)malloc(alloc*sizeof(cell*));
  add_cells(cells,ncells,root,&alloc);
}
