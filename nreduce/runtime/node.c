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
#include "messages.h"
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

static void endpoint_add_message(endpoint *endpt, message *msg);
static void node_send_locked(node *n, unsigned int sourcelocalid, endpointid destendpointid,
                             int tag, const void *data, int size);
static endpointid node_add_thread_locked(node *n, int localid, int type, int stacksize,
                                         endpoint_threadfun fun, void *arg, pthread_t *threadp);

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

static void handle_disconnection(node *n, connection *conn)
{
  assert(NODE_ALREADY_LOCKED(n));

  if (conn->isreg)
    return;

  /* A connection to another node has failed. Search for links that referenced endpoints
     on the other node, and sent an ENDPOINT_EXIT to the local endpoint in each case. */
  endpoint *endpt;
  for (endpt = n->endpoints.first; endpt; endpt = endpt->next) {
    int max = list_count(endpt->outlinks);
    endpointid *exited = (endpointid*)malloc(max*sizeof(endpointid));
    int count = 0;
    list *l;
    int i;

    /* We must grab the list of exited endpoints before calling node_send_locked(), because it
       will call through to endpoint_add_message() which will modify the endpoint list to remove
       the links, so we can't do this while traversing the list. */
    for (l = endpt->outlinks; l; l = l->next) {
      endpointid *epid = (endpointid*)l->data;
      if ((epid->ip == conn->ip) && (epid->port == conn->port))
        exited[count++] = *epid;
    }

    /* Now we've found the relevant endpoints, send ENDPOINT_EXIT for each */
    for (i = 0; i < count; i++) {
      endpoint_exit_msg msg;
      msg.epid = exited[i];
      node_send_locked(n,endpt->epid.localid,endpt->epid,MSG_ENDPOINT_EXIT,
                       &msg,sizeof(endpoint_exit_msg));
    }

    free(exited);
  }
}

static void notify_accept(node *n, connection *conn)
{
  listener *l = conn->l;
  accept_response_msg arm;

  if (!l->notify)
    return;

  assert(!socketid_isnull(&l->sockid));
  assert(!endpointid_isnull(&l->owner));

  conn->dontread = 1;
  conn->owner = l->owner;
  conn->isreg = 1;

  /* send message */
  assert(0 != l->accept_frameid);
  arm.ioid = l->accept_frameid;
  arm.sockid = conn->sockid;
  snprintf(arm.hostname,HOSTNAME_MAX,"%s",conn->hostname);
  arm.hostname[HOSTNAME_MAX] = '\0';
  arm.port = conn->port;

  node_send_locked(n,MANAGER_ID,l->owner,MSG_ACCEPT_RESPONSE,&arm,sizeof(arm));

  l->accept_frameid = 0;
  assert(!l->dontaccept);
  l->dontaccept = 1;
  node_notify(n);
}

/* FIXME: for all regular connections, perhaps we should assert that we have a frameid set */
static void notify_connect(node *n, connection *conn, int error)
{
  if (0 < conn->frameids[CONNECT_FRAMEADDR]) {
    connect_response_msg crm;

    crm.ioid = conn->frameids[CONNECT_FRAMEADDR];
    crm.error = error;
    if (error)
      memset(&crm.sockid,0,sizeof(crm.sockid));
    else
      crm.sockid = conn->sockid;
    memcpy(crm.errmsg,conn->errmsg,sizeof(crm.errmsg));

    conn->frameids[CONNECT_FRAMEADDR] = 0;

    node_send_locked(n,MANAGER_ID,conn->owner,MSG_CONNECT_RESPONSE,&crm,sizeof(crm));
  }

  if (error)
    handle_disconnection(n,conn);
}

static void notify_read(node *n, connection *conn)
{
  if (conn->isreg && (0 < conn->frameids[READ_FRAMEADDR])) {
    int rrmlen = sizeof(read_response_msg)+conn->recvbuf->nbytes;
    read_response_msg *rrm = (read_response_msg*)malloc(rrmlen);

    rrm->ioid = conn->frameids[READ_FRAMEADDR];
    rrm->sockid = conn->sockid;
    rrm->len = conn->recvbuf->nbytes;
    memcpy(rrm->data,conn->recvbuf->data,conn->recvbuf->nbytes);

    node_send_locked(n,MANAGER_ID,conn->owner,MSG_READ_RESPONSE,rrm,rrmlen);

    conn->frameids[READ_FRAMEADDR] = 0;
    conn->recvbuf->nbytes = 0;
    conn->dontread = 1;
    free(rrm);
  }
}

