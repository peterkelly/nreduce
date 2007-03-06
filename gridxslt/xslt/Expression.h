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

#include "util/String.h"
#include "util/XMLUtils.h"
#include "util/Debug.h"
#include "dataflow/Program.h"
#include "dataflow/SequenceType.h"

#define EXPR_MAXREFS 8

enum SyntaxType {
  XPATH_INVALID                     = 0,
  XPATH_FOR                         = 1,
  XPATH_SOME                        = 2,
  XPATH_EVERY                       = 3,
  XPATH_IF                          = 4,
  XPATH_VAR_IN                      = 5,
  XPATH_OR                          = 6,
  XPATH_AND                         = 7,

  XPATH_GENERAL_EQ                  = 8,
  XPATH_GENERAL_NE                  = 9,
  XPATH_GENERAL_LT                  = 10,
  XPATH_GENERAL_LE                  = 11,
  XPATH_GENERAL_GT                  = 12,
  XPATH_GENERAL_GE                  = 13,

  XPATH_VALUE_EQ                    = 14,
  XPATH_VALUE_NE                    = 15,
  XPATH_VALUE_LT                    = 16,
  XPATH_VALUE_LE                    = 17,
  XPATH_VALUE_GT                    = 18,
  XPATH_VALUE_GE                    = 19,

  XPATH_NODE_IS                     = 20,
  XPATH_NODE_PRECEDES               = 21,
  XPATH_NODE_FOLLOWS                = 22,

  XPATH_TO                          = 23,
  XPATH_ADD                         = 24,
  XPATH_SUBTRACT                    = 25,
  XPATH_MULTIPLY                    = 26,
  XPATH_DIVIDE                      = 27,
  XPATH_IDIVIDE                     = 28,
  XPATH_MOD                         = 29,
  XPATH_UNION                       = 30,
  XPATH_UNION2                      = 31,
  XPATH_INTERSECT                   = 32,
  XPATH_EXCEPT                      = 33,
  XPATH_INSTANCE_OF                 = 34,
  XPATH_TREAT                       = 35,
  XPATH_CASTABLE                    = 36,
  XPATH_CAST                        = 37,
  XPATH_UNARY_MINUS                 = 38,
  XPATH_UNARY_PLUS                  = 39,
  XPATH_ROOT                        = 40,
  XPATH_STRING_LITERAL              = 41,
  XPATH_INTEGER_LITERAL             = 42,
  XPATH_DECIMAL_LITERAL             = 43,
  XPATH_DOUBLE_LITERAL              = 44,
  XPATH_VAR_REF                     = 45,
  XPATH_EMPTY                       = 46,
  XPATH_CONTEXT_ITEM                = 47,
  XPATH_NODE_TEST                   = 48,
  XPATH_ACTUAL_PARAM                = 49,
  XPATH_FUNCTION_CALL               = 50,
  XPATH_PAREN                       = 51,
  XPATH_ATOMIC_TYPE                 = 52,
  XPATH_ITEM_TYPE                   = 53,
  XPATH_SEQUENCE                    = 54,
  XPATH_STEP                        = 55,
  XPATH_VARINLIST                   = 56,
  XPATH_PARAMLIST                   = 57,
  XPATH_FILTER                      = 58,

  XSLT_DECLARATION                  = 59,
  XSLT_IMPORT                       = 60,
  XSLT_INSTRUCTION                  = 61,
  XSLT_LITERAL_RESULT_ELEMENT       = 62,
  XSLT_MATCHING_SUBSTRING           = 63,
  XSLT_NON_MATCHING_SUBSTRING       = 64,
  XSLT_OTHERWISE                    = 65,
  XSLT_OUTPUT_CHARACTER             = 66,
  XSLT_PARAM                        = 67,
  XSLT_SORT                         = 68,
  XSLT_TRANSFORM                    = 69,
  XSLT_VARIABLE                     = 70,
  XSLT_WHEN                         = 71,
  XSLT_WITH_PARAM                   = 72,

