/*

Program to simulate a slow connection to a web server.

Place in cgi-bin directory and invoke by requesting like this:
http://localhost/cgi-bin/data?size=10M&speed=100k

where speed is the number of bytes per second. Both size and speed may be
suffixed by m or k to indicate megabytes or kilobytes.

*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <time.h>
#include <sys/time.h>
#include <errno.h>

#define BUFSIZE 16384

int size = -1;
int speed = 0;

struct timeval timeval_diff(struct timeval from, struct timeval to)
{
  struct timeval diff;
  diff.tv_sec = to.tv_sec - from.tv_sec;
  diff.tv_usec = to.tv_usec - from.tv_usec;
  if (0 > diff.tv_usec) {
    diff.tv_sec -= 1;
    diff.tv_usec += 1000000;
  }
  return diff;
}

int timeval_diffms(struct timeval from, struct timeval to)
{
  struct timeval diff = timeval_diff(from,to);
  return diff.tv_sec*1000 + diff.tv_usec/1000;
}

int parse_number(const char *value)
{
  if ((0 <= strlen(value)) && ('M' == toupper(value[strlen(value)-1])))
    return 1024*1024*atoi(value);
  else if ((0 <= strlen(value)) && ('K' == toupper(value[strlen(value)-1])))
    return 1024*atoi(value);
  else
    return atoi(value);
}

void process_pair(const char *name, const char *value)
{
  if (!strcmp(name,"size"))
    size = parse_number(value);
  else if (!strcmp(name,"speed"))
    speed = parse_number(value);
}

void parse_pair(char *pair)
{
  char *equals = strchr(pair,'=');
  if (NULL == equals)
    return;
  *equals = '\0';
  process_pair(pair,equals+1);
}

void parse_query_string(const char *query_string)
{
  char *copy = strdup(query_string);
  char *start = copy;
  char *c = start;
  int done = 0;
  while (!done) {
    if (('\0' == *c) || ('&' == *c)) {
      if ('\0' == *c)
        done = 1;
      *c = '\0';
      parse_pair(start);
      start = c+1;
    }
    c++;
  }
  free(copy);
}

int main(int argc, char **argv)
{
  setbuf(stdout,NULL);

  char *query_string = getenv("QUERY_STRING");
  if (NULL == query_string) {
    printf("Content-type: text/plain\r\n");
    printf("\r\n");
    printf("Error: no query string\n");
    exit(0);
  }

  parse_query_string(query_string);

  if (0 > size) {
    printf("Content-type: text/plain\r\n");
    printf("\r\n");
    printf("Error: no size specified\n");
    exit(0);
  }

  printf("Content-type: text/plain\r\n");
  printf("Content-length: %d\r\n",size);
  printf("\r\n");

  struct timeval start;
  gettimeofday(&start,NULL);

  char data[BUFSIZE];
  memset(data,'x',BUFSIZE);
  int remaining = size;
  int written = 0;
  double speedms = ((double)speed)/1000.0;
  while (0 < remaining) {
    int cur = (remaining > BUFSIZE) ? BUFSIZE : remaining;
    fwrite(data,1,cur,stdout);
    remaining -= cur;
    written += cur;

    if (0 < speed) {
      struct timeval now;
      gettimeofday(&now,NULL);

      int elapsed = timeval_diffms(start,now);
      int expected = (int)(elapsed*speedms);
      if (written > expected) {
        int sleepto = (int)(written/speedms);
        int delayms = sleepto - elapsed;

        if (0 < delayms) {
          struct timespec sleep_time;
          sleep_time.tv_sec = delayms/1000;
          sleep_time.tv_nsec = (speed%1000)*1000000;
          nanosleep(&sleep_time,NULL);
        }
      }
    }
  }

  return 0;
}
