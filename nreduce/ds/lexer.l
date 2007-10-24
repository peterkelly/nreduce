%option noyywrap
%option outfile="lexer.yy.c"
%x string comment comment2 sexpr
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
 * $Id: lexer.l 653 2007-10-02 05:46:20Z pmkelly $
 *
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "grammar.tab.h"
#include <string.h>
#include <stdlib.h>

static char str[16384];
static int strpos = 0;
const char *dsfilename = NULL;
int dsfileno;

%}

DIGIT                   [0-9]
DIGITS                  {DIGIT}+
HEXDIGIT                [0-9a-fA-F]
HEXDIGITS               {HEXDIGIT}+

INTEGER                  {DIGITS}
HEX                      0[xX]{HEXDIGIT}+
DOUBLE                   {DIGITS}\.{DIGITS}
IDCHAR                   [a-zA-Z]
IDENT                    {IDCHAR}({IDCHAR}|{DIGIT}|_)*
CHAR                     \'.\'

%%

"/*"                     { BEGIN(comment); }
<comment>[^*\n]*         { }
<comment>"*"+[^*/\n]*    { }
<comment>\n              { dslloc.first_line++; }
<comment>"*"+"/"         { BEGIN(INITIAL); }

"//"                     { BEGIN(comment2); }
<comment2>[^\n]*         { }
<comment2>\n             { dslloc.first_line++;
                           BEGIN(INITIAL); }

\"                       { BEGIN(string);
                           str[0] = '\0';
                           strpos = 0; }
<string>\"               { BEGIN(INITIAL);
                           str[strpos++] = '\0';
                           dslval.s = strdup(str);
                           return STRING; }
<string>\\n              { str[strpos++] = '\n'; }
<string>\\t              { str[strpos++] = '\t'; }
<string>\\r              { str[strpos++] = '\r'; }
<string>\\b              { str[strpos++] = '\b'; }
<string>\\f              { str[strpos++] = '\f'; }

<string>\\(.|\n)         { str[strpos++] = dstext[1]; }
<string>[^\\\n\"]+       { memcpy(&str[strpos],dstext,strlen(dstext));
                           strpos += strlen(dstext); }

"[["                     { return SQLEFT; }
"]]"                     { return SQRIGHT; }
"="                      { return '='; }
";"                      { return ';'; }
":"                      { return ':'; }
"*"                      { return '*'; }
"@"                      { return '@'; }
"+"                      { return PLUS; }
"++"                     { return PLUSPLUS; }
"+++"                    { return PLUSPLUSPLUS; }

{IDENT}                  { dslval.s = strdup(dstext); return IDENT; }
{INTEGER}                { dslval.i = atoi(dstext); return INTEGER; }

(" "|\n|\t)+             { char *c;
                           for (c = dstext; *c; c++)
                             if ('\n' == *c)
                               dslloc.first_line++; }
.                        { printf("Unrecognised string: \"%s\"\n",dstext);
                           return -1;

                           // hide warning about yyunput not being used
                           if (0)
                             yyunput(0,0); }

%%
