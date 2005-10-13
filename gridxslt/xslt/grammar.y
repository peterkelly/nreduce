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

#include "xslt.h"
#include "util/namespace.h"
#include "util/xmlutils.h"
#include "dataflow/dataflow.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <assert.h>

extern int lex_lineno;
int yylex();
int yyerror(const char *err);

extern xl_snode *parse_tree;
extern xp_expr *parse_expr;
extern df_seqtype *parse_seqtype;
extern int parse_firstline;
extern char *parse_errstr;
extern error_info *parse_ei;
extern char *parse_filename;

%}

%union {
  int axis;
  char *string;
  double number;
  xp_expr *expr;
  xl_snode *snode;
  int flags;
  qname qn;
  int gmethod;
  int compare;
  char c;
  df_seqtype *st;
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
%token OVERRIDE
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
%token <number> INTEGER_LITERAL
%token <number> DECIMAL_LITERAL
%token <number> DOUBLE_LITERAL

%type <qn> QName

%type <snode> XSLTDeclarations
%type <snode> XSLTDeclaration

%type <snode> XSLTMatchingSubstring
%type <snode> XSLTNonMatchingSubstring
%type <snode> XSLTOtherwise
%type <flags> XSLTParamOptions
%type <snode> XSLTParamNameValueOptions
%type <snode> XSLTParam
%type <snode> XSLTParams
%type <snode> XSLTFormalParams

%type <snode> XSLTSortClause
%type <snode> XSLTSorts
%type <snode> XSLTSort
%type <snode> XSLTTransform
%type <snode> XSLTVariableNameValue
%type <snode> XSLTVariable
%type <snode> XSLTWhen
%type <snode> XSLTWhens
%type <snode> XSLTWithParamNameValue
%type <snode> XSLTWithParam
%type <snode> XSLTWithParams
%type <snode> XSLTDeclFunction
%type <snode> XSLTDeclTemplate
%type <snode> XSLTInstrAnalyzeStringContent
%type <snode> XSLTInstrAnalyzeStringContents
%type <snode> XSLTInstrAnalyzeStringBlock
%type <snode> XSLTInstrAnalyzeString
%type <snode> XSLTInstrApplyImports
%type <snode> XSLTInstrApplyTemplatesParams
%type <snode> XSLTInstrApplyTemplatesParamsTo
%type <snode> XSLTInstrApplyTemplatesMode
%type <snode> XSLTInstrApplyTemplates
%type <snode> XSLTInstrAttributeName
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
%type <snode> XSLTInstrProcessingInstructionName
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
%type <compare> XPathGeneralComp
%type <compare> XPathValueComp
%type <compare> XPathNodeComp
%type <expr> XPathPathExpr
%type <expr> XPathRelativePathExpr
%type <expr> XPathStepExpr
%type <expr> XPathAxisStep
%type <expr> XPathForwardStep
%type <axis> XPathForwardAxis
%type <expr> XPathAbbrevForwardStep
%type <expr> XPathReverseStep
%type <axis> XPathReverseAxis
%type <expr> XPathAbbrevReverseStep
%type <expr> XPathNodeTest
%type <expr> XPathNameTest
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
%type <expr> XPathFunctionCallParams
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
| XPathExpr ',' XPathExprSingle   { $$ = xp_expr_new(XPATH_EXPR_SEQUENCE,$1,$3); }
;

XPathExprSingle:
  XPathForExpr                    { $$ = $1; }
| XPathQuantifiedExpr             { $$ = $1; }
| XPathIfExpr                     { $$ = $1; }
| XPathOrExpr                     { $$ = $1; }
;

XPathForExpr:
  FOR XPathVarInList RETURN XPathExprSingle
                                  { $$ = xp_expr_new(XPATH_EXPR_FOR,$4,NULL);
                                    $$->conditional = $2; }
;

XPathVarIn:
  '$' XPathVarName IN XPathExprSingle  { $$ = xp_expr_new(XPATH_EXPR_VAR_IN,$4,NULL);
                                    $$->qn = $2; }
;

XPathVarInList:
  XPathVarIn                      { $$ = $1; }
| XPathVarInList ',' XPathVarIn   { $$ = xp_expr_new(XPATH_EXPR_VARINLIST,$1,$3); }
;

XPathQuantifiedExpr:
  SOME XPathVarInList SATISFIES XPathExprSingle
                                  { $$ = xp_expr_new(XPATH_EXPR_SOME,$4,NULL);
                                    $$->conditional = $2; }
| EVERY XPathVarInList SATISFIES XPathExprSingle
                                  { $$ = xp_expr_new(XPATH_EXPR_EVERY,$4,NULL);
                                    $$->conditional = $2; }
;

XPathIfExpr:
  IF '(' XPathExpr ')' THEN XPathExprSingle ELSE XPathExprSingle
                                  { $$ = xp_expr_new(XPATH_EXPR_IF,$6,$8);
                                    $$->conditional = $3; }
;

XPathOrExpr:
  XPathAndExpr                    { $$ = $1; }
| XPathOrExpr OR XPathAndExpr     { $$ = xp_expr_new(XPATH_EXPR_OR,$1,$3); }
;

XPathAndExpr:
  XPathComparisonExpr             { $$ = $1; }
| XPathAndExpr AND XPathComparisonExpr
                                  { $$ = xp_expr_new(XPATH_EXPR_AND,$1,$3); }
;

XPathComparisonExpr:
  XPathRangeExpr                  { $$ = $1; }
| XPathRangeExpr XPathValueComp XPathRangeExpr
                                  { $$ = xp_expr_new(XPATH_EXPR_COMPARE_VALUES,$1,$3);
                                    $$->compare = $2; }
| XPathRangeExpr XPathGeneralComp XPathRangeExpr
                                  { $$ = xp_expr_new(XPATH_EXPR_COMPARE_GENERAL,$1,$3);
                                    $$->compare = $2; }
| XPathRangeExpr XPathNodeComp XPathRangeExpr
                                  { $$ = xp_expr_new(XPATH_EXPR_COMPARE_NODES,$1,$3);
                                    $$->compare = $2; }
;


XPathRangeExpr:
  XPathAdditiveExpr               { $$ = $1; }
| XPathAdditiveExpr TO XPathAdditiveExpr
                                  { $$ = xp_expr_new(XPATH_EXPR_TO,$1,$3); }
;

XPathAdditiveExpr:
  XPathMultiplicativeExpr         { $$ = $1; }
| XPathAdditiveExpr '+' XPathMultiplicativeExpr
                                  { $$ = xp_expr_new(XPATH_EXPR_ADD,$1,$3); }
| XPathAdditiveExpr '-' XPathMultiplicativeExpr
                                  { $$ = xp_expr_new(XPATH_EXPR_SUBTRACT,$1,$3); }
;

/* FIXME: need to interpret "*" as MULTIPLY operator in certain situations. Currently in
   lexer.l we only return this for the word "multiply", which isn't part of the spec
   Currently this has a shift/reduce conflict with wildcard
 */
XPathMultiplicativeExpr:
  XPathUnionExpr                  { $$ = $1; }
| XPathMultiplicativeExpr '*' XPathUnionExpr
                                  { $$ = xp_expr_new(XPATH_EXPR_MULTIPLY,$1,$3); }
| XPathMultiplicativeExpr DIV XPathUnionExpr
                                  { $$ = xp_expr_new(XPATH_EXPR_DIVIDE,$1,$3); }
| XPathMultiplicativeExpr IDIV XPathUnionExpr
                                  { $$ = xp_expr_new(XPATH_EXPR_IDIVIDE,$1,$3); }
| XPathMultiplicativeExpr MOD XPathUnionExpr
                                  { $$ = xp_expr_new(XPATH_EXPR_MOD,$1,$3); }
;

XPathUnionExpr:
  XPathIntersectExpr              { $$ = $1; }
| XPathUnionExpr UNION XPathIntersectExpr
                                  { $$ = xp_expr_new(XPATH_EXPR_UNION,$1,$3); }
| XPathUnionExpr '|' XPathIntersectExpr
                                  { $$ = xp_expr_new(XPATH_EXPR_UNION2,$1,$3); }
;

XPathIntersectExpr:
  XPathInstanceofExpr             { $$ = $1; }
| XPathIntersectExpr INTERSECT XPathInstanceofExpr
                                  { $$ = xp_expr_new(XPATH_EXPR_INTERSECT,$1,$3); }
| XPathIntersectExpr EXCEPT XPathInstanceofExpr
                                  { $$ = xp_expr_new(XPATH_EXPR_EXCEPT,$1,$3); }
;

XPathInstanceofExpr:
  XPathTreatExpr                  { $$ = $1; }
| XPathTreatExpr INSTANCE OF XPathSequenceType
                                  { $$ = xp_expr_new(XPATH_EXPR_INSTANCE_OF,$1,NULL);
                                    $$->seqtype = $4; }
;

XPathTreatExpr:
  XPathCastableExpr               { $$ = $1; }
| XPathCastableExpr TREAT AS XPathSequenceType
                                  { $$ = xp_expr_new(XPATH_EXPR_TREAT,$1,NULL);
                                    $$->seqtype = $4; }
;

XPathCastableExpr:
  XPathCastExpr                   { $$ = $1; }
| XPathCastExpr CASTABLE AS XPathSingleType
                                  { $$ = xp_expr_new(XPATH_EXPR_CASTABLE,$1,NULL);
                                    $$->seqtype = $4; }
;

XPathCastExpr:
  XPathUnaryExpr                  { $$ = $1; }
| XPathUnaryExpr CAST AS XPathSingleType
                                  { $$ = xp_expr_new(XPATH_EXPR_CAST,$1,NULL);
                                    $$->seqtype = $4; }
;

XPathUnaryExpr:
  '-' XPathUnaryExpr              { $$ = xp_expr_new(XPATH_EXPR_UNARY_MINUS,$2,NULL); }
| '+' XPathUnaryExpr              { $$ = xp_expr_new(XPATH_EXPR_UNARY_PLUS,$2,NULL); }
| XPathValueExpr                  { $$ = $1; }
;

XPathValueExpr:
  XPathPathExpr                   { $$ = $1; }
;

XPathGeneralComp:
  '='                             { $$ = XPATH_GENERAL_COMP_EQ; }
| SYMBOL_NE                       { $$ = XPATH_GENERAL_COMP_NE; }
| '<'                             { $$ = XPATH_GENERAL_COMP_LT; }
| SYMBOL_LE                       { $$ = XPATH_GENERAL_COMP_LE; }
| '>'                             { $$ = XPATH_GENERAL_COMP_GT; }
| SYMBOL_GE                       { $$ = XPATH_GENERAL_COMP_GE; }
;

XPathValueComp:
  VALUE_EQ                        { $$ = XPATH_VALUE_COMP_EQ; }
| VALUE_NE                        { $$ = XPATH_VALUE_COMP_NE; }
| VALUE_LT                        { $$ = XPATH_VALUE_COMP_LT; }
| VALUE_LE                        { $$ = XPATH_VALUE_COMP_LE; }
| VALUE_GT                        { $$ = XPATH_VALUE_COMP_GT; }
| VALUE_GE                        { $$ = XPATH_VALUE_COMP_GE; }
;

XPathNodeComp:
  IS                              { $$ = XPATH_NODE_COMP_IS; }
| NODE_PRECEDES                   { $$ = XPATH_NODE_COMP_PRECEDES; }
| NODE_FOLLOWS                    { $$ = XPATH_NODE_COMP_FOLLOWS; }
;

XPathPathExpr:
  '/'                             { $$ = xp_expr_new(XPATH_EXPR_ROOT,NULL,NULL); }
| '/' XPathRelativePathExpr       { $$ = xp_expr_new(XPATH_EXPR_ROOT,NULL,NULL);
                                    $$->left = $2; }
| SLASHSLASH XPathRelativePathExpr
                                  { xp_expr *dos;
                                    dos = xp_expr_new(XPATH_EXPR_NODE_TEST,NULL,NULL);
                                    dos->nodetest = XPATH_NODE_TEST_SEQTYPE;
                                    dos->seqtype = df_normalize_itemnode(0,NULL);
                                    dos->axis = AXIS_DESCENDANT_OR_SELF;
                                    $$ = xp_expr_new(XPATH_EXPR_ROOT,NULL,NULL);
                                    $$->left = xp_expr_new(XPATH_EXPR_STEP,dos,$2); }
| XPathRelativePathExpr           { $$ = $1; }
;

XPathRelativePathExpr:
  XPathStepExpr                   { $$ = $1; }
| XPathRelativePathExpr '/' XPathStepExpr
                                  { $$ = xp_expr_new(XPATH_EXPR_STEP,$1,$3); }
| XPathRelativePathExpr SLASHSLASH XPathStepExpr
                                  { xp_expr *dos;
                                    xp_expr *step1;
                                    dos = xp_expr_new(XPATH_EXPR_NODE_TEST,NULL,NULL);
                                    dos->nodetest = XPATH_NODE_TEST_SEQTYPE;
                                    dos->seqtype = df_normalize_itemnode(0,NULL);
                                    dos->axis = AXIS_DESCENDANT_OR_SELF;
                                    step1 = xp_expr_new(XPATH_EXPR_STEP,dos,$3);
                                    $$ = xp_expr_new(XPATH_EXPR_STEP,$1,step1); }
;

XPathStepExpr:
  XPathAxisStep                   { $$ = $1; }
| XPathFilterExpr                 { $$ = $1; }
;

XPathAxisStep:
  XPathForwardStep                { $$ = $1; }
| XPathForwardStep XPathPredicateList
                                  { $$ = xp_expr_new(XPATH_EXPR_FILTER,$1,$2); }
| XPathReverseStep                { $$ = $1; }
| XPathReverseStep XPathPredicateList
                                  { $$ = xp_expr_new(XPATH_EXPR_FILTER,$1,$2); }
;

XPathForwardStep:
  XPathForwardAxis XPathNodeTest  { $$ = $2;
                                    $$->axis = $1; }
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
                                    $$->axis = AXIS_CHILD; }
