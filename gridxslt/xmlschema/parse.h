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

#ifndef _XMLSCHEMA_PARSE_H
#define _XMLSCHEMA_PARSE_H

#include "xmlschema.h"

int xs_parse_value_constraint(xs_schema *s, xmlNodePtr n, xmlDocPtr doc, xs_value_constraint *vc,
                              const char *errname);
int xs_parse_ref(xs_schema *s, xmlNodePtr n, xmlDocPtr doc, char *attrname, int type,
                 void **obj, xs_reference **refptr);
int xs_parse_form(xs_schema *s, xmlNodePtr n, int *qualified);
int xs_parse_block_final(const char *str, int *extension, int *restriction,
                         int *substitution, int *list, int *union1);
int xs_parse_element(xmlNodePtr n, xmlDocPtr doc, char *ns,
                     xs_schema *s, list **particles_list);
int xs_parse_attribute_use(xs_schema *s, xmlNodePtr n, int *use);
int xs_parse_attribute(xs_schema *s, xmlNodePtr n, xmlDocPtr doc, char *ns,
                       int toplevel, list **aulist);
int xs_parse_attribute_group_def(xs_schema *s, xmlNodePtr n, xmlDocPtr doc, char *ns);
int xs_parse_attribute_group_ref(xs_schema *s, xmlNodePtr n, xmlDocPtr doc, char *ns,
                                 list **reflist);
int xs_parse_simple_type(xs_schema *s, xmlNodePtr n, xmlDocPtr doc, char *ns,
                         int toplevel, xs_type **tout);
int xs_parse_complex_type_attributes(xs_schema *s, xmlNodePtr n, xmlDocPtr doc, char *ns,
                                     xs_type *t, xmlNodePtr c);
int xs_parse_simple_content_children(xs_schema *s, xmlNodePtr n, xmlDocPtr doc, char *ns,
                                     xs_type *t, xmlNodePtr c);
int xs_parse_complex_content_children(xs_schema *s,
                                   xmlNodePtr n, xmlDocPtr doc, char *ns,
                                   xs_type *t, xmlNodePtr c, int effective_mixed);
int xs_parse_complex_type(xs_schema *s, xmlNodePtr n, xmlDocPtr doc, char *ns,
                          int toplevel, xs_type **tout,
                          char *container_name, char *container_ns);
int xs_parse_group_def(xmlNodePtr n, xmlDocPtr doc, char *ns, xs_schema *s);
int xs_parse_group_ref(xs_schema *s, xmlNodePtr n, xmlDocPtr doc, list **particles_list,
                       xs_particle **pout);
int xs_parse_model_group(xs_schema *s, xmlNodePtr n, char *ns, xmlDocPtr doc,
                         xs_model_group **mgout, char *container_name, char *container_ns);
int xs_parse_all_choice_sequence(xs_schema *s, xmlNodePtr n, char *ns, xmlDocPtr doc,
                                 list **particles_list, xs_particle **pout,
                                 char *container_name, char *container_ns);
int xs_parse_wildcard(xs_schema *s, xmlNodePtr n, xmlDocPtr doc, xs_wildcard **wout);
int xs_parse_any(xs_schema *s, xmlNodePtr n, xmlDocPtr doc, list **particles_list);
int xs_parse_schema(xs_schema *s, xmlNodePtr n, xmlDocPtr doc);

#endif /* _XMLSCHEMA_PARSE_H */
