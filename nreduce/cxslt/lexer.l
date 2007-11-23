%option noyywrap
%option outfile="lexer.yy.c"
%option stack
%x STRING1 STRING2 AVT OPERATOR VARNAME
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
int x_first_token = 1;
extern xmlNodePtr parse_node;
extern elcgen *parse_gen;

%}

DIGIT      [0-9]
DIGITS     {DIGIT}+
LETTER     [a-zA-Z]
INTEGER {DIGITS}
DECIMAL (\.{DIGITS}|{DIGITS}\.[0-9]*)
DOUBLE  (\.{DIGITS}|{DIGITS}(\.[0-9]*)?[eE][+-]?{DIGITS})
NCNAME ([a-zA-Z]|_)([a-zA-Z0-9]|\.|-|_)*
QNAME ({NCNAME}|{NCNAME}:{NCNAME})
WS         [ \t\r\n]*

%%

%{
  if (x_first_token) {
    BEGIN(INITIAL);
    x_first_token = 0;
  }
%}


<INITIAL,OPERATOR>\}               { BEGIN(AVT);
                                     string_buf_ptr = string_buf; }
<AVT>{
  \{                               { BEGIN(INITIAL);
                                     *string_buf_ptr = '\0';
                                     if (0 < strlen(string_buf)) {
                                       xlval.string = strdup(string_buf);
                                       return AVT_LITERAL;
                                     }
                                     else {
                                       return AVT_EMPTY;
                                     }
                                   }
  "{{"                             { *string_buf_ptr++ = '{'; }
  "}}"                             { *string_buf_ptr++ = '}'; }
  .                                { *string_buf_ptr++ = xtext[0]; }
}

<INITIAL>\"                        { BEGIN(OPERATOR);
                                     yy_push_state(STRING1);
                                     string_buf_ptr = string_buf; }
<OPERATOR>\"                       { yy_push_state(STRING1);
                                     string_buf_ptr = string_buf; }
<STRING1>{
  \"                               { yy_pop_state();
                                     *string_buf_ptr = '\0';
                                     xlval.string = strdup(string_buf);
                                     return STRING_LITERAL; }
  \\n                              { *string_buf_ptr++ = '\n'; }
  \\t                              { *string_buf_ptr++ = '\t'; }
  \\r                              { *string_buf_ptr++ = '\r'; }
  \\b                              { *string_buf_ptr++ = '\b'; }
  \\f                              { *string_buf_ptr++ = '\f'; }
  \\(.|\n)                         { *string_buf_ptr++ = xtext[1]; }
  [^\\\n\"]+                       { char *yptr = xtext;
                                     while ( *yptr )
                                       *string_buf_ptr++ = *yptr++; }
}

<INITIAL>\'                        { BEGIN(OPERATOR);
                                     yy_push_state(STRING2);
                                     string_buf_ptr = string_buf; }
<OPERATOR>\'                       { yy_push_state(STRING2);
                                     string_buf_ptr = string_buf; }
<STRING2>{
  \'                               { yy_pop_state();
                                     *string_buf_ptr = '\0';
                                     xlval.string = strdup(string_buf);
                                     return STRING_LITERAL; }
  \\n                              { *string_buf_ptr++ = '\n'; }
  \\t                              { *string_buf_ptr++ = '\t'; }
  \\r                              { *string_buf_ptr++ = '\r'; }
  \\b                              { *string_buf_ptr++ = '\b'; }
  \\f                              { *string_buf_ptr++ = '\f'; }
  \\(.|\n)                         { *string_buf_ptr++ = xtext[1]; }
  [^\\\n\']+                       { char *yptr = xtext;
                                     while ( *yptr )
                                       *string_buf_ptr++ = *yptr++; }
}

