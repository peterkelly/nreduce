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
static int compile_expression(elcgen *gen, expression *expr);

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
      *(p++) = '\"';
    }
    else if ('\n' == *c) {
      *(p++) = '\\';
      *(p++) = '\n';
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

static int compile_binary(elcgen *gen, expression *expr, const char *fun)
{
  array_printf(gen->buf,"(%s\n",fun);
  if (!compile_expression(gen,expr->left))
    return 0;
  array_printf(gen->buf,"\n");
  if (!compile_expression(gen,expr->right))
    return 0;
  array_printf(gen->buf,")");
  return 1;
}

static int compile_post(elcgen *gen, expression *expr, const char *service_url,
                        qname inelem, qname outelem,
                        list *inargs, list *outargs)
{
  list *l;
  expression *p;
  int r = 1;

  array_printf(gen->buf,"(letrec requestxml = ");
  array_printf(gen->buf,"(cons (xml::mkelem nil nil nil nil \""SOAPENV_NAMESPACE
               "\" \"soapenv\" \"Envelope\" nil \n");
  array_printf(gen->buf,"(cons (xml::mknamespace \""SOAPENV_NAMESPACE
               "\" \"soapenv\") nil)\n");
  array_printf(gen->buf,"(cons (xml::mkelem nil nil nil nil \""SOAPENV_NAMESPACE
               "\" \"soapenv\" \"Body\" nil nil\n");

  array_printf(gen->buf,"(cons (xml::mkelem nil nil nil nil \"%s\" \"operation\" \"%s\" nil "
               "(cons (xml::mknamespace \"%s\" \"operation\") nil) \n",
               inelem.uri,inelem.localpart,inelem.uri);

  //printf("nil");
  l = inargs;
  for (p = expr->left; p && r; p = p->right) {
    wsarg *argname = (wsarg*)l->data;
    assert(XPATH_ACTUAL_PARAM == p->type);
    /* FIXME: we should really use a different structure here, instead of using prefix to
       distinguish between array and normal args */
    if (argname->list) {
      printf("%s: compiling as array argument\n",argname->localpart);
      array_printf(gen->buf,"(append (map (!x.");
      array_printf(gen->buf,"(xml::mkelem nil nil nil nil \"%s\" nil \"%s\" nil nil ",
                   argname->uri,argname->localpart);
      array_printf(gen->buf,"(xml::item_children x))) ");
      array_printf(gen->buf,"(filter (!x.== (xml::item_type x) xml::TYPE_ELEMENT) ");
      array_printf(gen->buf,"(xslt::concomplex (xslt::get_children ");
      if (!compile_expression(gen,p->left))
        r = 0;
      array_printf(gen->buf,")))) ");
    }
    else {
      array_printf(gen->buf,"(cons (xml::mkelem nil nil nil nil \"%s\" nil \"%s\" nil nil ",
                   argname->uri,argname->localpart);
      array_printf(gen->buf,"(xslt::concomplex (xslt::get_children ");
      if (!compile_expression(gen,p->left))
        r = 0;
      array_printf(gen->buf,"))) \n");
    }
    l = l->next;
  }
  array_printf(gen->buf,"nil");
  for (p = expr->left; p; p = p->right)
    array_printf(gen->buf,")");

  array_printf(gen->buf,") nil)");
  array_printf(gen->buf,") nil)) nil)");
  array_printf(gen->buf,"request = (xslt::output nil requestxml)");
  array_printf(gen->buf,"response = (xslt::post \"%s\" request)",service_url);
  array_printf(gen->buf,"responsedoc = (xml::parsexml 1 response)");
  array_printf(gen->buf,"topelems = (xml::item_children responsedoc)");

  array_printf(gen->buf,"bodies = (xslt::apmap3 (!citem.!cpos.!csize.\n");
  array_printf(gen->buf,"(filter (xslt::name_test xml::TYPE_ELEMENT \"%s\" \"%s\") "
         "(xml::item_children citem))",SOAPENV_NAMESPACE,"Body");
  array_printf(gen->buf,") topelems)");

  array_printf(gen->buf,"respelems = (xslt::apmap3 (!citem.!cpos.!csize.\n");
  array_printf(gen->buf,"(filter (xslt::name_test xml::TYPE_ELEMENT \"%s\" \"%s\") "
         "(xml::item_children citem))",outelem.uri,outelem.localpart);
  array_printf(gen->buf,") bodies)");

  array_printf(gen->buf,"returns = (apmap xml::item_children respelems)");

  if (((wsarg*)outargs->data)->simple)
    array_printf(gen->buf,"in (apmap xml::item_children returns))");
  else
    array_printf(gen->buf,"in returns)");
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

static int compile_ws_call(elcgen *gen, expression *expr, const char *wsdlurl)
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
    r = compile_post(gen,expr,service_url,inelem,outelem,inargs,outargs);

  free(ident);
  free(inelem.uri);
  free(inelem.localpart);
  free(outelem.uri);
  free(outelem.localpart);
  list_free(inargs,free_wsarg_ptr);
  list_free(outargs,free_wsarg_ptr);
  return r;
}

static int compile_expression(elcgen *gen, expression *expr)
{
  switch (expr->type) {
  case XPATH_OR:         return compile_binary(gen,expr,"or"); /* FIXME: test */
  case XPATH_AND:        return compile_binary(gen,expr,"and"); /* FIXME: test */

  case XPATH_VALUE_EQ:   return compile_binary(gen,expr,"xslt::value_eq");
  case XPATH_VALUE_NE:   return compile_binary(gen,expr,"xslt::value_ne");
  case XPATH_VALUE_LT:   return compile_binary(gen,expr,"xslt::value_lt");
  case XPATH_VALUE_LE:   return compile_binary(gen,expr,"xslt::value_le");
  case XPATH_VALUE_GT:   return compile_binary(gen,expr,"xslt::value_gt");
  case XPATH_VALUE_GE:   return compile_binary(gen,expr,"xslt::value_ge");

  case XPATH_GENERAL_EQ: return compile_binary(gen,expr,"xslt::general_eq");
  case XPATH_GENERAL_NE: return compile_binary(gen,expr,"xslt::general_ne");
  case XPATH_GENERAL_LT: return compile_binary(gen,expr,"xslt::general_lt");
  case XPATH_GENERAL_LE: return compile_binary(gen,expr,"xslt::general_le");
  case XPATH_GENERAL_GT: return compile_binary(gen,expr,"xslt::general_gt");
  case XPATH_GENERAL_GE: return compile_binary(gen,expr,"xslt::general_ge");
  case XPATH_ADD:        return compile_binary(gen,expr,"xslt::add");
  case XPATH_SUBTRACT:   return compile_binary(gen,expr,"xslt::subtract");
  case XPATH_MULTIPLY:   return compile_binary(gen,expr,"xslt::multiply");
  case XPATH_DIVIDE:     return compile_binary(gen,expr,"xslt::divide");
  case XPATH_IDIVIDE:    return compile_binary(gen,expr,"xslt::idivide"); /* FIXME: test */
  case XPATH_MOD:        return compile_binary(gen,expr,"xslt::mod");
  case XPATH_INTEGER_LITERAL:
    array_printf(gen->buf,"(cons (xml::mknumber %f) nil)",expr->num);
    break;
  case XPATH_DECIMAL_LITERAL:
    array_printf(gen->buf,"(cons (xml::mknumber %f) nil)",expr->num);
    break;
  case XPATH_DOUBLE_LITERAL:
    array_printf(gen->buf,"(cons (xml::mknumber %f) nil)",expr->num);
    break;
  case XPATH_STRING_LITERAL: {
    char *esc = escape1(expr->str);
    array_printf(gen->buf,"(cons (xml::mkstring \"%s\") nil)",esc);
    free(esc);
    break;
  }
  case XPATH_TO: {
    array_printf(gen->buf,"(xslt::range\n");
    array_printf(gen->buf,"(xslt::getnumber ");
    if (!compile_expression(gen,expr->left))
      return 0;
    array_printf(gen->buf,")\n");
    array_printf(gen->buf,"(xslt::getnumber ");
    if (!compile_expression(gen,expr->right))
      return 0;
    array_printf(gen->buf,"))");
    break;
  }
  case XPATH_SEQUENCE:
    array_printf(gen->buf,"(append ");
    if (!compile_expression(gen,expr->left))
      return 0;
    array_printf(gen->buf,"\n");
    if (!compile_expression(gen,expr->right))
      return 0;
    array_printf(gen->buf,")");
    break;
  case XPATH_CONTEXT_ITEM:
    array_printf(gen->buf,"(cons citem nil)");
    break;
  case XPATH_UNARY_MINUS:
    array_printf(gen->buf,"(xslt::uminus ");
    if (!compile_expression(gen,expr->left))
      return 0;
    array_printf(gen->buf,")");
    break;
  case XPATH_UNARY_PLUS:
    array_printf(gen->buf,"(xslt::uplus ");
    if (!compile_expression(gen,expr->left))
      return 0;
    array_printf(gen->buf,")");
    break;
  case XPATH_PAREN:
    return compile_expression(gen,expr->left);
  case XPATH_EMPTY:
    array_printf(gen->buf,"nil");
    break;
  case XPATH_VAR_REF: {
    char *ident = nsname_to_ident(expr->qn.uri,expr->qn.localpart);
    array_printf(gen->buf,"%s",ident);
    free(ident);
    break;
  }
  case XPATH_FILTER:
    array_printf(gen->buf,"(xslt::filter3\n(!citem.!cpos.!csize.xslt::predicate_match cpos ");
    if (!compile_expression(gen,expr->right))
      return 0;
    array_printf(gen->buf,") ");
    if (!compile_expression(gen,expr->left))
      return 0;
    array_printf(gen->buf,")");
    break;
  case XPATH_STEP:
    array_printf(gen->buf,"\n// step\n");

    array_printf(gen->buf,"(xslt::path_result ");
    array_printf(gen->buf,"(xslt::apmap3 (!citem.!cpos.!csize.\n");
    if (!compile_expression(gen,expr->right))
      return 0;
    array_printf(gen->buf,") ");
    if (!compile_expression(gen,expr->left))
      return 0;
    array_printf(gen->buf,")");
    array_printf(gen->buf,")");
    break;
  case XPATH_FORWARD_AXIS_STEP:
    return compile_expression(gen,expr->left);
  case XPATH_REVERSE_AXIS_STEP:
    array_printf(gen->buf,"(reverse ");
    if (!compile_expression(gen,expr->left))
      return 0;
    array_printf(gen->buf,")");
    break;
  case XPATH_KIND_TEST:
  case XPATH_NAME_TEST: {

    array_printf(gen->buf,"(filter ");
    if (XPATH_KIND_TEST == expr->type) {
      switch (expr->kind) {
      case KIND_DOCUMENT:
        array_printf(gen->buf,"(xslt::type_test xml::TYPE_DOCUMENT)");
        break;
      case KIND_ELEMENT:
        array_printf(gen->buf,"(xslt::type_test xml::TYPE_ELEMENT)");
        break;
      case KIND_ATTRIBUTE:
        array_printf(gen->buf,"(xslt::type_test xml::TYPE_ATTRIBUTE)");
        break;
      case KIND_SCHEMA_ELEMENT:
        return gen_error(gen,"Schema element tests not supported");
      case KIND_SCHEMA_ATTRIBUTE:
        return gen_error(gen,"Schema attribute tests not supported");
      case KIND_PI:
        return gen_error(gen,"Processing instruction tests not supported");
        break;
      case KIND_COMMENT:
        array_printf(gen->buf,"(xslt::type_test xml::TYPE_COMMENT)");
        break;
      case KIND_TEXT:
        array_printf(gen->buf,"(xslt::type_test xml::TYPE_TEXT)");
        break;
      case KIND_ANY:
        array_printf(gen->buf,"xslt::any_test");
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
        array_printf(gen->buf,"(xslt::name_test %s \"%s\" \"%s\")",type,nsuri,localname);
      else if (localname)
        array_printf(gen->buf,"(xslt::wildcard_uri_test %s \"%s\")",type,localname);
      else if (nsuri)
        array_printf(gen->buf,"(xslt::wildcard_localname_test %s \"%s\")",type,nsuri);
      else
        array_printf(gen->buf,"(xslt::type_test %s)",type);
      free(nsuri);
      free(localname);
    }

    array_printf(gen->buf,"\n");
    switch (expr->axis) {
    case AXIS_SELF:
      array_printf(gen->buf,"(cons citem nil)");
      break;
    case AXIS_CHILD:
      array_printf(gen->buf,"(xml::item_children citem)");
      break;
    case AXIS_DESCENDANT:
      array_printf(gen->buf,"(xslt::node_descendants citem)");
      break;
    case AXIS_DESCENDANT_OR_SELF:
      array_printf(gen->buf,"(cons citem (xslt::node_descendants citem))");
      break;
    case AXIS_PARENT:
      array_printf(gen->buf,"(xslt::node_parent_list citem)");
      break;
    case AXIS_ANCESTOR:
      array_printf(gen->buf,"(xslt::node_ancestors citem)");
      break;
    case AXIS_ANCESTOR_OR_SELF:
      array_printf(gen->buf,"(xslt::node_ancestors_or_self citem)");
      break;
    case AXIS_PRECEDING_SIBLING:
      array_printf(gen->buf,"(xslt::node_preceding_siblings citem)");
      break;
    case AXIS_FOLLOWING_SIBLING:
      array_printf(gen->buf,"(xslt::node_following_siblings citem)");
      break;
    case AXIS_PRECEDING:
      array_printf(gen->buf,"(xslt::node_preceding citem)");
      break;
    case AXIS_FOLLOWING:
      array_printf(gen->buf,"(xslt::node_following citem)");
      break;
    case AXIS_ATTRIBUTE:
      array_printf(gen->buf,"(xml::item_attributes citem)");
      break;
    case AXIS_NAMESPACE:
      array_printf(gen->buf,"(xml::item_namespaces citem)");
      break;
    default:
      assert(!"unsupported axis");
      break;
    }
    array_printf(gen->buf,")");

    break;
  }
  case XPATH_FUNCTION_CALL:
    if (strcmp(expr->qn.prefix,"")) {

      if (have_user_function(gen,expr->qn)) {
        char *ident = nsname_to_ident(expr->qn.uri,expr->qn.localpart);
        expression *p;
        array_printf(gen->buf,"(%s",ident);

        for (p = expr->left; p; p = p->right) {
          assert(XPATH_ACTUAL_PARAM == p->type);
          array_printf(gen->buf," ");
          if (!compile_expression(gen,p->left))
            return 0;
        }
        array_printf(gen->buf,")");
        free(ident);
      }
      else if (!strncmp(expr->qn.uri,"wsdl-",5)) {
        return compile_ws_call(gen,expr,expr->qn.uri+5);
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
        array_printf(gen->buf,"(xslt::fn_string0 citem)");
      }
      else if (!strcmp(expr->qn.localpart,"string-length") && (0 == nargs)) {
        array_printf(gen->buf,"(xslt::fn_string_length0 citem)");
      }
      else if (!strcmp(expr->qn.localpart,"position")) {
        array_printf(gen->buf,"(cons (xml::mknumber cpos) nil)");
      }
      else if (!strcmp(expr->qn.localpart,"last")) {
        array_printf(gen->buf,"(cons (xml::mknumber csize) nil)");
      }
      else {
        char *funname = strdup(expr->qn.localpart);
        char *c;
        int r = 1;

        for (c = funname; '\0' != *c; c++)
          if ('-' == *c)
            *c = '_';

        array_printf(gen->buf,"(xslt::fn_%s",funname);

        for (p = expr->left; p && r; p = p->right) {
          assert(XPATH_ACTUAL_PARAM == p->type);
          array_printf(gen->buf," ");
          if (!compile_expression(gen,p->left))
            r = 0;
        }
        array_printf(gen->buf,")");
        free(funname);
        return r;
      }
    }
    break;
  case XPATH_ROOT:
    /* FIXME */
    return gen_error(gen,"Unsupported expression type: root",expr->type);
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
    r = compile_expression(gen,expr);
    free_expression(expr);
  }
  return r;
}

static int compile_avt_component(elcgen *gen, expression *expr)
{
  if (XPATH_STRING_LITERAL == expr->type) {
    char *esc = escape1(expr->str);
    array_printf(gen->buf,"\"%s\"",esc);
    free(esc);
  }
  else {
    array_printf(gen->buf,"(xslt::consimple ");
    if (!compile_expression(gen,expr))
      return 0;
    array_printf(gen->buf,")");
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
    array_printf(gen->buf,"(append ");
    if (!compile_avt_component(gen,cur->left))
      r = 0;

    count++;
    cur = cur->right;
  }

  if (r && !compile_avt_component(gen,cur))
    r = 0;

  while (0 < count--)
    array_printf(gen->buf,")");

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
      array_printf(gen->buf,"(cons (xml::mkattr nil nil nil nil \"%s\" \"%s\" \"%s\" ",
             attr->ns->href,attr->ns->prefix,attr->name);
    else
      array_printf(gen->buf,"(cons (xml::mkattr nil nil nil nil nil nil \"%s\" ",attr->name);

    value = xmlNodeListGetString(gen->parse_doc,attr->children,1);
    array_printf(gen->buf,"//xxx value: %s\n",value);
    r = compile_avt(gen,n,value);
    free(value);

    array_printf(gen->buf,")\n");

    count++;
  }
  array_printf(gen->buf,"nil");
  while (0 < count--)
    array_printf(gen->buf,")");
  return r;
}

static int compile_namespaces(elcgen *gen, xmlNodePtr n)
{
  xmlNsPtr ns;
  int count = 0;
  for (ns = n->nsDef; ns; ns = ns->next) {
    if (ns->prefix)
      array_printf(gen->buf,"(cons (xml::mknamespace \"%s\" \"%s\")\n",ns->href,ns->prefix);
    else
      array_printf(gen->buf,"(cons (xml::mknamespace \"%s\" nil)\n",ns->href);
    count++;
  }
  array_printf(gen->buf,"nil");
  while (0 < count--)
    array_printf(gen->buf,")");
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
        array_printf(gen->buf,"\n// sequence %s\n",select);
        r = compile_expr_string(gen,n,select);
        free(select);
        return r;
      }
      else if (!xmlStrcmp(n->name,"value-of")) {
        char *select = xmlGetProp(n,"select");
        array_printf(gen->buf,"\n// value-of %s\n",select);
        array_printf(gen->buf,"(cons (xml::mktext (xslt::consimple ");
        if (select)
          r = compile_expr_string(gen,n,select);
        else
          r = compile_sequence(gen,n->children);
        array_printf(gen->buf,")) nil)\n");
        free(select);
        return r;
      }
      else if (!xmlStrcmp(n->name,"text")) {
        char *str = xmlNodeListGetString(gen->parse_doc,n->children,1);
        char *esc = escape1(str);
        array_printf(gen->buf,"(cons (xml::mktext \"%s\") nil)",esc);
        free(esc);
        free(str);
      }
      else if (!xmlStrcmp(n->name,"for-each")) {
        char *select = xmlGetProp(n,"select");
        if (NULL == select)
          return gen_error(gen,"missing select attribute");

        array_printf(gen->buf,"\n// for-each %s\n",select);
        array_printf(gen->buf,"(letrec\n");
        array_printf(gen->buf,"select = ");
        if (!compile_expr_string(gen,n,select))
          r = 0;
        array_printf(gen->buf,"\nin\n");

        array_printf(gen->buf,"(xslt::apmap3 (!citem.!cpos.!csize.\n");
        if (r && !compile_sequence(gen,n->children))
          r = 0;
        array_printf(gen->buf,")\n");

        array_printf(gen->buf,"select))\n");
        free(select);
        return r;
      }
      else if (!xmlStrcmp(n->name,"if")) {
        char *test = xmlGetProp(n,"test");
        if (NULL == test)
          return gen_error(gen,"missing test attribute");
        array_printf(gen->buf,"\n// if %s\n",test);
        array_printf(gen->buf,"(if\n");
        array_printf(gen->buf,"(xslt::ebv\n");
        if (!compile_expr_string(gen,n,test))
          r = 0;
        array_printf(gen->buf,")\n");
        if (r && !compile_sequence(gen,n->children))
          r = 0;
        array_printf(gen->buf,"\nnil)");
        free(test);
        return r;
      }
      else if (!xmlStrcmp(n->name,"choose")) {
        xmlNodePtr child;
        int count = 0;
        int otherwise = 0;

        array_printf(gen->buf,"\n// choose\n");
        for (child = n->children; child && r; child = child->next) {
          if (is_element(child,XSLT_NAMESPACE,"when")) {
            // when
            char *test = xmlGetProp(child,"test");
            if (NULL == test)
              return gen_error(gen,"missing test attribute");

            array_printf(gen->buf,"\n// when %s\n",test);

            array_printf(gen->buf,"(if (xslt::ebv ");
            if (!compile_expr_string(gen,child,test))
              r = 0;
            array_printf(gen->buf,") ");
            if (r && !compile_sequence(gen,child->children))
              r = 0;
            array_printf(gen->buf," ");
            free(test);
            count++;
          }
          else if (is_element(child,XSLT_NAMESPACE,"otherwise")) {
            // otherwise
            array_printf(gen->buf,"// otherwise\n");

            if (!compile_sequence(gen,child->children))
              r = 0;
            otherwise = 1;
          }
        }

        if (!otherwise)
          array_printf(gen->buf,"nil");

        while (0 < count--)
          array_printf(gen->buf,")");
        return r;
      }
      else if (!xmlStrcmp(n->name,"element")) {
        /* FIXME: complete this, and handle namespaces properly */
        /* FIXME: avoid reparenting - set the parents correctly in the first place */
        char *name = xmlGetProp(n,"name");
        if (NULL == name)
          return gen_error(gen,"missing name attribute");

        array_printf(gen->buf,"\n// element %s\n",name);

        array_printf(gen->buf,"(letrec\n");

        array_printf(gen->buf,"attrs = (xslt::get_attributes content)\n");
        array_printf(gen->buf,"namespaces = (xslt::get_namespaces content)\n");
        array_printf(gen->buf,"children2 = (xslt::concomplex (xslt::get_children content))\n");
        array_printf(gen->buf,"children = (xslt::reparent children2 elem nil)\n");
        array_printf(gen->buf,"name = ");
        if (!compile_avt(gen,n,name))
          r = 0;
        // array_printf(gen->buf,"\"%s\"",name);
        array_printf(gen->buf,"elem = (xml::mkelem nil nil nil nil nil nil name attrs namespaces children)\n");
        array_printf(gen->buf,"content =\n");
        if (r && !compile_sequence(gen,n->children))
          r = 0;
        array_printf(gen->buf,"\nin\n(cons elem nil))");
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

        array_printf(gen->buf,"\n// attribute %s\n",name);
        array_printf(gen->buf,"(cons (xml::mkattr nil nil nil nil ");
        array_printf(gen->buf," nil "); // nsuri
        array_printf(gen->buf," nil "); // nsprefix

        // localname
        r = compile_avt(gen,n,name);
        if (r) {
          // value
          array_printf(gen->buf,"(xslt::consimple ");
          if (select)
            r = compile_expr_string(gen,n,select);
          else
            r = compile_sequence(gen,n->children);
          array_printf(gen->buf,")");
          array_printf(gen->buf,") nil)");
        }
        free(name);
        free(select);
        return r;
      }
      else {
        return gen_error(gen,"Unsupported XSLT instruction: %s",n->name);
      }
    }
    else {
      /* FIXME: avoid reparenting - set the parents correctly in the first place */
      array_printf(gen->buf,"(letrec\n");

      array_printf(gen->buf,"attrs = (append ");
      if (!compile_attributes(gen,n))
        r = 0;
      array_printf(gen->buf," (xslt::get_attributes content))\n");

      array_printf(gen->buf,"namespaces = (append ");
      if (!compile_namespaces(gen,n))
        r = 0;
      array_printf(gen->buf," (xslt::get_namespaces content))\n");

      array_printf(gen->buf,"children2 = (xslt::concomplex (xslt::get_children content))\n");
      array_printf(gen->buf,"children = (xslt::reparent children2 elem nil)\n");

      if (n->ns) {
        array_printf(gen->buf,"elem = (xml::mkelem nil nil nil nil \"%s\" \"%s\" \"%s\" attrs namespaces children)\n",
                n->ns->href,n->ns->prefix,n->name);
      }
      else {
        array_printf(gen->buf,"elem = (xml::mkelem nil nil nil nil nil nil \"%s\" attrs namespaces children)\n",n->name);
      }
      array_printf(gen->buf,"content =\n");
      if (!compile_sequence(gen,n->children))
        r = 0;
      array_printf(gen->buf,"\nin\n(cons elem nil))");
      return r;
    }
