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

#define NODE_C

/* FIXME: the sin_addr structure is supposed to be in network byte order; we are treating
   it as host by order here (which appears to be ok on x86) */

/* FIXME: use inet_aton() and inet_ntoa() when converting betwen struct_inaddr and strings */

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
#include "node.h"
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

const char *event_types[EVENT_COUNT] = {
  "CONN_ESTABLISHED",
  "CONN_FAILED",
  "CONN_ACCEPTED",
  "CONN_CLOSED",
  "CONN_IOERROR",
  "HANDSHAKE_DONE",
  "HANDSHAKE_FAILED",
  "DATA",
  "ENDPOINT_ADDITION",
  "ENDPOINT_REMOVAL",
  "SHUTDOWN",
};

/** @name Private functions
 * @{ */

static void dispatch_event(node *n, int event, connection *conn, endpoint *endpt)
{
  node_callback *cb;
  for (cb = n->callbacks.first; cb; cb = cb->next)
    cb->fun(n,cb->data,event,conn,endpt);
  if (conn && conn->l && conn->l->callback)
    conn->l->callback(n,conn->l->data,event,conn,endpt);
}

static void report_error(node *n, const char *format, ...)
{
  va_list ap;
  fprintf(n->logfile,"ERROR: ");
  va_start(ap,format);
  vfprintf(n->logfile,format,ap);
  va_end(ap);
  fprintf(n->logfile,"\n");
}

static void got_message(node *n, const msgheader *hdr, const void *data)
{
  endpoint *endpt;
  message *newmsg;

  if (NULL == (endpt = find_endpoint(n,hdr->destlocalid))) {
    report_error(n,"Message received for unknown endpoint %d",hdr->destlocalid);
    return;
  }

  newmsg = (message*)calloc(1,sizeof(message));
  newmsg->hdr = *hdr;
  newmsg->data = (char*)malloc(hdr->size);
  memcpy(newmsg->data,data,hdr->size);
  endpoint_add_message(endpt,newmsg);
}

static void process_received(node *n, connection *conn)
{
  int start = 0;

  if (conn->isreg) {
    assert(conn->l);
    assert(conn->l->callback);
    conn->l->callback(n,conn->l->data,EVENT_DATA,conn,NULL);
    return;
  }

  if (conn->isconsole) {
    console_process_received(n,conn);
    return;
  }

  if (!conn->donewelcome) {
    if (conn->recvbuf->nbytes < strlen(WELCOME_MESSAGE)) {
      if (strncmp(conn->recvbuf->data,WELCOME_MESSAGE,conn->recvbuf->nbytes)) {
        /* Data sent so far is not part of the welcome message; must be a console client */
        conn->isconsole = 1;
        console_process_received(n,conn);
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
        console_process_received(n,conn);
        return;
      }
      else {
        /* We have the welcome message; it's a peer and we want to exchange ids next */
        conn->donewelcome = 1;
        start += strlen(WELCOME_MESSAGE);
        array_append(conn->sendbuf,&n->mainl->port,sizeof(int));
      }
    }
  }

  if (0 > conn->port) {
    unsigned char *connip = (unsigned char*)&conn->localip.s_addr;

    if (sizeof(int) > conn->recvbuf->nbytes-start) {
      array_remove_data(conn->recvbuf,start);
      return;
    }
    conn->port = *(int*)&conn->recvbuf->data[start];
    start += sizeof(int);

    if (0 > conn->port)
      fatal("Client sent bad listen port: %d",conn->port);

    if (n->havelistenip) {
      if (memcmp(&n->listenip.s_addr,&conn->localip.s_addr,sizeof(struct in_addr))) {
        unsigned char *expect = (unsigned char*)&n->listenip.s_addr;
        fatal("Client is using a different IP to connect to me than what I expect "
              "(%u.%u.%u.%u instead of %u.%u.%u.%u)",
              connip[0],connip[1],connip[2],connip[3],
              expect[0],expect[1],expect[2],expect[3]);
      }
    }
    else {
      unsigned char *ipbytes = (unsigned char*)&conn->localip.s_addr;
      memcpy(&n->listenip.s_addr,&conn->localip.s_addr,sizeof(struct in_addr));
      n->havelistenip = 1;
      node_log(n,LOG_INFO,"Got my listenip: %u.%u.%u.%u",
               ipbytes[0],ipbytes[1],ipbytes[2],ipbytes[3]);
    }
    node_log(n,LOG_INFO,"Node %u.%u.%u.%u:%d connected",
             connip[0],connip[1],connip[2],connip[3],conn->port);
    dispatch_event(n,EVENT_HANDSHAKE_DONE,conn,NULL);
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
    got_message(n,hdr,&conn->recvbuf->data[start+MSG_HEADER_SIZE]);
    start += MSG_HEADER_SIZE+hdr->size;
  }

  /* remove the data we've consumed from the receive buffer */
  array_remove_data(conn->recvbuf,start);
}

