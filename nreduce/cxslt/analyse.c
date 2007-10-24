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

int is_expr_doc_order(expression *expr)
{
  switch (expr->type) {
  case XPATH_STEP:
  case XPATH_NODE_TEST:
  case XPATH_KIND_TEST:
  case XPATH_NAME_TEST:
  case XPATH_ROOT:
    return 1;
  case XPATH_FILTER:
    return (XPATH_NODE_TEST == expr->r.left->type);
    break;
  case XPATH_VAR_REF: {
    /* only in doc order if content is specified in child elements, not select attribute */
    return ((XSLT_VARIABLE == expr->target->type) && (NULL == expr->target->r.left));
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

static expression *find_derivative(expression *fun, expression *call)
{
  list *l;
  expression *ap;
  int allgeneral = 1;

  for (ap = call->r.left; ap; ap = ap->r.right)
    if (RESTYPE_GENERAL != ap->restype)
      allgeneral = 0;

  if (allgeneral)
    return fun;

  for (l = fun->derivatives; l; l = l->next) {
    expression *d = (expression*)l->data;
    int match = 1;
    expression *fp = d->r.children;
    for (ap = call->r.left; ap && match; ap = ap->r.right, fp = fp->next) {
      assert(fp && (XSLT_PARAM == fp->type));
      if (ap->restype != fp->restype)
        match = 0;
    }
    if (match)
      return d;
  }
  return NULL;
}

static expression *add_derivative(elcgen *gen, expression *fun, expression *call)
{
  expression *d = expr_copy(fun);
  expression *ap = call->r.left;
  expression *fp = d->r.children;
  char *newname;
  int r;
  expression **cptr;
  expr_set_parents(d,gen->root);
  for (cptr = &gen->root->r.children; *cptr; cptr = &((*cptr)->next))
    d->prev = *cptr;
  *cptr = d;
  r = expr_resolve_vars(gen,d);
  if (!r)
    fprintf(stderr,"unexpected error: %s\n",gen->error);
  assert(r); /* checks already done on the original */
  for (; ap; ap = ap->r.right, fp = fp->next) {
    assert(fp && (XSLT_PARAM == fp->type));
    fp->restype = ap->restype;
  }

  list_append(&fun->derivatives,d);

  newname = (char*)malloc(strlen(d->ident)+20);
  sprintf(newname,"%s%d",d->ident,list_count(fun->derivatives));
  free(d->ident);
  d->ident = newname;

  return d;
}

void expr_compute_restype(elcgen *gen, expression *expr, int ctxtype)
{
  expression *c;

  if ((NULL == expr) || (RESTYPE_UNKNOWN != expr->restype))
    return;

  expr->restype = RESTYPE_GENERAL;
  stack_push(gen->typestack,expr);

  switch (expr->type) {
  case XPATH_ADD:
  case XPATH_SUBTRACT:
  case XPATH_MULTIPLY:
  case XPATH_DIVIDE:
  case XPATH_IDIVIDE:
  case XPATH_MOD: {
    int left;
    int right;
    expr_compute_restype(gen,expr->r.left,ctxtype);
    expr_compute_restype(gen,expr->r.right,ctxtype);
    left = expr->r.left->restype;
    right = expr->r.right->restype;

    if ((RESTYPE_RECURSIVE == left) || (RESTYPE_RECURSIVE == right))
      expr->restype = RESTYPE_RECURSIVE;
    else if ((RESTYPE_NUMBER == left) && (RESTYPE_NUMBER == right))
      expr->restype = RESTYPE_NUMBER;
    else
      expr->restype = RESTYPE_GENERAL;
    break;
  }
  case XPATH_IF:
    expr_compute_restype(gen,expr->r.test,ctxtype);
    expr_compute_restype(gen,expr->r.left,ctxtype);
    expr_compute_restype(gen,expr->r.right,ctxtype);
    if (RESTYPE_RECURSIVE == expr->r.test->restype)
      expr->restype = RESTYPE_RECURSIVE;
    else
      expr->restype = unify_conditional_types(expr->r.left->restype,expr->r.right->restype);
    break;
  case XPATH_NUMERIC_LITERAL:
    expr->restype = RESTYPE_NUMBER;
    break;
  case XPATH_TO:
    expr_compute_restype(gen,expr->r.left,ctxtype);
    expr_compute_restype(gen,expr->r.right,ctxtype);
    expr->restype = RESTYPE_NUMLIST;
    break;
  case XPATH_CONTEXT_ITEM:
    expr->restype = ctxtype;
    break;
  case XPATH_FUNCTION_CALL:
    expr_compute_restype(gen,expr->r.left,ctxtype);
    if (NULL == expr->target) {
      expr->restype = RESTYPE_GENERAL;
    }
    else {
      expression *d = find_derivative(expr->target,expr);
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
        expr_compute_restype(gen,expr->target,RESTYPE_GENERAL);
        expr->restype = expr->target->restype;
        expr->target->called = 1;
      }
    }
    break;
  case XPATH_ACTUAL_PARAM: 	
    expr_compute_restype(gen,expr->r.left,ctxtype);
    expr->restype = expr->r.left->restype;
    expr_compute_restype(gen,expr->r.right,ctxtype);
    break;
  case XPATH_VAR_REF:
    expr_compute_restype(gen,expr->target,RESTYPE_GENERAL);
    expr->restype = expr->target->restype;
    break;
  case XSLT_FUNCTION:
    for (c = expr->r.children; c; c = c->next)
      expr_compute_restype(gen,c,RESTYPE_GENERAL);

    c = expr->r.children;
    while (c && (XSLT_PARAM == c->type))
      c = c->next;
    if ((NULL != c) && (NULL == c->next))
      expr->restype = c->restype;
    else
      expr->restype = RESTYPE_GENERAL;
    break;
  case XSLT_WHEN:
  case XSLT_OTHERWISE:
    expr_compute_restype(gen,expr->r.left,ctxtype);
    for (c = expr->r.children; c; c = c->next)
      expr_compute_restype(gen,c,ctxtype);

    if ((NULL != expr->r.children) && (NULL == expr->r.children->next))
      expr->restype = expr->r.children->restype;
    else
      expr->restype = RESTYPE_GENERAL;
    break;
  case XSLT_SEQUENCE:
    expr_compute_restype(gen,expr->r.left,ctxtype);
    expr->restype = expr->r.left->restype;
    break;
  case XSLT_FOR_EACH:
    expr_compute_restype(gen,expr->r.left,RESTYPE_GENERAL);
    for (c = expr->r.children; c; c = c->next) {
      if (RESTYPE_NUMLIST == expr->r.left->restype)
        expr_compute_restype(gen,c,RESTYPE_NUMBER);
      else
        expr_compute_restype(gen,c,RESTYPE_GENERAL);
    }
    break;
  case XSLT_CHOOSE: {
    expression *xchild;
    int latest = RESTYPE_UNKNOWN;
    int otherwise = 0;

    for (c = expr->r.children; c; c = c->next)
      expr_compute_restype(gen,c,ctxtype);

    for (xchild = expr->r.children; xchild; xchild = xchild->next) {
      latest = unify_conditional_types(latest,xchild->restype);

      if (XSLT_OTHERWISE == xchild->type)
        otherwise = 1;
    }
    if (otherwise)
      expr->restype = latest;
    else
      expr->restype = RESTYPE_GENERAL;
    break;
  }
  default:
    expr_compute_restype(gen,expr->r.test,RESTYPE_GENERAL);
    expr_compute_restype(gen,expr->r.left,RESTYPE_GENERAL);
    expr_compute_restype(gen,expr->r.right,RESTYPE_GENERAL);

    expr_compute_restype(gen,expr->r.name_avt,RESTYPE_GENERAL);
    expr_compute_restype(gen,expr->r.value_avt,RESTYPE_GENERAL);
    expr_compute_restype(gen,expr->r.namespace_avt,RESTYPE_GENERAL);

    for (c = expr->r.children; c; c = c->next)
      expr_compute_restype(gen,c,RESTYPE_GENERAL);
    for (c = expr->r.attributes; c; c = c->next)
      expr_compute_restype(gen,c,RESTYPE_GENERAL);

    expr->restype = RESTYPE_GENERAL;
    break;
  }
  stack_pop(gen->typestack);
  assert(RESTYPE_UNKNOWN != expr->restype);
}
