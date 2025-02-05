/*
 * This file is part of the NReduce project
 * Copyright (C) 2006-2010 Peter Kelly <kellypmk@gmail.com>
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
#include <stdio.h>
#include <pthread.h>
#include <netdb.h>
#include <sys/time.h>
#include "malloc.h"

#define STACK_LIMIT 10240

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
  int limit;
  void **data;
} stack;

void fatal(const char *format, ...);

int min(int a, int b);
int max(int a, int b);

array *array_new(int elemsize, int initroom);
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
void *stack_pop(stack *s);

void format_double(char *str, int size, double d);
void print_double(FILE *f, double d);
void print_escaped(FILE *f, const char *str);
void print_double_escaped(FILE *f, const char *str);
char *unescape(const char *chars);
char *escape(const char *chars);
char *mkstring(const char *data, int len);
char *substring(const char *str, int begin, int end);

struct timeval timeval_diff(struct timeval from, struct timeval to);
int timeval_diffms(struct timeval from, struct timeval to);
struct timeval timeval_addms(struct timeval t, int ms);

int hash(const void *mem, int size);

void parse_cmdline(const char *line, int *argc, char ***argv);
void free_args(int argc, char **argv);

int parse_address(const char *address, char **host, int *port);

void init_mutex(pthread_mutex_t *mutex);
void destroy_mutex(pthread_mutex_t *mutex);
int check_mutex_locked(pthread_mutex_t *mutex);
int check_mutex_unlocked(pthread_mutex_t *mutex);
void lock_mutex(pthread_mutex_t *mutex);
void unlock_mutex(pthread_mutex_t *mutex);

void enable_invalid_fpe();

int set_non_blocking(int fd);
char *lookup_hostname(in_addr_t addr);
int lookup_address(const char *host, in_addr_t *out, int *h_errout);
int determine_ip(in_addr_t *out);

array *read_file(const char *filename);

#endif
