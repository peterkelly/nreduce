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
//#define SOCKET_DEBUG

#define WELCOME_MESSAGE "Welcome to the nreduce 0.1 debug console. Enter commands below:\n\n> "

extern int ioready;

static socketcomm *socketcomm_new()
{
  return (socketcomm*)calloc(1,sizeof(socketcomm));
}

static void socketcomm_free(socketcomm *sc)
{
  /* FIXME: free message data */
  free(sc);
}

static void doio(task *tsk, int delayms);

static connection *find_connection(task *tsk, int id)
{
  socketcomm *sc = (socketcomm*)tsk->commdata;
  connection *conn;
  assert(0 <= id);
  for (conn = sc->wlist.first; conn; conn = conn->next) {
    if ((conn->ip.s_addr == tsk->idmap[id].nodeip.s_addr) &&
        (conn->port == tsk->idmap[id].nodeport))
      return conn;
  }
  return NULL;
}

static task *find_task(socketcomm *sc, int pid)
{
  task *tsk;
  for (tsk = sc->tasks.first; tsk; tsk = tsk->next)
    if (tsk->pid == pid)
      return tsk;
  return NULL;
}

static void socket_send(task *tsk, int destid, int tag, char *data, int size)
{
  msgheader hdr;
  connection *conn = find_connection(tsk,destid);

  if (NULL == conn)
    fatal("Could not find destination connection %d\n",destid);

  #ifdef MSG_DEBUG
  msg_print(tsk,destid,tag,data,size);
  #endif

  #ifdef SOCKET_DEBUG
  printf("socket_send: tag = %s, dest = %d, size = %d, my pid = %d\n",
         msg_names[tag],destid,size,tsk->pid);
  #endif

  if (destid == tsk->pid) {
    printf("%s sending to %s (that's me!): FIXME: support this case\n",
           conn->hostname,conn->hostname);
    abort();
  }

  memset(&hdr,0,sizeof(msgheader));
  hdr.stid.id = tsk->pid;
  hdr.dtid.id = destid;
  hdr.rsize = size;
  hdr.rtag = tag;

  array_append(conn->sendbuf,&hdr,sizeof(msgheader));
  array_append(conn->sendbuf,data,size);

  doio(tsk,0);
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
    msgheader hdr = *(msgheader*)&conn->recvbuf->data[start];
    message *newmsg;
    task *tsk;

    /* verify header */
    assert(0 <= hdr.rsize);
    assert(0 <= hdr.rtag);
    assert(MSG_COUNT > hdr.rtag);

    if (MSG_HEADER_SIZE+hdr.rsize > conn->recvbuf->nbytes-start)
      break; /* incomplete message */

    if (NULL == (tsk = find_task(sc,hdr.dtid.id)))
      fatal("Message received for unknown task %d\n",hdr.dtid.id);

    /* complete message present; add it to the mailbox */
    newmsg = (message*)calloc(1,sizeof(message));
    newmsg->hdr = hdr;
    newmsg->rawsize = hdr.rsize;
    newmsg->rawdata = (char*)malloc(hdr.rsize);
    memcpy(newmsg->rawdata,&conn->recvbuf->data[start+MSG_HEADER_SIZE],hdr.rsize);
    llist_append(&tsk->mailbox,newmsg);

    start += MSG_HEADER_SIZE+hdr.rsize;
  }

  /* remove the data we've consumed from the receive buffer */
  array_remove_data(conn->recvbuf,start);
}

