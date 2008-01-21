#include <stdio.h>
#include <stdlib.h>
#include <string.h>

char **genlist(int max)
{
  char **res = (char**)malloc(max*sizeof(char*));
  int i;
  for (i = 0; i < max; i++) {
    char tmp[100];
    char *rev;
    int len;
    int c;
    sprintf(tmp,"%d",7*i);
    len = strlen(tmp);
    rev = (char*)malloc(len+1);
    for (c = 0; c < len; c++)
      rev[c] = tmp[len-1-c];
    rev[len] = '\0';
    res[i] = rev;
  }
  return res;
}

char **merge(char **xlst, int xlen, char **ylst, int ylen);

char **merge_sort(char **list, int len)
{
  int middle;
  char **left;
  char **left2;
  char **right;
  char **right2;
  char **merged;

  if (len <= 1) {
    char **copy = (char**)malloc(len*sizeof(char*));
    memcpy(copy,list,len*sizeof(char*));
    return copy;
  }

  middle = len/2;
  left = (char**)malloc(middle*sizeof(char*));
  memcpy(left,list,middle*sizeof(char*));
  right = (char**)malloc((len-middle)*sizeof(char*));
  memcpy(right,&list[middle],(len-middle)*sizeof(char*));

  left2 = merge_sort(left,middle);
  right2 = merge_sort(right,len-middle);
  free(left);
  free(right);

  merged = merge(left2,middle,right2,len-middle);
  free(left2);
  free(right2);
  return merged;
}

char **merge(char **xlst, int xlen, char **ylst, int ylen)
{
  int x = 0;
  int y = 0;
  char **res = (char**)malloc((xlen+ylen)*sizeof(char*));
  int rpos = 0;
  while ((x < xlen) && (y < ylen)) {
    char *xval = xlst[x];
    char *yval = ylst[y];
    if (strcmp(xval,yval) < 0) {
      res[rpos++] = xval;
      x++;
    }
    else {
      res[rpos++] = yval;
      y++;
    }
  }
  if (x < xlen) {
    memcpy(&res[rpos],&xlst[x],(xlen-x)*sizeof(char*));
    rpos += (xlen-x);
  }
  if (y < ylen) {
    memcpy(&res[rpos],&ylst[y],(ylen-y)*sizeof(char*));
    rpos += (ylen-y);
  }
  return res;
}

int main(int argc, char **argv)
{
  int n = 1000;
  char **strings;
  char **sorted;
  int i;
  if (2 <= argc)
    n = atoi(argv[1]);

  strings = genlist(n);

  sorted = merge_sort(strings,n);

  for (i = 0; i < n; i++)
    printf("%s\n",sorted[i]);

  for (i = 0; i < n; i++)
    free(strings[i]);
  free(strings);
  free(sorted);
  return 0;
}
