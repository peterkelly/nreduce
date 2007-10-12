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
 * $Id: cxslt.c 669 2007-10-07 10:16:59Z pmkelly $
 *
 */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>
#include <assert.h>
#include <libxml/parser.h>
#include "cxslt.h"

static xsltnode *lookup_variable(xsltnode *xn, qname qn)
{
  for (; xn; xn = xn->parent) {
    xsltnode *xv;
    for (xv = xn->prev; xv; xv = xv->prev) {
      if ((XSLT_VARIABLE == xv->type) && qname_equals(xv->name_qn,qn))
        return xv;
    }
  }
  return NULL;
}

int is_expr_doc_order(xsltnode *xn, expression *expr)
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
    return is_expr_doc_order(xn,expr->left);
  case XPATH_VAR_REF: {
    xsltnode *var = lookup_variable(xn,expr->qn);
    return (var && (XSLT_VARIABLE == var->type) && !xmlHasProp(var->n,"select"));
    break;
  }
  default:
    break;
  }
  return 0;
}

static int typestack_has(elcgen *gen, void *ptr)
{
  int i;
  for (i = gen->typestack->count-1; i >= 0; i--)
    if (gen->typestack->data[i] == ptr)
      return 1;
  return 0;
}

static int unify_conditional_types(int a, int b)
{
  if (a == b)
    return a;

  if (RESTYPE_UNKNOWN == a)
    return b;
  if (RESTYPE_UNKNOWN == b)
    return a;

  if (RESTYPE_RECURSIVE == a)
    return b;
  if (RESTYPE_RECURSIVE == b)
    return a;

  return RESTYPE_GENERAL;
}

static xsltnode *find_derivative(xsltnode *fun, expression *call)
{
  list *l;
  expression *ap;
  int allgeneral = 1;

  for (ap = call->left; ap; ap = ap->right)
    if (RESTYPE_GENERAL != ap->restype)
      allgeneral = 0;

  if (allgeneral)
    return fun;

  for (l = fun->derivatives; l; l = l->next) {
    xsltnode *d = (xsltnode*)l->data;
    int match = 1;
    xsltnode *fp = d->children.first;
    for (ap = call->left; ap && match; ap = ap->right, fp = fp->next) {
      assert(fp && (XSLT_PARAM == fp->type));
      if (ap->restype != fp->restype)
        match = 0;
    }
    if (match)
      return d;
  }
  return NULL;
}

static xsltnode *add_derivative(elcgen *gen, xsltnode *fun, expression *call)
{
  xsltnode *d = copy_xsltnode(gen,fun,gen->root,0);
  expression *ap = call->left;
  xsltnode *fp = d->children.first;
  char *newname;
  for (; ap; ap = ap->right, fp = fp->next) {
    assert(fp && (XSLT_PARAM == fp->type));
    fp->restype = ap->restype;
  }

  list_append(&fun->derivatives,d);

  newname = (char*)malloc(strlen(d->name_ident)+20);
  sprintf(newname,"%s%d",d->name_ident,list_count(fun->derivatives));
  free(d->name_ident);
  d->name_ident = newname;

  return d;
}

static void xpath_compute_restype(elcgen *gen, expression *expr, int ctxtype)
{
  if ((NULL == expr) || (RESTYPE_UNKNOWN != expr->restype))
    return;

  expr->restype = RESTYPE_GENERAL;
  stack_push(gen->typestack,expr);
  gen->indent++;

  switch (expr->type) {
  case XPATH_ADD:
  case XPATH_SUBTRACT:
  case XPATH_MULTIPLY:
  case XPATH_DIVIDE:
  case XPATH_IDIVIDE:
  case XPATH_MOD: {
    int left;
    int right;
    xpath_compute_restype(gen,expr->left,ctxtype);
    xpath_compute_restype(gen,expr->right,ctxtype);
    left = expr->left->restype;
    right = expr->right->restype;

    if ((RESTYPE_RECURSIVE == left) || (RESTYPE_RECURSIVE == right))
      expr->restype = RESTYPE_RECURSIVE;
    else if ((RESTYPE_NUMBER == left) && (RESTYPE_NUMBER == right))
      expr->restype = RESTYPE_NUMBER;
    else
      expr->restype = RESTYPE_GENERAL;
    break;
  }
  case XPATH_IF:
    xpath_compute_restype(gen,expr->test,ctxtype);
    xpath_compute_restype(gen,expr->left,ctxtype);
    xpath_compute_restype(gen,expr->right,ctxtype);
    if (RESTYPE_RECURSIVE == expr->test->restype)
      expr->restype = RESTYPE_RECURSIVE;
    else
      expr->restype = unify_conditional_types(expr->left->restype,expr->right->restype);
    break;
  case XPATH_INTEGER_LITERAL:
  case XPATH_DECIMAL_LITERAL:
  case XPATH_DOUBLE_LITERAL:
    expr->restype = RESTYPE_NUMBER;
    break;
  case XPATH_PAREN:
    xpath_compute_restype(gen,expr->left,RESTYPE_GENERAL);
    expr->restype = expr->left->restype;
    break;
  case XPATH_TO:
    xpath_compute_restype(gen,expr->left,ctxtype);
    xpath_compute_restype(gen,expr->right,ctxtype);
    expr->restype = RESTYPE_NUMLIST;
    break;
  case XPATH_CONTEXT_ITEM:
    expr->restype = ctxtype;
    break;
  case XPATH_FUNCTION_CALL:
    xpath_compute_restype(gen,expr->left,ctxtype);
    if (NULL == expr->target) {
      expr->restype = RESTYPE_GENERAL;
    }
    else {
      xsltnode *d = find_derivative(expr->target,expr);
      if (NULL == d) {
        if (2 > list_count(expr->target->derivatives))
          d = add_derivative(gen,expr->target,expr);
        else
          d = expr->target;
      }
      expr->target = d;

      if (typestack_has(gen,expr->target)) {
        expr->restype = RESTYPE_RECURSIVE;
      }
      else {
        xslt_compute_restype(gen,expr->target,RESTYPE_GENERAL);
        expr->restype = expr->target->restype;
        expr->target->called = 1;
      }
    }
    break;
  case XPATH_ACTUAL_PARAM: 	
    xpath_compute_restype(gen,expr->left,ctxtype);
    expr->restype = expr->left->restype;
    xpath_compute_restype(gen,expr->right,ctxtype);
    break;
  case XPATH_VAR_REF:
    xslt_compute_restype(gen,expr->target,RESTYPE_GENERAL);
    expr->restype = expr->target->restype;
    break;
  default:
    xpath_compute_restype(gen,expr->test,RESTYPE_GENERAL);
    xpath_compute_restype(gen,expr->left,RESTYPE_GENERAL);
    xpath_compute_restype(gen,expr->right,RESTYPE_GENERAL);
    expr->restype = RESTYPE_GENERAL;
    break;
  }
  gen->indent--;
  stack_pop(gen->typestack);
  assert(RESTYPE_UNKNOWN != expr->restype);
}

