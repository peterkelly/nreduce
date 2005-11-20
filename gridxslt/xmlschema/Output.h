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

#ifndef _XMLSCHEMA_OUTPUT_H
#define _XMLSCHEMA_OUTPUT_H

#include "XMLSchema.h"
#include <stdio.h>

namespace GridXSLT {

class OutputVisitor : public SchemaVisitor {
  DISABLE_COPY(OutputVisitor)
public:
  OutputVisitor(xmlTextWriter *_writer) : SchemaVisitor(), writer(_writer) { }
  virtual ~OutputVisitor() { }

  virtual int type(Schema *s, int post, Type *t);
  virtual int attribute(Schema *s, int post, SchemaAttribute *a);
  virtual int element(Schema *s, int post, SchemaElement *e);

  virtual int attributeGroup(Schema *s, int post, AttributeGroup *ag);
  virtual int attributeGroupRef(Schema *s, int post,
                                AttributeGroupRef *agr);
  virtual int modelGroupDef(Schema *s, int post, ModelGroupDef *mgd);
  virtual int particle(Schema *s, int post, Particle *p);
  virtual int attributeUse(Schema *s, int post, AttributeUse *au);

  xmlTextWriter *writer;
};

class DumpVisitor : public SchemaVisitor {
  DISABLE_COPY(DumpVisitor)
public:
  DumpVisitor() : SchemaVisitor(), f(NULL), indent(0), in_builtin_type(0), found_non_builtin(0) { }
  virtual ~DumpVisitor() { }

  virtual int type(Schema *s, int post, Type *t);
  virtual int attribute(Schema *s, int post, SchemaAttribute *a);
  virtual int element(Schema *s, int post, SchemaElement *e);
  virtual int attributeGroup(Schema *s, int post, AttributeGroup *ag);
  virtual int attributeGroupRef(Schema *s, int post,
                                AttributeGroupRef *agr);
  virtual int modelGroupDef(Schema *s, int post, ModelGroupDef *mgd);
  virtual int modelGroup(Schema *s, int post, ModelGroup *mg);
  virtual int particle(Schema *s, int post, Particle *p);
  virtual int wildcard(Schema *s, int post, Wildcard *w);
  virtual int attributeUse(Schema *s, int post, AttributeUse *au);
  virtual int schema(Schema *s, int post, Schema *s2);

  void incIndent();
  void decIndent();
  void dump_printf(const char *format, ...);
  void dump_reference(char *type, Reference *r);
  void dump_enum_val(const char **enumvals, char *name, int val);
  void dump_value_constraint(ValueConstraint *vc);

  FILE *f;
  int indent;
  int in_builtin_type;
  int found_non_builtin;
};

};

typedef struct dumpinfo dumpinfo;

struct dumpinfo {
  FILE *f;
  int indent;
  int in_builtin_type;
  int found_non_builtin;
};

void output_xmlschema(FILE *f, GridXSLT::Schema *s);
void dump_xmlschema(FILE *f, GridXSLT::Schema *s);

#endif /* _XMLSCHEMA_OUTPUT_H */
