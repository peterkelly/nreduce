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

#ifndef _UTIL_NODE_H
#define _UTIL_NODE_H

#include "String.h"
#include "Namespace.h"
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

#define NODE_INVALID                  0
#define NODE_DOCUMENT                 ITEM_DOCUMENT
#define NODE_ELEMENT                  ITEM_ELEMENT
#define NODE_ATTRIBUTE                ITEM_ATTRIBUTE
#define NODE_PI                       ITEM_PI
#define NODE_COMMENT                  ITEM_COMMENT
#define NODE_TEXT                     ITEM_TEXT
#define NODE_NAMESPACE                ITEM_NAMESPACE

namespace GridXSLT {

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

  NSName resolveQName(const QName &qn) const;

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

};

#endif // _UTIL_NODE_H
