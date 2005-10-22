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

#include "builtins.h"
#include "util/xmlutils.h"
#include "util/debug.h"
#include "xpath.h"
#include <string.h>
#include <stdlib.h>
#include <ctype.h>

static seqtype *parse_argtype(xs_schema *schema, ns_map *namespaces, const char *str, int len)
{
  error_info ei;
  seqtype *st;
  char *argtype = strdup(str);
  argtype[len] = '\0';

  memset(&ei,0,sizeof(error_info));

  st = seqtype_parse(argtype,NULL,-1,&ei);

  if ((NULL == st) || (0 != seqtype_resolve(st,namespaces,schema,NULL,-1,&ei))) {
    fprintf(stderr,"FATAL: internal function uses unknown type \"%s\"\n",argtype);
    exit(1);
  }

  free(argtype);
  return st;

#if 0
  char *argtype = strdup(str);
  char *start = argtype;
  char *end;
  int occurs;
  xs_type *type;
  seqtype *st1;
  seqtype *st2;
  char *occ;

  argtype[len] = '\0';

  while (('\0' != *start) && isspace(*start))
    start++;

  end = start;
  while (('\0' != *end) && ('*' != *end) && ('+' != *end) && ('?' != *end) && !isspace(*end))
    end++;

  occ = end;
  while (('\0' != *occ) && isspace(*occ))
    occ++;

  switch (*occ) {
  case '*':
    occurs = OCCURS_ZERO_OR_MORE;
    break;
  case '+':
    occurs = OCCURS_ONE_OR_MORE;
    break;
  case '?':
    occurs = OCCURS_OPTIONAL;
    break;
  default:
    occurs = OCCURS_ONCE;
    break;
  }

  *end = '\0';

  if (NULL == (type = xs_lookup_type(schema,nsname_temp(XS_NAMESPACE,start)))) {
    fprintf(stderr,"FATAL: internal function uses unknown type \"%s\"\n",start);
    exit(1);
  }
  free(argtype);

  st1 = seqtype_new_atomic(type);
  if (OCCURS_ONCE == occurs)
    return st1;

  st2 = seqtype_new(SEQTYPE_OCCURRENCE);
  st2->occurrence = occurs;
  st2->left = st1;
  return st2;
#endif
}

static int parse_argtype_list(xs_schema *schema, ns_map *namespaces,
                              const char *str, seqtype ***types)
{
  const char *start = str;
  const char *c = str;
  list *typelist = NULL;
  list *l;
  int count;
  int i = 0;

  *types = NULL;

  while (1) {
    if (('\0' == *c) || (',' == *c)) {
      if ((c != start) && !is_all_whitespace(start,c-start)) {
        seqtype *st = parse_argtype(schema,namespaces,start,c-start);
        list_append(&typelist,st);
      }
      start = c+1;
    }
    if ('\0' == *c)
      break;
    c++;
  }

  count = list_count(typelist);
  *types = (seqtype**)calloc(count,sizeof(seqtype*));
  for (l = typelist; l; l = l->next) {
    (*types)[i++] = (seqtype*)l->data;
  }
  list_free(typelist,NULL);

  return count;
}

void df_init_builtin_functions(xs_schema *schema, list **builtin_functions,
                               gxfunctiondef ***modules)
{
  gxfunctiondef ***modptr;
  ns_map *namespaces = xs_g->namespaces;
/*   list *l; */
  for (modptr = modules; *modptr; modptr++) {
    gxfunctiondef *fundef;
    for (fundef = **modptr; fundef->fun; fundef++) {
      df_builtin_function *bif = (df_builtin_function*)calloc(1,sizeof(df_builtin_function));
      bif->fun = fundef->fun;
      bif->ident = nsname_new(fundef->ns,fundef->name);
      bif->nargs = parse_argtype_list(schema,namespaces,fundef->arguments,&bif->argtypes);
      bif->rettype = parse_argtype(schema,namespaces,fundef->returns,strlen(fundef->returns));
      list_push(builtin_functions,bif);

    }
  }

  /*
  for (l = *builtin_functions; l; l = l->next) {
    df_builtin_function *bif = (df_builtin_function*)l->data;
    stringbuf *buf = stringbuf_new(buf);
    int i;
    print("bif %p: fun=%p, ident=%#n nargs=%d\n",bif,bif->fun,bif->ident,bif->nargs);
    for (i = 0; i < bif->nargs; i++) {
      seqtype_print_fs(buf,bif->argtypes[i],xs_g->namespaces);
      print("arg %d: %s\n",i,buf->data);
      stringbuf_clear(buf);
    }
    seqtype_print_fs(buf,bif->rettype,xs_g->namespaces);
    print("returns: %s\n",buf->data);
    stringbuf_clear(buf);
    print("\n");
    stringbuf_free(buf);
  }
  */
}

