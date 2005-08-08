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
 */

#include "parse.h"
#include "util/namespace.h"
#include "util/stringbuf.h"
#include "util/macros.h"
#include <libxml/xmlwriter.h>
#include <libxml/xmlIO.h>
#include <libxml/parser.h>
#include <libxml/tree.h>
#include <stdio.h>
#include <string.h>
#include <assert.h>
#include <ctype.h>

static int parse_sequence_constructors(xl_snode *sn, xmlNodePtr n, xmlDocPtr doc,
                                       error_info *ei, const char *filename);

static const char *reserved_namespaces[7] = {
  XSLT_NAMESPACE,
  FN_NAMESPACE,
  XML_NAMESPACE,
  XS_NAMESPACE,
  XDT_NAMESPACE,
  XSI_NAMESPACE,
  NULL
};

static const nsname_t standard_attributes[13] = {
  { XSLT_NAMESPACE, "version" },
  { XSLT_NAMESPACE, "exclude-result-prefixes" },
  { XSLT_NAMESPACE, "extension-element-prefixes" },
  { XSLT_NAMESPACE, "xpath-default-namespace" },
  { XSLT_NAMESPACE, "default-collation" },
  { XSLT_NAMESPACE, "use-when" },
  { NULL, "version" },
  { NULL, "exclude-result-prefixes" },
  { NULL, "extension-element-prefixes" },
  { NULL, "xpath-default-namespace" },
  { NULL, "default-collation" },
  { NULL, "use-when" },
  { NULL, NULL }
};

void output_xslt_sequence_constructor(xmlTextWriter *writer, xl_snode *node);

static void xs_skip_others(xmlNodePtr *c)
{
  while ((*c) && (XML_COMMENT_NODE == (*c)->type))
    (*c) = (*c)->next;
}

static void xslt_next_element(xmlNodePtr *c)
{
  *c = (*c)->next;
  xs_skip_others(c);
}

static void print_node(xmlDocPtr doc, xmlNodePtr n, int indent)
{
  xmlNodePtr c;
  xmlNsPtr nslist;
  int i;

  for (nslist = n->nsDef; nslist; nslist = nslist->next)
    printf("NAMESPACE %d \"%s\": \"%s\"\n",nslist->type,nslist->prefix,nslist->href);

  if (XML_ELEMENT_NODE == n->type) {
    xmlAttrPtr attr;

    for (i = 0; i < indent; i++)
      printf("  ");
    printf("%s",n->name);

    for (attr = n->properties; attr; attr = attr->next)
      printf(" %s=%s",attr->name,xmlGetNsProp(n,attr->name,NULL));


    printf("\n");
  }
  for (c = n->children; c; c = c->next)
    print_node(doc,c,indent+1);
}

/* #define XSLT_DECLARATION                  0 */
/* #define XSLT_IMPORT                       1 */
/* #define XSLT_INSTRUCTION                  2 */
/* #define XSLT_LITERAL_RESULT_ELEMENT       3 */
/* #define XSLT_MATCHING_SUBSTRING           4 */
/* #define XSLT_NON_MATCHING_SUBSTRING       5 */
/* #define XSLT_OTHERWISE                    6 */
/* #define XSLT_OUTPUT_CHARACTER             7 */
/* #define XSLT_PARAM                        8 */
/* #define XSLT_SORT                         9 */
/* #define XSLT_TRANSFORM                    10 */
/* #define XSLT_VARIABLE                     11 */
/* #define XSLT_WHEN                         12 */
/* #define XSLT_WITH_PARAM                   13 */

static xp_expr *get_expr(xmlNodePtr n, const char *attrname, const char *str)
{
  xp_expr *expr;
  error_info ei;
  memset(&ei,0,sizeof(error_info));

  /* FIXME: filename */
  if (NULL == (expr = xp_expr_parse(str,"",n->line,&ei))) {
    error_info_print(&ei,stderr);
    error_info_free_vals(&ei);
    return NULL;
  }
  else {
    return expr;
  }
}

static xp_expr *get_expr_attr(xmlNodePtr n, const char *attrname, const char *str)
{
  if ((2 <= strlen(str) && ('{' == str[0]) && ('}' == str[strlen(str)-1]))) {
    char *expr_str = (char*)alloca(strlen(str)-1);
    memcpy(expr_str,str+1,strlen(str)-2);
    expr_str[strlen(str)-2] = '\0';
    return get_expr(n,attrname,expr_str);
  }
  else if ((NULL != strchr(str,'{')) || (NULL != strchr(str,'}'))) {
    fprintf(stderr,"FIXME: I don't support handling attributes with only partial "
                   "expressions yet!\n");
    exit(1);
  }
  else {
    xp_expr *expr = xp_expr_new(XPATH_EXPR_STRING_LITERAL,NULL,NULL);
    expr->strval = strdup(str);
    return expr;
  }
}

static void add_ns_defs(xl_snode *sn, xmlNodePtr node)
{
  /* FIXME: support namespace definitions located anywhere in the doc. Will need to test this
     everywhere a qname can be used inside an attribute, e.g. function:as */
  xmlNsPtr ns;
  for (ns = node->nsDef; ns; ns = ns->next)
    ns_add(sn->namespaces,ns->href,ns->prefix);
}

static xl_snode *add_node(xl_snode ***ptr, int type, const char *filename, xmlNodePtr n)
{
  xl_snode *new_node = xl_snode_new(type);
  new_node->deffilename = strdup(filename);
  new_node->defline = n->line;
  add_ns_defs(new_node,n);
  **ptr = new_node;
  *ptr = &new_node->next;
  return new_node;
}

static int parse_param(xl_snode *sn, xmlNodePtr n, xmlDocPtr doc,
                       error_info *ei, const char *filename)
{
  char *name = xmlGetNsProp(n,"name",NULL);
  char *select = xmlGetNsProp(n,"select",NULL);
  char *required = xmlGetNsProp(n,"required",NULL);
  char *tunnel = xmlGetNsProp(n,"tunnel",NULL);
  xl_snode **param_ptr = &sn->param;
  xl_snode *new_sn;

  CHECK_CALL(enforce_allowed_attributes(ei,filename,n,standard_attributes,
                                        "name","select","as","required","tunnel",NULL))
  if (!xmlHasNsProp(n,"name",NULL))
    return missing_attribute2(ei,filename,n->line,NULL,"name");

  while (*param_ptr)
    param_ptr = &((*param_ptr)->next);

  if ((XSLT_TRANSFORM != sn->type) &&
      (XSLT_DECL_FUNCTION != sn->type) &&
      (XSLT_DECL_TEMPLATE != sn->type))
    return invalid_element2(ei,filename,n);

  new_sn = add_node(&param_ptr,XSLT_PARAM,filename,n);











  if (NULL == name)
    return missing_attribute(n,"name");
  new_sn->qname = get_qname(name);
  if ((NULL != select) && (NULL == (new_sn->select = get_expr(n,"select",select))))
    return -1;
  if ((NULL != required) && !strcmp(required,"yes"))
    new_sn->flags |= FLAG_REQUIRED;
  if ((NULL != tunnel) && !strcmp(tunnel,"yes"))
    new_sn->flags |= FLAG_TUNNEL;

  CHECK_CALL(parse_sequence_constructors(new_sn,n,doc,ei,filename))

  return 0;
}

