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

void merge_sort(char **arr, char **aux, int lo, int hi)
{
  if (lo >= hi)
    return;

  int mid = lo+(hi+1-lo)/2;
  merge_sort(arr,aux,lo,mid-1);
  merge_sort(arr,aux,mid,hi);

  int pos = 0;
  int x = lo;
  int y = mid;
  while ((x <= mid-1) && (y <= hi)) {
    char *xval = arr[x];
    char *yval = arr[y];
    if (strcmp(xval,yval) < 0) {
      aux[pos++] = xval;
      x++;
    }
    else {
      aux[pos++] = yval;
      y++;
    }
  }
  if (x <= mid-1) {
    memcpy(&aux[pos],&arr[x],(mid-x)*sizeof(char*));
    pos += mid-x;
  }
  if (y <= hi) {
    memcpy(&aux[pos],&arr[y],(hi+1-y)*sizeof(char*));
    pos += hi+1-y;
  }
  memcpy(&arr[lo],aux,(hi+1-lo)*sizeof(char*));
}

int main(int argc, char **argv)
{
  int n = 1000;
  char **strings;
  char **temp;
  int i;
  if (2 <= argc)
    n = atoi(argv[1]);

  strings = genlist(n);
  temp = (char**)malloc(n*sizeof(char*));

  merge_sort(strings,temp,0,n-1);

  for (i = 0; i < n; i++)
    printf("%s\n",strings[i]);

  for (i = 0; i < n; i++)
    free(strings[i]);
  free(strings);
  free(temp);
  return 0;
}
