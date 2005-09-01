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

#ifndef _DATAFLOW_SEQUENCETYPE_H
#define _DATAFLOW_SEQUENCETYPE_H

#include "xmlschema/xmlschema.h"
#include "util/stringbuf.h"
#include "util/list.h"
#include "util/namespace.h"
#include "util/xmlutils.h"
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
#define SEQTYPE_NONE                  7
#define SEQTYPE_PAREN                 8

#define VALTYPE_INVALID               0
#define VALTYPE_INT                   1
#define VALTYPE_FLOAT                 2
#define VALTYPE_DOUBLE                3
#define VALTYPE_STRING                4
#define VALTYPE_BOOL                  5
#define VALTYPE_PAIR                  6
#define VALTYPE_NONE                  7

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

typedef struct df_itemtype df_itemtype;
typedef struct df_seqtype df_seqtype;
typedef struct df_seqpair df_seqpair;
typedef struct df_node df_node;
typedef struct df_value df_value;

struct df_itemtype {
  int kind;
  xs_type *type;
  xs_element *elem;
  xs_attribute *attr;
  qname typeref;
  qname elemref;
  qname attrref;
  qname localname;
  char *pistr;
  int nillable;
  df_seqtype *content;
};

struct df_seqtype {
  int type;
  df_seqtype *left;
  df_seqtype *right;
  df_itemtype *item;
  int occurrence;
  int isnode;
  int isitem;
  int valtype;
  int refcount;
};

struct df_seqpair {
  df_value *left;
  df_value *right;
};

struct df_node {
  int type;
  list *namespaces;
  list *attributes;
  char *prefix;
  nsname ident;
  char *target;
  char *value;
  int refcount;

  df_node *next;
  df_node *prev;
  df_node *first_child;
  df_node *last_child;
  df_node *parent;
  int nodeno;
};

struct df_value {
  df_seqtype *seqtype;
  union {
    int i;
    float f;
    double d;
    char *s;
    int b;
    df_seqpair pair;
    df_node *n;
  } value;
  int refcount;
};

df_itemtype *df_itemtype_new(int kind);
df_seqtype *df_seqtype_new(int type);
df_seqtype *df_seqtype_new_item(int kind);
df_seqtype *df_seqtype_new_atomic(xs_type *type);
df_seqtype *df_seqtype_ref(df_seqtype *st);
void df_itemtype_free(df_itemtype *it);
void df_seqtype_deref(df_seqtype *st);
df_seqtype *df_normalize_itemnode(int item);
df_seqtype *df_interleave_document_content(df_seqtype *content);

void df_itemtype_print_fs(stringbuf *buf, df_itemtype *it, list *namespaces);
void df_seqtype_print_fs(stringbuf *buf, df_seqtype *st, list *namespaces);

void df_itemtype_print_xpath(stringbuf *buf, df_itemtype *it, ns_map *namespaces);
void df_seqtype_print_xpath(stringbuf *buf, df_seqtype *st, ns_map *namespaces);

int df_seqtype_compatible(df_seqtype *from, df_seqtype *to);

int df_seqtype_resolve(df_seqtype *st, ns_map *namespaces, xs_schema *s, const char *filename,
                        int line, error_info *ei);

df_node *df_node_new(int type);
df_node *df_node_root(df_node *n);
df_node *df_node_ref(df_node *n);
void df_print_remaining(xs_globals *globals);
void df_node_free(df_node *n);
void df_node_deref(df_node *n);
df_node *df_node_deep_copy(df_node *n);
df_node *df_node_unique(df_node *n);
void df_node_add_child(df_node *n, df_node *c);
void df_node_insert_child(df_node *n, df_node *c, df_node *before);
void df_node_add_attribute(df_node *n, df_node *attr);
void df_node_add_namespace(df_node *n, df_node *ns);
df_value *df_node_to_value(df_node *n);
df_node *df_atomic_value_to_text_node(xs_globals *g, df_value *v);
df_node *df_node_from_xmlnode(xmlNodePtr xn);
int df_check_tree(df_node *n);
df_node *df_prev_node(df_node *node, df_node *subtree);
df_node *df_next_node(df_node *node, df_node *subtree);

df_value *df_value_new_string(xs_globals *g, const char *str);
df_value *df_value_new_int(xs_globals *g, int i);

df_value *df_value_new(df_seqtype *seqtype);
df_value *df_value_ref(df_value *v);
void df_node_print(xmlTextWriter *writer, df_node *n);
void df_value_printbuf(xs_globals *globals, stringbuf *buf, df_value *v);
void df_value_print(xs_globals *globals, FILE *f, df_value *v);
void df_value_deref(xs_globals *globals, df_value *v);
void df_value_deref_list(xs_globals *globals, list *l);
int df_value_equals(df_value *a, df_value *b);
void df_get_sequence_values(df_value *v, list **values);
list *df_sequence_to_list(df_value *seq);
df_value *df_list_to_sequence(xs_globals *g, list *values);
void df_node_to_string(df_node *n, stringbuf *buf);
char *df_value_as_string(xs_globals *g, df_value *v);
df_value *df_atomize(xs_globals *g, df_value *v);
df_value *df_cast(xs_globals *g, df_value *v, df_seqtype *as);

#ifndef _DATAFLOW_SEQUENCETYPE_C
extern const char *df_item_kinds[ITEM_COUNT];
#endif

#endif /* _DATAFLOW_SEQUENCETYPE_H */
