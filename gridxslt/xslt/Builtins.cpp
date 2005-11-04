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

#include "Builtins.h"
#include "util/XMLUtils.h"
#include "util/Debug.h"
#include "Expression.h"
#include <string.h>
#include <stdlib.h>
#include <ctype.h>

using namespace GridXSLT;

static SequenceType parse_argtype(Schema *schema, NamespaceMap *namespaces, const char *str, int len)
{
  Error ei;
  SequenceType st;
  char *argtype = strdup(str);
  argtype[len] = '\0';

  st = seqtype_parse(argtype,NULL,-1,&ei);

  if (st.isNull() || (0 != st.resolve(namespaces,schema,NULL,-1,&ei))) {
    fmessage(stderr,"FATAL: internal function uses unknown type \"%s\"\n",argtype);
    exit(1);
  }

  free(argtype);
  return st;
}

static int parse_argtype_list(Schema *schema, NamespaceMap *namespaces,
                              const char *str, List<SequenceType> types)
{
  const char *start = str;
  const char *c = str;

  while (1) {
    if (('\0' == *c) || (',' == *c)) {
      if ((c != start) && !is_all_whitespace(start,c-start)) {
        SequenceType st = parse_argtype(schema,namespaces,start,c-start);
        types.append(st);
      }
      start = c+1;
    }
    if ('\0' == *c)
      break;
    c++;
  }

  return types.count();
}

void GridXSLT::df_init_builtin_functions(Schema *schema,
                                         List<BuiltinFunction*> &builtin_functions,
                                         FunctionDefinition ***modules)
{
  FunctionDefinition ***modptr;
  NamespaceMap *namespaces = xs_g->namespaces;
/*   list *l; */
  for (modptr = modules; *modptr; modptr++) {
    FunctionDefinition *fundef;
    for (fundef = **modptr; fundef->fun; fundef++) {
      BuiltinFunction *bif = new BuiltinFunction();
      bif->m_fun = fundef->fun;
      bif->m_ident = NSName(fundef->ns,fundef->name);
      bif->m_nargs = parse_argtype_list(schema,namespaces,fundef->arguments,bif->m_argtypes);
      bif->m_context = fundef->context;

      if ((0 == bif->m_nargs) && !bif->m_context) {
        fmessage(stderr,"FATAL: internal function %* must require context since it has "
                 "no arguments\n",&bif->m_ident);
        exit(1);
      }

      bif->m_rettype = parse_argtype(schema,namespaces,fundef->returns,strlen(fundef->returns));
      builtin_functions.push(bif);

    }
  }
}

