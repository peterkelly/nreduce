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

#ifndef _DATAFLOW_SEQUENCETYPE2_H
#define _DATAFLOW_SEQUENCETYPE2_H

#include "xmlschema/XMLSchema.h"
#include "util/String.h"
#include "util/BinaryArray.h"
#include "util/List.h"
#include "util/Namespace.h"
#include "util/XMLUtils.h"
#include "util/Debug.h"
#include <libxml/xmlwriter.h>
#include <libxml/tree.h>

#define ITEM_INVALID                  0
#define ITEM_ATOMIC                   1
#define ITEM_DOCUMENT                 2
#define ITEM_ELEMENT                  3
#define ITEM_ATTRIBUTE                4
#define ITEM_PI                       5
#define ITEM_COMMENT                  6
#define ITEM_TEXT                     7
#define ITEM_NAMESPACE                8
#define ITEM_COUNT                    9

#define SEQTYPE_INVALID               0
#define SEQTYPE_ITEM                  1
#define SEQTYPE_OCCURRENCE            2
#define SEQTYPE_ALL                   3
#define SEQTYPE_SEQUENCE              4
#define SEQTYPE_CHOICE                5
#define SEQTYPE_EMPTY                 6

#define OCCURS_ONCE                   0
#define OCCURS_OPTIONAL               1
#define OCCURS_ZERO_OR_MORE           2
#define OCCURS_ONE_OR_MORE            3

#define NODE_INVALID                  0
#define NODE_DOCUMENT                 ITEM_DOCUMENT
#define NODE_ELEMENT                  ITEM_ELEMENT
#define NODE_ATTRIBUTE                ITEM_ATTRIBUTE
#define NODE_PI                       ITEM_PI
#define NODE_COMMENT                  ITEM_COMMENT
#define NODE_TEXT                     ITEM_TEXT
#define NODE_NAMESPACE                ITEM_NAMESPACE

namespace GridXSLT {

class SequenceTypeImpl;
class ValueImpl;
class Value;
class Context;

class ItemType {
  DISABLE_COPY(ItemType)
public:
  ItemType(int kind);
  ~ItemType();

  void printFS(StringBuffer &buf, NamespaceMap *namespaces);
  void printXPath(StringBuffer &buf, NamespaceMap *namespaces);

  int m_kind;
  Type *m_type;
  SchemaElement *m_elem;
  SchemaAttribute *m_attr;
  QName m_typeref;
  QName m_elemref;
  QName m_attrref;
  QName m_localname;
  String m_pistr;
  int m_nillable;
  SequenceTypeImpl *m_content;
};

class SequenceTypeImpl : public Shared<SequenceTypeImpl> {
  DISABLE_COPY(SequenceTypeImpl)
  friend class SequenceType;
public:
  SequenceTypeImpl(int type);
  SequenceTypeImpl(ItemType *item);
  SequenceTypeImpl(Type *type);
  ~SequenceTypeImpl();

  static SequenceTypeImpl *item();
  static SequenceTypeImpl *node();
  static SequenceTypeImpl *itemstar();
  static SequenceTypeImpl *nodestar();
  static SequenceTypeImpl *interleave(SequenceTypeImpl *content);

  static SequenceTypeImpl *all(SequenceTypeImpl *left, SequenceTypeImpl *right);
  static SequenceTypeImpl *choice(SequenceTypeImpl *left, SequenceTypeImpl *right);
  static SequenceTypeImpl *sequence(SequenceTypeImpl *left, SequenceTypeImpl *right);
  static SequenceTypeImpl *occurrence(SequenceTypeImpl *left, int occ);

  void init();

  void printFS(StringBuffer &buf, NamespaceMap *namespaces);
  void printXPath(StringBuffer &buf, NamespaceMap *namespaces);
  int resolve(NamespaceMap *namespaces, Schema *s, const String &filename,
              int line, Error *ei);
  void toList(list **types);
  int isDerivedFrom(SequenceTypeImpl *base);

  inline int type() const { return m_type; }
  inline SequenceTypeImpl *left() const { return m_left; }
  inline SequenceTypeImpl *right() const { return m_right; }
  inline ItemType *itemType() const { return m_item; }
  inline int occurs() const { return m_occurrence; }
  inline bool isNode() const { return m_isNode; }
  inline bool isItem() const { return m_isItem; }
  inline int valtype() const { return m_valtype; }

