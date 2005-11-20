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

#include "SequenceType.h"
#include "Program.h"
#include "util/Macros.h"
#include "util/Debug.h"
#include <string.h>
#include <ctype.h>
#include <math.h>

// FIXME: complete support for associating XML schema types with elements. Need a validating
// parser for this.

// Sequence type matching: Sections 2.5.4.1 and 2.5.4.2 have tests written for them. Postponing
// the remaining tests for 2.5.4.* until we can fully support element and attribute tests that
// use XML schema types.

//#define TRACE_ALLOC
//#define PRINT_SEQTYPE_ALLOC

using namespace GridXSLT;

#ifdef TRACE_ALLOC
list *allocnodes = NULL;
list *allocvalues = NULL;
list *allocseqtypes = NULL;
#endif

#define _DATAFLOW_SEQUENCETYPE_C

const char *GridXSLT::df_item_kinds[ITEM_COUNT] = {
  NULL,
  "atomic",
  "document",
  "element",
  "attribute",
  "processing-instruction",
  "comment",
  "text",
  "namespace",
};

static void df_print_objname(StringBuffer &buf, const NSName &ident, NamespaceMap *namespaces)
{
  if (!ident.m_ns.isNull()) {
    ns_def *def = namespaces->lookup_href(ident.m_ns);
    ASSERT(NULL != def); // FIXME: need to handle this gracefully... need to figure out where
                         // this can occur
    buf.format("%*:%*",&def->prefix,&ident.m_name);
  }
  else {
    buf.format("%*",&ident.m_name);
  }
}

static int is_ncname(const String &s)
{
  char *str1 = s.cstring();
  char *str = str1;
  if (!isalnum(*str) && ('_' != *str)) {
    free(str1);
    return 0;
  }

  for (str++; '\0' != *str; str++) {
    if (!isalnum(*str) && ('.' != *str) && ('-' != *str) && ('_' != *str)) {
      free(str1);
      return 0;
    }
  }

  free(str1);
  return 1;
}

// FIXME: merge with df_print_objname... only difference is the parameter order
static void print_objname(StringBuffer &buf, NamespaceMap *namespaces, const NSName &ident)
{
  if (!ident.m_ns.isNull()) {
    ns_def *def = namespaces->lookup_href(ident.m_ns);
    ASSERT(NULL != def); // FIXME: need to handle this gracefully... need to figure out where
                         // this can occur
    buf.format("%*:%*",&def->prefix,&ident.m_name);
  }
  else {
    buf.format("%*",&ident.m_name);
  }
}

ItemType::ItemType(int kind)
  : m_kind(kind),
    m_type(NULL),
    m_elem(NULL),
    m_attr(NULL),
    m_nillable(0),
    m_content(NULL)
{
}

ItemType::~ItemType()
{
  if (NULL != m_content)
    m_content->deref();
}

void ItemType::printFS(StringBuffer &buf, NamespaceMap *namespaces,  bool abbrev)
{
  switch (m_kind) {
  case ITEM_ATOMIC:
    df_print_objname(buf,m_type->def.ident,namespaces);
    break;
  case ITEM_DOCUMENT:
    buf.append("document");
    if (NULL != m_content) {
      buf.append(" { ");
      m_content->printFS(buf,namespaces,abbrev);
      buf.append(" }");
    }
    break;
  case ITEM_ELEMENT:
    buf.append("element");


    if (NULL != m_elem) {
      buf.append(" ");
      df_print_objname(buf,m_elem->def.ident,namespaces);

      if (!m_elem->toplevel) {
        if (m_elem->nillable)
          buf.append(" nillable");
        buf.append(" of type ");
        df_print_objname(buf,m_elem->type->def.ident,namespaces);
      }
    }
    else if (NULL != m_type) {
      if (m_nillable)
        buf.append(" nillable");
      buf.append(" of type ");
      df_print_objname(buf,m_type->def.ident,namespaces);
    }
    break;
  case ITEM_ATTRIBUTE:
    if (NULL != m_attr) {
      if (m_attr->toplevel) {
        buf.append("attribute ");
        df_print_objname(buf,m_attr->def.ident,namespaces);
      }
      else {
        buf.append("attribute ");
        df_print_objname(buf,m_attr->def.ident,namespaces);
        buf.append(" of type ");
        df_print_objname(buf,m_attr->type->def.ident,namespaces);
      }
    }
    else if (NULL != m_type) {
      buf.append("attribute of type ");
      df_print_objname(buf,m_type->def.ident,namespaces);
    }
    else {
      buf.append("attribute");
    }

    break;
  case ITEM_PI:
    buf.append("processing-instruction");
    break;
  case ITEM_COMMENT:
    buf.append("comment");
    break;
  case ITEM_TEXT:
    buf.append("text");
    break;
  case ITEM_NAMESPACE:
    buf.append("namespace"); /* FIXME: is this valid? */
    break;
  default:
    ASSERT(!"invalid item type");
    break;
  }
}

