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

#ifndef _UTIL_STRING_H
#define _UTIL_STRING_H

#include "Shared.h"

#include <stdarg.h>
#include <stdio.h>

namespace GridXSLT {

class StringBuffer;

class Printable {
public:
  Printable() { }
  virtual ~Printable() { }
  virtual void print(StringBuffer &buf) = 0;
  virtual char *checkPrintable() { return "yes"; }
};

class Char {
public:
  unsigned short c;
};

inline bool operator==(const Char &a, const Char &b) { return (a.c == b.c); }
inline bool operator!=(const Char &a, const Char &b) { return (a.c != b.c); }

class StringImpl : public Shared<StringImpl> {
  friend class StringBuffer;
public:
  StringImpl();
  StringImpl(const char *cstr);
  StringImpl(const char *cstr, unsigned int len);
  StringImpl(Char *chars, unsigned int len);
  ~StringImpl();

  char *cstring() const;
  void fprint(FILE *f) const;
  void dump() const;

//private:
  Char *m_chars;
  unsigned int m_len;

private:
  StringImpl(const StringImpl&);
  StringImpl &operator=(const StringImpl&);
};

class String : public Printable {
  friend String operator+(String const&a, String const&b);
  friend bool operator==(String const &a, String const &b);
  friend class StringBuffer;

public:
  String() {
    impl = 0;
  }
  String(StringImpl *_impl) {
    impl = _impl;
    if (impl)
      impl->ref();
  }
  String(const char *cstr) {
    if (cstr) {
      impl = new StringImpl(cstr);
      impl->ref();
    }
    else {
      impl = 0;
    }
  }
  String(const char *cstr, unsigned int len) {
    impl = new StringImpl(cstr,len);
    impl->ref();
  }
  String(const GridXSLT::String &other) {
    impl = other.impl;
    if (impl)
      impl->ref();
  }
  String &operator=(const String &other) {
    if (impl)
      impl->deref();
    impl = other.impl;
    if (impl)
      impl->ref();
    return *this;
  }
  ~String() {
    if (impl)
      impl->deref();
  }
  String(const StringBuffer &buf);
  String &operator=(const StringBuffer &buf);

  static String vformat(const char *format, va_list ap);
  static String format(const char *format, ...);

  virtual void print(StringBuffer &buf);

  inline bool isNull() const { return (0 == impl); }
  static String null() { return String(); }

  char *cstring() const { return impl ? impl->cstring() : NULL; }
  void fprint(FILE *f) const { impl->fprint(f); }
  void dump() const { impl->dump(); }

  unsigned int length() const { return impl->m_len; }
  const Char &charAt(unsigned int pos) const;
  int indexOf(const Char &c) const;
  String substring(unsigned int start) const;
  String substring(unsigned int start, unsigned int len) const;
  int indexOf(const String &other) const;
  bool isAllWhitespace() const;
  String toUpper() const;
  String toLower() const;
  double toDouble() const;
  int toInt() const;

  const Char *chars() const { return impl->m_chars; }

  StringImpl *handle() const { return impl; }

private:
  StringImpl *impl;
};

String operator+(String const&a, String const&b);
bool operator==(String const &a, String const &b);
inline bool operator!=(const String &a, const String &b) { return !(a==b); }

class StringBuffer : public Printable {
  DISABLE_COPY(StringBuffer)
public:
  StringBuffer();
  ~StringBuffer();

  virtual void print(StringBuffer &buf);

  void vformat(const char *format, va_list ap);
  void format(const char *format, ...);
  void append(const String &s);
  void clear();
  String contents() const;

private:
  void grow();

  Char *m_chars;
  unsigned int m_len;
  unsigned int m_alloc;
};

void fmessage(FILE *f, const char *format, ...);
void message(const char *format, ...);

};

#endif // _UTIL_STRING_H
