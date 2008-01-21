#include <stdio.h>
#include <stdlib.h>

/* Based on the nsieve program from http://shootout.alioth.debian.org/ by Alexei Svitkine */

int nsieve(int n, char *prime)
{
  int i, k;
  int count = 0;

  for (i = 2; i <= n; i++)
    prime[i] = 1;

  for (i = 2; i <= n; i++) {
    if (prime[i]) {
      for (k = i+i; k <= n; k+=i)
        prime[k] = 0;
      count++;
    }
  }
  return count;
}

int main(int argc, char **argv)
{
  int n = 10000;
  char *flags;
  if (2 <= argc)
    n = atoi(argv[1]);
  flags = (char*)calloc(1,n+1);
  printf("Primes up to %d: %d\n",n,nsieve(n,flags));
  free(flags);
  return 0;
}
