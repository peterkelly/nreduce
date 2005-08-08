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
 */

#ifndef _UTIL_XMLUTILS_H
#define _UTIL_XMLUTILS_H

#include <stdio.h>
#include <libxml/tree.h>
#include <libxml/xmlwriter.h>

typedef struct error_info error_info;
typedef struct qname_t qname_t;
typedef struct nsname_t nsname_t;

struct error_info {
  char *filename;
  int line;
  char *message;
  char *spec;
  char *section;
  char *errname;
};

struct qname_t {
  char *prefix;
  char *localpart;
};

struct nsname_t {
  char *ns;
  char *name;
};

qname_t get_qname(const char *name);
qname_t copy_qname(qname_t qname);
qname_t *get_qname_list(const char *list);

int get_ns_name_from_qname(xmlNodePtr n, xmlDocPtr doc, const char *name,
                           char **namespace, char **localpart);
void error_info_copy(error_info *to, error_info *from);
void error_info_print(error_info *ei, FILE *f);
void error_info_free_vals(error_info *ei);
int set_error_info(error_info *ei, const char *filename, int line, const char *spec,
                   const char *section, const char *format, ...);
int set_error_info2(error_info *ei, const char *filename, int line, const char *errname,
                    const char *format, ...);
int parse_error(xmlNodePtr n, const char *format, ...);
int invalid_element2(error_info *ei, const char *filename, xmlNodePtr n);
int invalid_element(xmlNodePtr n);
int missing_attribute2(error_info *ei, const char *filename, int line, const char *errname,
                       const char *attrname);
int missing_attribute(xmlNodePtr n, const char *attrname);
int attribute_not_allowed(error_info *ei, const char *filename, int line, const char *attrname);
int conflicting_attributes(error_info *ei, char *filename, int line, const char *errname,
                           const char *conflictor, const char *conflictee);
int invalid_attribute_val(error_info *ei, const char *filename, xmlNodePtr n, const char *attrname);
int check_element(xmlNodePtr n, const char *localname, const char *namespace);

int convert_to_nonneg_int(const char *str, int *val);
int parse_int_attr(error_info *ei, char *filename, xmlNodePtr n, const char *attrname, int *val);
int parse_optional_int_attr(error_info *ei, char *filename,
                            xmlNodePtr n, const char *attrname, int *val, int def);
int parse_boolean_attr(error_info *ei, char *filename, xmlNodePtr n, const char *attrname,
                       int *val);
int parse_optional_boolean_attr(error_info *ei, char *filename,
                                xmlNodePtr n, const char *attrname, int *val, int def);
char *get_full_name(const char *ns, const char *name);
char *build_qname(const char *prefix, const char *localpart);
void replace_whitespace(char *str);
void collapse_whitespace(char *str);
char *get_wscollapsed_attr(xmlNodePtr n, const char *attrname);
int ns_name_equals(const char *ns1, const char *name1, const char *ns2, const char *name2);
char *escape_str(const char *s);
int enforce_allowed_attributes(error_info *ei, const char *filename, xmlNodePtr n,
                               const nsname_t *stdattrs, ...);

typedef struct symbol_space_entry symbol_space_entry;
typedef struct symbol_space symbol_space;

struct symbol_space_entry {
  char *name;
  char *ns;
  void *object;
  symbol_space_entry *next;
};

struct symbol_space {
  symbol_space *fallback;
  symbol_space_entry *entries;
  char *type;
};

symbol_space *ss_new(symbol_space *fallback, const char *type);
void *ss_lookup_local(symbol_space *ss, const char *name, const char *ns);
void *ss_lookup(symbol_space *ss, const char *name, const char *ns);
int ss_add(symbol_space *ss, const char *name, const char *ns, void *object);
void ss_free(symbol_space *ss);

#endif /* _UTIL_XMLUTILS_H */
