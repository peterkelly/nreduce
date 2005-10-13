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

#include "dataflow/sequencetype.h"
#include "dataflow/dataflow.h"
#include "dataflow/engine.h"
#include "util/xmlutils.h"
#include "util/debug.h"
#include "util/macros.h"
#include "xpath.h"
#include <assert.h>
#include <string.h>

#define FNS SPECIAL_NAMESPACE

static void remove_zero_length_text_nodes(xs_globals *g, list **values)
{
  list **lptr = values;
  while (*lptr) {
    df_value *v = (df_value*)((*lptr)->data);
    if ((ITEM_TEXT == v->seqtype->item->kind) && (0 == strlen(v->value.n->value))) {
      list *del = *lptr;
      *lptr = (*lptr)->next;
      free(del);
      df_value_deref(g,v);
    }
    else {
      lptr = &((*lptr)->next);
    }
  }
}

static void merge_adjacent_text_nodes(xs_globals *g, list **values)
{
  list **lptr = values;
  while (*lptr) {
    df_value *v = (df_value*)((*lptr)->data);

    if (ITEM_TEXT == v->seqtype->item->kind) {
      stringbuf *buf = stringbuf_new();
      list **nextptr = &((*lptr)->next);
      df_node *textnode;

      stringbuf_format(buf,"%s",v->value.n->value);

      while (*nextptr && (ITEM_TEXT == ((df_value*)((*nextptr)->data))->seqtype->item->kind)) {
        df_value *v2 = (df_value*)((*nextptr)->data);
        list *del = *nextptr;
        stringbuf_format(buf,"%s",v2->value.n->value);
        df_value_deref(g,v2);
        *nextptr = (*nextptr)->next;
        free(del);
      }

      df_value_deref(g,v);

      textnode = df_node_new(NODE_TEXT);
      textnode->value = strdup(buf->data);
      (*lptr)->data = df_value_new_node(textnode);

      stringbuf_free(buf);
    }

    lptr = &((*lptr)->next);
  }
}

