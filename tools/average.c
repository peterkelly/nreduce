/*
 * This file is part of the nreduce project
 * Copyright (C) 2008-2009 Peter Kelly (pmk@cs.adelaide.edu.au)
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

#include <stdio.h>
#include <stdlib.h>
#include <stdlib.h>

int main(int argc, char **argv)
{
  int i;
  double total = 0.0;
  for (i = 1; i < argc; i++) {
    char *end = NULL;
    double val = strtod(argv[i],&end);
    if (('\0' == argv[i][0]) || ('\0' != *end)) {
      fprintf(stderr,"Invalid number: %s\n",argv[i]);
      exit(1);
    }
    total += val;
  }

  double avg = (argc > 1) ? total/(argc-1) : 0.0;
  printf("%.3f\n",avg);

  return 0;
}
