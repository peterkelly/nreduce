%{

#include "util/String.h"
#include "Reducer.h"

using namespace GridXSLT;

extern int mllex_lineno;
int yylex();
int yyerror(const char *err);

extern Cell *rootCell;

%}


%union {
  GridXSLT::Cell *c;
  GridXSLT::StringImpl *s;
}

%start Program

%token<s> CONSTANT
%token<s> VARIABLE
%token<s> BUILTIN
%token LAMBDA
%token NIL
%left SPACE


%debug

%type <c> Expr
%type <c> Body

%%

Body:
  Expr { $$ = $1; }
;

Expr:
  CONSTANT                                      { $$ = new Cell();
                                                  $$->m_type = ML_CONSTANT;
                                                  $$->m_value = String($1); }
| NIL                                           { $$ = new Cell();
                                                  $$->m_type = ML_NIL; }
| VARIABLE                                      { $$ = new Cell();
                                                  $$->m_type = ML_VARIABLE;
                                                  $$->m_value = String($1); }
| BUILTIN                                       { $$ = new Cell();
                                                  $$->m_type = ML_BUILTIN;
                                                  $$->m_value = String($1); }
| Expr SPACE Expr                                     { $$ = new Cell();
                                                  $$->m_type = ML_APPLICATION;
                                                  $$->m_left = $1->ref();
                                                  $$->m_right = $3->ref(); }
| LAMBDA VARIABLE  '.' Body                     { $$ = new Cell();
                                                  $$->m_value = String($2);
                                                  $$->m_left = $4->ref();
                                                  $$->m_type = ML_LAMBDA; }
| '(' Expr ')'                                  { $$ = $2; }
| SPACE Expr { $$ = $2; }
| Expr SPACE { $$ = $1; }
;

Program:
  Expr { rootCell = $1->ref(); }
;

%%

int yyerror(const char *err)
{
  fmessage(stderr,"Parse error at line %d: %s\n",mllex_lineno,err);
  return 1;
}