  XSLT_DECL_ATTRIBUTE_SET           = 73,
  XSLT_DECL_CHARACTER_MAP           = 74,
  XSLT_DECL_DECIMAL_FORMAT          = 75,
  XSLT_DECL_FUNCTION                = 76,
  XSLT_DECL_IMPORT_SCHEMA           = 77,
  XSLT_DECL_INCLUDE                 = 78,
  XSLT_DECL_KEY                     = 79,
  XSLT_DECL_NAMESPACE_ALIAS         = 80,
  XSLT_DECL_OUTPUT                  = 81,
  XSLT_DECL_PRESERVE_SPACE          = 82,
  XSLT_DECL_STRIP_SPACE             = 83,
  XSLT_DECL_TEMPLATE                = 84,

  XSLT_INSTR_ANALYZE_STRING         = 85,
  XSLT_INSTR_APPLY_IMPORTS          = 86,
  XSLT_INSTR_APPLY_TEMPLATES        = 87,
  XSLT_INSTR_ATTRIBUTE              = 88,
  XSLT_INSTR_CALL_TEMPLATE          = 89,
  XSLT_INSTR_CHOOSE                 = 90,
  XSLT_INSTR_COMMENT                = 91,
  XSLT_INSTR_COPY                   = 92,
  XSLT_INSTR_COPY_OF                = 93,
  XSLT_INSTR_ELEMENT                = 94,
  XSLT_INSTR_FALLBACK               = 95,
  XSLT_INSTR_FOR_EACH               = 96,
  XSLT_INSTR_FOR_EACH_GROUP         = 97,
  XSLT_INSTR_IF                     = 98,
  XSLT_INSTR_MESSAGE                = 99,
  XSLT_INSTR_NAMESPACE              = 100,
  XSLT_INSTR_NEXT_MATCH             = 101,
  XSLT_INSTR_NUMBER                 = 102,
  XSLT_INSTR_PERFORM_SORT           = 103,
  XSLT_INSTR_PROCESSING_INSTRUCTION = 104,
  XSLT_INSTR_RESULT_DOCUMENT        = 105,
  XSLT_INSTR_SEQUENCE               = 106,
  XSLT_INSTR_TEXT                   = 107,
  XSLT_INSTR_VALUE_OF               = 109,

  SYNTAXTYPE_COUNT                  = 109
};

enum NodeTest {
  XPATH_NODE_TEST_INVALID           = 0,
  XPATH_NODE_TEST_NAME              = 1,
  XPATH_NODE_TEST_SEQUENCETYPE      = 2
};

enum Axis {
  AXIS_INVALID                      = 0,
  AXIS_CHILD                        = 1,
  AXIS_DESCENDANT                   = 2,
  AXIS_ATTRIBUTE                    = 3,
  AXIS_SELF                         = 4,
  AXIS_DESCENDANT_OR_SELF           = 5,
  AXIS_FOLLOWING_SIBLING            = 6,
  AXIS_FOLLOWING                    = 7,
  AXIS_NAMESPACE                    = 8,
  AXIS_PARENT                       = 9,
  AXIS_ANCESTOR                     = 10,
  AXIS_PRECEDING_SIBLING            = 11,
  AXIS_PRECEDING                    = 12,
  AXIS_ANCESTOR_OR_SELF             = 13,
  AXIS_COUNT                        = 14
};

namespace GridXSLT {

class Statement;
class Compilation;
class var_reference;
class Expression;
class ActualParamExpr;

class SyntaxNode : public Printable {
public:
  SyntaxNode(SyntaxType type);
  virtual ~SyntaxNode();

  void addRef(SyntaxNode *sn) { m_refs[m_nrefs++] = sn; }

  virtual void setParent(SyntaxNode *parent, int *importpred);
  virtual int resolve(Schema *s, const String &filename, GridXSLT::Error *ei);
  virtual bool isExpression() = 0;
  virtual SyntaxNode *resolveVar(const QName &varname);
  virtual int findReferencedVars(Compilation *comp, Function *fun, SyntaxNode *below,
                                 List<var_reference*> &vars);
  virtual int compile(Compilation *comp, Function *fun, OutputPort **cursor);
  virtual void genELC(StringBuffer *buf);
  virtual void print(StringBuffer &buf) { ASSERT(!"Cannot print this type of syntax node"); }
  bool inConditional() const;
  bool isBelow(SyntaxNode *below) const;

