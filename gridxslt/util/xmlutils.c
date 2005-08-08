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

#include "xmlutils.h"
#include <libxml/xmlwriter.h>
#include <libxml/xmlIO.h>
#include <libxml/parser.h>
#include <libxml/tree.h>
#include <stdio.h>
#include <string.h>
#include <assert.h>
#include <ctype.h>
#include <stdarg.h>

qname_t get_qname(const char *name)
{
  qname_t qn;
  qn.localpart = xmlSplitQName2(name,((xmlChar**)&qn.prefix));
  if (NULL == qn.localpart)
    qn.localpart = strdup(name);
  return qn;
}

qname_t copy_qname(qname_t qname)
{
  qname_t copy;
  copy.prefix = qname.prefix ? strdup(qname.prefix) : NULL;
  copy.localpart = qname.localpart ? strdup(qname.localpart) : NULL;
  return copy;
}

qname_t *get_qname_list(const char *list)
{
  const char *start;
  const char *c;
  int count = 0;
  int pos = 0;
  qname_t *qnames;

  start = list;
  c = list;
  while (1) {
    if ('\0' == *c || isspace(*c)) {
      if (c != start)
        count++;
      start = c+1;
    }
    if ('\0' == *c)
      break;
    c++;
  }

  qnames = (qname_t*)calloc(count+1,sizeof(qname_t));

  start = list;
  c = list;
  while (1) {
    if ('\0' == *c || isspace(*c)) {
      if (c != start) {
        char *str = (char*)malloc(c-start+1);
        memcpy(str,start,c-start);
        str[c-start] = '\0';
        qnames[pos++] = get_qname(str);
        free(str);
      }
      start = c+1;
    }
    if ('\0' == *c)
      break;
    c++;
  }
  return qnames;
}

int get_ns_name_from_qname(xmlNodePtr n, xmlDocPtr doc, const char *name,
                           char **namespace, char **localpart)
{
  qname_t qname = get_qname(name);
  xmlNsPtr ns = NULL;

  *namespace = NULL;
  *localpart = NULL;

  /* FIXME: maybe set error info here instead of requiring thet caller to do it? */
  if ((NULL == (ns = xmlSearchNs(doc,n,qname.prefix))) && (NULL != qname.prefix)) {
    free(qname.prefix);
    free(qname.localpart);
    return -1;
  }
  *namespace = ns ? strdup(ns->href) : NULL;
  *localpart = qname.localpart;
  free(qname.prefix);

  return 0;
}

void error_info_copy(error_info *to, error_info *from)
{
  assert(NULL != from->filename);
  assert(NULL != from->message);
  error_info_free_vals(to);
  to->filename = strdup(from->filename);
  to->line = from->line;
  to->message = strdup(from->message);
  to->spec = from->spec ? strdup(from->spec) : NULL;
  to->section = from->section ? strdup(from->section) : NULL;
  to->errname = from->errname ? strdup(from->errname) : NULL;
}

void error_info_print(error_info *ei, FILE *f)
{
  if (ei->spec && ei->section)
    fprintf(stderr,"%s:%d: error: %s (%s, section %s)\n",
            ei->filename,ei->line,ei->message,ei->spec,ei->section);
  else if (ei->errname)
    fprintf(stderr,"%s:%d: error %s: %s\n",ei->filename,ei->line,ei->errname,ei->message);
  else
    fprintf(stderr,"%s:%d: error: %s\n",ei->filename,ei->line,ei->message);
}

void error_info_free_vals(error_info *ei)
{
  free(ei->filename);
  free(ei->message);
  free(ei->spec);
  free(ei->section);
  free(ei->errname);
  memset(ei,0,sizeof(error_info));
}

int set_error_info(error_info *ei, const char *filename, int line, const char *spec,
                   const char *section, const char *format, ...)
{
  int flen;
  va_list ap;

  error_info_free_vals(ei);

  va_start(ap,format);
  flen = vsnprintf(NULL,0,format,ap);
  va_end(ap);

  ei->message = (char*)malloc(flen+1);
  va_start(ap,format);
  vsnprintf(ei->message,flen+1,format,ap);
  va_end(ap);

  ei->filename = strdup(filename);
  ei->line = line;
  ei->spec = spec ? strdup(spec) : NULL;
  ei->section = section ? strdup(section) : NULL;

  return -1;
}

