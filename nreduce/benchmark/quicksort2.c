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

void quicksort(char **arr, int left, int right)
{
  int store = left;
  char *pivot = arr[right];
  int i;
  char *temp;
  for (i = left; i < right; i++) {
    if (strcmp(arr[i],pivot) < 0) {
      temp = arr[store];
      arr[store] = arr[i];
      arr[i] = temp;
      store++;
    }
  }
  arr[right] = arr[store];
  arr[store] = pivot;

  if (left < store-1)
    quicksort(arr,left,store-1);
  if (right > store+1)
    quicksort(arr,store+1,right);
}

int main(int argc, char **argv)
{
  int n = 1000;
  char **strings;
  int i;
  if (2 <= argc)
    n = atoi(argv[1]);

  strings = genlist(n);

  quicksort(strings,0,n-1);

  for (i = 0; i < n; i++)
    printf("%s\n",strings[i]);

  for (i = 0; i < n; i++)
    free(strings[i]);
  free(strings);
  return 0;
}
