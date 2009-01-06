/*
 * This file is part of the nreduce project
 * Copyright (C) 2006-2008 Peter Kelly (pmk@cs.adelaide.edu.au)
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
#include "node.h"
#include "netprivate.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <assert.h>
#include <ctype.h>
#include <stdarg.h>
#include <math.h>
#include <errno.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>

static listener *get_listener(node *n, socketid id)
{
  listener *l;
  assert(NODE_ALREADY_LOCKED(n));
  for (l = n->p->listeners.first; l; l = l->next)
    if (socketid_equals(&l->sockid,&id))
      return l;
  return NULL;
}

static void iothread_listen(node *n, endpoint *endpt, listen_msg *m, endpointid source)
{
  listener *l;
  listen_response_msg lrm;

  endpoint_link_locked(endpt,m->owner);

  l = node_listen_locked(n,m->ip,m->port,1,NULL,1,0,&m->owner,lrm.errmsg,ERRMSG_MAX);
  lrm.errmsg[ERRMSG_MAX] = '\0';

  lrm.ioid = m->ioid;
  if (NULL != l) {
    lrm.error = 0;
    lrm.sockid = l->sockid;
  }
  else {
    lrm.error = 1;
    memset(&lrm.sockid,0,sizeof(socketid));
  }

  endpoint_send_locked(endpt,source,MSG_LISTEN_RESPONSE,&lrm,sizeof(lrm));
}

static void iothread_accept(node *n, endpoint *endpt, accept_msg *m, endpointid source)
{
  listener *l;
  l = get_listener(n,m->sockid);
  assert(NULL != l);
  assert(0 == l->accept_frameid);
  l->accept_frameid = m->ioid;
  assert(l->dontaccept);
  l->dontaccept = 0;
  node_notify(n);
}

static void iothread_connect(node *n, endpoint *endpt, connect_msg *m, endpointid source)
{
  connection *conn;
  char hostname[HOSTNAME_MAX+1];

  endpoint_link_locked(endpt,m->owner);
  sprintf(hostname,IP_FORMAT,IP_ARGS(m->ip));
  conn = add_connection(n,hostname,m->ip,NULL);
  conn->isreg = 1;
  conn->port = m->port;
  assert(0 == conn->frameids[CONNECT_FRAMEADDR]);
  conn->frameids[CONNECT_FRAMEADDR] = m->ioid;
  conn->dontread = 1;
  conn->owner = m->owner;
  connection_fsm(conn,CE_REQUESTED);

  connect_pending(n);
}

static void iothread_connpair(node *n, endpoint *endpt, connpair_msg *m, endpointid source)
{
  connpair_response_msg crm;
  int fds[2];
  memset(&crm,0,sizeof(crm));
  crm.ioid = m->ioid;
  if (0 > socketpair(AF_UNIX,SOCK_STREAM,0,fds)) {
    crm.error = 1;
    snprintf(crm.errmsg,ERRMSG_MAX,"socketpair: %s",strerror(errno));
    crm.errmsg[ERRMSG_MAX] = '\0';
  }
  else if ((0 > set_non_blocking(fds[0])) || (0 > set_non_blocking(fds[1]))) {
    crm.error = 1;
    snprintf(crm.errmsg,ERRMSG_MAX,"cannot set non-blocking socket");
    crm.errmsg[ERRMSG_MAX] = '\0';
  }
  else {
    connection *ca = add_connection(n,"(internal)",0,NULL);
    connection *cb = add_connection(n,"(internal)",0,NULL);
    ca->sock = fds[0];
    cb->sock = fds[1];
    crm.a = ca->sockid;
    crm.b = cb->sockid;

    ca->dontread = 1;
    ca->isreg = 1;
    ca->owner = source;

    cb->dontread = 1;
    cb->isreg = 1;
    cb->owner = source;

    connection_fsm(ca,CE_CLIENT_CONNECTED);
    connection_fsm(cb,CE_CLIENT_CONNECTED);

    node_notify(n);
  }
  endpoint_send_locked(endpt,source,MSG_CONNPAIR_RESPONSE,&crm,sizeof(crm));
}

static void iothread_read(node *n, endpoint *endpt, read_msg *m, endpointid source)
{
  connection *conn;

  /* If the connection doesn't exist any more, ignore the request - the caller is about to
     receive a CONNECTION_CLOSED MESSAGE */
  if (NULL != (conn = connhash_lookup(n,m->sockid))) {
    if (CS_ACCEPTED_DONE_READING == conn->state)
      fatal("READ request for socket that has finished reading");
    assert(!conn->collected);
    assert(conn->dontread);
    assert(0 == conn->frameids[READ_FRAMEADDR]);

    if (!endpointid_equals(&conn->owner,&source)) {
      conn->owner = source;
      endpoint_link_locked(endpt,conn->owner);
    }

    conn->frameids[READ_FRAMEADDR] = m->ioid;
    conn->dontread = 0;
    node_notify(n);
  }
}

