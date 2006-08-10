/*
 * This file is part of the GridXSLT project
 * Copyright (C) 2006 Peter Kelly (pmk@cs.adelaide.edu.au)
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

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <alloca.h>

char start[9] = {
0, 1, 1,
1, 1, 0,
0, 1, 0
};

void printgrid(char *grid, int w, int h)
{
  int y;
  int x;
  for (y = 0; y < h; y++) {
    for (x = 0; x < w; x++) {
      if (grid[y*w+x])
        printf("##");
      else
        printf("--");
    }
    printf("\n");
  }
  printf("\n");
}

void life(int iter, int w, int h)
{
  char *grid1 = alloca(w*h);
  char *grid2 = alloca(w*h);
  int startw = 3;
  int starth = 3;
  int row;
  int col;
  int startrow = (w-startw)/2;
  int startcol = (h-starth)/2;
  int i;

  memset(grid1,0,w*h);
  for (row = 0; row < h; row++) {
    for (col = 0; col < w; col++) {
      if ((row >= startrow) && (row < startrow+starth) &&
          (col >= startcol) && (col < startcol+startw)) {
        grid1[row*w+col] = start[(row-startrow)*startw+(col-startcol)];
      }
    }
  }

  printgrid(grid1,w,h);

  for (i = 1; i < iter; i++) {
    char *tmp;
    int maxrow = h-1;
    int maxcol = w-1;
    memset(grid2,0,w*h);
    for (row = 0; row < h; row++) {
      for (col = 0; col < w; col++) {
        int count = 0;
        int curvalue = grid1[row*w+col];
        if ((row > 0) && (col > 0))
          count += grid1[(row-1)*w+(col-1)];
        if (row > 0)
          count += grid1[(row-1)*w+(col)];
        if ((row > 0) && (col < maxcol))
          count += grid1[(row-1)*w+(col+1)];

        if (col > 0)
          count += grid1[row*w+(col-1)];
        if (col < maxcol)
          count += grid1[row*w+(col+1)];

        if ((row < maxrow) && (col > 0))
          count += grid1[(row+1)*w+(col-1)];
        if (row < maxrow)
          count += grid1[(row+1)*w+(col)];
        if ((row < maxrow) && (col < maxcol))
          count += grid1[(row+1)*w+(col+1)];

        if (curvalue == 0) {
          if (count == 3)
            grid2[row*w+col] = 1;
        }

        if (curvalue == 1) {
          if ((count == 2) || (count == 3))
            grid2[row*w+col] = 1;
        }

        if ((curvalue == 0) && (count == 3))
          grid2[row*w+col] = 1;
        else if ((curvalue == 1) && ((count == 2) || (count == 3)))
          grid2[row*w+col] = 1;
        else
          grid2[row*w+col] = 0;
      }
    }
    printgrid(grid2,w,h);
    tmp = grid1;
    grid1 = grid2;
    grid2 = tmp;
  }

}


int main()
{
  setbuf(stdout,NULL);
  life(50,20,20);
  return 0;
}
