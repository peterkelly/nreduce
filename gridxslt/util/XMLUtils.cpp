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

#include "XMLUtils.h"
#include "Namespace.h"
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
#include <ctype.h>
#include <stdarg.h>
#include <unistd.h>
#include <limits.h>
#include <stdlib.h>

using namespace GridXSLT;

void XMLWriter::attribute(xmlTextWriterPtr writer, const String &name, const String &content)
{
  char *namestr = name.cstring();
  char *contentstr = content.cstring();
  xmlTextWriterWriteAttribute(writer,namestr,contentstr);
  free(namestr);
  free(contentstr);
}

void XMLWriter::comment(xmlTextWriterPtr writer, const String &content)
{
  char *contentstr = content.cstring();
  xmlTextWriterWriteComment(writer,contentstr);
  free(contentstr);
}

void XMLWriter::formatAttribute(xmlTextWriterPtr writer, const String &name,
                                const char *format, ...)
{
  StringBuffer buf;
  va_list ap;
  va_start(ap,format);
  buf.vformat(format,ap);
  va_end(ap);
  attribute(writer,name,buf.contents());
}

void XMLWriter::formatString(xmlTextWriterPtr writer, const char *format, ...)
{
  StringBuffer buf;
  va_list ap;
  va_start(ap,format);
  buf.vformat(format,ap);
  va_end(ap);
  string(writer,buf.contents());
}

void XMLWriter::pi(xmlTextWriterPtr writer, const String &target, const String &content)
{
  char *targetstr = target.cstring();
  char *contentstr = content.isNull() ? NULL : content.cstring();
  xmlTextWriterWritePI(writer,targetstr,contentstr);
  free(targetstr);
  free(contentstr);
}

void XMLWriter::raw(xmlTextWriterPtr writer, const String &content)
{
  char *contentstr = content.cstring();
  xmlTextWriterWriteRaw(writer,contentstr);
  free(contentstr);
}

void XMLWriter::string(xmlTextWriterPtr writer, const String &content)
{
  char *contentstr = content.cstring();
  xmlTextWriterWriteString(writer,contentstr);
  free(contentstr);
}

void XMLWriter::startElement(xmlTextWriterPtr writer, const String &name)
{
  char *namestr = name.cstring();
  xmlTextWriterStartElement(writer,namestr);
  free(namestr);
}

void XMLWriter::endElement(xmlTextWriterPtr writer)
{
  xmlTextWriterEndElement(writer);
}

Error::Error()
  : m_line(0)
{
}

Error::~Error()
{
}

void Error::fprint(FILE *f)
{
  if (!m_filename.isNull()) {
    char *rel;

    if (0 <= m_filename.indexOf(":")) {
      char *cwduri;
      char cwd[PATH_MAX];
      getcwd(cwd,PATH_MAX);
      cwduri = (char*)malloc(strlen("file://")+strlen(cwd)+2);
      sprintf(cwduri,"file://%s/",cwd);
      char *s = m_filename.cstring();
      rel = xmlBuildRelativeURI(s,cwduri);
      free(s);
      free(cwduri);
    }
    else {
      rel = m_filename.cstring();
    }

    if (0 <= m_line)
      fmessage(stderr,"%s:%d: ",rel,m_line);
    else
      fmessage(stderr,"%s: ",rel);
    free(rel);
  }

  if (!m_spec.isNull() && !m_section.isNull())
    fmessage(stderr,"error: %* (%*, section %*)\n",&m_message,&m_spec,&m_section);
  else if (!m_errname.isNull())
    fmessage(stderr,"error %*: %*\n",&m_errname,&m_message);
  else
    fmessage(stderr,"error: %*\n",&m_message);
}

void Error::clear()
{
  m_filename = String::null();
  m_line = 0;
  m_message = String::null();
  m_spec = String::null();
  m_section = String::null();
  m_errname = String::null();
}

int GridXSLT::XMLHasProp(xmlNodePtr n, const String &name)
{
  int r;
  char *namestr = name.cstring();
  r = (0 != xmlHasProp(n,namestr));
  free(namestr);
  return r;
}

int GridXSLT::XMLHasNsProp(xmlNodePtr n, const String &name, const String &ns)
{
  int r;
  char *namestr = name.cstring();
  char *nsstr = ns.isNull() ? NULL : ns.cstring();
  r = (0 != xmlHasNsProp(n,namestr,nsstr));
  free(namestr);
  free(nsstr);
  return r;
}

