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
 * $Id: gmachine.c 333 2006-08-24 05:00:25Z pmkelly $
 *
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "grammar.tab.h"
#include "gcode.h"
#include "nreduce.h"
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

#ifndef USE_MPI
static int send_around(process *proc, char tag)
{
  array *wr;
  int from;
  int rtag;
  char *data;
  int size;
  reader rd;

  wr = write_start();
  msg_send(proc,0,tag,wr->data,wr->nbytes);
  write_end(wr);

  from = msg_recvb(proc,&rtag,&data,&size);
  assert(0 <= from);

  rd = read_start(data,size);
  if (tag != rtag) {
    free(data);
    return READER_INCORRECT_CONTENTS;
  }

  free(data);
  return 0;
}

static int homegc(process *proc)
{
  int i;
  int *count = (int*)calloc(proc->grp->nprocs,sizeof(int));
  int sweeps;
  char *data;
  int size;
  int from;
  int tag;
  int r;
  reader rd;

  r = send_around(proc,MSG_STARTDISTGC);
  assert(READER_OK == r);
  fprintf(proc->output,"All processes started distributed garbage collection\n");

  for (i = 0; i < proc->grp->nprocs-1; i++) {
    msg_fsend(proc,i,MSG_MARKROOTS,"");
    count[i]++;
  }
  while (1) {
    int done = 0;

    from = msg_recvb(proc,&tag,&data,&size);
    assert(0 <= from);
    rd = read_start(data,size);
    assert(READER_OK == r);
    assert(MSG_UPDATE == tag);

/*     count[from]--; */
    for (i = 0; i < proc->grp->nprocs-1; i++) {
      int c;
      r = read_int(&rd,&c);
      assert(READER_OK == r);
      count[i] += c;
    }

    #ifdef DISTGC_DEBUG
    printf("after update from %d:",from);
    for (i = 0; i < proc->grp->nprocs-1; i++)
      printf(" %d",count[i]);
    printf("\n");
    #endif

    done = 1;
    for (i = 0; i < proc->grp->nprocs-1; i++)
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












  for (sweeps = 0; sweeps < proc->grp->nprocs-1; sweeps++)
    msg_fsend(proc,sweeps,MSG_SWEEP,"");

  while (0 < sweeps) {
    from = msg_recvb(proc,&tag,&data,&size);
    assert(0 <= from);
    rd = read_start(data,size);
    assert(READER_OK == r);
    assert(MSG_SWEEPACK == tag);
    sweeps--;
  }
  fprintf(proc->output,"All processes completed distributed garbage collection\n");

  free(count);
  return READER_OK;
}

static int handle_command(process *proc, const char *line, int *done)
{
  int r = READER_OK;
  if (!strcmp(line,"gc")) {
    r = homegc(proc);
    assert(READER_OK == r);
  }
  else if (!strcmp(line,"dump")) {
    r = send_around(proc,MSG_DUMP_INFO);
    if (READER_OK == r)
      printf("All processes dumped info\n");
  }
  else if (!strcmp(line,"test")) {
    r = send_around(proc,MSG_TEST);
    if (READER_OK == r)
      printf("All processes handled test message\n");
  }
  else if (!strcmp(line,"pause")) {
    r = send_around(proc,MSG_PAUSE);
    if (READER_OK == r)
      printf("All processes paused\n");
  }
  else if (!strcmp(line,"resume")) {
    r = send_around(proc,MSG_RESUME);
    if (READER_OK == r)
      printf("All processes resumed\n");
  }
  else if (!strcmp(line,"allstats")) {
    int op;
    int from;
    int tag;
    char *data;
    int size;
    array *wr;
    int totalallocs;
    reader rd;

    wr = write_start();
    write_int(wr,0);
    for (op = 0; op < OP_COUNT; op++)
      write_int(wr,0);
    msg_send(proc,0,MSG_ALLSTATS,wr->data,wr->nbytes);
    write_end(wr);

    from = msg_recvb(proc,&tag,&data,&size);
    printf("received %d bytes from %d\n",size,from);

    rd = read_start(data,size);
    if (MSG_ALLSTATS != tag)
      return READER_INCORRECT_CONTENTS;

    CHECK_READ(read_int(&rd,&totalallocs));
    printf("totalallocs %d\n",totalallocs);
    for (op = 0; op < OP_COUNT; op++) {
      int usage;
      CHECK_READ(read_int(&rd,&usage));
      printf("%-12s",op_names[op]);
      printf("  %-10d",usage);
      printf("\n");
    }
    CHECK_READ(read_end(&rd));
  }
  else if (!strcmp(line,"stats")) {
    int i;
    int count = proc->grp->nprocs-1;
    int op;

    int *totalallocs = (int*)calloc(count,sizeof(int));
    int *nglobals = (int*)calloc(count,sizeof(int));
    int *op_usage = (int*)calloc(count,OP_COUNT*sizeof(int));
    int *sendcount = (int*)calloc(count,MSG_COUNT*sizeof(int));
    int *sendbytes = (int*)calloc(count,MSG_COUNT*sizeof(int));
    int *recvcount = (int*)calloc(count,MSG_COUNT*sizeof(int));
    int *recvbytes = (int*)calloc(count,MSG_COUNT*sizeof(int));
    int from;
    int tag;
    int msg;

    for (i = 0; i < count; i++) {
      array *wr = write_start();
      msg_send(proc,i,MSG_ISTATS,wr->data,wr->nbytes);
      write_end(wr);
    }

    for (i = 0; i < count; i++) {
      char *data;
      int size;
      reader rd;
      int tmp;

      from = msg_recvb(proc,&tag,&data,&size);
      printf("received %d bytes from %d\n",size,from);

      rd = read_start(data,size);
      if (MSG_ISTATS != tag)
        return READER_INCORRECT_CONTENTS;
      CHECK_READ(read_int(&rd,&totalallocs[from]));
      CHECK_READ(read_int(&rd,&nglobals[from]));

      for (op = 0; op < OP_COUNT; op++) {
        CHECK_READ(read_int(&rd,&tmp));
        op_usage[from*OP_COUNT+op] = tmp;
      }

      for (msg = 0; msg < MSG_COUNT; msg++) {
        CHECK_READ(read_int(&rd,&tmp));
        sendcount[from*MSG_COUNT+msg] = tmp;
      }

      for (msg = 0; msg < MSG_COUNT; msg++) {
        CHECK_READ(read_int(&rd,&tmp));
        sendbytes[from*MSG_COUNT+msg] = tmp;
      }

      for (msg = 0; msg < MSG_COUNT; msg++) {
        CHECK_READ(read_int(&rd,&tmp));
        recvcount[from*MSG_COUNT+msg] = tmp;
      }

      for (msg = 0; msg < MSG_COUNT; msg++) {
        CHECK_READ(read_int(&rd,&tmp));
        recvbytes[from*MSG_COUNT+msg] = tmp;
      }

      CHECK_READ(read_end(&rd));
    }

    printf("                  ");
    for (from = 0; from < count; from++)
      printf("  Process %-2d      ",from);
    printf("\n");

    printf("                  ");
    for (from = 0; from < count; from++)
      printf("  ----------------");
    printf("\n");

    printf("totalallocs       ");
    for (from = 0; from < count; from++)
      printf("  %-16d",totalallocs[from]);
    printf("\n");

    printf("globals           ");
    for (from = 0; from < count; from++)
      printf("  %-16d",nglobals[from]);
    printf("\n");

    for (op = 0; op < OP_COUNT; op++) {
      printf("%-18s",op_names[op]);
      for (from = 0; from < count; from++)
        printf("  %-16d",op_usage[from*OP_COUNT+op]);
      printf("\n");
    }

    /* message totals */

    printf("send count        ");
    for (from = 0; from < count; from++) {
      int total = 0;
      for (msg = 0; msg < MSG_COUNT; msg++)
        total += sendcount[from*MSG_COUNT+msg];
      printf("  %-16d",total);
    }
    printf("\n");

    printf("send bytes        ");
    for (from = 0; from < count; from++) {
      int total = 0;
      for (msg = 0; msg < MSG_COUNT; msg++)
        total += sendbytes[from*MSG_COUNT+msg];
      printf("  %-16d",total);
    }
    printf("\n");

    printf("recv count        ");
    for (from = 0; from < count; from++) {
      int total = 0;
      for (msg = 0; msg < MSG_COUNT; msg++)
        total += recvcount[from*MSG_COUNT+msg];
      printf("  %-16d",total);
    }
    printf("\n");

    printf("recv bytes        ");
    for (from = 0; from < count; from++) {
      int total = 0;
      for (msg = 0; msg < MSG_COUNT; msg++)
        total += recvbytes[from*MSG_COUNT+msg];
      printf("  %-16d",total);
    }
    printf("\n");

    /* individual messages */

    for (msg = 0; msg < MSG_COUNT; msg++) {
      printf("%-18s",msg_names[msg]);
      for (from = 0; from < count; from++) {
        char str[19];
        snprintf(str,19,"  %d/%d",sendcount[from*MSG_COUNT+msg],recvcount[from*MSG_COUNT+msg]);
        printf("%-18s",str);
      }
      printf("\n");
    }

    free(totalallocs);
    free(nglobals);
    free(op_usage);
    free(sendcount);
    free(sendbytes);
    free(recvcount);
    free(recvbytes);
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

void console(process *proc)
{
  printf("Console started\n");
  array *input = array_new(sizeof(char));
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
    homegc(proc);
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

    if (READER_OK != handle_command(proc,line,&done)) {
      printf("Error reading data\n");
    }
  }
  while (!done && (EOF != c));

  printf("\nConsole done\n");
  array_free(input);

  for (i = 0; i < proc->grp->nprocs-1; i++) {
    wr = write_start();
    msg_send(proc,i,MSG_DONE,wr->data,wr->nbytes);
    write_end(wr);
  }
}
#endif
