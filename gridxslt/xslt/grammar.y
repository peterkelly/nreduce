%{
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

#include "Statement.h"
#include "util/Namespace.h"
#include "util/XMLUtils.h"
#include "dataflow/Program.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <assert.h>

/* FIXME: need to handle reserved words properly, e.g. using them as variable names.
This requires special handling in the lexer; see the document from the W3C on this */

using namespace GridXSLT;

extern int lex_lineno;
int yylex();
int yyerror(const char *err);

extern Statement *parse_tree;
extern Expression *parse_expr;
extern SequenceTypeImpl *parse_SequenceTypeImpl;
extern int parse_firstline;
extern char *parse_errstr;
extern Error *parse_ei;
extern String parse_filename;

%}

%union {
  Axis axis;
  char *string;
  int inumber;
  double number;
  GridXSLT::Expression *expr;
  GridXSLT::NodeTestExpr *ntexpr;
  GridXSLT::ActualParamExpr *apexpr;
  GridXSLT::ApplyTemplatesExpr *atexpr;
  GridXSLT::ParamExpr *pexpr;
  GridXSLT::FunctionExpr *fexpr;
  GridXSLT::VariableExpr *vexpr;
  GridXSLT::WithParamExpr *wpexpr;
  GridXSLT::Statement *snode;
  int flags;
  GridXSLT::parse_qname qn;
  GroupMethod gmethod;
  SyntaxType stype;
  char c;
  GridXSLT::SequenceTypeImpl *st;
}

%debug
%start XSLTTransform

%token DOTDOT
%token SLASHSLASH
%token COLONCOLON
%token AND
%token COMMENT
%token DIV
%token IDIV
%token MOD
%token NODE
%token OR
%token PROCESSING_INSTRUCTION
%token INSERT_PROCESSING_INSTRUCTION
%token TEXT

%token ANCESTOR
%token ANCESTOR_OR_SELF
%token ATTRIBUTE
%token CHILD
%token DESCENDANT
%token DESCENDANT_OR_SELF
%token FOLLOWING
%token FOLLOWING_SIBLING
%token NAMESPACE
%token PARENT
%token PRECEDING
%token PRECEDING_SIBLING
%token SELF

%token FUNCTION
%token VAR
%token FOR_EACH
%token CHOOSE
%token WHEN
%token OTHERWISE
%token CALL
%token VOID
%token NOOVERRIDE
%token REQUIRED
%token TUNNEL
%token TRANSFORM
%token IF
%token FOR_EACH_GROUP
%token BY
%token ADJACENT
%token STARTING_WITH
%token ENDING_WITH
%token MESSAGE
%token TERMINATE
%token FALLBACK
%token APPLY_TEMPLATES
%token MODE
%token TO
%token CALL_TEMPLATE
%token APPLY_IMPORTS
%token NEXT_MATCH
%token ANALYZE_STRING
%token MATCHING_SUBSTRING
%token NON_MATCHING_SUBSTRING
%token SORT
%token PERFORM_SORT
%token VALUE_OF
%token SEPARATOR
%token COPY
%token COPY_OF
%token SEQUENCE
%token GROUP
%token TEMPLATE
%token MATCH
%token JUST_EXPR
%token JUST_SEQTYPE
%token JUST_PATTERN
%token RETURN
%token FOR
%token IN
%token SOME
%token EVERY
%token SATISFIES
%token THEN
%token ELSE
%token UNION
%token INTERSECT
%token EXCEPT
%token INSTANCE
%token OF
%token TREAT
%token AS
%token CASTABLE
%token CAST
%token VALUE_EQ
%token VALUE_NE
%token VALUE_LT
%token VALUE_LE
%token VALUE_GT
%token VALUE_GE
%token SYMBOL_NE
%token SYMBOL_LE
%token SYMBOL_GE
%token IS
%token NODE_PRECEDES
%token NODE_FOLLOWS
%token DOCUMENT_NODE
%token SCHEMA_ATTRIBUTE
%token SCHEMA_ELEMENT
%token ITEM
%token ELEMENT
%token MULTIPLY
%token ID1
%token KEY

%token <string> NCNAME
%token <string> STRING_LITERAL
%token <inumber> INTEGER_LITERAL
%token <number> DECIMAL_LITERAL
%token <number> DOUBLE_LITERAL

%type <qn> QName

%type <snode> XSLTDeclarations
%type <snode> XSLTDeclaration

%type <snode> XSLTMatchingSubstring
%type <snode> XSLTNonMatchingSubstring
%type <snode> XSLTOtherwise
%type <flags> XSLTParamOptions
%type <pexpr> XSLTParamNameValueOptions
%type <pexpr> XSLTParam
%type <pexpr> XSLTParams
%type <snode> XSLTFormalParams

%type <snode> XSLTSortClause
%type <snode> XSLTSorts
%type <snode> XSLTSort
%type <snode> XSLTTransform
%type <snode> XSLTVariableNameValue
%type <snode> XSLTVariable
%type <snode> XSLTWhen
%type <snode> XSLTWhens
%type <wpexpr> XSLTWithParamNameValue
%type <wpexpr> XSLTWithParam
%type <wpexpr> XSLTWithParams
%type <fexpr> XSLTDeclFunction
%type <snode> XSLTDeclTemplate
%type <snode> XSLTInstrAnalyzeStringContent
%type <snode> XSLTInstrAnalyzeStringContents
%type <snode> XSLTInstrAnalyzeStringBlock
%type <snode> XSLTInstrAnalyzeString
%type <snode> XSLTInstrApplyImports
%type <atexpr> XSLTInstrApplyTemplatesParams
%type <atexpr> XSLTInstrApplyTemplatesMode
%type <atexpr> XSLTInstrApplyTemplates
%type <snode> XSLTInstrAttribute
%type <snode> XSLTInstrCallTemplate
%type <snode> XSLTInstrChoose
%type <snode> XSLTInstrComment
%type <snode> XSLTInstrCopy
%type <snode> XSLTInstrCopyOf
%type <snode> XSLTInstrElementName
%type <snode> XSLTInstrElement
%type <snode> XSLTInstrFallbacks
%type <snode> XSLTInstrFallback
%type <snode> XSLTInstrForEach
%type <gmethod> XSLTInstrForEachGroupMethod
%type <snode> XSLTInstrForEachGroup
%type <snode> XSLTInstrIf
%type <snode>  XSLTInstrMessageContent
%type <expr>  XSLTInstrMessageSelect
%type <snode> XSLTInstrMessage
%type <snode> XSLTInstrNamespace
%type <snode> XSLTInstrNextMatchFallbacks
%type <snode> XSLTInstrNextMatch
%type <snode> XSLTInstrPerformSort
%type <snode> XSLTInstrProcessingInstruction
%type <snode> XSLTInstrSequence
%type <snode> XSLTInstrText
%type <expr> XSLTInstrValueOfSeparator
%type <snode> XSLTInstrValueOf

%type <expr> XSLTPattern
%type <expr> XSLTPathPattern
%type <expr> XSLTRelativePathPattern
%type <expr> XSLTPatternStep
%type <axis> XSLTPatternAxis
%type <expr> XSLTIdKeyPattern
%type <expr> XSLTIdValue
%type <expr> XSLTKeyValue

%type <snode> XSLTSequenceConstructor
%type <snode> XSLTSequenceConstructors
%type <snode> XSLTSequenceConstructorBlock
%type <snode> XSLTSequenceConstructorBlockNE



