/*
 * This file is part of the nreduce project
 * Copyright (C) 2006-2007 Peter Kelly (pmk@cs.adelaide.edu.au)
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
#include "source.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <assert.h>
#include <ctype.h>
#include <stdarg.h>
#include <math.h>

static void set_parents(snode *s, snode *parent)
{
  s->parent = parent;
  switch (s->type) {
  case SNODE_APPLICATION:
    set_parents(s->left,s);
    set_parents(s->right,s);
    break;
  case SNODE_LAMBDA:
    set_parents(s->body,s);
    break;
  case SNODE_WRAP:
    set_parents(s->target,s);
    break;
  case SNODE_LETREC: {
    letrec *rec;
    for (rec = s->bindings; rec; rec = rec->next)
      set_parents(rec->value,s);
    set_parents(s->body,s);
    break;
  }
  case SNODE_SYMBOL:
    break;
  default:
    break;
  }
}

static letrec *find_binding(snode *s)
{
  snode *p;
  letrec *rec;
  for (p = s->parent; p; p = p->parent)
    if (SNODE_LETREC == p->type)
      for (rec = p->bindings; rec; rec = rec->next)
        if (!strcmp(rec->name,s->name))
          return rec;
  return NULL;
}

static snode *common_parent(snode *a, snode *b)
{
  snode *x;
  snode *y;
  for (y = b; y; y = y->parent)
    for (x = a; x; x = x->parent)
      if (x == y)
        return x;

  return NULL;
}

static void find_users(snode *s)
{
  switch (s->type) {
  case SNODE_APPLICATION:
    find_users(s->left);
    find_users(s->right);
    break;
  case SNODE_LAMBDA:
    find_users(s->body);
    break;
  case SNODE_WRAP:
    find_users(s->target);
    break;
  case SNODE_LETREC: {
    letrec *rec;
    for (rec = s->bindings; rec; rec = rec->next)
      find_users(rec->value);
    find_users(s->body);
    break;
  }
  case SNODE_SYMBOL: {
    letrec *rec;
    if (NULL != (rec = find_binding(s))) {
      s->nextuser = rec->users;
      rec->users = s;
    }
    break;
  }
  default:
    break;
  }
}

void remove_wrappers(snode *s)
{
  s->parent = NULL;
  s->nextuser = NULL;

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
    for (rec = s->bindings; rec; rec = rec->next) {
      rec->users = NULL;
      remove_wrappers(rec->value);
    }
    remove_wrappers(s->body);
    break;
  }
  default:
    break;
  }
}

static void sink_single(source *src, snode *s, int *changed)
{
  letrec **recptr;
  assert(SNODE_LETREC == s->type);
  recptr = &s->bindings;
  while (*recptr) {
    snode *user;
    snode *hu = NULL;
    int nusers = 0;

    if ((*recptr)->strict) {
      recptr = &((*recptr)->next);
      continue;
    }

    if ((*recptr)->users) {
      hu = (*recptr)->users;
      nusers = 1;
      for (user = (*recptr)->users->nextuser; user; user = user->nextuser) {
        hu = common_parent(user,hu);
        nusers++;
      }
    }

    while (hu && hu->parent && 
           ((SNODE_LETREC == hu->parent->type) || (SNODE_WRAP == hu->parent->type)))
      hu = hu->parent;

    while (hu && (SNODE_WRAP == hu->type))
      hu = hu->target;

    if (1 == nusers) {
      letrec *rec = *recptr;
      snode *u = rec->users;
      assert(SNODE_SYMBOL == u->type);
      free(u->name);
      u->type = SNODE_WRAP;
      u->target = rec->value;
      *recptr = rec->next;
      free(rec->name);
      free(rec);
    }
    else if (hu && (hu != s)) {
      letrec **otherpntr;
      letrec *thisrec;
      *changed = 1;

      assert(SNODE_WRAP != hu->type);
      if (SNODE_LETREC != hu->type) {
        snode *body = snode_new(-1,-1);
        memcpy(body,hu,sizeof(snode));

        switch (body->type) {
        case SNODE_APPLICATION:
          body->left->parent = body;
          body->right->parent = body;
          break;
        case SNODE_LAMBDA:
          body->body->parent = body;
          break;
        case SNODE_WRAP:
          body->target->parent = body;
          break;
        default:
          break;
        }

        memset(hu,0,sizeof(snode));
        hu->type = SNODE_LETREC;
        hu->body = body;
        hu->parent = body->parent;
        body->parent = hu;
      }

      otherpntr = &hu->bindings;
      while (*otherpntr)
        otherpntr = &((*otherpntr)->next);

      thisrec = *recptr;
      *recptr = thisrec->next;
      thisrec->next = NULL;
      *otherpntr = thisrec;

      thisrec->value->parent = hu;
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

static void sink_letrecs_r(source *src, snode *s, int *changed)
{
  switch (s->type) {
  case SNODE_APPLICATION:
    sink_letrecs_r(src,s->left,changed);
    sink_letrecs_r(src,s->right,changed);
    break;
  case SNODE_LAMBDA:
    sink_letrecs_r(src,s->body,changed);
    break;
  case SNODE_LETREC: {
    letrec *rec;
    for (rec = s->bindings; rec; rec = rec->next)
      sink_letrecs_r(src,rec->value,changed);
    sink_letrecs_r(src,s->body,changed);
    sink_single(src,s,changed);
    break;
  }
  default:
    break;
  }
}

void sink_letrecs(source *src, snode *s)
{
  int changed;
  do {
    changed = 0;
    set_parents(s,NULL);
    find_users(s);
    sink_letrecs_r(src,s,&changed);
    remove_wrappers(s);
  } while (changed);
}