| '@' XPathNodeTest               { $$ = $2;
                                    $$->axis = AXIS_ATTRIBUTE; }
;

XPathReverseStep:
  XPathReverseAxis XPathNodeTest  { $$ = $2;
                                    $$->axis = $1; }
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
  DOTDOT                          { $$ = xp_expr_new(XPATH_EXPR_NODE_TEST,NULL,NULL);
                                    $$->axis = AXIS_PARENT;
                                    $$->nodetest = XPATH_NODE_TEST_SEQTYPE;
                                    $$->seqtype = df_normalize_itemnode(0,NULL); }
;

XPathNodeTest:
  XPathKindTest                   { $$ = xp_expr_new(XPATH_EXPR_NODE_TEST,NULL,NULL);
                                    $$->nodetest = XPATH_NODE_TEST_SEQTYPE;
                                    $$->seqtype = $1; }
| XPathNameTest                        { $$ = $1; }
;

XPathNameTest:
  QName                           { $$ = xp_expr_new(XPATH_EXPR_NODE_TEST,NULL,NULL);
                                    $$->nodetest = XPATH_NODE_TEST_NAME;
                                    $$->qn = $1; }
| XPathWildcard                        { $$ = xp_expr_new(XPATH_EXPR_NODE_TEST,NULL,NULL);
                                    $$->nodetest = XPATH_NODE_TEST_NAME;
                                    $$->qn = $1; }
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
                                  { $$ = xp_expr_new(XPATH_EXPR_FILTER,$1,$2); }
