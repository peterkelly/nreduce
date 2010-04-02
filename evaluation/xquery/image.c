#include <stdio.h>
#include <unistd.h>

int main()
{
  unsigned char buf[2048];
  read(0,buf,2048);
  int y;
  int x;
/*   char *str = " .+#xxxxxxxxxxxxxxx"; */
  printf("<data xmlns=\"urn:d\">\n");
  for (y = 0; y < 32; y++) {
    printf("  <row>\n");
    for (x = 0; x < 32; x++) {
      int raw = buf[13+y*32+x];
/*       int val = (4-buf[17+y*32+x]/64); */
/*       printf("%c",str[val]); */
/*       printf("%c",str[val]); */
      printf("    <col>%d</col>\n",100*raw/140);
    }
    printf("  </row>\n");
  }
  printf("</data>\n");
  return 0;
}
