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

#define MSG_HEADER_SIZE sizeof(msgheader)
#define LISTEN_BACKLOG 10

const char *log_levels[LOG_COUNT] = {
  "NONE",
  "ERROR",
  "WARNING",
  "INFO",
  "DEBUG1",
  "DEBUG2",
};

const char *event_types[EVENT_COUNT] = {
  "NONE",
  "CONN_ESTABLISHED",
  "CONN_FAILED",
  "CONN_ACCEPTED",
  "CONN_CLOSED",
  "CONN_IOERROR",
  "HANDSHAKE_DONE",
  "HANDSHAKE_FAILED",
  "DATA_READ",
  "DATA_READFINISHED",
  "DATA_WRITTEN",
  "ENDPOINT_ADDITION",
  "ENDPOINT_REMOVAL",
  "SHUTDOWN",
};

/** @name Private functions
 * @{ */

static int set_non_blocking(int fd)
{
  int flags;
  if (0 > (flags = fcntl(fd,F_GETFL))) {
    perror("fcntl(F_GETFL)");
    return -1;
  }

  flags |= O_NONBLOCK;

  if (0 > fcntl(fd,F_SETFL,flags)) {
    perror("fcntl(F_SETFL)");
    return -1;
  }
  return 0;
}

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
    dispatch_event(n,EVENT_DATA_READ,conn,NULL);
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
      }
    }
  }

  if (conn->donewelcome && !conn->donehandshake) {
    unsigned char *connip = (unsigned char*)&conn->localip.s_addr;

    if (0 > conn->port) {
      if (sizeof(int) > conn->recvbuf->nbytes-start) {
        array_remove_data(conn->recvbuf,start);
        return;
      }
      conn->port = *(int*)&conn->recvbuf->data[start];
      start += sizeof(int);

      if (0 > conn->port)
        fatal("Client sent bad listen port: %d",conn->port);
    }

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
    conn->donehandshake = 1;
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
  conn->canread = 1;
  conn->canwrite = 1;
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
    node_log(n,LOG_INFO,"Connected to %s:%d",conn->hostname,conn->port);

    if (0 > setsockopt(conn->sock,SOL_TCP,TCP_NODELAY,&yes,sizeof(int))) {
      perror("setsockopt TCP_NODELAY");
      dispatch_event(n,EVENT_CONN_FAILED,conn,NULL);
      remove_connection(n,conn);
      return -1;
    }

    dispatch_event(n,EVENT_CONN_ESTABLISHED,conn,NULL);
  }
  else {
    node_log(n,LOG_WARNING,"Connection to %s:%d failed: %s",
             conn->hostname,conn->port,strerror(err));
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

  dispatch_event(n,EVENT_DATA_WRITTEN,conn,NULL);

  if ((0 == conn->sendbuf->nbytes) && conn->finwrite)
    done_writing(n,conn);
}

static void handle_read(node *n, connection *conn)
{
  /* Just do one read; there may still be more data pending after this, but we want to also
     give a fair chance for other connections to be read from. We also want to process messages
     as they arrive rather than waiting until can't ready an more, to avoid buffering up large
     numbers of messages */
  int r;
  array *buf = conn->recvbuf;

  array_mkroom(conn->recvbuf,IOSIZE);
  r = TEMP_FAILURE_RETRY(read(conn->sock,
                              &buf->data[buf->nbytes],
                              IOSIZE));

  if (0 > r) {
    node_log(n,LOG_WARNING,"read() from %s:%d failed: %s",
             conn->hostname,conn->port,strerror(errno));
    dispatch_event(n,EVENT_CONN_IOERROR,conn,NULL);
    remove_connection(n,conn);
    return;
  }

  if (0 == r) {
    if (conn->isreg || conn->isconsole)
      node_log(n,LOG_INFO,"read: Connection %s:%d closed by peer",conn->hostname,conn->port);
    else
      node_log(n,LOG_WARNING,"read: Connection %s:%d closed by peer",conn->hostname,conn->port);
    dispatch_event(n,EVENT_DATA_READFINISHED,conn,NULL);
    done_reading(n,conn);
    return;
  }

  conn->recvbuf->nbytes += r;
  conn->totalread += r;
  process_received(n,conn);
}

