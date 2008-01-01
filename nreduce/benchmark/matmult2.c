#include <stdio.h>
#include <stdlib.h>

int **createMatrix(int height, int width, int val)
{
  int **matrix = (int**)malloc(height*sizeof(int*));
  int x, y;
  for (y = 0; y < height; y++)
    matrix[y] = (int*)malloc(width*sizeof(int));
  for (y = 0; y < height; y++) {
    for (x = 0; x < width; x++) {
      matrix[y][x] = val%23;
      val++;
    }
  }
  return matrix;
}

void freeMatrix(int **matrix, int height)
{
  int y;
  for (y = 0; y < height; y++)
    free(matrix[y]);
  free(matrix);
}

void printMatrix(int **matrix, int width, int height)
{
  int x, y;
  for (y = 0; y < height; y++) {
    for (x = 0; x < width; x++)
      printf("%5d ",matrix[y][x]);
    printf("\n");
  }
}

int **multiply(int **a, int **b, int awidth, int aheight, int bwidth, int bheight)
{
  int height = aheight;
  int **result = (int**)malloc(height*sizeof(int*));
  int x, y, i;
  for (y = 0; y < height; y++)
    result[y] = (int*)malloc(bwidth*sizeof(int));

  int arows = aheight;
  int acols = awidth;
  int bcols = bwidth;

  for (y = 0; y < arows; y++)
    for (x = 0; x < bcols; x++)
      for (i = 0; i < acols; i++)
        result[y][x] += a[y][i] * b[i][x];
  return result;
}

int matsum(int **matrix, int width, int height)
{
  int total = 0;
  int x, y;
  for (y = 0; y < height; y++)
    for (x = 0; x < width; x++)
      total += matrix[y][x];
  return total;
}

int main(int argc, char **argv)
{
  int size = 10;
  int print = (argc != 2);
  int **a;
  int **b;
  int **c;
  if (2 <= argc)
    size = atoi(argv[1]);

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
  printf("sum = %d\n",matsum(c,size,size));

  freeMatrix(a,size);
  freeMatrix(b,size);
  freeMatrix(c,size);

  return 0;
}
