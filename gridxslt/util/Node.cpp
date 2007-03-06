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

#include "Node.h"
#include "Debug.h"
#include "XMLUtils.h"

using namespace GridXSLT;

Node::Node(int type)
  : m_type(type),
    m_refcount(0),
    m_next(NULL),
    m_prev(NULL),
    m_first_child(NULL),
    m_last_child(NULL),
    m_parent(NULL),
    m_nodeno(0),
    m_xn(NULL),
    m_line(0)
{
#ifdef TRACE_ALLOC
  list_append(&allocnodes,this);
#endif
}

Node::~Node()
{
  Node *c;

#ifdef TRACE_ALLOC
  list_remove_ptr(&allocnodes,this);
#endif

  Iterator<Node*> it;
  for (it = m_namespaces; it.haveCurrent(); it++)
    delete *it;
  for (it = m_attributes; it.haveCurrent(); it++)
    delete *it;

  c = m_first_child;
  while (c) {
    Node *next = c->m_next;
    delete c;
    c = next;
  }
}

Node *Node::root()
{
  Node *root = this;
  while (NULL != root->m_parent)
    root = root->m_parent;
  return root;
}

Node *Node::ref()
{
  root()->m_refcount++;
  return this;
}

void Node::deref()
{
  Node *r = root();
  r->m_refcount--;

/*   debugl("node_deref %s %p (root is %s %p), now have %d refs", */
/*         df_item_kinds[n->type],n,df_item_kinds[r->type],r,r->refcount); */

  ASSERT(0 <= r->m_refcount);
  if (0 == r->m_refcount)
    delete r;
}

Node *Node::deepCopy()
{
  Node *copy = new Node(m_type);
  Node *c;
/*   debugl("node_deep_copy %s %p -> %p",df_item_kinds[type],n,copy); */

  /* FIXME: namespaces */

  Iterator<Node*> it;
  for (it = m_attributes; it.haveCurrent(); it++) {
    Node *attr = *it;
    Node *attrcopy = attr->deepCopy();
    copy->addAttribute(attrcopy);
  }

  for (it = m_namespaces; it.haveCurrent(); it++) {
    Node *ns = *it;
    Node *nscopy = ns->deepCopy();
    copy->addNamespace(nscopy);
  }

  copy->m_prefix = m_prefix;
  copy->m_ident = m_ident;
  copy->m_target = m_target;
  copy->m_value = m_value;

  for (c = m_first_child; c; c = c->m_next) {
    Node *childcopy = c->deepCopy();
    copy->addChild(childcopy);
  }

  return copy;
}

void Node::addChild(Node *c)
{
  ASSERT(NULL == c->m_parent);
  ASSERT(NULL == c->m_prev);
  ASSERT(NULL == c->m_next);
  ASSERT(0 == c->m_refcount);

  if (NULL != m_last_child)
    m_last_child->m_next = this;

  c->m_prev = m_last_child;
  if (NULL == m_first_child) {
    m_first_child = c;
    m_last_child = c;
  }
  else {
    m_last_child->m_next = c;
    m_last_child = c;
  }
  c->m_parent = this;
}

void Node::insertChild(Node *c, Node *before)
{
  ASSERT(NULL == c->m_parent);
  ASSERT(NULL == c->m_prev);
  ASSERT(NULL == c->m_next);
  ASSERT(0 == c->m_refcount);

  if (NULL == before) {
    addChild(c);
    return;
  }

  c->m_prev = before->m_prev;
  before->m_prev = c;
  c->m_next = before;

  if (NULL != c->m_prev)
    c->m_prev->m_next = c;

  if (m_first_child == before)
    m_first_child = c;

  c->m_parent = this;
}

void Node::addAttribute(Node *attr)
{
  ASSERT(NULL == attr->m_parent);
  ASSERT(NULL == attr->m_prev);
  ASSERT(NULL == attr->m_next);
  ASSERT(0 == attr->m_refcount);

  m_attributes.append(attr);
  attr->m_parent = this;
}

void Node::addNamespace(Node *ns)
{
  ASSERT(NULL == ns->m_parent);
  ASSERT(NULL == ns->m_prev);
  ASSERT(NULL == ns->m_next);
  ASSERT(0 == ns->m_refcount);

  m_namespaces.append(ns);
  ns->m_parent = this;
}

