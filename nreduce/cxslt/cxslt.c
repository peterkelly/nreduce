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
/* FIXME: don't call exit() or fatal() from these routines -
   find another way to deal with the errors */

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

static void compile_sequence(elcgen *gen, xmlNodePtr first);
static void compile_expression(elcgen *gen, expression *expr);

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

static void check_attr(elcgen *gen, xmlNodePtr n, const char *value, const char *name)
{
  if (NULL == value) {
    fprintf(stderr,"%s:%d: Missing attribute: %s\n",gen->parse_filename,n->line,name);
    exit(1);
  }
}

static char *get_required_attr(elcgen *gen, xmlNodePtr n, const char *name)
{
  char *value = (char*)xmlGetProp(n,name);
  check_attr(gen,n,value,name);
  return value;
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

expression *new_TypeExpr(int type, seqtype *st, expression *right) { return NULL; }
expression *new_ForExpr(expression *left, expression *right) { return NULL; }
expression *new_VarInExpr(expression *left) { return NULL; }
expression *new_VarInListExpr(expression *left, expression *right) { return NULL; }
expression *new_QuantifiedExpr(int type, expression *left, expression *right) { return NULL; }
expression *new_XPathIfExpr(expression *cond, expression *tbranch, expression *fbranch) { return NULL; }
expression *new_RootExpr(expression *left) { return NULL; }

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

const char *lookup_nsuri(xmlNodePtr n, const char *prefix)
{
  xmlNs *ns;
/*   array_printf(gen->buf,"\n\nlooking up prefix \"%s\" in node %s\n",prefix,n->name); */
  for (; n && (XML_ELEMENT_NODE == n->type); n = n->parent) {
/*     array_printf(gen->buf,"node: %p %s\n",n,n->name); */
    for (ns = n->nsDef; ns; ns = ns->next) {
/*       array_printf(gen->buf,"ns: %s %s\n",ns->prefix,ns->href); */
      if (!xmlStrcmp(ns->prefix,prefix)) {
        return (const char*)ns->href;
      }
    }
  }

  if (!strcmp(prefix,""))
    return "";

  fprintf(stderr,"line %d: invalid namespace prefix \"%s\"\n",n->line,prefix);
  exit(1);
}

qname string_to_qname(const char *str, xmlNodePtr n)
{
  char *copy = strdup(str);
  char *colon = strchr(copy,':');
  qname qn;
  if (colon) {
    *colon = '\0';
    qn.uri = strdup(lookup_nsuri(n,copy));
    qn.prefix = strdup(copy);
    qn.localpart = strdup(colon+1);
  }
  else {
    qn.uri = strdup(lookup_nsuri(n,""));
    qn.prefix = strdup("");
    qn.localpart = strdup(copy);
  }
  free(copy);
  return qn;
}

void free_qname(qname qn)
{
  free(qn.uri);
  free(qn.prefix);
  free(qn.localpart);
}

void free_qname_ptr(void *qn)
{
  free_qname(*(qname*)qn);
  free(qn);
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
    if (c->ns &&
        !xmlStrcmp(c->ns->href,XSLT_NAMESPACE) &&
        !xmlStrcmp(c->name,"function")) {
      char *name = get_required_attr(gen,c,"name");
      qname funqn = string_to_qname(name,c);

      if (!strcmp(funqn.uri,qn.uri) && !strcmp(funqn.localpart,qn.localpart))
        have = 1;

      free_qname(funqn);
      free(name);

      if (have)
        return 1;
    }
  }
  return 0;
}

static void compile_binary(elcgen *gen, expression *expr, const char *fun)
{
  array_printf(gen->buf,"(%s\n",fun);
  compile_expression(gen,expr->left);
  array_printf(gen->buf,"\n");
  compile_expression(gen,expr->right);
  array_printf(gen->buf,")");
}

static void compile_ws_call(elcgen *gen, expression *expr, const char *wsdlurl)
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
  list *l;