static int parse_sequence_constructors(xl_snode *sn, xmlNodePtr n, xmlDocPtr doc,
                                       error_info *ei, const char *filename)
{
  xmlNodePtr c;
  xmlNodePtr c2;
  xl_snode *new_sn;
  xl_snode **child_ptr = &sn->child;
  xl_snode **param_ptr = &sn->param;
  xl_snode **sort_ptr = &sn->sort;

  /* FIXME: still need to do more checks on the tree after to test for e.g. duplicate parameter
     names, and content supplied inside <comment>s in addition to "select" being specified */

  /* FIXME: for yes/no values, complain if the value is something other than "yes" or "no"
     (unless of course the attribute is missing and not required). Should this check be
     case insensitive? */

  /* FIXME: verify that all missing_attribute() calls have a return before them! */

  for (c = n->children; c; c = c->next) {
    if (check_element(c,"analyze-string",XSLT_NAMESPACE)) {
      /* FIXME: add testcases to all.xl for use of fallback inside analyze-string */
      char *select = xmlGetNsProp(c,"select",NULL);
      char *regex = xmlGetNsProp(c,"regex",NULL);
      char *flags = xmlGetNsProp(c,"flags",NULL);
      int mcount = 0;
      int nmcount = 0;
      xl_snode **child2_ptr;

      if (NULL == select)
        return missing_attribute(c,"select");
      if (NULL == regex)
        return missing_attribute(c,"regex");

      new_sn = add_node(&child_ptr,XSLT_INSTR_ANALYZE_STRING,filename,c);
      child2_ptr = &new_sn->child;

      if (NULL == (new_sn->select = get_expr(c,"select",select)))
        return -1;
      if (NULL == (new_sn->expr1 = get_expr_attr(c,"regex",regex)))
        return -1;
      if ((NULL != flags) && (NULL == (new_sn->expr2 = get_expr_attr(c,"flags",flags))))
        return -1;

      for (c2 = c->children; c2; c2 = c2->next) {
        if (check_element(c2,"matching-substring",XSLT_NAMESPACE)) {
          xl_snode *matching = add_node(&child2_ptr,XSLT_MATCHING_SUBSTRING,filename,c2);
          CHECK_CALL(parse_sequence_constructors(matching,c2,doc,ei,filename))
          mcount++;
        }
        else if (check_element(c2,"non-matching-substring",XSLT_NAMESPACE)) {
          xl_snode *non_matching = add_node(&child2_ptr,XSLT_NON_MATCHING_SUBSTRING,filename,c2);
          CHECK_CALL(parse_sequence_constructors(non_matching,c2,doc,ei,filename))
          nmcount++;
        }
        else if (check_element(c2,"fallback",XSLT_NAMESPACE)) {
          xl_snode *fallback = add_node(&child2_ptr,XSLT_INSTR_FALLBACK,filename,c2);
          CHECK_CALL(parse_sequence_constructors(fallback,c2,doc,ei,filename))
        }
      }
      if ((0 == mcount) && (0 == nmcount))
        return parse_error(c,"XTSE1130: analyze-string must have at least a matching-substring "
                           "or non-matching-substring child");
      if ((1 < mcount) || (1 < nmcount))
        return parse_error(c,"matching-substring and non-matching-substring may only appear once "
                           "each inside analyze-string");
    }
    else if (check_element(c,"apply-imports",XSLT_NAMESPACE)) {

      new_sn = add_node(&child_ptr,XSLT_INSTR_APPLY_IMPORTS,filename,c);

      /* make sure all children are <xsl:param> elements */
      for (c2 = c->children; c2; c2 = c2->next) {
        if (XML_ELEMENT_NODE != c2->type)
          continue;
        if (!check_element(c2,"with-param",XSLT_NAMESPACE))
          return parse_error(c2,"not allowed here");
      }

      CHECK_CALL(parse_sequence_constructors(new_sn,c,doc,ei,filename))
    }
    else if (check_element(c,"apply-templates",XSLT_NAMESPACE)) {
      char *select = xmlGetNsProp(c,"select",NULL);
      char *mode = xmlGetNsProp(c,"mode",NULL);

      /* make sure all children are <xsl:sort> or <xsl:with-param> elements */
      for (c2 = c->children; c2; c2 = c2->next) {
        if (XML_ELEMENT_NODE != c2->type)
          continue;
        if (!check_element(c2,"sort",XSLT_NAMESPACE) &&
            !check_element(c2,"with-param",XSLT_NAMESPACE))
          return parse_error(c2,"not allowed here");
      }

      new_sn = add_node(&child_ptr,XSLT_INSTR_APPLY_TEMPLATES,filename,c);

      if ((NULL != select) && (NULL == (new_sn->select = get_expr(c,"select",select))))
        return -1;
      /* FIXME: multiple modes? */
      if ((NULL != mode))
        new_sn->mode = get_qname(mode);

      CHECK_CALL(parse_sequence_constructors(new_sn,c,doc,ei,filename))
    }
    else if (check_element(c,"attribute",XSLT_NAMESPACE)) {
      char *name = xmlGetNsProp(c,"name",NULL);
      /* FIXME: support for "namespace" attribute */
      char *select = xmlGetNsProp(c,"select",NULL);
      /* FIXME: support for "separator" attribute */
      /* FIXME: support for "type" attribute */
      /* FIXME: support for "validation" attribute */

      new_sn = add_node(&child_ptr,XSLT_INSTR_ATTRIBUTE,filename,c);

      if (NULL == name)
        return missing_attribute(c,"name");
      new_sn->qname = get_qname(name);
      if ((NULL != select) && (NULL == (new_sn->select = get_expr(c,"select",select))))
        return -1;
      CHECK_CALL(parse_sequence_constructors(new_sn,c,doc,ei,filename))
    }
    else if (check_element(c,"call-template",XSLT_NAMESPACE)) {
      char *name = xmlGetNsProp(c,"name",NULL);

      new_sn = add_node(&child_ptr,XSLT_INSTR_CALL_TEMPLATE,filename,c);

      if (NULL == name)
        return missing_attribute(c,"name");
      new_sn->qname = get_qname(name);

      /* make sure all children are <xsl:with-param> elements */
      for (c2 = c->children; c2; c2 = c2->next) {
        if (XML_ELEMENT_NODE != c2->type)
          continue;
        if (!check_element(c2,"with-param",XSLT_NAMESPACE))
          return parse_error(c2,"not allowed here");
      }

      CHECK_CALL(parse_sequence_constructors(new_sn,c,doc,ei,filename))
    }
    else if (check_element(c,"choose",XSLT_NAMESPACE)) {
      new_sn = add_node(&child_ptr,XSLT_INSTR_CHOOSE,filename,c);
      xl_snode *otherwise = NULL;
      xl_snode **child2_ptr = &new_sn->child;
      for (c2 = c->children; c2; c2 = c2->next) {
        if (XML_ELEMENT_NODE != c2->type)
          continue;

        if (check_element(c2,"otherwise",XSLT_NAMESPACE)) {
          if (NULL != otherwise)
            return parse_error(c2,"Only one <otherwise> is allowed inside <choose>");
          otherwise = add_node(&child2_ptr,XSLT_OTHERWISE,filename,c2);
          CHECK_CALL(parse_sequence_constructors(otherwise,c2,doc,ei,filename))
        }
        else if (check_element(c2,"when",XSLT_NAMESPACE)) {
          char *test = xmlGetNsProp(c2,"test",NULL);
          xl_snode *when = add_node(&child2_ptr,XSLT_WHEN,filename,c2);
          if (NULL != otherwise)
            return parse_error(c2,"No <when> allowed after an <otherwise>");
          if (NULL == test)
            return missing_attribute(c2,"test");
          if (NULL == (when->select = get_expr(c2,"test",test)))
            return -1;
          CHECK_CALL(parse_sequence_constructors(when,c2,doc,ei,filename))
        }
      }
    }
    else if (check_element(c,"comment",XSLT_NAMESPACE)) {
      char *select = xmlGetNsProp(c,"select",NULL);
      new_sn = add_node(&child_ptr,XSLT_INSTR_COMMENT,filename,c);
      if ((NULL != select) && (NULL == (new_sn->select = get_expr(c,"select",select))))
        return -1;
      CHECK_CALL(parse_sequence_constructors(new_sn,c,doc,ei,filename))
    }
    else if (check_element(c,"copy",XSLT_NAMESPACE)) {
      /* FIXME: support "copy-namespaces" */
      /* FIXME: support "inherit-namespaces" */
      /* FIXME: support "use-attribute-sets" */
      /* FIXME: support "type" */
      /* FIXME: support "validation" */
      new_sn = add_node(&child_ptr,XSLT_INSTR_COPY,filename,c);
      CHECK_CALL(parse_sequence_constructors(new_sn,c,doc,ei,filename))
    }
    else if (check_element(c,"copy-of",XSLT_NAMESPACE)) {
      char *select = xmlGetNsProp(c,"select",NULL);
      /* FIXME: support "copy-namespaces" */
      /* FIXME: support "type" */
      /* FIXME: support "validation" */
      new_sn = add_node(&child_ptr,XSLT_INSTR_COPY_OF,filename,c);
      if (NULL == select)
        return missing_attribute(c,"select");
      if (NULL == (new_sn->select = get_expr(c,"select",select)))
        return -1;
      for (c2 = c->children; c2; c2 = c2->next) {
        if (XML_ELEMENT_NODE == c2->type)
          return parse_error(c2,"not allowed here");
      }
    }
    else if (check_element(c,"element",XSLT_NAMESPACE)) {
      char *name = xmlGetNsProp(c,"name",NULL);
      /* FIXME: support "namespace" */
      /* FIXME: support "inherit-namespace" */
      /* FIXME: support "use-attribute-sets" */
      /* FIXME: support "type" */
      /* FIXME: support "validation" */
      new_sn = add_node(&child_ptr,XSLT_INSTR_ELEMENT,filename,c);
      if (NULL == name)
        return missing_attribute(c,"name");
      new_sn->name_expr = get_expr_attr(c,"name",name);
      CHECK_CALL(parse_sequence_constructors(new_sn,c,doc,ei,filename))
    }
    else if (check_element(c,"fallback",XSLT_NAMESPACE)) {
      new_sn = add_node(&child_ptr,XSLT_INSTR_FALLBACK,filename,c);
      CHECK_CALL(parse_sequence_constructors(new_sn,c,doc,ei,filename))
    }
    else if (check_element(c,"for-each",XSLT_NAMESPACE)) {
      char *select = xmlGetNsProp(c,"select",NULL);
      new_sn = add_node(&child_ptr,XSLT_INSTR_FOR_EACH,filename,c);
      if (NULL == select)
        return missing_attribute(c,"select");
      if (NULL == (new_sn->select = get_expr(c,"select",select)))
        return -1;
      CHECK_CALL(parse_sequence_constructors(new_sn,c,doc,ei,filename))
    }
    else if (check_element(c,"for-each-group",XSLT_NAMESPACE)) {
      char *select = xmlGetNsProp(c,"select",NULL);
      char *group_by = xmlGetNsProp(c,"group-by",NULL);
      char *group_adjacent = xmlGetNsProp(c,"group-adjacent",NULL);
      char *group_starting_with = xmlGetNsProp(c,"group-starting-with",NULL);
      char *group_ending_with = xmlGetNsProp(c,"group-ending-with",NULL);
      /* FIXME: support "collation" */
      int groupcount = 0;
      new_sn = add_node(&child_ptr,XSLT_INSTR_FOR_EACH_GROUP,filename,c);
      if (NULL == select)
        return missing_attribute(c,"select");
      if (NULL == (new_sn->select = get_expr(c,"select",select)))
        return -1;
      if (NULL != group_by) {
        groupcount++;
        new_sn->gmethod = XSLT_GROUPING_BY;
        new_sn->expr1 = get_expr(c,"group-by",group_by);
      }
      if (NULL != group_adjacent) {
        groupcount++;
        new_sn->gmethod = XSLT_GROUPING_ADJACENT;
        new_sn->expr1 = get_expr(c,"group-adjacent",group_adjacent);
      }
      if (NULL != group_starting_with) {
        groupcount++;
        new_sn->gmethod = XSLT_GROUPING_STARTING_WITH;
        new_sn->expr1 = get_expr(c,"group-starting-with",group_starting_with);
      }
      if (NULL != group_ending_with) {
        groupcount++;
        new_sn->gmethod = XSLT_GROUPING_ENDING_WITH;
        new_sn->expr1 = get_expr(c,"group-ending-with",group_ending_with);
      }
      if (0 == groupcount)
        return parse_error(c,"No grouping method specified");
      if (NULL == new_sn->expr1)
        return -1;
      if (1 < groupcount)
        return parse_error(c,"Only one grouping method can be specified");
      CHECK_CALL(parse_sequence_constructors(new_sn,c,doc,ei,filename))
    }
    else if (check_element(c,"if",XSLT_NAMESPACE)) {
      char *test = xmlGetNsProp(c,"test",NULL);
      new_sn = add_node(&child_ptr,XSLT_INSTR_IF,filename,c);
      if (NULL == test)
        return missing_attribute(c,"test");
      if (NULL == (new_sn->select = get_expr(c,"test",test)))
        return -1;
      CHECK_CALL(parse_sequence_constructors(new_sn,c,doc,ei,filename))
    }
    else if (check_element(c,"message",XSLT_NAMESPACE)) {
      char *select = xmlGetNsProp(c,"select",NULL);
      char *terminate = xmlGetNsProp(c,"terminate",NULL);
      new_sn = add_node(&child_ptr,XSLT_INSTR_MESSAGE,filename,c);
      if ((NULL != select) && (NULL == (new_sn->select = get_expr(c,"select",select))))
        return -1;
      if ((NULL != terminate) && !strcmp(terminate,"yes")) {
        new_sn->flags |= FLAG_TERMINATE;
      }
      else if ((NULL != terminate) && strcmp(terminate,"no")) {
        new_sn->flags |= FLAG_TERMINATE;
        new_sn->expr1 = get_expr_attr(c,"terminate",terminate);
        /* FIXME: handle the case where the attribute specifies another value other than one
           inside {}'s */
      }
      CHECK_CALL(parse_sequence_constructors(new_sn,c,doc,ei,filename))
    }
    else if (check_element(c,"namespace",XSLT_NAMESPACE)) {
      char *name = xmlGetNsProp(c,"name",NULL);
      char *select = xmlGetNsProp(c,"select",NULL);
      new_sn = add_node(&child_ptr,XSLT_INSTR_NAMESPACE,filename,c);
      if (NULL == name)
        return missing_attribute(c,"name");
      /* FIXME: if name specified just as a normal attribute value, assign to qname instead */
      if (NULL == (new_sn->name_expr = get_expr_attr(c,"name",name)))
        return -1;
      if ((NULL != select) && (NULL == (new_sn->select = get_expr(c,"select",select))))
        return -1;
      CHECK_CALL(parse_sequence_constructors(new_sn,c,doc,ei,filename))
    }
    else if (check_element(c,"next-match",XSLT_NAMESPACE)) {
      new_sn = add_node(&child_ptr,XSLT_INSTR_NEXT_MATCH,filename,c);

      /* make sure all children are <xsl:param> or <xsl:fallback> elements */
      for (c2 = c->children; c2; c2 = c2->next) {
        if (XML_ELEMENT_NODE != c2->type)
          continue;
        if (!check_element(c2,"with-param",XSLT_NAMESPACE) &&
            !check_element(c2,"fallback",XSLT_NAMESPACE))
          return parse_error(c2,"not allowed here");
      }

      CHECK_CALL(parse_sequence_constructors(new_sn,c,doc,ei,filename))
    }
    else if (check_element(c,"number",XSLT_NAMESPACE)) {
      /* FIXME */
    }
    else if (check_element(c,"perform-sort",XSLT_NAMESPACE)) {
      char *select = xmlGetNsProp(c,"select",NULL);
      new_sn = add_node(&child_ptr,XSLT_INSTR_PERFORM_SORT,filename,c);
      if ((NULL != select) && (NULL == (new_sn->select = get_expr(c,"select",select))))
        return -1;
      CHECK_CALL(parse_sequence_constructors(new_sn,c,doc,ei,filename))
    }
    else if (check_element(c,"processing-instruction",XSLT_NAMESPACE)) {
      char *name = xmlGetNsProp(c,"name",NULL);
      char *select = xmlGetNsProp(c,"select",NULL);
      new_sn = add_node(&child_ptr,XSLT_INSTR_PROCESSING_INSTRUCTION,filename,c);
      if (NULL == name)
        return missing_attribute(c,"name");
      /* FIXME: if it's just a normal (non-expr) attribute value, set qname instead */
      if ((NULL == (new_sn->name_expr = get_expr_attr(c,"name",name))))
        return -1;
      if ((NULL != select) && (NULL == (new_sn->select = get_expr(c,"select",select))))
        return -1;
      CHECK_CALL(parse_sequence_constructors(new_sn,c,doc,ei,filename))
    }
    else if (check_element(c,"result-document",XSLT_NAMESPACE)) {
      /* FIXME */
    }
    else if (check_element(c,"sequence",XSLT_NAMESPACE)) {
      char *select = xmlGetNsProp(c,"select",NULL);
      new_sn = add_node(&child_ptr,XSLT_INSTR_SEQUENCE,filename,c);
      if (NULL == select)
        return missing_attribute(c,"select");
      if (NULL == (new_sn->select = get_expr(c,"select",select)))
        return -1;
      /* make sure all children are <xsl:fallback> elements */
      for (c2 = c->children; c2; c2 = c2->next) {
        if (XML_ELEMENT_NODE != c2->type)
          continue;
        if (!check_element(c2,"fallback",XSLT_NAMESPACE))
          return parse_error(c2,"not allowed here");
      }
      CHECK_CALL(parse_sequence_constructors(new_sn,c,doc,ei,filename))
    }
    else if (check_element(c,"text",XSLT_NAMESPACE)) {
      /* FIXME: support "disable-output-escaping" */
      int len = 0;
      int pos = 0;
      new_sn = add_node(&child_ptr,XSLT_INSTR_TEXT,filename,c);
      for (c2 = c->children; c2; c2 = c2->next) {
        if ((XML_ELEMENT_NODE == c2->type) ||
            (XML_TEXT_NODE == c2->type))
          len += strlen(c2->content);
      }
      new_sn->strval = (char*)malloc(len+1);
      for (c2 = c->children; c2; c2 = c2->next) {
        if ((XML_ELEMENT_NODE == c2->type) ||
            (XML_TEXT_NODE == c2->type)) {
          memcpy(&new_sn->strval[pos],c2->content,strlen(c2->content));
          pos += strlen(c2->content);
        }
      }
      new_sn->strval[pos] = '\0';
    }
    else if (check_element(c,"value-of",XSLT_NAMESPACE)) {
      char *select = xmlGetNsProp(c,"select",NULL);
      char *separator = xmlGetNsProp(c,"separator",NULL);
      /* FIXME: support "disable-output-escaping" */

      new_sn = add_node(&child_ptr,XSLT_INSTR_VALUE_OF,filename,c);
      if ((NULL != select) && (NULL == (new_sn->select = get_expr(c,"select",select))))
        return -1;
      if ((NULL != separator) && (NULL == (new_sn->expr1 = get_expr_attr(c,"separator",separator))))
        return -1;

      CHECK_CALL(parse_sequence_constructors(new_sn,c,doc,ei,filename))
    }
    else if (check_element(c,"variable",XSLT_NAMESPACE)) {
      char *name = xmlGetNsProp(c,"name",NULL);
      char *select = xmlGetNsProp(c,"select",NULL);
      /* FIXME: attribute "as" for seqtype */
      new_sn = add_node(&child_ptr,XSLT_VARIABLE,filename,c);
      if (NULL == name)
        return missing_attribute(c,"name");
      new_sn->qname = get_qname(name);
      if ((NULL != select) && (NULL == (new_sn->select = get_expr(c,"select",select))))
        return -1;
      CHECK_CALL(parse_sequence_constructors(new_sn,c,doc,ei,filename))
    }
    else if (check_element(c,"param",XSLT_NAMESPACE)) {
      CHECK_CALL(parse_param(sn,c,doc,ei,filename))
    }
    else if (check_element(c,"with-param",XSLT_NAMESPACE)) {
      char *name = xmlGetNsProp(c,"name",NULL);
      char *select = xmlGetNsProp(c,"select",NULL);
      /* FIXME: support "as" */
      char *tunnel = xmlGetNsProp(c,"tunnel",NULL);

      if ((XSLT_INSTR_APPLY_TEMPLATES != sn->type) &&
          (XSLT_INSTR_APPLY_IMPORTS != sn->type) &&
          (XSLT_INSTR_CALL_TEMPLATE != sn->type) &&
          (XSLT_INSTR_NEXT_MATCH != sn->type))
        return parse_error(c,"not allowed here");

      new_sn = add_node(&param_ptr,XSLT_WITH_PARAM,filename,c);

      if (NULL == name)
        return missing_attribute(c,"name");
      new_sn->qname = get_qname(name);
      if ((NULL != select) && (NULL == (new_sn->select = get_expr(c,"select",select))))
        return -1;
      if ((NULL != tunnel) && !strcmp(tunnel,"yes"))
        new_sn->flags |= FLAG_TUNNEL;
      CHECK_CALL(parse_sequence_constructors(new_sn,c,doc,ei,filename))
    }
    else if (check_element(c,"sort",XSLT_NAMESPACE)) {
      char *select = xmlGetNsProp(c,"select",NULL);
      /* FIXME: support "lang" */
      /* FIXME: support "order" */
      /* FIXME: support "collation" */
      /* FIXME: support "stable" */
      /* FIXME: support "case-order" */
      /* FIXME: support "data-type" */
      /* FIXME: i think we are supposed to ensure that the <sort> elements are the first children
         of whatever parent they're in - see for example perform-sort which describves its content
         as being (xsl:sort+, sequence-constructor) */

      if ((XSLT_INSTR_APPLY_TEMPLATES != sn->type) &&
          (XSLT_INSTR_FOR_EACH != sn->type) &&
          (XSLT_INSTR_FOR_EACH_GROUP != sn->type) &&
          (XSLT_INSTR_PERFORM_SORT != sn->type))
        return parse_error(c,"not allowed here");

      new_sn = add_node(&sort_ptr,XSLT_SORT,filename,c);

      if ((NULL != select) && (NULL == (new_sn->select = get_expr(c,"select",select))))
        return -1;
      CHECK_CALL(parse_sequence_constructors(new_sn,c,doc,ei,filename))
    }
    else if (c->ns && (!strcmp(c->ns->href,XSLT_NAMESPACE))) {
      return parse_error(c,"Invalid element");
    }
    else if (XML_ELEMENT_NODE == c->type) {
      xmlAttrPtr attr;
      xl_snode **child2_ptr;
      /* literal result element */
      new_sn = add_node(&child_ptr,XSLT_INSTR_ELEMENT,filename,c);
      child2_ptr = &new_sn->child;

      new_sn->qname.localpart = strdup(c->name);
      /* FIXME: ensure that this prefix is ok to use in the result document... i.e. the
         prefix -> namespace mapping must be established in the result document */
      if (c->ns && c->ns->prefix)
        new_sn->qname.prefix = strdup(c->ns->prefix);

      /* copy attributes */
      for (attr = c->properties; attr; attr = attr->next) {
        xmlNodePtr t;
        xl_snode *anode;
        int len = 0;
        int pos = 0;
        char *val;

        /* FIXME: what about non-text children of the attribute? like entity (references)? */
        for (t = attr->children; t; t = t->next)
          if (XML_TEXT_NODE == t->type)
            len += strlen(t->content);

        val = (char*)malloc(len+1);

        for (t = attr->children; t; t = t->next) {
          if (XML_TEXT_NODE == t->type) {
            memcpy(&val[pos],t->content,strlen(t->content));
            pos += strlen(t->content);
          }
        }
        val[pos] = '\0';

        anode = add_node(&child2_ptr,XSLT_INSTR_ATTRIBUTE,filename,c);
        anode->qname.localpart = strdup(attr->name);
        if (attr->ns && attr->ns->prefix)
          anode->qname.prefix = strdup(attr->ns->prefix);

        if (NULL == (anode->select = get_expr_attr(c,attr->name,val)))
          return -1;
        free(val);
      }

      CHECK_CALL(parse_sequence_constructors(new_sn,c,doc,ei,filename))
    }
    /* FIXME: also support adding literal text from stylesheet file */
  }

  return 0;
}

