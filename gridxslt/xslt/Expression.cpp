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
extern String parse_filename;

const char *Expression_types[SYNTAXTYPE_COUNT] = {
  "invalid",
  "for",
  "some",
  "every",
  "if",
  "var_in",
  "or",
  "and",






  "general-comp-eq",
  "general-comp-ne",
  "general-comp-lt",
  "general-comp-le",
  "general-comp-gt",
  "general-comp-ge",

  "value-comp-eq",
  "value-comp-ne",
  "value-comp-lt",
  "value-comp-le",
  "value-comp-gt",
  "value-comp-ge",

  "node-comp-is",
  "node-comp-precedes",
  "node-comp-follows",




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

  "declaration",
  "import",
  "instruction",
  "literal-result-element",
  "matching-substring",
  "non-matching-substring",
  "otherwise",
  "output-character",
  "param",
  "sort",
  "transform",
  "variable",
  "when",
  "with-param",

  "attribute-set",
  "character-map",
  "decimal-format",
  "function",
  "import-schema",
  "include",
  "key",
  "namespace-alias",
  "output",
  "preserve-space",
  "strip-space",
  "template",

  "analyze-string",
  "apply-imports",
  "apply-templates",
  "attribute",
  "call-template",
  "choose",
  "comment",
  "copy",
  "copy-of",
  "element",
  "fallback",
  "for-each",
  "for-each-group",
  "if",
  "message",
  "namespace",
  "next-match",
  "number",
  "perform-sort",
  "processing-instruction",
  "result-document",
  "sequence",
  "text",
  "value-of"

};

const char **xslt_element_names = Expression_types;

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

SyntaxNode::SyntaxNode(SyntaxType type)
  : m_type(type),
    m_namespaces(new NamespaceMap()),
    m_outp(NULL),
    m_parent(NULL),
    m_nrefs(0)
{
  memset(&m_refs,0,EXPR_MAXREFS*sizeof(SyntaxNode*));
}

void SyntaxNode::setParent(SyntaxNode *parent, int *importpred)
{
  m_parent = parent;

  if (parent)
    m_namespaces->parent = parent->m_namespaces;

  for (unsigned i = 0; i < m_nrefs; i++)
    if (NULL != m_refs[i])
      m_refs[i]->setParent(this,importpred);
}

int SyntaxNode::resolve(Schema *s, const String &filename, GridXSLT::Error *ei)
{
  for (unsigned i = 0; i < m_nrefs; i++)
    if (NULL != m_refs[i])
      CHECK_CALL(m_refs[i]->resolve(s,filename,ei))
  return 0;
}

SyntaxNode *SyntaxNode::resolveVar(const QName &varname)
{
  if (m_parent)
    return m_parent->resolveVar(varname);
  else
    return NULL;
}

int SyntaxNode::findReferencedVars(Compilation *comp, Function *fun, SyntaxNode *below,
                                   List<var_reference*> &vars)
{
  for (unsigned int i = 0; i < m_nrefs; i++)
    if (NULL != m_refs[i])
      CHECK_CALL(m_refs[i]->findReferencedVars(comp,fun,below,vars))
  return 0;
}

int SyntaxNode::compile(Compilation *comp, Function *fun, OutputPort **cursor)
{
  message("Compilation of syntax node type %d is not supported\n",m_type);
  ASSERT(0);
  return 0;
}

void SyntaxNode::genELC(StringBuffer *buf)
{
  message("ELC generation for syntax node type %d is not supported\n",m_type);
  ASSERT(0);
}

bool SyntaxNode::inConditional() const
{
  if (!m_parent)
    return false;
  if ((XSLT_INSTR_CHOOSE == m_parent->m_type) ||
      ((XSLT_INSTR_IF == m_parent->m_type) &&
       (this != static_cast<XSLTIfExpr*>(m_parent)->m_select)) ||
      ((XPATH_IF == m_parent->m_type) &&
       (this != static_cast<XPathIfExpr*>(m_parent)->conditional())))
    return true;
  return m_parent->inConditional();
}

bool SyntaxNode::isBelow(SyntaxNode *below) const
{
  if (this == below)
    return true;
  else if (m_parent)
    return m_parent->isBelow(below);
  else
    return false;
}

SyntaxNode::~SyntaxNode()
{
  for (unsigned i = 0; i < m_nrefs; i++)
    if (NULL != m_refs[i])
      delete m_refs[i];
  delete m_namespaces;
}

Expression::Expression(SyntaxType _type)
  : SyntaxNode(_type),
    m_stmt(NULL)
{
  m_sloc.uri = parse_filename;
  m_sloc.line = parse_firstline+lex_lineno;
}

