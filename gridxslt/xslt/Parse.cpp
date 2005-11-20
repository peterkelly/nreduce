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

#include "Parse.h"
#include "util/Namespace.h"
#include "util/String.h"
#include "util/Macros.h"
#include "util/Debug.h"
#include "dataflow/Serialization.h"
#include <libxml/xmlwriter.h>
#include <libxml/xmlIO.h>
#include <libxml/parser.h>
#include <libxml/tree.h>
#include <libxml/uri.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>

using namespace GridXSLT;

#define ALLOWED_ATTRIBUTES(_node,...) \
      CHECK_CALL(enforce_allowed_attributes2(ei,filename,(_node),XSLT_NAMESPACE, \
                                            standard_attributes,0,__VA_ARGS__,NULL))
#define REQUIRED_ATTRIBUTE(_node,_name) \
      if (!(_node)->hasAttribute(_name)) \
        return missing_attribute(ei,filename,(_node)->m_line,String::null(),_name);

static int parse_sequence_constructors(Statement *sn, Node *start,
                                       Error *ei, const String &filename);

static int parse_xslt_uri(Error *ei, const String &filename, int line, const char *errname,
                          const String &full_uri, TransformExpr *sroot, const String &refsource);

// FIXME: should also allow xml:id (and others, e.g. XML Schema instance?)
// Should we really enforece the absence of attributes from other namespaces? Probably not
// strictly necessary and it may restrict other uses of the element trees which rely on
// the ability to add their own attributes

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

static NSName ATTR_AS                        = NSName("as");
static NSName ATTR_BYTE_ORDER_MARK           = NSName("byte-order-mark");
static NSName ATTR_CASE_ORDER                = NSName("order");
static NSName ATTR_CDATA_SECTION_ELEMENTS    = NSName("cdata-section-elements");
static NSName ATTR_COLLATION                 = NSName("collation");
static NSName ATTR_COPY_NAMESPACES           = NSName("copy-namespaces");
static NSName ATTR_DATA_TYPE                 = NSName("data-type");
static NSName ATTR_DISABLE_OUTPUT_ESCAPING   = NSName("disable-output-escaping");
static NSName ATTR_DOCTYPE_PUBLIC            = NSName("doctype-public");
static NSName ATTR_DOCTYPE_SYSTEM            = NSName("doctype-system");
static NSName ATTR_ELEMENTS                  = NSName("elements");
static NSName ATTR_ENCODING                  = NSName("encoding");
static NSName ATTR_ESCAPE_URI_ATTRIBUTES     = NSName("escape-uri-attributes");
static NSName ATTR_EXCLUDE_RESULT_PREFIXES   = NSName("exclude-result-prefixes");
static NSName ATTR_EXCLUDE_RESULT_PREFIXES2  = NSName(XSLT_NAMESPACE,"exclude-result-prefixes");
static NSName ATTR_FLAGS                     = NSName("flags");
static NSName ATTR_GROUP_ADJACENT            = NSName("group-adjacent");
static NSName ATTR_GROUP_BY                  = NSName("group-by");
static NSName ATTR_GROUP_ENDING_WITH         = NSName("group-ending-with");
static NSName ATTR_GROUP_STARTING_WITH       = NSName("group-starting-with");
static NSName ATTR_HREF                      = NSName("href");
static NSName ATTR_INCLUDE_CONTENT_TYPE      = NSName("include-content-type");
static NSName ATTR_INDENT                    = NSName("indent");
static NSName ATTR_INHERIT_NAMESPACES        = NSName("inherit-namespaces");
static NSName ATTR_LANG                      = NSName("lang");
static NSName ATTR_MATCH                     = NSName("match");
static NSName ATTR_MEDIA_TYPE                = NSName("media-type");
static NSName ATTR_METHOD                    = NSName("method");
static NSName ATTR_MODE                      = NSName("mode");
static NSName ATTR_NAME                      = NSName("name");
static NSName ATTR_NAMESPACE                 = NSName("namespace");
static NSName ATTR_NORMALIZATION_FORM        = NSName("normalization-form");
static NSName ATTR_OMIT_XML_DECLARATION      = NSName("omit-xml-declaration");
static NSName ATTR_ORDER                     = NSName("order");
static NSName ATTR_OVERRIDE                  = NSName("override");
static NSName ATTR_PRIORITY                  = NSName("priority");
static NSName ATTR_REGEX                     = NSName("regex");
static NSName ATTR_REQUIRED                  = NSName("required");
static NSName ATTR_SELECT                    = NSName("select");
static NSName ATTR_SEPARATOR                 = NSName("separator");
static NSName ATTR_STABLE                    = NSName("stable");
static NSName ATTR_STANDALONE                = NSName("standalone");
static NSName ATTR_TERMINATE                 = NSName("terminate");
static NSName ATTR_TEST                      = NSName("test");
static NSName ATTR_TUNNEL                    = NSName("tunnel");
static NSName ATTR_TYPE                      = NSName("type");
static NSName ATTR_UNDECLARE_PREFIXES        = NSName("undeclare-prefixes");
static NSName ATTR_USE_ATTRIBUTE_SETS        = NSName("use-attribute-sets");
static NSName ATTR_USE_CHARACTER_MAPS        = NSName("use-character-maps");
static NSName ATTR_VALIDATION                = NSName("validation");
static NSName ATTR_VERSION                   = NSName("version");

static NSName ELEM_ANALYZE_STRING            = NSName(XSLT_NAMESPACE,"analyze-string");
static NSName ELEM_APPLY_IMPORTS             = NSName(XSLT_NAMESPACE,"apply-imports");
static NSName ELEM_APPLY_TEMPLATES           = NSName(XSLT_NAMESPACE,"apply-templates");
static NSName ELEM_ATTRIBUTE                 = NSName(XSLT_NAMESPACE,"attribute");
static NSName ELEM_ATTRIBUTE_SET             = NSName(XSLT_NAMESPACE,"attribute-set");
static NSName ELEM_CALL_TEMPLATE             = NSName(XSLT_NAMESPACE,"call-template");
static NSName ELEM_CHARACTER_MAP             = NSName(XSLT_NAMESPACE,"character-map");
static NSName ELEM_CHOOSE                    = NSName(XSLT_NAMESPACE,"choose");
static NSName ELEM_COMMENT                   = NSName(XSLT_NAMESPACE,"comment");
static NSName ELEM_COPY                      = NSName(XSLT_NAMESPACE,"copy");
static NSName ELEM_COPY_OF                   = NSName(XSLT_NAMESPACE,"copy-of");
static NSName ELEM_DECIMAL_FORMAT            = NSName(XSLT_NAMESPACE,"decimal-format");
static NSName ELEM_ELEMENT                   = NSName(XSLT_NAMESPACE,"element");
static NSName ELEM_FALLBACK                  = NSName(XSLT_NAMESPACE,"fallback");
static NSName ELEM_FOR_EACH                  = NSName(XSLT_NAMESPACE,"for-each");
static NSName ELEM_FOR_EACH_GROUP            = NSName(XSLT_NAMESPACE,"for-each-group");
static NSName ELEM_FUNCTION                  = NSName(XSLT_NAMESPACE,"function");
static NSName ELEM_IF                        = NSName(XSLT_NAMESPACE,"if");
static NSName ELEM_IMPORT                    = NSName(XSLT_NAMESPACE,"import");
static NSName ELEM_IMPORT_SCHEMA             = NSName(XSLT_NAMESPACE,"import-schema");
static NSName ELEM_INCLUDE                   = NSName(XSLT_NAMESPACE,"include");
static NSName ELEM_KEY                       = NSName(XSLT_NAMESPACE,"key");
static NSName ELEM_MATCHING_SUBSTRING        = NSName(XSLT_NAMESPACE,"matching-substring");
static NSName ELEM_MESSAGE                   = NSName(XSLT_NAMESPACE,"message");
static NSName ELEM_NAMESPACE                 = NSName(XSLT_NAMESPACE,"namespace");
static NSName ELEM_NAMESPACE_ALIAS           = NSName(XSLT_NAMESPACE,"namespace-alias");
static NSName ELEM_NEXT_MATCH                = NSName(XSLT_NAMESPACE,"next-match");
static NSName ELEM_NON_MATCHING_SUBSTRING    = NSName(XSLT_NAMESPACE,"non-matching-substring");
static NSName ELEM_NUMBER                    = NSName(XSLT_NAMESPACE,"number");
static NSName ELEM_OTHERWISE                 = NSName(XSLT_NAMESPACE,"otherwise");
static NSName ELEM_OUTPUT                    = NSName(XSLT_NAMESPACE,"output");
static NSName ELEM_PARAM                     = NSName(XSLT_NAMESPACE,"param");
static NSName ELEM_PERFORM_SORT              = NSName(XSLT_NAMESPACE,"perform-sort");
static NSName ELEM_PRESERVE_SPACE            = NSName(XSLT_NAMESPACE,"preserve-space");
static NSName ELEM_PROCESSING_INSTRUCTION    = NSName(XSLT_NAMESPACE,"processing-instruction");
static NSName ELEM_RESULT_DOCUMENT           = NSName(XSLT_NAMESPACE,"result-document");
static NSName ELEM_SEQUENCE                  = NSName(XSLT_NAMESPACE,"sequence");
static NSName ELEM_SORT                      = NSName(XSLT_NAMESPACE,"sort");
static NSName ELEM_STRIP_SPACE               = NSName(XSLT_NAMESPACE,"strip-space");
static NSName ELEM_STYLESHEET                = NSName(XSLT_NAMESPACE,"stylesheet");
static NSName ELEM_TEMPLATE                  = NSName(XSLT_NAMESPACE,"template");
static NSName ELEM_TEXT                      = NSName(XSLT_NAMESPACE,"text");
static NSName ELEM_TRANSFORM                 = NSName(XSLT_NAMESPACE,"transform");
static NSName ELEM_VALUE_OF                  = NSName(XSLT_NAMESPACE,"value-of");
static NSName ELEM_VARIABLE                  = NSName(XSLT_NAMESPACE,"variable");
static NSName ELEM_WHEN                      = NSName(XSLT_NAMESPACE,"when");
static NSName ELEM_WITH_PARAM                = NSName(XSLT_NAMESPACE,"with-param");

