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

#include "output.h"
#include "parse.h"
#include "dataflow/serialization.h"
#include "util/Namespace.h"
#include "util/stringbuf.h"
#include "util/macros.h"
#include "util/debug.h"
#include <libxml/xmlwriter.h>
#include <libxml/xmlIO.h>
#include <libxml/parser.h>
#include <libxml/tree.h>
#include <libxml/uri.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>

using namespace GridXSLT;

void output_xslt_sequence_constructor(xmlTextWriter *writer, Statement *node);

static void expr_attr(xmlTextWriter *writer, char *attrname, Expression *expr, int brackets)
{
  StringBuffer buf;

  if (brackets) {
    if (XPATH_EXPR_STRING_LITERAL == expr->m_type) {
      /* expression can just be used directly as a value */
      XMLWriter::attribute(writer,attrname,expr->m_strval);
    }
    else {
      buf.append("{");
      expr->serialize(buf,0);
      buf.append("}");
      XMLWriter::attribute(writer,attrname,buf.contents());
    }
  }
  else {
    expr->serialize(buf,0);
    XMLWriter::attribute(writer,attrname,buf.contents());
  }
}

static void xslt_start_element(xmlTextWriter *writer, Statement *sn, const char *name)
{
  char *elemname = (char*)alloca(strlen("xsl:")+strlen(name));
  sprintf(elemname,"xsl:%s",name);
  xmlTextWriterStartElement(writer,elemname);

  for (Iterator<ns_def*> it = sn->m_namespaces->defs; it.haveCurrent(); it++) {
    ns_def *def = *it;
    XMLWriter::attribute(writer,String::format("xmlns:%*",&def->prefix),def->href);
  }
}

static void xslt_end_element(xmlTextWriter *writer)
{
  xmlTextWriterEndElement(writer);
}

void output_xslt_with_param(xmlTextWriter *writer, Statement *wp)
{
  Statement *c;

  ASSERT(XSLT_WITH_PARAM == wp->m_type);
  xslt_start_element(writer,wp,"with-param");

  xml_write_attr(writer,"name","%*",&wp->m_qn);

  /* FIXME: attribute "as" for SequenceType */
  if (wp->m_select)
    expr_attr(writer,"select",wp->m_select,0);
  if (wp->m_flags & FLAG_TUNNEL)
    XMLWriter::attribute(writer,"tunnel","yes");

  for (c = wp->m_child; c; c = c->m_next)
    output_xslt_sequence_constructor(writer,c);
  xslt_end_element(writer);
}

void output_xslt_param(xmlTextWriter *writer, Statement *node)
{
  ASSERT(XSLT_PARAM == node->m_type);

  xslt_start_element(writer,node,"param");

  xml_write_attr(writer,"name","%*",&node->m_qn);

  /* as */
  if (!node->m_st.isNull()) {
    StringBuffer buf;
    node->m_st.printXPath(buf,node->m_namespaces);
    XMLWriter::attribute(writer,"as",buf.contents());
  }

  /* flags */
  if (node->m_flags & FLAG_REQUIRED)
    XMLWriter::attribute(writer,"required","yes");
  if (node->m_flags & FLAG_TUNNEL)
    XMLWriter::attribute(writer,"tunnel","yes");

  /* default value */
  if (node->m_select)
    expr_attr(writer,"select",node->m_select,0);
  else if (node->m_child)
    output_xslt_sequence_constructor(writer,node->m_child);
    
  xslt_end_element(writer);

}

void output_xslt_sort(xmlTextWriter *writer, Statement *node)
{
  Statement *c;
  /* FIXME: support for "lang" */
  /* FIXME: support for "order" */
  /* FIXME: support for "collation" */
  /* FIXME: support for "stable" */
  /* FIXME: support for "case-order" */
  /* FIXME: support for "data-type" */
  xslt_start_element(writer,node,"sort");
  if (node->m_select) {
    expr_attr(writer,"select",node->m_select,0);
  }
  else {
    for (c = node->m_child; c; c = c->m_next)
      output_xslt_sequence_constructor(writer,c);
  }
  xslt_end_element(writer);
}

