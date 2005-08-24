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

#include "serialization.h"
#include <assert.h>
#include <string.h>

static df_value *df_normalize_sequence(xs_globals *g, df_value *seq, error_info *ei)
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
  df_node *doc = NULL;

  /* 1. If the sequence that is input to serialization is empty, create a sequence S1 that consists
     of a zero-length string. Otherwise, copy each item in the sequence that is input to
     serialization to create the new sequence S1. */
  if (NULL == in) {
    list_append(&s1,df_value_new_string(g,""));
  }
  else {
    for (l = in; l; l = l->next) {
      df_value *v = (df_value*)l->data;
      assert(SEQTYPE_ITEM == v->seqtype->type);
      list_append(&s1,df_value_ref(v));
    }
  }

  /* 2. For each item in S1, if the item is atomic, obtain the lexical representation of the item
     by casting it to an xs:string and copy the string representation to the new sequence;
     otherwise, copy the item, which will be a node, to the new sequence. The new sequence is S2. */
    for (l = s1; l; l = l->next) {
      df_value *v = (df_value*)l->data;
      if (ITEM_ATOMIC == v->seqtype->item->kind) {
        char *str = df_value_as_string(g,v);
        list_append(&s2,df_value_new_string(g,str));
        free(str);
      }
      else {
        list_append(&s2,df_value_ref(v));
      }
    }

  /* 3. For each subsequence of adjacent strings in S2, copy a single string to the new sequence
     equal to the values of the strings in the subsequence concatenated in order, each separated
     by a single space. Copy all other items to the new sequence. The new sequence is S3. */
  for (l = s2; l; l = l->next) {
    df_value *v = (df_value*)l->data;
    if (ITEM_ATOMIC == v->seqtype->item->kind) {
      /* must be a string; see previous step */
      stringbuf *buf = stringbuf_new();

      stringbuf_format(buf,"%s",v->value.s);

      while (l->next && (ITEM_ATOMIC == ((df_value*)l->next->data)->seqtype->item->kind)) {
        l = l->next;
        v = (df_value*)l->data;
        stringbuf_format(buf," %s",v->value.s);
      }

      list_append(&s3,df_value_new_string(g,buf->data));
      stringbuf_free(buf);
    }
    else {
      list_append(&s3,df_value_ref(v));
    }
  }

  /* 4. For each item in S3, if the item is a string, create a text node in the new sequence whose
     string value is equal to the string; otherwise, copy the item to the new sequence. The new
     sequence is S4. */
  for (l = s3; l; l = l->next) {
    df_value *v = (df_value*)l->data;
    if (ITEM_ATOMIC == v->seqtype->item->kind) {
      /* must be a string; see previous step */
      df_node *n = df_node_new(NODE_TEXT);
      n->value = strdup(v->value.s);
      list_append(&s4,df_node_to_value(n));
    }
    else {
      list_append(&s4,df_value_ref(v));
    }
  }

  /* 5. For each item in S4, if the item is a document node, copy its children to the new sequence;
     otherwise, copy the item to the new sequence. The new sequence is S5. */
  for (l = s4; l; l = l->next) {
    df_value *v = (df_value*)l->data;
    if (ITEM_DOCUMENT == v->seqtype->item->kind) {
      df_node *c;
      for (c = v->value.n->first_child; c; c = c->next)
        list_append(&s5,df_node_to_value(c));
    }
    else {
      list_append(&s5,df_value_ref(v));
    }
  }

  /* 6. For each subsequence of adjacent text nodes in S5, copy a single text node to the new
     sequence equal to the values of the text nodes in the subsequence concatenated in order. Any
     text nodes with values of zero length are dropped. Copy all other items to the new sequence.
     The new sequence is S6. */
  for (l = s5; l; l = l->next) {
    df_value *v = (df_value*)l->data;
    if (ITEM_TEXT == v->seqtype->item->kind) {
      stringbuf *buf = stringbuf_new();
      df_node *oldtext = v->value.n;

      stringbuf_format(buf,"%s",oldtext->value);

      while (l->next && (ITEM_TEXT == ((df_value*)l->next->data)->seqtype->item->kind)) {
        l = l->next;
        v = (df_value*)l->data;
        oldtext = v->value.n;
        stringbuf_format(buf,"%s",oldtext->value);
      }

      if (1 < buf->size) {
        df_node *text = df_node_new(NODE_TEXT);
        text->value = strdup(buf->data);
        list_append(&s6,df_node_to_value(text));
      }

      stringbuf_free(buf);
    }
    else {
      list_append(&s6,df_value_ref(v));
    }
  }

  /* 7. It is a serialization error [err:SE0001] if an item in S6 is an attribute node or a
     namespace node. Otherwise, construct a new sequence, S7, that consists of a single document
     node and copy all the items in the sequence, which are all nodes, as children of that
     document node. */
  for (l = s6; l; l = l->next) {
    df_value *v = (df_value*)l->data;
    if ((ITEM_ATTRIBUTE == v->seqtype->item->kind) ||
        (ITEM_NAMESPACE == v->seqtype->item->kind)) {
      error(ei,NULL,0,"SE0001","Output sequence cannot contain attribute or namespace nodes");
      r = 1;
      break;
    }
  }

  if (0 == r) {
    doc = df_node_new(NODE_DOCUMENT);
    for (l = s6; l; l = l->next) {
      df_value *v = (df_value*)l->data;
      assert(ITEM_ATOMIC != v->seqtype->item->kind);
      df_node_add_child(doc,df_node_deep_copy(v->value.n));
    }
  }

  list_free(in,NULL);

  df_value_deref_list(g,s1);
  df_value_deref_list(g,s2);
  df_value_deref_list(g,s3);
  df_value_deref_list(g,s4);
  df_value_deref_list(g,s5);
  df_value_deref_list(g,s6);

  list_free(s1,NULL);
  list_free(s2,NULL);
  list_free(s3,NULL);
  list_free(s4,NULL);
  list_free(s5,NULL);
  list_free(s6,NULL);

  if (0 == r)
    return df_node_to_value(doc);
  else
    return NULL;
}

static int serialize_stringbuf(void *context, const char * buffer, int len)
{
  stringbuf_append((stringbuf*)context,buffer,len);
  return len;
}

int df_serialize(xs_globals *g, df_value *v, stringbuf *buf, df_seroptions *options,
                 error_info *ei)
{
  df_value *normseq = df_normalize_sequence(g,v,ei);
  df_node *doc;
  xmlOutputBuffer *xb;
  xmlTextWriter *writer;

  if (NULL == normseq)
    return -1;

  doc = normseq->value.n;

  xb = xmlOutputBufferCreateIO(serialize_stringbuf,NULL,buf,NULL);
  writer = xmlNewTextWriter(xb);
/*   xmlTextWriterSetIndent(writer,1); */
/*   xmlTextWriterSetIndentString(writer,"  "); */
  xmlTextWriterStartDocument(writer,NULL,NULL,NULL);

  df_node_print(writer,doc);

  xmlTextWriterEndDocument(writer);
  xmlTextWriterFlush(writer);
  xmlFreeTextWriter(writer);

  df_value_deref(g,normseq);

  return 0;
}

