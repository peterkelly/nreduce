/*
 * This file is part of the GridXSLT project
 * Copyright (C) 2005 Peter Kelly (pmk@cs.adelaide.edu.au)
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
 */

#define _HTTP_HTTP_C

#include "HTTP.h"
#include "Message.h"
#include "Handlers.h"
#include "util/List.h"
#include "util/Debug.h"
#include "util/Network.h"
#include "util/String.h"
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>
#include <errno.h>
#include <netdb.h>
#include <stdarg.h>
#include <ctype.h>
#include <dirent.h>
#include <time.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/time.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <netinet/tcp.h>
#include <signal.h>
#include <libxml/uri.h>

using namespace GridXSLT;

const char *list_headers[8] = {
  "Connection",
  "Via",
  "Accept",
  "Accept-Charset",
  "Accept-Encoding",
  "Accept-Language",
  "TE",
  NULL
};

struct rctext response_codes[41] = {
  { HTTP_CONTINUE,                          "Continue" },
  { HTTP_SWITCHING_PROTOCOLS,               "Switching Protocols" },
  { HTTP_OK,                                "OK" },
  { HTTP_CREATED,                           "Created" },
  { HTTP_ACCEPTED,                          "Accepted" },
  { HTTP_NON_AUTHORITATIVE_INFORMATION,     "Non-Authoritative Information" },
  { HTTP_NO_CONTENT,                        "No Content" },
  { HTTP_RESET_CONTENT,                     "Reset Content" },
  { HTTP_PARTIAL_CONTENT,                   "Partial Content" },
  { HTTP_MULTIPLE_CHOICES,                  "Multiple Choices" },
  { HTTP_MOVED_PERMANENTLY,                 "Moved Permanently" },
  { HTTP_FOUND,                             "Found" },
  { HTTP_SEE_OTHER,                         "See Other" },
  { HTTP_NOT_MODIFIED,                      "Not Modified" },
  { HTTP_USE_PROXY,                         "Use Proxy" },
  { HTTP_UNUSED,                            "(Unused)" },
  { HTTP_TEMPORARY_REDIRECT,                "Temporary Redirect" },
  { HTTP_BAD_REQUEST,                       "Bad Request" },
  { HTTP_UNAUTHORIZED,                      "Unauthorized" },
  { HTTP_PAYMENT_REQUIRED,                  "Payment Required" },
  { HTTP_FORBIDDEN,                         "Forbidden" },
  { HTTP_NOT_FOUND,                         "Not Found" },
  { HTTP_METHOD_NOT_ALLOWED,                "Method Not Allowed" },
  { HTTP_NOT_ACCEPTABLE,                    "Not Acceptable" },
  { HTTP_PROXY_AUTHENTICATION_REQUIRED,     "Proxy Authentication Required" },
  { HTTP_REQUEST_TIMEOUT,                   "Request Timeout" },
  { HTTP_CONFLICT,                          "Conflict" },
  { HTTP_GONE,                              "Gone" },
  { HTTP_LENGTH_REQUIRED,                   "Length Required" },
  { HTTP_PRECONDITION_FAILED,               "Precondition Failed" },
  { HTTP_REQUEST_ENTITY_TOO_LARGE,          "Request Entity Too Large" },
  { HTTP_REQUEST_URI_TOO_LONG,              "Request-URI Too Long" },
  { HTTP_UNSUPPORTED_MEDIA_TYPE,            "Unsupported Media Type" },
  { HTTP_REQUESTED_RANGE_NOT_SATISFIABLE,   "Requested Range Not Satisfiable" },
  { HTTP_EXPECTATION_FAILED,                "Expectation Failed" },
  { HTTP_INTERNAL_SERVER_ERROR,             "Internal Server Error" },
  { HTTP_NOT_IMPLEMENTED,                   "Not Implemented" },
  { HTTP_BAD_GATEWAY,                       "Bad Gateway" },
  { HTTP_SERVICE_UNAVAILABLE,               "Service Unavailable" },
  { HTTP_GATEWAY_TIMEOUT,                   "Gateway Timeout" },
  { HTTP_HTTP_VERSION_NOT_SUPPORTED,        "HTTP Version Not Supported" }
};

typedef struct response response;
typedef struct connection connection;

