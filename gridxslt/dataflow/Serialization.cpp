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

#define _DATAFLOW_SERIALIZATION_C

#include "Serialization.h"
#include "util/Macros.h"
#include "util/Debug.h"
#include <string.h>
#include <stdlib.h>
#include <ctype.h>

using namespace GridXSLT;

#define INDENT_SPACES 3

/* @implements(xslt20:serialization-1) status { info } @end
   @implements(xslt20:serialization-2) status { info } @end
   @implements(xslt20:serialization-4) status { info } @end
   @implements(xslt20:serialization-5) status { info } @end
   @implements(xslt20:serialization-6) status { info } @end
   @implements(xslt20:serialization-7) status { info } @end
   @implements(xslt20:serialization-12) status { deferred } @end
   @implements(xslt20:serialization-13) status { deferred } @end
   @implements(xslt20:serialization-14) status { info } @end
   @implements(xslt20:serialization-20) status { info } @end
   @implements(xslt20:serialization-22) status { info } @end
*/

const char *df_seroptions::seroption_names[SEROPTION_COUNT] = {
  "byte-order-mark",
  "cdata-section-elements",
  "doctype-public",
  "doctype-system",
  "encoding",
  "escape-uri-attributes",
  "include-content-type",
  "indent",
  "media-type",
  "method",
  "normalization-form",
  "omit-xml-declaration",
  "standalone",
  "undeclare-prefixes",
  "use-character-maps",
  "version",
};

df_seroptions::df_seroptions(const NSName &method)
{
  /* @implements(xslt20:serialization-11)
     test { xslt/parse/output_defaults_xml.test }
     test { xslt/parse/output_defaults_html.test }
     test { xslt/parse/output_defaults_text.test }
     test { xslt/parse/output_defaults_xhtml.test }
     @end
     @implements(xslt20:serialization-21) @end */

  /* @implements(xslt20:serialization-16) status { deferred } @end */
  /* @implements(xslt20:serialization-17) status { deferred } @end */
  /* @implements(xslt20:serialization-18) status { deferred } @end */
  /* @implements(xslt20:serialization-19) status { deferred } @end */

  m_byte_order_mark = 0;
  m_doctype_public = String::null();
  m_doctype_system = String::null();
  m_encoding = "UTF-8";
  m_escape_uri_attributes = 1;
  m_include_content_type = 1;

  if (!method.isNull() && (method.m_ns.isNull()) &&
      (method.m_name == "html") || (method.m_name =="xhtml"))
    m_indent = 1;
  else
    m_indent = 0;


  m_media_type = String::null();
  if (!method.isNull() && (method.m_ns.isNull())) {
    if (method.m_name == "xml")
      m_media_type = "text/xml";
    else if ((method.m_name == "html") || (method.m_name =="xhtml"))
      m_media_type = "text/html";
    else if (method.m_name =="text")
      m_media_type = "text/plain";
  }

  m_method = method;
  if (m_method.isNull())
    m_method = NSName("xml");
  m_normalization_form = "none";
  m_omit_xml_declaration = 0;
  m_standalone = STANDALONE_OMIT;
  m_undeclare_prefixes = 0;
  m_version = "1.0";
}

df_seroptions::~df_seroptions()
{
}

static int invalid_seroption(Error *ei, const String &filename,
                             int line, const String &option, const String &value)
{
  return error(ei,filename,line,String::null(),"Invalid value for %*: %*",&option,&value);
}

int parse_yes_no(Error *ei, const String &filename, int line,
                 int *val, const String &name, const String &str)
{
  if (str.isNull())
    return 0;
  if ("yes" == str)
    *val = 1;
  else if ("no" == str)
    *val = 0;
  else
    return invalid_seroption(ei,filename,line,name,str);
  return 0;
}

typedef int (parsefun)(df_seroptions *options, Error *ei, const String &filename,
                       int line, const String &value, NamespaceMap *namespaces);

static int df_parse_byte_order_mark(df_seroptions *options, Error *ei,
                                           const String &filename, int line,
                                           const String &value, NamespaceMap *namespaces)
{
  CHECK_CALL(parse_yes_no(ei,filename,line,&options->m_byte_order_mark,
                          "byte-order-mark",value))
  return 0;
}

