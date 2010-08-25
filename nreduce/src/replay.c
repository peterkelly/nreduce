/*
 * This file is part of the NReduce project
 * Copyright (C) 2009-2010 Peter Kelly <kellypmk@gmail.com>
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

typedef struct rglobal {
  gaddr addr;
  struct rglobal *prev;
  struct rglobal *next;
  struct rglobal *addrnext;
} rglobal;

typedef struct rglobal_list {
  rglobal *first;
  rglobal *last;
} rglobal_list;

typedef struct rtask {
  rglobal_list globals;
  rglobal *addrhash[GLOBAL_HASH_SIZE];
} rtask;

typedef struct {
  rtask *tasks;

  int groupsize;
  int fds[MAX_TASKS];
  event *current_event[MAX_TASKS];
  event *next_event[MAX_TASKS];
  int global_ts;
  int nsweeping;
} replay;

rglobal *lookup_rglobal(rtask *tsk, gaddr addr)
{
  int h = hash(&addr,sizeof(gaddr));
  rglobal *g = tsk->addrhash[h];
  while (g && ((g->addr.tid != addr.tid) || (g->addr.lid != addr.lid)))
    g = g->addrnext;
  return g;
}

void add_rglobal(rtask *tsk, gaddr addr)
{
  assert(NULL == lookup_rglobal(tsk,addr));
  rglobal *rg = (rglobal*)calloc(1,sizeof(rglobal));
  rg->addr = addr;
  llist_append(&tsk->globals,rg);
  int h = hash(&addr,sizeof(gaddr));
  rg->addrnext = tsk->addrhash[h];
  tsk->addrhash[h] = rg;
}

void remove_rglobal(rtask *tsk, rglobal *glo)
{
  assert(lookup_rglobal(tsk,glo->addr) == glo);

  int h = hash(&glo->addr,sizeof(gaddr));
  rglobal **ptr = &tsk->addrhash[h];

  assert(glo);
  while (*ptr && (*ptr != glo))
    ptr = &(*ptr)->addrnext;
  if (*ptr == glo) {
    *ptr = glo->addrnext;
    glo->addrnext = NULL;
  }

  llist_remove(&tsk->globals,glo);
}

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
  rpl->tasks = (rtask*)calloc(rpl->groupsize,sizeof(rtask));
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

  if (0 <= earliest_tid) {
    memcpy(rpl->current_event[earliest_tid],rpl->next_event[earliest_tid],max_event_size);
    read_next_event(rpl,earliest_tid);
    return rpl->current_event[earliest_tid];
  }
  else {
    return NULL;
  }
}

int owner_has_global(replay *rpl, gaddr addr)
{
  return (NULL != lookup_rglobal(&rpl->tasks[addr.tid],addr));
}

int others_have_global(replay *rpl, gaddr addr)
{
  int tid;
  for (tid = 0; tid < rpl->groupsize; tid++) {
    if ((tid != addr.tid) &&
        (NULL != lookup_rglobal(&rpl->tasks[tid],addr)))
      return 1;
  }
  return 0;
}

int check_all_globals(replay *rpl)
{
  int tid;
  for (tid = 0; tid < rpl->groupsize; tid++) {
    rglobal *glo;
    for (glo = rpl->tasks[tid].globals.first; glo; glo = glo->next) {
      if ((glo->addr.tid != tid) &&
          !owner_has_global(rpl,glo->addr)) {
        printf("Global %d@%d referenced by task %d is not present in owner\n",
               glo->addr.lid,glo->addr.tid,tid);
        return 0;
      }
    }
  }
  printf("check_all_globals: ok\n");
  return 1;
}

int process_event(replay *rpl, event *evt)
{
  switch (evt->type) {
  case EV_TASK_START:
    /* FIXME */
    break;
  case EV_TASK_END:
    /* FIXME */
    break;
  case EV_SEND_FISH:
    /* FIXME */
    break;
  case EV_RECV_FISH:
    /* FIXME */
    break;
  case EV_SEND_FETCH:
    /* FIXME */
    break;
  case EV_RECV_FETCH:
    /* FIXME */
    break;
  case EV_RECV_MARKROOTS:
    /* FIXME */
    break;
  case EV_SEND_MARKENTRY:
    /* FIXME */
    break;
  case EV_RECV_MARKENTRY:
    /* FIXME */
    break;
  case EV_RECV_SWEEP:
    rpl->nsweeping++;
    break;
  case EV_SEND_SWEEPACK:
    rpl->nsweeping--;
    if ((0 == rpl->nsweeping) && !check_all_globals(rpl))
      return 0;
    break;
  case EV_SEND_RESPOND:
    /* FIXME */
    break;
  case EV_RECV_RESPOND:
    /* FIXME */
    break;
  case EV_SEND_SCHEDULE:
    /* FIXME */
    break;
  case EV_RECV_SCHEDULE:
    /* FIXME */
    break;
  case EV_RECV_STARTDISTGC:
    /* FIXME */
    break;
  case EV_RECV_PAUSE:
    /* FIXME */
    break;
  case EV_SEND_GOTPAUSE:
    /* FIXME */
    break;
  case EV_RECV_GOTPAUSE:
    /* FIXME */
    break;
  case EV_SEND_PAUSEACK:
    /* FIXME */
    break;
  case EV_RECV_RESUME:
    /* FIXME */
    break;
  case EV_ADD_GLOBAL: {
    ev_add_global *ev = (ev_add_global*)evt;

    if (ev->addr.tid != evt->tid) {
      /* Address of object in remote task */
      if (NULL == lookup_rglobal(&rpl->tasks[ev->addr.tid],ev->addr)) {
        printf("Target %d@%d does not have corresponding entry in owner task\n",
               ev->addr.lid,ev->addr.tid);
        return 0;
      }
    }
    else {
      /* Address of object in local task */
    }

    add_rglobal(&rpl->tasks[evt->tid],ev->addr);
    return 1;
  }
  case EV_REMOVE_GLOBAL: {
    ev_remove_global *ev = (ev_remove_global*)evt;
    rglobal *glo = lookup_rglobal(&rpl->tasks[evt->tid],ev->addr);
    if (NULL == glo) { /* global not found */
      printf("Global does not exist\n");
      return 0;
    }
    remove_rglobal(&rpl->tasks[evt->tid],glo);
    return 1;
  }
  case EV_KEEP_GLOBAL:
    /* FIXME */
    break;
  case EV_PRESERVE_TARGET:
    /* FIXME */
    break;
  case EV_GOT_REMOTEREF:
    /* FIXME */
    break;
  case EV_GOT_OBJECT:
    /* FIXME */
    break;
  }
  return 1;
}

void main_loop(replay *rpl)
{
  event *evt;
  while (NULL != (evt = replay_next_event(rpl))) {
    print_event(stdout,evt);
    if (!process_event(rpl,evt)) {
      printf("Failed\n");
      break;
    }
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

  return 0;
}
