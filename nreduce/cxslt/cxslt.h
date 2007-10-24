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

#define XSLT_NS "http://www.w3.org/1999/XSL/Transform"
#define WSDL_NS "http://schemas.xmlsoap.org/wsdl/"
#define WSDLSOAP_NS "http://schemas.xmlsoap.org/wsdl/soap/"
#define XMLSCHEMA_NS "http://www.w3.org/2001/XMLSchema"
#define SOAPENV_NS "http://schemas.xmlsoap.org/soap/envelope/"

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
#define XPATH_NUMERIC_LITERAL             42
#define XPATH_LAST_PREDICATE              43
#define XPATH_PREDICATE                   44
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
#define XPATH_NODE_TEST                   61
#define XPATH_UNUSED2                     62

#define XPATH_DSVAR                       63
#define XPATH_DSVAR_NUMERIC               64
#define XPATH_DSVAR_STRING                65
#define XPATH_DSVAR_VAR_REF               66
#define XPATH_DSVAR_NODETEST              67
#define XPATH_AXIS                        68

#define XSLT_FIRST                        69

#define XSLT_STYLESHEET                   69
#define XSLT_FUNCTION                     70
#define XSLT_TEMPLATE                     71
#define XSLT_VARIABLE                     72
#define XSLT_SEQUENCE                     73
#define XSLT_VALUE_OF                     74
#define XSLT_TEXT                         75
#define XSLT_FOR_EACH                     76
#define XSLT_IF                           77
#define XSLT_CHOOSE                       78
#define XSLT_ELEMENT                      79
#define XSLT_ATTRIBUTE                    80
#define XSLT_NAMESPACE                    81
#define XSLT_APPLY_TEMPLATES              82
#define XSLT_LITERAL_RESULT_ELEMENT       83
#define XSLT_LITERAL_TEXT_NODE            84
#define XSLT_PARAM                        85
#define XSLT_OUTPUT                       86
#define XSLT_STRIP_SPACE                  87
#define XSLT_WHEN                         88
#define XSLT_OTHERWISE                    89
#define XSLT_LITERAL_ATTRIBUTE            90

#define XSLT_DSVAR_INSTRUCTION            91
#define XSLT_DSVAR_TEXT                   92
#define XSLT_DSVAR_LITELEM                93
#define XSLT_DSVAR_TEXTINSTR              94

#define XPATH_COUNT                       95

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
#define AXIS_DSVAR_FORWARD                14
#define AXIS_DSVAR_REVERSE                15
#define AXIS_COUNT                        16

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

#define EXPR_REFCOUNT 8

typedef struct expression {
  int type;
  qname qn;
  int restype;
  char *ident;

  int axis;
  double num;
  char *str;
  int kind;
  struct expression *target;
  xmlNodePtr xmlnode;
  char *orig;

  list *derivatives;
  int called;

  union {
    struct {
      struct expression *test;
      struct expression *left;
      struct expression *right;

      struct expression *name_avt;
      struct expression *value_avt;
      struct expression *namespace_avt;

      struct expression *children;
      struct expression *attributes;
    } r;
    struct expression *refs[EXPR_REFCOUNT];
  };

  struct expression *next;
  struct expression *prev;
  struct expression *parent;
} expression;

typedef struct seqtype {
} seqtype;

const char *lookup_nsuri(xmlNodePtr n, const char *prefix);

expression *new_TypeExpr(int type, seqtype *st, expression *right);
expression *new_ForExpr(expression *left, expression *right);
expression *new_VarInExpr(expression *left);
expression *new_VarInListExpr(expression *left, expression *right);
expression *new_QuantifiedExpr(int type, expression *left, expression *right);

int is_forward_axis(int axis);
int is_reverse_axis(int axis);

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
  expression *root;
  stack *typestack;
  int ispattern;
} elcgen;

void free_wsarg_ptr(void *a);

int gen_error(elcgen *gen, const char *format, ...);
void gen_printf(elcgen *gen, const char *format, ...);
void gen_iprintf(elcgen *gen, int indent, const char *format, ...);
void gen_printorig(elcgen *gen, int indent, expression *expr);

int build_xslt_tree(elcgen *gen, xmlNodePtr n, expression **root);

/* tree */

expression *new_expression(int type);
expression *new_expression2(int type, expression *left, expression *right);
void free_expression(expression *expr);
expression *new_xsltnode(xmlNodePtr n, int type);
expression *lookup_function(elcgen *gen, qname qn);

void expr_set_parents(expression *expr, expression *parent);
expression *parse_xpath(elcgen *gen, expression *pnode, const char *str);
int parse_xslt(elcgen *gen, xmlNodePtr parent, expression *pnode);
int expr_resolve_vars(elcgen *gen, expression *expr);
int exclude_namespace(elcgen *gen, expression *expr, const char *uri);
void expr_print_tree(elcgen *gen, int indent, expression *expr);
expression *expr_copy(expression *orig);

/* analyse */

int is_expr_doc_order(expression *expr);
void expr_compute_restype(elcgen *gen, expression *expr, int ctxtype);

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

/* rules */

int compile_ws_call(elcgen *gen, int indent, expression *expr, const char *wsdlurl);
int compile_test(elcgen *gen, int indent, expression *expr);
int compile_ebv_expression(elcgen *gen, int indent, expression *expr);
int compile_user_function_call(elcgen *gen, int indent, expression *expr, int fromnum);
int compile_builtin_function_call(elcgen *gen, int indent, expression *expr);
int compile_choose(elcgen *gen, int indent, expression *expr);
int compile_num_expression(elcgen *gen, int indent, expression *expr);
int compile_predicate(elcgen *gen, int indent, expression *expr);
int compile_expression(elcgen *gen, int indent, expression *expr);
int compile_axis(elcgen *gen, int indent, expression *expr);
int compile_avt(elcgen *gen, int indent, expression *expr);
int compile_attributes(elcgen *gen, int indent, expression *expr);
int compile_namespaces(elcgen *gen, int indent, expression *expr);
int compile_instruction(elcgen *gen, int indent, expression *expr);
int compile_variable(elcgen *gen, int indent, expression *xchild);
int compile_num_instruction(elcgen *gen, int indent, expression *expr);
int compile_num_sequence(elcgen *gen, int indent, expression *xfirst);
int compile_sequence(elcgen *gen, int indent, expression *xfirst);


#ifndef _TREE_C
extern const char *expr_names[XPATH_COUNT];
extern const char *axis_names[AXIS_COUNT];
extern const char *kind_names[KIND_COUNT];
extern const char *restype_names[RESTYPE_COUNT];
extern const char *expr_refnames[EXPR_REFCOUNT];
#endif

#endif
