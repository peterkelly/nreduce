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

#define _XSLT_XPATH_C

#include "Expression.h"
#include "Statement.h"
#include "util/Namespace.h"
#include "util/Macros.h"
#include "util/Debug.h"
#include <stdlib.h>
#include <stdio.h>
#include <ctype.h>
#include <string.h>

using namespace GridXSLT;

extern int lex_lineno;
extern int parse_firstline;
extern char *parse_filename;

const char *Expression_types[XPATH_EXPR_COUNT] = {
  "invalid",
  "for",
  "some",
  "every",
  "if",
  "var_in",
  "or",
  "and",
  "compare_values",
  "compare_general",
  "compare_nodes",
  "to",
  "add",
  "subtract",
  "multiply",
  "divide",
  "idivide",
  "mod",
  "union",
  "union2",
  "intersect",
  "except",
  "instance_of",
  "treat",
  "castable",
  "cast",
  "unary_minus",
  "unary_plus",
  "root",
  "string_literal",
  "integer_literal",
  "decimal_literal",
  "double_literal",
  "var_ref",
  "empty",
  "context_item",
  "node_test",
  "actual_param",
  "function_call",
  "paren",
  "atomic_type",
  "item_type",
  "sequence",
  "step",
  "varinlist",
  "paramlist",
  "filter",
};

const char *xp_axis_types[AXIS_COUNT] = {
  NULL,
  "child",
  "descendant",
  "attribute",
  "self",
  "descendant-or-self",
  "following-sibling",
  "following",
  "namespace",
  "parent",
  "ancestor",
  "preceding-sibling",
  "preceding",
  "ancestor-or-self",
};

static void print_qname(StringBuffer &buf, const QName &qn)
{
  /* FIXME: make sure we're printing "*" when we're supposed to (wildcards); below just supports
     the case of localpart="*" */
  if (qn.isNull())
    buf.format("*");
  else
    buf.format("%*",&qn);
}

// static void print_qname_or_double_wildcard(StringBuffer &buf, const QName &qn)
// {
//   if (!qn.m_prefix.isNull())
//     buf.format("%*:",&qn.m_prefix);
//   else
//     buf.format("*:");
//   if (!qn.m_localPart.isNull())
//     buf.format("%*",&qn.m_localPart);
//   else
//     buf.format("*");
// }

// static void print_qname_or_wildcard(StringBuffer &buf, const QName &qn)
// {
//   if (qn.m_prefix.isNull() && qn.m_localPart.isNull())
//     buf.format("*");
//   else
//     print_qname(buf,qn);
// }

Expression::Expression(int _type, Expression *_left, Expression *_right)
  : m_type(_type),
    m_conditional(NULL),
    m_left(_left),
    m_right(_right),
    m_compare(0),
    m_nodetest(0),
    m_ival(0),
    m_dval(0),
    m_nillable(0),
    m_axis(0),
    m_parent(NULL),
    m_stmt(NULL),
    m_occ('\0'),
    m_outp(NULL)
{
  m_sloc.uri = parse_filename ? strdup(parse_filename) : NULL;
  m_sloc.line = parse_firstline+lex_lineno;
}

Expression::~Expression()
{
  if (NULL != m_conditional)
    delete m_conditional;
  if (NULL != m_left)
    delete m_left;
  if (NULL != m_right)
    delete m_right;
  sourceloc_free(m_sloc);
}

void Expression::setParent(Expression *parent, Statement *stmt)
{
  m_parent = parent;
  m_stmt = stmt;

  if (NULL != m_conditional)
    m_conditional->setParent(this,m_stmt);
  if (NULL != m_left)
    m_left->setParent(this,m_stmt);
  if (NULL != m_right)
    m_right->setParent(this,m_stmt);
}

int Expression::resolve(Schema *s, const char *filename, Error *ei)
{
  ASSERT(m_stmt);

  ASSERT(m_ident.m_name.isNull());
  ASSERT(m_ident.m_ns.isNull());

  if (!m_qn.isNull()) {
    m_ident = qname_to_nsname(m_stmt->m_namespaces,m_qn);
    if (m_ident.isNull())
      return error(ei,m_sloc.uri,m_sloc.line,"XPST0081",
                   "Could not resolve namespace for prefix \"%*\"",&m_qn.m_prefix);
  }


  if (NULL != m_conditional)
    CHECK_CALL(m_conditional->resolve(s,filename,ei))
  if (NULL != m_left)
    CHECK_CALL(m_left->resolve(s,filename,ei))
  if (NULL != m_right)
    CHECK_CALL(m_right->resolve(s,filename,ei))

  if (!m_st.isNull())
    CHECK_CALL(m_st.resolve(m_stmt->m_namespaces,s,m_sloc.uri,m_sloc.line,ei))

  return 0;
}

