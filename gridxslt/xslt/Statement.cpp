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

#include "Statement.h"
#include "parse.h"
#include "dataflow/serialization.h"
#include "util/macros.h"
#include "util/debug.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <math.h>

using namespace GridXSLT;

List<String> get_reserved_namespaces()
{
  List<String> lst;
  lst.append(XSLT_NAMESPACE);
  lst.append(FN_NAMESPACE);
  lst.append(XML_NAMESPACE);
  lst.append(XS_NAMESPACE);
  lst.append(XDT_NAMESPACE);
  lst.append(XSI_NAMESPACE);
  return lst;
}

static List<String> reserved_namespaces = get_reserved_namespaces();

extern FILE *yyin;
extern int lex_lineno;
int yyparse();

typedef struct yy_buffer_state *YY_BUFFER_STATE;
YY_BUFFER_STATE yy_scan_string(const char *str);
void yy_switch_to_buffer(YY_BUFFER_STATE new_buffer);
void yy_delete_buffer(YY_BUFFER_STATE buffer);
int yylex_destroy(void);

Statement *parse_tree = NULL;
Expression *parse_expr = NULL;
SequenceTypeImpl *parse_SequenceTypeImpl = NULL;
int parse_firstline = 0;
char *parse_errstr = NULL;
Error *parse_ei = NULL;
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

Statement::Statement(int type)
  : m_type(type),
    m_flags(0),
    m_parent(NULL),
    m_prev(NULL),
    m_child(NULL),
    m_next(NULL),
    m_param(NULL),
    m_sort(NULL),
    m_select(NULL),
    m_expr1(NULL),
    m_expr2(NULL),
    m_name_expr(NULL),
    m_namespaces(new NamespaceMap()),
    m_gmethod(0),
    m_seroptions(NULL),
    m_importpred(0),
    m_includens(0),
    m_literal(0),
    m_uri(NULL),
    m_templateno(0),
    m_tmpl(NULL),
    m_outp(NULL),
    m_templates(NULL)
{
  m_sloc.uri = NULL;
  m_sloc.line = 0;
}

Statement::~Statement()
{
  if (m_child)
    delete m_child;
  if (m_next)
    delete m_next;
  if (m_param)
    delete m_param;
  if (m_sort)
    delete m_sort;

  if (m_select)
    delete m_select;
  if (m_expr1)
    delete m_expr1;
  if (m_expr2)
    delete m_expr2;
  if (m_name_expr)
    delete m_name_expr;

  if (NULL != m_seroptions) {
    int i;
    for (i = 0; i < SEROPTION_COUNT; i++)
      free(m_seroptions[i]);
    free(m_seroptions);
  }

  sourceloc_free(m_sloc);
  free(m_uri);

  NamespaceMap_free(m_namespaces,0);
}

void Statement::printTree(int indent)
{
  message("%p [%d] %#i%s %*\n",this,m_importpred,2*indent,xslt_element_names[m_type],&m_ident);

  if (m_select)
    m_select->printTree(indent+1);
  if (m_expr1)
    m_expr1->printTree(indent+1);
  if (m_expr2)
    m_expr2->printTree(indent+1);
  if (m_name_expr)
    m_name_expr->printTree(indent+1);

  if (m_param) {
    message("%#i[param]\n",2*(indent+1));
    m_param->printTree(indent+1);
  }
  if (m_sort) {
    message("%#i[sort]\n",2*(indent+1));
    m_sort->printTree(indent+1);
  }
  if (m_child)
    m_child->printTree(indent+1);
  if (m_next)
    m_next->printTree(indent);
}

void Statement_set_parent(Statement *first, Statement *parent, int *importpred)
{
  Statement *sn;
  for (sn = first; sn; sn = sn->m_next) {
    if (NULL != parent)
      sn->m_namespaces->parent = parent->m_namespaces;

    if (sn->m_child) {
      Statement_set_parent(sn->m_child,sn,importpred);
      if (XSLT_IMPORT == sn->m_type)
        (*importpred)++;
    }
    if (sn->m_param)
      Statement_set_parent(sn->m_param,sn,importpred);
    if (sn->m_sort)
      Statement_set_parent(sn->m_sort,sn,importpred);
    if (sn->m_select)
      sn->m_select->setParent(NULL,sn);
    if (sn->m_expr1)
      sn->m_expr1->setParent(NULL,sn);
    if (sn->m_expr2)
      sn->m_expr2->setParent(NULL,sn);
    if (sn->m_name_expr)
      sn->m_name_expr->setParent(NULL,sn);

    sn->m_parent = parent;

    if (sn->m_next)
      sn->m_next->m_prev = sn;
  }

  for (sn = first; sn; sn = sn->m_next)
    sn->m_importpred = *importpred;
}