/*     array_printf(gen->buf,"???element %s\n",n->name); */
    break;
  }
  case XML_TEXT_NODE: {
    char *esc = escape1((const char*)n->content);
    array_printf(gen->buf,"(cons (xml::mktext \"%s\") nil)",esc);
    free(esc);
    break;
  }
  case XML_COMMENT_NODE: {
/*     array_printf(gen->buf,"???comment\n"); */
    array_printf(gen->buf,"nil");
    break;
  }
  default:
    array_printf(gen->buf,"??? %d\n",n->type);
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
    array_printf(gen->buf,"nil");
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

      array_printf(gen->buf,"(letrec %s = ",ident);
      if (select) {
        r = compile_expr_string(gen,child,select);
      }
      else {
        array_printf(gen->buf,"(cons (xml::mkdoc ");
        r = compile_sequence(gen,child->children);
        array_printf(gen->buf,") nil)");
      }
      array_printf(gen->buf," in ");

      free(ident);
      free_qname(qn);
      free(name);
      free(select);

      count++;
    }
    else if (!ignore_node(child)) {
      array_printf(gen->buf,"(append ");
      if (!compile_instruction(gen,child))
        r = 0;
      array_printf(gen->buf," ");
      count++;
    }
  }
  array_printf(gen->buf,"nil");
  while (0 < count--)
    array_printf(gen->buf,")");
  return r;
}

