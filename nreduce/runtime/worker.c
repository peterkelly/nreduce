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

/* FIXME: the sin_addr structure is supposed to be in network byte order; we are treating
   it as host by order here (which appears to be ok on x86) */

/* FIXME: must use re-entrant versions of all relevant socket functions, e.g. gethostbyname_r() */

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
  socketcomm *sc = (socketcomm*)calloc(1,sizeof(socketcomm));
  pthread_mutex_init(&sc->lock,NULL);
  pthread_cond_init(&sc->cond,NULL);
  sc->listenfd = -1;
  return sc;
}

static void socketcomm_free(socketcomm *sc)
{
  /* FIXME: free message data */
  if (0 <= sc->listenfd)
    close(sc->listenfd);
  pthread_mutex_destroy(&sc->lock);
  pthread_cond_destroy(&sc->cond);
  free(sc);
}

static connection *find_connection(socketcomm *sc, struct in_addr nodeip, short nodeport)
{
  connection *conn;
  for (conn = sc->connections.first; conn; conn = conn->next)
    if ((conn->ip.s_addr == nodeip.s_addr) && (conn->port == nodeport))
      return conn;
  return NULL;
}

endpoint *find_endpoint(socketcomm *sc, int localid)
{
  endpoint *endpt;
  for (endpt = sc->endpoints.first; endpt; endpt = endpt->next)
    if (endpt->localid == localid)
      break;
  return endpt;
}

task *find_task(socketcomm *sc, int localid)
{
  endpoint *endpt = find_endpoint(sc,localid);
  if (NULL == endpt)
    return NULL;
  if (TASK_ENDPOINT != endpt->type)
    fatal("Request for endpoint %d that is not a task\n",localid);
  return (task*)endpt->data;
}

