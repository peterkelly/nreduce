%option noyywrap
%option outfile="lexer.yy.c"
%x vstr1 vstr2
%{
/*
 * This file is part of the GridXSLT project
 * Copyright (C) 2005 Peter Kelly (pmk@cs.adelaide.edu.au)
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

/*

Note: this will probably have to be rewritten... see "Building a Tokenizer for XPath or XQuery"
(http://www.w3.org/TR/2005/WD-xquery-xpath-parsing-20050404/) for details.






NCNAME
LITERAL
NUMBER

Node: function names cannot include the node types




FIXME: add support for comments (: ... :)
*/


#include "Statement.h"
#include "util/String.h"
#include "grammar.tab.h"
#include <string.h>
#include <stdlib.h>

using namespace GridXSLT;

#define MAX_STR_CONST 1024

int lex_lineno;
char string_buf[MAX_STR_CONST];
char *string_buf_ptr;

/* FIXME: check length of string */
/* FIXME: support single-quoted strings */

/* FIXME: treat * as multiply where necessary, as described in 3.7 */
/* FIXME: other rules in 3.7 */


%}

DIGIT      [0-9]
DIGITS     {DIGIT}+
LETTER     [a-zA-Z]


INTEGER {DIGITS}
DECIMAL (\.{DIGITS}|{DIGITS}\.[0-9]*)
DOUBLE  (\.{DIGITS}|{DIGITS}(\.[0-9]*)?[eE][+-]?{DIGITS})

NCNAME ([a-zA-Z0-9]|_)([a-zA-Z0-9]|\.|-|_)*


%%

\"      string_buf_ptr = string_buf; BEGIN(vstr1);

<vstr1>\"         { /* saw closing quote - all done */
                  BEGIN(INITIAL);
                  *string_buf_ptr = '\0';
                  yylval.string = strdup(string_buf);
                  return STRING_LITERAL;
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




\'      string_buf_ptr = string_buf; BEGIN(vstr2);

<vstr2>\'         { /* saw closing quote - all done */
                  BEGIN(INITIAL);
                  *string_buf_ptr = '\0';
                  yylval.string = strdup(string_buf);
                  return STRING_LITERAL;
                }

<vstr2>\\n        *string_buf_ptr++ = '\n';
<vstr2>\\t        *string_buf_ptr++ = '\t';
<vstr2>\\r        *string_buf_ptr++ = '\r';
<vstr2>\\b        *string_buf_ptr++ = '\b';
<vstr2>\\f        *string_buf_ptr++ = '\f';

<vstr2>\\(.|\n)   *string_buf_ptr++ = yytext[1];

<vstr2>[^\\\n\']+ {
                  char *yptr = yytext;

                  while ( *yptr )
                    *string_buf_ptr++ = *yptr++;
                }










"$"             { return '$'; }
"("             { return '('; }
")"             { return ')'; }
"*"             { return '*'; }
"+"             { return '+'; }
","             { return ','; }
"-"             { return '-'; }
"."             { return '.'; }
"/"             { return '/'; }
"<"             { return '<'; }
"="             { return '='; }
">"             { return '>'; }
"@"             { return '@'; }
"["             { return '['; }
"]"             { return ']'; }
"{"             { return '{'; }
"}"             { return '}'; }
"|"             { return '|'; }
"%"             { return '%'; }
"#"             { return '#'; }
";"             { return ';'; }
":"             { return ':'; }
"?"             { return '?'; }
".."            { return DOTDOT; }
"//"            { return SLASHSLASH; }
"::"            { return COLONCOLON; }
"and"           { return AND; }
"comment"	      { return COMMENT; }
"div"           { return DIV; }
"udiv"          { return IDIV; }
"mod"           { return MOD; }
"node"          { return NODE; }
"or"            { return OR; }
"processing-instruction" { return PROCESSING_INSTRUCTION; }
"insert-processing-instruction" { return INSERT_PROCESSING_INSTRUCTION; }
"text"          { return TEXT; }

