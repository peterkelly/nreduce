#include <stdio.h>
#include <stdlib.h>
#include <string.h>


int main(int argc, char **argv)
{
  char *chars = "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz";
  int nchars = strlen(chars);
  int offset;
  int incr;
  int count;
  int pos;
  int i;

  if (4 > argc) {
    fprintf(stderr,"Usage: genstrand <offset> <incr> <count>\n");
    return 1;
  }

  offset = atoi(argv[1]);
  incr = atoi(argv[2]);
  count = atoi(argv[3]);

  pos = offset;
  for (i = 0; i < count; i++) {
    printf("%c",chars[pos]);
    pos = (pos+incr)%nchars;
  }

  return 0;
}
