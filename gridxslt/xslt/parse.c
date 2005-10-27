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

#include "parse.h"
#include "util/namespace.h"
#include "util/stringbuf.h"
#include "util/macros.h"
#include "util/debug.h"
#include "dataflow/serialization.h"
#include <libxml/xmlwriter.h>
#include <libxml/xmlIO.h>
#include <libxml/parser.h>
#include <libxml/tree.h>
#include <libxml/uri.h>
#include <stdio.h>
#include <string.h>
#include <assert.h>
#include <ctype.h>

using namespace GridXSLT;

#define ALLOWED_ATTRIBUTES(_node,...) \
      CHECK_CALL(enforce_allowed_attributes(ei,filename,(_node),XSLT_NAMESPACE, \
                                            standard_attributes,0,__VA_ARGS__,NULL))
#define REQUIRED_ATTRIBUTE(_node,_name) \
      if (!XMLHasNsProp((_node),(_name),String::null())) \
        return missing_attribute2(ei,filename,(_node)->line,String::null(),(_name));

static int parse_sequence_constructors(Statement *sn, xmlNodePtr start, xmlDocPtr doc,
                                       Error *ei, const char *filename);

static int parse_xslt_uri(Error *ei, const char *filename, int line, const char *errname,
                          const char *full_uri, Statement *sroot, const char *refsource);

/* FIXME: should also allow xml:id (and others, e.g. XML Schema instance?) */
/* Should we really enforece the absence of attributes from other namespaces? Probably not
   strictly necessary and it may restrict other uses of the element trees which rely on
   the ability to add their own attributes */

static List<NSName> make_attrs()
{
  List<NSName> attrs;
  attrs.append(NSName(XSLT_NAMESPACE,"version"));
  attrs.append(NSName(XSLT_NAMESPACE,"exclude-result-prefixes"));
  attrs.append(NSName(XSLT_NAMESPACE,"extension-element-prefixes"));
  attrs.append(NSName(XSLT_NAMESPACE,"xpath-default-namespace"));
  attrs.append(NSName(XSLT_NAMESPACE,"default-collation"));
  attrs.append(NSName(XSLT_NAMESPACE,"use-when"));
  attrs.append(NSName(String::null(),"version"));
  attrs.append(NSName(String::null(),"exclude-result-prefixes"));
  attrs.append(NSName(String::null(),"extension-element-prefixes"));
  attrs.append(NSName(String::null(),"xpath-default-namespace"));
  attrs.append(NSName(String::null(),"default-collation"));
  attrs.append(NSName(String::null(),"use-when"));
  return attrs;
}

static List<NSName> standard_attributes = make_attrs();

static String xstr_as = "as";
static String xstr_byte_order_mark = "byte-order-mark";
static String xstr_cdata_section_elements = "cdata-section-elements";
static String xstr_disable_output_escaping = "disable-output-escaping";
static String xstr_doctype_public = "doctype-public";
static String xstr_doctype_system = "doctype-system";
static String xstr_elements = "elements";
static String xstr_encoding = "encoding";
static String xstr_escape_uri_attributes = "escape-uri-attributes";
static String xstr_href = "href";
static String xstr_include_content_type = "include-content-type";
static String xstr_indent = "indent";
static String xstr_match = "match";
static String xstr_media_type = "media-type";
static String xstr_method = "method";
static String xstr_mode = "mode";
static String xstr_name = "name";
static String xstr_normalization_form = "normalization-form";
static String xstr_omit_xml_declaration = "omit-xml-declaration";
static String xstr_override = "override";
static String xstr_priority = "priority";
static String xstr_required = "required";
static String xstr_select = "select";
static String xstr_separator = "separator";
static String xstr_standalone = "standalone";
static String xstr_tunnel = "tunnel";
static String xstr_undeclare_prefixes = "undeclare-prefixes";
static String xstr_use_character_maps = "use-character-maps";
static String xstr_version = "version";


static void xslt_skip_others(xmlNodePtr *c)
{
  while ((*c) &&
         ((XML_COMMENT_NODE == (*c)->type) ||
          (XML_PI_NODE == (*c)->type) ||
          ((XML_TEXT_NODE == (*c)->type) &&
           is_all_whitespace((*c)->content,strlen((*c)->content)))))
    (*c) = (*c)->next;
}

static void xslt_next_element(xmlNodePtr *c)
{
  *c = (*c)->next;
  xslt_skip_others(c);
}

static xmlNodePtr xslt_first_child(xmlNodePtr n)
{
  xmlNodePtr c = n->children;
  xslt_skip_others(&c);
  return c;
}

static void print_node(xmlDocPtr doc, xmlNodePtr n, int indent)
{
  xmlNodePtr c;
  xmlNsPtr nslist;
  int i;

  for (nslist = n->nsDef; nslist; nslist = nslist->next)
    message("NAMESPACE %d \"%s\": \"%s\"\n",nslist->type,nslist->prefix,nslist->href);

  if (XML_ELEMENT_NODE == n->type) {
    xmlAttrPtr attr;

    for (i = 0; i < indent; i++)
      message("  ");
    message("%s",n->name);

    for (attr = n->properties; attr; attr = attr->next)
      message(" %s=%s",attr->name,XMLGetNsProp(n,attr->name,String::null()));


    message("\n");
  }
  for (c = n->children; c; c = c->next)
    print_node(doc,c,indent+1);
}

static Expression *get_expr(Error *ei, const char *filename,
                         xmlNodePtr n, const char *attrname, const char *str, int pattern)
{
  return Expression_parse(str,filename,n->line,ei,pattern);
}

static Expression *get_expr_attr(Error *ei, const char *filename,
                              xmlNodePtr n, const char *attrname, const char *str)
{
  if ((2 <= strlen(str) && ('{' == str[0]) && ('}' == str[strlen(str)-1]))) {
    char *expr_str = (char*)alloca(strlen(str)-1);
    memcpy(expr_str,str+1,strlen(str)-2);
    expr_str[strlen(str)-2] = '\0';
    return get_expr(ei,filename,n,attrname,expr_str,0);
  }
  else if ((NULL != strchr(str,'{')) || (NULL != strchr(str,'}'))) {
    fmessage(stderr,"FIXME: I don't support handling attributes with only partial "
                   "expressions yet!\n");
    exit(1);
  }
  else {
    Expression *expr = new Expression(XPATH_EXPR_STRING_LITERAL,NULL,NULL);
    expr->m_strval = str;
    return expr;
  }
}

static void add_ns_defs(Statement *sn, xmlNodePtr node)
{
  xmlNsPtr ns;
  for (ns = node->nsDef; ns; ns = ns->next)
    sn->m_namespaces->add_direct(ns->href,ns->prefix);
}

static Statement *add_node(Statement ***ptr, int type, const char *filename, xmlNodePtr n)
{
  Statement *new_node = new Statement(type);
  new_node->m_sloc.uri = strdup(filename);
  new_node->m_sloc.line = n->line;
  add_ns_defs(new_node,n);
  **ptr = new_node;
  *ptr = &new_node->m_next;
  return new_node;
}

static int xslt_invalid_element(Error *ei, const char *filename, xmlNodePtr n)
{
  /* @implements(xslt20:import-4)
     test { xslt/parse/import_nottoplevel.test }
     @end */
  if (check_element(n,"import",XSLT_NAMESPACE))
    return error(ei,filename,n->line,"XTSE0190",
                 "An xsl:import element must be a top-level element");
  /* @implements(xslt20:include-4)
     test { xslt/parse/include_nottoplevel.test }
     @end */
  else if (check_element(n,"include",XSLT_NAMESPACE))
    return error(ei,filename,n->line,"XTSE0170",
                 "An xsl:include element must be a top-level element");
  else
    return invalid_element2(ei,filename,n);
}


static int parse_optional_as(xmlNodePtr n, Statement *sn, Error *ei, const char *filename)
{
  if (XMLHasNsProp(n,"as",String::null())) {
    char *as = XMLGetNsProp(n,"as",String::null());
    sn->m_st = seqtype_parse(as,filename,n->line,ei);
    free(as);
    if (sn->m_st.isNull())
      return -1;
  }
  return 0;
}

static int parse_param(Statement *sn, xmlNodePtr n, xmlDocPtr doc,
                       Error *ei, const char *filename)
{
  char *name;
/*   char *select = XMLGetNsProp(n,"select",String::null()); */
/*   char *required = XMLGetNsProp(n,"required",String::null()); */
/*   char *tunnel = XMLGetNsProp(n,"tunnel",String::null()); */
  Statement **param_ptr = &sn->m_param;
  Statement *new_sn;
  char *fullname;
  char *fullns;

  /* @implements(xslt20:parameters-5) status { info } @end */

/*   ALLOWED_ATTRIBUTES(n,"name","select","as","required","tunnel") */
  ALLOWED_ATTRIBUTES(n,&xstr_name,&xstr_select,&xstr_as,&xstr_required,&xstr_tunnel)
  while (*param_ptr)
    param_ptr = &((*param_ptr)->m_next);

  if ((XSLT_TRANSFORM != sn->m_type) &&
      (XSLT_DECL_FUNCTION != sn->m_type) &&
      (XSLT_DECL_TEMPLATE != sn->m_type))
    return xslt_invalid_element(ei,filename,n);

  new_sn = add_node(&param_ptr,XSLT_PARAM,filename,n);


  /* @implements(xslt20:parameters-3)
     test { xslt/parse/function_param1.test }
     test { test/xslt/parse/function_param2.test }
     test { test/xslt/parse/function_param3.test }
     test { test/xslt/parse/function_param_noname.test }
     @end */
  REQUIRED_ATTRIBUTE(n,"name")

  name = XMLGetNsProp(n,"name",String::null());
  new_sn->m_qn = QName::parse(name);

  if (0 != get_ns_name_from_qname(n,doc,name,&fullns,&fullname)) {
    free(name);
    return error(ei,filename,n->line,"XTSE0280","Could not resolve namespace for prefix \"%*\"",
                 &new_sn->m_qn.m_prefix);
  }
  free(name);
  free(fullname);
  free(fullns);

  /* @implements(xslt20:parameters-8)
     test { xslt/parse/param_as1.test }
     test { xslt/parse/param_as2.test }
     test { xslt/parse/param_as3.test }
     @end */
  CHECK_CALL(parse_optional_as(n,new_sn,ei,filename))

#if 0
  if (NULL == name)
    return missing_attribute(n,"name");
  new_sn->m_qn = QName::parse(name);
  if ((NULL != select) && (NULL == (new_sn->m_select = get_expr(ei,filename,n,"select",select,0))))
    return -1;
  if ((NULL != required) && !strcmp(required,"yes"))
    new_sn->m_flags |= FLAG_REQUIRED;
  if ((NULL != tunnel) && !strcmp(tunnel,"yes"))
    new_sn->m_flags |= FLAG_TUNNEL;
#endif

  CHECK_CALL(parse_sequence_constructors(new_sn,n->children,doc,ei,filename))

  return 0;
}

