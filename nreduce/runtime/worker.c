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

#define CHUNKSIZE 1024
#define MSG_HEADER_SIZE (2*sizeof(int))
//#define SOCKET_DEBUG

extern int *ioreadyptr;

typedef struct socketmsg {
  char *rawdata;
  int rawsize;
  int sent;

  int rsize;
  int rtag;

  struct socketmsg *next;
  struct socketmsg *prev;
} socketmsg;

typedef struct socketmsglist {
  socketmsg *first;
  socketmsg *last;
} socketmsglist;

typedef struct socketcomm {
  nodeinfo *ni;
  socketmsglist *outgoing;
  socketmsglist *incoming;
  array **recvbuf;
} socketcomm;

static socketcomm *socketcomm_new(nodeinfo *ni)
{
  socketcomm *sc = (socketcomm*)calloc(1,sizeof(socketcomm));
  int i;
  sc->ni = ni;
  sc->outgoing = (socketmsglist*)calloc(ni->nworkers,sizeof(socketmsglist));
  sc->incoming = (socketmsglist*)calloc(ni->nworkers,sizeof(socketmsglist));
  sc->recvbuf = (array**)calloc(ni->nworkers,sizeof(array*));
  for (i = 0; i < ni->nworkers; i++)
    sc->recvbuf[i] = array_new(1);
  return sc;
}

static void socketcomm_free(socketcomm *sc)
{
  /* FIXME: free message data */
  free(sc);
}

static int send_outstanding(socketcomm *sc, int dest)
{
  while (1) {
    socketmsg *sm = sc->outgoing[dest].first;
    int w;
    int tosend;
    if (NULL == sm)
      return 0;
    tosend = sm->rawsize - sm->sent;
    if (tosend > CHUNKSIZE)
      tosend = CHUNKSIZE;
    w = TEMP_FAILURE_RETRY(write(sc->ni->workers[dest].sock,&sm->rawdata[sm->sent],tosend));
    if (0 > w) {
      if (EAGAIN == errno) {
        #ifdef SOCKET_DEBUG
        fprintf(stderr,"send() to %s: try again\n",sc->ni->workers[dest].hostname);
        #endif
        return 0;
      }
      fprintf(stderr,"Error sending to %s: %s\n",sc->ni->workers[dest].hostname,strerror(errno));
      return -1;
    }
    #ifdef SOCKET_DEBUG
    printf("Sent %d bytes to worker %d\n",w,dest);
    #endif
    sm->sent += w;
    assert(sm->sent <= sm->rawsize);
    if (sm->sent == sm->rawsize) {
      llist_remove(&sc->outgoing[dest],sm);
      free(sm->rawdata);
      free(sm);
    }
  }
  abort(); /* should never get here */
}

static void socket_send(task *tsk, int dest, int tag, char *data, int size)
{
  socketcomm *sc;
  socketmsg *newmsg;
  sc = (socketcomm*)tsk->commdata;

  #ifdef MSG_DEBUG
  msg_print(tsk,dest,tag,data,size);
  #endif

  #ifdef SOCKET_DEBUG
  printf("socket_send: tag = %s, dest = %d, size = %d, my pid = %d\n",
         msg_names[tag],dest,size,tsk->pid);
  #endif

  if (dest == tsk->pid) {
    printf("%s sending to %s (that's me!): FIXME: support this case\n",
           sc->ni->workers[dest].hostname,sc->ni->workers[dest].hostname);
    abort();
  }

  newmsg = (socketmsg*)calloc(1,sizeof(socketmsg));
  newmsg->rawsize = MSG_HEADER_SIZE+size;
  newmsg->rawdata = (char*)malloc(newmsg->rawsize);
  memcpy(&newmsg->rawdata[0],&size,sizeof(int));
  memcpy(&newmsg->rawdata[sizeof(int)],&tag,sizeof(int));
  memcpy(&newmsg->rawdata[MSG_HEADER_SIZE],data,size);

  assert(0 <= dest);
  assert(dest < sc->ni->nworkers);

  llist_append(&sc->outgoing[dest],newmsg);

  if (0 != send_outstanding(sc,dest))
    abort();
}

