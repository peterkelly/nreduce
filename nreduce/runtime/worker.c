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
#include <fcntl.h>
#include <unistd.h>
#include <signal.h>
#include <execinfo.h>

typedef struct taskid {
  int nodeip;
  short nodeport;
  short id;
} taskid;

#define CHUNKSIZE 1024
#define MSG_HEADER_SIZE sizeof(msgheader)
//#define SOCKET_DEBUG

extern int *ioreadyptr;

typedef struct msgheader {
  taskid stid;
  taskid dtid;
  int rsize;
  int rtag;
} msgheader;

typedef struct socketmsg {
  char *rawdata;
  int rawsize;
  int sent;

  msgheader hdr;

  struct socketmsg *next;
  struct socketmsg *prev;
} socketmsg;

typedef struct socketmsglist {
  socketmsg *first;
  socketmsg *last;
} socketmsglist;

typedef struct connection {
  int sock;
  char *hostname;
  struct in_addr ip;
  int port;
  array *sendbuf;
  array *recvbuf;
  struct connection *prev;
  struct connection *next;
} connection;

typedef struct connectionlist {
  connection *first;
  connection *last;
} connectionlist;

typedef struct workerlist {
  workerinfo *first;
  workerinfo *last;
} workerlist;

typedef struct socketcomm {
  socketmsglist incoming;
  connectionlist connections;
  workerlist wlist;
} socketcomm;

static socketcomm *socketcomm_new()
{
  return (socketcomm*)calloc(1,sizeof(socketcomm));
}

static void socketcomm_free(socketcomm *sc)
{
  /* FIXME: free message data */
  free(sc);
}

static void doio(task *tsk, int block, int delayms);

static workerinfo *find_worker(task *tsk, int id)
{
  socketcomm *sc = (socketcomm*)tsk->commdata;
  workerinfo *wrk;
  for (wrk = sc->wlist.first; wrk; wrk = wrk->next)
    if (wrk->id == id)
      return wrk;
  return NULL;
}

static void socket_send(task *tsk, int destid, int tag, char *data, int size)
{
  msgheader hdr;
  workerinfo *wrk = find_worker(tsk,destid);

  if (NULL == wrk)
    fatal("Could not find destination worker %d\n",destid);

  #ifdef MSG_DEBUG
  msg_print(tsk,wrk->id,tag,data,size);
  #endif

  #ifdef SOCKET_DEBUG
  printf("socket_send: tag = %s, dest = %d, size = %d, my pid = %d\n",
         msg_names[tag],wrk->id,size,tsk->pid);
  #endif

  if (wrk->id == tsk->pid) {
    printf("%s sending to %s (that's me!): FIXME: support this case\n",
           wrk->hostname,wrk->hostname);
    abort();
  }

  memset(&hdr,0,sizeof(msgheader));
  hdr.stid.id = tsk->pid;
  hdr.dtid.id = wrk->id;
  hdr.rsize = size;
  hdr.rtag = tag;

  array_append(wrk->sendbuf,&hdr,sizeof(msgheader));
  array_append(wrk->sendbuf,data,size);

  doio(tsk,0,0);
}

static void process_received(task *tsk, workerinfo *wkr)
{
  socketcomm *sc = (socketcomm*)tsk->commdata;
  int start = 0;

  /* inspect the next section of the input buffer to see if it contains a complete message */
  while (MSG_HEADER_SIZE <= wkr->recvbuf->nbytes-start) {
    msgheader hdr = *(msgheader*)&wkr->recvbuf->data[start];
    socketmsg *newmsg;

    /* verify header */

    assert(0 <= hdr.rsize);
    assert(0 <= hdr.rtag);
    assert(MSG_COUNT > hdr.rtag);
    assert(wkr->id == hdr.stid.id);
    assert(tsk->pid == hdr.dtid.id);

    if (MSG_HEADER_SIZE+hdr.rsize > wkr->recvbuf->nbytes-start)
      break; /* incomplete message */

    /* complete message present; add it to the mailbox */
    newmsg = (socketmsg*)calloc(1,sizeof(socketmsg));
    newmsg->hdr = hdr;
    newmsg->rawsize = hdr.rsize;
    newmsg->rawdata = (char*)malloc(hdr.rsize);
    memcpy(newmsg->rawdata,&wkr->recvbuf->data[start+MSG_HEADER_SIZE],hdr.rsize);
    llist_append(&sc->incoming,newmsg);

    start += MSG_HEADER_SIZE+hdr.rsize;
  }

  /* remove the data we've consumed from the receive buffer */
  array_remove_data(wkr->recvbuf,start);
}

