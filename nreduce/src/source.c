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
 * $Id: memory.c 326 2006-08-22 06:11:26Z pmkelly $
 *
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "grammar.tab.h"
#include "nreduce.h"
#include "gcode.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <assert.h>
#include <ctype.h>
#include <stdarg.h>
#include <math.h>

snode *snode_new(int fileno, int lineno)
{
  snode *c = calloc(1,sizeof(snode));
  c->sl.fileno = fileno;
  c->sl.lineno = lineno;
  return c;
}

void free_letrec(letrec *rec)
{
  free(rec->name);
  free(rec);
}

void snode_free(snode *c)
{
  if (NULL == c)
    return;
  switch (snodetype(c)) {
  case TYPE_APPLICATION:
  case TYPE_CONS:
    snode_free(c->left);
    snode_free(c->right);
    break;
  case TYPE_LAMBDA:
    snode_free(c->body);
    break;
  case TYPE_LETREC: {
    letrec *rec = c->bindings;
    while (rec) {
      letrec *next = rec->next;
      snode_free(rec->value);
      free_letrec(rec);
      rec = next;
    }
    snode_free(c->body);
    break;
  }
  case TYPE_BUILTIN:
  case TYPE_SYMBOL:
  case TYPE_SCREF:
  case TYPE_NIL:
  case TYPE_NUMBER:
  case TYPE_STRING:
    break;
  default:
    abort();
  }
  free(c->value);
  free(c->name);
  free(c);
}

const char *lookup_parsedfile(int fileno)
{
  assert(parsedfiles);
  assert(0 <= fileno);
  assert(fileno < array_count(parsedfiles));
  return array_item(parsedfiles,fileno,char*);
}

int add_parsedfile(const char *filename)
{
  char *copy = strdup(filename);
  if (NULL == parsedfiles)
    parsedfiles = array_new(sizeof(char*));
  array_append(parsedfiles,&copy,sizeof(char*));
  return array_count(parsedfiles)-1;
}

