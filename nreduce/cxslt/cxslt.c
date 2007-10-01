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

typedef struct x_buffer_state *X_BUFFER_STATE;
X_BUFFER_STATE x_scan_string(const char *str);
void x_switch_to_buffer(X_BUFFER_STATE new_buffer);
void x_delete_buffer(X_BUFFER_STATE buffer);
#if HAVE_XLEX_DESTROY
int xlex_destroy(void);
#endif

extern FILE *xin;
extern int lex_lineno;
int xparse();

static int compile_sequence(elcgen *gen, xmlNodePtr first);
static int compile_expression(elcgen *gen, xmlNodePtr n, expression *expr);

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

static void gen_printf(elcgen *gen, const char *format, ...)
{
  va_list ap;
  va_start(ap,format);
  array_vprintf(gen->buf,format,ap);
  va_end(ap);
}

static void gen_iprintf(elcgen *gen, const char *format, ...)
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

static void gen_printorig(elcgen *gen, const char *name, const char *expr)
{
  gen_iprintf(gen,"/* %s",name);
  if (expr) {
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
  }
  gen_printf(gen," */ ");
}

static int ignore_node(xmlNodePtr n)
{
  const char *c;
  if (XML_TEXT_NODE != n->type)
    return 0;
  for (c = (const char*)n->content; *c; c++)
    if (!isspace(*c))
      return 0;
  return 1;
}

expression *new_expression(int type)
{
  expression *expr = (expression*)calloc(1,sizeof(expression));
  expr->type = type;
  return expr;
}

