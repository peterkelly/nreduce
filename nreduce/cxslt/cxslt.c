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

void gen_iprintf(elcgen *gen, int indent, const char *format, ...)
{
  va_list ap;
  int i;
  array_printf(gen->buf,"\n");
  for (i = 0; i < indent; i++)
    array_printf(gen->buf,"  ");
  va_start(ap,format);
  array_vprintf(gen->buf,format,ap);
  va_end(ap);
}

void gen_printorig(elcgen *gen, int indent, expression *expr)
{
  /* FIXME: hack */
  if (XSLT_NAMESPACE == expr->type)
    gen_iprintf(gen,indent,"/* namespace");
  else
    gen_iprintf(gen,indent,"/* %s",expr_names[expr->type]);
  if (expr && expr->orig) {
    char *str2 = (char*)malloc(2*strlen(expr->orig)+1);
    const char *r;
    char *w = str2;
    for (r = expr->orig; *r; r++) {
      if ('*' == *r) {
        *(w++) = '*';
        *(w++) = ' ';
      }
      else {
        *(w++) = *r;
      }
    }
    *w = '\0';
    gen_printf(gen," %s",str2);
    free(str2);
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

int is_forward_axis(int axis)
{
  switch (axis) {
  case AXIS_CHILD:
  case AXIS_DESCENDANT:
  case AXIS_ATTRIBUTE:
  case AXIS_SELF:
  case AXIS_DESCENDANT_OR_SELF:
  case AXIS_FOLLOWING_SIBLING:
  case AXIS_FOLLOWING:
  case AXIS_NAMESPACE:
  case AXIS_DSVAR_FORWARD:
    return 1;
  default:
    return 0;
  }
}

int is_reverse_axis(int axis)
{
  switch (axis) {
  case AXIS_PARENT:
  case AXIS_ANCESTOR:
  case AXIS_PRECEDING_SIBLING:
  case AXIS_PRECEDING:
  case AXIS_ANCESTOR_OR_SELF:
  case AXIS_DSVAR_REVERSE:
    return 1;
  default:
    return 0;
  }
}

static int compile_post(elcgen *gen, int indent, expression *expr,
                        const char *service_url,
                        qname inelem, qname outelem,
                        list *inargs, list *outargs)
{
  list *l;
  expression *p;
  int r = 1;

  gen_iprintf(gen,indent,"(letrec");
  gen_iprintf(gen,indent+1,"returns = (xslt::call_ws");

  gen_iprintf(gen,indent+2,"\"%s\"",service_url);
  gen_iprintf(gen,indent+2,"\"%s\"",inelem.uri);
  gen_iprintf(gen,indent+2,"\"%s\"",inelem.localpart);
  gen_iprintf(gen,indent+2,"\"%s\"",outelem.uri);
  gen_iprintf(gen,indent+2,"\"%s\"",outelem.localpart);

  l = inargs;
  for (p = expr->r.left; p && r; p = p->r.right) {
    wsarg *argname = (wsarg*)l->data;
    assert(XPATH_ACTUAL_PARAM == p->type);

    if (argname->list)
      gen_iprintf(gen,indent+2,"(append (xslt::ws_arg_list");
    else
      gen_iprintf(gen,indent+2,"(cons (xslt::ws_arg");
    gen_printf(gen," \"%s\" \"%s\" ",argname->uri,argname->localpart);

    if (!compile_expression(gen,indent+3,p->r.left))
      r = 0;
    gen_printf(gen,")");

    l = l->next;
  }
  gen_iprintf(gen,indent+2,"nil");
  for (p = expr->r.left; p; p = p->r.right)
    gen_printf(gen,")");
  gen_printf(gen,")");
  gen_iprintf(gen,indent,"in");

  if (((wsarg*)outargs->data)->simple)
    gen_iprintf(gen,indent,"  (apmap xml::item_children returns))");
  else
    gen_iprintf(gen,indent,"  returns)");
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

int compile_ws_call(elcgen *gen, int indent, expression *expr, const char *wsdlurl)
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
  for (p = expr->r.left; p; p = p->r.right)
    supplied++;
  expected = list_count(inargs);

  if (supplied != expected)
    r = gen_error(gen,"Incorrect # args for web service call %s: expected %d, got %d",
                  expr->qn.localpart,expected,supplied);
  else if (1 != list_count(outargs))
    r = gen_error(gen,"Multiple return arguments for web service call %s",expr->qn.localpart);
  else
    r = compile_post(gen,indent,expr,service_url,inelem,outelem,inargs,outargs);

  free(ident);
  free(inelem.uri);
  free(inelem.localpart);
  free(outelem.uri);
  free(outelem.localpart);
  list_free(inargs,free_wsarg_ptr);
  list_free(outargs,free_wsarg_ptr);
  return r;
}

int compile_user_function_call(elcgen *gen, int indent, expression *expr, int fromnum)
{
  int pnum = 0;

  assert(expr->target);
  if (!fromnum && (RESTYPE_NUMBER == expr->target->restype)) {
    gen_iprintf(gen,indent,"(cons (xml::mknumber ");
    indent++;
  }

  gen_iprintf(gen,indent,"(%s",expr->target->ident);
  indent++;

  expression *ap = expr->r.left;
  expression *fp = expr->target->r.children;


  for (; ap; ap = ap->r.right, fp = fp->next) {
    assert(XPATH_ACTUAL_PARAM == ap->type);
    assert(fp && (XSLT_PARAM == fp->type));
    gen_printf(gen," ");
    if (RESTYPE_NUMBER == fp->restype) {
      if (RESTYPE_NUMBER == ap->r.left->restype) {
        if (!compile_num_expression(gen,indent,ap->r.left))
          return 0;
      }
      else {
        gen_iprintf(gen,indent,"(xslt::seq_to_number");
        if (!compile_expression(gen,indent,ap->r.left))
          return 0;
        gen_printf(gen,")");
      }
    }
    else {
      if (!compile_expression(gen,indent,ap->r.left))
        return 0;
    }
    pnum++;
  }
  indent--;
  gen_printf(gen,")");

  if (!fromnum && (RESTYPE_NUMBER == expr->target->restype)) {
    gen_iprintf(gen,indent,") nil)");
    indent--;
  }

  return 1;
}

int compile_builtin_function_call(elcgen *gen, int indent, expression *expr)
{
  expression *p;
  int nargs = 0;

  for (p = expr->r.left; p; p = p->r.right)
    nargs++;

  if (!strcmp(expr->qn.localpart,"string") && (0 == nargs)) {
    gen_iprintf(gen,indent,"(xslt::fn_string0 citem)");
  }
  else if (!strcmp(expr->qn.localpart,"string-length") && (0 == nargs)) {
    gen_iprintf(gen,indent,"(xslt::fn_string_length0 citem)");
  }
  else if (!strcmp(expr->qn.localpart,"root") && (0 == nargs)) {
    gen_printf(gen,"(cons (xml::item_root citem) nil)");
  }
  else if (!strcmp(expr->qn.localpart,"position")) {
    gen_iprintf(gen,indent,"(cons (xml::mknumber cpos) nil)");
  }
  else if (!strcmp(expr->qn.localpart,"last")) {
    gen_iprintf(gen,indent,"(cons (xml::mknumber csize) nil)");
  }
  else {
    char *funname = strdup(expr->qn.localpart);
    char *c;
    int r = 1;

    for (c = funname; '\0' != *c; c++)
      if ('-' == *c)
        *c = '_';

    gen_iprintf(gen,indent,"(xslt::fn_%s",funname);
    for (p = expr->r.left; p && r; p = p->r.right) {
      assert(XPATH_ACTUAL_PARAM == p->type);
      gen_printf(gen," ");
      r = r && compile_expression(gen,indent+1,p->r.left);
    }
    gen_printf(gen,")");
    free(funname);
    return r;
  }
  return 1;
}

int compile_attributes(elcgen *gen, int indent, expression *expr)
{
  expression *xattr;
  int count = 0;
  int r = 1;

  for (xattr = expr->r.attributes; xattr && r; xattr = xattr->next) {
    if (xattr->qn.uri)
      gen_printf(gen,"(cons (xml::mkattr nil nil nil nil \"%s\" \"%s\" \"%s\" ",
                 xattr->qn.uri,xattr->qn.prefix,xattr->qn.localpart);
    else
      gen_printf(gen,"(cons (xml::mkattr nil nil nil nil nil nil \"%s\" ",
                 xattr->qn.localpart);
    r = compile_avt(gen,indent,xattr->r.value_avt);
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

int compile_namespaces(elcgen *gen, int indent, expression *expr)
{
  xmlNsPtr ns;
  xmlNodePtr p;
  int count = 0;
  for (p = expr->xmlnode; p; p = p->parent) {
    for (ns = p->nsDef; ns; ns = ns->next) {
      if (have_prefix(expr->xmlnode,p,(char*)ns->prefix))
        continue;
      if (ns->prefix) {
        if (!exclude_namespace(gen,expr,(char*)ns->href)) {
          gen_iprintf(gen,indent,"(cons (xml::mknamespace \"%s\" \"%s\") ",ns->href,ns->prefix);
          count++;
        }
      }
      else {
        gen_iprintf(gen,indent,"(cons (xml::mknamespace \"%s\" nil) ",ns->href);
        count++;
      }
    }
  }
  gen_printf(gen,"nil");
  while (0 < count--)
    gen_printf(gen,")");
  return 1;
}

int compile_variable(elcgen *gen, int indent, expression *xchild)
{
  int r = 1;
  gen_iprintf(gen,indent,"(letrec %s = ",xchild->ident);
  if (xchild->r.left) {
    r = compile_expression(gen,indent+1,xchild->r.left);
  }
  else {
    gen_printf(gen,"(cons (xslt::copydoc (!x.x) (xml::mkdoc ");
    r = compile_sequence(gen,indent+1,xchild->r.children);
    gen_printf(gen,")) nil)");
  }
  gen_iprintf(gen,indent," in ");
  return r;
}

int compile_qname(elcgen *gen, int indent, expression *expr)
{
  gen_printf(gen," \"%s\" \"%s\" \"%s\" ",expr->qn.uri,expr->qn.prefix,expr->qn.localpart);
  return 1;
}

int compile_num_instruction(elcgen *gen, int indent, expression *expr)
{
  int r = 1;
  switch (expr->type) {
  case XSLT_SEQUENCE: {
    gen_printorig(gen,indent,expr);
    r = compile_num_expression(gen,indent,expr->r.left);
    return r;
  }
  case XSLT_CHOOSE: {
    expression *xchild;
    int count = 0;
    int otherwise = 0;

    gen_printorig(gen,indent,expr);

    for (xchild = expr->r.children; xchild && r; xchild = xchild->next) {
      switch (xchild->type) {
      case XSLT_WHEN:
        gen_printorig(gen,indent,xchild);
        gen_iprintf(gen,indent,"(if ");
        r = r && compile_ebv_expression(gen,indent+1,xchild->r.left);
        gen_printf(gen," ");
        r = r && compile_num_sequence(gen,indent+1,xchild->r.children);
        gen_printf(gen," ");
        count++;
        break;
      case XSLT_OTHERWISE:
        gen_printorig(gen,indent,xchild);
        r = r && compile_num_sequence(gen,indent,xchild->r.children);
        otherwise = 1;
        break;
      default:
        r = gen_error(gen,"Invalid element in choose: %s\n",expr_names[xchild->type]);
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

int compile_num_sequence(elcgen *gen, int indent, expression *xfirst)
{
  expression *xchild;
  int count = 0;
  int r = 1;

  for (xchild = xfirst; xchild && (XSLT_VARIABLE == xchild->type); xchild = xchild->next) {
    if (!compile_variable(gen,indent,xchild))
      return 0;
    count++;
  }

  assert(xchild);
  assert(NULL == xchild->next);

  r = compile_num_instruction(gen,indent,xchild);

  while (0 < count--)
    gen_printf(gen,")");
  return r;
}

int compile_sequence(elcgen *gen, int indent, expression *xfirst)
{
  expression *xchild;
  int count = 0;
  int r = 1;

  if (NULL == xfirst) {
    gen_printf(gen,"nil");
    return 1;
  }

  if (NULL == xfirst->next)
    return compile_instruction(gen,indent,xfirst);

  for (xchild = xfirst; xchild && r; xchild = xchild->next) {
    if (XSLT_VARIABLE == xchild->type) {
      /* FIXME: support top-level variables as well */
      r = compile_variable(gen,indent,xchild);
    }
    else {
      gen_printf(gen,"(append ");
      r = compile_instruction(gen,indent,xchild);
      gen_printf(gen," ");
    }
    count++;
  }
  gen_printf(gen,"nil");
  while (0 < count--)
    gen_printf(gen,")");
  return r;
}

static int compile_template(elcgen *gen, expression *expr, int pos)
{
  int r;
  gen_iprintf(gen,0,"");
  gen_iprintf(gen,0,"template%d citem cpos csize = ",pos);
  r = compile_sequence(gen,0,expr->r.children);
  return r;
}

static int compile_function(elcgen *gen, expression *expr)
{
  expression *pexpr;
  int r;

  if (NULL == expr->qn.localpart)
    return gen_error(gen,"XTSE0740: function must have a prefixed name");

  gen_iprintf(gen,0,"");
  gen_iprintf(gen,0,"%s",expr->ident);
  for (pexpr = expr->r.children; pexpr && (XSLT_PARAM == pexpr->type); pexpr = pexpr->next)
    gen_printf(gen," %s",pexpr->ident);
  gen_printf(gen," = ");

  if (RESTYPE_NUMBER == expr->restype) {
    r = compile_num_sequence(gen,1,pexpr);
  }
  else {
    gen_iprintf(gen,0,"(xslt::reparent ");
    r = compile_sequence(gen,1,pexpr);
    gen_iprintf(gen,0,"  nil nil nil)");
  }

  return r;
}

//#define PRINT_PATTERN
#ifdef PRINT_PATTERN
static void print_pattern(elcgen *gen, expression *expr)
{
  switch (expr->type) {
  case XPATH_STEP:
    gen_iprintf(gen,indent,"step");
    gen->indent++;
    print_pattern(gen,expr->r.left);
    print_pattern(gen,expr->r.right);
    gen->indent--;
    break;
  case XPATH_FILTER:
    gen_iprintf(gen,indent,"filter");
    gen->indent++;
    print_pattern(gen,expr->r.left);
    print_pattern(gen,expr->r.right);
    gen->indent--;
    break;
  case XPATH_ROOT:
    gen_iprintf(gen,indent,"root");
    break;
  case XPATH_NODE_TEST:
    if (XPATH_KIND_TEST == expr->r.right->type)
      gen_iprintf(gen,indent,"kind=%s (%s)",
                  kind_names[expr->r.right->kind],axis_names[expr->r.left->axis]);
    else if (XPATH_NAME_TEST == expr->r.right->type)
      gen_iprintf(gen,indent,"name=%s (%s)",expr->r.right->qn.localpart,
                  axis_names[expr->r.left->axis]);
    break;
  default:
    gen_iprintf(gen,indent,"unknown %d",expr->type);
  }
}
#endif

static int compile_pattern(elcgen *gen, int indent, expression *expr, int p)
{
  int r = 1;
  switch (expr->type) {
  case XPATH_STEP:
    gen_iprintf(gen,indent+1,"/* step */");
    if ((XPATH_NODE_TEST == expr->r.right->type) &&
        (XPATH_KIND_TEST == expr->r.right->r.right->type) &&
        (KIND_ANY == expr->r.right->r.right->kind) &&
        (AXIS_DESCENDANT_OR_SELF == expr->r.right->r.left->axis)) {
      gen_iprintf(gen,indent+1,"(xslt::check_aos p%d (!p%d.",p,p+1);
      compile_pattern(gen,indent+1,expr->r.left,p+1);
      gen_iprintf(gen,indent+1,"))",p);
    }
    else {
      gen_iprintf(gen,indent+1,"(letrec r = ");
      r = r && compile_pattern(gen,indent+1,expr->r.right,p);
      gen_printf(gen," in ");
      gen_iprintf(gen,indent+1,"(if r ");
      /* FIXME: need to take into account the case where parent is nil */
      gen_iprintf(gen,indent+1,"(letrec p%d = (xml::item_parent p%d) in ",p+1,p);
      r = r && compile_pattern(gen,indent+1,expr->r.left,p+1);
      gen_printf(gen,")");
      gen_iprintf(gen,indent+1,"nil))");
    }
    break;
  case XPATH_FILTER:
    /* FIXME: shouldn't we also be checking expr->r.left here? */
    gen_iprintf(gen,indent,"/* predicate */ ");
    gen_iprintf(gen,indent,"(letrec ");
    gen_iprintf(gen,indent,"  citem = p%d",p);
    gen_iprintf(gen,indent,"  cpos = (xslt::compute_pos p%d)");
    gen_iprintf(gen,indent,"  csize = (xslt::compute_size p%d)");
    gen_iprintf(gen,indent,"in");
    r = r && compile_predicate(gen,indent+1,expr->r.right);
    gen_printf(gen,")",p);
    break;
  case XPATH_ROOT:
    gen_printf(gen,"(if (== (xml::item_type p%d) xml::TYPE_DOCUMENT) p%d nil)",p,p);
    break;
  case XPATH_NODE_TEST:
    switch (expr->r.left->axis) {
    case AXIS_CHILD:
    case AXIS_ATTRIBUTE:
      gen_printf(gen,"(if (");
      r = r && compile_test(gen,indent,expr);
      gen_printf(gen," p%d) p%d nil)",p,p);
      break;
    default:
      return gen_error(gen,"Invalid axis in pattern: %d",expr->r.left->axis);
      break;
    }
    break;
  default:
    return gen_error(gen,"Unsupported pattern construct: %d",expr->type);
  }
  return r;
}

static int compile_apply_templates(elcgen *gen, expression *stylesheet)
{
  int templateno = 0;
  int count = 0;
  int r = 1;
  expression *xchild;

  /* TODO: take into account template priorities */
  /* TODO: test apply-templates with different select values */

  gen_iprintf(gen,0,"apply_templates lst = (apply_templates1 lst 1 (len lst))");
  gen_iprintf(gen,0,"");

  gen_iprintf(gen,0,"apply_templates1 lst cpos csize = ");
  gen_iprintf(gen,0,"(if lst");
  gen_iprintf(gen,0,"  (letrec");
  gen_iprintf(gen,0,"    p0 = (head lst)");
  gen_iprintf(gen,0,"    rest = (tail lst)");
  gen_iprintf(gen,0,"  in ");
  gen_iprintf(gen,0,"    (append ");
  for (xchild = stylesheet->r.children; xchild && r; xchild = xchild->next) {
    if ((XSLT_TEMPLATE == xchild->type) && xchild->r.left) {
      gen_printorig(gen,3,xchild);
      gen_iprintf(gen,3,"(if ");
#ifdef PRINT_PATTERN
      gen_iprintf(gen,3+1,"/*");
      print_pattern(gen,xchild->r.left);
      gen_iprintf(gen,3+1,"*/");
      gen_iprintf(gen,3+1,"");
#endif
      r = compile_pattern(gen,3+1,xchild->r.left,0);
      gen_iprintf(gen,3,"  (template%d p0 cpos csize)",templateno);
      count++;
      templateno++;
    }
  }

  /* 6.6 Built-in Template Rules */
  gen_iprintf(gen,3,"(if (== (xml::item_type p0) xml::TYPE_ELEMENT)");
  gen_iprintf(gen,3,"  (apply_templates (xml::item_children p0))");
  gen_iprintf(gen,3,"(if (== (xml::item_type p0) xml::TYPE_DOCUMENT)");
  gen_iprintf(gen,3,"  (apply_templates (xml::item_children p0))");
  gen_iprintf(gen,3,"(if (== (xml::item_type p0) xml::TYPE_TEXT)");
  gen_iprintf(gen,3,"  (cons (xml::mktext (xml::item_value p0)) nil)");
  gen_iprintf(gen,3,"(if (== (xml::item_type p0) xml::TYPE_ATTRIBUTE)");
  gen_iprintf(gen,3,"  (cons (xml::mktext (xml::item_value p0)) nil)");
  gen_iprintf(gen,3,"nil))))");

  while (0 < count--)
    gen_printf(gen,")");

  gen_iprintf(gen,3,"(apply_templates1 (tail lst) (+ cpos 1) csize)))");
  gen_iprintf(gen,0,"  nil)");
  gen_iprintf(gen,0,"");

  return r;
}

int build_xslt_tree(elcgen *gen, xmlNodePtr n, expression **root)
{
  expression *c;

  if ((XML_ELEMENT_NODE != n->type) ||
      (NULL == n->ns) || xmlStrcmp(n->ns->href,XSLT_NS) ||
      (xmlStrcmp(n->name,"stylesheet") && xmlStrcmp(n->name,"transform")))
    return gen_error(gen,"top-level element must be a xsl:stylesheet or xsl:transform");

  *root = new_xsltnode(n,XSLT_STYLESHEET);
  gen->toplevel = n;
  gen->root = *root;
  if (!parse_xslt(gen,n,*root))
    return 0;
  expr_set_parents(*root,NULL);
  if (!expr_resolve_vars(gen,*root))
    return 0;

  for (c = (*root)->r.children; c; c = c->next) {
    if (XSLT_TEMPLATE == c->type)
      expr_compute_restype(gen,c,RESTYPE_GENERAL);
  }

  return 1;
}

static int process_root(elcgen *gen, xmlNodePtr n)
{
  expression *child;
  int r = 1;
  int templateno = 0;
  expression *root;

  if (!build_xslt_tree(gen,n,&root))
    return 0;

/*   gen_iprintf(gen,0,"/\* tree:"); */
/*   expr_print_tree(gen,0,root); */
/*   gen_iprintf(gen,0,"*\/"); */

  r = r && compile_apply_templates(gen,root);

  for (child = root->r.children; child && r; child = child->next) {
    switch (child->type) {
    case XSLT_TEMPLATE:
      r = compile_template(gen,child,templateno++);
      break;
    case XSLT_FUNCTION:
      if (child->called)
        r = compile_function(gen,child);
      break;
    case XSLT_OUTPUT:
      if (attr_equals(child->xmlnode,"indent","yes"))
        gen->option_indent = 1;
      break;
    case XSLT_STRIP_SPACE:
      if (attr_equals(child->xmlnode,"elements","*"))
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
      gen_iprintf(gen,0,"");
      gen_iprintf(gen,0,"STRIPALL = %s\n",gen->option_strip ? "1" : "nil");
      gen_iprintf(gen,0,"INDENT = %s\n",gen->option_indent ? "1" : "nil");
      gen_iprintf(gen,0,
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
