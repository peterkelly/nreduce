/*
 * This file is part of the nreduce project
 * Copyright (C) 2007 Peter Kelly (pmk@cs.adelaide.edu.au)
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

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>
#include <assert.h>
#include <libxml/parser.h>
#include "cxslt.h"

/* FIXME: escape strings when printing (note that xmlChar complicates things) */
/* FIXME: can't use reserved words like eq, text, item for element names in path
   expressions. Need to fix the tokenizer/parser */
/* FIXME: use an autoconf script to set XLEX_DESTROY where appropriate - currently
   it does not get set at all */
/* FIXME: apparently we're supposed to free the strings returned form xmlGetProp() */
/* FIXME: report XPTY0020 if an axis step occurs where the context item is not a node?
          The check may be unnecessarily expensive, and the program will probably throw
          an error anyway... */

static int compile_sequence(elcgen *gen, xsltnode *xfirst);
static int compile_expression(elcgen *gen, xsltnode *xn, expression *expr);
static int compile_num_expression(elcgen *gen, xsltnode *xn, expression *expr);
static int compile_num_sequence(elcgen *gen, xsltnode *xfirst);

int gen_error(elcgen *gen, const char *format, ...)
{
  va_list ap;
  int len;
  char *newerror;
  va_start(ap,format);
  len = vsnprintf(NULL,0,format,ap);
  va_end(ap);

  newerror = (char*)malloc(len+1);
  va_start(ap,format);
  len = vsnprintf(newerror,len+1,format,ap);
  va_end(ap);

  free(gen->error);
  gen->error = newerror;

  return 0;
}

void gen_printf(elcgen *gen, const char *format, ...)
{
  va_list ap;
  va_start(ap,format);
  array_vprintf(gen->buf,format,ap);
  va_end(ap);
}

void gen_iprintf(elcgen *gen, const char *format, ...)
{
  va_list ap;
  int i;
  array_printf(gen->buf,"\n");
  for (i = 0; i < gen->indent; i++)
    array_printf(gen->buf,"  ");
  va_start(ap,format);
  array_vprintf(gen->buf,format,ap);
  va_end(ap);
}

static void gen_printorig(elcgen *gen, const char *name, xsltnode *xn, const char *attr)
{
  gen_iprintf(gen,"/* %s",name);
  if (attr && xmlHasProp(xn->n,attr)) {
    char *expr = xmlGetProp(xn->n,attr);
    char *expr2 = (char*)malloc(2*strlen(expr)+1);
    const char *r;
    char *w = expr2;
    for (r = expr; *r; r++) {
      if ('*' == *r) {
        *(w++) = '*';
        *(w++) = ' ';
      }
      else {
        *(w++) = *r;
      }
    }
    *w = '\0';
    gen_printf(gen," %s",expr2);
    free(expr2);
    free(expr);
  }
  gen_printf(gen," */ ");
}

expression *new_TypeExpr(int type, seqtype *st, expression *right)
{
  fatal("unsupported: TypeExpr");
  return NULL;
}

expression *new_ForExpr(expression *left, expression *right)
{
  fatal("unsupported: ForExpr");
  return NULL;
}

expression *new_VarInExpr(expression *left)
{
  fatal("unsupported: VarInExpr");
  return NULL;
}

expression *new_VarInListExpr(expression *left, expression *right)
{
  fatal("unsupported: VarInListExpr");
  return NULL;
}

expression *new_QuantifiedExpr(int type, expression *left, expression *right)
{
  fatal("unsupported: QuantifiedExpr");
  return NULL;
}

void free_wsarg_ptr(void *a)
{
  wsarg *arg = (wsarg*)a;
  free(arg->uri);
  free(arg->localpart);
  free(arg);
}

static char *escape1(const char *str2)
{
  // FIXME: probably not safe with char
  const char *str = (const char*)str2;
  const char *c;
  char *escaped;
  char *p;
  int len = strlen(str);
  for (c = str; *c; c++)
    if (('\\' == *c) || ('\"' == *c) || ('\n' == *c))
      len++;
  escaped = (char*)malloc(len+1);
  p = escaped;
  for (c = str; *c; c++) {
    if ('\\' == *c) {
      *(p++) = '\\';
      *(p++) = '\\';
    }
    else if ('\"' == *c) {
      *(p++) = '\\';
      *(p++) = '"';
    }
    else if ('\n' == *c) {
      *(p++) = '\\';
      *(p++) = 'n';
    }
    else {
      *(p++) = *c;
    }
  }
  assert(p == escaped+len);
  *p = '\0';
  return (char*)escaped;
}

static int compile_post(elcgen *gen, xsltnode *xn, expression *expr, const char *service_url,
                        qname inelem, qname outelem,
                        list *inargs, list *outargs)
{
  list *l;
  expression *p;
  int r = 1;

  gen_iprintf(gen,"(letrec");
  gen->indent++;
  gen_iprintf(gen,"returns = (xslt::call_ws");

  gen->indent++;

  gen_iprintf(gen,"\"%s\"",service_url);
  gen_iprintf(gen,"\"%s\"",inelem.uri);
  gen_iprintf(gen,"\"%s\"",inelem.localpart);
  gen_iprintf(gen,"\"%s\"",outelem.uri);
  gen_iprintf(gen,"\"%s\"",outelem.localpart);

  l = inargs;
  for (p = expr->left; p && r; p = p->right) {
    wsarg *argname = (wsarg*)l->data;
    assert(XPATH_ACTUAL_PARAM == p->type);

    if (argname->list)
      gen_iprintf(gen,"(append (xslt::ws_arg_list");
    else
      gen_iprintf(gen,"(cons (xslt::ws_arg");
    gen_printf(gen," \"%s\" \"%s\" ",argname->uri,argname->localpart);

    gen->indent++;
    if (!compile_expression(gen,xn,p->left))
      r = 0;
    gen->indent--;
    gen_printf(gen,")");

    l = l->next;
  }
  gen_iprintf(gen,"nil");
  for (p = expr->left; p; p = p->right)
    gen_printf(gen,")");
  gen->indent--;
  gen_printf(gen,")");
  gen->indent--;
  gen_iprintf(gen,"in");

  if (((wsarg*)outargs->data)->simple)
    gen_iprintf(gen,"  (apmap xml::item_children returns))");
  else
    gen_iprintf(gen,"  returns)");
  return r;
}

//#define WSCALL_DEBUG
#ifdef WSCALL_DEBUG
static void wscall_debug(elcgen *gen, expression *expr, const char *service_url,
                        qname inelem, qname outelem,
                        list *inargs, list *outargs)
{
  list *l;
  printf("/*\n");
  printf("service url = %s\n",service_url);
  printf("input element = {%s}%s\n",inelem.uri,inelem.localpart);
  printf("output element = {%s}%s\n",outelem.uri,outelem.localpart);

  printf("inargs =");
  for (l = inargs; l; l = l->next) {
    wsarg *wa = (wsarg*)l->data;
    printf(" {%s}%s (%s)",wa->uri,wa->localpart,wa->list ? "list" : "single");
  }
  printf("\n");

  printf("outargs =");
  for (l = outargs; l; l = l->next) {
    wsarg *wa = (wsarg*)l->data;
    printf(" {%s}%s (%s)",wa->uri,wa->localpart,wa->list ? "list" : "single");
  }
  printf("\n*/\n");
}
#endif

