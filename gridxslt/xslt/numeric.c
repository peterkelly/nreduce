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
#include <libxml/parser.h>
#include <libxml/tree.h>
#include <assert.h>
#include <string.h>
#include <errno.h>

using namespace GridXSLT;

#define FNS FN_NAMESPACE

static Value numeric_add(Environment *env, List<Value> &args)
{
  /* FIXME: type promotion and use appropriate typed operator */
  assert(args[0].isDerivedFrom(xs_g->int_type));
  assert(args[1].isDerivedFrom(xs_g->int_type));
  return Value(args[0].asInt() + args[1].asInt());
}

static Value numeric_subtract(Environment *env, List<Value> &args)
{
  /* FIXME: type promotion and use appropriate typed operator */
  assert(args[0].isDerivedFrom(xs_g->int_type));
  assert(args[1].isDerivedFrom(xs_g->int_type));
  return Value(args[0].asInt() - args[1].asInt());
}

static Value numeric_multiply(Environment *env, List<Value> &args)
{
  /* FIXME: type promotion and use appropriate typed operator */
  assert(args[0].isDerivedFrom(xs_g->int_type));
  assert(args[1].isDerivedFrom(xs_g->int_type));
  return Value(args[0].asInt() * args[1].asInt());
}

static Value numeric_divide(Environment *env, List<Value> &args)
{
  /* FIXME: type promotion and use appropriate typed operator */
  assert(args[0].isDerivedFrom(xs_g->int_type));
  assert(args[1].isDerivedFrom(xs_g->int_type));
  return Value(args[0].asInt() / args[1].asInt());
}

static Value numeric_integer_divide(Environment *env, List<Value> &args)
{
  /* FIXME: type promotion and use appropriate typed operator */
  assert(args[0].isDerivedFrom(xs_g->int_type));
  assert(args[1].isDerivedFrom(xs_g->int_type));
  return Value(args[0].asInt() / args[1].asInt());
}

static Value numeric_equal(Environment *env, List<Value> &args)
{
  /* FIXME: type promotion and use appropriate typed operator */
  assert(args[0].isDerivedFrom(xs_g->int_type));
  assert(args[1].isDerivedFrom(xs_g->int_type));

  if (args[0].asInt() == args[1].asInt())
    return Value(true);
  else
    return Value(false);
}

static Value numeric_less_than(Environment *env, List<Value> &args)
{
  /* FIXME: type promotion and use appropriate typed operator */
  assert(args[0].isDerivedFrom(xs_g->int_type));
  assert(args[1].isDerivedFrom(xs_g->int_type));

  if (args[0].asInt() < args[1].asInt())
    return Value(true);
  else
    return Value(false);
}

static Value numeric_greater_than(Environment *env, List<Value> &args)
{
  /* FIXME: type promotion and use appropriate typed operator */
  assert(args[0].isDerivedFrom(xs_g->int_type));
  assert(args[1].isDerivedFrom(xs_g->int_type));

  if (args[0].asInt() > args[1].asInt())
    return Value(true);
  else
    return Value(false);
}

FunctionDefinition numeric_fundefs[9] = {
  { numeric_add,            FNS, "numeric-add",
    "xsd:integer,xsd:integer",       "xsd:integer" },
  { numeric_subtract,       FNS, "numeric-subtract",
    "xsd:integer,xsd:integer",       "xsd:integer" },
  { numeric_multiply,       FNS, "numeric-multiply",
    "xsd:integer,xsd:integer",       "xsd:integer" },
  { numeric_divide,         FNS, "numeric-divide",
    "xsd:integer,xsd:integer",       "xsd:integer" },
  { numeric_integer_divide, FNS, "numeric-integer-divide",
    "xsd:integer,xsd:integer",       "xsd:integer" },
  { numeric_equal,          FNS, "numeric-equal",
    "xsd:integer,xsd:integer",       "xsd:boolean" },
  { numeric_less_than,      FNS, "numeric-less-than",
    "xsd:integer,xsd:integer",       "xsd:boolean" },
  { numeric_greater_than,   FNS, "numeric-greater-than",
    "xsd:integer,xsd:integer",       "xsd:boolean" },
  { NULL },
};

FunctionDefinition *numeric_module = numeric_fundefs;
