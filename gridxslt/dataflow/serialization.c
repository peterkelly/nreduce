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

#include "serialization.h"
#include "util/macros.h"
#include "util/debug.h"
#include <assert.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>

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

const char *seroption_names[SEROPTION_COUNT] = {
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

static int invalid_seroption(error_info *ei, const char *filename,
                             int line, const char *option, const char *value)
{
  return error(ei,filename,line,NULL,"Invalid value for %s: %s",option,value);
}

int parse_yes_no(error_info *ei, const char *filename, int line,
                 int *val, const char *name, const char *str)
{
  if (NULL == str)
    return 0;
  if (!strcmp(str,"yes"))
    *val = 1;
  else if (!strcmp(str,"no"))
    *val = 0;
  else
    return invalid_seroption(ei,filename,line,name,str);
  return 0;
}

typedef int (parsefun)(df_seroptions *options, error_info *ei, const char *filename,
                       int line, const char *value, ns_map *namespaces);

static int df_parse_byte_order_mark(df_seroptions *options, error_info *ei,
                                           const char *filename, int line,
                                           const char *value, ns_map *namespaces)
{
  CHECK_CALL(parse_yes_no(ei,filename,line,&options->byte_order_mark,
                          "byte-order-mark",value))
  return 0;
}

static int df_parse_cdata_section_elements(df_seroptions *options, error_info *ei,
                                           const char *filename, int line,
                                           const char *value, ns_map *namespaces)
{
  int r = 0;
  if (NULL != value) {
    qname *qnames = qname_list_parse(value);
    qname *qn;
    for (qn = qnames; !qname_isnull(*qn); qn++) {
      nsname *nn = (nsname*)malloc(sizeof(nsname));
      *nn = qname_to_nsname(namespaces,*qn);
      if (nsname_isnull(*nn)) {
        error(ei,filename,line,NULL,"Cannot resolve namespace prefix: %s",qn->prefix);
        r = 1;
        free(nn);
        break;
      }
      else {
        list_append(&options->cdata_section_elements,nn);
      }
    }
    free(qnames);
    if (0 != r)
      return -1;
  }
  return 0;
}

static int df_parse_doctype_public(df_seroptions *options, error_info *ei,
                                           const char *filename, int line,
                                           const char *value, ns_map *namespaces)
{
  if (NULL != value) {
    free(options->doctype_public);
    options->doctype_public = strdup(value);
  }
  return 0;
}

static int df_parse_doctype_system(df_seroptions *options, error_info *ei,
                                           const char *filename, int line,
                                           const char *value, ns_map *namespaces)
{
  if (NULL != value) {
    free(options->doctype_system);
    options->doctype_system = strdup(value);
  }
  return 0;
}

static int df_parse_encoding(df_seroptions *options, error_info *ei,
                                           const char *filename, int line,
                                           const char *value, ns_map *namespaces)
{
  if (NULL != value) {
    free(options->encoding);
    options->encoding = strdup(value);
  }
  return 0;
}

static int df_parse_escape_uri_attributes(df_seroptions *options, error_info *ei,
                                           const char *filename, int line,
                                           const char *value, ns_map *namespaces)
{
  CHECK_CALL(parse_yes_no(ei,filename,line,&options->escape_uri_attributes,
                          "escape-uri-attributes",value))
  return 0;
}

static int df_parse_include_content_type(df_seroptions *options, error_info *ei,
                                           const char *filename, int line,
                                           const char *value, ns_map *namespaces)
{
  CHECK_CALL(parse_yes_no(ei,filename,line,&options->include_content_type,
                          "include-content-type",value))
  return 0;
}

static int df_parse_indent(df_seroptions *options, error_info *ei,
                                           const char *filename, int line,
                                           const char *value, ns_map *namespaces)
{
  CHECK_CALL(parse_yes_no(ei,filename,line,&options->indent,
                          "indent",value))
  return 0;
}

static int df_parse_media_type(df_seroptions *options, error_info *ei,
                                           const char *filename, int line,
                                           const char *value, ns_map *namespaces)
{
  if (NULL != value) {
    free(options->media_type);
    options->media_type = strdup(value);
  }
  return 0;
}

static int df_parse_method(df_seroptions *options, error_info *ei,
                                           const char *filename, int line,
                                           const char *value, ns_map *namespaces)
{
  if (NULL != value) {
    qname qn = qname_parse(value);
    nsname nn = qname_to_nsname(namespaces,qn);
    if (nsname_isnull(nn)) {
      error(ei,filename,line,NULL,"Cannot resolve namespace prefix: %s",qn.prefix);
      qname_free(qn);
      return -1;
    }
    qname_free(qn);
    if ((NULL == nn.ns) &&
        strcmp(nn.name,"xml") &&
        strcmp(nn.name,"xhtml") &&
        strcmp(nn.name,"html") &&
        strcmp(nn.name,"text")) {
      error(ei,filename,line,NULL,"Invalid method: %#n (ethods other than xml, xhtml, html, "
            "and text must have a namespace associated with them)",nn);
      nsname_free(nn);
      return -1;
    }
    nsname_free(options->method);
    options->method = nn;
  }
  return 0;
}

static int df_parse_normalization_form(df_seroptions *options, error_info *ei,
                                           const char *filename, int line,
                                           const char *value, ns_map *namespaces)
{
  if (NULL != value) {
    free(options->normalization_form);
    options->normalization_form = strdup(value);
  }
  return 0;
}

static int df_parse_omit_xml_declaration(df_seroptions *options, error_info *ei,
                                           const char *filename, int line,
                                           const char *value, ns_map *namespaces)
{
  CHECK_CALL(parse_yes_no(ei,filename,line,&options->omit_xml_declaration,
                          "omit-xml-declaration",value))
  return 0;
}

static int df_parse_standalone(df_seroptions *options, error_info *ei,
                                           const char *filename, int line,
                                           const char *value, ns_map *namespaces)
{
  if (NULL != value) {
    if (!strcmp(value,"yes"))
      options->standalone = STANDALONE_YES;
    else if (!strcmp(value,"no"))
      options->standalone = STANDALONE_NO;
    else if (!strcmp(value,"omit"))
      options->standalone = STANDALONE_OMIT;
    else
      return invalid_seroption(ei,filename,line,"standalone",value);
  }
  return 0;
}

static int df_parse_undeclare_prefixes(df_seroptions *options, error_info *ei,
                                           const char *filename, int line,
                                           const char *value, ns_map *namespaces)
{
  CHECK_CALL(parse_yes_no(ei,filename,line,&options->undeclare_prefixes,
                          "undeclare-prefixes",value))
  return 0;
}

static int df_parse_use_character_maps(df_seroptions *options, error_info *ei,
                                           const char *filename, int line,
                                           const char *value, ns_map *namespaces)
{
  /* FIXME */
  return 0;
}

static int df_parse_version(df_seroptions *options, error_info *ei,
                                           const char *filename, int line,
                                           const char *value, ns_map *namespaces)
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

int df_parse_seroption(df_seroptions *options, int option, error_info *ei,
                       const char *filename, int line,
                       const char *value, ns_map *namespaces)
{
  assert(0 <= option);
  assert(SEROPTION_COUNT > option);
  /* FIXME: must always ensure that the values passed in are whitespace collapsed
     (including values that come from attribute value templates) */
  return parse_functions[option](options,ei,filename,line,value,namespaces);
}

df_seroptions *df_seroptions_new(nsname method)
{
  df_seroptions *options = (df_seroptions*)calloc(1,sizeof(df_seroptions));

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

  options->byte_order_mark = 0;
  options->cdata_section_elements = NULL;
  options->doctype_public = NULL;
  options->doctype_system = NULL;
  options->encoding = strdup("UTF-8");
  options->escape_uri_attributes = 1;
  options->include_content_type = 1;

  if (!nsname_isnull(method) && (NULL == method.ns) &&
      (!strcmp(method.name,"html") || (!strcmp(method.name,"xhtml"))))
    options->indent = 1;
  else
    options->indent = 0;


  options->media_type = NULL;
  if (!nsname_isnull(method) && (NULL == method.ns)) {
    if (!strcmp(method.name,"xml"))
      options->media_type = strdup("text/xml");
    else if (!strcmp(method.name,"html") || (!strcmp(method.name,"xhtml")))
      options->media_type = strdup("text/html");
    else if (!strcmp(method.name,"text"))
      options->media_type = strdup("text/plain");
  }

  options->method = nsname_copy(method);
  options->normalization_form = strdup("none");
  options->omit_xml_declaration = 0;
  options->standalone = STANDALONE_OMIT;
  options->undeclare_prefixes = 0;
  options->use_character_maps = NULL;
  options->version = strdup("1.0");

  return options;
}

void df_seroptions_free(df_seroptions *options)
{
  nsname_free(options->ident);
  list_free(options->use_character_maps,(list_d_t)nsname_ptr_free);
  free(options->doctype_public);
  free(options->doctype_system);
  free(options->encoding);
  free(options->media_type);
  nsname_free(options->method);
  free(options->normalization_form);
  list_free(options->use_character_maps,(list_d_t)nsname_ptr_free);
  free(options->version);
  free(options);
}



df_seroptions *df_seroptions_copy(df_seroptions *options)
{
  df_seroptions *copy = (df_seroptions*)calloc(1,sizeof(df_seroptions));

  copy->ident = nsname_copy(options->ident);
  copy->byte_order_mark = options->byte_order_mark;
  copy->cdata_section_elements = list_copy(options->cdata_section_elements,
                                           (list_copy_t)nsname_ptr_copy);
  copy->doctype_public = (options->doctype_public ? strdup(options->doctype_public) : NULL);
  copy->doctype_system = (options->doctype_system ? strdup(options->doctype_system) : NULL);
  copy->encoding = (options->encoding ? strdup(options->encoding) : NULL);
  copy->escape_uri_attributes = options->escape_uri_attributes;
  copy->include_content_type = options->include_content_type;
  copy->indent = options->indent;
  copy->media_type = (options->media_type ? strdup(options->media_type) : NULL);
  copy->method = nsname_copy(options->method);
  copy->normalization_form =
    (options->normalization_form ? strdup(options->normalization_form) : NULL);
  copy->omit_xml_declaration = options->omit_xml_declaration;
  copy->standalone = options->standalone;
  copy->undeclare_prefixes = options->undeclare_prefixes;
  copy->use_character_maps = list_copy(options->use_character_maps,
                                       (list_copy_t)nsname_ptr_copy);
  copy->version = (options->version ? strdup(options->version) : NULL);

  return copy;
}

static value *df_normalize_sequence(value *seq, error_info *ei)
{
  /* XSLT 2.0 and XQuery Serialization, section 2 */
  list *in = df_sequence_to_list(seq);
  list *s1 = NULL;
  list *s2 = NULL;
  list *s3 = NULL;
  list *s4 = NULL;
  list *s5 = NULL;
  list *s6 = NULL;
  list *l;
  int r = 0;
  node *doc = NULL;

  /* 1. If the sequence that is input to serialization is empty, create a sequence S1 that consists
     of a zero-length string. Otherwise, copy each item in the sequence that is input to
     serialization to create the new sequence S1. */
  if (NULL == in) {
    list_append(&s1,value_new_string(""));
  }
  else {
    for (l = in; l; l = l->next) {
      value *v = (value*)l->data;
      assert(SEQTYPE_ITEM == v->st->type);
      list_append(&s1,value_ref(v));
    }
  }

  /* 2. For each item in S1, if the item is atomic, obtain the lexical representation of the item
     by casting it to an xs:string and copy the string representation to the new sequence;
     otherwise, copy the item, which will be a node, to the new sequence. The new sequence is S2. */
    for (l = s1; l; l = l->next) {
      value *v = (value*)l->data;
      if (ITEM_ATOMIC == v->st->item->kind) {
        char *str = value_as_string(v);
        list_append(&s2,value_new_string(str));
        free(str);
      }
      else {
        list_append(&s2,value_ref(v));
      }
    }

  /* 3. For each subsequence of adjacent strings in S2, copy a single string to the new sequence
     equal to the values of the strings in the subsequence concatenated in order, each separated
     by a single space. Copy all other items to the new sequence. The new sequence is S3. */
  for (l = s2; l; l = l->next) {
    value *v = (value*)l->data;
    if (ITEM_ATOMIC == v->st->item->kind) {
      /* must be a string; see previous step */
      stringbuf *buf = stringbuf_new();

      stringbuf_format(buf,"%s",v->value.s);

      while (l->next && (ITEM_ATOMIC == ((value*)l->next->data)->st->item->kind)) {
        l = l->next;
        v = (value*)l->data;
        stringbuf_format(buf," %s",v->value.s);
      }

      list_append(&s3,value_new_string(buf->data));
      stringbuf_free(buf);
    }
    else {
      list_append(&s3,value_ref(v));
    }
  }

  /* 4. For each item in S3, if the item is a string, create a text node in the new sequence whose
     string value is equal to the string; otherwise, copy the item to the new sequence. The new
     sequence is S4. */
  for (l = s3; l; l = l->next) {
    value *v = (value*)l->data;
    if (ITEM_ATOMIC == v->st->item->kind) {
      /* must be a string; see previous step */
      node *n = node_new(NODE_TEXT);
      n->value = strdup(v->value.s);
      list_append(&s4,value_new_node(n));
    }
    else {
      list_append(&s4,value_ref(v));
    }
  }

  /* 5. For each item in S4, if the item is a document node, copy its children to the new sequence;
     otherwise, copy the item to the new sequence. The new sequence is S5. */
  for (l = s4; l; l = l->next) {
    value *v = (value*)l->data;
    if (ITEM_DOCUMENT == v->st->item->kind) {
      node *c;
      for (c = v->value.n->first_child; c; c = c->next)
        list_append(&s5,value_new_node(c));
    }
    else {
      list_append(&s5,value_ref(v));
    }
  }

  /* 6. For each subsequence of adjacent text nodes in S5, copy a single text node to the new
     sequence equal to the values of the text nodes in the subsequence concatenated in order. Any
     text nodes with values of zero length are dropped. Copy all other items to the new sequence.
     The new sequence is S6. */
  for (l = s5; l; l = l->next) {
    value *v = (value*)l->data;
    if (ITEM_TEXT == v->st->item->kind) {
      stringbuf *buf = stringbuf_new();
      node *oldtext = v->value.n;

      stringbuf_format(buf,"%s",oldtext->value);

      while (l->next && (ITEM_TEXT == ((value*)l->next->data)->st->item->kind)) {
        l = l->next;
        v = (value*)l->data;
        oldtext = v->value.n;
        stringbuf_format(buf,"%s",oldtext->value);
      }

      if (1 < buf->size) {
        node *text = node_new(NODE_TEXT);
        text->value = strdup(buf->data);
        list_append(&s6,value_new_node(text));
      }

      stringbuf_free(buf);
    }
    else {
      list_append(&s6,value_ref(v));
    }
  }

  /* 7. It is a serialization error [err:SE0001] if an item in S6 is an attribute node or a
     namespace node. Otherwise, construct a new sequence, S7, that consists of a single document
     node and copy all the items in the sequence, which are all nodes, as children of that
     document node. */
  for (l = s6; l; l = l->next) {
    value *v = (value*)l->data;
    if ((ITEM_ATTRIBUTE == v->st->item->kind) ||
        (ITEM_NAMESPACE == v->st->item->kind)) {
      error(ei,NULL,0,"SE0001","Output sequence cannot contain attribute or namespace nodes");
      r = 1;
      break;
    }
  }

  if (0 == r) {
    doc = node_new(NODE_DOCUMENT);
    for (l = s6; l; l = l->next) {
      value *v = (value*)l->data;
      assert(ITEM_ATOMIC != v->st->item->kind);
      node_add_child(doc,node_deep_copy(v->value.n));
    }
  }

  list_free(in,NULL);

  value_deref_list(s1);
  value_deref_list(s2);
  value_deref_list(s3);
  value_deref_list(s4);
  value_deref_list(s5);
  value_deref_list(s6);

  list_free(s1,NULL);
  list_free(s2,NULL);
  list_free(s3,NULL);
  list_free(s4,NULL);
  list_free(s5,NULL);
  list_free(s6,NULL);

  if (0 == r)
    return value_new_node(doc);
  else
    return NULL;
}

static int serialize_stringbuf(void *context, const char * buffer, int len)
{
  stringbuf_append((stringbuf*)context,buffer,len);
  return len;
}

static void number_node(node *n, int *num)
{
  node *c;
  n->nodeno = (*num)++;
  for (c = n->first_child; c; c = c->next)
    number_node(c,num);
}

static void dump_tree(node *n, int indent)
{
  node *c;
  if ((NULL != n->value) && !is_all_whitespace(n->value,strlen(n->value)))
    debug("%#i%d %s \"%s\"\n",2*indent,n->nodeno,df_item_kinds[n->type],n->value);
  else
    debug("%#i%d %s %#n\n",2*indent,n->nodeno,df_item_kinds[n->type],n->ident);
  for (c = n->first_child; c; c = c->next)
    dump_tree(c,indent+1);
}

static void insertws(node *parent, node *before, int have_newline, int depth)
{
  stringbuf *buf = stringbuf_new();
  node *wsnode = node_new(NODE_TEXT);
  if (!have_newline)
    stringbuf_format(buf,"\n%#i",INDENT_SPACES*depth);
  else
    stringbuf_format(buf,"%#i",INDENT_SPACES*depth+1);
  wsnode->value = strdup(buf->data);
  node_insert_child(parent,wsnode,before);
  stringbuf_free(buf);
}

static void add_indentation(node *n, int depth)
{
  node *c;
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

  for (c = n->first_child; c; c = c->next) {

    if (NODE_ELEMENT == c->type) {

      add_indentation(c,depth+1);

      debug("node %d: newline = %d, column = %d, depth = %d\n",c->nodeno,newline,column,depth);
      if (!nonws && (!newline || (INDENT_SPACES*depth >= column)))
        insertws(n,c,newline,depth);

      newline = 0;
      column = 0;
      nonws = 0;
    }
    else if (NODE_TEXT == c->type) {
      const char *ch;
      for (ch = c->value; '\0' != *ch; ch++) {
        if (!isspace(*ch))
          nonws = 1;
        if (('\n' == *ch) || ('\r' == *ch)) {
          newline = 1;
          column = 0;
        }
        column++;
      }
    }

    if (NULL == c->next) {
      if ((0 < depth) && !nonws && (!newline || (INDENT_SPACES*(depth-1) >= column)))
        insertws(n,c->next,newline,depth-1);
    }
  }
}

static node *get_indented_doc(node *n)
{
  node *copy = node_ref(node_deep_copy(n));
  int num = 0;

  number_node(copy,&num);
  dump_tree(copy,0);

  debug("before add_indentation\n");
  add_indentation(copy,0);
  debug("after add_indentation\n");

  return copy;
}

int df_serialize(value *v, stringbuf *buf, df_seroptions *options,
                 error_info *ei)
{
  value *normseq = df_normalize_sequence(v,ei);
  node *doc;
  xmlOutputBuffer *xb;
  xmlTextWriter *writer;
  char *newline;
  int contentstart;
  int contentlen;

  if (NULL == normseq)
    return -1;

  /* FIXME: when indent is disabled, saxon does not print a newline after the <?xml...?>
     declaration. If we want to be consistent with saxon (for testing purposes) we need to do
     this but I'm not sure if libxml supports it. */

  debug("serialize: after getting normseq: value has %d refs, doc has %d refs\n",
        normseq->refcount,normseq->value.n->refcount);

  doc = node_ref(normseq->value.n);
  value_deref(normseq);

  debug("serialize: after getting doc: doc has %d refs\n",doc->refcount);

  assert(node_check_tree(doc));

  if (options->indent) {
    node *olddoc = doc;
    doc = get_indented_doc(olddoc);
    assert(node_check_tree(doc));
    debug("identation: new doc has %d refs\n",doc->refcount);
    node_deref(olddoc);
  }

  xb = xmlOutputBufferCreateIO(serialize_stringbuf,NULL,buf,NULL);
  writer = xmlNewTextWriter(xb);

  xmlTextWriterStartDocument(writer,NULL,"UTF-8",NULL);

  node_print(writer,doc);

  xmlTextWriterEndDocument(writer);
  xmlTextWriterFlush(writer);
  xmlFreeTextWriter(writer);

  /* Remove newline after XML declaration */
  newline = strchr(buf->data,'\n');
  contentstart = newline+1-buf->data;
  contentlen = buf->size-1-contentstart;
  assert(NULL != newline);
  memmove(newline,newline+1,contentlen);
  buf->size--;
  buf->data[buf->size-1] = '\0';

  /* Remove newline at end */
  assert('\n' == buf->data[buf->size-2]);
  buf->size--;
  buf->data[buf->size-1] = '\0';

  node_deref(doc);

  return 0;
}

/* Note: this is only safe to call on a newly created tree! If there are variables that
   reference nodes in the tree then this could result invalid references due to whitespace
   text nodes being deleted. */
void df_strip_spaces(node *n, list *space_decls)
{
  node *c = n->first_child;
  int strip = ((NODE_ELEMENT == n->type) && !space_decl_preserve(space_decls,n->ident));
  while (c) {

    if ((NODE_TEXT == c->type) && strip && is_all_whitespace(c->value,strlen(c->value))) {
      node *next = c->next;
      if (n->first_child == c)
        n->first_child = c->next;
      if (n->last_child == c)
        n->last_child = c->prev;
      if (NULL != c->prev)
        c->prev->next = c->next;
      if (NULL != c->next)
        c->next->prev = c->prev;
      node_free(c);
      c = next;
    }
    else {
      df_strip_spaces(c,space_decls);
      c = c->next;
    }
  }
}

void df_namespace_fixup(node *n)
{
  /* XSLT 2.0: 5.7.3 Namespace Fixup */
}

static int space_decl_better(space_decl *nw, space_decl *existing)
{
  if (nw->importpred < existing->importpred)
    return 0;
  if (nw->priority < existing->priority)
    return 0;
  return 1;
}

int space_decl_preserve(list *space_decls, nsname elemname)
{
  space_decl *best_match = NULL;
  list *l;

  for (l = space_decls; l; l = l->next) {
    space_decl *decl = (space_decl*)l->data;
    if (nsnametest_matches(decl->nnt,elemname) &&
        ((NULL == best_match) || space_decl_better(decl,best_match)))
      best_match = decl;
  }

  return best_match ? best_match->preserve : 1;
}

space_decl *space_decl_copy(space_decl *decl)
{
  space_decl *copy = (space_decl*)calloc(1,sizeof(space_decl));
  copy->nnt = nsnametest_copy(decl->nnt);
  copy->importpred = decl->importpred;
  copy->priority = decl->priority;
  copy->preserve = decl->preserve;
  return copy;
}

void space_decl_free(space_decl *decl)
{
  nsnametest_free(decl->nnt);
  free(decl);
}

