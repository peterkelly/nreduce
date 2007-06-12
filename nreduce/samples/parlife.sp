function for(min,max,f)
{
  map(f,range(min,max));
}

function range(min,max)
{
  if (min == max)
    cons(min,nil);
  else
    cons(min,range(min+1,max));
}

function creategrid(rows,cols)
{
  map(lambda (n) { map(lambda (n) { 0; },range(1,rows)); },range(1,cols));
}

function printcell(cell)
{
  if (cell == 1)
    "##";
  else
    "--";
}

function printrow(row)
{
  if (row) {
    printcell(head(row));
    printrow(tail(row));
  }
  else
    "\n";
}

function printgrid(grid)
{
  if (grid) {
    printrow(head(grid));
    printgrid(tail(grid));
  }
  else
    nil;
}

function grows(grid)
{
  len(grid);
}

function gcols(grid)
{
  len(head(grid));
}

function makegrid(pat,rows,cols)
{
  let startrow = floor((rows - grows(pat)) / 2);
  let endrow   = startrow + grows(pat);
  let startcol = floor((cols - gcols(pat)) / 2);
  let endcol   = startcol + gcols(pat);

  for(0,rows-1,lambda(row) {
      for(0,cols-1,lambda(col) {
          if (row >= startrow && row < endrow &&
              col >= startcol && col < endcol)
            pat[row-startrow][col-startcol];
          else
            0;
        });
    });
}

function dolife(grid)
{
  let nrows = grows(grid);
  let ncols = gcols(grid);
  for(0,nrows-1,lambda(row) {
      for (0,ncols-1,lambda (col) {
          let curvalue = grid[row][col];
          let count = sumnb(grid,row,col);
          if (curvalue == 0 && count == 3)
            1;
          else if (curvalue == 1 && (count == 2 || count == 3))
            1;
          else 0;
        });
    });
}

function sumnb(g,row,col)
{
  let maxrow = grows(g)-1;
  let maxcol = gcols(g)-1;
  let upleft =    (row > 0 && col > 0) ?           g[row-1][col-1] : 0;
  let up =        (row > 0) ?                      g[row-1][col]   : 0;
  let upright =   (row > 0 && col < maxcol) ?      g[row-1][col+1] : 0;
  let left =      (col > 0) ?                      g[row][col-1]   : 0;
  let right =     (col < maxcol) ?                 g[row][col+1]   : 0;
  let downleft =  (row < maxrow && col > 0) ?      g[row+1][col-1] : 0;
  let down =      (row < maxrow) ?                 g[row+1][col]   : 0;
  let downright = (row < maxrow && col < maxcol) ? g[row+1][col+1] : 0;

  upleft + up + upright + left + right + downleft + down + downright;
}

function countcols(cols,nextrow,!total)
{
  if (cols) {
    if (head(cols) == 1)
      countcols(tail(cols),nextrow,total+1);
    else
      countcols(tail(cols),nextrow,total);
  }
  else
    countgrid(nextrow,total);
}

function countgrid(rows,!total)
{
  if (rows)
    countcols(head(rows),tail(rows),total);
  else
    total;
}

function loop(iter,grid)
{
  if (iter > 0) {
    let count = countgrid(grid,0);
/*     printgrid(grid); */
/*     "\n"; */
    "total: ".numtostring(count)." cells\n";
    loop(iter-1,dolife(grid));
  }
  else
    nil;
}

function main()
{
  let pattern = [[0,1,1],
                 [1,1,0],
                 [0,1,0]];
  let iterations = 500;
  let rows = 250;
  let cols = 250;

  loop(iterations,makegrid(pattern,rows,cols));
}