static int enforce_allowed_attributes2(Error *ei, const String &filename, Node *n,
                               const String &restrictns, List<NSName> &stdattrs, int xx, ...)
{
  va_list ap;

  for (Iterator<Node*> it = n->m_attributes; it.haveCurrent(); it++) {
    Node *attr = *it;
    int allowed = 0;
    NSName *cur;

    va_start(ap,xx);
    while ((NULL != (cur = va_arg(ap,NSName*))) && !allowed) {
      if (*cur == attr->m_ident)
        allowed = 1;
    }
    va_end(ap);

    if (!attr->m_ident.m_ns.isNull() && (restrictns != attr->m_ident.m_ns))
      allowed = 1;

    Iterator<NSName> sit;
    for (sit = stdattrs; sit.haveCurrent() && !allowed; sit++)
      if (*sit == attr->m_ident)
        allowed = 1;

    if (!allowed) {
      // FIXME: show fully-qualified name
      error(ei,filename,n->m_line,String::null(),
            "attribute \"%*\" is not permitted on element \"xsl:%*\"",
            &attr->m_qn,&n->m_ident.m_name);
      return -1;
    }
  }

  return 0;
}

static void xslt_skip_others(Node **n)
{
  while ((*n) &&
         ((NODE_COMMENT == (*n)->m_type) ||
          (NODE_PI == (*n)->m_type) ||
          ((NODE_TEXT == (*n)->m_type) &&
           (*n)->m_value.isAllWhitespace())))
    (*n) = (*n)->next();
}

static void xslt_next_element(Node **n)
{
  *n = (*n)->next();
  xslt_skip_others(n);
}

static Node *xslt_first_child(Node *n)
{
  Node *c = n->firstChild();
  xslt_skip_others(&c);
  return c;
}

static Expression *get_expr(Error *ei, const String &filename,
                         int line, const String &attrname, const String &str, int pattern)
{
  return Expression_parse(str,filename,line,ei,pattern);
}

static Expression *get_expr_attr(Error *ei, const String &filename,
                              int line, const String &attrname, const String &str)
{
  if (2 <= str.length() &&
      ("{" == str.substring(0,1)) && ("}" == str.substring(str.length()-1,1))) {
    String expr_str = str.substring(1,str.length()-2);
    return get_expr(ei,filename,line,attrname,expr_str,0);
  }
  else if ((0 <= str.indexOf("{")) && (0 <= str.indexOf("}"))) {
    fmessage(stderr,"FIXME: I don't support handling attributes with only partial "
                   "expressions yet! (%*)\n",&str);
    exit(1);
  }
  else {
    return new StringExpr(str);
  }
}

static void add_ns_defs(Statement *sn, Node *node)
{
  xmlNsPtr ns;
  for (ns = node->m_xn->nsDef; ns; ns = ns->next)
    sn->m_namespaces->add_direct(ns->href,ns->prefix);
}

static Statement *add_node(Statement ***ptr, Statement *new_node,
                            const String &filename, Node *n)
{
  new_node->m_sloc.uri = filename;
  new_node->m_sloc.line = n->m_line;
  add_ns_defs(new_node,n);
  **ptr = new_node;
  *ptr = &new_node->m_next;
  return new_node;
}

static int xslt_invalid_element(Error *ei, const String &filename, Node *n)
{
  // @implements(xslt20:import-4)
  // test { xslt/parse/import_nottoplevel.test }
  // @end
  if (n->m_ident == ELEM_IMPORT)
    return error(ei,filename,n->m_line,"XTSE0190",
                 "An xsl:import element must be a top-level element");
  // @implements(xslt20:include-4)
  // test { xslt/parse/include_nottoplevel.test }
  // @end
  else if (n->m_ident == ELEM_INCLUDE)
    return error(ei,filename,n->m_line,"XTSE0170",
                 "An xsl:include element must be a top-level element");
  else
    return invalid_element(ei,filename,n);
}


static int parse_optional_as(Node *n, SequenceType *st, Error *ei, const String &filename)
{
  String as = n->getAttribute(ATTR_AS);
  if (!as.isNull()) {
    *st = seqtype_parse(as,filename,n->m_line,ei);
    if ((*st).isNull())
      return -1;
  }
  return 0;
}

static int parse_param(Statement *sn, Node *n,
                       Error *ei, const String &filename)
{
  String required = n->getAttribute(ATTR_REQUIRED); // FIXME: support this
  String tunnel = n->getAttribute(ATTR_TUNNEL); // FIXME: support this
  Statement **param_ptr = &sn->m_param;

  // @implements(xslt20:parameters-5) status { info } @end

  ALLOWED_ATTRIBUTES(n,&ATTR_NAME,&ATTR_SELECT,&ATTR_AS,&ATTR_REQUIRED,&ATTR_TUNNEL)
  while (*param_ptr)
    param_ptr = &((*param_ptr)->m_next);

  if ((XSLT_TRANSFORM != sn->m_type) &&
      (XSLT_DECL_FUNCTION != sn->m_type) &&
      (XSLT_DECL_TEMPLATE != sn->m_type))
    return xslt_invalid_element(ei,filename,n);



  // @implements(xslt20:parameters-3)
  // test { xslt/parse/function_param1.test }
  // test { test/xslt/parse/function_param2.test }
  // test { test/xslt/parse/function_param3.test }
  // test { test/xslt/parse/function_param_noname.test }
  // @end
  REQUIRED_ATTRIBUTE(n,ATTR_NAME)

  String name = n->getAttribute(ATTR_NAME);
  QName qn = QName::parse(name);
  NSName resolved = n->resolveQName(qn);
  if (resolved.isNull()) {
    return error(ei,filename,n->m_line,"XTSE0280","Could not resolve namespace for prefix \"%*\"",
                 &qn.m_prefix);
  }

  Expression *selectExpr = NULL;

#if 1
  String select = n->getAttribute(ATTR_SELECT);
  if ((!select.isNull()) &&
      (NULL == (selectExpr = get_expr(ei,filename,n->m_line,"select",select,0))))
    return -1;
//   if ((NULL != required) && !strcmp(required,"yes"))
//     new_sn->m_flags |= FLAG_REQUIRED;
//   if ((NULL != tunnel) && !strcmp(tunnel,"yes"))
//     new_sn->m_flags |= FLAG_TUNNEL;
#endif

  ParamExpr *new_sn = new ParamExpr(qn,selectExpr,0);
  add_node(&param_ptr,new_sn,filename,n);

  // @implements(xslt20:parameters-8)
  // test { xslt/parse/param_as1.test }
  // test { xslt/parse/param_as2.test }
  // test { xslt/parse/param_as3.test }
  // @end
  CHECK_CALL(parse_optional_as(n,&new_sn->m_st,ei,filename))

  CHECK_CALL(parse_sequence_constructors(new_sn,n->firstChild(),ei,filename))

  return 0;
}

static bool shouldExcludePrefix(Node *from, const String &prefix)
{
  // FIXME: only take into account exclude-result-prefixes attributes with the appropriate
  // attribute... see XSLT 2.0 section 11.1.3 point 3 for details
  for (Node *n = from; n; n = n->parent()) {
    String exclude = n->getAttribute(ATTR_EXCLUDE_RESULT_PREFIXES);
    if (exclude.isNull())
      exclude = n->getAttribute(ATTR_EXCLUDE_RESULT_PREFIXES2);
    if (!exclude.isNull() && exclude.parseList().contains(prefix))
      return true;
  }
  return false;
}