static int parse_decls(xl_snode *sn, xmlNodePtr n, xmlDocPtr doc,
                       error_info *ei, const char *filename)
{
  xmlNodePtr c;
  xl_snode *new_sn;
  xl_snode **child_ptr = &sn->child;

  for (c = n->children; c; c = c->next) {
    if (check_element(c,"attribute-set",XSLT_NAMESPACE)) {
      /* FIXME */
    }
    else if (check_element(c,"character-map",XSLT_NAMESPACE)) {
      /* FIXME */
    }
    else if (check_element(c,"decimal-format",XSLT_NAMESPACE)) {
      /* FIXME */
    }
    else if (check_element(c,"function",XSLT_NAMESPACE)) {
      const char **rn;
      char *name;
      xmlNodePtr c2;

      CHECK_CALL(enforce_allowed_attributes(ei,filename,c,standard_attributes,
                                            "name","as","override",NULL))

      if (!xmlHasNsProp(c,"name",NULL))
        return missing_attribute2(ei,filename,c->line,NULL,"name");

      /* FIXME: ensure params get processed */

      new_sn = add_node(&child_ptr,XSLT_DECL_FUNCTION,filename,c);

      name = xmlGetNsProp(c,"name",NULL);
      new_sn->qname = get_qname(name);

      /* @implements(xslt20:stylesheet-functions-5)
         test { xslt/parse/function_name2.test }
         test { xslt/parse/function_name3.test }
         test { xslt/parse/function_name4.test }
         test { xslt/parse/function_name5.test }
         test { xslt/parse/function_name6.test }
         test { xslt/parse/function_name7.test }
         test { xslt/parse/function_name8.test }
         test { xslt/parse/function_name9.test }
         @end */
      if (NULL == new_sn->qname.prefix) {
        free(name);
        return set_error_info2(ei,filename,c->line,"XTSE0740",
                               "A stylesheet function MUST have a prefixed name, to remove any "
                               "risk of a clash with a function in the default function namespace");
      }

      if (0 != get_ns_name_from_qname(c,doc,name,&new_sn->ns,&new_sn->name)) {
        free(name);
        return set_error_info2(ei,filename,c->line,NULL,
                               "Could not resolve namespace for prefix \"%s\"",
                               new_sn->qname.prefix);
      }
      free(name);

      for (rn = reserved_namespaces; *rn; rn++) {
        if (!strcmp(new_sn->ns,*rn)) {
          return set_error_info2(ei,filename,c->line,"XTSE0740",
                                 "A function cannot have a prefix refers to a reserved namespace");
        }
      }

      if (xmlHasNsProp(c,"as",NULL)) {
        char *as = xmlGetNsProp(c,"as",NULL);
        new_sn->seqtype = df_seqtype_parse(as,filename,c->line,ei);
        free(as);
        if (NULL == new_sn->seqtype)
          return -1;
      }


      new_sn->flags |= FLAG_OVERRIDE;
      if (xmlHasNsProp(c,"override",NULL)) {
        char *override = xmlGetNsProp(c,"override",NULL);
        if (!strcmp(override,"no")) {
          new_sn->flags &= ~FLAG_OVERRIDE;
        }
        else if (strcmp(override,"yes")) {
          free(override);
          return invalid_attribute_val(ei,filename,c,"override");
        }
        free(override);
      }

      c2 = c->children;
      while ((NULL != c2) && check_element(c,"param",XSLT_NAMESPACE)) {
        CHECK_CALL(parse_param(sn,c2,doc,ei,filename))
        xslt_next_element(&c2);
      }

      CHECK_CALL(parse_sequence_constructors(new_sn,c,doc,ei,filename))
    }
    else if (check_element(c,"import-schema",XSLT_NAMESPACE)) {
      /* FIXME */
    }
    else if (check_element(c,"include",XSLT_NAMESPACE)) {
      /* FIXME */
    }
    else if (check_element(c,"key",XSLT_NAMESPACE)) {
      /* FIXME */
    }
    else if (check_element(c,"namespace-alias",XSLT_NAMESPACE)) {
      /* FIXME */
    }
    else if (check_element(c,"output",XSLT_NAMESPACE)) {
      /* FIXME */
    }
    else if (check_element(c,"preserve-space",XSLT_NAMESPACE)) {
      /* FIXME */
    }
    else if (check_element(c,"strip-space",XSLT_NAMESPACE)) {
      /* FIXME */
    }
    else if (check_element(c,"template",XSLT_NAMESPACE)) {
      char *match = xmlGetNsProp(c,"match",NULL);
      char *name = xmlGetNsProp(c,"name",NULL);
      /* FIXME: support "priority" */
      /* FIXME: support "mode" */
      /* FIXME: support "as" */

      /* FIXME: ensure params get processed */

      new_sn = add_node(&child_ptr,XSLT_DECL_TEMPLATE,filename,c);

      if (name)
        new_sn->qname = get_qname(name);
      if (match && (NULL == (new_sn->select = get_expr(c,"match",match))))
        return -1;
      CHECK_CALL(parse_sequence_constructors(new_sn,c,doc,ei,filename))
    }
    else if (XML_ELEMENT_NODE == c->type) {
      return parse_error(c,"Invalid element");
    }
    /* FIXME: check for other stuff that can appear here like <variable> and <param> */
  }

  return 0;
}

