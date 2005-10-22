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
#include <math.h>

#define FNS SPECIAL_NAMESPACE

static void remove_zero_length_text_nodes(list **values)
{
  list **lptr = values;
  while (*lptr) {
    value *v = (value*)((*lptr)->data);
    if ((ITEM_TEXT == v->st->item->kind) && (0 == strlen(v->value.n->value))) {
      list *del = *lptr;
      *lptr = (*lptr)->next;
      free(del);
      value_deref(v);
    }
    else {
      lptr = &((*lptr)->next);
    }
  }
}

static void merge_adjacent_text_nodes(list **values)
{
  list **lptr = values;
  while (*lptr) {
    value *v = (value*)((*lptr)->data);

    if (ITEM_TEXT == v->st->item->kind) {
      stringbuf *buf = stringbuf_new();
      list **nextptr = &((*lptr)->next);
      node *textnode;

      stringbuf_format(buf,"%s",v->value.n->value);

      while (*nextptr && (ITEM_TEXT == ((value*)((*nextptr)->data))->st->item->kind)) {
        value *v2 = (value*)((*nextptr)->data);
        list *del = *nextptr;
        stringbuf_format(buf,"%s",v2->value.n->value);
        value_deref(v2);
        *nextptr = (*nextptr)->next;
        free(del);
      }

      value_deref(v);

      textnode = node_new(NODE_TEXT);
      textnode->value = strdup(buf->data);
      (*lptr)->data = value_new_node(textnode);

      stringbuf_free(buf);
    }

    lptr = &((*lptr)->next);
  }
}

