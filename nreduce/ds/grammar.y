%{

#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include "ds.h"

extern const char *dsfilename;
int dslex(void);
int dserror(const char *err);
extern rule *parse_rules;
extern mapping *parse_mappings;

%}


%union {
  struct snode *c;
  char *s;
  int i;
  struct rule *r;
  struct syntax *x;
  struct mapping *m;
  struct varname *v;
}

%start Specification

%token<s> IDENT
%token<s> STRING
%token<i> INTEGER
%token SQLEFT
%token SQRIGHT
%token PLUS
%token PLUSPLUS
%token PLUSPLUSPLUS

%type <v> VarName
%type <x> Call
%type <x> SyntaxPart
%type <x> Syntax
%type <r> Rule
%type <r> Rules
%type <m> Mapping
%type <m> Mappings
%type <r> Specification

%left SYMBOL

%%

VarName:
  IDENT                           { $$ = (varname*)calloc(1,sizeof(varname));
                                    $$->name = $1; }
| IDENT ':' ':' IDENT             { $$ = (varname*)calloc(1,sizeof(varname));
                                    $$->name = $1;
                                    $$->type = $4; }
| IDENT ':' INTEGER               { $$ = (varname*)calloc(1,sizeof(varname));
                                    $$->name = $1;
                                    $$->suffix = (char*)malloc(30);
                                    sprintf($$->suffix,"%d",$3); }
| IDENT ':' INTEGER ':' IDENT     { $$ = (varname*)calloc(1,sizeof(varname));
                                    $$->name = $1;
                                    $$->suffix = (char*)malloc(30);
                                    sprintf($$->suffix,"%d",$3);
                                    $$->type = $5; }
| '*'                             { $$ = (varname*)calloc(1,sizeof(varname));
                                    $$->name = strdup("*"); }
;

Call:
  IDENT SQLEFT VarName SQRIGHT    { $$ = (syntax*)calloc(1,sizeof(syntax));
                                    $$->type = SYNTAX_CALL;
                                    $$->name = $1;
                                    $$->vn = $3;
                                    $$->str = varname_str($3); }
;

SyntaxPart:
  STRING                          { $$ = (syntax*)calloc(1,sizeof(syntax));
                                    $$->type = SYNTAX_STRING;
                                    $$->str = $1; }
| VarName                         { $$ = (syntax*)calloc(1,sizeof(syntax));
                                    $$->type = SYNTAX_VAR;
                                    $$->vn = $1;
                                    $$->str = varname_str($1); }
| '@'                             { $$ = (syntax*)calloc(1,sizeof(syntax));
                                    $$->type = SYNTAX_PRINTORIG; }
| Call                            { $$ = $1; }
| PLUS Call                       { $$ = $2;
                                    $$->indent = 1; }
| PLUSPLUS Call                   { $$ = $2;
                                    $$->indent = 2; }
| PLUSPLUSPLUS Call               { $$ = $2;
                                    $$->indent = 3; }
;

Syntax:
  SyntaxPart                      { $$ = $1; }
| SyntaxPart Syntax               { $$ = $1;
                                    $$->next = $2; }
;

Rule:
  IDENT SQLEFT Syntax SQRIGHT '=' Syntax ';'
                                  { (void)@$.first_line; /* to get dsloc included */
                                    $$ = (rule*)calloc(1,sizeof(rule));
                                    $$->name = $1;
                                    $$->pattern = $3;
                                    $$->action = $6; }
;

Rules:
  Rule                            { $$ = $1; }
| Rule Rules                      { $$ = $1;
                                    $$->next = $2; }
;

Mapping:
  ':' IDENT IDENT IDENT ';'       { $$ = (mapping*)calloc(1,sizeof(mapping));
                                    $$->name = $2;
                                    $$->language = $3;
                                    $$->fun = $4; }
;

Mappings:
  Mapping                         { $$ = $1; }
| Mapping Mappings                { $$ = $1;
                                    $$->next = $2; }
;

Specification:
  Rules                           { parse_mappings = NULL;
                                    parse_rules = $1; }
| Mappings Rules                  { parse_mappings = $1;
                                    parse_rules = $2; }
;

%%

int dserror(const char *err)
{
  fprintf(stderr,"%s:%d: parse error: %s\n",dsfilename,dslloc.first_line,err);
  return 1;
}
