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

typedef struct error_info error_info;
typedef struct qname qname;
typedef struct nsname nsname;
typedef struct qnametest qnametest;
typedef struct nsnametest nsnametest;
typedef struct sourceloc sourceloc;
typedef struct definition definition;

struct error_info {
  char *filename;
  int line;
  char *message;
  char *spec;
  char *section;
  char *errname;
};

struct qname {
  char *prefix;
  char *localpart;
};

struct nsname {
  char *ns;
  char *name;
};

struct qnametest {
  qname qn;
  int wcprefix;
  int wcname;
};

struct nsnametest {
  nsname nn;
  int wcns;
  int wcname;
};

struct sourceloc {
  char *uri;
  int line;
};

struct definition {
  nsname ident;
  sourceloc loc;
};

qname qname_parse(const char *name);
qname *qname_list_parse(const char *list);

int nullstr_equals(const char *a, const char *b);
int nsname_equals(const nsname nn1, const nsname nn2);
int nsname_isnull(const nsname nn);
int qname_isnull(const qname qn);

qname qname_temp(char *prefix, char *localpart);
nsname nsname_temp(char *ns, char *name);

qname qname_new(const char *prefix, const char *localpart);
nsname nsname_new(const char *ns, const char *name);

qname qname_copy(const qname qn);
nsname nsname_copy(const nsname nn);

void qname_free(qname qn);
void nsname_free(nsname nn);
void nsname_ptr_free(nsname *nn);
nsname *nsname_ptr_copy(const nsname *nn);

int nsnametest_matches(nsnametest *test, nsname nn);
void nsnametest_free(nsnametest *test);
nsnametest *nsnametest_copy(nsnametest *test);
void qnametest_ptr_free(qnametest *test);

sourceloc sourceloc_copy(sourceloc sloc);
void sourceloc_free(sourceloc sloc);

#define nosourceloc get_nosourceloc()
sourceloc get_nosourceloc();

void print(const char *format, ...);






int get_ns_name_from_qname(xmlNodePtr n, xmlDocPtr doc, const char *name,
                           char **namespace1, char **localpart);
void error_info_copy(error_info *to, error_info *from);
void error_info_print(error_info *ei, FILE *f);
void error_info_free_vals(error_info *ei);
int error(error_info *ei, const char *filename, int line,
          const char *errname, const char *format, ...);
int parse_error(xmlNodePtr n, const char *format, ...);
int invalid_element2(error_info *ei, const char *filename, xmlNodePtr n);
int invalid_element(xmlNodePtr n);
int missing_attribute2(error_info *ei, const char *filename, int line, const char *errname,
                       const char *attrname);
int missing_attribute(xmlNodePtr n, const char *attrname);
int attribute_not_allowed(error_info *ei, const char *filename, int line, const char *attrname);
int conflicting_attributes(error_info *ei, const char *filename, int line, const char *errname,
                           const char *conflictor, const char *conflictee);
int invalid_attribute_val(error_info *ei, const char *filename, xmlNodePtr n, const char *attrname);
int check_element(xmlNodePtr n, const char *localname, const char *namespace1);

int convert_to_nonneg_int(const char *str, int *val);
int parse_int_attr(error_info *ei, char *filename, xmlNodePtr n, const char *attrname, int *val);
int parse_optional_int_attr(error_info *ei, char *filename,
                            xmlNodePtr n, const char *attrname, int *val, int def);
int parse_boolean_attr(error_info *ei, char *filename, xmlNodePtr n, const char *attrname,
                       int *val);
int parse_optional_boolean_attr(error_info *ei, char *filename,
                                xmlNodePtr n, const char *attrname, int *val, int def);
void replace_whitespace(char *str);
void collapse_whitespace(char *str);
char *get_wscollapsed_attr(xmlNodePtr n, const char *attrname, const char *ns);


void xml_write_attr(xmlTextWriter *writer, const char *attrname, const char *format, ...);
qname xml_attr_qname(xmlNodePtr n, const char *attrname);
int xml_attr_strcmp(xmlNodePtr n, const char *attrname, const char *s);





char *escape_str(const char *s);
int enforce_allowed_attributes(error_info *ei, const char *filename, xmlNodePtr n,
                               const char *restrictns, const nsname *stdattrs, ...);
int is_all_whitespace(const char *s, int len);



char *get_real_uri(const char *filename);
char *get_relative_uri(const char *uri, const char *base);

xmlNodePtr get_element_by_id(xmlNodePtr n, const char *id);
int retrieve_uri_element(error_info *ei, const char *filename, int line, const char *errname,
                         const char *full_uri, xmlDocPtr *doc, xmlNodePtr *node,
                         const char *refsource);
int retrieve_uri(error_info *ei, const char *filename, int line, const char *errname,
                 const char *full_uri, stringbuf *buf, const char *refsource);







typedef struct symbol_space_entry symbol_space_entry;
typedef struct symbol_space symbol_space;

struct symbol_space_entry {
  nsname ident;
  void *object;
  symbol_space_entry *next;
};

struct symbol_space {
  symbol_space *fallback;
  symbol_space_entry *entries;
  char *type;
};

symbol_space *ss_new(symbol_space *fallback, const char *type);
void *ss_lookup_local(symbol_space *ss, const nsname ident);
void *ss_lookup(symbol_space *ss, const nsname ident);
int ss_add(symbol_space *ss, const nsname ident, void *object);
void ss_free(symbol_space *ss);

#endif /* _UTIL_XMLUTILS_H */
