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

#ifndef _XSLT_TYPE_CONVERSION_H
#define _XSLT_TYPE_CONVERSION_H

#include "util/String.h"
#include "dataflow/SequenceType.h"
#include "dataflow/Program.h"

namespace GridXSLT {

String typestr(const SequenceType &st);
Value invalidTypeConversion(Environment *env, const Value &source, const String &target);
Value invalidValueConversion(Environment *env, const Value &source, const String &target);
Value invalidString(Environment *env, const String &source, const String &target);

};

#endif /* _XSLT_TYPE_CONVERSION_H */
