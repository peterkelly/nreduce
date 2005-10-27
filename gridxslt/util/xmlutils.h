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
#include <libxml/tree.h>
#include <libxml/xmlwriter.h>
#include "stringbuf.h"
#include "String.h"
#include "list.h"

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
  QName(String prefix, String localPart);
  QName(const parse_qname &qn);

  static QName null();
  static QName parse(const char *name);
  static List<QName> parseList(const char *list);

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

class XMLWriter {
public:

  static void attribute(xmlTextWriterPtr writer, const String &name, const String &content);
  static void comment(xmlTextWriterPtr writer, const String &content);
  static void formatAttribute(xmlTextWriterPtr writer, const String &name, const char *format, ...);
  static void formatString(xmlTextWriterPtr writer, const char *format, ...);
  static void pi(xmlTextWriterPtr writer, const String &target, const String &content);
  static void raw(xmlTextWriterPtr writer, const String &content);
  static void string(xmlTextWriterPtr writer, const String &content);

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

int XMLHasProp(xmlNodePtr n, const String &name);
int XMLHasNsProp(xmlNodePtr n, const String &name, const String &ns);
char *XMLGetProp(xmlNodePtr n, const String &name);
char *XMLGetNsProp(xmlNodePtr n, const String &name, const String &ns);





int nullstr_equals(const char *a, const char *b);

NSNameTest *NSNameTest_copy(NSNameTest *test);

sourceloc sourceloc_copy(sourceloc sloc);
void sourceloc_free(sourceloc sloc);

#define nosourceloc get_nosourceloc()
sourceloc get_nosourceloc();















int get_ns_name_from_qname(xmlNodePtr n, xmlDocPtr doc, const char *name,
                           char **namespace1, char **localpart);
int error(GridXSLT::Error *ei, const GridXSLT::String &filename, int line,
          const GridXSLT::String &errname, const char *format, ...);
int parse_error(xmlNodePtr n, const char *format, ...);
int invalid_element2(GridXSLT::Error *ei, const GridXSLT::String &filename, xmlNodePtr n);
int invalid_element(xmlNodePtr n);
int missing_attribute2(GridXSLT::Error *ei, const GridXSLT::String &filename, int line,
                       const GridXSLT::String &errname,
                       const GridXSLT::String &attrname);
int missing_attribute(xmlNodePtr n, const GridXSLT::String &attrname);
int attribute_not_allowed(GridXSLT::Error *ei, const GridXSLT::String &filename, int line, const char *attrname);
int conflicting_attributes(GridXSLT::Error *ei, const GridXSLT::String &filename, int line, const GridXSLT::String &errname,
                           const GridXSLT::String &conflictor,
                           const GridXSLT::String &conflictee);
int invalid_attribute_val(GridXSLT::Error *ei, const GridXSLT::String &filename, xmlNodePtr n,
                          const GridXSLT::String &attrname);
int check_element(xmlNodePtr n, const GridXSLT::String &localname,
                  const GridXSLT::String &namespace1);

int convert_to_nonneg_int(const GridXSLT::String &str, int *val);
int parse_int_attr(GridXSLT::Error *ei, char *filename, xmlNodePtr n,
                   const GridXSLT::String &attrname, int *val);
int parse_optional_int_attr(GridXSLT::Error *ei, char *filename,
                            xmlNodePtr n, const GridXSLT::String &attrname, int *val, int def);
int parse_boolean_attr(GridXSLT::Error *ei, char *filename, xmlNodePtr n,
                       const GridXSLT::String &attrname, int *val);
int parse_optional_boolean_attr(GridXSLT::Error *ei, char *filename,
                                xmlNodePtr n, const GridXSLT::String &attrname, int *val, int def);
void replace_whitespace(char *str);
void collapse_whitespace(char *str);
char *get_wscollapsed_attr(xmlNodePtr n, const GridXSLT::String &attrname,
                           const GridXSLT::String &ns);


void xml_write_attr(xmlTextWriter *writer, const char *attrname, const char *format, ...);
QName xml_attr_qname(xmlNodePtr n, const char *attrname);
int xml_attr_strcmp(xmlNodePtr n, const char *attrname, const char *s);





GridXSLT::String escape_str(const GridXSLT::String &s);
int enforce_allowed_attributes(GridXSLT::Error *ei, const GridXSLT::String &filename, xmlNodePtr n,
                               const String &restrictns,
                               GridXSLT::List<GridXSLT::NSName> &stdattrs, int xx, ...);
int is_all_whitespace(const char *s, int len);



char *get_real_uri(const GridXSLT::String &filename);
char *get_relative_uri(const char *uri, const char *base);

xmlNodePtr get_element_by_id(xmlNodePtr n, const char *id);
int retrieve_uri_element(GridXSLT::Error *ei, const GridXSLT::String &filename, int line, const GridXSLT::String &errname,
                         const char *full_uri, xmlDocPtr *doc, xmlNodePtr *node,
                         const char *refsource);
int retrieve_uri(GridXSLT::Error *ei, const GridXSLT::String &filename, int line, const GridXSLT::String &errname,
                 const char *full_uri, stringbuf *buf, const char *refsource);


class symbol_space_entry {
  DISABLE_COPY(symbol_space_entry)
public:
  symbol_space_entry();
  ~symbol_space_entry();

  NSName ident;
  void *object;
  symbol_space_entry *next;
};

struct symbol_space {
  symbol_space *fallback;
  symbol_space_entry *entries;
  char *type;
};

symbol_space *ss_new(symbol_space *fallback, const char *type);
void *ss_lookup_local(symbol_space *ss, const NSName &ident);
void *ss_lookup(symbol_space *ss, const NSName &ident);
int ss_add(symbol_space *ss, const NSName &ident, void *object);
void ss_free(symbol_space *ss);

};







#endif /* _UTIL_XMLUTILS_H */