static int receive_incoming(task *tsk, int *tag, char **data, int *size)
{
  message *msg = tsk->mailbox.first;
  int from;
  if (NULL == msg)
    return -1;

  assert(msg->hdr.rsize == msg->rawsize);
  #ifdef SOCKET_DEBUG
  printf("have completed message from worker %d, rtag = %s, rsize = %d\n",
         msg->hdr.stid.id,msg_names[msg->hdr.rtag],msg->hdr.rsize);
  #endif
  *tag = msg->hdr.rtag;
  *data = msg->rawdata;
  *size = msg->hdr.rsize;
  from = msg->hdr.stid.id;
  assert(tsk->pid == msg->hdr.dtid.id);
  llist_remove(&tsk->mailbox,msg);
  free(msg);
  return from;
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
  if (0 > fdsetasync(sock,1))
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

static int rselect(int n, fd_set *readfds, fd_set *writefds, fd_set  *exceptfds, int delayms)
{
  if (0 > delayms) {
    while (1) {
      int s = select(n,readfds,writefds,exceptfds,NULL);
      if ((0 <= s) || (EINTR != errno))
        return s;
    }
  }
  else {
    struct timeval timeout;
    timeout.tv_sec = delayms/1000;
    timeout.tv_usec = (delayms%1000)*1000;
    while (1) {
      int s = select(n,readfds,writefds,exceptfds,&timeout);
      if ((0 <= s) || (EINTR != errno))
        return s;
    }
  }
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

  #ifdef SOCKET_DEBUG
  printf("Received %d bytes from %s\n",r,conn->hostname);
  #endif

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

  if ((0 > fdsetblocking(clientfd,0)) || (0 > fdsetasync(clientfd,1))) {
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

static void doio(task *tsk, int delayms)
{
  socketcomm *sc = (socketcomm*)tsk->commdata;
  int highest = -1;
  int s;
  fd_set readfds;
  fd_set writefds;
  int want2write = 0;
  connection *conn;
  connection *next;

  FD_ZERO(&readfds);
  FD_ZERO(&writefds);

  FD_SET(sc->listenfd,&readfds);
  if (highest < sc->listenfd)
    highest = sc->listenfd;

  for (conn = sc->wlist.first; conn; conn = conn->next) {
    assert(0 <= conn->sock);
    if (highest < conn->sock)
      highest = conn->sock;
    FD_SET(conn->sock,&readfds);

    if ((0 < conn->sendbuf->nbytes) || !conn->connected) {
      FD_SET(conn->sock,&writefds);
      want2write = 1;
    }
  }

  s = rselect(highest+1,&readfds,&writefds,NULL,delayms);

  if (0 > s)
    fatal("select: %s",strerror(errno));

  #ifdef SOCKET_DEBUG
  printf("select() returned %d\n",s);
  #endif

  if (want2write)
    ioready = 1;

  if (0 == s)
    return; /* timed out; no sockets are ready for reading or writing */

  /* Set the I/O ready flag again, because there may be more after what we read here */
  ioready = 1;

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

static int socket_recv(task *tsk, int *tag, char **data, int *size, int block, int delayms)
{
  int from;

  #ifdef SOCKET_DEBUG
  printf("socket_recv: block = %d, delayms = %d\n",block,delayms);
  #endif

  if (0 <= (from = receive_incoming(tsk,tag,data,size)))
    return from;

  /* Clear the I/O ready flag. If we got a SIGIO flag just before this it doesn't matter,
     because we're about to do a select() anyway. If a SIGIO comes in after we've cleared
     the flag it will just be set again. */
  ioready = 0;

  if (block)
    doio(tsk,-1);
  else
    doio(tsk,delayms);

  /* FIXME: there may be outstanding data! shouldn't return just yet if we've been asked to
     block and we don't have a completed message (for this particular task) yet. */

  return receive_incoming(tsk,tag,data,size);
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

static void sigio(int sig)
{
  ioready = 1;
}

static void exited()
{
  char *str = "exited\n";
  write(STDOUT_FILENO,str,strlen(str));
}

static void startlog(int logfd)
{
  int i;
  struct sigaction action;

  setbuf(stdout,NULL);
  setbuf(stderr,NULL);

  close(STDOUT_FILENO);
  close(STDERR_FILENO);

  dup2(logfd,STDOUT_FILENO);
  dup2(logfd,STDERR_FILENO);

  signal(SIGABRT,sigabrt);
  signal(SIGSEGV,sigsegv);


/*   signal(SIGIO,sigio); */
  sigaction(SIGIO,NULL,&action);
  action.sa_handler = sigio;
  action.sa_flags = SA_RESTART;
  sigaction(SIGIO,&action,NULL);


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

static int connect_workers(task *tsk, int want, int listenfd, int myindex)
{
  socketcomm *sc = (socketcomm*)tsk->commdata;
  int have;
  connection *conn;
  do {
    have = 0;
    doio(tsk,-1);
    for (conn = sc->wlist.first; conn; conn = conn->next)
      if (conn->connected && (0 <= conn->port))
        have++;
  } while (have < want);

  printf("All connections now established\n");
  return 0;
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

static void init_idmap(task *tsk, array *hostnames)
{
  int i;
  assert(tsk->groupsize == array_count(hostnames));
  tsk->idmap = (taskid*)calloc(tsk->groupsize,sizeof(taskid));
  for (i = 0; i < tsk->groupsize; i++) {
    struct hostent *he;
    char *hostname = array_item(hostnames,i,char*);

    if (NULL == (he = gethostbyname(hostname)))
      fatal("%s: %s\n",hostname,hstrerror(h_errno));

    if (4 != he->h_length)
      fatal("%s: invalid address type\n",hostname);

    tsk->idmap[i].nodeip = *((struct in_addr*)he->h_addr);
    tsk->idmap[i].nodeport = WORKER_PORT;
    tsk->idmap[i].id = i; /* FIXME: this won't be the case in general */
  }
}

int worker(const char *hostsfile, const char *masteraddr)
{
  char *mhost;
  int mport;
  int msock;
  struct in_addr addr;
  int i;
  int myindex;
  task *tsk;
  array *bcarr = NULL;
  socketcomm *sc;
  int logfd;
  char notify = '1';
  connection *conn;
  array *hostnames;
  int nworkers;
  int listenfd;
  int remaining;

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
  nworkers = array_count(hostnames);
  remaining = nworkers-1;

  /* connect to workers > myindex, accept connections from those < myindex */
  for (i = myindex+1; i < nworkers; i++) {
    if (NULL == (conn = initiate_connection(sc,array_item(hostnames,i,char*))))
      return -1;
    if (conn->connected)
      remaining--;
  }

  tsk = task_new();
  tsk->pid = myindex;
  tsk->groupsize = nworkers;
  tsk->bcdata = bcarr->data;
  tsk->bcsize = bcarr->nbytes;
  tsk->commdata = sc;
  tsk->sendf = socket_send;
  tsk->recvf = socket_recv;
  task_init(tsk);
  tsk->output = stdout;

  init_idmap(tsk,hostnames);

  llist_append(&sc->tasks,tsk);

  if (0 > connect_workers(tsk,remaining,listenfd,myindex))
    return -1;

  printf("Hosts:\n");
  for (conn = sc->wlist.first; conn; conn = conn->next)
    printf("%s, sock %d\n",conn->hostname,conn->sock);

  if (0 == myindex) {
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

  printf("before execute()\n");
  execute(tsk);

  printf("execute() returned; sleeping for 60 seconds\n");
  sleep(60);

  task_free(tsk);
  socketcomm_free(sc);

  if (bcarr)
    array_free(bcarr);

  free(mhost);
  close(listenfd);
  return 0;
}
