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

#ifndef TEMP_FAILURE_RETRY
# define TEMP_FAILURE_RETRY(expression) \
  (__extension__                                                              \
    ({ long int __result;                                                     \
       do __result = (long int) (expression);                                 \
       while (__result == -1L && errno == EINTR);                             \
       __result; }))
#endif

#include "compiler/bytecode.h"
#include "src/nreduce.h"
#include "network.h"
#include "runtime.h"
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

//#define CHUNKSIZE 1024
#define CHUNKSIZE 65536
#define MSG_HEADER_SIZE sizeof(msgheader)

#define WELCOME_MESSAGE "Welcome to the nreduce 0.1 debug console. Enter commands below:\n\n> "

static socketcomm *socketcomm_new()
{
  return (socketcomm*)calloc(1,sizeof(socketcomm));
}

static void socketcomm_free(socketcomm *sc)
{
  /* FIXME: free message data */
  pthread_mutex_destroy(&sc->lock);
  pthread_cond_destroy(&sc->cond);
  free(sc);
}

static connection *find_connection(socketcomm *sc, struct in_addr nodeip, short nodeport)
{
  connection *conn;
  for (conn = sc->wlist.first; conn; conn = conn->next)
    if ((conn->ip.s_addr == nodeip.s_addr) && (conn->port == nodeport))
      return conn;
  return NULL;
}

task *find_task(socketcomm *sc, int localid)
{
  task *tsk;
  for (tsk = sc->tasks.first; tsk; tsk = tsk->next)
    if (tsk->localid == localid)
      break;
  return tsk;
}

static void got_message(socketcomm *sc, const msgheader *hdr, const void *data)
{
  task *tsk;
  message *newmsg;

  if (NULL == (tsk = find_task(sc,hdr->destlocalid)))
    fatal("Message received for unknown task %d\n",hdr->destlocalid);

  newmsg = (message*)calloc(1,sizeof(message));
  newmsg->hdr = *hdr;
  newmsg->data = (char*)malloc(hdr->size);
  memcpy(newmsg->data,data,hdr->size);
  task_add_message(tsk,newmsg);
}

static void process_received(socketcomm *sc, connection *conn)
{
  int start = 0;

  if (conn->isconsole) {
    console_process_received(sc,conn);
    return;
  }

  if (!conn->donewelcome) {
    if (conn->recvbuf->nbytes < strlen(WELCOME_MESSAGE)) {
      if (strncmp(conn->recvbuf->data,WELCOME_MESSAGE,conn->recvbuf->nbytes)) {
        /* Data sent so far is not part of the welcome message; must be a console client */
        conn->isconsole = 1;
        console_process_received(sc,conn);
        return;
      }
      else {
        /* What we've received so far matches the welcome message but it isn't complete yet;
           wait for more data */
      }
    }
    else { /* conn->recvbuf->nbytes <= strlen(WELCOME_MESSAGE */
      if (strncmp(conn->recvbuf->data,WELCOME_MESSAGE,strlen(WELCOME_MESSAGE))) {
        /* Have enough bytes but it's not the welcome message; must be a console client */
        conn->isconsole = 1;
        console_process_received(sc,conn);
        return;
      }
      else {
        /* We have the welcome message; it's a peer and we want to exchange ids next */
        conn->donewelcome = 1;
        start += strlen(WELCOME_MESSAGE);
        array_append(conn->sendbuf,&sc->listenport,sizeof(int));
      }
    }
  }

  if (0 > conn->port) {
    if (sizeof(int) > conn->recvbuf->nbytes-start) {
      array_remove_data(conn->recvbuf,start);
      return;
    }
    conn->port = *(int*)&conn->recvbuf->data[start];
    start += sizeof(int);
  }

  /* inspect the next section of the input buffer to see if it contains a complete message */
  while (MSG_HEADER_SIZE <= conn->recvbuf->nbytes-start) {
    msgheader *hdr = (msgheader*)&conn->recvbuf->data[start];

    /* verify header */
    assert(0 <= hdr->size);
    assert(0 <= hdr->tag);
    assert(MSG_COUNT > hdr->tag);

    hdr->source.nodeip = conn->ip;
    hdr->source.nodeport = conn->port;

    if (MSG_HEADER_SIZE+hdr->size > conn->recvbuf->nbytes-start)
      break; /* incomplete message */

    /* complete message present; add it to the mailbox */
    got_message(sc,hdr,&conn->recvbuf->data[start+MSG_HEADER_SIZE]);
    start += MSG_HEADER_SIZE+hdr->size;
  }

  /* remove the data we've consumed from the receive buffer */
  array_remove_data(conn->recvbuf,start);
}