static int df_parse_cdata_section_elements(df_seroptions *options, Error *ei,
                                           const String &filename, int line,
                                           const String &value, NamespaceMap *namespaces)
{
  int r = 0;
  if (!value.isNull()) {
    List<QName> qnames = QName::parseList(value);
    for (Iterator<QName> it = qnames; it.haveCurrent(); it++) {
      NSName nn = qname_to_nsname(namespaces,*it);
      if (nn.isNull()) {
        error(ei,filename,line,String::null(),"Cannot resolve namespace prefix: %*",
              &(*it).m_prefix);
        r = 1;
        break;
      }
      else {
        options->m_cdata_section_elements.append(nn);
      }
    }
    if (0 != r)
      return -1;
  }
  return 0;
}

static int df_parse_doctype_public(df_seroptions *options, Error *ei,
                                           const String &filename, int line,
                                           const String &value, NamespaceMap *namespaces)
{
  if (!value.isNull())
    options->m_doctype_public = value;
  return 0;
}

static int df_parse_doctype_system(df_seroptions *options, Error *ei,
                                           const String &filename, int line,
                                           const String &value, NamespaceMap *namespaces)
{
  if (!value.isNull())
    options->m_doctype_system = value;
  return 0;
}

static int df_parse_encoding(df_seroptions *options, Error *ei,
                                           const String &filename, int line,
                                           const String &value, NamespaceMap *namespaces)
{
  if (!value.isNull())
    options->m_encoding = value;
  return 0;
}

static int df_parse_escape_uri_attributes(df_seroptions *options, Error *ei,
                                           const String &filename, int line,
                                           const String &value, NamespaceMap *namespaces)
{
  CHECK_CALL(parse_yes_no(ei,filename,line,&options->m_escape_uri_attributes,
                          "escape-uri-attributes",value))
  return 0;
}

static int df_parse_include_content_type(df_seroptions *options, Error *ei,
                                           const String &filename, int line,
                                           const String &value, NamespaceMap *namespaces)
{
  CHECK_CALL(parse_yes_no(ei,filename,line,&options->m_include_content_type,
                          "include-content-type",value))
  return 0;
}

static int df_parse_indent(df_seroptions *options, Error *ei,
                                           const String &filename, int line,
                                           const String &value, NamespaceMap *namespaces)
{
  CHECK_CALL(parse_yes_no(ei,filename,line,&options->m_indent,
                          "indent",value))
  return 0;
}

static int df_parse_media_type(df_seroptions *options, Error *ei,
                                           const String &filename, int line,
                                           const String &value, NamespaceMap *namespaces)
{
  if (!value.isNull())
    options->m_media_type = value;
  return 0;
}

static int df_parse_method(df_seroptions *options, Error *ei,
                                           const String &filename, int line,
                                           const String &value, NamespaceMap *namespaces)
{
  if (!value.isNull()) {
    QName qn = QName::parse(value);
    NSName nn = qname_to_nsname(namespaces,qn);
    if (nn.isNull()) {
      error(ei,filename,line,String::null(),"Cannot resolve namespace prefix: %*",&qn.m_prefix);
      return -1;
    }
    if (nn.m_ns.isNull() &&
        (nn.m_name != "xml") &&
        (nn.m_name != "xhtml") &&
        (nn.m_name != "html") &&
        (nn.m_name != "text")) {
      error(ei,filename,line,String::null(),
            "Invalid method: %* (methods other than xml, xhtml, html, "
            "and text must have a namespace associated with them)",&nn);
      return -1;
    }
    options->m_method = nn;
  }
  return 0;
}

static int df_parse_normalization_form(df_seroptions *options, Error *ei,
                                           const String &filename, int line,
                                           const String &value, NamespaceMap *namespaces)
{
  if (!value.isNull())
    options->m_normalization_form = value;
  return 0;
}

static int df_parse_omit_xml_declaration(df_seroptions *options, Error *ei,
                                           const String &filename, int line,
                                           const String &value, NamespaceMap *namespaces)
{
  CHECK_CALL(parse_yes_no(ei,filename,line,&options->m_omit_xml_declaration,
                          "omit-xml-declaration",value))
  return 0;
}

