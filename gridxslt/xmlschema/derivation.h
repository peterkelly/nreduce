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

#ifndef _XMLSCHEMA_DERIVATION_H
#define _XMLSCHEMA_DERIVATION_H

#include "xmlschema.h"

typedef struct ignore_pointless_data ignore_pointless_data;

struct ignore_pointless_data {
  list *allocp;
  list *allocmg;
};

int xs_check_simple_type_derivation_valid(xs_schema *s, xs_type *t);
int xs_check_simple_type_derivation_ok(xs_schema *s, xs_type *D, xs_type *B,
                                    int final_extension, int final_restriction,
                                    int final_list, int final_union);
int xs_create_simple_type_restriction(xs_schema *s, xs_type *base, int defline,
                                      xs_facetdata *facets, xs_type **restriction);

xs_range xs_particle_effective_total_range(xs_particle *p);

int xs_check_complex_restriction(xs_schema *s, xs_type *t);
int xs_check_complex_extension(xs_schema *s, xs_type *t);
int xs_check_complex_type_derivation_ok(xs_schema *s, xs_type *D, xs_type *B,
                                        int final_extension, int final_restriction);

int particle_emptiable(xs_schema *s, xs_particle *p);
int xs_check_particle_valid_extension(xs_schema *s, xs_particle *E, xs_particle *B);

xs_particle *ignore_pointless(xs_schema *s, xs_particle *p, ignore_pointless_data *ipd);
void ignore_pointless_free(ignore_pointless_data *ipd);

int xs_check_particle_valid_restriction(xs_schema *s, xs_particle *R, xs_particle *B);

#endif /* _XMLSCHEMA_DERIVATION_H */