;

XPathPredicateList:
  XPathPredicate                  { $$ = $1; }
| XPathPredicateList XPathPredicate
                                  { $$ = xp_expr_new(XPATH_EXPR_FILTER,$1,$2); }
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
  INTEGER_LITERAL                 { $$ = xp_expr_new(XPATH_EXPR_INTEGER_LITERAL,NULL,NULL);
                                    $$->ival = $1; }
| DECIMAL_LITERAL                 { $$ = xp_expr_new(XPATH_EXPR_DECIMAL_LITERAL,NULL,NULL);
                                    $$->dval = $1; }
| DOUBLE_LITERAL                  { $$ = xp_expr_new(XPATH_EXPR_DOUBLE_LITERAL,NULL,NULL);
                                    $$->dval = $1; }
;

XPathStringLiteral:
  STRING_LITERAL                  { $$ = xp_expr_new(XPATH_EXPR_STRING_LITERAL,NULL,NULL);
                                    $$->strval = $1; }
;

XPathVarRef:
  '$' XPathVarName                     { $$ = xp_expr_new(XPATH_EXPR_VAR_REF,NULL,NULL);
                                    $$->qn = $2; }
;

XPathParenthesizedExpr:
  '(' ')'                         { $$ = xp_expr_new(XPATH_EXPR_EMPTY,NULL,NULL); }
| '(' XPathExpr ')'               { $$ = xp_expr_new(XPATH_EXPR_PAREN,$2,NULL); }
;

XPathContextItemExpr:
  '.'                             { $$ = xp_expr_new(XPATH_EXPR_CONTEXT_ITEM,NULL,NULL); }
;

XPathFunctionCallParams:
  XPathExprSingle                 { $$ = xp_expr_new(XPATH_EXPR_ACTUAL_PARAM,$1,NULL); }
| XPathExprSingle ',' XPathFunctionCallParams
                                  { $$ = xp_expr_new(XPATH_EXPR_ACTUAL_PARAM,$1,NULL);
                                    $$->right = $3;
                                 /* xp_expr *p = xp_expr_new(XPATH_EXPR_ACTUAL_PARAM,$3,NULL);
                                    $$ = xp_expr_new(XPATH_EXPR_PARAMLIST,$1,p); */ }
;

XPathFunctionCall:
  QName '(' ')'                   { $$ = xp_expr_new(XPATH_EXPR_FUNCTION_CALL,NULL,NULL);
                                    $$->qn = $1; }
| QName '(' XPathFunctionCallParams ')'
                                  { $$ = xp_expr_new(XPATH_EXPR_FUNCTION_CALL,$3,NULL);
                                    $$->qn = $1; }
;

XPathSingleType:
  XPathAtomicType                 { $$ = $1; }
| XPathAtomicType '?'             { $$ = df_seqtype_new(SEQTYPE_OCCURRENCE);
                                    $$->left = $1;
                                    $$->occurrence = OCCURS_OPTIONAL; }
;

XPathSequenceType:
  XPathItemType                   { $$ = $1; }
