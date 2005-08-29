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
 * $Id$
 *
 */

#ifndef _UTIL_LIST_H
#define _UTIL_LIST_H

typedef struct list list;

typedef void (*list_d_t)(void *a);
typedef void* (*list_copy_t)(void *a);

struct list {
  void *data;
  list *next;
};

list *list_copy(list *orig, list_copy_t copy);
void list_append(list **l, void *data);
void list_push(list **l, void *data);
void *list_pop(list **l);
int list_count(list *l);
void list_free(list *l, list_d_t d);

int list_contains_string(list *l, const char *str);
int list_contains_ptr(list *l, const void *data);
void list_remove_ptr(list **l, void *ptr);
list *string_list_union(list *a, list *b);
list *string_list_intersection(list *a, list *b);

#endif /* _UTIL_LIST_H */