%type <expr> XPathExpr
%type <expr> XPathExprSingle
%type <expr> XPathForExpr
%type <expr> XPathVarIn
%type <expr> XPathVarInList
%type <expr> XPathQuantifiedExpr
%type <expr> XPathIfExpr
%type <expr> XPathOrExpr
%type <expr> XPathAndExpr
%type <expr> XPathComparisonExpr
%type <expr> XPathRangeExpr
%type <expr> XPathAdditiveExpr
%type <expr> XPathMultiplicativeExpr
%type <expr> XPathUnionExpr
%type <expr> XPathIntersectExpr
%type <expr> XPathInstanceofExpr
%type <expr> XPathTreatExpr
%type <expr> XPathCastableExpr
%type <expr> XPathCastExpr
%type <expr> XPathUnaryExpr
%type <expr> XPathValueExpr
%type <stype> XPathGeneralComp
%type <stype> XPathValueComp
%type <stype> XPathNodeComp
%type <expr> XPathPathExpr
%type <expr> XPathRelativePathExpr
%type <expr> XPathStepExpr
%type <expr> XPathAxisStep
%type <ntexpr> XPathForwardStep
%type <axis> XPathForwardAxis
%type <ntexpr> XPathAbbrevForwardStep
%type <ntexpr> XPathReverseStep
%type <axis> XPathReverseAxis
%type <ntexpr> XPathAbbrevReverseStep
%type <ntexpr> XPathNodeTest
%type <ntexpr> XPathNameTest
%type <qn> XPathWildcard
%type <expr> XPathFilterExpr
%type <expr> XPathPredicateList
%type <expr> XPathPredicate
%type <expr> XPathPrimaryExpr
%type <expr> XPathLiteral
%type <expr> XPathNumericLiteral
%type <expr> XPathStringLiteral
%type <expr> XPathVarRef
%type <expr> XPathParenthesizedExpr
%type <expr> XPathContextItemExpr
%type <apexpr> XPathFunctionCallParams
%type <expr> XPathFunctionCall
%type <st> XPathSingleType
%type <st> XPathSequenceType
%type <c> XPathOccurrenceIndicator
%type <st> XPathItemType
%type <st> XPathAtomicType

%type <st> XPathKindTest

%type <st> XPathDocumentTest
%type <st> XPathElementTest
%type <st> XPathAttributeTest
%type <st> XPathSchemaElementTest
%type <st> XPathSchemaAttributeTest
%type <st> XPathPITest
%type <st> XPathCommentTest
%type <st> XPathTextTest
%type <st> XPathAnyKindTest

%type <qn> XPathAttribNameOrWildcard
%type <qn> XPathElementNameOrWildcard
%type <qn> XPathTypeName
%type <qn> XPathVarName

%%



























XPathExpr:
  XPathExprSingle                 { $$ = $1; }
| XPathExpr ',' XPathExprSingle   { $$ = new SequenceExpr($1,$3); }
;

XPathExprSingle:
  XPathForExpr                    { $$ = $1; }
| XPathQuantifiedExpr             { $$ = $1; }
| XPathIfExpr                     { $$ = $1; }
| XPathOrExpr                     { $$ = $1; }
;

XPathForExpr:
  FOR XPathVarInList RETURN XPathExprSingle
                                  { $$ = new ForExpr($2,$4); }
;

XPathVarIn:
  '$' XPathVarName IN XPathExprSingle  { $$ = new VarInExpr($4);
                                    $$->m_qn = $2; }
;

XPathVarInList:
  XPathVarIn                      { $$ = $1; }
| XPathVarInList ',' XPathVarIn   { $$ = new VarInListExpr($1,$3); }
;

XPathQuantifiedExpr:
  SOME XPathVarInList SATISFIES XPathExprSingle
                                  { $$ = new QuantifiedExpr(XPATH_SOME,$2,$4); }
| EVERY XPathVarInList SATISFIES XPathExprSingle
                                  { $$ = new QuantifiedExpr(XPATH_EVERY,$2,$4); }
;

XPathIfExpr:
  IF '(' XPathExpr ')' THEN XPathExprSingle ELSE XPathExprSingle
                                  { $$ = new XPathIfExpr($3,$6,$8); }
;

XPathOrExpr:
  XPathAndExpr                    { $$ = $1; }
| XPathOrExpr OR XPathAndExpr     { $$ = new BinaryExpr(XPATH_OR,$1,$3); }
;

XPathAndExpr:
  XPathComparisonExpr             { $$ = $1; }
| XPathAndExpr AND XPathComparisonExpr
                                  { $$ = new BinaryExpr(XPATH_AND,$1,$3); }
;

XPathComparisonExpr:
  XPathRangeExpr                  { $$ = $1; }
| XPathRangeExpr XPathValueComp XPathRangeExpr
                                  { $$ = new BinaryExpr($2,$1,$3); }
| XPathRangeExpr XPathGeneralComp XPathRangeExpr
                                  { $$ = new BinaryExpr($2,$1,$3); }
| XPathRangeExpr XPathNodeComp XPathRangeExpr
                                  { $$ = new BinaryExpr($2,$1,$3); }
;


XPathRangeExpr:
  XPathAdditiveExpr               { $$ = $1; }
| XPathAdditiveExpr TO XPathAdditiveExpr
                                  { $$ = new BinaryExpr(XPATH_TO,$1,$3); }
;

XPathAdditiveExpr:
  XPathMultiplicativeExpr         { $$ = $1; }
| XPathAdditiveExpr '+' XPathMultiplicativeExpr
                                  { $$ = new BinaryExpr(XPATH_ADD,$1,$3); }
| XPathAdditiveExpr '-' XPathMultiplicativeExpr
                                  { $$ = new BinaryExpr(XPATH_SUBTRACT,$1,$3); }
;

/* FIXME: need to interpret "*" as MULTIPLY operator in certain situations. Currently in
   lexer.l we only return this for the word "multiply", which isn't part of the spec
   Currently this has a shift/reduce conflict with wildcard
 */
XPathMultiplicativeExpr:
  XPathUnionExpr                  { $$ = $1; }
| XPathMultiplicativeExpr '*' XPathUnionExpr
                                  { $$ = new BinaryExpr(XPATH_MULTIPLY,$1,$3); }
| XPathMultiplicativeExpr DIV XPathUnionExpr
                                  { $$ = new BinaryExpr(XPATH_DIVIDE,$1,$3); }
| XPathMultiplicativeExpr IDIV XPathUnionExpr
                                  { $$ = new BinaryExpr(XPATH_IDIVIDE,$1,$3); }
| XPathMultiplicativeExpr MOD XPathUnionExpr
                                  { $$ = new BinaryExpr(XPATH_MOD,$1,$3); }
;

XPathUnionExpr:
  XPathIntersectExpr              { $$ = $1; }
| XPathUnionExpr UNION XPathIntersectExpr
                                  { $$ = new BinaryExpr(XPATH_UNION,$1,$3); }
| XPathUnionExpr '|' XPathIntersectExpr
                                  { $$ = new BinaryExpr(XPATH_UNION2,$1,$3); }
;

XPathIntersectExpr:
  XPathInstanceofExpr             { $$ = $1; }
| XPathIntersectExpr INTERSECT XPathInstanceofExpr
                                  { $$ = new BinaryExpr(XPATH_INTERSECT,$1,$3); }
| XPathIntersectExpr EXCEPT XPathInstanceofExpr
                                  { $$ = new BinaryExpr(XPATH_EXCEPT,$1,$3); }
;

XPathInstanceofExpr:
  XPathTreatExpr                  { $$ = $1; }
| XPathTreatExpr INSTANCE OF XPathSequenceType
                                  { $$ = new TypeExpr(XPATH_INSTANCE_OF,$4,$1); }
;

XPathTreatExpr:
  XPathCastableExpr               { $$ = $1; }
| XPathCastableExpr TREAT AS XPathSequenceType
                                  { $$ = new TypeExpr(XPATH_TREAT,$4,$1); }
;

XPathCastableExpr:
  XPathCastExpr                   { $$ = $1; }
| XPathCastExpr CASTABLE AS XPathSingleType
                                  { $$ = new TypeExpr(XPATH_CASTABLE,$4,$1); }
;