void socket_send_raw(task *tsk, taskid desttaskid, int tag, const void *data, int size)
{
  socketcomm *sc = (socketcomm*)tsk->commdata;
  connection *conn;
  msgheader hdr;
  int c = 0;

  pthread_mutex_lock(&sc->lock);

  memset(&hdr,0,sizeof(msgheader));
  hdr.source.localid = tsk->localid;
  hdr.sourceindex = tsk->pid;
  hdr.destlocalid = desttaskid.localid;
  hdr.size = size;
  hdr.tag = tag;

  if ((desttaskid.nodeip.s_addr == sc->listenip.s_addr) &&
      (desttaskid.nodeport == sc->listenport)) {
    hdr.source.nodeip = sc->listenip;
    hdr.source.nodeport = sc->listenport;
    got_message(sc,&hdr,data);
  }
  else {
    if (NULL == (conn = find_connection(sc,desttaskid.nodeip,desttaskid.nodeport))) {
      unsigned char *addrbytes = (unsigned char*)&desttaskid.nodeip.s_addr;
      fatal("Could not find destination connection to %u.%u.%u.%u:%d",
            addrbytes[0],addrbytes[1],addrbytes[2],addrbytes[3],desttaskid.nodeport);
    }

    array_append(conn->sendbuf,&hdr,sizeof(msgheader));
    array_append(conn->sendbuf,data,size);
    write(sc->ioready_writefd,&c,1);
  }
  pthread_mutex_unlock(&sc->lock);
}

void socket_send(task *tsk, int destid, int tag, char *data, int size)
{
  #ifdef MSG_DEBUG
  msg_print(tsk,destid,tag,data,size);
  #endif

  if (destid == tsk->pid)
    fatal("Attempt to send message (with tag %d) to task on same host\n",msg_names[tag]);

  socket_send_raw(tsk,tsk->idmap[destid],tag,data,size);
}

static connection *add_connection(socketcomm *sc, const char *hostname, int sock)
{
  connection *conn = (connection*)calloc(1,sizeof(connection));
  conn->hostname = strdup(hostname);
  conn->ip.s_addr = 0;
  conn->sock = sock;
  conn->readfd = -1;
  conn->port = -1;
  conn->recvbuf = array_new(1);
  conn->sendbuf = array_new(1);
  array_append(conn->sendbuf,WELCOME_MESSAGE,strlen(WELCOME_MESSAGE));
  llist_append(&sc->wlist,conn);
  return conn;
}

static void remove_connection(socketcomm *sc, connection *conn)
{
  printf("Removing connection %s\n",conn->hostname);
  close(conn->sock);
  llist_remove(&sc->wlist,conn);
  free(conn->hostname);
  array_free(conn->recvbuf);
  array_free(conn->sendbuf);
  free(conn);
}

static connection *initiate_connection(socketcomm *sc, const char *hostname)
{
  struct sockaddr_in addr;
  struct hostent *he;
  int sock;
  int connected = 0;
  connection *conn;

  if (NULL == (he = gethostbyname(hostname))) {
    fprintf(stderr,"%s: %s\n",hostname,hstrerror(h_errno));
    return NULL;
  }

  if (4 != he->h_length) {
    fprintf(stderr,"%s: invalid address type\n",hostname);
    return NULL;
  }

  if (0 > (sock = socket(AF_INET,SOCK_STREAM,0))) {
    perror("socket");
    return NULL;
  }

  if (0 > fdsetblocking(sock,0))
    return NULL;

  addr.sin_family = AF_INET;
  addr.sin_port = htons(WORKER_PORT);
  addr.sin_addr = *((struct in_addr*)he->h_addr);
  memset(&addr.sin_zero,0,8);

  if (0 == connect(sock,(struct sockaddr*)&addr,sizeof(struct sockaddr))) {
    connected = 1;
    printf("Connected (immediately) to %s\n",hostname);
  }
  else if (EINPROGRESS != errno) {
    perror("connect");
    return NULL;
  }

  conn = add_connection(sc,hostname,sock);
  conn->connected = connected;
  conn->ip = *((struct in_addr*)he->h_addr);

  return conn;
}