/*FIXME: causes shift/reduce conflict with XPathAdditiveExpr */
| XPathItemType XPathOccurrenceIndicator
                                  { $$ = df_seqtype_new(SEQTYPE_OCCURRENCE);
                                    $$->left = $1;
                                    $$->occurrence = $2; }
| VOID '(' ')'                    { $$ = df_seqtype_new(SEQTYPE_EMPTY); }
;

XPathOccurrenceIndicator:
  '?'                             { $$ = OCCURS_OPTIONAL; }
| '*'                             { $$ = OCCURS_ZERO_OR_MORE; }
| '+'                             { $$ = OCCURS_ONE_OR_MORE; }
;

XPathItemType:
  XPathAtomicType                 { $$ = $1; }
| XPathKindTest                   { $$ = $1; }
| ITEM '(' ')'                    { $$ = df_normalize_itemnode(1,NULL); }
;

XPathAtomicType:
  QName                           { $$ = df_seqtype_new_item(ITEM_ATOMIC);
                                    $$->item->typeref = $1; }
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
  DOCUMENT_NODE '(' ')'           { $$ = df_seqtype_new_item(ITEM_DOCUMENT); }
| DOCUMENT_NODE '(' XPathElementTest ')'
                                  { $$ = df_seqtype_new_item(ITEM_DOCUMENT);
                                    $$->item->content = df_interleave_document_content($3); }
| DOCUMENT_NODE '(' XPathSchemaElementTest ')'
                                  { $$ = df_seqtype_new_item(ITEM_DOCUMENT);
                                    $$->item->content = df_interleave_document_content($3); }
;
 
XPathElementTest:
  ELEMENT '(' ')'                 { $$ = df_seqtype_new_item(ITEM_ELEMENT); }
| ELEMENT '(' XPathElementNameOrWildcard ')'
                                  { $$ = df_seqtype_new_item(ITEM_ELEMENT);
                                    $$->item->localname = $3; }
| ELEMENT '(' XPathElementNameOrWildcard ',' XPathTypeName ')'
                                  { $$ = df_seqtype_new_item(ITEM_ELEMENT);
                                    $$->item->localname = $3;
                                    $$->item->typeref = $5; }
| ELEMENT '(' XPathElementNameOrWildcard ',' XPathTypeName '?' ')'
                                  { $$ = df_seqtype_new_item(ITEM_ELEMENT);
                                    $$->item->localname = $3;
                                    $$->item->typeref = $5;
                                    $$->item->nillable = 1; }
;

XPathAttributeTest:
  ATTRIBUTE '(' ')'               { $$ = df_seqtype_new_item(ITEM_ATTRIBUTE); }
| ATTRIBUTE '(' XPathAttribNameOrWildcard ')'
                                  { $$ = df_seqtype_new_item(ITEM_ATTRIBUTE);
                                    $$->item->localname = $3; }
| ATTRIBUTE '(' XPathAttribNameOrWildcard ',' XPathTypeName ')'
                                  { $$ = df_seqtype_new_item(ITEM_ATTRIBUTE);
                                    $$->item->localname = $3;
                                    $$->item->typeref = $5; }

XPathSchemaElementTest:
  SCHEMA_ELEMENT '(' QName ')'    { $$ = df_seqtype_new_item(ITEM_ELEMENT);
                                    $$->item->elemref = $3; }
;

XPathSchemaAttributeTest:
  SCHEMA_ATTRIBUTE '(' QName ')'  { $$ = df_seqtype_new_item(ITEM_ATTRIBUTE);
                                    $$->item->attrref = $3; }
;

XPathPITest:
  PROCESSING_INSTRUCTION '(' ')'  { $$ = df_seqtype_new_item(ITEM_PI); }
| PROCESSING_INSTRUCTION '(' NCNAME ')'
                                  { df_seqtype *st = df_seqtype_new_item(ITEM_PI);
                                    st->item->pistr = $3;
                                    $$ = df_seqtype_new(SEQTYPE_OCCURRENCE);
                                    $$->left = st;
                                    $$->occurrence = OCCURS_OPTIONAL; }
| PROCESSING_INSTRUCTION '(' STRING_LITERAL ')'
                                  { df_seqtype *st = df_seqtype_new_item(ITEM_PI);
                                    st->item->pistr = $3;
                                    $$ = df_seqtype_new(SEQTYPE_OCCURRENCE);
                                    $$->left = st;
                                    $$->occurrence = OCCURS_OPTIONAL; }
;

XPathCommentTest:
  COMMENT '(' ')'                 { $$ = df_seqtype_new_item(ITEM_COMMENT); }
;

XPathTextTest:
  TEXT '(' ')'                    { $$ = df_seqtype_new_item(ITEM_TEXT); }
;

XPathAnyKindTest:
  NODE '(' ')'                    { $$ = df_normalize_itemnode(0,NULL); }
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
                                    $$->next = $2; }
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
                                  { $$ = xl_snode_new(XSLT_MATCHING_SUBSTRING);
                                    $$->child = $2; }
;

XSLTNonMatchingSubstring:
  NON_MATCHING_SUBSTRING XSLTSequenceConstructorBlock
                                  { $$ = xl_snode_new(XSLT_NON_MATCHING_SUBSTRING);
                                    $$->child = $2; }
;

XSLTOtherwise:
  OTHERWISE XSLTSequenceConstructorBlock
                                  { $$ = xl_snode_new(XSLT_OTHERWISE);
                                    $$->child = $2; }
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
  '$' QName XSLTParamOptions      { $$ = xl_snode_new(XSLT_PARAM);
                                    $$->qn = $2;
                                    $$->flags = $3; }
| '$' QName '=' XPathExprSingle XSLTParamOptions
                                  { $$ = xl_snode_new(XSLT_PARAM);
                                    $$->qn = $2;
                                    $$->select = $4;
                                    $$->flags = $5; }
| '$' QName '=' '{' '}' XSLTParamOptions
                                  { $$ = xl_snode_new(XSLT_PARAM);
                                    $$->qn = $2;
                                    $$->flags = $6; }
| '$' QName '=' '{' XSLTSequenceConstructors '}' XSLTParamOptions
                                  { $$ = xl_snode_new(XSLT_PARAM);
                                    $$->qn = $2;
                                    $$->child = $5;
                                    $$->flags = $7; }
;

XSLTParam:
  XSLTParamNameValueOptions       { $$ = $1; }