  SyntaxNode *parent() const { return m_parent; }

  SyntaxType m_type;
  NSName m_ident;
  QName m_qn;
  sourceloc m_sloc;
  NamespaceMap *m_namespaces;
  class GridXSLT::OutputPort *m_outp;

  SyntaxNode *m_parent;

  SyntaxNode **refs() { return m_refs; }
  unsigned int nrefs() const { return m_nrefs; }

protected:

  SyntaxNode *m_refs[EXPR_MAXREFS];
  unsigned int m_nrefs;
};

class Expression : public SyntaxNode {
public:
  Expression(SyntaxType _type);
  virtual ~Expression();

  virtual void setParent(SyntaxNode *parent, int *importpred);
  virtual int resolve(Schema *s, const String &filename, GridXSLT::Error *ei);
  virtual bool isExpression() { return true; }
  virtual SyntaxNode *resolveVar(const QName &varname);

  virtual void print(StringBuffer &buf);
  void printTree(int indent);

  Statement *m_stmt;

};

class BinaryExpr : public Expression {
public:
  BinaryExpr(SyntaxType type, Expression *left, Expression *right)
    : Expression(type), m_left(left), m_right(right) {
    addRef(m_left);
    addRef(m_right);
  }
  virtual ~BinaryExpr() { }

  virtual int compile(Compilation *comp, Function *fun, OutputPort **cursor);
  virtual void genELC(StringBuffer *buf);
  virtual void print(StringBuffer &buf);

protected:
  Expression *m_left;
  Expression *m_right;
};

class UnaryExpr : public Expression {
public:
  UnaryExpr(SyntaxType type, Expression *source) : Expression(type), m_source(source) {
    addRef(m_source);
  }
  virtual ~UnaryExpr() { }

  virtual int compile(Compilation *comp, Function *fun, OutputPort **cursor);
  virtual void genELC(StringBuffer *buf);
  virtual void print(StringBuffer &buf);

protected:
  Expression *m_source;
};

class XPathIfExpr : public Expression {
public:
  XPathIfExpr(Expression *conditional, Expression *trueExpr, Expression *falseExpr)
    : Expression(XPATH_IF),
      m_conditional(conditional), m_trueExpr(trueExpr), m_falseExpr(falseExpr) {
    addRef(m_conditional);
    addRef(m_trueExpr);
    addRef(m_falseExpr);
  }
  virtual ~XPathIfExpr() { }

  virtual int compile(Compilation *comp, Function *fun, OutputPort **cursor);
  virtual void print(StringBuffer &buf);

  Expression *conditional() const { return m_conditional; }

protected:
  Expression *m_conditional;
  Expression *m_trueExpr;
  Expression *m_falseExpr;
};

class TypeExpr : public UnaryExpr {
public:
  TypeExpr(SyntaxType type, const SequenceType &st, Expression *source)
    : UnaryExpr(type,source), m_st(st) { }
  virtual ~TypeExpr() { }

  virtual int resolve(Schema *s, const String &filename, GridXSLT::Error *ei);
  virtual int compile(Compilation *comp, Function *fun, OutputPort **cursor);
  virtual void print(StringBuffer &buf);

  GridXSLT::SequenceType m_st;
};

class FilterExpr : public Expression {
public:
  FilterExpr(Expression *left, Expression *right)
    : Expression(XPATH_FILTER), m_left(left), m_right(right) {
    addRef(m_left);
    addRef(m_right);
  }
  virtual ~FilterExpr() { }

  virtual void print(StringBuffer &buf);
  virtual int compile(Compilation *comp, Function *fun, OutputPort **cursor);
  virtual void genELC(StringBuffer *buf);

protected:
  Expression *m_left;
  Expression *m_right;
};

class StepExpr : public Expression {
public:
  StepExpr(Expression *left, Expression *right)
    : Expression(XPATH_STEP), m_left(left), m_right(right) {
    addRef(m_left);
    addRef(m_right);
  }
  virtual ~StepExpr() { }