static int handle_connected(socketcomm *sc, connection *conn)
{
  int err;
  int optlen = sizeof(int);

  if (0 > getsockopt(conn->sock,SOL_SOCKET,SO_ERROR,&err,&optlen)) {
    perror("getsockopt");
    return -1;
  }

  if (0 == err) {
    int yes = 1;
    conn->connected = 1;
    printf("Connected to %s\n",conn->hostname);

    if (0 > setsockopt(conn->sock,SOL_TCP,TCP_NODELAY,&yes,sizeof(int))) {
      perror("setsockopt TCP_NODELAY");
      return -1;
    }
  }
  else {
    printf("Connection to %s failed: %s\n",conn->hostname,strerror(err));
    return -1;
  }

  return 0;
}

static void handle_write(socketcomm *sc, connection *conn)
{
  while (0 < conn->sendbuf->nbytes) {
    int w = TEMP_FAILURE_RETRY(write(conn->sock,conn->sendbuf->data,conn->sendbuf->nbytes));
    if ((0 > w) && (EAGAIN == errno))
      break;

    if (0 > w) {
      fprintf(stderr,"write() to %s failed: %s\n",conn->hostname,strerror(errno));
      if (conn->isconsole)
        remove_connection(sc,conn);
      else
        abort();
      return;
    }

    if (0 == w) {
      fprintf(stderr,"write() to %s failed: channel closed (maybe it crashed?)\n",conn->hostname);
      if (conn->isconsole)
        remove_connection(sc,conn);
      else
        abort();
      return;
    }

    array_remove_data(conn->sendbuf,w);
  }

  if ((0 == conn->sendbuf->nbytes) && conn->toclose)
    remove_connection(sc,conn);
}

static void handle_read(socketcomm *sc, connection *conn)
{
  /* Just do one read; there may still be more data pending after this, but we want to also
     give a fair chance for other connections to be read from. We also want to process messages
     as they arrive rather than waiting until can't ready an more, to avoid buffering up large
     numbers of messages */
  int r;
  array *buf = conn->recvbuf;

  array_mkroom(conn->recvbuf,CHUNKSIZE);
  r = TEMP_FAILURE_RETRY(read(conn->sock,
                              &buf->data[buf->nbytes],
                              CHUNKSIZE));

  if ((0 > r) && (EAGAIN == errno))
    return;

  if (0 > r) {
    fprintf(stderr,"read() from %s failed: %s\n",conn->hostname,strerror(errno));
    if (conn->isconsole)
      remove_connection(sc,conn);
    else
      abort();
    return;
  }

  if (0 == r) {
    if (conn->isconsole) {
      printf("Client %s closed connection\n",conn->hostname);
      remove_connection(sc,conn);
    }
    else {
      fprintf(stderr,"read() from %s failed: channel closed (maybe it crashed?)\n",conn->hostname);
      abort();
    }
    return;
  }

  conn->recvbuf->nbytes += r;
  process_received(sc,conn);
}

