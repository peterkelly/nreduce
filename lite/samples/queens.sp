function main(args)
{
  if (len(args) == 0) {
    "Please specify a value for n\n";
  }
  else {
    let n = stringtonum(args[0]);
    //"Solutions for (queens ".numtostring(n).") = ".concat((map(showboard(n),queens(n))));
    "Number of solutions for (queens ".numtostring(n).") = ".numtostring(len(queens(n)));
    "\n";
  }
}

function ok(row,col,r,c)
{
  if (col == c)
    nil;
  else
    abs(col-c) != (row-r);
}

function check(row,col,lst)
{
  if (lst) {
    let cur = head(lst);
    let r = head(cur);
    let c = tail(cur);
    and(ok(row,col,r,c),check(row,col,tail(lst)));
  }
  else {
    1;
  }
}

function range(min,max)
{
  if (min == max)
    cons(min,nil);
  else
    cons(min,range(min+1,max));
}

function par_map(g,f,lst)
{
  parmap(g,0,0,f,lst);
}

function parmap(g,s,p,f,lst)
{
  if (lst) {
    let x = head(lst);
    let xs = tail(lst);
    let fx = f(x);
    let pmxs = parmap(g,s,p,f,xs);
    par(fx,par(forcelist(pmxs),cons(fx,pmxs)));
  }
}

function notnull(x)
{
  x;
}

function solve(row,n)
{
  if (row == 0)
    [[]];
  else {
    let rst = solve(row-1,n);
    concat(par_map(row,
                   lambda (rest) {
                     filter(notnull,
                            par_map(row,
                                    lambda (col) {
                                      if (check(row,col,rest))
                                        cons(cons(row,col),rest);
                                      else
                                        [];},
                                    range(1,n)));},
                   rst));
  }
}


function showboard(size,b)
{
  concat(map(showrow(size),b))."\n";
}

function showrow(size,lst)
{
  let c = tail(lst);
  rep(c-1,". ")."Q ".rep(size-c,". ")."\n";
}

function rep(n,x)
{
  if (n == 0)
    nil;
  else
    x.rep(n-1,x);
}



function queens(n)
{
  solve(n,n);
}

function concat(lst)
{
  if (lst)
    head(lst).concat(tail(lst));
}