static int df_parse_standalone(df_seroptions *options, Error *ei,
                                           const String &filename, int line,
                                           const String &value, NamespaceMap *namespaces)
{
  if (!value.isNull()) {
    if ("yes" == value)
      options->m_standalone = STANDALONE_YES;
    else if ("no" == value)
      options->m_standalone = STANDALONE_NO;
    else if ("omit" == value)
      options->m_standalone = STANDALONE_OMIT;
    else
      return invalid_seroption(ei,filename,line,"standalone",value);
  }
  return 0;
}

static int df_parse_undeclare_prefixes(df_seroptions *options, Error *ei,
                                           const String &filename, int line,
                                           const String &value, NamespaceMap *namespaces)
{
  CHECK_CALL(parse_yes_no(ei,filename,line,&options->m_undeclare_prefixes,
                          "undeclare-prefixes",value))
  return 0;
}

static int df_parse_use_character_maps(df_seroptions *options, Error *ei,
                                           const String &filename, int line,
                                           const String &value, NamespaceMap *namespaces)
{
  /* FIXME */
  return 0;
}

static int df_parse_version(df_seroptions *options, Error *ei,
                                           const String &filename, int line,
                                           const String &value, NamespaceMap *namespaces)
{
  /* FIXME */
  return 0;
}

parsefun *parse_functions[SEROPTION_COUNT] = {
  &df_parse_byte_order_mark,
  &df_parse_cdata_section_elements,
  &df_parse_doctype_public,
  &df_parse_doctype_system,
  &df_parse_encoding,
  &df_parse_escape_uri_attributes,
  &df_parse_include_content_type,
  &df_parse_indent,
  &df_parse_media_type,
  &df_parse_method,
  &df_parse_normalization_form,
  &df_parse_omit_xml_declaration,
  &df_parse_standalone,
  &df_parse_undeclare_prefixes,
  &df_parse_use_character_maps,
  &df_parse_version
};

int df_seroptions::parseOption(int option, Error *ei,
                               const String &filename, int line,
                               const String &value, NamespaceMap *namespaces)
{
  ASSERT(0 <= option);
  ASSERT(SEROPTION_COUNT > option);
  /* FIXME: must always ensure that the values passed in are whitespace collapsed
     (including values that come from attribute value templates) */
  return parse_functions[option](this,ei,filename,line,value,namespaces);
}

void df_seroptions::initFrom(const df_seroptions &other)
{
  m_ident = other.m_ident;
  m_byte_order_mark = other.m_byte_order_mark;
  m_cdata_section_elements = other.m_cdata_section_elements;
  m_doctype_public = other.m_doctype_public;
  m_doctype_system = other.m_doctype_system;
  m_encoding = other.m_encoding;
  m_escape_uri_attributes = other.m_escape_uri_attributes;
  m_include_content_type = other.m_include_content_type;
  m_indent = other.m_indent;
  m_media_type = other.m_media_type;
  m_method = other.m_method;
  m_normalization_form = other.m_normalization_form;
  m_omit_xml_declaration = other.m_omit_xml_declaration;
  m_standalone = other.m_standalone;
  m_undeclare_prefixes = other.m_undeclare_prefixes;
  m_use_character_maps = other.m_use_character_maps;
  m_version = other.m_version;
}

void printValues(List<Value> &values)
{
  for (Iterator<Value> it(values); it.haveCurrent(); it++)
    it->fprint(stdout);
}