static int check_duplicate_params(Statement *sn, Error *ei)
{
  Statement *p1;
  Statement *p2;

  /* @implements(xslt20:parameters-4)
     status { partial }
     test { xslt/parse/function_paramdup1.test }
     test { xslt/parse/function_paramdup2.test }
     test { xslt/parse/function_paramdup3.test }
     test { xslt/parse/function_paramdup4.test }
     issue { need to test with templates as well (not just functions) - also stylsheets? }
     @end */

  for (p1 = sn->m_param; p1; p1 = p1->m_next) {
    for (p2 = p1->m_next; p2; p2 = p2->m_next) {
      if (p1->m_ident == p2->m_ident) {
        return error(ei,p2->m_sloc.uri,p2->m_sloc.line,"XTSE0580",
                     "Duplicate parameter: %*",&p1->m_ident);
      }
    }
  }

  return 0;
}

int GridXSLT::Statement_resolve(Statement *first, Schema *s, const char *filename, Error *ei)
{
  Statement *sn;

  for (sn = first; sn; sn = sn->m_next) {

    ASSERT(sn->m_ident.m_name.isNull());
    ASSERT(sn->m_ident.m_ns.isNull());

    /* @implements(xslt20:qname-11)
       status { partial }
       issue { need to test this for other types of objects }
       test { xslt/parse/output_badnameprefix.test }
       @end */
    if (!sn->m_qn.isNull()) {
      sn->m_ident = qname_to_nsname(sn->m_namespaces,sn->m_qn);
      if (sn->m_ident.isNull())
        return error(ei,sn->m_sloc.uri,sn->m_sloc.line,"XTSE0280",
                     "Could not resolve namespace for prefix \"%*\"",&sn->m_qn.m_prefix);
    }

    if ((XSLT_DECL_FUNCTION == sn->m_type) && (!sn->m_ident.m_ns.isNull())) {
      /* @implements(xslt20:stylesheet-functions-5)
         test { xslt/parse/function_name4.test }
         test { xslt/parse/function_name5.test }
         test { xslt/parse/function_name6.test }
         test { xslt/parse/function_name7.test }
         test { xslt/parse/function_name8.test }
         test { xslt/parse/function_name9.test }
         @end */
      for (Iterator<String> it = reserved_namespaces; it.haveCurrent(); it++) {
        if ((sn->m_ident.m_ns == *it))
          return error(ei,sn->m_sloc.uri,sn->m_sloc.line,"XTSE0740",
                       "A function cannot have a prefix that refers to a reserved namespace");
      }
    }

    if (sn->m_child)
      CHECK_CALL(Statement_resolve(sn->m_child,s,filename,ei))
    if (sn->m_param)
      CHECK_CALL(Statement_resolve(sn->m_param,s,filename,ei))
    if (sn->m_sort)
      CHECK_CALL(Statement_resolve(sn->m_sort,s,filename,ei))
    if (sn->m_select)
      CHECK_CALL(sn->m_select->resolve(s,filename,ei))
    if (sn->m_expr1)
      CHECK_CALL(sn->m_expr1->resolve(s,filename,ei))
    if (sn->m_expr2)
      CHECK_CALL(sn->m_expr2->resolve(s,filename,ei))
    if (sn->m_name_expr)
      CHECK_CALL(sn->m_name_expr->resolve(s,filename,ei))

    if (!sn->m_st.isNull())
      CHECK_CALL(sn->m_st.resolve(sn->m_namespaces,s,sn->m_sloc.uri,sn->m_sloc.line,ei))

    if (XSLT_DECL_FUNCTION == sn->m_type)
      CHECK_CALL(check_duplicate_params(sn,ei))

    if (sn->m_next)
      sn->m_next->m_prev = sn;
  }

  return 0;
}

