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

static gxvalue *empty(gxcontext *ctxt, gxvalue **args)
{
  return mkbool(SEQTYPE_EMPTY == args[0]->seqtype->type);
}

static gxvalue *count(gxcontext *ctxt, gxvalue **args)
{
  list *seq = df_sequence_to_list(args[0]);
  int count = list_count(seq);
  list_free(seq,NULL);
  return mkint(count);
}

static gxvalue *doc(gxcontext *ctxt, gxvalue **args)
{
  char *uri;
  FILE *f;
  xmlDocPtr doc;
  xmlNodePtr root;
  df_node *docnode;
  df_node *rootelem;
  df_seqtype *restype;
  gxvalue *result;
  assert(df_check_derived_atomic_type(args[0],ctxt->g->string_type));
  uri = args[0]->value.s;
/*   debugl("Loading document \"%s\"...",uri); */

  if (NULL == (f = fopen(uri,"r"))) {
    fprintf(stderr,"Can't open %s: %s\n",uri,strerror(errno));
    return NULL; /* FIXME: raise an error here */
  }

  if (NULL == (doc = xmlReadFd(fileno(f),NULL,NULL,0))) {
    fclose(f);
    fprintf(stderr,"XML parse error.\n");
    return NULL; /* FIXME: raise an error here */
  }
  fclose(f);

  if (NULL == (root = xmlDocGetRootElement(doc))) {
    fprintf(stderr,"No root element.\n");
    xmlFreeDoc(doc);
    return NULL; /* FIXME: raise an error here */
  }

  docnode = df_node_new(NODE_DOCUMENT);
  rootelem = df_node_from_xmlnode(root);
  df_strip_spaces(rootelem,ctxt->space_decls);
  df_node_add_child(docnode,rootelem);

  xmlFreeDoc(doc);

  restype = df_seqtype_new_item(ITEM_DOCUMENT);
  result = df_value_new(restype);
  df_seqtype_deref(restype);
  result->value.n = docnode;
  assert(0 == result->value.n->refcount);
  result->value.n->refcount++;

  return result;
}

gxfunctiondef sequence_fundefs[4] = {
  { empty,    FNS, "empty",     "item()*",                  "xsd:boolean" },
  { count,    FNS, "count",     "item()*",                  "xsd:integer" },
  { doc,      FNS, "doc",       "xsd:string?",              "document-node()?" },
  { NULL },
};

gxfunctiondef *sequence_module = sequence_fundefs;
