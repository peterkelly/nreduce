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

#ifndef _XSLT_XSLT_H
#define _XSLT_XSLT_H

#include "Expression.h"
#include "xmlschema/XMLSchema.h"
#include "util/Namespace.h"
#include "util/XMLUtils.h"
#include "util/List.h"
#include "dataflow/SequenceType.h"
#include "dataflow/Serialization.h"

enum GroupMethod {
  GROUP_BY                  = 0,
  GROUP_ADJACENT            = 1,
  GROUP_STARTING_WITH       = 2,
  GROUP_ENDING_WITH         = 3
};

#define FLAG_REQUIRED                     1
#define FLAG_TUNNEL                       2
#define FLAG_OVERRIDE                     4
#define FLAG_DISABLE_OUTPUT_ESCAPING      8
#define FLAG_TERMINATE                    16

namespace GridXSLT {

class TransformExpr;

struct xslt_source {
  Schema *schema;
  TransformExpr *root;
  list *output_defs;
  list *space_decls;
};

class Statement : public SyntaxNode {
public:
  Statement(SyntaxType type);
  virtual ~Statement();

  void printTree(int indent);
  virtual int resolve(Schema *s, const String &filename, GridXSLT::Error *ei);
  virtual bool isExpression() { return false; }
  virtual SyntaxNode *resolveVar(const QName &varname);

  virtual void setParent(SyntaxNode *parent, int *importpred);
  virtual int findReferencedVars(Compilation *comp, Function *fun, SyntaxNode *below,
                                 List<var_reference*> &vars);

  Statement *m_prev;

  Statement *m_child;
  Statement *m_next;
  Statement *m_param;
  Statement *m_sort;

  int m_importpred;
};

class SCExpr : public Statement {
public:
  SCExpr(SyntaxType type) : Statement(type) { }
  virtual ~SCExpr() { }
};

class ApplyTemplatesExpr : public Statement {
public:
  ApplyTemplatesExpr(Expression *select)
    : Statement(XSLT_INSTR_APPLY_TEMPLATES), m_select(select) {
    addRef(m_select);
  }
  virtual ~ApplyTemplatesExpr() { }

  virtual int compile(Compilation *comp, Function *fun, OutputPort **cursor);

  QName m_mode;
  Expression *m_select;
};

class ApplyImportsExpr : public Statement {
public:
  ApplyImportsExpr() : Statement(XSLT_INSTR_APPLY_IMPORTS) { }
  virtual ~ApplyImportsExpr() { }
};

class ChooseExpr : public Statement {
public:
  ChooseExpr() : Statement(XSLT_INSTR_CHOOSE) { }
  virtual ~ChooseExpr() { }

  virtual int compile(Compilation *comp, Function *fun, OutputPort **cursor);
  virtual void genELC(StringBuffer *buf);
};

class ElementExpr : public Statement {
public:
  ElementExpr(Expression *name_expr, bool literal, bool includens)
    : Statement(XSLT_INSTR_ELEMENT),
      m_name_expr(name_expr), m_literal(literal), m_includens(includens) {
    addRef(m_name_expr);
  }
  virtual ~ElementExpr() { }

  virtual int compile(Compilation *comp, Function *fun, OutputPort **cursor);
  virtual void genELC(StringBuffer *buf);

  String m_ns;
  Expression *m_name_expr;
  bool m_literal;
  bool m_includens;
};

class AttributeExpr : public Statement {
public:
  AttributeExpr(Expression *name_expr, Expression *select, bool literal)
    : Statement(XSLT_INSTR_ATTRIBUTE),
      m_name_expr(name_expr), m_select(select), m_literal(literal) {
    addRef(m_name_expr);
    addRef(m_select);
  }
  virtual ~AttributeExpr() { }

  virtual int compile(Compilation *comp, Function *fun, OutputPort **cursor);
  virtual void genELC(StringBuffer *buf);

  String m_ns;
  Expression *m_name_expr;
  Expression *m_select;
  bool m_literal;
};

class NamespaceExpr : public SCExpr {
public:
  NamespaceExpr(Expression *name_expr, const String &name_const, Expression *href_expr)
    : SCExpr(XSLT_INSTR_NAMESPACE), m_name_expr(name_expr), m_select(href_expr), m_implicit(false) {
    m_strval = name_const;
    addRef(m_name_expr);
    addRef(m_select);
  }
  virtual ~NamespaceExpr() { }

  virtual int compile(Compilation *comp, Function *fun, OutputPort **cursor);
  virtual void genELC(StringBuffer *buf);

  Expression *m_name_expr;
  Expression *m_select;
  GridXSLT::String m_strval;
  bool m_implicit;
};

class PIExpr : public SCExpr {
public:
  PIExpr(Expression *name_expr, Expression *select)
    : SCExpr(XSLT_INSTR_PROCESSING_INSTRUCTION), m_name_expr(name_expr), m_select(select) {
    addRef(m_name_expr);
    addRef(m_select);
  }
  virtual ~PIExpr() { }