int set_error_info2(error_info *ei, const char *filename, int line, const char *errname,
                    const char *format, ...)
{
  int flen;
  va_list ap;

  error_info_free_vals(ei);

  va_start(ap,format);
  flen = vsnprintf(NULL,0,format,ap);
  va_end(ap);

  ei->message = (char*)malloc(flen+1);
  va_start(ap,format);
  vsnprintf(ei->message,flen+1,format,ap);
  va_end(ap);

  ei->filename = strdup(filename);
  ei->line = line;
  ei->errname = errname ? strdup(errname) : NULL;

  return -1;
}

int parse_error(xmlNodePtr n, const char *format, ...)
{
  va_list ap;
  
  fprintf(stderr,"Parse error at line %d: ",n->line);

  va_start(ap,format);
  vfprintf(stderr,format,ap);
  va_end(ap);

  if (n->ns && n->ns->prefix)
    fprintf(stderr," (%s:%s)",n->ns->prefix,n->name);
  else
    fprintf(stderr," (%s)",n->name);
  fprintf(stderr,"\n");
  return -1;
}

int invalid_element2(error_info *ei, const char *filename, xmlNodePtr n)
{
  if (n->ns && n->ns->href)
    return set_error_info(ei,filename,n->line,NULL,NULL,
                          "Element {%s}%s is not valid here",n->ns->href,
                          n->name);
  else
    return set_error_info(ei,filename,n->line,NULL,NULL,"Element %s is not valid here",n->name);
  return -1;
}

int invalid_element(xmlNodePtr n)
{
  if (n->ns && n->ns->prefix)
    fprintf(stderr,"%d: Element <%s:%s> is not valid here\n",n->line,n->ns->prefix,n->name);
  else
    fprintf(stderr,"%d: Element <%s> is not valid here\n",n->line,n->name);
  return -1;
}

int missing_attribute2(error_info *ei, const char *filename, int line, const char *errname,
                       const char *attrname)
{
  return set_error_info2(ei,filename,line,errname,"\"%s\" attribute missing",attrname);
}

int missing_attribute(xmlNodePtr n, const char *attrname)
{
  if (n->ns && n->ns->prefix)
    fprintf(stderr,"%d: No \"%s\" attribute specified on <%s:%s>\n",
            n->line,attrname,n->ns->prefix,n->name);
  else
    fprintf(stderr,"%d: No \"%s\" attribute specified on <%s>\n",
            n->line,attrname,n->name);
  return -1;
}

int attribute_not_allowed(error_info *ei, const char *filename, int line, const char *attrname)
{
  return set_error_info(ei,filename,line,NULL,NULL,"\"%s\" attribute not allowed here",attrname);
}

int conflicting_attributes(error_info *ei, char *filename, int line, const char *errname,
                           const char *conflictor, const char *conflictee)
{
  return set_error_info2(ei,filename,line,errname,
                         "\"%s\" attribute cannot be used in conjunction with \"%s\" here",
                         conflictor,conflictee);
}

int invalid_attribute_val(error_info *ei, const char *filename, xmlNodePtr n, const char *attrname)
{
  char *val = xmlGetProp(n,attrname);
  set_error_info(ei,filename,n->line,NULL,NULL,
                 "Invalid value \"%s\" for attribute \"%s\"",val,attrname);
  free(val);
  return -1;
}

int check_element(xmlNodePtr n, const char *localname, const char *namespace)
{
  return ((XML_ELEMENT_NODE == n->type) &&
          !strcmp(n->name,localname) &&
          (((NULL == n->ns) && (NULL == namespace)) ||
           ((NULL != n->ns) && (NULL != namespace) && !strcmp(n->ns->href,namespace))));
}