  inline bool isItemType() const { return (SEQTYPE_ITEM == m_valtype); }
  inline bool isSequenceType() const { return (SEQTYPE_SEQUENCE == m_valtype); }
  inline bool isEmptyType() const { return (SEQTYPE_EMPTY == m_valtype); }

private:
  int m_type;
  SequenceTypeImpl *m_left;
  SequenceTypeImpl *m_right;
  ItemType *m_item;
  int m_occurrence;
  bool m_isNode;
  bool m_isItem;
  int m_valtype;
};

class SequenceType : public Printable {

public:

  SequenceType() : impl(0) { }

  SequenceType(int type) { impl = new SequenceTypeImpl(type); impl->ref(); }
  SequenceType(ItemType *item) { impl = new SequenceTypeImpl(item); impl->ref(); }
  SequenceType(Type *type) { impl = new SequenceTypeImpl(type); impl->ref(); }

  SequenceType(SequenceTypeImpl *_impl) {
    impl = _impl;
    if (impl)
      impl->ref();
  }
  SequenceType(const SequenceType &other) {
    impl = other.impl;
    if (impl)
      impl->ref();
  }
  SequenceType &operator=(const SequenceType &other) {
    if (impl)
      impl->deref();
    impl = other.impl;
    if (impl)
      impl->ref();
    return *this;
  }
  ~SequenceType() { if (impl) impl->deref(); }

  inline bool isNull() const { return (0 == impl); }
  static SequenceType null() { return SequenceType(); }

  void printFS(StringBuffer &buf, NamespaceMap *namespaces) { impl->printFS(buf,namespaces); }
  void printXPath(StringBuffer &buf, NamespaceMap *namespaces) { impl->printXPath(buf,namespaces); }
  int resolve(NamespaceMap *namespaces, Schema *s, const String &filename, int line, Error *ei)
    { return impl->resolve(namespaces,s,filename,line,ei); }
  void toList(list **types) { impl->toList(types); }
  int isDerivedFrom(SequenceType &base) { return impl->isDerivedFrom(base.impl); }

  inline int type() const { return impl->type(); }
  inline SequenceType left() const { return impl->left(); }
  inline SequenceType right() const { return impl->right(); }
  inline ItemType *itemType() const { return impl->itemType(); }
  inline int occurs() const { return impl->occurs(); }
  inline bool isNode() const { return impl->isNode(); }
  inline bool isItem() const { return impl->isItem(); }
  inline int valtype() const { return impl->valtype(); }

  inline bool isItemType() const { return impl->isItemType(); }
  inline bool isSequenceType() const { return impl->isSequenceType(); }
  inline bool isEmptyType() const { return impl->isEmptyType(); }

  static SequenceType item() { return SequenceTypeImpl::item(); }
  static SequenceType node() { return SequenceTypeImpl::node(); }
  static SequenceType itemstar() { return SequenceTypeImpl::itemstar(); }
  static SequenceType nodestar() { return SequenceTypeImpl::nodestar(); }
  static SequenceType interleave(SequenceType &content)
    { return SequenceTypeImpl::interleave(content.impl); }

  static SequenceType all(const SequenceType &left, SequenceType &right)
    { return SequenceTypeImpl::all(left.impl,right.impl); }
  static SequenceType choice(const SequenceType &left, SequenceType &right)
    { return SequenceTypeImpl::choice(left.impl,right.impl); }
  static SequenceType sequence(const SequenceType &left, SequenceType &right)
    { return SequenceTypeImpl::sequence(left.impl,right.impl); }
  static SequenceType occurrence(const SequenceType &left, int occ)
    { return SequenceTypeImpl::occurrence(left.impl,occ); }

  void print(StringBuffer &buf);

private:
  SequenceTypeImpl *impl;
};

class Node {
  DISABLE_COPY(Node)
public:
  Node(int type);
  Node(xmlNodePtr xn);
  ~Node();

  Node *root();
  Node *ref();
  void deref();

  Node *deepCopy();
  void addChild(Node *c);
  void insertChild(Node *c, Node *before);
  void addAttribute(Node *attr);
  void addNamespace(Node *ns);
  int checkTree();
  Node *traversePrev(Node *subtree);
  Node *traverseNext(Node *subtree);
  void printXML(xmlTextWriter *writer);
  void printBuf(StringBuffer &buf);

  String getAttribute(const NSName &attrName) const;
  bool hasAttribute(const NSName &attrName) const;

  int m_type;
  List<Node*> m_namespaces;
  List<Node*> m_attributes;
  String m_prefix;
  NSName m_ident;
  QName m_qn;
  String m_target;
  String m_value;
  int m_refcount;

