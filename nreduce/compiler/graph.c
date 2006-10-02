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

#include "src/nreduce.h"
#include "source.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <assert.h>
#include <ctype.h>
#include <stdarg.h>
#include <math.h>

static void setneedclear_r(snode *c)
{
  if (c->tag & FLAG_NEEDCLEAR)
    return;
  c->tag |= FLAG_NEEDCLEAR;

  switch (snodetype(c)) {
  case TYPE_APPLICATION:
  case TYPE_CONS:
    setneedclear_r(c->left);
    setneedclear_r(c->right);
    break;
  case TYPE_LAMBDA:
    setneedclear_r(c->body);
    break;
  case TYPE_IND:
    abort();
    break;
  case TYPE_LETREC: {
    letrec *rec;
    for (rec = c->bindings; rec; rec = rec->next)
      setneedclear_r(rec->value);
    setneedclear_r(c->body);
    break;
  }
  }
}

static void cleargraph_r(snode *c, int flag)
{
  if (!(c->tag & FLAG_NEEDCLEAR))
    return;
  c->tag &= ~FLAG_NEEDCLEAR;
  c->tag &= ~flag;

  switch (snodetype(c)) {
  case TYPE_APPLICATION:
  case TYPE_CONS:
    cleargraph_r(c->left,flag);
    cleargraph_r(c->right,flag);
    break;
  case TYPE_LAMBDA:
    cleargraph_r(c->body,flag);
    break;
  case TYPE_IND:
    abort();
    break;
  case TYPE_LETREC: {
    letrec *rec;
    for (rec = c->bindings; rec; rec = rec->next)
      cleargraph_r(rec->value,flag);
    cleargraph_r(c->body,flag);
    break;
  }
  }
}

void cleargraph(snode *root, int flag)
{
  setneedclear_r(root);
  cleargraph_r(root,flag);
}