static connection *find_connection(node *n, struct in_addr nodeip, unsigned short nodeport)
{
  connection *conn;
  for (conn = n->connections.first; conn; conn = conn->next)
    if ((conn->ip.s_addr == nodeip.s_addr) && (conn->port == nodeport))
      return conn;
  return NULL;
}

static connection *add_connection(node *n, const char *hostname, int sock, listener *l)
{
  connection *conn = (connection*)calloc(1,sizeof(connection));
  conn->hostname = strdup(hostname);
  conn->ip.s_addr = 0;
  conn->sock = sock;
  conn->l = l;
  conn->n = n;
  conn->readfd = -1;
  conn->port = -1;
  conn->recvbuf = array_new(1);
  conn->sendbuf = array_new(1);
  if (l == n->mainl)
    array_append(conn->sendbuf,WELCOME_MESSAGE,strlen(WELCOME_MESSAGE));
  llist_append(&n->connections,conn);
  return conn;
}

static void remove_connection(node *n, connection *conn)
{
  node_log(n,LOG_INFO,"Removing connection %s",conn->hostname);
  close(conn->sock);
  llist_remove(&n->connections,conn);
  free(conn->hostname);
  array_free(conn->recvbuf);
  array_free(conn->sendbuf);
  free(conn);
}

static int handle_connected(node *n, connection *conn)
{
  int err;
  int optlen = sizeof(int);

  if (0 > getsockopt(conn->sock,SOL_SOCKET,SO_ERROR,&err,&optlen)) {
    perror("getsockopt");
    remove_connection(n,conn);
    return -1;
  }

  if (0 == err) {
    int yes = 1;
    conn->connected = 1;
    node_log(n,LOG_INFO,"Connected to %s",conn->hostname);

    if (0 > setsockopt(conn->sock,SOL_TCP,TCP_NODELAY,&yes,sizeof(int))) {
      perror("setsockopt TCP_NODELAY");
      dispatch_event(n,EVENT_CONN_FAILED,conn,NULL);
      remove_connection(n,conn);
      return -1;
    }

    dispatch_event(n,EVENT_CONN_ESTABLISHED,conn,NULL);
  }
  else {
    node_log(n,LOG_WARNING,"Connection to %s failed: %s",conn->hostname,strerror(err));
    dispatch_event(n,EVENT_CONN_FAILED,conn,NULL);
    remove_connection(n,conn);
    return -1;
  }

  return 0;
}

static void handle_write(node *n, connection *conn)
{
  while (0 < conn->sendbuf->nbytes) {
    int w = TEMP_FAILURE_RETRY(write(conn->sock,conn->sendbuf->data,conn->sendbuf->nbytes));
    if ((0 > w) && (EAGAIN == errno))
      break;

    if (0 > w) {
      node_log(n,LOG_WARNING,"write() to %s:%d failed: %s",
               conn->hostname,conn->port,strerror(errno));
      dispatch_event(n,EVENT_CONN_IOERROR,conn,NULL);
      remove_connection(n,conn);
      return;
    }

    if (0 == w) {
      node_log(n,LOG_WARNING,"write() to %s:%d failed: connection closed",
               conn->hostname,conn->port);
      dispatch_event(n,EVENT_CONN_IOERROR,conn,NULL);
      remove_connection(n,conn);
      return;
    }

    array_remove_data(conn->sendbuf,w);
  }

  if ((0 == conn->sendbuf->nbytes) && conn->toclose) {
    dispatch_event(n,EVENT_CONN_CLOSED,conn,NULL);
    remove_connection(n,conn);
  }
}