static void handle_new_connection(socketcomm *sc)
{
  struct sockaddr_in remote_addr;
  struct hostent *he;
  int sin_size = sizeof(struct sockaddr_in);
  int clientfd;
  connection *conn;
  int yes = 1;
  if (0 > (clientfd = accept(sc->listenfd,(struct sockaddr*)&remote_addr,&sin_size))) {
    perror("accept");
    return;
  }

  if (0 > fdsetblocking(clientfd,0)) {
    close(clientfd);
    return;
  }

  if (0 > setsockopt(clientfd,SOL_TCP,TCP_NODELAY,&yes,sizeof(int))) {
    perror("setsockopt TCP_NODELAY");
    return;
  }

  he = gethostbyaddr(&remote_addr.sin_addr,sizeof(struct in_addr),AF_INET);
  if (NULL == he) {
    unsigned char *addrbytes = (unsigned char*)&remote_addr.sin_addr;
    char hostname[100];
    sprintf(hostname,"%u.%u.%u.%u",
            addrbytes[0],addrbytes[1],addrbytes[2],addrbytes[3]);
    printf("Got connection from %s\n",hostname);
    conn = add_connection(sc,hostname,clientfd);
  }
  else {
    printf("Got connection from %s\n",he->h_name);
    conn = add_connection(sc,he->h_name,clientfd);
  }
  conn->connected = 1;
  conn->ip = remote_addr.sin_addr;
}

static void *ioloop(void *arg)
{
  socketcomm *sc = (socketcomm*)arg;

  pthread_mutex_lock(&sc->lock);
  while (1) {
    int highest = -1;
    int s;
    fd_set readfds;
    fd_set writefds;
    connection *conn;
    connection *next;
    int havewrite = 0;

    FD_ZERO(&readfds);
    FD_ZERO(&writefds);

    FD_SET(sc->listenfd,&readfds);
    if (highest < sc->listenfd)
      highest = sc->listenfd;

    FD_SET(sc->ioready_readfd,&readfds);
    if (highest < sc->ioready_readfd)
      highest = sc->ioready_readfd;

    for (conn = sc->wlist.first; conn; conn = conn->next) {
      assert(0 <= conn->sock);
      if (highest < conn->sock)
        highest = conn->sock;
      FD_SET(conn->sock,&readfds);

      if ((0 < conn->sendbuf->nbytes) || !conn->connected) {
        FD_SET(conn->sock,&writefds);
        havewrite = 1;
      }

      /* Note: it is possible that we did not find any data waiting to be written at this point,
         but some will become avaliable either just after we release the lock, or while the select
         is actually blocked. socket_send() writes a single byte of data to the sc's ioready pipe,
         which will cause the select to be woken, and we will look around again to write the
         data. */
    }

    pthread_mutex_unlock(&sc->lock);
    s = select(highest+1,&readfds,&writefds,NULL,NULL);
    pthread_mutex_lock(&sc->lock);

    if (0 > s)
      fatal("select: %s",strerror(errno));

    pthread_cond_broadcast(&sc->cond);

    if (FD_ISSET(sc->ioready_readfd,&readfds)) {
      int c;
      if (0 > read(sc->ioready_readfd,&c,1))
        fatal("Cann't read from ioready_readfd: %s\n",strerror(errno));
    }

    if (0 == s)
      continue; /* timed out; no sockets are ready for reading or writing */

    /* Do all the writing we can */
    for (conn = sc->wlist.first; conn; conn = next) {
      next = conn->next;
      if (FD_ISSET(conn->sock,&writefds)) {
        if (conn->connected)
          handle_write(sc,conn);
        else
          handle_connected(sc,conn);
      }
    }

    /* Read data */
    for (conn = sc->wlist.first; conn; conn = next) {
      next = conn->next;
      if (FD_ISSET(conn->sock,&readfds))
        handle_read(sc,conn);
    }

    /* accept new connections */
    if (FD_ISSET(sc->listenfd,&readfds))
      handle_new_connection(sc);
  }
  pthread_mutex_unlock(&sc->lock);
}

int socket_recv(task *tsk, int *tag, char **data, int *size, int delayms)
{
  message *msg = task_next_message(tsk,delayms);
  int from;
  if (NULL == msg)
    return -1;

  *tag = msg->hdr.tag;
  *data = msg->data;
  *size = msg->hdr.size;
  from = msg->hdr.sourceindex;
  assert(tsk->localid == msg->hdr.destlocalid);
  free(msg);
  return from;
}

static void sigabrt(int sig)
{
  char *str = "abort()\n";
  write(STDERR_FILENO,str,strlen(str));
}

static void *btarray[1024];

static void sigsegv(int sig)
{
  char *str = "Segmentation fault\n";
  int size;
  write(STDERR_FILENO,str,strlen(str));

  size = backtrace(btarray,1024);
  backtrace_symbols_fd(btarray,size,STDERR_FILENO);

  signal(sig,SIG_DFL);
  kill(getpid(),sig);
}

