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

#include "stringbuf.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stdarg.h>

stringbuf *stringbuf_new()
{
  stringbuf *buf = (stringbuf*)malloc(sizeof(stringbuf));
  buf->allocated = 2;
  buf->size = 1;
  buf->data = (char*)malloc(buf->allocated);
  buf->data[0] = '\0';
  return buf;
}

void stringbuf_printf(stringbuf *buf, const char *format, ...)
{
  int flen;
  va_list ap;

  va_start(ap,format);
  flen = vsnprintf(NULL,0,format,ap);
  va_end(ap);

  while (buf->size+flen >= buf->allocated) {
    buf->allocated *= 2;
    buf->data = (char*)realloc(buf->data,buf->allocated);
  }

  va_start(ap,format);
  vsprintf(buf->data+buf->size-1,format,ap);
  va_end(ap);

  buf->size += flen;
}

void stringbuf_append(stringbuf *buf, const char *data, int size)
{
  while (buf->size+size >= buf->allocated) {
    buf->allocated *= 2;
    buf->data = (char*)realloc(buf->data,buf->allocated);
  }
  if (NULL != data)
    memcpy(buf->data+buf->size-1,data,size);
  else
    memset(buf->data+buf->size-1,0,size);
  buf->size += size;
  buf->data[buf->size-1] = '\0';
}

void stringbuf_clear(stringbuf *buf)
{
  buf->size = 1;
  buf->data[0] = '\0';
}

void stringbuf_free(stringbuf *buf)
{
  free(buf->data);
  free(buf);
}
