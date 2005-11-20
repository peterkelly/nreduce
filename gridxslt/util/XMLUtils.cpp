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
#include "Debug.h"
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

int GridXSLT::invalid_element(Error *ei, const String &filename, Node *n)
{
  return error(ei,filename,n->m_line,String::null(),"Element %* is not valid here",&n->m_ident);
}

int GridXSLT::missing_attribute(Error *ei, const String &filename, int line,
                       const String &errname,
                       const NSName &attrname)
{
/*   return error(ei,filename,line,errname,"\"%s\" attribute missing",attrname); */
  char *err = errname.isNull() ? NULL : errname.cstring();
  error(ei,filename,line,err,"missing attribute: %*",&attrname);
  free(err);
  return -1;
}

int GridXSLT::attribute_not_allowed(Error *ei, const String &filename, int line, const NSName &attrname)
{
  return error(ei,filename,line,String::null(),"\"%*\" attribute not allowed here",&attrname);
}

int GridXSLT::conflicting_attributes(Error *ei, const String &filename, int line, const String &errname,
                           const String &conflictor, const String &conflictee)
{
  return error(ei,filename,line,errname,"\"%*\" attribute cannot be used in conjunction "
               "with \"%*\" here",&conflictor,&conflictee);
}

int GridXSLT::invalid_attribute_val(Error *ei, const String &filename, Node *n,
                          const NSName &attrname)
{
  String val = n->getAttribute(attrname);
  return error(ei,filename,n->m_line,String::null(),
               "Invalid value \"%*\" for attribute \"%*\"",&val,&attrname);
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

int GridXSLT::parse_int_attr(Error *ei, const String &filename, Node *n,
                             const GridXSLT::NSName &attrname, int *val)
{
  int cr;
  if (!n->hasAttribute(attrname)) {
    missing_attribute(ei,filename,n->m_line,String::null(),attrname);
    return -1;
  }

  String str = n->getAttribute(attrname).collapseWhitespace();
  cr = convert_to_nonneg_int(str,val);

  if (0 != cr)
    return invalid_attribute_val(ei,filename,n,attrname);
  else
    return 0;
}

int GridXSLT::parse_optional_int_attr(Error *ei, const String &filename, Node *n,
                            const GridXSLT::NSName &attrname, int *val, int def)
{
  if (n->hasAttribute(attrname))
    return parse_int_attr(ei,filename,n,attrname,val);
  *val = def;
  return 0;
}

int GridXSLT::parse_boolean_attr(Error *ei, const String &filename, Node *n,
                       const GridXSLT::NSName &attrname, int *val)
{
  int invalid = 0;
  if (!n->hasAttribute(attrname))
    return missing_attribute(ei,filename,n->m_line,String::null(),attrname);

  String str = n->getAttribute(attrname).collapseWhitespace();
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

int GridXSLT::parse_optional_boolean_attr(Error *ei, const String &filename, Node *n,
                                const GridXSLT::NSName &attrname, int *val, int def)
{
  if (n->hasAttribute(attrname))
    return parse_boolean_attr(ei,filename,n,attrname,val);
  *val = def;
  return 0;
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

String GridXSLT::get_real_uri(const String &filename)
{
  char *real;
  char *fnstr = filename.cstring();
  if (NULL != strstr(fnstr,"://")) {
    /* full uri */
    free(fnstr);
    return filename;
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
    String r = real;
    free(real);
    return r;
  }
}

String GridXSLT::get_relative_uri(const GridXSLT::String &uri1, const String &base1)
{
  char *uri = uri1.cstring();
  char *base = base1.cstring();
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
  free(uri);
  String r = rel;
  free(rel);
  free(base);
  return r;
}

static NSName ATTR_ID = NSName(XML_NAMESPACE,"id");

static Node *get_element_by_id(Node *n, const String &id)
{
  Node *c;

  if (n->getAttribute(ATTR_ID).collapseWhitespace() == id)
    return n;

  for (c = n->firstChild(); c; c = c->next()) {
    Node *match;
    if (NULL != (match = get_element_by_id(c,id)))
      return match;
  }
  return NULL;
}

int GridXSLT::retrieve_uri_element(Error *ei, const String &filename,
                                   int line, const String &errname,
                                   const GridXSLT::String &full_uri1,
                                   Node **nout,
                                   Node **rootout,
                                   const GridXSLT::String &refsource1)
{
  char *full_uri = full_uri1.cstring();
  char *refsource = refsource1.cstring();
  stringbuf *src = stringbuf_new();
  xmlNodePtr root;
  xmlURIPtr parsed;
  char *justdoc;
  char *hash;
  xmlDocPtr doc;

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
    free(full_uri);
    free(refsource);
    return -1;
  }

  if (NULL == (doc = xmlReadMemory(src->data,src->size-1,full_uri,NULL,0))) {
    error(ei,full_uri,-1,errname,"Parse error");
    stringbuf_free(src);
    free(justdoc);
    free(full_uri);
    free(refsource);
    return -1;
  }

  stringbuf_free(src);

  if (NULL == (root = xmlDocGetRootElement(doc))) {
    error(ei,full_uri,-1,errname,"No root element found");
    xmlFreeDoc(doc);
    free(justdoc);
    free(full_uri);
    free(refsource);
    return -1;
  }

  Node *rootNode = new Node(root);


  parsed = xmlParseURI(full_uri);
  Node *n;
  if (NULL != parsed->fragment) {
    /* @implements(xslt20:embedded-1) @end
       @implements(xslt20:embedded-2) @end
       @implements(xslt20:embedded-3) @end
       @implements(xslt20:embedded-4) @end
       @implements(xslt20:embedded-5) @end
       @implements(xslt20:embedded-6) @end
       @implements(xslt20:embedded-7) @end
       @implements(xslt20:embedded-8) @end */
    if (NULL == (n = get_element_by_id(rootNode,parsed->fragment))) {
      error(ei,full_uri,-1,errname,"No such fragment \"%s\"",parsed->fragment);
      xmlFreeDoc(doc);
      xmlFreeURI(parsed);
      free(justdoc);
      free(full_uri);
      free(refsource);
      return -1;
    }
  }
  else {
    n = rootNode;
  }

  *nout = n;
  *rootout = rootNode;
  // FIXME: fix original node and doc once we're sure that m_xn isn't needed anymore

  xmlFreeURI(parsed);
  free(justdoc);
  free(full_uri);
  free(refsource);
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

String GridXSLT::buildURI(const String &URI, const String &base)
{
  char *URI1 = URI.cstring();
  char *base1 = base.cstring();
  char *built = xmlBuildURI(URI1,base1);
  String r = built;
  free(built);
  free(URI1);
  free(base1);
  return r;
}

int GridXSLT::check_element(xmlNodePtr n, const String &localname, const String &namespace1)
{
  return ((XML_ELEMENT_NODE == n->type) &&
          (n->name == localname) &&
          (((NULL == n->ns) && (namespace1.isNull())) ||
           ((NULL != n->ns) && (!namespace1.isNull()) && (n->ns->href == namespace1))));
}

SymbolSpaceEntry::SymbolSpaceEntry()
  : object(NULL), next(NULL)
{
}

SymbolSpaceEntry::~SymbolSpaceEntry()
{
}

SymbolSpace::SymbolSpace(const String &_type)
  : entries(NULL)
{
  type = _type;
}

void *SymbolSpace::lookup(const NSName &ident)
{
  SymbolSpaceEntry *sse;
  for (sse = entries; sse; sse = sse->next)
    if (sse->ident == ident)
      return sse->object;
  return NULL;
}

int SymbolSpace::add(const NSName &ident, void *object)
{
  SymbolSpaceEntry **sptr = &entries;

  if (NULL != lookup(ident))
    return -1; /* already exists */

  while (*sptr)
    sptr = &((*sptr)->next);

  *sptr = new SymbolSpaceEntry();
  (*sptr)->ident = ident;
  (*sptr)->object = object;

  return 0;
}

SymbolSpace::~SymbolSpace()
{
  SymbolSpaceEntry *sse = entries;
  while (sse) {
    SymbolSpaceEntry *next = sse->next;
    delete sse;
    sse = next;
  }
}

