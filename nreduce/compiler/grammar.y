%{
/*
 * This file is part of the GridXSLT project
 * Copyright (C) 2006 Peter Kelly (pmk@cs.adelaide.edu.au)
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

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "src/nreduce.h"
#include "compiler/source.h"
#include "runtime/runtime.h"
#include <string.h>
#include <stdlib.h>

#ifdef WIN32
#define strdup _strdup
#endif

extern int yylineno;
extern const char *yyfilename;
extern int yyfileno;
int yylex(void);
int yyerror(const char *err);

extern struct snode *parse_root;

%}


%union {
  struct snode *c;
  char *s;
  int i;
  double d;
  int b;
}

%start Program

%token<s> SYMBOL
%token<b> BUILTIN
%token LAMBDA
%token NIL
%token SUPER
%token IN
%token PROGSTART
%token EQUALS
%token<i> INTEGER
%token<d> DOUBLE
%token<s> STRING

%type <c> SingleExpr
%type <c> AppliedExpr
%type <c> Expr
%type <c> SingleLambda
%type <c> Lambdas
%left SYMBOL

%%

SingleExpr:
  NIL                                           { $$ = snode_new(yyfileno,yylineno);
                                                  $$->tag = TYPE_NIL; }
| '(' ')'                                       { $$ = snode_new(yyfileno,yylineno);
                                                  $$->tag = TYPE_NIL; }
| INTEGER                                       { $$ = snode_new(yyfileno,yylineno);
                                                  $$->tag = TYPE_NUMBER;
                                                  $$->num = (double)($1); }
| DOUBLE                                        { $$ = snode_new(yyfileno,yylineno);
                                                  $$->tag = TYPE_NUMBER;
                                                  $$->num = $1; }
| STRING                                        { $$ = snode_new(yyfileno,yylineno);
                                                  $$->tag = TYPE_STRING;
                                                  $$->value = strdup($1); }
| SYMBOL                                      { $$ = snode_new(yyfileno,yylineno);
                                                  $$->tag = TYPE_SYMBOL;
                                                  $$->name = strdup($1); }
| BUILTIN                                       { $$ = snode_new(yyfileno,yylineno);
                                                  $$->tag = TYPE_BUILTIN;
                                                  $$->bif = $1; }
| EQUALS                                        { $$ = snode_new(yyfileno,yylineno);
                                                  $$->tag = TYPE_BUILTIN;
                                                  $$->bif = B_EQ; }
| '(' Expr ')'                                  { $$ = $2; }
;

SingleLambda:
  LAMBDA SYMBOL  '.'                          { $$ = snode_new(yyfileno,yylineno);
                                                  $$->tag = TYPE_LAMBDA;
                                                  $$->name = (void*)strdup($2);
                                                  $$->body = NULL; }
;

Lambdas:
  SingleLambda                                  { $$ = $1; }
| SingleLambda Lambdas                          { $$ = $1; $$->body = $2; }
;

AppliedExpr:
  SingleExpr                                    { $$ = snode_new(yyfileno,yylineno);
                                                  $$->tag = TYPE_CONS;
                                                  $$->left = $1;
                                                  $$->right = snode_new(yyfileno,yylineno);
                                                  $$->right->tag = TYPE_NIL; }
| SingleExpr AppliedExpr                        { $$ = snode_new(yyfileno,yylineno);
                                                  $$->tag = TYPE_CONS;
                                                  $$->left = $1;
                                                  $$->right = $2; }
;

Expr:
  AppliedExpr                                   { $$ = $1; }
| Lambdas AppliedExpr                           { snode *c = $1;
                                                  while (c->body)
                                                    c = c->body;
                                                  c->body = $2; }
;

Program:
  AppliedExpr { parse_root = $1; }
;

%%

int yyerror(const char *err)
{
  fprintf(stderr,"%s:%d: parse error: %s\n",yyfilename,yylineno,err);
  return 1;
}
