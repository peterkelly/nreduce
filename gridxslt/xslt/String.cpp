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

using namespace GridXSLT;

#define FNS FN_NAMESPACE

static Value string_join(Environment *env, List<Value> &args)
{
  // @implements(xpath20:func-string-join-1) @end
  // @implements(xpath20:func-string-join-2) @end
  // @implements(xpath20:func-string-join-3) @end
  // @implements(xpath20:func-string-join-4) @end

  List<Value> values = args[0].sequenceToList();
  String separator = args[1].asString();
  StringBuffer buf;
  Value res;

  for (Iterator<Value> it = values; it.haveCurrent(); it++) {
    buf.append((*it).asString());
    if (it.haveNext())
      buf.append(separator);
  }

  return Value(buf.contents());
}

static Value substring2(Environment *env, List<Value> &args)
{
  if (args[0].isEmpty())
    return String("");

  String sourceString = args[0].asString();
  double startingLoc = xpathround(args[1].asDouble());

  return sourceString.substring((unsigned int)startingLoc);
}

static Value substring3(Environment *env, List<Value> &args)
{
  if (args[0].isEmpty())
    return String("");

  String sourceString = args[0].asString();
  double startingLoc = xpathround(args[1].asDouble());
  double length = xpathround(args[2].asDouble());

  return sourceString.substring((unsigned int)startingLoc,(unsigned int)length);
}

FunctionDefinition string_fundefs[4] = {
  { string_join, FNS, "string-join", "xsd:string*,xsd:string",              "xsd:string", false },
  { substring2,  FNS, "substring",   "xsd:string?,xsd:double",              "xsd:string", false },
  { substring3,  FNS, "substring",   "xsd:string?,xsd:double,xsd:double ",  "xsd:string", false },
  { NULL },
};

FunctionDefinition *string_module = string_fundefs;