XPathCastExpr:
  XPathUnaryExpr                  { $$ = $1; }
| XPathUnaryExpr CAST AS XPathSingleType
                                  { $$ = new TypeExpr(XPATH_CAST,$4,$1); }
;

XPathUnaryExpr:
  '-' XPathUnaryExpr              { $$ = new UnaryExpr(XPATH_UNARY_MINUS,$2); }
| '+' XPathUnaryExpr              { $$ = new UnaryExpr(XPATH_UNARY_PLUS,$2); }
| XPathValueExpr                  { $$ = $1; }
;

XPathValueExpr:
  XPathPathExpr                   { $$ = $1; }
;

XPathGeneralComp:
  '='                             { $$ = XPATH_GENERAL_EQ; }
| SYMBOL_NE                       { $$ = XPATH_GENERAL_NE; }
| '<'                             { $$ = XPATH_GENERAL_LT; }
| SYMBOL_LE                       { $$ = XPATH_GENERAL_LE; }
| '>'                             { $$ = XPATH_GENERAL_GT; }
| SYMBOL_GE                       { $$ = XPATH_GENERAL_GE; }
;

XPathValueComp:
  VALUE_EQ                        { $$ = XPATH_VALUE_EQ; }
| VALUE_NE                        { $$ = XPATH_VALUE_NE; }
| VALUE_LT                        { $$ = XPATH_VALUE_LT; }
| VALUE_LE                        { $$ = XPATH_VALUE_LE; }
| VALUE_GT                        { $$ = XPATH_VALUE_GT; }
| VALUE_GE                        { $$ = XPATH_VALUE_GE; }
;

XPathNodeComp:
  IS                              { $$ = XPATH_NODE_IS; }
| NODE_PRECEDES                   { $$ = XPATH_NODE_PRECEDES; }
| NODE_FOLLOWS                    { $$ = XPATH_NODE_FOLLOWS; }
;

XPathPathExpr:
  '/'                             { $$ = new RootExpr(NULL); }
| '/' XPathRelativePathExpr       { $$ = new RootExpr($2); }
| SLASHSLASH XPathRelativePathExpr
                                  { Expression *dos;
                                    dos = new NodeTestExpr(XPATH_NODE_TEST_SEQUENCETYPE,
                                                           SequenceTypeImpl::node(),
                                                           AXIS_DESCENDANT_OR_SELF);
                                    $$ = new RootExpr(new StepExpr(dos,$2)); }
| XPathRelativePathExpr           { $$ = $1; }
;

XPathRelativePathExpr:
  XPathStepExpr                   { $$ = $1; }
| XPathRelativePathExpr '/' XPathStepExpr
                                  { $$ = new StepExpr($1,$3); }
| XPathRelativePathExpr SLASHSLASH XPathStepExpr
                                  { Expression *dos;
                                    Expression *step1;
                                    dos = new NodeTestExpr(XPATH_NODE_TEST_SEQUENCETYPE,
                                                           SequenceTypeImpl::node(),
                                                           AXIS_DESCENDANT_OR_SELF);
                                    step1 = new StepExpr(dos,$3);
                                    $$ = new StepExpr($1,step1); }
;

XPathStepExpr:
  XPathAxisStep                   { $$ = $1; }
| XPathFilterExpr                 { $$ = $1; }
;

XPathAxisStep:
  XPathForwardStep                { $$ = $1; }
| XPathForwardStep XPathPredicateList
                                  { $$ = new FilterExpr($1,$2); }
| XPathReverseStep                { $$ = $1; }
| XPathReverseStep XPathPredicateList
                                  { $$ = new FilterExpr($1,$2); }
;

XPathForwardStep:
  XPathForwardAxis XPathNodeTest  { $$ = $2;
                                    $$->m_axis = $1; }
| XPathAbbrevForwardStep          { $$ = $1; }
;

XPathForwardAxis:
  CHILD COLONCOLON                { $$ = AXIS_CHILD; }
| DESCENDANT COLONCOLON           { $$ = AXIS_DESCENDANT; }
| ATTRIBUTE COLONCOLON            { $$ = AXIS_ATTRIBUTE; }
| SELF COLONCOLON                 { $$ = AXIS_SELF; }
| DESCENDANT_OR_SELF COLONCOLON   { $$ = AXIS_DESCENDANT_OR_SELF; }
| FOLLOWING_SIBLING COLONCOLON    { $$ = AXIS_FOLLOWING_SIBLING; }
| FOLLOWING COLONCOLON            { $$ = AXIS_FOLLOWING; }
| NAMESPACE COLONCOLON            { $$ = AXIS_NAMESPACE; }
;

XPathAbbrevForwardStep:
  XPathNodeTest                   { $$ = $1;
                                    $$->m_axis = AXIS_CHILD; }
| '@' XPathNodeTest               { $$ = $2;
                                    $$->m_axis = AXIS_ATTRIBUTE; }
;

XPathReverseStep:
  XPathReverseAxis XPathNodeTest  { $$ = $2;
                                    $$->m_axis = $1; }
| XPathAbbrevReverseStep          { $$ = $1; }
;

XPathReverseAxis:
  PARENT COLONCOLON               { $$ = AXIS_PARENT; }
| ANCESTOR COLONCOLON             { $$ = AXIS_ANCESTOR; }
| PRECEDING_SIBLING COLONCOLON    { $$ = AXIS_PRECEDING_SIBLING; }
| PRECEDING COLONCOLON            { $$ = AXIS_PRECEDING; }
| ANCESTOR_OR_SELF COLONCOLON     { $$ = AXIS_ANCESTOR_OR_SELF; }
;

XPathAbbrevReverseStep:
  DOTDOT                          { $$ = new NodeTestExpr(XPATH_NODE_TEST_SEQUENCETYPE,
                                                          SequenceTypeImpl::node(),
                                                          AXIS_PARENT); }
;

XPathNodeTest:
  XPathKindTest                   { $$ = new NodeTestExpr(XPATH_NODE_TEST_SEQUENCETYPE,
                                                          $1,
                                                          AXIS_INVALID); }
| XPathNameTest                   { $$ = $1; }
;

XPathNameTest:
  QName                           { $$ = new NodeTestExpr(XPATH_NODE_TEST_NAME,
                                                          SequenceType::null(),
                                                          AXIS_INVALID);
                                    $$->m_qn = $1; }
| XPathWildcard                   { $$ = new NodeTestExpr(XPATH_NODE_TEST_NAME,
                                                          SequenceType::null(),
                                                          AXIS_INVALID);
                                    $$->m_qn = $1; }
;

XPathWildcard:
  '*'                             { $$.prefix = NULL;
                                    $$.localpart = NULL; }
| NCNAME ':' '*'                  { $$.prefix = $1;
                                    $$.localpart = NULL; }
| '*' ':' NCNAME                  { $$.prefix = NULL;
                                    $$.localpart = $3; }
;

XPathFilterExpr:
  XPathPrimaryExpr                { $$ = $1; }
| XPathPrimaryExpr XPathPredicateList
                                  { $$ = new FilterExpr($1,$2); }
;

XPathPredicateList:
  XPathPredicate                  { $$ = $1; }
| XPathPredicateList XPathPredicate
                                  { $$ = new FilterExpr($1,$2); }
;

XPathPredicate:
  '[' XPathExpr ']'               { $$ = $2; }
;

XPathPrimaryExpr:
  XPathLiteral                    { $$ = $1; }
| XPathVarRef                     { $$ = $1; }
| XPathParenthesizedExpr          { $$ = $1; }
| XPathContextItemExpr            { $$ = $1; }
| XPathFunctionCall               { $$ = $1; }
;

XPathLiteral:
  XPathNumericLiteral             { $$ = $1; }
| XPathStringLiteral              { $$ = $1; }
;

XPathNumericLiteral:
  INTEGER_LITERAL                 { $$ = new IntegerExpr($1); }
