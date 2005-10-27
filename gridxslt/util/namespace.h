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

#include "list.h"
#include "xmlutils.h"

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

struct ns_def {
  char *prefix;
  char *href;
};

class NamespaceMap {
public:
  NamespaceMap();
  ~NamespaceMap();

  void add_preferred(const String &href1, const String &preferred_prefix1);
  void add_direct(const String &href1, const String &prefix1);
  ns_def *lookup_prefix(const String &prefix1);
  ns_def *lookup_href(const String &href1);

  list *defs;
  NamespaceMap *parent;
};

void ns_def_free(ns_def *def);

NamespaceMap *NamespaceMap_new();
void NamespaceMap_free(NamespaceMap *map, int free_parents);

NSName qname_to_nsname(NamespaceMap *map, const QName &qn);
QName nsname_to_qname(NamespaceMap *map, const NSName &nn);

NSNameTest *QNameTest_to_NSNameTest(NamespaceMap *map, const QNameTest *qt);

};

#endif /* _UTIL_NAMESPACE_H */