int parse_xslt(FILE *f, xl_snode *sroot, error_info *ei, const char *filename)
{
  xmlDocPtr doc;
  xmlNodePtr root;

  if (NULL == (doc = xmlReadFd(fileno(f),NULL,NULL,0))) {
    fprintf(stderr,"XML parse error.\n");
    return -1;
  }

  if (NULL == (root = xmlDocGetRootElement(doc))) {
    fprintf(stderr,"No root element.\n");
    xmlFreeDoc(doc);
    return -1;
  }

  if (!check_element(root,"transform",XSLT_NAMESPACE)) {
    printf("Invalid element at root: %s\n",root->name);
    xmlFreeDoc(doc);
    return -1;
  }

  if (0 != parse_decls(sroot,root,doc,ei,filename)) {
    xmlFreeDoc(doc);
    return -1;
  }

  add_ns_defs(sroot,root);

  if (root->ns) {
/*     printf("root element namespace prefix \"%s\" href \"%s\"\n", */
/*            root->ns->prefix,root->ns->href); */
/*   printf("root element prefix = \"%s\"\n",root->prefix); */
  }


/*   print_node(doc,root,0); */

  xmlFreeDoc(doc);

  xl_snode_set_parent(sroot,NULL);

/*   printf("=================== DONE BUILDING TREE FROM XSL SOURCE =================\n"); */
  return 0;
}













