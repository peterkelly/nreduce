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
 */

#include "sequencetype.h"
#include "dataflow.h"
#include "funops.h"
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
  free(it->typeref.prefix);
  free(it->typeref.localpart);
  free(it->elemref.prefix);
  free(it->elemref.localpart);
  free(it->attrref.prefix);
  free(it->attrref.localpart);
  free(it->localname.prefix);
  free(it->localname.localpart);
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

df_seqtype *df_normalize_itemnode(int item)
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
    choice = df_add_alternative(choice,i7);
  }

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

static void df_print_objname(stringbuf *buf, char *name, char *ns, list *namespaces)
{
  if (NULL != ns) {
    char *prefix = NULL;
    list *l;
    for (l = namespaces; l; l = l->next) {
      ns_def *nsdef = (ns_def*)l->data;
      if (!strcmp(nsdef->href,ns))
        prefix = nsdef->prefix;
    }

    assert(NULL != prefix); /* FIXME: what to do if we can't find one? */
    stringbuf_printf(buf,"%s:%s",prefix,name);
  }
  else {
    stringbuf_printf(buf,"%s",name);
  }
}

void df_itemtype_print_fs(stringbuf *buf, df_itemtype *it, list *namespaces)
{
  switch (it->kind) {
  case ITEM_ATOMIC:
    df_print_objname(buf,it->type->name,it->type->ns,namespaces);
    /* FIXME */
    break;
  case ITEM_DOCUMENT:
    stringbuf_printf(buf,"document");
    if (NULL != it->content) {
      stringbuf_printf(buf," { ");
      df_seqtype_print_fs(buf,it->content,namespaces);
      stringbuf_printf(buf," }");
    }
    break;
  case ITEM_ELEMENT:
    stringbuf_printf(buf,"element");


    if (NULL != it->elem) {
      stringbuf_printf(buf," ");
      df_print_objname(buf,it->elem->name,it->elem->ns,namespaces);

      if (!it->elem->toplevel) {
        if (it->elem->nillable)
          stringbuf_printf(buf," nillable");
        stringbuf_printf(buf," of type ");
        df_print_objname(buf,it->elem->type->name,it->elem->type->ns,namespaces);
      }
    }
    else if (NULL != it->type) {
      if (it->nillable)
        stringbuf_printf(buf," nillable");
      stringbuf_printf(buf," of type ");
      df_print_objname(buf,it->type->name,it->type->ns,namespaces);
    }
    break;
  case ITEM_ATTRIBUTE:
    if (NULL != it->attr) {
      if (it->attr->toplevel) {
        stringbuf_printf(buf,"attribute ");
        df_print_objname(buf,it->attr->name,it->attr->ns,namespaces);
      }
      else {
        stringbuf_printf(buf,"attribute ");
        df_print_objname(buf,it->attr->name,it->attr->ns,namespaces);
        stringbuf_printf(buf," of type ");
        df_print_objname(buf,it->attr->type->name,it->attr->type->ns,namespaces);
      }
    }
    else if (NULL != it->type) {
      stringbuf_printf(buf,"attribute of type ");
      df_print_objname(buf,it->type->name,it->type->ns,namespaces);
    }
    else {
      stringbuf_printf(buf,"attribute");
    }

    break;
  case ITEM_PI:
    stringbuf_printf(buf,"processing-instruction");
    break;
  case ITEM_COMMENT:
    stringbuf_printf(buf,"comment");
    break;
  case ITEM_TEXT:
    stringbuf_printf(buf,"text");
    break;
  default:
    assert(!"invalid item type");
    break;
  }
}