static void handle_read(node *n, connection *conn)
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
    node_log(n,LOG_WARNING,"read() from %s:%d failed: %s",
             conn->hostname,conn->port,strerror(errno));
    dispatch_event(n,EVENT_CONN_IOERROR,conn,NULL);
    remove_connection(n,conn);
    return;
  }

  if (0 == r) {
    if (conn->isreg || conn->isconsole)
      node_log(n,LOG_INFO,"Connection %s:%d closed by peer",conn->hostname,conn->port);
    else
      node_log(n,LOG_WARNING,"Connection %s:%d closed by peer",conn->hostname,conn->port);
    dispatch_event(n,EVENT_CONN_CLOSED,conn,NULL);
    remove_connection(n,conn);
    return;
  }

  conn->recvbuf->nbytes += r;
  process_received(n,conn);
}

static char *lookup_hostname(struct in_addr addr)
{
  struct hostent *he = gethostbyaddr(&addr,sizeof(struct in_addr),AF_INET);
  if (NULL != he) {
    return strdup(he->h_name);
  }
  else {
    unsigned char *c = (unsigned char*)&addr;
    char *hostname = (char*)malloc(100);
    sprintf(hostname,"%u.%u.%u.%u",c[0],c[1],c[2],c[3]);
    return hostname;
  }
}

static void handle_new_connection(node *n, listener *l)
{
  struct sockaddr_in remote_addr;
  int sin_size = sizeof(struct sockaddr_in);
  struct sockaddr_in local_addr;
  int local_size = sizeof(struct sockaddr_in);
  int clientfd;
  connection *conn;
  int yes = 1;
  unsigned char *lip;
  char *hostname;
  if (0 > (clientfd = accept(l->fd,(struct sockaddr*)&remote_addr,&sin_size))) {
    perror("accept");
    return;
  }

  if (0 > getsockname(clientfd,(struct sockaddr*)&local_addr,&local_size)) {
    perror("getsockname");
    return;
  }
  lip = (unsigned char*)&local_addr.sin_addr;

  if (0 > fdsetblocking(clientfd,0)) {
    close(clientfd);
    return;
  }

  if (0 > setsockopt(clientfd,SOL_TCP,TCP_NODELAY,&yes,sizeof(int))) {
    perror("setsockopt TCP_NODELAY");
    return;
  }

  hostname = lookup_hostname(remote_addr.sin_addr);
  node_log(n,LOG_INFO,"Got connection from %s on local ip %u.%u.%u.%u",
           hostname,lip[0],lip[1],lip[2],lip[3]);
  conn = add_connection(n,hostname,clientfd,l);
  free(hostname);

  conn->connected = 1;
  conn->ip = remote_addr.sin_addr;
  conn->localip = local_addr.sin_addr;

  dispatch_event(n,EVENT_CONN_ACCEPTED,conn,NULL);
}

