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

#include "stringbuf.h"
#include "xmlutils.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stdarg.h>
#include <unistd.h>
#include <errno.h>

stringbuf *stringbuf_new()
{
  stringbuf *buf = (stringbuf*)malloc(sizeof(stringbuf));
  buf->allocated = 2;
  buf->size = 1;
  buf->data = (char*)malloc(buf->allocated);
  buf->data[0] = '\0';
  return buf;
}

static int format_special(char *buf, int buflen, char c, va_list *ap)
{
  switch (c) {
  case 'q': {
    qname qn = va_arg(*ap,qname);
    if (!qn.localpart)
      return 0;
    else if (qn.prefix)
      return snprintf(buf,buflen,"%s:%s",qn.prefix,qn.localpart);
    else
      return snprintf(buf,buflen,"%s",qn.localpart);
    break;
  }
  case 'n': {
    nsname nn = va_arg(*ap,nsname);
    if (!nn.name)
      return 0;
    else if (nn.ns)
      return snprintf(buf,buflen,"{%s}%s",nn.ns,nn.name);
    else
      return snprintf(buf,buflen,"%s",nn.name);
    c++;
    break;
  }
  case 'i': {
    int indent = va_arg(*ap,int);
    if (buf)
      memset(buf,' ',indent);
    return indent;
  }
  }
  return 0;
}

#define STATE_NORMAL   0
#define STATE_FMTSTART 1

#define NUMLEN_MAX 10

static int format_str(char *buf, int buflen, const char *format, va_list ap)
{
  /* This should at some point be expanded to support the full printf() syntax... none of the
     code that uses this really needs this yet */
  const char *c;
  int len = 0;
  char *dest = buf;
  int remaining = buflen;

  for (c = format; '\0' != *c; c++) {
    int size = 0;
    if ('%' == *c) {
      c++;
      switch (*c) {
      case 'd': size = snprintf(dest,remaining,"%i",va_arg(ap,int)); break;
      case 'i': size = snprintf(dest,remaining,"%d",va_arg(ap,int)); break;
      case 'o': size = snprintf(dest,remaining,"%o",va_arg(ap,unsigned int)); break;
      case 'u': size = snprintf(dest,remaining,"%u",va_arg(ap,unsigned int)); break;
      case 'x': size = snprintf(dest,remaining,"%x",va_arg(ap,unsigned int)); break;
      case 'X': size = snprintf(dest,remaining,"%X",va_arg(ap,unsigned int)); break;
      case 'e': size = snprintf(dest,remaining,"%e",va_arg(ap,double)); break;
      case 'E': size = snprintf(dest,remaining,"%E",va_arg(ap,double)); break;
      case 'f': size = snprintf(dest,remaining,"%f",va_arg(ap,double)); break;
      case 'F': size = snprintf(dest,remaining,"%F",va_arg(ap,double)); break;
      case 'g': size = snprintf(dest,remaining,"%g",va_arg(ap,double)); break;
      case 'G': size = snprintf(dest,remaining,"%G",va_arg(ap,double)); break;
      case 'a': size = snprintf(dest,remaining,"%a",va_arg(ap,double)); break;
      case 'A': size = snprintf(dest,remaining,"%A",va_arg(ap,double)); break;
      case 'c': size = snprintf(dest,remaining,"%c",va_arg(ap,int)); break;
      case 's': size = snprintf(dest,remaining,"%s",va_arg(ap,char*)); break;
      case 'p': size = snprintf(dest,remaining,"%p",va_arg(ap,void*)); break;
      case 'n': size = snprintf(dest,remaining,"%n",va_arg(ap,int*)); break;
      case '%': size = snprintf(dest,remaining,"%%"); break;
      case '#': size = format_special(dest,remaining,*(++c),&ap); break;
      }
    }
    else {
      if (dest)
        *dest = *c;
      size = 1;
    }

    if (buf) {
      dest += size;
      remaining -= size;
    }
    len += size;
  }
  return len;
}

void stringbuf_vformat(stringbuf *buf, const char *format, va_list ap)
{
  int flen;
  va_list tmpap;

  va_copy(tmpap,ap);
  flen = format_str(NULL,0,format,tmpap);
  va_end(tmpap);

  while (buf->size+flen >= buf->allocated) {
    buf->allocated *= 2;
    buf->data = (char*)realloc(buf->data,buf->allocated);
  }

  va_copy(tmpap,ap);
  format_str(buf->data+buf->size-1,flen+1,format,tmpap);
  va_end(tmpap);

  buf->size += flen;
  buf->data[buf->size-1] = '\0';
}