static int parse_sequence_constructors(Statement *sn, Node *start,
                                       Error *ei, const String &filename)
{
  Statement **child_ptr = &sn->m_child;
  Statement **param_ptr = &sn->m_param;
  Statement **sort_ptr = &sn->m_sort;

  while (NULL != *child_ptr)
    child_ptr = &((*child_ptr)->m_next);

  // FIXME: still need to do more checks on the tree after to test for e.g. duplicate parameter
  // names, and content supplied inside <comment>s in addition to "select" being specified

  // FIXME: for yes/no values, complain if the value is something other than "yes" or "no"
  // (unless of course the attribute is missing and not required). Should this check be
  // case insensitive?

  xslt_skip_others(&start);
  for (Node *child = start; child; xslt_next_element(&child)) {
    if (child->m_ident == ELEM_ANALYZE_STRING) {
      ALLOWED_ATTRIBUTES(child,&ATTR_SELECT,&ATTR_REGEX,&ATTR_FLAGS)
      REQUIRED_ATTRIBUTE(child,ATTR_SELECT)
      REQUIRED_ATTRIBUTE(child,ATTR_REGEX)
      String select = child->getAttribute(ATTR_SELECT);
      String regex = child->getAttribute(ATTR_REGEX);
      String flags = child->getAttribute(ATTR_FLAGS);
      // FIXME: add testcases to all.xl for use of fallback inside analyze-string

      int mcount = 0;
      int nmcount = 0;
      Expression *selectExpr = NULL;
      Expression *expr1 = NULL;
      Expression *expr2 = NULL;

      if (NULL == (selectExpr = get_expr(ei,filename,child->m_line,"select",select,0)))
        return -1;
      if (NULL == (expr1 = get_expr_attr(ei,filename,child->m_line,"regex",regex)))
        return -1;
      if ((!flags.isNull()) &&
          (NULL == (expr2 = get_expr_attr(ei,filename,child->m_line,"flags",flags))))
        return -1;

      Statement *new_sn = new AnalyzeStringExpr(selectExpr,expr1,expr2);
      add_node(&child_ptr,new_sn,filename,child);
      Statement **child2_ptr = &new_sn->m_child;

      Node *c2;
      for (c2 = child->firstChild(); c2; c2 = c2->next()) {
        if (c2->m_ident == ELEM_MATCHING_SUBSTRING) {
          SCExpr *matching = new SCExpr(XSLT_MATCHING_SUBSTRING);
          add_node(&child2_ptr,matching,filename,c2);
          CHECK_CALL(parse_sequence_constructors(matching,c2->firstChild(),ei,filename))
          mcount++;
        }
        else if (c2->m_ident == ELEM_NON_MATCHING_SUBSTRING) {
          SCExpr *non_matching = new SCExpr(XSLT_NON_MATCHING_SUBSTRING);
          add_node(&child2_ptr,non_matching,filename,c2);
          CHECK_CALL(parse_sequence_constructors(non_matching,c2->firstChild(),ei,filename))
          nmcount++;
        }
        else if (c2->m_ident == ELEM_FALLBACK) {
          FallbackExpr *fallback = new FallbackExpr();
          add_node(&child2_ptr,fallback,filename,c2);
          CHECK_CALL(parse_sequence_constructors(fallback,c2->firstChild(),ei,filename))
        }
      }
      if ((0 == mcount) && (0 == nmcount))
        return error(ei,filename,child->m_line,"XTSE1130",
                           "analyze-string must have at least a matching-substring "
                           "or non-matching-substring child");
      if ((1 < mcount) || (1 < nmcount))
        return error(ei,filename,child->m_line,String::null(),
                           "matching-substring and non-matching-substring may only appear once "
                           "each inside analyze-string");
    }
    else if (child->m_ident == ELEM_APPLY_IMPORTS) {
      ALLOWED_ATTRIBUTES(child,NULL)
      ApplyImportsExpr *new_sn = new ApplyImportsExpr();
      add_node(&child_ptr,new_sn,filename,child);

      // make sure all children are <xsl:param> elements
      Node *c2;
      for (c2 = child->firstChild(); c2; c2 = c2->next()) {
        if (NODE_ELEMENT != c2->m_type)
          continue;
        if (c2->m_ident != ELEM_WITH_PARAM)
          return invalid_element(ei,filename,c2);
      }

      CHECK_CALL(parse_sequence_constructors(new_sn,child->firstChild(),ei,filename))
    }
    else if (child->m_ident == ELEM_APPLY_TEMPLATES) {
      ALLOWED_ATTRIBUTES(child,&ATTR_SELECT,&ATTR_MODE)
      String select = child->getAttribute(ATTR_SELECT);
      String mode = child->getAttribute(ATTR_MODE);

      // make sure all children are <xsl:sort> or <xsl:with-param> elements
      Node *c2;
      for (c2 = child->firstChild(); c2; c2 = c2->next()) {
        if (NODE_ELEMENT != c2->m_type)
          continue;
        if ((c2->m_ident != ELEM_SORT) &&
            (c2->m_ident != ELEM_WITH_PARAM))
          return invalid_element(ei,filename,c2);
      }

      Expression *selectExpr = NULL;

      if (!select.isNull() &&
          (NULL == (selectExpr = get_expr(ei,filename,child->m_line,"select",select,0))))
        return -1;

      ApplyTemplatesExpr *new_sn = new ApplyTemplatesExpr(selectExpr);
      add_node(&child_ptr,new_sn,filename,child);

      // FIXME: multiple modes?
      if (!mode.isNull())
        new_sn->m_mode = QName::parse(mode);

      CHECK_CALL(parse_sequence_constructors(new_sn,child->firstChild(),ei,filename))
    }
    else if (child->m_ident == ELEM_ATTRIBUTE) {
      ALLOWED_ATTRIBUTES(child,&ATTR_NAME,&ATTR_NAMESPACE,&ATTR_SELECT,&ATTR_SEPARATOR,
                         &ATTR_TYPE,&ATTR_VALIDATION)
      REQUIRED_ATTRIBUTE(child,ATTR_NAME)
      String name = child->getAttribute(ATTR_NAME);
      String namespace1 = child->getAttribute(ATTR_NAMESPACE); // FIXME: support this
      String select = child->getAttribute(ATTR_SELECT);
      String separator = child->getAttribute(ATTR_SEPARATOR); // FIXME: support this
      String type = child->getAttribute(ATTR_TYPE); // FIXME: support this
      String validation = child->getAttribute(ATTR_VALIDATION); // FIXME: support this

      Expression *selectExpr = NULL;

      if (!select.isNull() &&
          (NULL == (selectExpr = get_expr(ei,filename,child->m_line,"select",select,0))))
        return -1;


      Statement *new_sn = new AttributeExpr(NULL,selectExpr,false);
      add_node(&child_ptr,new_sn,filename,child);
      new_sn->m_qn = QName::parse(name);

      CHECK_CALL(parse_sequence_constructors(new_sn,child->firstChild(),ei,filename))
    }
    else if (child->m_ident == ELEM_CALL_TEMPLATE) {
      ALLOWED_ATTRIBUTES(child,&ATTR_NAME)
      REQUIRED_ATTRIBUTE(child,ATTR_NAME)
      String name = child->getAttribute(ATTR_NAME);

      CallTemplateExpr *new_sn = new CallTemplateExpr(QName::parse(name));
      add_node(&child_ptr,new_sn,filename,child);

      // make sure all children are <xsl:with-param> elements
      Node *c2;
      for (c2 = child->firstChild(); c2; c2 = c2->next()) {
        if (NODE_ELEMENT != c2->m_type)
          continue;
        if (c2->m_ident != ELEM_WITH_PARAM)
          return invalid_element(ei,filename,c2);
      }

      CHECK_CALL(parse_sequence_constructors(new_sn,child->firstChild(),ei,filename))
    }
    else if (child->m_ident == ELEM_CHOOSE) {
      ALLOWED_ATTRIBUTES(child,NULL)
      Statement *new_sn = add_node(&child_ptr,new ChooseExpr(),filename,child);
      Statement *otherwise = NULL;
      Statement **child2_ptr = &new_sn->m_child;
      Node *c2;
      for (c2 = child->firstChild(); c2; c2 = c2->next()) {
        if (NODE_ELEMENT != c2->m_type)
          continue;

        if (c2->m_ident == ELEM_OTHERWISE) {
          if (NULL != otherwise)
            return error(ei,filename,c2->m_line,String::null(),
                         "Only one <otherwise> is allowed inside <choose>");
          otherwise = new SCExpr(XSLT_OTHERWISE);
          add_node(&child2_ptr,otherwise,filename,c2);
          CHECK_CALL(parse_sequence_constructors(otherwise,c2->firstChild(),ei,filename))
        }
        else if (c2->m_ident == ELEM_WHEN) {
          if (NULL != otherwise)
            return error(ei,filename,c2->m_line,String::null(),
                         "No <when> allowed after an <otherwise>");

          REQUIRED_ATTRIBUTE(c2,ATTR_TEST)

          String test = c2->getAttribute(ATTR_TEST);

          Expression *testExpr = NULL;
          if (NULL == (testExpr = get_expr(ei,filename,c2->m_line,"test",test,0)))
            return -1;

          WhenExpr *when = new WhenExpr(testExpr);
          add_node(&child2_ptr,when,filename,c2);

          CHECK_CALL(parse_sequence_constructors(when,c2->firstChild(),ei,filename))
        }
        else {
          return xslt_invalid_element(ei,filename,c2);
        }
      }
    }
    else if (child->m_ident == ELEM_COMMENT) {
      ALLOWED_ATTRIBUTES(child,&ATTR_SELECT)
      String select = child->getAttribute(ATTR_SELECT);

      Expression *selectExpr = NULL;

      if (!select.isNull() &&
          (NULL == (selectExpr = get_expr(ei,filename,child->m_line,"select",select,0))))
        return -1;

      CommentExpr *new_sn = new CommentExpr(selectExpr);
      add_node(&child_ptr,new_sn,filename,child);

      CHECK_CALL(parse_sequence_constructors(new_sn,child->firstChild(),ei,filename))
    }
    else if (child->m_ident == ELEM_COPY) {
      ALLOWED_ATTRIBUTES(child,&ATTR_COPY_NAMESPACES,&ATTR_INHERIT_NAMESPACES,
                         &ATTR_USE_ATTRIBUTE_SETS,&ATTR_TYPE,&ATTR_VALIDATION)
      String copyNamespaces = child->getAttribute(ATTR_COPY_NAMESPACES);       // FIXME: support
      String inheritNamespaces = child->getAttribute(ATTR_INHERIT_NAMESPACES); // FIXME: support
      String useAttributeSets = child->getAttribute(ATTR_USE_ATTRIBUTE_SETS);  // FIXME: support
      String type = child->getAttribute(ATTR_TYPE);                            // FIXME: support
      String validation = child->getAttribute(ATTR_VALIDATION);                // FIXME: support

      CopyExpr *new_sn = new CopyExpr();
      add_node(&child_ptr,new_sn,filename,child);
      CHECK_CALL(parse_sequence_constructors(new_sn,child->firstChild(),ei,filename))
    }
    else if (child->m_ident == ELEM_COPY_OF) {
      ALLOWED_ATTRIBUTES(child,&ATTR_SELECT,&ATTR_COPY_NAMESPACES,&ATTR_TYPE,&ATTR_VALIDATION)
      REQUIRED_ATTRIBUTE(child,ATTR_SELECT)
      String select = child->getAttribute(ATTR_SELECT);
      String copyNamespaces = child->getAttribute(ATTR_COPY_NAMESPACES);       // FIXME: support
      String type = child->getAttribute(ATTR_TYPE);                            // FIXME: support
      String validation = child->getAttribute(ATTR_VALIDATION);                // FIXME: support

      Expression *selectExpr = NULL;

      if (NULL == (selectExpr = get_expr(ei,filename,child->m_line,"select",select,0)))
        return -1;

      CopyOfExpr *new_sn = new CopyOfExpr(selectExpr);
      add_node(&child_ptr,new_sn,filename,child);

      Node *c2;
      for (c2 = child->firstChild(); c2; c2 = c2->next()) {
        if (NODE_ELEMENT == c2->m_type)
          return invalid_element(ei,filename,c2);
      }
    }
    else if (child->m_ident == ELEM_ELEMENT) {
      ALLOWED_ATTRIBUTES(child,&ATTR_NAME,&ATTR_NAMESPACE,&ATTR_INHERIT_NAMESPACES,
                         &ATTR_USE_ATTRIBUTE_SETS,&ATTR_TYPE,&ATTR_VALIDATION)
      REQUIRED_ATTRIBUTE(child,ATTR_NAME)
      String name = child->getAttribute(ATTR_NAME);
      String namespace1 = child->getAttribute(ATTR_NAMESPACE);                 // FIXME: support
      String inheritNamespaces = child->getAttribute(ATTR_INHERIT_NAMESPACES); // FIXME: support
      String useAttributeSets = child->getAttribute(ATTR_USE_ATTRIBUTE_SETS);  // FIXME: support
      String type = child->getAttribute(ATTR_TYPE);                            // FIXME: support
      String validation = child->getAttribute(ATTR_VALIDATION);                // FIXME: support

      Expression *nameExpr = get_expr_attr(ei,filename,child->m_line,"name",name);
      Statement *new_sn = new ElementExpr(nameExpr,false,false);
      add_node(&child_ptr,new_sn,filename,child);
      CHECK_CALL(parse_sequence_constructors(new_sn,child->firstChild(),ei,filename))
    }
    else if (child->m_ident == ELEM_FALLBACK) {
      ALLOWED_ATTRIBUTES(child,NULL)
      Statement *new_sn = new FallbackExpr();
      add_node(&child_ptr,new_sn,filename,child);
      CHECK_CALL(parse_sequence_constructors(new_sn,child->firstChild(),ei,filename))
    }
    else if (child->m_ident == ELEM_FOR_EACH) {
      ALLOWED_ATTRIBUTES(child,&ATTR_SELECT)
      REQUIRED_ATTRIBUTE(child,ATTR_SELECT)
      String select = child->getAttribute(ATTR_SELECT);

      Expression *selectExpr = NULL;

      if (NULL == (selectExpr = get_expr(ei,filename,child->m_line,"select",select,0)))
        return -1;

      Statement *new_sn = new ForEachExpr(selectExpr);
      add_node(&child_ptr,new_sn,filename,child);

      CHECK_CALL(parse_sequence_constructors(new_sn,child->firstChild(),ei,filename))
    }
    else if (child->m_ident == ELEM_FOR_EACH_GROUP) {
      ALLOWED_ATTRIBUTES(child,&ATTR_SELECT,&ATTR_GROUP_BY,&ATTR_GROUP_ADJACENT,
                         &ATTR_GROUP_STARTING_WITH,&ATTR_GROUP_ENDING_WITH,&ATTR_COLLATION)
      REQUIRED_ATTRIBUTE(child,ATTR_SELECT)
      String select = child->getAttribute(ATTR_SELECT);
      String groupBy = child->getAttribute(ATTR_GROUP_BY);
      String groupAdjacent = child->getAttribute(ATTR_GROUP_ADJACENT);
      String groupStartingWith = child->getAttribute(ATTR_GROUP_STARTING_WITH);
      String groupEndingWith = child->getAttribute(ATTR_GROUP_ENDING_WITH);
      String collation = child->getAttribute(ATTR_COLLATION); // FIXME: support

      int groupcount = 0;

      Expression *selectexpr;
      if (NULL == (selectexpr = get_expr(ei,filename,child->m_line,"select",select,0)))
        return -1;

      GroupMethod gmethod = GROUP_BY;
      Expression *expr1 = NULL;

      if (!groupBy.isNull()) {
        groupcount++;
        gmethod = GROUP_BY;
        expr1 = get_expr(ei,filename,child->m_line,"group-by",groupBy,0);
      }
      if (!groupAdjacent.isNull()) {
        groupcount++;
        gmethod = GROUP_ADJACENT;
        expr1 = get_expr(ei,filename,child->m_line,"group-adjacent",groupAdjacent,0);
      }
      if (!groupStartingWith.isNull()) {
        groupcount++;
        gmethod = GROUP_STARTING_WITH;
        expr1 = get_expr(ei,filename,child->m_line,"group-starting-with",groupStartingWith,0);
      }
      if (!groupEndingWith.isNull()) {
        groupcount++;
        gmethod = GROUP_ENDING_WITH;
        expr1 = get_expr(ei,filename,child->m_line,"group-ending-with",groupEndingWith,0);
      }
      if (0 == groupcount) {
        delete selectexpr;
        return error(ei,filename,child->m_line,String::null(),"No grouping method specified");
      }

      ForEachGroupExpr *new_sn = new ForEachGroupExpr(gmethod,selectexpr,expr1);
      add_node(&child_ptr,new_sn,filename,child);

      if (NULL == new_sn->m_expr1)
        return -1;
      if (1 < groupcount)
        return error(ei,filename,child->m_line,String::null(),
                     "Only one grouping method can be specified");
      CHECK_CALL(parse_sequence_constructors(new_sn,child->firstChild(),ei,filename))
    }
    else if (child->m_ident == ELEM_IF) {
      ALLOWED_ATTRIBUTES(child,&ATTR_TEST)
      REQUIRED_ATTRIBUTE(child,ATTR_TEST)
      String test = child->getAttribute(ATTR_TEST);

      Expression *selectExpr = NULL;

      if (NULL == (selectExpr = get_expr(ei,filename,child->m_line,"test",test,0)))
        return -1;

      Statement *new_sn = new XSLTIfExpr(selectExpr);
      add_node(&child_ptr,new_sn,filename,child);

      CHECK_CALL(parse_sequence_constructors(new_sn,child->firstChild(),ei,filename))
    }
    else if (child->m_ident == ELEM_MESSAGE) {
      ALLOWED_ATTRIBUTES(child,&ATTR_SELECT,&ATTR_TERMINATE)
      String select = child->getAttribute(ATTR_SELECT);
      String terminate = child->getAttribute(ATTR_TERMINATE);

      Expression *select_expr = NULL;
      bool term = false;
      Expression *terminate_expr = NULL;

      if (!select.isNull()) {
        if (NULL == (select_expr = get_expr(ei,filename,child->m_line,"select",select,0)))
          return -1;
      }
      if (!terminate.isNull() && (terminate == "yes")) {
        term = true;
      }
      else if (!terminate.isNull() && (terminate != "no")) {
        term = true;
        terminate_expr = get_expr_attr(ei,filename,child->m_line,"terminate",terminate);
        // FIXME: handle the case where the attribute specifies another value other than one
        // inside {}'s
      }

      Statement *new_sn = new MessageExpr(select_expr,term,terminate_expr);
      add_node(&child_ptr,new_sn,filename,child);

      CHECK_CALL(parse_sequence_constructors(new_sn,child->firstChild(),ei,filename))
    }
    else if (child->m_ident == ELEM_NAMESPACE) {
      ALLOWED_ATTRIBUTES(child,&ATTR_NAME,&ATTR_SELECT)
      REQUIRED_ATTRIBUTE(child,ATTR_NAME)
      String name = child->getAttribute(ATTR_NAME);
      String select = child->getAttribute(ATTR_SELECT);

      // @implements(xslt20:creating-namespace-nodes-1)
      // test { xslt/parse/namespace1.test }
      // test { xslt/parse/namespace2.test }
      // test { xslt/parse/namespace3.test }
      // test { xslt/parse/namespace_badattr.test }
      // test { xslt/parse/namespace_noname.test }
      // test { xslt/parse/namespace_otherattr.test }
      // @end
      // @implements(xslt20:creating-namespace-nodes-3)
      // status { partial }
      // issue { need to test how this evaluates with the empty string (should be default
      //         namespace i.e. NULL) }
      // @end
      // @implements(xslt20:creating-namespace-nodes-7) status { info } @end


      Expression *name_expr;
      if (NULL == (name_expr = get_expr_attr(ei,filename,child->m_line,"name",name)))
        return -1;

      // @implements(xslt20:creating-namespace-nodes-6)
      // status { partial }
      // fixme { we need to allow and correctly handle fallback instructions }
      // test { xslt/parse/namespace_selectcontent.test }
      // @end
      if (!select.isNull() && (NULL != xslt_first_child(child))) {
        error(ei,filename,child->m_line,"XTSE0910",
              "It is a static error if the select attribute of the xsl:namespace element is "
              "present when the element has content other than one or more xsl:fallback "
              "instructions, or if the select attribute is absent when the element has "
              "empty content.");
        delete name_expr;
        return -1;
      }

      Expression *selectExpr = NULL;

      if (!select.isNull() &&
          (NULL == (selectExpr = get_expr(ei,filename,child->m_line,"select",select,0))))
        return -1;

      NamespaceExpr *new_sn = new NamespaceExpr(name_expr,String::null(),selectExpr);
      add_node(&child_ptr,new_sn,filename,child);

      CHECK_CALL(parse_sequence_constructors(new_sn,child->firstChild(),ei,filename))
    }
    else if (child->m_ident == ELEM_NEXT_MATCH) {
      NextMatchExpr *new_sn = new NextMatchExpr();
      add_node(&child_ptr,new_sn,filename,child);

      // make sure all children are <xsl:param> or <xsl:fallback> elements
      Node *c2;
      for (c2 = child->firstChild(); c2; c2 = c2->next()) {
        if (NODE_ELEMENT != c2->m_type)
          continue;
        if ((c2->m_ident != ELEM_WITH_PARAM) &&
            (c2->m_ident != ELEM_FALLBACK))
          return invalid_element(ei,filename,c2);
      }

      CHECK_CALL(parse_sequence_constructors(new_sn,child->firstChild(),ei,filename))
    }
    else if (child->m_ident == ELEM_NUMBER) {
      // FIXME
    }
    else if (child->m_ident == ELEM_PERFORM_SORT) {
      ALLOWED_ATTRIBUTES(child,&ATTR_SELECT)
      String select = child->getAttribute(ATTR_SELECT);

      Expression *selectExpr = NULL;

      if (!select.isNull() &&
          (NULL == (selectExpr = get_expr(ei,filename,child->m_line,"select",select,0))))
        return -1;

      Statement *new_sn = new PerformSortExpr(selectExpr);
      add_node(&child_ptr,new_sn,filename,child);

      CHECK_CALL(parse_sequence_constructors(new_sn,child->firstChild(),ei,filename))
    }
    else if (child->m_ident == ELEM_PROCESSING_INSTRUCTION) {
      ALLOWED_ATTRIBUTES(child,&ATTR_NAME,&ATTR_SELECT)
      REQUIRED_ATTRIBUTE(child,ATTR_NAME)
      String name = child->getAttribute(ATTR_NAME);
      String select = child->getAttribute(ATTR_SELECT);

      Expression *nameExpr = NULL;
      Expression *selectExpr = NULL;

      // FIXME: if it's just a normal (non-expr) attribute value, set qname instead
      if ((NULL == (nameExpr = get_expr_attr(ei,filename,child->m_line,"name",name))))
        return -1;
      if (!select.isNull() &&
          (NULL == (selectExpr = get_expr(ei,filename,child->m_line,"select",select,0))))
        return -1;

      PIExpr *new_sn = new PIExpr(nameExpr,selectExpr);
      add_node(&child_ptr,new_sn,filename,child);

      CHECK_CALL(parse_sequence_constructors(new_sn,child->firstChild(),ei,filename))
    }
    else if (child->m_ident == ELEM_RESULT_DOCUMENT) {
      // FIXME
    }
    else if (child->m_ident == ELEM_SEQUENCE) {
      ALLOWED_ATTRIBUTES(child,&ATTR_SELECT)
      REQUIRED_ATTRIBUTE(child,ATTR_SELECT)
      String select = child->getAttribute(ATTR_SELECT);

      Expression *selectExpr = NULL;

      if (NULL == (selectExpr = get_expr(ei,filename,child->m_line,"select",select,0)))
        return -1;

      Statement *new_sn = new XSLTSequenceExpr(selectExpr);
      add_node(&child_ptr,new_sn,filename,child);

      // make sure all children are <xsl:fallback> elements
      Node *c2;
      for (c2 = child->firstChild(); c2; c2 = c2->next()) {
        if (NODE_ELEMENT != c2->m_type)
          continue;
        if (c2->m_ident != ELEM_FALLBACK)
          return invalid_element(ei,filename,c2);
      }
      CHECK_CALL(parse_sequence_constructors(new_sn,child->firstChild(),ei,filename))
    }
    else if (child->m_ident == ELEM_TEXT) {
      ALLOWED_ATTRIBUTES(child,&ATTR_DISABLE_OUTPUT_ESCAPING)
      String disableOutputEscaping = child->getAttribute(ATTR_DISABLE_OUTPUT_ESCAPING); // FIXME

      StringBuffer buf;

      Node *c2;
      for (c2 = child->firstChild(); c2; c2 = c2->next())
        if (NODE_TEXT == c2->m_type)
          buf.append(c2->m_value);
      add_node(&child_ptr,new TextExpr(buf,false),filename,child);
    }
    else if (child->m_ident == ELEM_VALUE_OF) {
      ALLOWED_ATTRIBUTES(child,&ATTR_SELECT,&ATTR_SEPARATOR,&ATTR_DISABLE_OUTPUT_ESCAPING)
      String select = child->getAttribute(ATTR_SELECT);
      String separator = child->getAttribute(ATTR_SEPARATOR);
      String disableOutputEscaping = child->getAttribute(ATTR_DISABLE_OUTPUT_ESCAPING); // FIXME

      // @implements(xslt20:value-of-2)
      // test { xslt/parse/value-of1.test }
      // test { xslt/parse/value-of2.test }
      // test { xslt/parse/value-of3.test }
      // test { xslt/parse/value-of4.test }
      // test { xslt/parse/value-of_badattr.test }
      // test { xslt/parse/value-of_otherattr.test }
      // @end
      //
      // @implements(xslt20:value-of-5)
      // test { xslt/parse/value-of_selcont1.test }
      // test { xslt/parse/value-of_selcont2.test }
      // @end

      if (child->hasAttribute(ATTR_SELECT) && (NULL != xslt_first_child(child)))
        return error(ei,filename,child->m_line,"XTSE0870",
                     "select attribute cannot be present when the "
                     "value-of instruction contains child instructions");

      if (!child->hasAttribute(ATTR_SELECT) && (NULL == xslt_first_child(child)))
        return error(ei,filename,child->m_line,"XTSE0870",
                     "Either a select attribute must be specified, "
                     "or one or more child instructions must be present");

      Expression *selectExpr = NULL;
      Expression *expr1 = NULL;

      if (!select.isNull() &&
          (NULL == (selectExpr = get_expr(ei,filename,child->m_line,"select",select,0))))
        return -1;

      if (!separator.isNull() &&
          (NULL == (expr1 = get_expr_attr(ei,filename,child->m_line,"separator",separator))))
        return -1;

      Statement *new_sn = new ValueOfExpr(selectExpr,expr1);
      add_node(&child_ptr,new_sn,filename,child);

      CHECK_CALL(parse_sequence_constructors(new_sn,child->firstChild(),ei,filename))
    }
    else if (child->m_ident == ELEM_VARIABLE) {
      ALLOWED_ATTRIBUTES(child,&ATTR_NAME,&ATTR_SELECT,&ATTR_AS)
      REQUIRED_ATTRIBUTE(child,ATTR_NAME)
      String name = child->getAttribute(ATTR_NAME);
      String select = child->getAttribute(ATTR_SELECT);
      String as = child->getAttribute(ATTR_AS); // FIXME: support

      Expression *selectExpr = NULL;

      if (!select.isNull() &&
          (NULL == (selectExpr = get_expr(ei,filename,child->m_line,"select",select,0))))
        return -1;

      Statement *new_sn = new VariableExpr(QName::parse(name),selectExpr);
      add_node(&child_ptr,new_sn,filename,child);

      CHECK_CALL(parse_sequence_constructors(new_sn,child->firstChild(),ei,filename))
    }
    else if (child->m_ident == ELEM_PARAM) {
      ASSERT(!"params should already be handled by the parent function/template/stylesheet");
    }
    else if (child->m_ident == ELEM_WITH_PARAM) {
      ALLOWED_ATTRIBUTES(child,&ATTR_NAME,&ATTR_SELECT,&ATTR_AS,&ATTR_TUNNEL)
      REQUIRED_ATTRIBUTE(child,ATTR_NAME)
      String name = child->getAttribute(ATTR_NAME);
      String select = child->getAttribute(ATTR_SELECT);
      String tunnel = child->getAttribute(ATTR_TUNNEL);
      String as = child->getAttribute(ATTR_AS); // FIXME: support

      if ((XSLT_INSTR_APPLY_TEMPLATES != sn->m_type) &&
          (XSLT_INSTR_APPLY_IMPORTS != sn->m_type) &&
          (XSLT_INSTR_CALL_TEMPLATE != sn->m_type) &&
          (XSLT_INSTR_NEXT_MATCH != sn->m_type))
        return invalid_element(ei,filename,child);

      QName qn = QName::parse(name);
      Expression *selectExpr = NULL;
      if (!select.isNull() &&
          (NULL == (selectExpr = get_expr(ei,filename,child->m_line,"select",select,0))))
        return -1;
      bool tun = false;
      if (!tunnel.isNull() && (tunnel == "yes"))
        tun = true;

      WithParamExpr *new_sn = new WithParamExpr(qn,selectExpr);
      add_node(&param_ptr,new_sn,filename,child);

      if (tun)
        new_sn->m_flags |= FLAG_TUNNEL;

      CHECK_CALL(parse_sequence_constructors(new_sn,child->firstChild(),ei,filename))
    }
    else if (child->m_ident == ELEM_SORT) {
      ALLOWED_ATTRIBUTES(child,&ATTR_SELECT,&ATTR_LANG,&ATTR_ORDER,&ATTR_COLLATION,
                         &ATTR_STABLE,&ATTR_CASE_ORDER,&ATTR_DATA_TYPE)
      String select = child->getAttribute(ATTR_SELECT);
      String lang = child->getAttribute(ATTR_LANG);             // FIXME: support
      String order = child->getAttribute(ATTR_ORDER);           // FIXME: support
      String collation = child->getAttribute(ATTR_COLLATION);   // FIXME: support
      String stable = child->getAttribute(ATTR_STABLE);         // FIXME: support
      String caseOrder = child->getAttribute(ATTR_CASE_ORDER);  // FIXME: support
      String dataType = child->getAttribute(ATTR_DATA_TYPE);    // FIXME: support

      // FIXME: i think we are supposed to ensure that the <sort> elements are the first children
      // of whatever parent they're in - see for example perform-sort which describves its content
      // as being (xsl:sort+, sequence-constructor)

      if ((XSLT_INSTR_APPLY_TEMPLATES != sn->m_type) &&
          (XSLT_INSTR_FOR_EACH != sn->m_type) &&
          (XSLT_INSTR_FOR_EACH_GROUP != sn->m_type) &&
          (XSLT_INSTR_PERFORM_SORT != sn->m_type))
        return invalid_element(ei,filename,child);

      Expression *selectExpr = NULL;

      if (!select.isNull() &&
          (NULL == (selectExpr = get_expr(ei,filename,child->m_line,"select",select,0))))
        return -1;

      Statement *new_sn = new SortExpr(selectExpr);
      add_node(&sort_ptr,new_sn,filename,child);

      CHECK_CALL(parse_sequence_constructors(new_sn,child->firstChild(),ei,filename))
    }
    else if (child->m_ident.m_ns == XSLT_NAMESPACE) {
      return xslt_invalid_element(ei,filename,child);
    }
    else if (NODE_ELEMENT == child->m_type) {
      Statement **child2_ptr;

      // literal result element
      ElementExpr *new_sn = new ElementExpr(NULL,true,true);
      add_node(&child_ptr,new_sn,filename,child);
      child2_ptr = &new_sn->m_child;

      // FIXME: ensure that the prefix is ok to use in the result document... i.e. the
      // prefix -> namespace mapping must be established in the result document
      new_sn->m_qn = child->m_qn;




      // message("BEGIN Parsing literal result element %s\n",c->name);

      int level = 0;
      for (Node *tn = child; tn; tn = tn->parent()) {
        for (xmlNsPtr ns = tn->m_xn->nsDef; ns; ns = ns->next) {
          if ((ns->href != XSLT_NAMESPACE) && !shouldExcludePrefix(child,ns->prefix)) {
            NamespaceExpr *nsnode = new NamespaceExpr(new StringExpr(ns->prefix),
                                                      String::null(),
                                                      new StringExpr(ns->href));
            nsnode->m_implicit = true;
            add_node(&child2_ptr,nsnode,filename,child);
          }
        }
        level++;
      }

      // message("END Parsing literal result element\n");

      // copy attributes

      for (Iterator<Node*> ait = child->m_attributes; ait.haveCurrent(); ait++) {
        Node *attr = *ait;
        String val = attr->m_value;

        Expression *selectExpr = NULL;

        selectExpr = get_expr_attr(ei,filename,child->m_line,attr->m_ident.m_name,val);
        if (NULL == selectExpr)
          return -1;

        AttributeExpr *anode = new AttributeExpr(NULL,selectExpr,true);
        add_node(&child2_ptr,anode,filename,child);
        anode->m_qn.m_localPart = attr->m_ident.m_name;
//      FIXME: copy prefix
//        if (attr->m_xn->ns && attr->m_xn->ns->prefix)
//          anode->m_qn.m_prefix = attr->m_xn->ns->prefix;
      }

      CHECK_CALL(parse_sequence_constructors(new_sn,child->firstChild(),ei,filename))
    }
    else if (NODE_TEXT == child->m_type) {
      // @implements(xslt20:literal-text-nodes-1)
      // test { xslt/eval/literaltext1.test }
      // test { xslt/eval/literaltext2.test }
      // test { xslt/eval/literaltext3.test }
      // test { xslt/eval/literaltext4.test }
      // @end
      // @implements(xslt20:literal-text-nodes-2) @end
      TextExpr *new_sn = new TextExpr(child->m_value,true);
      add_node(&child_ptr,new_sn,filename,child);
    }
  }

  return 0;
}