static int compile_ws_call(elcgen *gen, xsltnode *xn, expression *expr, const char *wsdlurl)
{
  char *ident = nsname_to_ident(expr->qn.uri,expr->qn.localpart);
  wsdlfile *wf;
  char *service_url;
  qname inelem;
  qname outelem;
  list *inargs = NULL;
  list *outargs = NULL;
  int supplied;
  int expected;
  expression *p;
  int r = 1;

  if (!process_wsdl(gen,wsdlurl,&wf))
    return 0;

  if (!wsdl_get_url(gen,wf,&service_url))
    return gen_error(gen,"%s: %s",wf->filename,gen->error);

  if (!wsdl_get_operation_messages(gen,wf,expr->qn.localpart,&inelem,&outelem,&inargs,&outargs))
    return gen_error(gen,"%s: %s",wf->filename,gen->error);

#ifdef WSCALL_DEBUG
  wscall_debug(gen,expr,service_url,inelem,outelem,inargs,outargs);;
#endif

  supplied = 0;
  for (p = expr->left; p; p = p->right)
    supplied++;
  expected = list_count(inargs);

  if (supplied != expected)
    r = gen_error(gen,"Incorrect # args for web service call %s: expected %d, got %d",
                  expr->qn.localpart,expected,supplied);
  else if (1 != list_count(outargs))
    r = gen_error(gen,"Multiple return arguments for web service call %s",expr->qn.localpart);
  else
    r = compile_post(gen,xn,expr,service_url,inelem,outelem,inargs,outargs);

  free(ident);
  free(inelem.uri);
  free(inelem.localpart);
  free(outelem.uri);
  free(outelem.localpart);
  list_free(inargs,free_wsarg_ptr);
  list_free(outargs,free_wsarg_ptr);
  return r;
}

static int compile_test(elcgen *gen, xsltnode *xn, expression *expr)
{
  assert((XPATH_KIND_TEST == expr->type) || (XPATH_NAME_TEST == expr->type));
  if (XPATH_KIND_TEST == expr->type) {
    switch (expr->kind) {
    case KIND_DOCUMENT:
      gen_printf(gen,"(xslt::type_test xml::TYPE_DOCUMENT)");
      break;
    case KIND_ELEMENT:
      gen_printf(gen,"(xslt::type_test xml::TYPE_ELEMENT)");
      break;
    case KIND_ATTRIBUTE:
      gen_printf(gen,"(xslt::type_test xml::TYPE_ATTRIBUTE)");
      break;
    case KIND_SCHEMA_ELEMENT:
      return gen_error(gen,"Schema element tests not supported");
    case KIND_SCHEMA_ATTRIBUTE:
      return gen_error(gen,"Schema attribute tests not supported");
    case KIND_PI:
      return gen_error(gen,"Processing instruction tests not supported");
      break;
    case KIND_COMMENT:
      gen_printf(gen,"(xslt::type_test xml::TYPE_COMMENT)");
      break;
    case KIND_TEXT:
      gen_printf(gen,"(xslt::type_test xml::TYPE_TEXT)");
      break;
    case KIND_ANY:
      gen_printf(gen,"xslt::any_test");
      break;
    default:
      abort();
    }
  }
  else {
    char *nsuri = expr->qn.uri ? escape1(expr->qn.uri) : NULL;
    char *localname = expr->qn.localpart ? escape1(expr->qn.localpart) : NULL;
    char *type = (AXIS_ATTRIBUTE == expr->axis) ? "xml::TYPE_ATTRIBUTE" : "xml::TYPE_ELEMENT";

    if (nsuri && localname)
      gen_printf(gen,"(xslt::name_test %s \"%s\" \"%s\")",type,nsuri,localname);
    else if (localname)
      gen_printf(gen,"(xslt::wildcard_uri_test %s \"%s\")",type,localname);
    else if (nsuri)
      gen_printf(gen,"(xslt::wildcard_localname_test %s \"%s\")",type,nsuri);
    else
      gen_printf(gen,"(xslt::type_test %s)",type);
    free(nsuri);
    free(localname);
  }
  return 1;
}

static int compile_binary(elcgen *gen, xsltnode *xn, expression *expr, const char *fun)
{
  int r = 1;
  gen_iprintf(gen,"(%s ",fun);
  gen->indent++;
  r = r && compile_expression(gen,xn,expr->left);
  gen_printf(gen," ");
  r = r && compile_expression(gen,xn,expr->right);
  gen_printf(gen,")");
  gen->indent--;
  return r;
}

static int compile_num_binary(elcgen *gen, xsltnode *xn, expression *expr, const char *fun)
{
  int r = 1;
  gen_iprintf(gen,"(%s ",fun);
  gen->indent++;
  r = r && compile_num_expression(gen,xn,expr->left);
  gen_printf(gen," ");
  r = r && compile_num_expression(gen,xn,expr->right);
  gen_printf(gen,")");
  gen->indent--;
  return r;
}

static int compile_ebv_binary(elcgen *gen, xsltnode *xn, expression *expr, const char *fun,
                              const char *op)
{
  if ((RESTYPE_NUMBER == expr->left->restype) && (RESTYPE_NUMBER == expr->right->restype))
    return compile_num_binary(gen,xn,expr,op);
  else
    return compile_binary(gen,xn,expr,fun);
}

