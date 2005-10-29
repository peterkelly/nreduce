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

#ifndef _UTIL_NAMESPACE_H
#define _UTIL_NAMESPACE_H

#include "List.h"
#include "String.h"
#include <libxml/tree.h>
#include <libxml/xmlwriter.h>

#define XSLT_NAMESPACE Namespaces::XSLT
#define XHTML_NAMESPACE Namespaces::XHTML
#define WSDL_NAMESPACE Namespaces::WSDL
#define SOAP_NAMESPACE Namespaces::SOAP
#define BPEL_NAMESPACE Namespaces::BPEL
#define XS_NAMESPACE Namespaces::XS
#define XDT_NAMESPACE Namespaces::XDT
#define FN_NAMESPACE Namespaces::FN
#define ERR_NAMESPACE Namespaces::ERR
#define XSI_NAMESPACE Namespaces::XSI
#define SVG_NAMESPACE Namespaces::SVG
#define XML_NAMESPACE Namespaces::XML
#define GX_NAMESPACE Namespaces::GX
#define SPECIAL_NAMESPACE Namespaces::SPECIAL

namespace GridXSLT {

class QName;
class NSName;

struct parse_qname {
  char *prefix;
  char *localpart;
};

struct sourceloc {
  char *uri;
  int line;
};

class QName : public Printable {
public:
  QName();
  QName(const String &prefix, const String &localPart);
  QName(const parse_qname &qn);

  static QName null();
  static QName parse(const String &name);
  static List<QName> parseList(const String &list);

  virtual void print(StringBuffer &buf);

  bool isNull() const { return m_localPart.isNull(); }

  String m_prefix;
  String m_localPart;
};

bool operator==(const QName &a, const QName &b);
inline bool operator!=(const QName &a, const QName &b) { return !(a==b); }

class NSName : public Printable {
public:
  NSName();
  NSName(String ns, String name);

  static NSName null();

  virtual void print(StringBuffer &buf);

  bool isNull() const { return m_name.isNull(); }

  String m_ns;
  String m_name;
};

bool operator==(const NSName &a, const NSName &b);
inline bool operator!=(const NSName &a, const NSName &b) { return !(a==b); }

class QNameTest {
public:
  QNameTest();
  ~QNameTest();

  QName qn;
  int wcprefix;
  int wcname;
};

class NSNameTest {
public:
  NSNameTest();
  ~NSNameTest();

  bool matches(const NSName &nn1);

  NSName nn;
  int wcns;
  int wcname;
};

class definition {
public:
  definition();
  ~definition();

  NSName ident;
  sourceloc loc;
};

class Namespaces {
public:
  static String XSLT;
  static String XHTML;
  static String WSDL;
  static String SOAP;
  static String BPEL;
  static String XS;
  static String XDT;
  static String FN;
  static String ERR;
  static String XSI;
  static String SVG;
  static String XML;
  static String GX;
  static String SPECIAL;
};

class ns_def {
public:
  ns_def(const String &_prefix, const String &_href) : prefix(_prefix), href(_href) { }
  String prefix;
  String href;
};

class NamespaceMap {
public:
  NamespaceMap();
  ~NamespaceMap();

  void add_preferred(const String &href1, const String &preferred_prefix1);
  void add_direct(const String &href, const String &prefix);
  ns_def *lookup_prefix(const String &prefix);
  ns_def *lookup_href(const String &href);

  ManagedPtrList<ns_def*> defs;
  NamespaceMap *parent;
};

void ns_def_free(ns_def *def);

void NamespaceMap_free(NamespaceMap *map, int free_parents);

NSName qname_to_nsname(NamespaceMap *map, const QName &qn);
QName nsname_to_qname(NamespaceMap *map, const NSName &nn);

NSNameTest *QNameTest_to_NSNameTest(NamespaceMap *map, const QNameTest *qt);

NSNameTest *NSNameTest_copy(NSNameTest *test);

sourceloc sourceloc_copy(sourceloc sloc);
void sourceloc_free(sourceloc sloc);

#define nosourceloc get_nosourceloc()
sourceloc get_nosourceloc();

int get_ns_name_from_qname(xmlNodePtr n, xmlDocPtr doc, const char *name,
                           char **namespace1, char **localpart);

};

#endif /* _UTIL_NAMESPACE_H */