| XPathSequenceType XSLTParamNameValueOptions
                                  { $$ = $2;
                                    $$->seqtype = $1; }
;

XSLTParams:
  XSLTParam                       { $$ = $1; }
| XSLTParam ',' XSLTParams        { $$ = $1;
                                    $$->next = $3; }
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
                                    $$->next = $3; }
;

XSLTSort:
  XPathExprSingle                 { $$ = xl_snode_new(XSLT_SORT);
                                    $$->select = $1; }
| '{' XSLTSequenceConstructors '}'
                                  { $$ = xl_snode_new(XSLT_SORT);
                                    $$->child = $2; }
| '{' '}'                         { $$ = xl_snode_new(XSLT_SORT); }
;


/* FIXME: add imports */
XSLTTransform:
  TRANSFORM '{' XSLTDeclarations '}'
                                  { $$ = parse_tree = xl_snode_new(XSLT_TRANSFORM);
                                    $$->child = $3;
                                    ns_add_direct($$->namespaces,XSLT_NAMESPACE,"xsl");
                                    ns_add_direct($$->namespaces,XHTML_NAMESPACE,"xhtml"); }
| JUST_EXPR XPathExpr             { parse_expr = $2; }
| JUST_PATTERN XSLTPattern        { parse_expr = $2; }
| JUST_SEQTYPE XPathSequenceType  { parse_seqtype = $2; }
;

XSLTVariableNameValue:
  '$' QName                       { $$ = xl_snode_new(XSLT_VARIABLE);
                                    $$->qn = $2; }
| '$' QName '=' XPathExprSingle   { $$ = xl_snode_new(XSLT_VARIABLE);
                                    $$->qn = $2; 
                                    $$->select = $4; }
| '$' QName '=' '{' '}'           { $$ = xl_snode_new(XSLT_VARIABLE);
                                    $$->qn = $2;  }
| '$' QName '=' '{' XSLTSequenceConstructors '}'
                                  { $$ = xl_snode_new(XSLT_VARIABLE);
                                    $$->qn = $2; 
                                    $$->child = $5; }
;

XSLTVariable:
  VAR XSLTVariableNameValue ';'   { $$ = $2; }
| XPathSequenceType XSLTVariableNameValue ';'
                                  { $$ = $2; /* FIXME: type */ }
;

XSLTWhen:
  WHEN '(' XPathExpr ')' XSLTSequenceConstructorBlock
                                  { $$ = xl_snode_new(XSLT_WHEN);
                                    $$->select = $3;
                                    assert($$->select);
                                    $$->child = $5; }
;

XSLTWhens:
  XSLTWhen                        { $$ = $1; }
| XSLTWhen XSLTWhens              { $$ = $1;
                                    $$->next = $2; }
;

XSLTWithParamNameValue:
  '$' QName '=' XPathExprSingle   { $$ = xl_snode_new(XSLT_VARIABLE);
                                    $$->qn = $2; 
                                    $$->select = $4; }
| '$' QName '=' '{' '}'           { $$ = xl_snode_new(XSLT_VARIABLE);
                                    $$->qn = $2;  }
| '$' QName '=' '{' XSLTSequenceConstructors '}'
                                  { $$ = xl_snode_new(XSLT_VARIABLE);
                                    $$->qn = $2; 
                                    $$->child = $5; }
;


XSLTWithParam:
  XSLTWithParamNameValue          { $$ = $1;
                                    $$->type = XSLT_WITH_PARAM; }
| XSLTWithParamNameValue TUNNEL   { $$ = $1;
                                    $$->type = XSLT_WITH_PARAM;
                                    $$->flags |= FLAG_TUNNEL; }
| XPathSequenceType XSLTWithParamNameValue
                                  { $$ = $2;
                                    $$->type = XSLT_WITH_PARAM;
                                    /* FIXME: type */ }
| XPathSequenceType XSLTWithParamNameValue TUNNEL
                                  { $$ = $2;
                                    $$->type = XSLT_WITH_PARAM;
                                    $$->flags |= FLAG_TUNNEL;
                                    /* FIXME: type */ }
;

XSLTWithParams:
  XSLTWithParam                   { $$ = $1; }
| XSLTWithParam ',' XSLTWithParams
                                  { $$ = $1;
                                    $$->next = $3; }
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
                                  { $$ = xl_snode_new(XSLT_DECL_FUNCTION);
                                    $$->qn = $2;
                                    $$->param = $3;
                                    $$->child = $4; }
| FUNCTION QName XSLTFormalParams OVERRIDE XSLTSequenceConstructorBlock
                                  { $$ = xl_snode_new(XSLT_DECL_FUNCTION);
                                    $$->qn = $2;
                                    $$->param = $3;
                                    $$->flags = FLAG_OVERRIDE;
                                    $$->child = $5; }
| XPathSequenceType QName XSLTFormalParams XSLTSequenceConstructorBlock
                                  { $$ = xl_snode_new(XSLT_DECL_FUNCTION);
                                    $$->seqtype = $1;
                                    $$->qn = $2;
                                    $$->param = $3;
                                    $$->child = $4; }
| XPathSequenceType QName XSLTFormalParams OVERRIDE XSLTSequenceConstructorBlock
                                  { $$ = xl_snode_new(XSLT_DECL_FUNCTION);
                                    $$->seqtype = $1;
                                    $$->qn = $2;
                                    $$->param = $3;
                                    $$->flags = FLAG_OVERRIDE;
                                    $$->child = $5; }
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
                                  { $$ = xl_snode_new(XSLT_DECL_TEMPLATE);
                                    $$->qn = $2;
                                    $$->child = $3; }
| TEMPLATE QName XSLTFormalParams XSLTSequenceConstructorBlock
                                  { $$ = xl_snode_new(XSLT_DECL_TEMPLATE);
                                    $$->qn = $2;
                                    $$->param = $3;
                                    $$->child = $4; }
| TEMPLATE MATCH '(' XPathExpr ')' XSLTSequenceConstructorBlock
                                  { $$ = xl_snode_new(XSLT_DECL_TEMPLATE);
                                    $$->select = $4; 
                                    $$->child = $6; }
| TEMPLATE QName MATCH '(' XPathExpr ')' XSLTSequenceConstructorBlock
                                  { $$ = xl_snode_new(XSLT_DECL_TEMPLATE);
                                    $$->qn = $2; 
                                    $$->select = $5; 
                                    $$->child = $7; }