/* FIXME: we should allow multiple WRITE messages to be sent without the caller having to first
   process the WRITE_RESPONSE. This is useful for things like the echo test, where if it supplies
   an ioid (which is irrelevant in this case) greater than 0, we can sometimes get crashes.

   Ideally we should just remove the whole ioid thing altogether, and send the sockid back
   in the WRITE_RESPONSE message instead of the ioid. The code in builtins.c should be
   modified to associate a {sockid,operation} combination with a suspended frame, instead of an
   ioid. */
static void iothread_write(node *n, endpoint *endpt, write_msg *m, endpointid source)
{
  connection *conn;
  int spaceleft = 0;

  /* If the connection doesn't exist any more, ignore the request - the caller is about to
     receive a CONNECTION_CLOSED MESSAGE */
  if (NULL != (conn = connhash_lookup(n,m->sockid))) {
    assert(!conn->collected);
    assert(NULL != conn->sendbuf);

    if (!endpointid_equals(&conn->owner,&source)) {
      conn->owner = source;
      endpoint_link_locked(endpt,conn->owner);
    }

    if (0 < m->len)
      array_append(conn->sendbuf,m->data,m->len);
    else
      conn->finwrite = 1;

    assert(0 == conn->frameids[WRITE_FRAMEADDR]);

    /* The calling frame will block when it sends us the WRITE message. If the write buffer
       still has room left for more data, wake it up immediately. Otherwise, keep it blocked
       until some of the data has been written. */
    if (n->iosize > conn->sendbuf->nbytes)
      spaceleft = 1;
    else
      conn->frameids[WRITE_FRAMEADDR] = m->ioid;

    node_notify(n);
  }

  if (spaceleft || (0 == m->len)) {
    write_response_msg wrm;
    wrm.ioid = m->ioid;
    endpoint_send_locked(endpt,source,MSG_WRITE_RESPONSE,&wrm,sizeof(wrm));
  }
}

static void iothread_delete_connection(node *n, endpoint *endpt,
                                      delete_connection_msg *m, endpointid source)
{
  connection *conn;

  if (NULL != (conn = connhash_lookup(n,m->sockid))) {
    conn->collected = 1;
    if (!conn->finwrite) {
      conn->finwrite = 1;
      node_notify(n);
    }
    connection_fsm(conn,CE_FINREAD);
  }
}

static void iothread_delete_listener(node *n, endpoint *endpt,
                                    delete_listener_msg *m, endpointid source)
{
  listener *l;

  l = get_listener(n,m->sockid);
  if (NULL != l)
    node_remove_listener(n,l);
}

