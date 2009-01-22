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
 * $Id: runtime.h 849 2009-01-22 01:01:00Z pmkelly $
 *
 */

#include <stdio.h>
#include <math.h>

int main(int argc, char **argv)
{
  double exp;
  int prev = -1;
  for (exp = 0; floor(pow(2,exp)) <= 4096.0; exp += 0.25) {
    int i = (int)pow(2,exp);
    if (i != prev)
      printf(" %d",i);
    prev = i;
  }
  printf("\n");
  return 0;
}
