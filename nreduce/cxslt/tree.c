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

const char *expr_names[XPATH_COUNT] = {
  /* xpath */
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
  "numeric-literal",
  "last-predicate",
  "predicate",
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
  "node-test",
  "unused2",

  "dsvar",
  "dsvar-numeric",
  "dsvar-string",
  "dsvar-var-ref",
  "dsvar-nodetest",
  "axis",

  /* xslt */
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
  "namespace",
  "apply-templates",
  "literal-result-element",
  "literal-text-node",
  "param",
  "output",
  "strip-space",
  "when",
  "otherwise",
  "literal-attribute",

  "dsvar-instruction",
  "dsvar-text",
  "dsvar-litelem",
  "dsvar-textinstr",
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
  "dsvar-forward",
  "dsvar-reverse",
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

const char *restype_names[RESTYPE_COUNT] = {
  "UNKNOWN",
  "RECURSIVE",
  "GENERAL",
  "NUMBER",
  "NUMLIST",
};

const char *expr_refnames[EXPR_REFCOUNT] = {
  "->r.test",
  "->r.left",
  "->r.right",
  "->r.name_avt",
  "->r.value_avt",
  "->r.namespace_avt",
  "->r.children",
  "->r.attributes",
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
  expr->r.left = left;
  expr->r.right = right;
  return expr;
}

void free_expression(expression *expr)
{
  expression *c;

  if (NULL == expr)
    return;

  free_qname(expr->qn);
  free(expr->str);
  free(expr->ident);
  free(expr->orig);

  free_expression(expr->r.test);
  free_expression(expr->r.left);
  free_expression(expr->r.right);

  free_expression(expr->r.name_avt);
  free_expression(expr->r.value_avt);
  free_expression(expr->r.namespace_avt);

  for (c = expr->r.children; c; c = c->next)
    free_expression(c);
  for (c = expr->r.attributes; c; c = c->next)
    free_expression(c);

  free(expr);
}

expression *new_xsltnode(xmlNodePtr n, int type)
{
  expression *expr = (expression*)calloc(1,sizeof(expression));
  expr->xmlnode = n;
  expr->type = type;
  return expr;
}

static pthread_mutex_t xpath_lock = PTHREAD_MUTEX_INITIALIZER;

int parse_firstline = 0;
expression *parse_expr = NULL;
xmlNodePtr parse_node = NULL;

void expr_set_parents(expression *expr, expression *parent)
{
  expression *prev = NULL;

  while (NULL != expr) {
    expr->parent = parent;
    expr->prev = prev;

    expr_set_parents(expr->r.test,expr);
    expr_set_parents(expr->r.left,expr);
    expr_set_parents(expr->r.right,expr);

    expr_set_parents(expr->r.name_avt,expr);
    expr_set_parents(expr->r.value_avt,expr);
    expr_set_parents(expr->r.namespace_avt,expr);

    expr_set_parents(expr->r.children,expr);
    expr_set_parents(expr->r.attributes,expr);

    prev = expr;
    expr = expr->next;
  }
}

expression *parse_xpath(elcgen *gen, expression *pnode, const char *str)
{
  X_BUFFER_STATE bufstate;
  int r;
  expression *expr;

  pthread_mutex_lock(&xpath_lock);

  parse_node = pnode ? pnode->xmlnode : NULL;
  parse_firstline = parse_node ? parse_node->line : 0;
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
  else
    expr_set_parents(expr,pnode);

  return expr;
}

static int parse_expr_attr(elcgen *gen, xmlNodePtr n, const char *attr,
                           expression **expr, int required, expression *pnode)
{
  char *value;
  assert(pnode->xmlnode == n);
  if (!xmlHasProp(n,attr)) {
    *expr = NULL;
    if (required)
      return gen_error(gen,"%s element missing %s attribute",n->name,attr);
    else
      return 1;
  }

  value = xmlGetProp(n,attr);
  *expr = parse_xpath(gen,pnode,value);
  free(value);

  return (NULL != *expr);
}

static int parse_avt(elcgen *gen, xmlNodePtr n, const char *attr,
                     expression **expr, int required, expression *pnode)
{
  char *value;
  char *tmp;
  assert(pnode->xmlnode == n);
  if (!xmlHasProp(n,attr)) {
    *expr = NULL;
    if (required)
      return gen_error(gen,"%s element missing %s attribute",n->name,attr);
    else
      return 1;
  }

  value = xmlGetProp(n,attr);
  if (gen->ispattern && (!strncmp(value,"#E",2))) {
    *expr = new_expression(XPATH_DSVAR);
    (*expr)->str = strdup(value+1);
  }
  else {
    tmp = (char*)malloc(strlen(value)+3);
    sprintf(tmp,"}%s{",value);
    *expr = parse_xpath(gen,pnode,tmp);
    free(tmp);
  }
  free(value);

  return (NULL != *expr);
}

static int parse_name(elcgen *gen, xmlNodePtr n, expression *expr, int required)
{
  char *name;
  if (!xmlHasProp(n,"name")) {
    if (required)
      return gen_error(gen,"%s element missing name attribute",n->name);
    else
      return 1;
  }

  name = xmlGetProp(n,"name");
  expr->qn = string_to_qname(name,n);
  expr->ident = nsname_to_ident(expr->qn.uri,expr->qn.localpart);
  free(name);

  return 1;
}

static int parse_attributes(elcgen *gen, xmlNodePtr elem, expression *pnode)
{
  xmlAttrPtr attr;
  expression **aptr = &pnode->r.attributes;

  for (attr = pnode->xmlnode->properties; attr; attr = attr->next) {
    if ((attr->ns && strcmp((char*)attr->ns->href,XSLT_NS)) ||
        (NULL == attr->ns)) {
      char *value = xmlNodeListGetString(gen->parse_doc,attr->children,1);
      char *tmp = (char*)malloc(strlen(value)+3);

      expression *newnode = new_xsltnode(pnode->xmlnode,XSLT_LITERAL_ATTRIBUTE);
      *aptr = newnode;
      aptr = &newnode->next;

      sprintf(tmp,"}%s{",value);
      newnode->qn.prefix = attr->ns ? strdup((char*)attr->ns->prefix) : strdup("");
      newnode->qn.uri = attr->ns ? strdup((char*)attr->ns) : strdup("");
      newnode->qn.localpart = strdup((char*)attr->name);
      newnode->ident = nsname_to_ident(newnode->qn.uri,newnode->qn.localpart);
      newnode->r.value_avt  = parse_xpath(gen,newnode,tmp);
      free(tmp);
      free(value);

      if (NULL == newnode->r.value_avt)
        return 0;
    }

  }
  return 1;
}

int parse_xslt(elcgen *gen, xmlNodePtr parent, expression *pnode)
{
  xmlNodePtr n;
  int r = 1;
  expression **cptr = &pnode->r.children;
  for (n = parent->children; n && r; n = n->next) {
    expression *newnode = NULL;
    if (ignore_node(n))
      continue;
    if (XML_ELEMENT_NODE == n->type) {
      if (n->ns && !xmlStrcmp(n->ns->href,XSLT_NS)) {
        if (!xmlStrcmp(n->name,"output")) {
          newnode = new_xsltnode(n,XSLT_OUTPUT);
        }
        else if (!xmlStrcmp(n->name,"strip-space")) {
          newnode = new_xsltnode(n,XSLT_STRIP_SPACE);
        }
        else if (!xmlStrcmp(n->name,"function")) {
          newnode = new_xsltnode(n,XSLT_FUNCTION);
          r = r && parse_name(gen,n,newnode,1);
        }
        else if (!xmlStrcmp(n->name,"template")) {
          newnode = new_xsltnode(n,XSLT_TEMPLATE);
          newnode->orig = xmlGetProp(n,"match");
          r = r && parse_expr_attr(gen,n,"match",&newnode->r.left,0,newnode);
          r = r && parse_name(gen,n,newnode,0);
        }
        else if (!xmlStrcmp(n->name,"param")) {
          newnode = new_xsltnode(n,XSLT_PARAM);
          r = r && parse_name(gen,n,newnode,1);
        }
        else if (!xmlStrcmp(n->name,"when")) {
          newnode = new_xsltnode(n,XSLT_WHEN);
          newnode->orig = xmlGetProp(n,"test");
          r = r && parse_expr_attr(gen,n,"test",&newnode->r.left,1,newnode);
        }
        else if (!xmlStrcmp(n->name,"otherwise")) {
          newnode = new_xsltnode(n,XSLT_OTHERWISE);
        }
        else if (!xmlStrcmp(n->name,"variable")) {
          newnode = new_xsltnode(n,XSLT_VARIABLE);
          r = r && parse_expr_attr(gen,n,"select",&newnode->r.left,0,newnode);
          r = r && parse_name(gen,n,newnode,1);
        }
        else if (!xmlStrcmp(n->name,"sequence")) {
          newnode = new_xsltnode(n,XSLT_SEQUENCE);
          newnode->orig = xmlGetProp(n,"select");
          r = r && parse_expr_attr(gen,n,"select",&newnode->r.left,1,newnode);
        }
        else if (!xmlStrcmp(n->name,"value-of")) {
          newnode = new_xsltnode(n,XSLT_VALUE_OF);
          r = r && parse_expr_attr(gen,n,"select",&newnode->r.left,0,newnode);
        }
        else if (!xmlStrcmp(n->name,"text")) {
          char *str = xmlNodeListGetString(gen->parse_doc,n->children,1);
          if (!strncmp(str,"#S",2) && gen->ispattern) {
            newnode = new_xsltnode(n,XSLT_DSVAR_TEXTINSTR);
            newnode->str = strdup((char*)str+1);
            free(str);
          }
          else {
            newnode = new_xsltnode(n,XSLT_TEXT);
            newnode->str = str;
          }
        }
        else if (!xmlStrcmp(n->name,"for-each")) {
          newnode = new_xsltnode(n,XSLT_FOR_EACH);
          newnode->orig = xmlGetProp(n,"select");
          r = r && parse_expr_attr(gen,n,"select",&newnode->r.left,1,newnode);
        }
        else if (!xmlStrcmp(n->name,"if")) {
          newnode = new_xsltnode(n,XSLT_IF);
          newnode->orig = xmlGetProp(n,"test");
          r = r && parse_expr_attr(gen,n,"test",&newnode->r.left,1,newnode);
        }
        else if (!xmlStrcmp(n->name,"choose")) {
          newnode = new_xsltnode(n,XSLT_CHOOSE);
        }
        else if (!xmlStrcmp(n->name,"element")) {
          newnode = new_xsltnode(n,XSLT_ELEMENT);
          newnode->orig = xmlGetProp(n,"name");
          r = r && parse_avt(gen,n,"name",&newnode->r.name_avt,1,newnode);
          r = r && parse_avt(gen,n,"namespace",&newnode->r.namespace_avt,0,newnode);
        }
        else if (!xmlStrcmp(n->name,"attribute")) {
          newnode = new_xsltnode(n,XSLT_ATTRIBUTE);
          newnode->orig = xmlGetProp(n,"name");
          r = r && parse_expr_attr(gen,n,"select",&newnode->r.left,0,newnode);
          r = r && parse_avt(gen,n,"name",&newnode->r.name_avt,1,newnode);
        }
        else if (!xmlStrcmp(n->name,"namespace")) {
          newnode = new_xsltnode(n,XSLT_NAMESPACE);
          newnode->orig = xmlGetProp(n,"name");
          r = r && parse_expr_attr(gen,n,"select",&newnode->r.left,0,newnode);
          r = r && parse_avt(gen,n,"name",&newnode->r.name_avt,1,newnode);
        }
        else if (!xmlStrcmp(n->name,"apply-templates")) {
          newnode = new_xsltnode(n,XSLT_APPLY_TEMPLATES);
          newnode->orig = xmlGetProp(n,"select");
          r = r && parse_expr_attr(gen,n,"select",&newnode->r.left,0,newnode);
        }
        else if (!strncmp((char*)n->name,"_Y",2) && gen->ispattern) {
          newnode = new_xsltnode(n,XSLT_DSVAR_LITELEM);
/*           newnode->str = strdup((char*)n->name+1); */
          newnode->str = strdup("Y::"); /* FIXME: hack */
        }
        else {
          r = gen_error(gen,"Unsupported XSLT instruction: %s",n->name);
        }
      }
      else {
        newnode = new_xsltnode(n,XSLT_LITERAL_RESULT_ELEMENT);
        newnode->qn.uri = n->ns ? strdup((char*)n->ns->href) : strdup("");
        newnode->qn.prefix = (n->ns && n->ns->prefix) ? strdup((char*)n->ns->prefix) : strdup("");
        newnode->qn.localpart = strdup((char*)n->name);
        r = r && parse_attributes(gen,n,newnode);
      }
    }
    else if (XML_TEXT_NODE == n->type) {
      if (!strncmp((char*)n->content,"#I",2) && gen->ispattern) {
        newnode = new_xsltnode(n,XSLT_DSVAR_INSTRUCTION);
        newnode->str = strdup((char*)n->content+1);
      }
      else if (!strncmp((char*)n->content,"#S",2) && gen->ispattern) {
        newnode = new_xsltnode(n,XSLT_DSVAR_TEXT);
        newnode->str = strdup((char*)n->content+1);
      }
      else {
        newnode = new_xsltnode(n,XSLT_LITERAL_TEXT_NODE);
        newnode->str = strdup((char*)n->content);
      }
    }
    if (newnode) {

      *cptr = newnode;
      cptr = &newnode->next;

      if ((XSLT_TEXT != newnode->type) && (XSLT_DSVAR_TEXTINSTR != newnode->type)) {
        if (!parse_xslt(gen,n,newnode))
          return 0;
      }
    }
  }
  return r;
}

static expression *lookup_var_ref(expression *expr, qname qn)
{
  for (; expr; expr = expr->parent) {
    expression *xv;
    for (xv = expr->prev; xv; xv = xv->prev) {
      if (((XSLT_VARIABLE == xv->type) || (XSLT_PARAM == xv->type)) &&
          qname_equals(xv->qn,qn))
        return xv;
    }
  }
  return NULL;
}

expression *lookup_function(elcgen *gen, qname qn)
{
  expression *c;
  for (c = gen->root->r.children; c; c = c->next)
    if ((XSLT_FUNCTION == c->type) && qname_equals(c->qn,qn))
      return c;
  return NULL;
}

static int expr_resolve_ref(elcgen *gen, expression *expr)
{
  if (XPATH_VAR_REF == expr->type) {
    expr->target = lookup_var_ref(expr,expr->qn);
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
  return 1;
}

static int expr_semantic_check(elcgen *gen, expression *expr)
{
  int r = 1;
  switch (expr->type) {
  case XPATH_FUNCTION_CALL:
    if (expr->target) {
      int formalparams = 0;
      int actualparams = 0;
      expression *fp;
      expression *ap;
      for (ap = expr->r.left; ap; ap = ap->r.right) {
        assert(XPATH_ACTUAL_PARAM == ap->type);
        actualparams++;
      }
      for (fp = expr->target->r.children; fp; fp = fp->next) {
        if (XSLT_PARAM == fp->type)
          formalparams++;
        else
          break;
      }
      if (formalparams != actualparams)
        return gen_error(gen,"Insufficient arguments to function {%s}%s\n",
                         expr->target->qn.uri,expr->target->qn.localpart);
    }
    break;
  case XSLT_CHOOSE: {
    expression *xchild;
    int otherwise = 0;
    int whencount = 0;
    for (xchild = expr->r.children; xchild; xchild = xchild->next) {
      if (otherwise)
        return gen_error(gen,"choose contains an element after otherwise: %s",
                         expr_names[xchild->type]);
      if ((XSLT_WHEN != xchild->type) &&
          (XSLT_OTHERWISE != xchild->type) &&
          (XSLT_DSVAR_INSTRUCTION != xchild->type))
        return gen_error(gen,"choose element contains invalid child: %s",
                         expr_names[xchild->type]);
      if (XSLT_WHEN == xchild->type)
        whencount++;
      if (XSLT_OTHERWISE == xchild->type)
        otherwise = 1;
    }
    if ((0 == whencount) && !gen->ispattern)
      return gen_error(gen,"choose must contain at least one when");
    break;
  }
  }
  return r;
}

int expr_resolve_vars(elcgen *gen, expression *expr)
{
  expression *c;
  int r = 1;
  if (NULL == expr)
    return 1;

  r = r && expr_resolve_vars(gen,expr->r.test);
  r = r && expr_resolve_vars(gen,expr->r.left);
  r = r && expr_resolve_vars(gen,expr->r.right);

  r = r && expr_resolve_vars(gen,expr->r.left);
  r = r && expr_resolve_vars(gen,expr->r.name_avt);
  r = r && expr_resolve_vars(gen,expr->r.value_avt);
  r = r && expr_resolve_vars(gen,expr->r.namespace_avt);
  for (c = expr->r.children; c && r; c = c->next)
    r = r && expr_resolve_vars(gen,c);
  for (c = expr->r.attributes; c && r; c = c->next)
    r = r && expr_resolve_vars(gen,c);
  r = r && expr_resolve_ref(gen,expr);
  r = r && expr_semantic_check(gen,expr);
  return r;
}

int exclude_namespace(elcgen *gen, expression *expr, const char *uri)
{
  int found = 0;
  expression *xp;

  if (!strcmp(uri,XSLT_NS))
    return 1;

  for (xp = expr; xp && !found; xp = xp->parent) {
    char *str;
    if (xp->xmlnode->ns && !strcmp((char*)xp->xmlnode->ns->href,XSLT_NS))
      str = xmlGetNsProp(xp->xmlnode,"exclude-result-prefixes",NULL);
    else
      str = xmlGetNsProp(xp->xmlnode,"exclude-result-prefixes",XSLT_NS);
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
              found = (NULL != xmlSearchNsByHref(gen->parse_doc,xp->xmlnode,(xmlChar*)uri));
            }
            else if (!strcmp(start,"#default")) {
              found = ((NULL != xp->xmlnode->ns) && !strcmp((char*)xp->xmlnode->ns->href,uri));
            }
            else {
              ns = xmlSearchNs(gen->parse_doc,xp->xmlnode,(xmlChar*)start);
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

void expr_print_tree(elcgen *gen, int indent, expression *expr)
{
  expression *c;
  if (NULL == expr)
    return;
  gen_iprintf(gen,indent,"%s",expr_names[expr->type]);
  if (expr->qn.localpart && expr->qn.uri && expr->qn.localpart) { /* null means wildcard */
    if (!strcmp(expr->qn.uri,""))
      gen_printf(gen," %s",expr->qn.localpart);
    else
      gen_printf(gen," {%s}%s",expr->qn.uri,expr->qn.localpart);
  }
  if (expr->str) {
    char *esc = escape(expr->str);
    gen_printf(gen," \"%s\"",esc);
    free(esc);
  }
  if (RESTYPE_UNKNOWN != expr->restype)
    gen_printf(gen," [%s]",restype_names[expr->restype]);
  if ((XSLT_FUNCTION == expr->type) && !expr->called)
    gen_printf(gen," -- not called");
  expr_print_tree(gen,indent+1,expr->r.test);
  expr_print_tree(gen,indent+1,expr->r.left);
  expr_print_tree(gen,indent+1,expr->r.right);

  expr_print_tree(gen,indent+1,expr->r.name_avt);
  expr_print_tree(gen,indent+1,expr->r.value_avt);
  expr_print_tree(gen,indent+1,expr->r.namespace_avt);
  for (c = expr->r.children; c; c = c->next)
    expr_print_tree(gen,indent+1,c);
  for (c = expr->r.attributes; c; c = c->next)
    expr_print_tree(gen,indent+1,c);
}

expression *expr_copy(expression *orig)
{
  expression *copy;
  expression *oc;
  expression **cptr;

  if (NULL == orig)
    return NULL;

  /* fields */
  copy = (expression*)calloc(1,sizeof(expression));
  copy->type = orig->type;
  copy->qn = copy_qname(orig->qn);
  copy->xmlnode = orig->xmlnode;
  copy->ident = orig->ident ? strdup(orig->ident) : NULL;
  copy->axis = orig->axis;
  copy->num = orig->num;
  copy->str = orig->str ? strdup(orig->str) : NULL;
  copy->orig = orig->orig ? strdup(orig->orig) : NULL;
  copy->kind = orig->kind;

  /* branches */
  copy->r.test = expr_copy(orig->r.test);
  copy->r.left = expr_copy(orig->r.left);
  copy->r.right = expr_copy(orig->r.right);

  copy->r.name_avt = expr_copy(orig->r.name_avt);
  copy->r.value_avt = expr_copy(orig->r.value_avt);
  copy->r.namespace_avt = expr_copy(orig->r.namespace_avt);

  /* children and attributes */
  cptr = &copy->r.children;
  for (oc = orig->r.children; oc; oc = oc->next) {
    *cptr = expr_copy(oc);
    cptr = &(*cptr)->next;
  }

  cptr = &copy->r.attributes;
  for (oc = orig->r.attributes; oc; oc = oc->next) {
    *cptr = expr_copy(oc);
    cptr = &(*cptr)->next;
  }

  /* don't want to copy these properties - they're unique to the instance */
  copy->restype = RESTYPE_UNKNOWN;
  copy->derivatives = NULL;
  copy->called = 0;

  return copy;
}