int convert_to_nonneg_int(const char *str, int *val)
{
  char *end = NULL;
  int l = strtol(str,&end,10);
  if (('\0' != *str) && ('\0' == *end) && (0 <= l)) {
    *val = l;
    return 0;
  }
  else {
    return 1;
  }
}

int parse_int_attr(error_info *ei, char *filename, xmlNodePtr n, const char *attrname, int *val)
{
  char *str;
  int cr;
  if (!xmlHasProp(n,attrname))
    return missing_attribute2(ei,filename,n->line,NULL,attrname);

  str = get_wscollapsed_attr(n,attrname);
  cr = convert_to_nonneg_int(str,val);
  free(str);

  if (0 != cr)
    return invalid_attribute_val(ei,filename,n,attrname);
  else
    return 0;
}

int parse_optional_int_attr(error_info *ei, char *filename, xmlNodePtr n,
                            const char *attrname, int *val, int def)
{
  if (xmlHasProp(n,attrname))
    return parse_int_attr(ei,filename,n,attrname,val);
  *val = def;
  return 0;
}

int parse_boolean_attr(error_info *ei, char *filename, xmlNodePtr n, const char *attrname, int *val)
{
  char *str;
  int invalid = 0;
  if (!xmlHasProp(n,attrname))
    return missing_attribute(n,attrname);

  /* FIXME: support 1, 0 */
  str = get_wscollapsed_attr(n,attrname);
  if (!strcmp(str,"true") || !strcmp(str,"1"))
    *val = 1;
  else if (!strcmp(str,"false") || !strcmp(str,"0"))
      *val = 0;
  else
    invalid = 1;
  free(str);

  if (invalid)
    return invalid_attribute_val(ei,filename,n,attrname);
  else
    return 0;
}

int parse_optional_boolean_attr(error_info *ei, char *filename, xmlNodePtr n,
                                const char *attrname, int *val, int def)
{
  if (xmlHasProp(n,attrname))
    return parse_boolean_attr(ei,filename,n,attrname,val);
  *val = def;
  return 0;
}

char *get_full_name(const char *ns, const char *name)
{
  char *str;
  if (NULL == name)
    return NULL;
  if (NULL == ns)
    return strdup(name);
  str = (char*)malloc(strlen("{}")+strlen(ns)+strlen(name)+1);
  sprintf(str,"{%s}%s",ns,name);
  return str;
}

char *build_qname(const char *prefix, const char *localpart)
{
  if (NULL != prefix) {
    char *qname = (char*)malloc(strlen(prefix)+1+strlen(localpart)+1);
    sprintf(qname,"%s:%s",prefix,localpart);
    return qname;
  }
  else {
    return strdup(localpart);
  }
}


void replace_whitespace(char *str)
{
  char *c;
  if (NULL == str)
    return;

  /* replace tab, line feed, and carriage return with space */
  for (c = str; '\0' != *c; c++)
    if (('\t' == *c) || ('\n' == *c) || ('\r' == *c))
      *c = ' ';
}

void collapse_whitespace(char *str)
{
  char *c;
  char *w;
  int started = 0;
  char prev = '\0';
  char *end = str;
  if (NULL == str)
    return;

  replace_whitespace(str);

  w = str;
  for (c = str; '\0' != *c; c++) {
    /* remove leading spaces */
    if (' ' != *c)
      started = 1;
    if (started &&
        /* replace multiple spaces with single space */
        ((' ' != *c) || (' ' != prev))) {
      prev = *(w++) = *c;
      if (' ' != *c)
        end = w;
    }
  }
  /* remove trailing spaces */
  *end = '\0';
}

char *get_wscollapsed_attr(xmlNodePtr n, const char *attrname)
{
  char *val = xmlGetProp(n,attrname);
  collapse_whitespace(val);
  return val;
}

int ns_name_equals(const char *ns1, const char *name1, const char *ns2, const char *name2)
{
  return (!strcmp(name1,name2) &&
          (((NULL == ns1) && (NULL == ns2)) ||
           ((NULL != ns1) && (NULL != ns2) && !strcmp(ns1,ns2))));
}

