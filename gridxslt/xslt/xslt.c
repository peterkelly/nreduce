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
#include "parse.h"
#include "dataflow/serialization.h"
#include "util/macros.h"
#include "util/debug.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
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
seqtype *parse_seqtype = NULL;
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
  if (NULL != sn->st)
    seqtype_deref(sn->st);
  free(sn->strval);

  if (NULL != sn->seroptions) {
    int i;
    for (i = 0; i < SEROPTION_COUNT; i++)
      free(sn->seroptions[i]);
    free(sn->seroptions);
  }
  list_free(sn->qnametests,(void*)qnametest_ptr_free);

  sourceloc_free(sn->sloc);
  nsname_free(sn->ident);
  free(sn->uri);

  ns_map_free(sn->namespaces,0);
  free(sn);
}

void xl_snode_set_parent(xl_snode *first, xl_snode *parent, int *importpred)
{
  xl_snode *sn;
  for (sn = first; sn; sn = sn->next) {
    if (NULL != parent)
      sn->namespaces->parent = parent->namespaces;

    if (sn->child) {
      xl_snode_set_parent(sn->child,sn,importpred);
      if (XSLT_IMPORT == sn->type)
        (*importpred)++;
    }
    if (sn->param)
      xl_snode_set_parent(sn->param,sn,importpred);
    if (sn->sort)
      xl_snode_set_parent(sn->sort,sn,importpred);
    if (sn->select)
      xp_set_parent(sn->select,NULL,sn);
    if (sn->expr1)
      xp_set_parent(sn->expr1,NULL,sn);
    if (sn->expr2)
      xp_set_parent(sn->expr2,NULL,sn);
    if (sn->name_expr)
      xp_set_parent(sn->name_expr,NULL,sn);

    sn->parent = parent;

    if (sn->next)
      sn->next->prev = sn;
  }

  for (sn = first; sn; sn = sn->next)
    sn->importpred = *importpred;
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
        return error(ei,p2->sloc.uri,p2->sloc.line,"XTSE0580",
                     "Duplicate parameter: %#n",p1->ident);
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

    /* @implements(xslt20:qname-11)
       status { partial }
       issue { need to test this for other types of objects }
       test { xslt/parse/output_badnameprefix.test }
       @end */
    if (!qname_isnull(sn->qn)) {
      sn->ident = qname_to_nsname(sn->namespaces,sn->qn);
      if (nsname_isnull(sn->ident))
        return error(ei,sn->sloc.uri,sn->sloc.line,"XTSE0280",
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
          return error(ei,sn->sloc.uri,sn->sloc.line,"XTSE0740",
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

    if (NULL != sn->st)
      CHECK_CALL(seqtype_resolve(sn->st,sn->namespaces,s,sn->sloc.uri,sn->sloc.line,ei))

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
                    xp_expr **expr, xl_snode **sn, seqtype **st)
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

xl_snode *xl_snode_parse(const char *str, const char *filename, int baseline, error_info *ei)
{
  xl_snode *sn = NULL;
  if (0 != parse_xl_syntax(str,filename,baseline,ei,NULL,&sn,NULL))
    return NULL;
  assert(NULL != sn);
  return sn;
}

void xl_snode_print_tree(xl_snode *sn, int indent)
{
  print("%p [%d] %#i%s %#n\n",sn,sn->importpred,2*indent,xslt_element_names[sn->type],sn->ident);

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

xl_snode *xl_first_decl(xl_snode *root)
{
  return root->child;
}

xl_snode *xl_next_decl(xl_snode *sn)
{
  if ((NULL != sn->child) &&
      (XSLT_DECL_FUNCTION != sn->type) &&
      (XSLT_DECL_TEMPLATE != sn->type))
    return sn->child;

  while ((NULL != sn) && (NULL == sn->next))
    sn = sn->parent;

  if (NULL != sn)
    sn = sn->next;

  return sn;
}

static int ends_with(const char *filename, const char *ext)
{
  return ((strlen(ext) <= strlen(filename)) &&
          (!strcasecmp(filename+strlen(filename)-strlen(ext),ext)));
}


typedef struct output_defstr output_defstr;

struct output_defstr {
  nsname ident;
  nsname method;
  char **unpoptions;
  xl_snode **defby;
  df_seroptions *options;
};

static output_defstr *get_output_defstr(xslt_source *source, list **deflist, nsname ident)
{
  list *l;
  output_defstr *od;
  for (l = *deflist; l; l = l->next) {
    od = (output_defstr*)l->data;
    if (nsname_equals(od->ident,ident))
      return od;
  }
  od = (output_defstr*)calloc(1,sizeof(output_defstr));
  od->ident = nsname_copy(ident);
  od->unpoptions = (char**)calloc(SEROPTION_COUNT,sizeof(char*));
  od->defby = (xl_snode**)calloc(SEROPTION_COUNT,sizeof(xl_snode*));
  list_append(deflist,od);
  return od;
}

static void output_defstr_free(output_defstr *od)
{
  int i;
  nsname_free(od->ident);
  nsname_free(od->method);
  for (i = 0; i < SEROPTION_COUNT; i++)
    free(od->unpoptions[i]);
  free(od->unpoptions);
  free(od->defby);
  free(od);
}

int xslt_build_output_defs(error_info *ei, xslt_source *source)
{
  xl_snode *sn;
  list *deflist = NULL;
  list *l;

  /* @implements(xslt20:serialization-9)
     status { partial }
     issue { need more tests }
     test { xslt/parse/output_merge1.test }
     @end */

  /* We always have a default, unnamed output definition */
  get_output_defstr(source,&deflist,nsname_temp(NULL,NULL));

  /* First figure out the applicable method for each output definition; this affects the defaults
     that are used */
  for (sn = xl_first_decl(source->root); sn; sn = xl_next_decl(sn)) {
    if (XSLT_DECL_OUTPUT == sn->type) {
      output_defstr *od = get_output_defstr(source,&deflist,sn->ident);

      if (NULL != sn->seroptions[SEROPTION_METHOD]) {
        qname qn = qname_parse(sn->seroptions[SEROPTION_METHOD]);
        od->method = qname_to_nsname(sn->namespaces,qn);

        /* @implements(xslt20:serialization-15)
           test { xslt/parse/output_badmethodprefix.test }
           test { xslt/parse/output_badmethod.test }
           @end */
        if (nsname_isnull(od->method)) {
          error(ei,sn->sloc.uri,sn->sloc.line,"XTSE1570",
                "Could not resolve namespace for prefix \"%s\"",qn.prefix);
          qname_free(qn);
          list_free(deflist,(void*)output_defstr_free);
          return -1;
        }
        qname_free(qn);

        if ((NULL == od->method.ns) &&
            strcmp(od->method.name,"xml") &&
            strcmp(od->method.name,"html") &&
            strcmp(od->method.name,"xhtml") &&
            strcmp(od->method.name,"text")) {
          error(ei,sn->sloc.uri,sn->sloc.line,"XTSE1570",
                "If the method has a null namespace, it must be one of xml, html, xhtml, or text");
          list_free(deflist,(void*)output_defstr_free);
          return -1;
        }
      }
    }
  }

  /* Now create the parsed options object for each output def */
  for (l = deflist; l; l = l->next) {
    output_defstr *od = (output_defstr*)l->data;
    od->options = df_seroptions_new(od->method);
    od->options->ident = nsname_copy(od->ident);
    list_append(&source->output_defs,od->options);
  }

  /* Parse the remaining options */
  for (sn = xl_first_decl(source->root); sn; sn = xl_next_decl(sn)) {
    if (XSLT_DECL_OUTPUT == sn->type) {
      output_defstr *od = get_output_defstr(source,&deflist,sn->ident);
      int i;
      for (i = 0; i < SEROPTION_COUNT; i++) {
        /* FIXME: take into account import precedence */
        if (NULL != sn->seroptions[i]) {

          if ((SEROPTION_CDATA_SECTION_ELEMENTS == i) ||
              (SEROPTION_USE_CHARACTER_MAPS == i)) {

            /* These two options are lists of nsnames; multiple output declarations add to the
               list instead of replacing the existing value. This is handled by
               df_parse_seroption(); here we just have to avoid checking for conflicting values. */
          }
          else if (NULL == od->unpoptions[i]) {
            od->unpoptions[i] = strdup(sn->seroptions[i]);
            od->defby[i] = sn;
          }
          else if (strcmp(od->unpoptions[i],sn->seroptions[i])) {
            /* @implements(xslt20:serialization-10)
               test { xslt/parse/output_conflict1.test }
               test { xslt/parse/output_conflict2.test }
               test { xslt/parse/output_conflict3.test }
               test { xslt/parse/output_conflict4.test }
               test { xslt/parse/output_conflict5.test }
               test { xslt/parse/output_conflict6.test }
               test { xslt/parse/output_conflict7.test }
               test { xslt/parse/output_conflict8.test }
               @end */
            if (od->defby[i]->importpred != sn->importpred) {
              /* use value from declaration with higher import precedence; this should always
                 be sn since we are traverse importees before importers */
              assert(od->defby[i]->importpred < sn->importpred);
              free(od->unpoptions[i]);
              od->unpoptions[i] = strdup(sn->seroptions[i]);
              od->defby[i] = sn;
            }
            else {
              error(ei,sn->sloc.uri,sn->sloc.line,"XTSE1560","Conflicting value for option %s: "
                    "previously set to %s by another output declaration",
                    seroption_names[i],od->unpoptions[i]);
              list_free(deflist,(void*)output_defstr_free);
              return -1;
            }
          }

          if (0 != df_parse_seroption(od->options,i,ei,sn->sloc.uri,sn->sloc.line,
                                      sn->seroptions[i],sn->namespaces)) {
            list_free(deflist,(void*)output_defstr_free);
            return -1;
          }

        }
      }
    }
  }

  for (l = deflist; l; l = l->next) {
/*     output_defstr *od = (output_defstr*)l->data; */
  }

  list_free(deflist,(void*)output_defstr_free);
  return 0;
}

int xslt_build_space_decls(error_info *ei, xslt_source *source)
{
  xl_snode *sn;

  for (sn = xl_first_decl(source->root); sn; sn = xl_next_decl(sn)) {
    if ((XSLT_DECL_STRIP_SPACE == sn->type) || (XSLT_DECL_PRESERVE_SPACE == sn->type)) {
      int preserve = (XSLT_DECL_PRESERVE_SPACE == sn->type);
      list *l;
      for (l = sn->qnametests; l; l = l->next) {
        qnametest *qt = (qnametest*)l->data;
        nsnametest *nt = qnametest_to_nsnametest(sn->namespaces,qt);
        space_decl *decl;
        if (NULL == nt)
          return error(ei,sn->sloc.uri,sn->sloc.line,"XTSE0280",
                       "Could not resolve namespace for prefix \"%s\"",qt->qn.prefix);
        decl = (space_decl*)calloc(1,sizeof(space_decl));
        decl->nnt = nt;
        decl->importpred = sn->importpred;
        if (!nt->wcns && !nt->wcname)
          decl->priority = 0;
        else if (nt->wcns && nt->wcname)
          decl->priority = -0.5;
        else
          decl->priority = -0.25;
        decl->preserve = preserve;
        list_append(&source->space_decls,decl);
      }
    }
  }


  return 0;
}

static int xslt_post_process(error_info *ei, xslt_source *source, const char *uri)
{
  int importpred = 1;
  /* Note: the order of these matters! Some later processing steps rely on earlier ones */

  xl_snode_set_parent(source->root,NULL,&importpred);

  CHECK_CALL(xl_snode_resolve(source->root,source->schema,uri,ei))
  CHECK_CALL(xslt_build_output_defs(ei,source))
  CHECK_CALL(xslt_build_space_decls(ei,source))
  return 0;
}

int xslt_parse(error_info *ei, const char *uri, xslt_source **source)
{
  *source = (xslt_source*)calloc(1,sizeof(xslt_source));
  (*source)->schema = xs_schema_new(xs_g);

  /* XSLiTe syntax */
  if (ends_with(uri,".xl")) {
    stringbuf *input;
    char buf[1024];
    int r;
    list *l;
    FILE *f;

    if (NULL == (f = fopen(uri,"r"))) {
      error(ei,uri,-1,NULL,"%s",strerror(errno));
      xslt_source_free(*source);
      *source = NULL;
      return -1;
    }

    input = stringbuf_new();
    while (0 < (r = fread(buf,1,1024,f)))
      stringbuf_append(input,buf,r);

    fclose(f);

    if (NULL == ((*source)->root = xl_snode_parse(input->data,uri,0,ei))) {
      stringbuf_free(input);
      xslt_source_free(*source);
      *source = NULL;
      return -1;
    }

    /* add default namespaces declared in schema */
    for (l = xs_g->namespaces->defs; l; l = l->next) {
      ns_def *ns = (ns_def*)l->data;
      ns_add_direct((*source)->root->namespaces,ns->href,ns->prefix);
    }

    /* temp - functions and operators namespace */
    ns_add_direct((*source)->root->namespaces,FN_NAMESPACE,"fn");

    stringbuf_free(input);
  }
  /* Standard XSLT syntax */
  else {
    char *realuri = get_real_uri(uri);
    (*source)->root = xl_snode_new(XSLT_TRANSFORM);
    if (0 != parse_xslt_relative_uri(ei,NULL,0,NULL,realuri,realuri,(*source)->root)) {
      free(realuri);
      xslt_source_free(*source);
      *source = NULL;
      return -1;
    }
    free(realuri);
  }

  if (0 != xslt_post_process(ei,*source,uri)) {
    xslt_source_free(*source);
    *source = NULL;
    return -1;
  }

  return 0;
}

void xslt_source_free(xslt_source *source)
{
  list_free(source->output_defs,(void*)df_seroptions_free);
  list_free(source->space_decls,(void*)space_decl_free);
  if (NULL != source->root)
    xl_snode_free(source->root);
  xs_schema_free(source->schema);
  free(source);
}

df_seroptions *xslt_get_output_def(xslt_source *source, nsname ident)
{
  list *l;
  for (l = source->output_defs; l; l = l->next) {
    df_seroptions *options = (df_seroptions*)l->data;
    if (nsname_equals(options->ident,ident))
      return options;
  }
  return NULL;
}

