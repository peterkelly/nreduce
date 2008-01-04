/*
 * This file is part of the nreduce project
 * Copyright (C) 2006-2007 Peter Kelly (pmk@cs.adelaide.edu.au)
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
#include "network/node.h"
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
  int argc;
  char **argv;
} launcher;

static int get_responses(node *n, launcher *lr, int tag,
                         int count, endpointid *managerids, int *responses)
{
  int *gotresponse = (int*)calloc(count,sizeof(int));
  int allresponses;
  int i;
  do {
    message *msg = endpoint_receive(lr->endpt,-1);
    int sender = -1;

    if (MSG_KILL == msg->tag) {
      lr->cancel = 1;
      free(gotresponse);
      return -1;
    }

    if (tag != msg->tag)
      fatal("%d: Got invalid response tag: %d",tag,msg->tag);

    if ((MSG_STARTTASKRESP == tag) || (MSG_INITTASKRESP == tag)) {
      for (i = 0; i < count; i++)
        if (endpointid_equals(&msg->source,&lr->endpointids[i]))
          sender = i;
    }
    else {
      for (i = 0; i < count; i++)
        if (endpointid_equals(&msg->source,&managerids[i]))
          sender = i;
    }

    if (0 > sender) {
      endpointid id = msg->source;
      unsigned char *c = (unsigned char*)&id.ip;
      node_log(n,LOG_ERROR,"%d: Got response from unknown source %u.%u.%u.%u:%u/%u",
               tag,c[0],c[1],c[2],c[3],id.port,id.localid);
      abort();
    }

    if (sizeof(int) != msg->size)
      fatal("%d: incorrect message size (%d)",tag,msg->size);

    if (gotresponse[sender])
      fatal("%d: Already have response for this source",tag);

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

static void launcher_free(launcher *lr)
{
  int i;
  for (i = 0; i < lr->argc; i++)
    free(lr->argv[i]);
  free(lr->argv);
  free(lr->endpointids);
  free(lr->localids);
  free(lr->bcdata);
  free(lr->managerids);
  free(lr);
}

static void send_newtask(launcher *lr)
{
  int ntsize = sizeof(newtask_msg)+lr->bcsize;
  newtask_msg *ntmsg;
  int i;
  char *pos;

  for (i = 0; i < lr->argc; i++)
    ntsize += strlen(lr->argv[i])+1;

  ntmsg = (newtask_msg*)calloc(1,ntsize);
  ntmsg->groupsize = lr->count;
  ntmsg->argc = lr->argc;
  ntmsg->bcsize = lr->bcsize;
  memcpy(&ntmsg->bcdata,lr->bcdata,lr->bcsize);

  pos = ((char*)ntmsg)+sizeof(newtask_msg)+lr->bcsize;
  for (i = 0; i < lr->argc; i++) {
    memcpy(pos,lr->argv[i],strlen(lr->argv[i]));
    pos += strlen(lr->argv[i]);
    *pos = '\0';
    pos++;
  }
  assert(pos == ((char*)ntmsg)+ntsize);

  ntmsg->out_sockid = lr->out_sockid;
  for (i = 0; i < lr->count; i++) {
    ntmsg->tid = i;
    endpoint_send(lr->endpt,lr->managerids[i],MSG_NEWTASK,(char*)ntmsg,ntsize);
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

  /* send to output thread */
  initmsg->localid = 0;
  endpoint_send(lr->endpt,lr->out_sockid.coordid,MSG_INITTASK,(char*)initmsg,initsize);

  /* send to managers */
  for (i = 0; i < lr->count; i++) {
    initmsg->localid = lr->endpointids[i].localid;
    endpoint_send(lr->endpt,lr->endpointids[i],MSG_INITTASK,(char*)initmsg,initsize);
  }
  free(initmsg);
}

static void send_starttask(launcher *lr)
{
  int i;
  for (i = 0; i < lr->count; i++)
    endpoint_send(lr->endpt,lr->endpointids[i],MSG_STARTTASK,&lr->localids[i],sizeof(int));
}

