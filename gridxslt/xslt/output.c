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
#include "util/namespace.h"
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
#include <assert.h>
#include <ctype.h>

void output_xslt_sequence_constructor(xmlTextWriter *writer, xl_snode *node);

static void expr_attr(xmlTextWriter *writer, char *attrname, xp_expr *expr, int brackets)
{
  stringbuf *buf = stringbuf_new();

  if (brackets) {
    if (XPATH_EXPR_STRING_LITERAL == expr->type) {
      /* expression can just be used directly as a value */
      xmlTextWriterWriteAttribute(writer,attrname,expr->strval);
    }
    else {
      stringbuf_format(buf,"{");
      xp_expr_serialize(buf,expr,0);
      stringbuf_format(buf,"}");
      xmlTextWriterWriteAttribute(writer,attrname,buf->data);
    }
  }
  else {
    xp_expr_serialize(buf,expr,0);
    xmlTextWriterWriteAttribute(writer,attrname,buf->data);
  }

  stringbuf_free(buf);
}

static void xslt_start_element(xmlTextWriter *writer, xl_snode *sn, const char *name)
{
  list *l;
  char *elemname = (char*)alloca(strlen("xsl:")+strlen(name));
  sprintf(elemname,"xsl:%s",name);
  xmlTextWriterStartElement(writer,elemname);

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

  xml_write_attr(writer,"name","%#q",wp->qn);

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

  xml_write_attr(writer,"name","%#q",node->qn);

  /* as */
  if (NULL != node->seqtype) {
    stringbuf *buf = stringbuf_new();
    df_seqtype_print_xpath(buf,node->seqtype,node->namespaces);
    xmlTextWriterWriteAttribute(writer,"as",buf->data);
    stringbuf_free(buf);
  }

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
    xml_write_attr(writer,"name","%#q",node->qn);
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
    if (!qname_isnull(node->mode))
      xml_write_attr(writer,"mode","%#q",node->mode);
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
    if (!qname_isnull(node->qn))
      xml_write_attr(writer,"name","%#q",node->qn);
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
    xml_write_attr(writer,"name","%#q",node->qn);
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

    c = node->child;

    if (node->literal) {
      stringbuf *buf = stringbuf_new();
      stringbuf_format(buf,"%#q",node->qn);
      xmlTextWriterStartElement(writer,buf->data);
      stringbuf_free(buf);

      while (c && (XSLT_INSTR_ATTRIBUTE == c->type) && c->literal) {
        stringbuf *qnstr = stringbuf_new();
        stringbuf_format(qnstr,"%#q",c->qn);
        expr_attr(writer,qnstr->data,c->select,1);
        stringbuf_free(qnstr);
        c = c->next;
      }
    }
    else {
      xmlTextWriterStartElement(writer,"xsl:element");
      expr_attr(writer,"name",node->name_expr,1);
    }

    for (; c; c = c->next)
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
    if (!qname_isnull(node->qn))
      xml_write_attr(writer,"name","%#q",node->qn);
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
    if (node->literal) {
      xmlTextWriterWriteFormatString(writer,"%s",node->strval);
    }
    else {
      xslt_start_element(writer,node,"text");
      xmlTextWriterWriteFormatString(writer,"%s",node->strval);
      xslt_end_element(writer);
    }
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

static char *qnametest_list_str(list *qnametests)
{
  char *str;
  stringbuf *buf = stringbuf_new();
  list *l;

  for (l = qnametests; l; l = l->next) {
    qnametest *qt = (qnametest*)l->data;
    if (qt->wcprefix && qt->wcname)
      stringbuf_format(buf,"*");
    else if (qt->wcprefix)
      stringbuf_format(buf,"*:%s",qt->qn.localpart);
    else if (qt->wcname)
      stringbuf_format(buf,"%s:*",qt->qn.prefix);
    else
      stringbuf_format(buf,"%#q",qt->qn);
    if (l->next)
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
    char *rel = get_relative_uri(sn->child->uri,node->uri);
    xslt_start_element(writer,sn,"import");
    xmlTextWriterWriteAttribute(writer,"href",rel);
    xslt_end_element(writer);
    free(rel);
  }

  for (; sn; sn = sn->next) {
    switch (sn->type) {
    case XSLT_DECL_INCLUDE: {
      char *rel = get_relative_uri(sn->child->uri,node->uri);
      xslt_start_element(writer,sn,"include");
      xmlTextWriterWriteAttribute(writer,"href",rel);
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
      xml_write_attr(writer,"name","%#q",sn->qn);
      if (!(sn->flags & FLAG_OVERRIDE))
        xmlTextWriterWriteAttribute(writer,"override","no");

      if (NULL != sn->seqtype) {
        stringbuf *buf = stringbuf_new();
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
    case XSLT_DECL_OUTPUT: {
      int i;
      xslt_start_element(writer,sn,"output");
      if (!qname_isnull(sn->qn))
        xml_write_attr(writer,"name","%#q",sn->qn);
      for (i = 0; i < SEROPTION_COUNT; i++)
        if (NULL != sn->seroptions[i])
          xml_write_attr(writer,seroption_names[i],"%s",sn->seroptions[i]);
      xslt_end_element(writer);
      break;
    }
    case XSLT_PARAM:
      break;
    case XSLT_DECL_PRESERVE_SPACE: {
      char *elements = qnametest_list_str(sn->qnametests);
      xslt_start_element(writer,sn,"preserve-space");
      xml_write_attr(writer,"elements",elements);
      xslt_end_element(writer);
      free(elements);
      break;
    }
    case XSLT_DECL_STRIP_SPACE: {
      char *elements = qnametest_list_str(sn->qnametests);
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
      if (!qname_isnull(sn->qn))
        xml_write_attr(writer,"name","%#q",sn->qn);
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