static void exited()
{
  char *str = "exited\n";
  write(STDOUT_FILENO,str,strlen(str));
}

static void startlog(int logfd)
{
  int i;

  setbuf(stdout,NULL);
  setbuf(stderr,NULL);

  close(STDOUT_FILENO);
  close(STDERR_FILENO);

  dup2(logfd,STDOUT_FILENO);
  dup2(logfd,STDERR_FILENO);

  signal(SIGABRT,sigabrt);
  signal(SIGSEGV,sigsegv);

  atexit(exited);

  for (i = 0; i < 20; i++)
    printf("\n");
  printf("==================================================\n");
}

static int wait_for_startup_notification(int msock)
{
  int r;
  unsigned char c;
  do {
    r = TEMP_FAILURE_RETRY(read(msock,&c,1));
  } while ((0 > r) && (EAGAIN == errno));
  if (0 > r) {
    fprintf(stderr,"read() from master: %s\n",strerror(errno));
    return -1;
  }
  return c;
}

static int connect_workers(socketcomm *sc, int want, int listenfd, int myindex)
{
  int have;
  connection *conn;
  do {
    have = 0;

    pthread_mutex_lock(&sc->lock);
    pthread_cond_wait(&sc->cond,&sc->lock);
    pthread_mutex_unlock(&sc->lock);

    for (conn = sc->wlist.first; conn; conn = conn->next)
      if (conn->connected && (0 <= conn->port))
        have++;
  } while (have < want);

  printf("All connections now established\n");
  return 0;
}

task *add_task(socketcomm *sc, int pid, int groupsize, const char *bcdata, int bcsize, int localid)
{
  task *tsk = task_new(pid,groupsize,bcdata,bcsize);
  tsk->localid = localid;

  tsk->commdata = sc;
  tsk->output = stdout;

  pthread_mutex_lock(&sc->lock);
  llist_append(&sc->tasks,tsk);
  pthread_mutex_unlock(&sc->lock);

  if ((0 == pid) && (NULL != bcdata)) {
    frame *initial = frame_alloc(tsk);
    initial->address = 0;
    initial->fno = -1;
    initial->data = (pntr*)malloc(sizeof(pntr));
    initial->alloc = 1;
    initial->c = alloc_cell(tsk);
    initial->c->type = CELL_FRAME;
    make_pntr(initial->c->field1,initial);
    run_frame(tsk,initial);
  }

  return tsk;
}