String GridXSLT::XMLGetProp(xmlNodePtr n, const String &name)
{
  char *r;
  char *namestr = name.cstring();
  r = xmlGetProp(n,namestr);
  String str(r);
  free(namestr);
  free(r);
  return str;
}

String GridXSLT::XMLGetNsProp(xmlNodePtr n, const String &name, const String &ns)
{
  char *r;
  char *namestr = name.cstring();
  char *nsstr = ns.isNull() ? NULL : ns.cstring();
  r = xmlGetNsProp(n,namestr,nsstr);
  String str(r);
  free(namestr);
  free(nsstr);
  free(r);
  return str;
}

int GridXSLT::error(Error *ei, const String &filename, int line,
          const String &errname, const char *format, ...)
{
  va_list ap;
  StringBuffer buf;

  ei->clear();

  va_start(ap,format);
  buf.vformat(format,ap);
  ei->m_message = buf.contents();
  va_end(ap);

  ei->m_filename = filename;
  ei->m_line = line;
  ei->m_errname = errname;

  return -1;
}

int GridXSLT::parse_error(xmlNodePtr n, const char *format, ...)
{
  va_list ap;
  
  fmessage(stderr,"Parse error at line %d: ",n->line);

  va_start(ap,format);
  vfprintf(stderr,format,ap);
  va_end(ap);

  if (n->ns && n->ns->prefix)
    fmessage(stderr," (%s:%s)",n->ns->prefix,n->name);
  else
    fmessage(stderr," (%s)",n->name);
  fmessage(stderr,"\n");
  return -1;
}

int GridXSLT::invalid_element2(Error *ei, const String &filename, xmlNodePtr n)
{
  if (n->ns && n->ns->href)
    return error(ei,filename,n->line,String::null(),"Element {%s}%s is not valid here",n->ns->href,n->name);
  else
    return error(ei,filename,n->line,String::null(),"Element %s is not valid here",n->name);
  return -1;
}

int GridXSLT::invalid_element(xmlNodePtr n)
{
  if (n->ns && n->ns->prefix)
    fmessage(stderr,"%d: Element <%s:%s> is not valid here\n",n->line,n->ns->prefix,n->name);
  else
    fmessage(stderr,"%d: Element <%s> is not valid here\n",n->line,n->name);
  return -1;
}

int GridXSLT::missing_attribute2(Error *ei, const String &filename, int line,
                       const String &errname,
                       const String &attrname)
{
/*   return error(ei,filename,line,errname,"\"%s\" attribute missing",attrname); */
  char *err = errname.isNull() ? NULL : errname.cstring();
  error(ei,filename,line,err,"missing attribute: %*",&attrname);
  free(err);
  return -1;
}

int GridXSLT::missing_attribute(xmlNodePtr n, const String &attrname)
{
  if (n->ns && n->ns->prefix)
    fmessage(stderr,"%d: No \"%*\" attribute specified on <%s:%s>\n",
            n->line,&attrname,n->ns->prefix,n->name);
  else
    fmessage(stderr,"%d: No \"%*\" attribute specified on <%s>\n",
            n->line,&attrname,n->name);
  return -1;
}

int GridXSLT::attribute_not_allowed(Error *ei, const String &filename, int line, const char *attrname)
{
  return error(ei,filename,line,String::null(),"\"%s\" attribute not allowed here",attrname);
}

int GridXSLT::conflicting_attributes(Error *ei, const String &filename, int line, const String &errname,
                           const String &conflictor, const String &conflictee)
{
  return error(ei,filename,line,errname,"\"%*\" attribute cannot be used in conjunction "
               "with \"%*\" here",&conflictor,&conflictee);
}

int GridXSLT::invalid_attribute_val(Error *ei, const String &filename, xmlNodePtr n,
                          const String &attrname)
{
  char *namestr = attrname.cstring();
  char *val = XMLGetProp(n,namestr).cstring();
  free(namestr);
  error(ei,filename,n->line,String::null(),"Invalid value \"%s\" for attribute \"%*\"",val,&attrname);
  free(val);
  return -1;
}

int GridXSLT::check_element(xmlNodePtr n, const String &localname, const String &namespace1)
{
  return ((XML_ELEMENT_NODE == n->type) &&
          (n->name == localname) &&
          (((NULL == n->ns) && (namespace1.isNull())) ||
           ((NULL != n->ns) && (!namespace1.isNull()) && (n->ns->href == namespace1))));
}