void ItemType::printXPath(StringBuffer &buf, NamespaceMap *namespaces)
{
  switch (m_kind) {
  case ITEM_ATOMIC:
    print_objname(buf,namespaces,m_type->def.ident);
    break;
  case ITEM_DOCUMENT:
    buf.append("document-node(");
    if (NULL != m_content) {
      ASSERT(SEQTYPE_ALL == m_content->type());
      ASSERT(SEQTYPE_ITEM == m_content->left()->type());
      ASSERT(ITEM_ELEMENT == m_content->left()->itemType()->m_kind);
      m_content->left()->itemType()->printXPath(buf,namespaces);
    }
    buf.append(")");
    break;
  case ITEM_ELEMENT:
    if (NULL != m_elem) {
      if (m_elem->toplevel) {
        buf.append("schema-element(");
        print_objname(buf,namespaces,m_elem->def.ident);
        buf.append(")");
      }
      else {
        buf.append("element(");
        print_objname(buf,namespaces,m_elem->def.ident);

        if ((m_elem->type->def.ident.m_ns != XS_NAMESPACE) ||
            (m_elem->type->def.ident.m_name != "anyType") ||
            !m_elem->nillable) {
          buf.append(",");
          print_objname(buf,namespaces,m_elem->type->def.ident);
          if (m_elem->nillable)
            buf.append("?");
        }
        buf.append(")");
      }

    }
    else if (NULL != m_type) {
      buf.append("element(*,");
      print_objname(buf,namespaces,m_type->def.ident);
      if (m_nillable)
        buf.append("?");
      buf.append(")");
    }
    else {
      buf.append("element()");
    }
    break;
  case ITEM_ATTRIBUTE:
    if (NULL != m_attr) {
      if (m_attr->toplevel) {
        buf.append("schema-attribute(");
        print_objname(buf,namespaces,m_attr->def.ident);
        buf.append(")");
      }
      else {
        buf.append("attribute(");
        print_objname(buf,namespaces,m_attr->def.ident);

        if ((m_attr->type->def.ident.m_ns != XS_NAMESPACE) ||
            (m_attr->type->def.ident.m_name != "anySimpleType")) {
          buf.append(",");
          print_objname(buf,namespaces,m_attr->type->def.ident);
        }
        buf.append(")");
      }
    }
    else if (NULL != m_type) {
      buf.append("attribute(*,");
      print_objname(buf,namespaces,m_type->def.ident);
      buf.append(")");
    }
    else {
      buf.append("attribute()");
    }
    break;
  case ITEM_PI:
    buf.append("processing-instruction(");
    if (!m_pistr.isNull()) {
      if (is_ncname(m_pistr))
        buf.format("%*",&m_pistr);
      else
        buf.format("\"%*\"",&m_pistr);
    }
    buf.append(")");
    break;
  case ITEM_COMMENT:
    buf.append("comment()");
    break;
  case ITEM_TEXT:
    buf.append("text()");
    break;
  case ITEM_NAMESPACE:
    buf.append("namespace()"); /* FIXME: is this valid? */
    break;
  default:
    ASSERT(!"invalid item type");
    break;
  }
}

static int resolve_object(const QName &qn, NamespaceMap *namespaces, Schema *s,
                          const String &filename, int line,Error *ei, void **obj, int type,
                          const String &errname)
{
  ns_def *def = NULL;
  String ns;

  if (qn.isNull())
    return 0;

/*   debugl("Resolving %s:%s",qn.prefix,qn.localpart); */

  if (qn.m_prefix == "__xdt") {
    ns = XDT_NAMESPACE;
  }
  else {
    if ((!qn.m_prefix.isNull()) &&
        (NULL == (def = namespaces->lookup_prefix(qn.m_prefix))))
      return error(ei,filename,line,errname,"Invalid namespace prefix: %*",&qn.m_prefix);

    ns = def ? def->href : String::null();
  }

  if (NULL == (*obj = s->getObject(type,NSName(ns,qn.m_localPart))))
    return error(ei,filename,line,errname,"No such %s: %*",xs_object_types[type],&qn);

  return 0;
}