static int compile_ebv_expression(elcgen *gen, xsltnode *xn, expression *expr)
{
  int r;
  switch (expr->type) {
  case XPATH_OR:
    gen_printf(gen,"(|| ");
    gen->indent++;
    r = r && compile_ebv_expression(gen,xn,expr->left);
    gen_printf(gen," ");
    r = r && compile_ebv_expression(gen,xn,expr->right);
    gen->indent--;
    gen_printf(gen,")");
    break;
  case XPATH_AND:
    gen_printf(gen,"(&& ");
    gen->indent++;
    r = r && compile_ebv_expression(gen,xn,expr->left);
    gen_printf(gen," ");
    r = r && compile_ebv_expression(gen,xn,expr->right);
    gen->indent--;
    gen_printf(gen,")");
    break;
  case XPATH_VALUE_EQ:   return compile_ebv_binary(gen,xn,expr,"xslt::value_eq_ebv","==");
  case XPATH_VALUE_NE:   return compile_ebv_binary(gen,xn,expr,"xslt::value_ne_ebv","!=");
  case XPATH_VALUE_LT:   return compile_ebv_binary(gen,xn,expr,"xslt::value_lt_ebv","<");
  case XPATH_VALUE_LE:   return compile_ebv_binary(gen,xn,expr,"xslt::value_le_ebv","<=");
  case XPATH_VALUE_GT:   return compile_ebv_binary(gen,xn,expr,"xslt::value_gt_ebv",">");
  case XPATH_VALUE_GE:   return compile_ebv_binary(gen,xn,expr,"xslt::value_ge_ebv",">=");
  case XPATH_GENERAL_EQ: return compile_ebv_binary(gen,xn,expr,"xslt::general_eq","==");
  case XPATH_GENERAL_NE: return compile_ebv_binary(gen,xn,expr,"xslt::general_ne","!=");
  case XPATH_GENERAL_LT: return compile_ebv_binary(gen,xn,expr,"xslt::general_lt","<");
  case XPATH_GENERAL_LE: return compile_ebv_binary(gen,xn,expr,"xslt::general_le","<=");
  case XPATH_GENERAL_GT: return compile_ebv_binary(gen,xn,expr,"xslt::general_gt",">");
  case XPATH_GENERAL_GE: return compile_ebv_binary(gen,xn,expr,"xslt::general_ge",">=");
  case XPATH_PAREN:
    return compile_ebv_expression(gen,xn,expr->left);
  default:
    gen_iprintf(gen,"(xslt::ebv ");
    r = r && compile_expression(gen,xn,expr);
    gen_printf(gen,")");
  }
  return r;
}

static int compile_user_function_call(elcgen *gen, xsltnode *xn,
                                      xmlNodePtr n, expression *expr, int fromnum)
{
  int pnum = 0;

  assert(expr->target);
  assert(xn->n == n);
  if (!fromnum && (RESTYPE_NUMBER == expr->target->restype)) {
    gen_iprintf(gen,"(cons (xml::mknumber ");
    gen->indent++;
  }

  gen_iprintf(gen,"(%s",expr->target->name_ident);
  gen->indent++;

  expression *ap = expr->left;
  xsltnode *fp = expr->target->children.first;


  for (; ap; ap = ap->right, fp = fp->next) {
    assert(XPATH_ACTUAL_PARAM == ap->type);
    assert(fp && (XSLT_PARAM == fp->type));
    gen_printf(gen," ");
    if (RESTYPE_NUMBER == fp->restype) {
      if (RESTYPE_NUMBER == ap->left->restype) {
        if (!compile_num_expression(gen,xn,ap->left))
          return 0;
      }
      else {
        gen_iprintf(gen,"(xslt::seq_to_number");
        if (!compile_expression(gen,xn,ap->left))
          return 0;
        gen_printf(gen,")");
      }
    }
    else {
      if (!compile_expression(gen,xn,ap->left))
        return 0;
    }
    pnum++;
  }
  gen->indent--;
  gen_printf(gen,")");

  if (!fromnum && (RESTYPE_NUMBER == expr->target->restype)) {
    gen_iprintf(gen,") nil)");
    gen->indent--;
  }

  return 1;
}

static int compile_builtin_function_call(elcgen *gen, xsltnode *xn,
                                         xmlNodePtr n, expression *expr)
{
  expression *p;
  int nargs = 0;

  for (p = expr->left; p; p = p->right)
    nargs++;

  if (!strcmp(expr->qn.localpart,"string") && (0 == nargs)) {
    gen_iprintf(gen,"(xslt::fn_string0 citem)");
  }
  else if (!strcmp(expr->qn.localpart,"string-length") && (0 == nargs)) {
    gen_iprintf(gen,"(xslt::fn_string_length0 citem)");
  }
  else if (!strcmp(expr->qn.localpart,"root") && (0 == nargs)) {
    gen_printf(gen,"(cons (xml::item_root citem) nil)");
  }
  else if (!strcmp(expr->qn.localpart,"position")) {
    gen_iprintf(gen,"(cons (xml::mknumber cpos) nil)");
  }
  else if (!strcmp(expr->qn.localpart,"last")) {
    gen_iprintf(gen,"(cons (xml::mknumber csize) nil)");
  }
  else {
    char *funname = strdup(expr->qn.localpart);
    char *c;
    int r = 1;

    for (c = funname; '\0' != *c; c++)
      if ('-' == *c)
        *c = '_';

    gen_iprintf(gen,"(xslt::fn_%s",funname);
    gen->indent++;
    for (p = expr->left; p && r; p = p->right) {
      assert(XPATH_ACTUAL_PARAM == p->type);
      gen_printf(gen," ");
      r = r && compile_expression(gen,xn,p->left);
    }
    gen->indent--;
    gen_printf(gen,")");
    free(funname);
    return r;
  }
  return 1;
}

static int compile_num_expression(elcgen *gen, xsltnode *xn, expression *expr)
{
  int r = 1;
  assert((RESTYPE_NUMBER == expr->restype) || (RESTYPE_RECURSIVE == expr->restype));
  switch (expr->type) {
  case XPATH_ADD:        return compile_num_binary(gen,xn,expr,"+");
  case XPATH_SUBTRACT:   return compile_num_binary(gen,xn,expr,"-");
  case XPATH_MULTIPLY:   return compile_num_binary(gen,xn,expr,"*");
  case XPATH_DIVIDE:     return compile_num_binary(gen,xn,expr,"/");
  case XPATH_IDIVIDE:    return gen_error(gen,"idivide not yet supported");
  case XPATH_MOD:        return compile_num_binary(gen,xn,expr,"%");
  case XPATH_INTEGER_LITERAL:
  case XPATH_DECIMAL_LITERAL:
  case XPATH_DOUBLE_LITERAL:
    gen_iprintf(gen,"%f",expr->num);
    break;
  case XPATH_PAREN:
    return compile_num_expression(gen,xn,expr->left);
  case XPATH_IF: {
    gen_iprintf(gen,"(if ");
    gen->indent++;
    r = r && compile_ebv_expression(gen,xn,expr->test);
    gen_printf(gen," ");
    r = r && compile_num_expression(gen,xn,expr->left);
    gen_printf(gen," ");
    r = r && compile_num_expression(gen,xn,expr->right);
    gen->indent--;
    gen_printf(gen,")");
    break;
  }
  case XPATH_VAR_REF:
    gen_printf(gen,"%s",expr->target->name_ident);
    break;
  case XPATH_FUNCTION_CALL:
    return compile_user_function_call(gen,xn,xn->n,expr,1);
  case XPATH_CONTEXT_ITEM:
    gen_iprintf(gen,"(xml::item_value citem)");
    break;
  default:
    return gen_error(gen,"Unsupported numeric expression type: %d",expr->type);
  }
  return r;
}

