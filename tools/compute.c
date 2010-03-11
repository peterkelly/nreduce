/*
 * This file is part of the nreduce project
 * Copyright (C) 2008-2009 Peter Kelly (pmk@cs.adelaide.edu.au)
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 *
 * $Id$
 *
 */

#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <netdb.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <math.h>

void run(long long total)
{
  long long i;
  double a = 1;
  double b = 1;
  double c = 1;
  double d = 1;
  for (i = 0; i < total; i++) {
    a += 0.00001;
    b += a;
    c += b;
    d += c;
  }
}

long long get_micro_time()
{
  struct timeval tv;
  gettimeofday(&tv,NULL);
  return (long long)tv.tv_sec*1000000 + (long long)tv.tv_usec;
}

long long get_comp_per_ms()
{
  char *cpmstr = getenv("COMP_PER_MS");
  if (NULL == cpmstr) {
    fprintf(stderr,"Environment variable COMP_PER_MS not set; run with -calibrate to determine\n");
    exit(-1);
  }
  char *end = NULL;
  long long cpm = strtol(cpmstr,&end,10);
  if (('\0' == cpmstr[0]) || ('\0' != *end)) {
    fprintf(stderr,"Environment variable COMP_PER_MS invalid (\"%s\"); must be a number\n",
            cpmstr);
    exit(1);
  }
  return cpm;
}

int main(int argc, char **argv)
{
  setbuf(stdout,NULL);

  if (2 > argc) {
    fprintf(stderr,"Usage: compute <ms>\n");
    return -1;
  }

  int ms = atoi(argv[1]);
  long long comp_per_ms = get_comp_per_ms();

  long long start = get_micro_time();
  run(comp_per_ms*ms);
  long long end = get_micro_time();
  double ratio = (end-start)/(ms*1000.0);
  printf("Comp per ms=%lld, Goal=%dms Actual=%lldms (%.3f%%)\n",
         comp_per_ms,ms,(end-start)/1000,100.0*ratio);

  return 0;
}
