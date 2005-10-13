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

static gxvalue *numeric_add(gxenvironment *env, gxvalue **args)
{
  /* FIXME: type promotion and use appropriate typed operator */
  assert(df_check_derived_atomic_type(args[0],env->g->int_type));
  assert(df_check_derived_atomic_type(args[1],env->g->int_type));
  return mkint(args[0]->value.i + args[1]->value.i);
}

static gxvalue *numeric_subtract(gxenvironment *env, gxvalue **args)
{
  /* FIXME: type promotion and use appropriate typed operator */
  assert(df_check_derived_atomic_type(args[0],env->g->int_type));
  assert(df_check_derived_atomic_type(args[1],env->g->int_type));
  return mkint(args[0]->value.i - args[1]->value.i);
}

static gxvalue *numeric_multiply(gxenvironment *env, gxvalue **args)
{
  /* FIXME: type promotion and use appropriate typed operator */
  assert(df_check_derived_atomic_type(args[0],env->g->int_type));
  assert(df_check_derived_atomic_type(args[1],env->g->int_type));
  return mkint(args[0]->value.i * args[1]->value.i);
}

static gxvalue *numeric_divide(gxenvironment *env, gxvalue **args)
{
  /* FIXME: type promotion and use appropriate typed operator */
  assert(df_check_derived_atomic_type(args[0],env->g->int_type));
  assert(df_check_derived_atomic_type(args[1],env->g->int_type));
  return mkint(args[0]->value.i / args[1]->value.i);
}

static gxvalue *numeric_integer_divide(gxenvironment *env, gxvalue **args)
{
  /* FIXME: type promotion and use appropriate typed operator */
  assert(df_check_derived_atomic_type(args[0],env->g->int_type));
  assert(df_check_derived_atomic_type(args[1],env->g->int_type));
  return mkint(args[0]->value.i / args[1]->value.i);
}

static gxvalue *numeric_equal(gxenvironment *env, gxvalue **args)
{
  /* FIXME: type promotion and use appropriate typed operator */
  assert(df_check_derived_atomic_type(args[0],env->g->int_type));
  assert(df_check_derived_atomic_type(args[1],env->g->int_type));

  if (args[0]->value.i == args[1]->value.i)
    return mkbool(1);
  else
    return mkbool(0);
}

static gxvalue *numeric_less_than(gxenvironment *env, gxvalue **args)
{
  /* FIXME: type promotion and use appropriate typed operator */
  assert(df_check_derived_atomic_type(args[0],env->g->int_type));
  assert(df_check_derived_atomic_type(args[1],env->g->int_type));

  if (args[0]->value.i < args[1]->value.i)
    return mkbool(1);
  else
    return mkbool(0);
}

static gxvalue *numeric_greater_than(gxenvironment *env, gxvalue **args)
{
  /* FIXME: type promotion and use appropriate typed operator */
  assert(df_check_derived_atomic_type(args[0],env->g->int_type));
  assert(df_check_derived_atomic_type(args[1],env->g->int_type));

  if (args[0]->value.i > args[1]->value.i)
    return mkbool(1);
  else
    return mkbool(0);
}

gxfunctiondef numeric_fundefs[9] = {
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

gxfunctiondef *numeric_module = numeric_fundefs;
