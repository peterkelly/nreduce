%option noyywrap
%option outfile="lexer.yy.c"
%x vstr1 comment comment2
%{

#include "util/String.h"
#include "Reducer.h"
#include "grammar.tab.h"

using namespace GridXSLT;

#define MAX_STR_CONST 1024

char string_buf[MAX_STR_CONST];
char *string_buf_ptr;

int mllex_lineno;

%}

DIGIT      [0-9]
DIGITS     {DIGIT}+
LETTER     [a-zA-Z]


INTEGER {DIGITS}
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

"!"     {         return LAMBDA; }
"nil"   {         return NIL; }
"."     {         return '.'; }
"("     {         return '('; }
")"     {         return ')'; }
"+"     {         mllval.s = new StringImpl(mltext); return BUILTIN; }
"-"     {         mllval.s = new StringImpl(mltext); return BUILTIN; }
"*"     {         mllval.s = new StringImpl(mltext); return BUILTIN; }
"/"     {         mllval.s = new StringImpl(mltext); return BUILTIN; }
"%"     {         mllval.s = new StringImpl(mltext); return BUILTIN; }
"="     {         mllval.s = new StringImpl(mltext); return BUILTIN; }
"!="    {         mllval.s = new StringImpl(mltext); return BUILTIN; }
"<"     {         mllval.s = new StringImpl(mltext); return BUILTIN; }
"<="    {         mllval.s = new StringImpl(mltext); return BUILTIN; }
">"     {         mllval.s = new StringImpl(mltext); return BUILTIN; }
">="    {         mllval.s = new StringImpl(mltext); return BUILTIN; }
"and"   {         mllval.s = new StringImpl(mltext); return BUILTIN; }
"or"    {         mllval.s = new StringImpl(mltext); return BUILTIN; }
"if"    {         mllval.s = new StringImpl(mltext); return BUILTIN; }
"ap"    {         mllval.s = new StringImpl(mltext); return BUILTIN; }
"eq"    {         mllval.s = new StringImpl(mltext); return BUILTIN; }
"cons"  {         mllval.s = new StringImpl(mltext); return BUILTIN; }
"head"  {         mllval.s = new StringImpl(mltext); return BUILTIN; }
"tail"  {         mllval.s = new StringImpl(mltext); return BUILTIN; }
"lambda?" {         mllval.s = new StringImpl(mltext); return BUILTIN; }
"value?" {         mllval.s = new StringImpl(mltext); return BUILTIN; }
"cons?" {         mllval.s = new StringImpl(mltext); return BUILTIN; }
"nil?"  {         mllval.s = new StringImpl(mltext); return BUILTIN; }
{IDENT} {         mllval.s = new StringImpl(mltext);
                  return VARIABLE; }

{DIGITS} {        mllval.s = new StringImpl(mltext);
                  return CONSTANT; }





\"      string_buf_ptr = string_buf; BEGIN(vstr1);

<vstr1>\"         { /* saw closing quote - all done */
                  BEGIN(INITIAL);
                  *string_buf_ptr = '\0';
                  mllval.s = new StringImpl(string_buf);
                  return CONSTANT;
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
for (char *c = mltext; *c; c++)
  if ('\n' == *c)
    mllex_lineno++;
return SPACE; }
. { message("Unrecognised string: \"%s\"\n",mltext); return -1;
if (0); yyunput(0,0); /* hide warning about yyunput not being used */  }

%%

