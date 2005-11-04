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

static Value root0(Environment *env, List<Value> &args)
{
  Context *c = args[0].asContext();

  if (!c->havefocus) {
    error(env->ei,env->sloc.uri,env->sloc.line,"FONC0001","undefined context item");
    return Value::null();
  }

  if (!c->item.isNode()) {
    error(env->ei,env->sloc.uri,env->sloc.line,"XPTY0006","context item is not a node");
    return Value::null();
  }

  return c->item.asNode()->root();
}

static Value root1(Environment *env, List<Value> &args)
{
  // FIXME: handle empty sequence
  return args[0].asNode()->root();
}

FunctionDefinition node_fundefs[3] = {
  { root1,    FNS, "root",     "node()?",                  "node()?", false },
  { root0,    FNS, "root",     "",                         "node()", true },
  { NULL },
};

FunctionDefinition *node_module = node_fundefs;