#if 0
  printf("\n\n\n");
  printf("//////////////// compile_ws_call\n");
  printf("uri = %s\n",expr->qn.uri);
  printf("prefix = %s\n",expr->qn.prefix);
  printf("localpart = %s\n",expr->qn.localpart);
#endif

  wf = process_wsdl(gen,wsdlurl);

  if (!wsdl_get_url(gen,wf,&service_url)) {
    gen_error(gen,"%s: %s",wf->filename,gen->error);
    fatal("%s",gen->error);
  }

  if (!wsdl_get_operation_messages(gen,wf,expr->qn.localpart,&inelem,&outelem,&inargs,&outargs)) {
    gen_error(gen,"%s: %s",wf->filename,gen->error);
    fatal("%s",gen->error);
    /* FIXME: return here instead of using fatal */
  }

#if 1
  printf("service url = %s\n",service_url);
  printf("input element = {%s}%s\n",inelem.uri,inelem.localpart);
  printf("output element = {%s}%s\n",outelem.uri,outelem.localpart);

  printf("inargs =");
  for (l = inargs; l; l = l->next) {
    qname *qn = (qname*)l->data;
    printf(" {%s}%s",qn->uri,qn->localpart);
  }
  printf("\n");

  printf("outargs =");
  for (l = outargs; l; l = l->next) {
    qname *qn = (qname*)l->data;
    printf(" {%s}%s",qn->uri,qn->localpart);
  }
  printf("\n");
#endif

  supplied = 0;
  for (p = expr->left; p; p = p->right)
    supplied++;
  expected = list_count(inargs);

  if (supplied != expected)
    fatal("Incorrect # args for web service call %s: expected %d, got %d\n",
          expr->qn.localpart,expected,supplied);


  array_printf(gen->buf,"(letrec requestxml = ");
  array_printf(gen->buf,"(cons (xml::mkelem nil nil nil nil \""SOAPENV_NAMESPACE
               "\" \"soapenv\" \"Envelope\" nil ");
  array_printf(gen->buf,"(cons (xml::mknamespace \""SOAPENV_NAMESPACE
               "\" \"soapenv\") nil)");
  array_printf(gen->buf,"(cons (xml::mkelem nil nil nil nil \""SOAPENV_NAMESPACE
               "\" \"soapenv\" \"Body\" nil nil");

  array_printf(gen->buf,"(cons (xml::mkelem nil nil nil nil \"%s\" nil \"%s\" nil "
               "(cons (xml::mknamespace \"%s\" nil) nil) ",
               inelem.uri,inelem.localpart,inelem.uri);

  //printf("nil");
  l = inargs;
  for (p = expr->left; p; p = p->right) {
    qname *argname = (qname*)l->data;
    assert(XPATH_ACTUAL_PARAM == p->type);
    array_printf(gen->buf,"(cons (xml::mkelem nil nil nil nil \"%s\" nil \"%s\" nil ",
                 argname->uri,argname->localpart);
    array_printf(gen->buf,"(cons (xml::mknamespace \"%s\" \"\") nil)",argname->uri);
    array_printf(gen->buf,"(xslt::concomplex (xslt::get_children ");
    compile_expression(gen,p->left);
    array_printf(gen->buf,"))) ");
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

  array_printf(gen->buf,"returns = (xslt::apmap3 (!citem.!cpos.!csize.(xml::item_children citem)) respelems)");

  array_printf(gen->buf,"in returns)");



  free(ident);
  free(inelem.uri);
  free(inelem.localpart);
  free(outelem.uri);
  free(outelem.localpart);
  list_free(inargs,free_qname_ptr);
  list_free(outargs,free_qname_ptr);
}

