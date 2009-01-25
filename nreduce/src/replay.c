/*
 * This file is part of the nreduce project
 * Copyright (C) 2009 Peter Kelly (pmk@cs.adelaide.edu.au)
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

#include "runtime/runtime.h"
#include "runtime/events.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>
#include <limits.h>
#include <fcntl.h>
#include <errno.h>
#include <unistd.h>
#include <sys/types.h>

#define MAX_TASKS 256

int max_event_size;

typedef struct {
  int groupsize;
  int fds[MAX_TASKS];
/*   int local_ts[MAX_TASKS]; */
  int next_ts[MAX_TASKS];
  event *current_event[MAX_TASKS];
  event *next_event[MAX_TASKS];
  int global_ts;
} replay;

void replay_open_logs(replay *rpl, const char *events_dir)
{
  DIR *dir;
  if (NULL == (dir = opendir(events_dir))) {
    perror(events_dir);
    exit(1);
  }

  rpl->groupsize = 0;
  while (MAX_TASKS > rpl->groupsize) {
    char filename[PATH_MAX];
    snprintf(filename,PATH_MAX,"%s/events.%d",events_dir,rpl->groupsize);
    int fd;
    if (0 <= (fd = open(filename,O_RDONLY))) {
      /* Log file exists for this tid */
      rpl->fds[rpl->groupsize] = fd;
      rpl->current_event[rpl->groupsize] = (event*)calloc(1,max_event_size);
      rpl->next_event[rpl->groupsize] = (event*)calloc(1,max_event_size);
      rpl->groupsize++;
    }
    else if (ENOENT == errno) {
      /* No more log file */
      break;
    }
    else {
      /* Error accessing log file */
      perror(filename);
      exit(1);
    }
  }

  closedir(dir);

  if (0 == rpl->groupsize) {
    fprintf(stderr,"Could not find any events.* log files in %s\n",events_dir);
    exit(1);
  }
}

void replay_process_headers(replay *rpl)
{
  int tid;
  int have_key = 0;
  int key = 0;
  for (tid = 0; tid < rpl->groupsize; tid++) {
    events_header eh;
    int r = read(rpl->fds[tid],&eh,sizeof(eh));
    if (0 > r) {
      fprintf(stderr,"events.%d: read: %s\n",tid,strerror(errno));
      exit(1);
    }
    else if (sizeof(eh) > r) {
      fprintf(stderr,"events.%d: truncated header\n",tid);
      exit(1);
    }
    else if (EVENTS_VERSION != eh.version) {
      fprintf(stderr,"events.%d: incorrect version (%d, expected %d)\n",
              tid,eh.version,EVENTS_VERSION);
      exit(1);
    }
    else if (eh.groupsize != rpl->groupsize) {
      fprintf(stderr,"events.%d: incorrect group size (%d, expected %d)\n",
              tid,eh.groupsize,rpl->groupsize);
      exit(1);
    }
    else if (eh.tid != tid) {
      fprintf(stderr,"events.%d: incorrect tid (%d, expected %d)\n",
              tid,eh.tid,tid);
      exit(1);
    }
    else if (have_key && (eh.key != key)) {
      fprintf(stderr,"events.%d: incorrect key (%d, expected %d)\n",
              tid,eh.key,key);
      exit(1);
    }
    else {
      /* Header ok */
      printf("events.%d: header ok\n",tid);
      key = eh.key;
      have_key = 1;
    }
  }
  printf("All headers ok, version=%d, groupsize=%d, key=%d\n",
         EVENTS_VERSION,rpl->groupsize,key);
}