int df_construct_complex_content(error_info *ei, sourceloc sloc, xs_globals *g,
                                 list *values, df_node *parent)
{
  list *l;
  list **lptr;
  int havenotnsattr = 0;

  values = list_copy(values,(list_copy_t)df_value_ref);

  /*

  @implements(xslt20:sequence-constructors-1) @end
  @implements(xslt20:sequence-constructors-2) @end
  @implements(xslt20:sequence-constructors-3) @end
  @implements(xslt20:sequence-constructors-4) @end
  @implements(xslt20:sequence-constructors-5) @end
  @implements(xslt20:sequence-constructors-6) @end
  @implements(xslt20:sequence-constructors-7) @end

  @implements(xslt20:constructing-complex-content-1) @end
  @implements(xslt20:constructing-complex-content-3) @end
  @implements(xslt20:constructing-complex-content-4)
  status { partial }
  issue { need support for namespace fixup }
  issue { need support for namespace inheritance }
  test { xslt/sequence/complex10.test }
  test { xslt/sequence/complex11.test }
  test { xslt/sequence/complex12.test }
  test { xslt/sequence/complex13.test }
  test { xslt/sequence/complex14.test }
  test { xslt/sequence/complex15.test }
  test { xslt/sequence/complex16.test }
  test { xslt/sequence/complex17.test }
  test { xslt/sequence/complex18.test }
  test { xslt/sequence/complex1.test }
  test { xslt/sequence/complex2.test }
  test { xslt/sequence/complex3.test }
  test { xslt/sequence/complex4.test }
  test { xslt/sequence/complex5.test }
  test { xslt/sequence/complex6.test }
  test { xslt/sequence/complex7.test }
  test { xslt/sequence/complex8.test }
  test { xslt/sequence/complex9.test }
  @end
  @implements(xslt20:constructing-complex-content-5)
  test { xslt/sequence/complexex.test }
  @end

  */


  /* XSLT 2.0: 5.7.1 Constructing Complex Content */

  /* 1. The containing instruction may generate attribute nodes and/or namespace nodes, as
     specified in the rules for the individual instruction. For example, these nodes may be
     produced by expanding an [xsl:]use-attribute-sets attribute, or by expanding the attributes
     of a literal result element. Any such nodes are prepended to the sequence produced by
     evaluating the sequence constructor. */

  /* FIXME */

  /* 2. Any atomic value in the sequence is cast to a string.

     Note: Casting from xs:QName or xs:NOTATION to xs:string always succeeds, because these values
     retain a prefix for this purpose. However, there is no guarantee that the prefix used will
     always be meaningful in the context where the resulting string is used. */

  /* FIXME */
  for (l = values; l; l = l->next) {
    df_value *v = (df_value*)l->data;
    assert(SEQTYPE_ITEM == v->seqtype->type);
    if (ITEM_ATOMIC == v->seqtype->item->kind) {
      char *str = df_value_as_string(g,v);
      df_value *newv = df_value_new_string(g,str);
      free(str);
      df_value_deref(g,v);
      l->data = newv;
    }
  }

  /* 3. Any consecutive sequence of strings within the result sequence is converted to a single
     text node, whose string value contains the content of each of the strings in turn, with a
     single space (#x20) used as a separator between successive strings. */
  lptr = &values;
  while (*lptr) {
    df_value *v = (df_value*)((*lptr)->data);
    if (ITEM_ATOMIC == v->seqtype->item->kind) {
      /* Must be a string; see previous step */
      stringbuf *buf = stringbuf_new();
      list **nextptr = &((*lptr)->next);
      df_node *textnode;

      stringbuf_format(buf,"%s",v->value.s);

      while (*nextptr && (ITEM_ATOMIC == ((df_value*)((*nextptr)->data))->seqtype->item->kind)) {
        df_value *v2 = (df_value*)((*nextptr)->data);
        list *del = *nextptr;
        stringbuf_format(buf," %s",v2->value.s);
        df_value_deref(g,v2);
        *nextptr = (*nextptr)->next;
        free(del);
      }

      df_value_deref(g,v);

      textnode = df_node_new(NODE_TEXT);
      textnode->value = strdup(buf->data);
      (*lptr)->data = df_value_new_node(textnode);

      stringbuf_free(buf);
    }
    lptr = &((*lptr)->next);
  }

  /* 4. Any document node within the result sequence is replaced by a sequence containing each of
     its children, in document order. */
  lptr = &values;
  while (*lptr) {
    df_value *v = (df_value*)((*lptr)->data);
    if (ITEM_DOCUMENT == v->seqtype->item->kind) {
      list *del = *lptr;
      df_node *doc = v->value.n;
      df_node *c;

      *lptr = (*lptr)->next;
      free(del);

      for (c = doc->first_child; c; c = c->next) {
        df_value *newv = df_value_new_node(c);
        list_push(lptr,newv);
        lptr = &((*lptr)->next);
      }

      df_value_deref(g,v);
    }
    else {
      lptr = &((*lptr)->next);
    }
  }

  /* 5. Zero-length text nodes within the result sequence are removed. */
  remove_zero_length_text_nodes(g,&values);

  /* 6. Adjacent text nodes within the result sequence are merged into a single text node. */
  merge_adjacent_text_nodes(g,&values);

  /* 7. Invalid namespace and attribute nodes are detected as follows. */

  for (l = values; l; l = l->next) {
    df_value *v = (df_value*)l->data;
    if ((ITEM_NAMESPACE == v->seqtype->item->kind) || (ITEM_ATTRIBUTE == v->seqtype->item->kind)) {

      int err = 0;


      /* [ERR XTDE0410] It is a non-recoverable dynamic error if the result sequence used to
         construct the content of an element node contains a namespace node or attribute node that
         is preceded in the sequence by a node that is neither a namespace node nor an attribute
         node. */
      if (havenotnsattr) {
        error(ei,sloc.uri,sloc.line,"XTDE0410","Namespaces and attributes can only "
              "appear at the start of a sequence used to construct complex content");
        err = 1;
      }


      /* [ERR XTDE0420] It is a non-recoverable dynamic error if the result sequence used to
         construct the content of a document node contains a namespace node or attribute node. */
      if (NODE_DOCUMENT == parent->type) {
        /* FIXME: write test for this; requires support for xsl:document */
        error(ei,sloc.uri,sloc.line,"XTDE0420","A sequence used to construct the "
              "content of a document node cannot contain namespace or attribute nodes");
        err = 1;
      }

      /* [ERR XTDE0430] It is a non-recoverable dynamic error if the result sequence contains two
         or more namespace nodes having the same name but different string values (that is,
         namespace nodes that map the same prefix to different namespace URIs). */
      if (ITEM_NAMESPACE == v->seqtype->item->kind) {
        list *l2;
        for (l2 = values; l2 != l; l2 = l2->next) {
          df_value *v2 = (df_value*)l2->data;
          if ((ITEM_NAMESPACE == v2->seqtype->item->kind) &&
              !strcmp(v->value.n->prefix,v2->value.n->prefix) &&
              strcmp(v->value.n->value,v2->value.n->value)) {
            error(ei,sloc.uri,sloc.line,"XTDE0430","Sequence contains two namespace "
                  "nodes trying to map the prefix \"%s\" to different URIs (\"%s\" and \"%s\")",
                  v->value.n->prefix,v->value.n->value,v2->value.n->value);
            err = 1;
            break;
          }
        }
      }

      /* [ERR XTDE0440] It is a non-recoverable dynamic error if the result sequence contains a
         namespace node with no name and the element node being constructed has a null namespace
         URI (that is, it is an error to define a default namespace when the element is in no
         namespace). */
      if ((NODE_ELEMENT == parent->type) && (NULL == parent->ident.ns) &&
          (ITEM_NAMESPACE == v->seqtype->item->kind) && (NULL == v->value.n->prefix)) {
        error(ei,sloc.uri,sloc.line,"XTDE0440","Sequence contains a namespace node "
              "with no name and the element node being constructed has a null namespace URI");
        err = 1;
      }

      if (err) {
        df_value_deref_list(g,values);
        list_free(values,NULL);
        return -1;
      }

    }
    else {
      havenotnsattr = 1;
    }
  }

  /* 8. If the result sequence contains two or more namespace nodes with the same name (or no name)
        and the same string value (that is, two namespace nodes mapping the same prefix to the same
        namespace URI), then all but one of the duplicate nodes are discarded.

      Note: Since the order of namespace nodes is undefined, it is not significant which of the
      duplicates is retained. */
  lptr = &values;
  while (*lptr) {
    df_value *v = (df_value*)((*lptr)->data);
    int removed = 0;
    if (ITEM_NAMESPACE == v->seqtype->item->kind) {
      list *l2;
      for (l2 = values; l2 != *lptr; l2 = l2->next) {
        df_value *v2 = (df_value*)l2->data;
        if ((ITEM_NAMESPACE == v2->seqtype->item->kind) &&
            !strcmp(v->value.n->prefix,v2->value.n->prefix)) {
          list *del = *lptr;
          *lptr = (*lptr)->next;
          df_value_deref(g,v);
          free(del);
          removed = 1;
          break;
        }
      }
    }
    if (!removed)
      lptr = &((*lptr)->next);
  }

  /* 9. If an attribute A in the result sequence has the same name as another attribute B that
        appears later in the result sequence, then attribute A is discarded from the result
        sequence. */
  lptr = &values;
  while (*lptr) {
    df_value *v = (df_value*)((*lptr)->data);
    int removed = 0;

    if (ITEM_ATTRIBUTE == v->seqtype->item->kind) {
      list *l2;
      for (l2 = (*lptr)->next; l2; l2 = l2->next) {
        df_value *v2 = (df_value*)l2->data;
        if ((ITEM_ATTRIBUTE == v2->seqtype->item->kind) &&
            nsname_equals(v->value.n->ident,v2->value.n->ident)) {
          list *del = *lptr;
          *lptr = (*lptr)->next;
          df_value_deref(g,v);
          free(del);
          removed = 1;
          break;
        }
      }
    }

    if (!removed)
      lptr = &((*lptr)->next);
  }

  /* FIXME */

  /* 10. Each node in the resulting sequence is attached as a namespace, attribute, or child of the
     newly constructed element or document node. Conceptually this involves making a deep copy of
     the node; in practice, however, copying the node will only be necessary if the existing node
     can be referenced independently of the parent to which it is being attached. When copying an
     element or processing instruction node, its base URI property is changed to be the same as
     that of its new parent, unless it has an xml:base attribute (see [XMLBASE]) that overrides
     this. If the element has an xml:base attribute, its base URI is the value of that attribute,
     resolved (if it is relative) against the base URI of the new parent node. */

  for (l = values; l; l = l->next) {
    df_value *v = (df_value*)l->data;
    /* FIXME: avoid copying here where possible */
    assert(ITEM_ATOMIC != v->seqtype->item->kind);

    if (ITEM_ATTRIBUTE == v->seqtype->item->kind)
      df_node_add_attribute(parent,df_node_deep_copy(v->value.n));
    else if (ITEM_NAMESPACE == v->seqtype->item->kind)
      df_node_add_namespace(parent,df_node_deep_copy(v->value.n));
    else
      df_node_add_child(parent,df_node_deep_copy(v->value.n));
  }

  /* 11. If the newly constructed node is an element node, then namespace fixup is applied to this
     node, as described in 5.7.3 Namespace Fixup. */

  /* FIXME */

  /* 12. If the newly constructed node is an element node, and if namespaces are inherited, then
     each namespace node of the newly constructed element (including any produced as a result of
     the namespace fixup process) is copied to each descendant element of the newly constructed
     element, unless that element or an intermediate element already has a namespace node with the
     same name (or absence of a name). */

  /* FIXME */


  df_value_deref_list(g,values);
  list_free(values,NULL);

  return 0;
}