static void iothread_handle_message(node *n, endpoint *endpt, message *msg)
{
  switch (msg->tag) {
  case MSG_ENDPOINT_EXIT:
    assert(sizeof(endpoint_exit_msg) == msg->size);
    node_handle_endpoint_exit(n,(endpoint_exit_msg*)msg->data);
    break;
  case MSG_LISTEN:
    assert(sizeof(listen_msg) == msg->size);
    iothread_listen(n,endpt,(listen_msg*)msg->data,msg->source);
    break;
  case MSG_ACCEPT:
    assert(sizeof(accept_msg) == msg->size);
    iothread_accept(n,endpt,(accept_msg*)msg->data,msg->source);
    break;
  case MSG_CONNECT:
    assert(sizeof(connect_msg) == msg->size);
    iothread_connect(n,endpt,(connect_msg*)msg->data,msg->source);
    break;
  case MSG_CONNPAIR:
    assert(sizeof(connpair_msg) <= msg->size);
    iothread_connpair(n,endpt,(connpair_msg*)msg->data,msg->source);
    break;
  case MSG_READ:
    assert(sizeof(read_msg) == msg->size);
    iothread_read(n,endpt,(read_msg*)msg->data,msg->source);
    break;
  case MSG_WRITE:
    assert(sizeof(write_msg) <= msg->size);
    iothread_write(n,endpt,(write_msg*)msg->data,msg->source);
    break;
  case MSG_DELETE_CONNECTION:
    assert(sizeof(delete_connection_msg) == msg->size);
    iothread_delete_connection(n,endpt,(delete_connection_msg*)msg->data,msg->source);
    break;
  case MSG_DELETE_LISTENER:
    assert(sizeof(delete_listener_msg) == msg->size);
    iothread_delete_listener(n,endpt,(delete_listener_msg*)msg->data,msg->source);
    break;
  default:
    fatal("iothread: received unknown message %d",msg->tag);
    break;
  }
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
      conn->errn = errno;
      snprintf(conn->errmsg,ERRMSG_MAX,"%s",strerror(errno));
      conn->errmsg[ERRMSG_MAX] = '\0';
      connection_fsm(conn,CE_ERROR);
      return;
    }

    if (0 == w) {
      node_log(n,LOG_WARNING,"write() to %s:%d failed: connection closed",
               conn->hostname,conn->port);
      snprintf(conn->errmsg,ERRMSG_MAX,"connection closed");
      connection_fsm(conn,CE_ERROR);
      return;
    }

    conn->totalwritten += w;
    array_remove_data(conn->sendbuf,w);
  }

  notify_write(n,conn);

  if ((0 == conn->sendbuf->nbytes) && conn->finwrite)
    connection_fsm(conn,CE_FINWRITE);
}

void handle_disconnection(node *n, connection *conn)
{
  assert(NODE_ALREADY_LOCKED(n));

  if (conn->isreg)
    return;

  /* A connection to another node has failed. Search for links that referenced endpoints
     on the other node, and sent an ENDPOINT_EXIT to the local endpoint in each case. */
  endpoint *endpt;
  for (endpt = n->p->endpoints.first; endpt; endpt = endpt->next) {
    int max = list_count(endpt->p->outlinks);
    endpointid *exited = (endpointid*)malloc(max*sizeof(endpointid));
    int count = 0;
    list *l;
    int i;

    /* We must grab the list of exited endpoints before calling node_send_locked(), because it
       will call through to endpoint_add_message() which will modify the endpoint list to remove
       the links, so we can't do this while traversing the list. */
    for (l = endpt->p->outlinks; l; l = l->next) {
      endpointid *epid = (endpointid*)l->data;
      if ((epid->ip == conn->ip) && (epid->port == conn->port))
        exited[count++] = *epid;
    }

    /* Now we've found the relevant endpoints, send ENDPOINT_EXIT for each */
    for (i = 0; i < count; i++) {
      endpoint_exit_msg msg;
      msg.epid = exited[i];
      msg.reason = 0;
      node_send_locked(n,endpt->epid.localid,endpt->epid,MSG_ENDPOINT_EXIT,
                       &msg,sizeof(endpoint_exit_msg));
    }

    free(exited);
  }
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
    endpointid source;

    source.ip = conn->ip;
    source.port = conn->port;
    source.localid = hdr->sourcelocalid;

    if (MSG_HEADER_SIZE+hdr->size1 > conn->recvbuf->nbytes-start)
      break; /* incomplete message */

    /* complete message present; add it to the mailbox */
    got_message(n,hdr,source,hdr->tag1,hdr->size1,&conn->recvbuf->data[start+MSG_HEADER_SIZE]);
    start += MSG_HEADER_SIZE+hdr->size1;
  }

  /* remove the data we've consumed from the receive buffer */
  array_remove_data(conn->recvbuf,start);
}

