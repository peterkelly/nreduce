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

#include "String.h"
#include "XMLUtils.h"
#include "Debug.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <ctype.h>

using namespace GridXSLT;

StringImpl::StringImpl()
{
  m_chars = NULL;
  m_len = 0;
}

StringImpl::StringImpl(const char *cstr)
{
  m_len = strlen(cstr);
  m_chars = new Char[m_len];
  for (unsigned int i = 0; i < m_len; i++)
    m_chars[i].c = (unsigned short)cstr[i];
}

StringImpl::StringImpl(const char *cstr, unsigned int len)
{
  m_len = len;
  m_chars = new Char[m_len];
  for (unsigned int i = 0; i < m_len; i++)
    m_chars[i].c = (unsigned short)cstr[i];
}

StringImpl::StringImpl(Char *chars, unsigned int len)
{
  m_chars = chars;
  m_len = len;
}

StringImpl::~StringImpl()
{
  delete [] m_chars;
}

char *StringImpl::cstring() const
{
  char *cstr = (char*)malloc(m_len+1);
  for (unsigned int i = 0; i < m_len; i++)
    cstr[i] = (char)m_chars[i].c;
  cstr[m_len] = '\0';
  return cstr;
}

void StringImpl::fprint(FILE *f) const
{
  char *cstr = cstring();
  fprintf(f,"%s",cstr);
  free(cstr);
}

void StringImpl::dump() const
{
  char *cstr = cstring();
  printf("%s",cstr);
  free(cstr);
}

String::String(const GridXSLT::StringBuffer &buf)
{
  String contents = buf.contents();
  impl = contents.handle();
  impl->ref();
}

String &String::operator=(const StringBuffer &buf)
{
  if (impl)
    impl->deref();
  String contents = buf.contents();
  impl = contents.handle();
  impl->ref();
  return *this;
}

String String::vformat(const char *format, va_list ap)
{
  StringBuffer buf;
  buf.vformat(format,ap);
  return buf;
}

String String::format(const char *format, ...)
{
  StringBuffer buf;
  va_list ap;
  va_start(ap,format);
  buf.vformat(format,ap);
  va_end(ap);
  return buf;
}

const Char &String::charAt(unsigned int pos) const
{
  ASSERT(pos <= impl->m_len);
  return impl->m_chars[pos];
}

int String::indexOf(const Char &c) const
{
  for (unsigned int i = impl->m_len; i; i++) {
    if (impl->m_chars[i] == c)
      return (int)i;
  }
  return -1;
}

String String::substring(unsigned int start) const
{
  if (start >= impl->m_len)
    return String("");
  unsigned int len = impl->m_len - start;
  Char *chars = new Char[len];
  for (unsigned int i = 0; i < len; i++)
    chars[i] = impl->m_chars[start+i];
  return new StringImpl(chars,len);
}

String String::substring(unsigned int start, unsigned int len) const
{
  if (start >= impl->m_len)
    return String();
  if (start + len > impl->m_len)
    len = impl->m_len - start;
  Char *chars = new Char[len];
  for (unsigned int i = 0; i < len; i++)
    chars[i] = impl->m_chars[start+i];
  return new StringImpl(chars,len);
}

int String::indexOf(const String &other) const
{
  if ((other.length() == 0) || (other.length() > length()))
    return -1;

  unsigned int laststart = length() - other.length();

  for (unsigned int pos = 0; pos < laststart; pos++)
    if (!memcmp(&impl->m_chars[pos],&other.impl->m_chars[0],other.length()*sizeof(Char)))
      return (int)pos;

  return -1;
}

bool String::isAllWhitespace() const
{
  unsigned int i;
  for (i = 0; i < impl->m_len; i++)
    if ((impl->m_chars[i].c < 256) && !isspace(impl->m_chars[i].c))
      return 0;
  return 1;
}

String String::toUpper() const
{
  Char *chars = new Char[impl->m_len];
  for (unsigned int i = 0; i < impl->m_len; i++) {
    if (((unsigned short)'a' <= impl->m_chars[i].c) &&
        ((unsigned short)'z' >= impl->m_chars[i].c))
      chars[i].c = impl->m_chars[i].c - (unsigned short)('a' - 'A');
    else
      chars[i].c = impl->m_chars[i].c;
  }

  return new StringImpl(chars,impl->m_len);
}

String String::toLower() const
{
  Char *chars = new Char[impl->m_len];
  for (unsigned int i = 0; i < impl->m_len; i++) {
    if (((unsigned short)'A' <= impl->m_chars[i].c) &&
        ((unsigned short)'Z' >= impl->m_chars[i].c))
      chars[i].c = impl->m_chars[i].c + (unsigned short)('a' - 'A');
    else
      chars[i].c = impl->m_chars[i].c;
  }

  return new StringImpl(chars,impl->m_len);
}

double String::toDouble() const
{
  char *s = cstring();
  double d = atof(s);
  free(s);
  return d;
}