int SequenceTypeImpl::resolve(NamespaceMap *namespaces, Schema *s, const String &filename,
                          int line, Error *ei)
{
  if (NULL != m_left)
    CHECK_CALL(m_left->resolve(namespaces,s,filename,line,ei))
  if (NULL != m_right)
    CHECK_CALL(m_right->resolve(namespaces,s,filename,line,ei))

  if (NULL != m_item) {
    ns_def *def = NULL;
    ItemType *i = m_item;
    String ns;
    CHECK_CALL(resolve_object(i->m_typeref,namespaces,s,filename,line,ei,
               (void**)&i->m_type,XS_OBJECT_TYPE,"XPST0051"))
    CHECK_CALL(resolve_object(i->m_elemref,namespaces,s,filename,line,ei,
               (void**)&i->m_elem,XS_OBJECT_ELEMENT,String::null()))
    CHECK_CALL(resolve_object(i->m_attrref,namespaces,s,filename,line,ei,
               (void**)&i->m_attr,XS_OBJECT_ATTRIBUTE,String::null()))

    if ((!m_item->m_localname.m_prefix.isNull()) &&
        (NULL == (def = namespaces->lookup_prefix(m_item->m_localname.m_prefix))))
      return error(ei,filename,line,String::null(),"Invalid namespace prefix: %*",
                   &m_item->m_localname.m_prefix);

    ns = def ? def->href : String::null();

    if ((ITEM_ELEMENT == m_item->m_kind) && (!m_item->m_localname.m_localPart.isNull())) {
      m_item->m_elem = new SchemaElement(s->as);
      m_item->m_elem->def.ident = NSName(ns,m_item->m_localname.m_localPart);
      m_item->m_elem->type = i->m_type;
      i->m_type = NULL;
      m_item->m_elem->nillable = i->m_nillable;
      m_item->m_elem->computed_type = 1;

      if (NULL == m_item->m_elem->type) {
        m_item->m_elem->type = xs_g->complex_ur_type;
        m_item->m_elem->nillable = 1;
        i->m_nillable = 1;
      }
    }
    else if ((ITEM_ATTRIBUTE == m_item->m_kind) && (!m_item->m_localname.m_localPart.isNull())) {
      m_item->m_attr = new SchemaAttribute(s->as);
      m_item->m_attr->def.ident = NSName(ns,m_item->m_localname.m_localPart);
      m_item->m_attr->type = i->m_type;
      i->m_type = NULL;

      if (NULL == m_item->m_attr->type)
        m_item->m_attr->type = xs_g->simple_ur_type;
    }
    else if ((ITEM_DOCUMENT == m_item->m_kind) && (NULL != m_item->m_content)) {
      CHECK_CALL(m_item->m_content->resolve(namespaces,s,filename,line,ei))
    }
  }

  return 0;
}

void SequenceTypeImpl::toList(list **types)
{
  if (SEQTYPE_SEQUENCE == m_type) {
    m_left->toList(types);
    m_right->toList(types);
  }
  else if (SEQTYPE_ITEM == m_type) {
    list_append(types,this);
  }
  else {
    ASSERT(SEQTYPE_EMPTY == m_type);
  }
}

static bool validate_sequence_item(SequenceTypeImpl *base, SequenceTypeImpl *st)
{
//   message("validate_sequence_item: base->itemType()->m_kind = %s\n",
//           df_item_kinds[base->itemType()->m_kind]);
  if (base->itemType()->m_kind != st->itemType()->m_kind)
    return false;

  switch (base->itemType()->m_kind) {
  case ITEM_ATOMIC: {
    /* FIXME: handle union and list simple types */
    Type *basetype = base->itemType()->m_type;
    Type *derived = st->itemType()->m_type;
//     message("validate_sequence_item: checking if %* is derived from %*\n",
//             &derived->def.ident,&basetype->def.ident);
    while (1) {
      if (basetype == derived)
        return true;
      if (derived->base == derived)
        return false;
      derived = derived->base;
    }
    break;
  }
  case ITEM_DOCUMENT:
    /* FIXME: check further details of type */
    return true;
  case ITEM_ELEMENT:
    /* FIXME: check further details of type */
    return true;
  case ITEM_ATTRIBUTE:
    /* FIXME: check further details of type */
    return true;
  case ITEM_PI:
    return true;
  case ITEM_COMMENT:
    return true;
  case ITEM_TEXT:
    return true;
  case ITEM_NAMESPACE:
    return true;
  }

  ASSERT(!"invalid item kind");
  return false;
}

static bool validate_sequence_list(SequenceTypeImpl *base, list **lptr)
{
  switch (base->type()) {
  case SEQTYPE_ITEM: {
    if (NULL == *lptr)
      return false;
    SequenceTypeImpl *item = (SequenceTypeImpl*)((*lptr)->data);
    SequenceType tst(item);
    SequenceType bst(base);
    ASSERT(SEQTYPE_ITEM == item->type());
    if (validate_sequence_item(base,item)) {
//       message("validate_sequence_list 2: base %* matches item %*\n",&bst,&tst);
//       message("prev remaining: %d\n",list_count(*lptr));
      *lptr = (*lptr)->next;
//       message("now remaining: %d\n",list_count(*lptr));
      return true;
    }
//     message("validate_sequence_list 2: base %* DOES NOT match item %*\n",&bst,&tst);
    return false;
  }
  case SEQTYPE_OCCURRENCE: {
    list *backup = *lptr;
    switch (base->occurs()) {
    case OCCURS_ONCE:
      return validate_sequence_list(base->left(),lptr);
    case OCCURS_OPTIONAL:
      if (validate_sequence_list(base->left(),lptr))
        return true;
      *lptr = backup;
      return true;
    case OCCURS_ZERO_OR_MORE:
      while (validate_sequence_list(base->left(),lptr))
        backup = *lptr;
      *lptr = backup;
      return true;
    case OCCURS_ONE_OR_MORE:
      if (!validate_sequence_list(base->left(),lptr))
        return false;
      backup = *lptr;
      while (validate_sequence_list(base->left(),lptr))
        backup = *lptr;
      *lptr = backup;
      return true;
    }
    ASSERT(!"invalid occurrence type");
    break;
  }
  case SEQTYPE_ALL:
    ASSERT(!"SEQTYPE_ALL not yet supported");
    break;
  case SEQTYPE_SEQUENCE:
    return (validate_sequence_list(base->left(),lptr) &&
            validate_sequence_list(base->right(),lptr));
  case SEQTYPE_CHOICE: {
    list *backup = *lptr;
    if (validate_sequence_list(base->left(),lptr))
      return true;
    *lptr = backup;
    return validate_sequence_list(base->right(),lptr);
  }
  case SEQTYPE_EMPTY:
    return (0 == *lptr);
  }
  ASSERT(!"invalid sequence type");
  return 0;
}