Expression::~Expression()
{
  sourceloc_free(m_sloc);
}

void Expression::setParent(SyntaxNode *parent, int *importpred)
{
  SyntaxNode::setParent(parent,importpred);

  SyntaxNode *s = parent;
  while (s->isExpression())
    s = s->parent();

  m_stmt = static_cast<Statement*>(s);

}

int Expression::resolve(Schema *s, const String &filename, Error *ei)
{
  ASSERT(m_ident.m_name.isNull());
  ASSERT(m_ident.m_ns.isNull());

  if (!m_qn.isNull()) {
    m_ident = qname_to_nsname(m_namespaces,m_qn);
    if (m_ident.isNull())
      return error(ei,m_sloc.uri,m_sloc.line,"XPST0081",
                   "Could not resolve namespace for prefix \"%*\"",&m_qn.m_prefix);
  }

  return SyntaxNode::resolve(s,filename,ei);
}

int TypeExpr::resolve(Schema *s, const String &filename, GridXSLT::Error *ei)
{
  if (!m_st.isNull())
    CHECK_CALL(m_st.resolve(m_namespaces,s,m_sloc.uri,m_sloc.line,ei))
  return Expression::resolve(s,filename,ei);
}

void BinaryExpr::print(StringBuffer &buf)
{

  String str;
  switch (m_type) {
  case XPATH_OR:            str = "or";        break;
  case XPATH_AND:           str = "and";       break;
  case XPATH_VALUE_EQ:      str = "eq";        break;
  case XPATH_VALUE_NE:      str = "ne";        break;
  case XPATH_VALUE_LT:      str = "lt";        break;
  case XPATH_VALUE_LE:      str = "le";        break;
  case XPATH_VALUE_GT:      str = "gt";        break;
  case XPATH_VALUE_GE:      str = "ge";        break;
  case XPATH_GENERAL_EQ:    str = "=";         break;
  case XPATH_GENERAL_NE:    str = "!=";        break;
  case XPATH_GENERAL_LT:    str = "<";         break;
  case XPATH_GENERAL_LE:    str = "<=";        break;
  case XPATH_GENERAL_GT:    str = ">";         break;
  case XPATH_GENERAL_GE:    str = ">=";        break;
  case XPATH_NODE_IS:       str = "is";        break;
  case XPATH_NODE_PRECEDES: str = "<<";        break;
  case XPATH_NODE_FOLLOWS:  str = ">>";        break;
  case XPATH_TO:            str = "to";        break;
  case XPATH_ADD:           str = "+";         break;
  case XPATH_SUBTRACT:      str = "-";         break;
  case XPATH_MULTIPLY:      str = "*";         break;
  case XPATH_DIVIDE:        str = "div";       break;
  case XPATH_IDIVIDE:       str = "idiv";      break;
  case XPATH_MOD:           str = "mod";       break;
  case XPATH_UNION:         str = "union";     break;
  case XPATH_UNION2:        str = "|";         break;
  case XPATH_INTERSECT:     str = "intersect"; break;
  case XPATH_EXCEPT:        str = "except";    break;
  default:                  ASSERT(0);         break;
  }

  m_left->print(buf);
  buf.format(" %* ",&str);
  m_right->print(buf);
}

void UnaryExpr::print(StringBuffer &buf)
{
  if (XPATH_UNARY_MINUS == m_type)
    buf.format("-");
  else
    buf.format("+");
  m_source->print(buf);
}

void TypeExpr::print(StringBuffer &buf)
{
  switch (m_type) {
  case XPATH_INSTANCE_OF: {
    m_source->print(buf);
    buf.format(" instance of ");
    m_st.printXPath(buf,m_namespaces);
    break;
  }
  case XPATH_TREAT:
    /* FIXME */
    ASSERT(0);
    break;
    break;
  case XPATH_CASTABLE:
    /* FIXME */
    ASSERT(0);
    break;
    break;
  case XPATH_CAST:
    /* FIXME */
    ASSERT(0);
    break;
    break;
  default:
    ASSERT(0);
    break;
  }
}

int NodeTestExpr::resolve(Schema *s, const String &filename, GridXSLT::Error *ei)
{
  if (!m_st.isNull())
    CHECK_CALL(m_st.resolve(m_namespaces,s,m_sloc.uri,m_sloc.line,ei))
  return Expression::resolve(s,filename,ei);
}

void NodeTestExpr::print(StringBuffer &buf)
{
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
      m_st.printXPath(buf,m_namespaces);
      break;
    default:
      ASSERT(0);
      break;
    }
  }
}

void IntegerExpr::print(StringBuffer &buf)
{
  buf.format("%d",m_ival);
}