static int compile_expression(elcgen *gen, xsltnode *xn, expression *expr)
{
  xmlNodePtr n = xn->n;
  int r = 1;

  if (RESTYPE_NUMBER == expr->restype) {
    gen_iprintf(gen,"(cons (xml::mknumber ");
    r = compile_num_expression(gen,xn,expr);
    gen_printf(gen,") nil)");
    return r;
  }

  switch (expr->type) {
  case XPATH_OR:
  case XPATH_AND:
    gen_iprintf(gen,"(cons (xml::mkbool ");
    r = compile_ebv_expression(gen,xn,expr);
    gen_printf(gen,") nil)");
    break;
  case XPATH_VALUE_EQ:   return compile_binary(gen,xn,expr,"xslt::value_eq");
  case XPATH_VALUE_NE:   return compile_binary(gen,xn,expr,"xslt::value_ne");
  case XPATH_VALUE_LT:   return compile_binary(gen,xn,expr,"xslt::value_lt");
  case XPATH_VALUE_LE:   return compile_binary(gen,xn,expr,"xslt::value_le");
  case XPATH_VALUE_GT:   return compile_binary(gen,xn,expr,"xslt::value_gt");
  case XPATH_VALUE_GE:   return compile_binary(gen,xn,expr,"xslt::value_ge");
  case XPATH_GENERAL_EQ:
  case XPATH_GENERAL_NE:
  case XPATH_GENERAL_LT:
  case XPATH_GENERAL_LE:
  case XPATH_GENERAL_GT:
  case XPATH_GENERAL_GE:
    gen_iprintf(gen,"(cons (xml::mkbool ");
    r = compile_ebv_expression(gen,xn,expr);
    gen_printf(gen,") nil)");
    break;
  case XPATH_ADD:        return compile_binary(gen,xn,expr,"xslt::add");
  case XPATH_SUBTRACT:   return compile_binary(gen,xn,expr,"xslt::subtract");
  case XPATH_MULTIPLY:   return compile_binary(gen,xn,expr,"xslt::multiply");
  case XPATH_DIVIDE:     return compile_binary(gen,xn,expr,"xslt::divide");
  case XPATH_IDIVIDE:    return compile_binary(gen,xn,expr,"xslt::idivide"); /* FIXME: test */
  case XPATH_MOD:        return compile_binary(gen,xn,expr,"xslt::mod");
  case XPATH_INTEGER_LITERAL:
    gen_iprintf(gen,"(cons (xml::mknumber %f) nil)",expr->num);
    break;
  case XPATH_DECIMAL_LITERAL:
    gen_iprintf(gen,"(cons (xml::mknumber %f) nil)",expr->num);
    break;
  case XPATH_DOUBLE_LITERAL:
    gen_iprintf(gen,"(cons (xml::mknumber %f) nil)",expr->num);
    break;
  case XPATH_STRING_LITERAL: {
    char *esc = escape1(expr->str);
    gen_iprintf(gen,"(cons (xml::mkstring \"%s\") nil)",esc);
    free(esc);
    break;
  }
  case XPATH_IF: {
    gen_iprintf(gen,"(if ");
    gen->indent++;
    r = r && compile_ebv_expression(gen,xn,expr->test);
    gen_printf(gen," ");
    r = r && compile_expression(gen,xn,expr->left);
    gen_printf(gen," ");
    r = r && compile_expression(gen,xn,expr->right);
    gen->indent--;
    gen_printf(gen,")");
    break;
  }
  case XPATH_TO: {
    /* FIXME: need the checks specified in XPath 2.0 section 3.3.1 */
    gen_iprintf(gen,"(xslt::range ");
    gen->indent++;
    gen_iprintf(gen,"(xslt::getnumber ");
    gen->indent++;
    r = r && compile_expression(gen,xn,expr->left);
    gen->indent--;
    gen_printf(gen,") ");
    gen_iprintf(gen,"(xslt::getnumber ");
    gen->indent++;
    r = r && compile_expression(gen,xn,expr->right);
    gen->indent--;
    gen_printf(gen,"))");
    gen->indent--;
    break;
  }
  case XPATH_SEQUENCE:
    gen_iprintf(gen,"(append ");
    r = r && compile_expression(gen,xn,expr->left);
    gen_printf(gen," ");
    r = r && compile_expression(gen,xn,expr->right);
    gen_printf(gen,")");
    break;
  case XPATH_CONTEXT_ITEM:
    gen_iprintf(gen,"(cons citem nil)");
    break;
  case XPATH_UNARY_MINUS:
    gen_iprintf(gen,"(xslt::uminus ");
    gen->indent++;
    r = r && compile_expression(gen,xn,expr->left);
    gen->indent--;
    gen_printf(gen,")");
    break;
  case XPATH_UNARY_PLUS:
    gen_iprintf(gen,"(xslt::uplus ");
    gen->indent++;
    r = r && compile_expression(gen,xn,expr->left);
    gen->indent--;
    gen_printf(gen,")");
    break;
  case XPATH_PAREN:
    return compile_expression(gen,xn,expr->left);
  case XPATH_EMPTY:
    gen_printf(gen,"nil");
    break;
  case XPATH_VAR_REF: {
    gen_printf(gen,"%s",expr->target->name_ident);
    break;
  }
  case XPATH_FILTER:
    gen_printorig(gen,"predicate",NULL,NULL);
    gen_iprintf(gen,"(xslt::filter3");
    gen->indent++;
    gen_iprintf(gen,"(!citem.!cpos.!csize.xslt::predicate_match cpos ");
    r = r && compile_expression(gen,xn,expr->right);
    gen_printf(gen,") ");
    r = r && compile_expression(gen,xn,expr->left);
    gen_printf(gen,")");
    gen->indent--;
    break;
  case XPATH_STEP:
    gen_printorig(gen,"path step",NULL,NULL);

    if (is_expr_doc_order(xn,expr->left) && is_expr_doc_order(xn,expr->right))
      gen_iprintf(gen,"(xslt::path_result");
    else
      gen_iprintf(gen,"(xslt::path_result_sort");
    gen->indent++;
    gen_iprintf(gen,"(xslt::apmap3 (!citem.!cpos.!csize.");
    gen->indent++;
    r = r && compile_expression(gen,xn,expr->right);
    gen_printf(gen,") ");
    r = r && compile_expression(gen,xn,expr->left);
    gen->indent--;
    gen_printf(gen,")");
    gen_printf(gen,")");
    gen->indent--;
    break;
  case XPATH_FORWARD_AXIS_STEP:
    return compile_expression(gen,xn,expr->left);
  case XPATH_REVERSE_AXIS_STEP:
    gen_iprintf(gen,"(reverse ");
    gen->indent++;
    r = r && compile_expression(gen,xn,expr->left);
    gen->indent--;
    gen_printf(gen,")");
    break;
  case XPATH_KIND_TEST:
  case XPATH_NAME_TEST:
    gen_iprintf(gen,"(filter ");
    gen->indent++;

    r = r && compile_test(gen,xn,expr);

    switch (expr->axis) {
    case AXIS_SELF:
      gen_iprintf(gen,"(cons citem nil)");
      break;
    case AXIS_CHILD:
      gen_iprintf(gen,"(xml::item_children citem)");
      break;
    case AXIS_DESCENDANT:
      gen_iprintf(gen,"(xslt::node_descendants citem)");
      break;
    case AXIS_DESCENDANT_OR_SELF:
      gen_iprintf(gen,"(cons citem (xslt::node_descendants citem))");
      break;
    case AXIS_PARENT:
      gen_iprintf(gen,"(xslt::node_parent_list citem)");
      break;
    case AXIS_ANCESTOR:
      gen_iprintf(gen,"(xslt::node_ancestors citem)");
      break;
    case AXIS_ANCESTOR_OR_SELF:
      gen_iprintf(gen,"(xslt::node_ancestors_or_self citem)");
      break;
    case AXIS_PRECEDING_SIBLING:
      gen_iprintf(gen,"(xslt::node_preceding_siblings citem)");
      break;
    case AXIS_FOLLOWING_SIBLING:
      gen_iprintf(gen,"(xslt::node_following_siblings citem)");
      break;
    case AXIS_PRECEDING:
      gen_iprintf(gen,"(xslt::node_preceding citem)");
      break;
    case AXIS_FOLLOWING:
      gen_iprintf(gen,"(xslt::node_following citem)");
      break;
    case AXIS_ATTRIBUTE:
      gen_iprintf(gen,"(xml::item_attributes citem)");
      break;
    case AXIS_NAMESPACE:
      gen_iprintf(gen,"(xml::item_namespaces citem)");
      break;
    default:
      assert(!"unsupported axis");
      break;
    }
    gen_printf(gen,")");
    gen->indent--;

    break;
  case XPATH_FUNCTION_CALL:
    if (!strcmp(expr->qn.prefix,""))
      return compile_builtin_function_call(gen,xn,n,expr);
    else if (expr->target)
      return compile_user_function_call(gen,xn,n,expr,0);
    else if (!strncmp(expr->qn.uri,"wsdl-",5))
      return compile_ws_call(gen,xn,expr,expr->qn.uri+5);
    else
      return gen_error(gen,"Call to non-existent function {%s}%s",
                       expr->qn.uri ? expr->qn.uri : "",expr->qn.localpart);
    break;
  case XPATH_ROOT:
    gen_printf(gen,"(cons (xml::item_root citem) nil)");
    break;
  default:
    return gen_error(gen,"Unsupported expression type: %d",expr->type);
  }
  return r;
}