static void notify_closed(node *n, connection *conn, int error)
{
  if (0 != conn->owner.localid) {
    connection_event_msg cem;
    cem.sockid = conn->sockid;
    cem.error = error;
    memcpy(cem.errmsg,conn->errmsg,sizeof(cem.errmsg));
    node_send_locked(n,MANAGER_ID,conn->owner,MSG_CONNECTION_CLOSED,&cem,sizeof(cem));
  }
  handle_disconnection(n,conn);
}

static void notify_write(node *n, connection *conn)
{
  if ((0 < conn->frameids[WRITE_FRAMEADDR]) && (IOSIZE > conn->sendbuf->nbytes)) {
    write_response_msg wrm;
    wrm.ioid = conn->frameids[WRITE_FRAMEADDR];
    conn->frameids[WRITE_FRAMEADDR] = 0;
    node_send_locked(n,MANAGER_ID,conn->owner,MSG_WRITE_RESPONSE,&wrm,sizeof(wrm));
  }
}

static void got_message(node *n, const msgheader *hdr, const void *data)
{
  endpoint *endpt;
  message *newmsg;

  if (NULL == (endpt = find_endpoint(n,hdr->destlocalid))) {
    endpointid_str source;
    print_endpointid(source,hdr->source);

    /* This handles the case where a link message is sent but the destination has
       already exited. If the link were to arrive just before the endpoint exits, then
       the endpoint removal code would take care of sending the exit message. */
    if (MSG_LINK == hdr->tag) {
      endpointid destid;
      destid.ip = n->listenip;
      destid.port = n->listenport;
      destid.localid = hdr->destlocalid;
      assert(sizeof(endpointid) == hdr->size);
      node_send_locked(n,hdr->destlocalid,*(endpointid*)data,MSG_ENDPOINT_EXIT,
                       &destid,sizeof(endpointid));
    }
    else if (MSG_UNLINK == hdr->tag) {
      /* don't need to do anything in this case */
    }
    else {
/*       node_log(n,LOG_WARNING,"Message %s received for unknown endpoint %d (source %s)", */
/*                msg_names[hdr->tag],hdr->destlocalid,source); */
    }
    return;
  }

  newmsg = (message*)calloc(1,sizeof(message));
  newmsg->hdr = *hdr;
  newmsg->data = (char*)malloc(hdr->size);
  memcpy(newmsg->data,data,hdr->size);
  endpoint_add_message(endpt,newmsg);
}

static void remove_connection(node *n, connection *conn)
{
  assert(NODE_ALREADY_LOCKED(n));
  node_log(n,LOG_INFO,"Removing connection %s",conn->hostname);
  close(conn->sock);
  llist_remove(&n->connections,conn);
  free(conn->hostname);
  array_free(conn->recvbuf);
  array_free(conn->sendbuf);
  free(conn);
}

static void start_console(node *n, connection *conn)
{
  socketid *sockid = (socketid*)malloc(sizeof(socketid));
  conn->isreg = 1;
  conn->frameids[READ_FRAMEADDR] = 1;
  memcpy(sockid,&conn->sockid,sizeof(socketid));
  conn->owner = node_add_thread_locked(n,0,CONSOLE_ENDPOINT,0,console_thread,sockid,NULL);
}

