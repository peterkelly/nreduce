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
#include <libxml/parser.h>
#include <libxml/tree.h>
#include <assert.h>
#include <string.h>
#include <errno.h>

#define FNS FN_NAMESPACE

static gxvalue *string1(gxenvironment *env, gxvalue **args)
{
  gxvalue *result;
  char *str;
  if (SEQTYPE_EMPTY == args[0]->seqtype->type) {
    result = mkstring("");
  }
  else {
    assert(SEQTYPE_ITEM == args[0]->seqtype->type);
    str = df_value_as_string(env->g,args[0]);
    result = mkstring(str);
    free(str);
  }
  return result;
}

gxfunctiondef accessor_fundefs[9] = {
  { string1,  FNS, "string",            "item()?",       "xsd:string" },
  { NULL },
};

gxfunctiondef *accessor_module = accessor_fundefs;
