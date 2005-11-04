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

#include "util/String.h"
#include "util/List.h"
#include "util/XMLUtils.h"
#include "xmlschema/xmlschema.h"
#include "dataflow/SequenceType.h"
#include "dataflow/Program.h"
#include <stdio.h>

using namespace GridXSLT;

int main(int argc, char **argv)
{
  ASSERT(2 <= argc);
  String a = argv[1];
  List<String> lst = a.parseList();
  for (Iterator<String> it = lst; it.haveCurrent(); it++)
    message("\"%*\"\n",&(*it));

  return 0;
}
