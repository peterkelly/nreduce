function nfib(n)
{
  if (n<=1)
    1;
  else
    nfib(n-2)+nfib(n-1);
}

function loop(n,max)
{
  if (n > max)
    nil;
  else {
    let res = nfib(n);
    "nfib(".numtostring(n).") = ".numtostring(res)."\n";
    loop(n+1,max);
  }
}

function main(args)
{
  if (args)
    loop(0,stringtonum(head(args)));
  else
    "Please specify maximum value\n";
  "Done\n";
}
