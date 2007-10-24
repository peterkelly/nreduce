%option noyywrap
%option outfile="lexer.yy.c"
%x string1 string2 avt
%{
/*
 * This file is part of the nreduce project
 * Copyright (C) 2005-2007 Peter Kelly (pmk@cs.adelaide.edu.au)
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
 * $Id: lexer.l 194 2005-11-08 14:23:42Z pmkelly $
 *
 */

/*
Note: this will probably have to be rewritten... see "Building a Tokenizer for XPath or XQuery"
(http://www.w3.org/TR/2005/WD-xquery-xpath-parsing-20050404/) for details.

FIXME: add support for comments (: ... :)
FIXME: check length of string
FIXME: support single-quoted strings
FIXME: treat * as multiply where necessary, as described in 3.7
FIXME: other rules in 3.7
*/

#include "cxslt.h"
#include "grammar.tab.h"
#include <string.h>
#include <stdlib.h>

#define MAX_STR_CONST 1024

int lex_lineno;
char string_buf[MAX_STR_CONST];
char *string_buf_ptr;

%}

DIGIT      [0-9]
DIGITS     {DIGIT}+
LETTER     [a-zA-Z]
INTEGER {DIGITS}
DECIMAL (\.{DIGITS}|{DIGITS}\.[0-9]*)
DOUBLE  (\.{DIGITS}|{DIGITS}(\.[0-9]*)?[eE][+-]?{DIGITS})
NCNAME ([a-zA-Z]|_)([a-zA-Z0-9]|\.|-|_)*

%%

\}                       { string_buf_ptr = string_buf; BEGIN(avt); }
<avt>\{                  { BEGIN(INITIAL);
                           *string_buf_ptr = '\0';
                           if (0 < strlen(string_buf)) {
                             xlval.string = strdup(string_buf);
                             return AVT_LITERAL;
                           }
                           else {
                             return AVT_EMPTY;
                           } }
<avt>"{{"                { *string_buf_ptr++ = '{'; }
<avt>"}}"                { *string_buf_ptr++ = '}'; }
<avt>.                   { *string_buf_ptr++ = xtext[0]; }

\"                       { string_buf_ptr = string_buf; BEGIN(string1); }
<string1>\"              { /* saw closing quote - all done */
                           BEGIN(INITIAL);
                           *string_buf_ptr = '\0';
                           xlval.string = strdup(string_buf);
                           return STRING_LITERAL; }
<string1>\\n             { *string_buf_ptr++ = '\n'; }
<string1>\\t             { *string_buf_ptr++ = '\t'; }
<string1>\\r             { *string_buf_ptr++ = '\r'; }
<string1>\\b             { *string_buf_ptr++ = '\b'; }
<string1>\\f             { *string_buf_ptr++ = '\f'; }
<string1>\\(.|\n)        { *string_buf_ptr++ = xtext[1]; }
<string1>[^\\\n\"]+      { char *yptr = xtext;
                           while ( *yptr )
                             *string_buf_ptr++ = *yptr++; }

\'                       { string_buf_ptr = string_buf; BEGIN(string2); }
<string2>\'              { /* saw closing quote - all done */
                           BEGIN(INITIAL);
                           *string_buf_ptr = '\0';
                           xlval.string = strdup(string_buf);
                           return STRING_LITERAL; }
<string2>\\n             { *string_buf_ptr++ = '\n'; }
<string2>\\t             { *string_buf_ptr++ = '\t'; }
<string2>\\r             { *string_buf_ptr++ = '\r'; }
<string2>\\b             { *string_buf_ptr++ = '\b'; }
<string2>\\f             { *string_buf_ptr++ = '\f'; }
<string2>\\(.|\n)        { *string_buf_ptr++ = xtext[1]; }
<string2>[^\\\n\']+      { char *yptr = xtext;
                           while ( *yptr )
                             *string_buf_ptr++ = *yptr++; }

