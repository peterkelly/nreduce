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
#include <libxml/parser.h>
#include <libxml/tree.h>
#include <assert.h>
#include <string.h>
#include <errno.h>

using namespace GridXSLT;

#define FNS FN_NAMESPACE

static Value root1(Environment *env, List<Value> &args)
{
  Node *n;
  assert(args[0].type().type() == SEQTYPE_ITEM);
  assert(args[0].type().itemType()->m_kind != ITEM_ATOMIC);
  n = args[0].asNode();
  while (NULL != n->m_parent)
    n = n->m_parent;
  return Value(n);
}

FunctionDefinition node_fundefs[2] = {
  { root1,    FNS, "root",     "node()?",                  "node()?" },
  { NULL },
};

FunctionDefinition *node_module = node_fundefs;