bool SequenceTypeImpl::isDerivedFrom(SequenceTypeImpl *base)
{
  SequenceType st1(this);
  SequenceType st2(base);
//   message("SequenceTypeImpl::isDerivedFrom(): this = %*, base = %*\n",&st1,&st2);
  list *types = NULL;
  list *types2;
  toList(&types);
  types2 = types;

  bool valid = validate_sequence_list(base,&types2);

  if (NULL != types2) { // didn't match the whole sequence
//     message("isDerivedFrom: remaining: %d of %d sequence items\n",
//             list_count(types),list_count(types2));
    valid = false;
  }

  list_free(types,NULL);
//   message("SequenceTypeImpl::isDerivedFrom() returning %d\n",valid);

  return valid;
}

SequenceTypeImpl::SequenceTypeImpl(int type)
{
  init();
  m_type = type;
}

SequenceTypeImpl::SequenceTypeImpl(ItemType *item)
{
  init();
  m_type = SEQTYPE_ITEM;
  m_item = item;
}

SequenceTypeImpl::SequenceTypeImpl(Type *type)
{
  init();
  m_type = SEQTYPE_ITEM;
  m_item = new ItemType(ITEM_ATOMIC);
  m_item->m_type = type;
}

SequenceTypeImpl::~SequenceTypeImpl()
{
#ifdef PRINT_SEQTYPE_ALLOC
  message("SequenceTypeImpl::~SequenceTypeImpl() %p\n",this);
#endif
  if (NULL != m_item)
    delete m_item;
  if (NULL != m_left)
    m_left->deref();
  if (NULL != m_right)
    m_right->deref();
#ifdef TRACE_ALLOC
  list_remove_ptr(&allocseqtypes,this);
#endif
}

static SequenceTypeImpl *add_alternative(SequenceTypeImpl *st1, SequenceTypeImpl *st2)
{
  return SequenceTypeImpl::choice(st1,st2);
}

static SequenceTypeImpl *create_item_or_node(int item)
{
  SequenceTypeImpl *i1 = new SequenceTypeImpl(new ItemType(ITEM_ELEMENT));
  SequenceTypeImpl *i2 = new SequenceTypeImpl(new ItemType(ITEM_ATTRIBUTE));
  SequenceTypeImpl *i3 = new SequenceTypeImpl(new ItemType(ITEM_TEXT));
  SequenceTypeImpl *i4 = new SequenceTypeImpl(new ItemType(ITEM_DOCUMENT));
  SequenceTypeImpl *i5 = new SequenceTypeImpl(new ItemType(ITEM_COMMENT));
  SequenceTypeImpl *i6 = new SequenceTypeImpl(new ItemType(ITEM_PI));
  SequenceTypeImpl *i7 = new SequenceTypeImpl(new ItemType(ITEM_NAMESPACE)); // not part of spec

  SequenceTypeImpl *choice = add_alternative(i1,i2);
  choice = add_alternative(choice,i3);
  choice = add_alternative(choice,i4);
  choice = add_alternative(choice,i5);
  choice = add_alternative(choice,i6);
  choice = add_alternative(choice,i7);

  if (item) {
    SequenceTypeImpl *i7 = new SequenceTypeImpl(new ItemType(ITEM_ATOMIC));
    /* FIXME: don't use xdt prefix here; it may not actually be mapped in the current context, or
       maybe mapped to a different namespace than the one we expect */
    i7->itemType()->m_typeref = QName("__xdt","anyAtomicType");
    i7->itemType()->m_type = xs_g->any_atomic_type;
    choice = add_alternative(choice,i7);
  }

  return choice;
}

SequenceTypeImpl *SequenceTypeImpl::item()
{
  SequenceTypeImpl *st = create_item_or_node(1);
  st->m_isItem = 1;
  return st;
}

SequenceTypeImpl *SequenceTypeImpl::node()
{
  SequenceTypeImpl *st = create_item_or_node(0);
  st->m_isNode = 1;
  return st;
}

SequenceTypeImpl *SequenceTypeImpl::itemstar()
{
  return SequenceTypeImpl::occurrence(SequenceTypeImpl::item(),OCCURS_ZERO_OR_MORE);
}

SequenceTypeImpl *SequenceTypeImpl::nodestar()
{
  return SequenceTypeImpl::occurrence(SequenceTypeImpl::node(),OCCURS_ZERO_OR_MORE);
}

SequenceTypeImpl *SequenceTypeImpl::interleave(SequenceTypeImpl *content)
{
  SequenceTypeImpl *pi = new SequenceTypeImpl(new ItemType(ITEM_PI));
  SequenceTypeImpl *comment = new SequenceTypeImpl(new ItemType(ITEM_COMMENT));
  SequenceTypeImpl *choice = SequenceTypeImpl::choice(pi,comment);
  SequenceTypeImpl *occurrence = SequenceTypeImpl::occurrence(choice,SEQTYPE_OCCURRENCE);
  SequenceTypeImpl *all = SequenceTypeImpl::all(content,occurrence);

  /* FIXME: formal semantics 3.5.4 says that text nodes should also be allowed, but the actual
     formal definition doesn't include this - what to do here? */
  return all;
}

