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

#ifndef _UTIL_BINARY_ARRAY_H
#define _UTIL_BINARY_ARRAY_H

#include "Shared.h"
#include "String.h"

namespace GridXSLT {

class BinaryArrayImpl : public Shared<BinaryArrayImpl> {
  DISABLE_COPY(BinaryArrayImpl)
public:
  BinaryArrayImpl();
  BinaryArrayImpl(unsigned char *data, unsigned int size);
  ~BinaryArrayImpl();

  void append(const unsigned char *data, unsigned int size);
  String toBase64() const;
  String toHex() const;

  unsigned int size() const { return m_size; }
  unsigned char *data() const { return m_data; }

  static BinaryArrayImpl *fromBase64(const String &str);
  static BinaryArrayImpl *fromHex(const String &str);

protected:
  unsigned int m_size;
  unsigned int m_allocated;
  unsigned char *m_data;
};

class BinaryArray {
public:
  BinaryArray() {
    impl = 0;
  }
  BinaryArray(BinaryArrayImpl *_impl) {
    impl = _impl;
    if (impl)
      impl->ref();
  }
  BinaryArray(const BinaryArray &other) {
    impl = other.impl;
    if (impl)
      impl->ref();
  }
  BinaryArray &operator=(const BinaryArray &other) {
    if (impl)
      impl->deref();
    impl = other.impl;
    if (impl)
      impl->ref();
    return *this;
  }
  ~BinaryArray() {
    if (impl)
      impl->deref();
  }

  inline bool isNull() const { return (0 == impl); }
  static BinaryArray null() { return BinaryArray(); }

  void append(const unsigned char *data, unsigned int size) const
    { impl->append(data,size); }
  String toBase64() const { return impl->toBase64(); }
  String toHex() const { return impl->toHex(); }

  unsigned int size() const { return impl->size(); }
  unsigned char *data() const { return impl->data(); }

  static BinaryArray fromBase64(const String &str)
    { return BinaryArrayImpl::fromBase64(str); }
  static BinaryArray fromHex(const String &str)
    { return BinaryArrayImpl::fromHex(str); }

  BinaryArrayImpl *handle() const { return impl; }

private:
  BinaryArrayImpl *impl;
};


};

#endif // _UTIL_BINARY_ARRAY_H