static void task_received(task *tsk, int worker)
{
  socketcomm *sc = (socketcomm*)tsk->commdata;
  int start = 0;

  /* inspect the next section of the input buffer to see if it contains a complete message */
  while (MSG_HEADER_SIZE <= sc->recvbuf[worker]->nbytes-start) {
    int rsize = *(int*)&sc->recvbuf[worker]->data[start];
    int rtag = *(int*)&sc->recvbuf[worker]->data[start+sizeof(int)];
    socketmsg *newmsg;

    /* verify header */
    assert(0 <= rsize);
    assert(0 <= rtag);
    assert(MSG_COUNT > rtag);

    if (MSG_HEADER_SIZE+rsize > sc->recvbuf[worker]->nbytes-start)
      break; /* incomplete message */

    /* complete message present; add it to the mailbox */
    newmsg = (socketmsg*)calloc(1,sizeof(socketmsg));
    newmsg->rsize = rsize;
    newmsg->rtag = rtag;
    newmsg->rawsize = rsize;
    newmsg->rawdata = (char*)malloc(rsize);
    memcpy(newmsg->rawdata,&sc->recvbuf[worker]->data[start+MSG_HEADER_SIZE],rsize);
    llist_append(&sc->incoming[worker],newmsg);

    start += MSG_HEADER_SIZE+rsize;
  }

  /* remove the data we've consumed from the receive buffer */
  array_remove_data(sc->recvbuf[worker],start);
}

static int receive_incoming(task *tsk, int i, int *tag, char **data, int *size)
{
  socketcomm *sc = (socketcomm*)tsk->commdata;
  socketmsg *msg = sc->incoming[i].first;
  if ((NULL != msg) && (msg->rsize == msg->rawsize)) {
    #ifdef SOCKET_DEBUG
    printf("have completed message from %s, rtag = %s, rsize = %d\n",
           sc->ni->workers[i].hostname,msg_names[msg->rtag],msg->rsize);
    #endif
    *tag = msg->rtag;
    *data = msg->rawdata;
    *size = msg->rsize;
    llist_remove(&sc->incoming[i],msg);
    free(msg);
    return i;
  }
  else if (NULL != msg) {
    #ifdef SOCKET_DEBUG
    printf("%s: msg->rsize = %d, msg->rawsize = %d\n",
           sc->ni->workers[i].hostname,msg->rsize,msg->rawsize);
    #endif
  }
  return -1;
}

int rselect(int n, fd_set *readfds, fd_set *writefds, fd_set  *exceptfds,  struct timeval *timeout)
{
  while (1) {
    int s = select(n,readfds,writefds,exceptfds,timeout);
    if ((0 <= s) || (EINTR != errno))
      return s;
  }
}

static int wait_for_data(task *tsk, int block, int delayms, fd_set *rfds)
{
  socketcomm *sc = (socketcomm*)tsk->commdata;
  int i;
  int highest = -1;
  int s;

  FD_ZERO(rfds);

  for (i = 0; i < sc->ni->nworkers; i++) {
    if (0 > sc->ni->workers[i].sock)
      continue;
    if (highest < sc->ni->workers[i].sock)
      highest = sc->ni->workers[i].sock;
    FD_SET(sc->ni->workers[i].sock,rfds);
  }

  if (block) {
    printf("Blocking select()...");
    s = rselect(highest+1,rfds,NULL,NULL,NULL);
    printf("done\n");
  }
  else {
    struct timeval timeout;
    timeout.tv_sec = delayms/1000;
    timeout.tv_usec = (delayms%1000)*1000;
    s = rselect(highest+1,rfds,NULL,NULL,&timeout);
  }

  return s;
}

static int receive_data(task *tsk, fd_set *rfds, int *tag, char **data, int *size)
{
  socketcomm *sc = (socketcomm*)tsk->commdata;
  int i;
  for (i = 0; i < sc->ni->nworkers; i++) {
    char *hostname = sc->ni->workers[i].hostname;
    int from;

    if ((0 > sc->ni->workers[i].sock) || !FD_ISSET(sc->ni->workers[i].sock,rfds))
      continue;

    while (1) {
      int r;

      array_mkroom(sc->recvbuf[i],CHUNKSIZE);
      r = TEMP_FAILURE_RETRY(read(sc->ni->workers[i].sock,
                                  &sc->recvbuf[i]->data[sc->recvbuf[i]->nbytes],
                                  CHUNKSIZE));

      if ((0 > r) && (EAGAIN == errno))
        break;

      if (0 > r)
        fatal("read() from %s failed: %s\n",hostname,strerror(errno));

      if (0 == r)
        fatal("read() from %s failed: channel closed (maybe it crashed?)\n",hostname);

      #ifdef SOCKET_DEBUG
      printf("Received %d bytes from %s\n",r,sc->ni->workers[i].hostname);
      #endif

      sc->recvbuf[i]->nbytes += r;
      task_received(tsk,i);

      if (0 <= (from = receive_incoming(tsk,i,tag,data,size)))
        return from;
    }
  }
  return -1;
}

