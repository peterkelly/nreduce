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

#define _XSLT_XSLT_C

#include "xslt.h"
#include "util/macros.h"
#include "util/debug.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <math.h>

static const char *reserved_namespaces[7] = {
  XSLT_NAMESPACE,
  FN_NAMESPACE,
  XML_NAMESPACE,
  XS_NAMESPACE,
  XDT_NAMESPACE,
  XSI_NAMESPACE,
  NULL
};

extern FILE *yyin;
extern int lex_lineno;
int yyparse();

typedef void *YY_BUFFER_STATE;
YY_BUFFER_STATE yy_scan_string(const char *str);
void yy_switch_to_buffer(YY_BUFFER_STATE new_buffer);
void yy_delete_buffer(YY_BUFFER_STATE buffer);

xl_snode *parse_tree = NULL;
xp_expr *parse_expr = NULL;
df_seqtype *parse_seqtype = NULL;
int parse_firstline = 0;
char *parse_errstr = NULL;
error_info *parse_ei = NULL;
const char *parse_filename = NULL;

const char *xslt_element_names[] = {
  "declaration",
  "import",
  "instruction",
  "literal-result-element",
  "matching-substring",
  "non-matching-substring",
  "otherwise",
  "output-character",
  "param",
  "sort",
  "transform",
  "variable",
  "when",
  "with-param",

  "attribute-set",
  "character-map",
  "decimal-format",
  "function",
  "import-schema",
  "include",
  "key",
  "namespace-alias",
  "output",
  "preserve-space",
  "strip-space",
  "template",

  "analyze-string",
  "apply-imports",
  "apply-templates",
  "attribute",
  "call-template",
  "choose",
  "comment",
  "copy",
  "copy-of",
  "element",
  "fallback",
  "for-each",
  "for-each-group",
  "if",
  "message",
  "namespace",
  "next-match",
  "number",
  "perform-sort",
  "processing-instruction",
  "result-document",
  "sequence",
  "text",
  "value-of"
};

xl_snode *xl_snode_new(int type)
{
  xl_snode *sn = (xl_snode*)calloc(1,sizeof(xl_snode));
  sn->type = type;
  sn->namespaces = ns_map_new();
  return sn;
}

void xl_snode_free(xl_snode *sn)
{
  if (sn->child)
    xl_snode_free(sn->child);
  if (sn->next)
    xl_snode_free(sn->next);
  if (sn->param)
    xl_snode_free(sn->param);
  if (sn->sort)
    xl_snode_free(sn->sort);

  if (sn->select)
    xp_expr_free(sn->select);
  if (sn->expr1)
    xp_expr_free(sn->expr1);
  if (sn->expr2)
    xp_expr_free(sn->expr2);
  if (sn->name_expr)
    xp_expr_free(sn->name_expr);

  qname_free(sn->qn);
  qname_free(sn->mode);
  if (NULL != sn->seqtype)
    df_seqtype_deref(sn->seqtype);
  free(sn->strval);
  free(sn->deffilename);
  nsname_free(sn->ident);
  free(sn->uri);

  ns_map_free(sn->namespaces,0);
  free(sn);
}

void xl_snode_set_parent(xl_snode *first, xl_snode *parent)
{
  xl_snode *sn;
  for (sn = first; sn; sn = sn->next) {
    sn->parent = parent;
    if (NULL != parent)
      sn->namespaces->parent = parent->namespaces;

    if (sn->child)
      xl_snode_set_parent(sn->child,sn);
    if (sn->param)
      xl_snode_set_parent(sn->param,sn);
    if (sn->sort)
      xl_snode_set_parent(sn->sort,sn);
    if (sn->select)
      xp_set_parent(sn->select,NULL,sn);
    if (sn->expr1)
      xp_set_parent(sn->expr1,NULL,sn);
    if (sn->expr2)
      xp_set_parent(sn->expr2,NULL,sn);
    if (sn->name_expr)
      xp_set_parent(sn->name_expr,NULL,sn);

    if (sn->next)
      sn->next->prev = sn;
  }
}