void xslt_compute_restype(elcgen *gen, xsltnode *xn, int ctxtype)
{
  xsltnode *c;

  if ((NULL == xn) || (RESTYPE_UNKNOWN != xn->restype))
    return;

  stack_push(gen->typestack,xn);
  gen->indent++;

  xn->restype = RESTYPE_GENERAL;

  switch (xn->type) {
  case XSLT_FUNCTION:
    for (c = xn->children.first; c; c = c->next)
      xslt_compute_restype(gen,c,RESTYPE_GENERAL);

    c = xn->children.first;
    while (c && (XSLT_PARAM == c->type))
      c = c->next;
    if ((NULL != c) && (NULL == c->next))
      xn->restype = c->restype;
    else
      xn->restype = RESTYPE_GENERAL;
    break;
  case XSLT_WHEN:
  case XSLT_OTHERWISE:
    xpath_compute_restype(gen,xn->expr,ctxtype);
    for (c = xn->children.first; c; c = c->next)
      xslt_compute_restype(gen,c,ctxtype);

    if ((NULL != xn->children.first) && (NULL == xn->children.first->next))
      xn->restype = xn->children.first->restype;
    else
      xn->restype = RESTYPE_GENERAL;
    break;
  case XSLT_SEQUENCE:
    xpath_compute_restype(gen,xn->expr,ctxtype);
    xn->restype = xn->expr->restype;
    break;
  case XSLT_FOR_EACH:
    xpath_compute_restype(gen,xn->expr,RESTYPE_GENERAL);
    for (c = xn->children.first; c; c = c->next) {
      if (RESTYPE_NUMLIST == xn->expr->restype)
        xslt_compute_restype(gen,c,RESTYPE_NUMBER);
      else
        xslt_compute_restype(gen,c,RESTYPE_GENERAL);
    }
    break;
  case XSLT_CHOOSE: {
    xsltnode *xchild;
    int latest = RESTYPE_UNKNOWN;
    int otherwise = 0;

    for (c = xn->children.first; c; c = c->next)
      xslt_compute_restype(gen,c,ctxtype);

    for (xchild = xn->children.first; xchild; xchild = xchild->next) {
      latest = unify_conditional_types(latest,xchild->restype);

      if (XSLT_OTHERWISE == xchild->type)
        otherwise = 1;
    }
    if (otherwise)
      xn->restype = latest;
    else
      xn->restype = RESTYPE_GENERAL;
    break;
  }
  default:

    xpath_compute_restype(gen,xn->expr,RESTYPE_GENERAL);
    xpath_compute_restype(gen,xn->name_avt,RESTYPE_GENERAL);
    xpath_compute_restype(gen,xn->value_avt,RESTYPE_GENERAL);
    xpath_compute_restype(gen,xn->namespace_avt,RESTYPE_GENERAL);

    for (c = xn->children.first; c; c = c->next)
      xslt_compute_restype(gen,c,RESTYPE_GENERAL);
    for (c = xn->attributes.first; c; c = c->next)
      xslt_compute_restype(gen,c,RESTYPE_GENERAL);

    xn->restype = RESTYPE_GENERAL;
    break;
  }

  gen->indent--;
  stack_pop(gen->typestack);
  assert(RESTYPE_UNKNOWN != xn->restype);
}
