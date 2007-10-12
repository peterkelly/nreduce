/*
 * This file is part of the nreduce project
 * Copyright (C) 2007 Peter Kelly (pmk@cs.adelaide.edu.au)
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
 * $Id: cxslt.c 603 2007-08-14 02:44:16Z pmkelly $
 *
 */

#ifndef _CXSLT_H
#define _CXSLT_H

#include <libxml/tree.h>
#include "network/util.h"
#include "xslt.h"

#define XSLT_NAMESPACE "http://www.w3.org/1999/XSL/Transform"
#define WSDL_NAMESPACE "http://schemas.xmlsoap.org/wsdl/"
#define WSDLSOAP_NAMESPACE "http://schemas.xmlsoap.org/wsdl/soap/"
#define XMLSCHEMA_NAMESPACE "http://www.w3.org/2001/XMLSchema"
#define SOAPENV_NAMESPACE "http://schemas.xmlsoap.org/soap/envelope/"

#define xmlStrcmp(a,b) xmlStrcmp((xmlChar*)(a),(xmlChar*)(b))
#define xmlStrdup(a) ((char*)xmlStrdup((xmlChar*)(a)))
#define xmlGetProp(a,b) ((char*)xmlGetProp(a,(xmlChar*)(b)))
#define xmlGetNsProp(a,b,c) ((char*)xmlGetNsProp(a,(xmlChar*)(b),(xmlChar*)(c)))
#define xmlHasProp(a,b) (xmlHasProp(a,(xmlChar*)(b)))
#define xmlNodeListGetString(doc,list,inLine) (char*)xmlNodeListGetString(doc,list,inLine)

/* cxslt */

#define XPATH_INVALID                     0
#define XPATH_FOR                         1
#define XPATH_SOME                        2
#define XPATH_EVERY                       3
#define XPATH_IF                          4
#define XPATH_VAR_IN                      5
#define XPATH_OR                          6
#define XPATH_AND                         7

#define XPATH_GENERAL_EQ                  8
#define XPATH_GENERAL_NE                  9
#define XPATH_GENERAL_LT                  10
#define XPATH_GENERAL_LE                  11
#define XPATH_GENERAL_GT                  12
#define XPATH_GENERAL_GE                  13

#define XPATH_VALUE_EQ                    14
#define XPATH_VALUE_NE                    15
#define XPATH_VALUE_LT                    16
#define XPATH_VALUE_LE                    17
#define XPATH_VALUE_GT                    18
#define XPATH_VALUE_GE                    19

#define XPATH_NODE_IS                     20
#define XPATH_NODE_PRECEDES               21
#define XPATH_NODE_FOLLOWS                22

#define XPATH_TO                          23
#define XPATH_ADD                         24
#define XPATH_SUBTRACT                    25
#define XPATH_MULTIPLY                    26
#define XPATH_DIVIDE                      27
#define XPATH_IDIVIDE                     28
#define XPATH_MOD                         29
#define XPATH_UNION                       30
#define XPATH_UNION2                      31
#define XPATH_INTERSECT                   32
#define XPATH_EXCEPT                      33
#define XPATH_INSTANCE_OF                 34
#define XPATH_TREAT                       35
#define XPATH_CASTABLE                    36
#define XPATH_CAST                        37
#define XPATH_UNARY_MINUS                 38
#define XPATH_UNARY_PLUS                  39
#define XPATH_ROOT                        40
#define XPATH_STRING_LITERAL              41
#define XPATH_INTEGER_LITERAL             42
#define XPATH_DECIMAL_LITERAL             43
#define XPATH_DOUBLE_LITERAL              44
#define XPATH_VAR_REF                     45
#define XPATH_EMPTY                       46
#define XPATH_CONTEXT_ITEM                47
#define XPATH_KIND_TEST                   48
#define XPATH_ACTUAL_PARAM                49
#define XPATH_FUNCTION_CALL               50
#define XPATH_PAREN                       51
#define XPATH_ATOMIC_TYPE                 52
#define XPATH_ITEM_TYPE                   53
#define XPATH_SEQUENCE                    54
#define XPATH_STEP                        55
#define XPATH_VARINLIST                   56
#define XPATH_PARAMLIST                   57
#define XPATH_FILTER                      58
#define XPATH_NAME_TEST                   59

