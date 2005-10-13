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

#ifndef _UTIL_STRINGBUF_H
#define _UTIL_STRINGBUF_H

#include <stdio.h>
#include <stdarg.h>

typedef struct stringbuf stringbuf;
typedef struct circbuf circbuf;

struct stringbuf {
  int size;
  int allocated;
  char *data;
};

struct circbuf {
  char *data;
  int start;
  int end;
  int alloc;
};

stringbuf *stringbuf_new();
void stringbuf_vformat(stringbuf *buf, const char *format, va_list ap);
void stringbuf_format(stringbuf *buf, const char *format, ...);
void stringbuf_append(stringbuf *buf, const char *data, int size);
void stringbuf_clear(stringbuf *buf);
void stringbuf_free(stringbuf *buf);

circbuf *circbuf_new();
int circbuf_size(circbuf *cb);
int circbuf_suck(circbuf *cb, int fd, int size);
void circbuf_append(circbuf *cb, const char *data, int size);
int circbuf_get(circbuf *cb, char *out, int size);
int circbuf_mvstart(circbuf *cb, int size);
int circbuf_read(circbuf *cb, char *out, int size);
void circbuf_free(circbuf *cb);

#endif /* _UTIL_STRINGBUF_H */