static int compile_template(elcgen *gen, xmlNodePtr n)
{
  int r;
  array_printf(gen->buf,"\n");
  array_printf(gen->buf,"top citem cpos csize =\n");
  r = compile_sequence(gen,n->children);
  array_printf(gen->buf,"\n");
  array_printf(gen->buf,"\n");
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

  array_printf(gen->buf,"\n");

  ident = nsname_to_ident(fqn.uri,fqn.localpart);
  array_printf(gen->buf,"%s",ident);
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
        array_printf(gen->buf," %s",pn_ident);
        free(pn_ident);
        free_qname(pqn);
      }
    }
  }

  array_printf(gen->buf," =\n");
  r = compile_sequence(gen,pn);
  array_printf(gen->buf,"\n");
  array_printf(gen->buf,"\n");

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
    array_printf(gen->buf,"import xml\n");
    array_printf(gen->buf,"import xslt\n");

    gen->parse_doc = doc;
    gen->parse_filename = xslturl;
    root = xmlDocGetRootElement(doc);
    printf("/*\n\n");
    if (process_root(gen,root)) {
      printf("\n*/\n");

      array_printf(gen->buf,"STRIPALL = %s\n",gen->option_strip ? "1" : "nil");
      array_printf(gen->buf,"INDENT = %s\n",gen->option_indent ? "1" : "nil");
      array_printf(gen->buf,
                   "main args =\n"
                   "(letrec\n"
                   "  input =\n"
                   "    (if (== (len args) 0)\n"
                   "      (xml::mkdoc nil)\n"
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