Node::Node(xmlNodePtr xn)
  : m_type(0),
    m_refcount(0),
    m_next(NULL),
    m_prev(NULL),
    m_first_child(NULL),
    m_last_child(NULL),
    m_parent(NULL),
    m_nodeno(0),
    m_xn(xn),
    m_line(xn->line)
{
  xmlNodePtr c;
  struct _xmlAttr *aptr;
  ASSERT(xn);

  switch (xn->type) {
  case XML_ELEMENT_NODE: {
    xmlNs *ns;
    xmlNodePtr p;
    m_type = NODE_ELEMENT;
    m_ident.m_name = xn->name;
    m_qn.m_localPart = xn->name;
    if (xn->ns) {
      m_ident.m_ns = xn->ns->href;
      m_qn.m_prefix = xn->ns->prefix;
    }

    for (aptr = xn->properties; aptr; aptr = aptr->next) {
      Node *attr = new Node(NODE_ATTRIBUTE);
      StringBuffer buf;
      xmlNodePtr ac;
      attr->m_ident.m_name = aptr->name;
      attr->m_qn.m_localPart = aptr->name;
      if (aptr->ns) {
        attr->m_ident.m_ns = aptr->ns->href;
        attr->m_qn.m_prefix = aptr->ns->prefix;
      }

      for (ac = aptr->children; ac; ac = ac->next) {
        if (NULL != ac->content)
          buf.append(ac->content);
      }

      attr->m_value = buf.contents();
      addAttribute(attr);
    }


    for (p = xn; p; p = p->parent) {
      for (ns = p->nsDef; ns; ns = ns->next) {
//         message("Node::Node(): copying namespace mapping %s=%s\n",ns->prefix,ns->prefix);
        Node *nsnode = new Node(NODE_NAMESPACE);
        nsnode->m_prefix = ns->prefix;
        nsnode->m_value = ns->href;
        addNamespace(nsnode);
      }
    }

    break;
  }
  case XML_ATTRIBUTE_NODE:
    /* Should never see this in the main element tree */
    ASSERT(0);
    break;
  case XML_TEXT_NODE:
    m_type = NODE_TEXT;
    m_value = xn->content;
    break;
  case XML_PI_NODE:
    m_type = NODE_PI;
    m_target = xn->name;
    m_value = xn->content;
    break;
  case XML_COMMENT_NODE:
    m_type = NODE_COMMENT;
    /* FIXME: do we know that xn->content will always be non-NULL here? */
    m_value = xn->content;
    break;
  case XML_DOCUMENT_NODE:
    m_type = NODE_DOCUMENT;
    break;
  default:
    /* FIXME: support other node types such as CDATA sections and entities */
    ASSERT(!"node type not supported");
  }

  for (c = xn->children; c; c = c->next) {
    Node *cn = new Node(c);
    if (NULL != cn)
      addChild(cn);
  }
}

int Node::checkTree()
{
  Node *c;
  Node *prev = NULL;
  for (c = m_first_child; c; c = c->m_next) {
    if (c->m_prev != prev)
      return 0;
    if ((NULL == c->m_next) && (c != m_last_child))
      return 0;
    if (c->m_parent != this)
      return 0;
    if (!c->checkTree())
      return 0;
    prev = c;
  }
  return 1;
}

Node *Node::traversePrev(Node *subtree)
{
  Node *n = this;
  if (NULL != n->m_prev) {
    n = n->m_prev;
    while (NULL != n->m_last_child)
      n = n->m_last_child;
    return n;
  }

  if (n->m_parent == subtree)
    return NULL;
  else
    return n->m_parent;
}

Node *Node::traverseNext(Node *subtree)
{
  Node *n = this;
  if (NULL != n->m_first_child)
    return n->m_first_child;

  while ((NULL != n) && (NULL == n->m_next)) {
    if (n->m_parent == subtree)
      return NULL;
    else
      n = n->m_parent;
  }

  if (NULL != n)
    return n->m_next;

  return NULL;
}