static void compile_expression(elcgen *gen, expression *expr)
{
  switch (expr->type) {
  case XPATH_OR:         compile_binary(gen,expr,"or");                    break; /* FIXME: test */
  case XPATH_AND:        compile_binary(gen,expr,"and");                   break; /* FIXME: test */

  case XPATH_VALUE_EQ:   compile_binary(gen,expr,"xslt::value_eq");        break;
  case XPATH_VALUE_NE:   compile_binary(gen,expr,"xslt::value_ne");        break;
  case XPATH_VALUE_LT:   compile_binary(gen,expr,"xslt::value_lt");        break;
  case XPATH_VALUE_LE:   compile_binary(gen,expr,"xslt::value_le");        break;
  case XPATH_VALUE_GT:   compile_binary(gen,expr,"xslt::value_gt");        break;
  case XPATH_VALUE_GE:   compile_binary(gen,expr,"xslt::value_ge");        break;

  case XPATH_GENERAL_EQ: compile_binary(gen,expr,"xslt::general_eq");      break;
  case XPATH_GENERAL_NE: compile_binary(gen,expr,"xslt::general_ne");      break;
  case XPATH_GENERAL_LT: compile_binary(gen,expr,"xslt::general_lt");      break;
  case XPATH_GENERAL_LE: compile_binary(gen,expr,"xslt::general_le");      break;
  case XPATH_GENERAL_GT: compile_binary(gen,expr,"xslt::general_gt");      break;
  case XPATH_GENERAL_GE: compile_binary(gen,expr,"xslt::general_ge");      break;
  case XPATH_ADD:        compile_binary(gen,expr,"xslt::add");             break;
  case XPATH_SUBTRACT:   compile_binary(gen,expr,"xslt::subtract");        break;
  case XPATH_MULTIPLY:   compile_binary(gen,expr,"xslt::multiply");        break;
  case XPATH_DIVIDE:     compile_binary(gen,expr,"xslt::divide");          break;
  case XPATH_IDIVIDE:    compile_binary(gen,expr,"xslt::idivide");         break; /* FIXME: test */
  case XPATH_MOD:        compile_binary(gen,expr,"xslt::mod");             break;
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
    compile_expression(gen,expr->left);
    array_printf(gen->buf,")\n");
    array_printf(gen->buf,"(xslt::getnumber ");
    compile_expression(gen,expr->right);
    array_printf(gen->buf,"))");
    break;
  }
  case XPATH_SEQUENCE:
    array_printf(gen->buf,"(append ");
    compile_expression(gen,expr->left);
    array_printf(gen->buf,"\n");
    compile_expression(gen,expr->right);
    array_printf(gen->buf,")");
    break;
  case XPATH_CONTEXT_ITEM:
    array_printf(gen->buf,"(cons citem nil)");
    break;
  case XPATH_UNARY_MINUS:
    array_printf(gen->buf,"(xslt::uminus ");
    compile_expression(gen,expr->left);
    array_printf(gen->buf,")");
    break;
  case XPATH_UNARY_PLUS:
    array_printf(gen->buf,"(xslt::uplus ");
    compile_expression(gen,expr->left);
    array_printf(gen->buf,")");
    break;
  case XPATH_PAREN:
    compile_expression(gen,expr->left);
    break;
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
    compile_expression(gen,expr->right);
    array_printf(gen->buf,") ");
    compile_expression(gen,expr->left);
    array_printf(gen->buf,")");
    break;
  case XPATH_STEP:
    array_printf(gen->buf,"\n// step\n");

    array_printf(gen->buf,"(xslt::path_result ");
    array_printf(gen->buf,"(xslt::apmap3 (!citem.!cpos.!csize.\n");
    compile_expression(gen,expr->right);
    array_printf(gen->buf,") ");
    compile_expression(gen,expr->left);
    array_printf(gen->buf,")");
    array_printf(gen->buf,")");
    break;
  case XPATH_FORWARD_AXIS_STEP:
    compile_expression(gen,expr->left);
    break;
  case XPATH_REVERSE_AXIS_STEP:
    array_printf(gen->buf,"(reverse ");
    compile_expression(gen,expr->left);
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
        fprintf(stderr,"Schema element tests not supported\n");
        exit(1);
        break;
      case KIND_SCHEMA_ATTRIBUTE:
        fprintf(stderr,"Schema attribute tests not supported\n");
        exit(1);
        break;
      case KIND_PI:
        fprintf(stderr,"Processing instruction tests not supported\n");
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
          compile_expression(gen,p->left);
        }
        array_printf(gen->buf,")");
        free(ident);
      }
      else if (!strncmp(expr->qn.uri,"wsdl-",5)) {
        compile_ws_call(gen,expr,expr->qn.uri+5);
      }
      else {
        fprintf(stderr,"Call to non-existent function {%s}%s\n",
                expr->qn.uri ? expr->qn.uri : "",expr->qn.localpart);
        exit(1);
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

        for (c = funname; '\0' != *c; c++)
          if ('-' == *c)
            *c = '_';

        array_printf(gen->buf,"(xslt::fn_%s",funname);

        for (p = expr->left; p; p = p->right) {
          assert(XPATH_ACTUAL_PARAM == p->type);
          array_printf(gen->buf," ");
          compile_expression(gen,p->left);
        }
        array_printf(gen->buf,")");
        free(funname);
      }
    }
    break;
  default:
    fprintf(stderr,"Unsupported expression type: %d\n",expr->type);
    exit(1);
  }
}