  Node *next() const { return m_next; }
  Node *prev() const { return m_prev; }
  Node *firstChild() const { return m_first_child; }
  Node *lastChild() const { return m_last_child; }
  Node *parent() const { return m_parent; }

  Node *m_next;
  Node *m_prev;
  Node *m_first_child;
  Node *m_last_child;
  Node *m_parent;
  int m_nodeno;

  xmlNodePtr m_xn;
  int m_line;
};

class ValueImpl : public GridXSLT::Shared<ValueImpl> {
  DISABLE_COPY(ValueImpl)
public:
  ValueImpl(const SequenceType &_st);
  ValueImpl(const SequenceType &_st, const Value &left, const Value &right);
  ValueImpl(Type *t);
  ValueImpl(Context *c);
  ValueImpl(int i);
  ValueImpl(float f);
  ValueImpl(double d);
  ValueImpl(char *s);
  ValueImpl(const String &s);
  ValueImpl(bool b);
  ValueImpl(Node *n);
  ValueImpl(const URI &uri);
  ValueImpl(const NSName &nn);
  ~ValueImpl();

  void init(const SequenceType &_st);
  void init(Type *t);

  int isDerivedFrom(Type *type) const;
  void printbuf(StringBuffer &buf);
  void fprint(FILE *f);
  ValueImpl **sequenceToArray();
  void getSequenceValues(List<Value> &values);
  List<Value> sequenceToList();
  String convertToString();
  ValueImpl *atomize();

  inline bool isContext() const { return isDerivedFrom(xs_g->context_type); }
  inline bool isInteger() const { return isDerivedFrom(xs_g->integer_type); }
  inline bool isFloat() const { return isDerivedFrom(xs_g->float_type); }
  inline bool isDouble() const { return isDerivedFrom(xs_g->double_type); }
  inline bool isDecimal() const { return (isDerivedFrom(xs_g->decimal_type) && !isInteger()); }
  inline bool isString() const { return isDerivedFrom(xs_g->string_type); }
  inline bool isBoolean() const { return isDerivedFrom(xs_g->boolean_type); }
  inline bool isNode() const
    { return (SEQTYPE_ITEM == st.type()) && (ITEM_ATOMIC != st.itemType()->m_kind); }
  inline bool isNumeric() const { return (isInteger() || isFloat() || isDouble() || isDecimal()); }

  inline bool isUntypedAtomic() const { return isDerivedFrom(xs_g->untyped_atomic); }
  inline bool isBase64Binary() const { return isDerivedFrom(xs_g->base64_binary_type); }
  inline bool isHexBinary() const { return isDerivedFrom(xs_g->hex_binary_type); }
  inline bool isAnyURI() const { return isDerivedFrom(xs_g->any_uri_type); }
  inline bool isQName() const { return isDerivedFrom(xs_g->qname_type); }

  inline Context *asContext() const { ASSERT(isContext()); return value.c; }
  inline int asInteger() const { ASSERT(isInteger()); return value.i; }
  inline float asFloat() const { ASSERT(isFloat()); return value.f; }
  inline double asDouble() const { ASSERT(isDouble()); return value.d; }
  inline double asDecimal() const { ASSERT(isDecimal()); return value.d; }
  inline String asString() const { ASSERT(isString()); return value.s; }
  inline bool asBoolean() const { ASSERT(isBoolean()); return value.b; }
  inline Node *asNode() const { ASSERT(isNode()); return value.n; }

  inline String asUntypedAtomic() const { ASSERT(isUntypedAtomic()); return value.s; }
  inline BinaryArray asBase64Binary() const { ASSERT(isBase64Binary()); return value.a; }
  inline BinaryArray asHexBinary() const { ASSERT(isHexBinary()); return value.a; }
  inline URI asAnyURI() const { ASSERT(isAnyURI()); return *value.u; }
  inline NSName asQName() const { ASSERT(isQName()); return *value.q; }

  inline SequenceType &type() { return st; }

  SequenceType st;
  union {
    Context *c;
    int i;
    float f;
    double d;
    StringImpl *s;
    bool b;
    BinaryArrayImpl *a;
    URI *u;
    NSName *q;
    struct {
      ValueImpl *left;
      ValueImpl *right;
    } pair;
    Node *n;
  } value;

  static void printRemaining();
};




class Value : public Printable {
  friend class ValueImpl;
public:

