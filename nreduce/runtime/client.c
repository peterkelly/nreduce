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
#include "messages.h"
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
      unsigned char *c = (unsigned char*)&id.ip;
      node_log(n,LOG_ERROR,"%s: Got response from unknown source %u.%u.%u.%u:%u/%u",
               msg_names[tag],c[0],c[1],c[2],c[3],id.port,id.localid);
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

static void launcher_endpoint_close(node *n, endpoint *endpt)
{
  launcher *sa = (launcher*)endpt->data;
  assert(NODE_ALREADY_LOCKED(sa->n));
  unlock_mutex(&n->lock);
  sa->cancel = 1;
  endpoint_forceclose(sa->endpt);

  if (0 > pthread_join(sa->thread,NULL))
    fatal("pthread_join: %s",strerror(errno));
  lock_mutex(&n->lock);
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
    ntmsg->tid = i;
    node_send(lr->n,lr->endpt->epid.localid,lr->managerids[i],MSG_NEWTASK,(char*)ntmsg,ntsize);
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
    node_send(lr->n,lr->endpt->epid.localid,lr->managerids[i],MSG_INITTASK,(char*)initmsg,initsize);
  }
  free(initmsg);
}

static void send_starttask(launcher *lr)
{
  int i;
  for (i = 0; i < lr->count; i++)
    node_send(lr->n,lr->endpt->epid.localid,lr->managerids[i],MSG_STARTTASK,
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
  lr->endpt = node_add_endpoint(n,0,LAUNCHER_ENDPOINT,arg,launcher_endpoint_close);

  for (i = 0; i < count; i++) {
    unsigned char *ipbytes = (unsigned char*)&lr->managerids[i].ip;
    node_log(n,LOG_INFO,"Launcher: manager %u.%u.%u.%u:%d %d",
             ipbytes[0],ipbytes[1],ipbytes[2],ipbytes[3],lr->managerids[i].port,
             lr->managerids[i].localid);

    lr->endpointids[i].ip = lr->managerids[i].ip;
    lr->endpointids[i].port = lr->managerids[i].port;
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

  assert(NODE_ALREADY_LOCKED(n));

  if (n->isworker)
    count++;

  for (conn = n->connections.first; conn; conn = conn->next)
    if (0 <= conn->port)
      count++;

  *managerids = (endpointid*)calloc(count,sizeof(endpointid));

  if (n->isworker) {
    (*managerids)[i].ip = n->listenip;
    (*managerids)[i].port = n->listenport;
    (*managerids)[i].localid = MANAGER_ID;
    i++;
  }

  for (conn = n->connections.first; conn; conn = conn->next) {
    if (0 <= conn->port) {
      (*managerids)[i].ip = conn->ip;
      (*managerids)[i].port = conn->port;
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
  if (0 != source_process(src,0,0,0))
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

static array *read_nodes(const char *hostsfile)
{
  array *hostnames;
  int start = 0;
  int pos = 0;
  array *filedata;
  FILE *f;
  int r;
  char buf[1024];

  if (NULL == (f = fopen(hostsfile,"r"))) {
    perror(hostsfile);
    return NULL;
  }

  filedata = array_new(1,0);
  while (0 < (r = fread(buf,1,1024,f)))
    array_append(filedata,buf,r);
  fclose(f);

  hostnames = array_new(sizeof(char*),0);
  while (1) {
    if ((pos == filedata->nbytes) ||
        ('\n' == filedata->data[pos])) {
      if (pos > start) {
        char *hostname = (char*)malloc(pos-start+1);
        memcpy(hostname,&filedata->data[start],pos-start);
        hostname[pos-start] = '\0';
        array_append(hostnames,&hostname,sizeof(char*));
      }
      start = pos+1;
    }
    if (pos == filedata->nbytes)
      break;
    pos++;
  }

  free(filedata);
  return hostnames;
}

static int client_run(node *n, const char *nodesfile, const char *filename)
{
  int bcsize;
  char *bcdata;
  source *src;
  int count;
  endpointid *managerids;

  array *nodes = read_nodes(nodesfile);
  int i;

  if (NULL == nodes)
    return -1;

  count = array_count(nodes);
  managerids = (endpointid*)calloc(count,sizeof(endpointid));

  for (i = 0; i < count; i++) {
    char *nodename = array_item(nodes,i,char*);
    char *host = NULL;
    int port = 0;

    if (0 != parse_address(nodename,&host,&port)) {
      fprintf(stderr,"Invalid node address: %s\n",nodename);
      return -1;
    }
    managerids[i].port = port;
    managerids[i].localid = MANAGER_ID;

    if (0 > lookup_address(n,host,&managerids[i].ip)) {
      fprintf(stderr,"%s: hostname lookup failed\n",host);
      return -1;
    }
    free(host);
  }

  src = source_new();
  if (0 != source_parse_file(src,filename,""))
    return -1;
  if (0 != source_process(src,0,0,0))
    return -1;
  if (0 != source_compile(src,&bcdata,&bcsize))
    return -1;
  source_free(src);

  node_log(n,LOG_INFO,"Compiled");
  start_launcher(n,bcdata,bcsize,managerids,count);
  free(managerids);
  free(bcdata);
  return 0;
}

static void client_callback(struct node *n, void *data, int event,
                            connection *conn, endpoint *endpt)
{
  if (EVENT_CONN_FAILED == event) {
    fprintf(stderr,"Connection to %s:%d failed\n",conn->hostname,conn->port);
    exit(1);
  }
}

int do_client(const char *nodesfile, const char *program)
{
  node *n;
  int r = 0;

  n = node_new(LOG_INFO);

  node_add_callback(n,client_callback,NULL);

  if (NULL == node_listen(n,n->listenip,0,NULL,NULL,0,1)) {
    node_free(n);
    return -1;
  }

  node_start_iothread(n);

  if (0 != client_run(n,nodesfile,program))
    exit(1);

  if (0 > pthread_join(n->iothread,NULL))
    fatal("pthread_join: %s",strerror(errno));

  node_close_endpoints(n);
  node_close_connections(n);
  node_remove_callback(n,client_callback,NULL);
  node_free(n);

  return r;
}
