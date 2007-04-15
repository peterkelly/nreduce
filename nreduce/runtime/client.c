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
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/select.h>
#include <sys/time.h>
#include <time.h>
#include <fcntl.h>
#include <unistd.h>
#include <signal.h>

typedef struct launcher {
  node *n;
  char *bcdata;
  int bcsize;
  int count;
  endpointid *managerids;
  int cancel;
  endpoint *endpt;
  endpointid *endpointids;
  int *localids;
  socketid out_sockid;
} launcher;

static int get_responses(node *n, launcher *lr, int tag,
                         int count, endpointid *managerids, int *responses)
{
  int *gotresponse = (int*)calloc(count,sizeof(int));
  int allresponses;
  int i;
  do {
    message *msg = endpoint_next_message(lr->endpt,-1);
    int sender = -1;

    if (MSG_KILL == msg->hdr.tag) {
      lr->cancel = 1;
      free(gotresponse);
      return -1;
    }

    if (tag != msg->hdr.tag)
      fatal("%s: Got invalid response tag: %d",msg_names[tag],msg->hdr.tag);

    for (i = 0; i < count; i++)
      if (endpointid_equals(&msg->hdr.source,&managerids[i]))
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

static void launcher_free(launcher *sa)
{
  free(sa->endpointids);
  free(sa->localids);
  free(sa->bcdata);
  free(sa->managerids);
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
  ntmsg->out_sockid = lr->out_sockid;
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

static void launcher_startgc(node *n, endpoint *endpt, launcher *lr)
{
  int msglen = sizeof(startgc_msg)+lr->count*sizeof(endpointid);
  message *msg;
  startgc_msg *sgcm = (startgc_msg*)calloc(1,msglen);
  sgcm->count = lr->count;
  memcpy(sgcm->idmap,lr->endpointids,lr->count*sizeof(endpointid));
  node_send(n,endpt->epid.localid,lr->managerids[0],MSG_STARTGC,sgcm,msglen);
  free(sgcm);

  msg = endpoint_next_message(endpt,-1);
  if (MSG_KILL == msg->hdr.tag) {
    lr->cancel = 1;
  }
  else {
    assert(MSG_STARTGC_RESPONSE == msg->hdr.tag);
    printf("garbage collector started\n");
  }
  message_free(msg);
}

static void launcher_thread(node *n, endpoint *endpt, void *arg)
{
  launcher *lr = (launcher*)arg;
  int count = lr->count;
  int i;

  lr->endpt = endpt;

  node_log(n,LOG_INFO,"Distributed process creation starting");

  lr->endpointids = (endpointid*)calloc(count,sizeof(endpointid));
  lr->localids = (int*)calloc(count,sizeof(int));

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
  if (0 != get_responses(n,lr,MSG_NEWTASKRESP,count,lr->managerids,lr->localids)) {
    assert(lr->cancel);
    launcher_free(arg);
    return;
  }
  for (i = 0; i < count; i++)
    lr->endpointids[i].localid = lr->localids[i];
  node_log(n,LOG_INFO,"All tasks created");

  /* Send INITTASK messages, giving each task a copy of the idmap */
  send_inittask(lr);

  /* Wait for all INITTASK messages to be processed */
  if (0 != get_responses(n,lr,MSG_INITTASKRESP,count,lr->managerids,NULL)) {
    assert(lr->cancel);
    launcher_free(arg);
    return;
  }
  node_log(n,LOG_INFO,"All tasks initialised");

  /* Start all tasks */
  send_starttask(lr);

  /* Wait for notification of all tasks starting */
  if (0 != get_responses(n,lr,MSG_STARTTASKRESP,count,lr->managerids,NULL)) {
    assert(lr->cancel);
    launcher_free(arg);
    return;
  }

  /* Start the garbage collector */
  if (1 < lr->count)
    launcher_startgc(n,endpt,lr);

  node_log(n,LOG_INFO,"Distributed process creation done");
  launcher_free(arg);
}

void start_launcher(node *n, const char *bcdata, int bcsize,
                    endpointid *managerids, int count, pthread_t *threadp, socketid out_sockid)
{
  launcher *lr = (launcher*)calloc(1,sizeof(launcher));
  lr->n = n;
  lr->bcsize = bcsize;
  lr->bcdata = malloc(bcsize);
  lr->count = count;
  lr->managerids = malloc(count*sizeof(endpointid));
  memcpy(lr->managerids,managerids,count*sizeof(endpointid));
  memcpy(lr->bcdata,bcdata,bcsize);
  lr->out_sockid = out_sockid;

  node_add_thread(n,0,LAUNCHER_ENDPOINT,0,launcher_thread,lr,threadp);
}

static int client_run(node *n, list *nodes, const char *filename, socketid out_sockid)
{
  int bcsize;
  char *bcdata;
  source *src;
  int count = list_count(nodes);
  endpointid *managerids = (endpointid*)calloc(count,sizeof(endpointid));
  list *l;
  int i;
  pthread_t thread;
  int nopartialsink = 1;

  i = 0;
  for (l = nodes; l; l = l->next) {
    managerids[i] = *(endpointid*)l->data;
    managerids[i].localid = MANAGER_ID;
    i++;
  }

  src = source_new();
  if (0 != source_parse_file(src,filename,""))
    return -1;
  if (0 != source_process(src,0,nopartialsink,nopartialsink,0))
    return -1;
  if (0 != source_compile(src,&bcdata,&bcsize))
    return -1;
  source_free(src);

  node_log(n,LOG_INFO,"Compiled");
  start_launcher(n,bcdata,bcsize,managerids,count,&thread,out_sockid);

  if (0 != pthread_join(thread,NULL))
    fatal("pthread_join: %s",strerror(errno));

  free(managerids);
  free(bcdata);
  return 0;
}

/* FIXME: make sure that when the task has an error, the message is sent back to the client */
static void output_thread(node *n, endpoint *endpt, void *arg)
{
  int done = 0;
  while (!done) {
    message *msg = endpoint_next_message(endpt,-1);
    switch (msg->hdr.tag) {
    case MSG_WRITE: {
      char *tmp;
      write_response_msg wrm;
      write_msg *m = (write_msg*)msg->data;
      assert(sizeof(write_msg) <= msg->hdr.size);
      tmp = malloc(m->len+1);
      memcpy(tmp,m->data,m->len);
      tmp[m->len] = '\0';
      printf("%s",tmp);
      free(tmp);

      wrm.ioid = m->ioid;
      node_send(n,endpt->epid.localid,msg->hdr.source,MSG_WRITE_RESPONSE,&wrm,sizeof(wrm));
      break;
    }
    case MSG_FINWRITE: {
      finwrite_response_msg frm;
      finwrite_msg *m = (finwrite_msg*)msg->data;
      assert(sizeof(finwrite_msg) == msg->hdr.size);
      frm.ioid = m->ioid;
      node_send(n,endpt->epid.localid,msg->hdr.source,MSG_FINWRITE_RESPONSE,
                &frm,sizeof(frm));
      break;
    }
    case MSG_DELETE_CONNECTION:
      done = 1;
      break;
    case MSG_KILL:
      done = 1;
      break;
    default:
      fatal("invalid message: %d",msg->hdr.tag);
      break;
    }
    message_free(msg);
  }
}

static void client_callback(struct node *n, void *data, int event,
                            connection *conn, endpoint *endpt)
{
  if (EVENT_CONN_FAILED == event) {
    fprintf(stderr,"Connection to %s:%d failed\n",conn->hostname,conn->port);
    exit(1);
  }
}

typedef struct {
  endpointid initial;
  endpointid first;
  int want;
  list *nodes;
} find_nodes_data;

static void find_nodes_thread(node *n, endpoint *endpt, void *arg)
{
  find_nodes_data *fsd = (find_nodes_data*)arg;
  int done = 0;
  get_table_msg gtm;
  chordid id = rand()%KEYSPACE;
  find_successor_msg fsm;

  gtm.sender = endpt->epid;

  fsm.id = id;
  fsm.sender = endpt->epid;
  fsm.hops = 0;
  fsm.payload = 0;

  node_send(n,endpt->epid.localid,fsd->initial,MSG_FIND_SUCCESSOR,&fsm,sizeof(fsm));

  while (!done) {
    message *msg = endpoint_next_message(endpt,-1);
    switch (msg->hdr.tag) {
    case MSG_GOT_SUCCESSOR: {
      got_successor_msg *m = (got_successor_msg*)msg->data;
      assert(sizeof(got_successor_msg) == msg->hdr.size);
      node_send(n,endpt->epid.localid,m->successor.epid,MSG_GET_TABLE,&gtm,sizeof(gtm));
      break;
    }
    case MSG_REPLY_TABLE: {
      reply_table_msg *m = (reply_table_msg*)msg->data;
      assert(sizeof(reply_table_msg) == msg->hdr.size);

      if (endpointid_equals(&fsd->first,&m->cn.epid)) {
        done = 1;
      }
      else {
        endpointid *copy = (endpointid*)malloc(sizeof(endpointid));
        memcpy(copy,&m->cn.epid,sizeof(endpointid));
        list_push(&fsd->nodes,copy);
        printf("%d ("EPID_FORMAT")\n",m->cn.id,EPID_ARGS(m->cn.epid));
        if ((0 < fsd->want) && (list_count(fsd->nodes) >= fsd->want))
          done = 1;
        else
          node_send(n,endpt->epid.localid,m->fingers[1].epid,MSG_GET_TABLE,&gtm,sizeof(gtm));
      }

      if (0 == fsd->first.localid)
        fsd->first = m->cn.epid;
      break;
    }
    case MSG_KILL:
      done = 1;
      break;
    default:
      fatal("Invalid message: %d",msg->hdr.tag);
      break;
    }
  }
}

static list *find_nodes(node *n, endpointid initial, int want)
{
  pthread_t thread;
  find_nodes_data fsd;
  memset(&fsd,0,sizeof(fsd));
  fsd.initial = initial;
  fsd.want = want;
  node_add_thread(n,0,TEST_ENDPOINT,0,find_nodes_thread,&fsd,&thread);
  if (0 != pthread_join(thread,NULL))
    fatal("pthread_join: %s",strerror(errno));
  return fsd.nodes;
}

int do_client(char *initial_str, int argc, char **argv)
{
  node *n;
  int r = 0;
  char *cmd;
  endpointid initial;

  n = node_new(LOG_WARNING);

  node_add_callback(n,client_callback,NULL);

  if (NULL == node_listen(n,n->listenip,0,NULL,NULL,0,1,NULL,NULL,0)) {
    node_free(n);
    return -1;
  }

  node_start_iothread(n);

  if (0 > string_to_mainchordid(n,initial_str,&initial))
    exit(1);

  if (1 > argc) {
    fprintf(stderr,"Please specify a command\n");
    exit(1);
  }
  cmd = argv[0];

  if (!strcmp(cmd,"findall")) {
    list *nodes = find_nodes(n,initial,0);
    list_free(nodes,free);
  }
  else if (!strcmp(cmd,"run")) {
    if (3 > argc) {
      fprintf(stderr,"Please specify number of machines and program\n");
      exit(1);
    }
    else {
      int want = atoi(argv[1]);
      char *program = argv[2];
      list *nodes = find_nodes(n,initial,want);
      pthread_t thread;
      socketid out_sockid;
      out_sockid.managerid = node_add_thread(n,0,TEST_ENDPOINT,0,output_thread,NULL,&thread);
      out_sockid.sid = 1;
      if (0 != client_run(n,nodes,program,out_sockid))
        exit(1);
      list_free(nodes,free);

      if (0 != pthread_join(thread,NULL))
        fatal("pthread_join: %s",strerror(errno));

/*       printf("Program launched: %s\n",program); */
    }
  }
  else {
    fprintf(stderr,"Unknown command: %s\n",cmd);
    exit(1);
  }

  lock_node(n);
  node_shutdown_locked(n);
  unlock_node(n);

  if (0 != pthread_join(n->iothread,NULL))
    fatal("pthread_join: %s",strerror(errno));

  node_close_endpoints(n);
  node_close_connections(n);
  node_remove_callback(n,client_callback,NULL);
  node_free(n);

  return r;
}