int GridXSLT::convert_to_nonneg_int(const String &str, int *val)
{
  char *tmp = str.cstring();
  char *end = NULL;
  int l = strtol(tmp,&end,10);
  if (('\0' != *tmp) && ('\0' == *end) && (0 <= l)) {
    *val = l;
    free(tmp);
    return 0;
  }
  else {
    free(tmp);
    return 1;
  }
}

int GridXSLT::parse_int_attr(Error *ei, char *filename, xmlNodePtr n, const String &attrname, int *val)
{
  String str;
  char *tmp = attrname.cstring();
  int cr;
  if (!XMLHasProp(n,tmp)) {
    missing_attribute2(ei,filename,n->line,String::null(),tmp);
    free(tmp);
    return -1;
  }

  str = get_wscollapsed_attr(n,tmp,String::null());
  cr = convert_to_nonneg_int(str,val);
  free(tmp);

  if (0 != cr)
    return invalid_attribute_val(ei,filename,n,attrname);
  else
    return 0;
}

int GridXSLT::parse_optional_int_attr(Error *ei, char *filename, xmlNodePtr n,
                            const GridXSLT::String &attrname, int *val, int def)
{
  char *tmp = attrname.cstring();
  if (XMLHasProp(n,tmp)) {
    free(tmp);
    return parse_int_attr(ei,filename,n,attrname,val);
  }
  free(tmp);
  *val = def;
  return 0;
}

int GridXSLT::parse_boolean_attr(Error *ei, char *filename, xmlNodePtr n,
                       const String &attrname, int *val)
{
  String str;
  int invalid = 0;
  if (!XMLHasProp(n,attrname))
    return missing_attribute(n,attrname);

  /* FIXME: support 1, 0 */
  str = get_wscollapsed_attr(n,attrname,String::null());
  if ((str == "true") || (str == "1"))
    *val = 1;
  else if ((str == "false") || (str == "0"))
      *val = 0;
  else
    invalid = 1;

  if (invalid)
    return invalid_attribute_val(ei,filename,n,attrname);
  else
    return 0;
}

int GridXSLT::parse_optional_boolean_attr(Error *ei, char *filename, xmlNodePtr n,
                                const GridXSLT::String &attrname, int *val, int def)
{
  if (XMLHasProp(n,attrname))
    return parse_boolean_attr(ei,filename,n,attrname,val);
  *val = def;
  return 0;
}

void GridXSLT::replace_whitespace(char *str)
{
  char *c;
  if (NULL == str)
    return;

  /* replace tab, line feed, and carriage return with space */
  for (c = str; '\0' != *c; c++)
    if (('\t' == *c) || ('\n' == *c) || ('\r' == *c))
      *c = ' ';
}

void GridXSLT::collapse_whitespace(char *str)
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

String GridXSLT::get_wscollapsed_attr(xmlNodePtr n, const String &attrname, const String &ns)
{
  char *val = XMLGetNsProp(n,attrname,ns).cstring();
  collapse_whitespace(val);
  String str(val);
  free(val);
  return str;
}

void GridXSLT::xml_write_attr(xmlTextWriter *writer, const char *attrname, const char *format, ...)
{
  va_list ap;
  stringbuf *buf = stringbuf_new();

  va_start(ap,format);
  stringbuf_vformat(buf,format,ap);
  va_end(ap);

  XMLWriter::attribute(writer,attrname,buf->data);

  stringbuf_free(buf);
}

QName GridXSLT::xml_attr_qname(xmlNodePtr n, const char *attrname)
{
  char *name = XMLGetNsProp(n,attrname,String::null()).cstring();
  QName qn = QName::parse(name);
  free(name);
  return qn;
}

int GridXSLT::xml_attr_strcmp(xmlNodePtr n, const char *attrname, const char *s)
{
  char *val = XMLGetNsProp(n,attrname,String::null()).cstring();
  int r;
  r = strcmp(val,s);
  free(val);
  return r;
}

String GridXSLT::escape_str(const String &ss)
{
  /* FIXME: implement using proper string mechanisms! */
  char *q = (char*)malloc(2*ss.length()+1);
  char *qp = q;
  char *str = ss.cstring();
  char *s = str;
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
  free(str);
  String res(q);
  free(q);
  return res;
}

