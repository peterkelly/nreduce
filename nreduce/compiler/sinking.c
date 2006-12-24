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
 * $Id: renaming.c 343 2006-10-02 12:59:30Z pmkelly $
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

void remove_wrappers(snode *s)
{
  while (SNODE_WRAP == s->type) {
    snode *target = s->target;
    memcpy(s,target,sizeof(snode));
    free(target);
  }

  switch (s->type) {
  case SNODE_APPLICATION:
    remove_wrappers(s->left);
    remove_wrappers(s->right);
    break;
  case SNODE_LAMBDA:
    remove_wrappers(s->body);
    break;
  case SNODE_LETREC: {
    letrec *rec;
    for (rec = s->bindings; rec; rec = rec->next)
      remove_wrappers(rec->value);
    remove_wrappers(s->body);
    break;
  }
  default:
    break;
  }
}

static snode *highest_user(snode *s, const char *sym)
{
  switch (s->type) {
  case SNODE_APPLICATION: {
    snode *hu1 = highest_user(s->left,sym);
    snode *hu2 = highest_user(s->right,sym);
    if (hu1 && hu2)
      return s;
    else if (hu1)
      return hu1;
    else
      return hu2;
  }
  case SNODE_LAMBDA:
    return highest_user(s->body,sym);
  case SNODE_WRAP:
    return highest_user(s->target,sym);
  case SNODE_LETREC: {
    snode *hu = NULL;
    snode *tmp;
    letrec *rec;
    for (rec = s->bindings; rec; rec = rec->next) {
      if (NULL != (tmp = highest_user(rec->value,sym))) {
        if (hu || (tmp == rec->value))
          return s;
        else
          hu = tmp;
      }
    }
    if (NULL != (tmp = highest_user(s->body,sym))) {
      if (hu || (tmp == s->body))
        return s;
      else
        return tmp;
    }
    return hu;
  }
  case SNODE_SYMBOL:
    if (!strcmp(s->name,sym))
      return s;
    break;
  default:
    break;
  }
  return NULL;
}

static void sink_single(source *src, snode *s)
{
  letrec **recptr;
  assert(SNODE_LETREC == s->type);
  recptr = &s->bindings;
  while (*recptr) {
    snode *hu;
    hu = highest_user(s,(*recptr)->name);

    if (hu != s) {
      letrec **otherpntr;
      letrec *thisrec;

      assert(SNODE_WRAP != hu->type);
      if (SNODE_LETREC != hu->type) {
        snode *body = snode_new(-1,-1);
        memcpy(body,hu,sizeof(snode));
        memset(hu,0,sizeof(snode));
        hu->type = SNODE_LETREC;
        hu->body = body;
      }

      otherpntr = &hu->bindings;
      while (*otherpntr)
        otherpntr = &((*otherpntr)->next);

      thisrec = *recptr;
      *recptr = thisrec->next;
      thisrec->next = NULL;
      *otherpntr = thisrec;
    }
    else {
      recptr = &((*recptr)->next);
    }
  }
  if (NULL == s->bindings) {
    s->type = SNODE_WRAP;
    s->target = s->body;
    s->body = NULL;
  }
}

static void sink_letrecs_r(source *src, snode *s)
{
  switch (s->type) {
  case SNODE_APPLICATION:
    sink_letrecs_r(src,s->left);
    sink_letrecs_r(src,s->right);
    break;
  case SNODE_LAMBDA:
    sink_letrecs_r(src,s->body);
    break;
  case SNODE_LETREC: {
    letrec *rec;
    for (rec = s->bindings; rec; rec = rec->next)
      sink_letrecs_r(src,rec->value);
    sink_letrecs_r(src,s->body);
    sink_single(src,s);
    break;
  }
  default:
    break;
  }
}

void sink_letrecs(source *src, snode *s)
{
  sink_letrecs_r(src,s);
  remove_wrappers(s);
}
