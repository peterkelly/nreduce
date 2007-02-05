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

static void nodetest_callback(struct node *n, void *data, int event,
                              connection *conn, endpoint *endpt)
{
  switch (event) {
  case EVENT_CONN_ESTABLISHED:
    node_log(n,LOG_INFO,"EVENT_CONN_ESTABLISHED");
    break;
  case EVENT_CONN_FAILED:
    node_log(n,LOG_INFO,"EVENT_CONN_FAILED");
    break;
  case EVENT_CONN_ACCEPTED:
    node_log(n,LOG_INFO,"EVENT_CONN_ACCEPTED");
    break;
  case EVENT_CONN_CLOSED:
    node_log(n,LOG_INFO,"EVENT_CONN_CLOSED");
    break;
  case EVENT_CONN_IOERROR:
    node_log(n,LOG_INFO,"EVENT_CONN_IOERROR");
    break;
  case EVENT_HANDSHAKE_DONE:
    node_log(n,LOG_INFO,"EVENT_HANDSHAKE_DONE");
    break;
  case EVENT_HANDSHAKE_FAILED:
    node_log(n,LOG_INFO,"EVENT_HANDSHAKE_FAILED");
    break;
  case EVENT_SHUTDOWN:
    node_log(n,LOG_INFO,"EVENT_SHUTDOWN");
    break;
  }
}

typedef struct testserver_data {
  listener *l;
} testserver_data;

#define TC_STATE_READHEADER 0
#define TC_STATE_GOTHEADER  1
#define TC_STATE_ERROR      2
#define TC_STATE_OK         3

typedef struct header {
  char *name;
  char *value;
  struct header *next;
} header;

typedef struct testconn {
  int printed;
  int state;
  int ok;
  char *method;
  char *uri;
  char *version;
  header *headers;
  pthread_t thread;
} testconn;

static int end_of_line(const char *data, int len, int *pos)
{
  while (*pos < len) {
    if ((*pos < len) && ('\n' == data[*pos]))
      return 1;
    if (((*pos)+1 < len) && ('\r' == data[*pos]) && ('\n' == data[(*pos)+1]))
      return 1;
    (*pos)++;
  }
  return 0;
}

static void send_error(connection *conn, testconn *tc, int code, const char *str, const char *msg)
{
  cprintf(conn,"HTTP/1.1 %d %s\r\n",code,str);
  cprintf(conn,"Content-Type: text/plain\r\n");
  cprintf(conn,"Content-Length: %d\r\n",strlen(msg)+1);
  cprintf(conn,"Connection: close\r\n");
  cprintf(conn,"\r\n");
  cprintf(conn,"%s\n",msg);
  conn->toclose = 1;
}

#define PS_SRCHNAME     0
#define PS_NAME         1
#define PS_SRCHVALUE    2
#define PS_VALUE        3

static void parse_headers(connection *conn, testconn *tc, char *data, int len, int pos)
{
  int namestart = 0;
  int nameend = 0;
  int valstart = 0;
  int valend = 0;
  int state;

  state = PS_NAME;
  namestart = pos;
  while (pos < len) {
    switch (state) {
    case PS_SRCHNAME: {
      if (!isspace(data[pos])) {
        namestart = pos;
        state = PS_NAME;
      }
      else {
        state = PS_VALUE;
      }
    }
    case PS_NAME:
      if (':' == data[pos]) {
        nameend = pos;
        state = PS_SRCHVALUE;
      }
      break;
    case PS_SRCHVALUE:
      if (!isspace(data[pos])) {
        valstart = pos;
        state = PS_VALUE;
      }
      break;
    case PS_VALUE: {
      int nl = 0;
      if ('\n' == data[pos]) {
        valend = pos;
        nl = 1;
      }
      else if ((pos+1 < len) && ('\r' == data[pos]) && ('\n' == data[pos+1])) {
        valend = pos;
        pos++;
        nl = 1;
      }

      if (nl&& ((pos+1 >= len) || !isspace(data[pos+1]))) {
        char *name = (char*)malloc(nameend-namestart+1);
        char *value = (char*)malloc(valend-valstart+1);
        header *h = (header*)calloc(1,sizeof(header));

        while ((valend-1 >= valstart) && isspace(data[valend-1]))
          valend--;

        memcpy(name,&data[namestart],nameend-namestart);
        name[nameend-namestart] = '\0';

        memcpy(value,&data[valstart],valend-valstart);
        value[valend-valstart] = '\0';

        h->name = name;
        h->value = value;
        h->next = tc->headers;
        tc->headers = h;

        namestart = pos+1;
        state = PS_NAME;
      }
      break;
    }
    }
    pos++;
  }
}

static void parse_request(node *n, connection *conn, testconn *tc, char *data, int len)
{
  int pos = 0;
  if (!end_of_line(data,len,&pos)) {
    tc->state = TC_STATE_ERROR;
    node_log(n,LOG_WARNING,"Invalid request line");
    send_error(conn,tc,400,"Bad Request","Invalid request line");
  }

  if (3 != sscanf(data,"%as %as %as",&tc->method,&tc->uri,&tc->version)) {
    tc->state = TC_STATE_ERROR;
    node_log(n,LOG_WARNING,"Invalid request line");
    send_error(conn,tc,400,"Bad Request","Invalid request line");
  }

  if ((pos < len) && ('\n' == data[pos]))
    pos++;
  else if ((pos+1 < len) && ('\r' == data[pos]) && ('\n' == data[pos+1]))
    pos += 2;

  parse_headers(conn,tc,data,len,pos);
}