void expr_attr(xmlTextWriter *writer, char *attrname, xp_expr *expr, int brackets)
{
  stringbuf *buf = stringbuf_new();

  if (brackets) {
    if (XPATH_EXPR_STRING_LITERAL == expr->type) {
      /* expression can just be used directly as a value */
      xmlTextWriterWriteAttribute(writer,attrname,expr->strval);
    }
    else {
      stringbuf_printf(buf,"{");
      xp_expr_serialize(buf,expr,0);
      stringbuf_printf(buf,"}");
      xmlTextWriterWriteAttribute(writer,attrname,buf->data);
    }
  }
  else {
    xp_expr_serialize(buf,expr,0);
    xmlTextWriterWriteAttribute(writer,attrname,buf->data);
  }

  stringbuf_free(buf);
}

void qname_attr(xmlTextWriter *writer, char *attrname, qname_t qname)
{
  stringbuf *buf = stringbuf_new();
  if (qname.prefix)
    stringbuf_printf(buf,"%s:%s",qname.prefix,qname.localpart);
  else
    stringbuf_printf(buf,"%s",qname.localpart);
  xmlTextWriterWriteAttribute(writer,attrname,buf->data);
  stringbuf_free(buf);
}

void qname_list_attr(xmlTextWriter *writer, char *attrname, qname_t *qnames)
{
  stringbuf *buf = stringbuf_new();
  int i;
  for (i = 0; qnames[i].localpart; i++) {
    if (qnames[i].prefix)
      stringbuf_printf(buf,"%s:%s",qnames[i].prefix,qnames[i].localpart);
    else
      stringbuf_printf(buf,"%s",qnames[i].localpart);
    if (qnames[i+1].localpart)
      stringbuf_printf(buf," ");
  }
  xmlTextWriterWriteAttribute(writer,attrname,buf->data);
  stringbuf_free(buf);
}

static void xslt_start_element(xmlTextWriter *writer, xl_snode *sn, const char *name)
{
  list *l;
  char *qname = build_qname("xsl",name);
  xmlTextWriterStartElement(writer,qname);
  free(qname);

  for (l = sn->namespaces->defs; l; l = l->next) {
    ns_def *def = (ns_def*)l->data;
    char *attrname = (char*)malloc(strlen("xmlns:")+strlen(def->prefix)+1);
    sprintf(attrname,"xmlns:%s",def->prefix);
    xmlTextWriterWriteAttribute(writer,attrname,def->href);
    free(attrname);
  }
}

static void xslt_end_element(xmlTextWriter *writer)
{
  xmlTextWriterEndElement(writer);
}

void output_xslt_with_param(xmlTextWriter *writer, xl_snode *wp)
{
  xl_snode *c;

  assert(XSLT_WITH_PARAM == wp->type);
  xslt_start_element(writer,wp,"with-param");

  qname_attr(writer,"name",wp->qname);
  /* FIXME: attribute "as" for seqtype */
  if (wp->select)
    expr_attr(writer,"select",wp->select,0);
  if (wp->flags & FLAG_TUNNEL)
    xmlTextWriterWriteAttribute(writer,"tunnel","yes");

  for (c = wp->child; c; c = c->next)
    output_xslt_sequence_constructor(writer,c);
  xslt_end_element(writer);
}

void output_xslt_param(xmlTextWriter *writer, xl_snode *node)
{
  assert(XSLT_PARAM == node->type);

  xslt_start_element(writer,node,"param");

  /* FIXME: attribute "as" for seqtype */

  qname_attr(writer,"name",node->qname);

  /* flags */
  if (node->flags & FLAG_REQUIRED)
    xmlTextWriterWriteAttribute(writer,"required","yes");
  if (node->flags & FLAG_TUNNEL)
    xmlTextWriterWriteAttribute(writer,"tunnel","yes");

  /* default value */
  if (node->select)
    expr_attr(writer,"select",node->select,0);
  else if (node->child)
    output_xslt_sequence_constructor(writer,node->child);
    
  xslt_end_element(writer);

}

void output_xslt_sort(xmlTextWriter *writer, xl_snode *node)
{
  xl_snode *c;
  /* FIXME: support for "lang" */
  /* FIXME: support for "order" */
  /* FIXME: support for "collation" */
  /* FIXME: support for "stable" */
  /* FIXME: support for "case-order" */
  /* FIXME: support for "data-type" */
  xslt_start_element(writer,node,"sort");
  if (node->select) {
    expr_attr(writer,"select",node->select,0);
  }
  else {
    for (c = node->child; c; c = c->next)
      output_xslt_sequence_constructor(writer,c);
  }
  xslt_end_element(writer);
}