static void handle_read(node *n, connection *conn)
{
  /* Just do one read; there may still be more data pending after this, but we want to also
     give a fair chance for other connections to be read from. We also want to process messages
     as they arrive rather than waiting until can't ready an more, to avoid buffering up large
     numbers of messages */
  int r;
  array *buf = conn->recvbuf;

  assert(CANREAD(conn));

  array_mkroom(conn->recvbuf,n->iosize);
  r = TEMP_FAILURE_RETRY(read(conn->sock,
                              &buf->data[buf->nbytes],
                              n->iosize));

  if (0 > r) {
    node_log(n,LOG_WARNING,"read() from %s:%d failed: %s",
             conn->hostname,conn->port,strerror(errno));
    conn->errn = errno;
    snprintf(conn->errmsg,ERRMSG_MAX,"%s",strerror(errno));
    conn->errmsg[ERRMSG_MAX] = '\0';
    connection_fsm(conn,CE_ERROR);
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
      connection_fsm(conn,CE_FINWRITE);
    connection_fsm(conn,CE_FINREAD);
    return;
  }

  connection_fsm(conn,CE_READ);

  conn->recvbuf->nbytes += r;
  conn->totalread += r;
  process_received(n,conn);
}

static int handle_connected(node *n, connection *conn)
{
  int err;
  socklen_t optlen = sizeof(int);

  if (0 > getsockopt(conn->sock,SOL_SOCKET,SO_ERROR,&err,&optlen)) {
    conn->errn = errno;
    snprintf(conn->errmsg,ERRMSG_MAX,"getsockopt SO_ERROR: %s",strerror(errno));
    conn->errmsg[ERRMSG_MAX] = '\0';
    connection_fsm(conn,CE_ASYNC_FAILED);
    return -1;
  }

  if (0 == err) {
    int yes = 1;
    node_log(n,LOG_INFO,"Connected to %s:%d",conn->hostname,conn->port);

    if (0 > setsockopt(conn->sock,IPPROTO_TCP,TCP_NODELAY,&yes,sizeof(int))) {
      conn->errn = errno;
      snprintf(conn->errmsg,ERRMSG_MAX,"setsockopt TCP_NODELAY: %s",strerror(errno));
      conn->errmsg[ERRMSG_MAX] = '\0';
      connection_fsm(conn,CE_ASYNC_FAILED);
      return -1;
    }

    connection_fsm(conn,CE_ASYNC_OK);
  }
  else {
    node_log(n,LOG_WARNING,"Connection to %s:%d failed: %s",
             conn->hostname,conn->port,strerror(err));
    conn->errn = err;
    snprintf(conn->errmsg,ERRMSG_MAX,"%s",strerror(err));
    conn->errmsg[ERRMSG_MAX] = '\0';
    connection_fsm(conn,CE_ASYNC_FAILED);
    return -1;
  }

  return 0;
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

  hostname = lookup_hostname(remote_addr.sin_addr.s_addr);
  node_log(n,LOG_INFO,"Got connection from %s",hostname);
  conn = add_connection(n,hostname,remote_addr.sin_addr.s_addr,l);
  conn->sock = clientfd;
  free(hostname);

  conn->ip = remote_addr.sin_addr.s_addr;

  connection_fsm(conn,CE_CLIENT_CONNECTED);

  notify_accept(n,conn);
}

