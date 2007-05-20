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
 * $Id: grammar.y 170 2005-11-07 09:50:12Z pmkelly $
 *
 */

#include "cxslt.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <assert.h>

/* FIXME: need to handle reserved words properly, e.g. using them as variable names.
This requires special handling in the lexer; see the document from the W3C on this */

extern int lex_lineno;
int yylex();
int yyerror(const char *err);

extern expression *parse_expr;
extern int parse_firstline;
extern xmlNodePtr parse_node;

%}

%union {
  int axis;
  char *string;
  int inumber;
  double number;
  expression *expr;
  int flags;
  qname qn;
  int gmethod;
  int stype;
  char c;
  seqtype *st;
}

%debug
%start Top

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

/* FIXME: conflict with the use of elements that have these names */

%token IF
%token TO
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
%token VOID

%token <string> NCNAME
%token <string> STRING_LITERAL
%token <string> AVT_LITERAL
%token <inumber> INTEGER_LITERAL
%token <number> DECIMAL_LITERAL
%token <number> DOUBLE_LITERAL

%type <qn> QName

%type <expr> Top
%type <expr> AVTLiteral
%type <expr> TopExprs
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
%type <expr> XPathForwardAxisStep
%type <expr> XPathReverseAxisStep
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

%type <expr> XPathKindTest

%type <expr> XPathDocumentTest
%type <expr> XPathElementTest
%type <expr> XPathAttributeTest
%type <expr> XPathSchemaElementTest
%type <expr> XPathSchemaAttributeTest
%type <expr> XPathPITest
%type <expr> XPathCommentTest
%type <expr> XPathTextTest
%type <expr> XPathAnyKindTest

%type <qn> XPathAttribNameOrWildcard
%type <qn> XPathElementNameOrWildcard
%type <qn> XPathTypeName

%%

AVTLiteral:
  AVT_LITERAL                     { $$ = new_expression(XPATH_STRING_LITERAL);
                                    $$->str = $1; }
;

TopExprs:
  AVTLiteral                      { $$ = $1; }
| AVTLiteral TopExprs             { $$ = new_expression2(XPATH_AVT_COMPONENT,$1,$2); }
| AVTLiteral XPathExpr TopExprs   { expression *e = new_expression2(XPATH_AVT_COMPONENT,$2,$3);
                                    $$ = new_expression2(XPATH_AVT_COMPONENT,$1,e); }
;

Top:
  XPathExpr                       { parse_expr = $1; }
| XPathExpr TopExprs              { parse_expr = $1; }
| TopExprs                        { parse_expr = $1; }
;

XPathExpr:
  XPathExprSingle                 { $$ = $1; }
| XPathExprSingle ',' XPathExpr   { $$ = new_expression2(XPATH_SEQUENCE,$1,$3); }
;

XPathExprSingle:
  XPathForExpr                    { $$ = $1; }
| XPathQuantifiedExpr             { $$ = $1; }
| XPathIfExpr                     { $$ = $1; }
| XPathOrExpr                     { $$ = $1; }
;

XPathForExpr:
  FOR XPathVarInList RETURN XPathExprSingle
                                  { $$ = new_ForExpr($2,$4); }
;

XPathVarIn:
  '$' QName IN XPathExprSingle    { $$ = new_VarInExpr($4);
                                    $$->qn = $2; }
;

XPathVarInList:
  XPathVarIn                      { $$ = $1; }
| XPathVarInList ',' XPathVarIn   { $$ = new_VarInListExpr($1,$3); }
;

XPathQuantifiedExpr:
  SOME XPathVarInList SATISFIES XPathExprSingle
                                  { $$ = new_QuantifiedExpr(XPATH_SOME,$2,$4); }
| EVERY XPathVarInList SATISFIES XPathExprSingle
                                  { $$ = new_QuantifiedExpr(XPATH_EVERY,$2,$4); }
;

XPathIfExpr:
  IF '(' XPathExpr ')' THEN XPathExprSingle ELSE XPathExprSingle
                                  { $$ = new_XPathIfExpr($3,$6,$8); }
;

XPathOrExpr:
  XPathAndExpr                    { $$ = $1; }
| XPathOrExpr OR XPathAndExpr     { $$ = new_expression2(XPATH_OR,$1,$3); }
;

XPathAndExpr:
  XPathComparisonExpr             { $$ = $1; }
| XPathAndExpr AND XPathComparisonExpr
                                  { $$ = new_expression2(XPATH_AND,$1,$3); }
;

XPathComparisonExpr:
  XPathRangeExpr                  { $$ = $1; }
| XPathRangeExpr XPathValueComp XPathRangeExpr
                                  { $$ = new_expression2($2,$1,$3); }
