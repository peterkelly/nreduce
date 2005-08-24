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

struct stringbuf {
  int size;
  int allocated;
  char *data;
};

stringbuf *stringbuf_new();
void stringbuf_vformat(stringbuf *buf, const char *format, va_list ap);
void stringbuf_format(stringbuf *buf, const char *format, ...);
void stringbuf_append(stringbuf *buf, const char *data, int size);
void stringbuf_clear(stringbuf *buf);
void stringbuf_free(stringbuf *buf);

#endif /* _UTIL_STRINGBUF_H */
