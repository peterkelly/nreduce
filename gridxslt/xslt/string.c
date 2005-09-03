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

#include "dataflow/sequencetype.h"
#include "dataflow/dataflow.h"
#include "util/xmlutils.h"
#include <assert.h>

#define FNS FN_NAMESPACE

static gxvalue *string_join(gxcontext *ctxt, gxvalue **args)
{
  gxvalue **values = df_sequence_to_array(args[0]);
  stringbuf *buf = stringbuf_new();
  gxvalue *res;
  int i;

  print("New string-join()\n");

  for (i = 0; values[i]; i++) {
    stringbuf_format(buf,"%s",asstring(values[i]));
    if (values[i+1])
      stringbuf_format(buf,"%s",asstring(args[1]));
  }

  res = mkstring(buf->data);
  stringbuf_free(buf);
  free(values);
  return res;
}

static gxvalue *substring2(gxcontext *ctxt, gxvalue **args)
{
  const char *s = asstring(args[0]);
  return mkstring(s);
}

static gxvalue *substring3(gxcontext *ctxt, gxvalue **args)
{
  const char *s = asstring(args[0]);
  return mkstring(s);
}

gxfunctiondef string_fundefs[4] = {
  { string_join, FNS, "string-join", "xsd:string*,xsd:string",              "xsd:string" },
  { substring2,  FNS, "substring",   "xsd:string?,xsd:double",              "xsd:string" },
  { substring3,  FNS, "substring",   "xsd:string?,xsd:double,xsd:double ",  "xsd:string" },
  { NULL },
};

gxfunctiondef *string_module = string_fundefs;
