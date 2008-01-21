// Based on the nsieve program from http://shootout.alioth.debian.org/ by Alexei Svitkine

function nsieve(n,prime)
{
  var count = 0;

  for (var i = 2; i <= n; i++)
    prime[i] = true;

  for (var i = 2; i <= n; i++) {
    if (prime[i]) {
      for (var k = i+i; k <= n; k += i)
        prime[k] = false;
      count++;
    }
  }
  return count;
}

var n = 10000;
if (arguments.length > 0)
  n = arguments[0];
var prime = new Array(n+1);
print("Primes up to "+n+": "+nsieve(n,prime));