void Expression::serialize(StringBuffer &buf, int brackets)
{
/*   Expression *f; */

  switch (m_type) {
  case XPATH_EXPR_INVALID:
    fmessage(stderr,"Invalid expression type\n");
    ASSERT(0);
    break;
  case XPATH_EXPR_FOR:
    buf.format("for ");
    m_conditional->serialize(buf,1);
    buf.format(" return ");
    m_left->serialize(buf,1);
    break;
  case XPATH_EXPR_SOME:
    buf.format("some ");
    m_conditional->serialize(buf,1);
    buf.format(" satisfies ");
    m_left->serialize(buf,1);
    break;
  case XPATH_EXPR_EVERY:
    buf.format("every ");
    m_conditional->serialize(buf,1);
    buf.format(" satisfies ");
    m_left->serialize(buf,1);
    break;
  case XPATH_EXPR_IF:
    buf.format("if (");
    m_conditional->serialize(buf,1);
    buf.format(") then ");
    m_left->serialize(buf,1);
    buf.format(" else ");
    m_right->serialize(buf,1);
    break;
  case XPATH_EXPR_VAR_IN:
    buf.format("$");
    print_qname(buf,m_qn);
    buf.format(" in ");
    m_left->serialize(buf,1);
    break;
  case XPATH_EXPR_OR:
    printBinaryExpr(buf,"or");
    break;
  case XPATH_EXPR_AND:
    printBinaryExpr(buf,"and");
    break;
  case XPATH_EXPR_COMPARE_VALUES:
    switch (m_compare) {
    case XPATH_VALUE_COMP_EQ:
      printBinaryExpr(buf,"eq");
      break;
    case XPATH_VALUE_COMP_NE:
      printBinaryExpr(buf,"ne");
      break;
    case XPATH_VALUE_COMP_LT:
      printBinaryExpr(buf,"lt");
      break;
    case XPATH_VALUE_COMP_LE:
      printBinaryExpr(buf,"le");
      break;
    case XPATH_VALUE_COMP_GT:
      printBinaryExpr(buf,"gt");
      break;
    case XPATH_VALUE_COMP_GE:
      printBinaryExpr(buf,"ge");
      break;
    default:
      ASSERT(0);
      break;
    }
    break;
  case XPATH_EXPR_COMPARE_GENERAL:
    switch (m_compare) {
    case XPATH_GENERAL_COMP_EQ:
      printBinaryExpr(buf,"=");
      break;
    case XPATH_GENERAL_COMP_NE:
      printBinaryExpr(buf,"!=");
      break;
    case XPATH_GENERAL_COMP_LT:
      printBinaryExpr(buf,"<");
      break;
    case XPATH_GENERAL_COMP_LE:
      printBinaryExpr(buf,"<=");
      break;
    case XPATH_GENERAL_COMP_GT:
      printBinaryExpr(buf,">");
      break;
    case XPATH_GENERAL_COMP_GE:
      printBinaryExpr(buf,">=");
      break;
    default:
      ASSERT(0);
      break;
    }
    break;
  case XPATH_EXPR_COMPARE_NODES:
    switch (m_compare) {
    case XPATH_NODE_COMP_IS:
      printBinaryExpr(buf,"is");
      break;
    case XPATH_NODE_COMP_PRECEDES:
      printBinaryExpr(buf,"<<");
      break;
    case XPATH_NODE_COMP_FOLLOWS:
      printBinaryExpr(buf,">>");
      break;
    default:
      ASSERT(0);
      break;
    }
    break;
  case XPATH_EXPR_TO:
    printBinaryExpr(buf,"to");
    break;
  case XPATH_EXPR_ADD:
    printBinaryExpr(buf,"+");
    break;
  case XPATH_EXPR_SUBTRACT:
    printBinaryExpr(buf,"-");
    break;
  case XPATH_EXPR_MULTIPLY:
    printBinaryExpr(buf,"*");
    break;
  case XPATH_EXPR_DIVIDE:
    printBinaryExpr(buf,"div");
    break;
  case XPATH_EXPR_IDIVIDE:
    printBinaryExpr(buf,"idiv");
    break;
  case XPATH_EXPR_MOD:
    printBinaryExpr(buf,"mod");
    break;
  case XPATH_EXPR_UNION:
    printBinaryExpr(buf,"union");
    break;
  case XPATH_EXPR_UNION2:
    printBinaryExpr(buf,"|");
    break;
  case XPATH_EXPR_INTERSECT:
    printBinaryExpr(buf,"intersect");
    break;
  case XPATH_EXPR_EXCEPT:
    printBinaryExpr(buf,"except");
    break;
  case XPATH_EXPR_INSTANCE_OF: {
    m_left->serialize(buf,0);
    buf.format(" instance of ");
    m_st.printXPath(buf,m_stmt->m_namespaces);
    break;
  }
  case XPATH_EXPR_TREAT:
    /* FIXME */
    break;
  case XPATH_EXPR_CASTABLE:
    /* FIXME */
    break;
  case XPATH_EXPR_CAST:
    /* FIXME */
    break;
  case XPATH_EXPR_UNARY_MINUS:
    buf.format("-");
    m_left->serialize(buf,1);
    break;
  case XPATH_EXPR_UNARY_PLUS:
    buf.format("+");
    m_left->serialize(buf,1);
    break;
  case XPATH_EXPR_ROOT:
    buf.format("/");
    if (NULL != m_left)
      m_left->serialize(buf,1);
    break;
  case XPATH_EXPR_STRING_LITERAL: {
    /* FIXME: quote ' (and other?) characters inside string */
    buf.format("'%*'",&m_strval);
    break;
  }
  case XPATH_EXPR_INTEGER_LITERAL:
    buf.format("%d",m_ival);
    break;
  case XPATH_EXPR_DECIMAL_LITERAL:
    buf.format("%f",m_dval);
    break;
  case XPATH_EXPR_DOUBLE_LITERAL:
    buf.format("%f",m_dval);
    break;
  case XPATH_EXPR_VAR_REF:
    buf.format("$");
    print_qname(buf,m_qn);
    break;
  case XPATH_EXPR_EMPTY:
    break;
  case XPATH_EXPR_CONTEXT_ITEM:
    buf.format(".");
    break;
  case XPATH_EXPR_NODE_TEST:

    if ((AXIS_PARENT == m_axis) &&
        (XPATH_NODE_TEST_SEQUENCETYPE == m_nodetest) &&
        (m_st.isNode())) {
      /* abbreviated form .. for parent::node() */
      buf.format("..");
    }
    else {

      switch (m_axis) {
      case AXIS_CHILD:
        /* nothing, since child is default axis */
        break;
      case AXIS_DESCENDANT:
        buf.format("descendant::");
        break;
      case AXIS_ATTRIBUTE:
        /* use abbreviation @ instead of attribute:: */
        buf.format("@");
        break;
      case AXIS_SELF:
        buf.format("self::");
        break;
      case AXIS_DESCENDANT_OR_SELF:
        buf.format("descendant-or-self::");
        break;
      case AXIS_FOLLOWING_SIBLING:
        buf.format("following-sibling::");
        break;
      case AXIS_FOLLOWING:
        buf.format("following::");
        break;
      case AXIS_NAMESPACE:
        buf.format("namespace::");
        break;
      case AXIS_PARENT:
        buf.format("parent::");
        break;
      case AXIS_ANCESTOR:
        buf.format("ancestor::");
        break;
      case AXIS_PRECEDING_SIBLING:
        buf.format("preceding-sibling::");
        break;
      case AXIS_PRECEDING:
        buf.format("preceding::");
        break;
      case AXIS_ANCESTOR_OR_SELF:
        buf.format("ancestor-or-self::");
        break;
      default:
        fmessage(stderr,"unknown axis %d\n",m_axis);
        ASSERT(0);
        break;
      }
      switch (m_nodetest) {
      case XPATH_NODE_TEST_NAME:
        print_qname(buf,m_qn);
        break;
      case XPATH_NODE_TEST_SEQUENCETYPE:
        /* FIXME */
        ASSERT(!"not yet implemented");
        break;
      default:
        ASSERT(0);
        break;
      }
    }
    break;
  case XPATH_EXPR_ACTUAL_PARAM:
    m_left->serialize(buf,0);
    break;
  case XPATH_EXPR_FUNCTION_CALL:
    print_qname(buf,m_qn);
    buf.format("(");
    if (m_left)
      m_left->serialize(buf,1);
    buf.format(")");
    break;
  case XPATH_EXPR_PAREN:
    buf.format("(");
    m_left->serialize(buf,1);
    buf.format(")");
    break;

  case XPATH_EXPR_ATOMIC_TYPE:
    ASSERT(!"not yet implemented");
    break;
  case XPATH_EXPR_ITEM_TYPE:
    ASSERT(!"not yet implemented");
    break;
  case XPATH_EXPR_SEQUENCE:
    m_left->serialize(buf,1);
    buf.format(", ");
    m_right->serialize(buf,1);
    break;
  case XPATH_EXPR_STEP:
    m_left->serialize(buf,1);
    buf.format("/");
    m_right->serialize(buf,1);
    break;
  case XPATH_EXPR_VARINLIST:
    m_left->serialize(buf,1);
    buf.format(", ");
    m_right->serialize(buf,1);
    break;
  case XPATH_EXPR_PARAMLIST:
    m_left->serialize(buf,1);
    buf.format(", ");
    m_right->serialize(buf,1);
    break;
  case XPATH_EXPR_FILTER:
    ASSERT(!"not yet implemented");
    break;

  default:
    fmessage(stderr,"Unknown expression type %d\n",m_type);
    ASSERT(0);
    break;
  }
/*   for (f = filter; f; f = f->filter) { */
/*     Expression *x = f->filter; */
/*     buf.format("["); */
/*     f->filter = NULL; */
/*     f->serialize(buf,1); */
/*     f->filter = x; */
/*     buf.format("]"); */
/*   } */



/*   if (brackets) */
/*     buf.format(")"); */
}