int String::toInt() const
{
  char *s = cstring();
  int i = atoi(s);
  free(s);
  return i;
}

String String::collapseWhitespace() const
{
  Char *chars = new Char[impl->m_len];
  unsigned int pos = 0;
  bool haveSpace = false;
  for (unsigned int i = 0; i < impl->m_len; i++) {
    if (((unsigned short)'\t' == impl->m_chars[i].c) ||
        ((unsigned short)'\r' == impl->m_chars[i].c) ||
        ((unsigned short)'\n' == impl->m_chars[i].c) ||
        ((unsigned short)' ' == impl->m_chars[i].c)) {
      if (!haveSpace && (0 < pos))
        chars[pos++].c = ' ';
      haveSpace = true;
    }
    else {
      chars[pos++] = impl->m_chars[i];
      haveSpace = false;
    }
  }
  if ((0 < pos) && ((unsigned short)' ' == chars[pos-1].c))
    pos--;
  return new StringImpl(chars,pos);
}

String String::replaceWhitespace() const
{
  Char *chars = new Char[impl->m_len];
  memcpy(chars,impl->m_chars,impl->m_len*sizeof(Char));
  for (unsigned int i = 0; i < impl->m_len; i++) {
    if (((unsigned short)'\t' == chars[i].c) ||
        ((unsigned short)'\r' == chars[i].c) ||
        ((unsigned short)'\n' == chars[i].c))
      chars[i].c = ' ';
  }
  return new StringImpl(chars,impl->m_len);
}

List<String> String::parseList() const
{
  List<String> lst;
  unsigned int start = 0;
  unsigned int i;
  for (i = 0; i < impl->m_len; i++) {
    if (impl->m_chars[i].isSpace()) {
      if (i > start)
        lst.append(String(substring(start,i-start)));
      start = i+1;
    }
  }
  if (i > start)
    lst.append(String(substring(start,i-start)));
  return lst;
}

void String::print(StringBuffer &buf)
{
  buf.append(*this);
}

bool GridXSLT::operator==(String const &a, String const &b)
{
  if (a.impl == b.impl)
    return true;

  if (!a.impl)
    return !b.impl;
  if (!b.impl)
    return !a.impl;

  if (a.impl->m_len != b.impl->m_len)
    return false;

  return !memcmp(a.impl->m_chars,b.impl->m_chars,a.impl->m_len*sizeof(Char));
}

// bool operator==(const String &a, const char *b)
// {
//   String bstr(b);
//   return operator==(a,bstr);
// }

String GridXSLT::operator+(String const&a, String const&b)
{
//   printf("operator+(const String &a, const String &b)\n");
  unsigned int i;
  unsigned int pos = 0;
  unsigned int len = a.impl->m_len + b.impl->m_len;
  Char *chars = new Char[len];
  for (i = 0; i < a.impl->m_len; i++)
    chars[pos++].c = a.impl->m_chars[i].c;
  for (i = 0; i < b.impl->m_len; i++)
    chars[pos++].c = b.impl->m_chars[i].c;
  return new StringImpl(chars,len);
}

StringBuffer::StringBuffer()
{
  m_len = 0;
  m_alloc = 64;
  m_chars = (Char*)malloc(m_alloc*sizeof(Char));
}

StringBuffer::~StringBuffer()
{
  free(m_chars);
}

char *allocprintf(const char *format, int v)
{
  int size = snprintf(NULL,0,format,v);
  char *str = (char*)malloc(size+1);
  snprintf(str,size+1,format,v);
  return str;
}

char *allocprintf(const char *format, unsigned int v)
{
  int size = snprintf(NULL,0,format,v);
  char *str = (char*)malloc(size+1);
  snprintf(str,size+1,format,v);
  return str;
}

char *allocprintf(const char *format, double v)
{
  int size = snprintf(NULL,0,format,v);
  char *str = (char*)malloc(size+1);
  snprintf(str,size+1,format,v);
  return str;
}

char *allocprintf(const char *format, char *v)
{
  int size = snprintf(NULL,0,format,v);
  char *str = (char*)malloc(size+1);
  snprintf(str,size+1,format,v);
  return str;
}

char *allocprintf(const char *format, void *v)
{
  int size = snprintf(NULL,0,format,v);
  char *str = (char*)malloc(size+1);
  snprintf(str,size+1,format,v);
  return str;
}

#define FSTATE_CHAR     0
#define FSTATE_FMTSTART 1
#define FSTATE_SPECIAL  2

void StringBuffer::print(StringBuffer &buf)
{
  String c = contents();
  c.print(buf);
}