static int df_construct_complex_content(error_info *ei, sourceloc sloc, list *values, node *parent)
{
  list *l;
  list **lptr;
  int havenotnsattr = 0;

  values = list_copy(values,(list_copy_t)value_ref);

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
    value *v = (value*)l->data;
    assert(SEQTYPE_ITEM == v->st->type);
    if (ITEM_ATOMIC == v->st->item->kind) {
      char *str = value_as_string(v);
      value *newv = value_new_string(str);
      free(str);
      value_deref(v);
      l->data = newv;
    }
  }

  /* 3. Any consecutive sequence of strings within the result sequence is converted to a single
     text node, whose string value contains the content of each of the strings in turn, with a
     single space (#x20) used as a separator between successive strings. */
  lptr = &values;
  while (*lptr) {
    value *v = (value*)((*lptr)->data);
    if (ITEM_ATOMIC == v->st->item->kind) {
      /* Must be a string; see previous step */
      stringbuf *buf = stringbuf_new();
      list **nextptr = &((*lptr)->next);
      node *textnode;

      stringbuf_format(buf,"%s",v->value.s);

      while (*nextptr && (ITEM_ATOMIC == ((value*)((*nextptr)->data))->st->item->kind)) {
        value *v2 = (value*)((*nextptr)->data);
        list *del = *nextptr;
        stringbuf_format(buf," %s",v2->value.s);
        value_deref(v2);
        *nextptr = (*nextptr)->next;
        free(del);
      }

      value_deref(v);

      textnode = node_new(NODE_TEXT);
      textnode->value = strdup(buf->data);
      (*lptr)->data = value_new_node(textnode);

      stringbuf_free(buf);
    }
    lptr = &((*lptr)->next);
  }

  /* 4. Any document node within the result sequence is replaced by a sequence containing each of
     its children, in document order. */
  lptr = &values;
  while (*lptr) {
    value *v = (value*)((*lptr)->data);
    if (ITEM_DOCUMENT == v->st->item->kind) {
      list *del = *lptr;
      node *doc = v->value.n;
      node *c;

      *lptr = (*lptr)->next;
      free(del);

      for (c = doc->first_child; c; c = c->next) {
        value *newv = value_new_node(c);
        list_push(lptr,newv);
        lptr = &((*lptr)->next);
      }

      value_deref(v);
    }
    else {
      lptr = &((*lptr)->next);
    }
  }

  /* 5. Zero-length text nodes within the result sequence are removed. */
  remove_zero_length_text_nodes(&values);

  /* 6. Adjacent text nodes within the result sequence are merged into a single text node. */
  merge_adjacent_text_nodes(&values);

  /* 7. Invalid namespace and attribute nodes are detected as follows. */

  for (l = values; l; l = l->next) {
    value *v = (value*)l->data;
    if ((ITEM_NAMESPACE == v->st->item->kind) || (ITEM_ATTRIBUTE == v->st->item->kind)) {

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
      if (ITEM_NAMESPACE == v->st->item->kind) {
        list *l2;
        for (l2 = values; l2 != l; l2 = l2->next) {
          value *v2 = (value*)l2->data;
          if ((ITEM_NAMESPACE == v2->st->item->kind) &&
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
          (ITEM_NAMESPACE == v->st->item->kind) && (NULL == v->value.n->prefix)) {
        error(ei,sloc.uri,sloc.line,"XTDE0440","Sequence contains a namespace node "
              "with no name and the element node being constructed has a null namespace URI");
        err = 1;
      }

      if (err) {
        value_deref_list(values);
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
    value *v = (value*)((*lptr)->data);
    int removed = 0;
    if (ITEM_NAMESPACE == v->st->item->kind) {
      list *l2;
      for (l2 = values; l2 != *lptr; l2 = l2->next) {
        value *v2 = (value*)l2->data;
        if ((ITEM_NAMESPACE == v2->st->item->kind) &&
            !strcmp(v->value.n->prefix,v2->value.n->prefix)) {
          list *del = *lptr;
          *lptr = (*lptr)->next;
          value_deref(v);
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
    value *v = (value*)((*lptr)->data);
    int removed = 0;

    if (ITEM_ATTRIBUTE == v->st->item->kind) {
      list *l2;
      for (l2 = (*lptr)->next; l2; l2 = l2->next) {
        value *v2 = (value*)l2->data;
        if ((ITEM_ATTRIBUTE == v2->st->item->kind) &&
            nsname_equals(v->value.n->ident,v2->value.n->ident)) {
          list *del = *lptr;
          *lptr = (*lptr)->next;
          value_deref(v);
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
    value *v = (value*)l->data;
    /* FIXME: avoid copying here where possible */
    assert(ITEM_ATOMIC != v->st->item->kind);

    if (ITEM_ATTRIBUTE == v->st->item->kind)
      node_add_attribute(parent,node_deep_copy(v->value.n));
    else if (ITEM_NAMESPACE == v->st->item->kind)
      node_add_namespace(parent,node_deep_copy(v->value.n));
    else
      node_add_child(parent,node_deep_copy(v->value.n));
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


  value_deref_list(values);
  list_free(values,NULL);

  return 0;
}

static char *df_construct_simple_content(list *values, const char *separator)
{
  /* XSLT 2.0: 5.7.2 Constructing Simple Content */
  stringbuf *buf = stringbuf_new();
  char *str;
  list *l;

  values = list_copy(values,(list_copy_t)value_ref);

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
  remove_zero_length_text_nodes(&values);

  /* 2. Adjacent text nodes in the sequence are merged into a single text node. */
  merge_adjacent_text_nodes(&values);

  /* 3. The sequence is atomized. */
  for (l = values; l; l = l->next) {
    value *v = (value*)l->data;
    value *atom = df_atomize(v);
    value_deref(v);
    l->data = atom;
  }

  /* 4. Every value in the atomized sequence is cast to a string. */
  for (l = values; l; l = l->next) {
    value *v = (value*)l->data;
    char *s = value_as_string(v);
    value *strval = value_new_string(s);
    free(s);
    value_deref(v);
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
    value *v = (value*)l->data;
    stringbuf_format(buf,"%s",v->value.s);
    if (l->next)
      stringbuf_format(buf,"%s",separator);
  }

  /* 6. The string that results from this concatenation forms the string value of the new
     attribute, namespace, comment, processing-instruction, or text node. */
  str = strdup(buf->data);

  stringbuf_free(buf);
  value_deref_list(values);
  list_free(values,NULL);

  return str;
}

static value *element(gxenvironment *env, value **args)
{
  list *values = df_sequence_to_list(args[0]);
  value *elemvalue;
  gxnode *elem = node_new(NODE_ELEMENT);

  elemvalue = value_new_node(elem);

  assert(df_check_derived_atomic_type(args[1],xs_g->string_type));
  elem->ident.name = strdup(args[1]->value.s);

  /* FIXME: namespace (should be received on an input port) - what about prefix? */

  debug("element %p (\"%s\"): %d items in child sequence\n",
        elemvalue,elem->ident.name,list_count(values));
  if (0 != df_construct_complex_content(env->ei,env->sloc,values,elem)) {
    list_free(values,NULL);
    value_deref(elemvalue);
    return NULL;
  }

  list_free(values,NULL);

  return elemvalue;
}

static value *range(gxenvironment *env, value **args)
{
  int min;
  int max;
  list *range = NULL;
  value *result;

  /* FIXME: support empty sequences and conversion of other types when passed in */
  assert(df_check_derived_atomic_type(args[0],xs_g->int_type));
  assert(df_check_derived_atomic_type(args[1],xs_g->int_type));
  min = args[0]->value.i;
  max = args[1]->value.i;

  if (min <= max) {
    int i;
    for (i = max; i >= min; i--)
      list_push(&range,value_new_int(i));
  }

  result = df_list_to_sequence(range);
  value_deref_list(range);
  list_free(range,NULL);

  return result;
}

static value *contains_node(gxenvironment *env, value **args)
{
  list *nodevals = df_sequence_to_list(args[0]);
  node *n = args[1]->value.n;
  int found = 0;
  list *l;
  assert(SEQTYPE_ITEM == args[1]->st->type);
  assert(ITEM_ATOMIC != args[1]->st->item->kind);

  for (l = nodevals; l; l = l->next) {
    value *nv = (value*)l->data;
    assert(SEQTYPE_ITEM == nv->st->type);
    assert(ITEM_ATOMIC != nv->st->item->kind);
    if (nv->value.n == n)
      found = 1;
  }

  list_free(nodevals,NULL);

  return value_new_bool(found);
}

static value *select_root(gxenvironment *env, value **args)
{
  list *values = df_sequence_to_list(args[0]);
  list *output = NULL;
  list *l;
  value *result;

  for (l = values; l; l = l->next) {
    value *v = (value*)l->data;
    node *root;
    /* FIXME: raise a dynamic error here instead of asserting */
    if ((SEQTYPE_ITEM != v->st->type) || (ITEM_ATOMIC == v->st->item->kind)) {
      fprintf(stderr,"ERROR: non-node in selectroot input\n");
      assert(0);
    }
    root = v->value.n;
    while (NULL != root->parent)
      root = root->parent;
    list_append(&output,value_new_node(root));
  }

  list_free(values,NULL);
  result = df_list_to_sequence(output);
  value_deref_list(output);
  list_free(output,NULL);

  return result;
}

static int nodetest_matches(node *n, char *nametest, seqtype *seqtypetest)
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

static int append_matching_nodes(node *self, char *nametest, seqtype *seqtypetest, int axis,
                                 list **matches)
{
  node *c;
  list *l;
  switch (axis) {
  case AXIS_CHILD:
    for (c = self->first_child; c; c = c->next)
      if (nodetest_matches(c,nametest,seqtypetest))
        list_append(matches,value_new_node(c));
    break;
  case AXIS_DESCENDANT:
    for (c = self->first_child; c; c = c->next)
      CHECK_CALL(append_matching_nodes(c,nametest,seqtypetest,AXIS_DESCENDANT_OR_SELF,matches))
    break;
  case AXIS_ATTRIBUTE:
    for (l = self->attributes; l; l = l->next) {
      node *attr = (node*)l->data;
      if (nodetest_matches(attr,nametest,seqtypetest)) {
        value *val = value_new_node(attr);
        list_append(matches,val);
        debugl("adding matching attribute %p; root %p refcount is now %d",
              val->value.n,node_root(val->value.n),node_root(val->value.n)->refcount);
      }
    }
    break;
  case AXIS_SELF:
    if (nodetest_matches(self,nametest,seqtypetest))
      list_append(matches,value_new_node(self));
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

static value *select1(gxenvironment *env, value **args)
{
  list *values = df_sequence_to_list(args[0]);
  list *output = NULL;
  list *l;
  value *result;

  for (l = values; l; l = l->next) {
    value *v = (value*)l->data;
    /* FIXME: raise a dynamic error here instead of asserting */
    if ((SEQTYPE_ITEM != v->st->type) || (ITEM_ATOMIC == v->st->item->kind)) {
      fprintf(stderr,"ERROR: non-node in select input\n");
      assert(0);
    }
    if (ITEM_ATOMIC != v->st->item->kind) {
      if (0 != append_matching_nodes(v->value.n,env->instr->nametest,env->instr->seqtypetest,
                                       env->instr->axis,&output)) {
        value_deref_list(output);
        list_free(output,NULL);
        return NULL;
      }
    }
  }

  list_free(values,NULL);
  result = df_list_to_sequence(output);
  value_deref_list(output);
  list_free(output,NULL);

  return result;
}

static value *namespace2(gxenvironment *env, value **args)
{
  /* @implements(xslt20:creating-namespace-nodes-2)
     test { xslt/eval/namespace2.test }
     test { xslt/eval/namespace2.test }
     test { xslt/eval/namespace3.test }
     test { xslt/eval/namespace4.test }
     @end */

  node *nsnode = node_new(NODE_NAMESPACE);
  list *values = df_sequence_to_list(args[0]);
  char *prefix = df_construct_simple_content(values," ");
  list_free(values,NULL);
  if (0 == strlen(prefix)) {
    nsnode->prefix = NULL;
    free(prefix);
  }
  else {
    nsnode->prefix = prefix;
  }
  values = df_sequence_to_list(args[1]);
  nsnode->value = df_construct_simple_content(values," ");
  list_free(values,NULL);

  return value_new_node(nsnode);
}

static value *text(gxenvironment *env, value **args)
{
  node *textnode = node_new(NODE_TEXT);
  textnode->value = strdup(env->instr->str);
  return value_new_node(textnode);
}

static value *value_of(gxenvironment *env, value **args)
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

  node *textnode = node_new(NODE_TEXT);
  char *separator = value_as_string(args[1]);
  list *values = df_sequence_to_list(args[0]);
  textnode->value = df_construct_simple_content(values,separator);
  free(separator);
  list_free(values,NULL);
  return value_new_node(textnode);
}

static value *attribute2(gxenvironment *env, value **args)
{
  node *attr = node_new(NODE_ATTRIBUTE);
  list *values = df_sequence_to_list(args[0]);
  list *namevals = df_sequence_to_list(args[1]);

  /* FIXME: support arbitraty sequence as name - used for attribute value templates in
     xsl:attribute */
  /* FIXME: support namespace */

  assert(df_check_derived_atomic_type(args[1],xs_g->string_type));
  attr->ident.name = df_construct_simple_content(namevals," ");
  attr->value = df_construct_simple_content(values," ");

  list_free(values,NULL);
  list_free(namevals,NULL);

  return value_new_node(attr);
}

static value *document(gxenvironment *env, value **args)
{
  node *docnode = node_new(NODE_DOCUMENT);
  list *values = df_sequence_to_list(args[0]);
  value *docval = value_new_node(docnode);

  if (0 != df_construct_complex_content(env->ei,env->instr->sloc,values,docnode)) {
    value_deref(docval);
    list_free(values,NULL);
    return NULL;
  }

  /* FIXME: i think this is already checked df_construct_complex_content()? */
  if (NULL != docnode->attributes) {
    error(env->ei,"",0,"XTDE0420",
          "Attribute nodes cannot be added directly as children of a document");
    value_deref(docval);
    list_free(values,NULL);
    return NULL;
  }

  list_free(values,NULL);

  return docval;
}

static value *sequence(gxenvironment *env, value **args)
{
  value *pair;
  seqtype *st = seqtype_new(SEQTYPE_SEQUENCE);
  st->left = seqtype_ref(args[0]->st);
  st->right = seqtype_ref(args[1]->st);
  pair = value_new(st);
  seqtype_deref(st);
  pair->value.pair.left = value_ref(args[0]);
  pair->value.pair.right = value_ref(args[1]);
  return pair;
}

static value *output(gxenvironment *env, value **args)
{
  stringbuf *buf = stringbuf_new();
  assert(NULL != env->instr->seroptions);
  if (0 != df_serialize(args[0],buf,env->instr->seroptions,env->ei)) {
    stringbuf_free(buf);
    return NULL;
  }
  printf("%s",buf->data);
  stringbuf_free(buf);
  return df_list_to_sequence(NULL);
}

static value *filter(gxenvironment *env, value **args)
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
      value *v = (value*)vl->data;
      value *m = (value*)ml->data;

      vl = vl->next;
      ml = ml->next;
    }
*/
  return NULL;
}

static value *empty(gxenvironment *env, value **args)
{
  /* FIXME: this should take 0 parameters (only context)... remember to update all code that
     uses this function */
  seqtype *st = seqtype_new(SEQTYPE_EMPTY);
  value *result = value_new(st);
  seqtype_deref(st);
  return result;
}

value *ebv(gxenvironment *env, value **args)
{
  /* FIXME: need to complete/test this */

  /* 1. If its operand is an empty sequence, fn:boolean returns false. */
  if (SEQTYPE_EMPTY == args[0]->st->type)
    return value_new_bool(0);

  if (SEQTYPE_SEQUENCE != args[0]->st->type) {
    /* 3. If its operand is a singleton value of type xs:boolean or derived from xs:boolean,
       fn:boolean returns the value of its operand unchanged. */
    if (xs_type_is_derived(args[0]->st->item->type,xs_g->boolean_type))
      return value_ref(args[0]);

    /* 4. If its operand is a singleton value of type xs:string, xdt:untypedAtomic, or a type
       derived from one of these, fn:boolean returns false if the operand value has zero length;
       otherwise it returns true. */
    if (xs_type_is_derived(args[0]->st->item->type,xs_g->string_type) ||
        xs_type_is_derived(args[0]->st->item->type,xs_g->untyped_atomic))
      return value_new_bool(0 < strlen(asstring(args[0])));

    /* 5. If its operand is a singleton value of any numeric type or derived from a numeric type,
       fn:boolean returns false if the operand value is NaN or is numerically equal to zero;
       otherwise it returns true. */
    if (xs_type_is_derived(args[0]->st->item->type,xs_g->float_type))
      return value_new_bool((0.0 != asfloat(args[0])) || isnan(asfloat(args[0])));

    if (xs_type_is_derived(args[0]->st->item->type,xs_g->double_type))
      return value_new_bool((0.0 != asdouble(args[0])) || isnan(asdouble(args[0])));

    if (xs_type_is_derived(args[0]->st->item->type,xs_g->decimal_type))
      return value_new_bool((0.0 != asint(args[0])) || isnan(asint(args[0])));

    /* 2. If its operand is a sequence whose first item is a node, fn:boolean returns true. */
    if (ITEM_ATOMIC != args[0]->st->item->kind)
      return value_new_bool(1);
  }
  else {
    value **values = df_sequence_to_array(args[0]);
    if (ITEM_ATOMIC != values[0]->st->item->kind) {
      free(values);
      return value_new_bool(1);
    }
    free(values);
  }

  /* 6. In all other cases, fn:boolean raises a type error [err:FORG0006]. */

  error(env->ei,env->sloc.uri,env->sloc.line,"FORG0006",
        "Type error: cannot convert value to a boolean");
  return NULL;
}

static value *and2(gxenvironment *env, value **args)
{
  value *v1;
  value *v2;
  int r;
  if (NULL == (v1 = ebv(env,&args[0])))
    return NULL;
  if (NULL == (v2 = ebv(env,&args[1]))) {
    value_deref(v1);
    return NULL;
  }
  r = (asbool(v1) && asbool(v2));
  value_deref(v1);
  value_deref(v2);

  return value_new_bool(r);
}

static value *or2(gxenvironment *env, value **args)
{
  value *v1;
  value *v2;
  int r;
  if (NULL == (v1 = ebv(env,&args[0])))
    return NULL;
  if (NULL == (v2 = ebv(env,&args[1]))) {
    value_deref(v1);
    return NULL;
  }
  r = (asbool(v1) || asbool(v2));
  value_deref(v1);
  value_deref(v2);

  return value_new_bool(r);
}

gxfunctiondef special_fundefs[18] = {
  { element,       FNS, "element",       "item()*,xsd:string",        "element()"       },
  { range,         FNS, "range",         "xsd:integer?,xsd:integer?", "xsd:integer*"    },
  { contains_node, FNS, "contains-node", "node()*,node()",            "xsd:boolean"     },
  { select_root,   FNS, "select-root",   "node()",                    "node()"          },
  { select1,       FNS, "select",        "node()*",                   "node()*"         },
  { namespace2,    FNS, "namespace",     "item()*,item()*",           "node()"          },
  { text,          FNS, "text",          "item()*",                   "text()"          },
  { value_of,      FNS, "value-of",      "item()*,item()*",           "text()"          },
  { attribute2,    FNS, "attribute",     "item()*,item()*",           "attribute()"     },
  { document,      FNS, "document",      "item()*",                   "document-node()" },
  { sequence,      FNS, "sequence",      "item()*,item()*",           "item()*"         },
  { output,        FNS, "output",        "item()*",                   "item()*"         },
  { filter,        FNS, "filter",        "item()*",                   "item()*"         },
  { empty,         FNS, "empty",         "item()*",                   "item()*"         },
  { ebv,           FNS, "ebv",           "item()*",                   "xsd:boolean"     },
  { and2,          FNS, "and",           "item()*,item()*",           "xsd:boolean"     },
  { or2,           FNS, "or",            "item()*,item()*",           "xsd:boolean"     },
  { NULL },
};

gxfunctiondef *special_module = special_fundefs;
