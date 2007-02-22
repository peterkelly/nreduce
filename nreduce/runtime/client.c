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

static int get_responses(node *n, endpoint *endpt, int tag,
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
      fatal("%s: Got invalid response tag: %d",msg_names[tag],msg->hdr.tag);

    for (i = 0; i < count; i++)
      if (!memcmp(&msg->hdr.source,&managerids[i],sizeof(endpointid)))
        sender = i;

    if (0 > sender) {
      endpointid id = msg->hdr.source;
      unsigned char *c = (unsigned char*)&id.nodeip;
      node_log(n,LOG_ERROR,"%s: Got response from unknown source %u.%u.%u.%u:%d/%d",
               msg_names[tag],c[0],c[1],c[2],c[3],id.nodeport,id.localid);
      abort();
    }

    if (sizeof(int) != msg->hdr.size)
      fatal("%s: incorrect message size (%d)",msg->hdr.size,msg_names[tag]);

    if (gotresponse[sender])
      fatal("%s: Already have response for this source",msg_names[tag]);

    gotresponse[sender] = 1;
    if (responses)
      responses[sender] = *(int*)msg->data;

    allresponses = 1;
    for (i = 0; i < count; i++)
      if (!gotresponse[i])
        allresponses = 0;

    message_free(msg);
  } while (!allresponses);

  free(gotresponse);
  return 0;
}

void launcher_kill(launcher *sa)
{
  sa->cancel = 1;
  endpoint_forceclose(sa->endpt);

  if (0 > pthread_join(sa->thread,NULL))
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
    node_send(lr->n,lr->endpt->localid,lr->managerids[i],MSG_NEWTASK,(char*)ntmsg,ntsize);
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
    node_send(lr->n,lr->endpt->localid,lr->managerids[i],MSG_INITTASK,(char*)initmsg,initsize);
  }
  free(initmsg);
}

static void send_starttask(launcher *lr)
{
  int i;
  for (i = 0; i < lr->count; i++)
    node_send(lr->n,lr->endpt->localid,lr->managerids[i],MSG_STARTTASK,
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
    node_log(n,LOG_INFO,"Launcher: manager %u.%u.%u.%u:%d %d",
             ipbytes[0],ipbytes[1],ipbytes[2],ipbytes[3],lr->managerids[i].nodeport,
             lr->managerids[i].localid);

    lr->endpointids[i].nodeip = lr->managerids[i].nodeip;
    lr->endpointids[i].nodeport = lr->managerids[i].nodeport;
  }

  /* Send NEWTASK messages, containing the pid, groupsize, and bytecode */
  send_newtask(lr);

  /* Get NEWTASK responses, containing the local id of each task on the remote node */
  if (0 != get_responses(n,lr->endpt,MSG_NEWTASKRESP,count,lr->managerids,lr->localids)) {
    assert(lr->cancel);
    launcher_free(n,arg);
    return NULL;
  }
  for (i = 0; i < count; i++)
    lr->endpointids[i].localid = lr->localids[i];
  node_log(n,LOG_INFO,"All tasks created");

  /* Send INITTASK messages, giving each task a copy of the idmap */
  send_inittask(lr);

  /* Wait for all INITTASK messages to be processed */
  if (0 != get_responses(n,lr->endpt,MSG_INITTASKRESP,count,lr->managerids,NULL)) {
    assert(lr->cancel);
    launcher_free(n,arg);
    return NULL;
  }
  node_log(n,LOG_INFO,"All tasks initialised");

  /* Start all tasks */
  send_starttask(lr);

  /* Wait for notification of all tasks starting */
  if (0 != get_responses(n,lr->endpt,MSG_STARTTASKRESP,count,lr->managerids,NULL)) {
    assert(lr->cancel);
    launcher_free(n,arg);
    return NULL;
  }
  node_log(n,LOG_INFO,"All tasks started");

  node_log(n,LOG_INFO,"Distributed process creation done");

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

  node_log(n,LOG_INFO,"Distributed process creation starting");
  if (0 > pthread_create(&lr->thread,NULL,launcher_thread,lr))
    fatal("pthread_create: %s",strerror(errno));

  node_log(n,LOG_INFO,"Distributed process creation started");
}

