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
#include "chord.h"
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

static task *add_task(node *n, int pid, int groupsize, const char *bcdata, int bcsize)
{
  task *tsk = task_new(pid,groupsize,bcdata,bcsize,n);

  tsk->commdata = n;
  tsk->output = stdout;

  if ((0 == pid) && (NULL != bcdata)) {
    frame *initial = frame_new(tsk);
    initial->instr = bc_instructions(tsk->bcdata);
    initial->fno = -1;
    assert(initial->alloc == tsk->maxstack);
    initial->c = alloc_cell(tsk);
    initial->c->type = CELL_FRAME;
    make_pntr(initial->c->field1,initial);
    run_frame(tsk,initial);
  }

  return tsk;
}

static void manager_thread(node *n, endpoint *endpt, void *arg)
{
  message *msg;
  int done = 0;

  while (!done) {
    msg = endpoint_next_message(endpt,-1);
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
      node_send(n,endpt->epid.localid,msg->hdr.source,MSG_NEWTASKRESP,
                &newtsk->endpt->epid.localid,sizeof(int));
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

      for (i = 0; i < newtsk->groupsize; i++)
        if (!endpointid_equals(&newtsk->endpt->epid,&initmsg->idmap[i]))
          endpoint_link(newtsk->endpt,initmsg->idmap[i]);

      unlock_node(n);

      for (i = 0; i < newtsk->groupsize; i++) {
        endpointid_str str;
        print_endpointid(str,initmsg->idmap[i]);
        node_log(n,LOG_INFO,"INITTASK: idmap[%d] = %s",i,str);
      }

      node_send(n,endpt->epid.localid,msg->hdr.source,MSG_INITTASKRESP,&resp,sizeof(int));
      newtsk->haveidmap = 1; /* FIXME: another possible race condition */
      break;
    }
    case MSG_STARTTASK: {
      task *newtsk;
      int resp = 0;
      int localid;
      char semdata;
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

      write(newtsk->startfds[1],&semdata,1);
      newtsk->started = 1;
      unlock_node(n);

      node_send(n,endpt->epid.localid,msg->hdr.source,MSG_STARTTASKRESP,&resp,sizeof(int));
      break;
    }
    case MSG_START_CHORD: {
      start_chord_msg *m = (start_chord_msg*)msg->data;
      assert(sizeof(start_chord_msg) == msg->hdr.size);
      start_chord(n,0,m->initial,m->caller,m->stabilize_delay);
      break;
    }
    case MSG_KILL:
      node_log(n,LOG_INFO,"Manager received KILL");
      done = 1;
      break;
    default:
      fatal("Manager received invalid message: %d",msg->hdr.tag);
      break;
    }
    message_free(msg);
  }
}

void start_manager(node *n)
{
  node_add_thread(n,MANAGER_ID,MANAGER_ENDPOINT,0,manager_thread,NULL,NULL);
}