static int receive_incoming(task *tsk, int *tag, char **data, int *size)
{
  socketcomm *sc = (socketcomm*)tsk->commdata;
  socketmsg *msg = sc->incoming.first;
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
  llist_remove(&sc->incoming,msg);
  free(msg);
  return from;
}

static int rselect(int n, fd_set *readfds, fd_set *writefds, fd_set  *exceptfds, 
                   struct timeval *timeout)
{
  while (1) {
    int s = select(n,readfds,writefds,exceptfds,timeout);
    if ((0 <= s) || (EINTR != errno))
      return s;
  }
}

static void doio(task *tsk, int block, int delayms)
{
  socketcomm *sc = (socketcomm*)tsk->commdata;
  int highest = -1;
  int s;
  fd_set readfds;
  fd_set writefds;
  int want2write = 0;
  workerinfo *wkr;

  FD_ZERO(&readfds);
  FD_ZERO(&writefds);

  for (wkr = sc->wlist.first; wkr; wkr = wkr->next) {
    if (0 > wkr->sock)
      continue;
    if (highest < wkr->sock)
      highest = wkr->sock;
    FD_SET(wkr->sock,&readfds);
    if (0 < wkr->sendbuf->nbytes) {
      FD_SET(wkr->sock,&writefds);
      want2write = 1;
    }
  }

  if (block) {
    printf("Blocking select()...");
    s = rselect(highest+1,&readfds,&writefds,NULL,NULL);
    printf("done\n");
  }
  else {
    struct timeval timeout;
    timeout.tv_sec = delayms/1000;
    timeout.tv_usec = (delayms%1000)*1000;
    s = rselect(highest+1,&readfds,&writefds,NULL,&timeout);
  }

  if (0 > s)
    fatal("select: %s",strerror(errno));

  #ifdef SOCKET_DEBUG
  printf("select() returned %d\n",s);
  #endif

  if (want2write)
    *tsk->ioreadyptr = 1;

  if (0 == s)
    return; /* timed out; no sockets are ready for reading or writing */

  /* Set the I/O ready flag again, because there may be more after what we read here */
  *tsk->ioreadyptr = 1;

  /* Do all the writing we can */
  for (wkr = sc->wlist.first; wkr; wkr = wkr->next) {

    if ((0 > wkr->sock) || !FD_ISSET(wkr->sock,&writefds))
      continue;

    while (0 < wkr->sendbuf->nbytes) {
      int w = TEMP_FAILURE_RETRY(write(wkr->sock,
                                       wkr->sendbuf->data,
                                       wkr->sendbuf->nbytes));
      if ((0 > w) && (EAGAIN == errno))
        break;

      if (0 > w)
        fatal("write() to %s failed: %s\n",wkr->hostname,strerror(errno));

      if (0 == w)
        fatal("write() to %s failed: channel closed (maybe it crashed?)\n",wkr->hostname);

      array_remove_data(wkr->sendbuf,w);
    }
  }

  /* Read data, but only enough for us to get the next message. This is so that we won't
     be flooded with messages we don't need yet. */
  for (wkr = sc->wlist.first; wkr; wkr = wkr->next) {

    if ((0 > wkr->sock) || !FD_ISSET(wkr->sock,&readfds))
      continue;

    while (1) {
      int r;
      array *buf = wkr->recvbuf;

      array_mkroom(wkr->recvbuf,CHUNKSIZE);
      r = TEMP_FAILURE_RETRY(read(wkr->sock,
                                  &buf->data[buf->nbytes],
                                  CHUNKSIZE));

      if ((0 > r) && (EAGAIN == errno))
        break;

      if (0 > r)
        fatal("read() from %s failed: %s\n",wkr->hostname,strerror(errno));

      if (0 == r)
        fatal("read() from %s failed: channel closed (maybe it crashed?)\n",wkr->hostname);

      #ifdef SOCKET_DEBUG
      printf("Received %d bytes from %s\n",r,wkr->hostname);
      #endif

      wkr->recvbuf->nbytes += r;
      process_received(tsk,wkr);
    }
  }
}