static void *ioloop(void *arg)
{
  node *n = (node*)arg;

  pthread_mutex_lock(&n->lock);
  while (!n->shutdown) {
    int highest = -1;
    int s;
    fd_set readfds;
    fd_set writefds;
    connection *conn;
    connection *next;
    int havewrite = 0;
    listener *l;

    FD_ZERO(&readfds);
    FD_ZERO(&writefds);

    for (l = n->listeners.first; l; l = l->next) {
      FD_SET(l->fd,&readfds);
      if (highest < l->fd)
        highest = l->fd;
    }

    FD_SET(n->ioready_readfd,&readfds);
    if (highest < n->ioready_readfd)
      highest = n->ioready_readfd;

    for (conn = n->connections.first; conn; conn = conn->next) {
      assert(0 <= conn->sock);
      if (highest < conn->sock)
        highest = conn->sock;
      FD_SET(conn->sock,&readfds);

      if ((0 < conn->sendbuf->nbytes) || !conn->connected || conn->toclose) {
        FD_SET(conn->sock,&writefds);
        havewrite = 1;
      }

      /* Note: it is possible that we did not find any data waiting to be written at this point,
         but some will become avaliable either just after we release the lock, or while the select
         is actually blocked. socket_send() writes a single byte of data to the node's ioready pipe,
         which will cause the select to be woken, and we will look around again to write the
         data. */
    }

    pthread_mutex_unlock(&n->lock);
    s = select(highest+1,&readfds,&writefds,NULL,NULL);
    pthread_mutex_lock(&n->lock);

    if (0 > s)
      fatal("select: %s",strerror(errno));

    pthread_cond_broadcast(&n->cond);

    if (FD_ISSET(n->ioready_readfd,&readfds)) {
      int c;
      if (0 > read(n->ioready_readfd,&c,1))
        fatal("Cann't read from ioready_readfd: %s\n",strerror(errno));
    }

    if (0 == s)
      continue; /* timed out; no sockets are ready for reading or writing */

    /* Do all the writing we can */
    for (conn = n->connections.first; conn; conn = next) {
      next = conn->next;
      if (FD_ISSET(conn->sock,&writefds)) {
        if (conn->connected)
          handle_write(n,conn);
        else
          handle_connected(n,conn);
      }
    }

    /* Read data */
    for (conn = n->connections.first; conn; conn = next) {
      next = conn->next;
      if (FD_ISSET(conn->sock,&readfds))
        handle_read(n,conn);
    }

    /* accept new connections */
    for (l = n->listeners.first; l; l = l->next) {
      if (FD_ISSET(l->fd,&readfds))
        handle_new_connection(n,l);
    }
  }
  dispatch_event(n,EVENT_SHUTDOWN,NULL,NULL);
  pthread_mutex_unlock(&n->lock);
  return NULL;
}

/* @} */


/** @name Public functions
 * @{ */

node *node_new()
{
  node *n = (node*)calloc(1,sizeof(node));
  int pipefds[2];

  pthread_mutex_init(&n->lock,NULL);
  pthread_cond_init(&n->cond,NULL);
  pthread_cond_init(&n->closecond,NULL);
  n->nextlocalid = 1;

  if (0 > pipe(pipefds)) {
    perror("pipe");
    exit(1);
  }
  n->ioready_readfd = pipefds[0];
  n->ioready_writefd = pipefds[1];
  n->logfile = stdout;
  return n;
}

void node_free(node *n)
{
  if (n->mainl)
    node_remove_listener(n,n->mainl);
  /* FIXME: free message data */
  assert(NULL == n->callbacks.first);
  assert(NULL == n->listeners.first);
  pthread_cond_destroy(&n->closecond);
  pthread_mutex_destroy(&n->lock);
  pthread_cond_destroy(&n->cond);
  close(n->ioready_readfd);
  close(n->ioready_writefd);
  free(n);
}

void node_log(node *n, int level, const char *format, ...)
{
  va_list ap;
  if (level < n->loglevel)
    return;
  switch (level) {
  case LOG_INFO:
    fprintf(n->logfile,"INFO    ");
    break;
  case LOG_WARNING:
    fprintf(n->logfile,"WARNING ");
    break;
  case LOG_ERROR:
    fprintf(n->logfile,"ERROR   ");
    break;
  }
  va_start(ap,format);
  vfprintf(n->logfile,format,ap);
  va_end(ap);
  fprintf(n->logfile,"\n");
}