static int compile_avt_component(elcgen *gen, xsltnode *xn, expression *expr)
{
  if (XPATH_STRING_LITERAL == expr->type) {
    char *esc = escape1(expr->str);
    gen_printf(gen,"\"%s\" ",esc);
    free(esc);
  }
  else {
    gen_printf(gen,"(xslt::consimple ");
    if (!compile_expression(gen,xn,expr))
      return 0;
    gen_printf(gen,") ");
  }
  return 1;
}

static int compile_avt(elcgen *gen, xsltnode *xn, expression *expr)
{
  expression *cur = expr;
  int count = 0;
  int r = 1;

  while (r && (XPATH_AVT_COMPONENT == cur->type)) {
    gen_printf(gen,"(append ");
    r = r && compile_avt_component(gen,xn,cur->left);
    count++;
    cur = cur->right;
  }

  r = r && compile_avt_component(gen,xn,cur);

  while (0 < count--)
    gen_printf(gen,")");

  return r;
}

static int compile_attributes(elcgen *gen, xsltnode *xn)
{
  xsltnode *xattr;
  int count = 0;
  int r = 1;

  for (xattr = xn->attributes.first; xattr && r; xattr = xattr->next) {
    if (xattr->name_qn.uri)
      gen_printf(gen,"(cons (xml::mkattr nil nil nil nil \"%s\" \"%s\" \"%s\" ",
                 xattr->name_qn.uri,xattr->name_qn.prefix,xattr->name_qn.localpart);
    else
      gen_printf(gen,"(cons (xml::mkattr nil nil nil nil nil nil \"%s\" ",
                 xattr->name_qn.localpart);
    r = compile_avt(gen,xn,xattr->value_avt);
    gen_printf(gen,")\n");
    count++;
  }
  gen_printf(gen,"nil");
  while (0 < count--)
    gen_printf(gen,")");
  return r;
}

static int have_prefix(xmlNodePtr start, xmlNodePtr end, const char *prefix)
{
  xmlNsPtr ns;
  xmlNodePtr p;
  for (p = start; p != end; p = p->parent)
    for (ns = p->nsDef; ns; ns = ns->next)
      if (!nullstrcmp((char*)ns->prefix,prefix))
        return 1;
  return 0;
}

static int compile_namespaces(elcgen *gen, xsltnode *xn)
{
  xmlNsPtr ns;
  xmlNodePtr p;
  int count = 0;
  for (p = xn->n; p; p = p->parent) {
    for (ns = p->nsDef; ns; ns = ns->next) {
      if (have_prefix(xn->n,p,(char*)ns->prefix))
        continue;
      if (ns->prefix) {
        if (!exclude_namespace(gen,xn,(char*)ns->href)) {
          gen_iprintf(gen,"(cons (xml::mknamespace \"%s\" \"%s\") ",ns->href,ns->prefix);
          count++;
        }
      }
      else {
        gen_iprintf(gen,"(cons (xml::mknamespace \"%s\" nil) ",ns->href);
        count++;
      }
    }
  }
  gen_printf(gen,"nil");
  while (0 < count--)
    gen_printf(gen,")");
  return 1;
}