static Value df_normalize_sequence(Value &seq, Error *ei)
{
  /* XSLT 2.0 and XQuery Serialization, section 2 */
  List<Value> in = seq.sequenceToList();
  List<Value> s1;
  List<Value> s2;
  List<Value> s3;
  List<Value> s4;
  List<Value> s5;
  List<Value> s6;
  int r = 0;
  Node *doc = NULL;
  Iterator<Value> it;

  /* 1. If the sequence that is input to serialization is empty, create a sequence S1 that consists
     of a zero-length string. Otherwise, copy each item in the sequence that is input to
     serialization to create the new sequence S1. */
  if (in.isEmpty()) {
    s1.append(Value(""));
  }
  else {
    for (it = in; it.haveCurrent(); it++) {
      Value v = *it;
      ASSERT(SEQTYPE_ITEM == v.type().type());
      s1.append(v);
    }
  }

  /* 2. For each item in S1, if the item is atomic, obtain the lexical representation of the item
     by casting it to an xs:string and copy the string representation to the new sequence;
     otherwise, copy the item, which will be a node, to the new sequence. The new sequence is S2. */
    for (it = s1; it.haveCurrent(); it++) {
      Value v = *it;
      if (ITEM_ATOMIC == v.type().itemType()->m_kind)
        s2.append(Value(v.convertToString()));
      else
        s2.append(v);
    }

  /* 3. For each subsequence of adjacent strings in S2, copy a single string to the new sequence
     equal to the values of the strings in the subsequence concatenated in order, each separated
     by a single space. Copy all other items to the new sequence. The new sequence is S3. */
  for (it = s2; it.haveCurrent(); it++) {
    Value v = *it;
    if (ITEM_ATOMIC == v.type().itemType()->m_kind) {
      /* must be a string; see previous step */
      StringBuffer buf;

      buf.append(v.asString());

      while (it.haveNext() && (ITEM_ATOMIC == it.peek().type().itemType()->m_kind)) {
        it++;
        v = *it;
        buf.append(" ");
        buf.append(v.asString());
      }

      s3.append(Value(buf.contents()));
    }
    else {
      s3.append(v);
    }
  }

  /* 4. For each item in S3, if the item is a string, create a text node in the new sequence whose
     string value is equal to the string; otherwise, copy the item to the new sequence. The new
     sequence is S4. */
  for (it = s3; it.haveCurrent(); it++) {
    Value v = *it;
    if (ITEM_ATOMIC == v.type().itemType()->m_kind) {
      /* must be a string; see previous step */
      Node *n = new Node(NODE_TEXT);
      n->m_value = v.asString();
      s4.append(Value(n));
    }
    else {
      s4.append(v);
    }
  }

  /* 5. For each item in S4, if the item is a document node, copy its children to the new sequence;
     otherwise, copy the item to the new sequence. The new sequence is S5. */
  for (it = s4; it.haveCurrent(); it++) {
    Value v = *it;
    if (ITEM_DOCUMENT == v.type().itemType()->m_kind) {
      Node *c;
      for (c = v.asNode()->m_first_child; c; c = c->m_next)
        s5.append(Value(c));
    }
    else {
      s5.append(v);
    }
  }

  /* 6. For each subsequence of adjacent text nodes in S5, copy a single text node to the new
     sequence equal to the values of the text nodes in the subsequence concatenated in order. Any
     text nodes with values of zero length are dropped. Copy all other items to the new sequence.
     The new sequence is S6. */
  for (it = s5; it.haveCurrent(); it++) {
    Value v = *it;
    if (ITEM_TEXT == v.type().itemType()->m_kind) {
      StringBuffer buf;
      Node *oldtext = v.asNode();

      buf.append(oldtext->m_value);

      while (it.haveNext() && (ITEM_TEXT == it.peek().type().itemType()->m_kind)) {
        it++;
        v = *it;
        oldtext = v.asNode();
        buf.append(oldtext->m_value);
      }

      String contents = buf.contents();
      if (0 < contents.length()) {
        Node *text = new Node(NODE_TEXT);
        text->m_value = buf.contents();
        s6.append(Value(text));
      }
    }
    else {
      s6.append(v);
    }
  }

  /* 7. It is a serialization error [err:SE0001] if an item in S6 is an attribute node or a
     namespace node. Otherwise, construct a new sequence, S7, that consists of a single document
     node and copy all the items in the sequence, which are all nodes, as children of that
     document node. */
  for (it = s6; it.haveCurrent(); it++) {
    Value v = *it;
    if ((ITEM_ATTRIBUTE == v.type().itemType()->m_kind) ||
        (ITEM_NAMESPACE == v.type().itemType()->m_kind)) {
      error(ei,String::null(),0,"SE0001","Output sequence cannot contain attribute or namespace nodes");
      r = 1;
      break;
    }
  }

  if (0 == r) {
    doc = new Node(NODE_DOCUMENT);
    for (it = s6; it.haveCurrent(); it++) {
      Value v = *it;
      ASSERT(ITEM_ATOMIC != v.type().itemType()->m_kind);
      doc->addChild(v.asNode()->deepCopy());
    }
  }

  if (0 == r)
    return Value(doc);
  else
    return Value::null();
}

