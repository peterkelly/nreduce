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
 * $Id: util.h 613 2007-08-23 11:40:12Z pmkelly $
 *
 */

#ifndef _UTIL_H
#define _UTIL_H

#include <stdarg.h>
#include <stdio.h>
#include <pthread.h>
#include <sys/time.h>

#define STACK_LIMIT 10240

typedef struct list list;

////list like cons:  list = (data, pointer), 
////using (void *) for the data type to implement typeless data, so list can hold any type of data
struct list {
  void *data;
  list *next;
};

//// array of any type of data
typedef struct array {
  int elemsize;  //// the size for each element
  int alloc;     //// the total allocated space
  int nbytes;    //// used space
  char *data;    //// actual data
} array;

typedef struct stack {
  int alloc;
  int count;
  void **data;
} stack;

void fatal(const char *format, ...);

array *array_new(int elemsize, int initroom);
void array_mkroom(array *arr, const int size);
void array_append(array *arr, const void *data, int size);
#define array_item(_arr,_index,_type) (((_type*)(_arr)->data)[(_index)])
int array_count(array *arr);
void array_free(array *arr);

//// function pointer, it specifys the pointer to function list_d_t, which accept (void *) argument
//// and returns (void *)
typedef void (*list_d_t)(void *a);

list *list_new(void *data, list *next);
void list_append(list **l, void *data);
void list_push(list **l, void *data);
void *list_pop(list **l);
int list_count(list *l);
void list_free(list *l, list_d_t d);

int list_contains_string(list *l, const char *str);
int list_contains_ptr(list *l, const void *data);
void list_remove_ptr(list **l, void *ptr);

stack *stack_new(void);
void stack_free(stack *s);
void stack_push(stack *s, void *c);

void format_double(char *str, int size, double d);

int hash(const void *mem, int size);

#endif
