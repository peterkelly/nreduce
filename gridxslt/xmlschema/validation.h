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

#ifndef _XMLSCHEMA_VALIDATION_H
#define _XMLSCHEMA_VALIDATION_H

#include "util/stringbuf.h"
#include "xmlschema.h"
#include <libxml/tree.h>
#include <libxml/xmlwriter.h>

typedef struct xs_validator xs_validator;

#define XS_VALIDATOR_INV_NONE                 0
#define XS_VALIDATOR_INV_MISSING_ELEMENT      1
#define XS_VALIDATOR_INV_WRONG_ELEMENT        2
#define XS_VALIDATOR_INV_NOTHING_MORE_ALLOWED 3

struct xs_validator {
  xs_schema *s;
  error_info ei;

  int inv_type;
  xs_particle *inv_p;
  xs_element *inv_e;
  xmlNodePtr inv_n;
  int inv_lineno;
  int inv_count;
};

void xs_init_typeinfo(xs_schema *s);

xs_validator *xs_validator_new(xs_schema *s);
void xs_print_defs(xs_schema *s);
void xs_validator_free(xs_validator *v);

int validate_root(xs_validator *v, char *filename, xmlNodePtr n, stringbuf *encoded);
int xs_decode(xs_validator *v, xmlTextWriter *writer, xs_type *t, stringbuf *encoded, int pos);

#endif /* _XMLSCHEMA_VALIDATION_H */