static void process_received(node *n, connection *conn)
{
  int start = 0;

  notify_read(n,conn);

  if (conn->isreg)
    return;

  if (!conn->donewelcome) {
    if (conn->recvbuf->nbytes < strlen(WELCOME_MESSAGE)) {
      if (strncmp(conn->recvbuf->data,WELCOME_MESSAGE,conn->recvbuf->nbytes)) {
        /* Data sent so far is not part of the welcome message; must be a console client */
        start_console(n,conn);
        notify_read(n,conn);
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
        start_console(n,conn);
        notify_read(n,conn);
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
    unsigned char *connip = (unsigned char*)&conn->ip;

    if (0 > conn->port) {
      if (sizeof(int) > conn->recvbuf->nbytes-start) {
        array_remove_data(conn->recvbuf,start);
        return;
      }
      conn->port = (int)(*(unsigned short*)&conn->recvbuf->data[start]);
      start += sizeof(unsigned short);

      if (0 > conn->port)
        fatal("Client sent bad listen port: %d",conn->port);
    }

    node_log(n,LOG_INFO,"Node %u.%u.%u.%u:%d connected",
             connip[0],connip[1],connip[2],connip[3],conn->port);
    conn->donehandshake = 1;
  }

  /* inspect the next section of the input buffer to see if it contains a complete message */
  while (MSG_HEADER_SIZE <= conn->recvbuf->nbytes-start) {
    msgheader *hdr = (msgheader*)&conn->recvbuf->data[start];

    /* verify header */
    assert(0 <= hdr->size);
    assert(0 <= hdr->tag);
    assert(MSG_COUNT > hdr->tag);

    hdr->source.ip = conn->ip;
    hdr->source.port = conn->port;

    if (MSG_HEADER_SIZE+hdr->size > conn->recvbuf->nbytes-start)
      break; /* incomplete message */

    /* complete message present; add it to the mailbox */
    got_message(n,hdr,&conn->recvbuf->data[start+MSG_HEADER_SIZE]);
    start += MSG_HEADER_SIZE+hdr->size;
  }

  /* remove the data we've consumed from the receive buffer */
  array_remove_data(conn->recvbuf,start);
}

static connection *find_connection(node *n, in_addr_t nodeip, unsigned short nodeport)
{
  if (getenv("OUTGOING")) {
    connection *conn;
    for (conn = n->connections.first; conn; conn = conn->next)
      if ((conn->ip == nodeip) && (conn->port == nodeport) && conn->outgoing)
        return conn;
    return NULL;
  }
  else {
    connection *conn;
    for (conn = n->connections.first; conn; conn = conn->next)
      if ((conn->ip == nodeip) && (conn->port == nodeport))
        return conn;
    return NULL;
  }
}

static connection *add_connection(node *n, const char *hostname, int sock, listener *l)
{
  connection *conn = (connection*)calloc(1,sizeof(connection));
  assert(NODE_ALREADY_LOCKED(n));
  assert(MANAGER_ID == n->managerid.localid);
  conn->sockid.managerid = n->managerid;
  conn->sockid.sid = n->nextsid++;
  conn->hostname = strdup(hostname);
  conn->ip = 0;
  conn->sock = sock;
  conn->l = l;
  conn->n = n;
/*   conn->readfd = -1; */
  conn->port = -1;
  conn->recvbuf = array_new(1,IOSIZE*2);
  conn->sendbuf = array_new(1,IOSIZE*2);
  conn->canread = 1;
  conn->canwrite = 1;
  if (l == n->mainl)
    array_append(conn->sendbuf,WELCOME_MESSAGE,strlen(WELCOME_MESSAGE));
  llist_append(&n->connections,conn);
  return conn;
}

static int handle_connected(node *n, connection *conn)
{
  int err;
  socklen_t optlen = sizeof(int);

  if (0 > getsockopt(conn->sock,SOL_SOCKET,SO_ERROR,&err,&optlen)) {
    snprintf(conn->errmsg,ERRMSG_MAX,"getsockopt SO_ERROR: %s",strerror(errno));
    conn->errmsg[ERRMSG_MAX] = '\0';
    notify_connect(n,conn,1);
    remove_connection(n,conn);
    return -1;
  }

  if (0 == err) {
    int yes = 1;
    conn->connected = 1;
    node_log(n,LOG_INFO,"Connected to %s:%d",conn->hostname,conn->port);

    if (0 > setsockopt(conn->sock,IPPROTO_TCP,TCP_NODELAY,&yes,sizeof(int))) {
      snprintf(conn->errmsg,ERRMSG_MAX,"setsockopt TCP_NODELAY: %s",strerror(errno));
      conn->errmsg[ERRMSG_MAX] = '\0';
      notify_connect(n,conn,1);
      remove_connection(n,conn);
      return -1;
    }

    notify_connect(n,conn,0);
  }
  else {
    node_log(n,LOG_WARNING,"Connection to %s:%d failed: %s",
             conn->hostname,conn->port,strerror(err));
    snprintf(conn->errmsg,ERRMSG_MAX,"%s",strerror(err));
    conn->errmsg[ERRMSG_MAX] = '\0';
    notify_connect(n,conn,1);
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
      snprintf(conn->errmsg,ERRMSG_MAX,"%s",strerror(errno));
      conn->errmsg[ERRMSG_MAX] = '\0';
      notify_closed(n,conn,1);
      remove_connection(n,conn);
      return;
    }

    if (0 == w) {
      node_log(n,LOG_WARNING,"write() to %s:%d failed: connection closed",
               conn->hostname,conn->port);
      snprintf(conn->errmsg,ERRMSG_MAX,"connection closed");
      notify_closed(n,conn,1);
      remove_connection(n,conn);
      return;
    }

    array_remove_data(conn->sendbuf,w);
  }

  notify_write(n,conn);

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

  assert(conn->canread);

  array_mkroom(conn->recvbuf,IOSIZE);
  r = TEMP_FAILURE_RETRY(read(conn->sock,
                              &buf->data[buf->nbytes],
                              IOSIZE));

  if (0 > r) {
    node_log(n,LOG_WARNING,"read() from %s:%d failed: %s",
             conn->hostname,conn->port,strerror(errno));
    snprintf(conn->errmsg,ERRMSG_MAX,"%s",strerror(errno));
    conn->errmsg[ERRMSG_MAX] = '\0';
    notify_closed(n,conn,1);
    remove_connection(n,conn);
    return;
  }

  if (0 == r) {
    if (conn->isreg)
      node_log(n,LOG_INFO,"read: Connection %s:%d closed by peer",conn->hostname,conn->port);
    else
      node_log(n,LOG_WARNING,"read: Connection %s:%d closed by peer",conn->hostname,conn->port);
    notify_read(n,conn);
    handle_disconnection(n,conn);

    /* Don't close "regular" connections (socket streams accessible to ELC programs) here
       because we may still need to send more data. In other cases, if the console client or
       another node disconnects, we are done with the connection. */
    if (!conn->isreg)
      done_writing(n,conn);
    done_reading(n,conn);
    return;
  }

  conn->recvbuf->nbytes += r;
  conn->totalread += r;
  process_received(n,conn);
}

char *lookup_hostname(node *n, in_addr_t addr)
{
  struct hostent *he;
  char *res;
  struct in_addr addr1;
  addr1.s_addr = addr;

  lock_mutex(&n->liblock);
  he = gethostbyaddr(&addr1,sizeof(struct in_addr),AF_INET);
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

int lookup_address(node *n, const char *host, in_addr_t *out, int *h_errout)
{
  struct hostent *he;
  int r = 0;

  lock_mutex(&n->liblock);
  if (NULL == (he = gethostbyname(host))) {
    if (h_errout)
      *h_errout = h_errno;
    node_log(n,LOG_WARNING,"%s: %s",host,hstrerror(h_errno));
    r = -1;
  }
  else if (4 != he->h_length) {
    if (h_errout)
      *h_errout = HOST_NOT_FOUND;
    node_log(n,LOG_WARNING,"%s: invalid address type",host);
    r = -1;
  }
  else {
    *out = (*((struct in_addr*)he->h_addr)).s_addr;
  }
  unlock_mutex(&n->liblock);
  return r;
}

static void handle_new_connection(node *n, listener *l)
{
  struct sockaddr_in remote_addr;
  socklen_t sin_size = sizeof(struct sockaddr_in);
  int clientfd;
  connection *conn;
  int yes = 1;
  char *hostname;

  if (0 > (clientfd = accept(l->fd,(struct sockaddr*)&remote_addr,&sin_size))) {
    perror("accept");
    return;
  }

  if (0 > set_non_blocking(clientfd)) {
    close(clientfd);
    return;
  }

  if (0 > setsockopt(clientfd,IPPROTO_TCP,TCP_NODELAY,&yes,sizeof(int))) {
    perror("setsockopt TCP_NODELAY");
    return;
  }

  hostname = lookup_hostname(n,remote_addr.sin_addr.s_addr);
  node_log(n,LOG_INFO,"Got connection from %s",hostname);
  conn = add_connection(n,hostname,clientfd,l);
  free(hostname);

  conn->connected = 1;
  conn->ip = remote_addr.sin_addr.s_addr;

  notify_accept(n,conn);
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
         is actually blocked. node_notify() writes a single byte of data to the node's ioready pipe,
         which will cause the select to be woken, and we will look around again to write the
         data. */
    }

    unlock_mutex(&n->lock);
    s = select(highest+1,&readfds,&writefds,NULL,NULL);
    lock_mutex(&n->lock);

    if (0 > s) {
      /* Ignore bad file descriptor error. It is possible (and legitimate) that a connection
         could be closed by another thread while we are waiting in the select loop. */
      if (EBADF != errno)
        fatal("select: %s",strerror(errno));
    }

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
      if (FD_ISSET(conn->sock,&readfds) && conn->canread)
        handle_read(n,conn);
    }

    /* accept new connections */
    for (l = n->listeners.first; l; l = l->next) {
      if (FD_ISSET(l->fd,&readfds))
        handle_new_connection(n,l);
    }
  }
  unlock_mutex(&n->lock);
  return NULL;
}