/* FIXME: need to handle the case where the connection is closed and this thread is running */
static void *http_thread(void *data)
{
  connection *conn = (connection*)data;
/*   testconn *tc = (testconn*)conn->data; */
  node *n = conn->n;
  int i;

  node_log(n,LOG_INFO,"http_thread starting");

  connection_printf(conn,"HTTP/1.1 200 OK\r\n"
                       "Connection: close\r\n"
                       "Content-Type: text/plain; charset=UTF-8\r\n"
                       "Transfer-Encoding: chunked\r\n"
                       "\r\n"
                       "5\r\n"
                       "test\n"
                       "\r\n");

  for (i = 0; i < 10; i++) {
    node_log(n,LOG_INFO,"i = %02d",i);
    connection_printf(conn,"7\r\ni = %02d\n\r\n",i);
    sleep(1);
  }
  connection_printf(conn,"0\r\n\r\n");
  connection_close(conn);

  node_log(n,LOG_INFO,"http_thread exiting");
  return NULL;
}

static void testserver_callback(struct node *n, void *data, int event,
                                connection *conn, endpoint *endpt)
{
  switch (event) {
  case EVENT_CONN_ACCEPTED: {
    testconn *tc = (testconn*)calloc(1,sizeof(testconn));
    node_log(n,LOG_INFO,"testserver: connection accepted");
    conn->isreg = 1;
    conn->data = tc;
    break;
  }
  case EVENT_CONN_CLOSED:
    node_log(n,LOG_INFO,"testserver: connection closed");
    break;
  case EVENT_CONN_IOERROR:
    node_log(n,LOG_INFO,"testserver: connection error");
    break;
  case EVENT_DATA: {
    testconn *tc = (testconn*)conn->data;
    if (TC_STATE_READHEADER == tc->state) {
      char *data = conn->recvbuf->data;
      int len = conn->recvbuf->nbytes;
      int newlines = 0;
      int pos = 0;

      for (pos = 0; (pos < len) && (2 > newlines); pos++) {
        if ((pos < len) && ('\n' == data[pos])) {
          newlines++;
        }
        else if ((pos+1 < len) && ('\r' == data[pos]) && ('\n' == data[pos+1])) {
          pos++;
          newlines++;
        }
        else {
          newlines = 0;
        }
      }

      assert(pos <= len);

      if (2 == newlines) {
        node_log(n,LOG_INFO,"Got complete header; ends at %d",pos);
        tc->state = TC_STATE_OK;
        parse_request(n,conn,tc,data,pos);

        if (TC_STATE_OK == tc->state) {
          header *h;
          node_log(n,LOG_INFO,"method = \"%s\"",tc->method);
          node_log(n,LOG_INFO,"uri = \"%s\"",tc->uri);
          node_log(n,LOG_INFO,"version = \"%s\"",tc->version);
          for (h = tc->headers; h; h = h->next) {
            node_log(n,LOG_INFO,"Header: %s=\"%s\"",h->name,h->value);
          }

/*           cprintf(conn,"HTTP/1.1 200 OK\r\n" */
/*                        "Content-Type: text/plain\r\n" */
/*                        "Connection: close\r\n" */
/*                        "\r\n" */
/*                        "test\n"); */
/*           conn->toclose = 1; */

          if (0 > wrap_pthread_create(&tc->thread,NULL,http_thread,conn))
            fatal("pthread_create: %s",strerror(errno));

        }
        else {
          node_log(n,LOG_WARNING,"Request had error");
        }
      }
    }
    break;
  }
  }

  if ((EVENT_CONN_CLOSED == event) || (EVENT_CONN_IOERROR == event)) {
    testconn *tc = (testconn*)conn->data;
    while (tc->headers) {
      header *next = tc->headers->next;
      free(tc->headers->name);
      free(tc->headers->value);
      free(tc->headers);
      tc->headers = next;
    }
    free(tc->method);
    free(tc->uri);
    free(tc->version);
    free(tc);
  }
}

int nodetest(const char *host, int port)
{
  node *n = node_new();
  testserver_data tsd;

  memset(&tsd,0,sizeof(testserver_data));

  if (NULL == (n->mainl = node_listen(n,host,port,NULL,NULL)))
    return -1;

  if (NULL == (tsd.l = node_listen(n,host,port+1,testserver_callback,&tsd)))
    return -1;

  node_add_callback(n,nodetest_callback,NULL);
  node_start_iothread(n);


  node_log(n,LOG_INFO,"Blocking main thread");
  if (0 > wrap_pthread_join(n->iothread,NULL))
    fatal("pthread_join: %s",strerror(errno));
  node_log(n,LOG_INFO,"IO thread finished");

  node_close_endpoints(n);
  node_close_connections(n);
  node_remove_callback(n,nodetest_callback,NULL);
  node_remove_listener(n,tsd.l);
  node_free(n);

  return 0;
}