  Expression *m_name_expr;
  Expression *m_select;
};

class ForEachExpr : public SCExpr {
public:
  ForEachExpr(Expression *select) : SCExpr(XSLT_INSTR_FOR_EACH), m_select(select) {
    addRef(m_select);
  }
  virtual ~ForEachExpr() { }

  virtual int compile(Compilation *comp, Function *fun, OutputPort **cursor);
  virtual void genELC(StringBuffer *buf);

  Expression *m_select;
};

class XSLTIfExpr : public SCExpr {
public:
  XSLTIfExpr(Expression *select) : SCExpr(XSLT_INSTR_IF), m_select(select) {
    addRef(m_select);
  }
  virtual ~XSLTIfExpr() { }

  virtual int compile(Compilation *comp, Function *fun, OutputPort **cursor);
  virtual void genELC(StringBuffer *buf);

  Expression *m_select;
};

class TextExpr : public Statement {
public:
  TextExpr(const String &strval, bool literal)
    : Statement(XSLT_INSTR_TEXT), m_strval(strval), m_literal(literal) { }
  virtual ~TextExpr() { }

  virtual int compile(Compilation *comp, Function *fun, OutputPort **cursor);
  virtual void genELC(StringBuffer *buf);

  GridXSLT::String m_strval;
  bool m_literal;
};

class ValueOfExpr : public Statement {
public:
  ValueOfExpr(Expression *select, Expression *flags)
    : Statement(XSLT_INSTR_VALUE_OF), m_select(select), m_expr1(flags) {
    addRef(m_select);
    addRef(m_expr1);
  }
  virtual ~ValueOfExpr() { }

  virtual int compile(Compilation *comp, Function *fun, OutputPort **cursor);
  virtual void genELC(StringBuffer *buf);

  Expression *m_select;
  Expression *m_expr1;
};

class XSLTSequenceExpr : public Statement {
public:
  XSLTSequenceExpr(Expression *select) : Statement(XSLT_INSTR_SEQUENCE), m_select(select) {
    addRef(m_select);
  }
  virtual ~XSLTSequenceExpr() { }

  virtual int compile(Compilation *comp, Function *fun, OutputPort **cursor);
  virtual void genELC(StringBuffer *buf);

  Expression *m_select;
};

class VariableExpr : public SCExpr {
public:
  VariableExpr(const QName &qn, Expression *select) : SCExpr(XSLT_VARIABLE), m_select(select) {
    m_qn = qn;
    addRef(select);
  }
  virtual ~VariableExpr() { }

  virtual SyntaxNode *resolveVar(const QName &varname);
  virtual void genELC(StringBuffer *buf);

  Expression *m_select;
};

class FunctionExpr : public SCExpr {
public:
  FunctionExpr(const QName &qn) : SCExpr(XSLT_DECL_FUNCTION), m_flags(0) { m_qn = qn; }
  virtual ~FunctionExpr() { }

  virtual SyntaxNode *resolveVar(const QName &varname);
  virtual int resolve(Schema *s, const String &filename, GridXSLT::Error *ei);
  virtual void genELC(StringBuffer *buf);

  GridXSLT::SequenceType m_st;

  int m_flags;
};

class AnalyzeStringExpr : public SCExpr {
public:
  AnalyzeStringExpr(Expression *select, Expression *regex, Expression *reflags)
    : SCExpr(XSLT_INSTR_ANALYZE_STRING), m_select(select), m_expr1(regex), m_expr2(reflags) {
    addRef(m_select);
    addRef(m_expr1);
    addRef(m_expr2);
  }
  virtual ~AnalyzeStringExpr() { }

  Expression *m_select;
  Expression *m_expr1;
  Expression *m_expr2;
};

class ForEachGroupExpr : public SCExpr {
public:
  ForEachGroupExpr(GroupMethod gmethod, Expression *select, Expression *expr1)
    : SCExpr(XSLT_INSTR_FOR_EACH_GROUP), m_gmethod(gmethod), m_select(select), m_expr1(expr1) {
    addRef(select);
    addRef(expr1);
  }
  virtual ~ForEachGroupExpr() { }

  GroupMethod m_gmethod;
  Expression *m_select;
  Expression *m_expr1;
};

class CallTemplateExpr : public Statement {
public:
  CallTemplateExpr(const QName &qn) : Statement(XSLT_INSTR_CALL_TEMPLATE) { m_qn = qn; }
  virtual ~CallTemplateExpr() { }
};

class CommentExpr : public SCExpr {
public:
  CommentExpr(Expression *select) : SCExpr(XSLT_INSTR_COMMENT), m_select(select) {
    addRef(select);
  }
  virtual ~CommentExpr() { }

  Expression *m_select;
};

class CopyExpr : public SCExpr {
public:
  CopyExpr() : SCExpr(XSLT_INSTR_COPY) { }
  virtual ~CopyExpr() { }
};

class CopyOfExpr : public Statement {
public:
  CopyOfExpr(Expression *select) : Statement(XSLT_INSTR_COPY_OF), m_select(select) {
    addRef(m_select);
  }
  virtual ~CopyOfExpr() { }

