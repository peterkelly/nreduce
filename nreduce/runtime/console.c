/*
 * This file is part of the GridXSLT project
 * Copyright (C) 2006 Peter Kelly (pmk@cs.adelaide.edu.au)
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

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "compiler/bytecode.h"
#include "src/nreduce.h"
#include "runtime.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <assert.h>
#include <ctype.h>
#include <stdarg.h>
#include <math.h>
#include <errno.h>
#include <sys/time.h>
#include <time.h>

static int send_around(task *tsk, char tag)
{
  array *wr;
  int from;
  int rtag;
  char *data;
  int size;
  reader rd;

  wr = write_start();
  msg_send(tsk,0,tag,wr->data,wr->nbytes);
  write_end(wr);

  from = msg_recv(tsk,&rtag,&data,&size,-1);
  assert(0 <= from);

  rd = read_start(data,size);
  if (tag != rtag) {
    free(data);
    return READER_INCORRECT_CONTENTS;
  }

  free(data);
  return 0;
}

static int homegc(task *tsk)
{
  int i;
  int *count = (int*)calloc(tsk->grp->nprocs,sizeof(int));
  int sweeps;
  char *data;
  int size;
  int from;
  int tag;
  int r;
  reader rd;

  r = send_around(tsk,MSG_STARTDISTGC);
  assert(READER_OK == r);
  fprintf(tsk->output,"All tasks started distributed garbage collection\n");

  for (i = 0; i < tsk->grp->nprocs-1; i++) {
    msg_fsend(tsk,i,MSG_MARKROOTS,"");
    count[i]++;
  }
  while (1) {
    int done = 0;

    from = msg_recv(tsk,&tag,&data,&size,-1);
    assert(0 <= from);
    rd = read_start(data,size);
    assert(READER_OK == r);
    assert(MSG_UPDATE == tag);

/*     count[from]--; */
    for (i = 0; i < tsk->grp->nprocs-1; i++) {
      int c;
      r = read_int(&rd,&c);
      assert(READER_OK == r);
      count[i] += c;
    }

    #ifdef DISTGC_DEBUG
    printf("after update from %d:",from);
    for (i = 0; i < tsk->grp->nprocs-1; i++)
      printf(" %d",count[i]);
    printf("\n");
    #endif

    done = 1;
    for (i = 0; i < tsk->grp->nprocs-1; i++)
      if (count[i])
        done = 0;

    if (done)
      break;
  }
  #ifdef DISTGC_DEBUG
  printf("done marking\n");
  #endif

#if 0
    struct timespec time;
    time.tv_sec = 0;
    time.tv_nsec = 250*1000*1000;
/*     time.tv_nsec = 100*1000*1000; */
/*     time.tv_nsec =  10*1000*1000; */
    nanosleep(&time,NULL);
#endif












  for (sweeps = 0; sweeps < tsk->grp->nprocs-1; sweeps++)
    msg_fsend(tsk,sweeps,MSG_SWEEP,"");

  while (0 < sweeps) {
    from = msg_recv(tsk,&tag,&data,&size,-1);
    assert(0 <= from);
    rd = read_start(data,size);
    assert(READER_OK == r);
    assert(MSG_SWEEPACK == tag);
    sweeps--;
  }
  fprintf(tsk->output,"All tasks completed distributed garbage collection\n");

  free(count);
  return READER_OK;
}

static int handle_command(task *tsk, const char *line, int *done)
{
  int r = READER_OK;
  if (!strcmp(line,"gc")) {
    r = homegc(tsk);
    assert(READER_OK == r);
  }
  else if (!strcmp(line,"exit") || !strcmp(line,"quit") || !strcmp(line,"q")) {
    *done = 1;
    return 0;
  }
  else if (0 < strlen(line)) {
    printf("Unknown command: %s\n",line);
  }
  return r;
}

void console(task *tsk)
{
  printf("Console started\n");
  array *input = array_new(sizeof(char),0);
  int c;
  int i;
  char c2;
  char *line;
  array *wr;
  int done = 0;

  #ifdef CONTINUOUS_DISTGC
  while (1) {
    struct timespec time;
    time.tv_sec = 0;
    time.tv_nsec = 250*1000*1000;
/*     time.tv_nsec = 100*1000*1000; */
/*     time.tv_nsec =  10*1000*1000; */
    nanosleep(&time,NULL);
    homegc(tsk);
  }
  #endif

  do {
    printf("> ");

    input->nbytes = 0;
    while ((EOF != (c = fgetc(stdin))) && ('\n' != c)) {
      c2 = (char)c;
      array_append(input,&c2,1);
    }

    c2 = '\0';
    array_append(input,&c2,1);
    line = (char*)input->data;

    if (READER_OK != handle_command(tsk,line,&done)) {
      printf("Error reading data\n");
    }
  }
  while (!done && (EOF != c));

  printf("\nConsole done\n");
  array_free(input);

  for (i = 0; i < tsk->grp->nprocs-1; i++) {
    wr = write_start();
    msg_send(tsk,i,MSG_DONE,wr->data,wr->nbytes);
    write_end(wr);
  }
}
