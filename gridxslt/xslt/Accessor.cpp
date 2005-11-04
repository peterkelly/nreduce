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

#include "dataflow/SequenceType.h"
#include "dataflow/Program.h"
#include "util/XMLUtils.h"
#include "util/Debug.h"
#include <libxml/parser.h>
#include <libxml/tree.h>
#include <string.h>
#include <errno.h>

using namespace GridXSLT;

#define FNS FN_NAMESPACE

static Value string1(Environment *env, List<Value> &args)
{
  Value result;
  if (SEQTYPE_EMPTY == args[0].type().type()) {
    result = Value("");
  }
  else {
    ASSERT(SEQTYPE_ITEM == args[0].type().type());
    result = Value(args[0].convertToString());
  }
  return result;
}

FunctionDefinition accessor_fundefs[9] = {
  { string1,  FNS, "string",            "item()?",       "xsd:string", false },
  { NULL },
};

FunctionDefinition *accessor_module = accessor_fundefs;
