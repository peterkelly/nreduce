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

#define _UTIL_XMLUTILS_C

#include "xmlutils.h"
#include "namespace.h"
#include <libxml/xmlwriter.h>
#include <libxml/xmlIO.h>
#include <libxml/parser.h>
#include <libxml/tree.h>
#include <libxml/uri.h>
#include <curl/curl.h>
#include <curl/types.h>
#include <curl/easy.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <assert.h>
#include <ctype.h>
#include <stdarg.h>
#include <unistd.h>
#include <limits.h>
#include <stdlib.h>

qname qname_parse(const char *name)
{
  qname qn;
  qn.localpart = xmlSplitQName2(name,((xmlChar**)&qn.prefix));
  if (NULL == qn.localpart)
    qn.localpart = strdup(name);
  return qn;
}

qname *qname_list_parse(const char *list)
{
  const char *start;
  const char *c;
  int count = 0;
  int pos = 0;
  qname *qnames;

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

  qnames = (qname*)calloc(count+1,sizeof(qname));

  start = list;
  c = list;
  while (1) {
    if ('\0' == *c || isspace(*c)) {
      if (c != start) {
        char *str = (char*)malloc(c-start+1);
        memcpy(str,start,c-start);
        str[c-start] = '\0';
        qnames[pos++] = qname_parse(str);
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

int nullstr_equals(const char *a, const char *b)
{
  if ((NULL == a) && (NULL == b))
    return 1;
  else if ((NULL != a) && (NULL != b))
    return !strcmp(a,b);
  else
    return 0;
}

int nsname_equals(const nsname nn1, const nsname nn2)
{
  return (nullstr_equals(nn1.ns,nn2.ns) && nullstr_equals(nn1.name,nn2.name));
}

int nsname_isnull(const nsname nn)
{
  return (NULL == nn.name);
}

int qname_isnull(const qname qn)
{
  return (NULL == qn.localpart);
}

qname qname_temp(char *prefix, char *localpart)
{
  qname qn;
  qn.prefix = prefix;
  qn.localpart = localpart;
  return qn;
}

nsname nsname_temp(char *ns, char *name)
{
  nsname nn;
  nn.ns = ns;
  nn.name = name;
  return nn;
}

qname qname_new(const char *prefix, const char *localpart)
{
  qname qn;
  qn.prefix = prefix ? strdup(prefix) : NULL;
  qn.localpart = localpart ? strdup(localpart) : NULL;
  return qn;
}

nsname nsname_new(const char *ns, const char *name)
{
  nsname nn;
  nn.ns = ns ? strdup(ns) : NULL;
  nn.name = name ? strdup(name) : NULL;
  return nn;
}

qname qname_copy(const qname qn)
{
  return qname_new(qn.prefix,qn.localpart);
}

nsname nsname_copy(const nsname nn)
{
  return nsname_new(nn.ns,nn.name);
}

void qname_free(qname qn)
{
  free(qn.prefix);
  free(qn.localpart);
  qn.prefix = NULL;
  qn.localpart = NULL;
}

void nsname_free(nsname nn)
{
  free(nn.ns);
  free(nn.name);
  nn.ns = NULL;
  nn.name = NULL;
}

void nsname_ptr_free(nsname *nn)
{
  nsname_free(*nn);
  free(nn);
}

nsname *nsname_ptr_copy(const nsname *nn)
{
  nsname *copy = (nsname*)malloc(sizeof(nsname));
  *copy = nsname_copy(*nn);
  return copy;
}

int nsnametest_matches(nsnametest *test, nsname nn)
{
  if (test->wcns && test->wcname)
    return 1;
  else if (test->wcns)
    return nullstr_equals(test->nn.name,nn.name);
  else if (test->wcname)
    return nullstr_equals(test->nn.ns,nn.ns);
  else
    return nsname_equals(test->nn,nn);
}

void nsnametest_free(nsnametest *test)
{
  nsname_free(test->nn);
  free(test);
}

nsnametest *nsnametest_copy(nsnametest *test)
{
  nsnametest *copy = (nsnametest*)calloc(1,sizeof(nsnametest));
  copy->nn = nsname_copy(test->nn);
  copy->wcns = test->wcns;
  copy->wcname = test->wcname;
  return copy;
}

void qnametest_ptr_free(qnametest *test)
{
  qname_free(test->qn);
  free(test);
}

sourceloc sourceloc_copy(sourceloc sloc)
{
  sourceloc copy;
  copy.uri = sloc.uri ? strdup(sloc.uri) : NULL;
  copy.line = sloc.line;
  return copy;
}

void sourceloc_free(sourceloc sloc)
{
  free(sloc.uri);
}

const sourceloc nosourceloc1 = { uri: NULL, line: -1 };

sourceloc get_nosourceloc()
{
  return nosourceloc1;
}

void print(const char *format, ...)
{
  va_list ap;
  stringbuf *buf = stringbuf_new();

  va_start(ap,format);
  stringbuf_vformat(buf,format,ap);
  va_end(ap);

  printf("%s",buf->data);
  stringbuf_free(buf);
}

int get_ns_name_from_qname(xmlNodePtr n, xmlDocPtr doc, const char *name,
                           char **namespace1, char **localpart)
{
  qname qn = qname_parse(name);
  xmlNsPtr ns = NULL;

  *namespace1 = NULL;
  *localpart = NULL;

  /* FIXME: maybe set error info here instead of requiring thet caller to do it? */
  if ((NULL == (ns = xmlSearchNs(doc,n,qn.prefix))) && (NULL != qn.prefix)) {
    qname_free(qn);
    return -1;
  }
  *namespace1 = ns ? strdup(ns->href) : NULL;
  *localpart = qn.localpart;
  free(qn.prefix);

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
  if (NULL != ei->filename) {
    char *rel;

    if (NULL != strchr(ei->filename,':')) {
      char *cwduri;
      char cwd[PATH_MAX];
      getcwd(cwd,PATH_MAX);
      cwduri = (char*)malloc(strlen("file://")+strlen(cwd)+2);
      sprintf(cwduri,"file://%s/",cwd);
      rel = xmlBuildRelativeURI(ei->filename,cwduri);
      free(cwduri);
    }
    else {
      rel = strdup(ei->filename);
    }

    if (0 <= ei->line)
      fprintf(stderr,"%s:%d: ",rel,ei->line);
    else
      fprintf(stderr,"%s: ",rel);
    free(rel);
  }

  if (ei->spec && ei->section)
    fprintf(stderr,"error: %s (%s, section %s)\n",ei->message,ei->spec,ei->section);
  else if (ei->errname)
    fprintf(stderr,"error %s: %s\n",ei->errname,ei->message);
  else
    fprintf(stderr,"error: %s\n",ei->message);

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

int error(error_info *ei, const char *filename, int line,
          const char *errname, const char *format, ...)
{
  va_list ap;
  stringbuf *buf;

  error_info_free_vals(ei);

  va_start(ap,format);
  buf = stringbuf_new();
  stringbuf_vformat(buf,format,ap);
  ei->message = strdup(buf->data);
  stringbuf_free(buf);
  va_end(ap);

  ei->filename = filename ? strdup(filename) : NULL;
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
    return error(ei,filename,n->line,NULL,"Element {%s}%s is not valid here",n->ns->href,n->name);
  else
    return error(ei,filename,n->line,NULL,"Element %s is not valid here",n->name);
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
/*   return error(ei,filename,line,errname,"\"%s\" attribute missing",attrname); */
  return error(ei,filename,line,errname,"missing attribute: %s",attrname);
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
  return error(ei,filename,line,NULL,"\"%s\" attribute not allowed here",attrname);
}

int conflicting_attributes(error_info *ei, const char *filename, int line, const char *errname,
                           const char *conflictor, const char *conflictee)
{
  return error(ei,filename,line,errname,"\"%s\" attribute cannot be used in conjunction "
               "with \"%s\" here",conflictor,conflictee);
}

int invalid_attribute_val(error_info *ei, const char *filename, xmlNodePtr n, const char *attrname)
{
  char *val = xmlGetProp(n,attrname);
  error(ei,filename,n->line,NULL,"Invalid value \"%s\" for attribute \"%s\"",val,attrname);
  free(val);
  return -1;
}

int check_element(xmlNodePtr n, const char *localname, const char *namespace1)
{
  return ((XML_ELEMENT_NODE == n->type) &&
          !strcmp(n->name,localname) &&
          (((NULL == n->ns) && (NULL == namespace1)) ||
           ((NULL != n->ns) && (NULL != namespace1) && !strcmp(n->ns->href,namespace1))));
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

  str = get_wscollapsed_attr(n,attrname,NULL);
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
  str = get_wscollapsed_attr(n,attrname,NULL);
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

char *get_wscollapsed_attr(xmlNodePtr n, const char *attrname, const char *ns)
{
  char *val = xmlGetNsProp(n,attrname,ns);
  collapse_whitespace(val);
  return val;
}

void xml_write_attr(xmlTextWriter *writer, const char *attrname, const char *format, ...)
{
  va_list ap;
  stringbuf *buf = stringbuf_new();

  va_start(ap,format);
  stringbuf_vformat(buf,format,ap);
  va_end(ap);

  xmlTextWriterWriteAttribute(writer,attrname,buf->data);

  stringbuf_free(buf);
}

qname xml_attr_qname(xmlNodePtr n, const char *attrname)
{
  char *name = xmlGetNsProp(n,attrname,NULL);
  qname qn = qname_parse(name);
  free(name);
  return qn;
}

int xml_attr_strcmp(xmlNodePtr n, const char *attrname, const char *s)
{
  char *val = xmlGetNsProp(n,attrname,NULL);
  int r;
  r = strcmp(val,s);
  free(val);
  return r;
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
                               const char *restrictns, const nsname *stdattrs, ...)
{
  va_list ap;
  xmlAttr *attr;

  for (attr = n->properties; attr; attr = attr->next) {
    int allowed = 0;
    char *cur;
    const nsname *s;
    const char *ns = attr->ns ? attr->ns->href : NULL;
    nsname attrnn;

    va_start(ap,stdattrs);
    while ((NULL != (cur = va_arg(ap,char*))) && !allowed) {
      if (!strcmp(cur,attr->name))
        allowed = 1;
    }
    va_end(ap);

    attrnn = nsname_new(ns,attr->name);
    if ((NULL != ns) && strcmp(restrictns,ns))
      allowed = 1;
    for (s = stdattrs; s->name && !allowed; s++)
      if (nsname_equals(*s,attrnn))
        allowed = 1;
    nsname_free(attrnn);

    if (!allowed) {
      qname attrqn = qname_new(attr->ns ? attr->ns->prefix : NULL,attr->name);
      qname nodeqn = qname_new(n->ns ? n->ns->prefix : NULL,n->name);
      error(ei,filename,n->line,NULL,"attribute \"%#q\" is not permitted on element \"%#q\"",
            attrqn,nodeqn);
      qname_free(attrqn);
      qname_free(nodeqn);
      return -1;
    }
  }

  return 0;
}

int is_all_whitespace(const char *s, int len)
{
  int i;
  for (i = 0; i < len; i++)
    if (!isspace(s[i]))
      return 0;
  return 1;
}

static size_t write_buf(void *ptr, size_t size, size_t nmemb, void *data)
{
  stringbuf_append((stringbuf*)data,(const char*)ptr,size*nmemb);
  return size*nmemb;
}

char *get_real_uri(const char *filename)
{
  char *real;
  if (NULL != strstr(filename,"://")) {
    /* full uri */
    return strdup(filename);
  }
  else {
    char path[PATH_MAX];
    if ('/' == filename[0]) {
      /* absolute filename */
      realpath(filename,path);
    }
    else {
      /* relative filename */
      char *full;
      char cwd[PATH_MAX];
      getcwd(cwd,PATH_MAX);
      full = (char*)malloc(strlen(cwd)+1+strlen(filename)+1);
      sprintf(full,"%s/%s",cwd,filename);
      realpath(full,path);
      free(full);
    }
    real = (char*)malloc(strlen("file://")+strlen(path)+1);
    sprintf(real,"file://%s",path);
    return real;
  }
}

char *get_relative_uri(const char *uri, const char *base)
{
  char *relfilename = xmlBuildRelativeURI(uri,base);
  char *rel;
  xmlURIPtr parsed = xmlParseURI(uri);
  if (NULL == parsed->fragment) {
    rel = strdup(relfilename);
  }
  else {
    char *escfrag = xmlURIEscape(parsed->fragment);
    rel = (char*)malloc(strlen(relfilename)+1+strlen(escfrag)+1);
    sprintf(rel,"%s#%s",relfilename,escfrag);
    free(escfrag);
  }
  xmlFreeURI(parsed);
  free(relfilename);
  return rel;
}

xmlNodePtr get_element_by_id(xmlNodePtr n, const char *id)
{
  xmlNodePtr c;
  if (xmlHasNsProp(n,"id",XML_NAMESPACE)) {
    char *nid = xmlGetNsProp(n,"id",XML_NAMESPACE);
    int equal;
    collapse_whitespace(nid);
    equal = !strcmp(nid,id);
    free(nid);
    if (equal)
      return n;
  }
  for (c = n->children; c; c = c->next) {
    xmlNodePtr match;
    if (NULL != (match = get_element_by_id(c,id)))
      return match;
  }
  return NULL;
}

int retrieve_uri_element(error_info *ei, const char *filename, int line, const char *errname,
                         const char *full_uri, xmlDocPtr *doc, xmlNodePtr *node,
                         const char *refsource)
{
  stringbuf *src = stringbuf_new();
  xmlNodePtr root;
  xmlURIPtr parsed;
  char *justdoc;
  char *hash;

  /* FIXME: support normal ID attributes (not just xml:id)... there seems to be rather complex
     rules regarding what exactly constitutes an ID attribute; I think we need to examine
     the DTD or schema of the imported document because it may not actually be called "id".
     xml:id will have to do for now. */

  justdoc = strdup(full_uri);

  if (NULL != (hash = strchr(justdoc,'#')))
    *hash = '\0';

  if (0 != retrieve_uri(ei,filename,line,errname,full_uri,src,refsource)) {
    stringbuf_free(src);
    free(justdoc);
    return -1;
  }

  if (NULL == (*doc = xmlReadMemory(src->data,src->size-1,full_uri,NULL,0))) {
    error(ei,full_uri,-1,errname,"Parse error");
    stringbuf_free(src);
    free(justdoc);
    return -1;
  }

  stringbuf_free(src);

  if (NULL == (root = xmlDocGetRootElement(*doc))) {
    error(ei,full_uri,-1,errname,"No root element found");
    xmlFreeDoc(*doc);
    free(justdoc);
    return -1;
  }

  parsed = xmlParseURI(full_uri);
  if (NULL != parsed->fragment) {
    /* @implements(xslt20:embedded-1) @end
       @implements(xslt20:embedded-2) @end
       @implements(xslt20:embedded-3) @end
       @implements(xslt20:embedded-4) @end
       @implements(xslt20:embedded-5) @end
       @implements(xslt20:embedded-6) @end
       @implements(xslt20:embedded-7) @end
       @implements(xslt20:embedded-8) @end */
    if (NULL == (*node = get_element_by_id(root,parsed->fragment))) {
      error(ei,full_uri,-1,errname,"No such fragment \"%s\"",parsed->fragment);
      xmlFreeDoc(*doc);
      xmlFreeURI(parsed);
      free(justdoc);
      return -1;
    }
  }
  else {
    *node = root;
  }

  xmlFreeURI(parsed);
  free(justdoc);
  return 0;
}

int retrieve_uri(error_info *ei, const char *filename, int line, const char *errname,
                 const char *full_uri1, stringbuf *buf, const char *refsource)
{
  char *colon;
  char *hash;
  char *uri = strdup(full_uri1);

  if (NULL != (hash = strchr(uri,'#')))
    *hash = '\0';

  if (NULL == (colon = strchr(uri,':'))) {
    error(ei,filename,line,errname,"\"%s\" is not a valid absolute URI",uri);
    free(uri);
    return -1;
  }

  if (!strncasecmp(uri,"http:",5)) {
    CURL *h;
    CURLcode cr;
    long rc = 0;

    h = curl_easy_init();
    curl_easy_setopt(h,CURLOPT_URL,uri);
    curl_easy_setopt(h,CURLOPT_WRITEFUNCTION,write_buf);
    curl_easy_setopt(h,CURLOPT_WRITEDATA,buf);
    curl_easy_setopt(h,CURLOPT_USERAGENT,"libcurl-agent/1.0");
    cr = curl_easy_perform(h);

    if (0 != cr) {
      error(ei,filename,line,errname,"Error retrieving \"%s\": %s",uri,curl_easy_strerror(cr));
      curl_easy_cleanup(h);
      free(uri);
      return -1;
    }

    curl_easy_getinfo(h,CURLINFO_RESPONSE_CODE,&rc);
    if (200 != rc) {
      /* FIXME: would be nice to display a string instead of just the response code */
      error(ei,filename,line,errname,"Error retrieving schema \"%s\": HTTP %d",uri,rc);
      curl_easy_cleanup(h);
      free(uri);
      return -1;
    }

    curl_easy_cleanup(h);
  }
  else if (!strncasecmp(uri,"file://",7)) {
    FILE *f;
    char tmp[1024];
    int r;
    const char *importfn = uri+7;

    if (strncasecmp(refsource,"file:",5)) {
      error(ei,filename,line,errname,"Permission denied for file \"%s\"; cannot access local file "
            "from a remote resource",importfn);
      free(uri);
      return -1;
    }

    if (NULL == (f = fopen(importfn,"r"))) {
      error(ei,uri,-1,errname,"%s",strerror(errno));
      free(uri);
      return -1;
    }

    while (0 < (r = fread(tmp,1,1024,f)))
      stringbuf_append(buf,tmp,r);

    fclose(f);
  }
  else {
    int methodlen = colon-uri;
    char *method = (char*)malloc(methodlen+1);
    memcpy(method,uri,methodlen);
    method[methodlen] = '\0';
    error(ei,filename,line,errname,"Method \"%s\" not supported",method,strerror(errno));
    free(method);
    free(uri);
    return -1;
  }
  free(uri);

  return 0;
}

symbol_space *ss_new(symbol_space *fallback, const char *type)
{
  symbol_space *ss = (symbol_space*)calloc(1,sizeof(symbol_space));
  ss->fallback = fallback;
  ss->type = strdup(type);
  return ss;
}

void *ss_lookup_local(symbol_space *ss, const nsname ident)
{
  symbol_space_entry *sse;
  for (sse = ss->entries; sse; sse = sse->next)
    if (nsname_equals(sse->ident,ident))
      return sse->object;
  return NULL;
}

void *ss_lookup(symbol_space *ss, const nsname ident)
{
  void *object;
  if (NULL != (object = ss_lookup_local(ss,ident)))
    return object;
  else if (NULL != ss->fallback)
    return ss_lookup(ss->fallback,ident);
  else
    return NULL;
}

int ss_add(symbol_space *ss, const nsname ident, void *object)
{
  symbol_space_entry **sptr = &ss->entries;

  if (NULL != ss_lookup_local(ss,ident))
    return -1; /* already exists */

  while (*sptr)
    sptr = &((*sptr)->next);

  *sptr = (symbol_space_entry*)calloc(1,sizeof(symbol_space_entry));
  (*sptr)->ident = nsname_copy(ident);
  (*sptr)->object = object;

  return 0;
}

void ss_free(symbol_space *ss)
{
  symbol_space_entry *sse = ss->entries;
  while (sse) {
    symbol_space_entry *next = sse->next;
    nsname_free(sse->ident);
    free(sse);
    sse = next;
  }
  free(ss->type);
  free(ss);
}

