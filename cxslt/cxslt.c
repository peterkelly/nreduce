/*
 * This file is part of the GridXSLT project
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

#define xmlStrcmp(a,b) xmlStrcmp((xmlChar*)(a),(xmlChar*)(b))
#define xmlStrdup(a) ((char*)xmlStrdup((xmlChar*)(a)))
#define xmlGetProp(a,b) ((char*)xmlGetProp(a,(xmlChar*)(b)))

/* FIXME: escape strings when printing (note that xmlChar complicates things) */
/* FIXME: can't use reserved words like eq, text, item for element names in path
   expressions. Need to fix the tokenizer/parser */
/* FIXME: use an autoconf script to set YYLEX_DESTROY where appropriate - currently
   it does not get set at all */

#define XSLT_NAMESPACE "http://www.w3.org/1999/XSL/Transform"

int parse_firstline = 0;
expression *parse_expr = NULL;
xmlNodePtr parse_node = NULL;

typedef struct yy_buffer_state *YY_BUFFER_STATE;
YY_BUFFER_STATE yy_scan_string(const char *str);
void yy_switch_to_buffer(YY_BUFFER_STATE new_buffer);
void yy_delete_buffer(YY_BUFFER_STATE buffer);
#if HAVE_YYLEX_DESTROY
int yylex_destroy(void);
#endif

extern FILE *yyin;
extern int lex_lineno;
int yyparse();

void compile_sequence(xmlNodePtr first);
void compile_expression(expression *expr);

static int option_indent = 0;
static int option_strip = 0;

void check_attr(const char *value, const char *name)
{
  if (NULL == value) {
    fprintf(stderr,"Missing attribute: %s\n",name);
    exit(1);
  }
}

int ignore_node(xmlNodePtr n)
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
/*   switch (expr->type) { */
/*   } */
  if (expr->left)
    free_expression(expr->left);
  if (expr->right)
    free_expression(expr->right);
  if (expr->str)
    free(expr->str);
}

