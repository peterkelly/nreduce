#include <math.h>
#include <stdio.h>
#include <stdlib.h>

double mandel(double Cr, double Ci)
{
  double res;
  double Zr = 0.0;
  double Zi = 0.0;
  for (res = 0; res < 4096; res++) {
    double newZr = ((Zr * Zr) - (Zi * Zi)) + Cr;
    double newZi = (2.0 * (Zr * Zi)) + Ci;
    double mag = sqrt((newZr * newZr) + (newZi * newZi));
    if (mag > 2.0)
      return res;
    Zr = newZr;
    Zi = newZi;
  }
  return res;
}

void printcell(double num)
{
  if (num > 40)
    printf("  ");
  else if (num > 30)
    printf("''");
  else if (num > 20)
    printf("--");
  else if (num > 10)
    printf("**");
  else
    printf("##");
}

void mloop(double minx, double maxx, double miny, double maxy, double incr)
{
  double x;
  double y;
  for (y = miny; y < maxy; y += incr) {
    for (x = minx; x < maxx; x += incr)
      printcell(mandel(x,y));
    printf("\n");
  }
}

int main(int argc, char **argv)
{
  int n = 32;
  double incr;
  if (2 <= argc)
    n = atoi(argv[1]);

  incr = 2.0/((double)n);
  mloop(-1.5,0.5,-1.0,1.0,incr);
  return 0;
}
