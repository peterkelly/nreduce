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

#ifndef _DATAFLOW_SERIALIZATION_H
#define _DATAFLOW_SERIALIZATION_H

#include "util/String.h"
#include "util/XMLUtils.h"
#include "util/Namespace.h"
#include "SequenceType.h"

#define SERMETHOD_XML                     0
#define SERMETHOD_HTML                    1
#define SERMETHOD_XHTML                   2
#define SERMETHOD_TEXT                    3

#define NMFORM_NFC                        0
#define NMFORM_NFD                        1
#define NMFORM_NKFC                       2
#define NMFORM_NKFD                       3
#define NMFORM_FULLY_NORMALIZED           4
#define NMFORM_NONE                       5
#define NMFORM_OTHER                      6

#define STANDALONE_YES                    0
#define STANDALONE_NO                     1
#define STANDALONE_OMIT                   2

#define SEROPTION_BYTE_ORDER_MARK         0
#define SEROPTION_CDATA_SECTION_ELEMENTS  1
#define SEROPTION_DOCTYPE_PUBLIC          2
#define SEROPTION_DOCTYPE_SYSTEM          3
#define SEROPTION_ENCODING                4
#define SEROPTION_ESCAPE_URI_ATTRIBUTES   5
#define SEROPTION_INCLUDE_CONTENT_TYPE    6
#define SEROPTION_INDENT                  7
#define SEROPTION_MEDIA_TYPE              8
#define SEROPTION_METHOD                  9
#define SEROPTION_NORMALIZATION_FORM      10
#define SEROPTION_OMIT_XML_DECLARATION    11
#define SEROPTION_STANDALONE              12
#define SEROPTION_UNDECLARE_PREFIXES      13
#define SEROPTION_USE_CHARACTER_MAPS      14
#define SEROPTION_VERSION                 15

#define SEROPTION_COUNT                   16

namespace GridXSLT {

class df_seroptions {
public:
  df_seroptions(const NSName &method);
  ~df_seroptions();

  df_seroptions(const df_seroptions &other) { initFrom(other); }

  int parseOption(int option, GridXSLT::Error *ei,
                  const String &filename, int line,
                  const GridXSLT::String &value, NamespaceMap *namespaces);


  NSName m_ident;
  int m_byte_order_mark;
  List<NSName> m_cdata_section_elements;
  String m_doctype_public;
  String m_doctype_system;
  String m_encoding;
  int m_escape_uri_attributes;
  int m_include_content_type;
  int m_indent;
  String m_media_type;
  NSName m_method;
  String m_normalization_form;
  int m_omit_xml_declaration;
  int m_standalone;
  int m_undeclare_prefixes;
  List<NSName> m_use_character_maps;
  String m_version;

  static const char *seroption_names[SEROPTION_COUNT];

private:
  void initFrom(const df_seroptions &other);

};

struct space_decl {
  NSNameTest *nnt;
  int importpred;
  double priority;
  int preserve;
};

int df_serialize(Value &v, stringbuf *buf, df_seroptions *options, Error *ei);

void df_strip_spaces(Node *n, list *space_decls);
void df_namespace_fixup(Node *n);

int space_decl_preserve(list *space_decls, const NSName &elemname);
space_decl *space_decl_copy(space_decl *decl);
void space_decl_free(space_decl *decl);

};

#endif /* _DATAFLOW_SERIALIZATION_H */
