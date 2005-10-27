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
#include "util/namespace.h"
#include "util/macros.h"
#include "util/debug.h"
#include <stdlib.h>
#include <stdio.h>
#include <assert.h>
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

void print_qname(stringbuf *buf, const QName &qn)
{
  /* FIXME: make sure we're printing "*" when we're supposed to (wildcards); below just supports
     the case of localpart="*" */
  if (qn.isNull())
    stringbuf_format(buf,"*");
  else
    stringbuf_format(buf,"%*",&qn);
}

void print_qname_or_double_wildcard(stringbuf *buf, const QName &qn)
{
  if (!qn.m_prefix.isNull())
    stringbuf_format(buf,"%*:",&qn.m_prefix);
  else
    stringbuf_format(buf,"*:");
  if (!qn.m_localPart.isNull())
    stringbuf_format(buf,"%*",&qn.m_localPart);
  else
    stringbuf_format(buf,"*");
}

void print_qname_or_wildcard(stringbuf *buf, const QName &qn)
{
  if (qn.m_prefix.isNull() && qn.m_localPart.isNull())
    stringbuf_format(buf,"*");
  else
    print_qname(buf,qn);
}

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
  assert(m_stmt);

  assert(m_ident.m_name.isNull());
  assert(m_ident.m_ns.isNull());

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