  virtual int compile(Compilation *comp, Function *fun, OutputPort **cursor);
  virtual void genELC(StringBuffer *buf);
  virtual void print(StringBuffer &buf);

protected:
  Expression *m_left;
  Expression *m_right;
};

class RootExpr : public UnaryExpr {
public:
  RootExpr(Expression *source) : UnaryExpr(XPATH_ROOT,source) { }
  virtual ~RootExpr() { }

  virtual int compile(Compilation *comp, Function *fun, OutputPort **cursor);
  virtual void print(StringBuffer &buf);
};

class IntegerExpr : public Expression {
public:
  IntegerExpr(int i) : Expression(XPATH_INTEGER_LITERAL), m_ival(i) { }
  virtual ~IntegerExpr() { }

  virtual int compile(Compilation *comp, Function *fun, OutputPort **cursor);
  virtual void genELC(StringBuffer *buf);
  virtual void print(StringBuffer &buf);

  int m_ival;
};

class DecimalExpr : public Expression {
public:
  DecimalExpr(double d) : Expression(XPATH_DECIMAL_LITERAL), m_dval(d) { }
  virtual ~DecimalExpr() { }

  virtual int compile(Compilation *comp, Function *fun, OutputPort **cursor);
  virtual void genELC(StringBuffer *buf);
  virtual void print(StringBuffer &buf);

  double m_dval;
};

class DoubleExpr : public Expression {
public:
  DoubleExpr(double d) : Expression(XPATH_DOUBLE_LITERAL), m_dval(d) { }
  virtual ~DoubleExpr() { }

  virtual int compile(Compilation *comp, Function *fun, OutputPort **cursor);
  virtual void genELC(StringBuffer *buf);
  virtual void print(StringBuffer &buf);

  double m_dval;
};

class StringExpr : public Expression {
public:
  StringExpr(const String &s) : Expression(XPATH_STRING_LITERAL), m_strval(s) { }
  virtual ~StringExpr() { }

  virtual int compile(Compilation *comp, Function *fun, OutputPort **cursor);
  virtual void genELC(StringBuffer *buf);
  virtual void print(StringBuffer &buf);

  GridXSLT::String m_strval;
};

class VarRefExpr : public Expression {
public:
  VarRefExpr(const QName &qn) : Expression(XPATH_VAR_REF) { m_qn = qn; }
  virtual ~VarRefExpr() { }

  virtual int compile(Compilation *comp, Function *fun, OutputPort **cursor);
  virtual int findReferencedVars(Compilation *comp, Function *fun, SyntaxNode *below,
                                 List<var_reference*> &vars);
  virtual void genELC(StringBuffer *buf);
};

class EmptyExpr : public Expression {
public:
  EmptyExpr() : Expression(XPATH_EMPTY) { }
  virtual ~EmptyExpr() { }

  virtual int compile(Compilation *comp, Function *fun, OutputPort **cursor);
};

class ContextItemExpr : public Expression {
public:
  ContextItemExpr() : Expression(XPATH_CONTEXT_ITEM) { }
  virtual ~ContextItemExpr() { }

  virtual int compile(Compilation *comp, Function *fun, OutputPort **cursor);
  virtual void genELC(StringBuffer *buf);
  virtual void print(StringBuffer &buf);
};

class NodeTestExpr : public Expression {
public:
  NodeTestExpr(NodeTest nt, const SequenceType &st, Axis axis)
    : Expression(XPATH_NODE_TEST),
      m_nodetest(nt), m_st(st), m_axis(axis) { }
  virtual ~NodeTestExpr() { }

  virtual int resolve(Schema *s, const String &filename, GridXSLT::Error *ei);
  virtual int compile(Compilation *comp, Function *fun, OutputPort **cursor);
  virtual void genELC(StringBuffer *buf);
  virtual void print(StringBuffer &buf);

