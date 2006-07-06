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

#include "grammar.tab.h"
#include "nreduce.h"
#include "gcode.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <assert.h>
#include <ctype.h>
#include <stdarg.h>

array *array_new()
{
  array *arr = (array*)malloc(sizeof(array));
  arr->alloc = 120;
  arr->size = 0;
  arr->data = malloc(arr->alloc);
  return arr;
}

int array_equals(array *a, array *b)
{
  return ((a->size == b->size) &&
          !memcmp(a->data,b->data,a->size));
}

void array_mkroom(array *arr, const int size)
{
  while (arr->size+size > arr->alloc) {
    arr->alloc *= 2;
    arr->data = realloc(arr->data,arr->alloc);
  }
}

void array_append(array *arr, const void *data, int size)
{
  array_mkroom(arr,size);
  memmove(arr->data+arr->size,data,size);
  arr->size += size;
}

void array_free(array *arr)
{
  free(arr->data);
  free(arr);
}

typedef struct setit {
  char *c;
  char *start;
  char old;
} setit;

setit new_setit(char *str)
{
  setit it;
  it.c = str+1;
  it.start = str+1;
  it.old = '\0';
  return it;
}

void cleanup_it(setit *it)
{
  if ('\0' != it->old)
    *it->c = it->old;
}

char *next_item(setit *it)
{
  if ('\0' != it->old) {
    *it->c = it->old;
    it->old = '\0';
    it->start = it->c+1;
    it->c++;
  }
  while ('\0' != *it->c) {
    if ((',' == *it->c) || ('}' == *it->c)) {
      if (it->start != it->c) {
        it->old = *it->c;
        *it->c = '\0';
        return it->start;
      }
    }
    it->c++;
  }
  return NULL;
}

int is_set(char *s)
{
  return ((2 <= strlen(s)) &&
          ('{' == s[0]) &&
          ('}' == s[strlen(s)-1]));
}

int set_contains(char *s, char *item)
{
  setit it = new_setit(s);
  char *check;
  while (NULL != (check = next_item(&it))) {
    if (!strcmp(check,item)) {
      cleanup_it(&it);
      return 1;
    }
  }
  cleanup_it(&it);
  return 0;
}

char *set_empty()
{
  return strdup("{}");
}

char *set_intersection(char *a, char *b)
{
  setit it;
  char *item;
  int count = 0;

  assert(is_set(a));
  assert(is_set(b));

  if (set_contains(a,"*"))
    return strdup(b);

  if (set_contains(b,"*"))
    return strdup(a);

  if (a == b)
    return strdup(a);

  char *res = (char*)malloc(strlen(a)+1);
  char *resptr = res;

  assert(is_set(a));
  assert(is_set(b));

  resptr += sprintf(resptr,"{");

  it = new_setit(a);
  while (NULL != (item = next_item(&it))) {
    if (set_contains(b,item)) {
      if (0 != count)
        resptr += sprintf(resptr,",");
      resptr += sprintf(resptr,"%s",item);
      count++;
    }
  }
  cleanup_it(&it);

  resptr += sprintf(resptr,"}");

  return res;
}

char *set_union(char *a, char *b)
{
  setit it;
  char *item;
  int count = 0;

  assert(is_set(a));
  assert(is_set(b));

  if (set_contains(a,"*") || set_contains(b,"*"))
    return strdup("{*}");

  if (a == b)
    return strdup(a);

  char *res = (char*)malloc(strlen(a)+strlen(b)+1);
  char *resptr = res;

  resptr += sprintf(resptr,"{");

  it = new_setit(a);
  while (NULL != (item = next_item(&it))) {
    if (0 != count)
      resptr += sprintf(resptr,",");
    resptr += sprintf(resptr,"%s",item);
    count++;
  }
  cleanup_it(&it);

  it = new_setit(b);
  while (NULL != (item = next_item(&it))) {
    if (!set_contains(a,item)) {
      if (0 != count)
        resptr += sprintf(resptr,",");
      resptr += sprintf(resptr,"%s",item);
      count++;
    }
  }
  cleanup_it(&it);

  resptr += sprintf(resptr,"}");

  return res;
}

list *list_new(void *data, list *next)
{
  list *l = (list*)malloc(sizeof(list));
  l->data = data;
  l->next = next;
  return l;
}

list *list_copy(list *orig, list_copy_t copy)
{
  list *newstart = NULL;
  list **newptr = &newstart;
  while (orig) {
    *newptr = (list*)malloc(sizeof(list));

    if (copy)
      (*newptr)->data = copy(orig->data);
    else
      (*newptr)->data = orig->data;
    (*newptr)->next = NULL;
    newptr = &((*newptr)->next);
    orig = orig->next;
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

void print_quoted_string(FILE *f, const char *str)
{
  fprintf(f,"\"");
  const char *c;
  for (c = str; *c; c++) {
    switch (*c) {
    case '\n':
      fprintf(f,"\\n");
      break;
    case '\r':
      fprintf(f,"\\r");
      break;
    case '\t':
      fprintf(f,"\\t");
      break;
    case '"':
      fprintf(f,"\\\"");
      break;
    default:
      fprintf(f,"%c",*c);
      break;
    }
  }
  fprintf(f,"\"");
}
