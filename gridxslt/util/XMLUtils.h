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

#ifndef _UTIL_XMLUTILS_H
#define _UTIL_XMLUTILS_H

#include <stdio.h>
#include "String.h"
#include "List.h"
#include "Namespace.h"
#include "Node.h"

namespace GridXSLT {

class Node;

class XMLWriter {
public:

  static void attribute(xmlTextWriterPtr writer, const String &name, const String &content);
  static void comment(xmlTextWriterPtr writer, const String &content);
  static void formatAttribute(xmlTextWriterPtr writer, const String &name, const char *format, ...);
  static void formatString(xmlTextWriterPtr writer, const char *format, ...);
  static void pi(xmlTextWriterPtr writer, const String &target, const String &content);
  static void raw(xmlTextWriterPtr writer, const String &content);
  static void string(xmlTextWriterPtr writer, const String &content);
  static void startElement(xmlTextWriterPtr writer, const String &name);
  static void endElement(xmlTextWriterPtr writer);

};

class Error {
public:
  Error();
  ~Error();

  void fprint(FILE *f);
  void clear();

  String m_filename;
  int m_line;
  String m_message;
  String m_spec;
  String m_section;
  String m_errname;
};





int error(GridXSLT::Error *ei, const GridXSLT::String &filename, int line,
          const GridXSLT::String &errname, const char *format, ...);
int invalid_element(GridXSLT::Error *ei, const GridXSLT::String &filename, Node *n);
int missing_attribute(GridXSLT::Error *ei, const GridXSLT::String &filename, int line,
                       const GridXSLT::String &errname,
                       const GridXSLT::NSName &attrname);
int attribute_not_allowed(GridXSLT::Error *ei, const GridXSLT::String &filename, int line, const NSName &attrname);
int conflicting_attributes(GridXSLT::Error *ei, const GridXSLT::String &filename, int line, const GridXSLT::String &errname,
                           const GridXSLT::String &conflictor,
                           const GridXSLT::String &conflictee);
int invalid_attribute_val(GridXSLT::Error *ei, const GridXSLT::String &filename, Node *n,
                          const GridXSLT::NSName &attrname);

int convert_to_nonneg_int(const GridXSLT::String &str, int *val);
int parse_int_attr(GridXSLT::Error *ei, const String &filename, Node *n,
                   const GridXSLT::NSName &attrname, int *val);
int parse_optional_int_attr(GridXSLT::Error *ei, const String &filename,
                            Node *n, const GridXSLT::NSName &attrname, int *val, int def);
int parse_boolean_attr(GridXSLT::Error *ei, const String &filename, Node *n,
                       const GridXSLT::NSName &attrname, int *val);
int parse_optional_boolean_attr(GridXSLT::Error *ei, const String &filename,
                                Node *n, const GridXSLT::NSName &attrname, int *val, int def);






GridXSLT::String escape_str(const GridXSLT::String &s);
int enforce_allowed_attributes(GridXSLT::Error *ei, const GridXSLT::String &filename, xmlNodePtr n,
                               const String &restrictns,
                               GridXSLT::List<GridXSLT::NSName> &stdattrs, int xx, ...);
int is_all_whitespace(const char *s, int len);



String get_real_uri(const GridXSLT::String &filename);
String get_relative_uri(const GridXSLT::String &uri, const String &base);

int retrieve_uri_element(GridXSLT::Error *ei, const GridXSLT::String &filename,
                         int line, const GridXSLT::String &errname,
                         const GridXSLT::String &full_uri, Node **nout, Node **rootout,
                         const GridXSLT::String &refsource);
int retrieve_uri(GridXSLT::Error *ei, const GridXSLT::String &filename, int line, const GridXSLT::String &errname,
                 const char *full_uri, stringbuf *buf, const char *refsource);

String buildURI(const String &URI, const String &base);

int check_element(xmlNodePtr n, const GridXSLT::String &localname,
                  const GridXSLT::String &namespace1);

class SymbolSpaceEntry {
  DISABLE_COPY(SymbolSpaceEntry)
public:
  SymbolSpaceEntry();
  ~SymbolSpaceEntry();

  NSName ident;
  void *object;
  SymbolSpaceEntry *next;
};

class SymbolSpace {
  DISABLE_COPY(SymbolSpace)
public:
  SymbolSpace(const String &_type);
  ~SymbolSpace();

  void *lookup(const NSName &ident);
  int add(const NSName &ident, void *object);

  SymbolSpaceEntry *entries;
  String type;
};

};

#endif /* _UTIL_XMLUTILS_H */