static int socket_recv2(task *tsk, int *tag, char **data, int *size, int block, int delayms)
{
  socketcomm *sc = (socketcomm*)tsk->commdata;
  int i;
  fd_set rfds;
  int nready;

  #ifdef SOCKET_DEBUG
  printf("socket_recv: block = %d, delayms = %d\n",block,delayms);
  #endif

  /* first see if there's any complete messages in the queue */
  for (i = 0; i < sc->ni->nworkers; i++) {
    int from;
    if (0 <= (from = receive_incoming(tsk,i,tag,data,size)))
      return from;
  }

  /* Clear the I/O ready flag. If we got a SIGIO flag just before this it doesn't matter,
     because we're about to do a select() anyway. If a SIGIO comes in after we've cleared
     the flag it will just be set again. */
  *tsk->ioreadyptr = 0;

  /* now try to read some more messages from the network */
  nready = wait_for_data(tsk,block,delayms,&rfds);

  if (0 > nready) {
    perror("select");
    abort();
  }

  #ifdef SOCKET_DEBUG
  printf("select() returned %d\n",nready);
  #endif

  if (0 == nready)
    return -1;

  /* Set the I/O ready flag again, because receive_data() may not consume all of the data that is
     available to be read - it only reads the minimum necessary to get the next message */
  *tsk->ioreadyptr = 1;

  return receive_data(tsk,&rfds,tag,data,size);
}

static int socket_recv(task *tsk, int *tag, char **data, int *size, int block, int delayms)
{
  return socket_recv2(tsk,tag,data,size,block,delayms);
}

void sigabrt(int sig)
{
  char *str = "abort()\n";
  write(STDERR_FILENO,str,strlen(str));
}

void *btarray[1024];

void sigsegv(int sig)
{
  char *str = "Segmentation fault\n";
  int size;
  write(STDERR_FILENO,str,strlen(str));

  size = backtrace(btarray,1024);
  backtrace_symbols_fd(btarray,size,STDERR_FILENO);

  signal(sig,SIG_DFL);
  kill(getpid(),sig);
}

void sigio(int sig)
{
/*   char *str = "SIGIO\n"; */
/*   write(STDOUT_FILENO,str,strlen(str)); */
  if (ioreadyptr)
    *ioreadyptr = 1;
}

void exited()
{
  char *str = "exited\n";
  write(STDOUT_FILENO,str,strlen(str));
}

void startlog(int logfd)
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

int wait_for_startup_notification(int msock)
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

