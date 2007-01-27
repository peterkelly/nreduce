#include <stdio.h>
#include <stdlib.h>

int nfib(int n)
{
  if (n <= 1)
    return 1;
  else
    return nfib(n-2)+nfib(n-1);
}

int main(int argc, char **argv)
{
  int i;
  int max;

  if (2 > argc) {
    fprintf(stderr,"no max value supplied\n");
    return -1;
  }

  max = atoi(argv[1]);

  for (i = 0; i <= max; i++)
    printf("nfib(%d) = %d\n",i,nfib(i));
  return 0;
}