  NodeTest m_nodetest;
  GridXSLT::SequenceType m_st;
  Axis m_axis;
};

class SequenceExpr : public Expression {
public:
  SequenceExpr(Expression *left, Expression *right)
    : Expression(XPATH_SEQUENCE), m_left(left), m_right(right) {
    addRef(m_left);
    addRef(m_right);
  }
  virtual ~SequenceExpr() { }

  virtual int compile(Compilation *comp, Function *fun, OutputPort **cursor);
  virtual void print(StringBuffer &buf);
  virtual void genELC(StringBuffer *buf);

protected:
  Expression *m_left;
  Expression *m_right;
};

class ParenExpr : public Expression {
public:
  ParenExpr(Expression *left) : Expression(XPATH_PAREN), m_left(left) {
    addRef(m_left);
  }
  virtual ~ParenExpr() { }

  virtual int compile(Compilation *comp, Function *fun, OutputPort **cursor);
  virtual void print(StringBuffer &buf);
  virtual void genELC(StringBuffer *buf);

protected:
  Expression *m_left;
};

class QuantifiedExpr : public Expression {
public:
  QuantifiedExpr(SyntaxType type, Expression *vars, Expression *left)
    : Expression(type), m_conditional(vars), m_left(left) {
    addRef(m_conditional);
    addRef(m_left);
  }
  virtual ~QuantifiedExpr() { }

  virtual void print(StringBuffer &buf);

  Expression *conditional() const { return m_conditional; }

protected:
  Expression *m_conditional;
  Expression *m_left;
};

class ForExpr : public Expression {
public:
  ForExpr(Expression *vars, Expression *left)
    : Expression(XPATH_FOR), m_conditional(vars), m_left(left) {
    addRef(m_conditional);
    addRef(m_left);
  }
  virtual ~ForExpr() { }

  virtual void print(StringBuffer &buf);
  virtual SyntaxNode *resolveVar(const QName &varname);

  Expression *conditional() const { return m_conditional; }

protected:
  Expression *m_conditional;
  Expression *m_left;
};

class VarInExpr : public Expression {
public:
  VarInExpr(Expression *left)
    : Expression(XPATH_VAR_IN), m_left(left) {
    addRef(m_left);
  }
  virtual ~VarInExpr() { }

  virtual void print(StringBuffer &buf);

protected:
  Expression *m_left;
};

class VarInListExpr : public Expression {
public:
  VarInListExpr(Expression *left, Expression *right)
    : Expression(XPATH_VARINLIST), m_left(left), m_right(right) {
    addRef(m_left);
    addRef(m_right);
  }
  virtual ~VarInListExpr() { }

  virtual void print(StringBuffer &buf);

protected:
  Expression *m_left;
  Expression *m_right;
};

class ActualParamExpr : public Expression {
public:
  ActualParamExpr(Expression *left, ActualParamExpr *right)
    : Expression(XPATH_ACTUAL_PARAM), m_left(left), m_right(right) {
    addRef(m_left);
    addRef(m_right);
  }
  virtual ~ActualParamExpr() { }

  virtual void print(StringBuffer &buf);

  Expression *left() const { return m_left; }
  ActualParamExpr *right() const { return m_right; }

protected:
  Expression *m_left;
  ActualParamExpr *m_right;
};

class CallExpr : public Expression {
public:
  CallExpr(ActualParamExpr *params)
    : Expression(XPATH_FUNCTION_CALL), m_left(params) {
    addRef(m_left);
  }
  virtual ~CallExpr() { }

  virtual int compile(Compilation *comp, Function *fun, OutputPort **cursor);
  virtual void print(StringBuffer &buf);
  virtual void genELC(StringBuffer *buf);

protected:
  ActualParamExpr *m_left;
};

Expression *Expression_parse(const String &str, const String &filename, int baseline, Error *ei,
                       int pattern);
SequenceType seqtype_parse(const String &str, const String &filename, int baseline, Error *ei);

};

#ifndef _XSLT_XPATH_C
extern const char *Expression_types[SYNTAXTYPE_COUNT];
extern const char **xslt_element_names;
extern const char *xp_axis_types[AXIS_COUNT];
#endif

#endif /* _XSLT_XPATH_H */
