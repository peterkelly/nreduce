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

#include "nreduce.h"
#include <string.h>

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
%token LETREC
%token IN
%token PROGSTART
%token EQUALS
%token<i> INTEGER
%token<d> DOUBLE
%token<s> STRING

%debug

%type <c> SingleExpr
%type <c> AppliedExpr
%type <c> Expr
%type <c> SingleLambda
%type <c> Lambdas
%type <c> LetVar
%type <c> LetVars
%type <c> Arguments
%type <c> Supercombinator
%type <c> Supercombinators
%left SYMBOL

%%

LetVar:
  SYMBOL SingleExpr                           { $$ = alloc_sourcecell(yyfilename,yylineno);
                                                  $$->field1 = alloc_sourcecell(yyfilename,yylineno);
                                                  ((cell*)$$->field1)->tag = TYPE_VARDEF;
                                                  ((cell*)$$->field1)->field1 = (void*)strdup($1);
                                                  ((cell*)$$->field1)->field2 = $2;
                                                  $$->field2 = NULL;
                                                  $$->tag = TYPE_VARLNK; }
;

LetVars:
  LetVar                                        { $$ = $1; }
| LetVar LetVars                                { $$ = $1;
                                                  $$->field2 = $2; }
;

SingleExpr:
  NIL                                           { $$ = globnil; }
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
| LETREC LetVars IN                             { $$ = alloc_sourcecell(yyfilename,yylineno);
                                                  $$->tag = TYPE_LETREC;
                                                  $$->field1 = $2;
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

Arguments:
  SYMBOL Arguments                              { $$ = alloc_sourcecell(yyfilename,yylineno);
                                                  $$->tag = TYPE_VARDEF;
                                                  $$->field1 = strdup($1);
                                                  $$->field2 = $2; }
| { $$ = NULL; }
;

Supercombinator:
  SUPER '(' SYMBOL Arguments ')' SingleExpr  { scomb *sc = add_scomb($3);
                                         cell *argc;
                                         int argno;
                                         sc->nargs = 0;
                                         for (argc = $4; argc; argc = (cell*)argc->field2)
                                           sc->nargs++;
                                         sc->argnames = (char**)malloc(sc->nargs*sizeof(char*));
                                         argno = 0;
                                         for (argc = $4; argc; argc = (cell*)argc->field2)
                                           sc->argnames[argno++] = strdup((char*)argc->field1);
                                         sc->body = $6;
                                         $$ = NULL; }
;

Supercombinators:
  Supercombinator Supercombinators {}
| {}
;

Program:
  Supercombinators '(' Expr ')' { parse_root = $3; }
;

%%

int yyerror(const char *err)
{
  fprintf(stderr,"%s:%d: parse error: %s\n",yyfilename,yylineno,err);
  return 1;
}