char *df_construct_simple_content(xs_globals *g, list *values, const char *separator)
{
  /* XSLT 2.0: 5.7.2 Constructing Simple Content */
  stringbuf *buf = stringbuf_new();
  char *str;
  list *l;

  values = list_copy(values,(list_copy_t)df_value_ref);

  /* @implements(xslt20:constructing-simple-content-1) @end */
  /* @implements(xslt20:constructing-simple-content-2)
     status { partial }
     issue { need support for attribute value templates with fixed and variable parts }
     @end */
  /* @implements(xslt20:constructing-simple-content-3)
     test { xslt/sequence/simple1.test }
     test { xslt/sequence/simple2.test }
     test { xslt/sequence/simple3.test }
     test { xslt/sequence/simple4.test }
     @end */
  /* @implements(xslt20:constructing-simple-content-4)
     status { partial }
     issue { need support for attribute value templates with fixed and variable parts }
     @end */

  /* 1. Zero-length text nodes in the sequence are discarded. */
  remove_zero_length_text_nodes(g,&values);

  /* 2. Adjacent text nodes in the sequence are merged into a single text node. */
  merge_adjacent_text_nodes(g,&values);

  /* 3. The sequence is atomized. */
  for (l = values; l; l = l->next) {
    df_value *v = (df_value*)l->data;
    df_value *atom = df_atomize(g,v);
    df_value_deref(g,v);
    l->data = atom;
  }

  /* 4. Every value in the atomized sequence is cast to a string. */
  for (l = values; l; l = l->next) {
    df_value *v = (df_value*)l->data;
    char *s = df_value_as_string(g,v);
    df_value *strval = df_value_new_string(g,s);
    free(s);
    df_value_deref(g,v);
    l->data = strval;
  }

  /* 5. The strings within the resulting sequence are concatenated, with a (possibly zero-length)
     separator inserted between successive strings. The default separator is a single space. In the
     case of xsl:attribute and xsl:value-of, a different separator can be specified using the
     separator attribute of the instruction; it is permissible for this to be a zero-length string,
     in which case the strings are concatenated with no separator. In the case of xsl:comment,
     xsl:processing-instruction, and xsl:namespace, and when expanding an attribute value template,
     the default separator cannot be changed. */
  for (l = values; l; l = l->next) {
    df_value *v = (df_value*)l->data;
    stringbuf_format(buf,"%s",v->value.s);
    if (l->next)
      stringbuf_format(buf,"%s",separator);
  }

  /* 6. The string that results from this concatenation forms the string value of the new
     attribute, namespace, comment, processing-instruction, or text node. */
  str = strdup(buf->data);

  stringbuf_free(buf);
  df_value_deref_list(g,values);
  list_free(values,NULL);

  return str;
}

