#include <stdio.h>
#include <unistd.h>

int main()
{
  int total = 0;
  char buf[1024];
  int r;
  while (0 < (r = read(STDIN_FILENO,buf,1024)))
    total += r;
  printf("%.1f",total/(1024.0*1024.0));
  return 0;
}
