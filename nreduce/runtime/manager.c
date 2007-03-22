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
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>

static int manager_handle_message(node *n, endpoint *endpt, message *msg)
{
  switch (msg->hdr.tag) {
  case MSG_NEWTASK: {
    newtask_msg *ntmsg;
    task *newtsk;
    if (sizeof(newtask_msg) > msg->hdr.size)
      fatal("NEWTASK: invalid message size");
    ntmsg = (newtask_msg*)msg->data;
    if (sizeof(newtask_msg)+ntmsg->bcsize > msg->hdr.size)
      fatal("NEWTASK: invalid bytecode size");

    node_log(n,LOG_INFO,"NEWTASK pid = %d, groupsize = %d, bcsize = %d",
             ntmsg->tid,ntmsg->groupsize,ntmsg->bcsize);

    newtsk = add_task(n,ntmsg->tid,ntmsg->groupsize,ntmsg->bcdata,ntmsg->bcsize);
    node_send(n,endpt->localid,msg->hdr.source,MSG_NEWTASKRESP,&newtsk->endpt->localid,sizeof(int));
    break;
  }
  case MSG_INITTASK: {
    inittask_msg *initmsg;
    task *newtsk;
    int i;
    int resp = 0;
    if (sizeof(inittask_msg) > msg->hdr.size)
      fatal("INITTASK: invalid message size");
    initmsg = (inittask_msg*)msg->data;
    if (sizeof(initmsg)+initmsg->count*sizeof(endpointid) > msg->hdr.size)
      fatal("INITTASK: invalid idmap size");

    node_log(n,LOG_INFO,"INITTASK: localid = %d, count = %d",initmsg->localid,initmsg->count);

    lock_node(n);
    newtsk = find_task(n,initmsg->localid);

    if (NULL == newtsk)
      fatal("INITTASK: task with localid %d does not exist",initmsg->localid);

    if (newtsk->haveidmap)
      fatal("INITTASK: task with localid %d already has an idmap",initmsg->localid);

    if (initmsg->count != newtsk->groupsize)
      fatal("INITTASK: idmap size does not match expected");
    memcpy(newtsk->idmap,initmsg->idmap,newtsk->groupsize*sizeof(endpointid));
    unlock_node(n);

    for (i = 0; i < newtsk->groupsize; i++) {
      endpointid id = initmsg->idmap[i];
      unsigned char *c = (unsigned char*)&id.nodeip;
      node_log(n,LOG_INFO,"INITTASK: idmap[%d] = %u.%u.%u.%u:%d/%d",
               i,c[0],c[1],c[2],c[3],id.nodeport,id.localid);
    }

    node_send(n,endpt->localid,msg->hdr.source,MSG_INITTASKRESP,&resp,sizeof(int));
    newtsk->haveidmap = 1; /* FIXME: another possible race condition */
    break;
  }
  case MSG_STARTTASK: {
    task *newtsk;
    int resp = 0;
    int localid;
    if (sizeof(int) > msg->hdr.size)
      fatal("STARTTASK: invalid message size");
    localid = *(int*)msg->data;

    node_log(n,LOG_INFO,"STARTTASK: localid = %d",localid);

    lock_node(n);
    newtsk = find_task(n,localid);

    if (NULL == newtsk)
      fatal("STARTTASK: task with localid %d does not exist",localid);

    if (!newtsk->haveidmap)
      fatal("STARTTASK: task with localid %d does not have an idmap",localid);

    if (newtsk->started)
      fatal("STARTTASK: task with localid %d has already been started",localid);

    sem_post(&newtsk->startsem);
    newtsk->started = 1;
    unlock_node(n);

    node_send(n,endpt->localid,msg->hdr.source,MSG_STARTTASKRESP,&resp,sizeof(int));
    break;
  }
  default:
    node_log(n,LOG_INFO,"Manager received invalid message: %d",msg->hdr.tag);
    break;
  }
  return 0;
}

typedef struct manager {
  node *n;
  pthread_t thread;
} manager;

static void manager_endpoint_close(endpoint *endpt)
{
  manager *man = (manager*)endpt->data;
  node *n = man->n;
  pthread_t thread = man->thread;

  unlock_mutex(&n->lock);
  endpoint_forceclose(endpt);
  if (0 > pthread_join(thread,NULL))
    fatal("pthread_join: %s",strerror(errno));
  lock_mutex(&n->lock);
}

static void *manager_thread(void *arg)
{
  manager *man = (manager*)arg;
  node *n = man->n;
  message *msg;
  endpoint *endpt;

  endpt = node_add_endpoint(n,MANAGER_ID,MANAGER_ENDPOINT,man,manager_endpoint_close);

  while (NULL != (msg = endpoint_next_message(endpt,-1))) {
    int r = manager_handle_message(n,endpt,msg);
    assert(0 == r);
    message_free(msg);
  }

  node_remove_endpoint(n,endpt);
  free(man);

  return NULL;
}

void start_manager(node *n)
{
  manager *man = (manager*)calloc(1,sizeof(manager));
  man->n = n;
  if (0 > pthread_create(&man->thread,NULL,manager_thread,man))
    fatal("pthread_create: %s",strerror(errno));
}
