function main(args)
{
  if (args) {
    let N = stringtonum(head(args));
    let pi = dopi(1,N,0);
    "pi = ".numtostring(pi)."\n";
  }
  else {
    "Usage: pi <N>\n";
  }
}

function dopi(k,N,total)
{
  if (k > N) {
    total;
  }
  else {
    let x = k-0.5;
    let newtotal = total + 4*N/(N*N+x*x);
    // echo("k = ".numtostring(k).", pi = ".numtostring(newtotal)."\n");
    dopi(k+1,N,newtotal);
  }
}
