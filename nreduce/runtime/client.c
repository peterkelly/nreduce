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
#include "network.h"
#include "runtime.h"
#include "node.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <assert.h>
#include <ctype.h>
#include <stdarg.h>
#include <math.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/select.h>
#include <sys/time.h>
#include <time.h>
#include <fcntl.h>
#include <unistd.h>
#include <signal.h>
#include <execinfo.h>

extern const char *prelude;

static void get_responses(endpoint *endpt, int tag,
                          int count, endpointid *managerids, int *responses)
{
  int *gotresponse = (int*)calloc(count,sizeof(int));
  int allresponses;
  int i;
  do {
    message *msg = endpoint_next_message(endpt,-1);
    int sender = -1;
    assert(msg);

    if (tag != msg->hdr.tag)
      fatal("%s: Got invalid response tag: %d\n",msg_names[tag],msg->hdr.tag);

    for (i = 0; i < count; i++)
      if (!memcmp(&msg->hdr.source,&managerids[i],sizeof(endpointid)))
        sender = i;

    if (0 > sender) {
      fprintf(stderr,"%s: Got response from unknown source ",msg_names[tag]);
      print_endpointid(stderr,msg->hdr.source);
      fprintf(stderr,"\n");
      abort();
    }

    if (sizeof(int) != msg->hdr.size)
      fatal("%s: incorrect message size (%d)\n",msg->hdr.size,msg_names[tag]);

    if (gotresponse[sender])
      fatal("%s: Already have response for this source\n",msg_names[tag]);

    gotresponse[sender] = 1;
    if (responses)
      responses[sender] = *(int*)msg->data;

    allresponses = 1;
    for (i = 0; i < count; i++)
      if (!gotresponse[i])
        allresponses = 0;

    free(msg->data);
    free(msg);
  } while (!allresponses);

  free(gotresponse);
}

typedef struct {
  node *n;
  char *bcdata;
  int bcsize;
  int count;
  endpointid *managerids;
  pthread_t thread;
} startarg;

/* FIXME: handle the case where a shutdown request occurs while this is running */
static void *start_task(void *arg)
{
  startarg *sa = (startarg*)arg;
  node *n = sa->n;
  int count = sa->count;
  endpointid *managerids = sa->managerids;
  endpointid *endpointids = (endpointid*)calloc(count,sizeof(endpointid));
  int *localids = (int*)calloc(count,sizeof(int));
  char *bcdata = sa->bcdata;
  int bcsize = sa->bcsize;
  int ntsize = sizeof(newtask_msg)+bcsize;
  newtask_msg *ntmsg = (newtask_msg*)calloc(1,ntsize);
  int initsize = sizeof(inittask_msg)+count*sizeof(endpointid);
  inittask_msg *initmsg = (inittask_msg*)calloc(1,initsize);
  int i;

  endpoint *endpt = node_add_endpoint(n,0,LAUNCHER_ENDPOINT,arg);

  printf("start_task: before sleep\n");
  sleep(3);
  printf("start_task: after sleep\n");

  ntmsg->groupsize = count;
  ntmsg->bcsize = bcsize;
  memcpy(&ntmsg->bcdata,bcdata,bcsize);

  for (i = 0; i < count; i++) {
    unsigned char *ipbytes = (unsigned char*)&managerids[i].nodeip.s_addr;
    printf("start_task(): manager %u.%u.%u.%u:%d %d\n",
           ipbytes[0],ipbytes[1],ipbytes[2],ipbytes[3],managerids[i].nodeport,
           managerids[i].localid);

    endpointids[i].nodeip = managerids[i].nodeip;
    endpointids[i].nodeport = managerids[i].nodeport;
  }

  /* Send NEWTASK messages, containing the pid, groupsize, and bytecode */
  for (i = 0; i < count; i++) {
    ntmsg->pid = i;
    node_send(n,endpt,managerids[i],MSG_NEWTASK,(char*)ntmsg,ntsize);
  }

  /* Get NEWTASK responses, containing the local id of each task on the remote node */
  get_responses(endpt,MSG_NEWTASKRESP,count,managerids,localids);
  for (i = 0; i < count; i++)
    endpointids[i].localid = localids[i];
  printf("All tasks created\n");

  /* Send INITTASK messages, giving each task a copy of the idmap */
  initmsg->count = count;
  memcpy(initmsg->idmap,endpointids,count*sizeof(endpointid));
  for (i = 0; i < count; i++) {
    initmsg->localid = endpointids[i].localid;
    node_send(n,endpt,managerids[i],MSG_INITTASK,(char*)initmsg,initsize);
  }

  /* Wait for all INITTASK messages to be processed */
  get_responses(endpt,MSG_INITTASKRESP,count,managerids,NULL);
  printf("All tasks initialised\n");

  /* Start all tasks */
  for (i = 0; i < count; i++)
    node_send(n,endpt,managerids[i],MSG_STARTTASK,&localids[i],sizeof(int));

  /* Wait for notification of all tasks starting */
  get_responses(endpt,MSG_STARTTASKRESP,count,managerids,NULL);
  printf("All tasks started\n");

  printf("Distributed process creation done\n");

  free(bcdata);
  free(ntmsg);
  free(initmsg);
  free(managerids);
  free(endpointids);
  free(localids);
  node_remove_endpoint(n,endpt);
  free(arg);
  return NULL;
}