typedef int (*connection_reader)(connection *c, const char *buf, int size);
typedef int (*connection_writer)(connection *c);
typedef void (*connection_cleanup)(connection *c);

struct response {
  msgwriter *mw;
  connection_reader reader;
  connection_writer writer;
  connection_cleanup cleanup;
  void *hd;
  urihandler *uh;
};

struct connection {
  struct sockaddr_in remote_addr;
  eventman *em;

  msgparser req;
  response resp;

  int httpversion;

  fdhandler *handler;
};




int server_request(msghandler *mh, const char *method, const char *uri, const char *version);
int server_headers(msghandler *mh, list *hl);
int server_data(msghandler *mh, const char *buf, int size);
int server_end(msghandler *mh);
int server_error(msghandler *mh, int rc, const char *msg);






void connection_init_msghandler(connection *c)
{
  c->req.handler.request = server_request;
  c->req.handler.headers = server_headers;
  c->req.handler.data = server_data;
  c->req.handler.end = server_end;
  c->req.handler.d = c;
}

void connection_reset(connection *c)
{
  msgparser_reset(&c->req);
  if (NULL != c->resp.mw)
    msgwriter_free(c->resp.mw);

  if (NULL != c->resp.uh) {
    if (NULL != c->resp.uh->cleanup)
      c->resp.uh->cleanup(c->resp.uh);
    free(c->resp.uh);
  }

  memset(&c->resp,0,sizeof(response));
  c->httpversion = 0;
  c->resp.mw = msgwriter_new(c->handler->fd);
  connection_init_msghandler(c);
}

connection *connection_new()
{
  connection *c = (connection*)calloc(1,sizeof(connection));
  c->req.cb = circbuf_new();
  connection_init_msghandler(c);
  return c;
}

void connection_free(connection *c)
{
  if (NULL != c->resp.uh) {
    if (NULL != c->resp.uh->cleanup)
      c->resp.uh->cleanup(c->resp.uh);
    free(c->resp.uh);
  }

  list_free(c->req.headers,(list_d_t)header_free);
  if (NULL != c->req.cb)
    circbuf_free(c->req.cb);
  if (NULL != c->resp.mw)
    msgwriter_free(c->resp.mw);
  free(c);
}

/******************************************************************************/
/* URL handlers */




/******************************************************************************/
/* Request processing */










int server_request(msghandler *mh, const char *method, const char *uri, const char *version)
{
  connection *c = (connection*)mh->d;
  debug("Method = \"%s\"\n",method);
  debug("Request URI = \"%s\"\n",uri);
  debug("Version = \"%s\"\n",version);

  if (!strcasecmp(version,"HTTP/1.0"))
    c->httpversion = HTTP_VERSION_1_0;
  else
    c->httpversion = HTTP_VERSION_1_1;
  return 0;
}

int server_headers(msghandler *mh, list *hl)
{
  connection *c = (connection*)mh->d;
  char *connhdr;
  struct stat statbuf;
  char *filename;
  char *uri;
  int tree = 0;

  debug("Finished headers\n");
  c->handler->reading = 0;
  c->handler->writing = 1;

  if (NULL != (connhdr = msgparser_get_header(&c->req,"Connection"))) {
    /* FIXME: Connection may contain multiple tokens, need to treat as a list */
    if (!strcasecmp(connhdr,"Keep-Alive")) {
      c->resp.mw->keepalive = 1;
    }
    else if (!strcasecmp(connhdr,"Close")) {
      c->resp.mw->close = 1;
    }
  }

  if (1 < msgparser_count_header(&c->req,"Host")) {
    error_response(c->resp.mw,400,"Duplicate \"host\" header supplied");
    return 0;
  }

  if (NULL == msgparser_get_header(&c->req,"Host")) {
    error_response(c->resp.mw,400,"No \"host\" header supplied");
    return 0;
  }


  if (!strncmp(c->req.uri,"/tree/",6)) {
    uri = c->req.uri+5;
    tree = 1;
  }
  else {
    uri = c->req.uri;
  }

  filename = xmlURIUnescapeString(uri,strlen(uri),NULL);

  if (0 != stat(filename,&statbuf)) {
    message("Not found: %s (%s)\n",filename,strerror(errno));
    error_response(c->resp.mw,404,"The url %s was not found on this server",filename);
  }
  else {
    if (S_ISDIR(statbuf.st_mode)) {
      if (tree)
        c->resp.uh = handle_tree(filename,c->resp.mw);
      else
        c->resp.uh = handle_dir(filename,c->resp.mw);
    }
    else if (S_ISREG(statbuf.st_mode)) {
      c->resp.uh = handle_file(filename,c->resp.mw,statbuf.st_size);
    }
    else {
      error_response(c->resp.mw,500,"Unknown file type");
    }
  }
  free(filename);



  return 0;
}