void output_xslt_sequence_constructor(xmlTextWriter *writer, Statement *node)
{
  Statement *c;
  Statement *c2;

  switch (node->m_type) {
  case XSLT_VARIABLE:
    xslt_start_element(writer,node,"variable");
    xml_write_attr(writer,"name","%*",&node->m_qn);
    /* FIXME: attribute "as" for SequenceType */
    if (node->m_select)
      expr_attr(writer,"select",node->m_select,0);
    else if (node->m_child) {
      for (c = node->m_child; c; c = c->m_next)
        output_xslt_sequence_constructor(writer,c);
    }
    xslt_end_element(writer);
    break;
  case XSLT_INSTR_ANALYZE_STRING:
    xslt_start_element(writer,node,"analyze-string");
    expr_attr(writer,"select",node->m_select,0);
    expr_attr(writer,"regex",node->m_expr1,1);
    if (node->m_expr2)
      expr_attr(writer,"flags",node->m_expr2,1);
    for (c = node->m_child; c; c = c->m_next) {

      if (XSLT_MATCHING_SUBSTRING == c->m_type) {
        xslt_start_element(writer,c,"matching-substring");
        for (c2 = c->m_child; c2; c2 = c2->m_next)
          output_xslt_sequence_constructor(writer,c2);
        xslt_end_element(writer);
      }
      else if (XSLT_NON_MATCHING_SUBSTRING == c->m_type) {
        xslt_start_element(writer,c,"non-matching-substring");
        for (c2 = c->m_child; c2; c2 = c2->m_next)
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
    for (c = node->m_param; c; c = c->m_next)
      output_xslt_with_param(writer,c);
    xslt_end_element(writer);
    break;
  case XSLT_INSTR_APPLY_TEMPLATES:
    /* FIXME: support for multiple modes, as well as #default and #all */
    xslt_start_element(writer,node,"apply-templates");
    if (node->m_select)
      expr_attr(writer,"select",node->m_select,0);
    if (!node->m_mode.isNull())
      xml_write_attr(writer,"mode","%*",&node->m_mode);
    for (c = node->m_param; c; c = c->m_next)
      output_xslt_with_param(writer,c);
    for (c = node->m_sort; c; c = c->m_next)
      output_xslt_sort(writer,c);
    xslt_end_element(writer);
    break;
  case XSLT_INSTR_ATTRIBUTE:
    /* FIXME: support for "namespace" attribute */
    /* FIXME: support for "separator" attribute */
    /* FIXME: support for "type" attribute */
    /* FIXME: support for "validation" attribute */
    xslt_start_element(writer,node,"attribute");
    if (!node->m_qn.isNull())
      xml_write_attr(writer,"name","%*",&node->m_qn);
    else
      expr_attr(writer,"name",node->m_name_expr,1);
    if (node->m_select)
      expr_attr(writer,"select",node->m_select,0);
    for (c = node->m_child; c; c = c->m_next)
      output_xslt_sequence_constructor(writer,c);
    xslt_end_element(writer);
    break;
  case XSLT_INSTR_CALL_TEMPLATE:
    xslt_start_element(writer,node,"call-template");
    xml_write_attr(writer,"name","%*",&node->m_qn);
    for (c = node->m_param; c; c = c->m_next)
      output_xslt_with_param(writer,c);
    xslt_end_element(writer);
    break;
  case XSLT_INSTR_CHOOSE:
    xslt_start_element(writer,node,"choose");
    for (c = node->m_child; c; c = c->m_next) {
      if (XSLT_WHEN == c->m_type) {
        xslt_start_element(writer,c,"when");
        ASSERT(c->m_select);
        expr_attr(writer,"test",c->m_select,0);
        for (c2 = c->m_child; c2; c2 = c2->m_next)
          output_xslt_sequence_constructor(writer,c2);
        xslt_end_element(writer);
      }
      else if (XSLT_OTHERWISE == c->m_type) {
        xslt_start_element(writer,c,"otherwise");
        for (c2 = c->m_child; c2; c2 = c2->m_next)
          output_xslt_sequence_constructor(writer,c2);
        xslt_end_element(writer);
      }
      else {
        /* FIXME: return with error, don't exit! */
        fmessage(stderr,"Unexpected element inside <xsl:transform>: %d\n",c->m_type);
        exit(1);
      }
    }
    xslt_end_element(writer);
    break;
  case XSLT_INSTR_COMMENT:
    xslt_start_element(writer,node,"comment");
    if (node->m_select)
      expr_attr(writer,"select",node->m_select,0);
    for (c = node->m_child; c; c = c->m_next)
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
    for (c = node->m_child; c; c = c->m_next)
      output_xslt_sequence_constructor(writer,c);
    xslt_end_element(writer);
    break;
  case XSLT_INSTR_COPY_OF:
    /* FIXME: support "copy-namespaces" */
    /* FIXME: support "type" */
    /* FIXME: support "validation" */
    xslt_start_element(writer,node,"copy-of");
    expr_attr(writer,"select",node->m_select,0);
    xslt_end_element(writer);
    break;
  case XSLT_INSTR_ELEMENT:
    /* FIXME: support "namespace" */
    /* FIXME: support "inherit-namespace" */
    /* FIXME: support "use-attribute-sets" */
    /* FIXME: support "type" */
    /* FIXME: support "validation" */

    c = node->m_child;

    if (node->m_literal) {
      stringbuf *buf = stringbuf_new();
      stringbuf_format(buf,"%*",&node->m_qn);
      xmlTextWriterStartElement(writer,buf->data);
      stringbuf_free(buf);

      while (c && (XSLT_INSTR_ATTRIBUTE == c->m_type) && c->m_literal) {
        stringbuf *qnstr = stringbuf_new();
        stringbuf_format(qnstr,"%*",&c->m_qn);
        expr_attr(writer,qnstr->data,c->m_select,1);
        stringbuf_free(qnstr);
        c = c->m_next;
      }
    }
    else {
      xmlTextWriterStartElement(writer,"xsl:element");
      expr_attr(writer,"name",node->m_name_expr,1);
    }

    for (; c; c = c->m_next)
      output_xslt_sequence_constructor(writer,c);
    xmlTextWriterEndElement(writer);
    break;
  case XSLT_INSTR_FALLBACK:
    xslt_start_element(writer,node,"fallback");
    for (c = node->m_child; c; c = c->m_next)
      output_xslt_sequence_constructor(writer,c);
    xslt_end_element(writer);
    break;
  case XSLT_INSTR_FOR_EACH:
    xslt_start_element(writer,node,"for-each");
    expr_attr(writer,"select",node->m_select,0);
    for (c = node->m_sort; c; c = c->m_next)
      output_xslt_sort(writer,c);
    for (c = node->m_child; c; c = c->m_next)
      output_xslt_sequence_constructor(writer,c);
    xslt_end_element(writer);
    break;
  case XSLT_INSTR_FOR_EACH_GROUP:
    /* FIXME: support "collation" */
    /* FIXME: enforce the restriction that for starting-with and ending-with, the result
       must be a pattern (xslt section 5.5.2) */
    xslt_start_element(writer,node,"for-each-group");
    expr_attr(writer,"select",node->m_select,0);
    switch (node->m_gmethod) {
    case XSLT_GROUPING_BY:
      expr_attr(writer,"group-by",node->m_expr1,0);
      break;
    case XSLT_GROUPING_ADJACENT:
      expr_attr(writer,"group-adjacent",node->m_expr1,0);
      break;
    case XSLT_GROUPING_STARTING_WITH:
      expr_attr(writer,"group-starting-with",node->m_expr1,0);
      break;
    case XSLT_GROUPING_ENDING_WITH:
      expr_attr(writer,"group-ending-with",node->m_expr1,0);
      break;
    default:
      ASSERT(0);
      break;
    } 
    for (c = node->m_sort; c; c = c->m_next)
      output_xslt_sort(writer,c);
    for (c = node->m_child; c; c = c->m_next)
      output_xslt_sequence_constructor(writer,c);
    xslt_end_element(writer);
    break;
  case XSLT_INSTR_IF:
    xslt_start_element(writer,node,"if");
    expr_attr(writer,"test",node->m_select,0);
    for (c = node->m_child; c; c = c->m_next)
      output_xslt_sequence_constructor(writer,c);
    xslt_end_element(writer);
    break;
  case XSLT_INSTR_MESSAGE:
    xslt_start_element(writer,node,"message");
    if (node->m_select)
      expr_attr(writer,"select",node->m_select,0);
    if (node->m_flags & FLAG_TERMINATE) {
      if (node->m_expr1)
        expr_attr(writer,"terminate",node->m_expr1,1);
      else
        XMLWriter::attribute(writer,"terminate","yes");
    }
    for (c = node->m_child; c; c = c->m_next)
      output_xslt_sequence_constructor(writer,c);
    xslt_end_element(writer);
    break;
  case XSLT_INSTR_NAMESPACE:
    xslt_start_element(writer,node,"namespace");
    if (node->m_name_expr)
      expr_attr(writer,"name",node->m_name_expr,1);
    else
      XMLWriter::attribute(writer,"name",node->m_strval);
    if (node->m_select)
      expr_attr(writer,"select",node->m_select,0);
    for (c = node->m_child; c; c = c->m_next)
      output_xslt_sequence_constructor(writer,c);
    xslt_end_element(writer);
    break;
  case XSLT_INSTR_NEXT_MATCH:
    xslt_start_element(writer,node,"next-match");
    for (c = node->m_param; c; c = c->m_next)
      output_xslt_with_param(writer,c);
    for (c = node->m_child; c; c = c->m_next)
      output_xslt_sequence_constructor(writer,c);
    xslt_end_element(writer);
    break;
  case XSLT_INSTR_NUMBER:
    /* FIXME */
    break;
  case XSLT_INSTR_PERFORM_SORT:
    xslt_start_element(writer,node,"perform-sort");
    if (node->m_select)
      expr_attr(writer,"select",node->m_select,0);
    for (c = node->m_sort; c; c = c->m_next)
      output_xslt_sort(writer,c);
    for (c = node->m_child; c; c = c->m_next)
      output_xslt_sequence_constructor(writer,c);
    xslt_end_element(writer);
    break;
  case XSLT_INSTR_PROCESSING_INSTRUCTION:
    xslt_start_element(writer,node,"processing-instruction");
    if (!node->m_qn.isNull())
      xml_write_attr(writer,"name","%*",&node->m_qn);
    else
      expr_attr(writer,"name",node->m_name_expr,1);
    if (node->m_select)
      expr_attr(writer,"select",node->m_select,0);
    for (c = node->m_child; c; c = c->m_next)
      output_xslt_sequence_constructor(writer,c);
    xslt_end_element(writer);
    break;
  case XSLT_INSTR_RESULT_DOCUMENT:
    /* FIXME */
    break;
  case XSLT_INSTR_SEQUENCE:
    xslt_start_element(writer,node,"sequence");
    expr_attr(writer,"select",node->m_select,0);
    for (c = node->m_child; c; c = c->m_next)
      output_xslt_sequence_constructor(writer,c);
    xslt_end_element(writer);
    break;
  case XSLT_INSTR_TEXT:
    /* FIXME: support "disable-output-escaping" */
    if (node->m_literal) {
      XMLWriter::string(writer,node->m_strval);
    }
    else {
      xslt_start_element(writer,node,"text");
      XMLWriter::string(writer,node->m_strval);
      xslt_end_element(writer);
    }
    break;
  case XSLT_INSTR_VALUE_OF:
    xslt_start_element(writer,node,"value-of");
    if (node->m_select)
      expr_attr(writer,"select",node->m_select,0);
    if (node->m_expr1)
      expr_attr(writer,"separator",node->m_expr1,1);
    for (c = node->m_child; c; c = c->m_next)
      output_xslt_sequence_constructor(writer,c);
    xslt_end_element(writer);
    break;
  /* FIXME: allow result-elements here */
  default:
    break;
  }
}

static char *QNameTest_list_str(List<QNameTest*> &QNameTests)
{
  char *str;
  stringbuf *buf = stringbuf_new();

  for (Iterator<QNameTest*> it = QNameTests; it.haveCurrent(); it++) {
    QNameTest *qt = *it;
    if (qt->wcprefix && qt->wcname)
      stringbuf_format(buf,"*");
    else if (qt->wcprefix)
      stringbuf_format(buf,"*:%*",&qt->qn.m_localPart);
    else if (qt->wcname)
      stringbuf_format(buf,"%*:*",&qt->qn.m_prefix);
    else
      stringbuf_format(buf,"%*",&qt->qn);
    if (it.haveNext())
      stringbuf_format(buf," ");
  }

  str = strdup(buf->data);
  stringbuf_free(buf);
  return str;
}

/*

Section 3.6: Stylesheet Element
Content: (xsl:import*, other-declarations)

Note: <xsl:stylesheet> and <xsl:transform> have the same meaning. In the syntax
tree they are treated as XSLT_TRANSFORM

*/
void output_xslt(FILE *f, Statement *node)
{
  Statement *sn;
  Statement *c;
  xmlOutputBuffer *buf = xmlOutputBufferCreateFile(f,NULL);
  xmlTextWriter *writer = xmlNewTextWriter(buf);

  ASSERT(XSLT_TRANSFORM == node->m_type);

  xmlTextWriterSetIndent(writer,1);
  xmlTextWriterSetIndentString(writer,"  ");
  xmlTextWriterStartDocument(writer,NULL,NULL,NULL);
  xslt_start_element(writer,node,"transform");
  XMLWriter::attribute(writer,"version","2.0");

  for (sn = node->m_child; sn && (XSLT_IMPORT == sn->m_type); sn = sn->m_next) {
    char *rel = get_relative_uri(sn->m_child->m_uri,node->m_uri);
    xslt_start_element(writer,sn,"import");
    XMLWriter::attribute(writer,"href",rel);
    xslt_end_element(writer);
    free(rel);
  }

  for (; sn; sn = sn->m_next) {
    switch (sn->m_type) {
    case XSLT_DECL_INCLUDE: {
      char *rel = get_relative_uri(sn->m_child->m_uri,node->m_uri);
      xslt_start_element(writer,sn,"include");
      XMLWriter::attribute(writer,"href",rel);
      xslt_end_element(writer);
      free(rel);
      break;
    }
    case XSLT_DECL_ATTRIBUTE_SET:
      break;
    case XSLT_DECL_CHARACTER_MAP:
      break;
    case XSLT_DECL_DECIMAL_FORMAT:
      break;
    case XSLT_DECL_FUNCTION: {
      xslt_start_element(writer,sn,"function");
      xml_write_attr(writer,"name","%*",&sn->m_qn);
      if (!(sn->m_flags & FLAG_OVERRIDE))
        XMLWriter::attribute(writer,"override","no");

      if (!sn->m_st.isNull()) {
        StringBuffer buf;
        sn->m_st.printXPath(buf,sn->m_namespaces);
        XMLWriter::attribute(writer,"as",buf.contents());
      }

      for (c = sn->m_param; c; c = c->m_next)
        output_xslt_param(writer,c);
      for (c = sn->m_child; c; c = c->m_next)
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
    case XSLT_DECL_OUTPUT: {
      int i;
      xslt_start_element(writer,sn,"output");
      if (!sn->m_qn.isNull())
        xml_write_attr(writer,"name","%*",&sn->m_qn);
      for (i = 0; i < SEROPTION_COUNT; i++)
        if (NULL != sn->m_seroptions[i])
          xml_write_attr(writer,df_seroptions::seroption_names[i],"%s",sn->m_seroptions[i]);
      xslt_end_element(writer);
      break;
    }
    case XSLT_PARAM:
      break;
    case XSLT_DECL_PRESERVE_SPACE: {
      char *elements = QNameTest_list_str(sn->m_QNameTests);
      xslt_start_element(writer,sn,"preserve-space");
      xml_write_attr(writer,"elements",elements);
      xslt_end_element(writer);
      free(elements);
      break;
    }
    case XSLT_DECL_STRIP_SPACE: {
      char *elements = QNameTest_list_str(sn->m_QNameTests);
      xslt_start_element(writer,sn,"strip-space");
      xml_write_attr(writer,"elements",elements);
      xslt_end_element(writer);
      free(elements);
      break;
    }
    case XSLT_DECL_TEMPLATE:
      /* FIXME: support "priority" */
      /* FIXME: support "mode" */
      /* FIXME: support "as" */
      xslt_start_element(writer,sn,"template");
      if (!sn->m_qn.isNull())
        xml_write_attr(writer,"name","%*",&sn->m_qn);
      if (sn->m_select)
        expr_attr(writer,"match",sn->m_select,0);
      for (c = sn->m_param; c; c = c->m_next)
        output_xslt_param(writer,c);
      for (c = sn->m_child; c; c = c->m_next)
        output_xslt_sequence_constructor(writer,c);
      xslt_end_element(writer);
      break;
    case XSLT_VARIABLE:
      break;
    default:
      /* FIXME: return with error, don't exit! */
      fmessage(stderr,"Unexpected element inside <xsl:transform>: %d\n",sn->m_type);
      exit(1);
      break;
    }
  }

  xslt_end_element(writer);
  xmlTextWriterEndDocument(writer);
  xmlTextWriterFlush(writer);
  xmlFreeTextWriter(writer);
}
