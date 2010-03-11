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
 * $Id: runtime.h 849 2009-01-22 01:01:00Z pmkelly $
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
#include <netinet/tcp.h>

#define BACKLOG 100
#define DEFAULT_MAX_THREADS 3

int comp_per_ms = 0;

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

int start_listening(int port)
{
  int yes = 1;
  struct sockaddr_in local_addr;
  int sock;

  if (0 > (sock = socket(AF_INET,SOCK_STREAM,0))) {
    perror("socket");
    return -1;
  }

  if (0 > setsockopt(sock,SOL_SOCKET,SO_REUSEADDR,&yes,sizeof(int))) {
    perror("setsockopt");
    close(sock);
    return -1;
  }

  local_addr.sin_family = AF_INET;
  local_addr.sin_port = htons(port);
  local_addr.sin_addr.s_addr = INADDR_ANY;
  memset(&local_addr.sin_zero,0,8);

  if (0 > bind(sock,(struct sockaddr*)&local_addr,sizeof(struct sockaddr))) {
    perror("bind");
    close(sock);
    return -1;
  }

  if (0 > listen(sock,BACKLOG)) {
    perror("listen");
    close(sock);
    return -1;
  }

  return sock;
}

pthread_mutex_t lock;
pthread_cond_t cond;
int nthreads = 0;

#define BUFSIZE 1024

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

#define CALIB_WORK 1000000000

long long get_micro_time()
{
  struct timeval tv;
  gettimeofday(&tv,NULL);
  return (long long)tv.tv_sec*1000000 + (long long)tv.tv_usec;
}

int calibrate()
{
  printf("Warmup\n");
  run(CALIB_WORK);
  long long start = get_micro_time();

  printf("Calibration\n");
  run(CALIB_WORK);
  long long end = get_micro_time();
  long long ms = (end-start)/1000;
  printf("%d work took %lldms\n",CALIB_WORK,ms);
  int comp_per_ms = CALIB_WORK/ms;
  printf("comp per ms = %d\n",comp_per_ms);

  printf("Testing\n");
  int i;
  for (i = 1; i <= 4096; i *= 2) {
    long long start = get_micro_time();
    run(comp_per_ms*i);
    long long end = get_micro_time();
    double ratio = (end-start)/(i*1000.0);
    printf("Goal=%dms Actual=%lldms (%.3f%% error)\n",
           i,(end-start)/1000,fabs(100.0-100.0*ratio));
  }

  return comp_per_ms;
}

void handle(int sock)
{
  /* Let client know connection has been accepted */
  char dot = '.';
  write(sock,&dot,1);

  /* Read all data from client */
  int alloc = 1024;
  int size = 0;
  char *data = (char*)malloc(alloc);
  int r;
  while (0 < (r = read(sock,&data[size],1024))) {
    alloc += r;
    size += r;
    data = (char*)realloc(data,alloc);
  }
  data[size] = '\0';

  /* Get value & computation time */
  int value;
  int ms;
  if (2 == sscanf(data,"%d %d",&value,&ms)) {

    /* Compute for requested time */
    run(comp_per_ms*ms);

    /* Send back value */
    snprintf(data,1024,"%d",value);
    write(sock,data,strlen(data));
  }

  free(data);
}

void *connection_handler(void *arg)
{
  int sock = (int)arg;
  handle(sock);
  close(sock);

  pthread_mutex_lock(&lock);
  nthreads--;
  pthread_cond_broadcast(&cond);
  pthread_mutex_unlock(&lock);

  return NULL;
}

void compute_service(int port, int direct, int maxthreads)
{
  int listensock;
  int sock;
  int nconnections = 0;
  struct timeval start;

  printf("maxthreads = %d\n",maxthreads);

  if (direct) {
    printf("Using single thread for all responses\n");
  }
  else {
    printf("Using separate threads for each response\n");
  }

  printf("COMP_PER_MS = %d\n",comp_per_ms);

  printf("Starting compute service on port %d\n",port);

  /* Create the listening socket */
  if (0 > (listensock = start_listening(port)))
    exit(-1);

  pthread_mutex_init(&lock,NULL);
  pthread_cond_init(&cond,NULL);

  /* Repeatedly accept connections from clients */
  printf("Waiting for connection...\n");
  while (1) {
    if (!direct) {
      /* Wait until there is a thread available */
      pthread_mutex_lock(&lock);
      while (nthreads >= maxthreads) {
        pthread_cond_wait(&cond,&lock);
      }
      nthreads++;
      pthread_mutex_unlock(&lock);
    }

    /* Accept the connection */
    if (0 > (sock = accept(listensock,NULL,0))) {
      perror("accept");
      exit(-1);
    }

    int yes = 1;
    if (0 > setsockopt(sock,IPPROTO_TCP,TCP_NODELAY,&yes,sizeof(int))) {
      perror("setsockopt TCP_NODELAY");
      exit(-1);
    }

    nconnections++;
    if (1 == nconnections) {
      gettimeofday(&start,NULL);
      printf("# connections = %d\n",nconnections);
    }
    else {
      struct timeval now;
      gettimeofday(&now,NULL);
      int ms = timeval_diffms(start,now);
      double rate = 1000.0*((double)nconnections)/ms;
      printf("# connections = %d, rate = %.3f c/s\n",nconnections,rate);
    }

    if (direct) {
      handle(sock);
      close(sock);
    }
    else {

      /* Start a new thread to handle the connection */
      pthread_t thread;
      if (0 != pthread_create(&thread,NULL,connection_handler,(void*)sock)) {
        perror("pthread_create");
        exit(-1);
      }
      if (0 != pthread_detach(thread)) {
        perror("pthread_detach");
        exit(-1);
      }
    }
  }

  pthread_cond_destroy(&cond);
  pthread_mutex_destroy(&lock);
}

void usage()
{
  fprintf(stderr,"Usage: svc_compute OPTIONS <port>\n");
  fprintf(stderr,"       svc_compute -calibrate\n");
  fprintf(stderr,"Options:\n");
  fprintf(stderr,"-m N    Set maximum no.of threads to N (default %d)\n",DEFAULT_MAX_THREADS);
  exit(1);
}

int main(int argc, char **argv)
{
  int port = -1;
  int maxthreads = DEFAULT_MAX_THREADS;

  setbuf(stdout,NULL);

  int argpos;
  for (argpos = 1; argpos < argc; argpos++) {
    if (!strcmp(argv[argpos],"-calibrate")) {
      calibrate();
      exit(0);
    }
    else if (!strcmp(argv[argpos],"-m")) {
      if (argpos+1 >= argc)
        usage();
      char *end = NULL;
      maxthreads = strtol(argv[++argpos],&end,10);
      if (('\0' == argv[1][0]) || ('\0' != *end)) {
        fprintf(stderr,"Invalid max. thread count: %s\n",argv[argpos]);
        exit(1);
      }

    }
    else {
      char *end = NULL;
      port = strtol(argv[argpos],&end,10);
      if (('\0' == argv[1][0]) || ('\0' != *end)) {
        fprintf(stderr,"Invalid port number: %s\n",argv[argpos]);
        exit(1);
      }
    }
  }

  if (0 > port)
    usage();

  comp_per_ms = calibrate();

  int direct = (getenv("DIRECT") != NULL);
  compute_service(port,direct,maxthreads);

  return 0;
}