void read_next_event(replay *rpl, int tid)
{
  /* Read first part of event, containing the fields common to all events */
  int r = read(rpl->fds[tid],rpl->next_event[tid],sizeof(event));

  if (0 > r) {
    fprintf(stderr,"events.%d: read: %s\n",tid,strerror(errno));
    exit(1);
  }
  else if (0 == r) {
    rpl->next_event[tid]->timestamp = TIMESTAMP_MAX;
    rpl->next_event[tid]->type = EV_NONE;
    rpl->next_event[tid]->tid = tid;
/*     printf("read_next_event %d: no more events\n",tid); */
    return;
  }
  else if (sizeof(event) != r) {
    fprintf(stderr,"events.%d: truncated event (while reading timestamp)\n",tid);
    exit(1);
  }

  if (0 == event_sizes[rpl->next_event[tid]->type]) {
    fprintf(stderr,"events.%d: unimplemented event type %d\n",tid,rpl->next_event[tid]->type);
    exit(1);
  }

  /* Read second prt of event, containing type-specific fields */
  char *remaddr = ((char*)rpl->next_event[tid])+sizeof(event);
  int size = event_sizes[rpl->next_event[tid]->type]-sizeof(event);
  r = read(rpl->fds[tid],remaddr,size);
  if (0 > r) {
    fprintf(stderr,"events.%d: read: %s\n",tid,strerror(errno));
    exit(1);
  }
  else if (size > r) {
    fprintf(stderr,"events.%d: truncated event (while reading event data, type %d)\n",
            tid,rpl->next_event[tid]->type);
    exit(1);
  }
  else {
    /* event read ok */
  }

/*   printf("read_next_event %d: type %d\n",tid,rpl->next_event[tid]->type); */
}

void read_initial_events(replay *rpl)
{
  int tid;
  for (tid = 0; tid < rpl->groupsize; tid++)
    read_next_event(rpl,tid);
}

/* Reads the next event from the log file with the earliest next timestamp, or in the
   case where multiple logs have the same timestamp, the lowest tid.
   The returned structure will be one of the ev_ types, and can be cased based on the
   type field. */
event *replay_next_event(replay *rpl)
{
  int tid;
  timestamp_t earliest = TIMESTAMP_MAX;
  int earliest_tid = -1;
  for (tid = 0; tid < rpl->groupsize; tid++) {
    if ((EV_NONE != rpl->next_event[tid]->type) &&
        ((0 > earliest_tid) || (earliest > rpl->next_event[tid]->timestamp))) {
      earliest_tid = tid;
      earliest = rpl->next_event[tid]->timestamp;
    }
  }

/*   printf("replay_next_event: earliest_tid = %d\n",earliest_tid); */

  if (0 <= earliest_tid) {
    memcpy(rpl->current_event[earliest_tid],rpl->next_event[earliest_tid],max_event_size);
    read_next_event(rpl,earliest_tid);
    return rpl->current_event[earliest_tid];
  }
  else {
    return NULL;
  }
}


void main_loop(replay *rpl)
{
  event *evt;
  while (NULL != (evt = replay_next_event(rpl))) {
    print_event(stdout,evt);
  }
}



int compute_max_event_size()
{
  int max = 0;
  int type;
  for (type = 0; type < EV_COUNT; type++) {
    if (max < event_sizes[type])
      max = event_sizes[type];
  }
  return max;
}

int main(int argc, char **argv)
{
  replay rpl;
  setbuf(stdout,NULL);
  memset(&rpl,0,sizeof(rpl));
  max_event_size = compute_max_event_size();
  printf("max event size = %d\n",compute_max_event_size());

  if (2 > argc) {
    fprintf(stderr,"Usage: replay <events_dir>\n");
    exit(1);
  }

  char *events_dir = argv[1];
  replay_open_logs(&rpl,events_dir);
  replay_process_headers(&rpl);
  read_initial_events(&rpl);
  main_loop(&rpl);

/*   char *prefix = "events."; */
/*   int prefixlen = strlen(prefix); */
/*   struct dirent *entry; */
/*   int maxtid = -1; */
/*   while (NULL != (entry = readdir(dir))) { */
/*     if (!strncmp(entry->d_name,prefix,prefixlen)) { */
/*       char *tidstr = &entry->d_name[prefixlen]; */
/*       char *endp = NULL; */
/*       int tid = strtol(tidstr,&end,10); */
/*       if (('\0' == *tidstr) || ('\0' != *endptr)) { */
/*         fprintf(stderr,"Bad events filename: %s\n",entry->d_name); */
/*         exit(1); */
/*       } */
/*     } */
/*   } */

  return 0;
}