static char *lookup_hostname(node *n, struct in_addr addr)
{
  struct hostent *he;
  char *res;

  lock_mutex(&n->liblock);
  he = gethostbyaddr(&addr,sizeof(struct in_addr),AF_INET);
  if (NULL != he) {
    res = strdup(he->h_name);
  }
  else {
    unsigned char *c = (unsigned char*)&addr;
    char *hostname = (char*)malloc(100);
    sprintf(hostname,"%u.%u.%u.%u",c[0],c[1],c[2],c[3]);
    res = hostname;
  }
  unlock_mutex(&n->liblock);
  return res;
}

static int lookup_address(node *n, const char *host, struct in_addr *out)
{
  struct hostent *he;
  int r = 0;

  lock_mutex(&n->liblock);
  if (NULL == (he = gethostbyname(host))) {
    node_log(n,LOG_WARNING,"%s: %s",host,hstrerror(h_errno));
    r =  -1;
  }
  else if (4 != he->h_length) {
    node_log(n,LOG_WARNING,"%s: invalid address type",host);
    r = -1;
  }
  else {
    *out = *((struct in_addr*)he->h_addr);
  }
  unlock_mutex(&n->liblock);
  return r;
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

  if (0 > set_non_blocking(clientfd)) {
    close(clientfd);
    return;
  }

  if (0 > setsockopt(clientfd,SOL_TCP,TCP_NODELAY,&yes,sizeof(int))) {
    perror("setsockopt TCP_NODELAY");
    return;
  }

  hostname = lookup_hostname(n,remote_addr.sin_addr);
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

  lock_mutex(&n->lock);
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
      if (!l->dontaccept)
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

      if (conn->canread && !conn->dontread)
        FD_SET(conn->sock,&readfds);

      if (conn->canwrite && ((0 < conn->sendbuf->nbytes) || !conn->connected || conn->finwrite)) {
        FD_SET(conn->sock,&writefds);
        havewrite = 1;
      }

      /* Note: it is possible that we did not find any data waiting to be written at this point,
         but some will become avaliable either just after we release the lock, or while the select
         is actually blocked. socket_send() writes a single byte of data to the node's ioready pipe,
         which will cause the select to be woken, and we will look around again to write the
         data. */
    }

    unlock_mutex(&n->lock);
    s = select(highest+1,&readfds,&writefds,NULL,NULL);
    lock_mutex(&n->lock);

    if (0 > s)
      fatal("select: %s",strerror(errno));

    pthread_cond_broadcast(&n->cond);

    assert(!FD_ISSET(n->ioready_readfd,&readfds) || n->notified);

    if (FD_ISSET(n->ioready_readfd,&readfds) || n->notified) {
      int c;
      if (0 > read(n->ioready_readfd,&c,1))
        fatal("Can't read from ioready_readfd: %s",strerror(errno));
      n->notified = 0;
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
  unlock_mutex(&n->lock);
  return NULL;
}

/* @} */

/** @name Public functions
 * @{ */

node *node_new(int loglevel)
{
  node *n = (node*)calloc(1,sizeof(node));
  int pipefds[2];
  char *logenv;

  init_mutex(&n->lock);
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
  n->loglevel = loglevel;

  if (NULL != (logenv = getenv("LOG_LEVEL"))) {
    int i;
    for (i = 0; i < LOG_COUNT; i++) {
      if (!strcasecmp(logenv,log_levels[i])) {
        n->loglevel = i;
        break;
      }
    }
    if (LOG_COUNT == i) {
      fprintf(stderr,"Invalid log level: %s\n",logenv);
      exit(1);
    }
  }

  init_mutex(&n->liblock);
  return n;
}

void node_free(node *n)
{
  if (n->mainl)
    node_remove_listener(n,n->mainl);
  assert(NULL == n->endpoints.first);
  assert(NULL == n->callbacks.first);
  assert(NULL == n->listeners.first);
  destroy_mutex(&n->liblock);
  pthread_cond_destroy(&n->closecond);
  destroy_mutex(&n->lock);
  pthread_cond_destroy(&n->cond);
  close(n->ioready_readfd);
  close(n->ioready_writefd);
  free(n);
}

void node_log(node *n, int level, const char *format, ...)
{
  va_list ap;
  char *newfmt;
  if (level > n->loglevel)
    return;
  assert(0 <= level);
  assert(LOG_COUNT > level);

  /* Use only a single vfprintf call to avoid interleaving of messages from different threads */
  newfmt = (char*)malloc(8+1+strlen(format)+1+1);
  sprintf(newfmt,"%-8s %s\n",log_levels[level],format);
  va_start(ap,format);
  vfprintf(n->logfile,newfmt,ap);
  va_end(ap);
  free(newfmt);
}