void StringBuffer::vformat(const char *fm, va_list ap)
{
  int state = FSTATE_CHAR;
  const char *c = fm;
  const char *start = fm;
  char fmtstr[20];
  int fmtpos = 0;
  while (*c) {
    switch (state) {
    case FSTATE_CHAR:
      if ('%' == *c) {
        state = FSTATE_FMTSTART;
        append(String(start,c-start));
        memset(fmtstr,0,20);
        fmtpos = 0;
        fmtstr[fmtpos++] = '%';
      }
      break;
    case FSTATE_FMTSTART: {
      fmtstr[fmtpos++] = *c;
      if (fmtpos >= 19)
        return;

      char *dest = NULL;
      state = FSTATE_CHAR;
      switch (*c) {
      case 'd': dest = allocprintf(fmtstr,va_arg(ap,int)); break;
      case 'i': dest = allocprintf(fmtstr,va_arg(ap,int)); break;
      case 'o': dest = allocprintf(fmtstr,va_arg(ap,unsigned int)); break;
      case 'u': dest = allocprintf(fmtstr,va_arg(ap,unsigned int)); break;
      case 'x': dest = allocprintf(fmtstr,va_arg(ap,unsigned int)); break;
      case 'X': dest = allocprintf(fmtstr,va_arg(ap,unsigned int)); break;
      case 'e': dest = allocprintf(fmtstr,va_arg(ap,double)); break;
      case 'E': dest = allocprintf(fmtstr,va_arg(ap,double)); break;
      case 'f': dest = allocprintf(fmtstr,va_arg(ap,double)); break;
      case 'F': dest = allocprintf(fmtstr,va_arg(ap,double)); break;
      case 'g': dest = allocprintf(fmtstr,va_arg(ap,double)); break;
      case 'G': dest = allocprintf(fmtstr,va_arg(ap,double)); break;
      case 'a': dest = allocprintf(fmtstr,va_arg(ap,double)); break;
      case 'A': dest = allocprintf(fmtstr,va_arg(ap,double)); break;
      case 'c': dest = allocprintf(fmtstr,va_arg(ap,int)); break;
      case 's': dest = allocprintf(fmtstr,va_arg(ap,char*)); break;
      case 'p': dest = allocprintf(fmtstr,va_arg(ap,void*)); break;
      case 'n': dest = allocprintf(fmtstr,va_arg(ap,int*)); break;
      case '%': dest = allocprintf(fmtstr,""); break;
      case '*': va_arg(ap,Printable*)->print(*this); start = c+1; break;
      case '#': state = FSTATE_SPECIAL; break;
      default: state = FSTATE_FMTSTART; break;
      }

      if (NULL != dest) {
        append(String(dest));
        free(dest);
        start = c+1;
      }

      break;
    }
    case FSTATE_SPECIAL:
      switch (*c) {
      case 'i': {
        int indent = va_arg(ap,int);
        char *str = (char*)alloca(indent+1);
        memset(str,' ',indent);
        str[indent] = '\0';
        append(str);
      }
      }
      start = c+1;
      state = FSTATE_CHAR;
      break;
    }
    c++;
  }

  if (c > start)
    append(String(start,c-start));
}

void StringBuffer::format(const char *format, ...)
{
  va_list ap;
  va_start(ap,format);
  vformat(format,ap);
  va_end(ap);
}

void StringBuffer::append(const String &s)
{
  while (m_alloc < m_len + s.length())
    grow();
  memcpy(&m_chars[m_len],s.impl->m_chars,s.impl->m_len*sizeof(Char));
  m_len += s.length();
}

void StringBuffer::clear()
{
  m_len = 0;
}

String StringBuffer::contents() const
{
  Char *chars = new Char[m_len];
  memcpy(chars,m_chars,m_len*sizeof(Char));
  return new StringImpl(chars,m_len);
}

void StringBuffer::grow()
{
  m_alloc *= 2;
  m_chars = (Char*)realloc(m_chars,m_alloc*sizeof(Char));
}

void GridXSLT::fmessage(FILE *f, const char *format, ...)
{
  va_list ap;

  va_start(ap,format);
  StringBuffer buf;
  buf.vformat(format,ap);
  buf.contents().fprint(f);
  va_end(ap);
}

void GridXSLT::message(const char *format, ...)
{
  va_list ap;

  va_start(ap,format);
  StringBuffer buf;
  buf.vformat(format,ap);
  buf.contents().dump();
  va_end(ap);
}





stringbuf *stringbuf_new()
{
  stringbuf *buf = (stringbuf*)malloc(sizeof(stringbuf));
  buf->allocated = 2;
  buf->size = 1;
  buf->data = (char*)malloc(buf->allocated);
  buf->data[0] = '\0';
  return buf;
}

void stringbuf_vformat(stringbuf *buf, const char *format, va_list ap)
{
  StringBuffer b;
  b.vformat(format,ap);
  char *contents = b.contents().cstring();
  stringbuf_append(buf,contents,strlen(contents));
  free(contents);
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

URI::URI(const String &str)
  : m_str(str)
{
}

URI::URI(const URI &other)
  : m_str(other.m_str)
{
}

URI &URI::operator=(const URI &other)
{
  m_str = other.m_str;
  return *this;
}

String URI::toString() const
{
  return m_str;
}