| TEMPLATE XSLTFormalParams MATCH '(' XPathExpr ')' XSLTSequenceConstructorBlock
                                  { $$ = xl_snode_new(XSLT_DECL_TEMPLATE);
                                    $$->param = $2; 
                                    $$->select = $5; 
                                    $$->child = $7; }
| TEMPLATE QName XSLTFormalParams MATCH '(' XPathExpr ')' XSLTSequenceConstructorBlock
                                  { $$ = xl_snode_new(XSLT_DECL_TEMPLATE);
                                    $$->qn = $2;
                                    $$->param = $3; 
                                    $$->select = $6; 
                                    $$->child = $8; }
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
                                    $$->next = $2; }
;

XSLTInstrAnalyzeStringBlock:
  XSLTInstrAnalyzeStringContent   { $$ = $1; }
| '{' XSLTInstrAnalyzeStringContents '}'
                                  { $$ = $2; }
;

XSLTInstrAnalyzeString:
  ANALYZE_STRING '(' XPathExprSingle ',' XPathExprSingle ')' XSLTInstrAnalyzeStringBlock
                                  { $$ = xl_snode_new(XSLT_INSTR_ANALYZE_STRING);
                                    $$->select = $3;
                                    $$->expr1 = $5;
                                    $$->child = $7; }
| ANALYZE_STRING '(' XPathExprSingle ',' XPathExprSingle ',' XPathExprSingle ')' XSLTInstrAnalyzeStringBlock
                                  { $$ = xl_snode_new(XSLT_INSTR_ANALYZE_STRING);
                                    $$->select = $3;
                                    $$->expr1 = $5;
                                    $$->expr2 = $7;
                                    $$->child = $9; }
;

XSLTInstrApplyImports:
  APPLY_IMPORTS ';'               { $$ = xl_snode_new(XSLT_INSTR_APPLY_IMPORTS); }
| APPLY_IMPORTS '(' ')' ';'       { $$ = xl_snode_new(XSLT_INSTR_APPLY_IMPORTS); }
| APPLY_IMPORTS '(' XSLTWithParams ')' ';'
                                  { $$ = xl_snode_new(XSLT_INSTR_APPLY_IMPORTS);
                                    $$->param = $3; }
;

XSLTInstrApplyTemplatesParams:
  APPLY_TEMPLATES                 { $$ = xl_snode_new(XSLT_INSTR_APPLY_TEMPLATES); }
| APPLY_TEMPLATES '(' ')'         { $$ = xl_snode_new(XSLT_INSTR_APPLY_TEMPLATES); }
| APPLY_TEMPLATES '(' XSLTWithParams ')'
                                  { $$ = xl_snode_new(XSLT_INSTR_APPLY_TEMPLATES);
                                    $$->param = $3; }
;

XSLTInstrApplyTemplatesParamsTo:
  XSLTInstrApplyTemplatesParams TO '(' XPathExpr ')'
                                  { $$ = $1;
                                    $$->select = $4; }
| XSLTInstrApplyTemplatesParams   { $$ = $1; }
;

XSLTInstrApplyTemplatesMode:
  XSLTInstrApplyTemplatesParamsTo MODE QName
                                  { $$ = $1;
                                    $$->mode = $3; }
| XSLTInstrApplyTemplatesParamsTo
                                  { $$ = $1; }
;

XSLTInstrApplyTemplates:
  XSLTInstrApplyTemplatesMode XSLTSortClause ';'
                                  { $$ = $1;
                                    $$->sort = $2; }
| XSLTInstrApplyTemplatesMode ';' { $$ = $1; }
;

XSLTInstrAttributeName:
  '#' QName                       { $$ = xl_snode_new(XSLT_INSTR_ATTRIBUTE);
                                    $$->qn = $2; }
| '#' '(' XPathExpr ')'                { $$ = xl_snode_new(XSLT_INSTR_ATTRIBUTE);
                                    $$->name_expr = $3; }
;

XSLTInstrAttribute:
  XSLTInstrAttributeName ';'      { $$ = $1; }
| XSLTInstrAttributeName '(' XPathExpr')' ';'
                                  { $$ = $1;
                                    $$->select = $3; }
| XSLTInstrAttributeName XSLTSequenceConstructorBlock
                                  { $$ = $1;
                                    $$->child = $2; }
;

XSLTInstrCallTemplate:
  CALL_TEMPLATE QName ';'         { $$ = xl_snode_new(XSLT_INSTR_CALL_TEMPLATE);
                                    $$->qn = $2; }
| CALL_TEMPLATE QName '(' ')' ';' { $$ = xl_snode_new(XSLT_INSTR_CALL_TEMPLATE);
                                    $$->qn = $2; }
| CALL_TEMPLATE QName '(' XSLTWithParams ')' ';'
                                  { $$ = xl_snode_new(XSLT_INSTR_CALL_TEMPLATE);
                                    $$->qn = $2;
                                    $$->param = $4; }
;

XSLTInstrChoose:
  CHOOSE '{' XSLTWhens '}'        { $$ = xl_snode_new(XSLT_INSTR_CHOOSE);
                                    $$->child = $3; }
| CHOOSE '{' XSLTWhens XSLTOtherwise '}'
                                  { xl_snode *o;
                                    $$ = xl_snode_new(XSLT_INSTR_CHOOSE);
                                    o = $3;
                                    assert(o);
                                    while (o->next)
                                      o = o->next;
                                    o->next = $4;
                                    $$->child = $3; }
;

XSLTInstrComment:
  COMMENT '(' XPathExpr ')' ';'        { $$ = xl_snode_new(XSLT_INSTR_COMMENT);
                                    $$->select = $3; }
| COMMENT XSLTSequenceConstructorBlock
                                  { $$ = xl_snode_new(XSLT_INSTR_COMMENT);
                                    $$->child = $2; }
;

XSLTInstrCopy:
  COPY ';'                        { $$ = xl_snode_new(XSLT_INSTR_COPY); }
| COPY XSLTSequenceConstructorBlock
                                  { $$ = xl_snode_new(XSLT_INSTR_COPY);
                                    $$->child = $2; }
;

