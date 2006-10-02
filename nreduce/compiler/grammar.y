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
#include "util.h"
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
%token SUPER
%token LETREC
%token IN
%token EQUALS
%token<i> INTEGER
%token<d> DOUBLE
%token<s> STRING

%type <c> SingleExpr
%type <c> AppliedExpr
%type <c> Expr
%type <c> SingleLambda
%type <c> Lambdas
%type <l> Arguments
%type <lr> Letrec
%type <lr> Letrecs
%left SYMBOL

%%

SingleExpr:
  NIL                             { $$ = snode_new(yyfileno,@1.first_line);
                                    $$->type = SNODE_NIL; }
| '(' ')'                         { $$ = snode_new(yyfileno,@1.first_line);
                                    $$->type = SNODE_NIL; }
| INTEGER                         { $$ = snode_new(yyfileno,@1.first_line);
                                    $$->type = SNODE_NUMBER;
                                    $$->num = (double)($1); }
| DOUBLE                          { $$ = snode_new(yyfileno,@1.first_line);
                                    $$->type = SNODE_NUMBER;
                                    $$->num = $1; }
| STRING                          { $$ = snode_new(yyfileno,@1.first_line);
                                    $$->type = SNODE_STRING;
                                    $$->value = strdup($1); }
| SYMBOL                          { $$ = snode_new(yyfileno,@1.first_line);
                                    $$->type = SNODE_SYMBOL;
                                    $$->name = strdup($1); }
| EQUALS                          { $$ = snode_new(yyfileno,@1.first_line);
                                    $$->type = SNODE_SYMBOL;
                                    $$->name = strdup("="); }
| '(' Expr ')'                    { $$ = $2; }
;

SingleLambda:
  LAMBDA SYMBOL  '.'              { $$ = snode_new(yyfileno,@1.first_line);
                                    $$->type = SNODE_LAMBDA;
                                    $$->name = (void*)strdup($2);
                                    $$->body = NULL; }
;

Lambdas:
  SingleLambda                    { $$ = $1; }
| SingleLambda Lambdas            { $$ = $1; $$->body = $2; }
;

AppliedExpr:
  SingleExpr                      { $$ = $1; }
| AppliedExpr SingleExpr          { $$ = snode_new(yyfileno,@1.first_line);
                                    $$->type = SNODE_APPLICATION;
                                    $$->left = $1;
                                    $$->right = $2; }
;

Letrec:
  SYMBOL EQUALS SingleExpr        { $$ = (letrec*)calloc(1,sizeof(letrec));
                                    $$->name = $1;
                                    $$->value = $3; }
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
| LETREC Letrecs IN SingleExpr    { $$ = snode_new(yyfileno,@1.first_line);
                                    $$->type = SNODE_LETREC;
                                    $$->bindings = $2;
                                    $$->body = $4; }
;

Arguments:
  SYMBOL                          { $$ = list_new($1,NULL); }
| SYMBOL Arguments                { $$ = list_new($1,$2); }
;

Definition:
  Arguments EQUALS SingleExpr     { char *name = (char*)$1->data;
                                    list *l;
                                    int argno = 0;
                                    scomb *sc;

                                    if (NULL != get_scomb(parse_src,name)) {
                                      fprintf(stderr,"Duplicate supercombinator: %s\n",name);
                                      return -1;
                                    }

                                    sc = add_scomb(parse_src,name);
                                    //    sc->sl = name->sl; /* FIXME */
                                    sc->nargs = list_count($1->next);
                                    sc->argnames = (char**)calloc(sc->nargs,sizeof(char*));
                                    for (l = $1->next; l; l = l->next)
                                      sc->argnames[argno++] = strdup((char*)l->data);
                                    list_free(l,free);

                                    sc->body = $3; }
;

Definitions:
  Definition                      { }
| Definition Definitions          { }
;

Program:
  Definitions                     { }
;

%%

int yyerror(const char *err)
{
  fprintf(stderr,"%s:%d: parse error: %s\n",yyfilename,yylloc.first_line,err);
  return 1;
}