SequenceTypeImpl *SequenceTypeImpl::all(SequenceTypeImpl *left, SequenceTypeImpl *right)
{
  SequenceTypeImpl *st = new SequenceTypeImpl(SEQTYPE_ALL);
  st->m_left = left->ref();
  st->m_right = right->ref();
  return st;
}

SequenceTypeImpl *SequenceTypeImpl::choice(SequenceTypeImpl *left, SequenceTypeImpl *right)
{
  SequenceTypeImpl *st = new SequenceTypeImpl(SEQTYPE_CHOICE);
  st->m_left = left->ref();
  st->m_right = right->ref();
  return st;
}

SequenceTypeImpl *SequenceTypeImpl::sequence(SequenceTypeImpl *left, SequenceTypeImpl *right)
{
  SequenceTypeImpl *st = new SequenceTypeImpl(SEQTYPE_SEQUENCE);
  st->m_left = left->ref();
  st->m_right = right->ref();
  return st;
}

SequenceTypeImpl *SequenceTypeImpl::occurrence(SequenceTypeImpl *left, int occ)
{
  // If the type in question is already the same occurrence indicator, no need to do anything...
  // FIXME: is there something in formal semantics about this?
  if ((left->type() == SEQTYPE_OCCURRENCE) && (left->occurs() == occ))
    return left;

  SequenceTypeImpl *st = new SequenceTypeImpl(SEQTYPE_OCCURRENCE);
  st->m_left = left->ref();
  st->m_occurrence = occ;
  return st;
}

void SequenceTypeImpl::init()
{
#ifdef PRINT_SEQTYPE_ALLOC
  message("SequenceTypeImpl::init() %p\n",this);
#endif
  m_type = 0;
  m_left = NULL;
  m_right = NULL;
  m_item = NULL;
  m_occurrence = 0;
  m_isNode = 0;
  m_isItem = 0;
  m_valtype = 0;
#ifdef TRACE_ALLOC
  list_push(&allocseqtypes,this);
#endif
}

void SequenceTypeImpl::printFS(StringBuffer &buf, NamespaceMap *namespaces, bool abbrev)
{
  // item() and node() are not actually part of the formal semantics type syntax - however since
  // we currently use this output function for debugging purposes, it is more convenient to
  // show the condensed form from XPath syntax rather than the full expansion

  // We can only do this if the abbreviation is specifically allowed... for some tests we need
  // the formal semantics core syntax
  if (abbrev && m_isItem) {
    buf.append("item()");
    return;
  }

  if (abbrev && m_isNode) {
    buf.append("node()");
    return;
  }

  switch (m_type) {
  case SEQTYPE_ITEM:
    m_item->printFS(buf,namespaces,abbrev);
    break;
  case SEQTYPE_OCCURRENCE:
    m_left->printFS(buf,namespaces,abbrev);
    switch (m_occurrence) {
    case OCCURS_ONCE:
      break;
    case OCCURS_OPTIONAL:
      buf.append("?");
      break;
    case OCCURS_ZERO_OR_MORE:
      buf.append("*");
      break;
    case OCCURS_ONE_OR_MORE:
      buf.append("+");
      break;
    default:
      ASSERT(!"invalid occurrence");
      break;
    }
    break;
  case SEQTYPE_ALL:
    buf.append("(");
    m_left->printFS(buf,namespaces,abbrev);
    buf.append(" & ");
    m_right->printFS(buf,namespaces,abbrev);
    buf.append(")");
    break;
  case SEQTYPE_SEQUENCE:
    buf.append("(");
    m_left->printFS(buf,namespaces,abbrev);
    buf.append(" , ");
    m_right->printFS(buf,namespaces,abbrev);
    buf.append(")");
    break;
  case SEQTYPE_CHOICE:
    buf.append("(");
    m_left->printFS(buf,namespaces,abbrev);
    buf.append(" | ");
    m_right->printFS(buf,namespaces,abbrev);
    buf.append(")");
    break;
  case SEQTYPE_EMPTY:
    buf.append("empty");
    break;
  default:
    ASSERT(!"invalid sequence type");
    break;
  }
}