static int compile_instruction(elcgen *gen, xsltnode *xn)
{
  int r = 1;
  switch (xn->type) {
  case XSLT_SEQUENCE: {
    gen_printorig(gen,"sequence",xn,"select");
    r = compile_expression(gen,xn,xn->expr);
    return r;
  }
  case XSLT_VALUE_OF: {
    gen_iprintf(gen,"(xslt::construct_value_of ");
    if (xn->expr)
      r = compile_expression(gen,xn,xn->expr);
    else
      r = compile_sequence(gen,xn->children.first);
    gen_printf(gen,")");
    return r;
  }
  case XSLT_TEXT: {
    char *str = xmlNodeListGetString(gen->parse_doc,xn->n->children,1);
    char *esc = escape1(str);
    gen_iprintf(gen,"(xslt::construct_text \"%s\")",esc);
    free(esc);
    free(str);
    break;
  }
  case XSLT_FOR_EACH: {
    gen_printorig(gen,"for-each",xn,"select");
    gen_iprintf(gen,"(xslt::foreach3 ");
    gen->indent++;
    r = r && compile_expression(gen,xn,xn->expr);
    gen_iprintf(gen,"(!citem.!cpos.!csize.");
    gen->indent++;
    r = r && compile_sequence(gen,xn->children.first);
    gen_printf(gen,"))");
    gen->indent--;
    gen->indent--;
    return r;
  }
  case XSLT_IF: {
    gen_printorig(gen,"if",xn,"test");
    gen_iprintf(gen,"(if ");
    gen->indent++;
    r = r && compile_ebv_expression(gen,xn,xn->expr);
    gen_printf(gen," ");
    r = r && compile_sequence(gen,xn->children.first);
    gen_iprintf(gen,"nil)");
    gen->indent--;
    return r;
  }
  case XSLT_CHOOSE: {
    xsltnode *xchild;
    int count = 0;
    int otherwise = 0;

    gen_printorig(gen,"choose",NULL,NULL);

    for (xchild = xn->children.first; xchild && r; xchild = xchild->next) {
      switch (xchild->type) {
      case XSLT_WHEN:
        gen_printorig(gen,"when",xchild,"test");
        gen_iprintf(gen,"(if ");
        gen->indent++;
        r = r && compile_ebv_expression(gen,xchild,xchild->expr);
        gen_printf(gen," ");
        r = r && compile_sequence(gen,xchild->children.first);
        gen_printf(gen," ");
        gen->indent--;
        count++;
        break;
      case XSLT_OTHERWISE:
        gen_printorig(gen,"otherwise",NULL,NULL);
        r = r && compile_sequence(gen,xchild->children.first);
        otherwise = 1;
        break;
      default:
        r = gen_error(gen,"Invalid element in choose: %s\n",xslt_names[xchild->type]);
        break;
      }
    }

    if (!otherwise)
      gen_printf(gen,"nil");
    while (0 < count--)
      gen_printf(gen,")");
    return r;
  }
  case XSLT_ELEMENT: {
    /* FIXME: complete this, and handle namespaces properly */
    gen_printorig(gen,"element",xn,"name");
    gen_iprintf(gen,"(xslt::construct_elem1 ");

    if (xn->namespace_avt)
      r = compile_avt(gen,xn,xn->namespace_avt);
    else
      gen_iprintf(gen,"nil ");

    r = r && compile_avt(gen,xn,xn->name_avt);

    gen->indent++;
    gen_printf(gen," nil nil ");
    r = r && compile_sequence(gen,xn->children.first);
    gen->indent--;
    gen_printf(gen,")");
    return r;
  }
  case XSLT_ATTRIBUTE: {
    /* FIXME: handle namespaces properly */
    gen_printorig(gen,"attribute",xn,"name");
    gen_iprintf(gen,"(cons (xml::mkattr nil nil nil nil ");
    gen_printf(gen,"nil "); // nsuri
    gen_printf(gen,"nil "); // nsprefix

    gen->indent++;
    // localname
    r = compile_avt(gen,xn,xn->name_avt);
    // value
    gen_printf(gen,"(xslt::consimple ");
    if (xn->expr)
      r = r && compile_expression(gen,xn,xn->expr);
    else
      r = r && compile_sequence(gen,xn->children.first);
    gen_printf(gen,")");
    gen_printf(gen,") nil)");
    gen->indent--;
    return r;
  }
  case XSLT_INAMESPACE: {
    gen_printorig(gen,"namespace",xn,"name");
    gen_iprintf(gen,"(cons (xml::mknamespace (xslt::consimple ");

    if (xn->expr)
      r = compile_expression(gen,xn,xn->expr);
    else
      r = compile_sequence(gen,xn->children.first);
    gen_printf(gen,") ");

    r = r && compile_avt(gen,xn,xn->name_avt);
    gen_printf(gen,") nil)");
    return r;
  }
  case XSLT_APPLY_TEMPLATES: {
    gen_printorig(gen,"apply-templates",xn,"select");
    gen_iprintf(gen,"(apply_templates ");
    gen->indent++;
    if (xn->expr)
      r = compile_expression(gen,xn,xn->expr);
    else
      gen_printf(gen,"(xml::item_children citem)");
    gen->indent--;
    gen_printf(gen,")");
    break;
  }
  case XSLT_LITERAL_RESULT_ELEMENT: 
    /* literal result element */
    gen_iprintf(gen,"(xslt::construct_elem2 ");
    if (xn->n->ns && xn->n->ns->prefix)
      gen_printf(gen," \"%s\" \"%s\" \"%s\" ",xn->n->ns->href,xn->n->ns->prefix,xn->n->name);
    else if (xn->n->ns)
      gen_printf(gen," \"%s\" \"\" \"%s\" ",xn->n->ns->href,xn->n->name);
    else
      gen_printf(gen," nil nil \"%s\" ",xn->n->name);
    gen->indent++;
    r = compile_attributes(gen,xn);
    gen_printf(gen," ");
    r = r && compile_namespaces(gen,xn);
    gen_printf(gen," ");
    r = r && compile_sequence(gen,xn->children.first);
    gen->indent--;
    gen_printf(gen,")");
    return r;
  case XSLT_LITERAL_TEXT_NODE: {
    char *esc = escape1((const char*)xn->n->content);
    gen_iprintf(gen,"(xslt::construct_text \"%s\")",esc);
    free(esc);
    break;
  }
  default:
    return gen_error(gen,"Invalid XSLT node: %d\n",xn->type);
  }
  return 1;
}

static int compile_variable(elcgen *gen, xsltnode *xchild)
{
  int r = 1;
  gen_iprintf(gen,"(letrec %s = ",xchild->name_ident);
  gen->indent++;
  if (xchild->expr) {
    r = compile_expression(gen,xchild,xchild->expr);
  }
  else {
    gen_printf(gen,"(cons (xslt::copydoc (!x.x) (xml::mkdoc ");
    r = compile_sequence(gen,xchild->children.first);
    gen_printf(gen,")) nil)");
  }
  gen->indent--;
  gen_iprintf(gen," in ");
  return r;
}

static int compile_num_instruction(elcgen *gen, xsltnode *xn)
{
  int r = 1;
  switch (xn->type) {
  case XSLT_SEQUENCE: {
    gen_printorig(gen,"sequence",xn,"select");
    r = compile_num_expression(gen,xn,xn->expr);
    return r;
  }
  case XSLT_CHOOSE: {
    xsltnode *xchild;
    int count = 0;
    int otherwise = 0;

    gen_printorig(gen,"choose",NULL,NULL);

    for (xchild = xn->children.first; xchild && r; xchild = xchild->next) {
      switch (xchild->type) {
      case XSLT_WHEN:
        gen_printorig(gen,"when",xchild,"test");
        gen_iprintf(gen,"(if ");
        gen->indent++;
        r = r && compile_ebv_expression(gen,xchild,xchild->expr);
        gen_printf(gen," ");
        r = r && compile_num_sequence(gen,xchild->children.first);
        gen_printf(gen," ");
        gen->indent--;
        count++;
        break;
      case XSLT_OTHERWISE:
        gen_printorig(gen,"otherwise",NULL,NULL);
        r = r && compile_num_sequence(gen,xchild->children.first);
        otherwise = 1;
        break;
      default:
        r = gen_error(gen,"Invalid element in choose: %s\n",xslt_names[xchild->type]);
        break;
      }
    }

    assert(otherwise);
    while (0 < count--)
      gen_printf(gen,")");
    return r;
  }
  default:
    abort(); /* should not encounter any other instruction here */
  }
  return r;
}

