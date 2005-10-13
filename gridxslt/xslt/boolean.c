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

static gxvalue *not(gxenvironment *env, gxvalue **args)
{
  /* FIXME: we can't assume the input is a boolean! arg def is item()*; we have to reduce it
     to a boolean ourselves */
  assert(df_check_derived_atomic_type(args[0],env->g->boolean_type));
  return mkbool(!asbool(args[0]));
}

gxfunctiondef boolean_fundefs[2] = {
  { not,    FNS, "not",     "item()*",                  "xsd:boolean" },
  { NULL },
};

gxfunctiondef *boolean_module = boolean_fundefs;
