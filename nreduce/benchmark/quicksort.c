#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

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

char **quicksort(char **list, int len)
{
  char *pivot = list[0];
  char **res = (char**)malloc(len*sizeof(char*));
  char **before = (char**)malloc(len*sizeof(char*));
  char **after = (char**)malloc(len*sizeof(char*));
  int nbefore = 0;
  int nafter = 0;
  int pos;

  for (pos = 1; pos < len; pos++) {
    char *cur = list[pos];
    if (strcmp(cur,pivot) < 0)
      before[nbefore++] = cur;
    else
      after[nafter++] = cur;
  }

  if (0 < nbefore) {
    char **sbefore = quicksort(before,nbefore);
    memcpy(&res[0],sbefore,nbefore*sizeof(char*));
    free(sbefore);
  }
  res[nbefore] = pivot;
  if (0 < nafter) {
    char **safter = quicksort(after,nafter);
    memcpy(&res[nbefore+1],safter,nafter*sizeof(char*));
    free(safter);
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

  sorted = quicksort(strings,n);

  for (i = 0; i < n; i++)
    printf("%s\n",sorted[i]);

  for (i = 0; i < n; i++)
    free(strings[i]);
  free(strings);
  free(sorted);
  return 0;
}