void SequenceTypeImpl::printXPath(StringBuffer &buf, NamespaceMap *namespaces)
{
  if (SEQTYPE_ITEM == m_type) {
    m_item->printXPath(buf,namespaces);
  }
  else if (SEQTYPE_OCCURRENCE == m_type) {
    ASSERT(SEQTYPE_ITEM == m_left->m_type);
    if ((ITEM_PI == m_left->m_item->m_kind) &&
        (OCCURS_OPTIONAL == m_occurrence) &&
        (!m_left->m_item->m_pistr.isNull())) {
      m_left->printXPath(buf,namespaces);
    }
    else {
      m_left->printXPath(buf,namespaces);
      switch (m_occurrence) {
      case OCCURS_ONCE:
        break;
      case OCCURS_OPTIONAL:
        buf.append("?");
        break;
      case OCCURS_ZERO_OR_MORE:
        buf.append("*");
        break;
      case OCCURS_ONE_OR_MORE:
        buf.append("+");
        break;
      default:
        ASSERT(!"invalid occurrence");
        break;
      }
    }
  }
  else if (SEQTYPE_EMPTY == m_type) {
    buf.append("void()");
  }
  else if (m_isNode) {
    buf.append("node()");
  }
  else if (m_isItem) {
    buf.append("item()");
  }
  else if (SEQTYPE_CHOICE == m_type) {
    /* FIXME: temp - just so we cab print the select nodes of the built-in templates */
    m_left->printXPath(buf,namespaces);
    buf.append(" or ");
    m_right->printXPath(buf,namespaces);
  }
  else {
    ASSERT(!"sequencetype not expressible in xpath syntax");
  }
}

void SequenceType::print(StringBuffer &buf)
{
  impl->printFS(buf,xs_g->namespaces,false);
}

ValueImpl::ValueImpl(const SequenceType &_st)
{
  init(_st);
}

ValueImpl::ValueImpl(const SequenceType &_st, const Value &left, const Value &right)
{
  init(_st);
  value.pair.left = left.impl->ref();
  value.pair.right = right.impl->ref();
}

ValueImpl::ValueImpl(Type *t)
{
  init(t);
}

ValueImpl::ValueImpl(Context *c)
{
  init(xs_g->context_type);
  value.c = c;
}

ValueImpl::ValueImpl(int i)
{
  init(xs_g->integer_type);
  value.i = i;
}

ValueImpl::ValueImpl(float f)
{
  init(xs_g->float_type);
  value.f = f;
}

ValueImpl::ValueImpl(double d)
{
  init(xs_g->double_type);
  value.d = d;
}

ValueImpl::ValueImpl(char *s)
{
  init(xs_g->string_type);
  String str(s);
  value.s = str.handle()->ref();
}

ValueImpl::ValueImpl(const String &s)
{
  init(xs_g->string_type);
  value.s = s.handle()->ref();
}

ValueImpl::ValueImpl(bool b)
{
  init(xs_g->boolean_type);
  value.b = b;
}

ValueImpl::ValueImpl(Node *n)
{
  init(SequenceType(new ItemType(n->m_type)));
  value.n = n->ref();
}

ValueImpl::ValueImpl(const URI &uri)
{
  init(xs_g->any_uri_type);
  value.u = new URI(uri);
}

ValueImpl::ValueImpl(const NSName &nn)
{
  init(xs_g->qname_type);
  value.q = new NSName(nn);
}

ValueImpl::~ValueImpl()
{
  if (SEQTYPE_ITEM == st.type()) {
    if (ITEM_ATOMIC == st.itemType()->m_kind) {
      if (isString() || isUntypedAtomic())
        value.s->deref();
      else if (isContext())
        delete value.c;
      else if (isBase64Binary() || isHexBinary())
        value.a->deref();
      else if (isAnyURI())
        delete value.u;
      else if (isQName())
        delete value.q;
    }
    else if (NULL != value.n) {
      value.n->deref();
    }
  }
  else if (SEQTYPE_SEQUENCE == st.type()) {
    value.pair.left->deref();
    value.pair.right->deref();
  }
#ifdef TRACE_ALLOC
  list_remove_ptr(&allocvalues,this);
#endif
}

void ValueImpl::init(const SequenceType &_st)
{
#ifdef TRACE_ALLOC
  list_append(&allocvalues,this);
#endif
  st = _st;
}

void ValueImpl::init(Type *t)
{
  init(SequenceType(t));
}

bool ValueImpl::isDerivedFrom(Type *type) const
{
  Type *t1;
  if (SEQTYPE_ITEM != st.type())
    return false;

  if (ITEM_ATOMIC != st.itemType()->m_kind)
    return false;

  for (t1 = st.itemType()->m_type; t1 != t1->base; t1 = t1->base)
    if (t1 == type)
      return true;

  return false;
}

static int xmlwrite_stringbuf(void *context, const char * buffer, int len)
{
  stringbuf_append((stringbuf*)context,buffer,len);
  return len;
}

static void printDouble(double d, StringBuffer &buf)
{
  if (d == double(int(d))) {
    int i = int(d);
    if (signbit(d) && (0.0 == d))
      buf.format("-0");
    else
      buf.format("%d",i);
  }
  else if ((0.000001 < fabs(d)) && (1000000.0 > fabs(d))) {
    int ipart = int(d);
    double fraction;

    if (0.0 < d)
      fraction = d - floor(d);
    else
      fraction = d - ceil(d);

    int start = signbit(d) ? 1 : 0;

    char tmp[100];
    sprintf(tmp,"%f",fraction);
    int pos = strlen(tmp)-1;
    while ((2+start < pos) && ('0' == tmp[pos]))
      tmp[pos--] = '\0';
    ASSERT('0' == tmp[start]);
    ASSERT('.' == tmp[start+1]);

    buf.format("%d.%s",ipart,tmp+start+2);
  }
  else if (0.0 == d) {
    buf.format("0");
  }
  else if (isnan(d)) {
    buf.format("NaN");
  }
  else if (isinf(d) && (0.0 < d)) {
    buf.format("INF");
  }
  else if (isinf(d) && (0.0 > d)) {
    buf.format("-INF");
  }
  else {
    buf.format("%f",d);
  }
}



