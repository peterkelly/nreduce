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

#ifndef _XSLT_XPATH_H
#define _XSLT_XPATH_H

#include "util/stringbuf.h"
#include "util/xmlutils.h"
#include "dataflow/dataflow.h"
#include "dataflow/sequencetype.h"

#define XPATH_EXPR_INVALID                0
#define XPATH_EXPR_FOR                    1
#define XPATH_EXPR_SOME                   2
#define XPATH_EXPR_EVERY                  3
#define XPATH_EXPR_IF                     4
#define XPATH_EXPR_VAR_IN                 5
#define XPATH_EXPR_OR                     6
#define XPATH_EXPR_AND                    7
#define XPATH_EXPR_COMPARE_VALUES         8
#define XPATH_EXPR_COMPARE_GENERAL        9
#define XPATH_EXPR_COMPARE_NODES          10
#define XPATH_EXPR_TO                     11
#define XPATH_EXPR_ADD                    12
#define XPATH_EXPR_SUBTRACT               13
#define XPATH_EXPR_MULTIPLY               14
#define XPATH_EXPR_DIVIDE                 15
#define XPATH_EXPR_IDIVIDE                16
#define XPATH_EXPR_MOD                    17
#define XPATH_EXPR_UNION                  18
#define XPATH_EXPR_UNION2                 19
#define XPATH_EXPR_INTERSECT              20
#define XPATH_EXPR_EXCEPT                 21
#define XPATH_EXPR_INSTANCE_OF            22
#define XPATH_EXPR_TREAT                  23
#define XPATH_EXPR_CASTABLE               24
#define XPATH_EXPR_CAST                   25
#define XPATH_EXPR_UNARY_MINUS            26
#define XPATH_EXPR_UNARY_PLUS             27
#define XPATH_EXPR_ROOT                   28
#define XPATH_EXPR_STRING_LITERAL         29
#define XPATH_EXPR_INTEGER_LITERAL        30
#define XPATH_EXPR_DECIMAL_LITERAL        31
#define XPATH_EXPR_DOUBLE_LITERAL         32
#define XPATH_EXPR_VAR_REF                33
#define XPATH_EXPR_EMPTY                  34
#define XPATH_EXPR_CONTEXT_ITEM           35
#define XPATH_EXPR_NODE_TEST              36
#define XPATH_EXPR_ACTUAL_PARAM           37
#define XPATH_EXPR_FUNCTION_CALL          38
#define XPATH_EXPR_PAREN                  39
#define XPATH_EXPR_ATOMIC_TYPE            40
#define XPATH_EXPR_ITEM_TYPE              41
#define XPATH_EXPR_SEQUENCE               42
#define XPATH_EXPR_STEP                   43
#define XPATH_EXPR_VARINLIST              44
#define XPATH_EXPR_PARAMLIST              45
#define XPATH_EXPR_FILTER                 46

#define XPATH_EXPR_COUNT                  47

#define XPATH_NODE_TEST_INVALID           0
#define XPATH_NODE_TEST_NAME              1
#define XPATH_NODE_TEST_SEQTYPE           2

#define XPATH_GENERAL_COMP_INVALID        0
#define XPATH_GENERAL_COMP_EQ             1
#define XPATH_GENERAL_COMP_NE             2
#define XPATH_GENERAL_COMP_LT             3
#define XPATH_GENERAL_COMP_LE             4
#define XPATH_GENERAL_COMP_GT             5
#define XPATH_GENERAL_COMP_GE             6

#define XPATH_VALUE_COMP_INVALID          0
#define XPATH_VALUE_COMP_EQ               1
#define XPATH_VALUE_COMP_NE               2
#define XPATH_VALUE_COMP_LT               3
#define XPATH_VALUE_COMP_LE               4
#define XPATH_VALUE_COMP_GT               5
#define XPATH_VALUE_COMP_GE               6

#define XPATH_NODE_COMP_INVALID           0
#define XPATH_NODE_COMP_IS                1
#define XPATH_NODE_COMP_PRECEDES          2
#define XPATH_NODE_COMP_FOLLOWS           3

#define AXIS_INVALID                  0
#define AXIS_CHILD                    1
#define AXIS_DESCENDANT               2
#define AXIS_ATTRIBUTE                3
#define AXIS_SELF                     4
#define AXIS_DESCENDANT_OR_SELF       5
#define AXIS_FOLLOWING_SIBLING        6
#define AXIS_FOLLOWING                7
#define AXIS_NAMESPACE                8
#define AXIS_PARENT                   9
#define AXIS_ANCESTOR                 10
#define AXIS_PRECEDING_SIBLING        11
#define AXIS_PRECEDING                12
#define AXIS_ANCESTOR_OR_SELF         13
#define AXIS_COUNT                    14

typedef struct xp_expr xp_expr;
typedef struct xl_snode xl_snode;

struct xp_expr {
  int type;

  xp_expr *conditional;
  xp_expr *left;
  xp_expr *right;

  df_seqtype *seqtype;
  int compare;
  int nodetest;
  char *strval;
  int ival;
  double dval;
  qname qn;
  qname type_qname;
  int nillable;
  int axis;

  nsname ident;

  xp_expr *parent;
  xl_snode *stmt;
  char occ;
  sourceloc sloc;

  struct df_outport *outp;
};

xp_expr *xp_expr_new(int type, xp_expr *left, xp_expr *right);
xp_expr *xp_expr_parse(const char *str, const char *filename, int baseline, error_info *ei,
                       int pattern);
df_seqtype *df_seqtype_parse(const char *str, const char *filename, int baseline, error_info *ei);
void xp_expr_free(xp_expr *xp);
void xp_set_parent(xp_expr *xp, xp_expr *parent, xl_snode *stmt);
int xp_expr_resolve(xp_expr *e, xs_schema *s, const char *filename, error_info *ei);
void xp_expr_serialize(stringbuf *buf, xp_expr *e, int brackets);
void xp_expr_print_tree(xp_expr *e, int indent);

#ifndef _XSLT_XPATH_C
extern const char *xp_expr_types[XPATH_EXPR_COUNT];
extern const char *xp_axis_types[AXIS_COUNT];
#endif

#endif /* _XSLT_XPATH_H */