static int parse_import_include(Statement *parent, Node *n, Error *ei,
                                const String &filename, bool isImport, Statement ***child_ptr)
{
  Statement *import = new IncludeExpr(isImport);
  add_node(child_ptr,import,filename,n);
  TransformExpr *transform = new TransformExpr();
  Statement *p;
  String full_uri;
  import->m_child = transform;

  transform->m_parent = import;
  import->m_parent = parent;

  // FIXME: test with fragments (including another stylsheet in the same document but with
  // a different id)

  // @implements(xslt20:locating-modules-1) status { info } @end
  // @implements(xslt20:locating-modules-2) status { info } @end
  // @implements(xslt20:locating-modules-3)
  // test { xslt/parse/import_fragments.test }
  // test { xslt/parse/import_samedoc1.test }
  // test { xslt/parse/import_samedoc2.test }
  // test { xslt/parse/include_fragments.test }
  // test { xslt/parse/include_samedoc1.test }
  // test { xslt/parse/include_samedoc2.test }
  // @end
  // @implements(xslt20:locating-modules-5) status { info } @end


  // @implements(xslt20:include-1)
  // test { xslt/parse/include_badattr.test }
  // test { xslt/parse/include_otherattr.test }
  // test { xslt/parse/include_badchildren.test }
  // @end
  // @implements(xslt20:import-1)
  // test { xslt/parse/import_badattr.test }
  // test { xslt/parse/import_otherattr.test }
  // test { xslt/parse/import_badchildren.test }
  // @end

  // @implements(xslt20:include-2)
  // test { xslt/parse/include1.test }
  // test { xslt/parse/include2.test }
  // @end
  // @implements(xslt20:import-2)
  // test { xslt/parse/import.test }
  // @end


  // @implements(xslt20:include-5) status { info } @end
  // @implements(xslt20:include-6) status { info } @end

  // @implements(xslt20:import-6) status { info } @end
  // @implements(xslt20:import-7) status { info } @end
  // @implements(xslt20:import-8) status { info } @end
  // @implements(xslt20:import-9) status { info } @end
  // @implements(xslt20:import-10) status { info } @end
  // @implements(xslt20:import-11) status { info } @end
  // @implements(xslt20:import-12) status { info } @end
  // @implements(xslt20:import-13) status { info } @end
  // @implements(xslt20:import-14) status { info } @end

  ASSERT(XSLT_TRANSFORM == parent->m_type);
  TransformExpr *tparent = static_cast<TransformExpr*>(parent);
  ASSERT(!tparent->m_uri.isNull());

  ALLOWED_ATTRIBUTES(n,&ATTR_HREF)

  // @implements(xslt20:include-3)
  // test { xslt/parse/include_nohref.test }
  // @end
  // @implements(xslt20:import-3)
  // test { xslt/parse/import_nohref.test }
  // @end
  REQUIRED_ATTRIBUTE(n,ATTR_HREF)

  String href = n->getAttribute(ATTR_HREF);

  // @implements(xslt20:locating-modules-6)
  // test { xslt/parse/import_baduri.test }
  // test { xslt/parse/include_baduri.test }
  // test { xslt/parse/import_badmethod.test }
  // test { xslt/parse/include_badmethod.test }
  // test { xslt/parse/import_nonexistant.test }
  // test { xslt/parse/include_nonexistant.test }
  // @end
  full_uri = buildURI(href,tparent->m_uri);
  if (full_uri.isNull()) {
    error(ei,filename,n->m_line,"XTSE0165","\"%*\" is not a valid relative or absolute URI",&href);
    return -1;
  }

  for (p = static_cast<Statement*>(transform->m_parent);
       p;
       p = static_cast<Statement*>(p->m_parent)) {
    if ((p->m_type == XSLT_TRANSFORM) && (static_cast<TransformExpr*>(p)->m_uri == full_uri)) {
      // @implements(xslt20:import-15)
      // test { xslt/parse/import_recursive1.test }
      // test { xslt/parse/import_recursive2.test }
      // test { xslt/parse/import_recursive3.test }
      // @end
      if (isImport)
        error(ei,filename,n->m_line,"XTSE0210",
              "It is a static error if a stylesheet module directly or indirectly imports itself");
      // @implements(xslt20:include-7)
      // test { xslt/parse/include_recursive1.test }
      // test { xslt/parse/include_recursive2.test }
      // test { xslt/parse/include_recursive3.test }
      // @end
      else
        error(ei,filename,n->m_line,"XTSE0180",
              "It is a static error if a stylesheet module directly or indirectly includes itself");
      return -1;
    }
  }

  if (0 != parse_xslt_uri(ei,filename,n->m_line,"XTSE0165",full_uri,transform,tparent->m_uri)) {
    return -1;
  }

  Node *c;
  if (NULL != (c = xslt_first_child(n)))
    return xslt_invalid_element(ei,filename,c);

  return 0;
}