static int compile_num_sequence(elcgen *gen, xsltnode *xfirst)
{
  xsltnode *xchild;
  int count = 0;
  int r = 1;

  for (xchild = xfirst; xchild && (XSLT_VARIABLE == xchild->type); xchild = xchild->next) {
    if (!compile_variable(gen,xchild))
      return 0;
    count++;
  }

  assert(xchild);
  assert(NULL == xchild->next);

  r = compile_num_instruction(gen,xchild);

  while (0 < count--)
    gen_printf(gen,")");
  return r;
}

static int compile_sequence(elcgen *gen, xsltnode *xfirst)
{
  xsltnode *xchild;
  int count = 0;
  int r = 1;

  if (NULL == xfirst) {
    gen_printf(gen,"nil");
    return 1;
  }

  if (NULL == xfirst->next)
    return compile_instruction(gen,xfirst);

  for (xchild = xfirst; xchild && r; xchild = xchild->next) {
    if (XSLT_VARIABLE == xchild->type) {
      /* FIXME: support top-level variables as well */
      r = compile_variable(gen,xchild);
    }
    else {
      gen_printf(gen,"(append ");
      r = compile_instruction(gen,xchild);
      gen_printf(gen," ");
    }
    count++;
  }
  gen_printf(gen,"nil");
  while (0 < count--)
    gen_printf(gen,")");
  return r;
}

static int compile_template(elcgen *gen, xsltnode *xn, int pos)
{
  int r;
  gen_iprintf(gen,"");
  gen_iprintf(gen,"template%d citem cpos csize = ",pos);
  r = compile_sequence(gen,xn->children.first);
  return r;
}

static int compile_function(elcgen *gen, xsltnode *xn)
{
  xsltnode *pxn;
  int r;

  if (NULL == xn->name_qn.localpart)
    return gen_error(gen,"XTSE0740: function must have a prefixed name");

  gen_iprintf(gen,"");
  gen_iprintf(gen,"%s",xn->name_ident);
  for (pxn = xn->children.first; pxn && (XSLT_PARAM == pxn->type); pxn = pxn->next)
    gen_printf(gen," %s",pxn->name_ident);
  gen_printf(gen," = ");

  if (RESTYPE_NUMBER == xn->restype) {
    gen->indent++;
    r = compile_num_sequence(gen,pxn);
    gen->indent--;
  }
  else {
    gen_iprintf(gen,"(xslt::reparent ");
    gen->indent++;
    r = compile_sequence(gen,pxn);
    gen->indent--;
    gen_iprintf(gen,"  nil nil nil)");
  }

  return r;
}

//#define PRINT_PATTERN
#ifdef PRINT_PATTERN
static void print_pattern(elcgen *gen, expression *expr)
{
  switch (expr->type) {
  case XPATH_FORWARD_AXIS_STEP:
    gen_iprintf(gen,"forward");
    gen->indent++;
    print_pattern(gen,expr->left);
    gen->indent--;
    break;
  case XPATH_REVERSE_AXIS_STEP:
    gen_iprintf(gen,"reverse");
    gen->indent++;
    print_pattern(gen,expr->left);
    gen->indent--;
    break;
  case XPATH_STEP:
    gen_iprintf(gen,"step");
    gen->indent++;
    print_pattern(gen,expr->left);
    print_pattern(gen,expr->right);
    gen->indent--;
    break;
  case XPATH_FILTER:
    gen_iprintf(gen,"filter");
    gen->indent++;
    print_pattern(gen,expr->left);
    print_pattern(gen,expr->right);
    gen->indent--;
    break;
  case XPATH_ROOT:
    gen_iprintf(gen,"root");
    break;
  case XPATH_KIND_TEST:
    gen_iprintf(gen,"kind=%s (%s)",kind_names[expr->kind],axis_names[expr->axis]);
    break;
  case XPATH_NAME_TEST:
    gen_iprintf(gen,"name=%s (%s)",expr->qn.localpart,axis_names[expr->axis]);
    break;
  default:
    gen_iprintf(gen,"unknown %d",expr->type);
  }
}
#endif

static int compile_pattern(elcgen *gen, xsltnode *xn, expression *expr, int p)
{
  int r = 1;
  switch (expr->type) {
  case XPATH_FORWARD_AXIS_STEP:
  case XPATH_REVERSE_AXIS_STEP:
    return compile_pattern(gen,xn,expr->left,p);
  case XPATH_STEP:
    gen->indent++;
    gen_iprintf(gen,"/* step %d/%d */ ",expr->left->type,expr->right->type);
    if ((XPATH_KIND_TEST == expr->right->type) &&
        (KIND_ANY == expr->right->kind) &&
        (AXIS_DESCENDANT_OR_SELF == expr->right->axis)) {
      gen_iprintf(gen,"(xslt::check_aos p%d (!p%d.",p,p+1);
      compile_pattern(gen,xn,expr->left,p+1);
      gen_iprintf(gen,"))",p);
    }
    else {
      gen_iprintf(gen,"(letrec r = ");
      r = r && compile_pattern(gen,xn,expr->right,p);
      gen_printf(gen," in ");
      gen_iprintf(gen,"(if r ");
      /* FIXME: need to take into account the case where parent is nil */
      gen_iprintf(gen,"(letrec p%d = (xml::item_parent p%d) in ",p+1,p);
      r = r && compile_pattern(gen,xn,expr->left,p+1);
      gen_printf(gen,")");
      gen_iprintf(gen,"nil))");
    }
    gen->indent--;
    break;
  case XPATH_FILTER:
    gen_iprintf(gen,"/* predicate %d/%d */ ",expr->left->type,expr->right->type);
    gen_iprintf(gen,"(letrec ");
    gen_iprintf(gen,"  citem = p%d",p);
    gen_iprintf(gen,"  cpos = (xslt::compute_pos p%d)");
    gen_iprintf(gen,"  csize = (xslt::compute_size p%d)");
    gen_iprintf(gen,"in");
    gen_iprintf(gen,"  (xslt::predicate_match cpos ");
    gen->indent++;
    r = r && compile_expression(gen,xn,expr->right);
    gen->indent--;
    gen_printf(gen,"))",p);
    break;
  case XPATH_ROOT:
    gen_printf(gen,"(if (== (xml::item_type p%d) xml::TYPE_DOCUMENT) p%d nil)",p,p);
    break;
  case XPATH_KIND_TEST:
  case XPATH_NAME_TEST:
    switch (expr->axis) {
    case AXIS_CHILD:
    case AXIS_ATTRIBUTE:
      gen_printf(gen,"(if (");
      r = r && compile_test(gen,xn,expr);
      gen_printf(gen," p%d) p%d nil)",p,p);
      break;
    default:
      return gen_error(gen,"Invalid axis in pattern: %d",expr->axis);
      break;
    }
    break;
  default:
    return gen_error(gen,"Unsupported pattern construct: %d",expr->type);
  }
  return r;
}