char *escape_str(const char *s)
{
  char *q = (char*)malloc(2*strlen(s)+1);
  char *qp = q;
  while ('\0' != *s) {
    switch (*s) {
    case '\n':
      *(qp++) = '\\';
      *(qp++) = 'n';
      s++;
      break;
    case '\t':
      *(qp++) = '\\';
      *(qp++) = 't';
      s++;
      break;
    case '\r':
      *(qp++) = '\\';
      *(qp++) = 'r';
      s++;
      break;
    case '\b':
      *(qp++) = '\\';
      *(qp++) = 'b';
      s++;
      break;
    case '\f':
      *(qp++) = '\\';
      *(qp++) = 'f';
      s++;
      break;
    case '\\':
      *(qp++) = '\\';
      *(qp++) = '\\';
      s++;
      break;
    case '"':
      *(qp++) = '\\';
      *(qp++) = '"';
      s++;
      break;
    /* FIXME: any other cases? check what the spec says about quoted characters */
    default:
      *(qp++) = *(s++);
      break;
    }
  }
  *qp = '\0';
  return q;
}

int enforce_allowed_attributes(error_info *ei, const char *filename, xmlNodePtr n,
                               const nsname_t *stdattrs, ...)
{
  va_list ap;
  xmlAttr *attr;

  for (attr = n->properties; attr; attr = attr->next) {
    int allowed = 0;
    char *cur;
    const nsname_t *s;
    const char *ns = attr->ns ? attr->ns->href : NULL;

    va_start(ap,stdattrs);
    while ((NULL != (cur = va_arg(ap,char*))) && !allowed) {
      if (!strcmp(cur,attr->name))
        allowed = 1;
    }
    va_end(ap);

    for (s = stdattrs; s->name && !allowed; s++)
      if (ns_name_equals(s->ns,s->name,ns,attr->name))
        allowed = 1;

    if (!allowed) {
      char *attrqname = build_qname(attr->ns ? attr->ns->prefix : NULL,attr->name);
      char *nodeqname = build_qname(n->ns ? n->ns->prefix : NULL,n->name);
      set_error_info2(ei,filename,n->line,NULL,
                             "attribute \"%s\" is not permitted on element \"%s\"",
                             attrqname,nodeqname);
      free(attrqname);
      free(nodeqname);
      return -1;
    }
  }

  return 0;
}

symbol_space *ss_new(symbol_space *fallback, const char *type)
{
  symbol_space *ss = (symbol_space*)calloc(1,sizeof(symbol_space));
  ss->fallback = fallback;
  ss->type = strdup(type);
  return ss;
}

void *ss_lookup_local(symbol_space *ss, const char *name, const char *ns)
{
  symbol_space_entry *sse;
  for (sse = ss->entries; sse; sse = sse->next) {
    if ((((NULL == sse->ns) && (NULL == ns)) ||
          ((NULL != sse->ns) && (NULL != ns) && !strcmp(sse->ns,ns))) &&
        !strcmp(sse->name,name))
      return sse->object;
  }
  return NULL;
}

void *ss_lookup(symbol_space *ss, const char *name, const char *ns)
{
  void *object;
  if (NULL != (object = ss_lookup_local(ss,name,ns)))
    return object;
  else if (NULL != ss->fallback)
    return ss_lookup(ss->fallback,name,ns);
  else
    return NULL;
}

int ss_add(symbol_space *ss, const char *name, const char *ns, void *object)
{
  symbol_space_entry **sptr = &ss->entries;

  if (NULL != ss_lookup_local(ss,name,ns))
    return -1; /* already exists */

  while (*sptr)
    sptr = &((*sptr)->next);

  *sptr = (symbol_space_entry*)calloc(1,sizeof(symbol_space_entry));
  (*sptr)->name = strdup(name);
  (*sptr)->ns = ns ? strdup(ns) : NULL;
  (*sptr)->object = object;

  return 0;
}

void ss_free(symbol_space *ss)
{
  symbol_space_entry *sse = ss->entries;
  while (sse) {
    symbol_space_entry *next = sse->next;
    free(sse->name);
    free(sse->ns);
    free(sse);
    sse = next;
  }
  free(ss->type);
  free(ss);
}