static int parse_strip_preserve(Statement *parent, Node *n, Error *ei,
                                const String &filename, bool preserve, Statement ***child_ptr)
{
  SpaceExpr *space = new SpaceExpr(preserve);
  add_node(child_ptr,space,filename,n);

  ALLOWED_ATTRIBUTES(n,&ATTR_ELEMENTS)
  REQUIRED_ATTRIBUTE(n,ATTR_ELEMENTS)

  String elements = n->getAttribute(ATTR_ELEMENTS).collapseWhitespace();
  List<String> unptests = elements.parseList();

  for (Iterator<String> it = unptests; it.haveCurrent(); it++)
    space->m_qnametests.append(new QNameTest((*it)));

  Node *c;
  if (NULL != (c = xslt_first_child(n)))
    return xslt_invalid_element(ei,filename,c);

  return 0;
}

static int parse_decls(Statement *sn, Node *n,
                       Error *ei, const String &filename)
{
  Node *child = xslt_first_child(n);
  Statement **child_ptr = &sn->m_child;

  // @implements(xslt20:stylesheet-element-14) status { info } @end
  // @implements(xslt20:stylesheet-element-15)
  // test { xslt/parse/template_badattr.test }
  // @end

  // FIXME: parse imports here; these come before all other top-level elements
  while (child && (child->m_ident == ELEM_IMPORT)) {
    CHECK_CALL(parse_import_include(sn,child,ei,filename,true,&child_ptr))
    xslt_next_element(&child);
  }

  for (; child; xslt_next_element(&child)) {
    if (child->m_ident == ELEM_INCLUDE) {
      CHECK_CALL(parse_import_include(sn,child,ei,filename,false,&child_ptr))
    }
    else if (child->m_ident == ELEM_ATTRIBUTE_SET) {
      // FIXME
    }
    else if (child->m_ident == ELEM_CHARACTER_MAP) {
      // FIXME
    }
    else if (child->m_ident == ELEM_DECIMAL_FORMAT) {
      // FIXME
    }
    else if (child->m_ident == ELEM_FUNCTION) {

      // @implements(xslt20:stylesheet-functions-1) status { info } @end
      // @implements(xslt20:stylesheet-functions-6) status { info } @end
      // @implements(xslt20:stylesheet-functions-20) status { info } @end
      // @implements(xslt20:stylesheet-functions-26) status { info } @end
      // @implements(xslt20:stylesheet-functions-27) status { info } @end
      // @implements(xslt20:stylesheet-functions-28) status { info } @end

      // @implements(xslt20:stylesheet-functions-2)
      // status { partial }
      // issue { need to test invalid child sequences }
      // test { xslt/parse/function_badattr.test }
      // test { xslt/parse/function_otherattr.test }
      // @end
      ALLOWED_ATTRIBUTES(child,&ATTR_NAME,&ATTR_AS,&ATTR_OVERRIDE)

      // @implements(xslt20:stylesheet-functions-3)
      // test { xslt/parse/function_name1.test }
      // test { xslt/parse/function_name3.test }
      // test { xslt/parse/function_noname.test }
      // @end
      REQUIRED_ATTRIBUTE(child,ATTR_NAME)

      String name = child->getAttribute(ATTR_NAME);
      QName qn = QName::parse(name);
      FunctionExpr *new_sn = new FunctionExpr(qn);
      add_node(&child_ptr,new_sn,filename,child);

      // @implements(xslt20:stylesheet-functions-5)
      // test { xslt/parse/function_name2.test }
      // @end
      if (new_sn->m_qn.m_prefix.isNull())
        return error(ei,filename,child->m_line,"XTSE0740",
                     "A stylesheet function MUST have a prefixed name, to remove any "
                     "risk of a clash with a function in the default function namespace");

      // @implements(xslt20:stylesheet-functions-17)
      // test { function_as1.test }
      // test { function_as2.test }
      // test { function_as3.test }
      // @end
      CHECK_CALL(parse_optional_as(child,&new_sn->m_st,ei,filename))

      // @implements(xslt20:stylesheet-functions-12)
      // status { partial }
      // test { xslt/parse/function_override1.test }
      // test { xslt/parse/function_override2.test }
      // test { xslt/parse/function_override3.test }
      // @end
      new_sn->m_flags |= FLAG_OVERRIDE;

      String override = child->getAttribute(ATTR_OVERRIDE);

      if (!override.isNull()) {
        if (override == "no")
          new_sn->m_flags &= ~FLAG_OVERRIDE;
        else if (override != "yes")
          return invalid_attribute_val(ei,filename,child,ATTR_OVERRIDE);
      }

      // @implements(xslt20:stylesheet-functions-7)
      // status { partial }
      // issue { need tests to ensure content model is enforced }
      // @end

      Node *c2 = xslt_first_child(child);
      while (c2 && (c2->m_ident == ELEM_PARAM)) {
        CHECK_CALL(parse_param(new_sn,c2,ei,filename))
        xslt_next_element(&c2);
      }

      CHECK_CALL(parse_sequence_constructors(new_sn,c2,ei,filename))
    }
    else if (child->m_ident == ELEM_IMPORT_SCHEMA) {
      // FIXME
    }
    else if (child->m_ident == ELEM_KEY) {
      // FIXME
    }
    else if (child->m_ident == ELEM_NAMESPACE_ALIAS) {
      // FIXME
    }
    else if (child->m_ident == ELEM_OUTPUT) {
      int i;

      // @implements(xslt20:serialization-3)
      // test { xslt/parse/output1.test }
      // test { xslt/parse/output2.test }
      // test { xslt/parse/output3.test }
      // test { xslt/parse/output4.test }
      // test { xslt/parse/output_badattr.test }
      // test { xslt/parse/output_badchildren.test }
      // test { xslt/parse/output_otherattr.test }
      // @end

      ALLOWED_ATTRIBUTES(child,
                          &ATTR_NAME,
                          &ATTR_METHOD,
                          &ATTR_BYTE_ORDER_MARK,
                          &ATTR_CDATA_SECTION_ELEMENTS,
                          &ATTR_DOCTYPE_PUBLIC,
                          &ATTR_DOCTYPE_SYSTEM,
                          &ATTR_ENCODING,
                          &ATTR_ESCAPE_URI_ATTRIBUTES,
                          &ATTR_INCLUDE_CONTENT_TYPE,
                          &ATTR_INDENT,
                          &ATTR_MEDIA_TYPE,
                          &ATTR_NORMALIZATION_FORM,
                          &ATTR_OMIT_XML_DECLARATION,
                          &ATTR_STANDALONE,
                          &ATTR_UNDECLARE_PREFIXES,
                          &ATTR_USE_CHARACTER_MAPS,
                          &ATTR_VERSION)

      OutputExpr *new_sn = new OutputExpr();
      add_node(&child_ptr,new_sn,filename,child);

      String name = child->getAttribute(ATTR_NAME);
      if (!name.isNull())
        new_sn->m_qn = QName::parse(name);

      new_sn->m_seroptions = (char**)calloc(SEROPTION_COUNT,sizeof(char*));
      for (i = 0; i < SEROPTION_COUNT; i++) {
        String val = child->getAttribute(NSName(df_seroptions::seroption_names[i]));
        if (!val.isNull())
          new_sn->m_seroptions[i] = val.collapseWhitespace().cstring();
      }

      Node *c2;
      if (NULL != (c2 = xslt_first_child(child)))
        return xslt_invalid_element(ei,filename,c2);
    }
    else if (child->m_ident == ELEM_PARAM) {
      // FIXME
    }
    else if (child->m_ident == ELEM_PRESERVE_SPACE) {
      CHECK_CALL(parse_strip_preserve(sn,child,ei,filename,true,&child_ptr))
    }
    else if (child->m_ident == ELEM_STRIP_SPACE) {
      CHECK_CALL(parse_strip_preserve(sn,child,ei,filename,false,&child_ptr))
    }
    else if (child->m_ident == ELEM_TEMPLATE) {
      // FIXME: support "priority"
      // FIXME: support "mode"
      // FIXME: support "as"

      // FIXME: ensure params get processed

      // @implements(xslt20:rules-1) status { info } @end

      // @implements(xslt20:defining-templates-1)
      // status { partial }
      // issue { need to test invalid child sequences }
      // test { xslt/parse/template_badattr.test }
      // test { xslt/parse/function_otherattr.test }
      // @end
      ALLOWED_ATTRIBUTES(child,&ATTR_MATCH,&ATTR_NAME,&ATTR_PRIORITY,&ATTR_MODE,&ATTR_AS)

      // @implements(xslt20:defining-templates-2) status { info } @end

      // @implements(xslt20:defining-templates-3)
      // test { xslt/parse/template_nomatchname.test }
      // test { xslt/parse/template_matchmode.test }
      // test { xslt/parse/template_matchpriority.test }
      // @end


      if (!child->hasAttribute(ATTR_MATCH) && !child->hasAttribute(ATTR_NAME))
          return error(ei,filename,child->m_line,"XTSE0500",
                       "\"name\" and/or \"match\" attribute required");

      if (!child->hasAttribute(ATTR_MATCH) && child->hasAttribute(ATTR_MODE))
        return conflicting_attributes(ei,filename,child->m_line,"XTSE0500","mode","name");
      if (!child->hasAttribute(ATTR_MATCH) && child->hasAttribute(ATTR_PRIORITY))
        return conflicting_attributes(ei,filename,child->m_line,"XTSE0500","priority","name");

      // @implements(xslt20:defining-templates-4)
      // status { info }
      // @end

      QName qn;
      Expression *matchExpr = NULL;


      String name = child->getAttribute(ATTR_NAME);
      if (!name.isNull())
        qn = QName::parse(name);

      String match = child->getAttribute(ATTR_MATCH);
      if (!match.isNull() &&
          (NULL == (matchExpr = get_expr(ei,filename,child->m_line,"match",match,1))))
        return -1;

      TemplateExpr *new_sn = new TemplateExpr(qn,matchExpr);
      add_node(&child_ptr,new_sn,filename,child);

      // Parse params
      Node *c2 = xslt_first_child(child);
      while (c2 && (c2->m_ident == ELEM_PARAM)) {
        CHECK_CALL(parse_param(new_sn,c2,ei,filename))
        xslt_next_element(&c2);
      }

      CHECK_CALL(parse_sequence_constructors(new_sn,c2,ei,filename))
    }
    else if (child->m_ident == ELEM_VARIABLE) {
      // FIXME
    }
    else if (NODE_ELEMENT == child->m_type) {
      Node *post = child;

      // @implements(xslt20:import-5)
      // test { xslt/parse/import_notatstart.test }
      // @end
      if (child->m_ident == ELEM_IMPORT)
        return error(ei,filename,child->m_line,"XTSE0200",
                     "The xsl:import element children must precede "
                     "all other element children of an xsl:stylesheet element, including any "
                     "xsl:include element children and any user-defined data elements.");


      // @implements(xslt20:user-defined-top-level-2)
      // test { xslt/parse/transform_userdatanons.test }
      // @end
      if (child->m_ident.m_ns.isNull()) {
        return error(ei,filename,child->m_line,
                     "XTSE0130","User-defined data elements only permitted if "
                     "they have a non-null namespace URI");
      }
      else if (child->m_ident.m_ns == XSLT_NAMESPACE)
        return xslt_invalid_element(ei,filename,child);

      // @implements(xslt20:user-defined-top-level-1)
      // test { xslt/parse/transform_userdata.test }
      // @end
      // otherwise; it's a valid user-define data element; just ignore it

      // @implements(xslt20:user-defined-top-level-3) status { info } @end
      // @implements(xslt20:user-defined-top-level-4) status { info } @end
      // @implements(xslt20:user-defined-top-level-5) status { info } @end

      // @implements(xslt20:user-defined-top-level-6)
      // test { xslt/parse/transform_userdataimport.test }
      // @end
      xslt_next_element(&post);
      while (post) {
        if (post->m_ident == ELEM_IMPORT) {
          return error(ei,filename,child->m_line,"XTSE0140",
                       "A user-defined data element must not precede an import element");
        }
        xslt_next_element(&post);
      }

    }
    // FIXME: check for other stuff that can appear here like <variable> and <param>
    // FIXME: support "user-defined data elements"; I think we also need to make these
    // accessible to code within the stylesheet (i.e. put them in a DOM tree)
  }

  return 0;
}