static int check_duplicate_params(xl_snode *sn, error_info *ei)
{
  xl_snode *p1;
  xl_snode *p2;

  /* @implements(xslt20:parameters-4)
     status { partial }
     test { xslt/parse/function_paramdup1.test }
     test { xslt/parse/function_paramdup2.test }
     test { xslt/parse/function_paramdup3.test }
     test { xslt/parse/function_paramdup4.test }
     issue { need to test with templates as well (not just functions) - also stylsheets? }
     @end */

  for (p1 = sn->param; p1; p1 = p1->next) {
    for (p2 = p1->next; p2; p2 = p2->next) {
      if (nsname_equals(p1->ident,p2->ident)) {
        return error(ei,p2->deffilename,p2->defline,"XTSE0580","Duplicate parameter: %#n",
                     p1->ident);
      }
    }
  }

  return 0;
}

int xl_snode_resolve(xl_snode *first, xs_schema *s, const char *filename, error_info *ei)
{
  xl_snode *sn;

  for (sn = first; sn; sn = sn->next) {

    assert(NULL == sn->ident.name);
    assert(NULL == sn->ident.ns);

    if (!qname_isnull(sn->qn)) {
      sn->ident = qname_to_nsname(sn->namespaces,sn->qn);
      if (nsname_isnull(sn->ident))
        return error(ei,filename,sn->defline,"XTSE0280",
                     "Could not resolve namespace for prefix \"%s\"",sn->qn.prefix);
    }

    if ((XSLT_DECL_FUNCTION == sn->type) && (NULL != sn->ident.ns)) {
      /* @implements(xslt20:stylesheet-functions-5)
         test { xslt/parse/function_name4.test }
         test { xslt/parse/function_name5.test }
         test { xslt/parse/function_name6.test }
         test { xslt/parse/function_name7.test }
         test { xslt/parse/function_name8.test }
         test { xslt/parse/function_name9.test }
         @end */
      const char **rn;
      for (rn = reserved_namespaces; *rn; rn++) {
        if (!strcmp(sn->ident.ns,*rn))
          return error(ei,filename,sn->defline,"XTSE0740",
                       "A function cannot have a prefix that refers to a reserved namespace");
      }
    }

    if (sn->child)
      CHECK_CALL(xl_snode_resolve(sn->child,s,filename,ei))
    if (sn->param)
      CHECK_CALL(xl_snode_resolve(sn->param,s,filename,ei))
    if (sn->sort)
      CHECK_CALL(xl_snode_resolve(sn->sort,s,filename,ei))
    if (sn->select)
      CHECK_CALL(xp_expr_resolve(sn->select,s,filename,ei))
    if (sn->expr1)
      CHECK_CALL(xp_expr_resolve(sn->expr1,s,filename,ei))
    if (sn->expr2)
      CHECK_CALL(xp_expr_resolve(sn->expr2,s,filename,ei))
    if (sn->name_expr)
      CHECK_CALL(xp_expr_resolve(sn->name_expr,s,filename,ei))

    if (NULL != sn->seqtype)
      CHECK_CALL(df_seqtype_resolve(sn->seqtype,sn->namespaces,s,filename,sn->defline,ei))

    if (XSLT_DECL_FUNCTION == sn->type)
      CHECK_CALL(check_duplicate_params(sn,ei))

    if (sn->next)
      sn->next->prev = sn;
  }
  return 0;
}

xl_snode *xl_snode_resolve_var(xl_snode *from, qname varname)
{
  /* FIXME: support for namespaces (note that variables with different prefixes could
     potentially have the same namespace href */
  xl_snode *sn = from;

  /* Note, we only look for variable definitions _prior_ to the current node (in document order).
     XSLT only allows variables to be referenced from nodes _following_ the declarations. */
  while (1) {
    while (sn && !sn->prev) {
      sn = sn->parent;

      if (sn && (XSLT_DECL_FUNCTION == sn->type)) {
        xl_snode *p;
        for (p = sn->param; p; p = p->next)
          if (!strcmp(p->qn.localpart,varname.localpart))
            return p;
      }

    }
    if (sn)
      sn = sn->prev;
    if (!sn)
      break;
/*     printf("xl_snode_resolve_var: checking %s\n",xslt_element_names[sn->type]); */
    /* FIXME: comparison needs to be namespace aware (keep in mind prefix mappings could differ) */
    if ((XSLT_VARIABLE == sn->type) && !strcmp(sn->qn.localpart,varname.localpart))
      return sn;
  }

  return NULL;
}

