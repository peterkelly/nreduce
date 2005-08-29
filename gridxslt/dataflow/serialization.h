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

#include "util/stringbuf.h"
#include "util/xmlutils.h"
#include "util/namespace.h"
#include "sequencetype.h"

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

typedef struct df_seroptions df_seroptions;

struct df_seroptions {
  nsname ident;
  int byte_order_mark;
  list *cdata_section_elements;
  char *doctype_public;
  char *doctype_system;
  char *encoding;
  int escape_uri_attributes;
  int include_content_type;
  int indent;
  char *media_type;
  nsname method;
  char *normalization_form;
  int omit_xml_declaration;
  int standalone;
  int undeclare_prefixes;
  list *use_character_maps;
  char *version;
};

int df_parse_seroption(df_seroptions *options, int option, error_info *ei,
                       const char *filename, int line,
                       const char *value, ns_map *namespaces);

df_seroptions *df_seroptions_new(nsname method);
void df_seroptions_free(df_seroptions *options);
df_seroptions *df_seroptions_copy(df_seroptions *options);

int df_serialize(xs_globals *g, df_value *v, stringbuf *buf, df_seroptions *options,
                 error_info *ei);

#ifndef _DATAFLOW_SERIALIZATION_C
const char *seroption_names[SEROPTION_COUNT];
#endif

#endif /* _DATAFLOW_SERIALIZATION_H */