static int serialize_stringbuf(void *context, const char * buffer, int len)
{
  stringbuf_append((stringbuf*)context,buffer,len);
  return len;
}

static void number_node(Node *n, int *num)
{
  Node *c;
  n->m_nodeno = (*num)++;
  for (c = n->m_first_child; c; c = c->m_next)
    number_node(c,num);
}

static void dump_tree(Node *n, int indent)
{
  Node *c;
  if ((!n->m_value.isNull()) && !n->m_value.isAllWhitespace())
    debug("%#i%d %s \"%*\"\n",2*indent,n->m_nodeno,df_item_kinds[n->m_type],&n->m_value);
  else
    debug("%#i%d %s %*\n",2*indent,n->m_nodeno,df_item_kinds[n->m_type],&n->m_ident);
  for (c = n->m_first_child; c; c = c->m_next)
    dump_tree(c,indent+1);
}

static void insertws(Node *parent, Node *before, int have_newline, int depth)
{
  StringBuffer buf;
  Node *wsnode = new Node(NODE_TEXT);
  if (!have_newline)
    buf.format("\n%#i",INDENT_SPACES*depth);
  else
    buf.format("%#i",INDENT_SPACES*depth+1);
  wsnode->m_value = buf.contents();
  parent->insertChild(wsnode,before);
}

static void add_indentation(Node *n, int depth)
{
  Node *c;
  int newline = 0;
  int column = 0;
  int nonws = 0;

  /* @implements(xslt-xquery-serialization:xml-indent-1) @end
     @implements(xslt-xquery-serialization:xml-indent-2)
     status { partial }
     issue { still need to deal with xml:space attribute }
     test { xslt/output/indent1.test }
     test { xslt/output/indent2.test }
     test { xslt/output/indent3.test }
     test { xslt/output/indent4.test }
     test { xslt/output/indent5.test }
     test { xslt/output/indent6.test }
     test { xslt/output/indent7.test }
     test { xslt/output/indent8.test }
     test { xslt/output/indent9.test }
     test { xslt/output/indent10.test }
     test { xslt/output/indent11.test }
     test { xslt/output/indent12.test }
     test { xslt/output/indent13.test }
     @end
     @implements(xslt-xquery-serialization:xml-indent-3) @end
     @implements(xslt-xquery-serialization:xml-indent-4) @end */

  for (c = n->m_first_child; c; c = c->m_next) {

    if (NODE_ELEMENT == c->m_type) {

      add_indentation(c,depth+1);

      debug("node %d: newline = %d, column = %d, depth = %d\n",c->m_nodeno,newline,column,depth);
      if (!nonws && (!newline || (INDENT_SPACES*depth >= column)))
        insertws(n,c,newline,depth);

      newline = 0;
      column = 0;
      nonws = 0;
    }
    else if (NODE_TEXT == c->m_type) {
      const char *ch;
      char *value = c->m_value.cstring();
      for (ch = value; '\0' != *ch; ch++) {
        if (!isspace(*ch))
          nonws = 1;
        if (('\n' == *ch) || ('\r' == *ch)) {
          newline = 1;
          column = 0;
        }
        column++;
      }
      free(value);
    }

    if (NULL == c->m_next) {
      if ((0 < depth) && !nonws && (!newline || (INDENT_SPACES*(depth-1) >= column)))
        insertws(n,c->m_next,newline,depth-1);
    }
  }
}

static Node *get_indented_doc(Node *n)
{
  Node *copy = n->deepCopy()->ref();
  int num = 0;

  number_node(copy,&num);
  dump_tree(copy,0);

  debug("before add_indentation\n");
  add_indentation(copy,0);
  debug("after add_indentation\n");

  return copy;
}

