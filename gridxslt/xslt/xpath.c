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

#define _XSLT_XPATH_C

#include "xpath.h"
#include "xslt.h"
#include "util/namespace.h"
#include "util/macros.h"
#include "util/debug.h"
#include <stdlib.h>
#include <stdio.h>
#include <assert.h>
#include <ctype.h>
#include <string.h>

extern int lex_lineno;
extern int parse_firstline;
extern char *parse_filename;

const char *xp_expr_types[XPATH_EXPR_COUNT] = {
  "invalid",
  "for",
  "some",
  "every",
  "if",
  "var_in",
  "or",
  "and",
  "compare_values",
  "compare_general",
  "compare_nodes",
  "to",
  "add",
  "subtract",
  "multiply",
  "divide",
  "idivide",
  "mod",
  "union",
  "union2",
  "intersect",
  "except",
  "instance_of",
  "treat",
  "castable",
  "cast",
  "unary_minus",
  "unary_plus",
  "root",
  "string_literal",
  "integer_literal",
  "decimal_literal",
  "double_literal",
  "var_ref",
  "empty",
  "context_item",
  "node_test",
  "actual_param",
  "function_call",
  "paren",
  "atomic_type",
  "item_type",
  "sequence",
  "step",
  "varinlist",
  "paramlist",
  "filter",
};

const char *xp_axis_types[AXIS_COUNT] = {
  NULL,
  "child",
  "descendant",
  "attribute",
  "self",
  "descendant-or-self",
  "following-sibling",
  "following",
  "namespace",
  "parent",
  "ancestor",
  "preceding-sibling",
  "preceding",
  "ancestor-or-self",
};

xp_expr *xp_expr_new(int type, xp_expr *left, xp_expr *right)
{
  xp_expr *xp = (xp_expr*)calloc(1,sizeof(xp_expr));
  xp->type = type;
  xp->left = left;
  xp->right = right;
  xp->sloc.uri = parse_filename ? strdup(parse_filename) : NULL;
  xp->sloc.line = parse_firstline+lex_lineno;
  return xp;
}

xp_expr *xp_expr_parse(const char *str, const char *filename, int baseline, error_info *ei,
                       int pattern)
{
  int r;
  char *source = (char*)malloc(strlen("__just_pattern ")+strlen(str)+1);
  xp_expr *expr = NULL;
  if (pattern)
    sprintf(source,"__just_pattern %s",str);
  else
    sprintf(source,"__just_expr %s",str);
  r = parse_xl_syntax(source,filename,baseline,ei,&expr,NULL,NULL);
  free(source);
  if (0 != r)
    return NULL;
  assert(NULL != expr);
  return expr;
}

seqtype *seqtype_parse(const char *str, const char *filename, int baseline, error_info *ei)
{
  int r;
  char *source = (char*)malloc(strlen("__just_seqtype ")+strlen(str)+1);
  seqtype *st = NULL;
  sprintf(source,"__just_seqtype %s",str);
  r = parse_xl_syntax(source,filename,baseline,ei,NULL,NULL,&st);
  free(source);
  if (0 != r)
    return NULL;
  assert(NULL != st);
  return st;
}

void xp_expr_free(xp_expr *xp)
{
  if (NULL != xp->conditional)
    xp_expr_free(xp->conditional);
  if (NULL != xp->left)
    xp_expr_free(xp->left);
  if (NULL != xp->right)
    xp_expr_free(xp->right);
  if (NULL != xp->st)
    seqtype_deref(xp->st);
  qname_free(xp->qn);
  qname_free(xp->type_qname);
  nsname_free(xp->ident);
  sourceloc_free(xp->sloc);
  free(xp->strval);
  free(xp);
}

void xp_set_parent(xp_expr *e, xp_expr *parent, xl_snode *stmt)
{
  e->parent = parent;
  e->stmt = stmt;

  if (NULL != e->conditional)
    xp_set_parent(e->conditional,e,stmt);
  if (NULL != e->left)
    xp_set_parent(e->left,e,stmt);
  if (NULL != e->right)
    xp_set_parent(e->right,e,stmt);
}

int xp_expr_resolve(xp_expr *e, xs_schema *s, const char *filename, error_info *ei)
{
  assert(e->stmt);

  assert(NULL == e->ident.name);
  assert(NULL == e->ident.ns);

  if (!qname_isnull(e->qn)) {
    e->ident = qname_to_nsname(e->stmt->namespaces,e->qn);
    if (nsname_isnull(e->ident))
      return error(ei,e->sloc.uri,e->sloc.line,"XPST0081",
                   "Could not resolve namespace for prefix \"%s\"",e->qn.prefix);
  }


  if (NULL != e->conditional)
    CHECK_CALL(xp_expr_resolve(e->conditional,s,filename,ei))
  if (NULL != e->left)
    CHECK_CALL(xp_expr_resolve(e->left,s,filename,ei))
  if (NULL != e->right)
    CHECK_CALL(xp_expr_resolve(e->right,s,filename,ei))

  if (NULL != e->st)
    CHECK_CALL(seqtype_resolve(e->st,e->stmt->namespaces,s,e->sloc.uri,e->sloc.line,ei))

  return 0;
}

