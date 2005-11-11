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

#include "BinaryArray.h"
#include <string.h>
#include <stdlib.h>
#include <ctype.h>

using namespace GridXSLT;

BinaryArrayImpl::BinaryArrayImpl()
  : m_size(0),
    m_allocated(0),
    m_data(NULL)
{
}

BinaryArrayImpl::BinaryArrayImpl(unsigned char *data, unsigned int size)
  : m_size(size),
    m_allocated(size),
    m_data(data)
{
}

BinaryArrayImpl::~BinaryArrayImpl()
{
  free(m_data);
}

void BinaryArrayImpl::append(const unsigned char *data, unsigned int size)
{
  while (m_allocated < m_size+size) {
    if (0 == m_allocated)
      m_allocated = 64;
    else
      m_allocated *= 2;
    m_data = (unsigned char*)realloc(m_data,m_allocated);
  }

  memcpy(&m_data[m_size],data,size);
  m_size += size;
}

static const char *base64chars = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

String BinaryArrayImpl::toBase64() const
{
  char *b64 = (char*)malloc(2*m_size+1);
  unsigned int i;
  unsigned int pos = 0;
  for (i = 0; i < m_size; i += 3) {
    unsigned char c1;
    unsigned char c2;
    unsigned char c3;

    if (i == m_size - 2) {
      c1 = m_data[i];
      c2 = m_data[i+1];
      c3 = 0;
    }
    else if (i == m_size - 1) {
      c1 = m_data[i];
      c2 = 0;
      c3 = 0;
    }
    else {
      c1 = m_data[i];
      c2 = m_data[i+1];
      c3 = m_data[i+2];
    }

    unsigned char six1 = (c1 & 0xFC) >> 2;
    unsigned char six2 = ((c1 & 0x03) << 4) | ((c2 & 0xF0) >> 4);
    unsigned char six3 = ((c2 & 0x0F) << 2) | ((c3 & 0xC0) >> 6);
    unsigned char six4 = c3 & 0x3F;

    b64[pos++] = base64chars[six1];
    b64[pos++] = base64chars[six2];
    if (i == m_size - 2) {
      b64[pos++] = base64chars[six3];
      b64[pos++] = '=';
    }
    else if (i == m_size - 1) {
      b64[pos++] = '=';
      b64[pos++] = '=';
    }
    else {
      b64[pos++] = base64chars[six3];
      b64[pos++] = base64chars[six4];
    }
  }
  b64[pos] = '\0';

  String str = String(b64);

//   StringBuffer buf;
//   for (i = 0; i < pos; i += 76) {
//     char line[77];
//     if (i + 76 <= pos) {
//       memcpy(line,&b64[i],76);
//       line[76] = '\0';
//       buf.format("%s\n",line);
//     }
//     else {
//       memcpy(line,&b64[i],pos-i);
//       line[pos-i] = '\0';
//       buf.format("%s\n",line);
//     }
//   }

  free(b64);

  return str;
}

String BinaryArrayImpl::toHex() const
{
  StringBuffer buf;
  for (unsigned int i = 0; i < m_size; i++) {
    unsigned char hi = m_data[i] >> 4;
    unsigned char lo = m_data[i] & 0xF;
    char hichar = (hi < 10 ? hi+'0' : (hi-10)+'A');
    char lochar = (lo < 10 ? lo+'0' : (lo-10)+'A');
    buf.format("%c%c",hichar,lochar);
  }
  return buf;
}

BinaryArrayImpl *BinaryArrayImpl::fromBase64(const String &str)
{
  unsigned int len = str.length();
  const Char *chars = str.chars();
  BinaryArrayImpl *arr = new BinaryArrayImpl();

  unsigned char quad[4];
  unsigned int qsize = 0;
  bool foundend = false;

  unsigned int i;
  for (i = 0; i < len; i++) {
    char *c;
    if (isspace(chars[i].c))
      continue;

    if (foundend) {
      delete arr;
      return NULL;
    }

    if ('=' == chars[i].c) {
      quad[qsize++] = 0xFF;
    }
    else {
      if ((255 < chars[i].c) || (NULL == (c = strchr(base64chars,chars[i].c)))) {
        delete arr;
        return NULL;
      }
      quad[qsize++] = c-base64chars;
    }
    if (4 == qsize) {

      if ((0xFF == quad[0]) || (0xFF == quad[1]) ||
          ((0xFF == quad[2]) && (0xFF != quad[3]))) {
        delete arr;
        return NULL;
      }

      if (0xFF == quad[2]) {
        unsigned char dec[1];
        dec[0] = (quad[0] << 2) | (quad[1] >> 4);
        arr->append(dec,1);
        foundend = true;
      }
      else if (0xFF == quad[3]) {
        unsigned char dec[2];
        dec[0] = (quad[0] << 2) | (quad[1] >> 4);
        dec[1] = (quad[1] << 4) | (quad[2] >> 2);
        arr->append(dec,2);
        foundend = true;
      }
      else {
        unsigned char dec[3];
        dec[0] = (quad[0] << 2) | (quad[1] >> 4);
        dec[1] = (quad[1] << 4) | (quad[2] >> 2);
        dec[2] = (quad[2] << 6) | quad[3];
        arr->append(dec,3);
      }

      qsize = 0;
    }
  }

  if (0 != qsize) {
    delete arr;
    return NULL;
  }

  return arr;
}

static int hexval(unsigned short c)
{
  if (('0' <= c) && ('9' >= c))
    return c-'0';
  else if (('A' <= c) && ('F' >= c))
    return 10+c-'A';
  else if (('a' <= c) && ('f' >= c))
    return 10+c-'a';
  else
    return -1;
}

BinaryArrayImpl *BinaryArrayImpl::fromHex(const String &str)
{
  String collapsed = str.collapseWhitespace();
  unsigned int len = str.length();
  const Char *chars = str.chars();
  if (0 != (len % 2))
    return NULL;

  unsigned char *data = (unsigned char*)malloc(len/2);
  for (unsigned int pos = 0; pos < len/2; pos++) {
    Char hi = chars[pos*2];
    Char lo = chars[pos*2+1];
    int hival = hexval(hi.c);
    int loval = hexval(lo.c);
    if ((0 > hival) || (0 > loval)) {
      free(data);
      return NULL;
    }
    data[pos] = hival*0x10 + loval;
  }

  return new BinaryArrayImpl(data,len/2);
}

