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
#include "network.h"
#include "node.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <assert.h>
#include <ctype.h>
#include <stdarg.h>
#include <math.h>
#include <errno.h>

static int manager_handle_message(node *n, endpoint *endpt, message *msg)
{
/*   printf("Manager got %s (%d bytes)\n",msg_names[msg->hdr.tag],msg->hdr.size); */
  switch (msg->hdr.tag) {
  case MSG_NEWTASK: {
    newtask_msg *ntmsg;
    task *newtsk;
    if (sizeof(newtask_msg) > msg->hdr.size)
      fatal("NEWTASK: invalid message size\n");
    ntmsg = (newtask_msg*)msg->data;
    if (sizeof(newtask_msg)+ntmsg->bcsize > msg->hdr.size)
      fatal("NEWTASK: invalid bytecode size\n");

    printf("NEWTASK pid = %d, groupsize = %d, bcsize = %d\n",
           ntmsg->pid,ntmsg->groupsize,ntmsg->bcsize);

    newtsk = add_task(n,ntmsg->pid,ntmsg->groupsize,ntmsg->bcdata,ntmsg->bcsize);

    node_send(n,endpt,msg->hdr.source,MSG_NEWTASKRESP,&newtsk->endpt->localid,sizeof(int));
    break;
  }
  case MSG_INITTASK: {
    inittask_msg *initmsg;
    task *newtsk;
    int i;
    int resp = 0;
    if (sizeof(inittask_msg) > msg->hdr.size)
      fatal("INITTASK: invalid message size\n");
    initmsg = (inittask_msg*)msg->data;
    if (sizeof(initmsg)+initmsg->count*sizeof(endpointid) > msg->hdr.size)
      fatal("INITTASK: invalid idmap size\n");

    printf("INITTASK: localid = %d, count = %d\n",initmsg->localid,initmsg->count);

    pthread_mutex_lock(&n->lock);
    newtsk = find_task(n,initmsg->localid);
    pthread_mutex_unlock(&n->lock);

    if (NULL == newtsk)
      fatal("INITTASK: task with localid %d does not exist\n",initmsg->localid);

    if (newtsk->haveidmap)
      fatal("INITTASK: task with localid %d already has an idmap\n",initmsg->localid);

    if (initmsg->count != newtsk->groupsize)
      fatal("INITTASK: idmap size does not match expected\n");

    for (i = 0; i < newtsk->groupsize; i++) {
      printf("INITTASK: idmap[%d] = ",i);
      print_endpointid(stdout,initmsg->idmap[i]);
      printf("\n");
    }

    memcpy(newtsk->idmap,initmsg->idmap,newtsk->groupsize*sizeof(endpointid));
    node_send(n,endpt,msg->hdr.source,MSG_INITTASKRESP,&resp,sizeof(int));
    newtsk->haveidmap = 1;
    break;
  }
  case MSG_STARTTASK: {
    task *newtsk;
    int resp = 0;
    int localid;
    if (sizeof(int) > msg->hdr.size)
      fatal("STARTTASK: invalid message size\n");
    localid = *(int*)msg->data;

    printf("STARTTASK: localid = %d\n",localid);

    pthread_mutex_lock(&n->lock);
    newtsk = find_task(n,localid);
    pthread_mutex_unlock(&n->lock);

    if (NULL == newtsk)
      fatal("STARTTASK: task with localid %d does not exist\n",localid);

    if (!newtsk->haveidmap)
      fatal("STARTTASK: task with localid %d does not have an idmap\n",localid);

    if (newtsk->started)
      fatal("STARTTASK: task with localid %d has already been started\n",localid);

    if (0 > wrap_pthread_create(&newtsk->thread,NULL,(void*)execute,newtsk))
      fatal("pthread_create: %s",strerror(errno));

    node_send(n,endpt,msg->hdr.source,MSG_STARTTASKRESP,&resp,sizeof(int));
    newtsk->started = 1;
    break;
  }
  default:
    printf("Manager received invalid message: %d\n",msg->hdr.tag);
    break;
  }
  return 0;
}

static void *manager(void *arg)
{
  node *n = (node*)arg;
  message *msg;

  n->managerendpt = node_add_endpoint(n,MANAGER_ID,MANAGER_ENDPOINT,NULL);

  while (NULL != (msg = endpoint_next_message(n->managerendpt,-1))) {
    int r = manager_handle_message(n,n->managerendpt,msg);
    assert(0 == r);
    free(msg->data);
    free(msg);
  }

  node_remove_endpoint(n,n->managerendpt);
  n->managerendpt = NULL;

  return NULL;
}

void start_manager(node *n)
{
  if (0 > wrap_pthread_create(&n->managerthread,NULL,manager,n))
    fatal("pthread_create: %s",strerror(errno));
}
