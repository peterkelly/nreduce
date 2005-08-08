/*
 * This file is part of the GridXSLT project
 * Copyright (C) 2005 Peter Kelly (pmk@cs.adelaide.edu.au)
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
 */

#include "list.h"
#include <stdlib.h>
#include <string.h>
#include <assert.h>

list *list_new(void *data, list *next)
{
  list *l = (list*)malloc(sizeof(list));
  l->data = data;
  l->next = next;
  return l;
}

list *copy_list(list *old)
{
  list *newstart = NULL;
  list **newptr = &newstart;
  while (old) {
    *newptr = (list*)malloc(sizeof(list));
    (*newptr)->data = old->data;
    (*newptr)->next = NULL;
    newptr = &((*newptr)->next);
    old = old->next;
  }
  return newstart;
}

void list_append(list **l, void *data)
{
  list **lptr = l;
  while (*lptr)
    lptr = &((*lptr)->next);
  *lptr = list_new(data,NULL);
}

void list_push(list **l, void *data)
{
  *l = list_new(data,*l);
}

void *list_pop(list **l)
{
  void *data;
  list *next;
  if (NULL == *l)
    return NULL;
  data = (*l)->data;
  next = (*l)->next;
  free(*l);
  *l = next;
  return data;
}

int list_count(list *l)
{
  int count = 0;
  for (; l; l = l->next)
    count++;
  return count;
}

void list_free(list *l, list_d_t d)
{
  while (l) {
    list *next = l->next;
    if (d)
      d(l->data);
    free(l);
    l = next;
  }
}

int list_contains_string(list *l, const char *str)
{
  for (; l; l = l->next)
    if (((NULL == str) && (NULL == l->data)) ||
        ((NULL != str) && (NULL != l->data) && !strcmp(str,(char*)l->data)))
      return 1;
  return 0;
}

int list_contains_ptr(list *l, const void *data)
{
  for (; l; l = l->next)
    if (l->data == data)
      return 1;
  return 0;
}

void list_remove_ptr(list **l, void *ptr)
{
  list *old;
  assert(list_contains_ptr(*l,ptr));
  while ((*l)->data != ptr)
    l = &((*l)->next);
  old = *l;
  *l = (*l)->next;
  free(old);
}

list *string_list_union(list *a, list *b)
{
  list *result = NULL;
  list *l;

  for (l = a; l; l = l->next) {
    char *str = (char*)l->data;
    if (!list_contains_string(result,str))
      list_append(&result,str ? strdup(str) : NULL);
  }

  for (l = b; l; l = l->next) {
    char *str = (char*)l->data;
    if (!list_contains_string(result,str))
      list_append(&result,str ? strdup(str) : NULL);
  }

  return result;
}

list *string_list_intersection(list *a, list *b)
{
  list *result = NULL;
  list *l;

  for (l = a; l; l = l->next) {
    char *str = (char*)l->data;
    if (!list_contains_string(result,str) && list_contains_string(b,str))
      list_append(&result,str ? strdup(str) : NULL);
  }

  return result;
}

