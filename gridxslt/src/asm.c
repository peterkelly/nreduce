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

#include <stdio.h>
#include "util/stringbuf.h"

typedef void (fp)(char *s, int len);

char data[31] = {
 0x55,                                 /* push   %ebp */
 0x89, 0xe5,                           /* mov    %esp,%ebp */
 0x81, 0xec, 0x04, 0x00, 0x00, 0x00,   /* sub    $0x4,%esp */
 0xb8, 0x04, 0x00, 0x00, 0x00,         /* mov    $0x4,%eax */
 0xbb, 0x01, 0x00, 0x00, 0x00,         /* mov    $0x1,%ebx */
 0x8b, 0x4d, 0x08,                     /* mov    0x8(%ebp),%ecx */
 0x8b, 0x55, 0x0c,                     /* mov    0xc(%ebp),%edx */
 0xcd, 0x80,                           /* int    $0x80 */
 0x89, 0xec,                           /* mov    %ebp,%esp */
 0x5d,                                 /* pop    %ebp */
 0xc3,                                 /* ret     */
};

int main()
{
  fp *fptr;
  setbuf(stdout,NULL);
  message("start\n");

  fptr = (fp*)data;
  fptr("This is a test",7);

  message("end\n");

  return 0;
}
