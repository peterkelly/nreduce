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

#define XS_VALIDATOR_INV_NONE                 0
#define XS_VALIDATOR_INV_MISSING_ELEMENT      1
#define XS_VALIDATOR_INV_WRONG_ELEMENT        2
#define XS_VALIDATOR_INV_NOTHING_MORE_ALLOWED 3

namespace GridXSLT {

struct xs_validator {
  Schema *s;
  GridXSLT::Error ei;

  int inv_type;
  Particle *inv_p;
  SchemaElement *inv_e;
  xmlNodePtr inv_n;
  int inv_lineno;
  int inv_count;
};

void xs_init_typeinfo(Schema *s);

xs_validator *xs_validator_new(Schema *s);
void xs_print_defs(Schema *s);
void xs_validator_free(xs_validator *v);

int validate_root(xs_validator *v, char *filename, xmlNodePtr n, stringbuf *encoded);
int xs_decode(xs_validator *v, xmlTextWriter *writer, Type *t, stringbuf *encoded, int pos);

};

#endif /* _XMLSCHEMA_VALIDATION_H */
