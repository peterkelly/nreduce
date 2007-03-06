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
 * $Id: Compilation.cpp 216 2005-11-13 06:46:07Z pmkelly $
 *
 */

#include "Compilation.h"
#include "Builtins.h"
#include "util/List.h"
#include "util/Macros.h"
#include "util/Debug.h"
#include "dataflow/Optimization.h"
#include "dataflow/Validity.h"
#include <string.h>

using namespace GridXSLT;

void TransformExpr::genELC(StringBuffer *buf)
{
  Statement *sn;

  for (sn = m_child; sn; sn = static_cast<Statement*>(xl_next_decl(sn))) {
    switch (sn->m_type) {
    case XSLT_DECL_FUNCTION:
      sn->genELC(buf);
      break;
    case XSLT_DECL_TEMPLATE:
      sn->genELC(buf);
      break;
    default:
      break;
    }
  }
}

void genELCSequence(StringBuffer *buf, Statement *parent)
{
  Statement *s;
  int count = 0;

  if (NULL == parent->m_child) {
    buf->format("nil");
    return;
  }

  if (NULL == parent->m_child->m_next) {
    parent->m_child->genELC(buf);
    return;
  }

  for (s = parent->m_child; s; s = s->m_next) {
    if (XSLT_VARIABLE == s->m_type) {
      if (NULL == ((VariableExpr*)s)->m_select) {
        fmessage(stderr,"Variables with content not supported yet\n");
        abort();
      }

      buf->format("(letrec %* = ",&s->m_qn.m_localPart);
      ((VariableExpr*)s)->m_select->genELC(buf);
      buf->format(" in ");
      count++;
    }
    else {
      buf->format("(append ");
      s->genELC(buf);
      buf->format(" ");
      count++;
    }
  }
  buf->format("nil");
  while (0 < count--)
    buf->format(")");
}

void TemplateExpr::genELC(StringBuffer *buf)
{
  buf->format("top citem cpos csize =\n");
  genELCSequence(buf,this);
  buf->format("\n\n");
}

void NamespaceExpr::genELC(StringBuffer *buf)
{
  buf->format("// namespace\n");

  buf->format("(cons\n");
  buf->format("(xml:mknamespace\n");

  buf->format("(xslt:consimple ");
  if (m_select)
    m_select->genELC(buf);
  else
    genELCSequence(buf,this);
  buf->format(")\n");

  if (m_name_expr) {
    buf->format("(xslt:consimple ");
    m_name_expr->genELC(buf);
    buf->format(")");
  }
  else {
    String escaped = escape_str(m_strval);
    buf->format("\"*%\"",&escaped);
  }

  buf->format(") nil)");
}

void StepExpr::genELC(StringBuffer *buf)
{
  // Note: node sequences are handled differently from atomic value sequences...
  // see XPath 2.0 section 3.2
  buf->format("(xslt:apmap3 (!citem.!cpos.!csize.\n");
  m_right->genELC(buf);
  buf->format(") ");
  m_left->genELC(buf);
  buf->format(")");
}

// FIXME: check the left/right stuff with these
void FilterExpr::genELC(StringBuffer *buf)
{
  buf->format("(xslt:filter3\n(!citem.!cpos.!csize.xslt:ebv ");
  m_right->genELC(buf);
  buf->format(") ");
  m_left->genELC(buf);
  buf->format(")");
}

void NodeTestExpr::genELC(StringBuffer *buf)
{
  buf->format("(filter ");
  bool found = false;

  if (XPATH_NODE_TEST_NAME == m_nodetest) {
    if (AXIS_ATTRIBUTE == m_axis) {
      if (m_qn.isNull()) {
        buf->format("xslt:attributetest");
      }
      else {
        String name = escape_str(m_qn.m_localPart);
        buf->format("(xslt:attrnametest \"%*\")",&name);
      }
    }
    else {
      if (m_qn.isNull()) {
        buf->format("xslt:elementtest");
      }
      else {
        String name = escape_str(m_qn.m_localPart);
        buf->format("(xslt:nametest \"%*\")",&name);
      }
    }
    found = true;
  }
  else if ((XPATH_NODE_TEST_SEQUENCETYPE == m_nodetest) &&
           m_st.isNode()) {
    buf->format("xslt:anykindtest");
    found = true;
  }
  else if ((XPATH_NODE_TEST_SEQUENCETYPE == m_nodetest) &&
           (SEQTYPE_ITEM == m_st.type())) {
    ItemType *it = m_st.itemType();


    //buf->format("sequence type is %d\n",m_st.type());
    if (ITEM_ELEMENT == it->m_kind) {
      buf->format("xslt:elementtest");
      found = true;
    }
    else if (ITEM_TEXT == it->m_kind) {
      buf->format("xslt:texttest");
      found = true;
    }
    else if (ITEM_ATTRIBUTE == it->m_kind) {
      buf->format("xslt:attributetest");
      found = true;
    }
  }

  if (!found)
    ASSERT(!"unsupported node test");

  buf->format("\n");
  switch (m_axis) {
  case AXIS_CHILD:
    buf->format("(xml:node_children citem)");
    break;
  case AXIS_ATTRIBUTE:
    buf->format("(xml:node_attributes citem)");
    break;
  default:
    ASSERT(!"unsupported axis");
    break;
  }
  buf->format(")");
}

void FunctionExpr::genELC(StringBuffer *buf)
{
  buf->format("%*",&m_qn.m_localPart);
  Statement *p;
  for (p = m_param; p; p = p->m_next)
    buf->format(" %*",&p->m_qn.m_localPart);
  buf->format(" = (letrec citem = nil cpos = nil csize = nil in ");
  genELCSequence(buf,this);
  buf->format(")\n\n");
}

void CallExpr::genELC(StringBuffer *buf)
{
  // FIXME: check that the correct number of parameters is supplied
  buf->format("(%*",&m_qn.m_localPart);
  ActualParamExpr *a;
  for (a = m_left; a; a = a->right()) {
    buf->format(" ");
    a->left()->genELC(buf);
  }
  buf->format(")");
}

void VarRefExpr::genELC(StringBuffer *buf)
{
  buf->format("%*",&m_qn.m_localPart);
}

void VariableExpr::genELC(StringBuffer *buf)
{
  abort();
//   if (NULL == m_select) {
//     fmessage(stderr,"Variables with content not supported yet\n");
//     abort();
//   }

//   buf->format("(letrec %* = ",&m_qn.m_localPart);
//   m_select->genELC(buf);
//   buf->format(" in ");
}

void ParenExpr::genELC(StringBuffer *buf)
{
  m_left->genELC(buf);
}

void SequenceExpr::genELC(StringBuffer *buf)
{
  buf->format("// sequence: ");
  m_left->print(*buf);
  buf->format("\n");

  buf->format("(append ");
  m_left->genELC(buf);
  buf->format("\n");
  m_right->genELC(buf);
  buf->format(")");
}

void ValueOfExpr::genELC(StringBuffer *buf)
{
  buf->format("(cons (cons xml:TYPE_TEXT (xslt:consimple ");
  if (m_select)
    m_select->genELC(buf);
  else
    genELCSequence(buf,this);
  buf->format(")) nil)\n");
}

void UnaryExpr::genELC(StringBuffer *buf)
{
  if (XPATH_UNARY_MINUS == m_type)
    buf->format("(xslt:uminus ");
  else
    buf->format("(xslt:uplus ");
  m_source->genELC(buf);
  buf->format(")");
}