static int socket_recv2(task *tsk, int *tag, char **data, int *size, int block, int delayms)
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
  *tsk->ioreadyptr = 0;

  doio(tsk,block,delayms);

  return receive_incoming(tsk,tag,data,size);
}

static int socket_recv(task *tsk, int *tag, char **data, int *size, int block, int delayms)
{
  return socket_recv2(tsk,tag,data,size,block,delayms);
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
/*   char *str = "SIGIO\n"; */
/*   write(STDOUT_FILENO,str,strlen(str)); */
  if (ioreadyptr)
    *ioreadyptr = 1;
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

static int lookup_hostname(workerinfo *wkr)
{
  struct hostent *he;
  if (NULL == (he = gethostbyname(wkr->hostname))) {
    fprintf(stderr,"%s: %s\n",wkr->hostname,hstrerror(h_errno));
    return -1;
  }

  if (4 != he->h_length) {
    fprintf(stderr,"%s: %s\n",wkr->hostname,hstrerror(h_errno));
    return -1;
  }

  wkr->ip = *((struct in_addr*)he->h_addr_list[0]);
  return 0;
}

static int initiate_connection(workerinfo *wkr)
{
  struct sockaddr_in addr;

  if (0 > lookup_hostname(wkr))
    return -1;

  if (0 > (wkr->sock = socket(AF_INET,SOCK_STREAM,0))) {
    perror("socket");
    return -1;
  }

  if (0 > fdsetblocking(wkr->sock,0))
    return -1;
  if (0 > fdsetasync(wkr->sock,1))
    return -1;

  addr.sin_family = AF_INET;
  addr.sin_port = htons(WORKER_PORT);
  addr.sin_addr = wkr->ip;
  memset(&addr.sin_zero,0,8);

  if (0 == connect(wkr->sock,(struct sockaddr*)&addr,sizeof(struct sockaddr))) {
    wkr->connected = 1;
    printf("Connected (immediately) to %s\n",wkr->hostname);
  }
  else if (EINPROGRESS != errno) {
    perror("connect");
    return -1;
  }

  return 0;
}

static int connect_workers(socketcomm *sc, int nworkers, int listenfd, int myindex)
{
  int remaining = nworkers-1;
  workerinfo *wkr;

  /* connect to workers > myindex, accept connections from those < myindex */
  for (wkr = sc->wlist.first; wkr; wkr = wkr->next) {
    if (wkr->id > myindex) {
      if (0 > initiate_connection(wkr))
        return -1;
      if (wkr->connected) {
        printf("CONNECTED IMMEDIATELY TO %s\n",wkr->hostname);
        remaining--;
      }
    }
  }

  while (0 < remaining) {
    fd_set readfds;
    fd_set writefds;
    int highest = listenfd;

    FD_ZERO(&readfds);
    FD_ZERO(&writefds);

    FD_SET(listenfd,&readfds);

    for (wkr = sc->wlist.first; wkr; wkr = wkr->next) {
      if ((wkr->id > myindex) && !wkr->connected) {
        FD_SET(wkr->sock,&writefds);
        if (highest < wkr->sock)
          highest = wkr->sock;
      }
    }

    if (0 > TEMP_FAILURE_RETRY(select(highest+1,&readfds,&writefds,NULL,NULL))) {
      perror("select");
      return -1;
    }

    if (FD_ISSET(listenfd,&readfds)) {
      struct sockaddr_in remote_addr;
      int sin_size = sizeof(struct sockaddr_in);
      int clientfd;
      if (0 > (clientfd = accept(listenfd,(struct sockaddr*)&remote_addr,&sin_size))) {
        perror("accept");
        return -1;
      }

      if (0 > fdsetblocking(clientfd,0))
        return -1;
      if (0 > fdsetasync(clientfd,1))
        return -1;

      for (wkr = sc->wlist.first; wkr; wkr = wkr->next) {

        if (0 == wkr->ip.s_addr) {
          if (0 > lookup_hostname(wkr))
            return -1;
        }

        if (!memcmp(&wkr->ip,&remote_addr.sin_addr,sizeof(struct in_addr))) {
          int yes = 1;
          wkr->sock = clientfd;
          wkr->connected = 1;
          printf("Got connection from %s\n",wkr->hostname);

          if (0 > setsockopt(wkr->sock,SOL_TCP,TCP_NODELAY,&yes,sizeof(int))) {
            perror("setsockopt TCP_NODELAY");
            return -1;
          }

          remaining--;
        }
      }
    }

    for (wkr = sc->wlist.first; wkr; wkr = wkr->next) {
      if ((wkr->id > myindex) && !wkr->connected && FD_ISSET(wkr->sock,&writefds)) {
        int err;
        int optlen = sizeof(int);

        if (0 > getsockopt(wkr->sock,SOL_SOCKET,SO_ERROR,&err,&optlen)) {
          perror("getsockopt");
          return -1;
        }

        if (0 == err) {
          int yes = 1;
          wkr->connected = 1;
          printf("Connected to %s\n",wkr->hostname);

          if (0 > setsockopt(wkr->sock,SOL_TCP,TCP_NODELAY,&yes,sizeof(int))) {
            perror("setsockopt TCP_NODELAY");
            return -1;
          }

          remaining--;
        }
        else {
          printf("Connection to %s failed: %s\n",wkr->hostname,strerror(err));
          return -1;
        }
      }
    }

  }
  printf("All connections now established\n");

  return 0;
}

#define CHUNKSIZE 1024
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
  task *tsk;
  array *bcarr = NULL;
  socketcomm *sc;
  int logfd;
  char notify = '1';
  workerinfo *wkr;
  array *hostnames;
  int nworkers;
  int listenfd;

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

  sc = socketcomm_new();
  nworkers = array_count(hostnames);
  for (i = 0; i < nworkers; i++) {
    workerinfo *wkr = (workerinfo*)calloc(1,sizeof(workerinfo));
    wkr->hostname = array_item(hostnames,i,char*);
    wkr->ip.s_addr = 0;
    wkr->sock = -1;
    wkr->pid = -1;
    wkr->readfd = -1;
    wkr->id = i;
    wkr->recvbuf = array_new(1);
    wkr->sendbuf = array_new(1);
    llist_append(&sc->wlist,wkr);
  }


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

  if (0 > connect_workers(sc,nworkers,listenfd,myindex))
    return -1;

  printf("Hosts:\n");
  for (wkr = sc->wlist.first; wkr; wkr = wkr->next)
    printf("%s, sock %d\n",wkr->hostname,wkr->sock);

  if (0 > fdsetblocking(msock,1))
    return -1;

  if (NULL == (bcarr = read_data(msock)))
    return -1;

  printf("Got bytecode, size = %d bytes\n",bcarr->nbytes);

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