int connect_workers(nodeinfo *ni, int myindex)
{
  /* connect to workers > myindex, accept connections from those < myindex */
  int i;
  int remaining = ni->nworkers-1;

  for (i = myindex+1; i < ni->nworkers; i++) {
    struct sockaddr_in addr;

    if (0 > (ni->workers[i].sock = socket(AF_INET,SOCK_STREAM,0))) {
      perror("socket");
      return -1;
    }

    if (0 > fdsetblocking(ni->workers[i].sock,0))
      return -1;
    if (0 > fdsetasync(ni->workers[i].sock,1))
      return -1;

    addr.sin_family = AF_INET;
    addr.sin_port = htons(WORKER_PORT);
    addr.sin_addr = ni->workers[i].ip;
    memset(&addr.sin_zero,0,8);

    if (0 == connect(ni->workers[i].sock,(struct sockaddr*)&addr,sizeof(struct sockaddr))) {
      ni->workers[i].connected = 1;
      printf("Connected (immediately) to %s\n",ni->workers[i].hostname);
      remaining--;
    }
    else if (EINPROGRESS != errno) {
      perror("connect");
      return -1;
    }
  }

  while (0 < remaining) {
    fd_set readfds;
    fd_set writefds;
    int highest = ni->listenfd;

    FD_ZERO(&readfds);
    FD_ZERO(&writefds);

    for (i = 0; i < myindex; i++)
      if (!ni->workers[i].connected)
        FD_SET(ni->listenfd,&readfds);

    for (i = myindex+1; i < ni->nworkers; i++) {
      if (!ni->workers[i].connected) {
        FD_SET(ni->workers[i].sock,&writefds);
        if (highest < ni->workers[i].sock)
          highest = ni->workers[i].sock;
      }
    }

    if (0 > TEMP_FAILURE_RETRY(select(highest+1,&readfds,&writefds,NULL,NULL))) {
      perror("select");
      return -1;
    }

    if (FD_ISSET(ni->listenfd,&readfds)) {
      struct sockaddr_in remote_addr;
      int sin_size = sizeof(struct sockaddr_in);
      int clientfd;
      if (0 > (clientfd = accept(ni->listenfd,(struct sockaddr*)&remote_addr,&sin_size))) {
        perror("accept");
        return -1;
      }

      if (0 > fdsetasync(clientfd,1))
        return -1;

      for (i = 0; i < ni->nworkers; i++) {
        if (!memcmp(&ni->workers[i].ip,&remote_addr.sin_addr,sizeof(struct in_addr))) {
          int yes = 1;
          ni->workers[i].sock = clientfd;
          ni->workers[i].connected = 1;
          printf("Got connection from %s\n",ni->workers[i].hostname);

          if (0 > setsockopt(ni->workers[i].sock,SOL_TCP,TCP_NODELAY,&yes,sizeof(int))) {
            perror("setsockopt TCP_NODELAY");
            return -1;
          }

          remaining--;
        }
      }
    }

    for (i = myindex+1; i < ni->nworkers; i++) {
      if (!ni->workers[i].connected && FD_ISSET(ni->workers[i].sock,&writefds)) {
        int err;
        int optlen = sizeof(int);

        if (0 > getsockopt(ni->workers[i].sock,SOL_SOCKET,SO_ERROR,&err,&optlen)) {
          perror("getsockopt");
          return -1;
        }

        if (0 == err) {
          int yes = 1;
          ni->workers[i].connected = 1;
          printf("Connected to %s\n",ni->workers[i].hostname);

          if (0 > setsockopt(ni->workers[i].sock,SOL_TCP,TCP_NODELAY,&yes,sizeof(int))) {
            perror("setsockopt TCP_NODELAY");
            return -1;
          }

          remaining--;
        }
        else {
          printf("Connection to %s failed: %s\n",ni->workers[i].hostname,strerror(err));
          return -1;
        }
      }
    }

  }
  printf("All connections now established\n");

  return 0;
}

#define CHUNKSIZE 1024
array *read_data(int fd)
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
  nodeinfo *ni;
  int i;
  int myindex;
  task *tsk;
  array *bcarr = NULL;
  socketcomm *sc;
  int logfd;
  char notify = '1';

  if (0 > (logfd = open("/tmp/nreduce.log",O_WRONLY|O_APPEND|O_CREAT,0666))) {
    perror("/tmp/nreduce.log");
    exit(1);
  }

  write(STDOUT_FILENO,&notify,1);

  startlog(logfd);

  printf("Running as worker, hostsfile = %s, master = %s, pid = %d\n",
         hostsfile,masteraddr,getpid());

  if (NULL == (ni = nodeinfo_init(hostsfile)))
    return -1;

  if (0 > parse_address(masteraddr,&mhost,&mport)) {
    fprintf(stderr,"Invalid address: %s\n",masteraddr);
    return -1;
  }

  addr.s_addr = INADDR_ANY;
  if (0 > (ni->listenfd = start_listening(addr,WORKER_PORT))) {
    free(mhost);
    return -1;
  }

  if (0 > (msock = connect_host(mhost,mport))) {
    free(mhost);
    close(ni->listenfd);
    fprintf(stderr,"Can't connect to %s: %s\n",masteraddr,strerror(errno));
    return -1;
  }

  if (0 > (myindex = wait_for_startup_notification(msock)))
    return -1;

  printf("Got startup notification; i am worker %d (%s)\n",
         myindex,ni->workers[myindex].hostname);

  if (0 > connect_workers(ni,myindex))
    return -1;

  printf("Hosts:\n");
  for (i = 0; i < ni->nworkers; i++)
    printf("%s, sock %d\n",ni->workers[i].hostname,ni->workers[i].sock);

/*   write(msock,"Here I am",strlen("Here I am")); */

  if (0 > fdsetblocking(msock,1))
    return -1;

  if (NULL == (bcarr = read_data(msock)))
    return -1;

  printf("Got bytecode, size = %d bytes\n",bcarr->nbytes);

  sc = socketcomm_new(ni);

  tsk = task_new();
  tsk->pid = myindex;
  tsk->groupsize = ni->nworkers;
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
  close(ni->listenfd);
  return 0;
}