listener *node_listen(node *n, const char *host, int port, node_callbackfun callback, void *data)
{
  struct in_addr addr;
  int fd;
  int actualport = 0;

  addr.s_addr = INADDR_ANY;
  if (NULL != host) {
    struct in_addr any;
    struct hostent *he;
    if (NULL == (he = gethostbyname(host))) {
      node_log(n,LOG_WARNING,"node_listen(%s): %s",host,hstrerror(h_errno));
      return NULL;
    }

    if (4 != he->h_length) {
      node_log(n,LOG_WARNING,"node_listen(%s): invalid address type",host);
      return NULL;
    }

    n->listenip = *((struct in_addr*)he->h_addr);

    any.s_addr = INADDR_ANY;
    if (memcmp(&n->listenip,&any,sizeof(struct in_addr)))
      n->havelistenip = 1;
    addr = *((struct in_addr*)he->h_addr);
  }

  if (0 > (fd = start_listening(addr,port,&actualport))) {
    node_free(n);
    return NULL;
  }
  assert((0 == port) || (actualport == port));

  if (0 == port)
    node_log(n,LOG_INFO,"Listening on port %d",actualport);

  return node_add_listener(n,actualport,fd,callback,data);
}

void node_add_callback(node *n, node_callbackfun fun, void *data)
{
  node_callback *cb = (node_callback*)calloc(1,sizeof(node_callback));
  cb->fun = fun;
  cb->data = data;

  pthread_mutex_lock(&n->lock);
  llist_append(&n->callbacks,cb);
  pthread_mutex_unlock(&n->lock);
}

void node_remove_callback(node *n, node_callbackfun fun, void *data)
{
  node_callback *cb;

  pthread_mutex_lock(&n->lock);
  cb = n->callbacks.first;
  while (cb && ((cb->fun != fun) || (cb->data != data)))
    cb = cb->next;
  assert(cb);
  llist_remove(&n->callbacks,cb);
  pthread_mutex_unlock(&n->lock);

  free(cb);
}

listener *node_add_listener(node *n, int port, int fd, node_callbackfun callback, void *data)
{
  listener *l = (listener*)calloc(1,sizeof(listener));
  l->port = port;
  l->fd = fd;
  l->callback = callback;
  l->data = data;

  pthread_mutex_lock(&n->lock);
  llist_append(&n->listeners,l);
  pthread_mutex_unlock(&n->lock);
  return l;
}

void node_remove_listener(node *n, listener *l)
{
  connection *conn;

  pthread_mutex_lock(&n->lock);

  conn = n->connections.first;
  while (conn) {
    connection *next = conn->next;
    if (conn->l == l)
      remove_connection(n,conn);
    conn = next;
  }

  llist_remove(&n->listeners,l);

  pthread_mutex_unlock(&n->lock);

  close(l->fd);
  free(l);
}

void node_start_iothread(node *n)
{
  if (0 > wrap_pthread_create(&n->iothread,NULL,ioloop,n))
    fatal("pthread_create: %s",strerror(errno));
}

void node_close_endpoints(node *n)
{
  endpoint *endpt;
  pthread_mutex_lock(&n->lock);
  while (NULL != (endpt = n->endpoints.first)) {
    if (TASK_ENDPOINT == endpt->type) {
      task_kill_locked((task*)endpt->data);
    }
    else if (LAUNCHER_ENDPOINT == endpt->type) {
      pthread_mutex_unlock(&n->lock);
      launcher_kill((launcher*)endpt->data);
      pthread_mutex_lock(&n->lock);
    }
    else {
      /* FIXME: handle other endpoint types here... e.g. part-way through distributed
         process creation */
      fatal("Other endpoint type (%d) still active",endpt->type);
    }
  }
  pthread_mutex_unlock(&n->lock);
}

void node_close_connections(node *n)
{
  pthread_mutex_lock(&n->lock);
  while (n->connections.first)
    remove_connection(n,n->connections.first);
  pthread_mutex_unlock(&n->lock);
}