static gxvalue *element(gxenvironment *env, gxvalue **args)
{
  list *values = df_sequence_to_list(args[0]);
  gxvalue *elemvalue;
  gxnode *elem = df_node_new(NODE_ELEMENT);

  elemvalue = mknode(elem);

  assert(df_check_derived_atomic_type(args[1],env->g->string_type));
  elem->ident.name = strdup(args[1]->value.s);

  /* FIXME: namespace (should be received on an input port) - what about prefix? */

  debug("element %p (\"%s\"): %d items in child sequence\n",
        elemvalue,elem->ident.name,list_count(values));
  if (0 != df_construct_complex_content(env->ei,env->sloc,env->g,values,elem)) {
    vderef(elemvalue);
    return NULL;
  }

  list_free(values,NULL);

  return elemvalue;
}

static gxvalue *range(gxenvironment *env, gxvalue **args)
{
  int min;
  int max;
  list *range = NULL;
  gxvalue *result;

  /* FIXME: support empty sequences and conversion of other types when passed in */
  assert(df_check_derived_atomic_type(args[0],env->g->int_type));
  assert(df_check_derived_atomic_type(args[1],env->g->int_type));
  min = args[0]->value.i;
  max = args[1]->value.i;

  if (min <= max) {
    int i;
    for (i = max; i >= min; i--)
      list_push(&range,mkint(i));
  }

  result = df_list_to_sequence(env->g,range);
  df_value_deref_list(env->g,range);
  list_free(range,NULL);

  return result;
}

