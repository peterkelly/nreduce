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

#define _TREE_C

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>
#include <assert.h>
#include <libxml/parser.h>
#include "cxslt.h"

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

const char *xpath_names[XPATH_COUNT] = {
  "invalid",
  "for",
  "some",
  "every",
  "if",
  "var-in",
  "or",
  "and",

  "general-eq",
  "general-ne",
  "general-lt",
  "general-le",
  "general-gt",
  "general-ge",

  "value-eq",
  "value-ne",
  "value-lt",
  "value-le",
  "value-gt",
  "value-ge",

  "node-is",
  "node-precedes",
  "node-follows",

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
  "instance-of",
  "treat",
  "castable",
  "cast",
  "unary-minus",
  "unary-plus",
  "root",
  "string-literal",
  "integer-literal",
  "decimal-literal",
  "double-literal",
  "var-ref",
  "empty",
  "context-item",
  "kind-test",
  "actual-param",
  "function-call",
  "paren",
  "atomic-type",
  "item-type",
  "sequence",
  "step",
  "varinlist",
  "paramlist",
  "filter",
  "name-test",

  "avt-component",
  "forward-axis-step",
  "reverse-axis-step",
};

const char *xslt_names[XSLT_COUNT] = {
  "invalid",
  "stylesheet",
  "function",
  "template",
  "variable",
  "sequence",
  "value-of",
  "text",
  "for-each",
  "if",
  "choose",
  "element",
  "attribute",
  "inamespace",
  "apply-templates",
  "literal-result-element",
  "literal-text-node",
  "param",
  "output",
  "strip-space",
  "when",
  "otherwise",
  "literal-attribute",
};