| DECIMAL_LITERAL                 { $$ = new DecimalExpr($1); }
| DOUBLE_LITERAL                  { $$ = new DoubleExpr($1); }
;

XPathStringLiteral:
  STRING_LITERAL                  { $$ = new StringExpr($1);
                                    free($1); }
;

XPathVarRef:
  '$' XPathVarName                     { $$ = new VarRefExpr($2); }
;

XPathParenthesizedExpr:
  '(' ')'                         { $$ = new EmptyExpr(); }
| '(' XPathExpr ')'               { $$ = new ParenExpr($2); }
;

XPathContextItemExpr:
  '.'                             { $$ = new ContextItemExpr(); }
;

XPathFunctionCallParams:
  XPathExprSingle                 { $$ = new ActualParamExpr($1,NULL); }
| XPathExprSingle ',' XPathFunctionCallParams
                                  { $$ = new ActualParamExpr($1,$3);
                                 /* Expression *p = new Expression(XPATH_ACTUAL_PARAM,$3,NULL);
                                    $$ = new Expression(XPATH_PARAMLIST,$1,p); */ }
;

XPathFunctionCall:
  QName '(' ')'                   { $$ = new CallExpr(NULL);
                                    $$->m_qn = $1; }
| QName '(' XPathFunctionCallParams ')'
                                  { $$ = new CallExpr($3);
                                    $$->m_qn = $1; }
;

XPathSingleType:
  XPathAtomicType                 { $$ = $1; }
| XPathAtomicType '?'             { $$ = SequenceTypeImpl::occurrence($1,OCCURS_OPTIONAL); }
;

XPathSequenceType:
  XPathItemType                   { $$ = $1; }
/*FIXME: causes shift/reduce conflict with XPathAdditiveExpr */
| XPathItemType XPathOccurrenceIndicator
                                  { $$ = SequenceTypeImpl::occurrence($1,$2); }
| VOID '(' ')'                    { $$ = new SequenceTypeImpl(SEQTYPE_EMPTY); }
;

XPathOccurrenceIndicator:
  '?'                             { $$ = OCCURS_OPTIONAL; }
| '*'                             { $$ = OCCURS_ZERO_OR_MORE; }
| '+'                             { $$ = OCCURS_ONE_OR_MORE; }
;

XPathItemType:
  XPathAtomicType                 { $$ = $1; }
| XPathKindTest                   { $$ = $1; }
| ITEM '(' ')'                    { $$ = SequenceTypeImpl::item(); }
;

XPathAtomicType:
  QName                           { $$ = new SequenceTypeImpl(new ItemType(ITEM_ATOMIC));
                                    $$->itemType()->m_typeref = $1; }
;

XPathKindTest:
  XPathDocumentTest               { $$ = $1; }
| XPathElementTest                { $$ = $1; }
| XPathAttributeTest              { $$ = $1; }
| XPathSchemaElementTest          { $$ = $1; }
| XPathSchemaAttributeTest        { $$ = $1; }
| XPathPITest                     { $$ = $1; }
| XPathCommentTest                { $$ = $1; }
| XPathTextTest                   { $$ = $1; }
| XPathAnyKindTest                { $$ = $1; }
;

XPathDocumentTest:
  DOCUMENT_NODE '(' ')'           { $$ = new SequenceTypeImpl(new ItemType(ITEM_DOCUMENT)); }
| DOCUMENT_NODE '(' XPathElementTest ')'
                                  { $$ = new SequenceTypeImpl(new ItemType(ITEM_DOCUMENT));
                                    $$->itemType()->m_content = SequenceTypeImpl::interleave($3);
                                    $$->itemType()->m_content->ref(); }
| DOCUMENT_NODE '(' XPathSchemaElementTest ')'
                                  { $$ = new SequenceTypeImpl(new ItemType(ITEM_DOCUMENT));
                                    $$->itemType()->m_content = SequenceTypeImpl::interleave($3);
                                    $$->itemType()->m_content->ref(); }
;
 
XPathElementTest:
  ELEMENT '(' ')'                 { $$ = new SequenceTypeImpl(new ItemType(ITEM_ELEMENT)); }
| ELEMENT '(' XPathElementNameOrWildcard ')'
                                  { $$ = new SequenceTypeImpl(new ItemType(ITEM_ELEMENT));
                                    $$->itemType()->m_localname = $3; }
| ELEMENT '(' XPathElementNameOrWildcard ',' XPathTypeName ')'
                                  { $$ = new SequenceTypeImpl(new ItemType(ITEM_ELEMENT));
                                    $$->itemType()->m_localname = $3;
                                    $$->itemType()->m_typeref = $5; }
| ELEMENT '(' XPathElementNameOrWildcard ',' XPathTypeName '?' ')'
                                  { $$ = new SequenceTypeImpl(new ItemType(ITEM_ELEMENT));
                                    $$->itemType()->m_localname = $3;
                                    $$->itemType()->m_typeref = $5;
                                    $$->itemType()->m_nillable = 1; }
;

XPathAttributeTest:
  ATTRIBUTE '(' ')'               { $$ = new SequenceTypeImpl(new ItemType(ITEM_ATTRIBUTE)); }
| ATTRIBUTE '(' XPathAttribNameOrWildcard ')'
                                  { $$ = new SequenceTypeImpl(new ItemType(ITEM_ATTRIBUTE));
                                    $$->itemType()->m_localname = $3; }
| ATTRIBUTE '(' XPathAttribNameOrWildcard ',' XPathTypeName ')'
                                  { $$ = new SequenceTypeImpl(new ItemType(ITEM_ATTRIBUTE));
                                    $$->itemType()->m_localname = $3;
                                    $$->itemType()->m_typeref = $5; }

XPathSchemaElementTest:
  SCHEMA_ELEMENT '(' QName ')'    { $$ = new SequenceTypeImpl(new ItemType(ITEM_ELEMENT));
                                    $$->itemType()->m_elemref = $3; }
;

XPathSchemaAttributeTest:
  SCHEMA_ATTRIBUTE '(' QName ')'  { $$ = new SequenceTypeImpl(new ItemType(ITEM_ATTRIBUTE));
                                    $$->itemType()->m_attrref = $3; }
;

XPathPITest:
  PROCESSING_INSTRUCTION '(' ')'  { $$ = new SequenceTypeImpl(new ItemType(ITEM_PI)); }
| PROCESSING_INSTRUCTION '(' NCNAME ')'
                                  { SequenceTypeImpl *st = new SequenceTypeImpl(new ItemType(ITEM_PI));
                                    st->itemType()->m_pistr = $3;
                                    free($3);
                                    $$ = SequenceTypeImpl::occurrence(st,OCCURS_OPTIONAL); }
| PROCESSING_INSTRUCTION '(' STRING_LITERAL ')'
                                  { SequenceTypeImpl *st = new SequenceTypeImpl(new ItemType(ITEM_PI));
                                    st->itemType()->m_pistr = $3;
                                    free($3);
                                    $$ = SequenceTypeImpl::occurrence(st,OCCURS_OPTIONAL); }
;

XPathCommentTest:
  COMMENT '(' ')'                 { $$ = new SequenceTypeImpl(new ItemType(ITEM_COMMENT)); }
;

XPathTextTest:
  TEXT '(' ')'                    { $$ = new SequenceTypeImpl(new ItemType(ITEM_TEXT)); }
;

XPathAnyKindTest:
  NODE '(' ')'                    { $$ = SequenceTypeImpl::node(); }
;

XPathAttribNameOrWildcard:
  QName                           { $$ = $1; }
| '*'                             { $$.prefix = NULL;
                                    $$.localpart = NULL; }
;

XPathElementNameOrWildcard:
  QName                           { $$ = $1; }
| '*'                             { $$.prefix = NULL;
                                    $$.localpart = NULL; }
;

XPathTypeName:
  QName                           { $$ = $1; }