#define XPATH_AVT_COMPONENT               60
#define XPATH_FORWARD_AXIS_STEP           61
#define XPATH_REVERSE_AXIS_STEP           62

#define XPATH_COUNT                       63

#define XSLT_INVALID                      0
#define XSLT_STYLESHEET                   1
#define XSLT_FUNCTION                     2
#define XSLT_TEMPLATE                     3
#define XSLT_VARIABLE                     4
#define XSLT_SEQUENCE                     5
#define XSLT_VALUE_OF                     6
#define XSLT_TEXT                         7
#define XSLT_FOR_EACH                     8
#define XSLT_IF                           9
#define XSLT_CHOOSE                       10
#define XSLT_ELEMENT                      11
#define XSLT_ATTRIBUTE                    12
#define XSLT_INAMESPACE                   13
#define XSLT_APPLY_TEMPLATES              14
#define XSLT_LITERAL_RESULT_ELEMENT       15
#define XSLT_LITERAL_TEXT_NODE            16
#define XSLT_PARAM                        17
#define XSLT_OUTPUT                       18
#define XSLT_STRIP_SPACE                  19
#define XSLT_WHEN                         20
#define XSLT_OTHERWISE                    21
#define XSLT_LITERAL_ATTRIBUTE            22
#define XSLT_COUNT                        23

#define AXIS_INVALID                      0
#define AXIS_CHILD                        1
#define AXIS_DESCENDANT                   2
#define AXIS_ATTRIBUTE                    3
#define AXIS_SELF                         4
#define AXIS_DESCENDANT_OR_SELF           5
#define AXIS_FOLLOWING_SIBLING            6
#define AXIS_FOLLOWING                    7
#define AXIS_NAMESPACE                    8
#define AXIS_PARENT                       9
#define AXIS_ANCESTOR                     10
#define AXIS_PRECEDING_SIBLING            11
#define AXIS_PRECEDING                    12
#define AXIS_ANCESTOR_OR_SELF             13
#define AXIS_COUNT                        14

#define SEQTYPE_INVALID                   0
#define SEQTYPE_ITEM                      1
#define SEQTYPE_OCCURRENCE                2
#define SEQTYPE_ALL                       3
#define SEQTYPE_SEQUENCE                  4
#define SEQTYPE_CHOICE                    5
#define SEQTYPE_EMPTY                     6

#define KIND_INVALID                      0
#define KIND_DOCUMENT                     1
#define KIND_ELEMENT                      2
#define KIND_ATTRIBUTE                    3
#define KIND_SCHEMA_ELEMENT               4
#define KIND_SCHEMA_ATTRIBUTE             5
#define KIND_PI                           6
#define KIND_COMMENT                      7
#define KIND_TEXT                         8
#define KIND_ANY                          9
#define KIND_COUNT                        10

#define OCCURS_ONCE                       0
#define OCCURS_OPTIONAL                   1
#define OCCURS_ZERO_OR_MORE               2
#define OCCURS_ONE_OR_MORE                3

#define RESTYPE_UNKNOWN                   0
#define RESTYPE_RECURSIVE                 1
#define RESTYPE_GENERAL                   2
#define RESTYPE_NUMBER                    3
#define RESTYPE_NUMLIST                   4
#define RESTYPE_COUNT                     5

#define QNAME_NULL { uri: NULL, prefix : NULL, localpart : NULL }

typedef struct qname {
  char *uri;
  char *prefix;
  char *localpart;
} qname;

typedef struct wsarg {
  int list;
  int simple;
  char *uri;
  char *localpart;
} wsarg;

typedef struct expression {
  int type;
  struct expression *test;
  struct expression *left;
  struct expression *right;
  qname qn;
  int axis;
  double num;
  char *str;
  int kind;
  struct xsltnode *target;
  int restype;
} expression;

