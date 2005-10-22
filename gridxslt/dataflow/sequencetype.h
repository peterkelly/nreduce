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

typedef struct itemtype itemtype;
typedef struct seqtype seqtype;
typedef struct df_seqpair df_seqpair;
typedef struct node node;
typedef struct value value;
typedef node gxnode;

struct itemtype {
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
  seqtype *content;
};

struct seqtype {
  int type;
  seqtype *left;
  seqtype *right;
  itemtype *item;
  int occurrence;
  int isnode;
  int isitem;
  int valtype;
  int refcount;
};

struct df_seqpair {
  value *left;
  value *right;
};

struct node {
  int type;
  list *namespaces;
  list *attributes;
  char *prefix;
  nsname ident;
  char *target;
  char *value;
  int refcount;

  node *next;
  node *prev;
  node *first_child;
  node *last_child;
  node *parent;
  int nodeno;
};

struct value {
  seqtype *st;
  union {
    int i;
    float f;
    double d;
    char *s;
    int b;
    df_seqpair pair;
    node *n;
  } value;
  int refcount;
};

#define checkatomic(_v,_type)                                                                \
                        assert(SEQTYPE_ITEM == (_v)->st->type);                              \
                        assert(ITEM_ATOMIC == (_v)->st->item->kind);                         \
                        assert(xs_type_is_derived((_v)->st->item->type,_type));

#define asint(_v)    ({ checkatomic(_v,xs_g->decimal_type) (_v)->value.i;})
#define asfloat(_v)  ({ checkatomic(_v,xs_g->float_type)   (_v)->value.f;})
#define asdouble(_v) ({ checkatomic(_v,xs_g->double_type)  (_v)->value.d;})
#define asstring(_v) ({ checkatomic(_v,xs_g->string_type)  (_v)->value.s;})
#define asbool(_v)   ({ checkatomic(_v,xs_g->boolean_type) (_v)->value.b;})
#define asnode(_v)   ({ assert(SEQTYPE_ITEM == (_v)->st->type);                              \
                        assert(ITEM_ATOMIC != (_v)->st->item->kind);                         \
                        (_v)->value.n;})


/* #define asint(_v) ((_v)->value.i) */

/* #define asfloat(_v) ((_v)->value.f) */
/* #define asdouble(_v) ((_v)->value.d) */
/* #define asstring(_v) ((_v)->value.s) */
/* #define asbool(_v) ((_v)->value.b) */
/* #define asnode(_v) ((_v)->value.n) */

itemtype *itemtype_new(int kind);
seqtype *seqtype_new(int type);
seqtype *seqtype_new_item(int kind);
seqtype *seqtype_new_atomic(xs_type *type);
seqtype *seqtype_ref(seqtype *st);
void itemtype_free(itemtype *it);
void seqtype_deref(seqtype *st);
seqtype *df_normalize_itemnode(int item);
seqtype *df_interleave_document_content(seqtype *content);

void seqtype_print_fs(stringbuf *buf, seqtype *st, ns_map *namespaces);
void seqtype_print_xpath(stringbuf *buf, seqtype *st, ns_map *namespaces);

int seqtype_compatible(seqtype *from, seqtype *to);

int seqtype_resolve(seqtype *st, ns_map *namespaces, xs_schema *s, const char *filename,
                        int line, error_info *ei);
void seqtype_to_list(seqtype *st, list **types);
int seqtype_derived(seqtype *base, seqtype *st);

node *node_new(int type);
node *node_root(node *n);
node *node_ref(node *n);
void df_print_remaining();
void node_free(node *n);
void node_deref(node *n);
node *node_deep_copy(node *n);
node *node_unique(node *n);
void node_add_child(node *n, node *c);
void node_insert_child(node *n, node *c, node *before);
void node_add_attribute(node *n, node *attr);
void node_add_namespace(node *n, node *ns);
node *node_from_xmlnode(xmlNodePtr xn);
int node_check_tree(node *n);
node *node_traverse_prev(node *n, node *subtree);
node *node_traverse_next(node *n, node *subtree);
void node_print(xmlTextWriter *writer, node *n);

value *value_new_int(int i);
value *value_new_float(float f);
value *value_new_double(double d);
value *value_new_string(const char *str);
value *value_new_bool(int b);
value *value_new_node(node *n);

value *value_new(seqtype *seqtype);
value *value_ref(value *v);
void value_deref(value *v);
void value_deref_list(list *l);
void value_printbuf(stringbuf *buf, value *v);
void value_print(FILE *f, value *v);
int value_equals(value *a, value *b);
value **df_sequence_to_array(value *seq);
void df_get_sequence_values(value *v, list **values);
list *df_sequence_to_list(value *seq);
value *df_list_to_sequence(list *values);
void node_to_string(node *n, stringbuf *buf);
char *value_as_string(value *v);
value *df_atomize(value *v);
value *df_cast(value *v, seqtype *as);

#ifndef _DATAFLOW_SEQUENCETYPE_C
extern const char *df_item_kinds[ITEM_COUNT];
#endif

#endif /* _DATAFLOW_SEQUENCETYPE_H */