;

XPathVarName:
  QName                           { $$ = $1; }
;





















QName:
  NCNAME                          { $$.prefix = NULL;
                                    $$.localpart = $1; }
| NCNAME ':' NCNAME               { $$.prefix = $1;
                                    $$.localpart = $3; }
;















XSLTDeclarations:
  XSLTDeclaration                 { $$ = $1; }
| XSLTDeclaration XSLTDeclarations
                                  { $$ = $1;
                                    $$->m_next = $2; }
;

XSLTDeclaration:
/*
  XSLTDeclInclude                 { $$ = NULL; }
| XSLTDeclAttributeSet            { $$ = NULL; }
| XSLTDeclCharacterMap            { $$ = NULL; }
| XSLTDeclDecimalFormat           { $$ = NULL; }

| 
*/XSLTDeclFunction                { $$ = $1; }
/*
| XSLTDeclImportSchema            { $$ = NULL; }
| XSLTDeclKey                     { $$ = NULL; }
| XSLTDeclNamespaceAlias          { $$ = NULL; }
| XSLTDeclOutput                  { $$ = NULL; }
| XSLTParam                       { $$ = NULL; }
| XSLTDeclPreserveSpace           { $$ = NULL; }
| XSLTDeclStripSpace              { $$ = NULL; }
*/
| XSLTDeclTemplate                { $$ = $1; }
/*
| XSLTVariable                    { $$ = NULL; }
*/
;

/* FIXME
XSLTImport:
;
*/

/* FIXME
XSLTInstruction:
;
*/

/* FIXME
XSLTLiteralResultElement:
;
*/

XSLTMatchingSubstring:
  MATCHING_SUBSTRING XSLTSequenceConstructorBlock
                                  { $$ = new SCExpr(XSLT_MATCHING_SUBSTRING);
                                    $$->m_child = $2; }
;

XSLTNonMatchingSubstring:
  NON_MATCHING_SUBSTRING XSLTSequenceConstructorBlock
                                  { $$ = new SCExpr(XSLT_NON_MATCHING_SUBSTRING);
                                    $$->m_child = $2; }
;

XSLTOtherwise:
  OTHERWISE XSLTSequenceConstructorBlock
                                  { $$ = new SCExpr(XSLT_OTHERWISE);
                                    $$->m_child = $2; }
;

/* FIXME
XSLTOutputCharacter:
;
*/

XSLTParamOptions:
  REQUIRED                        { $$ = FLAG_REQUIRED; }
| TUNNEL                          { $$ = FLAG_TUNNEL; }
| REQUIRED TUNNEL                 { $$ = FLAG_REQUIRED|FLAG_TUNNEL; }
| TUNNEL REQUIRED                 { $$ = FLAG_REQUIRED|FLAG_TUNNEL; }
|                                 { $$ = 0; }
;

XSLTParamNameValueOptions:
  '$' QName XSLTParamOptions      { $$ = new ParamExpr($2,NULL,$3); }
| '$' QName '=' XPathExprSingle XSLTParamOptions
                                  { $$ = new ParamExpr($2,$4,$5); }
| '$' QName '=' '{' '}' XSLTParamOptions
                                  { $$ = new ParamExpr($2,NULL,$6); }
| '$' QName '=' '{' XSLTSequenceConstructors '}' XSLTParamOptions
                                  { ParamExpr *pe = new ParamExpr($2,NULL,$7);
                                    pe->m_child = $5;
                                    $$ = pe; }
;

XSLTParam:
  XSLTParamNameValueOptions       { $$ = $1; }
| XPathSequenceType XSLTParamNameValueOptions
                                  { $$ = $2;
                                    $$->m_st = $1; }
;

XSLTParams:
  XSLTParam                       { $$ = $1; }
| XSLTParam ',' XSLTParams        { $$ = $1;
                                    $$->m_next = $3; }
;

XSLTFormalParams:
  '(' ')'                         { $$ = NULL; }
| '(' XSLTParams ')'              { $$ = $2; }
;












XSLTSortClause:
  SORT '(' XSLTSorts ')'          { $$ = $3; }
;

XSLTSorts:
  XSLTSort                        { $$ = $1; }
| XSLTSort ',' XSLTSorts          { $$ = $1;
                                    $$->m_next = $3; }
;

XSLTSort:
  XPathExprSingle                 { $$ = new SortExpr($1); }
| '{' XSLTSequenceConstructors '}'
                                  { SortExpr *se = new SortExpr(NULL);
                                    se->m_child = $2;
                                    $$ = se; }
| '{' '}'                         { $$ = new SortExpr(NULL); }
;


/* FIXME: add imports */
XSLTTransform:
  TRANSFORM '{' XSLTDeclarations '}'
                                  { TransformExpr *te = new TransformExpr();
                                    te->m_child = $3;
                                    te->m_namespaces->add_direct(XSLT_NAMESPACE,"xsl");
                                    te->m_namespaces->add_direct("urn:functions","f");
                                    $$ = te;
                                    parse_tree = te; }
| JUST_EXPR XPathExpr             { parse_expr = $2; }
| JUST_PATTERN XSLTPattern        { parse_expr = $2; }
| JUST_SEQTYPE XPathSequenceType  { parse_SequenceTypeImpl = $2; }
;

XSLTVariableNameValue:
  '$' QName                       { $$ = new VariableExpr($2,NULL); }
| '$' QName '=' XPathExprSingle   { $$ = new VariableExpr($2,$4);  }
| '$' QName '=' '{' '}'           { $$ = new VariableExpr($2,NULL); }
| '$' QName '=' '{' XSLTSequenceConstructors '}'
                                  { VariableExpr *ve = new VariableExpr($2,NULL);
                                    ve->m_child = $5;
                                    $$ = ve; }
;

XSLTVariable:
  VAR XSLTVariableNameValue ';'   { $$ = $2; }
| XPathSequenceType XSLTVariableNameValue ';'
                                  { $$ = $2; /* FIXME: type */ }
;

XSLTWhen:
  WHEN '(' XPathExpr ')' XSLTSequenceConstructorBlock
                                  { WhenExpr *we = new WhenExpr($3);
                                    we->m_child = $5;
                                    $$ = we; }
;

XSLTWhens:
  XSLTWhen                        { $$ = $1; }
| XSLTWhen XSLTWhens              { $$ = $1;
                                    $$->m_next = $2; }
;

XSLTWithParamNameValue:
  '$' QName '=' XPathExprSingle   { $$ = new WithParamExpr($2,$4); }
| '$' QName '=' '{' '}'           { $$ = new WithParamExpr($2,NULL); }
| '$' QName '=' '{' XSLTSequenceConstructors '}'
                                  { $$ = new WithParamExpr($2,NULL);
                                    $$->m_child = $5; }
;


XSLTWithParam:
  XSLTWithParamNameValue          { $$ = $1; }
| XSLTWithParamNameValue TUNNEL   { $$ = $1;
                                    $$->m_flags |= FLAG_TUNNEL; }
| XPathSequenceType XSLTWithParamNameValue
                                  { $$ = $2;
                                    /* FIXME: type */ }
| XPathSequenceType XSLTWithParamNameValue TUNNEL
                                  { $$ = $2;
                                    $$->m_flags |= FLAG_TUNNEL;
                                    /* FIXME: type */ }
;

XSLTWithParams:
  XSLTWithParam                   { $$ = $1; }
| XSLTWithParam ',' XSLTWithParams
                                  { $$ = $1;
                                    $$->m_next = $3; }
;












/* FIXME
XSLTDeclInclude:
;
*/

/* FIXME
XSLTDeclAttributeSet:
;
*/

/* FIXME
XSLTDeclCharacterMap:
;
*/

/* FIXME
XSLTDeclDecimalFormat:
;
*/

