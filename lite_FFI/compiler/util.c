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
 * $Id: util.c 613 2007-08-23 11:40:12Z pmkelly $
 *
 */

#define _GNU_SOURCE /* for PTHREAD_ERRORCHECK_MUTEX_INITIALIZER_NP */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "src/nreduce.h"
#include "util.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <assert.h>
#include <ctype.h>
#include <stdarg.h>
#include <math.h>
#include <netdb.h>
#include <netinet/tcp.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <unistd.h>
#include <errno.h>
#include <fenv.h>

void fatal(const char *format, ...)
{
  va_list ap;
  fprintf(stderr,"fatal error: ");
  va_start(ap,format);
  vfprintf(stderr,format,ap);
  va_end(ap);
  fprintf(stderr,"\n");
  abort();
}

//// create a new array
//// Return: the pointer to the newly created array
array *array_new(int elemsize, int initroom)
{
  array *arr = (array*)malloc(sizeof(array));
  arr->elemsize = elemsize;
  if (0 < initroom)
    arr->alloc = elemsize*initroom;
  else
    arr->alloc = 1024;   //// default allocate 1024 bytes
  arr->nbytes = 0;       //// no data by default
  arr->data = malloc(arr->alloc);
  return arr;
}

//// increase the array if the total size of elements to be added exceeds the allocated space of arr.
void array_mkroom(array *arr, const int size)
{
  if (arr->nbytes+size > arr->alloc) {
    while (arr->nbytes+size > arr->alloc)
      arr->alloc *= 2;    //// double the allocated space each time
    arr->data = realloc(arr->data,arr->alloc);
  }
}

//// append (data) to the (arr), and renew the array parameters
void array_append(array *arr, const void *data, int size)
{
  array_mkroom(arr,size);	//// increase the array IF size is larger than the available space
           //// (address of arr->data) + (the used space) = the available address
  memmove((char*)(arr->data)+arr->nbytes, data, size);
  arr->nbytes += size;
}

//// count the number of elements in (arr)
int array_count(array *arr)
{
  return arr->nbytes/arr->elemsize;
}

//// free (arr)
void array_free(array *arr)
{
  free(arr->data);
  free(arr);
}

//// creat a new list: arguments: pointer to the data, pointer to next list
//// Return: the pointer to the newly created list
list *list_new(void *data, list *next)
{
  list *l = (list*)malloc(sizeof(list));
  l->data = data;
  l->next = next;
  return l;
}

//// append data to the end of the list l.
//// (data1 ->next) (data2 ->next) will be (data1 ->next) (data2 ->next) (data ->next)
void list_append(list **l, void *data)
{
  list **lptr = l;
  while (*lptr)
    lptr = &((*lptr)->next);
  *lptr = list_new(data,NULL);
}

//// push data to the top of the given list **l
//// (data1 ->next (data2 ->next), will be (data ->next (data1 ->next) (data2 ->next))
void list_push(list **l, void *data)
{
  *l = list_new(data,*l);
}

//// pop the 1st data in the list:
//// (data1 ->next (data2 ->next) (data3 ->next)), will pop data1.
void *list_pop(list **l)
{
  void *data;
  list *next;
  if (*l == NULL)
    return NULL;
  data = (*l)->data;
  next = (*l)->next;
  free(*l);
  *l = next;
  return data;
}

//// Return the number of elements in the list l
int list_count(list *l)
{
  int count = 0;
  for (; l; l = l->next)
    count++;
  return count;
}

////
void list_free(list *l, list_d_t d) /* Can be called from native code */
{
  while (l) {
    list *next = l->next;
    if (d)
      d(l->data);
    free(l);
    l = next;
  }
}

//// test if the given string str is contained in the list data.
//// Return 1 indicates the str is in list l, 0 otherwise
int list_contains_string(list *l, const char *str)
{
  for (; l; l = l->next)
    if (((NULL == str) && (NULL == l->data)) ||
        ((NULL != str) && (NULL != l->data) && !strcmp(str,(char*)l->data)))
      return 1;
  return 0;
}

//// test if the given data pointer (data) in in the list l.
//// Return 1 if l contians data pointer, 0 otherwise.
int list_contains_ptr(list *l, const void *data)
{
  for (; l; l = l->next)
    if (l->data == data)
      return 1;
  return 0;
}

//// remove the specified data pointer from the list l
void list_remove_ptr(list **l, void *ptr)
{
  list *old;
  assert(list_contains_ptr(*l,ptr));  ////make sure the data pointer is contained in the list
  while ((*l)->data != ptr)
    l = &((*l)->next);
  old = *l;
  *l = (*l)->next;
  free(old);
}

//// create new stack to store pointers (no type is specified, void *)
stack *stack_new(void)
{
  stack *s = (stack*)calloc(1,sizeof(stack));
  s->alloc = 1;
  s->count = 0;
  s->data = (void**)malloc(s->alloc*sizeof(void*));
  return s;
}

//// free the stack (s), by free its data first, and then the stack itself
void stack_free(stack *s)
{
  free(s->data);
  free(s);
}

//// push c into the stack (s)
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

//// get the sign of the number (d), '+' or '-'
static int getsignbit(double d)
{
  char tmp[100];
  sprintf(tmp,"%f",d);
  return ('-' == tmp[0]);
}

void format_double(char *str, int size, double d)
{
  if (isnan(d)) {	/// isnan: is Not a Number
    snprintf(str,size,"NaN");
  }
  else if (isinf(d) && (d > 0.0)) {	//// isinf: is infinity?
    snprintf(str,size,"INF");
  }
  else if (isinf(d) && (d < 0.0)) {
    snprintf(str,size,"-INF");
  }
  else if (d == floor(d)) {
    if ((0.0 == d) && (getsignbit(d) && (0.0 == d)))
      snprintf(str,size,"-0");
    else
      snprintf(str,size,"%.0f",d);
  }
  else if ((0.000001 < fabs(d)) && (fabs(d) < 1000000.0)) {
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

    snprintf(str,size,"%d.%s",ipart,tmp+start+2);
  }
  else if (0.0 == d) {
    snprintf(str,size,"0");
  }
  else {
    snprintf(str,size,"%f",d);
  }
}


//// hash function
//// (mem) could be a string, like "main", size could be strlen(main)
int hash(const void *mem, int size)
{
  int h = 0;
  int i;
  for (i = 0; i < size; i++)
    h += ((char*)mem)[i];
  h %= GLOBAL_HASH_SIZE;
  if (0 > h)
    h += GLOBAL_HASH_SIZE;
  return h;
}
