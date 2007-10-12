/*
 * This file is part of the nreduce project
 * Copyright (C) 2007 Peter Kelly (pmk@cs.adelaide.edu.au)
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
 * $Id: cxslt.c 639 2007-09-27 07:25:01Z pmkelly $
 *
 */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>
#include <assert.h>
#include <libxml/parser.h>
#include "cxslt.h"

int attr_equals(xmlNodePtr n, const char *name, const char *value)
{
  int equals = 0;
  if (xmlHasProp(n,name)) {
    char *str = xmlGetProp(n,name);
    if (!strcmp(str,value))
      equals = 1;
    free(str);
  }
  return equals;
}

const char *lookup_nsuri(xmlNodePtr n, const char *prefix)
{
  xmlNs *ns;
/*   array_printf(gen->buf,"\n\nlooking up prefix \"%s\" in node %s\n",prefix,n->name); */
  for (; n && (XML_ELEMENT_NODE == n->type); n = n->parent) {
/*     array_printf(gen->buf,"node: %p %s\n",n,n->name); */
    for (ns = n->nsDef; ns; ns = ns->next) {
/*       array_printf(gen->buf,"ns: %s %s\n",ns->prefix,ns->href); */
      if (!xmlStrcmp(ns->prefix,prefix)) {
        return (const char*)ns->href;
      }
    }
  }

  if (!strcmp(prefix,""))
    return "";

  fprintf(stderr,"line %d: invalid namespace prefix \"%s\"\n",n->line,prefix);
  exit(1);
}

qname string_to_qname(const char *str, xmlNodePtr n)
{
  char *copy = strdup(str);
  char *colon = strchr(copy,':');
  qname qn;
  if (colon) {
    *colon = '\0';
    qn.uri = strdup(lookup_nsuri(n,copy));
    qn.prefix = strdup(copy);
    qn.localpart = strdup(colon+1);
  }
  else {
    qn.uri = strdup(lookup_nsuri(n,""));
    qn.prefix = strdup("");
    qn.localpart = strdup(copy);
  }
  free(copy);
  return qn;
}

void free_qname(qname qn)
{
  free(qn.uri);
  free(qn.prefix);
  free(qn.localpart);
}

int nullstrcmp(const char *a, const char *b)
{
  if ((NULL == a) && (NULL == b))
    return 0;
  else if (NULL == a)
    return -1;
  else if (NULL == b)
    return 1;
  else
    return strcmp(a,b);
}

qname get_qname_attr(xmlNodePtr n, const char *attrname)
{
  char *str = xmlGetProp(n,attrname);
  qname qn = string_to_qname(str,n);
  free(str);
  return qn;
}

int is_element(xmlNodePtr n, const char *ns, const char *name)
{
  return ((XML_ELEMENT_NODE == n->type) &&
          (NULL != n->ns) &&
          !xmlStrcmp(n->ns->href,ns) &&
          !xmlStrcmp(n->name,name));
}

static char *string_to_ident(const char *str)
{
  const char *hexdigits = "0123456789ABCDEF";
  const char *c;
  int len = 0;
  int pos = 0;
  char *ret;

  for (c = str; '\0' != *c; c++) {
    if ((('A' <= *c) && ('Z' >= *c)) ||
        (('a' <= *c) && ('z' >= *c)) ||
        (('0' <= *c) && ('9' >= *c)))
      len++;
    else
      len += 3;
  }

  ret = (char*)malloc(len+1);

  for (c = str; '\0' != *c; c++) {
    if ((('A' <= *c) && ('Z' >= *c)) ||
        (('a' <= *c) && ('z' >= *c)) ||
        (('0' <= *c) && ('9' >= *c))) {
      ret[pos++] = *c;
    }
    else {
      ret[pos++] = '_';
      ret[pos++] = hexdigits[(*c)&0x0F];
      ret[pos++] = hexdigits[((*c)&0xF0)>>4];
    }
  }
  ret[pos] = '\0';

  return ret;
}

char *nsname_to_ident(const char *nsuri, const char *localname)
{
  if (strcmp(nsuri,"")) {
    int add = !strncmp(nsuri,"http://",7) ? 7 : 0;
    char *nsident = string_to_ident(nsuri+add);
    char *lnident = string_to_ident(localname);
    char *full = (char*)malloc(1+strlen(nsident)+1+strlen(lnident)+1);
    sprintf(full,"V%s_%s",nsident,lnident);
    free(nsident);
    free(lnident);
    return full;
  }
  else {
    char *lnident = string_to_ident(localname);
    char *full = (char*)malloc(1+strlen(lnident)+1);
    sprintf(full,"V%s",lnident);
    free(lnident);
    return full;
  }
}

int qname_equals(qname a, qname b)
{
  return (!nullstrcmp(a.uri,b.uri) && !nullstrcmp(a.localpart,b.localpart));
}

qname copy_qname(qname orig)
{
  qname copy;
  copy.uri = orig.uri ? strdup(orig.uri) : NULL;
  copy.prefix = orig.prefix ? strdup(orig.prefix) : NULL;
  copy.localpart = orig.localpart ? strdup(orig.localpart) : NULL;
  return copy;
}
