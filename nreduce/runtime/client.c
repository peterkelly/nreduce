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

static int get_responses(endpoint *endpt, int tag,
                         int count, endpointid *managerids, int *responses)
{
  int *gotresponse = (int*)calloc(count,sizeof(int));
  int allresponses;
  int i;
  do {
    message *msg = endpoint_next_message(endpt,-1);
    int sender = -1;

    if (NULL == msg) {
      free(gotresponse);
      return -1;
    }

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
  return 0;
}

void launcher_kill(launcher *sa)
{
  sa->cancel = 1;
  endpoint_forceclose(sa->endpt);

  if (0 > wrap_pthread_join(sa->thread,NULL))
    fatal("pthread_join: %s",strerror(errno));
}

static void launcher_free(node *n, launcher *sa)
{
  free(sa->endpointids);
  free(sa->localids);
  free(sa->bcdata);
  free(sa->managerids);
  node_remove_endpoint(n,sa->endpt);
  free(sa);
}

static void send_newtask(launcher *lr)
{
  int ntsize = sizeof(newtask_msg)+lr->bcsize;
  newtask_msg *ntmsg = (newtask_msg*)calloc(1,ntsize);
  int i;
  ntmsg->groupsize = lr->count;
  ntmsg->bcsize = lr->bcsize;
  memcpy(&ntmsg->bcdata,lr->bcdata,lr->bcsize);
  for (i = 0; i < lr->count; i++) {
    ntmsg->pid = i;
    node_send(lr->n,lr->endpt,lr->managerids[i],MSG_NEWTASK,(char*)ntmsg,ntsize);
  }
  free(ntmsg);
}

static void send_inittask(launcher *lr)
{
  int initsize = sizeof(inittask_msg)+lr->count*sizeof(endpointid);
  inittask_msg *initmsg = (inittask_msg*)calloc(1,initsize);
  int i;
  initmsg->count = lr->count;
  memcpy(initmsg->idmap,lr->endpointids,lr->count*sizeof(endpointid));
  for (i = 0; i < lr->count; i++) {
    initmsg->localid = lr->endpointids[i].localid;
    node_send(lr->n,lr->endpt,lr->managerids[i],MSG_INITTASK,(char*)initmsg,initsize);
  }
  free(initmsg);
}

static void send_starttask(launcher *lr)
{
  int i;
  for (i = 0; i < lr->count; i++)
    node_send(lr->n,lr->endpt,lr->managerids[i],MSG_STARTTASK,
              &lr->localids[i],sizeof(int));
}

static void *launcher_thread(void *arg)
{
  launcher *lr = (launcher*)arg;
  node *n = lr->n;
  int count = lr->count;
  int i;

  lr->endpointids = (endpointid*)calloc(count,sizeof(endpointid));
  lr->localids = (int*)calloc(count,sizeof(int));
  lr->endpt = node_add_endpoint(n,0,LAUNCHER_ENDPOINT,arg);

  for (i = 0; i < count; i++) {
    unsigned char *ipbytes = (unsigned char*)&lr->managerids[i].nodeip.s_addr;
    printf("Launcher: manager %u.%u.%u.%u:%d %d\n",
           ipbytes[0],ipbytes[1],ipbytes[2],ipbytes[3],lr->managerids[i].nodeport,
           lr->managerids[i].localid);

    lr->endpointids[i].nodeip = lr->managerids[i].nodeip;
    lr->endpointids[i].nodeport = lr->managerids[i].nodeport;
  }

  /* Send NEWTASK messages, containing the pid, groupsize, and bytecode */
  send_newtask(lr);

  /* Get NEWTASK responses, containing the local id of each task on the remote node */
  if (0 != get_responses(lr->endpt,MSG_NEWTASKRESP,count,lr->managerids,lr->localids)) {
    assert(lr->cancel);
    launcher_free(n,arg);
    return NULL;
  }
  for (i = 0; i < count; i++)
    lr->endpointids[i].localid = lr->localids[i];
  printf("All tasks created\n");

  /* Send INITTASK messages, giving each task a copy of the idmap */
  send_inittask(lr);

  /* Wait for all INITTASK messages to be processed */
  if (0 != get_responses(lr->endpt,MSG_INITTASKRESP,count,lr->managerids,NULL)) {
    assert(lr->cancel);
    launcher_free(n,arg);
    return NULL;
  }
  printf("All tasks initialised\n");

  /* Start all tasks */
  send_starttask(lr);

  /* Wait for notification of all tasks starting */
  if (0 != get_responses(lr->endpt,MSG_STARTTASKRESP,count,lr->managerids,NULL)) {
    assert(lr->cancel);
    launcher_free(n,arg);
    return NULL;
  }
  printf("All tasks started\n");

  printf("Distributed process creation done\n");

  /* FIXME: thread will leak here if we're not killed during shutdown */

  launcher_free(n,arg);
  return NULL;
}

void start_launcher(node *n, const char *bcdata, int bcsize, endpointid *managerids, int count)
{
  launcher *lr = (launcher*)calloc(1,sizeof(launcher));
  lr->n = n;
  lr->bcsize = bcsize;
  lr->bcdata = malloc(bcsize);
  lr->count = count;
  lr->managerids = malloc(count*sizeof(endpointid));
  memcpy(lr->managerids,managerids,count*sizeof(endpointid));
  memcpy(lr->bcdata,bcdata,bcsize);
  lr->havethread = 1;

  printf("Distributed process creation starting\n");
  if (0 > wrap_pthread_create(&lr->thread,NULL,launcher_thread,lr))
    fatal("pthread_create: %s",strerror(errno));

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
  start_launcher(n,bcdata,bcsize,managerids,count);
  free(managerids);
  free(bcdata);
  return 0;
}
