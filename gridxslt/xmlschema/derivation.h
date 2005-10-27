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

#ifndef _XMLSCHEMA_DERIVATION_H
#define _XMLSCHEMA_DERIVATION_H

#include "xmlschema.h"

namespace GridXSLT {

int xs_check_simple_type_derivation_valid(Schema *s, Type *t);
int xs_check_simple_type_derivation_ok(Schema *s, Type *D, Type *B,
                                    int final_extension, int final_restriction,
                                    int final_list, int final_union);
int xs_create_simple_type_restriction(Schema *s, Type *base, int defline,
                                      xs_facetdata *facets, Type **restriction);

Range Particle_effective_total_range(Particle *p);

int xs_check_complex_restriction(Schema *s, Type *t);
int xs_check_complex_extension(Schema *s, Type *t);
int xs_check_complex_type_derivation_ok(Schema *s, Type *D, Type *B,
                                        int final_extension, int final_restriction);

int particle_emptiable(Schema *s, Particle *p);
int xs_check_particle_valid_extension(Schema *s, Particle *E, Particle *B);

Particle *ignore_pointless(Schema *s, Particle *p, xs_allocset *as);

int xs_check_particle_valid_restriction(Schema *s, Particle *R, Particle *B);

};

#endif /* _XMLSCHEMA_DERIVATION_H */
