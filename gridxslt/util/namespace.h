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

#ifndef _UTIL_NAMESPACE_H
#define _UTIL_NAMESPACE_H

#include "list.h"
#include "xmlutils.h"

#define XSLT_NAMESPACE "http://www.w3.org/1999/XSL/Transform"
#define XHTML_NAMESPACE "http://www.w3.org/1999/xhtml"
#define WSDL_NAMESPACE "http://schemas.xmlsoap.org/wsdl/"
#define SOAP_NAMESPACE "http://schemas.xmlsoap.org/wsdl/soap/"
#define BPEL_NAMESPACE "http://schemas.xmlsoap.org/ws/2003/03/business-process/"
#define XS_NAMESPACE "http://www.w3.org/2001/XMLSchema"
#define XDT_NAMESPACE "http://www.w3.org/2005/04/xpath-datatypes"
#define FN_NAMESPACE "http://www.w3.org/2005/04/xpath-functions"
#define ERR_NAMESPACE "http://www.w3.org/2004/07/xqt-errors"
#define XSI_NAMESPACE "http://www.w3.org/2001/XMLSchema-instance"
#define SVG_NAMESPACE "http://www.w3.org/2000/svg"
#define XML_NAMESPACE "http://www.w3.org/XML/1998/namespace"
#define GX_NAMESPACE "http://gridxslt.sourceforge.net"

typedef struct ns_def ns_def;
typedef struct ns_map ns_map;

struct ns_def {
  char *prefix;
  char *href;
};

struct ns_map {
  list *defs;
  ns_map *parent;
};

void ns_def_free(ns_def *def);

ns_map *ns_map_new();
void ns_map_free(ns_map *map, int free_parents);

void ns_add_preferred(ns_map *map, const char *href, const char *preferred_prefix);
void ns_add_direct(ns_map *map, const char *href, const char *prefix);
ns_def *ns_lookup_prefix(ns_map *map, const char *prefix);
ns_def *ns_lookup_href(ns_map *map, const char *href);

nsname qname_to_nsname(ns_map *map, const qname qn);
qname nsname_to_qname(ns_map *map, const nsname nn);

nsnametest *qnametest_to_nsnametest(ns_map *map, const qnametest *qt);

#endif /* _UTIL_NAMESPACE_H */
