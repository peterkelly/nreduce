#include <stdio.h>
#include <stdlib.h>
#include <stdlib.h>

int main(int argc, char **argv)
{
  int i;
  double min = 0.0;
  for (i = 1; i < argc; i++) {
    char *end = NULL;
    double val = strtod(argv[i],&end);
    if (('\0' == argv[i][0]) || ('\0' != *end)) {
      fprintf(stderr,"Invalid number: %s\n",argv[i]);
      exit(1);
    }
    if ((1 == i) || (min > val))
      min = val;
  }

  printf("%.3f\n",min);

  return 0;
}