const char *lookup_nsuri(xmlNodePtr n, const char *prefix)
{
  xmlNs *ns;
/*   printf("\n\nlooking up prefix \"%s\" in node %s\n",prefix,n->name); */
  for (; n && (XML_ELEMENT_NODE == n->type); n = n->parent) {
/*     printf("node: %p %s\n",n,n->name); */
    for (ns = n->nsDef; ns; ns = ns->next) {
/*       printf("ns: %s %s\n",ns->prefix,ns->href); */
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

void qname_to_nsname(const char *qname, xmlNodePtr n,
                    char **nsuri, char **localname)
{
  char *qncopy = xmlStrdup(qname);
  char *colon = strchr(qncopy,':');
  if (colon) {
    *colon = '\0';
    *nsuri = xmlStrdup(lookup_nsuri(n,qncopy));
    *localname = xmlStrdup(colon+1);
  }
  else {
    *nsuri = xmlStrdup(lookup_nsuri(n,""));
    *localname = xmlStrdup(qncopy);
  }
  free(qncopy);
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

char *string_to_ident(const char *str)
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

char *nsname_to_ident(const char *nsuri, const char *localname)
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

char *escape(const char *str2)
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

void compile_binary(expression *expr, const char *fun)
{
  printf("(%s\n",fun);
  compile_expression(expr->left);
  printf("\n");
  compile_expression(expr->right);
  printf(")");
}

void too_many_args(const char *fname)
{
  fprintf(stderr,"%s: too many arguments\n",fname);
  exit(1);
}

void compile_expression(expression *expr)
{
  switch (expr->type) {
  case XPATH_OR:         compile_binary(expr,"or");                   break; /* FIXME: test */
  case XPATH_AND:        compile_binary(expr,"and");                  break; /* FIXME: test */

  case XPATH_VALUE_EQ:   compile_binary(expr,"xslt:value_eq");        break;
  case XPATH_VALUE_NE:   compile_binary(expr,"xslt:value_ne");        break;
  case XPATH_VALUE_LT:   compile_binary(expr,"xslt:value_lt");        break;
  case XPATH_VALUE_LE:   compile_binary(expr,"xslt:value_le");        break;
  case XPATH_VALUE_GT:   compile_binary(expr,"xslt:value_gt");        break;
  case XPATH_VALUE_GE:   compile_binary(expr,"xslt:value_ge");        break;

  case XPATH_GENERAL_EQ: compile_binary(expr,"xslt:general_eq");      break;
  case XPATH_GENERAL_NE: compile_binary(expr,"xslt:general_ne");      break;
  case XPATH_GENERAL_LT: compile_binary(expr,"xslt:general_lt");      break;
  case XPATH_GENERAL_LE: compile_binary(expr,"xslt:general_le");      break;
  case XPATH_GENERAL_GT: compile_binary(expr,"xslt:general_gt");      break;
  case XPATH_GENERAL_GE: compile_binary(expr,"xslt:general_ge");      break;
  case XPATH_ADD:        compile_binary(expr,"xslt:add");             break;
  case XPATH_SUBTRACT:   compile_binary(expr,"xslt:subtract");        break;
  case XPATH_MULTIPLY:   compile_binary(expr,"xslt:multiply");        break;
  case XPATH_DIVIDE:     compile_binary(expr,"xslt:divide");          break;
  case XPATH_IDIVIDE:    compile_binary(expr,"xslt:idivide");         break; /* FIXME: test */
  case XPATH_MOD:        compile_binary(expr,"xslt:mod");             break;
  case XPATH_INTEGER_LITERAL:
    printf("(cons (xml:mknumber %f) nil)",expr->num);
    break;
  case XPATH_DECIMAL_LITERAL:
    printf("(cons (xml:mknumber %f) nil)",expr->num);
    break;
  case XPATH_DOUBLE_LITERAL:
    printf("(cons (xml:mknumber %f) nil)",expr->num);
    break;
  case XPATH_STRING_LITERAL: {
    char *esc = escape(expr->str);
    printf("(cons (xml:mkstring \"%s\") nil)",esc);
    free(esc);
    break;
  }
  case XPATH_TO: {
    printf("(xslt:range\n");
    printf("(xslt:getnumber ");
    compile_expression(expr->left);
    printf(")\n");
    printf("(xslt:getnumber ");
    compile_expression(expr->right);
    printf("))");
    break;
  }
  case XPATH_SEQUENCE:
    printf("(append ");
    compile_expression(expr->left);
    printf("\n");
    compile_expression(expr->right);
    printf(")");
    break;
  case XPATH_CONTEXT_ITEM:
    printf("(cons citem nil)");
    break;
  case XPATH_UNARY_MINUS:
    printf("(xslt:uminus ");
    compile_expression(expr->left);
    printf(")");
    break;
  case XPATH_UNARY_PLUS:
    printf("(xslt:uplus ");
    compile_expression(expr->left);
    printf(")");
    break;
  case XPATH_PAREN:
    compile_expression(expr->left);
    break;
  case XPATH_EMPTY:
    printf("nil");
    break;
  case XPATH_VAR_REF: {
    char *ident = nsname_to_ident(expr->qn.uri,expr->qn.localpart);
    printf("%s",ident);
    free(ident);
    break;
  }
  case XPATH_FILTER:
    printf("(xslt:filter3\n(!citem.!cpos.!csize.xslt:ebv ");
    compile_expression(expr->right);
    printf(") ");
    compile_expression(expr->left);
    printf(")");
    break;
  case XPATH_STEP:
    /* FIXME: the resulting set should only contain one instance of each node; need to
       somehow filter out duplicates */
    printf("\n// step\n");
/*     Note: node sequences are handled differently from atomic value sequences... (?) */
/*     see XPath 2.0 section 3.2 */
    printf("(xslt:apmap3 (!citem.!cpos.!csize.\n");
    compile_expression(expr->right);
    printf(") ");
    compile_expression(expr->left);
    printf(")");
    break;
  case XPATH_KIND_TEST:
  case XPATH_NAME_TEST: {

    printf("(filter ");
    if (XPATH_KIND_TEST == expr->type) {
      switch (expr->kind) {
      case KIND_DOCUMENT:
        printf("(xslt:type_test xml:TYPE_DOCUMENT)");
        break;
      case KIND_ELEMENT:
        printf("(xslt:type_test xml:TYPE_ELEMENT)");
        break;
      case KIND_ATTRIBUTE:
        printf("(xslt:type_test xml:TYPE_ATTRIBUTE)");
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
        printf("(xslt:type_test xml:TYPE_COMMENT)");
        break;
      case KIND_TEXT:
        printf("(xslt:type_test xml:TYPE_TEXT)");
        break;
      case KIND_ANY:
        printf("xslt:any_test");
        break;
      default:
        abort();
      }
    }
    else {
      char *nsuri = expr->qn.uri ? escape(expr->qn.uri) : NULL;
      char *localname = expr->qn.localpart ? escape(expr->qn.localpart) : NULL;
      char *type = (AXIS_ATTRIBUTE == expr->axis) ? "xml:TYPE_ATTRIBUTE" : "xml:TYPE_ELEMENT";

      if (nsuri && localname)
        printf("(xslt:name_test %s \"%s\" \"%s\")",type,nsuri,localname);
      else if (localname)
        printf("(xslt:wildcard_uri_test %s \"%s\")",type,localname);
      else if (nsuri)
        printf("(xslt:wildcard_localname_test %s \"%s\")",type,nsuri);
      else
        printf("(xslt:type_test %s)",type);
      free(nsuri);
      free(localname);
    }

    printf("\n");
    switch (expr->axis) {
    case AXIS_SELF:
      printf("(cons citem nil)");
      break;
    case AXIS_CHILD:
      printf("(xml:node_children citem)");
      break;
    case AXIS_DESCENDANT:
      printf("(xslt:node_descendants citem)");
      break;
    case AXIS_DESCENDANT_OR_SELF:
      printf("(cons citem (xslt:node_descendants citem))");
      break;
    case AXIS_PARENT:
      printf("(xslt:node_parent_list citem)");
      break;
    case AXIS_ANCESTOR:
      printf("(reverse (xslt:node_ancestors citem))");
      break;
    case AXIS_ANCESTOR_OR_SELF:
      printf("(reverse (xslt:node_ancestors_or_self citem))");
      break;
    case AXIS_PRECEDING_SIBLING:
      printf("(reverse (xslt:node_preceding_siblings citem))");
      break;
    case AXIS_FOLLOWING_SIBLING:
      printf("(xslt:node_following_siblings citem)");
      break;
    case AXIS_PRECEDING:
      printf("(reverse (xslt:node_preceding citem))");
      break;
    case AXIS_FOLLOWING:
      printf("(xslt:node_following citem)");
      break;
    case AXIS_ATTRIBUTE:
      printf("(xml:node_attributes citem)");
      break;
    case AXIS_NAMESPACE:
      fprintf(stderr,"Namespace axis is not supported\n");
      exit(1);
      break;
    default:
      assert(!"unsupported axis");
      break;
    }
    printf(")");

    break;
  }
  case XPATH_FUNCTION_CALL:
    if (strcmp(expr->qn.prefix,"")) {
      char *ident = nsname_to_ident(expr->qn.uri,expr->qn.localpart);
      expression *p;
      printf("(%s",ident);

      for (p = expr->left; p; p = p->right) {
        assert(XPATH_ACTUAL_PARAM == p->type);
        printf(" ");
        compile_expression(p->left);
      }
      printf(")");
    }
    else {
      expression *p;
      int nargs = 0;

      for (p = expr->left; p; p = p->right)
        nargs++;

      if (!strcmp(expr->qn.localpart,"string") && (0 == nargs)) {
        printf("(xslt:fn_string0 citem)");
      }
      else if (!strcmp(expr->qn.localpart,"position")) {
        printf("(cons (xml:mknumber cpos) nil)");
      }
      else if (!strcmp(expr->qn.localpart,"last")) {
        printf("(cons (xml:mknumber csize) nil)");
      }
      else {
        printf("(xslt:fn_%s",expr->qn.localpart);

        for (p = expr->left; p; p = p->right) {
          assert(XPATH_ACTUAL_PARAM == p->type);
          printf(" ");
          compile_expression(p->left);
        }
        printf(")");
      }
    }
    break;
  default:
    fprintf(stderr,"Unsupported expression type: %d\n",expr->type);
    exit(1);
  }
}

void compile_expr_string(xmlNodePtr n, const char *str)
{
  YY_BUFFER_STATE bufstate;
  int r;
  parse_node = n;
  parse_firstline = n->line;
  parse_expr = NULL;
  lex_lineno = 0;

/*   printf("// parsing string: %s\n",str); */

  bufstate = yy_scan_string(str);
  yy_switch_to_buffer(bufstate);

  r = yyparse();

  yy_delete_buffer(bufstate);
#if HAVE_YYLEX_DESTROY
  yylex_destroy();
#endif

  if (0 != r) {
    fprintf(stderr,"XPath parse error\n");
    exit(1);
  }

  assert(parse_expr);
  compile_expression(parse_expr);
  free_expression(parse_expr);
  parse_expr = NULL;

/*   printf("(cons (xml:mktext \"???\") nil)"); */
}

void compile_attributes(xmlNodePtr n)
{
  xmlAttrPtr attr;
  int count = 0;

  for (attr = n->properties; attr; attr = attr->next) {
    xmlNodePtr child;
    if (attr->ns)
      printf("(cons (xml:mkattr \"%s\" \"%s\" \"%s\" \"",
             attr->ns->href,attr->ns->prefix,attr->name);
    else
      printf("(cons (xml:mkattr nil nil \"%s\" \"",attr->name);

    for (child = attr->children; child; child = child->next)
      if (XML_TEXT_NODE == child->type)
        printf("%s",child->content);
    printf("\")\n");

    count++;
  }
  printf("nil");
  while (0 < count--)
    printf(")");
}

void compile_namespaces(xmlNodePtr n)
{
  xmlNsPtr ns;
  int count = 0;
  for (ns = n->nsDef; ns; ns = ns->next) {
    printf("(cons (xml:mknamespace \"%s\" \"%s\")\n",ns->href,ns->prefix);
    count++;
  }
  printf("nil");
  while (0 < count--)
    printf(")");
}

void compile_instruction(xmlNodePtr n)
{
  switch (n->type) {
  case XML_ELEMENT_NODE: {
/*     xmlNodePtr child; */
    if (n->ns && !xmlStrcmp(n->ns->href,XSLT_NAMESPACE)) {
      if (!xmlStrcmp(n->name,"sequence")) {
        char *select = xmlGetProp(n,"select");
        check_attr(select,"select");

        printf("\n// sequence %s\n",select);
        compile_expr_string(n,select);
      }
      else if (!xmlStrcmp(n->name,"value-of")) {
        char *select = xmlGetProp(n,"select");
        if (select) {
          printf("\n// value-of %s\n",select);
          printf("(cons (xml:mktext (xslt:consimple ");
          compile_expr_string(n,select);
          printf(")) nil)\n");
        }
        else {
          /* FIXME: support content within a value attribute */
          check_attr(select,"select");
        }
      }
      else if (!xmlStrcmp(n->name,"text")) {
        xmlNodePtr child;
        int count = 0;
        int len = 0;
        int pos = 0;
        char *str;
        char *esc;
        for (child = n->children; child; child = child->next) {
          if (XML_TEXT_NODE == child->type) {
            count++;
            len += xmlStrlen(child->content);
          }
        }
        printf("\n// text: %d children, %d len\n",count,len);
        str = (char*)malloc(len+1);

        for (child = n->children; child; child = child->next) {
          if (XML_TEXT_NODE == child->type) {
            memcpy(&str[pos],child->content,xmlStrlen(child->content));
            pos += xmlStrlen(child->content);
          }
        }
        assert(pos == len);
        str[pos] = '\0';

        esc = escape(str);
        printf("(cons (xml:mktext \"%s\") nil)",esc);
        free(esc);
        free(str);
      }
      else if (!xmlStrcmp(n->name,"for-each")) {
        char *select = xmlGetProp(n,"select");
        check_attr(select,"select");

        printf("\n// for-each %s\n",select);
        printf("(letrec\n");
        printf("select = ");
        compile_expr_string(n,select);
        printf("\nin\n");

        printf("(xslt:apmap3 (!citem.!cpos.!csize.\n");
        compile_sequence(n->children);
        printf(")\n");

        printf("select))\n");
      }
      else if (!xmlStrcmp(n->name,"if")) {
        char *test = xmlGetProp(n,"test");
        check_attr(test,"test");
        printf("\n// if %s\n",test);
        printf("(if\n");
        printf("(xslt:ebv\n");
        compile_expr_string(n,test);
        printf(")\n");
        compile_sequence(n->children);
        printf("\nnil)");
      }
      else if (!xmlStrcmp(n->name,"choose")) {
        xmlNodePtr child;
        int count = 0;
        int otherwise = 0;

        printf("\n// choose\n");
        for (child = n->children; child; child = child->next) {
          if (child->ns && !xmlStrcmp(child->ns->href,XSLT_NAMESPACE) &&
              !xmlStrcmp(child->name,"when")) {
            // when
            char *test = xmlGetProp(child,"test");
            check_attr(test,"test");

            printf("\n// when %s\n",test);

            printf("(if (xslt:ebv ");
            compile_expr_string(child,test);
            printf(") ");
            compile_sequence(child->children);
            printf(" ");
            count++;
          }
          else if (child->ns && !xmlStrcmp(child->ns->href,XSLT_NAMESPACE) &&
                   !xmlStrcmp(child->name,"otherwise")) {
            // otherwise
            printf("// otherwise\n");

            compile_sequence(child->children);
            otherwise = 1;
          }
        }

        if (!otherwise)
          printf("nil");

        while (0 < count--)
          printf(")");
      }
      else {
        fprintf(stderr,"Unsupported XSLT instruction: %s\n",n->name);
        exit(1);
      }
    }
    else {
      printf("(letrec\n");

      printf("attrs = (append ");
      compile_attributes(n);
      printf(" (xslt:get_attributes content))\n");

      printf("namespaces = (append ");
      compile_namespaces(n);
      printf(" (xslt:get_namespaces content))\n");

      printf("children = (xslt:concomplex (xslt:get_children content))\n");

      if (n->ns) {
        printf("elem = (xml:mkelem \"%s\" \"%s\" \"%s\" attrs namespaces children)\n",
               n->ns->href,n->ns->prefix,n->name);
      }
      else {
        printf("elem = (xml:mkelem nil nil \"%s\" attrs namespaces children)\n",n->name);
      }
      printf("content =\n");
      compile_sequence(n->children);
      printf("\nin\n(cons elem nil))");
    }
/*     printf("???element %s\n",n->name); */
    break;
  }
  case XML_TEXT_NODE: {
    char *esc = escape((const char*)n->content);
    printf("(cons (xml:mktext \"%s\") nil)",esc);
    free(esc);
    break;
  }
  case XML_COMMENT_NODE: {
    printf("???comment\n");
    break;
  }
  default:
    printf("??? %d\n",n->type);
    break;
  }
}

void compile_sequence(xmlNodePtr first)
{
  xmlNodePtr child;
  int count = 0;
  int nchildren = 0;
  for (child = first; child; child = child->next)
    if (!ignore_node(child))
      nchildren++;

  if (0 == nchildren) {
    printf("nil");
    return;
  }

  if (1 == nchildren) {
    child = first;
    while (ignore_node(child))
      child = child->next;
    compile_instruction(child);
    return;
  }

  for (child = first; child; child = child->next) {
    if (child->ns && !xmlStrcmp(child->ns->href,XSLT_NAMESPACE) &&
        !xmlStrcmp(child->name,"variable")) {
      char *name = xmlGetProp(child,"name");
      char *select = xmlGetProp(child,"select");
      char *ident;
      qname qn;
      check_attr(name,"name");

      qn = string_to_qname(name,child);
      ident = nsname_to_ident(qn.uri,qn.localpart);

      printf("(letrec %s = ",ident);
      if (select) {
	compile_expr_string(child,select);
      }
      else {
	printf("(cons (xml:mkdoc ");
	compile_sequence(child->children);
	printf(") nil)");
      }
      printf(" in ");

      free(ident);
      free(qn.uri);
      free(qn.prefix);
      free(qn.localpart);

      count++;
    }
    else if (!ignore_node(child)) {
      printf("(append ");
      compile_instruction(child);
      printf(" ");
      count++;
    }
  }
  printf("nil");
  while (0 < count--)
    printf(")");
}

void compile_template(xmlNodePtr n)
{
  printf("\n");
  printf("top citem cpos csize =\n");
  compile_sequence(n->children);
  printf("\n");
  printf("\n");
}

void compile_function(xmlNodePtr child)
{
  char *name = xmlGetProp(child,"name");
  char *nsuri = NULL;
  char *localname = NULL;
  char *ident;
  xmlNodePtr pn;
  check_attr(name,"name");
  qname_to_nsname(name,child,&nsuri,&localname);
  if (NULL == localname) {
    fprintf(stderr,"XTSE0740: function must have a prefixed name");
    exit(1);
  }

  ident = nsname_to_ident(nsuri,localname);

  printf("\n");
  printf("%s",ident);

  for (pn = child->children; pn; pn = pn->next) {
    if (XML_ELEMENT_NODE == pn->type) {
      if (pn->ns && !xmlStrcmp(pn->ns->href,XSLT_NAMESPACE) &&
          !xmlStrcmp(pn->name,"param")) {
        char *pname = xmlGetProp(pn,"name");
        char *pn_nsuri = NULL;
        char *pn_localname = NULL;
        char *pn_ident;
        check_attr(pname,"name");
        qname_to_nsname(pname,pn,&pn_nsuri,&pn_localname);
        pn_ident = nsname_to_ident(pn_nsuri,pn_localname);
        printf(" %s",pn_ident);
        free(pn_nsuri);
        free(pn_localname);
        free(pn_ident);
      }
      else {
        break;
      }
    }
  }

  printf(" =\n");
  compile_sequence(pn);
  printf("\n");
  printf("\n");

  free(nsuri);
  free(localname);
  free(ident);
}

void process_toplevel(xmlNodePtr n2)
{
  xmlNodePtr child;
  for (child = n2->children; child; child = child->next) {
    if (child->ns && !xmlStrcmp(child->ns->href,XSLT_NAMESPACE)) {
      if (!xmlStrcmp(child->name,"template")) {
        compile_template(child);
      }
      else if (!xmlStrcmp(child->name,"function")) {
        compile_function(child);
      }
      else if (!xmlStrcmp(child->name,"output")) {
        char *indent = xmlGetProp(child,"indent");
        if (indent && !xmlStrcmp(indent,"yes"))
          option_indent = 1;
        free(indent);
      }
      else if (!xmlStrcmp(child->name,"strip-space")) {
        char *elements = xmlGetProp(child,"elements");
        check_attr(elements,"elements");
        if (!xmlStrcmp(elements,"*"))
          option_strip = 1;
        free(elements);
      }
    }
  }
}

void process_root(xmlNodePtr n)
{
  if (XML_ELEMENT_NODE == n->type) {
    if (n->ns && !xmlStrcmp(n->ns->href,XSLT_NAMESPACE) &&
        (!xmlStrcmp(n->name,"stylesheet") || !xmlStrcmp(n->name,"transform"))) {
      process_toplevel(n);
    }
    else {
      fprintf(stderr,"top-level element must be a xsl:stylesheet or xsl:transform");
      exit(1);
    }
  }
}



int main(int argc, char **argv)
{
  char *sourcefile;
  char *xsltfile;
  xmlDocPtr doc;
  xmlNodePtr root;

  setbuf(stdout,NULL);

  if (3 > argc) {
    fprintf(stderr,"Usage: cxslt <sourcefile> <xsltfile>\n");
    exit(1);
  }

  sourcefile = argv[1];
  xsltfile = argv[2];

  if (NULL == (doc = xmlReadFile(xsltfile,NULL,0))) {
    fprintf(stderr,"Error parsing %s\n",xsltfile);
    exit(1);
  }

  printf("import xml\n");
  printf("import xslt\n");
  printf("FILENAME = \"%s\"\n",sourcefile);

  root = xmlDocGetRootElement(doc);
  process_root(root);

  printf("STRIPALL = %s\n",option_strip ? "1" : "nil");
  printf("INDENT = %s\n",option_indent ? "1" : "nil");
  printf("main = (xslt:output INDENT (cons (xml:mkdoc (xslt:concomplex (top (xml:parsexml "
         "STRIPALL (readb FILENAME)) 1 1))) nil))\n");

  xmlFreeDoc(doc);

  return 0;
}