  Value() : impl(0) { }
  Value(const SequenceType &_st) { impl = new ValueImpl(_st); impl->ref(); }
  Value(const SequenceType &_st, const Value &left, const Value &right)
    { impl = new ValueImpl(_st,left,right); impl->ref(); }
  Value(Type *t) { impl = new ValueImpl(t); impl->ref(); }
  Value(Context *c) { impl = new ValueImpl(c); impl->ref(); }
  Value(int i) { impl = new ValueImpl(i); impl->ref(); }
  Value(float f) { impl = new ValueImpl(f); impl->ref(); }
  Value(double d) { impl = new ValueImpl(d); impl->ref(); }
  Value(char *s) { impl = new ValueImpl(s); impl->ref(); }
  Value(const String &s) { impl = new ValueImpl(s); impl->ref(); }
  Value(bool b) { impl = new ValueImpl(b); impl->ref(); }
  Value(Node *n) { impl = new ValueImpl(n); impl->ref(); }
  Value(const URI &uri) { impl = new ValueImpl(uri); impl->ref(); }
  Value(const NSName &nn) { impl = new ValueImpl(nn); impl->ref(); }

  static Value decimal(double d);
  static Value untypedAtomic(const String &str);
  static Value base64Binary(const BinaryArray &a);
  static Value hexBinary(const BinaryArray &a);
  static Value empty();

  Value(ValueImpl *_impl) {
    impl = _impl;
    if (impl)
      impl->ref();
  }
  Value(const Value &other) {
    impl = other.impl;
    if (impl)
      impl->ref();
  }
  Value &operator=(const Value &other) {
    if (impl)
      impl->deref();
    impl = other.impl;
    if (impl)
      impl->ref();
    return *this;
  }
  ~Value() { if (impl) impl->deref(); }

  inline bool isNull() const { return (0 == impl); }
  static Value null() { return Value(); }

  int isDerivedFrom(Type *type) const { return impl->isDerivedFrom(type); }
  void fprint(FILE *f) const { impl->fprint(f); }
  ValueImpl **sequenceToArray() const { return impl->sequenceToArray(); }
  void getSequenceValues(List<Value> &values) const { impl->getSequenceValues(values); }
  List<Value> sequenceToList() const { return impl->sequenceToList(); }
  String convertToString() const { return impl->convertToString(); }
  Value atomize() const { return impl->atomize(); }

  inline bool isContext() const { return impl->isContext(); }
  inline bool isInteger() const { return impl->isInteger(); }
  inline bool isFloat() const { return impl->isFloat(); }
  inline bool isDouble() const { return impl->isDouble(); }
  inline bool isDecimal() const { return impl->isDecimal(); }
  inline bool isString() const { return impl->isString(); }
  inline bool isBoolean() const { return impl->isBoolean(); }
  inline bool isNode() const { return impl->isNode(); }
  inline bool isNumeric() const { return impl->isNumeric(); }

  inline bool isUntypedAtomic() const { return impl->isUntypedAtomic(); }
  inline bool isBase64Binary() const { return impl->isBase64Binary(); }
  inline bool isHexBinary() const { return impl->isHexBinary(); }
  inline bool isAnyURI() const { return impl->isAnyURI(); }
  inline bool isQName() const { return impl->isQName(); }

  inline Context *asContext() const { return impl->asContext(); }
  inline int asInteger() const { return impl->asInteger(); }
  inline float asFloat() const { return impl->asFloat(); }
  inline double asDouble() const { return impl->asDouble(); }
  inline double asDecimal() const { return impl->asDecimal(); }
  inline String asString() const { return impl->asString(); }
  inline bool asBoolean() const { return impl->asBoolean(); }
  inline Node *asNode() const { return impl->asNode(); }

  inline String asUntypedAtomic() const { return impl->asUntypedAtomic(); }
  inline BinaryArray asBase64Binary() const { return impl->asBase64Binary(); }
  inline BinaryArray asHexBinary() const { return impl->asHexBinary(); }
  inline URI asAnyURI() const { return impl->asAnyURI(); }
  inline NSName asQName() const { return impl->asQName(); }

  inline SequenceType &type() const { return impl->type(); }

  static Value listToSequence(list *values);
  static Value listToSequence2(List<Value> &values);

  virtual void print(StringBuffer &buf) { impl->printbuf(buf); }

private:
  ValueImpl *impl;
};

#ifndef _DATAFLOW_SEQUENCETYPE_C
extern const char *df_item_kinds[ITEM_COUNT];
#endif

};

#endif // _DATAFLOW_SEQUENCETYPE2_H