Statement *GridXSLT::Statement_resolve_var(Statement *from, const QName &varname)
{
  /* FIXME: support for namespaces (note that variables with different prefixes could
     potentially have the same namespace href */
  Statement *sn = from;

  /* Note, we only look for variable definitions _prior_ to the current node (in document order).
     XSLT only allows variables to be referenced from nodes _following_ the declarations. */
  while (1) {
    while (sn && !sn->m_prev) {
      sn = sn->m_parent;

      if (sn && (XSLT_DECL_FUNCTION == sn->m_type)) {
        Statement *p;
        for (p = sn->m_param; p; p = p->m_next)
          if (p->m_qn.m_localPart == varname.m_localPart)
            return p;
      }

    }
    if (sn)
      sn = sn->m_prev;
    if (!sn)
      break;
/*     message("Statement_resolve_var: checking %s\n",xslt_element_names[sn->m_type]); */
    /* FIXME: comparison needs to be namespace aware (keep in mind prefix mappings could differ) */
    if ((XSLT_VARIABLE == sn->m_type) && (sn->m_qn.m_localPart == varname.m_localPart))
      return sn;
  }

  return NULL;
}

void GridXSLT::Expression_resolve_var(Expression *from, const QName &varname,
                                      Expression **defexpr, Statement **defnode)
{
  Expression *e;

  *defexpr = NULL;
  *defnode = NULL;

/*   debugl("Expression_resolve_var: varname \"%s\"",varname.localpart); */
/*   debugl("Expression_resolve_var: from = %s",Expression_types[from->type]); */
  /* First try to find a declaration of the variable in the XPath expression tree */
  for (e = from->m_parent; e; e = e->m_parent) {
/*     debugl("Expression_resolve_var: checking parent %s",Expression_types[from->type]); */
    if (XPATH_EXPR_FOR == e->m_type) {
      Expression *ve = e->m_conditional;
/*         debugl("Expression_resolve_var: for var: %s",ve->qn.localpart); */
        /* FIXME: comparison needs to be namespace aware (keep in mind prefix mappings
           could differ) */
        if (varname.m_localPart == ve->m_qn.m_localPart) {
/*           debugl("Expression_resolve_var: found it!"); */
          *defexpr = ve;
          return;
        }
    }
  }

  /* If not found, look in the XSLT syntax tree */
/*   debugl("Expression_resolve_var: looking in XSLT tree"); */
  if (NULL == (*defnode = Statement_resolve_var(from->m_stmt,varname))) {
/*     debugl("Expression_resolve_var: not found"); */
  }
  else {
/*     debugl("Expression_resolve_var: found - XSLT def"); */
  }
  return;
}

int GridXSLT::parse_xl_syntax(const String &str, const char *filename, int baseline, Error *ei,
                    Expression **expr, Statement **sn, SequenceTypeImpl **st)
{
  YY_BUFFER_STATE bufstate;
  int r;
  parse_expr = NULL;
  parse_SequenceTypeImpl = NULL;
  parse_tree = NULL;
  parse_ei = ei;
  parse_filename = filename;
  parse_firstline = baseline;
  lex_lineno = 0;

  char *cstr = str.cstring();
  bufstate = yy_scan_string(cstr);
  free(cstr);
  yy_switch_to_buffer(bufstate);

  r = yyparse();

  yy_delete_buffer(bufstate);
  yylex_destroy();

  if (NULL != expr)
    *expr = parse_expr;
  if (NULL != sn)
    *sn = parse_tree;
  if (NULL != st)
    *st = parse_SequenceTypeImpl;

  parse_expr = NULL;
  parse_SequenceTypeImpl = NULL;
  parse_tree = NULL;
  parse_ei = NULL;
  parse_filename = NULL;
  parse_firstline = baseline;
  lex_lineno = 0;

  return r;
}

Statement *GridXSLT::Statement_parse(const char *str, const char *filename, int baseline, Error *ei)
{
  Statement *sn = NULL;
  if (0 != parse_xl_syntax(str,filename,baseline,ei,NULL,&sn,NULL))
    return NULL;
  ASSERT(NULL != sn);
  return sn;
}

Statement *GridXSLT::xl_first_decl(Statement *root)
{
  return root->m_child;
}

Statement *GridXSLT::xl_next_decl(Statement *sn)
{
  if ((NULL != sn->m_child) &&
      (XSLT_DECL_FUNCTION != sn->m_type) &&
      (XSLT_DECL_TEMPLATE != sn->m_type))
    return sn->m_child;

  while ((NULL != sn) && (NULL == sn->m_next))
    sn = sn->m_parent;

  if (NULL != sn)
    sn = sn->m_next;

  return sn;
}

static int ends_with(const char *filename, const char *ext)
{
  return ((strlen(ext) <= strlen(filename)) &&
          (!strcasecmp(filename+strlen(filename)-strlen(ext),ext)));
}


typedef struct output_defstr output_defstr;

