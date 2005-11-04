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
#include "util/debug.h"
#include "special.h"
#include <libxml/parser.h>
#include <libxml/tree.h>
#include <string.h>
#include <errno.h>

using namespace GridXSLT;

#define FNS FN_NAMESPACE

static Value not1(Environment *env, List<Value> &args)
{
  Value v1;
  List<Value> arg0;
  arg0.append(args[0]);
  v1 = ebv(env,arg0);
  if (v1.isNull())
    return Value::null();

  return Value(!v1.asBool());
}

static Value true0(Environment *env, List<Value> &args)
{
  return Value(true);
}

static Value false0(Environment *env, List<Value> &args)
{
  return Value(false);
}

FunctionDefinition boolean_fundefs[4] = {
  { not1,    FNS, "not",     "item()*",                  "xsd:boolean", false },
  { true0,   FNS, "true",    "",                         "xsd:boolean", true },
  { false0,  FNS, "false",   "",                         "xsd:boolean", true },
  { NULL },
};

FunctionDefinition *boolean_module = boolean_fundefs;