#E:{DIGIT}?:[a-zA-Z]?      { xlval.string = strdup(yytext+1); return DSVAR_EXPRESSION; }
#F:{DIGIT}?:[a-zA-Z]?      { xlval.string = strdup(yytext+1); return DSVAR_FORWARD_AXIS; }
#R:{DIGIT}?:[a-zA-Z]?      { xlval.string = strdup(yytext+1); return DSVAR_REVERSE_AXIS; }
#N:{DIGIT}?:[a-zA-Z]?      { xlval.string = strdup(yytext+1); return DSVAR_NUMERIC; }
#S:{DIGIT}?:[a-zA-Z]?      { xlval.string = strdup(yytext+1); return DSVAR_STRING; }
#Q:{DIGIT}?:[a-zA-Z]?      { xlval.string = strdup(yytext+1); return DSVAR_QNAME; }
#T:{DIGIT}?:[a-zA-Z]?      { xlval.string = strdup(yytext+1); return DSVAR_NODETEST; }
#axis:                   { return DSTYPE_AXIS; }
#nodetest:               { return DSTYPE_NODETEST; }
"$"                      { return '$'; }
"("                      { return '('; }
")"                      { return ')'; }
"*"                      { return '*'; }
"+"                      { return '+'; }
","                      { return ','; }
"-"                      { return '-'; }
"."                      { return '.'; }
"/"                      { return '/'; }
"<"                      { return '<'; }
"="                      { return '='; }
">"                      { return '>'; }
"@"                      { return '@'; }
"["                      { return '['; }
"]"                      { return ']'; }
"|"                      { return '|'; }
"%"                      { return '%'; }
";"                      { return ';'; }
":"                      { return ':'; }
"?"                      { return '?'; }
".."                     { return DOTDOT; }
"//"                     { return SLASHSLASH; }
"::"                     { return COLONCOLON; }
"and"                    { return AND; }
"comment"	         { return COMMENT; }
"div"                    { return DIV; }
"idiv"                   { return IDIV; }
"mod"                    { return MOD; }
"node"                   { return NODE; }
"or"                     { return OR; }
"processing-instruction" { return PROCESSING_INSTRUCTION; }
"text"                   { return TEXT; }

"ancestor"               { return ANCESTOR; }
"ancestor-or-self"       { return ANCESTOR_OR_SELF; }
"attribute"              { return ATTRIBUTE; }
"child"                  { return CHILD; }
"descendant"             { return DESCENDANT; }
"descendant-or-self"     { return DESCENDANT_OR_SELF; }
"following"              { return FOLLOWING; }
"following-sibling"      { return FOLLOWING_SIBLING; }
"namespace"              { return NAMESPACE; }
"parent"                 { return PARENT; }
"preceding"              { return PRECEDING; }
"preceding-sibling"      { return PRECEDING_SIBLING; }
"self"                   { return SELF; }

"if"                     { return IF; }
"to"                     { return TO; }
"return"                 { return RETURN; }
"for"                    { return FOR; }
"in"                     { return IN; }
"some"                   { return SOME; }
"every"                  { return EVERY; }
"satisfies"              { return SATISFIES; }
"then"                   { return THEN; }
"else"                   { return ELSE; }
"union"                  { return UNION; }
"intersect"              { return INTERSECT; }
"except"                 { return EXCEPT; }
"instance"               { return INSTANCE; }
"of"                     { return OF; }
"treat"                  { return TREAT; }
"as"                     { return AS; }
"castable"               { return CASTABLE; }
"cast"                   { return CAST; }
"eq"                     { return VALUE_EQ; }
"ne"                     { return VALUE_NE; }
"lt"                     { return VALUE_LT; }
"le"                     { return VALUE_LE; }
"gt"                     { return VALUE_GT; }
"ge"                     { return VALUE_GE; }
"!="                     { return SYMBOL_NE; }
"<="                     { return SYMBOL_LE; }
">="                     { return SYMBOL_GE; }
"is"                     { return IS; }
"<<"                     { return NODE_PRECEDES; }
">>"                     { return NODE_FOLLOWS; }
"document-node"          { return DOCUMENT_NODE; }
"schema-attribute"       { return SCHEMA_ATTRIBUTE; }
"schema-element"         { return SCHEMA_ELEMENT; }
"item"                   { return ITEM; }
"element"                { return ELEMENT; }
"multiply"               { return MULTIPLY; }
"id"                     { return ID1; }
"key"                    { return KEY; }
"void"                   { return VOID; }

{INTEGER}                { xlval.number = atof(xtext); return NUMERIC_LITERAL; }
{DECIMAL}                { xlval.number = atof(xtext); return NUMERIC_LITERAL; }
{DOUBLE}                 { xlval.number = atof(xtext); return NUMERIC_LITERAL; }
{NCNAME}                 { xlval.string = strdup(xtext); return NCNAME; }

" "
"\t"
"\n"                     { lex_lineno++; }
.                        { fprintf(stderr,"Unrecognised string: \"%s\"\n",xtext);
                           return -1;
                           if (0) /* hide warning about yyunput not being used */ 
                             yyunput(0,0); }

%%