static void get_responses(socketcomm *sc, task *tsk, int tag,
                          int count, taskid *managerids, int *responses)
{
  int *gotresponse = (int*)calloc(count,sizeof(int));
  int allresponses;
  int i;
  do {
    message *msg = task_next_message(tsk,-1);
    int sender = -1;
    assert(msg);

    if (tag != msg->hdr.tag)
      fatal("%s: Got invalid response tag: %d\n",msg_names[tag],msg->hdr.tag);

    for (i = 0; i < count; i++)
      if (!memcmp(&msg->hdr.source,&managerids[i],sizeof(taskid)))
        sender = i;

    if (0 > sender) {
      fprintf(stderr,"%s: Got response from unknown source ",msg_names[tag]);
      print_taskid(stderr,msg->hdr.source);
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
  } while (!allresponses);

  free(gotresponse);
}

typedef struct {
  socketcomm *sc;
  array *bcarr;
  array *hostnames;
} startarg;

static void *start_task(void *arg)
{
  startarg *sa = (startarg*)arg;
  socketcomm *sc = sa->sc;
  task *tsk = add_task(sc,0,0,NULL,0,1234); /* FIXME: use variable task id */
  int count = array_count(sa->hostnames);
  taskid *managerids = (taskid*)calloc(count,sizeof(taskid));
  taskid *taskids = (taskid*)calloc(count,sizeof(taskid));
  int *localids = (int*)calloc(count,sizeof(int));
  char *bcdata = sa->bcarr->data;
  int bcsize = sa->bcarr->nbytes;
  int ntsize = sizeof(newtask_msg)+bcsize;
  newtask_msg *ntmsg = (newtask_msg*)calloc(1,ntsize);
  int initsize = sizeof(inittask_msg)+count*sizeof(taskid);
  inittask_msg *initmsg = (inittask_msg*)calloc(1,initsize);
  int i;

  ntmsg->groupsize = count;
  ntmsg->bcsize = bcsize;
  memcpy(&ntmsg->bcdata,bcdata,bcsize);

  /* FIXME: should probably do this resolution outside of the task starting routine */
  for (i = 0; i < count; i++) {
    struct hostent *he;
    char *hostname = array_item(sa->hostnames,i,char*);

    if (NULL == (he = gethostbyname(hostname)))
      fatal("%s: %s\n",hostname,hstrerror(h_errno));

    if (4 != he->h_length)
      fatal("%s: invalid address type\n",hostname);

    managerids[i].nodeip = *((struct in_addr*)he->h_addr);
    managerids[i].nodeport = WORKER_PORT;
    managerids[i].localid = MANAGER_ID;

    taskids[i].nodeip = *((struct in_addr*)he->h_addr);
    taskids[i].nodeport = WORKER_PORT;
  }

  /* Send NEWTASK messages, containing the pid, groupsize, and bytecode */
  for (i = 0; i < count; i++) {
    ntmsg->pid = i;
    socket_send_raw(tsk,managerids[i],MSG_NEWTASK,(char*)ntmsg,ntsize);
  }

  /* Get NEWTASK responses, containing the local id of each task on the remote node */
  get_responses(sc,tsk,MSG_NEWTASKRESP,count,managerids,localids);
  for (i = 0; i < count; i++)
    taskids[i].localid = localids[i];
  printf("All tasks created\n");

  /* Send INITTASK messages, giving each task a copy of the idmap */
  initmsg->count = count;
  memcpy(initmsg->idmap,taskids,count*sizeof(taskid));
  for (i = 0; i < count; i++) {
    initmsg->localid = taskids[i].localid;
    socket_send_raw(tsk,managerids[i],MSG_INITTASK,(char*)initmsg,initsize);
  }

  /* Wait for all INITTASK messages to be processed */
  get_responses(sc,tsk,MSG_INITTASKRESP,count,managerids,NULL);
  printf("All tasks initialised\n");

  /* Start all tasks */
  for (i = 0; i < count; i++)
    socket_send_raw(tsk,managerids[i],MSG_STARTTASK,&localids[i],sizeof(int));

  /* Wait for notification of all tasks starting */
  get_responses(sc,tsk,MSG_STARTTASKRESP,count,managerids,NULL);
  printf("All tasks started\n");

  free(ntmsg);
  free(managerids);
  free(taskids);
  free(localids);

  return NULL;
}

static void start_task_using_manager(socketcomm *sc, array *bcarr, array *hostnames)
{
  startarg arg;
  pthread_t startthread;
  arg.sc = sc;
  arg.bcarr = bcarr;
  arg.hostnames = hostnames;
  printf("Distributed process creation starting\n");
  if (0 > pthread_create(&startthread,NULL,start_task,&arg))
    fatal("pthread_create: %s",strerror(errno));
  if (0 > pthread_join(startthread,NULL))
    fatal("pthread_join: %s",strerror(errno));
  printf("Distributed process creation done\n");
}

static array *read_data(int fd)
{
  int size;
  int got = 0;
  array *arr;
  int r = TEMP_FAILURE_RETRY(read(fd,&size,sizeof(int)));
  if (0 > r) {
    perror("read");
    return NULL;
  }
  if (sizeof(int) != r) {
    fprintf(stderr,"Error reading size, only got %d bytes\n",r);
    return NULL;
  }
  arr = array_new(1);
  printf("size = %d\n",size);
  while (got < size) {
    char buf[CHUNKSIZE];
    int want = (CHUNKSIZE < size-got) ? CHUNKSIZE : size-got;
    if (0 > (r = TEMP_FAILURE_RETRY(read(fd,buf,want)))) {
      perror("read");
      array_free(arr);
      return NULL;
    }
    array_append(arr,buf,r);
    got += r;
  }
  printf("total got: %d\n",got);
  return arr;
}

int worker(const char *hostsfile, const char *masteraddr)
{
  char *mhost;
  int mport;
  int msock;
  struct in_addr addr;
  int i;
  int myindex;
  array *bcarr = NULL;
  socketcomm *sc;
  int logfd;
  char notify = '1';
  connection *conn;
  array *hostnames;
  int nworkers;
  int listenfd;
  int remaining;
  int pipefds[2];
  struct hostent *he;
  char *hostname;

  if (0 > (logfd = open("/tmp/nreduce.log",O_WRONLY|O_APPEND|O_CREAT,0666))) {
    perror("/tmp/nreduce.log");
    exit(1);
  }

  write(STDOUT_FILENO,&notify,1);

  startlog(logfd);

  printf("Running as worker, hostsfile = %s, master = %s, pid = %d\n",
         hostsfile,masteraddr,getpid());

  if (NULL == (hostnames = read_hostnames(hostsfile)))
    return -1;

  if (0 > parse_address(masteraddr,&mhost,&mport)) {
    fprintf(stderr,"Invalid address: %s\n",masteraddr);
    return -1;
  }

  addr.s_addr = INADDR_ANY;
  if (0 > (listenfd = start_listening(addr,WORKER_PORT))) {
    free(mhost);
    return -1;
  }

  if (0 > (msock = connect_host(mhost,mport))) {
    free(mhost);
    close(listenfd);
    fprintf(stderr,"Can't connect to %s: %s\n",masteraddr,strerror(errno));
    return -1;
  }

  if (0 > (myindex = wait_for_startup_notification(msock)))
    return -1;

  printf("Got startup notification; i am worker %d\n",myindex);

  if (0 > fdsetblocking(msock,1))
    return -1;

  if (NULL == (bcarr = read_data(msock)))
    return -1;

  printf("Got bytecode, size = %d bytes\n",bcarr->nbytes);

  sc = socketcomm_new();
  sc->listenfd = listenfd;
  sc->listenport = WORKER_PORT;

  hostname = array_item(hostnames,myindex,char*);
  if (NULL == (he = gethostbyname(hostname)))
    fatal("%s: %s\n",hostname,hstrerror(h_errno));

  if (4 != he->h_length)
    fatal("%s: invalid address type\n",hostname);

  sc->listenip = *((struct in_addr*)he->h_addr);
  printf("my ip is %u.%u.%u.%u\n",
         ((unsigned char*)&sc->listenip.s_addr)[0],
         ((unsigned char*)&sc->listenip.s_addr)[1],
         ((unsigned char*)&sc->listenip.s_addr)[2],
         ((unsigned char*)&sc->listenip.s_addr)[3]);

  sc->nextlocalid = 1;
  nworkers = array_count(hostnames);
  remaining = nworkers-1;

  pipe(pipefds);
  sc->ioready_readfd = pipefds[0];
  sc->ioready_writefd = pipefds[1];
  pthread_mutex_init(&sc->lock,NULL);
  pthread_cond_init(&sc->cond,NULL);

  /* connect to workers > myindex, accept connections from those < myindex */
  for (i = myindex+1; i < nworkers; i++) {
    if (NULL == (conn = initiate_connection(sc,array_item(hostnames,i,char*))))
      return -1;
    if (conn->connected)
      remaining--;
  }

  start_manager(sc);

  if (0 > pthread_create(&sc->iothread,NULL,ioloop,sc))
    fatal("pthread_create: %s",strerror(errno));

  if (0 > connect_workers(sc,remaining,listenfd,myindex))
    return -1;

  printf("Hosts:\n");
  for (conn = sc->wlist.first; conn; conn = conn->next)
    printf("%s, sock %d\n",conn->hostname,conn->sock);

  if (0 == myindex)
    start_task_using_manager(sc,bcarr,hostnames);

  printf("suspending main thread\n");
  if (0 > pthread_join(sc->iothread,NULL))
    fatal("pthread_join: %s",strerror(errno));

  socketcomm_free(sc);

  if (bcarr)
    array_free(bcarr);

  free(mhost);
  close(listenfd);
  return 0;
}