int GridXSLT::enforce_allowed_attributes(Error *ei, const String &filename, xmlNodePtr n,
                               const String &restrictns, List<NSName> &stdattrs, int xx, ...)
{
  va_list ap;
  xmlAttr *attr;

  for (attr = n->properties; attr; attr = attr->next) {
    int allowed = 0;
    String *cur;
    const char *ns = attr->ns ? attr->ns->href : NULL;
    NSName attrnn;
    String attrName = attr->name;

    va_start(ap,xx);
    while ((NULL != (cur = va_arg(ap,String*))) && !allowed) {
      if (*cur == attrName)
        allowed = 1;
    }
    va_end(ap);

    attrnn = NSName(ns,attr->name);
    if ((NULL != ns) && (restrictns != ns))
      allowed = 1;

    Iterator<NSName> it;
    for (it = stdattrs; it.haveCurrent() && !allowed; it++)
      if (*it == attrnn)
        allowed = 1;

    if (!allowed) {
      QName attrqn = QName(attr->ns ? attr->ns->prefix : NULL,attr->name);
      QName nodeqn = QName(n->ns ? n->ns->prefix : NULL,n->name);
      error(ei,filename,n->line,String::null(),
            "attribute \"%*\" is not permitted on element \"%*\"",
            &attrqn,&nodeqn);
      return -1;
    }
  }

  return 0;
}

int GridXSLT::is_all_whitespace(const char *s, int len)
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

char *GridXSLT::get_real_uri(const String &filename)
{
  char *real;
  char *fnstr = filename.cstring();
  if (NULL != strstr(fnstr,"://")) {
    /* full uri */
    free(fnstr);
    return filename.cstring();
  }
  else {
    char path[PATH_MAX];
    if ('/' == fnstr[0]) {
      /* absolute filename */
      realpath(fnstr,path);
    }
    else {
      /* relative filename */
      char *full;
      char cwd[PATH_MAX];
      getcwd(cwd,PATH_MAX);
      full = (char*)malloc(strlen(cwd)+1+strlen(fnstr)+1);
      sprintf(full,"%s/%s",cwd,fnstr);
      realpath(full,path);
      free(full);
    }
    real = (char*)malloc(strlen("file://")+strlen(path)+1);
    sprintf(real,"file://%s",path);
    free(fnstr);
    return real;
  }
}

char *GridXSLT::get_relative_uri(const char *uri, const char *base)
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

xmlNodePtr GridXSLT::get_element_by_id(xmlNodePtr n, const char *id)
{
  xmlNodePtr c;
  if (XMLHasNsProp(n,"id",XML_NAMESPACE)) {
    char *nid = XMLGetNsProp(n,"id",XML_NAMESPACE).cstring();
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

int GridXSLT::retrieve_uri_element(Error *ei, const String &filename, int line, const String &errname,
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

int GridXSLT::retrieve_uri(Error *ei, const String &filename, int line, const String &errname,
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

#ifndef DISABLE_CURL
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
#endif
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

symbol_space_entry::symbol_space_entry()
  : object(NULL), next(NULL)
{
}

symbol_space_entry::~symbol_space_entry()
{
}

symbol_space *GridXSLT::ss_new(symbol_space *fallback, const char *type)
{
  symbol_space *ss = (symbol_space*)calloc(1,sizeof(symbol_space));
  ss->fallback = fallback;
  ss->type = strdup(type);
  return ss;
}

void *GridXSLT::ss_lookup_local(symbol_space *ss, const NSName &ident)
{
  symbol_space_entry *sse;
  for (sse = ss->entries; sse; sse = sse->next)
    if (sse->ident == ident)
      return sse->object;
  return NULL;
}

void *GridXSLT::ss_lookup(symbol_space *ss, const NSName &ident)
{
  void *object;
  if (NULL != (object = ss_lookup_local(ss,ident)))
    return object;
  else if (NULL != ss->fallback)
    return ss_lookup(ss->fallback,ident);
  else
    return NULL;
}

int GridXSLT::ss_add(symbol_space *ss, const NSName &ident, void *object)
{
  symbol_space_entry **sptr = &ss->entries;

  if (NULL != ss_lookup_local(ss,ident))
    return -1; /* already exists */

  while (*sptr)
    sptr = &((*sptr)->next);

  *sptr = new symbol_space_entry();
  (*sptr)->ident = ident;
  (*sptr)->object = object;

  return 0;
}

void GridXSLT::ss_free(symbol_space *ss)
{
  symbol_space_entry *sse = ss->entries;
  while (sse) {
    symbol_space_entry *next = sse->next;
    delete sse;
    sse = next;
  }
  free(ss->type);
  free(ss);
}