| XPathRangeExpr XPathGeneralComp XPathRangeExpr
                                  { $$ = new_expression2($2,$1,$3); }
| XPathRangeExpr XPathNodeComp XPathRangeExpr
                                  { $$ = new_expression2($2,$1,$3); }
;


XPathRangeExpr:
  XPathAdditiveExpr               { $$ = $1; }
| XPathAdditiveExpr TO XPathAdditiveExpr
                                  { $$ = new_expression2(XPATH_TO,$1,$3); }
;

XPathAdditiveExpr:
  XPathMultiplicativeExpr         { $$ = $1; }
| XPathAdditiveExpr '+' XPathMultiplicativeExpr
                                  { $$ = new_expression2(XPATH_ADD,$1,$3); }
| XPathAdditiveExpr '-' XPathMultiplicativeExpr
                                  { $$ = new_expression2(XPATH_SUBTRACT,$1,$3); }
;

/* FIXME: need to interpret "*" as MULTIPLY operator in certain situations. Currently in
   lexer.l we only return this for the word "multiply", which isn't part of the spec
   Currently this has a shift/reduce conflict with wildcard
 */
XPathMultiplicativeExpr:
  XPathUnionExpr                  { $$ = $1; }
| XPathMultiplicativeExpr '*' XPathUnionExpr
                                  { $$ = new_expression2(XPATH_MULTIPLY,$1,$3); }
| XPathMultiplicativeExpr DIV XPathUnionExpr
                                  { $$ = new_expression2(XPATH_DIVIDE,$1,$3); }
| XPathMultiplicativeExpr IDIV XPathUnionExpr
                                  { $$ = new_expression2(XPATH_IDIVIDE,$1,$3); }
| XPathMultiplicativeExpr MOD XPathUnionExpr
                                  { $$ = new_expression2(XPATH_MOD,$1,$3); }
;

XPathUnionExpr:
  XPathIntersectExpr              { $$ = $1; }
| XPathUnionExpr UNION XPathIntersectExpr
                                  { $$ = new_expression2(XPATH_UNION,$1,$3); }
| XPathUnionExpr '|' XPathIntersectExpr
                                  { $$ = new_expression2(XPATH_UNION2,$1,$3); }
;

XPathIntersectExpr:
  XPathInstanceofExpr             { $$ = $1; }
| XPathIntersectExpr INTERSECT XPathInstanceofExpr
                                  { $$ = new_expression2(XPATH_INTERSECT,$1,$3); }
| XPathIntersectExpr EXCEPT XPathInstanceofExpr
                                  { $$ = new_expression2(XPATH_EXCEPT,$1,$3); }
;

XPathInstanceofExpr:
  XPathTreatExpr                  { $$ = $1; }
| XPathTreatExpr INSTANCE OF XPathSequenceType
                                  { $$ = new_TypeExpr(XPATH_INSTANCE_OF,$4,$1); }
;

XPathTreatExpr:
  XPathCastableExpr               { $$ = $1; }
| XPathCastableExpr TREAT AS XPathSequenceType
                                  { $$ = new_TypeExpr(XPATH_TREAT,$4,$1); }
;

XPathCastableExpr:
  XPathCastExpr                   { $$ = $1; }
| XPathCastExpr CASTABLE AS XPathSingleType
                                  { $$ = new_TypeExpr(XPATH_CASTABLE,$4,$1); }
;

XPathCastExpr:
  XPathUnaryExpr                  { $$ = $1; }
| XPathUnaryExpr CAST AS XPathSingleType
                                  { $$ = new_TypeExpr(XPATH_CAST,$4,$1); }
;

XPathUnaryExpr:
  '-' XPathUnaryExpr              { $$ = new_expression(XPATH_UNARY_MINUS);
                                    $$->left = $2; }
| '+' XPathUnaryExpr              { $$ = new_expression(XPATH_UNARY_PLUS);
                                    $$->left = $2 }
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
  '/'                             { $$ = new_RootExpr(NULL); }
| '/' XPathRelativePathExpr       { $$ = new_RootExpr($2); }
| SLASHSLASH XPathRelativePathExpr { /*
                                      expression *dos;
                                    dos = new NodeTestExpr(XPATH_NODE_TEST_SEQUENCETYPE,
                                                           SequenceTypeImpl::node(),
                                                           AXIS_DESCENDANT_OR_SELF);
                                                           $$ = new_RootExpr(new_StepExpr(dos,$2));*/ }
| XPathRelativePathExpr           { $$ = $1; }
;

XPathRelativePathExpr:
  XPathStepExpr                   { $$ = $1; }
| XPathRelativePathExpr '/' XPathStepExpr
                                  { $$ = new_expression2(XPATH_STEP,$1,$3); }
