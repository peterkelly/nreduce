#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "nreduce.h"

cell *cons(cell *a, cell *b)
{
  cell *c = alloc_cell();
  c->tag = TYPE_CONS;
  c->field1 = a;
  c->field2 = b;
  return c;
}

cell *head(cell *c)
{
  return (cell*)c->field1;
}

cell *tail(cell *c)
{
  return (cell*)c->field2;
}

cell *item(int n, cell *lst)
{
  if (n == 0)
    return head(lst);
  else
    return item(n-1,tail(lst));
}

int len(cell *lst)
{
  if (TYPE_NIL == celltype(lst))
    return 0;
  else
    return 1+len(tail(lst));
}

void printcell(cell *c)
{
  if ((TYPE_INT == celltype(c)) && (atoi("1") == atoi((char*)c->field1)))
    printf("##");
  else
    printf("--");
}

void printrow(cell *row)
{
  if (TYPE_NIL != celltype(row)) {
    printcell(head(row));
    printrow(tail(row));
  }
  else {
    printf("\n");
  }
}

void printgrid(cell *grid)
{
  if (TYPE_NIL != celltype(grid)) {
    printrow(head(grid));
    printgrid(tail(grid));
  }
}

int grows(cell *grid)
{
  return len(grid);
}

int gcols(cell *grid)
{
  return len(head(grid));
}

cell *gcell(cell *grid, int row, int col)
{
  return item(col,item(row,grid));
}

cell *makegrid(cell *pat, int rows, int cols)
{
  return NULL; /* FIXME */
#if 0
  int startrow = (rows-grows(pat))/2;
  int endrow = startrow+grows(pat);
  int startcol = (cols-gcols(pat))/2;
  int endcol = startcol+gcols(pat);
  int row;
  int col;
  cell *grid = NULL;
  cell **rowptr = &grid;
  for (row = 0; row < rows; row++) {
    cell **colptr;
    *rowptr = alloc_cell();
    (*rowptr)->tag = TYPE_CONS;
    colptr = (cell**)&((*rowptr)->field1);
    rowptr = (cell**)&((*rowptr)->field1);
    for (col = 0; col < cols; col++) {
      
    }
  }
  (*rowptr) = alloc_cell();
  (*rowptr)->tag = TYPE_NIL;
#endif
}

void life(int iter, int w, int height)
{
/*   cell *grid1; */
/*   cell *grid2; */


}

int main()
{
  setbuf(stdout,NULL);
  initmem();
  life(50,20,20);
  cleanup();
  return 0;
}