void start_task_using_manager(node *n, const char *bcdata, int bcsize,
                              endpointid *managerids, int count)
{
  startarg *arg = (startarg*)calloc(1,sizeof(startarg));
  pthread_t startthread;
  arg->n = n;
  arg->bcsize = bcsize;
  arg->bcdata = malloc(bcsize);
  arg->count = count;
  arg->managerids = malloc(count*sizeof(endpointid));
  memcpy(arg->managerids,managerids,count*sizeof(endpointid));
  memcpy(arg->bcdata,bcdata,bcsize);
  printf("Distributed process creation starting\n");
  if (0 > wrap_pthread_create(&startthread,NULL,start_task,arg))
    fatal("pthread_create: %s",strerror(errno));

/*   if (0 > wrap_pthread_join(startthread,NULL)) */
/*     fatal("pthread_join: %s",strerror(errno)); */
/*   printf("Distributed process creation done\n"); */

  if (0 > pthread_detach(startthread))
    fatal("pthread_detach: %s",strerror(errno));
  printf("Distributed process creation started\n");
}

int get_managerids(node *n, endpointid **managerids)
{
  int count = 1;
  int i = 0;
  connection *conn;

  if (!n->havelistenip)
    fatal("I don't have my listen IP yet!");

  for (conn = n->connections.first; conn; conn = conn->next)
    if (0 <= conn->port)
      count++;

  *managerids = (endpointid*)calloc(count,sizeof(endpointid));

  (*managerids)[i].nodeip = n->listenip;
  (*managerids)[i].nodeport = n->listenport;
  (*managerids)[i].localid = MANAGER_ID;
  i++;

  for (conn = n->connections.first; conn; conn = conn->next) {
    if (0 <= conn->port) {
      (*managerids)[i].nodeip = conn->ip;
      (*managerids)[i].nodeport = conn->port;
      (*managerids)[i].localid = MANAGER_ID;
      i++;
    }
  }

  return count;
}

int run_program(node *n, const char *filename)
{
  int bcsize;
  char *bcdata;
  source *src;
  int count;
  endpointid *managerids;

  src = source_new();
  if (0 != source_parse_string(src,prelude,"prelude.l"))
    return -1;
  if (0 != source_parse_file(src,filename,""))
    return -1;
  if (0 != source_process(src,0,0))
    return -1;
  if (0 != source_compile(src,&bcdata,&bcsize))
    return -1;
  source_free(src);

  count = get_managerids(n,&managerids);

  printf("Compiled\n");
  start_task_using_manager(n,bcdata,bcsize,managerids,count);
  free(managerids);
  free(bcdata);
  return 0;
}
