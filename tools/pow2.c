#include <stdio.h>
#include <math.h>

int main(int argc, char **argv)
{
  double exp;
  int prev = -1;
  for (exp = 0; floor(pow(2,exp)) <= 4096.0; exp += 0.25) {
    int i = (int)pow(2,exp);
    if (i != prev)
      printf(" %d",i);
    prev = i;
  }
  printf("\n");
  return 0;
}