<OPERATOR>{
  "then"                           { BEGIN(INITIAL); return THEN; }
  "else"                           { BEGIN(INITIAL); return ELSE; }
  "and"                            { BEGIN(INITIAL); return AND; }
  ","                              { BEGIN(INITIAL); return ','; }
  "div"                            { BEGIN(INITIAL); return DIV; }
  "="                              { BEGIN(INITIAL); return '='; }
  "except"                         { BEGIN(INITIAL); return EXCEPT; }
  "eq"                             { BEGIN(INITIAL); return VALUE_EQ; }
  "ge"                             { BEGIN(INITIAL); return VALUE_GE; }
  "gt"                             { BEGIN(INITIAL); return VALUE_GT; }
  "le"                             { BEGIN(INITIAL); return VALUE_LE; }
  "lt"                             { BEGIN(INITIAL); return VALUE_LT; }
  "ne"                             { BEGIN(INITIAL); return VALUE_NE; }
  ">="                             { BEGIN(INITIAL); return SYMBOL_GE; }
  ">>"                             { BEGIN(INITIAL); return NODE_FOLLOWS; }
  ">"                              { BEGIN(INITIAL); return '>'; }
  "idiv"                           { BEGIN(INITIAL); return IDIV; }
  "intersect"                      { BEGIN(INITIAL); return INTERSECT; }
  "in"                             { BEGIN(INITIAL); return IN; }
  "is"                             { BEGIN(INITIAL); return IS; }
  "["                              { BEGIN(INITIAL); return '['; }
  "<="                             { BEGIN(INITIAL); return SYMBOL_LE; }
  "<<"                             { BEGIN(INITIAL); return NODE_PRECEDES; }
  "<"                              { BEGIN(INITIAL); return '<'; }
  "-"                              { BEGIN(INITIAL); return '-'; }
  "mod"                            { BEGIN(INITIAL); return MOD; }
  "*"                              { BEGIN(INITIAL); return '*'; }
  "!="                             { BEGIN(INITIAL); return SYMBOL_NE; }
  "or"                             { BEGIN(INITIAL); return OR; }
  "+"                              { BEGIN(INITIAL); return '+'; }
  "return"                         { BEGIN(INITIAL); return RETURN; }
  "satisfies"                      { BEGIN(INITIAL); return SATISFIES; }
  "//"                             { BEGIN(INITIAL); return SLASHSLASH; }
  "/"                              { BEGIN(INITIAL); return '/'; }
  "to"                             { BEGIN(INITIAL); return TO; }
  "union"                          { BEGIN(INITIAL); return UNION; }
  "|"                              { BEGIN(INITIAL); return '|'; }

  "castable"/{WS}"as"              { return CASTABLE; }
  "cast"/{WS}"as"                  { return CAST; }
  "instance"/{WS}"of"              { return INSTANCE; }
  "treat"/{WS}"as"                 { return TREAT; }
  "of"                             { return OF; }
  "as"                             { return AS; }

  "$"                              { BEGIN(VARNAME); return '$'; }
  "for"/{WS}"$"                    { BEGIN(VARNAME); return FOR; }

  ")"                              { return ')'; }
  "?"                              { return '?'; }
  "]"                              { return ']'; }
}

#E:{DIGIT}?:[a-zA-Z]?              { xlval.string = strdup(yytext+1); return DSVAR_EXPRESSION; }
#F:{DIGIT}?:[a-zA-Z]?              { xlval.string = strdup(yytext+1); return DSVAR_FORWARD_AXIS; }
#R:{DIGIT}?:[a-zA-Z]?              { xlval.string = strdup(yytext+1); return DSVAR_REVERSE_AXIS; }
#N:{DIGIT}?:[a-zA-Z]?              { xlval.string = strdup(yytext+1); return DSVAR_NUMERIC; }
#S:{DIGIT}?:[a-zA-Z]?              { xlval.string = strdup(yytext+1); return DSVAR_STRING; }
#Q:{DIGIT}?:[a-zA-Z]?              { xlval.string = strdup(yytext+1); return DSVAR_QNAME; }
#T:{DIGIT}?:[a-zA-Z]?              { xlval.string = strdup(yytext+1); return DSVAR_NODETEST; }
#axis:                             { return DSTYPE_AXIS; }
#nodetest:                         { return DSTYPE_NODETEST; }
"]"                                { return ']'; }
"%"                                { return '%'; }
";"                                { return ';'; }
":"                                { return ':'; }
"?"                                { return '?'; }
"::"                               { return COLONCOLON; }

