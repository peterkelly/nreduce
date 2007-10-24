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
  char *string;
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

%token <string> DSVAR_EXPRESSION
%token <string> DSVAR_FORWARD_AXIS
%token <string> DSVAR_REVERSE_AXIS
%token <string> DSVAR_NUMERIC
%token <string> DSVAR_STRING
%token <string> DSVAR_QNAME
%token <string> DSVAR_NODETEST
%token DSTYPE_AXIS
%token DSTYPE_NODETEST

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
%token AVT_EMPTY
%token <number> NUMERIC_LITERAL

%type <qn> QName

%type <expr> Top
%type <expr> AVTLiteral
%type <expr> AVTExpr
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
%type <expr> StepExpr
%type <expr> ForwardStep
%type <expr> ForwardAxis
%type <expr> AbbrevForwardStep
%type <expr> ReverseStep
%type <expr> ReverseAxis
%type <expr> AbbrevReverseStep
%type <expr> NodeTest
%type <expr> NameTest
%type <qn> Wildcard
%type <expr> FilterExpr
%type <expr> VarPredicate
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

AVTExpr:
  AVTLiteral                      { $$ = $1; }
| AVTLiteral Expr                 { $$ = new_expression2(XPATH_AVT_COMPONENT,$1,$2); }
| AVTLiteral AVTExpr              { $$ = new_expression2(XPATH_AVT_COMPONENT,$1,$2); }
| AVTLiteral Expr AVTExpr         { expression *e = new_expression2(XPATH_AVT_COMPONENT,$2,$3);
                                    $$ = new_expression2(XPATH_AVT_COMPONENT,$1,e); }
| AVT_EMPTY                       { $$ = new_expression(XPATH_STRING_LITERAL);
                                    $$->str = strdup(""); }
| AVT_EMPTY Expr                  { $$ = $2; }
| AVT_EMPTY AVTExpr               { $$ = $2; }
| AVT_EMPTY Expr AVTExpr          { $$ = new_expression2(XPATH_AVT_COMPONENT,$2,$3); }
;

Top:
  Expr                            { parse_expr = $1; }
| AVTExpr                         { parse_expr = $1; }
| Expr AVTExpr                    { parse_expr = new_expression2(XPATH_AVT_COMPONENT,$1,$2); }
| VarPredicate                    { parse_expr = $1; }
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
                                  { $$ = new_expression(XPATH_IF);
                                    $$->r.test = $3;
                                    $$->r.left = $6;
                                    $$->r.right = $8; }
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
                                    $$->r.left = $2; }
| '+' UnaryExpr                   { $$ = new_expression(XPATH_UNARY_PLUS);
                                    $$->r.left = $2 }
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
| '/' StepExpr                    { expression *root = new_expression2(XPATH_ROOT,NULL,NULL);
                                    $$ = new_expression2(XPATH_STEP,root,$2); }
| SLASHSLASH StepExpr             { expression *root = new_expression2(XPATH_ROOT,NULL,NULL);
                                    expression *step1;
                                    expression *test = new_expression(XPATH_KIND_TEST);
                                    expression *axis = new_expression(XPATH_AXIS);
                                    expression *nt = new_expression2(XPATH_NODE_TEST,axis,test);
                                    axis->axis = AXIS_DESCENDANT_OR_SELF;
                                    test->kind = KIND_ANY;
                                    step1 = new_expression2(XPATH_STEP,root,nt);
                                    $$ = new_expression2(XPATH_STEP,step1,$2); }
| StepExpr                        { $$ = $1; }
| PathExpr '/' StepExpr           { $$ = new_expression2(XPATH_STEP,$1,$3); }
| PathExpr SLASHSLASH StepExpr
                                  { expression *step1;
                                    expression *test = new_expression(XPATH_KIND_TEST);
                                    expression *axis = new_expression(XPATH_AXIS);
                                    expression *nt = new_expression2(XPATH_NODE_TEST,axis,test);
                                    axis->axis = AXIS_DESCENDANT_OR_SELF;
                                    test->kind = KIND_ANY;
                                    step1 = new_expression2(XPATH_STEP,$1,nt);
                                    $$ = new_expression2(XPATH_STEP,step1,$3); }
;

StepExpr:
  ForwardStep                     { $$ = $1; }
| ForwardStep '[' Predicate       { $$ = new_expression2(XPATH_FILTER,$1,$3); }
| ReverseStep                     { $$ = $1; }
| ReverseStep '[' Predicate       { $$ = new_expression2(XPATH_FILTER,$1,$3); }
| FilterExpr                      { $$ = $1; }
;

ForwardStep:
  ForwardAxis COLONCOLON NodeTest { $$ = $3;
                                    $$->r.left = $1; }
| AbbrevForwardStep               { $$ = $1; }
;

ForwardAxis:
  CHILD                           { $$ = new_expression(XPATH_AXIS);
                                    $$->axis = AXIS_CHILD; }
| DESCENDANT                      { $$ = new_expression(XPATH_AXIS);
                                    $$->axis = AXIS_DESCENDANT; }
| ATTRIBUTE                       { $$ = new_expression(XPATH_AXIS);
                                    $$->axis = AXIS_ATTRIBUTE; }
| SELF                            { $$ = new_expression(XPATH_AXIS);
                                    $$->axis = AXIS_SELF; }
| DESCENDANT_OR_SELF              { $$ = new_expression(XPATH_AXIS);
                                    $$->axis = AXIS_DESCENDANT_OR_SELF; }
| FOLLOWING_SIBLING               { $$ = new_expression(XPATH_AXIS);
                                    $$->axis = AXIS_FOLLOWING_SIBLING; }