void stringbuf_format(stringbuf *buf, const char *format, ...)
{
  va_list ap;

  va_start(ap,format);
  stringbuf_vformat(buf,format,ap);
  va_end(ap);
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









circbuf *circbuf_new()
{
  circbuf *cb = (circbuf*)calloc(1,sizeof(circbuf));
  cb->alloc = 4;
  cb->data = (char*)calloc(1,4);
  return cb;
}

static void circbuf_grow(circbuf *cb)
{
  int oldalloc = cb->alloc;

/*   if (0 == cb->alloc) */
/*     cb->alloc = 16; */
/*   else */
/*     cb->alloc *= 2; */

  cb->alloc += 1024;

  cb->data = (char*)realloc(cb->data,cb->alloc);
  memset(cb->data+oldalloc,0,cb->alloc-oldalloc);

  if (cb->start > cb->end) {
    int oldstart = cb->start;
    int start2alloc = oldalloc - oldstart;
    cb->start = cb->alloc - start2alloc;
    memmove(cb->data+cb->start,cb->data+oldstart,start2alloc);
  }
}

int circbuf_size(circbuf *cb)
{
  if (cb->start <= cb->end)
    return cb->end - cb->start;
  else
    return cb->alloc - cb->start + cb->end;
}

int circbuf_suck(circbuf *cb, int fd, int size)
{
  int r;
  while (size >= cb->alloc - circbuf_size(cb))
    circbuf_grow(cb);

  if (cb->start <= cb->end) {
    int tillend = cb->alloc - cb->end;
    if (size <= tillend) {
      if (0 <= (r = read(fd,cb->data+cb->end,size)))
        cb->end += r;
      return r;
    }
    else {
      if (0 > (r = read(fd,cb->data+cb->end,tillend)))
        return r;
      cb->end += r;

      if (tillend > r)
        return r;

      if (0 > (r = read(fd,cb->data,size-tillend))) {
        if (EAGAIN == errno)
          return tillend;
        else
          return -1;
      }

      cb->end = r;

      return r;
    }
  }
  else {
    if (0 <= (r = read(fd,cb->data+cb->end,size)))
      cb->end += r;
    return r;
  }
}

void circbuf_append(circbuf *cb, const char *data, int size)
{
  while (size >= cb->alloc - circbuf_size(cb))
    circbuf_grow(cb);

  if (cb->start <= cb->end) {
    int tillend = cb->alloc - cb->end;
    if (size <= tillend) {
      memcpy(cb->data+cb->end,data,size);
      cb->end += size;
    }
    else {
      memcpy(cb->data+cb->end,data,tillend);
      memcpy(cb->data,data+tillend,size-tillend);
      cb->end = size-tillend;
    }
  }
  else {
    memcpy(cb->data+cb->end,data,size);
    cb->end += size;
  }
}

int circbuf_get(circbuf *cb, char *out, int size)
{
  int bs = circbuf_size(cb);
  if (size > bs)
    size = bs;

  if (cb->start <= cb->end) {
    memcpy(out,cb->data+cb->start,size);
  }
  else {
    int tillend = cb->alloc - cb->start;

    if (size <= tillend) {
      memcpy(out,cb->data+cb->start,size);
    }
    else {
      memcpy(out,cb->data+cb->start,tillend);
      memcpy(out+tillend,cb->data,size-tillend);
    }
  }

  return size;
}

int circbuf_mvstart(circbuf *cb, int size)
{
  int bs = circbuf_size(cb);
  if (size > bs)
    size = bs;

  if (cb->start <= cb->end) {
    cb->start += size;
  }
  else {
    int tillend = cb->alloc - cb->start;

    if (size <= tillend) {
      cb->start += size;
    }
    else {
      cb->start = size-tillend;
    }
  }

  return size;
}

int circbuf_read(circbuf *cb, char *out, int size)
{
  circbuf_get(cb,out,size);
  return circbuf_mvstart(cb,size);
}

void circbuf_free(circbuf *cb)
{
  free(cb->data);
  free(cb);
}