expression *new_expression2(int type, expression *left, expression *right)
{
  expression *expr = new_expression(type);
  expr->left = left;
  expr->right = right;
  return expr;
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

expression *new_XPathIfExpr(expression *cond, expression *tbranch, expression *fbranch)
{
  fatal("unsupported: XPathIfExpr");
  return NULL;
}

void free_expression(expression *expr)
{
  if (expr->left)
    free_expression(expr->left);
  if (expr->right)
    free_expression(expr->right);
  if (expr->str)
    free(expr->str);
  free_qname(expr->qn);
  free(expr);
}

void free_wsarg_ptr(void *a)
{
  wsarg *arg = (wsarg*)a;
  free(arg->uri);
  free(arg->localpart);
  free(arg);
}

static char *string_to_ident(const char *str)
{
  const char *hexdigits = "0123456789ABCDEF";
  const char *c;
  int len = 0;
  int pos = 0;
  char *ret;

  for (c = str; '\0' != *c; c++) {
    if ((('A' <= *c) && ('Z' >= *c)) ||
        (('a' <= *c) && ('z' >= *c)) ||
        (('0' <= *c) && ('9' >= *c)))
      len++;
    else
      len += 3;
  }

  ret = (char*)malloc(len+1);

  for (c = str; '\0' != *c; c++) {
    if ((('A' <= *c) && ('Z' >= *c)) ||
        (('a' <= *c) && ('z' >= *c)) ||
        (('0' <= *c) && ('9' >= *c))) {
      ret[pos++] = *c;
    }
    else {
      ret[pos++] = '_';
      ret[pos++] = hexdigits[(*c)&0x0F];
      ret[pos++] = hexdigits[((*c)&0xF0)>>4];
    }
  }
  ret[pos] = '\0';

  return ret;
}

static char *nsname_to_ident(const char *nsuri, const char *localname)
{
  if (nsuri) {
    char *nsident = string_to_ident(nsuri);
    char *lnident = string_to_ident(localname);
    char *full = (char*)malloc(1+strlen(nsident)+1+strlen(lnident)+1);
    sprintf(full,"V%s_%s",nsident,lnident);
    free(nsident);
    free(lnident);
    return full;
  }
  else {
    char *lnident = string_to_ident(localname);
    char *full = (char*)malloc(1+strlen(lnident)+1);
    sprintf(full,"V%s",lnident);
    free(lnident);
    return full;
  }
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

static int have_user_function(elcgen *gen, qname qn)
{
  xmlNodePtr c;
  int have = 0;
  for (c = gen->toplevel->children; c; c = c->next) {
    if (is_element(c,XSLT_NAMESPACE,"function") && xmlHasProp(c,"name")) {
      qname funqn = get_qname_attr(c,"name");
      if (funqn.uri && qn.uri && 
          !strcmp(funqn.uri,qn.uri) && !strcmp(funqn.localpart,qn.localpart))
        have = 1;
      free_qname(funqn);

      if (have)
        return 1;
    }
  }
  return 0;
}

static int compile_binary(elcgen *gen, xmlNodePtr n, expression *expr, const char *fun)
{
  gen_iprintf(gen,"(%s ",fun);
  gen->indent++;
  if (!compile_expression(gen,n,expr->left))
    return 0;
  gen_printf(gen," ");
  if (!compile_expression(gen,n,expr->right))
    return 0;
  gen_printf(gen,")");
  gen->indent--;
  return 1;
}

static int compile_post(elcgen *gen, xmlNodePtr n, expression *expr, const char *service_url,
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
    if (!compile_expression(gen,n,p->left))
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

#define WSCALL_DEBUG
#ifdef WSCALL_DEBUG
static void wscall_debug(elcgen *gen, expression *expr, const char *service_url,
                        qname inelem, qname outelem,
                        list *inargs, list *outargs)
{
  list *l;
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
  printf("\n");
}
#endif

static int compile_ws_call(elcgen *gen, xmlNodePtr n, expression *expr, const char *wsdlurl)
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
    r = compile_post(gen,n,expr,service_url,inelem,outelem,inargs,outargs);

  free(ident);
  free(inelem.uri);
  free(inelem.localpart);
  free(outelem.uri);
  free(outelem.localpart);
  list_free(inargs,free_wsarg_ptr);
  list_free(outargs,free_wsarg_ptr);
  return r;
}

static xmlNodePtr lookup_variable(xmlNodePtr n, qname qn)
{
  for (; n; n = n->parent) {
    xmlNodePtr v;
    for (v = n->prev; v; v = v->prev) {
      if (is_element(v,XSLT_NAMESPACE,"variable") && xmlHasProp(v,"name")) {
        qname varname = get_qname_attr(v,"name");
        if (!nullstrcmp(varname.uri,qn.uri) &&
            !strcmp(varname.localpart,qn.localpart)) {
          free_qname(varname);
          return v;
        }
        free_qname(varname);
      }
    }
  }
  return NULL;
}

static int is_expr_doc_order(xmlNodePtr n, expression *expr)
{
  switch (expr->type) {
  case XPATH_STEP:
  case XPATH_FORWARD_AXIS_STEP:
  case XPATH_REVERSE_AXIS_STEP:
  case XPATH_KIND_TEST:
  case XPATH_NAME_TEST:
  case XPATH_ROOT:
    return 1;
  case XPATH_PAREN:
    return is_expr_doc_order(n,expr->left);
  case XPATH_VAR_REF: {
    xmlNodePtr var = lookup_variable(n,expr->qn);
    return !xmlHasProp(var,"select");
    break;
  }
  default:
    break;
  }
  return 0;
}

static int compile_expression(elcgen *gen, xmlNodePtr n, expression *expr)
{
  switch (expr->type) {
  case XPATH_OR:         return compile_binary(gen,n,expr,"or"); /* FIXME: test */
  case XPATH_AND:        return compile_binary(gen,n,expr,"and"); /* FIXME: test */

  case XPATH_VALUE_EQ:   return compile_binary(gen,n,expr,"xslt::value_eq");
  case XPATH_VALUE_NE:   return compile_binary(gen,n,expr,"xslt::value_ne");
  case XPATH_VALUE_LT:   return compile_binary(gen,n,expr,"xslt::value_lt");
  case XPATH_VALUE_LE:   return compile_binary(gen,n,expr,"xslt::value_le");
  case XPATH_VALUE_GT:   return compile_binary(gen,n,expr,"xslt::value_gt");
  case XPATH_VALUE_GE:   return compile_binary(gen,n,expr,"xslt::value_ge");

  case XPATH_GENERAL_EQ: return compile_binary(gen,n,expr,"xslt::general_eq");
  case XPATH_GENERAL_NE: return compile_binary(gen,n,expr,"xslt::general_ne");
  case XPATH_GENERAL_LT: return compile_binary(gen,n,expr,"xslt::general_lt");
  case XPATH_GENERAL_LE: return compile_binary(gen,n,expr,"xslt::general_le");
  case XPATH_GENERAL_GT: return compile_binary(gen,n,expr,"xslt::general_gt");
  case XPATH_GENERAL_GE: return compile_binary(gen,n,expr,"xslt::general_ge");
  case XPATH_ADD:        return compile_binary(gen,n,expr,"xslt::add");
  case XPATH_SUBTRACT:   return compile_binary(gen,n,expr,"xslt::subtract");
  case XPATH_MULTIPLY:   return compile_binary(gen,n,expr,"xslt::multiply");
  case XPATH_DIVIDE:     return compile_binary(gen,n,expr,"xslt::divide");
  case XPATH_IDIVIDE:    return compile_binary(gen,n,expr,"xslt::idivide"); /* FIXME: test */
  case XPATH_MOD:        return compile_binary(gen,n,expr,"xslt::mod");
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
  case XPATH_TO: {
    gen_iprintf(gen,"(xslt::range ");
    gen->indent++;
    gen_iprintf(gen,"(xslt::getnumber ");
    gen->indent++;
    if (!compile_expression(gen,n,expr->left))
      return 0;
    gen->indent--;
    gen_printf(gen,") ");
    gen_iprintf(gen,"(xslt::getnumber ");
    gen->indent++;
    if (!compile_expression(gen,n,expr->right))
      return 0;
    gen->indent--;
    gen_printf(gen,"))");
    gen->indent--;
    break;
  }
  case XPATH_SEQUENCE:
    gen_iprintf(gen,"(append ");
    if (!compile_expression(gen,n,expr->left))
      return 0;
    gen_printf(gen," ");
    if (!compile_expression(gen,n,expr->right))
      return 0;
    gen_printf(gen,")");
    break;
  case XPATH_CONTEXT_ITEM:
    gen_iprintf(gen,"(cons citem nil)");
    break;
  case XPATH_UNARY_MINUS:
    gen_iprintf(gen,"(xslt::uminus ");
    gen->indent++;
    if (!compile_expression(gen,n,expr->left))
      return 0;
    gen->indent--;
    gen_printf(gen,")");
    break;
  case XPATH_UNARY_PLUS:
    gen_iprintf(gen,"(xslt::uplus ");
    gen->indent++;
    if (!compile_expression(gen,n,expr->left))
      return 0;
    gen->indent--;
    gen_printf(gen,")");
    break;
  case XPATH_PAREN:
    return compile_expression(gen,n,expr->left);
  case XPATH_EMPTY:
    gen_printf(gen,"nil");
    break;
  case XPATH_VAR_REF: {
    char *ident = nsname_to_ident(expr->qn.uri,expr->qn.localpart);
    gen_printf(gen,"%s",ident);
    free(ident);
    break;
  }
  case XPATH_FILTER:
    gen_printorig(gen,"predicate",NULL);
    gen_iprintf(gen,"(xslt::filter3");
    gen->indent++;
    gen_iprintf(gen,"(!citem.!cpos.!csize.xslt::predicate_match cpos ");
    if (!compile_expression(gen,n,expr->right))
      return 0;
    gen_printf(gen,") ");
    if (!compile_expression(gen,n,expr->left))
      return 0;
    gen_printf(gen,")");
    gen->indent--;
    break;
  case XPATH_STEP:
    gen_printorig(gen,"path step",NULL);
    gen_iprintf(gen,"/* left = %d, right = %d */ ",expr->left->type,expr->right->type);

    if (is_expr_doc_order(n,expr->left) && is_expr_doc_order(n,expr->right))
      gen_iprintf(gen,"(xslt::path_result");
    else
      gen_iprintf(gen,"(xslt::path_result_sort");
    gen->indent++;
    gen_iprintf(gen,"(xslt::apmap3 (!citem.!cpos.!csize.");
    gen->indent++;
    if (!compile_expression(gen,n,expr->right))
      return 0;
    gen_printf(gen,") ");
    if (!compile_expression(gen,n,expr->left))
      return 0;
    gen->indent--;
    gen_printf(gen,")");
    gen_printf(gen,")");
    gen->indent--;
    break;
  case XPATH_FORWARD_AXIS_STEP:
    return compile_expression(gen,n,expr->left);
  case XPATH_REVERSE_AXIS_STEP:
    gen_iprintf(gen,"(reverse ");
    gen->indent++;
    if (!compile_expression(gen,n,expr->left))
      return 0;
    gen->indent--;
    gen_printf(gen,")");
    break;
  case XPATH_KIND_TEST:
  case XPATH_NAME_TEST:
    gen_iprintf(gen,"(filter");
    gen->indent++;
    if (XPATH_KIND_TEST == expr->type) {
      switch (expr->kind) {
      case KIND_DOCUMENT:
        gen_iprintf(gen,"(xslt::type_test xml::TYPE_DOCUMENT)");
        break;
      case KIND_ELEMENT:
        gen_iprintf(gen,"(xslt::type_test xml::TYPE_ELEMENT)");
        break;
      case KIND_ATTRIBUTE:
        gen_iprintf(gen,"(xslt::type_test xml::TYPE_ATTRIBUTE)");
        break;
      case KIND_SCHEMA_ELEMENT:
        return gen_error(gen,"Schema element tests not supported");
      case KIND_SCHEMA_ATTRIBUTE:
        return gen_error(gen,"Schema attribute tests not supported");
      case KIND_PI:
        return gen_error(gen,"Processing instruction tests not supported");
        break;
      case KIND_COMMENT:
        gen_iprintf(gen,"(xslt::type_test xml::TYPE_COMMENT)");
        break;
      case KIND_TEXT:
        gen_iprintf(gen,"(xslt::type_test xml::TYPE_TEXT)");
        break;
      case KIND_ANY:
        gen_iprintf(gen,"xslt::any_test");
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
        gen_iprintf(gen,"(xslt::name_test %s \"%s\" \"%s\")",type,nsuri,localname);
      else if (localname)
        gen_iprintf(gen,"(xslt::wildcard_uri_test %s \"%s\")",type,localname);
      else if (nsuri)
        gen_iprintf(gen,"(xslt::wildcard_localname_test %s \"%s\")",type,nsuri);
      else
        gen_iprintf(gen,"(xslt::type_test %s)",type);
      free(nsuri);
      free(localname);
    }

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
    if (strcmp(expr->qn.prefix,"")) {

      if (have_user_function(gen,expr->qn)) {
        char *ident = nsname_to_ident(expr->qn.uri,expr->qn.localpart);
        expression *p;
        gen_iprintf(gen,"(%s",ident);
        gen->indent++;
        for (p = expr->left; p; p = p->right) {
          assert(XPATH_ACTUAL_PARAM == p->type);
          gen_printf(gen," ");
          if (!compile_expression(gen,n,p->left))
            return 0;
        }
        gen->indent--;
        gen_printf(gen,")");
        free(ident);
      }
      else if (!strncmp(expr->qn.uri,"wsdl-",5)) {
        return compile_ws_call(gen,n,expr,expr->qn.uri+5);
      }
      else {
        return gen_error(gen,"Call to non-existent function {%s}%s",
                         expr->qn.uri ? expr->qn.uri : "",expr->qn.localpart);
      }
    }
    else {
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
          if (!compile_expression(gen,n,p->left))
            r = 0;
        }
        gen->indent--;
        gen_printf(gen,")");
        free(funname);
        return r;
      }
    }
    break;
  case XPATH_ROOT:
    gen_printf(gen,"(cons (xml::item_root citem) nil)");
    break;
  default:
    return gen_error(gen,"Unsupported expression type: %d",expr->type);
  }
  return 1;
}

static pthread_mutex_t xpath_lock = PTHREAD_MUTEX_INITIALIZER;

int parse_firstline = 0;
expression *parse_expr = NULL;
xmlNodePtr parse_node = NULL;

static expression *parse_xpath(elcgen *gen, xmlNodePtr n, const char *str)
{
  X_BUFFER_STATE bufstate;
  int r;
  expression *expr;

  pthread_mutex_lock(&xpath_lock);

  parse_node = n;
  parse_firstline = n->line;
  parse_expr = NULL;
  lex_lineno = 0;

  bufstate = x_scan_string(str);
  x_switch_to_buffer(bufstate);

  r = xparse();

  x_delete_buffer(bufstate);
#if HAVE_XLEX_DESTROY
  xlex_destroy();
#endif

  if (0 != r) {
    gen_error(gen,"XPath parse error");
    pthread_mutex_unlock(&xpath_lock);
    return NULL;
  }

  expr = parse_expr;
  parse_expr = NULL;

  pthread_mutex_unlock(&xpath_lock);

  if (NULL == expr)
    gen_error(gen,"XPath parse returned NULL expr");

  return expr;
}

static int compile_expr_string(elcgen *gen, xmlNodePtr n, const char *str)
{
  int r = 0;
  expression *expr = parse_xpath(gen,n,str);
  if (expr) {
    r = compile_expression(gen,n,expr);
    free_expression(expr);
  }
  return r;
}

static int compile_avt_component(elcgen *gen, xmlNodePtr n, expression *expr)
{
  if (XPATH_STRING_LITERAL == expr->type) {
    char *esc = escape1(expr->str);
    gen_printf(gen,"\"%s\" ",esc);
    free(esc);
  }
  else {
    gen_printf(gen,"(xslt::consimple ");
    if (!compile_expression(gen,n,expr))
      return 0;
    gen_printf(gen,") ");
  }
  return 1;
}

static int compile_avt(elcgen *gen, xmlNodePtr n, const char *str)
{
  char *tmp = (char*)malloc(strlen(str)+3);
  expression *expr;
  expression *cur;
  int count = 0;
  int r = 1;
  sprintf(tmp,"}%s{",str);

  cur = expr = parse_xpath(gen,n,tmp);
  if (NULL == expr) {
    free(tmp);
    return 0;
  }

  while (r && (XPATH_AVT_COMPONENT == cur->type)) {
    gen_printf(gen,"(append ");
    if (!compile_avt_component(gen,n,cur->left))
      r = 0;

    count++;
    cur = cur->right;
  }

  if (r && !compile_avt_component(gen,n,cur))
    r = 0;

  while (0 < count--)
    gen_printf(gen,")");

  free_expression(expr);
  free(tmp);
  return r;
}

static int compile_attributes(elcgen *gen, xmlNodePtr n)
{
  xmlAttrPtr attr;
  int count = 0;
  int r = 1;

  for (attr = n->properties; attr && r; attr = attr->next) {
    char *value;
    if (attr->ns)
      gen_printf(gen,"(cons (xml::mkattr nil nil nil nil \"%s\" \"%s\" \"%s\" ",
             attr->ns->href,attr->ns->prefix,attr->name);
    else
      gen_printf(gen,"(cons (xml::mkattr nil nil nil nil nil nil \"%s\" ",attr->name);

    value = xmlNodeListGetString(gen->parse_doc,attr->children,1);
    gen_printf(gen,"//xxx value: %s\n",value);
    r = compile_avt(gen,n,value);
    free(value);

    gen_printf(gen,")\n");

    count++;
  }
  gen_printf(gen,"nil");
  while (0 < count--)
    gen_printf(gen,")");
  return r;
}

static int compile_namespaces(elcgen *gen, xmlNodePtr n)
{
  xmlNsPtr ns;
  int count = 0;
  for (ns = n->nsDef; ns; ns = ns->next) {
    if (ns->prefix)
      gen_printf(gen,"(cons (xml::mknamespace \"%s\" \"%s\")\n",ns->href,ns->prefix);
    else
      gen_printf(gen,"(cons (xml::mknamespace \"%s\" nil)\n",ns->href);
    count++;
  }
  gen_printf(gen,"nil");
  while (0 < count--)
    gen_printf(gen,")");
  return 1;
}

/* FIXME: add a parameter saying where it will be used (e.g. complex content,
   simple content, boolean value) */
static int compile_instruction(elcgen *gen, xmlNodePtr n)
{
  int r = 1;
  switch (n->type) {
  case XML_ELEMENT_NODE: {
/*     xmlNodePtr child; */
    if (n->ns && !xmlStrcmp(n->ns->href,XSLT_NAMESPACE)) {
      if (!xmlStrcmp(n->name,"sequence")) {
        char *select = xmlGetProp(n,"select");
        int r;
        if (NULL == select)
          return gen_error(gen,"missing select attribute");
        gen_printorig(gen,"sequence",select);
        r = compile_expr_string(gen,n,select);
        free(select);
        return r;
      }
      else if (!xmlStrcmp(n->name,"value-of")) {
        char *select = xmlGetProp(n,"select");
        gen_iprintf(gen,"(xslt::construct_value_of ");
        if (select)
          r = compile_expr_string(gen,n,select);
        else
          r = compile_sequence(gen,n->children);
        gen_printf(gen,")");
        free(select);
        return r;
      }
      else if (!xmlStrcmp(n->name,"text")) {
        char *str = xmlNodeListGetString(gen->parse_doc,n->children,1);
        char *esc = escape1(str);
        gen_iprintf(gen,"(xslt::construct_text \"%s\")",esc);
        free(esc);
        free(str);
      }
      else if (!xmlStrcmp(n->name,"for-each")) {
        char *select = xmlGetProp(n,"select");
        if (NULL == select)
          return gen_error(gen,"missing select attribute");

        gen_printorig(gen,"for-each",select);
        gen_iprintf(gen,"(xslt::foreach3 ");
        gen->indent++;
        if (!compile_expr_string(gen,n,select))
          r = 0;
        gen_iprintf(gen,"(!citem.!cpos.!csize.");
        gen->indent++;
        if (r && !compile_sequence(gen,n->children))
          r = 0;
        gen_printf(gen,"))");
        gen->indent--;
        gen->indent--;

        free(select);
        return r;
      }
      else if (!xmlStrcmp(n->name,"if")) {
        char *test = xmlGetProp(n,"test");
        if (NULL == test)
          return gen_error(gen,"missing test attribute");
        gen_printorig(gen,"if",test);
        gen_iprintf(gen,"(if (xslt::ebv ");
        gen->indent++;
        if (!compile_expr_string(gen,n,test))
          r = 0;
        gen_printf(gen,") ");
        if (r && !compile_sequence(gen,n->children))
          r = 0;
        gen_iprintf(gen,"nil)");
        gen->indent--;
        free(test);
        return r;
      }
      else if (!xmlStrcmp(n->name,"choose")) {
        xmlNodePtr child;
        int count = 0;
        int otherwise = 0;

        gen_printorig(gen,"choose",NULL);
        for (child = n->children; child && r; child = child->next) {
          if (is_element(child,XSLT_NAMESPACE,"when")) {
            // when
            char *test = xmlGetProp(child,"test");
            if (NULL == test)
              return gen_error(gen,"missing test attribute");

            gen_printorig(gen,"when",test);

            gen_iprintf(gen,"(if (xslt::ebv ");
            gen->indent++;
            if (!compile_expr_string(gen,child,test))
              r = 0;
            gen_printf(gen,") ");
            if (r && !compile_sequence(gen,child->children))
              r = 0;
            gen_printf(gen," ");
            gen->indent--;
            free(test);
            count++;
          }
          else if (is_element(child,XSLT_NAMESPACE,"otherwise")) {
            // otherwise
            gen_printorig(gen,"otherwise",NULL);

            if (!compile_sequence(gen,child->children))
              r = 0;
            otherwise = 1;
          }
        }

        if (!otherwise)
          gen_printf(gen,"nil");

        while (0 < count--)
          gen_printf(gen,")");
        return r;
      }
      else if (!xmlStrcmp(n->name,"element")) {
        /* FIXME: complete this, and handle namespaces properly */
        char *name = xmlGetProp(n,"name");
        if (NULL == name)
          return gen_error(gen,"missing name attribute");

        gen_printorig(gen,"element",name);
        gen_iprintf(gen,"(xslt::construct_elem nil nil ");
        gen->indent++;
        if (!compile_avt(gen,n,name))
          r = 0;
        gen_printf(gen," nil nil ");
        if (r && !compile_sequence(gen,n->children))
          r = 0;
        gen->indent--;
        gen_printf(gen,")");
        free(name);
        return r;
      }
      else if (!xmlStrcmp(n->name,"attribute")) {
        /* FIXME: handle namespaces properly */
        char *select;
        char *name = xmlGetProp(n,"name");
        if (NULL == name)
          return gen_error(gen,"missing name attribute");
        select = xmlGetProp(n,"select");

        gen_printorig(gen,"attribute",name);
        gen_iprintf(gen,"(cons (xml::mkattr nil nil nil nil ");
        gen_printf(gen,"nil "); // nsuri
        gen_printf(gen,"nil "); // nsprefix

        gen->indent++;
        // localname
        r = compile_avt(gen,n,name);
        if (r) {
          // value
          gen_printf(gen,"(xslt::consimple ");
          if (select)
            r = compile_expr_string(gen,n,select);
          else
            r = compile_sequence(gen,n->children);
          gen_printf(gen,")");
          gen_printf(gen,") nil)");
        }
        gen->indent--;
        free(name);
        free(select);
        return r;
      }
      else {
        return gen_error(gen,"Unsupported XSLT instruction: %s",n->name);
      }
    }
    else {
      gen_iprintf(gen,"(xslt::construct_elem");
      if (n->ns)
        gen_printf(gen," \"%s\" \"%s\" \"%s\" ",n->ns->href,n->ns->prefix,n->name);
      else
        gen_printf(gen," nil nil \"%s\" ",n->name);
      gen->indent++;
      r = compile_attributes(gen,n);
      gen_printf(gen," ");
      r = r && compile_namespaces(gen,n);
      gen_printf(gen," ");
      r = r && compile_sequence(gen,n->children);
      gen->indent--;
      gen_printf(gen,")");
      return r;
    }
    break;
  }
  case XML_TEXT_NODE: {
    char *esc = escape1((const char*)n->content);
    gen_iprintf(gen,"(xslt::construct_text \"%s\")",esc);
    free(esc);
    break;
  }
  case XML_COMMENT_NODE: {
/*     gen_printf(gen,"???comment\n"); */
    gen_printf(gen,"nil");
    break;
  }
  default:
    return gen_error(gen,"Encountered unexpected element type %d at line %d",
                     n->type,n->line);
    break;
  }
  return 1;
}

static int compile_sequence(elcgen *gen, xmlNodePtr first)
{
  xmlNodePtr child;
  int count = 0;
  int nchildren = 0;
  int r = 1;
  for (child = first; child; child = child->next)
    if (!ignore_node(child))
      nchildren++;

  if (0 == nchildren) {
    gen_printf(gen,"nil");
    return 1;
  }

  if (1 == nchildren) {
    child = first;
    while (ignore_node(child))
      child = child->next;
    return compile_instruction(gen,child);
  }

  for (child = first; child && r; child = child->next) {
    if (is_element(child,XSLT_NAMESPACE,"variable")) {
      /* FIXME: support top-level variables as well */
      char *select;
      char *ident;
      qname qn;
      char *name = xmlGetProp(child,"name");
      if (NULL == name)
        return gen_error(gen,"missing name attribute");
      select = xmlGetProp(child,"select");

      qn = string_to_qname(name,child);
      ident = nsname_to_ident(qn.uri,qn.localpart);

      gen_iprintf(gen,"(letrec %s = ",ident);
      gen->indent++;
      if (select) {
        r = compile_expr_string(gen,child,select);
      }
      else {
        gen_printf(gen,"(cons (xslt::copydoc (!x.x) (xml::mkdoc ");
        r = compile_sequence(gen,child->children);
        gen_printf(gen,")) nil)");
      }
      gen->indent--;
      gen_iprintf(gen," in ");

      free(ident);
      free_qname(qn);
      free(name);
      free(select);

      count++;
    }
    else if (!ignore_node(child)) {
      gen_printf(gen,"(append ");
      if (!compile_instruction(gen,child))
        r = 0;
      gen_printf(gen," ");
      count++;
    }
  }
  gen_printf(gen,"nil");
  while (0 < count--)
    gen_printf(gen,")");
  return r;
}

static int compile_template(elcgen *gen, xmlNodePtr n)
{
  int r;
  gen_printf(gen,"\n");
  gen_printf(gen,"top citem cpos csize = ");
  r = compile_sequence(gen,n->children);
  gen_printf(gen,"\n");
  gen_printf(gen,"\n");
  return r;
}

static int compile_function(elcgen *gen, xmlNodePtr child)
{
  char *ident;
  xmlNodePtr pn;
  int r;
  qname fqn;
  char *name = xmlGetProp(child,"name");
  if (NULL == name)
    return gen_error(gen,"missing name attribute");
  fqn = string_to_qname(name,child);

  if (NULL == fqn.localpart) {
    free_qname(fqn);
    return gen_error(gen,"XTSE0740: function must have a prefixed name");
  }

  gen_printf(gen,"\n");

  ident = nsname_to_ident(fqn.uri,fqn.localpart);
  gen_printf(gen,"%s",ident);
  free(ident);

  for (pn = child->children; pn; pn = pn->next) {
    if (XML_ELEMENT_NODE == pn->type) {
      if (!is_element(pn,XSLT_NAMESPACE,"param"))
        break;
      else if (!xmlHasProp(pn,"name")) {
        return gen_error(gen,"missing name attribute");
      }
      else {
        qname pqn = get_qname_attr(pn,"name");
        char *pn_ident = nsname_to_ident(pqn.uri,pqn.localpart);
        gen_printf(gen," %s",pn_ident);
        free(pn_ident);
        free_qname(pqn);
      }
    }
  }

  gen_printf(gen," = ");
  gen_iprintf(gen,"(xslt::reparent ");
  gen->indent++;
  r = compile_sequence(gen,pn);
  gen->indent--;
  gen_iprintf(gen,"  nil nil nil)\n");

  free_qname(fqn);
  free(name);
  return r;
}

static int process_root(elcgen *gen, xmlNodePtr n)
{
  xmlNodePtr child;
  int r = 1;
  if ((XML_ELEMENT_NODE != n->type) ||
      (NULL == n->ns) || xmlStrcmp(n->ns->href,XSLT_NAMESPACE) ||
      (xmlStrcmp(n->name,"stylesheet") && xmlStrcmp(n->name,"transform")))
    return gen_error(gen,"top-level element must be a xsl:stylesheet or xsl:transform");

  gen->toplevel = n;
  for (child = n->children; child && r; child = child->next) {
    if (is_element(child,XSLT_NAMESPACE,"template"))
      r = compile_template(gen,child);
    else if (is_element(child,XSLT_NAMESPACE,"function"))
      r = compile_function(gen,child);
    else if (is_element(child,XSLT_NAMESPACE,"output") && attr_equals(child,"indent","yes"))
      gen->option_indent = 1;
    else if (is_element(child,XSLT_NAMESPACE,"strip-space") && attr_equals(child,"elements","*"))
      gen->option_strip = 1;
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
    printf("/*\n\n");
    if (process_root(gen,root)) {
      printf("\n*/\n");

      gen_printf(gen,"STRIPALL = %s\n",gen->option_strip ? "1" : "nil");
      gen_printf(gen,"INDENT = %s\n",gen->option_indent ? "1" : "nil");
      gen_printf(gen,
                   "main args stdin =\n"
                   "(letrec\n"
                   "  input =\n"
                   "    (if (== (len args) 0)\n"
                   "      (if stdin\n"
                   "        (xml::parsexml STRIPALL stdin)\n"
                   "        (xml::mkdoc nil))\n"
                   "      (xml::parsexml STRIPALL (readb (head args))))\n"
                   "  result = (top input 1 1)\n"
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
  free(gen);

  return r;
}
