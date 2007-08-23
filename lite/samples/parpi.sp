function main(args)
{
  if (args) {
    let N = stringtonum(head(args));
    let pi = dopi(N);
    "pi = ".numtostring(pi)."\n";
  }
  else {
    "Usage: pi <N>\n";
  }
}

function dopi(N)
{
  let parts = map(pipart(N),range(1,N));
  seq(parlist(parts),sum(parts,0));
}

function pipart(N,k)
{
  let x = k-0.5;
  4*N/(N*N+x*x);
}

function sum(lst,total)
{
  if (lst)
    sum(tail(lst),total+head(lst));
  else
    total;
}

function range(min,max)
{
  if (min > max)
    nil;
  else
    cons(min,range(min+1,max));
}