XSLTDeclFunction:
  FUNCTION QName XSLTFormalParams XSLTSequenceConstructorBlock
                                  { $$ = new FunctionExpr($2);
                                    $$->m_flags = FLAG_OVERRIDE;
                                    $$->m_param = $3;
                                    $$->m_child = $4; }
| FUNCTION QName XSLTFormalParams NOOVERRIDE XSLTSequenceConstructorBlock
                                  { $$ = new FunctionExpr($2);
                                    $$->m_param = $3;
                                    $$->m_child = $5; }
| XPathSequenceType QName XSLTFormalParams XSLTSequenceConstructorBlock
                                  { $$ = new FunctionExpr($2);
                                    $$->m_flags = FLAG_OVERRIDE;
                                    $$->m_st = $1;
                                    $$->m_param = $3;
                                    $$->m_child = $4; }
| XPathSequenceType QName XSLTFormalParams NOOVERRIDE XSLTSequenceConstructorBlock
                                  { $$ = new FunctionExpr($2);
                                    $$->m_st = $1;
                                    $$->m_param = $3;
                                    $$->m_child = $5; }
;

/* FIXME
XSLTDeclImportSchema:
;
*/

/* FIXME
XSLTDeclKey:
;
*/

/* FIXME
XSLTDeclNamespaceAlias:
;
*/

/* FIXME
XSLTDeclOutput:
;
*/

/* FIXME
XSLTDeclPreserveSpace:
;
*/

/* FIXME
XSLTDeclStripSpace:
;
*/

XSLTDeclTemplate:
  TEMPLATE QName XSLTSequenceConstructorBlock
                                  { TemplateExpr *te = new TemplateExpr($2,NULL);
                                    te->m_child = $3;
                                    $$ = te; }
| TEMPLATE QName XSLTFormalParams XSLTSequenceConstructorBlock
                                  { TemplateExpr *te = new TemplateExpr($2,NULL);
                                    te->m_param = $3;
                                    te->m_child = $4;
                                    $$ = te; }
| TEMPLATE MATCH '(' XPathExpr ')' XSLTSequenceConstructorBlock
                                  { TemplateExpr *te = new TemplateExpr(QName::null(),$4);
                                    te->m_child = $6;
                                    $$ = te; }
| TEMPLATE QName MATCH '(' XPathExpr ')' XSLTSequenceConstructorBlock
                                  { TemplateExpr *te = new TemplateExpr($2,$5);
                                    te->m_child = $7;
                                    $$ = te; }
| TEMPLATE XSLTFormalParams MATCH '(' XPathExpr ')' XSLTSequenceConstructorBlock
                                  { TemplateExpr *te = new TemplateExpr(QName::null(),$5);
                                    te->m_param = $2; 
                                    te->m_child = $7;
                                    $$ = te; }
| TEMPLATE QName XSLTFormalParams MATCH '(' XPathExpr ')' XSLTSequenceConstructorBlock
                                  { TemplateExpr *te = new TemplateExpr($2,$6);
                                    te->m_param = $3; 
                                    te->m_child = $8;
                                    $$ = te; }
;







XSLTInstrAnalyzeStringContent:
  XSLTMatchingSubstring           { $$ = $1; }
| XSLTNonMatchingSubstring        { $$ = $1; }
| XSLTInstrFallback               { $$ = $1; }
;

XSLTInstrAnalyzeStringContents:
  XSLTInstrAnalyzeStringContent   { $$ = $1; }
| XSLTInstrAnalyzeStringContent XSLTInstrAnalyzeStringContents
                                  { $$ = $1;
                                    $$->m_next = $2; }
;

XSLTInstrAnalyzeStringBlock:
  XSLTInstrAnalyzeStringContent   { $$ = $1; }
| '{' XSLTInstrAnalyzeStringContents '}'
                                  { $$ = $2; }
;

XSLTInstrAnalyzeString:
  ANALYZE_STRING '(' XPathExprSingle ',' XPathExprSingle ')' XSLTInstrAnalyzeStringBlock
                                  { $$ = new AnalyzeStringExpr($3,$5,NULL);
                                    $$->m_child = $7; }
| ANALYZE_STRING '(' XPathExprSingle ',' XPathExprSingle ',' XPathExprSingle ')' XSLTInstrAnalyzeStringBlock
                                  { $$ = new AnalyzeStringExpr($3,$5,$7);
                                    $$->m_child = $9; }
;

XSLTInstrApplyImports:
  APPLY_IMPORTS ';'               { $$ = new ApplyImportsExpr(); }
| APPLY_IMPORTS '(' ')' ';'       { $$ = new ApplyImportsExpr(); }
| APPLY_IMPORTS '(' XSLTWithParams ')' ';'
                                  { ApplyImportsExpr *ai = new ApplyImportsExpr();
                                    ai->m_param = $3;
                                    $$ = ai; }
;

XSLTInstrApplyTemplatesParams:
  APPLY_TEMPLATES                 { $$ = new ApplyTemplatesExpr(NULL); }
| APPLY_TEMPLATES '(' ')'         { $$ = new ApplyTemplatesExpr(NULL); }
| APPLY_TEMPLATES '(' XSLTWithParams ')'
                                  { $$ = new ApplyTemplatesExpr(NULL);
                                    $$->m_param = $3; }
| APPLY_TEMPLATES TO '(' XPathExpr ')'
                                  { $$ = new ApplyTemplatesExpr($4); }
| APPLY_TEMPLATES '(' ')' TO '(' XPathExpr ')'
                                  { $$ = new ApplyTemplatesExpr($6); }
| APPLY_TEMPLATES '(' XSLTWithParams ')' TO '(' XPathExpr ')'
                                  { $$ = new ApplyTemplatesExpr($7);
                                    $$->m_param = $3; }

;

XSLTInstrApplyTemplatesMode:
  XSLTInstrApplyTemplatesParams   MODE QName
                                  { $$ = $1;
                                    $$->m_mode = $3; }
| XSLTInstrApplyTemplatesParams  
                                  { $$ = $1; }
;

XSLTInstrApplyTemplates:
  XSLTInstrApplyTemplatesMode XSLTSortClause ';'
                                  { $$ = $1;
                                    $$->m_sort = $2; }
| XSLTInstrApplyTemplatesMode ';' { $$ = $1; }
;

XSLTInstrAttribute:
  '#' QName ';'                   { AttributeExpr *ae = new AttributeExpr(NULL,NULL,false);
                                    ae->m_qn = $2;
                                    $$ = ae; }
| '#' '(' XPathExpr ')' ';'       { $$ = new AttributeExpr($3,NULL,false); }
| '#' QName '(' XPathExpr')' ';'  { AttributeExpr *ae = new AttributeExpr(NULL,$4,false);
                                    ae->m_qn = $2;
                                    $$ = ae; }
| '#' '(' XPathExpr ')' '(' XPathExpr')' ';'
                                  { $$ = new AttributeExpr($3,$6,false); }
| '#' QName XSLTSequenceConstructorBlock
                                  { AttributeExpr *ae = new AttributeExpr(NULL,NULL,false);
                                    ae->m_qn = $2;
                                    ae->m_child = $3;
                                    $$ = ae; }
| '#' '(' XPathExpr ')' XSLTSequenceConstructorBlock
                                  { AttributeExpr *ae = new AttributeExpr($3,NULL,false);
                                    ae->m_child = $5;
                                    $$ = ae; }
;

XSLTInstrCallTemplate:
  CALL_TEMPLATE QName ';'         { $$ = new CallTemplateExpr($2); }
| CALL_TEMPLATE QName '(' ')' ';' { $$ = new CallTemplateExpr($2); }
| CALL_TEMPLATE QName '(' XSLTWithParams ')' ';'
                                  { CallTemplateExpr *ct = new CallTemplateExpr($2);
                                    ct->m_param = $4;
                                    $$ = ct; }
;