void Expression::printBinaryExpr(StringBuffer &buf, char *op)
{
  m_left->serialize(buf,1);
  buf.format(" %s ",op);
  m_right->serialize(buf,1);
}

void Expression::printTree(int indent)
{
  message("%p %#i%s %*\n",this,2*indent,Expression_types[m_type],&m_ident);
  if (m_conditional)
    m_conditional->printTree(indent+1);
  if (m_left)
    m_left->printTree(indent+1);
  if (m_right)
    m_right->printTree(indent+1);
}

Expression *GridXSLT::Expression_parse(const String &str, const char *filename, int baseline, Error *ei,
                       int pattern)
{
  int r;
  String source;
  Expression *expr = NULL;
  if (pattern)
    source = String::format("__just_pattern %*",&str);
  else
    source = String::format("__just_expr %*",&str);
  r = parse_xl_syntax(source,filename,baseline,ei,&expr,NULL,NULL);
  if (0 != r)
    return NULL;
  ASSERT(NULL != expr);
  return expr;
}

SequenceType GridXSLT::seqtype_parse(const String &str, const char *filename, int baseline, Error *ei)
{
  int r;
  String source = String::format("__just_seqtype %*",&str);
  SequenceTypeImpl *stimpl = NULL;
  r = parse_xl_syntax(source,filename,baseline,ei,NULL,NULL,&stimpl);
  if (0 != r)
    return SequenceType();
  ASSERT(NULL != stimpl);
  return SequenceType(stimpl);
}

/*

  @implements(xpath20:id-normative-references-1) status { info } @end
  @implements(xpath20:id-non-normative-references-1) status { info } @end
  @implements(xpath20:id-background-material-1) status { info } @end

  @implements(xpath20:id-glossary-1) status { info } @end

  @implements(xpath20:id-revisions-log-1) status { info } @end
  @implements(xpath20:N14CF6-1) status { info } @end
  @implements(xpath20:N14D62-1) status { info } @end
  @implements(xpath20:N14DA3-1) status { info } @end
  @implements(xpath20:N14DBC-1) status { info } @end
  @implements(xpath20:N14DD9-1) status { info } @end

*/
