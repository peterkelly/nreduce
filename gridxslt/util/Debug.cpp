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

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stdarg.h>
#include "Debug.h"
#include "String.h"

using namespace GridXSLT;

void GridXSLT::debugl(const char *format, ...)
{
#ifdef DEBUG
  va_list ap;
  stringbuf *buf;

  if (NULL == getenv("DEBUG"))
    return;

  buf = stringbuf_new();
  va_start(ap,format);
  stringbuf_vformat(buf,format,ap);
  va_end(ap);

  message("%s",buf->data);
  stringbuf_free(buf);
  message("\n");
#endif
}

void GridXSLT::debug(const char *format, ...)
{
#ifdef DEBUG
  va_list ap;
  stringbuf *buf;

  if (NULL == getenv("DEBUG"))
    return;

  buf = stringbuf_new();
  va_start(ap,format);
  stringbuf_vformat(buf,format,ap);
  va_end(ap);

  message("%s",buf->data);
  stringbuf_free(buf);

#endif
}

void GridXSLT::debug_indent(int indent, const char *format, ...)
{
#ifdef DEBUG
  int i;
  va_list ap;
  if (NULL == getenv("DEBUG"))
    return;
  for (i = 0; i < indent; i++)
    message("  ");
  va_start(ap,format);
  vprintf(format,ap);
  va_end(ap);
  message("\n");
#endif
}

void GridXSLT::fatal(const char *msg, const char *filename, int line)
{
  fprintf(stderr,"ASSERTION FAILURE: %s:%d: %s\n",filename,line,msg);
  *((int*)NULL) = 0;
  exit(1);
}