XSLTInstrChoose:
  CHOOSE '{' XSLTWhens '}'        { $$ = new ChooseExpr();
                                    $$->m_child = $3; }
| CHOOSE '{' XSLTWhens XSLTOtherwise '}'
                                  { Statement *o;
                                    $$ = new ChooseExpr();
                                    o = $3;
                                    assert(o);
                                    while (o->m_next)
                                      o = o->m_next;
                                    o->m_next = $4;
                                    $$->m_child = $3; }
;

XSLTInstrComment:
  COMMENT '(' XPathExpr ')' ';'   { $$ = new CommentExpr($3); }
| COMMENT XSLTSequenceConstructorBlock
                                  { CommentExpr *comment = new CommentExpr(NULL);
                                    comment->m_child = $2;
                                    $$ = comment; }
;

XSLTInstrCopy:
  COPY ';'                        { $$ = new CopyExpr(); }
| COPY XSLTSequenceConstructorBlock
                                  { CopyExpr *copy = new CopyExpr();
                                    copy->m_child = $2;
                                    $$ = copy; }
;

XSLTInstrCopyOf:
  COPY_OF '(' XPathExpr ')' ';'   { $$ = new CopyOfExpr($3); }
;

XSLTInstrElementName:
  '%' QName                       { ElementExpr *ee = new ElementExpr(NULL,true,false);
                                    ee->m_qn = $2;
                                    $$ = ee; }
| '%' '(' XPathExpr ')'           { $$ = new ElementExpr($3,false,false); }
;

XSLTInstrElement:
  XSLTInstrElementName ';'        { $$ = $1; }
| XSLTInstrElementName XSLTSequenceConstructorBlock
                                  { $$ = $1;
                                    $$->m_child = $2; }
;

XSLTInstrFallbacks:
  XSLTInstrFallback               { $$ = $1; }
| XSLTInstrFallback XSLTInstrFallbacks
                                  { $$ = $1;
                                    $$->m_next = $2; }
;

XSLTInstrFallback:
  FALLBACK XSLTSequenceConstructorBlock
                                  { FallbackExpr *fallback = new FallbackExpr();
                                    fallback->m_child = $2;
                                    $$ = fallback; }
;

XSLTInstrForEach:
  FOR_EACH '(' XPathExpr ')' XSLTSequenceConstructorBlock
                                  { $$ = new ForEachExpr($3);
                                    $$->m_child = $5; }
| FOR_EACH '(' XPathExpr ')' XSLTSortClause XSLTSequenceConstructorBlock
                                  { $$ = new ForEachExpr($3);
                                    $$->m_sort = $5;
                                    $$->m_child = $6; }
;


XSLTInstrForEachGroupMethod:
  BY                              { $$ = GROUP_BY; }
| ADJACENT                        { $$ = GROUP_ADJACENT; }
| STARTING_WITH                   { $$ = GROUP_STARTING_WITH; }
| ENDING_WITH                     { $$ = GROUP_ENDING_WITH; }
;

XSLTInstrForEachGroup:
  FOR_EACH '(' XPathExpr ')'
    GROUP XSLTInstrForEachGroupMethod '(' XPathExpr ')'
    XSLTSequenceConstructorBlock  { ForEachGroupExpr *feg = new ForEachGroupExpr($6,$3,$8); 
                                    feg->m_child = $10;
                                    $$ = feg; }
| FOR_EACH '(' XPathExpr ')'
    GROUP XSLTInstrForEachGroupMethod '(' XPathExpr ')'
    XSLTSortClause
    XSLTSequenceConstructorBlock  { ForEachGroupExpr *feg = new ForEachGroupExpr($6,$3,$8); 
                                    feg->m_sort = $10;
                                    feg->m_child = $11;
                                    $$ = feg; }
;

XSLTInstrIf:
  IF '(' XPathExpr ')' XSLTSequenceConstructorBlock
                                  { $$ = new XSLTIfExpr($3);
                                    $$->m_child = $5; }
;

XSLTInstrMessageContent:
  XSLTSequenceConstructorBlock    { $$ = $1; }
| ';'                             { $$ = NULL; }
;

XSLTInstrMessageSelect:
  MESSAGE                         { $$ = NULL; }
| MESSAGE '(' XPathExpr ')'            { $$ = $3; }
;

XSLTInstrMessage:
  XSLTInstrMessageSelect XSLTInstrMessageContent
                                  { MessageExpr *me = new MessageExpr($1,false,NULL);
                                    me->m_child = $2;
                                    $$ = me; }
| XSLTInstrMessageSelect TERMINATE XSLTInstrMessageContent
                                  { MessageExpr *me = new MessageExpr($1,true,NULL);
                                    me->m_child = $3;
                                    $$ = me; }
| XSLTInstrMessageSelect TERMINATE '(' XPathExpr ')' XSLTInstrMessageContent
                                  { MessageExpr *me = new MessageExpr($1,true,$4);
                                    me->m_child = $6;
                                    $$ = me; }
;

XSLTInstrNamespace:
  NAMESPACE NCNAME '(' XPathExpr ')' ';'
                                  { NamespaceExpr *ne = new NamespaceExpr(NULL,$2,$4);
                                    free($2);
                                    $$ = ne; }
| NAMESPACE NCNAME XSLTSequenceConstructorBlock
                                  { NamespaceExpr *ne = new NamespaceExpr(NULL,$2,NULL);
                                    free($2);
                                    ne->m_child = $3;
                                    $$ = ne; }
| NAMESPACE '(' XPathExpr ')' '(' XPathExpr ')' ';'
                                  { $$ = new NamespaceExpr($3,String::null(),$6); }
| NAMESPACE '(' XPathExpr ')' XSLTSequenceConstructorBlock
                                  { NamespaceExpr *ne = new NamespaceExpr($3,String::null(),NULL);
                                    ne->m_child = $5;
                                    $$ = ne; }
;

XSLTInstrNextMatchFallbacks:
  '{' '}'                         { $$ = NULL; }
| '{' XSLTInstrFallbacks '}'      { $$ = $2; }
| XSLTInstrFallback               { $$ = $1; }
| ';'                             { $$ = NULL; }
;

XSLTInstrNextMatch:
  NEXT_MATCH XSLTInstrNextMatchFallbacks
                                  { NextMatchExpr *nme = new NextMatchExpr();
                                    nme->m_child = $2;
                                    $$ = nme; }
| NEXT_MATCH '(' ')' XSLTInstrNextMatchFallbacks
                                  { NextMatchExpr *nme = new NextMatchExpr();
                                    nme->m_child = $4;
                                    $$ = nme; }
| NEXT_MATCH '(' XSLTWithParams ')' XSLTInstrNextMatchFallbacks
                                  { NextMatchExpr *nme = new NextMatchExpr();
                                    nme->m_param = $3;
                                    nme->m_child = $5;
                                    $$ = nme; }
;

/* FIXME
XSLTInstrNumber:
;
*/

XSLTInstrPerformSort:
  PERFORM_SORT XSLTSortClause XSLTSequenceConstructorBlock
                                  { PerformSortExpr *pse = new PerformSortExpr(NULL);
                                    pse->m_sort = $2;
                                    pse->m_child = $3;
                                    $$ = pse; }
| PERFORM_SORT '(' XPathExpr ')' XSLTSortClause XSLTInstrNextMatchFallbacks
                                  { PerformSortExpr *pse = new PerformSortExpr($3);
                                    pse->m_sort = $5;
                                    pse->m_child = $6;
                                    $$ = pse; }
;


/* FIXME: find a better syntax for this (can't use process-instruction(...) as this conflicts
  with the PI nodetest in XPAth */
XSLTInstrProcessingInstruction:
  INSERT_PROCESSING_INSTRUCTION QName '(' XPathExpr ')' ';'
                                  { PIExpr *pe = new PIExpr(NULL,$4);
                                    pe->m_qn = $2;
                                    $$ = pe; }