void df_seqtype_print_fs(stringbuf *buf, df_seqtype *st, list *namespaces)
{
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
      stringbuf_printf(buf,"?");
      break;
    case OCCURS_ZERO_OR_MORE:
      stringbuf_printf(buf,"*");
      break;
    case OCCURS_ONE_OR_MORE:
      stringbuf_printf(buf,"+");
      break;
    default:
      assert(!"invalid occurrence");
      break;
    }
    break;
  case SEQTYPE_ALL:
    stringbuf_printf(buf,"(");
    df_seqtype_print_fs(buf,st->left,namespaces);
    stringbuf_printf(buf," & ");
    df_seqtype_print_fs(buf,st->right,namespaces);
    stringbuf_printf(buf,")");
    break;
  case SEQTYPE_SEQUENCE:
    stringbuf_printf(buf,"(");
    df_seqtype_print_fs(buf,st->left,namespaces);
    stringbuf_printf(buf," , ");
    df_seqtype_print_fs(buf,st->right,namespaces);
    stringbuf_printf(buf,")");
    break;
  case SEQTYPE_CHOICE:
    stringbuf_printf(buf,"(");
    df_seqtype_print_fs(buf,st->left,namespaces);
    stringbuf_printf(buf," | ");
    df_seqtype_print_fs(buf,st->right,namespaces);
    stringbuf_printf(buf,")");
    break;
  case SEQTYPE_EMPTY:
    stringbuf_printf(buf,"empty");
    break;
  case SEQTYPE_NONE:
    stringbuf_printf(buf,"none");
    break;
  case SEQTYPE_PAREN:
    stringbuf_printf(buf,"(");
    df_seqtype_print_fs(buf,st->left,namespaces);
    stringbuf_printf(buf,")");
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
    stringbuf_printf(buf,"%s:%s",def->prefix,name);
  }
  else {
    stringbuf_printf(buf,"%s",name);
  }
}

void df_itemtype_print_xpath(stringbuf *buf, df_itemtype *it, ns_map *namespaces)
{
  switch (it->kind) {
  case ITEM_ATOMIC:
    print_objname(buf,namespaces,it->type->name,it->type->ns);
    break;
  case ITEM_DOCUMENT:
    stringbuf_printf(buf,"document-node(");
    if (NULL != it->content) {
      assert(SEQTYPE_ALL == it->content->type);
      assert(SEQTYPE_ITEM == it->content->left->type);
      assert(ITEM_ELEMENT == it->content->left->item->kind);
      df_itemtype_print_xpath(buf,it->content->left->item,namespaces);
    }
    stringbuf_printf(buf,")");
    break;
  case ITEM_ELEMENT:
    if (NULL != it->elem) {
      if (it->elem->toplevel) {
        stringbuf_printf(buf,"schema-element(");
        print_objname(buf,namespaces,it->elem->name,it->elem->ns);
        stringbuf_printf(buf,")");
      }
      else {
        stringbuf_printf(buf,"element(");
        print_objname(buf,namespaces,it->elem->name,it->elem->ns);

        if (strcmp(it->elem->type->ns,XS_NAMESPACE) ||
            strcmp(it->elem->type->name,"anyType") ||
            !it->elem->nillable) {
          stringbuf_printf(buf,",");
          print_objname(buf,namespaces,it->elem->type->name,it->elem->type->ns);
          if (it->elem->nillable)
            stringbuf_printf(buf,"?");
        }
        stringbuf_printf(buf,")");
      }

    }
    else if (NULL != it->type) {
      stringbuf_printf(buf,"element(*,");
      print_objname(buf,namespaces,it->type->name,it->type->ns);
      if (it->nillable)
        stringbuf_printf(buf,"?");
      stringbuf_printf(buf,")");
    }
    else {
      stringbuf_printf(buf,"element()");
    }
    break;
  case ITEM_ATTRIBUTE:
    if (NULL != it->attr) {
      if (it->attr->toplevel) {
        stringbuf_printf(buf,"schema-attribute(");
        print_objname(buf,namespaces,it->attr->name,it->attr->ns);
        stringbuf_printf(buf,")");
      }
      else {
        stringbuf_printf(buf,"attribute(");
        print_objname(buf,namespaces,it->attr->name,it->attr->ns);

        if (strcmp(it->attr->type->ns,XS_NAMESPACE) ||
            strcmp(it->attr->type->name,"anySimpleType")) {
          stringbuf_printf(buf,",");
          print_objname(buf,namespaces,it->attr->type->name,it->attr->type->ns);
        }
        stringbuf_printf(buf,")");
      }
    }
    else if (NULL != it->type) {
      stringbuf_printf(buf,"attribute(*,");
      print_objname(buf,namespaces,it->type->name,it->type->ns);
      stringbuf_printf(buf,")");
    }
    else {
      stringbuf_printf(buf,"attribute()");
    }
    break;
  case ITEM_PI:
    stringbuf_printf(buf,"processing-instruction(");
    if (NULL != it->pistr) {
      if (is_ncname(it->pistr))
        stringbuf_printf(buf,"%s",it->pistr);
      else
        stringbuf_printf(buf,"\"%s\"",it->pistr);
    }
    stringbuf_printf(buf,")");
    break;
  case ITEM_COMMENT:
    stringbuf_printf(buf,"comment()");
    break;
  case ITEM_TEXT:
    stringbuf_printf(buf,"text()");
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
        stringbuf_printf(buf,"?");
        break;
      case OCCURS_ZERO_OR_MORE:
        stringbuf_printf(buf,"*");
        break;
      case OCCURS_ONE_OR_MORE:
        stringbuf_printf(buf,"+");
        break;
      default:
        assert(!"invalid occurrence");
        break;
      }
    }
  }
  else if (SEQTYPE_EMPTY == st->type) {
    stringbuf_printf(buf,"void()");
  }
  else if (st->isnode) {
    stringbuf_printf(buf,"node()");
  }
  else if (st->isitem) {
    stringbuf_printf(buf,"item()");
  }
  else {
    assert(!"sequencetype not expressible in xpath syntax");
  }
}


















