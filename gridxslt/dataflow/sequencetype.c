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

#include "sequencetype.h"
#include "dataflow.h"
#include "util/macros.h"
#include "util/namespace.h"
#include "util/debug.h"
#include <libxml/xmlwriter.h>
#include <libxml/xmlIO.h>
#include <libxml/parser.h>
#include <libxml/tree.h>
#include <assert.h>
#include <string.h>
#include <ctype.h>

#define _DATAFLOW_SEQUENCETYPE_C

const char *df_item_kinds[ITEM_COUNT] = {
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

static list *allocnodes = NULL;
static list *allocvalues = NULL;

df_itemtype *df_itemtype_new(int kind)
{
  df_itemtype *st = (df_itemtype*)calloc(1,sizeof(df_itemtype));
  st->kind = kind;
  return st;
}

df_seqtype *df_seqtype_new(int type)
{
  df_seqtype *st = (df_seqtype*)calloc(1,sizeof(df_seqtype));
  st->type = type;
  st->refcount = 1;
  return st;
}

df_seqtype *df_seqtype_new_item(int kind)
{
  df_seqtype *st = (df_seqtype*)calloc(1,sizeof(df_seqtype));
  st->type = SEQTYPE_ITEM;
  st->item = df_itemtype_new(kind);
  st->refcount = 1;
  return st;
}

df_seqtype *df_seqtype_new_atomic(xs_type *type)
{
  df_seqtype *st = df_seqtype_new_item(ITEM_ATOMIC);
  st->item->type = type;
  return st;
}

df_seqtype *df_seqtype_ref(df_seqtype *st)
{
  st->refcount++;
  return st;
}

void df_itemtype_free(df_itemtype *it)
{
  if (NULL != it->pistr)
    free(it->pistr);
  if (NULL != it->content)
    df_seqtype_deref(it->content);
  qname_free(it->typeref);
  qname_free(it->elemref);
  qname_free(it->attrref);
  qname_free(it->localname);
  free(it);
}

void df_seqtype_deref(df_seqtype *st)
{
  st->refcount--;
  assert(0 <= st->refcount);
  if (0 == st->refcount) {
    if (NULL != st->item)
      df_itemtype_free(st->item);
    if (NULL != st->left)
      df_seqtype_deref(st->left);
    if (NULL != st->right)
      df_seqtype_deref(st->right);
    free(st);
  }
}

df_seqtype *df_add_alternative(df_seqtype *st1, df_seqtype *st2)
{
  df_seqtype *options = (df_seqtype*)calloc(1,sizeof(df_seqtype));
  options->type = SEQTYPE_CHOICE;
  options->left = st1;
  options->right = st2;
  options->refcount = 1;
  return options;
}

df_seqtype *df_normalize_itemnode(int item, xs_globals *g)
{
  df_seqtype *i1 = df_seqtype_new_item(ITEM_ELEMENT);
  df_seqtype *i2 = df_seqtype_new_item(ITEM_ATTRIBUTE);
  df_seqtype *i3 = df_seqtype_new_item(ITEM_TEXT);
  df_seqtype *i4 = df_seqtype_new_item(ITEM_DOCUMENT);
  df_seqtype *i5 = df_seqtype_new_item(ITEM_COMMENT);
  df_seqtype *i6 = df_seqtype_new_item(ITEM_PI);

  df_seqtype *choice = df_add_alternative(i1,i2);
  choice = df_add_alternative(choice,i3);
  choice = df_add_alternative(choice,i4);
  choice = df_add_alternative(choice,i5);
  choice = df_add_alternative(choice,i6);

  if (item) {
    df_seqtype *i7 = df_seqtype_new_item(ITEM_ATOMIC);
    /* FIXME: don't use xdt prefix here; it may not actually be mapped in the current context, or
       maybe mapped to a different namespace than the one we expect */
    i7->item->typeref.prefix = strdup("__xdt");
    i7->item->typeref.localpart = strdup("anyAtomicType");
    if (NULL != g)
      i7->item->type = g->any_atomic_type;
    choice = df_add_alternative(choice,i7);
  }

  if (item)
    choice->isitem = 1;
  else
    choice->isnode = 1;

  return choice;
}

df_seqtype *df_interleave_document_content(df_seqtype *content)
{
  df_seqtype *pi = df_seqtype_new_item(ITEM_PI);
  df_seqtype *comment = df_seqtype_new_item(ITEM_COMMENT);
  df_seqtype *choice = df_seqtype_new(SEQTYPE_CHOICE);
  df_seqtype *occurrence = df_seqtype_new(SEQTYPE_OCCURRENCE);
  df_seqtype *all = df_seqtype_new(SEQTYPE_ALL);
  choice->left = pi;
  choice->right = comment;
  occurrence->left = choice;
  occurrence->occurrence = OCCURS_ZERO_OR_MORE;
  all->left = content;
  all->right = occurrence;
  /* FIXME: formal semantics 3.5.4 says that text nodes should also be allowed, but the actual
     formal definition doesn't include this - what to do here? */
  return all;
}

static void df_print_objname(stringbuf *buf, char *name, char *ns, ns_map *namespaces)
{
  if (NULL != ns) {
    ns_def *def = ns_lookup_href(namespaces,ns);
    assert(NULL != def); /* FIXME: what to do if we can't find one? */
    stringbuf_format(buf,"%s:%s",def->prefix,name);
  }
  else {
    stringbuf_format(buf,"%s",name);
  }
}

static void df_itemtype_print_fs(stringbuf *buf, df_itemtype *it, ns_map *namespaces)
{
  switch (it->kind) {
  case ITEM_ATOMIC:
    df_print_objname(buf,it->type->def.ident.name,it->type->def.ident.ns,namespaces);
    break;
  case ITEM_DOCUMENT:
    stringbuf_format(buf,"document");
    if (NULL != it->content) {
      stringbuf_format(buf," { ");
      df_seqtype_print_fs(buf,it->content,namespaces);
      stringbuf_format(buf," }");
    }
    break;
  case ITEM_ELEMENT:
    stringbuf_format(buf,"element");


    if (NULL != it->elem) {
      stringbuf_format(buf," ");
      df_print_objname(buf,it->elem->def.ident.name,it->elem->def.ident.ns,namespaces);

      if (!it->elem->toplevel) {
        if (it->elem->nillable)
          stringbuf_format(buf," nillable");
        stringbuf_format(buf," of type ");
        df_print_objname(buf,it->elem->type->def.ident.name,it->elem->type->def.ident.ns,namespaces);
      }
    }
    else if (NULL != it->type) {
      if (it->nillable)
        stringbuf_format(buf," nillable");
      stringbuf_format(buf," of type ");
      df_print_objname(buf,it->type->def.ident.name,it->type->def.ident.ns,namespaces);
    }
    break;
  case ITEM_ATTRIBUTE:
    if (NULL != it->attr) {
      if (it->attr->toplevel) {
        stringbuf_format(buf,"attribute ");
        df_print_objname(buf,it->attr->def.ident.name,it->attr->def.ident.ns,namespaces);
      }
      else {
        stringbuf_format(buf,"attribute ");
        df_print_objname(buf,it->attr->def.ident.name,it->attr->def.ident.ns,namespaces);
        stringbuf_format(buf," of type ");
        df_print_objname(buf,it->attr->type->def.ident.name,it->attr->type->def.ident.ns,namespaces);
      }
    }
    else if (NULL != it->type) {
      stringbuf_format(buf,"attribute of type ");
      df_print_objname(buf,it->type->def.ident.name,it->type->def.ident.ns,namespaces);
    }
    else {
      stringbuf_format(buf,"attribute");
    }

    break;
  case ITEM_PI:
    stringbuf_format(buf,"processing-instruction");
    break;
  case ITEM_COMMENT:
    stringbuf_format(buf,"comment");
    break;
  case ITEM_TEXT:
    stringbuf_format(buf,"text");
    break;
  case ITEM_NAMESPACE:
    stringbuf_format(buf,"namespace"); /* FIXME: is this valid? */
    break;
  default:
    assert(!"invalid item type");
    break;
  }
}

void df_seqtype_print_fs(stringbuf *buf, df_seqtype *st, ns_map *namespaces)
{
  /* item() and node() are not actually part of the formal semantics type syntax - however since
     we currently use this output function for debugging purposes, it is more convenient to
     show the condensed form from XPath syntax rather than the full expansion */
  if (st->isitem) {
    stringbuf_format(buf,"item()");
    return;
  }

  if (st->isnode) {
    stringbuf_format(buf,"node()");
    return;
  }

  switch (st->type) {
  case SEQTYPE_ITEM:
    df_itemtype_print_fs(buf,st->item,namespaces);
    break;
  case SEQTYPE_OCCURRENCE:
    df_seqtype_print_fs(buf,st->left,namespaces);
    switch (st->occurrence) {
    case OCCURS_ONCE:
      break;
    case OCCURS_OPTIONAL:
      stringbuf_format(buf,"?");
      break;
    case OCCURS_ZERO_OR_MORE:
      stringbuf_format(buf,"*");
      break;
    case OCCURS_ONE_OR_MORE:
      stringbuf_format(buf,"+");
      break;
    default:
      assert(!"invalid occurrence");
      break;
    }
    break;
  case SEQTYPE_ALL:
    stringbuf_format(buf,"(");
    df_seqtype_print_fs(buf,st->left,namespaces);
    stringbuf_format(buf," & ");
    df_seqtype_print_fs(buf,st->right,namespaces);
    stringbuf_format(buf,")");
    break;
  case SEQTYPE_SEQUENCE:
    stringbuf_format(buf,"(");
    df_seqtype_print_fs(buf,st->left,namespaces);
    stringbuf_format(buf," , ");
    df_seqtype_print_fs(buf,st->right,namespaces);
    stringbuf_format(buf,")");
    break;
  case SEQTYPE_CHOICE:
    stringbuf_format(buf,"(");
    df_seqtype_print_fs(buf,st->left,namespaces);
    stringbuf_format(buf," | ");
    df_seqtype_print_fs(buf,st->right,namespaces);
    stringbuf_format(buf,")");
    break;
  case SEQTYPE_EMPTY:
    stringbuf_format(buf,"empty");
    break;
  default:
    assert(!"invalid sequence type");
    break;
  }
}


















static int is_ncname(char *str)
{
  if (!isalnum(*str) && ('_' != *str))
    return 0;

  for (str++; '\0' != *str; str++)
    if (!isalnum(*str) && ('.' != *str) && ('-' != *str) && ('_' != *str))
      return 0;

  return 1;
}

static void print_objname(stringbuf *buf, ns_map *namespaces, char *name, char *ns)
{
  if (NULL != ns) {
    ns_def *def = ns_lookup_href(namespaces,ns);
    assert(NULL != def); /* FIXME: what to do if we can't find one? */
    stringbuf_format(buf,"%s:%s",def->prefix,name);
  }
  else {
    stringbuf_format(buf,"%s",name);
  }
}

static void df_itemtype_print_xpath(stringbuf *buf, df_itemtype *it, ns_map *namespaces)
{
  switch (it->kind) {
  case ITEM_ATOMIC:
    print_objname(buf,namespaces,it->type->def.ident.name,it->type->def.ident.ns);
    break;
  case ITEM_DOCUMENT:
    stringbuf_format(buf,"document-node(");
    if (NULL != it->content) {
      assert(SEQTYPE_ALL == it->content->type);
      assert(SEQTYPE_ITEM == it->content->left->type);
      assert(ITEM_ELEMENT == it->content->left->item->kind);
      df_itemtype_print_xpath(buf,it->content->left->item,namespaces);
    }
    stringbuf_format(buf,")");
    break;
  case ITEM_ELEMENT:
    if (NULL != it->elem) {
      if (it->elem->toplevel) {
        stringbuf_format(buf,"schema-element(");
        print_objname(buf,namespaces,it->elem->def.ident.name,it->elem->def.ident.ns);
        stringbuf_format(buf,")");
      }
      else {
        stringbuf_format(buf,"element(");
        print_objname(buf,namespaces,it->elem->def.ident.name,it->elem->def.ident.ns);

        if (strcmp(it->elem->type->def.ident.ns,XS_NAMESPACE) ||
            strcmp(it->elem->type->def.ident.name,"anyType") ||
            !it->elem->nillable) {
          stringbuf_format(buf,",");
          print_objname(buf,namespaces,it->elem->type->def.ident.name,it->elem->type->def.ident.ns);
          if (it->elem->nillable)
            stringbuf_format(buf,"?");
        }
        stringbuf_format(buf,")");
      }

    }
    else if (NULL != it->type) {
      stringbuf_format(buf,"element(*,");
      print_objname(buf,namespaces,it->type->def.ident.name,it->type->def.ident.ns);
      if (it->nillable)
        stringbuf_format(buf,"?");
      stringbuf_format(buf,")");
    }
    else {
      stringbuf_format(buf,"element()");
    }
    break;
  case ITEM_ATTRIBUTE:
    if (NULL != it->attr) {
      if (it->attr->toplevel) {
        stringbuf_format(buf,"schema-attribute(");
        print_objname(buf,namespaces,it->attr->def.ident.name,it->attr->def.ident.ns);
        stringbuf_format(buf,")");
      }
      else {
        stringbuf_format(buf,"attribute(");
        print_objname(buf,namespaces,it->attr->def.ident.name,it->attr->def.ident.ns);

        if (strcmp(it->attr->type->def.ident.ns,XS_NAMESPACE) ||
            strcmp(it->attr->type->def.ident.name,"anySimpleType")) {
          stringbuf_format(buf,",");
          print_objname(buf,namespaces,it->attr->type->def.ident.name,it->attr->type->def.ident.ns);
        }
        stringbuf_format(buf,")");
      }
    }
    else if (NULL != it->type) {
      stringbuf_format(buf,"attribute(*,");
      print_objname(buf,namespaces,it->type->def.ident.name,it->type->def.ident.ns);
      stringbuf_format(buf,")");
    }
    else {
      stringbuf_format(buf,"attribute()");
    }
    break;
  case ITEM_PI:
    stringbuf_format(buf,"processing-instruction(");
    if (NULL != it->pistr) {
      if (is_ncname(it->pistr))
        stringbuf_format(buf,"%s",it->pistr);
      else
        stringbuf_format(buf,"\"%s\"",it->pistr);
    }
    stringbuf_format(buf,")");
    break;
  case ITEM_COMMENT:
    stringbuf_format(buf,"comment()");
    break;
  case ITEM_TEXT:
    stringbuf_format(buf,"text()");
    break;
  case ITEM_NAMESPACE:
    stringbuf_format(buf,"namespace()"); /* FIXME: is this valid? */
    break;
  default:
    assert(!"invalid item type");
    break;
  }
}

void df_seqtype_print_xpath(stringbuf *buf, df_seqtype *st, ns_map *namespaces)
{
  if (SEQTYPE_ITEM == st->type) {
    df_itemtype_print_xpath(buf,st->item,namespaces);
  }
  else if (SEQTYPE_OCCURRENCE == st->type) {
    assert(SEQTYPE_ITEM == st->left->type);
    if ((ITEM_PI == st->left->item->kind) &&
        (OCCURS_OPTIONAL == st->occurrence) &&
        (NULL != st->left->item->pistr)) {
      df_seqtype_print_xpath(buf,st->left,namespaces);
    }
    else {
      df_seqtype_print_xpath(buf,st->left,namespaces);
      switch (st->occurrence) {
      case OCCURS_ONCE:
        break;
      case OCCURS_OPTIONAL:
        stringbuf_format(buf,"?");
        break;
      case OCCURS_ZERO_OR_MORE:
        stringbuf_format(buf,"*");
        break;
      case OCCURS_ONE_OR_MORE:
        stringbuf_format(buf,"+");
        break;
      default:
        assert(!"invalid occurrence");
        break;
      }
    }
  }
  else if (SEQTYPE_EMPTY == st->type) {
    stringbuf_format(buf,"void()");
  }
  else if (st->isnode) {
    stringbuf_format(buf,"node()");
  }
  else if (st->isitem) {
    stringbuf_format(buf,"item()");
  }
  else if (SEQTYPE_CHOICE == st->type) {
    /* FIXME: temp - just so we cab print the select nodes of the built-in templates */
    df_seqtype_print_xpath(buf,st->left,namespaces);
    stringbuf_format(buf," or ");
    df_seqtype_print_xpath(buf,st->right,namespaces);
  }
  else {
    assert(!"sequencetype not expressible in xpath syntax");
  }
}


















int df_seqtype_compatible(df_seqtype *from, df_seqtype *to)
{
  return 0;
}

static int resolve_object(qname qn, ns_map *namespaces, xs_schema *s, const char *filename,
                          int line,error_info *ei, void **obj, int type)
{
  ns_def *def = NULL;
  char *ns = NULL;

  if (NULL == qn.localpart)
    return 0;

/*   debugl("Resolving %s:%s",qn.prefix,qn.localpart); */

  if ((NULL != qn.prefix) && !strcmp(qn.prefix,"__xdt")) {
    ns = XDT_NAMESPACE;
  }
  else {
    if ((NULL != qn.prefix) &&
        (NULL == (def = ns_lookup_prefix(namespaces,qn.prefix))))
      return error(ei,filename,line,NULL,"Invalid namespace prefix: %s",qn.prefix);

    ns = def ? def->href : NULL;
  }

  if (NULL == (*obj = xs_lookup_object(s,type,nsname_temp(ns,qn.localpart)))) {
    if (NULL != qn.prefix)
      return error(ei,filename,line,NULL,"No such %s: %s:%s",
                   xs_object_types[type],qn.prefix,qn.localpart);
    else
      return error(ei,filename,line,NULL,"No such %s: %s",
                   xs_object_types[type],qn.localpart);
  }

  return 0;
}

int df_seqtype_resolve(df_seqtype *st, ns_map *namespaces, xs_schema *s, const char *filename,
                        int line, error_info *ei)
{
  if (NULL != st->left)
    CHECK_CALL(df_seqtype_resolve(st->left,namespaces,s,filename,line,ei))
  if (NULL != st->right)
    CHECK_CALL(df_seqtype_resolve(st->right,namespaces,s,filename,line,ei))

  if (NULL != st->item) {
    ns_def *def = NULL;
    df_itemtype *i = st->item;
    char *ns = NULL;
    CHECK_CALL(resolve_object(i->typeref,namespaces,s,filename,line,ei,
               (void**)&i->type,XS_OBJECT_TYPE))
    CHECK_CALL(resolve_object(i->elemref,namespaces,s,filename,line,ei,
               (void**)&i->elem,XS_OBJECT_ELEMENT))
    CHECK_CALL(resolve_object(i->attrref,namespaces,s,filename,line,ei,
               (void**)&i->attr,XS_OBJECT_ATTRIBUTE))

    if ((NULL != st->item->localname.prefix) &&
        (NULL == (def = ns_lookup_prefix(namespaces,st->item->localname.prefix))))
      return error(ei,filename,line,NULL,"Invalid namespace prefix: %s",st->item->localname.prefix);

    ns = def ? def->href : NULL;

    if ((ITEM_ELEMENT == st->item->kind) && (NULL != st->item->localname.localpart)) {
      st->item->elem = xs_element_new(s->as);
      st->item->elem->def.ident = nsname_new(ns,st->item->localname.localpart);
      st->item->elem->type = i->type;
      i->type = NULL;
      st->item->elem->nillable = i->nillable;
      st->item->elem->computed_type = 1;

      if (NULL == st->item->elem->type) {
        st->item->elem->type = s->globals->complex_ur_type;
        st->item->elem->nillable = 1;
        i->nillable = 1;
      }
    }
    else if ((ITEM_ATTRIBUTE == st->item->kind) && (NULL != st->item->localname.localpart)) {
      st->item->attr = xs_attribute_new(s->as);
      st->item->attr->def.ident = nsname_new(ns,st->item->localname.localpart);
      st->item->attr->type = i->type;
      i->type = NULL;

      if (NULL == st->item->attr->type)
        st->item->attr->type = s->globals->simple_ur_type;
    }
    else if ((ITEM_DOCUMENT == st->item->kind) && (NULL != st->item->content)) {
      CHECK_CALL(df_seqtype_resolve(st->item->content,namespaces,s,filename,line,ei))
    }
  }

  return 0;
}

void df_seqtype_to_list(df_seqtype *st, list **types)
{
  if (SEQTYPE_SEQUENCE == st->type) {
    df_seqtype_to_list(st->left,types);
    df_seqtype_to_list(st->right,types);
  }
  else if (SEQTYPE_ITEM == st->type) {
    list_append(types,st);
  }
  else {
    assert(SEQTYPE_EMPTY == st->type);
  }
}

static int validate_sequence_item(df_seqtype *base, df_seqtype *st)
{
  if (base->item->kind != st->item->kind)
    return 0;

  switch (base->item->kind) {
  case ITEM_ATOMIC: {
    /* FIXME: handle union and list simple types */
    xs_type *basetype = base->item->type;
    xs_type *derived = st->item->type;
    while (1) {
      if (basetype == derived)
        return 1;
      if (derived->base == derived)
        return 0;
      derived = derived->base;
    }
    break;
  }
  case ITEM_DOCUMENT:
    /* FIXME: check further details of type */
    return 1;
  case ITEM_ELEMENT:
    /* FIXME: check further details of type */
    return 1;
  case ITEM_ATTRIBUTE:
    /* FIXME: check further details of type */
    return 1;
  case ITEM_PI:
    return 1;
  case ITEM_COMMENT:
    return 1;
  case ITEM_TEXT:
    return 1;
  case ITEM_NAMESPACE:
    return 1;
  }

  assert(!"invalid item kind");
  return 0;
}

static int validate_sequence_list(df_seqtype *base, list **lptr)
{
  switch (base->type) {
  case SEQTYPE_ITEM:
    if (NULL == *lptr)
      return 0;
    assert(SEQTYPE_ITEM == ((df_seqtype*)((*lptr)->data))->type);
    if (validate_sequence_item(base,(df_seqtype*)((*lptr)->data))) {
      *lptr = (*lptr)->next;
      return 1;
    }
    return 0;
  case SEQTYPE_OCCURRENCE: {
    list *backup = *lptr;
    switch (base->occurrence) {
    case OCCURS_ONCE:
      return validate_sequence_list(base->left,lptr);
    case OCCURS_OPTIONAL:
      if (validate_sequence_list(base->left,lptr))
        return 1;
      *lptr = backup;
      return 1;
    case OCCURS_ZERO_OR_MORE:
      while (validate_sequence_list(base->left,lptr))
        backup = *lptr;
      *lptr = backup;
      return 1;
    case OCCURS_ONE_OR_MORE:
      if (!validate_sequence_list(base->left,lptr))
        return 0;
      backup = *lptr;
      while (validate_sequence_list(base->left,lptr))
        backup = *lptr;
      *lptr = backup;
      return 1;
    }
    assert(!"invalid occurrence type");
    break;
  }
  case SEQTYPE_ALL:
    assert(!"SEQTYPE_ALL not yet supported");
    break;
  case SEQTYPE_SEQUENCE:
    return (validate_sequence_list(base->left,lptr) &&
            validate_sequence_list(base->right,lptr));
  case SEQTYPE_CHOICE: {
    list *backup = *lptr;
    if (validate_sequence_list(base->left,lptr))
      return 1;
    *lptr = backup;
    return validate_sequence_list(base->right,lptr);
  }
  case SEQTYPE_EMPTY:
    assert(!"EMPTY is not a valid specifier type");
    break;
  }
  assert(!"invalid sequence type");
  return 0;
}

int df_seqtype_derived(df_seqtype *base, df_seqtype *st)
{
  list *types = NULL;
  int r;
  df_seqtype_to_list(st,&types);
  r = validate_sequence_list(base,&types);
  list_free(types,NULL);
  return r;
}

df_node *df_node_new(int type)
{
  df_node *n = (df_node*)calloc(1,sizeof(df_node));
/*   debugl("df_node_new %p %s",n,df_item_kinds[type]); */
  list_append(&allocnodes,n);
  n->type = type;
  n->refcount = 0;
  return n;
}

df_node *df_node_root(df_node *n)
{
  df_node *root = n;
  while (NULL != root->parent)
    root = root->parent;
  return root;
}

df_node *df_node_ref(df_node *n)
{
  df_node_root(n)->refcount++;
  return n;
}

void df_print_remaining(xs_globals *globals)
{
  list *l;
  for (l = allocnodes; l; l = l->next) {
    df_node *n = (df_node*)l->data;
    if (NODE_ELEMENT == n->type)
      printf("remaining node %p %-10s %-10s - %-3d refs\n",n,"element",n->ident.name,n->refcount);
    else if (NODE_TEXT == n->type)
      printf("remaining node %p %-10s %-10s - %-3d refs\n",n,"text",n->value,n->refcount);
    else
      printf("remaining node %p %-10s %-10s - %-3d refs\n",n,df_item_kinds[n->type],"",n->refcount);
  }
  for (l = allocvalues; l; l = l->next) {
    df_value *v = (df_value*)l->data;
    stringbuf *stbuf = stringbuf_new();
    stringbuf *valbuf = stringbuf_new();
    df_seqtype_print_fs(stbuf,v->seqtype,globals->namespaces);
    if ((SEQTYPE_ITEM == v->seqtype->type) &&
        (ITEM_ATOMIC == v->seqtype->item->kind))
      df_value_printbuf(globals,valbuf,v);
    printf("remaining value %p %-20s - %-3d refs : %s\n",v,stbuf->data,v->refcount,valbuf->data);
    stringbuf_free(stbuf);
    stringbuf_free(valbuf);
  }
}

void df_node_free(df_node *n)
{
  df_node *c;
/*   if (NODE_ELEMENT == n->type) */
/*     debugl("df_node_free %p element %s",n,n->name); */
/*   else */
/*     debugl("df_node_free %p %s",n,df_item_kinds[n->type]); */

  list_remove_ptr(&allocnodes,n);

  list_free(n->namespaces,(void*)df_node_free);
  list_free(n->attributes,(void*)df_node_free);

  c = n->first_child;
  while (c) {
    df_node *next = c->next;
    df_node_free(c);
    c = next;
  }

  free(n->prefix);
  nsname_free(n->ident);
  free(n->target);
  free(n->value);
  free(n);
}

void df_node_deref(df_node *n)
{
  df_node *root = df_node_root(n);
  root->refcount--;

/*   debugl("df_node_deref %s %p (root is %s %p), now have %d refs", */
/*         df_item_kinds[n->type],n,df_item_kinds[root->type],root,root->refcount); */

  assert(0 <= root->refcount);
  if (0 == root->refcount)
    df_node_free(root);
}

df_node *df_node_deep_copy(df_node *n)
{
  df_node *copy = df_node_new(n->type);
  df_node *c;
  list *l;
/*   debugl("df_node_deep_copy %s %p -> %p",df_item_kinds[n->type],n,copy); */

  /* FIXME: namespaces */

  for (l = n->attributes; l; l = l->next) {
    df_node *attr = (df_node*)l->data;
    df_node *attrcopy = df_node_deep_copy(attr);
    df_node_add_attribute(copy,attrcopy);
  }

  for (l = n->namespaces; l; l = l->next) {
    df_node *ns = (df_node*)l->data;
    df_node *nscopy = df_node_deep_copy(ns);
    df_node_add_namespace(copy,nscopy);
  }

  copy->prefix = n->prefix ? strdup(n->prefix) : NULL;
  copy->ident = nsname_copy(n->ident);
  copy->target = n->target ? strdup(n->target) : NULL;
  copy->value = n->value ? strdup(n->value) : NULL;

  for (c = n->first_child; c; c = c->next) {
    df_node *childcopy = df_node_deep_copy(c);
    df_node_add_child(copy,childcopy);
  }

  return copy;
}

void df_node_add_child(df_node *n, df_node *c)
{
  assert(NULL == c->parent);
  assert(NULL == c->prev);
  assert(NULL == c->next);
  assert(0 == c->refcount);

  if (NULL != n->last_child)
    n->last_child->next = n;

  c->prev = n->last_child;
  if (NULL == n->first_child) {
    n->first_child = c;
    n->last_child = c;
  }
  else {
    n->last_child->next = c;
    n->last_child = c;
  }
  c->parent = n;
}

void df_node_insert_child(df_node *n, df_node *c, df_node *before)
{
  assert(NULL == c->parent);
  assert(NULL == c->prev);
  assert(NULL == c->next);
  assert(0 == c->refcount);

  if (NULL == before) {
    df_node_add_child(n,c);
    return;
  }

  c->prev = before->prev;
  before->prev = c;
  c->next = before;

  if (NULL != c->prev)
    c->prev->next = c;

  if (n->first_child == before)
    n->first_child = c;

  c->parent = n;
}

void df_node_add_attribute(df_node *n, df_node *attr)
{
  assert(NULL == attr->parent);
  assert(NULL == attr->prev);
  assert(NULL == attr->next);
  assert(0 == attr->refcount);

  list_append(&n->attributes,attr);
  attr->parent = n;
}

void df_node_add_namespace(df_node *n, df_node *ns)
{
  assert(NULL == ns->parent);
  assert(NULL == ns->prev);
  assert(NULL == ns->next);
  assert(0 == ns->refcount);

  list_append(&n->namespaces,ns);
  ns->parent = n;
}

df_node *df_atomic_value_to_text_node(xs_globals *g, df_value *v)
{
  df_node *text = df_node_new(NODE_TEXT);
  stringbuf *buf = stringbuf_new();
  assert(SEQTYPE_ITEM == v->seqtype->type);
  assert(ITEM_ATOMIC == v->seqtype->item->kind);
  df_value_printbuf(g,buf,v);
  /* FIXME: properly join atomic values
     see rule 3 of 5.7.1 Constructing Complex Content */
  stringbuf_format(buf," ");
  text->value = strdup(buf->data);
  stringbuf_free(buf);
  return text;
}

df_node *df_node_from_xmlnode(xmlNodePtr xn)
{
  df_node *n = NULL;
  xmlNodePtr c;
  struct _xmlAttr *aptr;

  switch (xn->type) {
  case XML_ELEMENT_NODE: {
    xmlNs *ns;
    xmlNodePtr p;
    n = df_node_new(NODE_ELEMENT);
    n->ident.name = strdup(xn->name);
    /* FIXME: prefix/namespace */

    for (aptr = xn->properties; aptr; aptr = aptr->next) {
      df_node *attr = df_node_new(NODE_ATTRIBUTE);
      stringbuf *valbuf = stringbuf_new();
      xmlNodePtr ac;
      attr->ident.name = strdup(aptr->name);

      for (ac = aptr->children; ac; ac = ac->next) {
        if (NULL != ac->content)
          stringbuf_format(valbuf,"%s",ac->content);
      }

      attr->value = strdup(valbuf->data);
      stringbuf_free(valbuf);
      df_node_add_attribute(n,attr);
    }


    for (p = xn; p; p = p->parent) {
      for (ns = p->nsDef; ns; ns = ns->next) {
        df_node *nsnode = df_node_new(NODE_NAMESPACE);
        nsnode->prefix = ns->prefix ? strdup(ns->prefix) : NULL;
        nsnode->value = strdup(ns->href);
        df_node_add_namespace(n,nsnode);
      }
    }

    break;
  }
  case XML_ATTRIBUTE_NODE:
    /* Should never see this in the main element tree */
    assert(0);
    break;
  case XML_TEXT_NODE:
    n = df_node_new(NODE_TEXT);
    n->value = strdup(xn->content);
    break;
  case XML_PI_NODE:
    n = df_node_new(NODE_PI);
    n->target = strdup(xn->name);
    n->value = xn->content ? strdup(xn->content) : NULL;
    break;
  case XML_COMMENT_NODE:
    n = df_node_new(NODE_COMMENT);
    /* FIXME: do we know that xn->content will always be non-NULL here? */
    n->value = strdup(xn->content);
    break;
  case XML_DOCUMENT_NODE:
    n = df_node_new(NODE_DOCUMENT);
    break;
  default:
    /* FIXME: support other node types such as CDATA sections and entities */
    assert(!"node type not supported");
  }

  for (c = xn->children; c; c = c->next) {
    df_node *cn = df_node_from_xmlnode(c);
    if (NULL != cn)
      df_node_add_child(n,cn);
  }

  return n;
}

int df_check_tree(df_node *n)
{
  df_node *c;
  df_node *prev = NULL;
  for (c = n->first_child; c; c = c->next) {
    if (c->prev != prev)
      return 0;
    if ((NULL == c->next) && (c != n->last_child))
      return 0;
    if (c->parent != n)
      return 0;
    if (!df_check_tree(c))
      return 0;
    prev = c;
  }
  return 1;
}

df_node *df_prev_node(df_node *node, df_node *subtree)
{
  if (NULL != node->prev) {
    node = node->prev;
    while (NULL != node->last_child)
      node = node->last_child;
    return node;
  }

  if (node->parent == subtree)
    return NULL;
  else
    return node->parent;
}

df_node *df_next_node(df_node *node, df_node *subtree)
{
  if (NULL != node->first_child)
    return node->first_child;

  while ((NULL != node) && (NULL == node->next)) {
    if (node->parent == subtree)
      return NULL;
    else
      node = node->parent;
  }

  if (NULL != node)
    return node->next;

  return NULL;
}

df_value *df_value_new_atomic(xs_type *t)
{
  df_seqtype *st = df_seqtype_new_atomic(t);
  df_value *v;
  v = df_value_new(st);
  df_seqtype_deref(st);
  return v;
}


df_value *df_value_new_int(xs_globals *g, int i)
{
  df_value *v = df_value_new_atomic(g->int_type);
  v->value.i = i;
  return v;
}

df_value *df_value_new_float(xs_globals *g, float f)
{
  df_value *v = df_value_new_atomic(g->float_type);
  v->value.f = f;
  return v;
}

df_value *df_value_new_double(xs_globals *g, double d)
{
  df_value *v = df_value_new_atomic(g->double_type);
  v->value.d = d;
  return v;
}

df_value *df_value_new_string(xs_globals *g, const char *str)
{
  df_value *v = df_value_new_atomic(g->string_type);
  v->value.s = strdup(str);
  return v;
}

df_value *df_value_new_bool(xs_globals *g, int b)
{
  df_value *v = df_value_new_atomic(g->boolean_type);
  v->value.b = b;
  return v;
}

df_value *df_value_new_node(df_node *n)
{
  df_seqtype *st = df_seqtype_new_item(n->type);
  df_value *v = df_value_new(st);
  df_seqtype_deref(st);
  v->value.n = df_node_ref(n);
  return v;
}

df_value *df_value_new(df_seqtype *seqtype)
{
  df_value *v = (df_value*)calloc(1,sizeof(df_value));
  list_append(&allocvalues,v);
  v->seqtype = df_seqtype_ref(seqtype);
  v->refcount = 1;
  return v;
}

df_value *df_value_ref(df_value *v)
{
  v->refcount++;
  return v;
}

void df_node_print(xmlTextWriter *writer, df_node *n)
{
  switch (n->type) {
  case NODE_DOCUMENT: {
    df_node *c;
    for (c = n->first_child; c; c = c->next)
      df_node_print(writer,c);
    break;
  }
  case NODE_ELEMENT: {
    list *l;
    df_node *c;
/*     debugl("df_node_print NODE_ELEMENT: %s",n->name); */
    xmlTextWriterStartElement(writer,n->ident.name);
    for (l = n->attributes; l; l = l->next)
      df_node_print(writer,(df_node*)l->data);
    for (l = n->namespaces; l; l = l->next)
      df_node_print(writer,(df_node*)l->data);
    for (c = n->first_child; c; c = c->next)
      df_node_print(writer,c);
    xmlTextWriterEndElement(writer);
    break;
  }
  case NODE_ATTRIBUTE:
    xmlTextWriterWriteAttribute(writer,n->ident.name,n->value);
    break;
  case NODE_PI:
    xmlTextWriterWritePI(writer,n->target,n->value);
    break;
  case NODE_COMMENT:
    xmlTextWriterWriteComment(writer,n->value);
    break;
  case NODE_TEXT:
/*     debugl("df_node_print NODE_TEXT: %s",n->value); */
    xmlTextWriterWriteString(writer,n->value);
    break;
  case NODE_NAMESPACE: {
    df_node *elem = n->parent;
    df_node *p;
    int have = 0;

    /* Only print it if it's not already in scope */
    if (NULL != elem) {
      for (p = elem->parent; p && !have; p = p->parent) {
        list *l;
        for (l = p->namespaces; l; l = l->next) {
          df_node *pns = (df_node*)l->data;
          if (nullstr_equals(pns->prefix,n->prefix) && !strcmp(pns->value,n->value)) {
/*             debug("Namespace mapping %s=%s already exists on parent node %#n of %#n\n", */
/*                   n->prefix,n->value,p->ident,n->ident); */
            have = 1;
          }
        }
      }
    }

    if (!have) {
      if (NULL == n->prefix) {
        xmlTextWriterWriteAttribute(writer,"xmlns",n->value);
      }
      else {
        char *attrname = (char*)alloca(strlen("xmlns:")+strlen(n->prefix)+1);
        sprintf(attrname,"xmlns:%s",n->prefix);
        xmlTextWriterWriteAttribute(writer,attrname,n->value);
      }
    }
    break;
  }
  default:
    assert(!"invalid node type");
    break;
  }
}

static int xmlwrite_stringbuf(void *context, const char * buffer, int len)
{
  stringbuf_append((stringbuf*)context,buffer,len);
  return len;
}

void df_value_printbuf(xs_globals *globals, stringbuf *buf, df_value *v)
{
  if (SEQTYPE_ITEM == v->seqtype->type) {

    if (ITEM_ATOMIC == v->seqtype->item->kind) {
      if (v->seqtype->item->type == globals->int_type)
        stringbuf_format(buf,"%d",v->value.i);
      else if (v->seqtype->item->type == globals->double_type)
        stringbuf_format(buf,"%f",v->value.d);
      else if (v->seqtype->item->type == globals->decimal_type)
        stringbuf_format(buf,"%f",v->value.d);
      else if (v->seqtype->item->type == globals->string_type)
        stringbuf_format(buf,"%s",v->value.s);
      else if (v->seqtype->item->type == globals->boolean_type)
        stringbuf_format(buf,"%s",v->value.b ? "true" : "false");
      else
        stringbuf_format(buf,"(atomic value)");
    }
    else if ((ITEM_ELEMENT == v->seqtype->item->kind) ||
             (ITEM_DOCUMENT == v->seqtype->item->kind)) {
      xmlOutputBuffer *xb = xmlOutputBufferCreateIO(xmlwrite_stringbuf,NULL,buf,NULL);
      xmlTextWriter *writer = xmlNewTextWriter(xb);
      xmlTextWriterSetIndent(writer,1);
      xmlTextWriterSetIndentString(writer,"  ");
      xmlTextWriterStartDocument(writer,NULL,NULL,NULL);

      df_node_print(writer,v->value.n);

      xmlTextWriterEndDocument(writer);
      xmlTextWriterFlush(writer);
      xmlFreeTextWriter(writer);
    }
    else if ((ITEM_TEXT == v->seqtype->item->kind) ||
             (ITEM_ATTRIBUTE == v->seqtype->item->kind)) {
      stringbuf_format(buf,"%s",v->value.n->value);
    }
    else {
      /* FIXME */
      stringbuf_format(buf,"(item, kind %d)",v->seqtype->item->kind);
    }
  }
  else if (SEQTYPE_SEQUENCE == v->seqtype->type) {
    df_value_printbuf(globals,buf,v->value.pair.left);
    stringbuf_format(buf,", ");
    df_value_printbuf(globals,buf,v->value.pair.right);
  }
  else if (SEQTYPE_EMPTY == v->seqtype->type) {
    stringbuf_format(buf,"(empty)",v->seqtype->type);
  }
  else {
    stringbuf_format(buf,"(value, seqtype %d)",v->seqtype->type);
  }
}

void df_value_print(xs_globals *globals, FILE *f, df_value *v)
{
  stringbuf *buf = stringbuf_new();
  df_value_printbuf(globals,buf,v);
  fprintf(f,"%s",buf->data);
  stringbuf_free(buf);
}

void df_value_deref(xs_globals *globals, df_value *v)
{
  v->refcount--;
  if (0 > v->refcount) {
    fprintf(stderr,"WARNING: too many derefs!\n");
  }
  assert(0 <= v->refcount);
  if (0 == v->refcount) {
    if (SEQTYPE_ITEM == v->seqtype->type) {
      if (ITEM_ATOMIC == v->seqtype->item->kind) {
        if (v->seqtype->item->type == globals->string_type)
          free(v->value.s);
      }
      else if (NULL != v->value.n) {
        df_node_deref(v->value.n);
      }
    }
    else if (SEQTYPE_SEQUENCE == v->seqtype->type) {
      df_value_deref(globals,v->value.pair.left);
      df_value_deref(globals,v->value.pair.right);
    }
    df_seqtype_deref(v->seqtype);
    free(v);
    list_remove_ptr(&allocvalues,v);
  }
}

void df_value_deref_list(xs_globals *globals, list *l)
{
  for (; l; l = l->next)
    df_value_deref(globals,(df_value*)l->data);
}

int df_value_equals(df_value *a, df_value *b)
{
  return 0;
}

static int df_count_sequence_values(df_value *seq)
{
  if (SEQTYPE_SEQUENCE == seq->seqtype->type) {
    return df_count_sequence_values(seq->value.pair.left) +
           df_count_sequence_values(seq->value.pair.right);
  }
  else if (SEQTYPE_ITEM == seq->seqtype->type) {
    return 1;
  }
  else {
    assert(SEQTYPE_EMPTY == seq->seqtype->type);
    return 0;
  }
}

static void df_build_sequence_array(df_value *seq, df_value ***valptr)
{
  if (SEQTYPE_SEQUENCE == seq->seqtype->type) {
    df_build_sequence_array(seq->value.pair.left,valptr);
    df_build_sequence_array(seq->value.pair.right,valptr);
  }
  else if (SEQTYPE_ITEM == seq->seqtype->type) {
    *((*valptr)++) = seq;
  }
}

df_value **df_sequence_to_array(df_value *seq)
{
  int count = df_count_sequence_values(seq);
  df_value **valptr;
  df_value **values = (df_value**)malloc((count+1)*sizeof(df_value*));
  valptr = values;
  df_build_sequence_array(seq,&valptr);
  assert(valptr == values+count);
  values[count] = NULL;
  return values;
}

void df_get_sequence_values(df_value *v, list **values)
{
  if (SEQTYPE_SEQUENCE == v->seqtype->type) {
    df_get_sequence_values(v->value.pair.left,values);
    df_get_sequence_values(v->value.pair.right,values);
  }
  else if (SEQTYPE_ITEM == v->seqtype->type) {
    list_append(values,v);
  }
  else {
    assert(SEQTYPE_EMPTY == v->seqtype->type);
  }
}

list *df_sequence_to_list(df_value *seq)
{
  list *l = NULL;
  df_get_sequence_values(seq,&l);
  return l;
}

df_value *df_list_to_sequence(xs_globals *g, list *values)
{
  list *l;
  df_value *result = NULL;
  if (NULL == values) {
    df_seqtype *emptytype = df_seqtype_new(SEQTYPE_EMPTY);
    result = df_value_new(emptytype);
    df_seqtype_deref(emptytype);
    return result;
  }

  for (l = values; l; l = l->next) {
    df_value *v = (df_value*)l->data;
    if (NULL == result) {
      result = df_value_ref(v);
    }
    else {
      df_seqtype *st = df_seqtype_new(SEQTYPE_SEQUENCE);
      df_value *newresult = df_value_new(st);
      df_seqtype_deref(st);
      st->left = df_seqtype_ref(result->seqtype);
      st->right = df_seqtype_ref(v->seqtype);
      newresult->value.pair.left = result;
      newresult->value.pair.right = df_value_ref(v);
      result = newresult;
    }
  }
  return result;
}

void df_node_to_string(df_node *n, stringbuf *buf)
{
  switch (n->type) {
  case NODE_DOCUMENT: {
    df_node *c;
    for (c = n->first_child; c; c = c->next)
      df_node_to_string(c,buf);
    break;
  }
  case NODE_ELEMENT: {
    df_node *c;
    for (c = n->first_child; c; c = c->next)
      df_node_to_string(c,buf);
    break;
  }
  case NODE_ATTRIBUTE:
    stringbuf_format(buf,n->value);
    break;
  case NODE_PI:
    /* FIXME */
    break;
  case NODE_COMMENT:
    /* FIXME */
    break;
  case NODE_TEXT:
    stringbuf_format(buf,n->value);
    break;
  default:
    assert(!"invalid node type");
    break;
  }
}

char *df_value_as_string(xs_globals *g, df_value *v)
{
  df_value *atomicv;
  char *str;
  stringbuf *buf;

  /* FIXME: implement using df_cast() */

  if (SEQTYPE_EMPTY == v->seqtype->type) {
    return strdup("");
  }

  if ((SEQTYPE_ITEM != v->seqtype->type) ||
      (ITEM_ATOMIC != v->seqtype->item->kind))
    atomicv = df_atomize(g,v);
  else
    atomicv = df_value_ref(v);

  buf = stringbuf_new();
  df_value_printbuf(g,buf,atomicv);
  str = strdup(buf->data);
  df_value_deref(g,atomicv);
  stringbuf_free(buf);

  return str;
}

df_value *df_atomize(xs_globals *g, df_value *v)
{
  if (SEQTYPE_SEQUENCE == v->seqtype->type) {
    df_seqtype *atomicseq;
    df_value *atom;
    df_value *left = df_atomize(g,v->value.pair.left);
    df_value *right = df_atomize(g,v->value.pair.right);

    atomicseq = df_seqtype_new(SEQTYPE_SEQUENCE);
    atomicseq->left = df_seqtype_ref(left->seqtype);
    atomicseq->right = df_seqtype_ref(right->seqtype);

    atom = df_value_new(atomicseq);
    atom->value.pair.left = left;
    atom->value.pair.right = right;
    return atom;
  }
  else if (SEQTYPE_ITEM == v->seqtype->type) {
    if (ITEM_ATOMIC == v->seqtype->item->kind) {
      return df_value_ref(v);
    }
    else {
      /* FIXME: this is just a quick and dirty implementation of node atomization... need to
         follow the rules set out in XPath 2.0 section 2.5.2 */
      df_seqtype *strtype = df_seqtype_new_atomic(g->string_type);
      df_value *atom = df_value_new(strtype);
      stringbuf *buf = stringbuf_new();
      df_node_to_string(v->value.n,buf);
      atom->value.s = strdup(buf->data);
      stringbuf_free(buf);
      return atom;
    }
  }
  else {
    assert(SEQTYPE_EMPTY == v->seqtype->type);
    /* FIXME: is this safe? are there any situations in which df_atomize could be called with
       an empty sequence? */
    assert(!"can't atomize an empty sequence");
    return NULL;
  }
}

df_value *df_cast(xs_globals *g, df_value *v, df_seqtype *as)
{
  /* FIXME */
  return NULL;
}