| INSERT_PROCESSING_INSTRUCTION '(' XPathExpr ')' '(' XPathExpr ')' ';'
                                  { $$ = new PIExpr($3,$6); }
| INSERT_PROCESSING_INSTRUCTION QName XSLTSequenceConstructorBlock
                                  { PIExpr *pe = new PIExpr(NULL,NULL);
                                    pe->m_qn = $2;
                                    pe->m_child = $3;
                                    $$ = pe; }
| INSERT_PROCESSING_INSTRUCTION '(' XPathExpr ')' XSLTSequenceConstructorBlock
                                  { PIExpr *pe = new PIExpr($3,NULL);
                                    pe->m_child = $5;
                                    $$ = pe; }
;

/* FIXME
XSLTInstrResultDocument:
;
*/

XSLTInstrSequence:
  SEQUENCE '(' XPathExpr ')' XSLTInstrNextMatchFallbacks
                                  { $$ = new XSLTSequenceExpr($3);
                                    $$->m_child = $5; }
;

XSLTInstrText:
  TEXT STRING_LITERAL ';'         { $$ = new TextExpr($2,false);
                                    free($2); }
;

XSLTInstrValueOfSeparator:
  SEPARATOR '(' XPathExpr ')'          { $$ = $3; }
| /* empty */                     { $$ = NULL; }
;

XSLTInstrValueOf:
  VALUE_OF '(' XPathExpr ')' XSLTInstrValueOfSeparator ';'
                                  { $$ = new ValueOfExpr($3,$5); }
| VALUE_OF XSLTInstrValueOfSeparator XSLTSequenceConstructorBlockNE
                                  { ValueOfExpr *voe = new ValueOfExpr(NULL,$2);
                                    voe->m_child = $3;
                                    $$ = voe; }
;













XSLTPattern:
  XSLTPathPattern                 { $$ = $1; }
| XSLTPattern '|' XSLTPathPattern { $$ = new BinaryExpr(XPATH_UNION2,$1,$3); }
;

XSLTPathPattern:
  XSLTRelativePathPattern         { $$ = $1 }
| '/'                             { $$ = new RootExpr(NULL); }
| '/' XSLTRelativePathPattern     { $$ = new RootExpr($2); }
| SLASHSLASH XSLTRelativePathPattern
                                  { Expression *dos;
                                    dos = new NodeTestExpr(XPATH_NODE_TEST_SEQUENCETYPE,
                                                           SequenceTypeImpl::node(),
                                                           AXIS_DESCENDANT_OR_SELF);
                                    $$ = new RootExpr(new StepExpr(dos,$2)); }
| XSLTIdKeyPattern                { $$ = NULL; /* FIXME */ }
| XSLTIdKeyPattern '/' XSLTRelativePathPattern
                                  { $$ = NULL; /* FIXME */ }
| XSLTIdKeyPattern SLASHSLASH XSLTRelativePathPattern
                                  { $$ = NULL; /* FIXME */ }
;

XSLTRelativePathPattern:
  XSLTPatternStep                 { $$ = $1; }
| XSLTPatternStep '/' XSLTRelativePathPattern
                                  { $$ = new StepExpr($1,$3); }

| XSLTPatternStep SLASHSLASH XSLTRelativePathPattern
                                  { Expression *dos;
                                    Expression *step1;
                                    dos = new NodeTestExpr(XPATH_NODE_TEST_SEQUENCETYPE,
                                                           SequenceTypeImpl::node(),
                                                           AXIS_DESCENDANT_OR_SELF);
                                    step1 = new StepExpr(dos,$3);
                                    $$ = new StepExpr($1,step1); }
;

XSLTPatternStep:
  XPathNodeTest                   { $1->m_axis = AXIS_CHILD;
                                    $$ = $1; }
| XPathNodeTest XPathPredicateList
                                  { FilterExpr *fe = new FilterExpr($1,$2);
                                    fe->m_axis = AXIS_CHILD;
                                    $$ = fe; }
| XSLTPatternAxis XPathNodeTest   { $2->m_axis = $1;
                                    $$ = $2; }
| XSLTPatternAxis XPathNodeTest XPathPredicateList
                                  { FilterExpr *fe = new FilterExpr($2,$3);
                                    fe->m_axis = $1;
                                    $$ = fe; }
;

XSLTPatternAxis:
  CHILD COLONCOLON                { $$ = AXIS_CHILD; }
| ATTRIBUTE COLONCOLON            { $$ = AXIS_ATTRIBUTE }
| '@'                             { $$ = AXIS_ATTRIBUTE }
;

XSLTIdKeyPattern:
  ID1 '(' XSLTIdValue ')'         { $$ = NULL; /* FIXME */ }
| KEY '(' XPathStringLiteral ',' XSLTKeyValue ')'
                                  { $$ = NULL; /* FIXME */ }
;

XSLTIdValue:
  XPathStringLiteral              { $$ = NULL; /* FIXME */ }
| XPathVarRef                     { $$ = NULL; /* FIXME */ }
;

XSLTKeyValue:
  XPathLiteral                    { $$ = NULL; /* FIXME */ }
| XPathVarRef                     { $$ = NULL; /* FIXME */ }
;













XSLTSequenceConstructor:
  XSLTVariable                    { $$ = $1; }
| XSLTInstrAnalyzeString          { $$ = $1; }
| XSLTInstrApplyImports           { $$ = $1; }
| XSLTInstrApplyTemplates         { $$ = $1; }
| XSLTInstrAttribute              { $$ = $1; }
| XSLTInstrCallTemplate           { $$ = $1; }
| XSLTInstrChoose                 { $$ = $1; }
| XSLTInstrComment                { $$ = $1; }
| XSLTInstrCopy                   { $$ = $1; }
| XSLTInstrCopyOf                 { $$ = $1; }
| XSLTInstrElement                { $$ = $1; }
| XSLTInstrFallback               { $$ = $1; }
| XSLTInstrForEach                { $$ = $1; }
| XSLTInstrForEachGroup           { $$ = $1; }
| XSLTInstrIf                     { $$ = $1; }
| XSLTInstrMessage                { $$ = $1; }
| XSLTInstrNamespace              { $$ = $1; }
| XSLTInstrNextMatch              { $$ = $1; }
/*
| XSLTInstrNumber                 { $$ = 0; }
*/
| XSLTInstrPerformSort            { $$ = $1; }
| XSLTInstrProcessingInstruction  { $$ = $1; }
/*
| XSLTInstrResultDocument         { $$ = 0; }
*/
| XSLTInstrSequence               { $$ = $1; }
| XSLTInstrText                   { $$ = $1; }
| XSLTInstrValueOf                { $$ = $1; }
;

XSLTSequenceConstructors:
  XSLTSequenceConstructor         { $$ = $1; }
| XSLTSequenceConstructor XSLTSequenceConstructors
                                  { $$ = $1;
                                    $$->m_next = $2; }
;

XSLTSequenceConstructorBlock:
  '{' XSLTSequenceConstructors '}'
                                  { $$ = $2; }
| '{' '}'                         { $$ = NULL; }
| XSLTSequenceConstructor         { $$ = $1; }
;

XSLTSequenceConstructorBlockNE:
  '{' XSLTSequenceConstructors '}'
                                  { $$ = $2; }
| XSLTSequenceConstructor         { $$ = $1; }
;

/* FIXME
XSLTImports:
  XSLTImport
| XSLTImports
;
*/

































%%

int yyerror(const char *err)
{
  if (parse_ei)
    error(parse_ei,parse_filename,parse_firstline+lex_lineno,String::null(),"%s",err);
  else if (parse_errstr)
    fmessage(stderr,"Parse error at line %d%s: %s\n",parse_firstline+lex_lineno,parse_errstr,err);
  else
    fmessage(stderr,"Parse error at line %d: %s\n",parse_firstline+lex_lineno,err);
  return 1;
}