listener *node_listen(node *n, const char *host, int port, node_callbackfun callback, void *data,
                      int dontaccept)
{
  int fd;
  int actualport = 0;
  int yes = 1;
  struct sockaddr_in local_addr;
  struct sockaddr_in new_addr;
  int new_size = sizeof(struct sockaddr_in);
  listener *l;
  struct in_addr any;
  any.s_addr = INADDR_ANY;

  local_addr.sin_family = AF_INET;
  local_addr.sin_port = htons(port);
  local_addr.sin_addr.s_addr = INADDR_ANY;
  memset(&local_addr.sin_zero,0,8);

  if ((NULL != host) && (0 > lookup_address(n,host,&local_addr.sin_addr))) {
    return NULL;
  }

  if (!n->havelistenip && (memcmp(&local_addr.sin_addr,&any,sizeof(struct in_addr)))) {
    n->havelistenip = 1;
    n->listenip = local_addr.sin_addr;
  }

  if (-1 == (fd = socket(AF_INET,SOCK_STREAM,0))) {
    node_log(n,LOG_ERROR,"socket: %s",strerror(errno));
    return NULL;
  }

  if (-1 == setsockopt(fd,SOL_SOCKET,SO_REUSEADDR,&yes,sizeof(int))) {
    node_log(n,LOG_ERROR,"setsockopt: %s",strerror(errno));
    return NULL;
  }

  if (-1 == bind(fd,(struct sockaddr*)&local_addr,sizeof(struct sockaddr))) {
    node_log(n,LOG_ERROR,"bind: %s",strerror(errno));
    return NULL;
  }

  if (0 > getsockname(fd,(struct sockaddr*)&new_addr,&new_size)) {
    node_log(n,LOG_ERROR,"getsockname: %s",strerror(errno));
    return NULL;
  }
  actualport = ntohs(new_addr.sin_port);

  if (-1 == listen(fd,LISTEN_BACKLOG)) {
    node_log(n,LOG_ERROR,"listen: %s",strerror(errno));
    return NULL;
  }

  assert((0 == port) || (actualport == port));

  if (0 == port)
    node_log(n,LOG_INFO,"Listening on port %d",actualport);

  l = (listener*)calloc(1,sizeof(listener));
  l->hostname = host ? strdup(host) : NULL;
  l->port = port;
  l->fd = fd;
  l->callback = callback;
  l->data = data;
  l->dontaccept = dontaccept;

  lock_node(n);
  llist_append(&n->listeners,l);
  node_notify(n);
  unlock_node(n);

  return l;
}

void node_add_callback(node *n, node_callbackfun fun, void *data)
{
  node_callback *cb = (node_callback*)calloc(1,sizeof(node_callback));
  cb->fun = fun;
  cb->data = data;

  lock_node(n);
  llist_append(&n->callbacks,cb);
  unlock_node(n);
}

void node_remove_callback(node *n, node_callbackfun fun, void *data)
{
  node_callback *cb;

  lock_node(n);
  cb = n->callbacks.first;
  while (cb && ((cb->fun != fun) || (cb->data != data)))
    cb = cb->next;
  assert(cb);
  llist_remove(&n->callbacks,cb);
  unlock_node(n);

  free(cb);
}

void node_remove_listener(node *n, listener *l)
{
  connection *conn;

  lock_node(n);

  conn = n->connections.first;
  while (conn) {
    connection *next = conn->next;
    if (conn->l == l) {
      dispatch_event(n,EVENT_CONN_CLOSED,conn,NULL);
      remove_connection(n,conn);
    }
    conn = next;
  }

  llist_remove(&n->listeners,l);

  unlock_node(n);

  free(l->hostname);
  close(l->fd);
  free(l);
}

void node_start_iothread(node *n)
{
  if (0 > pthread_create(&n->iothread,NULL,ioloop,n))
    fatal("pthread_create: %s",strerror(errno));
}

void node_close_endpoints(node *n)
{
  endpoint *endpt;
  lock_mutex(&n->lock);
  while (NULL != (endpt = n->endpoints.first)) {
    if (TASK_ENDPOINT == endpt->type) {
      task_kill_locked((task*)endpt->data);
    }
    else if (LAUNCHER_ENDPOINT == endpt->type) {
      unlock_mutex(&n->lock);
      launcher_kill((launcher*)endpt->data);
      lock_mutex(&n->lock);
    }
    else {
      fatal("Other endpoint type (%d) still active",endpt->type);
    }
  }
  unlock_mutex(&n->lock);
}

