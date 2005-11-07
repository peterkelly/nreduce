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

static Value seqempty(Environment *env, List<Value> &args)
{
  return Value(SEQTYPE_EMPTY == args[0].type().type());
}

static Value count(Environment *env, List<Value> &args)
{
  return Value((int)args[0].sequenceToList().count());
}

static Value doc(Environment *env, List<Value> &args)
{
  FILE *f;
  xmlDocPtr doc;
  xmlNodePtr root;
  Node *docnode;
  Node *rootelem;
  SequenceType restype;
  Value result;
/*   debugl("Loading document \"%s\"...",args[0].asString()); */

  String fn = args[0].asString();
  char *filename = fn.cstring();
  if (NULL == (f = fopen(filename,"r"))) {
    fmessage(stderr,"Can't open %*: %s\n",&fn,strerror(errno));
    free(filename);
    return Value::null(); /* FIXME: raise an error here */
  }
  free(filename);

  if (NULL == (doc = xmlReadFd(fileno(f),NULL,NULL,0))) {
    fclose(f);
    fmessage(stderr,"XML parse error.\n");
    return Value::null(); /* FIXME: raise an error here */
  }
  fclose(f);

  if (NULL == (root = xmlDocGetRootElement(doc))) {
    fmessage(stderr,"No root element.\n");
    xmlFreeDoc(doc);
    return Value::null(); /* FIXME: raise an error here */
  }

  docnode = new Node(NODE_DOCUMENT);
  rootelem = new Node(root);
  df_strip_spaces(rootelem,env->space_decls);
  docnode->addChild(rootelem);

  xmlFreeDoc(doc);

  return Value(docnode);
}

FunctionDefinition sequence_fundefs[6] = {
  { seqempty, FNS, "empty",     "item()*",                  "xsd:boolean", false },
  { count,    FNS, "count",     "item()*",                  "xsd:integer", false },
  { doc,      FNS, "doc",       "xsd:string?",              "document-node()?", false },
  { count,    FNS, "foo",  "item()*,item()*,item()*,item()*",   "xsd:integer", false },
  { count,    FNS, "cfoo",  "item()*,item()*,item()*,item()*",   "xsd:integer", true },
  { NULL },
};

FunctionDefinition *sequence_module = sequence_fundefs;