static int compile_apply_templates(elcgen *gen, xsltnode *xn)
{
  int templateno = 0;
  int count = 0;
  int r = 1;
  xsltnode *xchild;

  /* TODO: take into account template priorities */
  /* TODO: test apply-templates with different select values */

  gen_iprintf(gen,"apply_templates lst = (apply_templates1 lst 1 (len lst))");
  gen_iprintf(gen,"");

  gen_iprintf(gen,"apply_templates1 lst cpos csize = ");
  gen_iprintf(gen,"(if lst");
  gen_iprintf(gen,"  (letrec");
  gen_iprintf(gen,"    p0 = (head lst)");
  gen_iprintf(gen,"    rest = (tail lst)");
  gen_iprintf(gen,"  in ");
  gen_iprintf(gen,"    (append ");
  gen->indent += 3;
  for (xchild = xn->children.first; xchild && r; xchild = xchild->next) {
    if ((XSLT_TEMPLATE == xchild->type) && xchild->expr) {
      gen_printorig(gen,"template",xchild,"match");
      gen_iprintf(gen,"(if ");
      gen->indent++;
#ifdef PRINT_PATTERN
      gen_iprintf(gen,"/*");
      print_pattern(gen,xchild->expr);
      gen_iprintf(gen,"*/");
      gen_iprintf(gen,"");
#endif
      r = compile_pattern(gen,xchild,xchild->expr,0);
      gen->indent--;
      gen_iprintf(gen,"  (template%d p0 cpos csize)",templateno);
      count++;
      templateno++;
    }
  }

  /* 6.6 Built-in Template Rules */
  gen_iprintf(gen,"(if (== (xml::item_type p0) xml::TYPE_ELEMENT)");
  gen_iprintf(gen,"  (apply_templates (xml::item_children p0))");
  gen_iprintf(gen,"(if (== (xml::item_type p0) xml::TYPE_DOCUMENT)");
  gen_iprintf(gen,"  (apply_templates (xml::item_children p0))");
  gen_iprintf(gen,"(if (== (xml::item_type p0) xml::TYPE_TEXT)");
  gen_iprintf(gen,"  (cons (xml::mktext (xml::item_value p0)) nil)");
  gen_iprintf(gen,"(if (== (xml::item_type p0) xml::TYPE_ATTRIBUTE)");
  gen_iprintf(gen,"  (cons (xml::mktext (xml::item_value p0)) nil)");
  gen_iprintf(gen,"nil))))");

  while (0 < count--)
    gen_printf(gen,")");

  gen_iprintf(gen,"(apply_templates1 (tail lst) (+ cpos 1) csize)))");
  gen->indent -= 3;
  gen_iprintf(gen,"  nil)");
  gen_iprintf(gen,"");

  return r;
}

static int process_root(elcgen *gen, xmlNodePtr n)
{
  xsltnode *child;
  int r = 1;
  int templateno = 0;

  xsltnode *root;
  xsltnode *c;

  if ((XML_ELEMENT_NODE != n->type) ||
      (NULL == n->ns) || xmlStrcmp(n->ns->href,XSLT_NAMESPACE) ||
      (xmlStrcmp(n->name,"stylesheet") && xmlStrcmp(n->name,"transform")))
    return gen_error(gen,"top-level element must be a xsl:stylesheet or xsl:transform");

  root = new_xsltnode(n,XSLT_STYLESHEET);
  gen->toplevel = n;
  gen->root = root;

  if (!parse_xslt(gen,n,root))
    return 0;
  if (!xslt_resolve_vars(gen,root))
    return 0;

  for (c = root->children.first; c; c = c->next) {
    if (XSLT_TEMPLATE == c->type)
      xslt_compute_restype(gen,c,RESTYPE_GENERAL);
  }

  gen_iprintf(gen,"/* tree:");
  xslt_print_tree(gen,root);
  gen_iprintf(gen,"*/");

  r = r && compile_apply_templates(gen,root);

  for (child = root->children.first; child && r; child = child->next) {
    switch (child->type) {
    case XSLT_TEMPLATE:
      r = compile_template(gen,child,templateno++);
      break;
    case XSLT_FUNCTION:
      if (child->called)
        r = compile_function(gen,child);
      break;
    case XSLT_OUTPUT:
      if (attr_equals(child->n,"indent","yes"))
        gen->option_indent = 1;
      break;
    case XSLT_STRIP_SPACE:
      if (attr_equals(child->n,"elements","*"))
        gen->option_strip = 1;
      break;
    }
  }

  return r;
}

int cxslt(const char *xslt, const char *xslturl, char **result)
{
  xmlDocPtr doc;
  xmlNodePtr root;
  elcgen *gen = (elcgen*)calloc(1,sizeof(elcgen));
  char zero = '\0';
  int r = 0;

  gen->buf = array_new(1,0);
  gen->typestack = stack_new();

  *result = NULL;
  if (NULL == (doc = xmlReadMemory(xslt,strlen(xslt),NULL,NULL,0))) {
    gen_error(gen,"Error parsing %s",xslturl);
  }
  else {
    gen_printf(gen,"import xml\n");
    gen_printf(gen,"import xslt\n");

    gen->parse_doc = doc;
    gen->parse_filename = xslturl;
    root = xmlDocGetRootElement(doc);
    if (process_root(gen,root)) {
      gen_iprintf(gen,"");
      gen_iprintf(gen,"STRIPALL = %s\n",gen->option_strip ? "1" : "nil");
      gen_iprintf(gen,"INDENT = %s\n",gen->option_indent ? "1" : "nil");
      gen_iprintf(gen,
                   "main args stdin =\n"
                   "(letrec\n"
                   "  input =\n"
                   "    (if (== (len args) 0)\n"
                   "      (if stdin\n"
                   "        (xml::parsexml STRIPALL stdin)\n"
                   "        (xml::mkdoc nil))\n"
                   "      (xml::parsexml STRIPALL (readb (head args))))\n"
                   "  result = (apply_templates (cons input nil))\n"
                   "  doc = (cons (xml::mkdoc (xslt::concomplex result)) nil)\n"
                   "in\n"
                   " (xslt::output INDENT doc))\n");
      xmlFreeDoc(doc);
      array_append(gen->buf,&zero,1);
      r = 1;
    }
  }

  if (r) {
    *result = gen->buf->data;
    free(gen->buf);
    r = 1;
  }
  else {
    array_free(gen->buf);
    *result = gen->error;
  }
  stack_free(gen->typestack);
  free(gen);

  return r;
}
