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
 */

#ifndef _XMLSCHEMA_OUTPUT_H
#define _XMLSCHEMA_OUTPUT_H

#include "xmlschema.h"
#include <stdio.h>

typedef struct dumpinfo dumpinfo;

struct dumpinfo {
  FILE *f;
  int indent;
  int in_builtin_type;
  int found_non_builtin;
};

int dump_type(xs_schema *s, xmlDocPtr doc, void *data, int post, xs_type *t);
int dump_model_group(xs_schema *s, xmlDocPtr doc, void *data, int post, xs_model_group *mg);
int dump_wildcard(xs_schema *s, xmlDocPtr doc, void *data, int post, xs_wildcard *w);
int dump_particle(xs_schema *s, xmlDocPtr doc, void *data, int post, xs_particle *p);
int dump_model_group_def(xs_schema *s, xmlDocPtr doc, void *data, int post, xs_model_group_def *mgd);
int dump_element(xs_schema *s, xmlDocPtr doc, void *data, int post, xs_element *e);
int dump_attribute(xs_schema *s, xmlDocPtr doc, void *data, int post, xs_attribute *a);
int dump_attribute_use(xs_schema *s, xmlDocPtr doc, void *data, int post, xs_attribute_use *au);

void output_xmlschema(FILE *f, xs_schema *s);
void dump_xmlschema(FILE *f, xs_schema *s);

#endif /* _XMLSCHEMA_OUTPUT_H */