static void got_message(socketcomm *sc, const msgheader *hdr, const void *data)
{
  endpoint *endpt;
  message *newmsg;

  if (NULL == (endpt = find_endpoint(sc,hdr->destlocalid)))
    fatal("Message received for unknown endpoint %d\n",hdr->destlocalid);

  newmsg = (message*)calloc(1,sizeof(message));
  newmsg->hdr = *hdr;
  newmsg->data = (char*)malloc(hdr->size);
  memcpy(newmsg->data,data,hdr->size);
  endpoint_add_message(endpt,newmsg);
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
        array_append(conn->sendbuf,&conn->ip,sizeof(struct in_addr));
        array_append(conn->sendbuf,&sc->listenport,sizeof(int));
      }
    }
  }

  if (0 > conn->port) {
    struct in_addr localip;

    if (sizeof(struct in_addr)+sizeof(int) > conn->recvbuf->nbytes-start) {
      array_remove_data(conn->recvbuf,start);
      return;
    }
    memcpy(&localip.s_addr,&conn->recvbuf->data[start],sizeof(struct in_addr));
    start += sizeof(struct in_addr);
    conn->port = *(int*)&conn->recvbuf->data[start];
    start += sizeof(int);

    if (sc->havelistenip) {
      if (memcmp(&sc->listenip.s_addr,&localip.s_addr,sizeof(struct in_addr)))
        fatal("Client is using a different IP to connect to me than what I expect\n");
    }
    else {
      unsigned char *ipbytes = (unsigned char*)&localip.s_addr;
      memcpy(&sc->listenip.s_addr,&localip.s_addr,sizeof(struct in_addr));
      sc->havelistenip = 1;
      printf("Got my listenip: %u.%u.%u.%u\n",ipbytes[0],ipbytes[1],ipbytes[2],ipbytes[3]);
    }
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

void socket_send_raw(socketcomm *sc, endpoint *endpt,
                     endpointid destendpointid, int tag, const void *data, int size)
{
  connection *conn;
  msgheader hdr;
  int c = 0;

  pthread_mutex_lock(&sc->lock);

  memset(&hdr,0,sizeof(msgheader));
  hdr.source.localid = endpt->localid;
  hdr.destlocalid = destendpointid.localid;
  hdr.size = size;
  hdr.tag = tag;

  if ((destendpointid.nodeip.s_addr == sc->listenip.s_addr) &&
      (destendpointid.nodeport == sc->listenport)) {
    hdr.source.nodeip = sc->listenip;
    hdr.source.nodeport = sc->listenport;
    got_message(sc,&hdr,data);
  }
  else {
    if (NULL == (conn = find_connection(sc,destendpointid.nodeip,destendpointid.nodeport))) {
      unsigned char *addrbytes = (unsigned char*)&destendpointid.nodeip.s_addr;
      fatal("Could not find destination connection to %u.%u.%u.%u:%d",
            addrbytes[0],addrbytes[1],addrbytes[2],addrbytes[3],destendpointid.nodeport);
    }

    array_append(conn->sendbuf,&hdr,sizeof(msgheader));
    array_append(conn->sendbuf,data,size);
    write(sc->ioready_writefd,&c,1);
  }
  pthread_mutex_unlock(&sc->lock);
}

void socket_send(task *tsk, int destid, int tag, char *data, int size)
{
  socketcomm *sc = (socketcomm*)tsk->commdata;
  #ifdef MSG_DEBUG
  msg_print(tsk,destid,tag,data,size);
  #endif

  if (destid == tsk->pid)
    fatal("Attempt to send message (with tag %d) to task on same host\n",msg_names[tag]);

  socket_send_raw(sc,tsk->endpt,tsk->idmap[destid],tag,data,size);
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
  llist_append(&sc->connections,conn);
  return conn;
}

static void remove_connection(socketcomm *sc, connection *conn)
{
  printf("Removing connection %s\n",conn->hostname);
  close(conn->sock);
  llist_remove(&sc->connections,conn);
  free(conn->hostname);
  array_free(conn->recvbuf);
  array_free(conn->sendbuf);
  free(conn);
}

connection *initiate_connection(socketcomm *sc, const char *hostname, int port)
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
  addr.sin_port = htons(port);
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
    remove_connection(sc,conn);
    return -1;
  }

  if (0 == err) {
    int yes = 1;
    conn->connected = 1;
    printf("Connected to %s\n",conn->hostname);

    if (0 > setsockopt(conn->sock,SOL_TCP,TCP_NODELAY,&yes,sizeof(int))) {
      perror("setsockopt TCP_NODELAY");
      remove_connection(sc,conn);
      return -1;
    }
  }
  else {
    printf("Connection to %s failed: %s\n",conn->hostname,strerror(err));
    remove_connection(sc,conn);
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
  while (!sc->shutdown) {
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

    for (conn = sc->connections.first; conn; conn = conn->next) {
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
    for (conn = sc->connections.first; conn; conn = next) {
      next = conn->next;
      if (FD_ISSET(conn->sock,&writefds)) {
        if (conn->connected)
          handle_write(sc,conn);
        else
          handle_connected(sc,conn);
      }
    }

    /* Read data */
    for (conn = sc->connections.first; conn; conn = next) {
      next = conn->next;
      if (FD_ISSET(conn->sock,&readfds))
        handle_read(sc,conn);
    }

    /* accept new connections */
    if (FD_ISSET(sc->listenfd,&readfds))
      handle_new_connection(sc);
  }
  pthread_mutex_unlock(&sc->lock);
  return NULL;
}

static int get_idmap_index(task *tsk, endpointid tid)
{
  int i;
  for (i = 0; i < tsk->groupsize; i++)
    if (!memcmp(&tsk->idmap[i],&tid,sizeof(endpointid)))
      return i;
  return -1;
}

int socket_recv(task *tsk, int *tag, char **data, int *size, int delayms)
{
  message *msg = endpoint_next_message(tsk->endpt,delayms);
  int from;
  if (NULL == msg)
    return -1;

  *tag = msg->hdr.tag;
  *data = msg->data;
  *size = msg->hdr.size;
  from = get_idmap_index(tsk,msg->hdr.source);
  assert(0 <= from);
  assert(tsk->endpt->localid == msg->hdr.destlocalid);
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

void add_endpoint(socketcomm *sc, endpoint *endpt)
{
  pthread_mutex_lock(&sc->lock);
  llist_append(&sc->endpoints,endpt);
  pthread_mutex_unlock(&sc->lock);
}

void remove_endpoint(socketcomm *sc, endpoint *endpt)
{
  pthread_mutex_lock(&sc->lock);
  llist_remove(&sc->endpoints,endpt);
  pthread_mutex_unlock(&sc->lock);
}

task *add_task(socketcomm *sc, int pid, int groupsize, const char *bcdata, int bcsize, int localid)
{
  task *tsk = task_new(pid,groupsize,bcdata,bcsize,localid);

  tsk->commdata = sc;
  tsk->output = stdout;

  add_endpoint(sc,tsk->endpt);

  if ((0 == pid) && (NULL != bcdata)) {
    frame *initial = frame_new(tsk);
    initial->instr = bc_instructions(tsk->bcdata);
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

static void get_responses(socketcomm *sc, endpoint *endpt, int tag,
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
  socketcomm *sc;
  char *bcdata;
  int bcsize;
  int count;
  endpointid *managerids;
} startarg;

/* FIXME: handle the case where a shutdown request occurs while this is running */
static void *start_task(void *arg)
{
  startarg *sa = (startarg*)arg;
  socketcomm *sc = sa->sc;
  endpoint *endpt = endpoint_new(1234,LAUNCHER_ENDPOINT,NULL); /* FIXME: use variable task id */
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

  add_endpoint(sc,endpt);

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
    socket_send_raw(sc,endpt,managerids[i],MSG_NEWTASK,(char*)ntmsg,ntsize);
  }

  /* Get NEWTASK responses, containing the local id of each task on the remote node */
  get_responses(sc,endpt,MSG_NEWTASKRESP,count,managerids,localids);
  for (i = 0; i < count; i++)
    endpointids[i].localid = localids[i];
  printf("All tasks created\n");

  /* Send INITTASK messages, giving each task a copy of the idmap */
  initmsg->count = count;
  memcpy(initmsg->idmap,endpointids,count*sizeof(endpointid));
  for (i = 0; i < count; i++) {
    initmsg->localid = endpointids[i].localid;
    socket_send_raw(sc,endpt,managerids[i],MSG_INITTASK,(char*)initmsg,initsize);
  }

  /* Wait for all INITTASK messages to be processed */
  get_responses(sc,endpt,MSG_INITTASKRESP,count,managerids,NULL);
  printf("All tasks initialised\n");

  /* Start all tasks */
  for (i = 0; i < count; i++)
    socket_send_raw(sc,endpt,managerids[i],MSG_STARTTASK,&localids[i],sizeof(int));

  /* Wait for notification of all tasks starting */
  get_responses(sc,endpt,MSG_STARTTASKRESP,count,managerids,NULL);
  printf("All tasks started\n");

  printf("Distributed process creation done\n");

  free(bcdata);
  free(ntmsg);
  free(initmsg);
  free(managerids);
  free(endpointids);
  free(localids);
  remove_endpoint(sc,endpt);
  endpoint_free(endpt);
  free(arg);
  return NULL;
}

void start_task_using_manager(socketcomm *sc, const char *bcdata, int bcsize,
                              endpointid *managerids, int count)
{
  startarg *arg = (startarg*)calloc(1,sizeof(startarg));
  pthread_t startthread;
  arg->sc = sc;
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
    fatal("pthread_join: %s",strerror(errno));
  printf("Distributed process creation started\n");
}

int worker(const char *host, int port)
{
  struct in_addr addr;
  socketcomm *sc;
  int listenfd;
  int pipefds[2];
  endpoint *endpt;

  signal(SIGABRT,sigabrt);
  signal(SIGSEGV,sigsegv);
  atexit(exited);

  sc = socketcomm_new();

  addr.s_addr = INADDR_ANY;
  if (NULL != host) {
    struct hostent *he;
    if (NULL == (he = gethostbyname(host))) {
      fprintf(stderr,"%s: %s\n",host,hstrerror(h_errno));
      return -1;
    }

    if (4 != he->h_length) {
      fprintf(stderr,"%s: invalid address type\n",host);
      return -1;
    }

    sc->listenip = *((struct in_addr*)he->h_addr);
    sc->havelistenip = 1;
    addr = *((struct in_addr*)he->h_addr);
  }

  if (0 > (listenfd = start_listening(addr,port))) {
    socketcomm_free(sc);
    return -1;
  }

  sc->listenfd = listenfd;
  sc->listenport = port;
  sc->nextlocalid = 1;

  pipe(pipefds);
  sc->ioready_readfd = pipefds[0];
  sc->ioready_writefd = pipefds[1];

  start_manager(sc);

  if (0 > wrap_pthread_create(&sc->iothread,NULL,ioloop,sc))
    fatal("pthread_create: %s",strerror(errno));

  printf("Worker started, pid = %d, listening addr = %s:%d\n",
         getpid(),host ? host : "0.0.0.0",port);

  if (0 > wrap_pthread_join(sc->iothread,NULL))
    fatal("pthread_join: %s",strerror(errno));

  endpoint_forceclose(sc->managerendpt);
  if (0 > wrap_pthread_join(sc->managerthread,NULL))
    fatal("pthread_join: %s",strerror(errno));

  while (NULL != (endpt = sc->endpoints.first)) {
    if (TASK_ENDPOINT == endpt->type) {
      task *tsk = (task*)endpt->data;
      remove_endpoint(sc,endpt);
      tsk->done = 1;
      if (0 > wrap_pthread_join(tsk->thread,NULL))
        fatal("pthread_join: %s",strerror(errno));
      endpoint_forceclose(endpt);
      task_free(tsk);
    }
    else {
      /* FIXME: handle other endpoint types here... e.g. part-way through distributed
         process creation */
      fatal("Other endpoint type (%d) still active",endpt->type);
    }
  }

  while (sc->connections.first)
    remove_connection(sc,sc->connections.first);

  socketcomm_free(sc);

  close(listenfd);
  return 0;
}