void print_binary_expr(stringbuf *buf, xp_expr *expr, char *op)
{
  xp_expr_serialize(buf,expr->left,1);
  stringbuf_format(buf," %s ",op);
  xp_expr_serialize(buf,expr->right,1);
}

void print_qname(stringbuf *buf, qname qn)
{
  /* FIXME: make sure we're printing "*" when we're supposed to (wildcards); below just supports
     the case of localpart="*" */
  if (qname_isnull(qn))
    stringbuf_format(buf,"*");
  else
    stringbuf_format(buf,"%#n",qn);
}

void print_qname_or_double_wildcard(stringbuf *buf, qname qn)
{
  if (qn.prefix)
    stringbuf_format(buf,"%s:",qn.prefix);
  else
    stringbuf_format(buf,"*:");
  if (qn.localpart)
    stringbuf_format(buf,"%s",qn.localpart);
  else
    stringbuf_format(buf,"*");
}

void print_qname_or_wildcard(stringbuf *buf, qname qn)
{
  if ((NULL == qn.prefix) && (NULL == qn.localpart))
    stringbuf_format(buf,"*");
  else
    print_qname(buf,qn);
}


void xp_expr_serialize(stringbuf *buf, xp_expr *e, int brackets)
{
/*   xp_expr *f; */

  switch (e->type) {
  case XPATH_EXPR_INVALID:
    fprintf(stderr,"Invalid expression type\n");
    assert(0);
    break;
  case XPATH_EXPR_FOR:
    stringbuf_format(buf,"for ");
    xp_expr_serialize(buf,e->conditional,1);
    stringbuf_format(buf," return ");
    xp_expr_serialize(buf,e->left,1);
    break;
  case XPATH_EXPR_SOME:
    stringbuf_format(buf,"some ");
    xp_expr_serialize(buf,e->conditional,1);
    stringbuf_format(buf," satisfies ");
    xp_expr_serialize(buf,e->left,1);
    break;
  case XPATH_EXPR_EVERY:
    stringbuf_format(buf,"every ");
    xp_expr_serialize(buf,e->conditional,1);
    stringbuf_format(buf," satisfies ");
    xp_expr_serialize(buf,e->left,1);
    break;
  case XPATH_EXPR_IF:
    stringbuf_format(buf,"if (");
    xp_expr_serialize(buf,e->conditional,1);
    stringbuf_format(buf,") then ");
    xp_expr_serialize(buf,e->left,1);
    stringbuf_format(buf," else ");
    xp_expr_serialize(buf,e->right,1);
    break;
  case XPATH_EXPR_VAR_IN:
    stringbuf_format(buf,"$");
    print_qname(buf,e->qn);
    stringbuf_format(buf," in ");
    xp_expr_serialize(buf,e->left,1);
    break;
  case XPATH_EXPR_OR:
    print_binary_expr(buf,e,"or");
    break;
  case XPATH_EXPR_AND:
    print_binary_expr(buf,e,"and");
    break;
  case XPATH_EXPR_COMPARE_VALUES:
    switch (e->compare) {
    case XPATH_VALUE_COMP_EQ:
      print_binary_expr(buf,e,"eq");
      break;
    case XPATH_VALUE_COMP_NE:
      print_binary_expr(buf,e,"ne");
      break;
    case XPATH_VALUE_COMP_LT:
      print_binary_expr(buf,e,"lt");
      break;
    case XPATH_VALUE_COMP_LE:
      print_binary_expr(buf,e,"le");
      break;
    case XPATH_VALUE_COMP_GT:
      print_binary_expr(buf,e,"gt");
      break;
    case XPATH_VALUE_COMP_GE:
      print_binary_expr(buf,e,"ge");
      break;
    default:
      assert(0);
      break;
    }
    break;
  case XPATH_EXPR_COMPARE_GENERAL:
    switch (e->compare) {
    case XPATH_GENERAL_COMP_EQ:
      print_binary_expr(buf,e,"=");
      break;
    case XPATH_GENERAL_COMP_NE:
      print_binary_expr(buf,e,"!=");
      break;
    case XPATH_GENERAL_COMP_LT:
      print_binary_expr(buf,e,"<");
      break;
    case XPATH_GENERAL_COMP_LE:
      print_binary_expr(buf,e,"<=");
      break;
    case XPATH_GENERAL_COMP_GT:
      print_binary_expr(buf,e,">");
      break;
    case XPATH_GENERAL_COMP_GE:
      print_binary_expr(buf,e,">=");
      break;
    default:
      assert(0);
      break;
    }
    break;
  case XPATH_EXPR_COMPARE_NODES:
    switch (e->compare) {
    case XPATH_NODE_COMP_IS:
      print_binary_expr(buf,e,"is");
      break;
    case XPATH_NODE_COMP_PRECEDES:
      print_binary_expr(buf,e,"<<");
      break;
    case XPATH_NODE_COMP_FOLLOWS:
      print_binary_expr(buf,e,">>");
      break;
    default:
      assert(0);
      break;
    }
    break;
  case XPATH_EXPR_TO:
    print_binary_expr(buf,e,"to");
    break;
  case XPATH_EXPR_ADD:
    print_binary_expr(buf,e,"+");
    break;
  case XPATH_EXPR_SUBTRACT:
    print_binary_expr(buf,e,"-");
    break;
  case XPATH_EXPR_MULTIPLY:
    print_binary_expr(buf,e,"*");
    break;
  case XPATH_EXPR_DIVIDE:
    print_binary_expr(buf,e,"div");
    break;
  case XPATH_EXPR_IDIVIDE:
    print_binary_expr(buf,e,"idiv");
    break;
  case XPATH_EXPR_MOD:
    print_binary_expr(buf,e,"mod");
    break;
  case XPATH_EXPR_UNION:
    print_binary_expr(buf,e,"union");
    break;
  case XPATH_EXPR_UNION2:
    print_binary_expr(buf,e,"|");
    break;
  case XPATH_EXPR_INTERSECT:
    print_binary_expr(buf,e,"intersect");
    break;
  case XPATH_EXPR_EXCEPT:
    print_binary_expr(buf,e,"except");
    break;
  case XPATH_EXPR_INSTANCE_OF:
    xp_expr_serialize(buf,e->left,0);
    stringbuf_format(buf," instance of ");
    seqtype_print_xpath(buf,e->st,e->stmt->namespaces);
    break;
  case XPATH_EXPR_TREAT:
    /* FIXME */
    break;
  case XPATH_EXPR_CASTABLE:
    /* FIXME */
    break;
  case XPATH_EXPR_CAST:
    /* FIXME */
    break;
  case XPATH_EXPR_UNARY_MINUS:
    stringbuf_format(buf,"-");
    xp_expr_serialize(buf,e->left,1);
    break;
  case XPATH_EXPR_UNARY_PLUS:
    stringbuf_format(buf,"+");
    xp_expr_serialize(buf,e->left,1);
    break;
  case XPATH_EXPR_ROOT:
    stringbuf_format(buf,"/");
    if (NULL != e->left)
      xp_expr_serialize(buf,e->left,1);
    break;
  case XPATH_EXPR_STRING_LITERAL:
    /* FIXME: quote ' (and other?) characters inside string */
    stringbuf_format(buf,"'%s'",e->strval);
    break;
  case XPATH_EXPR_INTEGER_LITERAL:
    stringbuf_format(buf,"%d",e->ival);
    break;
  case XPATH_EXPR_DECIMAL_LITERAL:
    stringbuf_format(buf,"%f",e->dval);
    break;
  case XPATH_EXPR_DOUBLE_LITERAL:
    stringbuf_format(buf,"%f",e->dval);
    break;
  case XPATH_EXPR_VAR_REF:
    stringbuf_format(buf,"$");
    print_qname(buf,e->qn);
    break;
  case XPATH_EXPR_EMPTY:
    break;
  case XPATH_EXPR_CONTEXT_ITEM:
    stringbuf_format(buf,".");
    break;
  case XPATH_EXPR_NODE_TEST:

    if ((AXIS_PARENT == e->axis) &&
        (XPATH_NODE_TEST_SEQTYPE == e->nodetest) &&
        (e->st->isnode)) {
      /* abbreviated form .. for parent::node() */
      stringbuf_format(buf,"..");
    }
    else {

      switch (e->axis) {
      case AXIS_CHILD:
        /* nothing, since child is default axis */
        break;
      case AXIS_DESCENDANT:
        stringbuf_format(buf,"descendant::");
        break;
      case AXIS_ATTRIBUTE:
        /* use abbreviation @ instead of attribute:: */
        stringbuf_format(buf,"@");
        break;
      case AXIS_SELF:
        stringbuf_format(buf,"self::");
        break;
      case AXIS_DESCENDANT_OR_SELF:
        stringbuf_format(buf,"descendant-or-self::");
        break;
      case AXIS_FOLLOWING_SIBLING:
        stringbuf_format(buf,"following-sibling::");
        break;
      case AXIS_FOLLOWING:
        stringbuf_format(buf,"following::");
        break;
      case AXIS_NAMESPACE:
        stringbuf_format(buf,"namespace::");
        break;
      case AXIS_PARENT:
        stringbuf_format(buf,"parent::");
        break;
      case AXIS_ANCESTOR:
        stringbuf_format(buf,"ancestor::");
        break;
      case AXIS_PRECEDING_SIBLING:
        stringbuf_format(buf,"preceding-sibling::");
        break;
      case AXIS_PRECEDING:
        stringbuf_format(buf,"preceding::");
        break;
      case AXIS_ANCESTOR_OR_SELF:
        stringbuf_format(buf,"ancestor-or-self::");
        break;
      default:
        fprintf(stderr,"unknown axis %d\n",e->axis);
        assert(0);
        break;
      }
      switch (e->nodetest) {
      case XPATH_NODE_TEST_NAME:
        print_qname(buf,e->qn);
        break;
      case XPATH_NODE_TEST_SEQTYPE:
        /* FIXME */
        assert(!"not yet implemented");
        break;
      default:
        assert(0);
        break;
      }
    }
    break;
  case XPATH_EXPR_ACTUAL_PARAM:
    xp_expr_serialize(buf,e->left,0);
    break;
  case XPATH_EXPR_FUNCTION_CALL:
    print_qname(buf,e->qn);
    stringbuf_format(buf,"(");
    if (e->left)
      xp_expr_serialize(buf,e->left,1);
    stringbuf_format(buf,")");
    break;
  case XPATH_EXPR_PAREN:
    stringbuf_format(buf,"(");
    xp_expr_serialize(buf,e->left,1);
    stringbuf_format(buf,")");
    break;

  case XPATH_EXPR_ATOMIC_TYPE:
    assert(!"not yet implemented");
    break;
  case XPATH_EXPR_ITEM_TYPE:
    assert(!"not yet implemented");
    break;
  case XPATH_EXPR_SEQUENCE:
    xp_expr_serialize(buf,e->left,1);
    stringbuf_format(buf,", ");
    xp_expr_serialize(buf,e->right,1);
    break;
  case XPATH_EXPR_STEP:
    xp_expr_serialize(buf,e->left,1);
    stringbuf_format(buf,"/");
    xp_expr_serialize(buf,e->right,1);
    break;
  case XPATH_EXPR_VARINLIST:
    xp_expr_serialize(buf,e->left,1);
    stringbuf_format(buf,", ");
    xp_expr_serialize(buf,e->right,1);
    break;
  case XPATH_EXPR_PARAMLIST:
    xp_expr_serialize(buf,e->left,1);
    stringbuf_format(buf,", ");
    xp_expr_serialize(buf,e->right,1);
    break;
  case XPATH_EXPR_FILTER:
    assert(!"not yet implemented");
    break;

  default:
    fprintf(stderr,"Unknown expression type %d\n",e->type);
    assert(0);
    break;
  }
/*   for (f = e->filter; f; f = f->filter) { */
/*     xp_expr *x = f->filter; */
/*     stringbuf_format(buf,"["); */
/*     f->filter = NULL; */
/*     xp_expr_serialize(buf,f,1); */
/*     f->filter = x; */
/*     stringbuf_format(buf,"]"); */
/*   } */



/*   if (brackets) */
/*     stringbuf_format(buf,")"); */
}

void xp_expr_print_tree(xp_expr *e, int indent)
{
  print("%p %#i%s %#n\n",e,2*indent,xp_expr_types[e->type],e->ident);
  if (e->conditional)
    xp_expr_print_tree(e->conditional,indent+1);
  if (e->left)
    xp_expr_print_tree(e->left,indent+1);
  if (e->right)
    xp_expr_print_tree(e->right,indent+1);
}

/*

  @implements(xpath20:id-normative-references-1) status { info } @end
  @implements(xpath20:id-non-normative-references-1) status { info } @end
  @implements(xpath20:id-background-material-1) status { info } @end

  @implements(xpath20:id-glossary-1) status { info } @end

  @implements(xpath20:id-revisions-log-1) status { info } @end
  @implements(xpath20:N14CF6-1) status { info } @end
  @implements(xpath20:N14D62-1) status { info } @end
  @implements(xpath20:N14DA3-1) status { info } @end
  @implements(xpath20:N14DBC-1) status { info } @end
  @implements(xpath20:N14DD9-1) status { info } @end

*/