static gxvalue *contains_node(gxenvironment *env, gxvalue **args)
{
  list *nodevals = df_sequence_to_list(args[0]);
  df_node *n = args[1]->value.n;
  int found = 0;
  list *l;
  assert(SEQTYPE_ITEM == args[1]->seqtype->type);
  assert(ITEM_ATOMIC != args[1]->seqtype->item->kind);

  for (l = nodevals; l; l = l->next) {
    df_value *nv = (df_value*)l->data;
    assert(SEQTYPE_ITEM == nv->seqtype->type);
    assert(ITEM_ATOMIC != nv->seqtype->item->kind);
    if (nv->value.n == n)
      found = 1;
  }

  list_free(nodevals,NULL);

  return mkbool(found);
}

static gxvalue *select_root(gxenvironment *env, gxvalue **args)
{
  list *values = df_sequence_to_list(args[0]);
  list *output = NULL;
  list *l;
  df_value *result;

  for (l = values; l; l = l->next) {
    df_value *v = (df_value*)l->data;
    df_node *root;
    /* FIXME: raise a dynamic error here instead of asserting */
    if ((SEQTYPE_ITEM != v->seqtype->type) || (ITEM_ATOMIC == v->seqtype->item->kind)) {
      fprintf(stderr,"ERROR: non-node in selectroot input\n");
      assert(0);
    }
    root = v->value.n;
    while (NULL != root->parent)
      root = root->parent;
    list_append(&output,df_value_new_node(root));
  }

  list_free(values,NULL);
  result = df_list_to_sequence(env->g,output);
  df_value_deref_list(env->g,output);
  list_free(output,NULL);

  return result;
}

