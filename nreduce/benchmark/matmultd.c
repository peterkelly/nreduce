#include <stdio.h>
#include <stdlib.h>
#include <math.h>

double **createMatrix(double height, double width, double val)
{
  double **matrix = (double**)malloc(height*sizeof(double*));
  double x, y;
  for (y = 0; y < height; y++)
    matrix[(int)y] = (double*)malloc(width*sizeof(double));
  for (y = 0; y < height; y++) {
    for (x = 0; x < width; x++) {
      matrix[(int)y][(int)x] = fmod(val,23);
      val++;
    }
  }
  return matrix;
}

void freeMatrix(double **matrix, double height)
{
  double y;
  for (y = 0; y < height; y++)
    free(matrix[(int)y]);
  free(matrix);
}

void printMatrix(double **matrix, double width, double height)
{
  double x, y;
  for (y = 0; y < height; y++) {
    for (x = 0; x < width; x++)
      printf("%5d ",(int)matrix[(int)y][(int)x]);
    printf("\n");
  }
}

double **multiply(double **a, double **b,
                  double awidth, double aheight, double bwidth, double bheight)
{
  int height = aheight;
  double **result = (double**)malloc(height*sizeof(double*));
  double x, y, i;
  for (y = 0; y < height; y++)
    result[(int)y] = (double*)malloc(bwidth*sizeof(double));

  int arows = aheight;
  int acols = awidth;
  int bcols = bwidth;

  for (y = 0; y < arows; y++)
    for (x = 0; x < bcols; x++)
      for (i = 0; i < acols; i++)
        result[(int)y][(int)x] += a[(int)y][(int)i] * b[(int)i][(int)x];
  return result;
}

double matsum(double **matrix, double width, double height)
{
  double total = 0;
  double x, y;
  for (y = 0; y < height; y++)
    for (x = 0; x < width; x++)
      total += matrix[(int)y][(int)x];
  return total;
}

int main(int argc, char **argv)
{
  double size = 10;
  int print = (argc != 2);
  double **a;
  double **b;
  double **c;
  if (2 <= argc)
    size = atof(argv[1]);

  a = createMatrix(size,size,1);
  b = createMatrix(size,size,2);
  if (print) {
    printMatrix(a,size,size);
    printf("--\n");
    printMatrix(b,size,size);
    printf("--\n");
  }

  c = multiply(a,b,size,size,size,size);
  if (print) {
    printMatrix(c,size,size);
    printf("\n");
  }
  printf("sum = %.0f\n",matsum(c,size,size));

  freeMatrix(a,size);
  freeMatrix(b,size);
  freeMatrix(c,size);

  return 0;
}