static void determine_ip(node *n)
{
  in_addr_t addr = 0;
  char hostname[4097];
  FILE *hf = popen("hostname","r");
  int size;
  if (NULL == hf) {
    fprintf(stderr,
            "Could not determine IP address, because executing the hostname command "
            "failed (%s)\n",strerror(errno));
    exit(1);
  }
  size = fread(hostname,1,4096,hf);
  if (0 > hf) {
    fprintf(stderr,
            "Could not determine IP address, because reading output from the hostname "
            "command failed (%s)\n",strerror(errno));
    pclose(hf);
    exit(1);
  }
  pclose(hf);
  if ((0 < size) && ('\n' == hostname[size-1]))
    size--;
  hostname[size] = '\0';

  if (0 > lookup_address(n,hostname,&addr,NULL)) {
    fprintf(stderr,
            "Could not determine IP address, because the address \"%s\" "
            "returned by the hostname command could not be resolved (%s)\n",
            hostname,strerror(errno));
    exit(1);
  }
  n->listenip = addr;
}

node *node_new(int loglevel)
{
  node *n = (node*)calloc(1,sizeof(node));
  int pipefds[2];
  char *logenv;

  init_mutex(&n->lock);
  init_mutex(&n->liblock);
  pthread_cond_init(&n->cond,NULL);
  pthread_cond_init(&n->closecond,NULL);
  n->nextlocalid = FIRST_ID;
  n->nextsid = 2;

  determine_ip(n);

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

  return n;
}