static pthread_mutex_t xpath_lock = PTHREAD_MUTEX_INITIALIZER;

int parse_firstline = 0;
expression *parse_expr = NULL;
xmlNodePtr parse_node = NULL;

static expression *parse_xpath(xmlNodePtr n, const char *str)
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
    fprintf(stderr,"XPath parse error\n");
    exit(1);
  }

  assert(parse_expr);
  expr = parse_expr;
  parse_expr = NULL;

  pthread_mutex_unlock(&xpath_lock);

  return expr;
}

static void compile_expr_string(elcgen *gen, xmlNodePtr n, const char *str)
{
  expression *expr = parse_xpath(n,str);
  compile_expression(gen,expr);
  free_expression(expr);
}

static void compile_avt_component(elcgen *gen, expression *expr)
{
  if (XPATH_STRING_LITERAL == expr->type) {
    char *esc = escape1(expr->str);
    array_printf(gen->buf,"\"%s\"",esc);
    free(esc);
  }
  else {
    array_printf(gen->buf,"(xslt::consimple ");
    compile_expression(gen,expr);
    array_printf(gen->buf,")");
  }
}

static void compile_avt(elcgen *gen, xmlNodePtr n, const char *str)
{
  char *tmp = (char*)malloc(strlen(str)+3);
  expression *expr;
  expression *cur;
  int count = 0;
  sprintf(tmp,"}%s{",str);

  cur = expr = parse_xpath(n,tmp);

  while (XPATH_AVT_COMPONENT == cur->type) {
    array_printf(gen->buf,"(append ");
    compile_avt_component(gen,cur->left);

    count++;
    cur = cur->right;
  }

  compile_avt_component(gen,cur);

  while (0 < count--)
    array_printf(gen->buf,")");

  free_expression(expr);
  free(tmp);
}

static void compile_attributes(elcgen *gen, xmlNodePtr n)
{
  xmlAttrPtr attr;
  int count = 0;

  for (attr = n->properties; attr; attr = attr->next) {
    char *value;
    if (attr->ns)
      array_printf(gen->buf,"(cons (xml::mkattr nil nil nil nil \"%s\" \"%s\" \"%s\" ",
             attr->ns->href,attr->ns->prefix,attr->name);
    else
      array_printf(gen->buf,"(cons (xml::mkattr nil nil nil nil nil nil \"%s\" ",attr->name);

    value = xmlNodeListGetString(gen->parse_doc,attr->children,1);
    array_printf(gen->buf,"//xxx value: %s\n",value);
    compile_avt(gen,n,value);
    free(value);

    array_printf(gen->buf,")\n");

    count++;
  }
  array_printf(gen->buf,"nil");
  while (0 < count--)
    array_printf(gen->buf,")");
}

static void compile_namespaces(elcgen *gen, xmlNodePtr n)
{
  xmlNsPtr ns;
  int count = 0;
  for (ns = n->nsDef; ns; ns = ns->next) {
    array_printf(gen->buf,"(cons (xml::mknamespace \"%s\" \"%s\")\n",ns->href,ns->prefix);
    count++;
  }
  array_printf(gen->buf,"nil");
  while (0 < count--)
    array_printf(gen->buf,")");
}