int get_managerids(node *n, endpointid **managerids)
{
  int count = 0;
  int i = 0;
  connection *conn;

  if (n->isworker && !n->havelistenip)
    fatal("I don't have my listen IP yet!");

  if (n->isworker)
    count++;

  for (conn = n->connections.first; conn; conn = conn->next)
    if (0 <= conn->port)
      count++;

  *managerids = (endpointid*)calloc(count,sizeof(endpointid));

  if (n->isworker) {
    (*managerids)[i].nodeip = n->listenip;
    (*managerids)[i].nodeport = n->mainl->port;
    (*managerids)[i].localid = MANAGER_ID;
    i++;
  }

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
  if (0 != source_parse_file(src,filename,""))
    return -1;
  if (0 != source_process(src,0,0))
    return -1;
  if (0 != source_compile(src,&bcdata,&bcsize))
    return -1;
  source_free(src);

  count = get_managerids(n,&managerids);

  node_log(n,LOG_INFO,"Compiled");
  start_launcher(n,bcdata,bcsize,managerids,count);
  free(managerids);
  free(bcdata);
  return 0;
}

typedef struct client_data {
  pthread_mutex_t lock;
  pthread_cond_t cond;
  const char *host;
  int port;
  int event;
} client_data;

static void client_callback(struct node *n, void *data, int event,
                            connection *conn, endpoint *endpt)
{
  client_data *cd = (client_data*)data;
  if ((EVENT_HANDSHAKE_DONE == event) || (EVENT_CONN_FAILED == event)) {
    lock_mutex(&cd->lock);
    cd->event = event;
    pthread_cond_signal(&cd->cond);
    unlock_mutex(&cd->lock);
  }
}

int do_client(const char *host, int port, int argc, char **argv)
{
  node *n;
  int r = 0;
  client_data cd;
  connection *conn;

  if (1 > argc) {
    fprintf(stderr,"client: please specify a filename to run\n");
    return -1;
  }

  pthread_cond_init(&cd.cond,NULL);
  pthread_mutex_init(&cd.lock,NULL);
  cd.host = host;
  cd.port = port;
  cd.event = -1;

  n = node_new(LOG_INFO);

  if (NULL == (n->mainl = node_listen(n,host,0,NULL,NULL,0))) {
    node_free(n);
    return -1;
  }

  node_add_callback(n,client_callback,&cd);
  node_start_iothread(n);

  lock_node(n);
  conn = node_connect_locked(n,host,port,1);
  unlock_node(n);

  if (NULL == conn) {
    fprintf(stderr,"client: Connection to %s:%d failed\n",host,port);
    r = 1;
  }
  else {
    int event;
    /* find out if it's successful... */
    lock_mutex(&cd.lock);
    pthread_cond_wait(&cd.cond,&cd.lock);
    event = cd.event;
    unlock_mutex(&cd.lock);

    if (EVENT_HANDSHAKE_DONE != event) {
      fprintf(stderr,"client: Connection to %s:%d failed\n",host,port);
      r = 1;
    }
    else {
      printf("client: Connection to %s:%d ok\n",host,port);

      if (0 != run_program(n,argv[0]))
        exit(1);
    }
  }

  if (0 > pthread_join(n->iothread,NULL))
    fatal("pthread_join: %s",strerror(errno));

  node_close_endpoints(n);
  node_close_connections(n);
  node_remove_callback(n,client_callback,&cd);
  node_free(n);

  pthread_mutex_destroy(&cd.lock);
  pthread_cond_destroy(&cd.cond);

  return r;
}