typedef struct xsltnodelist {
  struct xsltnode *first;
  struct xsltnode *last;
} xsltnodelist;

typedef struct xsltnode {
  int type;
  xmlNodePtr n;
  qname name_qn;
  char *name_ident;
  xsltnodelist children;
  xsltnodelist attributes;
  struct xsltnode *next;
  struct xsltnode *prev;
  struct xsltnode *parent;
  expression *expr;
  expression *name_avt;
  expression *value_avt;
  expression *namespace_avt;
  int restype;
  list *derivatives;
  int called;
} xsltnode;

typedef struct seqtype {
} seqtype;

const char *lookup_nsuri(xmlNodePtr n, const char *prefix);

expression *new_TypeExpr(int type, seqtype *st, expression *right);
expression *new_ForExpr(expression *left, expression *right);
expression *new_VarInExpr(expression *left);
expression *new_VarInListExpr(expression *left, expression *right);
expression *new_QuantifiedExpr(int type, expression *left, expression *right);

typedef struct {
  char *filename;
  xmlDocPtr doc;
  xmlNodePtr root;
} wsdlfile;

typedef struct {
  list *wsdlfiles;
  xmlNodePtr toplevel;
  array *buf;
  int option_indent;
  int option_strip;
  xmlDocPtr parse_doc;
  const char *parse_filename;
  char *error;
  int indent;
  xsltnode *root;
  stack *typestack;
} elcgen;

void free_wsarg_ptr(void *a);

int gen_error(elcgen *gen, const char *format, ...);
void gen_printf(elcgen *gen, const char *format, ...);
void gen_iprintf(elcgen *gen, const char *format, ...);

/* tree */

expression *new_expression(int type);
expression *new_expression2(int type, expression *left, expression *right);
void free_expression(expression *expr);
xsltnode *new_xsltnode(xmlNodePtr n, int type);
void free_xsltnode(xsltnode *xn);
xsltnode *lookup_function(elcgen *gen, qname qn);

int parse_xslt(elcgen *gen, xmlNodePtr parent, xsltnode *pnode);
int xslt_resolve_vars(elcgen *gen, xsltnode *xn);
int exclude_namespace(elcgen *gen, xsltnode *xn2, const char *uri);
void xslt_print_tree(elcgen *gen, xsltnode *xn);
xsltnode *copy_xsltnode(elcgen *gen, xsltnode *orig, xsltnode *copyparent, int attr);

/* analyse */

int is_expr_doc_order(xsltnode *xn, expression *expr);
void xslt_compute_restype(elcgen *gen, xsltnode *xn, int ctxtype);

/* xmlutil */

int attr_equals(xmlNodePtr n, const char *name, const char *value);
qname string_to_qname(const char *str, xmlNodePtr n);
void free_qname(qname qn);
int nullstrcmp(const char *a, const char *b);
qname get_qname_attr(xmlNodePtr n, const char *attrname);
int is_element(xmlNodePtr n, const char *ns, const char *name);
char *nsname_to_ident(const char *nsuri, const char *localname);
int qname_equals(qname a, qname b);
qname copy_qname(qname orig);

/* wsdl */

#define STYLE_RPC          1
#define STYLE_DOCWRAPPED   2

int process_wsdl(elcgen *gen, const char *filename, wsdlfile **wfout);
int wsdl_get_style(elcgen *gen, xmlNodePtr wsdlroot, int *styleout);
int wsdl_get_url(elcgen *gen, wsdlfile *wf, char **urlout);
int wsdl_get_operation_messages(elcgen *gen,
                                wsdlfile *wf, const char *opname,
                                qname *inqn, qname *outqn,
                                list **inargs, list **outargs);

#ifndef _TREE_C
extern const char *xpath_names[XPATH_COUNT];
extern const char *xslt_names[XSLT_COUNT];
extern const char *axis_names[AXIS_COUNT];
extern const char *kind_names[KIND_COUNT];
extern const char *restype_names[RESTYPE_COUNT];
#endif

#endif
