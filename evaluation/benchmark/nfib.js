function nfib(n)
{
  if (n <= 1)
    return 1;
  else
    return nfib(n-2)+nfib(n-1);
}

var n = 24;
if (arguments.length > 0)
  n = arguments[0];

for (var i = 0; i <= n; i++)
  print("nfib("+i+") = "+nfib(i));
