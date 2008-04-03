%{
/*
 * This file is part of the nreduce project
 * Copyright (C) 2006-2007 Peter Kelly (pmk@cs.adelaide.edu.au)
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
 * $Id: grammar.y 613 2007-08-23 11:40:12Z pmkelly $
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

extern const char *yyfilename;
extern int yyfileno;
int yylex(void);
int yyerror(const char *err);

extern struct source *parse_src;
extern const char *parse_modname;

%}


%union {
  struct snode *c;
  char *s;
  int i;
  double d;
  struct list *l;
  struct letrec *lr;
}

%start Program

%token<s> SYMBOL
%token LAMBDA
%token LAMBDAWORD
%token NIL
%token IMPORT
%token LETREC
%token IN
%token FUNCTION
%token LET
%token IF
%token ELSE
%token FOREACH
%token APPEND
%token EQ
%token NE
%token LE
%token GE
%token SHL
%token SHR
%token CONDAND
%token CONDOR
%token<i> INTEGER
%token<d> DOUBLE
%token<s> STRING

/* ELC syntax */

%type <c> Number
%type <c> SingleExpr
%type <c> ListExpr
%type <c> SingleLambda
%type <c> Lambdas
%type <c> AppliedExpr
%type <lr> Letrec
%type <lr> Letrecs
%type <c> Expr
%type <s> Argument
%type <l> Arguments

 /* New syntax */

%left SYMBOL

%%

/* ELC syntax */

Number:
  INTEGER                         { $$ = snode_new(yyfileno,@$.first_line);
                                    $$->type = SNODE_NUMBER;
                                    $$->num = (double)($1); }
| DOUBLE                          { $$ = snode_new(yyfileno,@$.first_line);
                                    $$->type = SNODE_NUMBER;
                                    $$->num = $1; }
;

SingleExpr:
  Number                          { $$ = $1; }
| NIL                             { $$ = snode_new(yyfileno,@$.first_line);
                                    $$->type = SNODE_NIL; }
| STRING                          { $$ = snode_new(yyfileno,@$.first_line);
                                    $$->type = SNODE_STRING;
                                    $$->value = strdup($1); }
| SYMBOL                          { $$ = snode_new(yyfileno,@$.first_line);
                                    $$->type = SNODE_SYMBOL;
                                    $$->name = make_varname($1);
                                    free($1);
                                  }
| '+'                             { $$ = makesym(yyfileno,@$.first_line,"+"); }
| '-'                             { $$ = makesym(yyfileno,@$.first_line,"-"); }
| '*'                             { $$ = makesym(yyfileno,@$.first_line,"*"); }
| '/'                             { $$ = makesym(yyfileno,@$.first_line,"/"); }
| '%'                             { $$ = makesym(yyfileno,@$.first_line,"%"); }

| EQ                              { $$ = makesym(yyfileno,@$.first_line,"=="); }
| NE                              { $$ = makesym(yyfileno,@$.first_line,"!="); }
| '<'                             { $$ = makesym(yyfileno,@$.first_line,"<"); }
| LE                              { $$ = makesym(yyfileno,@$.first_line,"<="); }
| '>'                             { $$ = makesym(yyfileno,@$.first_line,">"); }
| GE                              { $$ = makesym(yyfileno,@$.first_line,">="); }

| SHL                             { $$ = makesym(yyfileno,@$.first_line,"<<"); }
| SHR                             { $$ = makesym(yyfileno,@$.first_line,">>"); }
| '&'                             { $$ = makesym(yyfileno,@$.first_line,"&"); }
| '|'                             { $$ = makesym(yyfileno,@$.first_line,"|"); }
| '^'                             { $$ = makesym(yyfileno,@$.first_line,"^"); }
| '~'                             { $$ = makesym(yyfileno,@$.first_line,"~"); }

| CONDAND                         { $$ = makesym(yyfileno,@$.first_line,"and"); }
| CONDOR                          { $$ = makesym(yyfileno,@$.first_line,"or"); }
| IF                              { $$ = makesym(yyfileno,@$.first_line,"if"); }
| ELSE                            { $$ = makesym(yyfileno,@$.first_line,"else"); }
| '(' Expr ')'                    { $$ = $2; }
;

ListExpr:
  SingleExpr                      { $$ = $1; }
| SingleExpr ',' ListExpr         { snode *cons;
                                    snode *app1;

                                    cons = snode_new(yyfileno,@$.first_line);
                                    cons->type = SNODE_SYMBOL;
                                    cons->name = strdup("cons");

                                    app1 = snode_new(yyfileno,@$.first_line);
                                    app1->type = SNODE_APPLICATION;
                                    app1->left = cons;
                                    app1->right = $1;

                                    $$ = snode_new(yyfileno,@$.first_line);
                                    $$->type = SNODE_APPLICATION;
                                    $$->left = app1;
                                    $$->right = $3; }
;

SingleLambda:
  LAMBDA SYMBOL  '.'              { $$ = snode_new(yyfileno,@$.first_line);
                                    $$->type = SNODE_LAMBDA;
                                    $$->name = make_varname($2);
                                    free($2);
                                    $$->body = NULL; }
;

Lambdas:
  SingleLambda                    { $$ = $1; }
| SingleLambda Lambdas            { $$ = $1; $$->body = $2; }
;

AppliedExpr:
  ListExpr                        { $$ = $1; }
| AppliedExpr ListExpr            { $$ = snode_new(yyfileno,@$.first_line);
                                    $$->type = SNODE_APPLICATION;
                                    $$->left = $1;
                                    $$->right = $2; }
;

Letrec:
  SYMBOL '=' ListExpr             { $$ = (letrec*)calloc(1,sizeof(letrec));
                                    $$->name = make_varname($1);
                                    free($1);
                                    $$->value = $3; }
| LAMBDA SYMBOL '=' ListExpr      { $$ = (letrec*)calloc(1,sizeof(letrec));
                                    $$->name = make_varname($2);
                                    free($2);
                                    $$->value = $4;
                                    $$->strict = 1; }
;

Letrecs:
  Letrec                          { $$ = $1; }
| Letrec Letrecs                  { $$ = $1;
                                    $$->next = $2; }
;

Expr:
  AppliedExpr                     { $$ = $1; }
| Lambdas AppliedExpr             { snode *c = $1;
                                    while (c->body)
                                      c = c->body;
                                    c->body = $2; }
| LETREC Letrecs IN ListExpr      { $$ = snode_new(yyfileno,@$.first_line);
                                    $$->type = SNODE_LETREC;
                                    $$->bindings = $2;
                                    $$->body = $4; }
;

Argument:
  SYMBOL                          { $$ = make_varname($1);
                                    free($1); }
| LAMBDA SYMBOL                   { char *name = make_varname($2);
                                    $$ = (char*)malloc(strlen(name)+2);
                                    sprintf($$,"!%s",name);
                                    free(name);
                                    free($2); }
;

Arguments:
  Argument                        { $$ = list_new($1,NULL); }
| Argument Arguments              { $$ = list_new($1,$2); }
;

Supercombinator:
  Arguments '=' ListExpr          { snode *body = $3;
                                    char *name = (char*)$1->data;
                                    list *argnames = $1->next;
                                    int r = create_scomb(parse_src,parse_modname,name,
                                                         argnames,body,yyfileno,@$.first_line);
                                    list_free($1,free);
                                    if (0 != r)
                                      return -1;
                                  }
;

/* Common */

Definition:
  Supercombinator                 { }
;

Definitions:
  Definition                      { }
| Definition Definitions          { }
;

Import:
  IMPORT SYMBOL                   { add_import(parse_src,$2); free($2); }
;

Imports:
  Import Imports                  { }
| /* empty */                     { }
;

Program:
  Imports Definitions             { }
;

%%

int yyerror(const char *err)
{
  fprintf(stderr,"%s:%d: parse error: %s\n",yyfilename,yylloc.first_line,err);
  return 1;
}
