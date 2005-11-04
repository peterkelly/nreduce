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

#ifndef _XMLSCHEMA_PARSE_H
#define _XMLSCHEMA_PARSE_H

#include "XMLSchema.h"

namespace GridXSLT {

int xs_parse_value_constraint(Schema *s, xmlNodePtr n, xmlDocPtr doc, ValueConstraint *vc,
                              const char *errname);
int xs_parse_ref(Schema *s, xmlNodePtr n, xmlDocPtr doc, char *attrname, int type,
                 void **obj, Reference **refptr);
int xs_parse_form(Schema *s, xmlNodePtr n, int *qualified);
int xs_parse_block_final(const char *str, int *extension, int *restriction,
                         int *substitution, int *list, int *union1);
int xs_parse_element(xmlNodePtr n, xmlDocPtr doc, char *ns,
                     Schema *s, list **particles_list);
int xs_parse_attribute_use(Schema *s, xmlNodePtr n, int *use);
int xs_parse_attribute(Schema *s, xmlNodePtr n, xmlDocPtr doc, char *ns,
                       int toplevel, list **aulist);
int xs_parse_attribute_group_def(Schema *s, xmlNodePtr n, xmlDocPtr doc, char *ns);
int xs_parse_attribute_group_ref(Schema *s, xmlNodePtr n, xmlDocPtr doc, char *ns,
                                 list **reflist);
int xs_parse_simple_type(Schema *s, xmlNodePtr n, xmlDocPtr doc, char *ns,
                         int toplevel, Type **tout);
int xs_parse_complex_type_attributes(Schema *s, xmlNodePtr n, xmlDocPtr doc, char *ns,
                                     Type *t, xmlNodePtr c);
int xs_parse_simple_content_children(Schema *s, xmlNodePtr n, xmlDocPtr doc, char *ns,
                                     Type *t, xmlNodePtr c);
int xs_parse_complex_content_children(Schema *s,
                                   xmlNodePtr n, xmlDocPtr doc, char *ns,
                                   Type *t, xmlNodePtr c, int effective_mixed);
int xs_parse_complex_type(Schema *s, xmlNodePtr n, xmlDocPtr doc, char *ns,
                          int toplevel, Type **tout);
int xs_parse_group_def(xmlNodePtr n, xmlDocPtr doc, char *ns, Schema *s);
int xs_parse_group_ref(Schema *s, xmlNodePtr n, xmlDocPtr doc, list **particles_list,
                       Particle **pout);
int xs_parse_model_group(Schema *s, xmlNodePtr n, char *ns, xmlDocPtr doc,
                         ModelGroup **mgout);
int xs_parse_all_choice_sequence(Schema *s, xmlNodePtr n, char *ns, xmlDocPtr doc,
                                 list **particles_list, Particle **pout);
int xs_parse_wildcard(Schema *s, xmlNodePtr n, xmlDocPtr doc, Wildcard **wout);
int xs_parse_any(Schema *s, xmlNodePtr n, xmlDocPtr doc, list **particles_list);
int xs_parse_schema(Schema *s, xmlNodePtr n, xmlDocPtr doc);

};

#endif /* _XMLSCHEMA_PARSE_H */
