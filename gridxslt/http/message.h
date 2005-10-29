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

#ifndef _HTTP_MESSAGE_H
#define _HTTP_MESSAGE_H

#include "util/List.h"
#include "util/stringbuf.h"

typedef struct msgwriter msgwriter;
typedef struct msghandler msghandler;
typedef struct header header;
typedef struct msgparser msgparser;

struct msgwriter {
  int fd;
  circbuf *cb;
  circbuf *headcb;
  int chunked;
  int bufferall;
  int write_error;
  int content_length;
  int finished;
  int httpversion;
  int inbody;

  int keepalive;
  int close;
};

msgwriter *msgwriter_new(int fd);
void msgwriter_free(msgwriter *mw);

void add_connection_header(msgwriter *mw);
void error_response(msgwriter *mw, int rc, const char *format, ...);


void msgwriter_request(msgwriter *mw, const char *method, const char *uri, int content_length);
void msgwriter_response(msgwriter *mw, int rc, int content_length);
void msgwriter_header(msgwriter *mw, const char *name, const char *value);
void msgwriter_format(msgwriter *mw, const char *format, ...);
void msgwriter_write(msgwriter *mw, const char *data, int size);
void msgwriter_end(msgwriter *mw);

int msgwriter_writepending(msgwriter *mw);
int msgwriter_done(msgwriter *mw);







struct msghandler {
  int (*request)(msghandler *mh, const char *method, const char *uri, const char *version);
  int (*response)(msghandler *mh, int rc, const char *version);
  int (*headers)(msghandler *mh, list *hl);
  int (*data)(msghandler *mh, const char *buf, int size);
  int (*end)(msghandler *mh);
  int (*error)(msghandler *mh, int rc, const char *msg);
  void *d;
};





struct header {
  char *name;
  char *value;
  list *values;
};

#define METHOD_MAXLEN       1024
#define URI_MAXLEN          1024
#define VERSION_MAXLEN      1024
#define HNAME_MAXLEN        1024
#define HVALUE_MAXLEN       16384

struct msgparser {
  char method[METHOD_MAXLEN+1];
  char uri[URI_MAXLEN+1];
  char version[VERSION_MAXLEN+1];

  char hname[HNAME_MAXLEN+1];
  char hvalue[HVALUE_MAXLEN+1];

  int methodlen;
  int urilen;
  int versionlen;
  int hnamelen;
  int hvaluelen;

  int state;

  circbuf *cb;
  list *headers;

  msghandler handler;
};

char *msgparser_get_header(msgparser *r, const char *name);
int msgparser_count_header(msgparser *r, const char *name);

void header_free(header *h);
void msgparser_reset(msgparser *mp);
int msgparser_parse(msgparser *mp, const char *buf, int size);
int msgparser_readavail(msgparser *mp, int fd);

#endif /* _HTTP_MESSAGE_H */
