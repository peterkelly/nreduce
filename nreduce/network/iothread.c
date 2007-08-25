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

static connection *get_connection(node *n, socketid id)
{
  connection *conn;
  assert(NODE_ALREADY_LOCKED(n));
  for (conn = n->p->connections.first; conn; conn = conn->next)
    if (socketid_equals(&conn->sockid,&id))
      return conn;
  return NULL;
}

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
  connect_response_msg crm;

  endpoint_link_locked(endpt,m->owner);

  conn = node_connect_locked(n,m->hostname,INADDR_ANY,m->port,0,crm.errmsg,ERRMSG_MAX);
  if (conn) {
    assert(0 == conn->frameids[CONNECT_FRAMEADDR]);
    conn->frameids[CONNECT_FRAMEADDR] = m->ioid;
    conn->dontread = 1;
    conn->owner = m->owner;
  }

  if (NULL == conn) {
    crm.errmsg[ERRMSG_MAX] = '\0';
    crm.ioid = m->ioid;
    crm.error = 1;
    memset(&crm.sockid,0,sizeof(crm.sockid));
    endpoint_send_locked(endpt,source,MSG_CONNECT_RESPONSE,&crm,sizeof(crm));
  }
}

static void iothread_read(node *n, endpoint *endpt, read_msg *m)
{
  connection *conn;

  /* If the connection doesn't exist any more, ignore the request - the caller is about to
     receive a CONNECTION_CLOSED MESSAGE */
  if (NULL != (conn = get_connection(n,m->sockid))) {
    if (!conn->canread)
      fatal("READ request for socket that has finished reading");
    assert(!conn->collected);
    assert(conn->dontread);
    assert(0 == conn->frameids[READ_FRAMEADDR]);

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
  if (NULL != (conn = get_connection(n,m->sockid))) {
    assert(!conn->collected);

    array_append(conn->sendbuf,m->data,m->len);

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

  if (spaceleft) {
    write_response_msg wrm;
    wrm.ioid = m->ioid;
    endpoint_send_locked(endpt,source,MSG_WRITE_RESPONSE,&wrm,sizeof(wrm));
  }
}

static void iothread_finwrite(node *n, endpoint *endpt, finwrite_msg *m, endpointid source)
{
  connection *conn;

  /* If the connection doesn't exist any more, ignore the request - the caller is about to
     receive a CONNECTION_CLOSED MESSAGE */
  conn = get_connection(n,m->sockid);
  if (conn) {
    assert(!conn->collected);
    conn->finwrite = 1;
    node_notify(n);
  }

  if (conn) {
    finwrite_response_msg frm;
    frm.ioid = m->ioid;
    endpoint_send_locked(endpt,source,MSG_FINWRITE_RESPONSE,&frm,sizeof(frm));
  }
}

static void iothread_delete_connection(node *n, endpoint *endpt,
                                      delete_connection_msg *m, endpointid source)
{
  connection *conn;

  if (NULL != (conn = get_connection(n,m->sockid))) {
    conn->collected = 1;
    if (!conn->finwrite) {
      conn->finwrite = 1;
      node_notify(n);
    }
    done_reading(n,conn);
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
  switch (msg->hdr.tag) {
  case MSG_ENDPOINT_EXIT:
    assert(sizeof(endpoint_exit_msg) == msg->hdr.size);
    node_handle_endpoint_exit(n,((endpoint_exit_msg*)msg->data)->epid);
    break;
  case MSG_LISTEN:
    assert(sizeof(listen_msg) == msg->hdr.size);
    iothread_listen(n,endpt,(listen_msg*)msg->data,msg->hdr.source);
    break;
  case MSG_ACCEPT:
    assert(sizeof(accept_msg) == msg->hdr.size);
    iothread_accept(n,endpt,(accept_msg*)msg->data,msg->hdr.source);
    break;
  case MSG_CONNECT:
    assert(sizeof(connect_msg) == msg->hdr.size);
    iothread_connect(n,endpt,(connect_msg*)msg->data,msg->hdr.source);
    break;
  case MSG_READ:
    assert(sizeof(read_msg) == msg->hdr.size);
    iothread_read(n,endpt,(read_msg*)msg->data);
    break;
  case MSG_WRITE:
    assert(sizeof(write_msg) <= msg->hdr.size);
    iothread_write(n,endpt,(write_msg*)msg->data,msg->hdr.source);
    break;
  case MSG_FINWRITE:
    assert(sizeof(finwrite_msg) == msg->hdr.size);
    iothread_finwrite(n,endpt,(finwrite_msg*)msg->data,msg->hdr.source);
    break;
  case MSG_DELETE_CONNECTION:
    assert(sizeof(delete_connection_msg) == msg->hdr.size);
    iothread_delete_connection(n,endpt,(delete_connection_msg*)msg->data,msg->hdr.source);
    break;
  case MSG_DELETE_LISTENER:
    assert(sizeof(delete_listener_msg) == msg->hdr.size);
    iothread_delete_listener(n,endpt,(delete_listener_msg*)msg->data,msg->hdr.source);
    break;
  default:
    fatal("iothread: received unknown message %d",msg->hdr.tag);
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

    /* verify header */
    assert(0 <= hdr->size);
    assert(0 <= hdr->tag);

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

static void handle_read(node *n, connection *conn)
{
  /* Just do one read; there may still be more data pending after this, but we want to also
     give a fair chance for other connections to be read from. We also want to process messages
     as they arrive rather than waiting until can't ready an more, to avoid buffering up large
     numbers of messages */
  int r;
  array *buf = conn->recvbuf;

  assert(conn->canread);

  array_mkroom(conn->recvbuf,n->iosize);
  r = TEMP_FAILURE_RETRY(read(conn->sock,
                              &buf->data[buf->nbytes],
                              n->iosize));

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
  conn = add_connection(n,hostname,clientfd,l);
  free(hostname);

  conn->connected = 1;
  conn->ip = remote_addr.sin_addr.s_addr;

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
    int havewrite = 0;
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
      if (FD_ISSET(conn->sock,&writefds)) {
        if (conn->connected)
          handle_write(n,conn);
        else
          handle_connected(n,conn);
      }
    }

    /* Read data */
    for (conn = n->p->connections.first; conn; conn = next) {
      next = conn->next;
      if (FD_ISSET(conn->sock,&readfds) && conn->canread)
        handle_read(n,conn);
    }

    /* accept new connections */
    for (l = n->p->listeners.first; l; l = l->next) {
      if (FD_ISSET(l->fd,&readfds))
        handle_new_connection(n,l);
    }

    /* Handle pending close requests - this is done here to avoid closing an fd while select()
      is looking at it */
    while (n->p->toclose) {
      int fd = (int)n->p->toclose->data;
      list *next = n->p->toclose->next;
      close(fd);
      free(n->p->toclose);
      n->p->toclose = next;
    }
  }
  unlock_mutex(&n->p->lock);
}

void node_start_iothread(node *n)
{
  node_add_thread2(n,"io",ioloop,NULL,&n->p->iothread,IO_ID,0);
}