class output_defstr {
  DISABLE_COPY(output_defstr)
public:
  output_defstr() : unpoptions(NULL), defby(NULL), options(NULL) { }
  ~output_defstr();
  NSName ident;
  NSName method;
  char **unpoptions;
  Statement **defby;
  df_seroptions *options;
};

output_defstr::~output_defstr()
{
  int i;
  for (i = 0; i < SEROPTION_COUNT; i++)
    free(unpoptions[i]);
  free(unpoptions);
  free(defby);
}

static output_defstr *get_output_defstr(xslt_source *source, List<output_defstr*> &deflist,
                                        const NSName &ident)
{
  output_defstr *od;
  for (Iterator<output_defstr*> it = deflist; it.haveCurrent(); it++) {
    od = *it;
    if (od->ident == ident)
      return od;
  }
  od = new output_defstr();
  od->ident = ident;
  od->unpoptions = (char**)calloc(SEROPTION_COUNT,sizeof(char*));
  od->defby = (Statement**)calloc(SEROPTION_COUNT,sizeof(Statement*));
  deflist.append(od);
  return od;
}

int xslt_build_output_defs(Error *ei, xslt_source *source)
{
  Statement *sn;
  ManagedPtrList<output_defstr*> deflist;

  /* @implements(xslt20:serialization-9)
     status { partial }
     issue { need more tests }
     test { xslt/parse/output_merge1.test }
     @end */

  /* We always have a default, unnamed output definition */
  get_output_defstr(source,deflist,NSName::null());

  /* First figure out the applicable method for each output definition; this affects the defaults
     that are used */
  for (sn = xl_first_decl(source->root); sn; sn = xl_next_decl(sn)) {
    if (XSLT_DECL_OUTPUT == sn->m_type) {
      output_defstr *od = get_output_defstr(source,deflist,sn->m_ident);

      if (NULL != sn->m_seroptions[SEROPTION_METHOD]) {
        QName qn = QName::parse(sn->m_seroptions[SEROPTION_METHOD]);
        od->method = qname_to_nsname(sn->m_namespaces,qn);

        /* @implements(xslt20:serialization-15)
           test { xslt/parse/output_badmethodprefix.test }
           test { xslt/parse/output_badmethod.test }
           @end */
        if (od->method.isNull())
          return error(ei,sn->m_sloc.uri,sn->m_sloc.line,"XTSE1570",
                       "Could not resolve namespace for prefix \"%*\"",&qn.m_prefix);

        if ((od->method.m_ns.isNull()) &&
            (od->method.m_name != "xml") &&
            (od->method.m_name != "html") &&
            (od->method.m_name != "xhtml") &&
            (od->method.m_name != "text"))
          return error(ei,sn->m_sloc.uri,sn->m_sloc.line,"XTSE1570","If the method has a null "
                       "namespace, it must be one of xml, html, xhtml, or text");
      }
    }
  }

  /* Now create the parsed options object for each output def */
  Iterator<output_defstr*> it;
  for (it = deflist; it.haveCurrent(); it++) {
    output_defstr *od = *it;
    od->options = new df_seroptions(od->method);
    od->options->m_ident = od->ident;
    list_append(&source->output_defs,od->options);
  }

  /* Parse the remaining options */
  for (sn = xl_first_decl(source->root); sn; sn = xl_next_decl(sn)) {
    if (XSLT_DECL_OUTPUT == sn->m_type) {
      output_defstr *od = get_output_defstr(source,deflist,sn->m_ident);
      int i;
      for (i = 0; i < SEROPTION_COUNT; i++) {
        /* FIXME: take into account import precedence */
        if (NULL != sn->m_seroptions[i]) {

          if ((SEROPTION_CDATA_SECTION_ELEMENTS == i) ||
              (SEROPTION_USE_CHARACTER_MAPS == i)) {

            /* These two options are lists of nsnames; multiple output declarations add to the
               list instead of replacing the existing value. This is handled by
               parseOption(); here we just have to avoid checking for conflicting values. */
          }
          else if (NULL == od->unpoptions[i]) {
            od->unpoptions[i] = strdup(sn->m_seroptions[i]);
            od->defby[i] = sn;
          }
          else if (strcmp(od->unpoptions[i],sn->m_seroptions[i])) {
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
            if (od->defby[i]->m_importpred != sn->m_importpred) {
              /* use value from declaration with higher import precedence; this should always
                 be sn since we are traverse importees before importers */
              ASSERT(od->defby[i]->m_importpred < sn->m_importpred);
              free(od->unpoptions[i]);
              od->unpoptions[i] = strdup(sn->m_seroptions[i]);
              od->defby[i] = sn;
            }
            else {
              return error(ei,sn->m_sloc.uri,sn->m_sloc.line,"XTSE1560",
                           "Conflicting value for option %s: "
                           "previously set to %s by another output declaration",
                            df_seroptions::seroption_names[i],od->unpoptions[i]);
            }
          }

          CHECK_CALL(od->options->parseOption(i,ei,sn->m_sloc.uri,sn->m_sloc.line,
                                              sn->m_seroptions[i],sn->m_namespaces))
        }
      }
    }
  }

  return 0;
}