void Node::printXML(xmlTextWriter *writer)
{
  switch (m_type) {
  case NODE_DOCUMENT: {
    Node *c;
    for (c = m_first_child; c; c = c->m_next)
      c->printXML(writer);
    break;
  }
  case NODE_ELEMENT: {
    Node *c;
/*     debugl("node_print NODE_ELEMENT: %s",m_name); */
    XMLWriter::startElement(writer,m_ident.m_name);
    Iterator<Node*> it;
    for (it = m_attributes; it.haveCurrent(); it++)
      (*it)->printXML(writer);
    for (it = m_namespaces; it.haveCurrent(); it++)
      (*it)->printXML(writer);
    for (c = m_first_child; c; c = c->m_next)
      c->printXML(writer);
    XMLWriter::endElement(writer);
    break;
  }
  case NODE_ATTRIBUTE:
    XMLWriter::attribute(writer,m_ident.m_name,m_value);
    break;
  case NODE_PI:
    XMLWriter::pi(writer,m_target,m_value);
    break;
  case NODE_COMMENT:
    XMLWriter::comment(writer,m_value);
    break;
  case NODE_TEXT:
/*     debugl("Node::printXML() NODE_TEXT: %s",m_value); */
    XMLWriter::string(writer,m_value);
    break;
  case NODE_NAMESPACE: {
    Node *elem = m_parent;
    Node *p;
    int have = 0;

    /* Only print it if it's not already in scope */
    if (NULL != elem) {
      for (p = elem->m_parent; p && !have; p = p->m_parent) {
        for (Iterator<Node*> it = p->m_namespaces; it.haveCurrent(); it++) {
          Node *pns = *it;
          if ((pns->m_prefix == m_prefix) && (pns->m_value == m_value)) {
            have = 1;
          }
        }
      }
    }

    if (!have) {
      if (m_prefix.isNull()) {
        XMLWriter::attribute(writer,"xmlns",m_value);
      }
      else {
        String attrname = String("xmlns:") + m_prefix;
        XMLWriter::attribute(writer,attrname,m_value);
      }
    }
    break;
  }
  default:
    ASSERT(!"invalid node type");
    break;
  }
}

void Node::printBuf(StringBuffer &buf)
{
  switch (m_type) {
  case NODE_DOCUMENT: {
    Node *c;
    for (c = m_first_child; c; c = c->m_next)
      c->printBuf(buf);
    break;
  }
  case NODE_ELEMENT: {
    Node *c;
    for (c = m_first_child; c; c = c->m_next)
      c->printBuf(buf);
    break;
  }
  case NODE_ATTRIBUTE:
    buf.append(m_value);
    break;
  case NODE_PI:
    /* FIXME */
    break;
  case NODE_COMMENT:
    /* FIXME */
    break;
  case NODE_TEXT:
    buf.append(m_value);
    break;
  default:
    ASSERT(!"invalid node type");
    break;
  }
}

String Node::getAttribute(const NSName &attrName) const
{
  for (Iterator<Node*> it = m_attributes; it.haveCurrent(); it++)
    if ((*it)->m_ident == attrName)
      return (*it)->m_value;
  return String::null();
}

bool Node::hasAttribute(const NSName &attrName) const
{
  for (Iterator<Node*> it = m_attributes; it.haveCurrent(); it++)
    if ((*it)->m_ident == attrName)
      return true;
  return false;
}

NSName Node::resolveQName(const QName &qn) const
{
  Iterator<Node*> nsit;
//   message("resolveQName: qn = %*\n",&qn);
  for (const Node *n = this; n; n = n->parent()) {
    for (nsit = n->m_namespaces; nsit.haveCurrent(); nsit++) {
      Node *nsnode = *nsit;
//       if (nsnode->m_prefix.isNull()) {
//         message("resolveQName: checking namespace mapping <null>=%*\n",&nsnode->m_value);
//       }
//       else {
//         message("resolveQName: checking namespace mapping %*=%*\n",
//                 &nsnode->m_prefix,&nsnode->m_value);
//       }
      if (nsnode->m_prefix == qn.m_prefix) {
        NSName nn = NSName(nsnode->m_value,qn.m_localPart);
//         message("resolveQName: returning %*\n",&nn);
        return nn;
      }
    }
  }
//   message("resolveQName: returning null\n");
  if (qn.m_prefix.isNull())
    return NSName(qn.m_localPart);
  else
    return NSName::null();
}

