%{
/*
 * This file is part of the nreduce project
 * Copyright (C) 2005-2007 Peter Kelly (pmk@cs.adelaide.edu.au)
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

%type <string> NCName
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
%type <expr> Expr
%type <expr> ExprSingle
%type <expr> ForExpr
%type <expr> VarIn
%type <expr> VarInList
%type <expr> QuantifiedExpr
%type <expr> IfExpr
%type <expr> OrExpr
%type <expr> AndExpr
%type <expr> ComparisonExpr
%type <expr> RangeExpr
%type <expr> AdditiveExpr
%type <expr> MultiplicativeExpr
%type <expr> UnionExpr
%type <expr> IntersectExpr
%type <expr> InstanceofExpr
%type <expr> TreatExpr
%type <expr> CastableExpr
%type <expr> CastExpr
%type <expr> UnaryExpr
%type <expr> ValueExpr
%type <stype> GeneralComp
%type <stype> ValueComp
%type <stype> NodeComp
%type <expr> PathExpr
%type <expr> RelativePathExpr
%type <expr> StepExpr
%type <expr> ForwardAxisStep
%type <expr> ReverseAxisStep
%type <expr> ForwardStep
%type <axis> ForwardAxis
%type <expr> AbbrevForwardStep
%type <expr> ReverseStep
%type <axis> ReverseAxis
%type <expr> AbbrevReverseStep
%type <expr> NodeTest
%type <expr> NameTest
%type <qn> Wildcard
%type <expr> FilterExpr
%type <expr> Predicate
%type <expr> PrimaryExpr
%type <expr> Literal
%type <expr> NumericLiteral
%type <expr> StringLiteral
%type <expr> VarRef
%type <expr> ParenthesizedExpr
%type <expr> ContextItemExpr
%type <expr> FunctionCallParams
%type <expr> FunctionCall
%type <st> SingleType
%type <st> SequenceType
%type <c> OccurrenceIndicator
%type <st> ItemType
%type <st> AtomicType

%type <expr> KindTest

%type <expr> DocumentTest
%type <expr> ElementTest
%type <expr> AttributeTest
%type <expr> SchemaElementTest
%type <expr> SchemaAttributeTest
%type <expr> PITest
%type <expr> CommentTest
%type <expr> TextTest
%type <expr> AnyKindTest

%type <qn> AttribNameOrWildcard
%type <qn> ElementNameOrWildcard
%type <qn> TypeName

%%

 /* FIXME: need to include other keywords here. Expect a lot of shift/reduce conflicts :( */
NCName:
  NCNAME                          { $$ = $1; }
| RETURN                          { $$ = strdup("return"); }
;

AVTLiteral:
  AVT_LITERAL                     { $$ = new_expression(XPATH_STRING_LITERAL);
                                    $$->str = $1; }
;

TopExprs:
  AVTLiteral                      { $$ = $1; }
| AVTLiteral TopExprs             { $$ = new_expression2(XPATH_AVT_COMPONENT,$1,$2); }
| AVTLiteral Expr TopExprs        { expression *e = new_expression2(XPATH_AVT_COMPONENT,$2,$3);
                                    $$ = new_expression2(XPATH_AVT_COMPONENT,$1,e); }
;

Top:
  Expr                            { parse_expr = $1; }
| Expr TopExprs                   { parse_expr = $1; }
| TopExprs                        { parse_expr = $1; }
;

Expr:
  ExprSingle                      { $$ = $1; }
| ExprSingle ',' Expr             { $$ = new_expression2(XPATH_SEQUENCE,$1,$3); }
;

ExprSingle:
  ForExpr                         { $$ = $1; }
| QuantifiedExpr                  { $$ = $1; }
| IfExpr                          { $$ = $1; }
| OrExpr                          { $$ = $1; }
;

ForExpr:
  FOR VarInList RETURN ExprSingle { $$ = new_ForExpr($2,$4); }
;

VarIn:
  '$' QName IN ExprSingle         { $$ = new_VarInExpr($4);
                                    $$->qn = $2; }
;

VarInList:
  VarIn                           { $$ = $1; }
| VarInList ',' VarIn             { $$ = new_VarInListExpr($1,$3); }
;

QuantifiedExpr:
  SOME VarInList SATISFIES ExprSingle
                                  { $$ = new_QuantifiedExpr(XPATH_SOME,$2,$4); }
| EVERY VarInList SATISFIES ExprSingle
                                  { $$ = new_QuantifiedExpr(XPATH_EVERY,$2,$4); }
;

IfExpr:
  IF '(' Expr ')' THEN ExprSingle ELSE ExprSingle
                                  { $$ = new_XPathIfExpr($3,$6,$8); }
;

OrExpr:
  AndExpr                         { $$ = $1; }
| OrExpr OR AndExpr               { $$ = new_expression2(XPATH_OR,$1,$3); }
;

AndExpr:
  ComparisonExpr                  { $$ = $1; }
| AndExpr AND ComparisonExpr      { $$ = new_expression2(XPATH_AND,$1,$3); }
;

ComparisonExpr:
  RangeExpr                       { $$ = $1; }
| RangeExpr ValueComp RangeExpr   { $$ = new_expression2($2,$1,$3); }
| RangeExpr GeneralComp RangeExpr { $$ = new_expression2($2,$1,$3); }
| RangeExpr NodeComp RangeExpr    { $$ = new_expression2($2,$1,$3); }
;


RangeExpr:
  AdditiveExpr                    { $$ = $1; }
| AdditiveExpr TO AdditiveExpr    { $$ = new_expression2(XPATH_TO,$1,$3); }
;

AdditiveExpr:
  MultiplicativeExpr              { $$ = $1; }
| AdditiveExpr '+' MultiplicativeExpr
                                  { $$ = new_expression2(XPATH_ADD,$1,$3); }
| AdditiveExpr '-' MultiplicativeExpr
                                  { $$ = new_expression2(XPATH_SUBTRACT,$1,$3); }
;

/* FIXME: need to interpret "*" as MULTIPLY operator in certain situations. Currently in
   lexer.l we only return this for the word "multiply", which isn't part of the spec
   Currently this has a shift/reduce conflict with wildcard
 */
MultiplicativeExpr:
  UnionExpr                       { $$ = $1; }
| MultiplicativeExpr '*' UnionExpr
                                  { $$ = new_expression2(XPATH_MULTIPLY,$1,$3); }
| MultiplicativeExpr DIV UnionExpr
                                  { $$ = new_expression2(XPATH_DIVIDE,$1,$3); }
| MultiplicativeExpr IDIV UnionExpr
                                  { $$ = new_expression2(XPATH_IDIVIDE,$1,$3); }
| MultiplicativeExpr MOD UnionExpr
                                  { $$ = new_expression2(XPATH_MOD,$1,$3); }
;

UnionExpr:
  IntersectExpr                   { $$ = $1; }
| UnionExpr UNION IntersectExpr   { $$ = new_expression2(XPATH_UNION,$1,$3); }
| UnionExpr '|' IntersectExpr     { $$ = new_expression2(XPATH_UNION2,$1,$3); }
;

IntersectExpr:
  InstanceofExpr                  { $$ = $1; }
| IntersectExpr INTERSECT InstanceofExpr
                                  { $$ = new_expression2(XPATH_INTERSECT,$1,$3); }
| IntersectExpr EXCEPT InstanceofExpr
                                  { $$ = new_expression2(XPATH_EXCEPT,$1,$3); }
;

InstanceofExpr:
  TreatExpr                       { $$ = $1; }
| TreatExpr INSTANCE OF SequenceType
                                  { $$ = new_TypeExpr(XPATH_INSTANCE_OF,$4,$1); }
;

TreatExpr:
  CastableExpr                    { $$ = $1; }
| CastableExpr TREAT AS SequenceType
                                  { $$ = new_TypeExpr(XPATH_TREAT,$4,$1); }
;

CastableExpr:
  CastExpr                        { $$ = $1; }
| CastExpr CASTABLE AS SingleType { $$ = new_TypeExpr(XPATH_CASTABLE,$4,$1); }
;

CastExpr:
  UnaryExpr                       { $$ = $1; }
| UnaryExpr CAST AS SingleType    { $$ = new_TypeExpr(XPATH_CAST,$4,$1); }
;

UnaryExpr:
  '-' UnaryExpr                   { $$ = new_expression(XPATH_UNARY_MINUS);
                                    $$->left = $2; }
| '+' UnaryExpr                   { $$ = new_expression(XPATH_UNARY_PLUS);
                                    $$->left = $2 }
| ValueExpr                       { $$ = $1; }
;

ValueExpr:
  PathExpr                        { $$ = $1; }
;

GeneralComp:
  '='                             { $$ = XPATH_GENERAL_EQ; }
| SYMBOL_NE                       { $$ = XPATH_GENERAL_NE; }
| '<'                             { $$ = XPATH_GENERAL_LT; }
| SYMBOL_LE                       { $$ = XPATH_GENERAL_LE; }
| '>'                             { $$ = XPATH_GENERAL_GT; }
| SYMBOL_GE                       { $$ = XPATH_GENERAL_GE; }
;

ValueComp:
  VALUE_EQ                        { $$ = XPATH_VALUE_EQ; }
| VALUE_NE                        { $$ = XPATH_VALUE_NE; }
| VALUE_LT                        { $$ = XPATH_VALUE_LT; }
| VALUE_LE                        { $$ = XPATH_VALUE_LE; }
| VALUE_GT                        { $$ = XPATH_VALUE_GT; }
| VALUE_GE                        { $$ = XPATH_VALUE_GE; }
;

NodeComp:
  IS                              { $$ = XPATH_NODE_IS; }
| NODE_PRECEDES                   { $$ = XPATH_NODE_PRECEDES; }
| NODE_FOLLOWS                    { $$ = XPATH_NODE_FOLLOWS; }
;

PathExpr:
  '/'                             { $$ = new_expression2(XPATH_ROOT,NULL,NULL); }
| '/' RelativePathExpr            { expression *root = new_expression2(XPATH_ROOT,NULL,NULL);
                                    $$ = new_expression2(XPATH_STEP,root,$2); }
| SLASHSLASH RelativePathExpr     { expression *root = new_expression2(XPATH_ROOT,NULL,NULL);
                                    expression *step1;
                                    expression *test = new_expression(XPATH_KIND_TEST);
                                    test->axis = AXIS_DESCENDANT_OR_SELF;
                                    test->kind = KIND_ANY;
                                    step1 = new_expression2(XPATH_STEP,root,test);
                                    $$ = new_expression2(XPATH_STEP,step1,$2); }
| RelativePathExpr                { $$ = $1; }
;

RelativePathExpr:
  StepExpr                        { $$ = $1; }
| RelativePathExpr '/' StepExpr   { $$ = new_expression2(XPATH_STEP,$1,$3); }
| RelativePathExpr SLASHSLASH StepExpr
                                  { expression *step1;
                                    expression *test = new_expression(XPATH_KIND_TEST);
                                    test->axis = AXIS_DESCENDANT_OR_SELF;
                                    test->kind = KIND_ANY;
                                    step1 = new_expression2(XPATH_STEP,$1,test);
                                    $$ = new_expression2(XPATH_STEP,step1,$3); }
;

StepExpr:
  ForwardAxisStep                 { $$ = new_expression2(XPATH_FORWARD_AXIS_STEP,$1,NULL); }
| ReverseAxisStep                 { $$ = new_expression2(XPATH_REVERSE_AXIS_STEP,$1,NULL); }
| FilterExpr                      { $$ = $1; }
;

ForwardAxisStep:
  ForwardStep                     { $$ = $1; }
| ForwardAxisStep Predicate       { $$ = new_expression2(XPATH_FILTER,$1,$2); }
;

ReverseAxisStep:
  ReverseStep                     { $$ = $1; }
| ReverseAxisStep Predicate       { $$ = new_expression2(XPATH_FILTER,$1,$2); }
;

ForwardStep:
  ForwardAxis NodeTest            { $$ = $2;
                                    $$->axis = $1; }
| AbbrevForwardStep               { $$ = $1; }
;

ForwardAxis:
  CHILD COLONCOLON                { $$ = AXIS_CHILD; }
| DESCENDANT COLONCOLON           { $$ = AXIS_DESCENDANT; }
| ATTRIBUTE COLONCOLON            { $$ = AXIS_ATTRIBUTE; }
| SELF COLONCOLON                 { $$ = AXIS_SELF; }
| DESCENDANT_OR_SELF COLONCOLON   { $$ = AXIS_DESCENDANT_OR_SELF; }
| FOLLOWING_SIBLING COLONCOLON    { $$ = AXIS_FOLLOWING_SIBLING; }
| FOLLOWING COLONCOLON            { $$ = AXIS_FOLLOWING; }
| NAMESPACE COLONCOLON            { $$ = AXIS_NAMESPACE; }
;

AbbrevForwardStep:
  NodeTest                        { $$ = $1;
                                    $$->axis = AXIS_CHILD;
                                    if ((XPATH_KIND_TEST == $$->type) &&
                                        ((KIND_ATTRIBUTE == $$->kind) ||
                                         (KIND_SCHEMA_ATTRIBUTE == $$->kind)))
                                      $$->axis = AXIS_ATTRIBUTE; }
| '@' NodeTest                    { $$ = $2;
                                    $$->axis = AXIS_ATTRIBUTE; }
;

ReverseStep:
  ReverseAxis NodeTest            { $$ = $2;
                                    $$->axis = $1; }
| AbbrevReverseStep               { $$ = $1; }
;

ReverseAxis:
  PARENT COLONCOLON               { $$ = AXIS_PARENT; }
| ANCESTOR COLONCOLON             { $$ = AXIS_ANCESTOR; }
| PRECEDING_SIBLING COLONCOLON    { $$ = AXIS_PRECEDING_SIBLING; }
| PRECEDING COLONCOLON            { $$ = AXIS_PRECEDING; }
| ANCESTOR_OR_SELF COLONCOLON     { $$ = AXIS_ANCESTOR_OR_SELF; }
;

AbbrevReverseStep:
  DOTDOT                          { /*$$ = new NodeTestExpr(XPATH_NODE_TEST_SEQUENCETYPE,
                                                          SequenceTypeImpl::node(),
                                                          AXIS_PARENT);*/ }
;

NodeTest:
  KindTest                        { $$ = $1; }
| NameTest                        { $$ = $1; }
;

NameTest:
  QName                           { $$ = new_expression(XPATH_NAME_TEST);
                                    $$->qn = $1; }
| Wildcard                        { $$ = new_expression(XPATH_NAME_TEST);
                                    $$->qn = $1; }
| ITEM                            { $$ = new_expression(XPATH_NAME_TEST);
                                    $$->qn.localpart = strdup("item"); }
;

Wildcard:
'*'                               { $$.uri = NULL;
                                    $$.prefix = NULL;
                                    $$.localpart = NULL; }
| NCName ':' '*'                  { $$.uri = strdup(lookup_nsuri(parse_node,$1));
                                    $$.prefix = $1;
                                    $$.localpart = NULL; }
| '*' ':' NCName                  { $$.uri = NULL;
                                    $$.prefix = NULL;
                                    $$.localpart = $3; }
;

FilterExpr:
  PrimaryExpr                     { $$ = $1; }
| FilterExpr Predicate            { $$ = new_expression2(XPATH_FILTER,$1,$2); }
;

Predicate:
  '[' Expr ']'                    { $$ = $2; }
;

PrimaryExpr:
  Literal                         { $$ = $1; }
| VarRef                          { $$ = $1; }
| ParenthesizedExpr               { $$ = $1; }
| ContextItemExpr                 { $$ = $1; }
| FunctionCall                    { $$ = $1; }
;

Literal:
  NumericLiteral                  { $$ = $1; }
| StringLiteral                   { $$ = $1; }
;

NumericLiteral:
  INTEGER_LITERAL                 { $$ = new_expression(XPATH_INTEGER_LITERAL);
                                    $$->num = $1; }
| DECIMAL_LITERAL                 { $$ = new_expression(XPATH_DECIMAL_LITERAL);
                                    $$->num = $1; }
| DOUBLE_LITERAL                  { $$ = new_expression(XPATH_DOUBLE_LITERAL);
                                    $$->num = $1; }
;

StringLiteral:
  STRING_LITERAL                  { $$ = new_expression(XPATH_STRING_LITERAL);
                                    $$->str = $1; }
;

VarRef:
'$' QName                         { $$ = new_expression(XPATH_VAR_REF);
                                    $$->qn = $2; }
;

ParenthesizedExpr:
  '(' ')'                         { $$ = new_expression(XPATH_EMPTY); }
| '(' Expr ')'                    { $$ = new_expression(XPATH_PAREN);
                                    $$->left = $2; }
;

ContextItemExpr:
  '.'                             { $$ = new_expression(XPATH_CONTEXT_ITEM); }
;

FunctionCallParams:
  ExprSingle                      { $$ = new_expression2(XPATH_ACTUAL_PARAM,$1,NULL); }
| ExprSingle ',' FunctionCallParams
                                  { $$ = new_expression2(XPATH_ACTUAL_PARAM,$1,$3); }
;

FunctionCall:
  QName '(' ')'                   { $$ = new_expression(XPATH_FUNCTION_CALL);
                                    $$->qn = $1; }
| QName '(' FunctionCallParams ')'
                                  { $$ = new_expression(XPATH_FUNCTION_CALL);
                                    $$->qn = $1;
                                    $$->left = $3; }
;

SingleType:
  AtomicType                      { $$ = $1; }
| AtomicType '?'                  { /*$$ = SequenceTypeImpl::occurrence($1,OCCURS_OPTIONAL);*/ }
;

SequenceType:
  ItemType                        { $$ = $1; }
/*FIXME: causes shift/reduce conflict with AdditiveExpr */
| ItemType OccurrenceIndicator    { /*$$ = SequenceTypeImpl::occurrence($1,$2);*/ }
| VOID '(' ')'                    { /*$$ = new SequenceTypeImpl(SEQTYPE_EMPTY);*/ }
;

OccurrenceIndicator:
  '?'                             { $$ = OCCURS_OPTIONAL; }
| '*'                             { $$ = OCCURS_ZERO_OR_MORE; }
| '+'                             { $$ = OCCURS_ONE_OR_MORE; }
;

ItemType:
  AtomicType                      { $$ = $1; }
| KindTest                        { $$ = NULL; /* FIXME $$ = $1;*/ }
| ITEM '(' ')'                    { /*$$ = SequenceTypeImpl::item();*/ }
;

AtomicType:
  QName                           { /*$$ = new SequenceTypeImpl(new ItemType(ITEM_ATOMIC));
                                      $$->itemType()->m_typeref = $1;*/ }
;

KindTest:
  DocumentTest                    { $$ = $1; }
| ElementTest                     { $$ = $1; }
| AttributeTest                   { $$ = $1; }
| SchemaElementTest               { $$ = $1; }
| SchemaAttributeTest             { $$ = $1; }
| PITest                          { $$ = $1; }
| CommentTest                     { $$ = $1; }
| TextTest                        { $$ = $1; }
| AnyKindTest                     { $$ = $1; }
;

DocumentTest:
DOCUMENT_NODE '(' ')'             { $$ = new_expression(XPATH_KIND_TEST);
                                    $$->kind = KIND_DOCUMENT; }
| DOCUMENT_NODE '(' ElementTest ')'
                                  { assert(!"FIXME: implement"); }
| DOCUMENT_NODE '(' SchemaElementTest ')'
                                  { assert(!"FIXME: implement"); }
;
 
ElementTest:
  ELEMENT '(' ')'                 { $$ = new_expression(XPATH_KIND_TEST);
                                    $$->kind = KIND_ELEMENT; }
| ELEMENT '(' ElementNameOrWildcard ')'
                                  { assert(!"FIXME: implement"); }
| ELEMENT '(' ElementNameOrWildcard ',' TypeName ')'
                                  { assert(!"FIXME: implement"); }
| ELEMENT '(' ElementNameOrWildcard ',' TypeName '?' ')'
                                  { assert(!"FIXME: implement"); }
;

AttributeTest:
  ATTRIBUTE '(' ')'               { $$ = new_expression(XPATH_KIND_TEST);
                                    $$->kind = KIND_ATTRIBUTE; }
| ATTRIBUTE '(' AttribNameOrWildcard ')'
                                  { assert(!"FIXME: implement"); }
| ATTRIBUTE '(' AttribNameOrWildcard ',' TypeName ')'
                                  { assert(!"FIXME: implement"); }
;

SchemaElementTest:
  SCHEMA_ELEMENT '(' QName ')'    { $$ = new_expression(XPATH_KIND_TEST);
                                    $$->kind = KIND_SCHEMA_ELEMENT; }
;

SchemaAttributeTest:
  SCHEMA_ATTRIBUTE '(' QName ')'  { $$ = new_expression(XPATH_KIND_TEST);
                                    $$->kind = KIND_SCHEMA_ATTRIBUTE; }
;

PITest:
PROCESSING_INSTRUCTION '(' ')'    { $$ = new_expression(XPATH_KIND_TEST);
                                    $$->kind = KIND_PI; }
| PROCESSING_INSTRUCTION '(' NCName ')'
                                  { assert(!"FIXME: implement"); }
| PROCESSING_INSTRUCTION '(' STRING_LITERAL ')'
                                  { assert(!"FIXME: implement"); }
;

CommentTest:
COMMENT '(' ')'                   { $$ = new_expression(XPATH_KIND_TEST);
                                    $$->kind = KIND_COMMENT; }
;

TextTest:
TEXT '(' ')'                      { $$ = new_expression(XPATH_KIND_TEST);
                                    $$->kind = KIND_TEXT; }
;

AnyKindTest:
NODE '(' ')'                      { $$ = new_expression(XPATH_KIND_TEST);
                                    $$->kind = KIND_ANY; }
;

AttribNameOrWildcard:
  QName                           { $$ = $1; }
| '*'                             { $$.uri = NULL;
                                    $$.prefix = NULL;
                                    $$.localpart = NULL; }
;

ElementNameOrWildcard:
  QName                           { $$ = $1; }
| '*'                             { $$.uri = NULL;
                                    $$.prefix = NULL;
                                    $$.localpart = NULL; }
;

TypeName:
  QName                           { $$ = $1; }
;

QName:
  NCName                          { $$.uri = strdup(lookup_nsuri(parse_node,""));
                                    $$.prefix = strdup("");
                                    $$.localpart = $1; }
| NCName ':' NCName               { $$.uri = strdup(lookup_nsuri(parse_node,$1));
                                    $$.prefix = $1;
                                    $$.localpart = $3; }
;


%%

int yyerror(const char *err)
{
  fprintf(stderr,"Parse error at line %d: %s\n",parse_firstline+lex_lineno,err);
  return 1;
}