int xslt_build_space_decls(Error *ei, xslt_source *source)
{
  Statement *sn;

  for (sn = xl_first_decl(source->root); sn; sn = xl_next_decl(sn)) {
    if ((XSLT_DECL_STRIP_SPACE == sn->m_type) || (XSLT_DECL_PRESERVE_SPACE == sn->m_type)) {
      int preserve = (XSLT_DECL_PRESERVE_SPACE == sn->m_type);
      for (Iterator<QNameTest*> it = sn->m_QNameTests; it.haveCurrent(); it++) {
        QNameTest *qt = *it;
        NSNameTest *nt = QNameTest_to_NSNameTest(sn->m_namespaces,qt);
        space_decl *decl;
        if (NULL == nt)
          return error(ei,sn->m_sloc.uri,sn->m_sloc.line,"XTSE0280",
                       "Could not resolve namespace for prefix \"%*\"",&qt->qn.m_prefix);
        decl = (space_decl*)calloc(1,sizeof(space_decl));
        decl->nnt = nt;
        decl->importpred = sn->m_importpred;
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

static int xslt_post_process(Error *ei, xslt_source *source, const char *uri)
{
  int importpred = 1;
  /* Note: the order of these matters! Some later processing steps rely on earlier ones */

  Statement_set_parent(source->root,NULL,&importpred);

  CHECK_CALL(Statement_resolve(source->root,source->schema,uri,ei))
  CHECK_CALL(xslt_build_output_defs(ei,source))
  CHECK_CALL(xslt_build_space_decls(ei,source))
  return 0;
}

int GridXSLT::xslt_parse(Error *ei, const char *uri, xslt_source **source)
{
  *source = (xslt_source*)calloc(1,sizeof(xslt_source));
  (*source)->schema = new Schema(xs_g);

  /* XSLiTe syntax */
  if (ends_with(uri,".xl")) {
    stringbuf *input;
    char buf[1024];
    int r;
    FILE *f;

    if (NULL == (f = fopen(uri,"r"))) {
      error(ei,uri,-1,String::null(),"%s",strerror(errno));
      xslt_source_free(*source);
      *source = NULL;
      return -1;
    }

    input = stringbuf_new();
    while (0 < (r = fread(buf,1,1024,f)))
      stringbuf_append(input,buf,r);

    fclose(f);

    if (NULL == ((*source)->root = Statement_parse(input->data,uri,0,ei))) {
      stringbuf_free(input);
      xslt_source_free(*source);
      *source = NULL;
      return -1;
    }

    /* add default namespaces declared in schema */
    Iterator<ns_def*> nsit;
    for (nsit = xs_g->namespaces->defs; nsit.haveCurrent(); nsit++) {
      ns_def *ns = *nsit;
      (*source)->root->m_namespaces->add_direct(ns->href,ns->prefix);
    }

    /* temp - functions and operators namespace */
    (*source)->root->m_namespaces->add_direct(FN_NAMESPACE,"fn");

    stringbuf_free(input);
  }
  /* Standard XSLT syntax */
  else {
    char *realuri = get_real_uri(uri);
    (*source)->root = new Statement(XSLT_TRANSFORM);
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

void GridXSLT::xslt_source_free(xslt_source *source)
{
  list *l;
  for (l = source->output_defs; l; l = l->next)
    delete static_cast<df_seroptions*>(l->data);
  list_free(source->output_defs,NULL);
  list_free(source->space_decls,(list_d_t)space_decl_free);
  if (NULL != source->root)
    delete source->root;
  delete source->schema;
  free(source);
}

df_seroptions *GridXSLT::xslt_get_output_def(xslt_source *source, const NSName &ident)
{
  list *l;
  for (l = source->output_defs; l; l = l->next) {
    df_seroptions *options = (df_seroptions*)l->data;
    if (options->m_ident == ident)
      return options;
  }
  return NULL;
}

