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
%token EQUALS
%token LBRACE
%token RBRACE
%token<i> INTEGER
%token<d> DOUBLE
%token<s> STRING

%type <c> SingleExpr
%type <c> ListExpr
%type <c> AppliedExpr
%type <c> Expr
%type <c> SingleLambda
%type <c> Lambdas
%type <s> Argument
%type <l> Arguments
%type <lr> Letrec
%type <lr> Letrecs
%left SYMBOL

%%

SingleExpr:
  NIL                             { $$ = snode_new(yyfileno,@$.first_line);
                                    $$->type = SNODE_NIL; }
| INTEGER                         { $$ = snode_new(yyfileno,@$.first_line);
                                    $$->type = SNODE_NUMBER;
                                    $$->num = (double)($1); }
| DOUBLE                          { $$ = snode_new(yyfileno,@$.first_line);
                                    $$->type = SNODE_NUMBER;
                                    $$->num = $1; }
| STRING                          { $$ = snode_new(yyfileno,@$.first_line);
                                    $$->type = SNODE_STRING;
                                    $$->value = strdup($1); }
| SYMBOL                          { $$ = snode_new(yyfileno,@$.first_line);
                                    $$->type = SNODE_SYMBOL;
                                    $$->name = strdup($1); }
| EQUALS                          { $$ = snode_new(yyfileno,@$.first_line);
                                    $$->type = SNODE_SYMBOL;
                                    $$->name = strdup("="); }
| LBRACE Expr RBRACE              { $$ = $2; }
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
                                    $$->name = (void*)strdup($2);
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
  SYMBOL EQUALS ListExpr          { $$ = (letrec*)calloc(1,sizeof(letrec));
                                    $$->name = $1;
                                    $$->value = $3; }
| LAMBDA SYMBOL EQUALS ListExpr   { $$ = (letrec*)calloc(1,sizeof(letrec));
                                    $$->name = $2;
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
  SYMBOL                          { $$ = $1; }
| LAMBDA SYMBOL                   { $$ = (char*)malloc(strlen($2)+2);
                                    sprintf($$,"!%s",$2);
                                    free($2); }
;

Arguments:
  Argument                        { $$ = list_new($1,NULL); }
| Argument Arguments              { $$ = list_new($1,$2); }
;

Definition:
  Arguments EQUALS ListExpr       { char *name = (char*)$1->data;
                                    list *l;
                                    int argno = 0;
                                    scomb *sc;
                                    char *scname;

                                    if (0 < strlen(parse_modname)) {
                                      scname = (char*)malloc(strlen(parse_modname)+1+
                                                             strlen(name)+1);
                                      sprintf(scname,"%s:%s",parse_modname,name);
                                    }
                                    else {
                                      scname = strdup(name);
                                    }

                                    if (NULL != get_scomb(parse_src,scname)) {
                                      sourceloc sl;
                                      sl.fileno = yyfileno;
                                      sl.lineno = @$.first_line;
                                      print_sourceloc(parse_src,stderr,sl);
                                      fprintf(stderr,"Duplicate supercombinator: %s\n",name);
                                      list_free($1,free);
                                      free(scname);
                                      return -1;
                                    }

                                    sc = add_scomb(parse_src,scname);
                                    sc->sl.fileno = yyfileno;
                                    sc->sl.lineno = @$.first_line;
                                    sc->nargs = list_count($1->next);
                                    sc->argnames = (char**)calloc(sc->nargs,sizeof(char*));
                                    sc->strictin = (int*)calloc(sc->nargs,sizeof(int));
                                    if (0 < strlen(parse_modname))
                                      sc->modname = strdup(parse_modname);
                                    for (l = $1->next; l; l = l->next) {
                                      char *argname = (char*)l->data;
                                      if ('!' == argname[0]) {
                                        argname++;
                                        sc->strictin[argno] = 1;
                                      }
                                      sc->argnames[argno++] = strdup(argname);
                                    }
                                    list_free($1,free);
                                    free(scname);

                                    sc->body = $3; }
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