static int parse_xslt_module(TransformExpr *sroot, Node *n,
                             Error *ei, const String &filename)
{
  sroot->m_uri = filename;

  // @implements(xslt20:stylesheet-element-3) @end

  // @implements(xslt20:stylesheet-element-4)
  // test { xslt/parse/transform_noversion.test }
  // @end
  REQUIRED_ATTRIBUTE(n,ATTR_VERSION)

  // @implements(xslt20:stylesheet-element-5) status { deferred } @end
  // @implements(xslt20:stylesheet-element-6) status { deferred } @end
  // @implements(xslt20:stylesheet-element-7) status { deferred } @end
  // @implements(xslt20:stylesheet-element-8) status { deferred } @end
  // @implements(xslt20:stylesheet-element-9) status { deferred } @end
  // @implements(xslt20:stylesheet-element-10) status { deferred } @end
  // @implements(xslt20:stylesheet-element-11) status { deferred } @end
  // @implements(xslt20:stylesheet-element-12) status { info } @end
  // @implements(xslt20:stylesheet-element-13) status { info } @end
  // @implements(xslt20:stylesheet-element-16) status { info } @end
  // @implements(xslt20:stylesheet-element-17) status { info } @end

  // @implements(xslt20:default-collation-attribute-1) status { deferred } @end
  // @implements(xslt20:default-collation-attribute-2) status { deferred } @end
  // @implements(xslt20:default-collation-attribute-3) status { deferred } @end
  // @implements(xslt20:default-collation-attribute-4) status { deferred } @end
  // @implements(xslt20:default-collation-attribute-5) status { deferred } @end
  // @implements(xslt20:default-collation-attribute-6) status { deferred } @end

  CHECK_CALL(parse_decls(sroot,n,ei,filename))
  add_ns_defs(sroot,n);
  return 0;
}

