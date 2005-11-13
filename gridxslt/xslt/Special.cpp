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

#include "dataflow/SequenceType.h"
#include "dataflow/Program.h"
#include "dataflow/Engine.h"
#include "util/XMLUtils.h"
#include "util/Debug.h"
#include "util/Macros.h"
#include "Expression.h"
#include "Special.h"
#include <string.h>
#include <math.h>

using namespace GridXSLT;

#define FNS SPECIAL_NAMESPACE

static void remove_zero_length_text_nodes(List<Value> &values)
{
  Iterator<Value> it = values;
  while (it.haveCurrent()) {
    Value v = *it;
    if ((ITEM_TEXT == v.type().itemType()->m_kind) && (0 == v.asNode()->m_value.length()))
      it.remove();
    else
      it++;
  }
}

static void merge_adjacent_text_nodes(List<Value> &values)
{
  Iterator<Value> it = values;
  while (it.haveCurrent()) {
    Value v = *it;

    if (ITEM_TEXT == v.type().itemType()->m_kind) {
      StringBuffer buf;
      Iterator<Value> next = it;
      next++;
      Node *textnode;

      buf.append(v.asNode()->m_value);

      while (next.haveCurrent() && (ITEM_TEXT == (*next).type().itemType()->m_kind)) {
        Value v2 = *next;
        buf.append(v2.asNode()->m_value);
        next.remove();
      }

      textnode = new Node(NODE_TEXT);
      textnode->m_value = buf.contents();
      it.replace(Value(textnode));
    }

    it++;
  }
}