/* FIXME: add a parameter saying where it will be used (e.g. complex content,
   simple content, boolean value) */
static void compile_instruction(elcgen *gen, xmlNodePtr n)
{
  switch (n->type) {
  case XML_ELEMENT_NODE: {
/*     xmlNodePtr child; */
    if (n->ns && !xmlStrcmp(n->ns->href,XSLT_NAMESPACE)) {
      if (!xmlStrcmp(n->name,"sequence")) {
        char *select = get_required_attr(gen,n,"select");

        array_printf(gen->buf,"\n// sequence %s\n",select);
        compile_expr_string(gen,n,select);
        free(select);
      }
      else if (!xmlStrcmp(n->name,"value-of")) {
        char *select = xmlGetProp(n,"select");
        array_printf(gen->buf,"\n// value-of %s\n",select);
        array_printf(gen->buf,"(cons (xml::mktext (xslt::consimple ");
        if (select)
          compile_expr_string(gen,n,select);
        else
          compile_sequence(gen,n->children);
        array_printf(gen->buf,")) nil)\n");
        free(select);
      }
      else if (!xmlStrcmp(n->name,"text")) {
        char *str = xmlNodeListGetString(gen->parse_doc,n->children,1);
        char *esc = escape1(str);
        array_printf(gen->buf,"(cons (xml::mktext \"%s\") nil)",esc);
        free(esc);
        free(str);
      }
      else if (!xmlStrcmp(n->name,"for-each")) {
        char *select = get_required_attr(gen,n,"select");

        array_printf(gen->buf,"\n// for-each %s\n",select);
        array_printf(gen->buf,"(letrec\n");
        array_printf(gen->buf,"select = ");
        compile_expr_string(gen,n,select);
        array_printf(gen->buf,"\nin\n");

        array_printf(gen->buf,"(xslt::apmap3 (!citem.!cpos.!csize.\n");
        compile_sequence(gen,n->children);
        array_printf(gen->buf,")\n");

        array_printf(gen->buf,"select))\n");
        free(select);
      }
      else if (!xmlStrcmp(n->name,"if")) {
        char *test = get_required_attr(gen,n,"test");
        array_printf(gen->buf,"\n// if %s\n",test);
        array_printf(gen->buf,"(if\n");
        array_printf(gen->buf,"(xslt::ebv\n");
        compile_expr_string(gen,n,test);
        array_printf(gen->buf,")\n");
        compile_sequence(gen,n->children);
        array_printf(gen->buf,"\nnil)");
        free(test);
      }
      else if (!xmlStrcmp(n->name,"choose")) {
        xmlNodePtr child;
        int count = 0;
        int otherwise = 0;

        array_printf(gen->buf,"\n// choose\n");
        for (child = n->children; child; child = child->next) {
          if (child->ns && !xmlStrcmp(child->ns->href,XSLT_NAMESPACE) &&
              !xmlStrcmp(child->name,"when")) {
            // when
            char *test = get_required_attr(gen,child,"test");

            array_printf(gen->buf,"\n// when %s\n",test);

            array_printf(gen->buf,"(if (xslt::ebv ");
            compile_expr_string(gen,child,test);
            array_printf(gen->buf,") ");
            compile_sequence(gen,child->children);
            array_printf(gen->buf," ");
            free(test);
            count++;
          }
          else if (child->ns && !xmlStrcmp(child->ns->href,XSLT_NAMESPACE) &&
                   !xmlStrcmp(child->name,"otherwise")) {
            // otherwise
            array_printf(gen->buf,"// otherwise\n");

            compile_sequence(gen,child->children);
            otherwise = 1;
          }
        }

        if (!otherwise)
          array_printf(gen->buf,"nil");

        while (0 < count--)
          array_printf(gen->buf,")");
      }
      else if (!xmlStrcmp(n->name,"element")) {
        /* FIXME: complete this, and handle namespaces properly */
        /* FIXME: avoid reparenting - set the parents correctly in the first place */
        char *name = get_required_attr(gen,n,"name");

        array_printf(gen->buf,"\n// element %s\n",name);

        array_printf(gen->buf,"(letrec\n");

        array_printf(gen->buf,"attrs = (xslt::get_attributes content)\n");
        array_printf(gen->buf,"namespaces = (xslt::get_namespaces content)\n");
        array_printf(gen->buf,"children2 = (xslt::concomplex (xslt::get_children content))\n");
        array_printf(gen->buf,"children = (xslt::reparent children2 elem nil)\n");
        array_printf(gen->buf,"name = ");
        compile_avt(gen,n,name);
        // array_printf(gen->buf,"\"%s\"",name);
        array_printf(gen->buf,"elem = (xml::mkelem nil nil nil nil nil nil name attrs namespaces children)\n");
        array_printf(gen->buf,"content =\n");
        compile_sequence(gen,n->children);
        array_printf(gen->buf,"\nin\n(cons elem nil))");
        free(name);
      }
      else if (!xmlStrcmp(n->name,"attribute")) {
        /* FIXME: handle namespaces properly */
        char *name = get_required_attr(gen,n,"name");
        char *select = xmlGetProp(n,"select");

        array_printf(gen->buf,"\n// attribute %s\n",name);
        array_printf(gen->buf,"(cons (xml::mkattr nil nil nil nil ");
        array_printf(gen->buf," nil "); // nsuri
        array_printf(gen->buf," nil "); // nsprefix

        // localname
        compile_avt(gen,n,name);

        // value
        array_printf(gen->buf,"(xslt::consimple ");
        if (select)
          compile_expr_string(gen,n,select);
        else
          compile_sequence(gen,n->children);
        array_printf(gen->buf,")");

        array_printf(gen->buf,") nil)");
        free(name);
        free(select);
      }
      else {
        fprintf(stderr,"Unsupported XSLT instruction: %s\n",n->name);
        exit(1);
      }
    }
    else {
      /* FIXME: avoid reparenting - set the parents correctly in the first place */
      array_printf(gen->buf,"(letrec\n");

      array_printf(gen->buf,"attrs = (append ");
      compile_attributes(gen,n);
      array_printf(gen->buf," (xslt::get_attributes content))\n");

      array_printf(gen->buf,"namespaces = (append ");
      compile_namespaces(gen,n);
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
      compile_sequence(gen,n->children);
      array_printf(gen->buf,"\nin\n(cons elem nil))");
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
}

static void compile_sequence(elcgen *gen, xmlNodePtr first)
{
  xmlNodePtr child;
  int count = 0;
  int nchildren = 0;
  for (child = first; child; child = child->next)
    if (!ignore_node(child))
      nchildren++;

  if (0 == nchildren) {
    array_printf(gen->buf,"nil");
    return;
  }

  if (1 == nchildren) {
    child = first;
    while (ignore_node(child))
      child = child->next;
    compile_instruction(gen,child);
    return;
  }

  for (child = first; child; child = child->next) {
    if (child->ns && !xmlStrcmp(child->ns->href,XSLT_NAMESPACE) &&
        !xmlStrcmp(child->name,"variable")) {
      /* FIXME: support top-level variables as well */
      char *name = get_required_attr(gen,child,"name");
      char *select = xmlGetProp(child,"select");
      char *ident;
      qname qn;

      qn = string_to_qname(name,child);
      ident = nsname_to_ident(qn.uri,qn.localpart);

      array_printf(gen->buf,"(letrec %s = ",ident);
      if (select) {
        compile_expr_string(gen,child,select);
      }
      else {
        array_printf(gen->buf,"(cons (xml::mkdoc ");
        compile_sequence(gen,child->children);
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
      compile_instruction(gen,child);
      array_printf(gen->buf," ");
      count++;
    }
  }
  array_printf(gen->buf,"nil");
  while (0 < count--)
    array_printf(gen->buf,")");
}

static void compile_template(elcgen *gen, xmlNodePtr n)
{
  array_printf(gen->buf,"\n");
  array_printf(gen->buf,"top citem cpos csize =\n");
  compile_sequence(gen,n->children);
  array_printf(gen->buf,"\n");
  array_printf(gen->buf,"\n");
}

static void compile_function(elcgen *gen, xmlNodePtr child)
{
  char *name = get_required_attr(gen,child,"name");
  char *ident;
  xmlNodePtr pn;
  qname fqn = string_to_qname(name,child);
  if (NULL == fqn.localpart) {
    fprintf(stderr,"XTSE0740: function must have a prefixed name");
    exit(1);
  }

  ident = nsname_to_ident(fqn.uri,fqn.localpart);

  array_printf(gen->buf,"\n");
  array_printf(gen->buf,"%s",ident);

  for (pn = child->children; pn; pn = pn->next) {
    if (XML_ELEMENT_NODE == pn->type) {
      if (pn->ns && !xmlStrcmp(pn->ns->href,XSLT_NAMESPACE) &&
          !xmlStrcmp(pn->name,"param")) {
        char *pname = get_required_attr(gen,pn,"name");
        char *pn_ident;
        qname pqn = string_to_qname(pname,pn);
        pn_ident = nsname_to_ident(pqn.uri,pqn.localpart);
        array_printf(gen->buf," %s",pn_ident);
        free_qname(pqn);
        free(pn_ident);
        free(pname);
      }
      else {
        break;
      }
    }
  }

  array_printf(gen->buf," =\n");
  compile_sequence(gen,pn);
  array_printf(gen->buf,"\n");
  array_printf(gen->buf,"\n");

  free_qname(fqn);
  free(ident);
  free(name);
}

static void process_toplevel(elcgen *gen, xmlNodePtr n2)
{
  xmlNodePtr child;
  for (child = n2->children; child; child = child->next) {
    if (child->ns && !xmlStrcmp(child->ns->href,XSLT_NAMESPACE)) {
      if (!xmlStrcmp(child->name,"template")) {
        compile_template(gen,child);
      }
      else if (!xmlStrcmp(child->name,"function")) {
        compile_function(gen,child);
      }
      else if (!xmlStrcmp(child->name,"output")) {
        char *indent = xmlGetProp(child,"indent");
        if (indent && !xmlStrcmp(indent,"yes"))
          gen->option_indent = 1;
        free(indent);
      }
      else if (!xmlStrcmp(child->name,"strip-space")) {
        char *elements = get_required_attr(gen,child,"elements");
        if (!xmlStrcmp(elements,"*"))
          gen->option_strip = 1;
        free(elements);
      }
    }
  }
}

static void process_root(elcgen *gen, xmlNodePtr n)
{
  if (XML_ELEMENT_NODE == n->type) {
    if (n->ns && !xmlStrcmp(n->ns->href,XSLT_NAMESPACE) &&
        (!xmlStrcmp(n->name,"stylesheet") || !xmlStrcmp(n->name,"transform"))) {
      gen->toplevel = n;
      process_toplevel(gen,n);
    }
    else {
      fprintf(stderr,"top-level element must be a xsl:stylesheet or xsl:transform");
      exit(1);
    }
  }
}

char *cxslt(const char *xslt, const char *xslturl)
{
  xmlDocPtr doc;
  xmlNodePtr root;
  elcgen *gen = (elcgen*)calloc(1,sizeof(elcgen));
  char zero = '\0';
  char *res;

  if (NULL == (doc = xmlReadMemory(xslt,strlen(xslt),NULL,NULL,0))) {
    fprintf(stderr,"Error parsing %s\n",xslturl);
    exit(1);
  }

  gen->buf = array_new(1,0);

  array_printf(gen->buf,"import xml\n");
  array_printf(gen->buf,"import xslt\n");

  gen->parse_doc = doc;
  gen->parse_filename = xslturl;
  root = xmlDocGetRootElement(doc);
  printf("/*\n\n");
  process_root(gen,root);
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
  res = gen->buf->data;
  free(gen->buf);
  free(gen);

  return res;
}