","                                { return ','; }
"("                                { return '('; }
"if"/{WS}"("                       { return IF; }
"-"                                { return '-'; }
"+"                                { return '+'; }
"//"                               { return SLASHSLASH; }
"/"                                { return '/'; }
"ancestor-or-self"/{WS}"::"        { return ANCESTOR_OR_SELF; }
"ancestor"/{WS}"::"                { return ANCESTOR; }
"attribute"/{WS}"::"               { return ATTRIBUTE; }
"child"/{WS}"::"                   { return CHILD; }
"descendant-or-self"/{WS}"::"      { return DESCENDANT_OR_SELF; }
"descendant"/{WS}"::"              { return DESCENDANT; }
"following-sibling"/{WS}"::"       { return FOLLOWING_SIBLING; }
"following"/{WS}"::"               { return FOLLOWING; }
"namespace"/{WS}"::"               { return NAMESPACE; }
"parent"/{WS}"::"                  { return PARENT; }
"preceding-sibling"/{WS}"::"       { return PRECEDING_SIBLING; }
"preceding"/{WS}"::"               { return PRECEDING; }
"self"/{WS}"::"                    { return SELF; }
"@"                                { return '@'; }

"$"                                { BEGIN(VARNAME); return '$'; }
"for"/{WS}"$"                      { BEGIN(VARNAME); return FOR; }
"some"/{WS}"$"                     { BEGIN(VARNAME); return SOME; }
"every"/{WS}"$"                    { BEGIN(VARNAME); return EVERY; }

"item"/{WS}"("                     { return ITEM; }
"element"/{WS}"("                  { return ELEMENT; }
"attribute"/{WS}"("                { return ATTRIBUTE; }
"schema-element"/{WS}"("           { return SCHEMA_ELEMENT; }
"schema-attribute"/{WS}"("         { return SCHEMA_ATTRIBUTE; }
"comment"/{WS}"("                  { return COMMENT; }
"text"/{WS}"("                     { return TEXT; }
"node"/{WS}"("                     { return NODE; }
"document-node"/{WS}"("            { return DOCUMENT_NODE; }
"processing-instruction"/{WS}"("   { return PROCESSING_INSTRUCTION; }

".."                               { BEGIN(OPERATOR); return DOTDOT; }
"."                                { BEGIN(OPERATOR); return '.'; }
({INTEGER}|{DECIMAL}|{DOUBLE})     { BEGIN(OPERATOR);
                                     xlval.number = atof(xtext);
                                     return NUMERIC_LITERAL; }
{QNAME}/{WS}"("                    { xlval.qn = string_to_qname(xtext,parse_node);
                                     return QNAME; }
<INITIAL,VARNAME>{QNAME}           { BEGIN(OPERATOR); 
                                     xlval.qn = string_to_qname(xtext,parse_node);
                                     return QNAME; }
")"                                { BEGIN(OPERATOR); return ')'; }
{NCNAME}":*"                       { BEGIN(OPERATOR); 
                                     char *prefix = strdup(xtext);
                                     char *colon = strchr(prefix,':');
                                     *colon = '\0';
                                     xlval.qn.uri = strdup(lookup_nsuri(parse_node,prefix));
                                     xlval.qn.prefix = strdup(prefix);
                                     xlval.qn.localpart = NULL;
                                     free(prefix);
                                     return WILDCARD; }
"*:"{NCNAME}                       { BEGIN(OPERATOR); 
                                     char *colon = strchr(xtext,':');
                                     xlval.qn.uri = NULL;
                                     xlval.qn.prefix = NULL;
                                     xlval.qn.localpart = strdup(colon+1);
                                     return WILDCARD; }
"*"                                { return '*'; }

<INITIAL,OPERATOR>{
  " "
  "\t"
  "\n"                             { lex_lineno++; }
  .                                { gen_error(parse_gen,"Unrecognised string: \"%s\"",xtext);
                                     return -1;
                                     if (0) { /* hide warning about unused functions */ 
                                       yyunput(0,0);
                                       yy_top_state();
                                     }
                                   }
}

%%