static int df_serialize_xml(Node *doc, stringbuf *buf, df_seroptions *options, Error *ei)
{
  if (options->m_indent) {
    doc = get_indented_doc(doc);
    ASSERT(doc->checkTree());
    debug("identation: new doc has %d refs\n",doc->m_refcount);
  }

  xmlOutputBuffer *xb = xmlOutputBufferCreateIO(serialize_stringbuf,NULL,buf,NULL);
  xmlTextWriter *writer = xmlNewTextWriter(xb);

  xmlTextWriterStartDocument(writer,NULL,"UTF-8",NULL);

  doc->printXML(writer);

  xmlTextWriterEndDocument(writer);
  xmlTextWriterFlush(writer);
  xmlFreeTextWriter(writer);

  /* Remove newline after XML declaration */
  char *newline = strchr(buf->data,'\n');
  int contentstart = newline+1-buf->data;
  int contentlen = buf->size-1-contentstart;
  ASSERT(NULL != newline);
  memmove(newline,newline+1,contentlen);
  buf->size--;
  buf->data[buf->size-1] = '\0';

  /* Remove newline at end */
  ASSERT('\n' == buf->data[buf->size-2]);
  buf->size--;
  buf->data[buf->size-1] = '\0';

  if (options->m_indent)
    doc->deref();

  return 0;
}

static int df_serialize_text(Node *doc, stringbuf *buf, df_seroptions *options, Error *ei)
{
  StringBuffer sb;
  doc->printBuf(sb);
  char *text = sb.contents().cstring();
  stringbuf_append(buf,text,strlen(text));
  free(text);
  return 0;
}

int GridXSLT::df_serialize(Value &v, stringbuf *buf, df_seroptions *options, Error *ei)
{
  Value normseq = df_normalize_sequence(v,ei);
  int r = 0;

  if (normseq.isNull())
    return -1;

  /* FIXME: when indent is disabled, saxon does not print a newline after the <?xml...?>
     declaration. If we want to be consistent with saxon (for testing purposes) we need to do
     this but I'm not sure if libxml supports it. */

  Node *doc = normseq.asNode()->ref();

  ASSERT(doc->checkTree());

  if (options->m_method == NSName("xml")) {
    r = df_serialize_xml(doc,buf,options,ei);
  }
  else if (options->m_method == NSName("text")) {
    r = df_serialize_text(doc,buf,options,ei);
  }
  else {
    r = error(ei,String::null(),0,String::null(),
              "Serialization method %* is not supported",&options->m_method);
  }

  doc->deref();

  return r;
}

/* Note: this is only safe to call on a newly created tree! If there are variables that
   reference nodes in the tree then this could result invalid references due to whitespace
   text nodes being deleted. */
void GridXSLT::df_strip_spaces(Node *n, list *space_decls)
{
  Node *c = n->m_first_child;
  int strip = ((NODE_ELEMENT == n->m_type) && !space_decl_preserve(space_decls,n->m_ident));
  while (c) {

    if ((NODE_TEXT == c->m_type) && strip && c->m_value.isAllWhitespace()) {
      Node *next = c->m_next;
      if (n->m_first_child == c)
        n->m_first_child = c->m_next;
      if (n->m_last_child == c)
        n->m_last_child = c->m_prev;
      if (NULL != c->m_prev)
        c->m_prev->m_next = c->m_next;
      if (NULL != c->m_next)
        c->m_next->m_prev = c->m_prev;
      delete c;
      c = next;
    }
    else {
      df_strip_spaces(c,space_decls);
      c = c->m_next;
    }
  }
}

void GridXSLT::df_namespace_fixup(Node *n)
{
  /* XSLT 2.0: 5.7.3 Namespace Fixup */
  /* FIXME */
}

static int space_decl_better(space_decl *nw, space_decl *existing)
{
  if (nw->importpred < existing->importpred)
    return 0;
  if (nw->priority < existing->priority)
    return 0;
  return 1;
}

int GridXSLT::space_decl_preserve(list *space_decls, const NSName &elemname)
{
  space_decl *best_match = NULL;
  list *l;

  for (l = space_decls; l; l = l->next) {
    space_decl *decl = (space_decl*)l->data;
    if (decl->nnt->matches(elemname) &&
        ((NULL == best_match) || space_decl_better(decl,best_match)))
      best_match = decl;
  }

  return best_match ? best_match->preserve : 1;
}

space_decl *GridXSLT::space_decl_copy(space_decl *decl)
{
  space_decl *copy = (space_decl*)calloc(1,sizeof(space_decl));
  copy->nnt = NSNameTest_copy(decl->nnt);
  copy->importpred = decl->importpred;
  copy->priority = decl->priority;
  copy->preserve = decl->preserve;
  return copy;
}

void GridXSLT::space_decl_free(space_decl *decl)
{
  delete decl->nnt;
  free(decl);
}