void node_close_connections(node *n)
{
  lock_node(n);
  while (n->connections.first) {
    dispatch_event(n,EVENT_CONN_CLOSED,n->connections.first,NULL);
    remove_connection(n,n->connections.first);
  }
  unlock_node(n);
}

#ifdef DEBUG_SHORT_KEEPALIVE
static int set_keepalive(node *n, int sock, int s)
{
  int one = 1;
  if (0 > setsockopt(sock,IPPROTO_TCP,TCP_KEEPIDLE,&s,sizeof(int))) {
    node_log(n,LOG_ERROR,"setsockopt(TCP_KEEPIDLE): %s",strerror(errno));
    return -1;
  }

  if (0 > setsockopt(sock,IPPROTO_TCP,TCP_KEEPINTVL,&s,sizeof(int))) {
    node_log(n,LOG_ERROR,"setsockopt(TCP_KEEPINTVL): %s",strerror(errno));
    return -1;
  }

  if (0 > setsockopt(sock,IPPROTO_TCP,TCP_KEEPCNT,&one,sizeof(int))) {
    node_log(n,LOG_ERROR,"setsockopt(TCP_KEEPCNT): %s",strerror(errno));
    return -1;
  }

  if (0 > setsockopt(sock,SOL_SOCKET,SO_KEEPALIVE,&one,sizeof(int))) {
    node_log(n,LOG_ERROR,"setsockopt(SO_KEEPALIVE): %s",strerror(errno));
    return -1;
  }

  node_log(n,LOG_DEBUG2,"Set connection keepalive interval to %ds",s);

  return 0;
}
#endif

connection *node_connect_locked(node *n, const char *dest, int port, int othernode)
{
  /* FIXME: add a lock here when modifying ther connection list, but *only* when the console
     is modified to run in a separate thread */
  struct sockaddr_in addr;
  int sock;
  int connected = 0;
  connection *conn;
  struct sockaddr_in local_addr;
  int local_size = sizeof(struct sockaddr_in);
  unsigned char *lip;
  char *hostname;

  assert(NODE_ALREADY_LOCKED(n));

  addr.sin_family = AF_INET;
  addr.sin_port = htons(port);
  memset(&addr.sin_zero,0,8);

  if (0 > lookup_address(n,dest,&addr.sin_addr))
    return NULL;

  if (0 > (sock = socket(AF_INET,SOCK_STREAM,0))) {
    perror("socket");
    return NULL;
  }

  if (0 > set_non_blocking(sock))
    return NULL;

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

#ifdef DEBUG_SHORT_KEEPALIVE
  /* Set a small keepalive interval so connection timeouts can be tested easily */
  if (0 > set_keepalive(n,sock,2))
    return NULL;
#endif

  lip = (unsigned char*)&local_addr.sin_addr.s_addr;
  node_log(n,LOG_INFO,"node_connect: local ip is %u.%u.%u.%u",lip[0],lip[1],lip[2],lip[3]);

  hostname = lookup_hostname(n,addr.sin_addr);

  if (othernode) {
    conn = add_connection(n,hostname,sock,n->mainl);
    array_append(conn->sendbuf,&n->mainl->port,sizeof(int));
  }
  else {
    conn = add_connection(n,hostname,sock,NULL);
    conn->isreg = 1;
  }

  free(hostname);
  conn->connected = connected;
  conn->ip = addr.sin_addr;
  conn->localip = local_addr.sin_addr;
  conn->port = port;

  if (conn->connected)
    dispatch_event(n,EVENT_CONN_ESTABLISHED,conn,NULL);

  node_notify(n);

  return conn;
}

void node_send_locked(node *n, int sourcelocalid, endpointid destendpointid,
                      int tag, const void *data, int size)
{
  connection *conn;
  msgheader hdr;

  assert(NODE_ALREADY_LOCKED(n));

  memset(&hdr,0,sizeof(msgheader));
  hdr.source.localid = sourcelocalid;
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
      node_log(n,LOG_ERROR,"Could not find destination connection to %u.%u.%u.%u:%d",
               addrbytes[0],addrbytes[1],addrbytes[2],addrbytes[3],destendpointid.nodeport);
    }
    else {
      array_append(conn->sendbuf,&hdr,sizeof(msgheader));
      array_append(conn->sendbuf,data,size);
      node_notify(n);
    }
  }
}

void node_send(node *n, int sourcelocalid, endpointid destendpointid,
               int tag, const void *data, int size)
{
  lock_node(n);
  node_send_locked(n,sourcelocalid,destendpointid,tag,data,size);
  unlock_node(n);
}