void DecimalExpr::print(StringBuffer &buf)
{
  buf.format("%f",m_dval);
}

void DoubleExpr::print(StringBuffer &buf)
{
  buf.format("%f",m_dval);
}

void StringExpr::print(StringBuffer &buf)
{
  // FIXME: quote ' (and other?) characters inside string
  buf.format("'%*'",&m_strval);
}

void QuantifiedExpr::print(StringBuffer &buf)
{
  switch (m_type) {
  case XPATH_SOME:
    buf.format("some ");
    m_conditional->print(buf);
    buf.format(" satisfies ");
    m_left->print(buf);
    break;
  case XPATH_EVERY:
    buf.format("every ");
    m_conditional->print(buf);
    buf.format(" satisfies ");
    m_left->print(buf);
    break;
  default:
    ASSERT(0);
    break;
  }
}

void ForExpr::print(StringBuffer &buf)
{
  buf.format("for ");
  m_conditional->print(buf);
  buf.format(" return ");
  m_left->print(buf);
}

void XPathIfExpr::print(StringBuffer &buf)
{
  buf.format("if (");
  m_conditional->print(buf);
  buf.format(") then ");
  m_trueExpr->print(buf);
  buf.format(" else ");
  m_falseExpr->print(buf);
}

void RootExpr::print(StringBuffer &buf)
{
  buf.format("/");
  if (NULL != m_source)
    m_source->print(buf);
}

void ContextItemExpr::print(StringBuffer &buf)
{
  buf.format(".");
}

void CallExpr::print(StringBuffer &buf)
{
  print_qname(buf,m_qn);
  buf.format("(");
  if (m_left)
    m_left->print(buf);
  buf.format(")");
}

void ParenExpr::print(StringBuffer &buf)
{
  buf.format("(");
  m_left->print(buf);
  buf.format(")");
}

void SequenceExpr::print(StringBuffer &buf)
{
  m_left->print(buf);
  buf.format(", ");
  m_right->print(buf);
}

void FilterExpr::print(StringBuffer &buf)
{
  m_left->print(buf);
  buf.format("[");
  m_right->print(buf);
  buf.format("]");
}

void StepExpr::print(StringBuffer &buf)
{
  m_left->print(buf);
  buf.format("/");
  m_right->print(buf);
}

void VarInExpr::print(StringBuffer &buf)
{
  buf.format("$");
  print_qname(buf,m_qn);
  buf.format(" in ");
  m_left->print(buf);
}

void VarInListExpr::print(StringBuffer &buf)
{
  m_left->print(buf);
  buf.format(", ");
  m_right->print(buf);
}

void ActualParamExpr::print(StringBuffer &buf)
{
  m_left->print(buf);
}

SyntaxNode *ForExpr::resolveVar(const QName &varname)
{
  // FIXME: comparison needs to be namespace aware (keep in mind prefix mappings could differ)
  if (varname.m_localPart == m_qn.m_localPart)
    return this;
  return Expression::resolveVar(varname);
}

SyntaxNode *Expression::resolveVar(const QName &varname)
{
  return SyntaxNode::resolveVar(varname);
}

void Expression::print(StringBuffer &buf)
{
/*   Expression *f; */

  switch (m_type) {
  case XPATH_INVALID:
    fmessage(stderr,"Invalid expression type\n");
    ASSERT(0);
    break;
  case XPATH_VAR_REF:
    buf.format("$");
    print_qname(buf,m_qn);
    break;
  case XPATH_EMPTY:
    break;
  case XPATH_ATOMIC_TYPE:
    ASSERT(!"not yet implemented");
    break;
  case XPATH_ITEM_TYPE:
    ASSERT(!"not yet implemented");
    break;
//   case XPATH_PARAMLIST:
//     m_left->print(buf);
//     buf.format(", ");
//     m_right->print(buf);
//     break;
  case XPATH_FILTER:
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
/*     f->print(buf); */
/*     f->filter = x; */
/*     buf.format("]"); */
/*   } */



/*   if (brackets) */
/*     buf.format(")"); */
}

void Expression::printTree(int indent)
{
  message("%p %#i%s %*\n",this,2*indent,Expression_types[m_type],&m_ident);

  for (unsigned i = 0; i < m_nrefs; i++)
    if (NULL != m_refs[i])
      static_cast<Expression*>(m_refs[i])->printTree(indent+1);
}

Expression *GridXSLT::Expression_parse(const String &str, const String &filename, int baseline, Error *ei,
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

SequenceType GridXSLT::seqtype_parse(const String &str, const String &filename, int baseline, Error *ei)
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