XSLTInstrCopyOf:
  COPY_OF '(' XPathExpr ')' ';'        { $$ = xl_snode_new(XSLT_INSTR_COPY_OF);
                                    $$->select = $3; }
;

XSLTInstrElementName:
  '%' QName                       { $$ = xl_snode_new(XSLT_INSTR_ELEMENT);
                                    $$->qn = $2; }
| '%' '(' XPathExpr ')'           { $$ = xl_snode_new(XSLT_INSTR_ELEMENT);
                                    $$->name_expr = $3; }
;

XSLTInstrElement:
  XSLTInstrElementName ';'        { $$ = $1; }
| XSLTInstrElementName XSLTSequenceConstructorBlock
                                  { $$ = $1;
                                    $$->child = $2; }
;

XSLTInstrFallbacks:
  XSLTInstrFallback               { $$ = $1; }
| XSLTInstrFallback XSLTInstrFallbacks
                                  { $$ = $1;
                                    $$->next = $2; }
;

XSLTInstrFallback:
  FALLBACK XSLTSequenceConstructorBlock
                                  { $$ = xl_snode_new(XSLT_INSTR_FALLBACK);
                                    $$->child = $2; }
;

XSLTInstrForEach:
  FOR_EACH '(' XPathExpr ')' XSLTSequenceConstructorBlock
                                  { $$ = xl_snode_new(XSLT_INSTR_FOR_EACH);
                                    $$->select = $3;
                                    $$->child = $5; }
| FOR_EACH '(' XPathExpr ')' XSLTSortClause XSLTSequenceConstructorBlock
                                  { $$ = xl_snode_new(XSLT_INSTR_FOR_EACH);
                                    $$->select = $3;
                                    $$->sort = $5;
                                    $$->child = $6; }
;


XSLTInstrForEachGroupMethod:
  BY                              { $$ = XSLT_GROUPING_BY; }
| ADJACENT                        { $$ = XSLT_GROUPING_ADJACENT; }
| STARTING_WITH                   { $$ = XSLT_GROUPING_STARTING_WITH; }
| ENDING_WITH                     { $$ = XSLT_GROUPING_ENDING_WITH; }
;

XSLTInstrForEachGroup:
  FOR_EACH '(' XPathExpr ')'
    GROUP XSLTInstrForEachGroupMethod '(' XPathExpr ')'
    XSLTSequenceConstructorBlock  { $$ = xl_snode_new(XSLT_INSTR_FOR_EACH_GROUP); 
                                    $$->select = $3;
                                    $$->gmethod = $6;
                                    $$->expr1 = $8;
                                    $$->child = $10; }
| FOR_EACH '(' XPathExpr ')'
    GROUP XSLTInstrForEachGroupMethod '(' XPathExpr ')'
    XSLTSortClause
    XSLTSequenceConstructorBlock  { $$ = xl_snode_new(XSLT_INSTR_FOR_EACH_GROUP); 
                                    $$->select = $3;
                                    $$->gmethod = $6;
                                    $$->expr1 = $8;
                                    $$->sort = $10;
                                    $$->child = $11; }
;

XSLTInstrIf:
  IF '(' XPathExpr ')' XSLTSequenceConstructorBlock
                                  { $$ = xl_snode_new(XSLT_INSTR_IF);
                                    $$->select = $3;
                                    $$->child = $5; }
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
                                  { $$ = xl_snode_new(XSLT_INSTR_MESSAGE);
                                    $$->select = $1;
                                    $$->child = $2; }
| XSLTInstrMessageSelect TERMINATE XSLTInstrMessageContent
                                  { $$ = xl_snode_new(XSLT_INSTR_MESSAGE);
                                    $$->select = $1;
                                    $$->flags |= FLAG_TERMINATE;
                                    $$->child = $3; }
| XSLTInstrMessageSelect TERMINATE '(' XPathExpr ')' XSLTInstrMessageContent
                                  { $$ = xl_snode_new(XSLT_INSTR_MESSAGE);
                                    $$->select = $1;
                                    $$->flags |= FLAG_TERMINATE;
                                    $$->expr1 = $4;
                                    $$->child = $6; }
;

XSLTInstrNamespace:
  NAMESPACE NCNAME '(' XPathExpr ')' ';'
                                  { $$ = xl_snode_new(XSLT_INSTR_NAMESPACE);
                                    $$->strval = $2;
                                    $$->select = $4; }
| NAMESPACE NCNAME XSLTSequenceConstructorBlock
                                  { $$ = xl_snode_new(XSLT_INSTR_NAMESPACE);
                                    $$->strval = $2;
                                    $$->child = $3; }
| NAMESPACE '(' XPathExpr ')' '(' XPathExpr ')' ';'
                                  { $$ = xl_snode_new(XSLT_INSTR_NAMESPACE);
                                    $$->name_expr = $3;
                                    $$->select = $6; }
| NAMESPACE '(' XPathExpr ')' XSLTSequenceConstructorBlock
                                  { $$ = xl_snode_new(XSLT_INSTR_NAMESPACE);
                                    $$->name_expr = $3;
                                    $$->child = $5; }
;

XSLTInstrNextMatchFallbacks:
  '{' '}'                         { $$ = NULL; }
| '{' XSLTInstrFallbacks '}'      { $$ = $2; }
| XSLTInstrFallback               { $$ = $1; }
| ';'                             { $$ = NULL; }
;

XSLTInstrNextMatch:
  NEXT_MATCH XSLTInstrNextMatchFallbacks
                                  { $$ = xl_snode_new(XSLT_INSTR_NEXT_MATCH);
                                    $$->child = $2; }
| NEXT_MATCH '(' ')' XSLTInstrNextMatchFallbacks
                                  { $$ = xl_snode_new(XSLT_INSTR_NEXT_MATCH);
                                    $$->child = $4; }
| NEXT_MATCH '(' XSLTWithParams ')' XSLTInstrNextMatchFallbacks
                                  { $$ = xl_snode_new(XSLT_INSTR_NEXT_MATCH);
                                    $$->param = $3;
                                    $$->child = $5; }
;

/* FIXME
XSLTInstrNumber:
;
*/

XSLTInstrPerformSort:
  PERFORM_SORT XSLTSortClause XSLTSequenceConstructorBlock
                                  { $$ = xl_snode_new(XSLT_INSTR_PERFORM_SORT);
                                    $$->sort = $2;
                                    $$->child = $3; }