/* This works as long as we don't recycle localids... */
void node_waitclose_locked(node *n, int localid)
{
  assert(NODE_ALREADY_LOCKED(n));
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
  assert(NODE_ALREADY_LOCKED(n));
  n->shutdown = 1;
  node_notify(n);
}

void node_notify(node *n)
{
  char c = 1;
  assert(NODE_ALREADY_LOCKED(n));
  if (!n->notified) {
    write(n->ioready_writefd,&c,1);
    n->notified = 1;
  }
}

void connection_printf(connection *conn, const char *format, ...)
{
  va_list ap;
  va_start(ap,format);
  lock_node(conn->n);
  array_vprintf(conn->sendbuf,format,ap);
  node_notify(conn->n);
  unlock_node(conn->n);
  va_end(ap);
}

void done_writing(node *n, connection *conn)
{
  if (!conn->canwrite)
    return;
  assert(NODE_ALREADY_LOCKED(n));
  assert(conn->canwrite);
  if (0 > shutdown(conn->sock,SHUT_WR))
    node_log(n,LOG_WARNING,"shutdown(SHUT_WR) on %s:%d failed: %s",
             conn->hostname,conn->port,strerror(errno));
  conn->canwrite = 0;
  if (!conn->canread && !conn->canwrite) {
    dispatch_event(n,EVENT_CONN_CLOSED,conn,NULL);
    remove_connection(n,conn);
  }
}

void done_reading(node *n, connection *conn)
{
  if (!conn->canread)
    return;
  assert(NODE_ALREADY_LOCKED(n));
  assert(conn->canread);
  if (0 > shutdown(conn->sock,SHUT_RD))
    node_log(n,LOG_WARNING,"shutdown(SHUT_RD) on %s:%d failed: %s",
             conn->hostname,conn->port,strerror(errno));
  conn->canread = 0;
  if (!conn->canread && !conn->canwrite) {
    dispatch_event(n,EVENT_CONN_CLOSED,conn,NULL);
    remove_connection(n,conn);
  }
}

/* @} */

/** @name Public functions - endpoints
 * @{ */

endpoint *node_add_endpoint(node *n, int localid, int type, void *data)
{
  endpoint *endpt = (endpoint*)calloc(1,sizeof(endpoint));
  init_mutex(&endpt->mailbox.lock);
  pthread_cond_init(&endpt->mailbox.cond,NULL);
  endpt->localid = localid;
  endpt->type = type;
  endpt->data = data;
  endpt->interruptptr = &endpt->tempinterrupt;

  lock_node(n);
  if (0 == endpt->localid) {
    if (0 == n->nextlocalid)
      fatal("localid wraparound");
    endpt->localid = n->nextlocalid++;
  }
  llist_append(&n->endpoints,endpt);
  dispatch_event(n,EVENT_ENDPOINT_ADDITION,NULL,endpt);
  unlock_node(n);
  return endpt;
}

void node_remove_endpoint(node *n, endpoint *endpt)
{
  lock_node(n);
  dispatch_event(n,EVENT_ENDPOINT_REMOVAL,NULL,endpt);
  llist_remove(&n->endpoints,endpt);
  pthread_cond_broadcast(&n->closecond);
  unlock_node(n);

  destroy_mutex(&endpt->mailbox.lock);
  pthread_cond_destroy(&endpt->mailbox.cond);
  free(endpt);
}

void endpoint_forceclose(endpoint *endpt)
{
  lock_mutex(&endpt->mailbox.lock);
  endpt->closed = 1;
  while (endpt->mailbox.first) {
    message *msg = endpt->mailbox.first;
    llist_remove(&endpt->mailbox,msg);
    free(msg->data);
    free(msg);
  }
  pthread_cond_broadcast(&endpt->mailbox.cond);
  unlock_mutex(&endpt->mailbox.lock);
}

void endpoint_add_message(endpoint *endpt, message *msg)
{
  lock_mutex(&endpt->mailbox.lock);
  if (!endpt->closed) {
    llist_append(&endpt->mailbox,msg);
    endpt->checkmsg = 1;
    if (endpt->interruptptr)
      *endpt->interruptptr = 1;
    pthread_cond_broadcast(&endpt->mailbox.cond);
  }
  unlock_mutex(&endpt->mailbox.lock);
}

message *endpoint_next_message(endpoint *endpt, int delayms)
{
  message *msg;
  lock_mutex(&endpt->mailbox.lock);

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
  unlock_mutex(&endpt->mailbox.lock);
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