void output_xslt_sequence_constructor(xmlTextWriter *writer, xl_snode *node)
{
  xl_snode *c;
  xl_snode *c2;

  switch (node->type) {
  case XSLT_VARIABLE:
    xslt_start_element(writer,node,"variable");
    qname_attr(writer,"name",node->qname);
    /* FIXME: attribute "as" for seqtype */
    if (node->select)
      expr_attr(writer,"select",node->select,0);
    else if (node->child) {
      for (c = node->child; c; c = c->next)
        output_xslt_sequence_constructor(writer,c);
    }
    xslt_end_element(writer);
    break;
  case XSLT_INSTR_ANALYZE_STRING:
    xslt_start_element(writer,node,"analyze-string");
    expr_attr(writer,"select",node->select,0);
    expr_attr(writer,"regex",node->expr1,1);
    if (node->expr2)
      expr_attr(writer,"flags",node->expr2,1);
    for (c = node->child; c; c = c->next) {

      if (XSLT_MATCHING_SUBSTRING == c->type) {
        xslt_start_element(writer,c,"matching-substring");
        for (c2 = c->child; c2; c2 = c2->next)
          output_xslt_sequence_constructor(writer,c2);
        xslt_end_element(writer);
      }
      else if (XSLT_NON_MATCHING_SUBSTRING == c->type) {
        xslt_start_element(writer,c,"non-matching-substring");
        for (c2 = c->child; c2; c2 = c2->next)
          output_xslt_sequence_constructor(writer,c2);
        xslt_end_element(writer);
      }
      else {
        output_xslt_sequence_constructor(writer,c);
    }
    }
    xslt_end_element(writer);
    break;
  case XSLT_INSTR_APPLY_IMPORTS:
    xslt_start_element(writer,node,"apply-imports");
    for (c = node->param; c; c = c->next)
      output_xslt_with_param(writer,c);
    xslt_end_element(writer);
    break;
  case XSLT_INSTR_APPLY_TEMPLATES:
    /* FIXME: support for multiple modes, as well as #default and #all */
    xslt_start_element(writer,node,"apply-templates");
    if (node->select)
      expr_attr(writer,"select",node->select,0);
    if (node->mode.localpart)
      qname_attr(writer,"mode",node->mode);
    for (c = node->param; c; c = c->next)
      output_xslt_with_param(writer,c);
    for (c = node->sort; c; c = c->next)
      output_xslt_sort(writer,c);
    xslt_end_element(writer);
    break;
  case XSLT_INSTR_ATTRIBUTE:
    /* FIXME: support for "namespace" attribute */
    /* FIXME: support for "separator" attribute */
    /* FIXME: support for "type" attribute */
    /* FIXME: support for "validation" attribute */
    xslt_start_element(writer,node,"attribute");
    if (node->qname.localpart)
      qname_attr(writer,"name",node->qname);
    else
      expr_attr(writer,"name",node->name_expr,1);
    if (node->select)
      expr_attr(writer,"select",node->select,0);
    for (c = node->child; c; c = c->next)
      output_xslt_sequence_constructor(writer,c);
    xslt_end_element(writer);
    break;
  case XSLT_INSTR_CALL_TEMPLATE:
    xslt_start_element(writer,node,"call-template");
    qname_attr(writer,"name",node->qname);
    for (c = node->param; c; c = c->next)
      output_xslt_with_param(writer,c);
    xslt_end_element(writer);
    break;
  case XSLT_INSTR_CHOOSE:
    xslt_start_element(writer,node,"choose");
    for (c = node->child; c; c = c->next) {
      if (XSLT_WHEN == c->type) {
        xslt_start_element(writer,c,"when");
        assert(c->select);
        expr_attr(writer,"test",c->select,0);
        for (c2 = c->child; c2; c2 = c2->next)
          output_xslt_sequence_constructor(writer,c2);
        xslt_end_element(writer);
      }
      else if (XSLT_OTHERWISE == c->type) {
        xslt_start_element(writer,c,"otherwise");
        for (c2 = c->child; c2; c2 = c2->next)
          output_xslt_sequence_constructor(writer,c2);
        xslt_end_element(writer);
      }
      else {
        /* FIXME: return with error, don't exit! */
        fprintf(stderr,"Unexpected element inside <xsl:transform>: %d\n",c->type);
        exit(1);
      }
    }
    xslt_end_element(writer);
    break;
  case XSLT_INSTR_COMMENT:
    xslt_start_element(writer,node,"comment");
    if (node->select)
      expr_attr(writer,"select",node->select,0);
    for (c = node->child; c; c = c->next)
      output_xslt_sequence_constructor(writer,c);
    xslt_end_element(writer);
    break;
  case XSLT_INSTR_COPY:
    /* FIXME: support "copy-namespaces" */
    /* FIXME: support "inherit-namespaces" */
    /* FIXME: support "use-attribute-sets" */
    /* FIXME: support "type" */
    /* FIXME: support "validation" */
    xslt_start_element(writer,node,"copy");
    for (c = node->child; c; c = c->next)
      output_xslt_sequence_constructor(writer,c);
    xslt_end_element(writer);
    break;
  case XSLT_INSTR_COPY_OF:
    /* FIXME: support "copy-namespaces" */
    /* FIXME: support "type" */
    /* FIXME: support "validation" */
    xslt_start_element(writer,node,"copy-of");
    expr_attr(writer,"select",node->select,0);
    xslt_end_element(writer);
    break;
  case XSLT_INSTR_ELEMENT:
    /* FIXME: support "namespace" */
    /* FIXME: support "inherit-namespace" */
    /* FIXME: support "use-attribute-sets" */
    /* FIXME: support "type" */
    /* FIXME: support "validation" */
    if (node->qname.localpart) {
      stringbuf *buf = stringbuf_new();
      if (node->qname.prefix)
        stringbuf_printf(buf,"%s:%s",node->qname.prefix,node->qname.localpart);
      else
        stringbuf_printf(buf,"%s",node->qname.localpart);
      xmlTextWriterStartElement(writer,buf->data);
      stringbuf_free(buf);
    }
    else {
      xmlTextWriterStartElement(writer,"xsl:element");
      expr_attr(writer,"name",node->name_expr,1);
    }
    for (c = node->child; c; c = c->next)
      output_xslt_sequence_constructor(writer,c);
    xmlTextWriterEndElement(writer);
    break;
  case XSLT_INSTR_FALLBACK:
    xslt_start_element(writer,node,"fallback");
    for (c = node->child; c; c = c->next)
      output_xslt_sequence_constructor(writer,c);
    xslt_end_element(writer);
    break;
  case XSLT_INSTR_FOR_EACH:
    xslt_start_element(writer,node,"for-each");
    expr_attr(writer,"select",node->select,0);
    for (c = node->sort; c; c = c->next)
      output_xslt_sort(writer,c);
    for (c = node->child; c; c = c->next)
      output_xslt_sequence_constructor(writer,c);
    xslt_end_element(writer);
    break;
  case XSLT_INSTR_FOR_EACH_GROUP:
    /* FIXME: support "collation" */
    /* FIXME: enforce the restriction that for starting-with and ending-with, the result
       must be a pattern (xslt section 5.5.2) */
    xslt_start_element(writer,node,"for-each-group");
    expr_attr(writer,"select",node->select,0);
    switch (node->gmethod) {
    case XSLT_GROUPING_BY:
      expr_attr(writer,"group-by",node->expr1,0);
      break;
    case XSLT_GROUPING_ADJACENT:
      expr_attr(writer,"group-adjacent",node->expr1,0);
      break;
    case XSLT_GROUPING_STARTING_WITH:
      expr_attr(writer,"group-starting-with",node->expr1,0);
      break;
    case XSLT_GROUPING_ENDING_WITH:
      expr_attr(writer,"group-ending-with",node->expr1,0);
      break;
    default:
      assert(0);
      break;
    } 
    for (c = node->sort; c; c = c->next)
      output_xslt_sort(writer,c);
    for (c = node->child; c; c = c->next)
      output_xslt_sequence_constructor(writer,c);
    xslt_end_element(writer);
    break;
  case XSLT_INSTR_IF:
    xslt_start_element(writer,node,"if");
    expr_attr(writer,"test",node->select,0);
    for (c = node->child; c; c = c->next)
      output_xslt_sequence_constructor(writer,c);
    xslt_end_element(writer);
    break;
  case XSLT_INSTR_MESSAGE:
    xslt_start_element(writer,node,"message");
    if (node->select)
      expr_attr(writer,"select",node->select,0);
    if (node->flags & FLAG_TERMINATE) {
      if (node->expr1)
        expr_attr(writer,"terminate",node->expr1,1);
      else
        xmlTextWriterWriteAttribute(writer,"terminate","yes");
    }
    for (c = node->child; c; c = c->next)
      output_xslt_sequence_constructor(writer,c);
    xslt_end_element(writer);
    break;
  case XSLT_INSTR_NAMESPACE:
    xslt_start_element(writer,node,"namespace");
    if (node->name_expr)
      expr_attr(writer,"name",node->name_expr,1);
    else
      xmlTextWriterWriteAttribute(writer,"name",node->strval);
    if (node->select)
      expr_attr(writer,"select",node->select,0);
    for (c = node->child; c; c = c->next)
      output_xslt_sequence_constructor(writer,c);
    xslt_end_element(writer);
    break;
  case XSLT_INSTR_NEXT_MATCH:
    xslt_start_element(writer,node,"next-match");
    for (c = node->param; c; c = c->next)
      output_xslt_with_param(writer,c);
    for (c = node->child; c; c = c->next)
      output_xslt_sequence_constructor(writer,c);
    xslt_end_element(writer);
    break;
  case XSLT_INSTR_NUMBER:
    /* FIXME */
    break;
  case XSLT_INSTR_PERFORM_SORT:
    xslt_start_element(writer,node,"perform-sort");
    if (node->select)
      expr_attr(writer,"select",node->select,0);
    for (c = node->sort; c; c = c->next)
      output_xslt_sort(writer,c);
    for (c = node->child; c; c = c->next)
      output_xslt_sequence_constructor(writer,c);
    xslt_end_element(writer);
    break;
  case XSLT_INSTR_PROCESSING_INSTRUCTION:
    xslt_start_element(writer,node,"processing-instruction");
    if (node->qname.localpart)
      qname_attr(writer,"name",node->qname);
    else
      expr_attr(writer,"name",node->name_expr,1);
    if (node->select)
      expr_attr(writer,"select",node->select,0);
    for (c = node->child; c; c = c->next)
      output_xslt_sequence_constructor(writer,c);
    xslt_end_element(writer);
    break;
  case XSLT_INSTR_RESULT_DOCUMENT:
    /* FIXME */
    break;
  case XSLT_INSTR_SEQUENCE:
    xslt_start_element(writer,node,"sequence");
    expr_attr(writer,"select",node->select,0);
    for (c = node->child; c; c = c->next)
      output_xslt_sequence_constructor(writer,c);
    xslt_end_element(writer);
    break;
  case XSLT_INSTR_TEXT:
    /* FIXME: support "disable-output-escaping" */
    xslt_start_element(writer,node,"text");
    xmlTextWriterWriteFormatString(writer,"%s",node->strval);
    xslt_end_element(writer);
    break;
  case XSLT_INSTR_VALUE_OF:
    xslt_start_element(writer,node,"value-of");
    if (node->select)
      expr_attr(writer,"select",node->select,0);
    if (node->expr1)
      expr_attr(writer,"separator",node->expr1,1);
    for (c = node->child; c; c = c->next)
      output_xslt_sequence_constructor(writer,c);
    xslt_end_element(writer);
    break;
  /* FIXME: allow result-elements here */
  default:
    break;
  }
}

