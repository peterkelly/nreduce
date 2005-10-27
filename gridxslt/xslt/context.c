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
#include "util/xmlutils.h"
#include <assert.h>

#define FNS FN_NAMESPACE

using namespace GridXSLT;

static Value position(Environment *env, List<Value> &args)
{
  if (!env->ctxt->havefocus) {
    error(env->ei,env->sloc.uri,env->sloc.line,"FONC0001","No context item");
    return Value::null();
  }

  return Value(env->ctxt->position);
}

static Value last(Environment *env, List<Value> &args)
{
  if (!env->ctxt->havefocus) {
    error(env->ei,env->sloc.uri,env->sloc.line,"FONC0001","No context item");
    return Value::null();
  }

  return Value(env->ctxt->size);
}

FunctionDefinition context_fundefs[3] = {
  { position,    FNS, "position",    "",                                    "xsd:integer" },
  { last,        FNS, "last",        "",                                    "xsd:integer" },
  { NULL },
};

FunctionDefinition *context_module = context_fundefs;
