%option noyywrap
%option outfile="lexer.yy.c"
%x vstr1 comment comment2 intline
%{

/* FIXME: Since / is in ident now it causes parse errors for comments in some cases...
   try //x at the start of a line */

#include "grammar.tab.h"
#include "nreduce.h"
#include <string.h>

#define MAX_STR_CONST 1024 /* FIXME: allow bigger strings! */

char string_buf[MAX_STR_CONST];
char *string_buf_ptr;

char *yyfilename = NULL;
int yylineno;

%}

DIGIT      [0-9]
DIGITS     {DIGIT}+
LETTER     [a-zA-Z]


XINTEGER {DIGITS}
XDOUBLE {DIGITS}\.{DIGITS}
IDCHAR [\+\-\*\/\%\<\>\=\&\|\[\]\,\:\?a-zA-Z_]
IDENT {IDCHAR}({IDCHAR}|{DIGIT}|_)*

 /* IDENT ([\+a-zA-Z]|_)([\+a-zA-Z0-9]|_)* */

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
"!="    {         yylval.s = strdup(yytext);
                  return SYMBOL; }
{IDENT} {         yylval.s = strdup(yytext);
                  return SYMBOL; }

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