/*

Section 3.6: Stylesheet Element
Content: (xsl:import*, other-declarations)

Note: <xsl:stylesheet> and <xsl:transform> have the same meaning. In the syntax
tree they are treated as XSLT_TRANSFORM

*/
void output_xslt(FILE *f, xl_snode *node)
{
  xl_snode *sn;
  xl_snode *c;
  xmlOutputBuffer *buf = xmlOutputBufferCreateFile(f,NULL);
  xmlTextWriter *writer = xmlNewTextWriter(buf);

  assert(XSLT_TRANSFORM == node->type);

  xmlTextWriterSetIndent(writer,1);
  xmlTextWriterSetIndentString(writer,"  ");
  xmlTextWriterStartDocument(writer,NULL,NULL,NULL);
  xslt_start_element(writer,node,"transform");
  xmlTextWriterWriteAttribute(writer,"version","2.0");

  for (sn = node->child; sn && (XSLT_IMPORT == sn->type); sn = sn->next) {
    /* import */
  }

  for (; sn; sn = sn->next) {
    switch (sn->type) {
    case XSLT_DECL_INCLUDE:
      break;
    case XSLT_DECL_ATTRIBUTE_SET:
      break;
    case XSLT_DECL_CHARACTER_MAP:
      break;
    case XSLT_DECL_DECIMAL_FORMAT:
      break;
    case XSLT_DECL_FUNCTION: {
      stringbuf *buf;
      xslt_start_element(writer,sn,"function");
      qname_attr(writer,"name",sn->qname);
      if (!(sn->flags & FLAG_OVERRIDE))
        xmlTextWriterWriteAttribute(writer,"override","no");

      if (NULL != sn->seqtype) {
        buf = stringbuf_new();
        df_seqtype_print_xpath(buf,sn->seqtype,sn->namespaces);
        xmlTextWriterWriteAttribute(writer,"as",buf->data);
        stringbuf_free(buf);
      }

      for (c = sn->param; c; c = c->next)
        output_xslt_param(writer,c);
      for (c = sn->child; c; c = c->next)
        output_xslt_sequence_constructor(writer,c);
      xslt_end_element(writer);
      break;
    }
    case XSLT_DECL_IMPORT_SCHEMA:
      break;
    case XSLT_DECL_KEY:
      break;
    case XSLT_DECL_NAMESPACE_ALIAS:
      break;
    case XSLT_DECL_OUTPUT:
      break;
    case XSLT_PARAM:
      break;
    case XSLT_DECL_PRESERVE_SPACE:
      break;
    case XSLT_DECL_STRIP_SPACE:
      break;
    case XSLT_DECL_TEMPLATE:
      /* FIXME: support "priority" */
      /* FIXME: support "mode" */
      /* FIXME: support "as" */
      xslt_start_element(writer,sn,"template");
      if (sn->qname.localpart)
        qname_attr(writer,"name",sn->qname);
      if (sn->select)
        expr_attr(writer,"match",sn->select,0);
      for (c = sn->param; c; c = c->next)
        output_xslt_param(writer,c);
      for (c = sn->child; c; c = c->next)
        output_xslt_sequence_constructor(writer,c);
      xslt_end_element(writer);
      break;
    case XSLT_VARIABLE:
      break;
    default:
      /* FIXME: return with error, don't exit! */
      fprintf(stderr,"Unexpected element inside <xsl:transform>: %d\n",sn->type);
      exit(1);
      break;
    }
  }

  xslt_end_element(writer);
  xmlTextWriterEndDocument(writer);
  xmlTextWriterFlush(writer);
  xmlFreeTextWriter(writer);
}