connection *node_connect(node *n, const char *dest, int port)
{
  /* FIXME: add a lock here when modifying ther connection list, but *only* when the console
     is modified to run in a separate thread */
  struct sockaddr_in addr;
  struct hostent *he;
  int sock;
  int connected = 0;
  connection *conn;
  struct sockaddr_in local_addr;
  int local_size = sizeof(struct sockaddr_in);
  unsigned char *lip;
  char *hostname;

  if (NULL == (he = gethostbyname(dest))) {
    node_log(n,LOG_WARNING,"node_connect(%s): %s",dest,hstrerror(h_errno));
    return NULL;
  }

  if (4 != he->h_length) {
    node_log(n,LOG_WARNING,"node_connect(%s): invalid address type",dest);
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
    node_log(n,LOG_INFO,"Connected (immediately) to %s",dest);
  }
  else if (EINPROGRESS != errno) {
    node_log(n,LOG_WARNING,"connect: %s",strerror(errno));
    return NULL;
  }

  if (0 > getsockname(sock,(struct sockaddr*)&local_addr,&local_size)) {
    node_log(n,LOG_WARNING,"getsockname: %s",strerror(errno));
    return NULL;
  }

  lip = (unsigned char*)&local_addr.sin_addr.s_addr;
  node_log(n,LOG_INFO,"node_connect: local ip is %u.%u.%u.%u",lip[0],lip[1],lip[2],lip[3]);

  hostname = lookup_hostname(addr.sin_addr);
  conn = add_connection(n,hostname,sock,n->mainl);
  free(hostname);
  conn->connected = connected;
  conn->ip = *((struct in_addr*)he->h_addr);
  conn->localip = local_addr.sin_addr;

  if (conn->connected)
    dispatch_event(n,EVENT_CONN_ESTABLISHED,conn,NULL);

  return conn;
}

void node_send(node *n, endpoint *endpt, endpointid destendpointid,
               int tag, const void *data, int size)
{
  connection *conn;
  msgheader hdr;
  char c = 0;

  pthread_mutex_lock(&n->lock);

  memset(&hdr,0,sizeof(msgheader));
  hdr.source.localid = endpt->localid;
  hdr.destlocalid = destendpointid.localid;
  hdr.size = size;
  hdr.tag = tag;

  if ((destendpointid.nodeip.s_addr == n->listenip.s_addr) &&
      (destendpointid.nodeport == n->mainl->port)) {
    hdr.source.nodeip = n->listenip;
    hdr.source.nodeport = n->mainl->port;
    got_message(n,&hdr,data);
  }
  else {
    if (NULL == (conn = find_connection(n,destendpointid.nodeip,destendpointid.nodeport))) {
      unsigned char *addrbytes = (unsigned char*)&destendpointid.nodeip.s_addr;
      /* FIXME: handle this gracefully */
      fatal("Could not find destination connection to %u.%u.%u.%u:%d",
            addrbytes[0],addrbytes[1],addrbytes[2],addrbytes[3],destendpointid.nodeport);
    }

    array_append(conn->sendbuf,&hdr,sizeof(msgheader));
    array_append(conn->sendbuf,data,size);
    write(n->ioready_writefd,&c,1);
  }
  pthread_mutex_unlock(&n->lock);
}

/* This works as long as we don't recycle localids... */
void node_waitclose_locked(node *n, int localid)
{
  while (1) {
    int alive = 0;
    endpoint *endpt;
    for (endpt = n->endpoints.first; endpt; endpt = endpt->next)
      if (endpt->localid == localid)
        alive = 1;

    if (!alive)
      break;

    pthread_cond_wait(&n->closecond,&n->lock);
  }
}

void node_shutdown_locked(node *n)
{
  char c = 0;
  n->shutdown = 1;
  write(n->ioready_writefd,&c,1);
}

void connection_printf(connection *conn, const char *format, ...)
{
  char c = 0;
  va_list ap;
  va_start(ap,format);
  pthread_mutex_lock(&conn->n->lock);
  array_vprintf(conn->sendbuf,format,ap);
  write(conn->n->ioready_writefd,&c,1);
  pthread_mutex_unlock(&conn->n->lock);
  va_end(ap);
}