int df_seqtype_compatible(df_seqtype *from, df_seqtype *to)
{
  return 0;
}

static int resolve_object(qname_t qname, ns_map *namespaces, xs_schema *s, const char *filename,
                          int line,error_info *ei, void **obj, int type)
{
  ns_def *def = NULL;
  char *ns = NULL;

  if (NULL == qname.localpart)
    return 0;

/*   debug("Resolving %s:%s",qname.prefix,qname.localpart); */

  if ((NULL != qname.prefix) && !strcmp(qname.prefix,"__xdt")) {
    ns = XDT_NAMESPACE;
  }
  else {
    if ((NULL != qname.prefix) &&
        (NULL == (def = ns_lookup_prefix(namespaces,qname.prefix))))
      return set_error_info2(ei,filename,line,NULL,"Invalid namespace prefix: %s",qname.prefix);

    ns = def ? def->href : NULL;
  }

  if (NULL == (*obj = xs_lookup_object(s,type,qname.localpart,ns))) {
    if (NULL != qname.prefix)
      return set_error_info2(ei,filename,line,NULL,"No such %s: %s:%s",
                             xs_object_types[type],qname.prefix,qname.localpart);
    else
      return set_error_info2(ei,filename,line,NULL,"No such %s: %s",
                             xs_object_types[type],qname.localpart);
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
      return set_error_info2(ei,filename,line,NULL,
                             "Invalid namespace prefix: %s",st->item->localname.prefix);

    ns = def ? def->href : NULL;

    if ((ITEM_ELEMENT == st->item->kind) && (NULL != st->item->localname.localpart)) {
      st->item->elem = xs_element_new(s->as);
      st->item->elem->name = strdup(st->item->localname.localpart);
      st->item->elem->ns = ns ? strdup(ns) : NULL;
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
      st->item->attr->name = strdup(st->item->localname.localpart);
      st->item->attr->ns = ns ? strdup(ns) : NULL;
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

df_node *df_node_new(int type)
{
  df_node *n = (df_node*)calloc(1,sizeof(df_node));
/*   debug("df_node_new %p %s",n,itemtypes[type]); */
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
      debug("remaining node %p %-10s %-10s - %-3d refs",n,"element",n->name,n->refcount);
    else if (NODE_TEXT == n->type)
      debug("remaining node %p %-10s %-10s - %-3d refs",n,"text",n->value,n->refcount);
    else
      debug("remaining node %p %-10s %-10s - %-3d refs",n,itemtypes[n->type],"",n->refcount);
  }
  for (l = allocvalues; l; l = l->next) {
    df_value *v = (df_value*)l->data;
    stringbuf *stbuf = stringbuf_new();
    stringbuf *valbuf = stringbuf_new();
    df_seqtype_print_fs(stbuf,v->seqtype,globals->namespaces->defs);
    if ((SEQTYPE_ITEM == v->seqtype->type) &&
        (ITEM_ATOMIC == v->seqtype->item->kind))
      df_value_printbuf(globals,valbuf,v);
    debug("remaining value %p %-20s - %-3d refs : %s",v,stbuf->data,v->refcount,valbuf->data);
    stringbuf_free(stbuf);
    stringbuf_free(valbuf);
  }
}

static void df_node_free(df_node *n)
{
  df_node *c;
  if (NODE_ELEMENT == n->type)
    debug("df_node_free %p element %s",n,n->name);
  else
    debug("df_node_free %p %s",n,itemtypes[n->type]);

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
  free(n->ns);
  free(n->name);
  free(n->value);
  free(n);
}

void df_node_deref(df_node *n)
{
  df_node *root = df_node_root(n);
  root->refcount--;

  debug("df_node_deref %s %p (root is %s %p), now have %d refs",
        itemtypes[n->type],n,itemtypes[root->type],root,root->refcount);

  assert(0 <= root->refcount);
  if (0 == root->refcount)
    df_node_free(root);
}

df_node *df_node_deep_copy(df_node *n)
{
  df_node *copy = df_node_new(n->type);
  df_node *c;
  list *l;
  debug("df_node_deep_copy %s %p -> %p",itemtypes[n->type],n,copy);

  /* FIXME: namespaces */

  for (l = n->attributes; l; l = l->next) {
    df_node *attr = (df_node*)l->data;
    df_node *attrcopy = df_node_deep_copy(attr);
    df_node_add_attribute(copy,attrcopy);
  }

  copy->prefix = n->prefix ? strdup(n->prefix) : NULL;
  copy->ns = n->ns ? strdup(n->ns) : NULL;
  copy->name = n->name ? strdup(n->name) : NULL;
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

void df_node_add_attribute(df_node *n, df_node *attr)
{
  assert(NULL == attr->parent);
  assert(NULL == attr->prev);
  assert(NULL == attr->next);
  assert(0 == attr->refcount);

  list_append(&n->attributes,attr);
  attr->parent = n;
}

df_value *df_node_to_value(df_node *n)
{
  df_seqtype *st = df_seqtype_new_item(n->type);
  df_value *v = df_value_new(st);
  df_seqtype_deref(st);
  v->value.n = df_node_ref(n);
  return v;
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
  stringbuf_printf(buf," ");
  text->value = strdup(buf->data);
  stringbuf_free(buf);
  return text;
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
/*     debug("df_node_print NODE_ELEMENT: %s",n->name); */
    xmlTextWriterStartElement(writer,n->name);
    for (l = n->attributes; l; l = l->next)
      df_node_print(writer,(df_node*)l->data);
    for (c = n->first_child; c; c = c->next)
      df_node_print(writer,c);
    xmlTextWriterEndElement(writer);
    break;
  }
  case NODE_ATTRIBUTE:
    xmlTextWriterWriteAttribute(writer,n->name,n->value);
    break;
  case NODE_PI:
    /* FIXME */
    break;
  case NODE_COMMENT:
    /* FIXME */
    break;
  case NODE_TEXT:
/*     debug("df_node_print NODE_TEXT: %s",n->value); */
    xmlTextWriterWriteString(writer,n->value);
    break;
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
        stringbuf_printf(buf,"%d",v->value.i);
      else if (v->seqtype->item->type == globals->string_type)
        stringbuf_printf(buf,"%s",v->value.s);
      else if (v->seqtype->item->type == globals->boolean_type)
        stringbuf_printf(buf,"%s",v->value.b ? "true" : "false");
      else
        stringbuf_printf(buf,"(atomic value)");
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
    else {
      stringbuf_printf(buf,"(item, kind %d)",v->seqtype->item->kind);
    }
  }
  else if (SEQTYPE_SEQUENCE == v->seqtype->type) {
    df_value_printbuf(globals,buf,v->value.pair.left);
    stringbuf_printf(buf,", ");
    df_value_printbuf(globals,buf,v->value.pair.right);
  }
  else if (SEQTYPE_EMPTY == v->seqtype->type) {
    stringbuf_printf(buf,"(empty)",v->seqtype->type);
  }
  else {
    stringbuf_printf(buf,"(value, seqtype %d)",v->seqtype->type);
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
    fprintf(stderr,"WARNING: too mant derefs!\n");
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