/*


  @implements(xslt20:normative-references-1) status { info } @end
  @implements(xslt20:other-references-1) status { info } @end


  @implements(xslt20:glossary-1) status { info } @end

  @implements(xslt20:d5e29073-1) status { info } @end
  @implements(xslt20:d5e29073-2) status { info } @end
  @implements(xslt20:d5e29073-3) status { info } @end
  @implements(xslt20:d5e29073-4) status { info } @end
  @implements(xslt20:d5e29073-5) status { info } @end
  @implements(xslt20:d5e29073-6) status { info } @end
  @implements(xslt20:d5e29073-7) status { info } @end
  @implements(xslt20:d5e29073-8) status { info } @end
  @implements(xslt20:d5e29073-9) status { info } @end

  @implements(xslt20:d5e29186-1) status { info } @end
  @implements(xslt20:d5e29186-2) status { info } @end
  @implements(xslt20:d5e29186-3) status { info } @end
  @implements(xslt20:d5e29186-4) status { info } @end
  @implements(xslt20:d5e29186-5) status { info } @end
  @implements(xslt20:d5e29186-6) status { info } @end
  @implements(xslt20:d5e29186-7) status { info } @end
  @implements(xslt20:d5e29186-8) status { info } @end
  @implements(xslt20:d5e29186-9) status { info } @end
  @implements(xslt20:d5e29186-10) status { info } @end
  @implements(xslt20:d5e29186-11) status { info } @end
  @implements(xslt20:d5e29186-12) status { info } @end
  @implements(xslt20:d5e29186-13) status { info } @end
  @implements(xslt20:d5e29186-14) status { info } @end
  @implements(xslt20:d5e29186-15) status { info } @end
  @implements(xslt20:d5e29186-16) status { info } @end
  @implements(xslt20:d5e29186-17) status { info } @end
  @implements(xslt20:d5e29186-18) status { info } @end
  @implements(xslt20:d5e29186-19) status { info } @end
  @implements(xslt20:d5e29186-20) status { info } @end
  @implements(xslt20:d5e29186-21) status { info } @end
  @implements(xslt20:d5e29186-22) status { info } @end
  @implements(xslt20:d5e29186-23) status { info } @end
  @implements(xslt20:d5e29186-24) status { info } @end
  @implements(xslt20:d5e29186-25) status { info } @end
  @implements(xslt20:d5e29186-26) status { info } @end
  @implements(xslt20:d5e29186-27) status { info } @end
  @implements(xslt20:d5e29186-28) status { info } @end
  @implements(xslt20:d5e29186-29) status { info } @end
  @implements(xslt20:d5e29186-30) status { info } @end
  @implements(xslt20:d5e29186-31) status { info } @end
  @implements(xslt20:d5e29186-32) status { info } @end
  @implements(xslt20:d5e29186-33) status { info } @end
  @implements(xslt20:d5e29186-34) status { info } @end
  @implements(xslt20:d5e29186-35) status { info } @end
  @implements(xslt20:d5e29186-36) status { info } @end
  @implements(xslt20:d5e29186-37) status { info } @end
  @implements(xslt20:d5e29186-38) status { info } @end
  @implements(xslt20:d5e29186-39) status { info } @end
  @implements(xslt20:d5e29186-40) status { info } @end
  @implements(xslt20:d5e29186-41) status { info } @end
  @implements(xslt20:d5e29186-42) status { info } @end
  @implements(xslt20:d5e29186-43) status { info } @end
  @implements(xslt20:d5e29186-44) status { info } @end
  @implements(xslt20:d5e29186-45) status { info } @end
  @implements(xslt20:d5e29186-46) status { info } @end
  @implements(xslt20:d5e29186-47) status { info } @end
  @implements(xslt20:d5e29186-48) status { info } @end
  @implements(xslt20:d5e29186-49) status { info } @end
  @implements(xslt20:d5e29186-50) status { info } @end
  @implements(xslt20:d5e29186-51) status { info } @end
  @implements(xslt20:d5e29186-52) status { info } @end
  @implements(xslt20:d5e29186-53) status { info } @end
  @implements(xslt20:d5e29186-54) status { info } @end
  @implements(xslt20:d5e29186-55) status { info } @end
  @implements(xslt20:d5e29186-56) status { info } @end
  @implements(xslt20:d5e29186-57) status { info } @end
  @implements(xslt20:d5e29186-58) status { info } @end
  @implements(xslt20:d5e29186-59) status { info } @end
  @implements(xslt20:d5e29186-60) status { info } @end
  @implements(xslt20:d5e29186-61) status { info } @end
  @implements(xslt20:d5e29186-62) status { info } @end
  @implements(xslt20:d5e29186-63) status { info } @end
  @implements(xslt20:d5e29186-64) status { info } @end
  @implements(xslt20:d5e29186-65) status { info } @end
  @implements(xslt20:d5e29186-66) status { info } @end
  @implements(xslt20:d5e29186-67) status { info } @end
  @implements(xslt20:d5e29186-68) status { info } @end
  @implements(xslt20:d5e29186-69) status { info } @end
  @implements(xslt20:d5e29186-70) status { info } @end
  @implements(xslt20:d5e29186-71) status { info } @end
  @implements(xslt20:d5e29186-72) status { info } @end
  @implements(xslt20:d5e29186-73) status { info } @end
  @implements(xslt20:d5e29186-74) status { info } @end
  @implements(xslt20:d5e29186-75) status { info } @end
  @implements(xslt20:d5e29186-76) status { info } @end
  @implements(xslt20:d5e29186-77) status { info } @end
  @implements(xslt20:d5e29186-78) status { info } @end
  @implements(xslt20:d5e29186-79) status { info } @end
  @implements(xslt20:d5e29186-80) status { info } @end
  @implements(xslt20:d5e29186-81) status { info } @end
  @implements(xslt20:d5e29186-82) status { info } @end
  @implements(xslt20:d5e29186-83) status { info } @end
  @implements(xslt20:d5e29186-84) status { info } @end
  @implements(xslt20:d5e29186-85) status { info } @end
  @implements(xslt20:d5e29186-86) status { info } @end
  @implements(xslt20:d5e29186-87) status { info } @end
  @implements(xslt20:d5e29186-88) status { info } @end
  @implements(xslt20:d5e29186-89) status { info } @end
  @implements(xslt20:d5e29186-90) status { info } @end
  @implements(xslt20:d5e29186-91) status { info } @end
  @implements(xslt20:d5e29186-92) status { info } @end
  @implements(xslt20:d5e29186-93) status { info } @end
  @implements(xslt20:d5e29186-94) status { info } @end
  @implements(xslt20:d5e29186-95) status { info } @end
  @implements(xslt20:d5e29186-96) status { info } @end
  @implements(xslt20:d5e29186-97) status { info } @end
  @implements(xslt20:d5e29186-98) status { info } @end
  @implements(xslt20:d5e29186-99) status { info } @end
  @implements(xslt20:d5e29186-100) status { info } @end
  @implements(xslt20:d5e29186-101) status { info } @end
  @implements(xslt20:d5e29186-102) status { info } @end
  @implements(xslt20:d5e29186-103) status { info } @end
  @implements(xslt20:d5e29186-104) status { info } @end
  @implements(xslt20:d5e29186-105) status { info } @end
  @implements(xslt20:d5e29186-106) status { info } @end
  @implements(xslt20:d5e29186-107) status { info } @end
  @implements(xslt20:d5e29186-108) status { info } @end
  @implements(xslt20:d5e29186-109) status { info } @end
  @implements(xslt20:d5e29186-110) status { info } @end
  @implements(xslt20:d5e29186-111) status { info } @end
  @implements(xslt20:d5e29186-112) status { info } @end
  @implements(xslt20:d5e29186-113) status { info } @end
  @implements(xslt20:d5e29186-114) status { info } @end
  @implements(xslt20:d5e29186-115) status { info } @end
  @implements(xslt20:d5e29186-116) status { info } @end
  @implements(xslt20:d5e29186-117) status { info } @end
  @implements(xslt20:d5e29186-118) status { info } @end
  @implements(xslt20:d5e29186-119) status { info } @end
  @implements(xslt20:d5e29186-120) status { info } @end
  @implements(xslt20:d5e29186-121) status { info } @end
  @implements(xslt20:d5e29186-122) status { info } @end
  @implements(xslt20:d5e29186-123) status { info } @end
  @implements(xslt20:d5e29186-124) status { info } @end
  @implements(xslt20:d5e29186-125) status { info } @end
  @implements(xslt20:d5e29186-126) status { info } @end
  @implements(xslt20:d5e29186-127) status { info } @end
  @implements(xslt20:d5e29186-128) status { info } @end
  @implements(xslt20:d5e29186-129) status { info } @end
  @implements(xslt20:d5e29186-130) status { info } @end
  @implements(xslt20:d5e29186-131) status { info } @end
  @implements(xslt20:d5e29186-132) status { info } @end
  @implements(xslt20:d5e29186-133) status { info } @end
  @implements(xslt20:d5e29186-134) status { info } @end
  @implements(xslt20:d5e29186-135) status { info } @end
  @implements(xslt20:d5e29186-136) status { info } @end
  @implements(xslt20:d5e29186-137) status { info } @end
  @implements(xslt20:d5e29186-138) status { info } @end
  @implements(xslt20:d5e29186-139) status { info } @end
  @implements(xslt20:d5e29186-140) status { info } @end
  @implements(xslt20:d5e29186-141) status { info } @end
  @implements(xslt20:d5e29186-142) status { info } @end
  @implements(xslt20:d5e29186-143) status { info } @end
  @implements(xslt20:d5e29186-144) status { info } @end
  @implements(xslt20:d5e29186-145) status { info } @end
  @implements(xslt20:d5e29186-146) status { info } @end
  @implements(xslt20:d5e29186-147) status { info } @end
  @implements(xslt20:d5e29186-148) status { info } @end
  @implements(xslt20:d5e29186-149) status { info } @end
  @implements(xslt20:d5e29186-150) status { info } @end
  @implements(xslt20:d5e29186-151) status { info } @end
  @implements(xslt20:d5e29186-152) status { info } @end
  @implements(xslt20:d5e29186-153) status { info } @end
  @implements(xslt20:d5e29186-154) status { info } @end
  @implements(xslt20:d5e29186-155) status { info } @end
  @implements(xslt20:d5e29186-156) status { info } @end
  @implements(xslt20:d5e29186-157) status { info } @end
  @implements(xslt20:d5e29186-158) status { info } @end
  @implements(xslt20:d5e29186-159) status { info } @end
  @implements(xslt20:d5e29186-160) status { info } @end
  @implements(xslt20:d5e29186-161) status { info } @end
  @implements(xslt20:d5e29186-162) status { info } @end

  @implements(xslt20:incompatibilities-1) status { info } @end
  @implements(xslt20:incompatibilities-2) status { info } @end
  @implements(xslt20:backwards-compatibility-behavior-1) status { info } @end
  @implements(xslt20:backwards-compatibility-behavior-2) status { info } @end
  @implements(xslt20:backwards-compatibility-behavior-3) status { info } @end
  @implements(xslt20:backwards-compatibility-behavior-4) status { info } @end
  @implements(xslt20:incompatility-without-schema-1) status { info } @end
  @implements(xslt20:incompatility-without-schema-2) status { info } @end
  @implements(xslt20:compatibility-with-schema-1) status { info } @end
  @implements(xslt20:compatibility-with-schema-2) status { info } @end
  @implements(xslt20:xpath-compatibility-1) status { info } @end
  @implements(xslt20:xpath-compatibility-2) status { info } @end
  @implements(xslt20:changes-since-1.0-1) status { info } @end
  @implements(xslt20:changes-since-1.0-2) status { info } @end
  @implements(xslt20:changes-since-1.0-3) status { info } @end
  @implements(xslt20:pervasive-changes-1) status { info } @end
  @implements(xslt20:major-features-1) status { info } @end
  @implements(xslt20:minor-changes-1) status { info } @end
  @implements(xslt20:changes-2005-04-1) status { info } @end
  @implements(xslt20:changes-2005-04-2) status { info } @end

*/