void ValueImpl::printbuf(StringBuffer &buf)
{
  if (SEQTYPE_ITEM == st.type()) {

    if (ITEM_ATOMIC == st.itemType()->m_kind) {
      if (isFloat())
        printDouble(double(value.f),buf);
      else if (isDouble())
        printDouble(value.d,buf);
      else if (isInteger())
        buf.format("%d",value.i);
      else if (isDecimal())
        printDouble(value.d,buf);
      else if (isString() || isUntypedAtomic())
        buf.append(value.s);
      else if (isAnyURI())
        buf.append(value.u->toString());
      else if (isBase64Binary())
        buf.append(value.a->toBase64());
      else if (isHexBinary())
        buf.append(value.a->toHex());
      else if (isBoolean())
        buf.format("%s",value.b ? "true" : "false");
      else
        buf.format("(atomic value %*)",&st.itemType()->m_type->def.ident); /* FIXME */
    }
    else if ((ITEM_ELEMENT == st.itemType()->m_kind) ||
             (ITEM_DOCUMENT == st.itemType()->m_kind)) {
      stringbuf *sb = stringbuf_new();
      xmlOutputBuffer *xb = xmlOutputBufferCreateIO(xmlwrite_stringbuf,NULL,sb,NULL);
      xmlTextWriter *writer = xmlNewTextWriter(xb);
      xmlTextWriterSetIndent(writer,1);
      xmlTextWriterSetIndentString(writer,"  ");
      xmlTextWriterStartDocument(writer,NULL,NULL,NULL);

      value.n->printXML(writer);

      xmlTextWriterEndDocument(writer);
      xmlTextWriterFlush(writer);
      xmlFreeTextWriter(writer);
      buf.append(sb->data);
      stringbuf_free(sb);
    }
    else if ((ITEM_TEXT == st.itemType()->m_kind) ||
             (ITEM_ATTRIBUTE == st.itemType()->m_kind)) {
      buf.append(value.n->m_value);
    }
    else {
      /* FIXME */
      buf.format("(item, kind %d)",st.itemType()->m_kind);
    }
  }
  else if (SEQTYPE_SEQUENCE == st.type()) {
    value.pair.left->printbuf(buf);
    buf.append(", ");
    value.pair.right->printbuf(buf);
  }
  else if (SEQTYPE_EMPTY == st.type()) {
    buf.append("(empty)");
  }
  else {
    buf.format("(value, SequenceTypeImpl %d)",st.type());
  }
}

void ValueImpl::fprint(FILE *f)
{
  StringBuffer buf;
  printbuf(buf);
  String contents = buf.contents();
  fmessage(f,"%*",&contents);
}

static int df_count_sequence_values(ValueImpl *seq)
{
  if (SEQTYPE_SEQUENCE == seq->st.type()) {
    return df_count_sequence_values(seq->value.pair.left) +
           df_count_sequence_values(seq->value.pair.right);
  }
  else if (SEQTYPE_ITEM == seq->st.type()) {
    return 1;
  }
  else {
    ASSERT(SEQTYPE_EMPTY == seq->st.type());
    return 0;
  }
}

static void df_build_sequence_array(ValueImpl *seq, ValueImpl ***valptr)
{
  if (SEQTYPE_SEQUENCE == seq->st.type()) {
    df_build_sequence_array(seq->value.pair.left,valptr);
    df_build_sequence_array(seq->value.pair.right,valptr);
  }
  else if (SEQTYPE_ITEM == seq->st.type()) {
    *((*valptr)++) = seq;
  }
}

ValueImpl **ValueImpl::sequenceToArray()
{
  int count = df_count_sequence_values(this);
  ValueImpl **valptr;
  ValueImpl **values = (ValueImpl**)malloc((count+1)*sizeof(ValueImpl*));
  valptr = values;
  df_build_sequence_array(this,&valptr);
  ASSERT(valptr == values+count);
  values[count] = NULL;
  return values;
}

void ValueImpl::getSequenceValues(List<Value> &values)
{
  if (SEQTYPE_SEQUENCE == st.type()) {
    value.pair.left->getSequenceValues(values);
    value.pair.right->getSequenceValues(values);
  }
  else if (SEQTYPE_ITEM == st.type()) {
    values.append(this);
  }
  else {
    ASSERT(SEQTYPE_EMPTY == st.type());
  }
}

List<Value> ValueImpl::sequenceToList()
{
  List<Value> values;
  getSequenceValues(values);
  return values;
}

String ValueImpl::convertToString()
{
  Value atomicv;
  StringBuffer buf;

  /* FIXME: implement using df_cast() */

  if (SEQTYPE_EMPTY == st.type()) {
    return strdup("");
  }

  if ((SEQTYPE_ITEM != st.type()) ||
      (ITEM_ATOMIC != st.itemType()->m_kind))
    atomicv = atomize();
  else
    atomicv = this;

  atomicv.print(buf);

  return buf.contents();
}

