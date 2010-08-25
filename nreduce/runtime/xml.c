/*
 * This file is part of the NReduce project
 * Copyright (C) 2006-2010 Peter Kelly <kellypmk@gmail.com>
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
 * $Id: builtins.c 897 2010-02-25 13:21:55Z pmkelly $
 *
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#define BUILTINS_C

#include "src/nreduce.h"
#include "compiler/source.h"
#include "compiler/bytecode.h"
#include "network/util.h"
#include "runtime.h"
#include "network/node.h"
#include "messages.h"
#include "events.h"
#include "cxslt/xslt.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <assert.h>
#include <ctype.h>
#include <stdarg.h>
#include <math.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <dirent.h>
#include <arpa/inet.h>
#include <libxml/parser.h>

int TYPE_STRING     = 0;
int TYPE_BOOLEAN    = 1;
int TYPE_NUMBER     = 2;
int TYPE_QNAME      = 3;
int UNTYPED_ATOMIC  = 4;

int MAX_ATOMIC_TYPE = 4;

int TYPE_ELEMENT    = 5;
int TYPE_TEXT       = 6;
int TYPE_COMMENT    = 7;
int TYPE_ATTRIBUTE  = 8;
int TYPE_NAMESPACE  = 9;
int TYPE_DOCUMENT   = 10;
int TYPE_PI         = 11;

int TYPE_ERROR      = 12;

int TOKEN_STARTELEM = 0;
int TOKEN_ENDELEM   = 1;
int TOKEN_TEXT      = 2;
int TOKEN_ATTRNAME  = 3;
int TOKEN_ATTRVALUE = 4;
int TOKEN_COMMENT   = 5;
int TOKEN_PITARGET  = 6;
int TOKEN_PICONTENT = 7;

int TYPE_POS = 0;
int VALUE_POS = 1;
int ROOT_POS = 2;
int PARENT_POS = 3;
int PREV_POS = 4;
int NEXT_POS = 5;
int NSURI_POS = 6;
int NSPREFIX_POS = 7;
int LOCALNAME_POS = 8;
int ATTRIBUTES_POS = 9;
int NAMESPACES_POS = 10;
int CHILDREN_POS = 11;

static char *strip(char *str)
{
  int start = 0;
  int end = strlen(str);

  while (str[start] && isspace(str[start]))
    start++;
  while ((end > start) && isspace(str[end-1]))
    end--;
  char *res = (char*)malloc(end-start+1);
  memcpy(res,&str[start],end-start);
  res[end-start] = '\0';
  return res;
}

static pntr mkitem(task *tsk,
                   pntr type, pntr value, pntr root, pntr parent, pntr prev, pntr next,
                   pntr nsuri, pntr nsprefix, pntr localname, pntr attributes, pntr namespaces,
                   pntr children)
{
  pntr pointers[12] = { type, value, root, parent,
                        prev, next, nsuri, nsprefix,
                        localname, attributes, namespaces, children };
  return pointers_to_list(tsk,pointers,12,tsk->globnilpntr);
}

static pntr traverse(task *tsk, xmlNodePtr n, int depth);

static pntr traverse_list(task *tsk, xmlNodePtr list, int depth)
{
  xmlNodePtr c;
  int nchildren = 0;
  for (c = list; c; c = c->next)
    nchildren++;
  pntr aref = tsk->globnilpntr;
  if (nchildren > 0) {
    aref = create_array(tsk,sizeof(pntr),nchildren);
    cell *refcell = get_pntr(aref);
    for (c = list; c; c = c->next) {
      pntr childp = traverse(tsk,c,depth+1);
      carray_append(tsk,&refcell,&childp,1,sizeof(pntr));
    }
  }
  return aref;
}

static pntr traverse(task *tsk, xmlNodePtr n, int depth)
{
  pntr nil = tsk->globnilpntr;
  switch (n->type) {
  case XML_ELEMENT_NODE: {
    pntr attributes = traverse_list(tsk,(xmlNodePtr)n->properties,depth+1);
    pntr children = traverse_list(tsk,n->children,depth+1);

    double type = TYPE_ELEMENT;
    pntr typep = *((pntr*)&type);
    pntr namep = string_to_array(tsk,(char*)n->name);
    pntr elemp = mkitem(tsk,
                        typep, // type
                        nil, // value
                        nil, // root
                        nil, // parent
                        nil, // prev
                        nil, // next
                        nil, // nsuri
                        nil, // nsprefix
                        namep, // localname
                        attributes, // attributes
                        nil, // namespaces
                        children); // children
    return elemp;
  }
  case XML_ATTRIBUTE_NODE: {
    assert(n->children);
    double type = TYPE_ATTRIBUTE;
    pntr typep = *((pntr*)&type);
    pntr namep = string_to_array(tsk,(char*)n->name);
    pntr valuep = string_to_array(tsk,(char*)n->children->content);
    pntr attrp = mkitem(tsk,
                        typep, // type
                        valuep, // value
                        nil, // root
                        nil, // parent
                        nil, // prev
                        nil, // next
                        nil, // nsuri
                        nil, // nsprefix
                        namep, // localname
                        nil, // attributes
                        nil, // namespaces
                        nil); // children
    return attrp;
    break;
  }
  case XML_TEXT_NODE: {
    char *stripped = strip((char*)n->content);
    double type = TYPE_TEXT;
    pntr typep = *((pntr*)&type);
    pntr valuep = string_to_array(tsk,stripped);
    pntr textp = mkitem(tsk,
                        typep, // type
                        valuep, // value
                        nil, // root
                        nil, // parent
                        nil, // prev
                        nil, // next
                        nil, // nsuri
                        nil, // nsprefix
                        nil, // localname
                        nil, // attributes
                        nil, // namespaces
                        nil); // children
    free(stripped);
    return textp;
  }
  default:
    fprintf(stderr,"Unknown element type\n");
    exit(1);
    break;
  }
  return tsk->globnilpntr;
}

static void set_root(pntr node, pntr root)
{
  assert(CELL_AREF == pntrtype(node));
  assert(0 == aref_index(node));
  assert(12 == aref_array(node)->size);
  ((pntr*)aref_array(node)->elements)[ROOT_POS] = root;
}

static void set_parent(pntr node, pntr parent)
{
  assert(CELL_AREF == pntrtype(node));
  assert(0 == aref_index(node));
  assert(12 == aref_array(node)->size);
  ((pntr*)aref_array(node)->elements)[PARENT_POS] = parent;
}

static void set_prev(pntr node, pntr prev)
{
  assert(CELL_AREF == pntrtype(node));
  assert(0 == aref_index(node));
  assert(12 == aref_array(node)->size);
  ((pntr*)aref_array(node)->elements)[PREV_POS] = prev;
}

static void set_next(pntr node, pntr next)
{
  assert(CELL_AREF == pntrtype(node));
  assert(0 == aref_index(node));
  assert(12 == aref_array(node)->size);
  ((pntr*)aref_array(node)->elements)[NEXT_POS] = next;
}

static pntr get_children(pntr node)
{
  assert(CELL_AREF == pntrtype(node));
  assert(0 == aref_index(node));
  assert(12 == aref_array(node)->size);
  return ((pntr*)aref_array(node)->elements)[CHILDREN_POS];
}

static void set_parents(task *tsk, pntr node, pntr root, pntr parent)
{
  assert(CELL_AREF == pntrtype(node));
  assert(0 == aref_index(node));
  assert(12 == aref_array(node)->size);
  set_root(node,root);
  set_parent(node,parent);
  pntr children = get_children(node);
  pntr prev = tsk->globnilpntr;
  while (CELL_AREF == pntrtype(children)) {
    carray *arr = aref_array(children);
    int i;
    for (i = 0; i < arr->size; i++) {
      pntr child = ((pntr*)arr->elements)[i];
      set_parents(tsk,child,root,node);
      set_prev(child,prev);
      if (CELL_NIL != pntrtype(prev))
        set_next(prev,child);
      prev = child;
    }
    children = aref_tail(children);
  }
}

void b_parsexmlfile(task *tsk, pntr *argstack)
{
  pntr nil = tsk->globnilpntr;
  pntr filenamepntr = argstack[0];
  char *filename;

  if (0 > array_to_string(filenamepntr,&filename)) {
    set_error(tsk,"parsexmlfile: filename is not a string");
    return;
  }

  int fd = open(filename,O_RDONLY);
  if (0 > fd) {
    set_error(tsk,"parsexmlfile: Can't open %s",filename);
    return;
  }

  int len = 0;
  int bufsize = 65536;
  char *data = malloc(bufsize);
  int r;
  while (0 < (r = read(fd,&data[len],bufsize))) {
    len += r;
    data = realloc(data,len+bufsize);
  }
  data = realloc(data,len+1);
  data[len] = '\0';
  close(fd);

  xmlDocPtr doc = xmlReadMemory(data,len,NULL,NULL,0);

  free(data);

  xmlNodePtr root = xmlDocGetRootElement(doc);
  pntr rootp = traverse(tsk,root,0);


  pntr children = pointers_to_list(tsk,&rootp,1,tsk->globnilpntr);

  double type = TYPE_DOCUMENT;
  pntr typep = *((pntr*)&type);
  pntr docp = mkitem(tsk,
                     typep, // type
                     nil, // value
                     nil, // root
                     nil, // parent
                     nil, // prev
                     nil, // next
                     nil, // nsuri
                     nil, // nsprefix
                     nil, // localname
                     nil, // attributes
                     nil, // namespaces
                     children); // children

  set_parents(tsk,docp,docp,nil);

/*   printf("Done building tree\n"); */

  /* Don't free the document - seems to cause a crash under Linux if this is not
     commented out :( */
/*   xmlFreeDoc(doc); */


  argstack[0] = docp;
}
