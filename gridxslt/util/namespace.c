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

#include "namespace.h"
#include "debug.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

using namespace GridXSLT;

String Namespaces::XSLT = "http://www.w3.org/1999/XSL/Transform";
String Namespaces::XHTML = "http://www.w3.org/1999/xhtml";
String Namespaces::WSDL = "http://schemas.xmlsoap.org/wsdl/";
String Namespaces::SOAP = "http://schemas.xmlsoap.org/wsdl/soap/";
String Namespaces::BPEL = "http://schemas.xmlsoap.org/ws/2003/03/business-process/";
String Namespaces::XS = "http://www.w3.org/2001/XMLSchema";
String Namespaces::XDT = "http://www.w3.org/2005/04/xpath-datatypes";
String Namespaces::FN = "http://www.w3.org/2005/04/xpath-functions";
String Namespaces::ERR = "http://www.w3.org/2004/07/xqt-errors";
String Namespaces::XSI = "http://www.w3.org/2001/XMLSchema-instance";
String Namespaces::SVG = "http://www.w3.org/2000/svg";
String Namespaces::XML = "http://www.w3.org/XML/1998/namespace";
String Namespaces::GX = "http://gridxslt.sourceforge.net";
String Namespaces::SPECIAL = "http://special";

void GridXSLT::ns_def_free(ns_def *def)
{
  free(def->prefix);
  free(def->href);
  free(def);
}

NamespaceMap::NamespaceMap()
  : defs(NULL), parent(NULL)
{
}

NamespaceMap::~NamespaceMap()
{
  list_free(defs,(list_d_t)ns_def_free);
}

NamespaceMap *GridXSLT::NamespaceMap_new()
{
  return new NamespaceMap();
}

void GridXSLT::NamespaceMap_free(NamespaceMap *map, int free_parents)
{
  if (free_parents)
    NamespaceMap_free(map->parent,1);
  delete map;
}

void NamespaceMap::add_preferred(const String &href1, const String &preferred_prefix1)
{
  char *href = href1.cstring();
  char *preferred_prefix = preferred_prefix1.cstring();
  ns_def *ns;
  char *prefix;
  int number = 1;

  if (NULL != lookup_href(href))
    return;

  if (NULL != preferred_prefix) {
    prefix = (char*)malloc(strlen(preferred_prefix)+100);
    sprintf(prefix,"%s",preferred_prefix);
  }
  else {
    preferred_prefix = "ns";
    prefix = (char*)malloc(100);
    sprintf(prefix,"ns0");
  }
  while (NULL != lookup_prefix(prefix))
    sprintf(prefix,"%s%d",preferred_prefix,number++);
  ns = (ns_def*)calloc(1,sizeof(ns_def));
  ns->prefix = prefix;
  ns->href = strdup(href);
  list_append(&defs,ns);
}

void NamespaceMap::add_direct(const String &href1, const String &prefix1)
{
  char *href = href1.cstring();
  char *prefix = prefix1.cstring();
  ns_def *ns = (ns_def*)calloc(1,sizeof(ns_def));
  ns->prefix = strdup(prefix);
  ns->href = strdup(href);
  list_append(&defs,ns);
}

ns_def *NamespaceMap::lookup_prefix(const String &prefix1)
{
  char *prefix = prefix1.cstring();
  list *l;
  for (l = defs; l; l = l->next) {
    ns_def *ns = (ns_def*)l->data;
    if (!strcmp(ns->prefix,prefix))
      return ns;
  }
  if (NULL != parent)
    return parent->lookup_prefix(prefix);
  else
    return NULL;
}

ns_def *NamespaceMap::lookup_href(const String &href1)
{
  char *href = href1.cstring();
  list *l;
  for (l = defs; l; l = l->next) {
    ns_def *ns = (ns_def*)l->data;
    if (!strcmp(ns->href,href))
      return ns;
  }
  if (NULL != parent)
    return parent->lookup_href(href);
  else
    return NULL;
}

NSName GridXSLT::qname_to_nsname(NamespaceMap *map, const QName &qn)
{
  NSName nn;
  ns_def *def;
  if (!qn.m_prefix.isNull()) {
    if (NULL == (def = map->lookup_prefix(qn.m_prefix)))
      return nn;
    nn.m_ns = strdup(def->href);
  }
  nn.m_name = qn.m_localPart.cstring();
  return nn;
}

QName GridXSLT::nsname_to_qname(NamespaceMap *map, const NSName &nn)
{
  QName qn;
  ns_def *def;
  if (!nn.m_ns.isNull()) {
    if (NULL == (def = map->lookup_href(nn.m_ns)))
      return qn;
    qn.m_prefix = strdup(def->prefix);
  }
  qn.m_localPart = nn.m_name;
  return qn;
}

NSNameTest *GridXSLT::QNameTest_to_NSNameTest(NamespaceMap *map, const QNameTest *qt)
{
  NSNameTest *nt = new NSNameTest();
  nt->wcns = qt->wcprefix;
  nt->wcname = qt->wcname;
  if (!qt->qn.m_prefix.isNull()) {
    ns_def *def;
    if (NULL == (def = map->lookup_prefix(qt->qn.m_prefix))) {
      delete nt;
      return NULL;
    }
    nt->nn.m_ns = def->href;
  }
  if (!qt->qn.m_localPart.isNull())
    nt->nn.m_name = qt->qn.m_localPart;
  return nt;
}

