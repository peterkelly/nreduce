#include <stdio.h>
#include <stdlib.h>
#include <stdlib.h>

int main(int argc, char **argv)
{
  int i;
  double total = 0.0;
  for (i = 1; i < argc; i++) {
    char *end = NULL;
    double val = strtod(argv[i],&end);
    if (('\0' == argv[i][0]) || ('\0' != *end)) {
      fprintf(stderr,"Invalid number: %s\n",argv[i]);
      exit(1);
    }
    total += val;
  }

  double avg = (argc > 1) ? total/(argc-1) : 0.0;
  printf("%.3f\n",avg);

  return 0;
}
