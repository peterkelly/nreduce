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
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <math.h>

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

  free(sn->qname.prefix);
  free(sn->qname.localpart);
  free(sn->mode.prefix);
  free(sn->mode.localpart);
  if (NULL != sn->seqtype)
    df_seqtype_deref(sn->seqtype);
  free(sn->strval);
  free(sn->deffilename);
  free(sn->name);
  free(sn->ns);

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

int xl_snode_resolve(xl_snode *first, xs_schema *s, const char *filename, error_info *ei)
{
  xl_snode *sn;
  for (sn = first; sn; sn = sn->next) {
/*     printf("xl_snode_resolve: %s\n",xslt_element_names[sn->type]); */
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

    if (sn->next)
      sn->next->prev = sn;
  }
  return 0;
}

xl_snode *xl_snode_resolve_var(xl_snode *from, qname_t varname)
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
          if (!strcmp(p->qname.localpart,varname.localpart))
            return p;
      }

    }
    if (sn)
      sn = sn->prev;
    if (!sn)
      break;
/*     printf("xl_snode_resolve_var: checking %s\n",xslt_element_names[sn->type]); */
    /* FIXME: comparison needs to be namespace aware (keep in mind prefix mappings could differ) */
    if ((XSLT_VARIABLE == sn->type) && !strcmp(sn->qname.localpart,varname.localpart))
      return sn;
  }

  return NULL;
}

void xp_expr_resolve_var(xp_expr *from, qname_t varname, xp_expr **defexpr, xl_snode **defnode)
{
  xp_expr *e;

  *defexpr = NULL;
  *defnode = NULL;

/*   printf("xp_expr_resolve_var: varname \"%s\"\n",varname.localpart); */
/*   printf("xp_expr_resolve_var: from = %s\n",xp_expr_types[from->type]); */
  /* First try to find a declaration of the variable in the XPath expression tree */
  for (e = from->parent; e; e = e->parent) {
/*     printf("xp_expr_resolve_var: checking parent %s\n",xp_expr_types[from->type]); */
    if (XPATH_EXPR_FOR == e->type) {
      xp_expr *ve = e->conditional;
/*         printf("xp_expr_resolve_var: for var: %s\n",ve->qname.localpart); */
        /* FIXME: comparison needs to be namespace aware (keep in mind prefix mappings
           could differ) */
        if (!strcmp(varname.localpart,ve->qname.localpart)) {
/*           printf("xp_expr_resolve_var: found it!\n"); */
          *defexpr = ve;
          return;
        }
    }
  }

  /* If not found, look in the XSLT syntax tree */
/*   printf("xp_expr_resolve_var: looking in XSLT tree\n"); */
  if (NULL == (*defnode = xl_snode_resolve_var(from->stmt,varname))) {
/*     printf("xp_expr_resolve_var: not found\n"); */
  }
  else {
/*     printf("xp_expr_resolve_var: found - XSLT def\n"); */
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
  int i;
  for (i = 0; i < indent; i++)
    printf("  ");
  printf("%s\n",xslt_element_names[sn->type]);

  if (sn->select)
    xp_expr_print_tree(sn->select,indent+1);
  if (sn->expr1)
    xp_expr_print_tree(sn->expr1,indent+1);
  if (sn->expr2)
    xp_expr_print_tree(sn->expr2,indent+1);
  if (sn->name_expr)
    xp_expr_print_tree(sn->name_expr,indent+1);

  if (sn->param) {
    for (i = 0; i < indent+1; i++)
      printf("  ");
    printf("[param]\n");
    xl_snode_print_tree(sn->param,indent+1);
  }
  if (sn->sort) {
    for (i = 0; i < indent+1; i++)
      printf("  ");
    printf("[osrt]\n");
    xl_snode_print_tree(sn->sort,indent+1);
  }
  if (sn->child)
    xl_snode_print_tree(sn->child,indent+1);
  if (sn->next)
    xl_snode_print_tree(sn->next,indent);
}