  Expression *m_select;
};

class FallbackExpr : public SCExpr {
public:
  FallbackExpr() : SCExpr(XSLT_INSTR_FALLBACK) { }
  virtual ~FallbackExpr() { }
};

class MessageExpr : public SCExpr {
public:
  MessageExpr(Expression *select, bool terminate, Expression *terminateExpr)
    : SCExpr(XSLT_INSTR_MESSAGE), m_select(select), m_expr1(terminateExpr) {
    addRef(m_select);
    addRef(m_expr1);
    m_flags = 0;
    if (terminate)
      m_flags |= FLAG_TERMINATE;
  }
  virtual ~MessageExpr() { }

  int m_flags;
  Expression *m_select;
  Expression *m_expr1;
};

class NextMatchExpr : public Statement {
public:
  NextMatchExpr() : Statement(XSLT_INSTR_NEXT_MATCH) { }
  virtual ~NextMatchExpr() { }
};

class PerformSortExpr : public SCExpr {
public:
  PerformSortExpr(Expression *select) : SCExpr(XSLT_INSTR_PERFORM_SORT), m_select(select) {
    addRef(m_select);
  }
  virtual ~PerformSortExpr() { }

  Expression *m_select;
};

class TemplateExpr : public SCExpr {
public:
  TemplateExpr(const QName &qn, Expression *match)
    : SCExpr(XSLT_DECL_TEMPLATE), m_tmpl(NULL), m_select(match) {
    m_qn = qn;
    addRef(m_select);
  }
  virtual ~TemplateExpr() { }

  virtual void genELC(StringBuffer *buf);

  class Template *m_tmpl;
  Expression *m_select;
};

class ParamExpr : public SCExpr {
public:
  ParamExpr(const QName &qn, Expression *select, int flags)
    : SCExpr(XSLT_PARAM), m_flags(flags), m_select(select) {
    m_qn = qn;
    addRef(m_select);
  }
  virtual ~ParamExpr() { }

  virtual int resolve(Schema *s, const String &filename, GridXSLT::Error *ei);

  GridXSLT::SequenceType m_st;
  int m_flags;
  Expression *m_select;
};

class SortExpr : public SCExpr {
public:
  SortExpr(Expression *select) : SCExpr(XSLT_SORT), m_select(select) {
    addRef(m_select);
  }
  virtual ~SortExpr() { }

  Expression *m_select;
};

class TransformExpr : public Statement {
public:
  TransformExpr() : Statement(XSLT_TRANSFORM) { }
  virtual ~TransformExpr() { }

  virtual void genELC(StringBuffer *buf);

  String m_uri;
};

class WhenExpr : public SCExpr {
public:
  WhenExpr(Expression *test) : SCExpr(XSLT_WHEN), m_select(test) {
    addRef(m_select);
  }
  virtual ~WhenExpr() { }

  Expression *m_select;
};

class WithParamExpr : public SCExpr {
public:
  WithParamExpr(const QName &qn, Expression *select)
    : SCExpr(XSLT_WITH_PARAM), m_flags(0), m_select(select) {
    m_qn = qn;
    addRef(m_select);
  }
  virtual ~WithParamExpr() { }
  int m_flags;
  Expression *m_select;
};

class IncludeExpr : public Statement {
public:
  IncludeExpr(bool import) : Statement(import ? XSLT_IMPORT : XSLT_DECL_INCLUDE) {
    m_import = import;
  }
  virtual ~IncludeExpr() { }

protected:
  bool m_import;
  String m_uri;
};

class SpaceExpr : public Statement {
public:
  SpaceExpr(bool preserve)
    : Statement(preserve ? XSLT_DECL_PRESERVE_SPACE : XSLT_DECL_STRIP_SPACE) {
    m_preserve = preserve;
  }
  virtual ~SpaceExpr() { }

  ManagedPtrList<QNameTest*> m_qnametests;
protected:
  bool m_preserve;
};

class OutputExpr : public Statement {
public:
  OutputExpr() : Statement(XSLT_DECL_OUTPUT), m_seroptions(NULL) { }
  virtual ~OutputExpr();

  char **m_seroptions;
};

int parse_xl_syntax(const String &str, const String &filename, int baseline, GridXSLT::Error *ei,
                    Expression **expr, Statement **sn, GridXSLT::SequenceTypeImpl **st);
Statement *Statement_parse(const char *str, const String &filename, int baseline, GridXSLT::Error *ei);
Statement *xl_first_decl(Statement *root);
Statement *xl_next_decl(Statement *sn);

int xslt_parse(GridXSLT::Error *ei, const char *uri, xslt_source **source);
void xslt_source_free(xslt_source *source);
GridXSLT::df_seroptions *xslt_get_output_def(xslt_source *source, const NSName &ident);

};

#endif /* _XSLT_XSLT_H */