static int parse_sequence_constructors(Statement *sn, xmlNodePtr start, xmlDocPtr doc,
                                       Error *ei, const char *filename)
{
  xmlNodePtr c;
  xmlNodePtr c2;
  Statement *new_sn;
  Statement **child_ptr = &sn->m_child;
  Statement **param_ptr = &sn->m_param;
  Statement **sort_ptr = &sn->m_sort;

  while (NULL != *child_ptr)
    child_ptr = &((*child_ptr)->m_next);

  /* FIXME: still need to do more checks on the tree after to test for e.g. duplicate parameter
     names, and content supplied inside <comment>s in addition to "select" being specified */

  /* FIXME: for yes/no values, complain if the value is something other than "yes" or "no"
     (unless of course the attribute is missing and not required). Should this check be
     case insensitive? */

  /* FIXME: verify that all missing_attribute() calls have a return before them! */

  xslt_skip_others(&start);
  for (c = start; c; xslt_next_element(&c)) {
    if (check_element(c,"analyze-string",XSLT_NAMESPACE)) {
      /* FIXME: add testcases to all.xl for use of fallback inside analyze-string */
      char *select = XMLGetNsProp(c,"select",String::null());
      char *regex = XMLGetNsProp(c,"regex",String::null());
      char *flags = XMLGetNsProp(c,"flags",String::null());
      int mcount = 0;
      int nmcount = 0;
      Statement **child2_ptr;

      if (NULL == select)
        return missing_attribute(c,"select");
      if (NULL == regex)
        return missing_attribute(c,"regex");

      new_sn = add_node(&child_ptr,XSLT_INSTR_ANALYZE_STRING,filename,c);
      child2_ptr = &new_sn->m_child;

      if (NULL == (new_sn->m_select = get_expr(ei,filename,c,"select",select,0)))
        return -1;
      if (NULL == (new_sn->m_expr1 = get_expr_attr(ei,filename,c,"regex",regex)))
        return -1;
      if ((NULL != flags) && (NULL == (new_sn->m_expr2 = get_expr_attr(ei,filename,c,"flags",flags))))
        return -1;

      for (c2 = c->children; c2; c2 = c2->next) {
        if (check_element(c2,"matching-substring",XSLT_NAMESPACE)) {
          Statement *matching = add_node(&child2_ptr,XSLT_MATCHING_SUBSTRING,filename,c2);
          CHECK_CALL(parse_sequence_constructors(matching,c2->children,doc,ei,filename))
          mcount++;
        }
        else if (check_element(c2,"non-matching-substring",XSLT_NAMESPACE)) {
          Statement *non_matching = add_node(&child2_ptr,XSLT_NON_MATCHING_SUBSTRING,filename,c2);
          CHECK_CALL(parse_sequence_constructors(non_matching,c2->children,doc,ei,filename))
          nmcount++;
        }
        else if (check_element(c2,"fallback",XSLT_NAMESPACE)) {
          Statement *fallback = add_node(&child2_ptr,XSLT_INSTR_FALLBACK,filename,c2);
          CHECK_CALL(parse_sequence_constructors(fallback,c2->children,doc,ei,filename))
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

      CHECK_CALL(parse_sequence_constructors(new_sn,c->children,doc,ei,filename))
    }
    else if (check_element(c,"apply-templates",XSLT_NAMESPACE)) {
      char *select = XMLGetNsProp(c,"select",String::null());
      char *mode = XMLGetNsProp(c,"mode",String::null());

      /* make sure all children are <xsl:sort> or <xsl:with-param> elements */
      for (c2 = c->children; c2; c2 = c2->next) {
        if (XML_ELEMENT_NODE != c2->type)
          continue;
        if (!check_element(c2,"sort",XSLT_NAMESPACE) &&
            !check_element(c2,"with-param",XSLT_NAMESPACE))
          return parse_error(c2,"not allowed here");
      }

      new_sn = add_node(&child_ptr,XSLT_INSTR_APPLY_TEMPLATES,filename,c);

      if ((NULL != select) && (NULL == (new_sn->m_select = get_expr(ei,filename,c,"select",select,0))))
        return -1;
      /* FIXME: multiple modes? */
      if ((NULL != mode))
        new_sn->m_mode = QName::parse(mode);

      free(select);
      free(mode);

      CHECK_CALL(parse_sequence_constructors(new_sn,c->children,doc,ei,filename))
    }
    else if (check_element(c,"attribute",XSLT_NAMESPACE)) {
      char *name = XMLGetNsProp(c,"name",String::null());
      /* FIXME: support for "namespace" attribute */
      char *select;
      /* FIXME: support for "separator" attribute */
      /* FIXME: support for "type" attribute */
      /* FIXME: support for "validation" attribute */

      new_sn = add_node(&child_ptr,XSLT_INSTR_ATTRIBUTE,filename,c);

      if (NULL == name)
        return missing_attribute(c,"name");
      new_sn->m_qn = QName::parse(name);
      free(name);


      select = XMLGetNsProp(c,"select",String::null());
      if ((NULL != select) &&
          (NULL == (new_sn->m_select = get_expr(ei,filename,c,"select",select,0)))) {
        free(select);
        return -1;
      }
      free(select);
      CHECK_CALL(parse_sequence_constructors(new_sn,c->children,doc,ei,filename))
    }
    else if (check_element(c,"call-template",XSLT_NAMESPACE)) {
      char *name = XMLGetNsProp(c,"name",String::null());

      new_sn = add_node(&child_ptr,XSLT_INSTR_CALL_TEMPLATE,filename,c);

      if (NULL == name)
        return missing_attribute(c,"name");
      new_sn->m_qn = QName::parse(name);

      /* make sure all children are <xsl:with-param> elements */
      for (c2 = c->children; c2; c2 = c2->next) {
        if (XML_ELEMENT_NODE != c2->type)
          continue;
        if (!check_element(c2,"with-param",XSLT_NAMESPACE))
          return parse_error(c2,"not allowed here");
      }

      CHECK_CALL(parse_sequence_constructors(new_sn,c->children,doc,ei,filename))
    }
    else if (check_element(c,"choose",XSLT_NAMESPACE)) {
      new_sn = add_node(&child_ptr,XSLT_INSTR_CHOOSE,filename,c);
      Statement *otherwise = NULL;
      Statement **child2_ptr = &new_sn->m_child;
      for (c2 = c->children; c2; c2 = c2->next) {
        if (XML_ELEMENT_NODE != c2->type)
          continue;

        if (check_element(c2,"otherwise",XSLT_NAMESPACE)) {
          if (NULL != otherwise)
            return parse_error(c2,"Only one <otherwise> is allowed inside <choose>");
          otherwise = add_node(&child2_ptr,XSLT_OTHERWISE,filename,c2);
          CHECK_CALL(parse_sequence_constructors(otherwise,c2->children,doc,ei,filename))
        }
        else if (check_element(c2,"when",XSLT_NAMESPACE)) {
          char *test = XMLGetNsProp(c2,"test",String::null());
          Statement *when = add_node(&child2_ptr,XSLT_WHEN,filename,c2);
          if (NULL != otherwise)
            return parse_error(c2,"No <when> allowed after an <otherwise>");
          if (NULL == test)
            return missing_attribute(c2,"test");
          if (NULL == (when->m_select = get_expr(ei,filename,c2,"test",test,0)))
            return -1;
          free(test);
          CHECK_CALL(parse_sequence_constructors(when,c2->children,doc,ei,filename))
        }
      }
    }
    else if (check_element(c,"comment",XSLT_NAMESPACE)) {
      char *select = XMLGetNsProp(c,"select",String::null());
      new_sn = add_node(&child_ptr,XSLT_INSTR_COMMENT,filename,c);
      if ((NULL != select) && (NULL == (new_sn->m_select = get_expr(ei,filename,c,"select",select,0))))
        return -1;
      CHECK_CALL(parse_sequence_constructors(new_sn,c->children,doc,ei,filename))
    }
    else if (check_element(c,"copy",XSLT_NAMESPACE)) {
      /* FIXME: support "copy-namespaces" */
      /* FIXME: support "inherit-namespaces" */
      /* FIXME: support "use-attribute-sets" */
      /* FIXME: support "type" */
      /* FIXME: support "validation" */
      new_sn = add_node(&child_ptr,XSLT_INSTR_COPY,filename,c);
      CHECK_CALL(parse_sequence_constructors(new_sn,c->children,doc,ei,filename))
    }
    else if (check_element(c,"copy-of",XSLT_NAMESPACE)) {
      char *select = XMLGetNsProp(c,"select",String::null());
      /* FIXME: support "copy-namespaces" */
      /* FIXME: support "type" */
      /* FIXME: support "validation" */
      new_sn = add_node(&child_ptr,XSLT_INSTR_COPY_OF,filename,c);
      if (NULL == select)
        return missing_attribute(c,"select");
      if (NULL == (new_sn->m_select = get_expr(ei,filename,c,"select",select,0)))
        return -1;
      for (c2 = c->children; c2; c2 = c2->next) {
        if (XML_ELEMENT_NODE == c2->type)
          return parse_error(c2,"not allowed here");
      }
    }
    else if (check_element(c,"element",XSLT_NAMESPACE)) {
      char *name = XMLGetNsProp(c,"name",String::null());
      /* FIXME: support "namespace" */
      /* FIXME: support "inherit-namespace" */
      /* FIXME: support "use-attribute-sets" */
      /* FIXME: support "type" */
      /* FIXME: support "validation" */
      new_sn = add_node(&child_ptr,XSLT_INSTR_ELEMENT,filename,c);
      if (NULL == name)
        return missing_attribute(c,"name");
      new_sn->m_name_expr = get_expr_attr(ei,filename,c,"name",name);
      CHECK_CALL(parse_sequence_constructors(new_sn,c->children,doc,ei,filename))
    }
    else if (check_element(c,"fallback",XSLT_NAMESPACE)) {
      new_sn = add_node(&child_ptr,XSLT_INSTR_FALLBACK,filename,c);
      CHECK_CALL(parse_sequence_constructors(new_sn,c->children,doc,ei,filename))
    }
    else if (check_element(c,"for-each",XSLT_NAMESPACE)) {
      char *select = XMLGetNsProp(c,"select",String::null());
      new_sn = add_node(&child_ptr,XSLT_INSTR_FOR_EACH,filename,c);
      if (NULL == select)
        return missing_attribute(c,"select");
      if (NULL == (new_sn->m_select = get_expr(ei,filename,c,"select",select,0))) {
        free(select);
        return -1;
      }
      free(select);
      CHECK_CALL(parse_sequence_constructors(new_sn,c->children,doc,ei,filename))
    }
    else if (check_element(c,"for-each-group",XSLT_NAMESPACE)) {
      char *select = XMLGetNsProp(c,"select",String::null());
      char *group_by = XMLGetNsProp(c,"group-by",String::null());
      char *group_adjacent = XMLGetNsProp(c,"group-adjacent",String::null());
      char *group_starting_with = XMLGetNsProp(c,"group-starting-with",String::null());
      char *group_ending_with = XMLGetNsProp(c,"group-ending-with",String::null());
      /* FIXME: support "collation" */
      int groupcount = 0;
      new_sn = add_node(&child_ptr,XSLT_INSTR_FOR_EACH_GROUP,filename,c);
      if (NULL == select)
        return missing_attribute(c,"select");
      if (NULL == (new_sn->m_select = get_expr(ei,filename,c,"select",select,0)))
        return -1;
      if (NULL != group_by) {
        groupcount++;
        new_sn->m_gmethod = XSLT_GROUPING_BY;
        new_sn->m_expr1 = get_expr(ei,filename,c,"group-by",group_by,0);
      }
      if (NULL != group_adjacent) {
        groupcount++;
        new_sn->m_gmethod = XSLT_GROUPING_ADJACENT;
        new_sn->m_expr1 = get_expr(ei,filename,c,"group-adjacent",group_adjacent,0);
      }
      if (NULL != group_starting_with) {
        groupcount++;
        new_sn->m_gmethod = XSLT_GROUPING_STARTING_WITH;
        new_sn->m_expr1 = get_expr(ei,filename,c,"group-starting-with",group_starting_with,0);
      }
      if (NULL != group_ending_with) {
        groupcount++;
        new_sn->m_gmethod = XSLT_GROUPING_ENDING_WITH;
        new_sn->m_expr1 = get_expr(ei,filename,c,"group-ending-with",group_ending_with,0);
      }
      if (0 == groupcount)
        return parse_error(c,"No grouping method specified");
      if (NULL == new_sn->m_expr1)
        return -1;
      if (1 < groupcount)
        return parse_error(c,"Only one grouping method can be specified");
      CHECK_CALL(parse_sequence_constructors(new_sn,c->children,doc,ei,filename))
    }
    else if (check_element(c,"if",XSLT_NAMESPACE)) {
      char *test = XMLGetNsProp(c,"test",String::null());
      new_sn = add_node(&child_ptr,XSLT_INSTR_IF,filename,c);
      if (NULL == test)
        return missing_attribute(c,"test");
      if (NULL == (new_sn->m_select = get_expr(ei,filename,c,"test",test,0)))
        return -1;
      CHECK_CALL(parse_sequence_constructors(new_sn,c->children,doc,ei,filename))
    }
    else if (check_element(c,"message",XSLT_NAMESPACE)) {
      char *select = XMLGetNsProp(c,"select",String::null());
      char *terminate = XMLGetNsProp(c,"terminate",String::null());
      new_sn = add_node(&child_ptr,XSLT_INSTR_MESSAGE,filename,c);
      if ((NULL != select) && (NULL == (new_sn->m_select = get_expr(ei,filename,c,"select",select,0))))
        return -1;
      if ((NULL != terminate) && !strcmp(terminate,"yes")) {
        new_sn->m_flags |= FLAG_TERMINATE;
      }
      else if ((NULL != terminate) && strcmp(terminate,"no")) {
        new_sn->m_flags |= FLAG_TERMINATE;
        new_sn->m_expr1 = get_expr_attr(ei,filename,c,"terminate",terminate);
        /* FIXME: handle the case where the attribute specifies another value other than one
           inside {}'s */
      }
      CHECK_CALL(parse_sequence_constructors(new_sn,c->children,doc,ei,filename))
    }
    else if (check_element(c,"namespace",XSLT_NAMESPACE)) {
      char *name;

      /* @implements(xslt20:creating-namespace-nodes-1)
         test { xslt/parse/namespace1.test }
         test { xslt/parse/namespace2.test }
         test { xslt/parse/namespace3.test }
         test { xslt/parse/namespace_badattr.test }
         test { xslt/parse/namespace_noname.test }
         test { xslt/parse/namespace_otherattr.test }
         @end
         @implements(xslt20:creating-namespace-nodes-3)
         status { partial }
         issue { need to test how this evaluates with the empty string (should be default
                 namespace i.e. NULL) }
         @end
         @implements(xslt20:creating-namespace-nodes-7) status { info } @end  */

/*       ALLOWED_ATTRIBUTES(c,"name","select") */
      ALLOWED_ATTRIBUTES(c,&xstr_name,&xstr_select)
      REQUIRED_ATTRIBUTE(c,"name")

      new_sn = add_node(&child_ptr,XSLT_INSTR_NAMESPACE,filename,c);

      name = XMLGetNsProp(c,"name",String::null());
      if (NULL == (new_sn->m_name_expr = get_expr_attr(ei,filename,c,"name",name))) {
        free(name);
        return -1;
      }
      free(name);

      /* @implements(xslt20:creating-namespace-nodes-6)
         status { partial }
         fixme { we need to allow and correctly handle fallback instructions }
         test { xslt/parse/namespace_selectcontent.test }
         @end */
      if (XMLHasNsProp(c,"select",String::null()) && (NULL != xslt_first_child(c))) {
        error(ei,filename,c->line,"XTSE0910","It is a static error if the select attribute of "
              "the xsl:namespace element is present when the element has content other than one or "
              "more xsl:fallback instructions, or if the select attribute is absent when the "
              "element has empty content.");
        return -1;
      }

      if (XMLHasNsProp(c,"select",String::null())) {
        char *select = XMLGetNsProp(c,"select",String::null());
        if (NULL == (new_sn->m_select = get_expr(ei,filename,c,"select",select,0))) {
          free(select);
          return -1;
        }
        free(select);
      }

      CHECK_CALL(parse_sequence_constructors(new_sn,c->children,doc,ei,filename))
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

      CHECK_CALL(parse_sequence_constructors(new_sn,c->children,doc,ei,filename))
    }
    else if (check_element(c,"number",XSLT_NAMESPACE)) {
      /* FIXME */
    }
    else if (check_element(c,"perform-sort",XSLT_NAMESPACE)) {
      char *select = XMLGetNsProp(c,"select",String::null());
      new_sn = add_node(&child_ptr,XSLT_INSTR_PERFORM_SORT,filename,c);
      if ((NULL != select) && (NULL == (new_sn->m_select = get_expr(ei,filename,c,"select",select,0))))
        return -1;
      CHECK_CALL(parse_sequence_constructors(new_sn,c->children,doc,ei,filename))
    }
    else if (check_element(c,"processing-instruction",XSLT_NAMESPACE)) {
      char *name = XMLGetNsProp(c,"name",String::null());
      char *select = XMLGetNsProp(c,"select",String::null());
      new_sn = add_node(&child_ptr,XSLT_INSTR_PROCESSING_INSTRUCTION,filename,c);
      if (NULL == name)
        return missing_attribute(c,"name");
      /* FIXME: if it's just a normal (non-expr) attribute value, set qname instead */
      if ((NULL == (new_sn->m_name_expr = get_expr_attr(ei,filename,c,"name",name))))
        return -1;
      if ((NULL != select) && (NULL == (new_sn->m_select = get_expr(ei,filename,c,"select",select,0))))
        return -1;
      CHECK_CALL(parse_sequence_constructors(new_sn,c->children,doc,ei,filename))
    }
    else if (check_element(c,"result-document",XSLT_NAMESPACE)) {
      /* FIXME */
    }
    else if (check_element(c,"sequence",XSLT_NAMESPACE)) {
      char *select = XMLGetNsProp(c,"select",String::null());
      new_sn = add_node(&child_ptr,XSLT_INSTR_SEQUENCE,filename,c);
      if (NULL == select)
        return missing_attribute(c,"select");
      if (NULL == (new_sn->m_select = get_expr(ei,filename,c,"select",select,0)))
        return -1;
      free(select);
      /* make sure all children are <xsl:fallback> elements */
      for (c2 = c->children; c2; c2 = c2->next) {
        if (XML_ELEMENT_NODE != c2->type)
          continue;
        if (!check_element(c2,"fallback",XSLT_NAMESPACE))
          return parse_error(c2,"not allowed here");
      }
      CHECK_CALL(parse_sequence_constructors(new_sn,c->children,doc,ei,filename))
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


      char *tmp;
      tmp = (char*)malloc(len+1);
      for (c2 = c->children; c2; c2 = c2->next) {
        if ((XML_ELEMENT_NODE == c2->type) ||
            (XML_TEXT_NODE == c2->type)) {
          memcpy(&tmp[pos],c2->content,strlen(c2->content));
          pos += strlen(c2->content);
        }
      }
      tmp[pos] = '\0';
      new_sn->m_strval = tmp;
      free(tmp);
    }
    else if (check_element(c,"value-of",XSLT_NAMESPACE)) {
/*       ALLOWED_ATTRIBUTES(c,"select","separator","disable-output-escaping") */
      ALLOWED_ATTRIBUTES(c,&xstr_select,&xstr_separator,&xstr_disable_output_escaping)
      new_sn = add_node(&child_ptr,XSLT_INSTR_VALUE_OF,filename,c);

      /* 
         @implements(xslt20:value-of-2)
         test { xslt/parse/value-of1.test }
         test { xslt/parse/value-of2.test }
         test { xslt/parse/value-of3.test }
         test { xslt/parse/value-of4.test }
         test { xslt/parse/value-of_badattr.test }
         test { xslt/parse/value-of_otherattr.test }
         @end

         @implements(xslt20:value-of-5)
         test { xslt/parse/value-of_selcont1.test }
         test { xslt/parse/value-of_selcont2.test }
         @end */

      if (XMLHasNsProp(c,"select",String::null()) && (NULL != xslt_first_child(c)))
        return error(ei,filename,c->line,"XTSE0870","select attribute cannot be present when the "
                     "value-of instruction contains child instructions");

      if (!XMLHasNsProp(c,"select",String::null()) && (NULL == xslt_first_child(c)))
        return error(ei,filename,c->line,"XTSE0870","Either a select attribute must be specified, "
                     "or one or more child instructions must be present");

      if (XMLHasNsProp(c,"select",String::null())) {
        char *select = XMLGetNsProp(c,"select",String::null());
        if (NULL == (new_sn->m_select = get_expr(ei,filename,c,"select",select,0))) {
          free(select);
          return -1;
        }
        free(select);
      }

      if (XMLHasNsProp(c,"separator",String::null())) {
        char *select = XMLGetNsProp(c,"separator",String::null());
        if (NULL == (new_sn->m_expr1 = get_expr_attr(ei,filename,c,"separator",select))) {
          free(select);
          return -1;
        }
        free(select);
      }

      CHECK_CALL(parse_sequence_constructors(new_sn,c->children,doc,ei,filename))
    }
    else if (check_element(c,"variable",XSLT_NAMESPACE)) {
      char *name = XMLGetNsProp(c,"name",String::null());
      char *select;
      /* FIXME: attribute "as" for SequenceType */
      new_sn = add_node(&child_ptr,XSLT_VARIABLE,filename,c);
      if (NULL == name)
        return missing_attribute(c,"name");
      new_sn->m_qn = QName::parse(name);
      free(name);

      select = XMLGetNsProp(c,"select",String::null());
      if ((NULL != select) &&
          (NULL == (new_sn->m_select = get_expr(ei,filename,c,"select",select,0)))) {
        free(select);
        return -1;
      }
      free(select);

      CHECK_CALL(parse_sequence_constructors(new_sn,c->children,doc,ei,filename))
    }
    else if (check_element(c,"param",XSLT_NAMESPACE)) {
      assert(!"params should already be handled by the parent function/template/stylesheet");
    }
    else if (check_element(c,"with-param",XSLT_NAMESPACE)) {
      char *name = XMLGetNsProp(c,"name",String::null());
      char *select = XMLGetNsProp(c,"select",String::null());
      /* FIXME: support "as" */
      char *tunnel = XMLGetNsProp(c,"tunnel",String::null());

      if ((XSLT_INSTR_APPLY_TEMPLATES != sn->m_type) &&
          (XSLT_INSTR_APPLY_IMPORTS != sn->m_type) &&
          (XSLT_INSTR_CALL_TEMPLATE != sn->m_type) &&
          (XSLT_INSTR_NEXT_MATCH != sn->m_type))
        return parse_error(c,"not allowed here");

      new_sn = add_node(&param_ptr,XSLT_WITH_PARAM,filename,c);

      if (NULL == name)
        return missing_attribute(c,"name");
      new_sn->m_qn = QName::parse(name);
      if ((NULL != select) && (NULL == (new_sn->m_select = get_expr(ei,filename,c,"select",select,0))))
        return -1;
      if ((NULL != tunnel) && !strcmp(tunnel,"yes"))
        new_sn->m_flags |= FLAG_TUNNEL;
      CHECK_CALL(parse_sequence_constructors(new_sn,c->children,doc,ei,filename))
    }
    else if (check_element(c,"sort",XSLT_NAMESPACE)) {
      char *select = XMLGetNsProp(c,"select",String::null());
      /* FIXME: support "lang" */
      /* FIXME: support "order" */
      /* FIXME: support "collation" */
      /* FIXME: support "stable" */
      /* FIXME: support "case-order" */
      /* FIXME: support "data-type" */
      /* FIXME: i think we are supposed to ensure that the <sort> elements are the first children
         of whatever parent they're in - see for example perform-sort which describves its content
         as being (xsl:sort+, sequence-constructor) */

      if ((XSLT_INSTR_APPLY_TEMPLATES != sn->m_type) &&
          (XSLT_INSTR_FOR_EACH != sn->m_type) &&
          (XSLT_INSTR_FOR_EACH_GROUP != sn->m_type) &&
          (XSLT_INSTR_PERFORM_SORT != sn->m_type))
        return parse_error(c,"not allowed here");

      new_sn = add_node(&sort_ptr,XSLT_SORT,filename,c);

      if ((NULL != select) && (NULL == (new_sn->m_select = get_expr(ei,filename,c,"select",select,0))))
        return -1;
      CHECK_CALL(parse_sequence_constructors(new_sn,c->children,doc,ei,filename))
    }
    else if (c->ns && (c->ns->href == XSLT_NAMESPACE)) {
      return xslt_invalid_element(ei,filename,c);
    }
    else if (XML_ELEMENT_NODE == c->type) {
      xmlAttrPtr attr;
      Statement **child2_ptr;
      /* literal result element */
      new_sn = add_node(&child_ptr,XSLT_INSTR_ELEMENT,filename,c);
      new_sn->m_literal = 1;
      child2_ptr = &new_sn->m_child;
      new_sn->m_includens = 1;

      new_sn->m_qn.m_localPart = c->name;
      /* FIXME: ensure that this prefix is ok to use in the result document... i.e. the
         prefix -> namespace mapping must be established in the result document */
      if (c->ns && c->ns->prefix)
        new_sn->m_qn.m_prefix = c->ns->prefix;

      /* copy attributes */
      for (attr = c->properties; attr; attr = attr->next) {
        xmlNodePtr t;
        Statement *anode;
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
        anode->m_literal = 1;
        anode->m_qn.m_localPart = attr->name;
        if (attr->ns && attr->ns->prefix)
          anode->m_qn.m_prefix = attr->ns->prefix;

        if (NULL == (anode->m_select = get_expr_attr(ei,filename,c,attr->name,val)))
          return -1;
        free(val);
      }

      CHECK_CALL(parse_sequence_constructors(new_sn,c->children,doc,ei,filename))
    }
    else if (XML_TEXT_NODE == c->type) {
      /* @implements(xslt20:literal-text-nodes-1)
         test { xslt/eval/literaltext1.test }
         test { xslt/eval/literaltext2.test }
         test { xslt/eval/literaltext3.test }
         test { xslt/eval/literaltext4.test }
         @end
         @implements(xslt20:literal-text-nodes-2) @end */
      new_sn = add_node(&child_ptr,XSLT_INSTR_TEXT,filename,c);
      new_sn->m_literal = 1;
      new_sn->m_strval = c->content;
    }
  }

  return 0;
}

static int parse_import_include(Statement *parent, xmlNodePtr n, xmlDocPtr doc, Error *ei,
                                const char *filename, int type, Statement ***child_ptr)
{
  char *href;
  Statement *import = add_node(child_ptr,type,filename,n);
  Statement *transform = new Statement(XSLT_TRANSFORM);
  xmlNodePtr c;
  Statement *p;
  char *full_uri;
  import->m_child = transform;

  transform->m_parent = import;
  import->m_parent = parent;

  /* FIXME: test with fragments (including another stylsheet in the same document but with
     a different id) */

/*

  @implements(xslt20:locating-modules-1) status { info } @end
  @implements(xslt20:locating-modules-2) status { info } @end
  @implements(xslt20:locating-modules-3)
  test { xslt/parse/import_fragments.test }
  test { xslt/parse/import_samedoc1.test }
  test { xslt/parse/import_samedoc2.test }
  test { xslt/parse/include_fragments.test }
  test { xslt/parse/include_samedoc1.test }
  test { xslt/parse/include_samedoc2.test }
  @end
  @implements(xslt20:locating-modules-5) status { info } @end


  @implements(xslt20:include-1)
  test { xslt/parse/include_badattr.test }
  test { xslt/parse/include_otherattr.test }
  test { xslt/parse/include_badchildren.test }
  @end
  @implements(xslt20:import-1)
  test { xslt/parse/import_badattr.test }
  test { xslt/parse/import_otherattr.test }
  test { xslt/parse/import_badchildren.test }
  @end

  @implements(xslt20:include-2)
  test { xslt/parse/include1.test }
  test { xslt/parse/include2.test }
  @end
  @implements(xslt20:import-2)
  test { xslt/parse/import.test }
  @end


  @implements(xslt20:include-5) status { info } @end
  @implements(xslt20:include-6) status { info } @end

  @implements(xslt20:import-6) status { info } @end
  @implements(xslt20:import-7) status { info } @end
  @implements(xslt20:import-8) status { info } @end
  @implements(xslt20:import-9) status { info } @end
  @implements(xslt20:import-10) status { info } @end
  @implements(xslt20:import-11) status { info } @end
  @implements(xslt20:import-12) status { info } @end
  @implements(xslt20:import-13) status { info } @end
  @implements(xslt20:import-14) status { info } @end

*/

  assert(XSLT_TRANSFORM == parent->m_type);
  assert(NULL != parent->m_uri);

/*   ALLOWED_ATTRIBUTES(n,"href") */
  ALLOWED_ATTRIBUTES(n,&xstr_href)

  /* @implements(xslt20:include-3)
     test { xslt/parse/include_nohref.test }
     @end
     @implements(xslt20:import-3)
     test { xslt/parse/import_nohref.test }
     @end */
  REQUIRED_ATTRIBUTE(n,"href")

  href = XMLGetNsProp(n,"href",String::null());

  /* @implements(xslt20:locating-modules-6)
     test { xslt/parse/import_baduri.test }
     test { xslt/parse/include_baduri.test }
     test { xslt/parse/import_badmethod.test }
     test { xslt/parse/include_badmethod.test }
     test { xslt/parse/import_nonexistant.test }
     test { xslt/parse/include_nonexistant.test }
     @end */
  if (NULL == (full_uri = xmlBuildURI(href,parent->m_uri))) {
    error(ei,filename,n->line,"XTSE0165","\"%s\" is not a valid relative or absolute URI",href);
    free(href);
    return -1;
  }
  free(href);

  for (p = transform->m_parent; p; p = p->m_parent) {
    if ((p->m_type == XSLT_TRANSFORM) && !strcmp(p->m_uri,full_uri)) {
      /* @implements(xslt20:import-15)
         test { xslt/parse/import_recursive1.test }
         test { xslt/parse/import_recursive2.test }
         test { xslt/parse/import_recursive3.test }
         @end */
      if (XSLT_IMPORT == type)
        error(ei,filename,n->line,"XTSE0210","It is a static error if a stylesheet module directly "
              "or indirectly imports itself");
      /* @implements(xslt20:include-7)
         test { xslt/parse/include_recursive1.test }
         test { xslt/parse/include_recursive2.test }
         test { xslt/parse/include_recursive3.test }
         @end */
      else
        error(ei,filename,n->line,"XTSE0180","It is a static error if a stylesheet module directly "
              "or indirectly includes itself");
      free(full_uri);
      return -1;
    }
  }

  if (0 != parse_xslt_uri(ei,filename,n->line,"XTSE0165",full_uri,transform,parent->m_uri)) {
    free(full_uri);
    return -1;
  }
  free(full_uri);

  if (NULL != (c = xslt_first_child(n)))
    return xslt_invalid_element(ei,filename,c);

  return 0;
}

list *parse_string_list(const char *input)
{
  list *strings = NULL;

  const char *start = input;
  const char *c = input;
  while (1) {
    if ('\0' == *c || isspace(*c)) {
      if (c != start) {
        char *str = (char*)malloc(c-start+1);
        memcpy(str,start,c-start);
        str[c-start] = '\0';
        list_append(&strings,str);
      }
      start = c+1;
    }
    if ('\0' == *c)
      break;
    c++;
  }

  return strings;
}

QNameTest *QNameTest_parse(const char *unparsed)
{
  char *str = strdup(unparsed);
  char *prefix;
  char *localpart;
  char *colon = strchr(str,':');
  QNameTest *test = new QNameTest();
  if (NULL == colon) {
    prefix = NULL;
    localpart = str;
  }
  else {
    *colon = '\0';
    prefix = str;
    localpart = colon+1;
  }

  if (NULL != prefix) {
    if (!strcmp(prefix,"*"))
      test->wcprefix = 1;
    else
      test->qn.m_prefix = prefix;
  }

  if (!strcmp(localpart,"*")) {
    if (NULL == prefix)
      test->wcprefix = 1;
    test->wcname = 1;
  }
  else {
    test->qn.m_localPart = localpart;
  }

  free(str);
  return test;
}

static int parse_strip_preserve(Statement *parent, xmlNodePtr n, xmlDocPtr doc, Error *ei,
                                const char *filename, int type, Statement ***child_ptr)
{
  xmlNodePtr c;
  Statement *space = add_node(child_ptr,type,filename,n);
  char *elements;
  list *unptests;
  list *l;

/*   ALLOWED_ATTRIBUTES(n,"elements") */
  ALLOWED_ATTRIBUTES(n,&xstr_elements)
  REQUIRED_ATTRIBUTE(n,"elements")

  elements = get_wscollapsed_attr(n,"elements",String::null());
  unptests = parse_string_list(elements);
  free(elements);

  for (l = unptests; l; l = l->next) {
    char *unpt = (char*)l->data;
    QNameTest *qt = QNameTest_parse(unpt);
    space->m_QNameTests.append(qt);
  }

  list_free(unptests,(list_d_t)free);

  if (NULL != (c = xslt_first_child(n)))
    return xslt_invalid_element(ei,filename,c);

  return 0;
}

static int parse_decls(Statement *sn, xmlNodePtr n, xmlDocPtr doc,
                       Error *ei, const char *filename)
{
  xmlNodePtr c = xslt_first_child(n);
  Statement *new_sn;
  Statement **child_ptr = &sn->m_child;

  /* @implements(xslt20:stylesheet-element-14) status { info } @end */
  /* @implements(xslt20:stylesheet-element-15)
     test { xslt/parse/template_badattr.test }
     @end */

  /* FIXME: parse imports here; these come before all other top-level elements */
  while (c && check_element(c,"import",XSLT_NAMESPACE)) {
    CHECK_CALL(parse_import_include(sn,c,doc,ei,filename,XSLT_IMPORT,&child_ptr))
    xslt_next_element(&c);
  }

  for (; c; xslt_next_element(&c)) {
    if (check_element(c,"include",XSLT_NAMESPACE)) {
      CHECK_CALL(parse_import_include(sn,c,doc,ei,filename,XSLT_DECL_INCLUDE,&child_ptr))
    }
    else if (check_element(c,"attribute-set",XSLT_NAMESPACE)) {
      /* FIXME */
    }
    else if (check_element(c,"character-map",XSLT_NAMESPACE)) {
      /* FIXME */
    }
    else if (check_element(c,"decimal-format",XSLT_NAMESPACE)) {
      /* FIXME */
    }
    else if (check_element(c,"function",XSLT_NAMESPACE)) {
      xmlNodePtr c2;

      /* @implements(xslt20:stylesheet-functions-1) status { info } @end */
      /* @implements(xslt20:stylesheet-functions-6) status { info } @end */
      /* @implements(xslt20:stylesheet-functions-20) status { info } @end */
      /* @implements(xslt20:stylesheet-functions-26) status { info } @end */
      /* @implements(xslt20:stylesheet-functions-27) status { info } @end */
      /* @implements(xslt20:stylesheet-functions-28) status { info } @end */

      /* @implements(xslt20:stylesheet-functions-2)
         status { partial }
         issue { need to test invalid child sequences }
         test { xslt/parse/function_badattr.test }
         test { xslt/parse/function_otherattr.test }
         @end */
/*       ALLOWED_ATTRIBUTES(c,"name","as","override") */
      ALLOWED_ATTRIBUTES(c,&xstr_name,&xstr_as,&xstr_override)

      /* @implements(xslt20:stylesheet-functions-3)
         test { xslt/parse/function_name1.test }
         test { xslt/parse/function_name3.test }
         test { xslt/parse/function_noname.test }
         @end */
      REQUIRED_ATTRIBUTE(c,"name")

      new_sn = add_node(&child_ptr,XSLT_DECL_FUNCTION,filename,c);
      new_sn->m_qn = xml_attr_qname(c,"name");

      /* @implements(xslt20:stylesheet-functions-5)
         test { xslt/parse/function_name2.test }
         @end */
      if (new_sn->m_qn.m_prefix.isNull())
        return error(ei,filename,c->line,"XTSE0740",
                     "A stylesheet function MUST have a prefixed name, to remove any "
                     "risk of a clash with a function in the default function namespace");

      /* @implements(xslt20:stylesheet-functions-17)
         test { function_as1.test }
         test { function_as2.test }
         test { function_as3.test }
         @end */
      CHECK_CALL(parse_optional_as(c,new_sn,ei,filename))

      /* @implements(xslt20:stylesheet-functions-12)
         status { partial }
         test { xslt/parse/function_override1.test }
         test { xslt/parse/function_override2.test }
         test { xslt/parse/function_override3.test }
         @end */
      new_sn->m_flags |= FLAG_OVERRIDE;
      if (XMLHasNsProp(c,"override",String::null())) {
        if (!xml_attr_strcmp(c,"override","no"))
          new_sn->m_flags &= ~FLAG_OVERRIDE;
        else if (xml_attr_strcmp(c,"override","yes"))
          return invalid_attribute_val(ei,filename,c,"override");
      }

      /* @implements(xslt20:stylesheet-functions-7)
         status { partial }
         issue { need tests to ensure content model is enforced }
         @end */
      c2 = xslt_first_child(c);
      while (c2 && check_element(c2,"param",XSLT_NAMESPACE)) {
        CHECK_CALL(parse_param(new_sn,c2,doc,ei,filename))
        xslt_next_element(&c2);
      }

      CHECK_CALL(parse_sequence_constructors(new_sn,c2,doc,ei,filename))
    }
    else if (check_element(c,"import-schema",XSLT_NAMESPACE)) {
      /* FIXME */
    }
    else if (check_element(c,"key",XSLT_NAMESPACE)) {
      /* FIXME */
    }
    else if (check_element(c,"namespace-alias",XSLT_NAMESPACE)) {
      /* FIXME */
    }
    else if (check_element(c,"output",XSLT_NAMESPACE)) {
      int i;
      xmlNodePtr c2;

      /* @implements(xslt20:serialization-3)
         test { xslt/parse/output1.test }
         test { xslt/parse/output2.test }
         test { xslt/parse/output3.test }
         test { xslt/parse/output4.test }
         test { xslt/parse/output_badattr.test }
         test { xslt/parse/output_badchildren.test }
         test { xslt/parse/output_otherattr.test }
         @end */

/*       ALLOWED_ATTRIBUTES(c, */
/*                          "name", */
/*                          "method", */
/*                          "byte-order-mark", */
/*                          "cdata-section-elements", */
/*                          "doctype-public", */
/*                          "doctype-system", */
/*                          "encoding", */
/*                          "escape-uri-attributes", */
/*                          "include-content-type", */
/*                          "indent", */
/*                          "media-type", */
/*                          "normalization-form", */
/*                          "omit-xml-declaration", */
/*                          "standalone", */
/*                          "undeclare-prefixes", */
/*                          "use-character-maps", */
/*                          "version") */
      ALLOWED_ATTRIBUTES(c,
                         &xstr_name,
                         &xstr_method,
                         &xstr_byte_order_mark,
                         &xstr_cdata_section_elements,
                         &xstr_doctype_public,
                         &xstr_doctype_system,
                         &xstr_encoding,
                         &xstr_escape_uri_attributes,
                         &xstr_include_content_type,
                         &xstr_indent,
                         &xstr_media_type,
                         &xstr_normalization_form,
                         &xstr_omit_xml_declaration,
                         &xstr_standalone,
                         &xstr_undeclare_prefixes,
                         &xstr_use_character_maps,
                         &xstr_version)

      new_sn = add_node(&child_ptr,XSLT_DECL_OUTPUT,filename,c);
      if (XMLHasNsProp(c,"name",String::null()))
        new_sn->m_qn = xml_attr_qname(c,"name");

      new_sn->m_seroptions = (char**)calloc(SEROPTION_COUNT,sizeof(char*));
      for (i = 0; i < SEROPTION_COUNT; i++)
        if (XMLHasNsProp(c,df_seroptions::seroption_names[i],String::null()))
          new_sn->m_seroptions[i] = get_wscollapsed_attr(c,df_seroptions::seroption_names[i],
                                                         String::null());

      if (NULL != (c2 = xslt_first_child(c)))
        return xslt_invalid_element(ei,filename,c2);
    }
    else if (check_element(c,"param",XSLT_NAMESPACE)) {
      /* FIXME */
    }
    else if (check_element(c,"preserve-space",XSLT_NAMESPACE)) {
      CHECK_CALL(parse_strip_preserve(sn,c,doc,ei,filename,XSLT_DECL_PRESERVE_SPACE,&child_ptr))
    }
    else if (check_element(c,"strip-space",XSLT_NAMESPACE)) {
      CHECK_CALL(parse_strip_preserve(sn,c,doc,ei,filename,XSLT_DECL_STRIP_SPACE,&child_ptr))
    }
    else if (check_element(c,"template",XSLT_NAMESPACE)) {
      /* FIXME: support "priority" */
      /* FIXME: support "mode" */
      /* FIXME: support "as" */

      /* FIXME: ensure params get processed */

      /* @implements(xslt20:rules-1) status { info } @end */

      /* @implements(xslt20:defining-templates-1)
         status { partial }
         issue { need to test invalid child sequences }
         test { xslt/parse/template_badattr.test }
         test { xslt/parse/function_otherattr.test }
         @end */
/*       ALLOWED_ATTRIBUTES(c,"match","name","priority","mode","as") */
      ALLOWED_ATTRIBUTES(c,&xstr_match,&xstr_name,&xstr_priority,&xstr_mode,&xstr_as)

      /* @implements(xslt20:defining-templates-2) status { info } @end */

      /* @implements(xslt20:defining-templates-3)
         test { xslt/parse/template_nomatchname.test }
         test { xslt/parse/template_matchmode.test }
         test { xslt/parse/template_matchpriority.test }
         @end */
      if (!XMLHasNsProp(c,"match",String::null()) && !XMLHasNsProp(c,"name",String::null()))
          return error(ei,filename,c->line,"XTSE0500",
                       "\"name\" and/or \"match\" attribute required");

      if (!XMLHasNsProp(c,"match",String::null()) && XMLHasNsProp(c,"mode",String::null()))
        return conflicting_attributes(ei,filename,c->line,"XTSE0500","mode","name");
      if (!XMLHasNsProp(c,"match",String::null()) && XMLHasNsProp(c,"priority",String::null()))
        return conflicting_attributes(ei,filename,c->line,"XTSE0500","priority","name");

      /* @implements(xslt20:defining-templates-4)
         status { info }
         @end */

      new_sn = add_node(&child_ptr,XSLT_DECL_TEMPLATE,filename,c);

      if (XMLHasNsProp(c,"name",String::null())) {
        char *name = XMLGetNsProp(c,"name",String::null());
        new_sn->m_qn = QName::parse(name);
        free(name);
      }
      if (XMLHasNsProp(c,"match",String::null())) {
        char *match = XMLGetNsProp(c,"match",String::null());
        if (match && (NULL == (new_sn->m_select = get_expr(ei,filename,c,"match",match,1)))) {
          free(match);
          return -1;
        }
        free(match);
      }
      CHECK_CALL(parse_sequence_constructors(new_sn,c->children,doc,ei,filename))
    }
    else if (check_element(c,"variable",XSLT_NAMESPACE)) {
      /* FIXME */
    }
    else if (XML_ELEMENT_NODE == c->type) {
      xmlNodePtr post = c;

      /* @implements(xslt20:import-5)
         test { xslt/parse/import_notatstart.test }
         @end */
      if (check_element(c,"import",XSLT_NAMESPACE))
        return error(ei,filename,n->line,"XTSE0200","The xsl:import element children must precede "
                     "all other element children of an xsl:stylesheet element, including any "
                     "xsl:include element children and any user-defined data elements.");


      /* @implements(xslt20:user-defined-top-level-2)
         test { xslt/parse/transform_userdatanons.test }
         @end */
      if (NULL == c->ns) {
        return error(ei,filename,n->line,"XTSE0130","User-defined data elements only permitted if "
                     "they have a non-null namespace URI");
      }
      else if (c->ns->href == XSLT_NAMESPACE)
        return xslt_invalid_element(ei,filename,c);

      /* @implements(xslt20:user-defined-top-level-1)
         test { xslt/parse/transform_userdata.test }
         @end */
      /* otherwise; it's a valid user-define data element; just ignore it */

      /* @implements(xslt20:user-defined-top-level-3) status { info } @end */
      /* @implements(xslt20:user-defined-top-level-4) status { info } @end */
      /* @implements(xslt20:user-defined-top-level-5) status { info } @end */

      /* @implements(xslt20:user-defined-top-level-6)
         test { xslt/parse/transform_userdataimport.test }
         @end */
      xslt_next_element(&post);
      while (post) {
        if (check_element(post,"import",XSLT_NAMESPACE))
          return error(ei,filename,n->line,"XTSE0140",
                       "A user-defined data element must not precede an import element");
        xslt_next_element(&post);
      }

    }
    /* FIXME: check for other stuff that can appear here like <variable> and <param> */
    /* FIXME: support "user-defined data elements"; I think we also need to make these
       accessible to code within the stylesheet (i.e. put them in a DOM tree) */
  }

  return 0;
}

static int parse_xslt_module(Statement *sroot, xmlNodePtr n, xmlDocPtr doc,
                             Error *ei, const char *filename)
{
  sroot->m_uri = strdup(filename);

  /* @implements(xslt20:stylesheet-element-3) @end */

  /* @implements(xslt20:stylesheet-element-4)
     test { xslt/parse/transform_noversion.test }
     @end */
  REQUIRED_ATTRIBUTE(n,"version")

  /* @implements(xslt20:stylesheet-element-5) status { deferred } @end */
  /* @implements(xslt20:stylesheet-element-6) status { deferred } @end */
  /* @implements(xslt20:stylesheet-element-7) status { deferred } @end */
  /* @implements(xslt20:stylesheet-element-8) status { deferred } @end */
  /* @implements(xslt20:stylesheet-element-9) status { deferred } @end */
  /* @implements(xslt20:stylesheet-element-10) status { deferred } @end */
  /* @implements(xslt20:stylesheet-element-11) status { deferred } @end */
  /* @implements(xslt20:stylesheet-element-12) status { info } @end */
  /* @implements(xslt20:stylesheet-element-13) status { info } @end */
  /* @implements(xslt20:stylesheet-element-16) status { info } @end */
  /* @implements(xslt20:stylesheet-element-17) status { info } @end */

  /* @implements(xslt20:default-collation-attribute-1) status { deferred } @end */
  /* @implements(xslt20:default-collation-attribute-2) status { deferred } @end */
  /* @implements(xslt20:default-collation-attribute-3) status { deferred } @end */
  /* @implements(xslt20:default-collation-attribute-4) status { deferred } @end */
  /* @implements(xslt20:default-collation-attribute-5) status { deferred } @end */
  /* @implements(xslt20:default-collation-attribute-6) status { deferred } @end */

  CHECK_CALL(parse_decls(sroot,n,doc,ei,filename))
  add_ns_defs(sroot,n);
  return 0;
}

int parse_xslt_relative_uri(Error *ei, const char *filename, int line, const char *errname,
                            const char *base_uri, const char *uri, Statement *sroot)
{
  char *full_uri;

  if (NULL == (full_uri = xmlBuildURI(uri,base_uri))) {
    return error(ei,filename,line,errname,"\"%s\" is not a valid relative or absolute URI",uri);
  }

  if (0 != parse_xslt_uri(ei,filename,line,errname,full_uri,sroot,base_uri)) {
    free(full_uri);
    return -1;
  }
  free(full_uri);

  return 0;
}

static int parse_xslt_uri(Error *ei, const char *filename, int line, const char *errname,
                          const char *full_uri, Statement *sroot, const char *refsource)
{
  xmlDocPtr doc;
  xmlNodePtr node;

  CHECK_CALL(retrieve_uri_element(ei,filename,line,errname,full_uri,&doc,&node,refsource))

  /* FIXME: check version */
  /* FIXME: support simplified stylsheet modules */
  if (!check_element(node,"transform",XSLT_NAMESPACE) &&
      !check_element(node,"stylesheet",XSLT_NAMESPACE)) {
    error(ei,filename,line,errname,"Expected element {%*}%s or {%*}%s",
          &XS_NAMESPACE,"transform",&XS_NAMESPACE,"schema");
    xmlFreeDoc(doc);
    return -1;
  }

  if (0 != parse_xslt_module(sroot,node,doc,ei,full_uri)) {
    xmlFreeDoc(doc);
    return -1;
  }

  xmlFreeDoc(doc);
  return 0;
}

/*
  @implements(xslt20:built-in-types-1)
  test { xslt/parse/builtintypes.test }
  @end
  @implements(xslt20:built-in-types-2) @end
  @implements(xslt20:built-in-types-3) @end
  @implements(xslt20:built-in-types-4) @end
  @implements(xslt20:built-in-types-5) @end
  @implements(xslt20:built-in-types-6) @end
  @implements(xslt20:built-in-types-7) @end

  @implements(xslt20:what-is-xslt-1) status { info } @end
  @implements(xslt20:what-is-xslt-2) status { info } @end
  @implements(xslt20:what-is-xslt-3) status { info } @end
  @implements(xslt20:what-is-xslt-4) status { info } @end
  @implements(xslt20:what-is-xslt-5) status { info } @end
  @implements(xslt20:what-is-xslt-6) status { info } @end
  @implements(xslt20:whats-new-in-xslt2-1) status { info } @end

  @implements(xslt20:terminology-1) status { info } @end
  @implements(xslt20:terminology-2) status { info } @end
  @implements(xslt20:terminology-3) status { info } @end
  @implements(xslt20:terminology-4) status { info } @end
  @implements(xslt20:terminology-5) status { info } @end
  @implements(xslt20:terminology-6) status { info } @end
  @implements(xslt20:terminology-7) status { info } @end
  @implements(xslt20:terminology-8) status { info } @end
  @implements(xslt20:terminology-9) status { info } @end
  @implements(xslt20:terminology-10) status { info } @end
  @implements(xslt20:terminology-11) status { info } @end
  @implements(xslt20:terminology-12) status { info } @end
  @implements(xslt20:terminology-13) status { info } @end
  @implements(xslt20:terminology-14) status { info } @end
  @implements(xslt20:terminology-15) status { info } @end
  @implements(xslt20:terminology-16) status { info } @end
  @implements(xslt20:terminology-17) status { info } @end

  @implements(xslt20:notation-1) status { info } @end
  @implements(xslt20:notation-2) status { info } @end
  @implements(xslt20:notation-3) status { info } @end
  @implements(xslt20:notation-7) status { info } @end
  @implements(xslt20:notation-8) status { info } @end
  @implements(xslt20:notation-9) status { info } @end
  @implements(xslt20:notation-10) status { info } @end

  @implements(xslt20:xslt-namespace-1) status { info } @end
  @implements(xslt20:xslt-namespace-2) status { info } @end
  @implements(xslt20:xslt-namespace-3) status { info } @end
  @implements(xslt20:xslt-namespace-4) status { info } @end
  @implements(xslt20:xslt-namespace-5) status { info } @end
  @implements(xslt20:xslt-namespace-6) status { info } @end

  @implements(xslt20:reserved-namespaces-1) status { info } @end
  @implements(xslt20:reserved-namespaces-2) status { info } @end
  @implements(xslt20:reserved-namespaces-4) status { info } @end

  @implements(xslt20:extension-attributes-1) status { info } @end
  @implements(xslt20:extension-attributes-2) status { info } @end
  @implements(xslt20:extension-attributes-3) status { info } @end

  @implements(xslt20:xslt-media-type-1) status { info } @end
  @implements(xslt20:xslt-media-type-2) status { info } @end
  @implements(xslt20:xslt-media-type-3) status { info } @end

  @implements(xslt20:backwards-1) status { deferred } @end
  @implements(xslt20:backwards-2) status { deferred } @end
  @implements(xslt20:backwards-3) status { deferred } @end
  @implements(xslt20:backwards-4) status { deferred } @end
  @implements(xslt20:backwards-5) status { deferred } @end
  @implements(xslt20:backwards-6) status { deferred } @end
  @implements(xslt20:backwards-7) status { deferred } @end
  @implements(xslt20:backwards-8) status { deferred } @end
  @implements(xslt20:backwards-9) status { deferred } @end
  @implements(xslt20:forwards-1) status { deferred } @end
  @implements(xslt20:forwards-2) status { deferred } @end
  @implements(xslt20:forwards-3) status { deferred } @end
  @implements(xslt20:forwards-4) status { deferred } @end
  @implements(xslt20:forwards-5) status { deferred } @end
  @implements(xslt20:forwards-6) status { deferred } @end
  @implements(xslt20:forwards-7) status { deferred } @end
  @implements(xslt20:forwards-8) status { deferred } @end
  @implements(xslt20:forwards-9) status { deferred } @end

  @implements(xslt20:combining-modules-1) status { info } @end
  @implements(xslt20:combining-modules-2) status { info } @end

  @implements(xslt20:conditional-inclusion-1) status { deferred } @end
  @implements(xslt20:conditional-inclusion-2) status { deferred } @end
  @implements(xslt20:conditional-inclusion-3) status { deferred } @end
  @implements(xslt20:conditional-inclusion-4) status { deferred } @end
  @implements(xslt20:conditional-inclusion-5) status { deferred } @end
  @implements(xslt20:conditional-inclusion-6) status { deferred } @end
  @implements(xslt20:conditional-inclusion-7) status { deferred } @end
  @implements(xslt20:conditional-inclusion-8) status { deferred } @end
  @implements(xslt20:conditional-inclusion-9) status { deferred } @end
  @implements(xslt20:conditional-inclusion-10) status { deferred } @end
  @implements(xslt20:conditional-inclusion-11) status { deferred } @end
  @implements(xslt20:conditional-inclusion-12) status { deferred } @end
  @implements(xslt20:conditional-inclusion-13) status { deferred } @end
  @implements(xslt20:conditional-inclusion-14) status { deferred } @end
  @implements(xslt20:conditional-inclusion-15) status { deferred } @end

  @implements(xslt20:import-schema-1) status { deferred } @end
  @implements(xslt20:import-schema-2) status { deferred } @end
  @implements(xslt20:import-schema-3) status { deferred } @end
  @implements(xslt20:import-schema-4) status { deferred } @end
  @implements(xslt20:import-schema-5) status { deferred } @end
  @implements(xslt20:import-schema-6) status { deferred } @end
  @implements(xslt20:import-schema-7) status { deferred } @end
  @implements(xslt20:import-schema-8) status { deferred } @end
  @implements(xslt20:import-schema-9) status { deferred } @end
  @implements(xslt20:import-schema-10) status { deferred } @end
  @implements(xslt20:import-schema-11) status { deferred } @end
  @implements(xslt20:import-schema-12) status { deferred } @end
  @implements(xslt20:import-schema-13) status { deferred } @end
  @implements(xslt20:import-schema-14) status { deferred } @end
  @implements(xslt20:import-schema-15) status { deferred } @end

  @implements(xslt20:data-model-1) status { info } @end
  @implements(xslt20:data-model-2) status { info } @end
  @implements(xslt20:data-model-3) status { info } @end
  @implements(xslt20:data-model-4) status { info } @end
  @implements(xslt20:data-model-5) status { info } @end

  @implements(xslt20:character-maps-1) status { deferred } @end
  @implements(xslt20:character-maps-2) status { deferred } @end
  @implements(xslt20:character-maps-3) status { deferred } @end
  @implements(xslt20:character-maps-4) status { deferred } @end
  @implements(xslt20:character-maps-5) status { deferred } @end
  @implements(xslt20:character-maps-6) status { deferred } @end
  @implements(xslt20:character-maps-7) status { deferred } @end
  @implements(xslt20:character-maps-8) status { deferred } @end
  @implements(xslt20:character-maps-9) status { deferred } @end
  @implements(xslt20:character-maps-10) status { deferred } @end
  @implements(xslt20:character-maps-11) status { deferred } @end
  @implements(xslt20:character-maps-12) status { deferred } @end
  @implements(xslt20:character-maps-13) status { deferred } @end
  @implements(xslt20:character-maps-14) status { deferred } @end
  @implements(xslt20:character-maps-15) status { deferred } @end
  @implements(xslt20:character-maps-16) status { deferred } @end
  @implements(xslt20:character-maps-17) status { deferred } @end
  @implements(xslt20:character-maps-18) status { deferred } @end
  @implements(xslt20:character-maps-19) status { deferred } @end
  @implements(xslt20:character-maps-20) status { deferred } @end
  @implements(xslt20:character-maps-21) status { deferred } @end

  @implements(xslt20:disable-output-escaping-1) status { deferred } @end
  @implements(xslt20:disable-output-escaping-2) status { deferred } @end
  @implements(xslt20:disable-output-escaping-3) status { deferred } @end
  @implements(xslt20:disable-output-escaping-4) status { deferred } @end
  @implements(xslt20:disable-output-escaping-5) status { deferred } @end
  @implements(xslt20:disable-output-escaping-6) status { deferred } @end
  @implements(xslt20:disable-output-escaping-7) status { deferred } @end
  @implements(xslt20:disable-output-escaping-8) status { deferred } @end
  @implements(xslt20:disable-output-escaping-9) status { deferred } @end
  @implements(xslt20:disable-output-escaping-10) status { deferred } @end
  @implements(xslt20:disable-output-escaping-11) status { deferred } @end
  @implements(xslt20:disable-output-escaping-12) status { deferred } @end
  @implements(xslt20:disable-output-escaping-13) status { deferred } @end
  @implements(xslt20:disable-output-escaping-14) status { deferred } @end
  @implements(xslt20:disable-output-escaping-15) status { deferred } @end
  @implements(xslt20:disable-output-escaping-16) status { deferred } @end

  @implements(xslt20:xml-versions-1) status { deferred } @end
  @implements(xslt20:xml-versions-2) status { deferred } @end
  @implements(xslt20:xml-versions-3) status { deferred } @end
  @implements(xslt20:xml-versions-4) status { deferred } @end
  @implements(xslt20:xml-versions-5) status { deferred } @end
  @implements(xslt20:stylesheet-stripping-1) status { deferred } @end
  @implements(xslt20:stylesheet-stripping-2) status { deferred } @end
  @implements(xslt20:stylesheet-stripping-3) status { deferred } @end
  @implements(xslt20:stylesheet-stripping-4) status { deferred } @end
  @implements(xslt20:stripping-annotations-1) status { deferred } @end
  @implements(xslt20:stripping-annotations-2) status { deferred } @end
  @implements(xslt20:stripping-annotations-3) status { deferred } @end
  @implements(xslt20:stripping-annotations-4) status { deferred } @end
  @implements(xslt20:stripping-annotations-5) status { deferred } @end
  @implements(xslt20:stripping-annotations-6) status { deferred } @end
  @implements(xslt20:strip-1) status { deferred } @end
  @implements(xslt20:strip-2) status { deferred } @end
  @implements(xslt20:strip-3) status { deferred } @end
  @implements(xslt20:strip-4) status { deferred } @end
  @implements(xslt20:strip-5) status { deferred } @end
  @implements(xslt20:strip-6) status { deferred } @end
  @implements(xslt20:strip-7) status { deferred } @end
  @implements(xslt20:strip-8) status { deferred } @end
  @implements(xslt20:strip-9) status { deferred } @end
  @implements(xslt20:strip-10) status { deferred } @end
  @implements(xslt20:strip-11) status { deferred } @end
  @implements(xslt20:strip-12) status { deferred } @end
  @implements(xslt20:strip-13) status { deferred } @end
  @implements(xslt20:strip-14) status { deferred } @end
  @implements(xslt20:strip-15) status { deferred } @end
  @implements(xslt20:strip-16) status { deferred } @end
  @implements(xslt20:id-in-data-model-1) status { deferred } @end
  @implements(xslt20:id-in-data-model-2) status { deferred } @end
  @implements(xslt20:d-o-e-in-data-model-1) status { deferred } @end
  @implements(xslt20:d-o-e-in-data-model-2) status { deferred } @end
  @implements(xslt20:d-o-e-in-data-model-3) status { deferred } @end
  @implements(xslt20:d-o-e-in-data-model-4) status { deferred } @end

  @implements(xslt20:number-1) status { deferred } @end
  @implements(xslt20:number-2) status { deferred } @end
  @implements(xslt20:number-3) status { deferred } @end
  @implements(xslt20:number-4) status { deferred } @end
  @implements(xslt20:number-5) status { deferred } @end
  @implements(xslt20:formatting-supplied-number-1) status { deferred } @end
  @implements(xslt20:formatting-supplied-number-2) status { deferred } @end
  @implements(xslt20:formatting-supplied-number-3) status { deferred } @end
  @implements(xslt20:formatting-supplied-number-4) status { deferred } @end
  @implements(xslt20:formatting-supplied-number-5) status { deferred } @end
  @implements(xslt20:formatting-supplied-number-6) status { deferred } @end
  @implements(xslt20:formatting-supplied-number-7) status { deferred } @end
  @implements(xslt20:numbering-based-on-position-1) status { deferred } @end
  @implements(xslt20:numbering-based-on-position-2) status { deferred } @end
  @implements(xslt20:numbering-based-on-position-3) status { deferred } @end
  @implements(xslt20:numbering-based-on-position-4) status { deferred } @end
  @implements(xslt20:numbering-based-on-position-5) status { deferred } @end
  @implements(xslt20:numbering-based-on-position-6) status { deferred } @end
  @implements(xslt20:numbering-based-on-position-7) status { deferred } @end
  @implements(xslt20:numbering-based-on-position-8) status { deferred } @end
  @implements(xslt20:numbering-based-on-position-9) status { deferred } @end
  @implements(xslt20:numbering-based-on-position-10) status { deferred } @end
  @implements(xslt20:numbering-based-on-position-11) status { deferred } @end
  @implements(xslt20:numbering-based-on-position-12) status { deferred } @end
  @implements(xslt20:numbering-based-on-position-13) status { deferred } @end
  @implements(xslt20:numbering-based-on-position-14) status { deferred } @end
  @implements(xslt20:numbering-based-on-position-15) status { deferred } @end
  @implements(xslt20:numbering-based-on-position-16) status { deferred } @end
  @implements(xslt20:numbering-based-on-position-17) status { deferred } @end
  @implements(xslt20:numbering-based-on-position-18) status { deferred } @end
  @implements(xslt20:numbering-based-on-position-19) status { deferred } @end
  @implements(xslt20:numbering-based-on-position-20) status { deferred } @end
  @implements(xslt20:numbering-based-on-position-21) status { deferred } @end
  @implements(xslt20:numbering-based-on-position-22) status { deferred } @end
  @implements(xslt20:convert-1) status { deferred } @end
  @implements(xslt20:convert-2) status { deferred } @end
  @implements(xslt20:convert-3) status { deferred } @end
  @implements(xslt20:convert-4) status { deferred } @end
  @implements(xslt20:convert-5) status { deferred } @end
  @implements(xslt20:convert-6) status { deferred } @end
  @implements(xslt20:convert-7) status { deferred } @end
  @implements(xslt20:convert-8) status { deferred } @end
  @implements(xslt20:convert-9) status { deferred } @end
  @implements(xslt20:convert-10) status { deferred } @end
  @implements(xslt20:convert-11) status { deferred } @end
  @implements(xslt20:convert-12) status { deferred } @end
  @implements(xslt20:convert-13) status { deferred } @end
  @implements(xslt20:convert-14) status { deferred } @end
  @implements(xslt20:convert-15) status { deferred } @end

  @implements(xslt20:key-1) status { deferred } @end
  @implements(xslt20:xsl-key-1) status { deferred } @end
  @implements(xslt20:xsl-key-2) status { deferred } @end
  @implements(xslt20:xsl-key-3) status { deferred } @end
  @implements(xslt20:xsl-key-4) status { deferred } @end
  @implements(xslt20:xsl-key-5) status { deferred } @end
  @implements(xslt20:xsl-key-6) status { deferred } @end
  @implements(xslt20:xsl-key-7) status { deferred } @end
  @implements(xslt20:xsl-key-8) status { deferred } @end
  @implements(xslt20:xsl-key-9) status { deferred } @end
  @implements(xslt20:xsl-key-10) status { deferred } @end
  @implements(xslt20:xsl-key-11) status { deferred } @end
  @implements(xslt20:xsl-key-12) status { deferred } @end
  @implements(xslt20:xsl-key-13) status { deferred } @end
  @implements(xslt20:xsl-key-14) status { deferred } @end
  @implements(xslt20:xsl-key-15) status { deferred } @end
  @implements(xslt20:xsl-key-16) status { deferred } @end
  @implements(xslt20:keys-1) status { deferred } @end
  @implements(xslt20:keys-2) status { deferred } @end
  @implements(xslt20:keys-3) status { deferred } @end
  @implements(xslt20:keys-4) status { deferred } @end
  @implements(xslt20:keys-5) status { deferred } @end
  @implements(xslt20:keys-6) status { deferred } @end
  @implements(xslt20:keys-7) status { deferred } @end
  @implements(xslt20:keys-8) status { deferred } @end
  @implements(xslt20:keys-9) status { deferred } @end
  @implements(xslt20:keys-10) status { deferred } @end
  @implements(xslt20:keys-11) status { deferred } @end
  @implements(xslt20:keys-12) status { deferred } @end
  @implements(xslt20:keys-13) status { deferred } @end
  @implements(xslt20:keys-14) status { deferred } @end
  @implements(xslt20:keys-15) status { deferred } @end
  @implements(xslt20:keys-16) status { deferred } @end
  @implements(xslt20:keys-17) status { deferred } @end
  @implements(xslt20:keys-18) status { deferred } @end

  @implements(xslt20:format-number-1) status { deferred } @end
  @implements(xslt20:format-number-2) status { deferred } @end
  @implements(xslt20:format-number-3) status { deferred } @end
  @implements(xslt20:format-number-4) status { deferred } @end
  @implements(xslt20:format-number-5) status { deferred } @end
  @implements(xslt20:format-number-6) status { deferred } @end
  @implements(xslt20:format-number-7) status { deferred } @end
  @implements(xslt20:format-number-8) status { deferred } @end
  @implements(xslt20:defining-decimal-format-1) status { deferred } @end
  @implements(xslt20:defining-decimal-format-2) status { deferred } @end
  @implements(xslt20:defining-decimal-format-3) status { deferred } @end
  @implements(xslt20:defining-decimal-format-4) status { deferred } @end
  @implements(xslt20:defining-decimal-format-5) status { deferred } @end
  @implements(xslt20:defining-decimal-format-6) status { deferred } @end
  @implements(xslt20:defining-decimal-format-7) status { deferred } @end
  @implements(xslt20:defining-decimal-format-8) status { deferred } @end
  @implements(xslt20:defining-decimal-format-9) status { deferred } @end
  @implements(xslt20:defining-decimal-format-10) status { deferred } @end
  @implements(xslt20:defining-decimal-format-11) status { deferred } @end
  @implements(xslt20:defining-decimal-format-12) status { deferred } @end
  @implements(xslt20:defining-decimal-format-13) status { deferred } @end
  @implements(xslt20:defining-decimal-format-14) status { deferred } @end
  @implements(xslt20:defining-decimal-format-15) status { deferred } @end
  @implements(xslt20:defining-decimal-format-16) status { deferred } @end
  @implements(xslt20:processing-picture-string-1) status { deferred } @end
  @implements(xslt20:processing-picture-string-2) status { deferred } @end
  @implements(xslt20:processing-picture-string-3) status { deferred } @end
  @implements(xslt20:processing-picture-string-4) status { deferred } @end
  @implements(xslt20:processing-picture-string-5) status { deferred } @end
  @implements(xslt20:processing-picture-string-6) status { deferred } @end
  @implements(xslt20:processing-picture-string-7) status { deferred } @end
  @implements(xslt20:analysing-picture-string-1) status { deferred } @end
  @implements(xslt20:analysing-picture-string-2) status { deferred } @end
  @implements(xslt20:analysing-picture-string-3) status { deferred } @end
  @implements(xslt20:analysing-picture-string-4) status { deferred } @end
  @implements(xslt20:analysing-picture-string-5) status { deferred } @end
  @implements(xslt20:formatting-the-number-1) status { deferred } @end
  @implements(xslt20:formatting-the-number-2) status { deferred } @end
  @implements(xslt20:formatting-the-number-3) status { deferred } @end
  @implements(xslt20:format-date-1) status { deferred } @end
  @implements(xslt20:format-date-2) status { deferred } @end
  @implements(xslt20:format-date-3) status { deferred } @end
  @implements(xslt20:format-date-4) status { deferred } @end
  @implements(xslt20:format-date-5) status { deferred } @end
  @implements(xslt20:format-date-6) status { deferred } @end
  @implements(xslt20:format-date-7) status { deferred } @end
  @implements(xslt20:format-date-8) status { deferred } @end
  @implements(xslt20:format-date-9) status { deferred } @end
  @implements(xslt20:format-date-10) status { deferred } @end
  @implements(xslt20:format-date-11) status { deferred } @end
  @implements(xslt20:format-date-12) status { deferred } @end
  @implements(xslt20:format-date-13) status { deferred } @end
  @implements(xslt20:format-date-14) status { deferred } @end
  @implements(xslt20:format-date-15) status { deferred } @end
  @implements(xslt20:format-date-16) status { deferred } @end
  @implements(xslt20:date-picture-string-1) status { deferred } @end
  @implements(xslt20:date-picture-string-2) status { deferred } @end
  @implements(xslt20:date-picture-string-3) status { deferred } @end
  @implements(xslt20:date-picture-string-4) status { deferred } @end
  @implements(xslt20:date-picture-string-5) status { deferred } @end
  @implements(xslt20:date-picture-string-6) status { deferred } @end
  @implements(xslt20:date-picture-string-7) status { deferred } @end
  @implements(xslt20:date-picture-string-8) status { deferred } @end
  @implements(xslt20:date-picture-string-9) status { deferred } @end
  @implements(xslt20:date-picture-string-10) status { deferred } @end
  @implements(xslt20:date-picture-string-11) status { deferred } @end
  @implements(xslt20:date-picture-string-12) status { deferred } @end
  @implements(xslt20:date-picture-string-13) status { deferred } @end
  @implements(xslt20:date-picture-string-14) status { deferred } @end
  @implements(xslt20:date-picture-string-15) status { deferred } @end
  @implements(xslt20:date-picture-string-16) status { deferred } @end
  @implements(xslt20:date-picture-string-17) status { deferred } @end
  @implements(xslt20:date-picture-string-18) status { deferred } @end
  @implements(xslt20:date-picture-string-19) status { deferred } @end
  @implements(xslt20:date-picture-string-20) status { deferred } @end
  @implements(xslt20:date-picture-string-21) status { deferred } @end
  @implements(xslt20:lang-cal-country-1) status { deferred } @end
  @implements(xslt20:lang-cal-country-2) status { deferred } @end
  @implements(xslt20:lang-cal-country-3) status { deferred } @end
  @implements(xslt20:lang-cal-country-4) status { deferred } @end
  @implements(xslt20:lang-cal-country-5) status { deferred } @end
  @implements(xslt20:lang-cal-country-6) status { deferred } @end
  @implements(xslt20:lang-cal-country-7) status { deferred } @end
  @implements(xslt20:lang-cal-country-8) status { deferred } @end
  @implements(xslt20:lang-cal-country-9) status { deferred } @end
  @implements(xslt20:lang-cal-country-10) status { deferred } @end
  @implements(xslt20:lang-cal-country-11) status { deferred } @end
  @implements(xslt20:lang-cal-country-12) status { deferred } @end
  @implements(xslt20:lang-cal-country-13) status { deferred } @end
  @implements(xslt20:lang-cal-country-14) status { deferred } @end
  @implements(xslt20:lang-cal-country-15) status { deferred } @end
  @implements(xslt20:lang-cal-country-16) status { deferred } @end
  @implements(xslt20:lang-cal-country-17) status { deferred } @end
  @implements(xslt20:lang-cal-country-18) status { deferred } @end
  @implements(xslt20:date-time-examples-1) status { deferred } @end
  @implements(xslt20:date-time-examples-2) status { deferred } @end
  @implements(xslt20:date-time-examples-3) status { deferred } @end

  @implements(xslt20:unparsed-entity-uri-1) status { deferred } @end
  @implements(xslt20:unparsed-entity-uri-2) status { deferred } @end
  @implements(xslt20:unparsed-entity-uri-3) status { deferred } @end
  @implements(xslt20:unparsed-entity-uri-4) status { deferred } @end
  @implements(xslt20:unparsed-entity-public-id-1) status { deferred } @end
  @implements(xslt20:unparsed-entity-public-id-2) status { deferred } @end
  @implements(xslt20:unparsed-entity-public-id-3) status { deferred } @end
  @implements(xslt20:unparsed-entity-public-id-4) status { deferred } @end
  @implements(xslt20:generate-id-1) status { deferred } @end
  @implements(xslt20:generate-id-2) status { deferred } @end
  @implements(xslt20:generate-id-3) status { deferred } @end
  @implements(xslt20:generate-id-4) status { deferred } @end
  @implements(xslt20:system-property-1) status { deferred } @end
  @implements(xslt20:system-property-2) status { deferred } @end
  @implements(xslt20:system-property-3) status { deferred } @end
  @implements(xslt20:system-property-4) status { deferred } @end
  @implements(xslt20:system-property-5) status { deferred } @end
  @implements(xslt20:system-property-6) status { deferred } @end
  @implements(xslt20:system-property-7) status { deferred } @end
  @implements(xslt20:system-property-8) status { deferred } @end
  @implements(xslt20:system-property-9) status { deferred } @end
  @implements(xslt20:system-property-10) status { deferred } @end
  @implements(xslt20:system-property-11) status { deferred } @end

  @implements(xslt20:message-1) status { deferred } @end
  @implements(xslt20:message-2) status { deferred } @end
  @implements(xslt20:message-3) status { deferred } @end
  @implements(xslt20:message-4) status { deferred } @end
  @implements(xslt20:message-5) status { deferred } @end
  @implements(xslt20:message-6) status { deferred } @end
  @implements(xslt20:message-7) status { deferred } @end
  @implements(xslt20:message-8) status { deferred } @end
  @implements(xslt20:message-9) status { deferred } @end
  @implements(xslt20:message-10) status { deferred } @end
  @implements(xslt20:message-11) status { deferred } @end
  @implements(xslt20:message-12) status { deferred } @end

  @implements(xslt20:external-objects-1) status { deferred } @end
  @implements(xslt20:external-objects-2) status { deferred } @end
  @implements(xslt20:extension-instruction-1) status { deferred } @end
  @implements(xslt20:extension-instruction-2) status { deferred } @end
  @implements(xslt20:designating-extension-namespace-1) status { deferred } @end
  @implements(xslt20:designating-extension-namespace-2) status { deferred } @end
  @implements(xslt20:testing-instruction-available-1) status { deferred } @end
  @implements(xslt20:testing-instruction-available-2) status { deferred } @end
  @implements(xslt20:testing-instruction-available-3) status { deferred } @end
  @implements(xslt20:testing-instruction-available-4) status { deferred } @end
  @implements(xslt20:testing-instruction-available-5) status { deferred } @end
  @implements(xslt20:testing-instruction-available-6) status { deferred } @end
  @implements(xslt20:testing-instruction-available-7) status { deferred } @end
  @implements(xslt20:fallback-1) status { deferred } @end
  @implements(xslt20:fallback-2) status { deferred } @end
  @implements(xslt20:fallback-3) status { deferred } @end
  @implements(xslt20:fallback-4) status { deferred } @end
  @implements(xslt20:fallback-5) status { deferred } @end
  @implements(xslt20:fallback-6) status { deferred } @end

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
