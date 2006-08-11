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

#include "nreduce.h"
#include <string.h>
#include <stdlib.h>

extern int yylineno;
extern char *yyfilename;
int yylex();
int yyerror(const char *err);

extern struct cell *parse_root;

%}


%union {
  struct cell *c;
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
  NIL                                           { $$ = globnil; }
| '(' ')'                                       { $$ = globnil; }
| INTEGER                                       { $$ = alloc_sourcecell(yyfilename,yylineno);
                                                  $$->tag = TYPE_INT;
                                                  $$->field1 = (void*)$1; }
| DOUBLE                                        { $$ = alloc_sourcecell(yyfilename,yylineno);
                                                  $$->tag = TYPE_DOUBLE;
                                                  celldouble($$) = $1; }
| STRING                                        { $$ = alloc_sourcecell(yyfilename,yylineno);
                                                  $$->tag = TYPE_STRING;
                                                  $$->field1 = (void*)strdup($1); }
| SYMBOL                                      { $$ = alloc_sourcecell(yyfilename,yylineno);
                                                  $$->tag = TYPE_SYMBOL;
                                                  $$->field1 = (void*)strdup($1); }
| BUILTIN                                       { $$ = alloc_sourcecell(yyfilename,yylineno);
                                                  $$->tag = TYPE_BUILTIN;
                                                  $$->field1 = (void*)$1; }
| EQUALS                                        { $$ = alloc_sourcecell(yyfilename,yylineno);
                                                  $$->tag = TYPE_BUILTIN;
                                                  $$->field1 = (void*)B_EQ; }
| '(' Expr ')'                                  { $$ = $2; }
;

SingleLambda:
  LAMBDA SYMBOL  '.'                          { $$ = alloc_sourcecell(yyfilename,yylineno);
                                                  $$->tag = TYPE_LAMBDA;
                                                  $$->field1 = (void*)strdup($2);
                                                  $$->field2 = NULL; }
;

Lambdas:
  SingleLambda                                  { $$ = $1; }
| SingleLambda Lambdas                          { $$ = $1; $$->field2 = $2; }
;

AppliedExpr:
  SingleExpr                                    { $$ = alloc_sourcecell(yyfilename,yylineno);
                                                  $$->tag = TYPE_CONS;
                                                  $$->field1 = $1;
                                                  $$->field2 = globnil; }
| SingleExpr AppliedExpr                        { $$ = alloc_sourcecell(yyfilename,yylineno);
                                                  $$->tag = TYPE_CONS;
                                                  $$->field1 = $1;
                                                  $$->field2 = $2; }
;

Expr:
  AppliedExpr                                   { $$ = $1; }
| Lambdas AppliedExpr                           { cell *c = $1;
                                                  while (c->field2)
                                                    c = (cell*)c->field2;
                                                  c->field2 = $2; }
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