void xp_expr_resolve_var(xp_expr *from, qname varname, xp_expr **defexpr, xl_snode **defnode)
{
  xp_expr *e;

  *defexpr = NULL;
  *defnode = NULL;

/*   debugl("xp_expr_resolve_var: varname \"%s\"",varname.localpart); */
/*   debugl("xp_expr_resolve_var: from = %s",xp_expr_types[from->type]); */
  /* First try to find a declaration of the variable in the XPath expression tree */
  for (e = from->parent; e; e = e->parent) {
/*     debugl("xp_expr_resolve_var: checking parent %s",xp_expr_types[from->type]); */
    if (XPATH_EXPR_FOR == e->type) {
      xp_expr *ve = e->conditional;
/*         debugl("xp_expr_resolve_var: for var: %s",ve->qn.localpart); */
        /* FIXME: comparison needs to be namespace aware (keep in mind prefix mappings
           could differ) */
        if (!strcmp(varname.localpart,ve->qn.localpart)) {
/*           debugl("xp_expr_resolve_var: found it!"); */
          *defexpr = ve;
          return;
        }
    }
  }

  /* If not found, look in the XSLT syntax tree */
/*   debugl("xp_expr_resolve_var: looking in XSLT tree"); */
  if (NULL == (*defnode = xl_snode_resolve_var(from->stmt,varname))) {
/*     debugl("xp_expr_resolve_var: not found"); */
  }
  else {
/*     debugl("xp_expr_resolve_var: found - XSLT def"); */
  }
  return;
}

int parse_xl_syntax(const char *str, const char *filename, int baseline, error_info *ei,
                    xp_expr **expr, xl_snode **sn, df_seqtype **st)
{
  YY_BUFFER_STATE *bufstate;
  int r;
  parse_expr = NULL;
  parse_seqtype = NULL;
  parse_tree = NULL;
  parse_ei = ei;
  parse_filename = filename;
  parse_firstline = baseline;
  lex_lineno = 0;

  bufstate = yy_scan_string(str);
  yy_switch_to_buffer(bufstate);

  r = yyparse();

  yy_delete_buffer(bufstate);

  if (NULL != expr)
    *expr = parse_expr;
  if (NULL != sn)
    *sn = parse_tree;
  if (NULL != st)
    *st = parse_seqtype;
  parse_expr = NULL;
  parse_seqtype = NULL;
  parse_tree = NULL;
  parse_ei = NULL;
  parse_filename = NULL;
  parse_firstline = baseline;
  lex_lineno = 0;

  return r;
}

xl_snode *xl_snode_parse(char *str, char *filename, int baseline, error_info *ei)
{
  xl_snode *sn = NULL;
  if (0 != parse_xl_syntax(str,filename,baseline,ei,NULL,&sn,NULL))
    return NULL;
  assert(NULL != sn);
  xl_snode_set_parent(sn,NULL);
  return sn;
}

void xl_snode_print_tree(xl_snode *sn, int indent)
{
  print("%p %#i%s %#n\n",sn,2*indent,xslt_element_names[sn->type],sn->ident);

  if (sn->select)
    xp_expr_print_tree(sn->select,indent+1);
  if (sn->expr1)
    xp_expr_print_tree(sn->expr1,indent+1);
  if (sn->expr2)
    xp_expr_print_tree(sn->expr2,indent+1);
  if (sn->name_expr)
    xp_expr_print_tree(sn->name_expr,indent+1);

  if (sn->param) {
    print("%#i[param]\n",2*(indent+1));
    xl_snode_print_tree(sn->param,indent+1);
  }
  if (sn->sort) {
    print("%#i[sort]\n",2*(indent+1));
    xl_snode_print_tree(sn->sort,indent+1);
  }
  if (sn->child)
    xl_snode_print_tree(sn->child,indent+1);
  if (sn->next)
    xl_snode_print_tree(sn->next,indent);
}
