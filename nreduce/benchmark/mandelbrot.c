#include <math.h>
#include <stdio.h>
#include <stdlib.h>

double mandel(double Zr, double Zi, double Cr, double Ci, double iterations)
{
  double res;
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

double mandel0(double Cr, double Ci, double iterations)
{
  return mandel(0.0,0.0,Cr,Ci,iterations);
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

void mloop(double minx, double maxx, double xincr, double miny, double maxy, double yincr)
{
  double x;
  double y;
  for (y = miny; y < maxy; y += yincr) {
    for (x = minx; x < maxx; x += xincr) {
      printcell(mandel0(x,y,4096));
    }
    printf("\n");
  }
}

void mandelrange(double minx, double maxx, double xincr, double miny, double maxy, double yincr)
{
  mloop(minx,maxx+xincr,xincr,miny,maxy,yincr);
}

int main(int argc, char **argv)
{
  double incr = 0.01;
  if (2 <= argc)
    incr = atof(argv[1]);
  printf("incr = %f\n",incr);
  mandelrange(-1.5,0.5,incr,-1.0,1.0,incr);
}
