%option noyywrap
%option outfile="lexer.yy.c"
%x string comment comment2 intline
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
 * $Id: lexer.l 603 2007-08-14 02:44:16Z pmkelly $
 *
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "grammar.tab.h"
#include "src/nreduce.h"
#include <string.h>
#include <stdlib.h>

array *lexstring = NULL;
const char *yyfilename = NULL;
int yyfileno;

%}

DIGIT                   [0-9]
DIGITS                  {DIGIT}+
HEXDIGIT                [0-9a-fA-F]
HEXDIGITS               {HEXDIGIT}+

INTEGER                  {DIGITS}
HEX                      0[xX]{HEXDIGIT}+
DOUBLE                   {DIGITS}\.{DIGITS}
IDCHAR                   [a-zA-Z_?:]
IDENT                    {IDCHAR}({IDCHAR}|{DIGIT}|_)*
CHAR                     \'.\'

%%

"/*"                     { BEGIN(comment); }
<comment>[^*\n]*         { }
<comment>"*"+[^*/\n]*    { }
<comment>\n              { yylloc.first_line++; }
<comment>"*"+"/"         { BEGIN(INITIAL); }

"//"                     { BEGIN(comment2); }
<comment2>[^\n]*         { }
<comment2>\n             { yylloc.first_line++;
                           BEGIN(INITIAL); }

"#!"                     { BEGIN(intline); }
<intline>[^\n]*          { }
<intline>\n              { yylloc.first_line++;
                           BEGIN(INITIAL); }

\"                       { BEGIN(string);
                           lexstring = array_new(sizeof(char),0);
                           lexstring->nbytes = 0; }
<string>\"               { char c = '\0';
                           BEGIN(INITIAL);
                           array_append(lexstring,&c,1);
                           yylval.s = strdup(lexstring->data);
                           array_free(lexstring);
                           lexstring = NULL;
                           return STRING; }
<string>\\n              { char c = '\n'; array_append(lexstring,&c,1); }
<string>\\t              { char c = '\t'; array_append(lexstring,&c,1); }
<string>\\r              { char c = '\r'; array_append(lexstring,&c,1); }
<string>\\b              { char c = '\b'; array_append(lexstring,&c,1); }
<string>\\f              { char c = '\f'; array_append(lexstring,&c,1); }

<string>\\(.|\n)         { array_append(lexstring,&yytext[1],1); }
<string>[^\\\n\"]+       { array_append(lexstring,yytext,strlen(yytext)); }

"letrec"                 { return LETREC; }
"in"                     { return IN; }
"lambda"                 { return LAMBDAWORD; }
"!"                      { return LAMBDA; }
"nil"                    { return NIL; }
"import"                 { return IMPORT; }
"."                      { return '.'; }
"("                      { return '('; }
")"                      { return ')'; }
"{"                      { return '{'; }
"}"                      { return '}'; }
","                      { return ','; }
";"                      { return ';'; }

"+"                      { return '+'; }
"-"                      { return '-'; }
"*"                      { return '*'; }
"/"                      { return '/'; }
"%"                      { return '%'; }

"="                      { return '='; }
"=="                     { return EQ; }
"!="                     { return NE; }
"<"                      { return '<'; }
"<="                     { return LE; }
">"                      { return '>'; }
">="                     { return GE; }

"<<"                     { return SHL; }
">>"                     { return SHR; }
"&"                      { return '&'; }
"|"                      { return '|'; }
"^"                      { return '^'; }
"~"                      { return '~'; }

"&&"                     { return CONDAND; }
"||"                     { return CONDOR; }
"?"                      { return '?'; }
":"                      { return ':'; }
"["                      { return '['; }
"]"                      { return ']'; }
"'\\\"'"                 { yylval.d = (double)'\"'; return DOUBLE; }
"'\\\''"                 { yylval.d = (double)'\''; return DOUBLE; }
"'\\n'"                  { yylval.d = (double)'\n'; return DOUBLE; }
"'\\t'"                  { yylval.d = (double)'\t'; return DOUBLE; }
"'\\r'"                  { yylval.d = (double)'\r'; return DOUBLE; }
"'\\b'"                  { yylval.d = (double)'\b'; return DOUBLE; }
"'\\g'"                  { yylval.d = (double)'\f'; return DOUBLE; }
"function"               { return FUNCTION; }
"let"                    { return LET; }
"if"                     { return IF; }
"else"                   { return ELSE; }
"foreach"                { return FOREACH; }
{IDENT}                  { yylval.s = strdup(yytext); return SYMBOL; }
{CHAR}                   { yylval.d = (double)yytext[1]; return DOUBLE; }
{HEX}                    { yylval.i = strtol(yytext,NULL,0); return INTEGER; }
{DOUBLE}                 { yylval.d = atof(yytext); return DOUBLE; }
{INTEGER}                { char *endptr = NULL; yylval.i = strtol(yytext,&endptr,0); 
                           if ('\0' != *endptr)
                             return -1;
                           return INTEGER; }

(" "|\n|\t)+             { char *c;
                           for (c = yytext; *c; c++)
                             if ('\n' == *c)
                               yylloc.first_line++; }
.                        { printf("Unrecognised string: \"%s\"\n",yytext);
                           return -1;

                           // hide warning about yyunput not being used
                           if (0)
                             yyunput(0,0); }

%%