const char *axis_names[AXIS_COUNT] = {
  "invalid",
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

const char *kind_names[KIND_COUNT] = {
  "invalid",
  "document",
  "element",
  "attribute",
  "schema-element",
  "schema-attribute",
  "pi",
  "comment",
  "text",
  "any",
};

static int ignore_node(xmlNodePtr n)
{
  const char *c;
  if (XML_COMMENT_NODE == n->type)
    return 1;
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

void free_expression(expression *expr)
{
  if (NULL == expr)
    return;
  free_expression(expr->left);
  free_expression(expr->right);
  free(expr->str);
  free_qname(expr->qn);
  free(expr);
}

xsltnode *new_xsltnode(xmlNodePtr n, int type)
{
  xsltnode *xn = (xsltnode*)calloc(1,sizeof(xsltnode));
  xn->n = n;
  xn->type = type;
  return xn;
}

void free_xsltnode(xsltnode *xn)
{
  xsltnode *c;
  for (c = xn->children.first; c; c = c->next)
    free_xsltnode(c);
  for (c = xn->attributes.first; c; c = c->next)
    free_xsltnode(c);
  free_qname(xn->name_qn);
  free(xn->name_ident);
  free_expression(xn->expr);
  free_expression(xn->name_avt);
  free_expression(xn->value_avt);
  free_expression(xn->namespace_avt);
  free(xn);
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

static int parse_expr_attr(elcgen *gen, xmlNodePtr n, const char *attr,
                           expression **expr, int required)
{
  char *value;
  if (!xmlHasProp(n,attr)) {
    *expr = NULL;
    if (required)
      return gen_error(gen,"%s element missing %s attribute",n->name,attr);
    else
      return 1;
  }

  value = xmlGetProp(n,attr);
  *expr = parse_xpath(gen,n,value);
  free(value);

  return (NULL != *expr);
}

static int parse_avt(elcgen *gen, xmlNodePtr n, const char *attr,
                     expression **expr, int required)
{
  char *value;
  char *tmp;
  if (!xmlHasProp(n,attr)) {
    *expr = NULL;
    if (required)
      return gen_error(gen,"%s element missing %s attribute",n->name,attr);
    else
      return 1;
  }

  value = xmlGetProp(n,attr);
  tmp = (char*)malloc(strlen(value)+3);
  sprintf(tmp,"}%s{",value);
  *expr = parse_xpath(gen,n,tmp);
  free(tmp);
  free(value);

  return (NULL != *expr);
}

static int parse_name(elcgen *gen, xmlNodePtr n, xsltnode *xn, int required)
{
  char *name;
  if (!xmlHasProp(n,"name")) {
    if (required)
      return gen_error(gen,"%s element missing name attribute",n->name);
    else
      return 1;
  }

  name = xmlGetProp(n,"name");
  xn->name_qn = string_to_qname(name,n);
  xn->name_ident = nsname_to_ident(xn->name_qn.uri,xn->name_qn.localpart);
  free(name);

  return 1;
}

static xsltnode *add_child(xsltnode *parent, xmlNodePtr n, int type)
{
  xsltnode *xn = new_xsltnode(n,type);
  xn->parent = parent;
  llist_append(&parent->children,xn);
  return xn;
}

static xsltnode *add_attribute(xsltnode *parent, xmlNodePtr n, int type)
{
  xsltnode *xn = new_xsltnode(n,type);
  xn->parent = parent;
  llist_append(&parent->attributes,xn);
  return xn;
}

static int parse_attributes(elcgen *gen, xmlNodePtr elem, xsltnode *pnode)
{
  xmlAttrPtr attr;

  for (attr = pnode->n->properties; attr; attr = attr->next) {
    if ((attr->ns && strcmp((char*)attr->ns->href,XSLT_NAMESPACE)) ||
        (NULL == attr->ns)) {
      char *value = xmlNodeListGetString(gen->parse_doc,attr->children,1);
      char *tmp = (char*)malloc(strlen(value)+3);

      xsltnode *newnode = add_attribute(pnode,pnode->n,XSLT_LITERAL_ATTRIBUTE);

      sprintf(tmp,"}%s{",value);
      newnode->name_qn.prefix = attr->ns ? strdup((char*)attr->ns->prefix) : strdup("");
      newnode->name_qn.uri = attr->ns ? strdup((char*)attr->ns) : strdup("");
      newnode->name_qn.localpart = strdup((char*)attr->name);
      newnode->name_ident = nsname_to_ident(newnode->name_qn.uri,newnode->name_qn.localpart);
      newnode->value_avt  = parse_xpath(gen,pnode->n,tmp);
      free(tmp);
      free(value);

      if (NULL == newnode->value_avt)
        return 0;
    }

  }
  return 1;
}

int parse_xslt(elcgen *gen, xmlNodePtr parent, xsltnode *pnode)
{
  xmlNodePtr n;
  int r = 1;
  for (n = parent->children; n && r; n = n->next) {
    xsltnode *newnode = NULL;
    if (ignore_node(n))
      continue;
    if (XML_ELEMENT_NODE == n->type) {
      if (n->ns && !xmlStrcmp(n->ns->href,XSLT_NAMESPACE)) {
        if (!xmlStrcmp(n->name,"output")) {
          newnode = add_child(pnode,n,XSLT_OUTPUT);
        }
        else if (!xmlStrcmp(n->name,"strip-space")) {
          newnode = add_child(pnode,n,XSLT_STRIP_SPACE);
        }
        else if (!xmlStrcmp(n->name,"function")) {
          newnode = add_child(pnode,n,XSLT_FUNCTION);
          r = r && parse_name(gen,n,newnode,1);
        }
        else if (!xmlStrcmp(n->name,"template")) {
          newnode = add_child(pnode,n,XSLT_TEMPLATE);
          r = r && parse_expr_attr(gen,n,"match",&newnode->expr,0);
          r = r && parse_name(gen,n,newnode,0);
        }
        else if (!xmlStrcmp(n->name,"param")) {
          newnode = add_child(pnode,n,XSLT_PARAM);
          r = r && parse_name(gen,n,newnode,1);
        }
        else if (!xmlStrcmp(n->name,"when")) {
          newnode = add_child(pnode,n,XSLT_WHEN);
          r = r && parse_expr_attr(gen,n,"test",&newnode->expr,1);
        }
        else if (!xmlStrcmp(n->name,"otherwise")) {
          newnode = add_child(pnode,n,XSLT_OTHERWISE);
        }
        else if (!xmlStrcmp(n->name,"variable")) {
          newnode = add_child(pnode,n,XSLT_VARIABLE);
          r = r && parse_expr_attr(gen,n,"select",&newnode->expr,0);
          r = r && parse_name(gen,n,newnode,1);
        }
        else if (!xmlStrcmp(n->name,"sequence")) {
          newnode = add_child(pnode,n,XSLT_SEQUENCE);
          r = r && parse_expr_attr(gen,n,"select",&newnode->expr,1);
        }
        else if (!xmlStrcmp(n->name,"value-of")) {
          newnode = add_child(pnode,n,XSLT_VALUE_OF);
          r = r && parse_expr_attr(gen,n,"select",&newnode->expr,0);
        }
        else if (!xmlStrcmp(n->name,"text")) {
          newnode = add_child(pnode,n,XSLT_TEXT);
        }
        else if (!xmlStrcmp(n->name,"for-each")) {
          newnode = add_child(pnode,n,XSLT_FOR_EACH);
          r = r && parse_expr_attr(gen,n,"select",&newnode->expr,1);
        }
        else if (!xmlStrcmp(n->name,"if")) {
          newnode = add_child(pnode,n,XSLT_IF);
          r = r && parse_expr_attr(gen,n,"test",&newnode->expr,1);
        }
        else if (!xmlStrcmp(n->name,"choose")) {
          newnode = add_child(pnode,n,XSLT_CHOOSE);
        }
        else if (!xmlStrcmp(n->name,"element")) {
          newnode = add_child(pnode,n,XSLT_ELEMENT);
          r = r && parse_avt(gen,n,"name",&newnode->name_avt,1);
          r = r && parse_avt(gen,n,"namespace",&newnode->namespace_avt,0);
        }
        else if (!xmlStrcmp(n->name,"attribute")) {
          newnode = add_child(pnode,n,XSLT_ATTRIBUTE);
          r = r && parse_expr_attr(gen,n,"select",&newnode->expr,0);
          r = r && parse_avt(gen,n,"name",&newnode->name_avt,1);
        }
        else if (!xmlStrcmp(n->name,"namespace")) {
          newnode = add_child(pnode,n,XSLT_INAMESPACE);
          r = r && parse_expr_attr(gen,n,"select",&newnode->expr,0);
          r = r && parse_avt(gen,n,"name",&newnode->name_avt,1);
        }
        else if (!xmlStrcmp(n->name,"apply-templates")) {
          newnode = add_child(pnode,n,XSLT_APPLY_TEMPLATES);
          r = r && parse_expr_attr(gen,n,"select",&newnode->expr,0);
        }
        else {
          r = gen_error(gen,"Unsupported XSLT instruction: %s",n->name);
        }
      }
      else {
        newnode = add_child(pnode,n,XSLT_LITERAL_RESULT_ELEMENT);
        r = r && parse_attributes(gen,n,newnode);
      }
    }
    else if (XML_TEXT_NODE == n->type) {
      newnode = add_child(pnode,n,XSLT_LITERAL_TEXT_NODE);
    }
    if (newnode) {
      if (!parse_xslt(gen,n,newnode))
        return 0;
    }
  }
  return r;
}

static xsltnode *lookup_var_ref(xsltnode *xn, qname qn)
{
  for (; xn; xn = xn->parent) {
    xsltnode *xv;
    for (xv = xn->prev; xv; xv = xv->prev) {
      if (((XSLT_VARIABLE == xv->type) || (XSLT_PARAM == xv->type)) &&
          qname_equals(xv->name_qn,qn))
        return xv;
    }
  }
  return NULL;
}

xsltnode *lookup_function(elcgen *gen, qname qn)
{
  xsltnode *c;
  for (c = gen->root->children.first; c; c = c->next)
    if ((XSLT_FUNCTION == c->type) && qname_equals(c->name_qn,qn))
      return c;
  return NULL;
}

static int xpath_resolve_vars(elcgen *gen, xsltnode *xn, expression *expr)
{
  int r = 1;
  if (NULL == expr)
    return 1;
  r = r && xpath_resolve_vars(gen,xn,expr->test);
  r = r && xpath_resolve_vars(gen,xn,expr->left);
  r = r && xpath_resolve_vars(gen,xn,expr->right);
  if (XPATH_VAR_REF == expr->type) {
    expr->target = lookup_var_ref(xn,expr->qn);
    if (NULL == expr->target) {
      if (strcmp(expr->qn.uri,""))
        return gen_error(gen,"variable {%s}%s not found",expr->qn.uri,expr->qn.localpart);
      else
        return gen_error(gen,"variable %s not found",expr->qn.localpart);
    }
  }
  else if (XPATH_FUNCTION_CALL == expr->type) {
    expr->target = lookup_function(gen,expr->qn);
    /* failed lookup is ok, might be to built-in function or web service */
  }
  return r;
}

static int xslt_semantic_check(elcgen *gen, xsltnode *xn)
{
  int r = 1;
  switch (xn->type) {
  case XSLT_CHOOSE: {
    xsltnode *xchild;
    int otherwise = 0;
    int whencount = 0;
    for (xchild = xn->children.first; xchild; xchild = xchild->next) {
      if (otherwise)
        return gen_error(gen,"choose contains an element after otherwise: %s",
                         xslt_names[xchild->type]);
      if ((XSLT_WHEN != xchild->type) && (XSLT_OTHERWISE != xchild->type))
        return gen_error(gen,"choose element contains invalid child: %s",
                         xslt_names[xchild->type]);
      if (XSLT_WHEN == xchild->type)
        whencount++;
      if (XSLT_OTHERWISE == xchild->type)
        otherwise = 1;
    }
    if (0 == whencount)
      return gen_error(gen,"choose must contain at least one when");
    break;
  }
  }
  return r;
}

int xslt_resolve_vars(elcgen *gen, xsltnode *xn)
{
  xsltnode *c;
  int r = 1;
  if (NULL == xn)
    return 1;
  r = r && xpath_resolve_vars(gen,xn,xn->expr);
  r = r && xpath_resolve_vars(gen,xn,xn->name_avt);
  r = r && xpath_resolve_vars(gen,xn,xn->value_avt);
  r = r && xpath_resolve_vars(gen,xn,xn->namespace_avt);
  for (c = xn->children.first; c && r; c = c->next)
    r = r && xslt_resolve_vars(gen,c);
  for (c = xn->attributes.first; c && r; c = c->next)
    r = r && xslt_resolve_vars(gen,c);
  r = r && xslt_semantic_check(gen,xn);
  return r;
}

int exclude_namespace(elcgen *gen, xsltnode *xn2, const char *uri)
{
  int found = 0;
  xsltnode *xp;

  if (!strcmp(uri,XSLT_NAMESPACE))
    return 1;

  for (xp = xn2; xp && !found; xp = xp->parent) {
    char *str;
    if (xp->n->ns && !strcmp((char*)xp->n->ns->href,XSLT_NAMESPACE))
      str = xmlGetNsProp(xp->n,"exclude-result-prefixes",NULL);
    else
      str = xmlGetNsProp(xp->n,"exclude-result-prefixes",XSLT_NAMESPACE);
    if (str) {
      int end = 0;
      char *start = str;
      char *c;

      for (c = str; !found && !end; c++) {
        end = ('\0' == *c);
        if (end || isspace(*c)) {
          if (c > start) {
            xmlNsPtr ns;
            *c = '\0';

            if (!strcmp(start,"#all")) {
              found = (NULL != xmlSearchNsByHref(gen->parse_doc,xp->n,(xmlChar*)uri));
            }
            else if (!strcmp(start,"#default")) {
              found = ((NULL != xp->n->ns) && !strcmp((char*)xp->n->ns->href,uri));
            }
            else {
              ns = xmlSearchNs(gen->parse_doc,xp->n,(xmlChar*)start);
              found = ((NULL != ns) && !strcmp((char*)ns->href,uri));
            }
          }
          start = c+1;
        }
      }
      free(str);
    }
  }
  return found;
}
