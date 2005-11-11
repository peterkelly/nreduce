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

#include "util/String.h"
#include "util/List.h"
#include "util/Debug.h"
#include "util/XMLUtils.h"
#include "util/BinaryArray.h"
#include "xmlschema/XMLSchema.h"
#include "dataflow/SequenceType.h"
#include "dataflow/Program.h"
#include <stdio.h>
#include <string.h>
#include <errno.h>

using namespace GridXSLT;

int main(int argc, char **argv)
{
  if (4 > argc) {
    fmessage(stderr,"Usage: binencode b64|hex from|to FILE\n");
    return 1;
  }

  char *filename = argv[3];

  FILE *f = fopen(filename,"r");
  if (NULL == f) {
    fmessage(stderr,"%s: %s\n",filename,strerror(errno));
    return -1;
  }

  BinaryArrayImpl *arr = new BinaryArrayImpl();
  unsigned char buf[1024];
  int r;
  while (0 < (r = fread(buf,1,1024,f)))
    arr->append(buf,r);
  fclose(f);

  if (!strcmp("b64",argv[1])) {
    if (!strcmp("from",argv[2])) {
      arr->append((unsigned char*)"\0",1);
      String str = String((char*)arr->data());
      BinaryArrayImpl *arr2 = BinaryArrayImpl::fromBase64(str);
      if (NULL == arr2) {
        fmessage(stderr,"Error converting from base64 data\n");
        return -1;
      }
      fwrite(arr2->data(),1,arr2->size(),stdout);
      delete arr2;
    }
    else if (!strcmp("to",argv[2])) {
      String str = arr->toBase64();
      message("%*\n",&str);
    }
    else {
      fmessage(stderr,"Invalid direction\n");
    }
  }
  else if (!strcmp("hex",argv[1])) {
    if (!strcmp("from",argv[2])) {
      arr->append((unsigned char*)"\0",1);
      String str = String((char*)arr->data());
      BinaryArrayImpl *arr2 = BinaryArrayImpl::fromHex(str);
      if (NULL == arr2) {
        fmessage(stderr,"Error converting from hex data\n");
        return -1;
      }
      fwrite(arr2->data(),1,arr2->size(),stdout);
      delete arr2;
    }
    else if (!strcmp("to",argv[2])) {
      String str = arr->toHex();
      message("%*",&str);
    }
    else {
      fmessage(stderr,"Invalid direction\n");
    }
  }
  else {
    fmessage(stderr,"Invalid file type\n");
    return 1;
  }


  delete arr;

  return 0;
}