void node_free(node *n)
{
  if (n->mainl)
    node_remove_listener(n,n->mainl);
  assert(NULL == n->endpoints.first);
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

listener *node_listen(node *n, in_addr_t ip, int port, int notify, void *data,
                      int dontaccept, int ismain, endpointid *owner, char *errmsg, int errlen)
{
  int fd;
  int actualport = 0;
  int yes = 1;
  struct sockaddr_in local_addr;
  struct sockaddr_in new_addr;
  socklen_t new_size = sizeof(struct sockaddr_in);
  listener *l;

  local_addr.sin_family = AF_INET;
  local_addr.sin_port = htons(port);
  local_addr.sin_addr.s_addr = ip;
  memset(&local_addr.sin_zero,0,8);

  if (-1 == (fd = socket(AF_INET,SOCK_STREAM,0))) {
    if (errmsg)
      snprintf(errmsg,errlen,"%s",strerror(errno));
    else
      node_log(n,LOG_ERROR,"socket: %s",strerror(errno));
    return NULL;
  }

  if (-1 == setsockopt(fd,SOL_SOCKET,SO_REUSEADDR,&yes,sizeof(int))) {
    if (errmsg)
      snprintf(errmsg,errlen,"%s",strerror(errno));
    else
      node_log(n,LOG_ERROR,"setsockopt: %s",strerror(errno));
    return NULL;
  }

  if (-1 == bind(fd,(struct sockaddr*)&local_addr,sizeof(struct sockaddr))) {
    if (errmsg)
      snprintf(errmsg,errlen,"%s",strerror(errno));
    else
      node_log(n,LOG_ERROR,"bind: %s",strerror(errno));
    return NULL;
  }

  if (0 > getsockname(fd,(struct sockaddr*)&new_addr,&new_size)) {
    if (errmsg)
      snprintf(errmsg,errlen,"%s",strerror(errno));
    else
      node_log(n,LOG_ERROR,"getsockname: %s",strerror(errno));
    return NULL;
  }
  actualport = ntohs(new_addr.sin_port);

  if (-1 == listen(fd,LISTEN_BACKLOG)) {
    if (errmsg)
      snprintf(errmsg,errlen,"%s",strerror(errno));
    else
      node_log(n,LOG_ERROR,"listen: %s",strerror(errno));
    return NULL;
  }

  assert((0 == port) || (actualport == port));

  l = (listener*)calloc(1,sizeof(listener));
  l->ip = ip;
  l->port = actualport;
  l->fd = fd;
  l->notify = notify;
  l->data = data;
  l->dontaccept = dontaccept;
  if (owner)
    l->owner = *owner;

  if (ismain) {
    n->listenport = actualport;
    n->mainl = l;

    n->managerid.ip = n->listenip;
    n->managerid.port = n->listenport;
    n->managerid.localid = MANAGER_ID;
  }

  lock_node(n);
  l->sockid.managerid = n->managerid;
  l->sockid.sid = n->nextsid++;
  llist_append(&n->listeners,l);
  node_notify(n);
  unlock_node(n);

  return l;
}

endpoint *find_endpoint(node *n, int localid)
{
  endpoint *endpt;
  assert(NODE_ALREADY_LOCKED(n));
  for (endpt = n->endpoints.first; endpt; endpt = endpt->next)
    if (endpt->epid.localid == localid)
      break;
  return endpt;
}

void node_remove_listener(node *n, listener *l)
{
  connection *conn;

  lock_node(n);

  conn = n->connections.first;
  while (conn) {
    connection *next = conn->next;
    if (conn->l == l) {
      notify_closed(n,conn,0);
      remove_connection(n,conn);
    }
    conn = next;
  }

  llist_remove(&n->listeners,l);
  node_notify(n);

  unlock_node(n);

  close(l->fd);
  free(l);
}

void node_start_iothread(node *n)
{
  if (0 != pthread_create(&n->iothread,NULL,ioloop,n))
    fatal("pthread_create: %s",strerror(errno));
}

/* This works as long as we don't recycle localids... */
static void node_waitclose_locked(node *n, int localid)
{
  assert(NODE_ALREADY_LOCKED(n));
  while (1) {
    int alive = 0;
    endpoint *endpt;
    for (endpt = n->endpoints.first; endpt; endpt = endpt->next)
      if (endpt->epid.localid == localid)
        alive = 1;

    if (!alive)
      break;

    pthread_cond_wait(&n->closecond,&n->lock);
  }
}

void node_close_endpoints(node *n)
{
  endpoint *endpt;
  lock_mutex(&n->lock);
  while (NULL != (endpt = n->endpoints.first)) {
    node_send_locked(n,endpt->epid.localid,endpt->epid,MSG_KILL,NULL,0);
    node_waitclose_locked(n,endpt->epid.localid);
  }
  unlock_mutex(&n->lock);
}

void node_close_connections(node *n)
{
  lock_node(n);
  while (n->connections.first) {
    notify_closed(n,n->connections.first,0);
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

connection *node_connect_locked(node *n, const char *dest, in_addr_t destaddr,
                                int port, int othernode, char *errmsg, int errlen)
{
  struct sockaddr_in addr;
  int sock;
  int connected = 0;
  connection *conn;
  char *hostname;
  int r;

  assert(NODE_ALREADY_LOCKED(n));

  addr.sin_family = AF_INET;
  addr.sin_port = htons(port);
  memset(&addr.sin_zero,0,8);

  if (dest) {
    int error = 0;
    if (0 > lookup_address(n,dest,&addr.sin_addr.s_addr,&error)) {
      if (errmsg)
        snprintf(errmsg,errlen,"%s",hstrerror(error));
      return NULL;
    }
  }
  else {
    addr.sin_addr.s_addr = destaddr;
  }

  if (0 > (sock = socket(AF_INET,SOCK_STREAM,0))) {
    if (errmsg)
      snprintf(errmsg,errlen,"%s",strerror(errno));
    perror("socket");
    return NULL;
  }

  if (0 > set_non_blocking(sock)) {
    if (errmsg)
      snprintf(errmsg,errlen,"cannot set non-blocking socket");
    return NULL;
  }

  /* We expect the connect() call to not succeed immediately, because the socket is in
     non-blocking mode */
  r = connect(sock,(struct sockaddr*)&addr,sizeof(struct sockaddr));
  assert(0 > r);
  if (EINPROGRESS != errno) {
    if (errmsg)
      snprintf(errmsg,errlen,"%s",strerror(errno));
    node_log(n,LOG_WARNING,"connect: %s",strerror(errno));
    return NULL;
  }

#ifdef DEBUG_SHORT_KEEPALIVE
  /* Set a small keepalive interval so connection timeouts can be tested easily */
  if (0 > set_keepalive(n,sock,2)) {
    if (errmsg)
      snprintf(errmsg,errlen,"error initializing keepalive");
    return NULL;
  }
#endif

  hostname = lookup_hostname(n,addr.sin_addr.s_addr);

  if (othernode) {
    assert(n->listenport == n->mainl->port);
    conn = add_connection(n,hostname,sock,n->mainl);
    array_append(conn->sendbuf,&n->listenport,sizeof(unsigned short));
  }
  else {
    conn = add_connection(n,hostname,sock,NULL);
    conn->isreg = 1;
  }

  free(hostname);
  conn->connected = connected;
  conn->ip = addr.sin_addr.s_addr;
  conn->port = port;
  conn->outgoing = 1;

  if (conn->connected)
    notify_connect(n,conn,0);

  node_notify(n);

  return conn;
}

static void node_send_locked(node *n, unsigned int sourcelocalid, endpointid destendpointid,
                             int tag, const void *data, int size)
{
  connection *conn;
  msgheader hdr;

  assert(NODE_ALREADY_LOCKED(n));
  assert(n->listenport == n->mainl->port);
  assert(INADDR_ANY != destendpointid.ip);
  assert(0 < destendpointid.port);

  memset(&hdr,0,sizeof(msgheader));
  hdr.source.ip = n->listenip;
  hdr.source.port = n->listenport;
  hdr.source.localid = sourcelocalid;
  hdr.destlocalid = destendpointid.localid;
  hdr.size = size;
  hdr.tag = tag;

  if ((destendpointid.ip == n->listenip) &&
      (destendpointid.port == n->listenport)) {
    got_message(n,&hdr,data);
  }
  else {
    if (NULL == (conn = find_connection(n,destendpointid.ip,destendpointid.port))) {
      unsigned char *addrbytes = (unsigned char*)&destendpointid.ip;
      node_log(n,LOG_INFO,"No connection yet to %u.%u.%u.%u:%d; establishing",
               addrbytes[0],addrbytes[1],addrbytes[2],addrbytes[3],destendpointid.port);
      conn = node_connect_locked(n,NULL,destendpointid.ip,destendpointid.port,1,NULL,0);
    }

    array_append(conn->sendbuf,&hdr,sizeof(msgheader));
    array_append(conn->sendbuf,data,size);
    node_notify(n);
  }
}

void node_shutdown(node *n)
{
  lock_node(n);
  n->shutdown = 1;
  node_notify(n);
  unlock_node(n);
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
    notify_closed(n,conn,0);
    remove_connection(n,conn);
  }
}

void done_reading(node *n, connection *conn)
{
  assert(NODE_ALREADY_LOCKED(n));
  if (!conn->canread)
    return;
  assert(conn->canread);
  if (0 > shutdown(conn->sock,SHUT_RD))
    node_log(n,LOG_WARNING,"shutdown(SHUT_RD) on %s:%d failed: %s",
             conn->hostname,conn->port,strerror(errno));
  conn->canread = 0;
  if (!conn->canread && !conn->canwrite) {
    notify_closed(n,conn,0);
    remove_connection(n,conn);
  }
}

static void *endpoint_thread(void *data)
{
  endpoint *endpt = (endpoint*)data;
  node *n = endpt->n;
  list *l;

  enable_invalid_fpe();

  /* Execute the thread */
  endpt->fun(n,endpt,endpt->data);

  /* Remove the endpoint */
  lock_node(n);
  for (l = endpt->inlinks; l; l = l->next)
    node_send_locked(n,endpt->epid.localid,*(endpointid*)l->data,MSG_ENDPOINT_EXIT,
                     &endpt->epid,sizeof(endpointid));
  llist_remove(&n->endpoints,endpt);
  pthread_cond_broadcast(&n->closecond);

  /* Free data */
  while (NULL != endpt->mailbox.first) {
    message *next = endpt->mailbox.first->next;
    message_free(endpt->mailbox.first);
    endpt->mailbox.first = next;
  }
  list_free(endpt->inlinks,free);
  list_free(endpt->outlinks,free);
  destroy_mutex(&endpt->mailbox.lock);
  pthread_cond_destroy(&endpt->mailbox.cond);
  free(endpt);
  unlock_node(n);
  return NULL;
}

static endpointid node_add_thread_locked(node *n, int localid, int type, int stacksize,
                                         endpoint_threadfun fun, void *arg, pthread_t *threadp)
{
  pthread_t thread;
  pthread_attr_t attr;
  endpointid epid;
  endpoint *endpt;

  assert(NODE_ALREADY_LOCKED(n));
  assert(0 < n->listenport);
  assert(n->listenport == n->mainl->port);

  /* Assign endpoint id */
  epid.ip = n->listenip;
  epid.port = n->listenport;
  if (0 == localid) {
    if (UINT32_MAX == n->nextlocalid)
      fatal("Out of local identifiers");
    epid.localid = n->nextlocalid++;
  }
  else {
    epid.localid = localid;
  }

  /* Create endpoint */
  endpt = (endpoint*)calloc(1,sizeof(endpoint));
  init_mutex(&endpt->mailbox.lock);
  pthread_cond_init(&endpt->mailbox.cond,NULL);
  endpt->epid = epid;
  endpt->type = type;
  endpt->data = arg;
  endpt->n = n;
  endpt->fun = fun;
  llist_append(&n->endpoints,endpt);

  /* Start thread */
  if (0 != pthread_attr_init(&attr))
    fatal("pthread_attr_init: %s",strerror(errno));

  if ((0 < stacksize) && (0 != pthread_attr_setstacksize(&attr,stacksize)))
    fatal("pthread_attr_setstacksize: %s",strerror(errno));

  if ((NULL == threadp) && (0 != pthread_attr_setdetachstate(&attr,PTHREAD_CREATE_DETACHED)))
    fatal("pthread_attr_setdetachstate: %s",strerror(errno));

  if (0 != pthread_create(&thread,&attr,endpoint_thread,endpt))
    fatal("pthread_create: %s",strerror(errno));

  if (0 != pthread_attr_destroy(&attr))
    fatal("pthread_attr_destroy: %s",strerror(errno));

  if (threadp)
    *threadp = thread;

  endpt->thread = thread;

  return epid;
}

endpointid node_add_thread(node *n, int localid, int type, int stacksize,
                           endpoint_threadfun fun, void *arg, pthread_t *threadp)
{
  endpointid epid;
  lock_node(n);
  epid = node_add_thread_locked(n,localid,type,stacksize,fun,arg,threadp);
  unlock_node(n);
  return epid;
}

static int endpoint_has_link(endpoint *endpt, endpointid epid)
{
  list *l;
  assert(NODE_ALREADY_LOCKED(endpt->n));
  for (l = endpt->outlinks; l; l = l->next)
    if (endpointid_equals((endpointid*)l->data,&epid))
      return 1;
  return 0;
}

void endpoint_link(endpoint *endpt, endpointid to)
{
  endpointid *copy = (endpointid*)malloc(sizeof(endpointid));
  assert(NODE_ALREADY_LOCKED(endpt->n));
  assert(!endpoint_has_link(endpt,to));
  memcpy(copy,&to,sizeof(endpointid));
  list_push(&endpt->outlinks,copy);
  node_send_locked(endpt->n,endpt->epid.localid,to,MSG_LINK,&endpt->epid,sizeof(endpointid));
}

static void endpoint_list_remove(list **lptr, endpointid to)
{
  while (*lptr) {
    if (endpointid_equals(((endpointid*)(*lptr)->data),&to)) {
      list *old = *lptr;
      *lptr = (*lptr)->next;
      free(old->data);
      free(old);
      return;
    }
    lptr = &((*lptr)->next);
  }
}

void endpoint_unlink(endpoint *endpt, endpointid to)
{
  assert(NODE_ALREADY_LOCKED(endpt->n));
  endpoint_list_remove(&endpt->outlinks,to);
  node_send_locked(endpt->n,endpt->epid.localid,to,MSG_UNLINK,&endpt->epid,sizeof(endpointid));
}

void endpoint_interrupt(endpoint *endpt) /* Can be called from native code */
{
  endpt->interrupt = 1;
  if (endpt->signal)
    pthread_kill(endpt->thread,SIGUSR1);
}

static void endpoint_add_message(endpoint *endpt, message *msg)
{
  assert(NODE_ALREADY_LOCKED(endpt->n));
  if (MSG_LINK == msg->hdr.tag) {
    assert(sizeof(endpointid) == msg->hdr.size);
    list_push(&endpt->inlinks,(endpointid*)msg->data);
    free(msg);
  }
  else if (MSG_UNLINK == msg->hdr.tag) {
    assert(sizeof(endpointid) == msg->hdr.size);
    endpoint_list_remove(&endpt->inlinks,*(endpointid*)msg->data);
    message_free(msg);
  }
  else {

    if (MSG_ENDPOINT_EXIT == msg->hdr.tag) {
      assert(sizeof(endpointid) == msg->hdr.size);
      endpoint_list_remove(&endpt->inlinks,*(endpointid*)msg->data);
      endpoint_list_remove(&endpt->outlinks,*(endpointid*)msg->data);
    }

    lock_mutex(&endpt->mailbox.lock);
    if (!endpt->closed) {
      llist_append(&endpt->mailbox,msg);
      endpt->checkmsg = 1;
      endpoint_interrupt(endpt);
      pthread_cond_broadcast(&endpt->mailbox.cond);
    }
    unlock_mutex(&endpt->mailbox.lock);
  }
}

void endpoint_send(endpoint *endpt, endpointid dest, int tag, const void *data, int size)
{
  lock_node(endpt->n);
  node_send_locked(endpt->n,endpt->epid.localid,dest,tag,data,size);
  unlock_node(endpt->n);
}

message *endpoint_receive(endpoint *endpt, int delayms)
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

int endpointid_equals(const endpointid *e1, const endpointid *e2)
{
  return ((e1->ip == e2->ip) && (e1->port == e2->port) && (e1->localid == e2->localid));
}

int endpointid_isnull(const endpointid *epid)
{
  return (0 == epid->localid);
}

void print_endpointid(endpointid_str str, endpointid epid)
{
  sprintf(str,EPID_FORMAT,EPID_ARGS(epid));
}

void message_free(message *msg)
{
  free(msg->data);
  free(msg);
}

int socketid_equals(const socketid *a, const socketid *b)
{
  return (endpointid_equals(&a->managerid,&b->managerid) && (a->sid == b->sid));
}

int socketid_isnull(const socketid *a)
{
  return (0 == a->sid);
}
