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