int server_data(msghandler *mh, const char *buf, int size)
{
/*   connection *c = (connection*)mh->d; */
  return 0;
}

int server_end(msghandler *mh)
{
/*   connection *c = (connection*)mh->d; */
  return 0;
}

int server_error(msghandler *mh, int rc, const char *msg)
{
  connection *c = (connection*)mh->d;
  error_response(c->resp.mw,rc,"%s",msg);
  return 0;
}

/******************************************************************************/
/* Connection management and I/O */

int http_read(eventman *em, fdhandler *h)
{
  connection *c = (connection*)h->data;
  return msgparser_readavail(&c->req,h->fd);
}

int http_write(eventman *em, fdhandler *h)
{
  connection *c = (connection*)h->data;

  if (0 != c->resp.mw->write_error) {
    message("Error writing to client: %s\n",strerror(c->resp.mw->write_error));
    return 1;
  }

  if (msgwriter_done(c->resp.mw)) {
    connection_reset(c);
    h->reading = 1;
    h->writing = 0;
    return 0;
  }

  if (msgwriter_writepending(c->resp.mw))
    return 0;

  if (!c->resp.mw->finished) {
    ASSERT(NULL != c->resp.uh);
    if (!c->resp.uh->write(c->resp.uh,c->resp.mw))
      msgwriter_end(c->resp.mw);
  }

  return 0;
}

void http_cleanup(eventman *em, fdhandler *h)
{
  connection *c = (connection*)h->data;
  connection_free(c);
}

int listen_read(eventman *em, fdhandler *h)
{
  connection *c = connection_new();
  int sin_size = sizeof(struct sockaddr_in);
  int serverfd;
  fdhandler *httphandler;
  c->em = em;
  if (-1 == (serverfd = accept(h->fd,(struct sockaddr*)&c->remote_addr,(socklen_t*)&sin_size))) {
    perror("accept");
    return -1;
  }
  c->resp.mw = msgwriter_new(serverfd);
  fcntl(serverfd,F_SETFL,fcntl(serverfd,F_GETFL)|O_NONBLOCK);

  message("Accepted connection\n");

  httphandler = (fdhandler*)calloc(1,sizeof(fdhandler));
  httphandler->fd = serverfd;
  httphandler->reading = 1;
  httphandler->readhandler = http_read;
  httphandler->writehandler = http_write;
  httphandler->cleanup = http_cleanup;
  httphandler->data = c;
  eventman_add_handler(em,httphandler);

  c->handler = httphandler;

  return 0;
}

int stdin_read(eventman *em, fdhandler *h)
{
  em->stopped = 1;
  return 0;
}

int http_main(const char *docroot)
{
  int listenfd;
  fdhandler *listenhandler;
  fdhandler *stdinhandler;
  eventman em;

  setbuf(stdout,NULL);
  signal(SIGPIPE,SIG_IGN);

  if (0 > (listenfd = start_server(PORT)))
    return -1;

  memset(&em,0,sizeof(eventman));

  listenhandler = (fdhandler*)calloc(1,sizeof(fdhandler));
  listenhandler->fd = listenfd;
  listenhandler->reading = 1;
  listenhandler->readhandler = listen_read;
  eventman_add_handler(&em,listenhandler);

  stdinhandler = (fdhandler*)calloc(1,sizeof(fdhandler));
  stdinhandler->fd = STDIN_FILENO;
  stdinhandler->reading = 1;
  stdinhandler->readhandler = stdin_read;
  eventman_add_handler(&em,stdinhandler);


  message("Listening on port %d, press a key to exit...\n",PORT);

  eventman_loop(&em);
  eventman_clear(&em);

  return 0;
}