"ancestor"      { return ANCESTOR; }
"ancestor-or-self" { return ANCESTOR_OR_SELF; }
"attribute"     { return ATTRIBUTE; }
"child"         { return CHILD; }
"descendant"    { return DESCENDANT; }
"descendant-or-self" { return DESCENDANT_OR_SELF; }
"following"     { return FOLLOWING; }
"following-sibling" { return FOLLOWING_SIBLING; }
"namespace"     { return NAMESPACE; }
"parent"        { return PARENT; }
"preceding"     { return PRECEDING; }
"preceding-sibling" { return PRECEDING_SIBLING; }
"self"          { return SELF; }



"function"      { return FUNCTION; }
"var"           { return VAR; }
"for-each"      { return FOR_EACH; }
"choose"        { return CHOOSE; }
"when"          { return WHEN; }
"otherwise"     { return OTHERWISE; }
"call"          { return CALL; }
"void"          { return VOID; }
"nooverride"      { return NOOVERRIDE; }
"required"      { return REQUIRED; }
"tunnel"        { return TUNNEL; }
"transform"     { return TRANSFORM; }
"if"            { return IF; }
"for-each-group" { return FOR_EACH_GROUP; }
"by"            { return BY; }
"adjacent"      { return ADJACENT; }
"starting-with" { return STARTING_WITH; }
"ending-with"   { return ENDING_WITH; }
"message"       { return MESSAGE; }
"terminate"     { return TERMINATE; }
"fallback"      { return FALLBACK; }
"apply-templates" { return APPLY_TEMPLATES; }
"mode"          { return MODE; }
"to"            { return TO; }
"call-template" { return CALL_TEMPLATE; }
"apply-imports" { return APPLY_IMPORTS; }
"next-match"    { return NEXT_MATCH; }
"analyze-string" { return ANALYZE_STRING; }
"matching-substring" { return MATCHING_SUBSTRING; }
"non-matching-substring" { return NON_MATCHING_SUBSTRING; }
"sort"          { return SORT; }
"perform-sort"  { return PERFORM_SORT; }
"value-of"      { return VALUE_OF; }
"separator"     { return SEPARATOR; }
"copy"          { return COPY; }
"copy-of"       { return COPY_OF; }
"sequence"      { return SEQUENCE; }
"group"         { return GROUP; }
"template"      { return TEMPLATE; }
"match"         { return MATCH; }
"__just_expr"   { return JUST_EXPR; }
"__just_seqtype" { return JUST_SEQTYPE; }
"__just_pattern" { return JUST_PATTERN; }
"return"        { return RETURN; }
"for"           { return FOR; }
"in"            { return IN; }
"some"          { return SOME; }
"every"         { return EVERY; }
"satisfies"     { return SATISFIES; }
"then"          { return THEN; }
"else"          { return ELSE; }
"union"         { return UNION; }
"intersect"     { return INTERSECT; }
"except"        { return EXCEPT; }
"instance"      { return INSTANCE; }
"of"            { return OF; }
"treat"         { return TREAT; }
"as"            { return AS; }
"castable"      { return CASTABLE; }
"cast"          { return CAST; }
"eq"            { return VALUE_EQ; }
"ne"            { return VALUE_NE; }
"lt"            { return VALUE_LT; }
"le"            { return VALUE_LE; }
"gt"            { return VALUE_GT; }
"ge"            { return VALUE_GE; }
"!="            { return SYMBOL_NE; }
"<="            { return SYMBOL_LE; }
">="            { return SYMBOL_GE; }
"is"            { return IS; }
"<<"            { return NODE_PRECEDES; }
">>"            { return NODE_FOLLOWS; }
"document-node" { return DOCUMENT_NODE; }
"schema-attribute" { return SCHEMA_ATTRIBUTE; }
"schema-element" { return SCHEMA_ELEMENT; }
"item"          { return ITEM; }
"element"       { return ELEMENT; }
"multiply"      { return MULTIPLY; }
"id"            { return ID1; }
"key"           { return KEY; }












{INTEGER} { yylval.inumber = atoi(yytext); return INTEGER_LITERAL; }
{DECIMAL} { yylval.number = atof(yytext); return DECIMAL_LITERAL; }
{DOUBLE} { yylval.number = atof(yytext); return DOUBLE_LITERAL; }
{NCNAME} { yylval.string = strdup(yytext); return NCNAME; }

" "
"\t"
"\n" { lex_lineno++; }
. { message("Unrecognised string: \"%s\"\n",yytext); return -1;
if (0); yyunput(0,0); /* hide warning about yyunput not being used */  }



%%