void Expression::serialize(stringbuf *buf, int brackets)
{
/*   Expression *f; */

  switch (m_type) {
  case XPATH_EXPR_INVALID:
    fmessage(stderr,"Invalid expression type\n");
    assert(0);
    break;
  case XPATH_EXPR_FOR:
    stringbuf_format(buf,"for ");
    m_conditional->serialize(buf,1);
    stringbuf_format(buf," return ");
    m_left->serialize(buf,1);
    break;
  case XPATH_EXPR_SOME:
    stringbuf_format(buf,"some ");
    m_conditional->serialize(buf,1);
    stringbuf_format(buf," satisfies ");
    m_left->serialize(buf,1);
    break;
  case XPATH_EXPR_EVERY:
    stringbuf_format(buf,"every ");
    m_conditional->serialize(buf,1);
    stringbuf_format(buf," satisfies ");
    m_left->serialize(buf,1);
    break;
  case XPATH_EXPR_IF:
    stringbuf_format(buf,"if (");
    m_conditional->serialize(buf,1);
    stringbuf_format(buf,") then ");
    m_left->serialize(buf,1);
    stringbuf_format(buf," else ");
    m_right->serialize(buf,1);
    break;
  case XPATH_EXPR_VAR_IN:
    stringbuf_format(buf,"$");
    print_qname(buf,m_qn);
    stringbuf_format(buf," in ");
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
      assert(0);
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
      assert(0);
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
      assert(0);
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
    stringbuf_format(buf," instance of ");
    StringBuffer sb;
    m_st.printXPath(sb,m_stmt->m_namespaces);
    char *str = sb.contents().cstring();
    stringbuf_format(buf,"%s",str);
    free(str);
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
    stringbuf_format(buf,"-");
    m_left->serialize(buf,1);
    break;
  case XPATH_EXPR_UNARY_PLUS:
    stringbuf_format(buf,"+");
    m_left->serialize(buf,1);
    break;
  case XPATH_EXPR_ROOT:
    stringbuf_format(buf,"/");
    if (NULL != m_left)
      m_left->serialize(buf,1);
    break;
  case XPATH_EXPR_STRING_LITERAL: {
    /* FIXME: quote ' (and other?) characters inside string */
    char *strval = m_strval.cstring();
    stringbuf_format(buf,"'%s'",strval);
    free(strval);
    break;
  }
  case XPATH_EXPR_INTEGER_LITERAL:
    stringbuf_format(buf,"%d",m_ival);
    break;
  case XPATH_EXPR_DECIMAL_LITERAL:
    stringbuf_format(buf,"%f",m_dval);
    break;
  case XPATH_EXPR_DOUBLE_LITERAL:
    stringbuf_format(buf,"%f",m_dval);
    break;
  case XPATH_EXPR_VAR_REF:
    stringbuf_format(buf,"$");
    print_qname(buf,m_qn);
    break;
  case XPATH_EXPR_EMPTY:
    break;
  case XPATH_EXPR_CONTEXT_ITEM:
    stringbuf_format(buf,".");
    break;
  case XPATH_EXPR_NODE_TEST:

    if ((AXIS_PARENT == m_axis) &&
        (XPATH_NODE_TEST_SEQUENCETYPE == m_nodetest) &&
        (m_st.isNode())) {
      /* abbreviated form .. for parent::node() */
      stringbuf_format(buf,"..");
    }
    else {

      switch (m_axis) {
      case AXIS_CHILD:
        /* nothing, since child is default axis */
        break;
      case AXIS_DESCENDANT:
        stringbuf_format(buf,"descendant::");
        break;
      case AXIS_ATTRIBUTE:
        /* use abbreviation @ instead of attribute:: */
        stringbuf_format(buf,"@");
        break;
      case AXIS_SELF:
        stringbuf_format(buf,"self::");
        break;
      case AXIS_DESCENDANT_OR_SELF:
        stringbuf_format(buf,"descendant-or-self::");
        break;
      case AXIS_FOLLOWING_SIBLING:
        stringbuf_format(buf,"following-sibling::");
        break;
      case AXIS_FOLLOWING:
        stringbuf_format(buf,"following::");
        break;
      case AXIS_NAMESPACE:
        stringbuf_format(buf,"namespace::");
        break;
      case AXIS_PARENT:
        stringbuf_format(buf,"parent::");
        break;
      case AXIS_ANCESTOR:
        stringbuf_format(buf,"ancestor::");
        break;
      case AXIS_PRECEDING_SIBLING:
        stringbuf_format(buf,"preceding-sibling::");
        break;
      case AXIS_PRECEDING:
        stringbuf_format(buf,"preceding::");
        break;
      case AXIS_ANCESTOR_OR_SELF:
        stringbuf_format(buf,"ancestor-or-self::");
        break;
      default:
        fmessage(stderr,"unknown axis %d\n",m_axis);
        assert(0);
        break;
      }
      switch (m_nodetest) {
      case XPATH_NODE_TEST_NAME:
        print_qname(buf,m_qn);
        break;
      case XPATH_NODE_TEST_SEQUENCETYPE:
        /* FIXME */
        assert(!"not yet implemented");
        break;
      default:
        assert(0);
        break;
      }
    }
    break;
  case XPATH_EXPR_ACTUAL_PARAM:
    m_left->serialize(buf,0);
    break;
  case XPATH_EXPR_FUNCTION_CALL:
    print_qname(buf,m_qn);
    stringbuf_format(buf,"(");
    if (m_left)
      m_left->serialize(buf,1);
    stringbuf_format(buf,")");
    break;
  case XPATH_EXPR_PAREN:
    stringbuf_format(buf,"(");
    m_left->serialize(buf,1);
    stringbuf_format(buf,")");
    break;

  case XPATH_EXPR_ATOMIC_TYPE:
    assert(!"not yet implemented");
    break;
  case XPATH_EXPR_ITEM_TYPE:
    assert(!"not yet implemented");
    break;
  case XPATH_EXPR_SEQUENCE:
    m_left->serialize(buf,1);
    stringbuf_format(buf,", ");
    m_right->serialize(buf,1);
    break;
  case XPATH_EXPR_STEP:
    m_left->serialize(buf,1);
    stringbuf_format(buf,"/");
    m_right->serialize(buf,1);
    break;
  case XPATH_EXPR_VARINLIST:
    m_left->serialize(buf,1);
    stringbuf_format(buf,", ");
    m_right->serialize(buf,1);
    break;
  case XPATH_EXPR_PARAMLIST:
    m_left->serialize(buf,1);
    stringbuf_format(buf,", ");
    m_right->serialize(buf,1);
    break;
  case XPATH_EXPR_FILTER:
    assert(!"not yet implemented");
    break;

  default:
    fmessage(stderr,"Unknown expression type %d\n",m_type);
    assert(0);
    break;
  }
/*   for (f = filter; f; f = f->filter) { */
/*     Expression *x = f->filter; */
/*     stringbuf_format(buf,"["); */
/*     f->filter = NULL; */
/*     f->serialize(buf,1); */
/*     f->filter = x; */
/*     stringbuf_format(buf,"]"); */
/*   } */



/*   if (brackets) */
/*     stringbuf_format(buf,")"); */
}

void Expression::printBinaryExpr(stringbuf *buf, char *op)
{
  m_left->serialize(buf,1);
  stringbuf_format(buf," %s ",op);
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

Expression *GridXSLT::Expression_parse(const char *str, const char *filename, int baseline, Error *ei,
                       int pattern)
{
  int r;
  char *source = (char*)malloc(strlen("__just_pattern ")+strlen(str)+1);
  Expression *expr = NULL;
  if (pattern)
    sprintf(source,"__just_pattern %s",str);
  else
    sprintf(source,"__just_expr %s",str);
  r = parse_xl_syntax(source,filename,baseline,ei,&expr,NULL,NULL);
  free(source);
  if (0 != r)
    return NULL;
  assert(NULL != expr);
  return expr;
}

SequenceType GridXSLT::seqtype_parse(const char *str, const char *filename, int baseline, Error *ei)
{
  int r;
  char *source = (char*)malloc(strlen("__just_seqtype ")+strlen(str)+1);
  SequenceTypeImpl *stimpl = NULL;
  sprintf(source,"__just_seqtype %s",str);
  r = parse_xl_syntax(source,filename,baseline,ei,NULL,NULL,&stimpl);
  free(source);
  if (0 != r)
    return SequenceType();
  assert(NULL != stimpl);
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