static int nodetest_matches(df_node *n, char *nametest, df_seqtype *seqtypetest)
{
  if (NULL != nametest) {
    return (((NODE_ELEMENT == n->type) || (NODE_ATTRIBUTE == n->type)) &&
            !strcmp(n->ident.name,nametest));
  }
  else {
    assert(NULL != seqtypetest);
    if (SEQTYPE_ITEM == seqtypetest->type) {
      switch (seqtypetest->item->kind) {
      case ITEM_ATOMIC:
        /* FIXME (does this even apply here?) */
        break;
      case ITEM_DOCUMENT:
        /* FIXME: support checks based on document type, e.g. document(element(foo)) */
        return (n->type == NODE_DOCUMENT);
        break;
      case ITEM_ELEMENT:
        if (NULL != seqtypetest->item->elem) {
          /* FIXME: support checks based on type annotation of node, e.g. element(*,xs:string) */
          return ((n->type == NODE_ELEMENT) &&
                  nsname_equals(n->ident,seqtypetest->item->elem->def.ident));
        }
        else {
          return (n->type == NODE_ELEMENT);
        }
        break;
      case ITEM_ATTRIBUTE:
        /* FIXME */
        break;
      case ITEM_PI:
        return (n->type == NODE_PI);
        break;
      case ITEM_COMMENT:
        return (n->type == NODE_COMMENT);
        break;
      case ITEM_TEXT:
        return (n->type == NODE_TEXT);
        break;
      case ITEM_NAMESPACE:
        return (n->type == NODE_NAMESPACE);
        break;
      default:
        assert(!"invalid item type");
        break;
      }
    }
    else if (SEQTYPE_CHOICE == seqtypetest->type) {
      return (nodetest_matches(n,NULL,seqtypetest->left) ||
              nodetest_matches(n,NULL,seqtypetest->right));
    }
  }

  return 0;
}

static int append_matching_nodes(df_node *self, char *nametest, df_seqtype *seqtypetest, int axis,
                                 list **matches)
{
  df_node *c;
  list *l;
  switch (axis) {
  case AXIS_CHILD:
    for (c = self->first_child; c; c = c->next)
      if (nodetest_matches(c,nametest,seqtypetest))
        list_append(matches,df_value_new_node(c));
    break;
  case AXIS_DESCENDANT:
    for (c = self->first_child; c; c = c->next)
      CHECK_CALL(append_matching_nodes(c,nametest,seqtypetest,AXIS_DESCENDANT_OR_SELF,matches))
    break;
  case AXIS_ATTRIBUTE:
    for (l = self->attributes; l; l = l->next) {
      df_node *attr = (df_node*)l->data;
      if (nodetest_matches(attr,nametest,seqtypetest)) {
        df_value *val = df_value_new_node(attr);
        list_append(matches,val);
        debugl("adding matching attribute %p; root %p refcount is now %d",
              val->value.n,df_node_root(val->value.n),df_node_root(val->value.n)->refcount);
      }
    }
    break;
  case AXIS_SELF:
    if (nodetest_matches(self,nametest,seqtypetest))
      list_append(matches,df_value_new_node(self));
    break;
  case AXIS_DESCENDANT_OR_SELF:
    CHECK_CALL(append_matching_nodes(self,nametest,seqtypetest,AXIS_SELF,matches))
    CHECK_CALL(append_matching_nodes(self,nametest,seqtypetest,AXIS_DESCENDANT,matches))
    break;
  case AXIS_FOLLOWING_SIBLING:
    /* FIXME */
    assert(!"not yet implemented");
    break;
  case AXIS_FOLLOWING:
    /* FIXME */
    assert(!"not yet implemented");
    break;
  case AXIS_NAMESPACE:
    /* FIXME */
    assert(!"not yet implemented");
    break;
  case AXIS_PARENT:
    /* FIXME */
    assert(!"not yet implemented");
    break;
  case AXIS_ANCESTOR:
    /* FIXME */
    assert(!"not yet implemented");
    break;
  case AXIS_PRECEDING_SIBLING:
    /* FIXME */
    assert(!"not yet implemented");
    break;
  case AXIS_PRECEDING:
    /* FIXME */
    assert(!"not yet implemented");
    break;
  case AXIS_ANCESTOR_OR_SELF:
    /* FIXME */
    assert(!"not yet implemented");
    break;
  default:
    assert(!"invalid axis");
    break;
  }
  return 0;
}

