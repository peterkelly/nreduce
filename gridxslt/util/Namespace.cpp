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

#include "Namespace.h"
#include "Debug.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <ctype.h>

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

QName::QName()
{
}

QName::QName(const String &prefix, const String &localPart)
  : m_prefix(prefix), m_localPart(localPart)
{
}

QName::QName(const String &localPart)
  : m_localPart(localPart)
{
}

QName::QName(const parse_qname &qn)
  : m_prefix(qn.prefix), m_localPart(qn.localpart)
{
  free(qn.prefix);
  free(qn.localpart);
}

QName QName::null()
{
  return QName(String::null(),String::null());
}

QName QName::parse(const String &name1)
{
  char *name = name1.cstring();
  char *prefix = NULL;
  char *localPart = xmlSplitQName2(name,&prefix);
  if (NULL == localPart)
    localPart = strdup(name);
  QName qn(prefix,localPart);
  free(prefix);
  free(localPart);
  free(name);
  return qn;
}

List<QName> QName::parseList(const String &list1)
{
  char *list = list1.cstring();
  List<QName> qnames;
  const char *start = list;
  const char *c = list;

  while (1) {
    if ('\0' == *c || isspace(*c)) {
      if (c != start) {
        char *str = (char*)malloc(c-start+1);
        memcpy(str,start,c-start);
        str[c-start] = '\0';
        qnames.append(QName::parse(str));
        free(str);
      }
      start = c+1;
    }
    if ('\0' == *c)
      break;
    c++;
  }
  free(list);
  return qnames;
}

void QName::print(StringBuffer &buf)
{
  if (isNull())
    return;
  else if (!m_prefix.isNull())
    buf.format("%*:%*",&m_prefix,&m_localPart);
  else
    buf.format("%*",&m_localPart);
}

bool GridXSLT::operator==(const QName &a, const QName &b)
{
  return ((a.m_prefix == b.m_prefix) && (a.m_localPart == b.m_localPart));
}

NSName::NSName()
{
}

NSName::NSName(const String &ns, const String &name)
  : m_ns(ns), m_name(name)
{
}

NSName::NSName(const String &name)
  : m_name(name)
{
}

NSName NSName::null()
{
  return NSName(String::null(),String::null());
}

void NSName::print(StringBuffer &buf)
{
  if (isNull())
    return;
  else if (!m_ns.isNull())
    buf.format("{%*}%*",&m_ns,&m_name);
  else
    buf.format("%*",&m_name);
}

bool GridXSLT::operator==(const NSName &a, const NSName &b)
{
  return ((a.m_ns == b.m_ns) && (a.m_name == b.m_name));
}

QNameTest::QNameTest()
  : wcprefix(false), wcname(false)
{
}

QNameTest::QNameTest(const String &str)
  : wcprefix(false), wcname(false)
{
  String prefix;
  String localpart;

  int colonpos = str.indexOf(":");
  if (0 > colonpos) {
    prefix = String::null();
    localpart = str;
  }
  else {
    prefix = str.substring(0,colonpos);
    localpart = str.substring(colonpos+1);
  }

  if (!prefix.isNull()) {
    if (prefix == "*")
      wcprefix = 1;
    else
      qn.m_prefix = prefix;
  }

  if (localpart == "*") {
    if (prefix.isNull())
      wcprefix = 1;
    wcname = 1;
  }
  else {
    qn.m_localPart = localpart;
  }
}

QNameTest::~QNameTest()
{
}

NSNameTest::NSNameTest()
  : wcns(0), wcname(0)
{
}

NSNameTest::~NSNameTest()
{
}

bool NSNameTest::matches(const NSName &nn1)
{
  if (wcns && wcname)
    return 1;
  else if (wcns)
    return (nn.m_name == nn1.m_name);
  else if (wcname)
    return (nn.m_ns == nn1.m_ns);
  else
    return (nn == nn1);
}

definition::definition()
{
}

definition::~definition()
{
}

void GridXSLT::ns_def_free(ns_def *def)
{
  free(def);
}

NamespaceMap::NamespaceMap()
  : parent(NULL)
{
}

NamespaceMap::~NamespaceMap()
{
}

void NamespaceMap::add_preferred(const String &href1, const String &preferred_prefix1)
{
  String href = href1;
  String preferred_prefix = preferred_prefix1;
  String prefix;
  int number = 1;

  if (NULL != lookup_href(href))
    return;

  if (!preferred_prefix.isNull()) {
    prefix = preferred_prefix;
  }
  else {
    preferred_prefix = "ns";
    prefix = "ns0";
  }
  while (NULL != lookup_prefix(prefix))
    prefix = String::format("%*%d",&preferred_prefix,number++);
  defs.append(new ns_def(prefix,href));
}

void NamespaceMap::add_direct(const String &href, const String &prefix)
{
  defs.append(new ns_def(prefix,href));
}

ns_def *NamespaceMap::lookup_prefix(const String &prefix)
{
  for (Iterator<ns_def*> it = defs; it.haveCurrent(); it++) {
    ns_def *ns = *it;
    if (ns->prefix == prefix)
      return ns;
  }
  if (NULL != parent)
    return parent->lookup_prefix(prefix);
  else
    return NULL;
}

ns_def *NamespaceMap::lookup_href(const String &href)
{
  for (Iterator<ns_def*> it = defs; it.haveCurrent(); it++) {
    ns_def *ns = *it;
    if (ns->href == href)
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
    nn.m_ns = def->href;
  }
  nn.m_name = qn.m_localPart;
  return nn;
}

QName GridXSLT::nsname_to_qname(NamespaceMap *map, const NSName &nn)
{
  QName qn;
  ns_def *def;
  if (!nn.m_ns.isNull()) {
    if (NULL == (def = map->lookup_href(nn.m_ns)))
      return qn;
    qn.m_prefix = def->prefix;
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

NSNameTest *GridXSLT::NSNameTest_copy(NSNameTest *test)
{
  NSNameTest *copy = new NSNameTest();
  *copy = *test;
  return copy;
}

sourceloc GridXSLT::sourceloc_copy(sourceloc sloc)
{
  sourceloc copy;
  copy.uri = sloc.uri;
  copy.line = sloc.line;
  return copy;
}

void GridXSLT::sourceloc_free(sourceloc sloc)
{
}

const sourceloc nosourceloc1 = sourceloc();

sourceloc GridXSLT::get_nosourceloc()
{
  return nosourceloc1;
}

int GridXSLT::get_ns_name_from_qname(xmlNodePtr n, xmlDocPtr doc, const char *name,
                           char **namespace1, char **localpart)
{
  QName qn = QName::parse(name);
  xmlNsPtr ns = NULL;

  *namespace1 = NULL;
  *localpart = NULL;

  /* FIXME: maybe set error info here instead of requiring thet caller to do it? */
  char *prefix = qn.m_prefix.cstring();
  if ((NULL == (ns = xmlSearchNs(doc,n,prefix))) && (NULL != prefix)) {
    free(prefix);
    return -1;
  }
  free(prefix);
  *namespace1 = ns ? strdup(ns->href) : NULL;
  *localpart = qn.m_localPart.cstring();

  return 0;
}

