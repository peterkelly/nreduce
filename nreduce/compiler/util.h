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

#ifndef _UTIL_H
#define _UTIL_H

#include <stdarg.h>
#include <pthread.h>

typedef struct list list;
struct list {
  void *data;
  list *next;
};

typedef struct array {
  int elemsize;
  int alloc;
  int nbytes;
  char *data;
} array;

typedef struct stack {
  int alloc;
  int count;
  void **data;
} stack;

array *array_new(int elemsize);
int array_equals(array *a, array *b);
void array_mkroom(array *arr, const int size);
void array_append(array *arr, const void *data, int size);
void array_vprintf(array *arr, const char *format, va_list ap);
void array_printf(array *arr, const char *format, ...);
void array_remove_data(array *arr, int nbytes);
void array_remove_items(array *arr, int count);
#define array_item(_arr,_index,_type) (((_type*)(_arr)->data)[(_index)])
int array_count(array *arr);
void array_free(array *arr);

#define llist_prepend(_ll,_obj) {                         \
    assert(!(_obj)->prev && !(_obj)->next);               \
    if ((_ll)->first) {                                   \
      (_obj)->next = (_ll)->first;                        \
      (_ll)->first->prev = (_obj);                        \
      (_ll)->first = (_obj);                              \
    }                                                     \
    else {                                                \
      (_ll)->first = (_ll)->last = (_obj);                \
    }                                                     \
  }

#define llist_append(_ll,_obj) {                          \
    assert(!(_obj)->prev && !(_obj)->next);               \
    if ((_ll)->last) {                                    \
      (_obj)->prev = (_ll)->last;                         \
      (_ll)->last->next = (_obj);                         \
      (_ll)->last = (_obj);                               \
    }                                                     \
    else {                                                \
      (_ll)->first = (_ll)->last = (_obj);                \
    }                                                     \
  }

#define llist_remove(_ll,_obj) {                          \
    assert((_obj)->prev || ((_ll)->first == (_obj)));     \
    assert((_obj)->next || ((_ll)->last == (_obj)));      \
    if ((_ll)->first == (_obj))                           \
      (_ll)->first = (_obj)->next;                        \
    if ((_ll)->last == (_obj))                            \
      (_ll)->last = (_obj)->prev;                         \
    if ((_obj)->next)                                     \
      (_obj)->next->prev = (_obj)->prev;                  \
    if ((_obj)->prev)                                     \
      (_obj)->prev->next = (_obj)->next;                  \
    (_obj)->next = NULL;                                  \
    (_obj)->prev = NULL;                                  \
  }

typedef void (*list_d_t)(void *a);
typedef void* (*list_copy_t)(void *a);

list *list_new(void *data, list *next);
list *list_copy(list *orig, list_copy_t copy);
void list_append(list **l, void *data);
void list_push(list **l, void *data);
void *list_pop(list **l);
int list_count(list *l);
void *list_item(list *l, int item);
void list_free(list *l, list_d_t d);

int list_contains_string(list *l, const char *str);
int list_contains_ptr(list *l, const void *data);
void list_remove_ptr(list **l, void *ptr);

stack *stack_new(void);
void stack_free(stack *s);
void stack_push(stack *s, void *c);

void format_double(char *str, int size, double d);
void print_double(FILE *f, double d);
void print_escaped(FILE *f, const char *str);
void print_double_escaped(FILE *f, const char *str);
void print_hex(FILE *f, int c);
void print_hexbyte(FILE *f, unsigned char val);
void print_bin(FILE *f, void *ptr, int nbytes);
void print_bin_rev(FILE *f, void *ptr, int nbytes);

struct timeval timeval_diff(struct timeval from, struct timeval to);
int timeval_diffms(struct timeval from, struct timeval to);

int hash(const void *mem, int size);
char *getcwd_alloc();

void parse_cmdline(const char *line, int *argc, char ***argv);
void free_args(int argc, char **argv);

int wrap_pthread_create(pthread_t *thread, pthread_attr_t *attr,
                        void *(*start_routine)(void *), void *arg);
int wrap_pthread_join(pthread_t th, void **thread_return);

#endif