| PERFORM_SORT '(' XPathExpr ')' XSLTSortClause XSLTInstrNextMatchFallbacks
                                  { $$ = xl_snode_new(XSLT_INSTR_PERFORM_SORT);
                                    $$->select = $3;
                                    $$->sort = $5;
                                    $$->child = $6; }
;


/* FIXME: find a better syntax for this (can't use process-instruction(...) as this conflicts
  with the PI nodetest in XPAth */
XSLTInstrProcessingInstructionName:
  INSERT_PROCESSING_INSTRUCTION QName    { $$ = xl_snode_new(XSLT_INSTR_PROCESSING_INSTRUCTION);
                                    $$->qn = $2; }

| INSERT_PROCESSING_INSTRUCTION '(' XPathExpr ')'
                                  { $$ = xl_snode_new(XSLT_INSTR_PROCESSING_INSTRUCTION);
                                    $$->name_expr = $3; }
;

XSLTInstrProcessingInstruction:
  XSLTInstrProcessingInstructionName '(' XPathExpr ')' ';'
                                  { $$ = $1;
                                    $$->select = $3; }
| XSLTInstrProcessingInstructionName XSLTSequenceConstructorBlock
                                  { $$ = $1;
                                    $$->child = $2; }
;

/* FIXME
XSLTInstrResultDocument:
;
*/

XSLTInstrSequence:
  SEQUENCE '(' XPathExpr ')' XSLTInstrNextMatchFallbacks
                                  { $$ = xl_snode_new(XSLT_INSTR_SEQUENCE);
                                    $$->select = $3;
                                    $$->child = $5; }
;

XSLTInstrText:
  TEXT STRING_LITERAL ';'                { $$ = xl_snode_new(XSLT_INSTR_TEXT);
                                    $$->strval = $2; }
;

XSLTInstrValueOfSeparator:
  SEPARATOR '(' XPathExpr ')'          { $$ = $3; }
| /* empty */                     { $$ = NULL; }
;

XSLTInstrValueOf:
  VALUE_OF '(' XPathExpr ')' XSLTInstrValueOfSeparator ';'
                                  { $$ = xl_snode_new(XSLT_INSTR_VALUE_OF);
                                    $$->select = $3;
                                    $$->expr1 = $5; }
| VALUE_OF XSLTInstrValueOfSeparator XSLTSequenceConstructorBlock
                                  { $$ = xl_snode_new(XSLT_INSTR_VALUE_OF);
                                    $$->expr1 = $2;
                                    $$->child = $3; }
;













XSLTPattern:
  XSLTPathPattern                 { $$ = $1; }
| XSLTPattern '|' XSLTPathPattern { $$ = xp_expr_new(XPATH_EXPR_UNION2,$1,$3); }
;

XSLTPathPattern:
  XSLTRelativePathPattern         { $$ = $1 }
| '/'                             { $$ = xp_expr_new(XPATH_EXPR_ROOT,NULL,NULL); }
| '/' XSLTRelativePathPattern     { $$ = xp_expr_new(XPATH_EXPR_ROOT,NULL,NULL);
                                    $$->left = $2; }
| SLASHSLASH XSLTRelativePathPattern
                                  { xp_expr *dos;
                                    dos = xp_expr_new(XPATH_EXPR_NODE_TEST,NULL,NULL);
                                    dos->nodetest = XPATH_NODE_TEST_SEQTYPE;
                                    dos->seqtype = df_normalize_itemnode(0,NULL);
                                    dos->axis = AXIS_DESCENDANT_OR_SELF;
                                    $$ = xp_expr_new(XPATH_EXPR_ROOT,NULL,NULL);
                                    $$->left = xp_expr_new(XPATH_EXPR_STEP,dos,$2); }
| XSLTIdKeyPattern                { $$ = NULL; /* FIXME */ }
| XSLTIdKeyPattern '/' XSLTRelativePathPattern
                                  { $$ = NULL; /* FIXME */ }
| XSLTIdKeyPattern SLASHSLASH XSLTRelativePathPattern
                                  { $$ = NULL; /* FIXME */ }
;

XSLTRelativePathPattern:
  XSLTPatternStep                 { $$ = $1; }
| XSLTPatternStep '/' XSLTRelativePathPattern
                                  { $$ = xp_expr_new(XPATH_EXPR_STEP,$1,$3); }

| XSLTPatternStep SLASHSLASH XSLTRelativePathPattern
                                  { xp_expr *dos;
                                    xp_expr *step1;
                                    dos = xp_expr_new(XPATH_EXPR_NODE_TEST,NULL,NULL);
                                    dos->nodetest = XPATH_NODE_TEST_SEQTYPE;
                                    dos->seqtype = df_normalize_itemnode(0,NULL);
                                    dos->axis = AXIS_DESCENDANT_OR_SELF;
                                    step1 = xp_expr_new(XPATH_EXPR_STEP,dos,$3);
                                    $$ = xp_expr_new(XPATH_EXPR_STEP,$1,step1); }
;

XSLTPatternStep:
  XPathNodeTest                   { $$ = $1;
                                    $$->axis = AXIS_CHILD; }
| XPathNodeTest XPathPredicateList
                                  { $$ = xp_expr_new(XPATH_EXPR_FILTER,$1,$2);
                                    $$->axis = AXIS_CHILD; }
| XSLTPatternAxis XPathNodeTest   { $$ = $2;
                                    $$->axis = $1; }
| XSLTPatternAxis XPathNodeTest XPathPredicateList
                                  { $$ = xp_expr_new(XPATH_EXPR_FILTER,$2,$3);
                                    $$->axis = $1; }
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
                                    $$->next = $2; }
;

XSLTSequenceConstructorBlock:
  '{' XSLTSequenceConstructors '}'
                                  { $$ = $2; }
| '{' '}'                         { $$ = NULL; }
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
    error(parse_ei,parse_filename,parse_firstline+lex_lineno,NULL,"%s",err);
  else if (parse_errstr)
    fprintf(stderr,"Parse error at line %d%s: %s\n",parse_firstline+lex_lineno,parse_errstr,err);
  else
    fprintf(stderr,"Parse error at line %d: %s\n",parse_firstline+lex_lineno,err);
  return 1;
}
