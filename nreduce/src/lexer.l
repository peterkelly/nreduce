%option noyywrap
%option outfile="lexer.yy.c"
%x vstr1 comment comment2 intline
%{

#include "grammar.tab.h"
#include "nreduce.h"

#define MAX_STR_CONST 1024 /* FIXME: allow bigger strings! */

char string_buf[MAX_STR_CONST];
char *string_buf_ptr;

char *yyfilename = NULL;

%}

DIGIT      [0-9]
DIGITS     {DIGIT}+
LETTER     [a-zA-Z]


XINTEGER {DIGITS}
XDOUBLE {DIGITS}\.{DIGITS}
IDENT ([a-zA-Z]|_)([a-zA-Z0-9]|_)*

%%



"/*"         BEGIN(comment);

<comment>[^*\n]*        /* eat anything that's not a '*' */
<comment>"*"+[^*/\n]*   /* eat up '*'s not followed by '/'s */
<comment>\n             
<comment>"*"+"/"        { BEGIN(INITIAL); }

"//"                     { BEGIN(comment2); }
<comment2>[^\n]*
<comment2>\n              { BEGIN(INITIAL); }

"#!"                      { BEGIN(intline); }
<intline>[^\n]*
<intline>\n               { BEGIN(INITIAL); }

"progstart"  {           return PROGSTART; }
"!"     {         return LAMBDA; }
"nil"   {         return NIL; }
"super" {         return SUPER; }
"letrec" {        return LETREC; }
"in"    {         return IN; }
"."     {         return '.'; }
"("     {         return '('; }
")"     {         return ')'; }
"["     {         return '['; }
"]"     {         return ']'; }
","     {         return ','; }
":"     {         return ':'; }
"+"     {         yylval.b = B_ADD; return BUILTIN; }
"-"     {         yylval.b = B_SUBTRACT; return BUILTIN; }
"*"     {         yylval.b = B_MULTIPLY; return BUILTIN; }
"/"     {         yylval.b = B_DIVIDE; return BUILTIN; }
"%"     {         yylval.b = B_MOD; return BUILTIN; }
"="     {         return EQUALS; }
"!="    {         yylval.b = B_NE; return BUILTIN; }
"<"     {         yylval.b = B_LT; return BUILTIN; }
"<="    {         yylval.b = B_LE; return BUILTIN; }
">"     {         yylval.b = B_GT; return BUILTIN; }
">="    {         yylval.b = B_GE; return BUILTIN; }
"&&"    {         yylval.b = B_AND; return BUILTIN; }
"||"    {         yylval.b = B_OR; return BUILTIN; }
"ap"    {         yylval.b = B_AP; return BUILTIN; }
"if"    {         yylval.b = B_IF; return BUILTIN; }
"cons"  {         yylval.b = B_CONS; return BUILTIN; }
"head"  {         yylval.b = B_HEAD; return BUILTIN; }
"tail"  {         yylval.b = B_TAIL; return BUILTIN; }
"lambda?" {       yylval.b = B_ISLAMBDA; return BUILTIN; }
"value?" {        yylval.b = B_ISVALUE; return BUILTIN; }
"cons?" {         yylval.b = B_ISCONS; return BUILTIN; }
"nil?"  {         yylval.b = B_ISNIL; return BUILTIN; }
"int?"  {         yylval.b = B_ISINT; return BUILTIN; }
"double?"  {      yylval.b = B_ISDOUBLE; return BUILTIN; }
"string?"  {      yylval.b = B_ISSTRING; return BUILTIN; }
"sqrt"  {         yylval.b = B_SQRT; return BUILTIN; }
"neg"  {          yylval.b = B_NEG; return BUILTIN; }
"union"  {        yylval.b = B_UNION; return BUILTIN; }
"intersect"  {    yylval.b = B_INTERSECT; return BUILTIN; }
{IDENT} {         yylval.s = strdup(yytext);
                  return VARIABLE; }

{XINTEGER} {         yylval.i = atoi(yytext); return INTEGER; }
{XDOUBLE}  {         yylval.d = atof(yytext); return DOUBLE; }





\"      string_buf_ptr = string_buf; BEGIN(vstr1);

<vstr1>\"         { /* saw closing quote - all done */
                  BEGIN(INITIAL);
                  *string_buf_ptr = '\0';
                  yylval.s = strdup(string_buf);
                  return STRING;
                }

<vstr1>\\n        *string_buf_ptr++ = '\n';
<vstr1>\\t        *string_buf_ptr++ = '\t';
<vstr1>\\r        *string_buf_ptr++ = '\r';
<vstr1>\\b        *string_buf_ptr++ = '\b';
<vstr1>\\f        *string_buf_ptr++ = '\f';

<vstr1>\\(.|\n)   *string_buf_ptr++ = yytext[1];

<vstr1>[^\\\n\"]+ {
                  char *yptr = yytext;

                  while ( *yptr )
                    *string_buf_ptr++ = *yptr++;
                }
(" "|\n|\t)+ { 
char *c;
for (c = yytext; *c; c++)
  if ('\n' == *c)
    yylineno++;
}
. { printf("Unrecognised string: \"%s\"\n",yytext); return -1;
if (0); yyunput(0,0); /* hide warning about yyunput not being used */  }

%%

