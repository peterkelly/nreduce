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
  memmove((char*)(arr->data)+arr->size,data,size);
  arr->size += size;
}

void array_free(array *arr)
{
  free(arr->data);
  free(arr);
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

void *list_item(list *l, int item)
{
  while (0 < item) {
    assert(l->next);
    l = l->next;
    item--;
  }
  return l->data;
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
  const char *c;
  fprintf(f,"\"");
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

void parse_check(int cond, snode *c, char *msg)
{
  if (cond)
    return;
  if (0 <= c->sl.fileno)
    fprintf(stderr,"%s:%d: ",lookup_parsedfile(c->sl.fileno),c->sl.lineno);
  fprintf(stderr,"%s\n",msg);
  exit(1);
}

void print_sourceloc(FILE *f, sourceloc sl)
{
  if (0 <= sl.fileno)
    fprintf(stderr,"%s:%d: ",lookup_parsedfile(sl.fileno),sl.lineno);
}

stack *stack_new(void)
{
  stack *s = (stack*)calloc(1,sizeof(stack));
  s->alloc = 1;
  s->count = 0;
  s->data = (void**)malloc(s->alloc*sizeof(void*));
  return s;
}

void stack_free(stack *s)
{
  free(s->data);
  free(s);
}

void stack_push(stack *s, void *c)
{
  if (s->count == s->alloc) {
    if (s->count >= STACK_LIMIT) {
      fprintf(stderr,"Out of stack space\n");
      exit(1);
    }
    s->alloc *= 2;
    s->data = (void**)realloc(s->data,s->alloc*sizeof(void*));
  }
  s->data[s->count++] = c;
}

int getsignbit(double d)
{
  char tmp[100];
  sprintf(tmp,"%f",d);
  return ('-' == tmp[0]);
}

void print_double(FILE *f, double d)
{
  if (d == (double)((int)(d))) {
    int i = (int)d;
    if (getsignbit(d) && (0.0 == d))
      printf("-0");
    else
      printf("%d",i);
  }
  else if ((0.000001 < fabs(d)) && (1000000.0 > fabs(d))) {
    int ipart = (int)d;
    double fraction;
	int start;
    char tmp[100];
	int pos;

    if (0.0 < d)
      fraction = d - floor(d);
    else
      fraction = d - ceil(d);

    start = getsignbit(d) ? 1 : 0;

    sprintf(tmp,"%f",fraction);
    pos = strlen(tmp)-1;
    while ((2+start < pos) && ('0' == tmp[pos]))
      tmp[pos--] = '\0';
    assert('0' == tmp[start]);
    assert('.' == tmp[start+1]);

    printf("%d.%s",ipart,tmp+start+2);
  }
  else if (0.0 == d) {
    printf("0");
  }
  else if (isnan(d)) {
    printf("NaN");
  }
  else if (isinf(d) && (0.0 < d)) {
    printf("INF");
  }
  else if (isinf(d) && (0.0 > d)) {
    printf("-INF");
  }
  else {
    printf("%f",d);
  }
}