int parse_xslt_relative_uri(Error *ei, const String &filename, int line, const char *errname,
                            const String &base_uri, const String &uri, TransformExpr *sroot)
{
  String full_uri;

  full_uri = buildURI(uri,base_uri);
  if (full_uri.isNull())
    return error(ei,filename,line,errname,"\"%*\" is not a valid relative or absolute URI",&uri);

  if (0 != parse_xslt_uri(ei,filename,line,errname,full_uri,sroot,base_uri))
    return -1;

  return 0;
}

static int parse_xslt_uri(Error *ei, const String &filename, int line, const char *errname,
                          const String &full_uri, TransformExpr *sroot, const String &refsource)
{
  Node *node = NULL;
  Node *root = NULL;
  CHECK_CALL(retrieve_uri_element(ei,filename,line,errname,full_uri,&node,&root,refsource))


  // FIXME: check version
  // FIXME: support simplified stylsheet modules
  if ((node->m_ident != ELEM_TRANSFORM) &&
      (node->m_ident != ELEM_STYLESHEET)) {
    error(ei,filename,line,errname,"Expected element {%*}%s or {%*}%s",
          &XS_NAMESPACE,"transform",&XS_NAMESPACE,"schema");
    delete root;
    return -1;
  }

  if (0 != parse_xslt_module(sroot,node,ei,full_uri)) {
    delete root;
    return -1;
  }

  delete root;
  return 0;
}