| XPathRelativePathExpr SLASHSLASH XPathStepExpr
                                  { expression *step1;
                                    expression *test = new_expression(XPATH_KIND_TEST);
                                    test->axis = AXIS_DESCENDANT_OR_SELF;
                                    test->kind = KIND_ANY;
                                    step1 = new_expression2(XPATH_STEP,$1,test);
                                    $$ = new_expression2(XPATH_STEP,step1,$3); }
;

XPathStepExpr:
  XPathForwardAxisStep            { $$ = $1; }
| XPathReverseAxisStep            { $$ = $1; }
| XPathFilterExpr                 { $$ = $1; }
;

XPathForwardAxisStep:
  XPathForwardStep                { $$ = $1; }
| XPathForwardAxisStep XPathPredicate
                                  { $$ = new_expression2(XPATH_FILTER,$1,$2); }
;

XPathReverseAxisStep:
  XPathReverseStep                { $$ = $1; }
| XPathReverseAxisStep XPathPredicate
                                  { $$ = new_expression2(XPATH_FILTER,$1,$2); }
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
XPathNodeTest                   { $$ = $1; /* FIXME: if nodetest is attribute use AXIS_ATTRIBUTE */
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
  DOTDOT                          { /*$$ = new NodeTestExpr(XPATH_NODE_TEST_SEQUENCETYPE,
                                                          SequenceTypeImpl::node(),
                                                          AXIS_PARENT);*/ }
;

XPathNodeTest:
  XPathKindTest                   { $$ = $1; }
| XPathNameTest                   { $$ = $1; }
;

XPathNameTest:
  QName                           { $$ = new_expression(XPATH_NAME_TEST);
                                    $$->qn = $1; }
| XPathWildcard                   { $$ = new_expression(XPATH_NAME_TEST);
                                    $$->qn = $1; }
;

XPathWildcard:
'*'                               { $$.uri = NULL;
                                    $$.prefix = NULL;
                                    $$.localpart = NULL; }
| NCNAME ':' '*'                  { $$.uri = strdup(lookup_nsuri(parse_node,$1));
                                    $$.prefix = $1;
                                    $$.localpart = NULL; }
| '*' ':' NCNAME                  { $$.uri = NULL;
                                    $$.prefix = NULL;
                                    $$.localpart = $3; }
;

XPathFilterExpr:
  XPathPrimaryExpr                { $$ = $1; }
| XPathFilterExpr XPathPredicate  { $$ = new_expression2(XPATH_FILTER,$1,$2); }
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
  INTEGER_LITERAL                 { $$ = new_expression(XPATH_INTEGER_LITERAL);
                                    $$->num = $1; }
| DECIMAL_LITERAL                 { $$ = new_expression(XPATH_DECIMAL_LITERAL);
                                    $$->num = $1; }
| DOUBLE_LITERAL                  { $$ = new_expression(XPATH_DOUBLE_LITERAL);
                                    $$->num = $1; }
;

XPathStringLiteral:
  STRING_LITERAL                  { $$ = new_expression(XPATH_STRING_LITERAL);
                                    $$->str = $1; }
;

XPathVarRef:
'$' QName                         { $$ = new_expression(XPATH_VAR_REF);
                                    $$->qn = $2; }
;

XPathParenthesizedExpr:
  '(' ')'                         { $$ = new_expression(XPATH_EMPTY); }
| '(' XPathExpr ')'               { $$ = new_expression(XPATH_PAREN);
                                    $$->left = $2; }
;

XPathContextItemExpr:
  '.'                             { $$ = new_expression(XPATH_CONTEXT_ITEM); }
;

XPathFunctionCallParams:
  XPathExprSingle                 { $$ = new_expression2(XPATH_ACTUAL_PARAM,$1,NULL); }
| XPathExprSingle ',' XPathFunctionCallParams
                                  { $$ = new_expression2(XPATH_ACTUAL_PARAM,$1,$3); }
;

XPathFunctionCall:
  QName '(' ')'                   { $$ = new_expression(XPATH_FUNCTION_CALL);
                                    $$->qn = $1; }
| QName '(' XPathFunctionCallParams ')'
                                  { $$ = new_expression(XPATH_FUNCTION_CALL);
                                    $$->qn = $1;
                                    $$->left = $3; }
;

XPathSingleType:
  XPathAtomicType                 { $$ = $1; }
| XPathAtomicType '?'             { /*$$ = SequenceTypeImpl::occurrence($1,OCCURS_OPTIONAL);*/ }
;

XPathSequenceType:
  XPathItemType                   { $$ = $1; }
/*FIXME: causes shift/reduce conflict with XPathAdditiveExpr */
| XPathItemType XPathOccurrenceIndicator
{ /*$$ = SequenceTypeImpl::occurrence($1,$2);*/ }
| VOID '(' ')'                    { /*$$ = new SequenceTypeImpl(SEQTYPE_EMPTY);*/ }
;

XPathOccurrenceIndicator:
  '?'                             { $$ = OCCURS_OPTIONAL; }
| '*'                             { $$ = OCCURS_ZERO_OR_MORE; }
| '+'                             { $$ = OCCURS_ONE_OR_MORE; }
;

XPathItemType:
  XPathAtomicType                 { $$ = $1; }
| XPathKindTest                   { $$ = NULL; /* FIXME $$ = $1;*/ }
| ITEM '(' ')'                    { /*$$ = SequenceTypeImpl::item();*/ }
;

XPathAtomicType:
  QName                           { /*$$ = new SequenceTypeImpl(new ItemType(ITEM_ATOMIC));
                                      $$->itemType()->m_typeref = $1;*/ }
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
DOCUMENT_NODE '(' ')'             { $$ = new_expression(XPATH_KIND_TEST);
                                    $$->kind = KIND_DOCUMENT; }
| DOCUMENT_NODE '(' XPathElementTest ')'
                                  { assert(!"FIXME: implement"); }
| DOCUMENT_NODE '(' XPathSchemaElementTest ')'
                                  { assert(!"FIXME: implement"); }
;
 
XPathElementTest:
  ELEMENT '(' ')'                 { $$ = new_expression(XPATH_KIND_TEST);
                                    $$->kind = KIND_ELEMENT; }
| ELEMENT '(' XPathElementNameOrWildcard ')'
                                  { assert(!"FIXME: implement"); }
| ELEMENT '(' XPathElementNameOrWildcard ',' XPathTypeName ')'
                                  { assert(!"FIXME: implement"); }
| ELEMENT '(' XPathElementNameOrWildcard ',' XPathTypeName '?' ')'
                                  { assert(!"FIXME: implement"); }
;

XPathAttributeTest:
  ATTRIBUTE '(' ')'               { $$ = new_expression(XPATH_KIND_TEST);
                                    $$->kind = KIND_ATTRIBUTE; }
| ATTRIBUTE '(' XPathAttribNameOrWildcard ')'
                                  { assert(!"FIXME: implement"); }
| ATTRIBUTE '(' XPathAttribNameOrWildcard ',' XPathTypeName ')'
                                  { assert(!"FIXME: implement"); }
;

XPathSchemaElementTest:
  SCHEMA_ELEMENT '(' QName ')'    { $$ = new_expression(XPATH_KIND_TEST);
                                    $$->kind = KIND_SCHEMA_ELEMENT; }
;

XPathSchemaAttributeTest:
  SCHEMA_ATTRIBUTE '(' QName ')'  { $$ = new_expression(XPATH_KIND_TEST);
                                    $$->kind = KIND_SCHEMA_ATTRIBUTE; }
;

XPathPITest:
PROCESSING_INSTRUCTION '(' ')'    { $$ = new_expression(XPATH_KIND_TEST);
                                    $$->kind = KIND_PI; }
| PROCESSING_INSTRUCTION '(' NCNAME ')'
                                  { assert(!"FIXME: implement"); }
| PROCESSING_INSTRUCTION '(' STRING_LITERAL ')'
                                  { assert(!"FIXME: implement"); }
;

XPathCommentTest:
COMMENT '(' ')'                   { $$ = new_expression(XPATH_KIND_TEST);
                                    $$->kind = KIND_COMMENT; }
;

XPathTextTest:
TEXT '(' ')'                      { $$ = new_expression(XPATH_KIND_TEST);
                                    $$->kind = KIND_TEXT; }
;

XPathAnyKindTest:
NODE '(' ')'                      { $$ = new_expression(XPATH_KIND_TEST);
                                    $$->kind = KIND_ANY; }
;

XPathAttribNameOrWildcard:
  QName                           { $$ = $1; }
| '*'                             { $$.uri = NULL;
                                    $$.prefix = NULL;
                                    $$.localpart = NULL; }
;

XPathElementNameOrWildcard:
  QName                           { $$ = $1; }
| '*'                             { $$.uri = NULL;
                                    $$.prefix = NULL;
                                    $$.localpart = NULL; }
;

XPathTypeName:
  QName                           { $$ = $1; }
;

QName:
  NCNAME                          { $$.uri = strdup(lookup_nsuri(parse_node,""));
                                    $$.prefix = strdup("");
                                    $$.localpart = $1; }
| NCNAME ':' NCNAME               { $$.uri = strdup(lookup_nsuri(parse_node,$1));
                                    $$.prefix = $1;
                                    $$.localpart = $3; }
;


%%

int yyerror(const char *err)
{
  fprintf(stderr,"Parse error at line %d: %s\n",parse_firstline+lex_lineno,err);
  return 1;
}