void connection_close(connection *conn)
{
  char c = 0;
  pthread_mutex_lock(&conn->n->lock);
  conn->toclose = 1;
  write(conn->n->ioready_writefd,&c,1);
  pthread_mutex_unlock(&conn->n->lock);
}

/* @} */

/** @name Public functions - endpoints
 * @{ */

endpoint *node_add_endpoint(node *n, int localid, int type, void *data)
{
  endpoint *endpt = (endpoint*)calloc(1,sizeof(endpoint));
  pthread_mutex_init(&endpt->mailbox.lock,NULL);
  pthread_cond_init(&endpt->mailbox.cond,NULL);
  endpt->localid = localid;
  endpt->type = type;
  endpt->data = data;
  endpt->interruptptr = &endpt->tempinterrupt;

  pthread_mutex_lock(&n->lock);
  if (0 == endpt->localid)
    endpt->localid = n->nextlocalid++; /* FIXME: handle wrap-around - allocate an unused # */
  llist_append(&n->endpoints,endpt);
  dispatch_event(n,EVENT_ENDPOINT_ADDITION,NULL,endpt);
  pthread_mutex_unlock(&n->lock);
  return endpt;
}

void node_remove_endpoint(node *n, endpoint *endpt)
{
  pthread_mutex_lock(&n->lock);
  dispatch_event(n,EVENT_ENDPOINT_REMOVAL,NULL,endpt);
  llist_remove(&n->endpoints,endpt);
  pthread_cond_broadcast(&n->closecond);
  pthread_mutex_unlock(&n->lock);

  pthread_mutex_destroy(&endpt->mailbox.lock);
  pthread_cond_destroy(&endpt->mailbox.cond);
  free(endpt);
}

void endpoint_forceclose(endpoint *endpt)
{
  pthread_mutex_lock(&endpt->mailbox.lock);
  endpt->closed = 1;
  while (endpt->mailbox.first) {
    message *msg = endpt->mailbox.first;
    llist_remove(&endpt->mailbox,msg);
    free(msg->data);
    free(msg);
  }
  pthread_cond_broadcast(&endpt->mailbox.cond);
  pthread_mutex_unlock(&endpt->mailbox.lock);
}

void endpoint_add_message(endpoint *endpt, message *msg)
{
  pthread_mutex_lock(&endpt->mailbox.lock);
  if (!endpt->closed) {
    llist_append(&endpt->mailbox,msg);
    endpt->checkmsg = 1;
    if (endpt->interruptptr)
      *endpt->interruptptr = 1;
    pthread_cond_broadcast(&endpt->mailbox.cond);
  }
  pthread_mutex_unlock(&endpt->mailbox.lock);
}

message *endpoint_next_message(endpoint *endpt, int delayms)
{
  message *msg;
  pthread_mutex_lock(&endpt->mailbox.lock);

  if ((NULL == endpt->mailbox.first) && (0 != delayms) && !endpt->closed) {
    if (0 > delayms) {
      pthread_cond_wait(&endpt->mailbox.cond,&endpt->mailbox.lock);
    }
    else {
      struct timeval now;
      struct timespec abstime;
      gettimeofday(&now,NULL);
      now = timeval_addms(now,delayms);
      abstime.tv_sec = now.tv_sec;
      abstime.tv_nsec = now.tv_usec * 1000;
      pthread_cond_timedwait(&endpt->mailbox.cond,&endpt->mailbox.lock,&abstime);
    }
  }

  msg = endpt->mailbox.first;
  if (NULL != msg)
    llist_remove(&endpt->mailbox,msg);
  endpt->checkmsg = (NULL != endpt->mailbox.first);
  pthread_mutex_unlock(&endpt->mailbox.lock);
  return msg;
}

/* @} */

/** @name Public functions - other
 * @{ */

void message_free(message *msg)
{
  free(msg->data);
  free(msg);
}

/* @} */
