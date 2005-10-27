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

#ifndef _UTIL_SHARED_H
#define _UTIL_SHARED_H

//#define NDEBUG

#define DISABLE_COPY(classname)           \
private:                                  \
  classname(const classname&);            \
  classname &operator=(const classname&);

namespace GridXSLT {

template <class type> class Shared {

public:
  Shared()
  {
    m_refCount = 0;
  }

  inline type* ref()
  {
    m_refCount++;
    return static_cast<type*>(this);
  }

  inline void deref()
  {
    if (0 == --m_refCount) {
      delete static_cast<type*>(this);
    }
  }

  inline int refCount() const { return m_refCount; }

private:
  int m_refCount;
};

};

#endif // _UTIL_SHARED_H