static gxvalue *select1(gxenvironment *env, gxvalue **args)
{
  list *values = df_sequence_to_list(args[0]);
  list *output = NULL;
  list *l;
  df_value *result;

  for (l = values; l; l = l->next) {
    df_value *v = (df_value*)l->data;
    /* FIXME: raise a dynamic error here instead of asserting */
    if ((SEQTYPE_ITEM != v->seqtype->type) || (ITEM_ATOMIC == v->seqtype->item->kind)) {
      fprintf(stderr,"ERROR: non-node in select input\n");
      assert(0);
    }
    if (ITEM_ATOMIC != v->seqtype->item->kind) {
      if (0 != append_matching_nodes(v->value.n,env->instr->nametest,env->instr->seqtypetest,
                                       env->instr->axis,&output)) {
        df_value_deref_list(env->g,output);
        list_free(output,NULL);
        return NULL;
      }
    }
  }

  list_free(values,NULL);
  result = df_list_to_sequence(env->g,output);
  df_value_deref_list(env->g,output);
  list_free(output,NULL);

  return result;
}

static gxvalue *namespace(gxenvironment *env, gxvalue **args)
{
  /* @implements(xslt20:creating-namespace-nodes-2)
     test { xslt/eval/namespace1.test }
     test { xslt/eval/namespace2.test }
     test { xslt/eval/namespace3.test }
     test { xslt/eval/namespace4.test }
     @end */

  df_node *nsnode = df_node_new(NODE_NAMESPACE);
  list *values = df_sequence_to_list(args[0]);
  char *prefix = df_construct_simple_content(env->g,values," ");
  list_free(values,NULL);
  if (0 == strlen(prefix)) {
    nsnode->prefix = NULL;
    free(prefix);
  }
  else {
    nsnode->prefix = prefix;
  }
  values = df_sequence_to_list(args[1]);
  nsnode->value = df_construct_simple_content(env->g,values," ");
  list_free(values,NULL);

  return mknode(nsnode);
}

static gxvalue *text(gxenvironment *env, gxvalue **args)
{
  df_node *textnode = df_node_new(NODE_TEXT);
  textnode->value = strdup(env->instr->str);
  return mknode(textnode);
}

static gxvalue *value_of(gxenvironment *env, gxvalue **args)
{
  /* @implements(xslt20:value-of-1)
     test { xslt/eval/value-of1.test }
     test { xslt/eval/value-of2.test }
     test { xslt/eval/value-of3.test }
     test { xslt/eval/value-of4.test }
     @end
     @implements(xslt20:value-of-3) @end
     @implements(xslt20:value-of-4) @end
     @implements(xslt20:value-of-6) @end
     @implements(xslt20:value-of-7) status { deferred } @end
     @implements(xslt20:value-of-8)
     test { xslt/eval/value-ofex.test }
     @end
     @implements(xslt20:value-of-9) @end
     @implements(xslt20:value-of-10) @end */

  df_node *textnode = df_node_new(NODE_TEXT);
  char *separator = df_value_as_string(env->g,args[1]);
  list *values = df_sequence_to_list(args[0]);
  textnode->value = df_construct_simple_content(env->g,values,separator);
  free(separator);
  list_free(values,NULL);
  return mknode(textnode);
}

static gxvalue *attribute2(gxenvironment *env, gxvalue **args)
{
  df_node *attr = df_node_new(NODE_ATTRIBUTE);
  list *values = df_sequence_to_list(args[0]);
  list *namevals = df_sequence_to_list(args[1]);

  /* FIXME: support arbitraty sequence as name - used for attribute value templates in
     xsl:attribute */
  /* FIXME: support namespace */

  assert(df_check_derived_atomic_type(args[1],env->g->string_type));
  attr->ident.name = df_construct_simple_content(env->g,namevals," ");
  attr->value = df_construct_simple_content(env->g,values," ");

  list_free(values,NULL);
  list_free(namevals,NULL);

  return mknode(attr);
}