static void launcher_startgc(node *n, endpoint *endpt, launcher *lr)
{
  int msglen = sizeof(startgc_msg)+lr->count*sizeof(endpointid);
  message *msg;
  startgc_msg *sgcm = (startgc_msg*)calloc(1,msglen);
  sgcm->count = lr->count;
  memcpy(sgcm->idmap,lr->endpointids,lr->count*sizeof(endpointid));
  endpoint_send(endpt,lr->managerids[0],MSG_STARTGC,sgcm,msglen);
  free(sgcm);

  msg = endpoint_receive(endpt,-1);
  if (MSG_KILL == msg->tag) {
    lr->cancel = 1;
  }
  else {
    assert(MSG_STARTGC_RESPONSE == msg->tag);
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
                    endpointid *managerids, int count, pthread_t *threadp, socketid out_sockid,
                    int argc, const char **argv)
{
  launcher *lr = (launcher*)calloc(1,sizeof(launcher));
  int i;
  lr->n = n;
  lr->bcsize = bcsize;
  lr->bcdata = malloc(bcsize);
  lr->count = count;
  lr->managerids = malloc(count*sizeof(endpointid));
  lr->argc = argc;
  lr->argv = (char**)malloc(argc*sizeof(char*));
  for (i = 0; i < argc; i++)
    lr->argv[i] = strdup(argv[i]);
  memcpy(lr->managerids,managerids,count*sizeof(endpointid));
  memcpy(lr->bcdata,bcdata,bcsize);
  lr->out_sockid = out_sockid;

  node_add_thread(n,"launcher",launcher_thread,lr,threadp);
}

static int client_run(node *n, list *nodes, const char *filename, socketid out_sockid,
                      int argc, const char **argv)
{
  int bcsize;
  char *bcdata;
  source *src;
  int count = list_count(nodes);
  endpointid *managerids = (endpointid*)calloc(count,sizeof(endpointid));
  list *l;
  int i;
  pthread_t thread;

  i = 0;
  for (l = nodes; l; l = l->next) {
    managerids[i] = *(endpointid*)l->data;
    managerids[i].localid = MANAGER_ID;
    i++;
  }

  src = source_new();
  if (0 != source_parse_file(src,filename,""))
    return -1;
  if (0 != source_process(src,0,0,0,0))
    return -1;
  if (0 != source_compile(src,&bcdata,&bcsize))
    return -1;
  source_free(src);

  node_log(n,LOG_INFO,"Compiled");
  start_launcher(n,bcdata,bcsize,managerids,count,&thread,out_sockid,argc,argv);

  if (0 != pthread_join(thread,NULL))
    fatal("pthread_join: %s",strerror(errno));

  free(managerids);
  free(bcdata);
  return 0;
}

void output_thread(node *n, endpoint *endpt, void *arg)
{
  output_arg *oa = (output_arg*)arg;
  int done = 0;
  int conn_deleted = 0;
  int havetasks = 0;
  oa->rc = 0;
  while (!done) {
    message *msg = endpoint_receive(endpt,-1);
    switch (msg->tag) {
    case MSG_WRITE: {
      write_response_msg wrm;
      write_msg *m = (write_msg*)msg->data;
      assert(sizeof(write_msg) <= msg->size);
      fwrite(m->data,1,m->len,stdout);
      wrm.ioid = m->ioid;
      endpoint_send(endpt,msg->source,MSG_WRITE_RESPONSE,&wrm,sizeof(wrm));
      break;
    }
    case MSG_REPORT_ERROR: {
      report_error_msg *m = (report_error_msg*)msg->data;
      assert(sizeof(report_error_msg) <= msg->size);
      fwrite(m->data,1,m->len,stderr);
      fprintf(stderr,"\n");
      oa->rc = m->rc;
      break;
    }
    case MSG_INITTASK: {
      inittask_msg *initmsg = (inittask_msg*)msg->data;
      int i;
      assert(sizeof(inittask_msg) <= msg->size);
      assert(sizeof(initmsg)+initmsg->count*sizeof(endpointid) <= msg->size);
      if (havetasks)
        break;
      havetasks = 1;
      for (i = 0; i < initmsg->count; i++)
        endpoint_link(endpt,initmsg->idmap[i]);
      break;
    }
    case MSG_ENDPOINT_EXIT: {
      endpoint_exit_msg *m = (endpoint_exit_msg*)msg->data;
      assert(sizeof(endpoint_exit_msg) == msg->size);
      if (!conn_deleted) {
        fprintf(stderr,"Unexpected task exit: "EPID_FORMAT"\n",EPID_ARGS(m->epid));
        oa->rc = 1;
      }
      done = 1;
      break;
    }
    case MSG_DELETE_CONNECTION:
      conn_deleted = 1;
      break;
    case MSG_KILL:
      done = 1;
      break;
    default:
      fatal("invalid message: %d",msg->tag);
      break;
    }
    message_free(msg);
  }
}

static void connection_thread(node *n, endpoint *endpt, void *arg)
{
  int done = 0;
  endpointid *destid = (endpointid*)arg;
  endpoint_link(endpt,*destid);
  while (!done) {
    message *msg = endpoint_receive(endpt,-1);
    switch (msg->tag) {
    case MSG_ENDPOINT_EXIT: {
      endpoint_exit_msg *m = (endpoint_exit_msg*)msg->data;
      unsigned char *c;
      int port;
      assert(sizeof(endpoint_exit_msg) == msg->size);
      c = (unsigned char*)&m->epid.ip;
      port = m->epid.port;
      fprintf(stderr,"Connection to %u.%u.%u.%u:%u failed\n",c[0],c[1],c[2],c[3],port);
      exit(1);
      break;
    }
    case MSG_KILL:
      done = 1;
      break;
    default:
      fatal("invalid message: %d",msg->tag);
      break;
    }
    message_free(msg);
  }
}

typedef struct {
  endpointid initial;
  endpointid first;
  int want;
  list *nodes;
  int print;
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

  endpoint_send(endpt,fsd->initial,MSG_FIND_SUCCESSOR,&fsm,sizeof(fsm));

  while (!done) {
    message *msg = endpoint_receive(endpt,-1);
    switch (msg->tag) {
    case MSG_GOT_SUCCESSOR: {
      got_successor_msg *m = (got_successor_msg*)msg->data;
      assert(sizeof(got_successor_msg) == msg->size);
      endpoint_send(endpt,m->successor.epid,MSG_GET_TABLE,&gtm,sizeof(gtm));
      break;
    }
    case MSG_REPLY_TABLE: {
      reply_table_msg *m = (reply_table_msg*)msg->data;
      assert(sizeof(reply_table_msg) == msg->size);

      if (endpointid_equals(&fsd->first,&m->cn.epid)) {
        done = 1;
      }
      else {
        endpointid *copy = (endpointid*)malloc(sizeof(endpointid));
        memcpy(copy,&m->cn.epid,sizeof(endpointid));
        list_push(&fsd->nodes,copy);
        if (fsd->print)
          printf("%d ("EPID_FORMAT")\n",m->cn.id,EPID_ARGS(m->cn.epid));
        if ((0 < fsd->want) && (list_count(fsd->nodes) >= fsd->want))
          done = 1;
        else
          endpoint_send(endpt,m->fingers[1].epid,MSG_GET_TABLE,&gtm,sizeof(gtm));
      }

      if (0 == fsd->first.localid)
        fsd->first = m->cn.epid;
      break;
    }
    case MSG_KILL:
      done = 1;
      break;
    default:
      fatal("Invalid message: %d",msg->tag);
      break;
    }
  }
}

static list *find_nodes(node *n, endpointid initial, int want, int print)
{
  pthread_t thread;
  find_nodes_data fsd;
  memset(&fsd,0,sizeof(fsd));
  fsd.initial = initial;
  fsd.want = want;
  fsd.print = print;
  node_add_thread(n,"find_nodes",find_nodes_thread,&fsd,&thread);
  if (0 != pthread_join(thread,NULL))
    fatal("pthread_join: %s",strerror(errno));
  return fsd.nodes;
}

typedef struct {
  list *chordids;
  list *tmpret;
} find_tasks_data;

static void find_tasks_thread(node *n, endpoint *endpt, void *arg)
{
  find_tasks_data *ftd = (find_tasks_data*)arg;
  list *l;
  int nmanagers = list_count(ftd->chordids);
  int done = 0;

  for (l = ftd->chordids; l; l = l->next) {
    get_tasks_msg gtm;
    endpointid managerid = *(endpointid*)l->data;
    managerid.localid = MANAGER_ID;
    gtm.sender = endpt->epid;
    endpoint_send(endpt,managerid,MSG_GET_TASKS,&gtm,sizeof(gtm));
  }

  while (done < nmanagers) {
    message *msg = endpoint_receive(endpt,-1);
    switch (msg->tag) {
    case MSG_GET_TASKS_RESPONSE: {
      int i;
      get_tasks_response_msg *gtrm = (get_tasks_response_msg*)msg->data;
      assert(sizeof(get_tasks_response_msg) <= msg->size);
      assert(sizeof(get_tasks_response_msg)+gtrm->count*sizeof(endpointid) == msg->size);
      for (i = 0; i < gtrm->count; i++) {
        endpointid *copy = (endpointid*)malloc(sizeof(endpointid));
        memcpy(copy,&gtrm->tasks[i],sizeof(endpointid));
        list_push(&ftd->tmpret,copy);
      }
      done++;
      break;
    }
    default:
      fatal("Invalid message: %d",msg->tag);
      break;
    }
    message_free(msg);
  }
}

static list *find_tasks(node *n, list *chordids)
{
  pthread_t thread;
  find_tasks_data ftd;
  ftd.chordids = chordids;
  ftd.tmpret = NULL;
  node_add_thread(n,"find_tasks",find_tasks_thread,&ftd,&thread);
  if (0 != pthread_join(thread,NULL))
    fatal("pthread_join: %s",strerror(errno));
  return ftd.tmpret;
}

typedef struct {
  list *epids;
} monitor_data;

static void monitor_thread(node *n, endpoint *endpt, void *arg)
{
  monitor_data *md = (monitor_data*)arg;
  get_stats_msg gsm;
  list *l;
  int count = list_count(md->epids);
  endpointid *epids = (endpointid*)calloc(count,sizeof(endpointid));
  char *got = (char*)calloc(count,1);
  get_stats_response_msg *messages =
    (get_stats_response_msg*)malloc(count*sizeof(get_stats_response_msg));
  int i;
  int remaining = 0;
  struct timeval monstart;
  struct timeval next;
  memset(&gsm,0,sizeof(gsm));
  gsm.sender = endpt->epid;

  i = 0;
  for (l = md->epids; l; l = l->next)
    epids[i++] = *(endpointid*)l->data;

  gettimeofday(&monstart,NULL);
  next = monstart;

  while (1) {
    struct timeval loopstart;
    struct timeval loopend;
    struct timeval diff;
    struct timespec sleep_time;

    gettimeofday(&loopstart,NULL);
    memset(got,0,count);
    memset(messages,0,count*sizeof(get_stats_response_msg*));
    remaining = count;
    for (i = 0; i < count; i++) {
      endpoint_send(endpt,epids[i],MSG_GET_STATS,&gsm,sizeof(gsm));
    }

    while (0 < remaining) {
      message *msg = endpoint_receive(endpt,-1);
      switch (msg->tag) {
      case MSG_GET_STATS_RESPONSE: {
        get_stats_response_msg *gtrm = (get_stats_response_msg*)msg->data;

        int index = -1;
        for (i = 0; i < count; i++) {
          if (endpointid_equals(&gtrm->epid,&epids[i]))
            index = i;
        }
        if (0 > index) {
          printf("error: endpointid "EPID_FORMAT" not in list\n",EPID_ARGS(gtrm->epid));
        }
        else if (got[index]) {
          printf("error: already have response for "EPID_FORMAT"\n",EPID_ARGS(gtrm->epid));
        }
        else {
          got[index] = 1;
          memcpy(&messages[index],gtrm,sizeof(get_stats_response_msg));
          remaining--;
        }
        break;
      }
      default:
        fatal("Invalid message: %d",msg->tag);
        break;
      }
      message_free(msg);
    }


    printf("%-30s %-10s %-10s %-10s %-10s %-10s %-10s\n",
           "Task","Sparked","Running","Blocked","Cells","Bytes","Allocated");
    for (i = 0; i < count; i++) {
      char tmp[100];
      snprintf(tmp,100,EPID_FORMAT,EPID_ARGS(messages[i].epid));
      printf("%-30s %-10d %-10d %-10d %-10d %-10d %-10d\n",
             tmp,messages[i].sparked,messages[i].running,messages[i].blocked,
             messages[i].cells,messages[i].bytes,messages[i].alloc);
    }

    gettimeofday(&loopend,NULL);
    printf("getstats took %dms\n",timeval_diffms(loopstart,loopend));
    printf("\n");

    next = timeval_addms(next,1000);
    diff = timeval_diff(loopend,next);
    sleep_time.tv_sec = diff.tv_sec;
    sleep_time.tv_nsec = diff.tv_usec * 1000;
    nanosleep(&sleep_time,NULL);
  }

  free(epids);
  free(got);
  free(messages);
}

static void monitor(node *n, list *epids)
{
  pthread_t thread;
  monitor_data md;
  md.epids = epids;
  node_add_thread(n,"monitor",monitor_thread,&md,&thread);
  if (0 != pthread_join(thread,NULL))
    fatal("pthread_join: %s",strerror(errno));
}

int do_client(char *initial_str, int argc, const char **argv)
{
  int r = 0;
  const char *cmd;
  endpointid initial;
  node *n = node_start(LOG_WARNING,0);
  if (NULL == n)
    return -1;

  if (0 > string_to_mainchordid(n,initial_str,&initial))
    exit(1);

  node_add_thread(n,"connection",connection_thread,&initial,NULL);

  if (1 > argc) {
    fprintf(stderr,"Please specify a command\n");
    exit(1);
  }
  cmd = argv[0];

  if (!strcmp(cmd,"findall")) {
    list *nodes = find_nodes(n,initial,0,1);
    list_free(nodes,free);
  }
  else if (!strcmp(cmd,"findtasks")) {
    list *nodes = find_nodes(n,initial,0,0);
    list *tasks = find_tasks(n,nodes);
    list *l;
    for (l = tasks; l; l = l->next) {
      endpointid *epid = (endpointid*)l->data;
      printf("Task: "EPID_FORMAT"\n",EPID_ARGS(*epid));
    }
    list_free(nodes,free);
    list_free(tasks,free);
  }
  else if (!strcmp(cmd,"monitor")) {
    list *nodes = find_nodes(n,initial,0,0);
    list *tasks = find_tasks(n,nodes);
    monitor(n,tasks);
    list_free(nodes,free);
    list_free(tasks,free);
  }
  else if (!strcmp(cmd,"run")) {
    if (3 > argc) {
      fprintf(stderr,"Please specify number of machines and program\n");
      exit(1);
    }
    else {
      int want = atoi(argv[1]);
      const char *program = argv[2];
      list *nodes = find_nodes(n,initial,want,0);
      pthread_t thread;
      socketid out_sockid;
      output_arg oa;
      memset(&oa,0,sizeof(oa));
      out_sockid.coordid = node_add_thread(n,"output",output_thread,&oa,&thread);
      out_sockid.sid = 2;
      if (0 != client_run(n,nodes,program,out_sockid,argc-3,((const char**)argv)+3))
        exit(1);
      list_free(nodes,free);

      if (0 != pthread_join(thread,NULL))
        fatal("pthread_join: %s",strerror(errno));

      r = oa.rc;
    }
  }
  else {
    fprintf(stderr,"Unknown command: %s\n",cmd);
    exit(1);
  }

  node_shutdown(n);

  node_run(n);

  return r;
}