ValueImpl *ValueImpl::atomize()
{
  if (SEQTYPE_SEQUENCE == st.type()) {
    SequenceType atomicseq;
    ValueImpl *left = value.pair.left->atomize();
    ValueImpl *right = value.pair.right->atomize();

    atomicseq = SequenceType::sequence(left->st,right->st);

    return new ValueImpl(atomicseq,left,right);
  }
  else if (SEQTYPE_ITEM == st.type()) {
    if (ITEM_ATOMIC == st.itemType()->m_kind) {
      return this;
    }
    else {
      /* FIXME: this is just a quick and dirty implementation of node atomization... need to
         follow the rules set out in XPath 2.0 section 2.5.2 */
      StringBuffer buf;
      value.n->printBuf(buf);
      ValueImpl *atom = new ValueImpl(buf.contents());
      return atom;
    }
  }
  else {
    ASSERT(SEQTYPE_EMPTY == st.type());
    /* FIXME: is this safe? are there any situations in which atomize() could be called with
       an empty sequence? */
    ASSERT(!"can't atomize an empty sequence");
    return NULL;
  }
}

void ValueImpl::printRemaining()
{
#ifdef TRACE_ALLOC
  list *l;
  for (l = allocnodes; l; l = l->next) {
    Node *n = (Node*)l->data;
    if (NODE_ELEMENT == n->m_type)
      message("remaining node %p %-10s %-10s - %-3d refs\n",n,"element",n->m_ident.name,n->m_refcount);
    else if (NODE_TEXT == n->m_type)
      message("remaining node %p %-10s %* - %-3d refs\n",n,"text",&n->m_value,n->m_refcount);
    else
      message("remaining node %p %-10s %-10s - %-3d refs\n",n,df_item_kinds[n->m_type],"",n->m_refcount);
  }
  for (l = allocvalues; l; l = l->next) {
    ValueImpl *v = (ValueImpl*)l->data;
    StringBuffer stbuf;
    StringBuffer valbuf;
    v->st.printFS(stbuf,xs_g->namespaces,true);
    if ((SEQTYPE_ITEM == v->st.type()) &&
        (ITEM_ATOMIC == v->st.itemType()->m_kind))
      v->printbuf(valbuf);
    message("remaining value %p %* - %-3d refs : %*",v,&stbuf,v->refCount(),&valbuf);
    if (v->isDerivedFrom(xs_g->string_type))
      message(" \"%s\"",v->value.s);
    message("\n");
  }
  for (l = allocseqtypes; l; l = l->next) {
    SequenceTypeImpl *st = (SequenceTypeImpl*)l->data;
    StringBuffer buf;
    st->printFS(buf,xs_g->namespaces,true);
    message("remaining SequenceTypeImpl %* - %d refs\n",buf,st->refCount());
  }
  message("SequenceTypeImpl instances remaining: %d\n",list_count(allocseqtypes));
#endif
}

Value Value::decimal(double d)
{
  ValueImpl *vimpl = new ValueImpl(xs_g->decimal_type);
  vimpl->value.d = d;
  return vimpl;
}

Value Value::untypedAtomic(const String &str)
{
  ValueImpl *vimpl = new ValueImpl(xs_g->untyped_atomic);
  vimpl->value.s = str.handle()->ref();
  return vimpl;
}

Value Value::base64Binary(const BinaryArray &a)
{
  ValueImpl *vimpl = new ValueImpl(xs_g->base64_binary_type);
  vimpl->value.a = a.handle()->ref();
  return vimpl;
}

Value Value::hexBinary(const BinaryArray &a)
{
  ValueImpl *vimpl = new ValueImpl(xs_g->hex_binary_type);
  vimpl->value.a = a.handle()->ref();
  return vimpl;
}

Value Value::empty()
{
  return Value(SequenceType(SEQTYPE_EMPTY));
}

Value Value::listToSequence(list *values)
{
  list *l;
  Value result;
  if (NULL == values)
    return Value(SequenceType(SEQTYPE_EMPTY));

  for (l = values; l; l = l->next) {
    ValueImpl *v = (ValueImpl*)l->data;
    if (result.isNull()) {
      result = v;
    }
    else {
      SequenceType st = SequenceType::sequence(result.type(),v->st);
      result = new ValueImpl(st,result.impl,v);
    }
  }
  return result;
}

Value Value::listToSequence2(List<Value> &values)
{
  Value result;
  if (values.isEmpty()) {
    result = new ValueImpl(SequenceType(SEQTYPE_EMPTY));
    return result;
  }

  for (Iterator<Value> it = values; it.haveCurrent(); it++) {
    Value v = *it;
    if (result.isNull()) {
      result = v;
    }
    else {
      SequenceType st = SequenceType::sequence(result.type(),v.type());
      result = Value(st,result.impl,v.impl);
    }
  }
  return result;
}

float GridXSLT::xpathroundf(float f)
{
  if (0.5 == (fabsf(f) - floorf(fabs(f))))
    return ceilf(f);
  else
    return roundf(f);
}

double GridXSLT::xpathround(double d)
{
  if (0.5 == (fabs(d) - floor(fabs(d))))
    return ceil(d);
  else
    return round(d);
}