static gxvalue *document(gxenvironment *env, gxvalue **args)
{
  df_node *docnode = df_node_new(NODE_DOCUMENT);
  list *values = df_sequence_to_list(args[0]);
  df_value *docval = df_value_new_node(docnode);

  if (0 != df_construct_complex_content(env->ei,env->instr->sloc,env->g,values,docnode)) {
    df_value_deref(env->g,docval);
    list_free(values,NULL);
    return NULL;
  }

  /* FIXME: i think this is already checked df_construct_complex_content()? */
  if (NULL != docnode->attributes) {
    error(env->ei,"",0,"XTDE0420",
          "Attribute nodes cannot be added directly as children of a document");
    df_value_deref(env->g,docval);
    list_free(values,NULL);
    return NULL;
  }

  list_free(values,NULL);

  return docval;
}

static gxvalue *sequence(gxenvironment *env, gxvalue **args)
{
  df_value *pair;
  df_seqtype *st = df_seqtype_new(SEQTYPE_SEQUENCE);
  st->left = df_seqtype_ref(args[0]->seqtype);
  st->right = df_seqtype_ref(args[1]->seqtype);
  pair = df_value_new(st);
  df_seqtype_deref(st);
  pair->value.pair.left = vref(args[0]);
  pair->value.pair.right = vref(args[1]);
  return pair;
}

static gxvalue *output(gxenvironment *env, gxvalue **args)
{
  stringbuf *buf = stringbuf_new();
  assert(NULL != env->instr->seroptions);
  if (0 != df_serialize(env->g,args[0],buf,env->instr->seroptions,env->ei)) {
    stringbuf_free(buf);
    return NULL;
  }
  printf("%s",buf->data);
  stringbuf_free(buf);
  return df_list_to_sequence(env->g,NULL);
}

static gxvalue *filter(gxenvironment *env, gxvalue **args)
{
  assert(0);
    /* FIXME */
/*
    list *values = df_sequence_to_list(a->values[0]);
    list *mask = df_sequence_to_list(a->values[1]);
    list *output = NULL;
    list *vl;
    list *ml;

    assert(list_count(values) == list_count(mask));

    vl = values;
    ml = mask;
    while (vl) {
      df_value *v = (df_value*)vl->data;
      df_value *m = (df_value*)ml->data;

      vl = vl->next;
      ml = ml->next;
    }
*/
  return NULL;
}

static gxvalue *empty(gxenvironment *env, gxvalue **args)
{
  df_seqtype *st = df_seqtype_new(SEQTYPE_EMPTY);
  df_value *result = df_value_new(st);
  df_seqtype_deref(st);
  return result;
}

gxfunctiondef special_fundefs[15] = {
  { element,       FNS, "element",       "item()*,xsd:string",        "element()"       },
  { range,         FNS, "range",         "xsd:integer?,xsd:integer?", "xsd:integer*"    },
  { contains_node, FNS, "contains-node", "node()*,node()",            "xsd:boolean"     },
  { select_root,   FNS, "select-root",   "node()",                    "node()"          },
  { select1,       FNS, "select",        "node()*",                   "node()*"         },
  { namespace,     FNS, "namespace",     "item()*,item()*",           "node()"          },
  { text,          FNS, "text",          "item()*",                   "text()"          },
  { value_of,      FNS, "value-of",      "item()*,item()*",           "text()"          },
  { attribute2,    FNS, "attribute",     "item()*,item()*",           "attribute()"     },
  { document,      FNS, "document",      "item()*",                   "document-node()" },
  { sequence,      FNS, "sequence",      "item()*,item()*",           "item()*"         },
  { output,        FNS, "output",        "item()*",                   "item()*"         },
  { filter,        FNS, "filter",        "item()*",                   "item()*"         },
  { empty,         FNS, "empty",         "item()*",                   "item()*"         },
  { NULL },
};

gxfunctiondef *special_module = special_fundefs;
