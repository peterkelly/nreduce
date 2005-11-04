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
#include "debug.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
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
    return 0;

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