static int df_construct_complex_content(Error *ei, sourceloc sloc,
                                        List<Value> &values, Node *parent)
{
  Iterator<Value> it;
  int havenotnsattr = 0;

  values = values.copy();

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
  for (it = values; it.haveCurrent(); it++) {
    Value v = *it;
    ASSERT(SEQTYPE_ITEM == v.type().type());
    if (ITEM_ATOMIC == v.type().itemType()->m_kind)
      it.replace(Value(v.convertToString()));
  }

  /* 3. Any consecutive sequence of strings within the result sequence is converted to a single
     text node, whose string value contains the content of each of the strings in turn, with a
     single space (#x20) used as a separator between successive strings. */
  it = values;
  while (it.haveCurrent()) {
    Value v = *it;
    if (ITEM_ATOMIC == v.type().itemType()->m_kind) {
      /* Must be a string; see previous step */
      StringBuffer buf;
      Iterator<Value> next = it;
      next++;
      Node *textnode;

      buf.append(v.asString());

      while (next.haveCurrent() && (ITEM_ATOMIC == (*next).type().itemType()->m_kind)) {
        Value v2 = *next;
        buf.append(" ");
        buf.append(v2.asString());
        next.remove();
      }

      textnode = new Node(NODE_TEXT);
      textnode->m_value = buf.contents();
      it.replace(Value(textnode));
    }
    it++;
  }

  /* 4. Any document node within the result sequence is replaced by a sequence containing each of
     its children, in document order. */

  it = values;
  while (it.haveCurrent()) {
    Value v = *it;
    if (ITEM_DOCUMENT == v.type().itemType()->m_kind) {
      it.remove();

      Node *doc = v.asNode();
      Node *c;

      for (c = doc->m_first_child; c; c = c->m_next) {
        it.insert(Value(c));
        it++;
      }
    }
    else {
      it++;
    }
  }

  /* 5. Zero-length text nodes within the result sequence are removed. */
  remove_zero_length_text_nodes(values);

  /* 6. Adjacent text nodes within the result sequence are merged into a single text node. */
  merge_adjacent_text_nodes(values);

  /* 7. Invalid namespace and attribute nodes are detected as follows. */

  for (it = values; it.haveCurrent(); it++) {
    Value v = *it;
    if ((ITEM_NAMESPACE == v.type().itemType()->m_kind) ||
        (ITEM_ATTRIBUTE == v.type().itemType()->m_kind)) {

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
      if (NODE_DOCUMENT == parent->m_type) {
        /* FIXME: write test for this; requires support for xsl:document */
        error(ei,sloc.uri,sloc.line,"XTDE0420","A sequence used to construct the "
              "content of a document node cannot contain namespace or attribute nodes");
        err = 1;
      }

      /* [ERR XTDE0430] It is a non-recoverable dynamic error if the result sequence contains two
         or more namespace nodes having the same name but different string values (that is,
         namespace nodes that map the same prefix to different namespace URIs). */
      if (ITEM_NAMESPACE == v.type().itemType()->m_kind) {
        Iterator<Value> l2;
        for (l2 = values; l2.m_current != it.m_current; l2++) {
          Value v2 = *l2;
          if ((ITEM_NAMESPACE == v2.type().itemType()->m_kind) &&
              (v.asNode()->m_prefix == v2.asNode()->m_prefix) &&
              (v.asNode()->m_value != v2.asNode()->m_value)) {
            error(ei,sloc.uri,sloc.line,"XTDE0430","Sequence contains two namespace "
                  "nodes trying to map the prefix \"%*\" to different URIs (\"%*\" and \"%*\")",
                  &v.asNode()->m_prefix,&v.asNode()->m_value,&v2.asNode()->m_value);
            err = 1;
            break;
          }
        }
      }

      /* [ERR XTDE0440] It is a non-recoverable dynamic error if the result sequence contains a
         namespace node with no name and the element node being constructed has a null namespace
         URI (that is, it is an error to define a default namespace when the element is in no
         namespace). */
      if ((NODE_ELEMENT == parent->m_type) && (parent->m_ident.m_ns.isNull()) &&
          (ITEM_NAMESPACE == v.type().itemType()->m_kind) && (v.asNode()->m_prefix.isNull())) {
        error(ei,sloc.uri,sloc.line,"XTDE0440","Sequence contains a namespace node "
              "with no name and the element node being constructed has a null namespace URI");
        err = 1;
      }

      if (err) {
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
  it = values;
  while (it.haveCurrent()) {
    Value v = *it;
    int removed = 0;
    if (ITEM_NAMESPACE == v.type().itemType()->m_kind) {
      Iterator<Value> l2;
      for (l2 = values; l2.m_current != it.m_current; l2++) {
        Value v2 = *l2;
        if ((ITEM_NAMESPACE == v2.type().itemType()->m_kind) &&
            (v.asNode()->m_prefix == v2.asNode()->m_prefix)) {
          it.remove();
          removed = 1;
          break;
        }
      }
    }
    if (!removed)
      it++;
  }

  /* 9. If an attribute A in the result sequence has the same name as another attribute B that
        appears later in the result sequence, then attribute A is discarded from the result
        sequence. */
  it = values;
  while (it.haveCurrent()) {
    Value v = *it;
    int removed = 0;

    if (ITEM_ATTRIBUTE == v.type().itemType()->m_kind) {
      Iterator<Value> l2 = it;
      l2++;
      for (; l2.haveCurrent(); l2++) {
        Value v2 = *l2;
        if ((ITEM_ATTRIBUTE == v2.type().itemType()->m_kind) &&
            (v.asNode()->m_ident == v2.asNode()->m_ident)) {
          it.remove();
          removed = 1;
          break;
        }
      }
    }

    if (!removed)
      it++;
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

  for (it = values; it.haveCurrent(); it++) {
    Value v = *it;
    /* FIXME: avoid copying here where possible */
    ASSERT(ITEM_ATOMIC != v.type().itemType()->m_kind);

    if (ITEM_ATTRIBUTE == v.type().itemType()->m_kind)
      parent->addAttribute(v.asNode()->deepCopy());
    else if (ITEM_NAMESPACE == v.type().itemType()->m_kind)
      parent->addNamespace(v.asNode()->deepCopy());
    else
      parent->addChild(v.asNode()->deepCopy());
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

  return 0;
}

static String df_construct_simple_content(List<Value> &values, const String &separator)
{
  /* XSLT 2.0: 5.7.2 Constructing Simple Content */
  StringBuffer buf;
  Iterator<Value> it;

  values = values.copy();

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
  remove_zero_length_text_nodes(values);

  /* 2. Adjacent text nodes in the sequence are merged into a single text node. */
  merge_adjacent_text_nodes(values);

  /* 3. The sequence is atomized. */
  for (it = values; it.haveCurrent(); it++) {
    Value v = *it;
    Value atom = v.atomize();
    it.replace(atom);
  }

  /* 4. Every value in the atomized sequence is cast to a string. */
  for (it = values; it.haveCurrent(); it++) {
    Value v = *it;
    it.replace(Value(v.convertToString()));
  }

  /* 5. The strings within the resulting sequence are concatenated, with a (possibly zero-length)
     separator inserted between successive strings. The default separator is a single space. In the
     case of xsl:attribute and xsl:value-of, a different separator can be specified using the
     separator attribute of the instruction; it is permissible for this to be a zero-length string,
     in which case the strings are concatenated with no separator. In the case of xsl:comment,
     xsl:processing-instruction, and xsl:namespace, and when expanding an attribute value template,
     the default separator cannot be changed. */
  for (it = values; it.haveCurrent(); it++) {
    Value v = *it;
    buf.append(v.asString());
    if (it.haveNext())
      buf.append(separator);
  }

  /* 6. The string that results from this concatenation forms the string value of the new
     attribute, namespace, comment, processing-instruction, or text node. */
  return buf.contents();
}

static Value element(Environment *env, List<Value> &args)
{
  List<Value> values = args[0].sequenceToList();
  Value elemvalue;
  Node *elem = new Node(NODE_ELEMENT);

  elemvalue = Value(elem);

  ASSERT(args[1].isDerivedFrom(xs_g->string_type));
  elem->m_ident.m_name = args[1].asString();

  /* FIXME: namespace (should be received on an input port) - what about prefix? */

  if (0 != df_construct_complex_content(env->ei,env->sloc,values,elem))
    return Value::null();

  return elemvalue;
}

static Value range(Environment *env, List<Value> &args)
{
  int min;
  int max;
  List<Value> range;

  /* FIXME: support empty sequences */
  /* FIXME: support conversion of other types when passed in (this should be done automatically
     by the compiler by inserting the appropriate conversion operators) */


/*   Value v0 = args[0]; */
/*   Value v1 = args[1]; */
/*   message("args[0] = %*\n",&v0); */
/*   message("args[1] = %*\n",&v1); */

/*   SequenceType st0 = args[0].type(); */
/*   SequenceType st1 = args[1].type(); */

/*   message("args[0].type() = %*\n",&st0); */
/*   message("args[1].type() = %*\n",&st1); */

  ASSERT(args[0].isDerivedFrom(xs_g->integer_type));
  ASSERT(args[1].isDerivedFrom(xs_g->integer_type));
  min = args[0].asInteger();
  max = args[1].asInteger();

  if (min <= max) {
    int i;
    for (i = max; i >= min; i--)
      range.push(Value(i));
  }

  return Value::listToSequence2(range);
}

static Value contains_node(Environment *env, List<Value> &args)
{
  List<Value> nodevals = args[0].sequenceToList();
  Node *n = args[1].asNode();
  bool found = false;
  ASSERT(SEQTYPE_ITEM == args[1].type().type());
  ASSERT(ITEM_ATOMIC != args[1].type().itemType()->m_kind);

  for (Iterator<Value> it = nodevals; it.haveCurrent(); it++) {
    Value nv = *it;
    ASSERT(SEQTYPE_ITEM == nv.type().type());
    ASSERT(ITEM_ATOMIC != nv.type().itemType()->m_kind);
    if (nv.asNode() == n)
      found = true;
  }

  return Value(found);
}

static Value select_root(Environment *env, List<Value> &args)
{
  List<Value> values = args[0].sequenceToList();
  List<Value> output;

  for (Iterator<Value> it = values; it.haveCurrent(); it++) {
    Value v = *it;
    Node *root;
    /* FIXME: raise a dynamic error here instead of ASSERTing */
    if ((SEQTYPE_ITEM != v.type().type()) || (ITEM_ATOMIC == v.type().itemType()->m_kind)) {
      fmessage(stderr,"ERROR: non-node in selectroot input\n");
      ASSERT(0);
    }
    root = v.asNode();
    while (NULL != root->m_parent)
      root = root->m_parent;
    output.append(Value(root));
  }

  return Value::listToSequence2(output);
}

static int nodetest_matches(Node *n, const String &nametest, SequenceType &seqtypetest)
{
  if (!nametest.isNull()) {
    return (((NODE_ELEMENT == n->m_type) || (NODE_ATTRIBUTE == n->m_type)) &&
            (n->m_ident.m_name == nametest));
  }
  else {
    ASSERT(!seqtypetest.isNull());
    if (SEQTYPE_ITEM == seqtypetest.type()) {
      switch (seqtypetest.itemType()->m_kind) {
      case ITEM_ATOMIC:
        /* FIXME (does this even apply here?) */
        break;
      case ITEM_DOCUMENT:
        /* FIXME: support checks based on document type, e.g. document(element(foo)) */
        return (n->m_type == NODE_DOCUMENT);
        break;
      case ITEM_ELEMENT:
        if (NULL != seqtypetest.itemType()->m_elem) {
          /* FIXME: support checks based on type annotation of node, e.g. element(*,xs:string) */
          return ((n->m_type == NODE_ELEMENT) &&
                  (n->m_ident == seqtypetest.itemType()->m_elem->def.ident));
        }
        else {
          return (n->m_type == NODE_ELEMENT);
        }
        break;
      case ITEM_ATTRIBUTE:
        /* FIXME */
        break;
      case ITEM_PI:
        return (n->m_type == NODE_PI);
        break;
      case ITEM_COMMENT:
        return (n->m_type == NODE_COMMENT);
        break;
      case ITEM_TEXT:
        return (n->m_type == NODE_TEXT);
        break;
      case ITEM_NAMESPACE:
        return (n->m_type == NODE_NAMESPACE);
        break;
      default:
        ASSERT(!"invalid item type");
        break;
      }
    }
    else if (SEQTYPE_CHOICE == seqtypetest.type()) {
      SequenceType left = seqtypetest.left();
      SequenceType right = seqtypetest.right();
      return (nodetest_matches(n,String::null(),left) || nodetest_matches(n,String::null(),right));
    }
  }

  return 0;
}

static int append_matching_nodes(Node *self, const String &nametest,
                                 SequenceType &seqtypetest, int axis, List<Value> &matches)
{
  Node *c;
  switch (axis) {
  case AXIS_CHILD:
    for (c = self->m_first_child; c; c = c->m_next)
      if (nodetest_matches(c,nametest,seqtypetest))
        matches.append(Value(c));
    break;
  case AXIS_DESCENDANT:
    for (c = self->m_first_child; c; c = c->m_next)
      CHECK_CALL(append_matching_nodes(c,nametest,seqtypetest,AXIS_DESCENDANT_OR_SELF,matches))
    break;
  case AXIS_ATTRIBUTE: {
    for (Iterator<Node*> it = self->m_attributes; it.haveCurrent(); it++) {
      Node *attr = *it;
      if (nodetest_matches(attr,nametest,seqtypetest))
        matches.append(Value(attr));
    }
    break;
  }
  case AXIS_SELF:
    if (nodetest_matches(self,nametest,seqtypetest))
      matches.append(Value(self));
    break;
  case AXIS_DESCENDANT_OR_SELF:
    CHECK_CALL(append_matching_nodes(self,nametest,seqtypetest,AXIS_SELF,matches))
    CHECK_CALL(append_matching_nodes(self,nametest,seqtypetest,AXIS_DESCENDANT,matches))
    break;
  case AXIS_FOLLOWING_SIBLING:
    /* FIXME */
    ASSERT(!"not yet implemented");
    break;
  case AXIS_FOLLOWING:
    /* FIXME */
    ASSERT(!"not yet implemented");
    break;
  case AXIS_NAMESPACE:
    /* FIXME */
    ASSERT(!"not yet implemented");
    break;
  case AXIS_PARENT:
    /* FIXME */
    ASSERT(!"not yet implemented");
    break;
  case AXIS_ANCESTOR:
    /* FIXME */
    ASSERT(!"not yet implemented");
    break;
  case AXIS_PRECEDING_SIBLING:
    /* FIXME */
    ASSERT(!"not yet implemented");
    break;
  case AXIS_PRECEDING:
    /* FIXME */
    ASSERT(!"not yet implemented");
    break;
  case AXIS_ANCESTOR_OR_SELF:
    /* FIXME */
    ASSERT(!"not yet implemented");
    break;
  default:
    ASSERT(!"invalid axis");
    break;
  }
  return 0;
}

static Value select1(Environment *env, List<Value> &args)
{
  List<Value> values = args[0].sequenceToList();
  List<Value> output;

  for (Iterator<Value> it = values; it.haveCurrent(); it++) {
    Value v = *it;
    /* FIXME: raise a dynamic error here instead of ASSERTing */
    if ((SEQTYPE_ITEM != v.type().type()) || (ITEM_ATOMIC == v.type().itemType()->m_kind)) {
      fmessage(stderr,"ERROR: non-node in select input\n");
      ASSERT(0);
    }
    if (ITEM_ATOMIC != v.type().itemType()->m_kind) {
      if (0 != append_matching_nodes(v.asNode(),env->instr->m_nametest,
                                     env->instr->m_type,
                                     env->instr->m_axis,output)) {
        return Value::null();
      }
    }
  }

  return Value::listToSequence2(output);
}

static Value namespace2(Environment *env, List<Value> &args)
{
  /* @implements(xslt20:creating-namespace-nodes-2)
     test { xslt/eval/namespace2.test }
     test { xslt/eval/namespace2.test }
     test { xslt/eval/namespace3.test }
     test { xslt/eval/namespace4.test }
     @end */

  Node *nsnode = new Node(NODE_NAMESPACE);
  List<Value> values = args[0].sequenceToList();
  String prefix = df_construct_simple_content(values," ");
  if (0 == prefix.length())
    nsnode->m_prefix = String::null();
  else
    nsnode->m_prefix = prefix;
  values = args[1].sequenceToList();
  nsnode->m_value = df_construct_simple_content(values," ");

  return Value(nsnode);
}

static Value text(Environment *env, List<Value> &args)
{
  Node *textnode = new Node(NODE_TEXT);
  textnode->m_value = env->instr->m_str;
  return Value(textnode);
}

static Value value_of(Environment *env, List<Value> &args)
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

  Node *textnode = new Node(NODE_TEXT);
  String separator = args[1].convertToString();
  List<Value> values = args[0].sequenceToList();
  textnode->m_value = df_construct_simple_content(values,separator);
  return Value(textnode);
}

static Value attribute2(Environment *env, List<Value> &args)
{
  Node *attr = new Node(NODE_ATTRIBUTE);
  List<Value> values = args[0].sequenceToList();
  List<Value> namevals = args[1].sequenceToList();

  /* FIXME: support arbitraty sequence as name - used for attribute value templates in
     xsl:attribute */
  /* FIXME: support namespace */

  ASSERT(args[1].isDerivedFrom(xs_g->string_type));
  attr->m_ident.m_name = df_construct_simple_content(namevals," ");
  attr->m_value = df_construct_simple_content(values," ");

  return Value(attr);
}

static Value document(Environment *env, List<Value> &args)
{
  Node *docnode = new Node(NODE_DOCUMENT);
  List<Value> values = args[0].sequenceToList();
  Value docval = Value(docnode);

  if (0 != df_construct_complex_content(env->ei,env->instr->m_sloc,values,docnode))
    return Value::null();

  /* FIXME: i think this is already checked df_construct_complex_content()? */
  if (!docnode->m_attributes.isEmpty()) {
    error(env->ei,"",0,"XTDE0420",
          "Attribute nodes cannot be added directly as children of a document");
    return Value::null();
  }

  return docval;
}

static Value sequence(Environment *env, List<Value> &args)
{
  if (args[0].type().isEmptyType())
    return args[1];

  if (args[1].type().isEmptyType())
    return args[0];

  SequenceType st = SequenceType::sequence(args[0].type(),args[1].type());
  return Value(st,args[0],args[1]);
}

static Value output(Environment *env, List<Value> &args)
{
  stringbuf *buf = stringbuf_new();
  ASSERT(NULL != env->instr->m_seroptions);
  Value arg0 = args[0];
  if (0 != df_serialize(arg0,buf,env->instr->m_seroptions,env->ei)) {
    stringbuf_free(buf);
    return Value::null();
  }
  message("%s",buf->data);
  stringbuf_free(buf);
  return Value::listToSequence(NULL);
}

static Value filter(Environment *env, List<Value> &args)
{
  List<Value> items = args[0].sequenceToList();
  List<Value> predicates = args[1].sequenceToList();
  List<Value> result;

  ASSERT(items.count() == predicates.count());

  Iterator<Value> iit;
  Iterator<Value> pit;

  int pos = 1;
  for (iit = items, pit = predicates; iit.haveCurrent(); iit++, pit++) {
    Value v = *iit;
    Value p = *pit;
    bool matches = false;

    if (p.isInteger()) {
      matches = (pos == (int)p.asInteger());
    }
    else if (p.isFloat()) {
      matches = (pos == (int)p.asFloat());
    }
    else if (p.isDouble()) {
      matches = (pos == (int)p.asDouble());
    }
    else {
      List<Value> current;
      current.append(p);
      Value b = ebv(env,current);
      if (b.isNull())
        return Value::null();
      matches = b.asBoolean();
    }

    if (matches)
      result.append(v);

    pos++;
  }

//  message("filter: %u input items, %u output items\n",items.count(),result.count());
  return Value::listToSequence2(result);
}

static Value spempty(Environment *env, List<Value> &args)
{
  /* FIXME: this should take 0 parameters (only context)... remember to update all code that
     uses this function */
  return Value::empty();
}

Value ebv(Environment *env, List<Value> &args)
{
  /* FIXME: need to complete/test this */

  /* 1. If its operand is an empty sequence, fn:boolean returns false. */
  if (SEQTYPE_EMPTY == args[0].type().type())
    return Value(false);

  if (SEQTYPE_SEQUENCE != args[0].type().type()) {
    /* 3. If its operand is a singleton value of type xs:boolean or derived from xs:boolean,
       fn:boolean returns the value of its operand unchanged. */
    if (args[0].type().itemType()->m_type->isDerived(xs_g->boolean_type))
      return args[0];

    /* 4. If its operand is a singleton value of type xs:string, xdt:untypedAtomic, or a type
       derived from one of these, fn:boolean returns false if the operand value has zero length;
       otherwise it returns true. */
    if (args[0].type().itemType()->m_type->isDerived(xs_g->string_type) ||
        args[0].type().itemType()->m_type->isDerived(xs_g->untyped_atomic))
      return Value(0 < args[0].asString().length());

    /* 5. If its operand is a singleton value of any numeric type or derived from a numeric type,
       fn:boolean returns false if the operand value is NaN or is numerically equal to zero;
       otherwise it returns true. */
    if (args[0].isFloat())
      return Value((0.0 != args[0].asFloat()) || isnan(args[0].asFloat()));

    if (args[0].isDouble())
      return Value((0.0 != args[0].asDouble()) || isnan(args[0].asDouble()));

    if (args[0].isInteger())
      return Value((0.0 != args[0].asInteger()) || isnan(args[0].asInteger()));

    /* 2. If its operand is a sequence whose first item is a node, fn:boolean returns true. */
    if (ITEM_ATOMIC != args[0].type().itemType()->m_kind)
      return Value(true);
  }
  else {
    List<Value> values = args[0].sequenceToList();
    if (ITEM_ATOMIC != values[0].type().itemType()->m_kind)
      return Value(false);
  }

  /* 6. In all other cases, fn:boolean raises a type error [err:FORG0006]. */

  error(env->ei,env->sloc.uri,env->sloc.line,"FORG0006",
        "Type error: cannot convert value to a boolean");
  return Value::null();
}

static Value and2(Environment *env, List<Value> &args)
{
  Value v1;
  Value v2;

  List<Value> arg0;
  arg0.append(args[0]);
  v1 = ebv(env,arg0);
  if (v1.isNull())
    return Value::null();

  List<Value> arg1;
  arg1.append(args[1]);
  v2 = ebv(env,arg1);
  if (v2.isNull()) {
    return Value::null();
  }
  return Value(v1.asBoolean() && v2.asBoolean());
}

static Value or2(Environment *env, List<Value> &args)
{
  Value v1;
  Value v2;

  List<Value> arg0;
  arg0.append(args[0]);
  v1 = ebv(env,arg0);
  if (v1.isNull())
    return Value::null();

  List<Value> arg1;
  arg1.append(args[1]);
  v2 = ebv(env,arg1);
  if (v2.isNull()) {
    return Value::null();
  }
  return Value(v1.asBoolean() || v2.asBoolean());
}

static Value dot(Environment *env, List<Value> &args)
{
  Context *c = args[0].asContext();

  if (!c->havefocus) {
    error(env->ei,env->sloc.uri,env->sloc.line,"FONC0001","No context item");
    return Value::null();
  }

  return c->item;
}

static Value instanceof(Environment *env, List<Value> &args)
{
  SequenceType st = args[0].type();
  if (args[0].type().isDerivedFrom(env->instr->m_type))
    return Value(true);
  else
    return Value(false);
}

static Value promote_float(Environment *env, List<Value> &args)
{
  return Value::null();
}

static Value promote_double(Environment *env, List<Value> &args)
{
  if (args[0].isDouble())
    return args[0];

  if (args[0].isFloat())
    return Value(double(args[0].asFloat()));

  // FIXME
  return Value::null();
}

static Value promote_string(Environment *env, List<Value> &args)
{
  // FIXME
  return Value::null();
}

static Value cast(Environment *env, List<Value> &args)
{
  ASSERT(0);


  Value oldv = args[0];
  SequenceType st = oldv.type();

  message("cast: from value %* (type %*) to type %*\n",&oldv,&st,&env->instr->m_type);


  // F&O 17.3: Casting from derived types to parent types
  if (st.isDerivedFrom(env->instr->m_type)) {
    // FIXME: this is only really appropriate for derivation by restriction - is it possible
    // for isDerivedFrom() to return true for other types of derivation?
    Value newv = Value(env->instr->m_type);
    if (newv.isContext()) {
      newv.handle()->value.c = new Context(*oldv.handle()->value.c);
    }
    else if (newv.isInteger())
      newv.handle()->value.i = oldv.handle()->value.i;
    else if (newv.isFloat())
      newv.handle()->value.f = oldv.handle()->value.f;
    else if (newv.isDouble())
      newv.handle()->value.d = oldv.handle()->value.d;
    else if (newv.isDecimal()) {
      if (oldv.isInteger())
        newv.handle()->value.d = double(oldv.handle()->value.i);
      else
        newv.handle()->value.d = oldv.handle()->value.d;
    }
    else if (newv.isString())
      newv.handle()->value.s = oldv.handle()->value.s->ref();
    else if (newv.isBoolean())
      newv.handle()->value.b = oldv.handle()->value.b;
    else if (newv.isNode())
      newv.handle()->value.n = oldv.handle()->value.n->ref();
    else if (newv.isUntypedAtomic())
      newv.handle()->value.s = oldv.handle()->value.s->ref();
    else if (newv.isBase64Binary())
      newv.handle()->value.a = oldv.handle()->value.a->ref();
    else if (newv.isHexBinary())
      newv.handle()->value.a = oldv.handle()->value.a->ref();
    else if (newv.isAnyURI())
      newv.handle()->value.u = new URI(*oldv.handle()->value.u);
    else if (newv.isQName())
      newv.handle()->value.q = new NSName(*oldv.handle()->value.q);
    else
      ASSERT(0);
    return newv;
  }

  ASSERT(0);
  // FIXME
  return Value::null();
}

FunctionDefinition special_fundefs[24] = {
  { element,       FNS, "element",       "item()*,xsd:string",        "element()", false       },
  { range,         FNS, "range",         "xsd:integer?,xsd:integer?", "xsd:integer*", false    },
  { contains_node, FNS, "contains-node", "node()*,node()",            "xsd:boolean", false     },
  { select_root,   FNS, "select-root",   "node()",                    "node()", false          },
  { select1,       FNS, "select",        "node()*",                   "node()*", false         },
  { namespace2,    FNS, "namespace",     "item()*,item()*",           "node()", false          },
  { text,          FNS, "text",          "item()*",                   "text()", false          },
  { value_of,      FNS, "value-of",      "item()*,item()*",           "text()", false          },
  { attribute2,    FNS, "attribute",     "item()*,item()*",           "attribute()", false     },
  { document,      FNS, "document",      "item()*",                   "document-node()", false },
  { sequence,      FNS, "sequence",      "item()*,item()*",           "item()*", false         },
  { output,        FNS, "output",        "item()*",                   "item()*", false         },
  { filter,        FNS, "filter",        "item()*,item()*",           "item()*", false         },
  { spempty,       FNS, "empty",         "item()*",                   "item()*", false         },
  { ebv,           FNS, "ebv",           "item()*",                   "xsd:boolean", false     },
  { and2,          FNS, "and",           "item()*,item()*",           "xsd:boolean", false     },
  { or2,           FNS, "or",            "item()*,item()*",           "xsd:boolean", false     },
  { dot,           FNS, "dot",           "",                          "item()", true           },
  { instanceof,    FNS, "instanceof",    "item()*",                   "xsd:boolean", false     },
  { promote_float,   FNS, "promote-float",  "item()",                    "xsd:float", false    },
  { promote_double,  FNS, "promote-double", "item()",                    "xsd:double", false   },
  { promote_string,  FNS, "promote-string", "item()",                    "xsd:string", false   },
  { cast,          FNS, "cast",          "item()",                    "item()", false          },
  { NULL },
};

FunctionDefinition *special_module = special_fundefs;