| FOLLOWING                       { $$ = new_expression(XPATH_AXIS);
                                    $$->axis = AXIS_FOLLOWING; }
| NAMESPACE                       { $$ = new_expression(XPATH_AXIS);
                                    $$->axis = AXIS_NAMESPACE; }
| DSVAR_FORWARD_AXIS              { $$ = new_expression(XPATH_AXIS);
                                    $$->axis = AXIS_DSVAR_FORWARD;
                                    $$->str = $1; }
;

AbbrevForwardStep:
  NodeTest                        { $$ = $1;
                                    $$->r.left = new_expression(XPATH_AXIS);
                                    $$->r.left->axis = AXIS_CHILD;
                                    if ((XPATH_KIND_TEST == $$->r.right->type) &&
                                        ((KIND_ATTRIBUTE == $$->r.right->kind) ||
                                         (KIND_SCHEMA_ATTRIBUTE == $$->r.right->kind)))
                                      $$->r.left->axis = AXIS_ATTRIBUTE; }
| '@' NodeTest                    { $$ = $2;
                                    $$->r.left = new_expression(XPATH_AXIS);
                                    $$->r.left->axis = AXIS_ATTRIBUTE; }
;

ReverseStep:
  ReverseAxis COLONCOLON NodeTest { $$ = $3;
                                    $$->r.left = $1; }
| AbbrevReverseStep               { $$ = $1; }
;

ReverseAxis:
  PARENT                          { $$ = new_expression(XPATH_AXIS);
                                    $$->axis = AXIS_PARENT; }
| ANCESTOR                        { $$ = new_expression(XPATH_AXIS);
                                    $$->axis = AXIS_ANCESTOR; }
| PRECEDING_SIBLING               { $$ = new_expression(XPATH_AXIS);
                                    $$->axis = AXIS_PRECEDING_SIBLING; }
| PRECEDING                       { $$ = new_expression(XPATH_AXIS);
                                    $$->axis = AXIS_PRECEDING; }
| ANCESTOR_OR_SELF                { $$ = new_expression(XPATH_AXIS);
                                    $$->axis = AXIS_ANCESTOR_OR_SELF; }
| DSVAR_REVERSE_AXIS              { $$ = new_expression(XPATH_AXIS);
                                    $$->axis = AXIS_DSVAR_REVERSE;
                                    $$->str = $1; }
;

AbbrevReverseStep:
  DOTDOT                          { abort();
                                    /*$$ = new NodeTestExpr(XPATH_NODE_TEST_SEQUENCETYPE,
                                                          SequenceTypeImpl::node(),
                                                          AXIS_PARENT);*/ }
;

NodeTest:
  KindTest                        { $$ = new_expression2(XPATH_NODE_TEST,NULL,$1); }
| NameTest                        { $$ = new_expression2(XPATH_NODE_TEST,NULL,$1); }
| DSVAR_NODETEST                  { $$ = new_expression(XPATH_DSVAR_NODETEST);
                                    $$->str = $1; }
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
| PrimaryExpr '[' Predicate       { $$ = new_expression2(XPATH_FILTER,$1,$3); }
;

Predicate:
  Expr ']'                        { $$ = $1; }
| Expr ']' '[' Predicate          { $$ = new_expression2(XPATH_PREDICATE,$1,$4); }
;

VarPredicate:
  DSVAR_EXPRESSION ']' '[' DSVAR_EXPRESSION
                                  { expression *dsvar1 = new_expression(XPATH_DSVAR);
                                    expression *dsvar2 = new_expression(XPATH_DSVAR);
                                    dsvar1->str = $1;
                                    dsvar2->str = $4;
                                    $$ = new_expression2(XPATH_PREDICATE,dsvar1,dsvar2); }
;

PrimaryExpr:
  Literal                         { $$ = $1; }
| VarRef                          { $$ = $1; }
| ParenthesizedExpr               { $$ = $1; }
| ContextItemExpr                 { $$ = $1; }
| FunctionCall                    { $$ = $1; }
| DSVAR_EXPRESSION                { $$ = new_expression(XPATH_DSVAR);
                                    $$->str = $1; }
| DSTYPE_AXIS ForwardAxis         { $$ = $2; }
| DSTYPE_AXIS ReverseAxis         { $$ = $2; }
| DSTYPE_NODETEST KindTest        { $$ = $2; }
| DSTYPE_NODETEST NameTest        { $$ = $2; }
;

Literal:
  NumericLiteral                  { $$ = $1; }
| StringLiteral                   { $$ = $1; }
;

NumericLiteral:
  NUMERIC_LITERAL                 { $$ = new_expression(XPATH_NUMERIC_LITERAL);
                                    $$->num = $1; }
| DSVAR_NUMERIC                   { $$ = new_expression(XPATH_DSVAR_NUMERIC);
                                    $$->str = $1; }
;

StringLiteral:
  STRING_LITERAL                  { $$ = new_expression(XPATH_STRING_LITERAL);
                                    $$->str = $1; }
| DSVAR_STRING                    { $$ = new_expression(XPATH_DSVAR_STRING);
                                    $$->str = $1; }
;

VarRef:
'$' QName                         { $$ = new_expression(XPATH_VAR_REF);
                                    $$->qn = $2; }
| '$' DSVAR_QNAME                 { $$ = new_expression(XPATH_DSVAR_VAR_REF);
                                    $$->str = $2; }
;

ParenthesizedExpr:
  '(' ')'                         { $$ = new_expression(XPATH_EMPTY); }
| '(' Expr ')'                    { $$ = $2; }
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
                                    $$->r.left = $3; }
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
