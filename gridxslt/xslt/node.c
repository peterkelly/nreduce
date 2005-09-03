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

static gxvalue *root1(gxcontext *ctxt, gxvalue **args)
{
  df_node *n;
  assert(args[0]->seqtype->type == SEQTYPE_ITEM);
  assert(args[0]->seqtype->item->kind != ITEM_ATOMIC);
  n = args[0]->value.n;
  while (NULL != n->parent)
    n = n->parent;
  return mknode(n);
}

gxfunctiondef node_fundefs[2] = {
  { root1,    FNS, "root",     "node()?",                  "node()?" },
  { NULL },
};

gxfunctiondef *node_module = node_fundefs;
