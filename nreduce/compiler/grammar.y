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
%token NIL
%token IMPORT
%token LETREC
%token IN
%token<d> NUMBER
%token<s> STRING

%type <c> SingleExpr
%type <c> Lambdas
%type <c> AppliedExpr
%type <c> LetrecValue
%type <lr> Letrec
%type <lr> Letrecs
%type <s> Argument
%type <l> Arguments

%left SYMBOL

%%

SingleExpr:
  NIL                             { $$ = snode_new(yyfileno,@$.first_line);
                                    $$->type = SNODE_NIL; }
| NUMBER                          { $$ = snode_new(yyfileno,@$.first_line);
                                    $$->type = SNODE_NUMBER;
                                    $$->num = $1; }
| STRING                          { $$ = snode_new(yyfileno,@$.first_line);
                                    $$->type = SNODE_STRING;
                                    $$->value = strdup($1); }
| SYMBOL                          { $$ = snode_new(yyfileno,@$.first_line);
                                    $$->type = SNODE_SYMBOL;
                                    $$->name = make_varname($1);
                                    free($1);
                                  }
| '(' Lambdas ')'                 { $$ = $2; }
| '(' LETREC Letrecs IN SingleExpr ')'
                                  { $$ = snode_new(yyfileno,@$.first_line);
                                    $$->type = SNODE_LETREC;
                                    $$->bindings = $3;
                                    $$->body = $5; }
;

Lambdas:
  LAMBDA SYMBOL  '.' Lambdas      { $$ = snode_new(yyfileno,@$.first_line);
                                    $$->type = SNODE_LAMBDA;
                                    $$->name = make_varname($2);
                                    free($2);
                                    $$->body = $4; }
| AppliedExpr                     { $$ = $1; }
;

AppliedExpr:
  SingleExpr                      { $$ = $1; }
| AppliedExpr SingleExpr          { $$ = snode_new(yyfileno,@$.first_line);
                                    $$->type = SNODE_APPLICATION;
                                    $$->left = $1;
                                    $$->right = $2; }
;

LetrecValue:
  '=' SingleExpr                  { $$ = $2; }
| SYMBOL LetrecValue              { $$ = snode_new(yyfileno,@$.first_line);
                                    $$->type = SNODE_LAMBDA;
                                    $$->name = make_varname($1);
                                    free($1);
                                    $$->body = $2; }
;

Letrec:
  SYMBOL LetrecValue              { $$ = (letrec*)calloc(1,sizeof(letrec));
                                    $$->name = make_varname($1);
                                    free($1);
                                    $$->value = $2; }
| LAMBDA SYMBOL LetrecValue       { $$ = (letrec*)calloc(1,sizeof(letrec));
                                    $$->name = make_varname($2);
                                    free($2);
                                    $$->value = $3;
                                    $$->strict = 1; }
;

Letrecs:
  Letrec                          { $$ = $1; }
| Letrec Letrecs                  { $$ = $1;
                                    $$->next = $2; }
;

Argument:
  SYMBOL                          { $$ = make_varname($1);
                                    free($1); }
| LAMBDA SYMBOL                   { char *name = make_varname($2);
                                    $$ = (char*)malloc(strlen(name)+2);
                                    sprintf($$,"!%s",name);
                                    free(name);
                                    free($2); }
| '@' SYMBOL                      { char *name = make_varname($2);
                                    $$ = (char*)malloc(strlen(name)+2);
                                    sprintf($$,"@%s",name);
                                    free(name);
                                    free($2); }
;

Arguments:
  Argument                        { $$ = list_new($1,NULL); }
| Argument Arguments              { $$ = list_new($1,$2); }
;

Definition:
  Arguments '=' SingleExpr        { snode *body = $3;
                                    char *name = (char*)$1->data;
                                    list *argnames = $1->next;
                                    int r = create_scomb(parse_src,parse_modname,name,
                                                         argnames,body,yyfileno,@$.first_line);
                                    list_free($1,free);
                                    if (0 != r)
                                      return -1;
                                  }
;

Definitions:
  Definition                      { }
| Definition Definitions          { }
;

Import:
IMPORT SYMBOL                     { add_import(parse_src,$2); free($2); }
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
