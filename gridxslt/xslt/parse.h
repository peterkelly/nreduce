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

#ifndef _XSLT_PARSE_H
#define _XSLT_PARSE_H

#include "xslt.h"
#include "util/xmlutils.h"
#include <stdio.h>

int parse_xslt_relative_uri(error_info *ei, const char *filename, int line, const char *errname,
                            const char *base_uri, const char *uri, xl_snode *sroot);
int parse_xslt_uri(error_info *ei, const char *filename, int line, const char *errname,
                   const char *full_uri, xl_snode *sroot, const char *refsource);

#endif /* _XSLT_PARSE_H */