static void ioloop(node *n, endpoint *endpt, void *arg)
{
  lock_mutex(&n->p->lock);
  while (!n->p->shutdown) {
    int highest = -1;
    int s;
    fd_set readfds;
    fd_set writefds;
    connection *conn;
    connection *next;
    listener *l;

    FD_ZERO(&readfds);
    FD_ZERO(&writefds);

    for (l = n->p->listeners.first; l; l = l->next) {
      if (!l->dontaccept)
        FD_SET(l->fd,&readfds);
      if (highest < l->fd)
        highest = l->fd;
    }

    FD_SET(n->p->ioready_readfd,&readfds);
    if (highest < n->p->ioready_readfd)
      highest = n->p->ioready_readfd;

    for (conn = n->p->connections.first; conn; conn = conn->next) {
      assert(!conn->iswaiting);
      if (0 > conn->sock)
        continue;
      if (highest < conn->sock)
        highest = conn->sock;

      /* FIXME: we should also test for readability if we have yet to read any data at all over
         the connection, so we can tell when it has been accepted */
      if (CANREAD(conn) && !conn->dontread)
        FD_SET(conn->sock,&readfds);

      if (CANWRITE(conn) && ((0 < conn->sendbuf->nbytes) || conn->finwrite))
        FD_SET(conn->sock,&writefds);
      else if (CS_CONNECTING == conn->state)
        FD_SET(conn->sock,&writefds);

      /* Note: it is possible that we did not find any data waiting to be written at this point,
         but some will become avaliable either just after we release the lock, or while the select
         is actually blocked. node_notify() writes a single byte of data to the node's ioready pipe,
         which will cause the select to be woken, and we will look around again to write the
         data. */
    }

    unlock_mutex(&n->p->lock);
    s = select(highest+1,&readfds,&writefds,NULL,NULL);
    lock_mutex(&n->p->lock);

    if (0 > s) {
      /* Ignore bad file descriptor error. It is possible (and legitimate) that a connection
         could be closed by another thread while we are waiting in the select loop. */
      if (EBADF != errno)
        fatal("select: %s",strerror(errno));
    }

    assert(!FD_ISSET(n->p->ioready_readfd,&readfds) || n->p->notified);

    if (FD_ISSET(n->p->ioready_readfd,&readfds) || n->p->notified) {
      int c;
      if (0 > read(n->p->ioready_readfd,&c,1))
        fatal("Can't read from ioready_readfd: %s",strerror(errno));
      n->p->notified = 0;
    }

    if (0 == s)
      continue; /* timed out; no sockets are ready for reading or writing */

    /* Process messages */
    if (endpt->interrupt) {
      message *msg;
      endpt->interrupt = 0;
      while (NULL != (msg = endpoint_receive(endpt,0))) {
        iothread_handle_message(n,endpt,msg);
        message_free(msg);
      }
    }

    /* Do all the writing we can */
    for (conn = n->p->connections.first; conn; conn = next) {
      next = conn->next;
      if ((0 <= conn->sock) && FD_ISSET(conn->sock,&writefds)) {
        if (CS_CONNECTING == conn->state)
          handle_connected(n,conn);
        else
          handle_write(n,conn);
      }
    }

    /* Read data */
    for (conn = n->p->connections.first; conn; conn = next) {
      next = conn->next;
      if ((0 <= conn->sock) && FD_ISSET(conn->sock,&readfds) && CANREAD(conn))
        handle_read(n,conn);
    }

    /* accept new connections */
    for (l = n->p->listeners.first; l; l = l->next) {
      if (FD_ISSET(l->fd,&readfds))
        handle_new_connection(n,l);
    }

    /* Handle pending close requests - this is done here to avoid closing an fd while select()
      is looking at it */
    node_close_pending(n);
  }
  unlock_mutex(&n->p->lock);
}

void node_start_iothread(node *n)
{
  node_add_thread2(n,"io",ioloop,NULL,&n->p->iothread,IO_ID,0);
}
