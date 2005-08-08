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

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

int main()
{
  int i;
  int j;
  int k;
  int a[3][3];
  int b[3][3];
  int c[3][3];

  memset(a,0,9*sizeof(int));
  memset(b,0,9*sizeof(int));
  memset(c,0,9*sizeof(int));

  fscanf(stdin,"%d %d %d",&a[0][0],&a[0][1],&a[0][2]);
  fscanf(stdin,"%d %d %d",&a[1][0],&a[1][1],&a[1][2]);
  fscanf(stdin,"%d %d %d",&a[2][0],&a[2][1],&a[2][2]);

  fscanf(stdin,"%d %d %d",&b[0][0],&b[0][1],&b[0][2]);
  fscanf(stdin,"%d %d %d",&b[1][0],&b[1][1],&b[1][2]);
  fscanf(stdin,"%d %d %d",&b[2][0],&b[2][1],&b[2][2]);

  for (i = 0; i < 3; i++)
    for (k = 0; k < 3; k++)
      for (j = 0; j < 3; j++)
        c[i][k] += a[i][j]*b[j][k];

  for (i = 0; i < 3; i++)
    printf("%4d %4d %4d      %4d %4d %4d      %4d %4d %4d\n",
           a[i][0],a[i][1],a[i][2],b[i][0],b[i][1],b[i][2],c[i][0],c[i][1],c[i][2]);

  return 0;
}

