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

#include "message.h"
#include "http.h"
#include "util/debug.h"
#include "util/network.h"
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <stdlib.h>
#include <assert.h>
#include <ctype.h>

msgwriter *msgwriter_new(int fd)
{
  msgwriter *mw = (msgwriter*)calloc(1,sizeof(msgwriter));
  mw->fd = fd;
  mw->cb = circbuf_new();
  return mw;
}

void msgwriter_free(msgwriter *mw)
{
  circbuf_free(mw->cb);
  if (NULL != mw->headcb)
    circbuf_free(mw->headcb);
  free(mw);
}

void msgwriter_writeraw(msgwriter *mw, const char *data, int size)
{
  if (0 != mw->write_error)
    return;

  if (mw->bufferall || (0 < circbuf_size(mw->cb))) {
    circbuf_append(mw->cb,data,size);
  }
  else {
    int w = write(mw->fd,data,size);
/*     debug("write(%d) returned %d (response_writeraw)\n",size,w); */
    if ((0 > w) && (EAGAIN == errno))
      circbuf_append(mw->cb,data,size);
    else if (0 > w)
      mw->write_error = errno;
    else if (w < size)
      circbuf_append(mw->cb,data+w,size-w);
  }
}

void msgwriter_start(msgwriter *mw, int content_length)
{
  if (0 <= content_length) {
    char tempstr[100];
    sprintf(tempstr,"%d",content_length);
    msgwriter_header(mw,"Content-Length",tempstr);
  }
  else if (HTTP_VERSION_1_1 == mw->httpversion) {
    msgwriter_header(mw,"Transfer-Encoding","chunked");
    mw->chunked = 1;
  }
  else { /* HTTP/1.0 */
    mw->bufferall = 1;
  }
}

void add_connection_header(msgwriter *mw)
{
  if (mw->keepalive)
    msgwriter_header(mw,"Connection","Keep-Alive");
  else if (mw->close)
    msgwriter_header(mw,"Connection","Close");
}

void error_response(msgwriter *mw, int rc, const char *format, ...)
{
  char *text = "";
  int i;
  stringbuf *msg;
  va_list ap;
  for (i = 0; i < rc; i++)
    if (response_codes[i].rc == rc)
      text = response_codes[i].text;

  msgwriter_response(mw,rc,-1);
  add_connection_header(mw);
  msgwriter_header(mw,"Content-Type","text/html");

  msgwriter_format(mw,"<html>\n"
                    "  <head>\n"
                    "    <title>%d %s</title>\n"
                    "  </head>\n"
                    "  <body>\n"
                    "    <h1>%d %s</h1>\n"
                    "    ",rc,text,rc,text);

  msg = stringbuf_new();
  va_start(ap,format);
  stringbuf_vformat(msg,format,ap);
  va_end(ap);
  msgwriter_write(mw,msg->data,msg->size-1);
  stringbuf_free(msg);

  msgwriter_format(mw,"\n"
                    "  </body>\n"
                    "</html>\n");

  msgwriter_end(mw);
}

void msgwriter_request(msgwriter *mw, const char *method, const char *uri,
                             int content_length)
{
  stringbuf *sb = stringbuf_new();
  stringbuf_format(sb,"%s %s %s\r\n",method,uri,HTTP_VERSION);
  msgwriter_writeraw(mw,sb->data,sb->size-1);
  stringbuf_free(sb);
  msgwriter_start(mw,content_length);
}

void msgwriter_response(msgwriter *mw, int rc, int content_length)
{
  char *text = "";
  int i;
  stringbuf *sb;
  mw->content_length = content_length;
  for (i = 0; i < rc; i++)
    if (response_codes[i].rc == rc)
      text = response_codes[i].text;

  sb = stringbuf_new();
  stringbuf_format(sb,"HTTP/%s %d %s\r\n",HTTP_VERSION,rc,text);
  msgwriter_writeraw(mw,sb->data,sb->size-1);
  stringbuf_free(sb);

  msgwriter_start(mw,content_length);
}

void msgwriter_header(msgwriter *mw, const char *name, const char *value)
{
  char *header = (char*)alloca(strlen(name)+2+strlen(value)+3);
  sprintf(header,"%s: %s\r\n",name,value);
  msgwriter_writeraw(mw,header,strlen(name)+2+strlen(value)+2);
}

void msgwriter_startbody(msgwriter *mw)
{
  char *str = "\r\n";

  if (mw->bufferall) {
    mw->headcb = mw->cb;
    mw->cb = circbuf_new();
  }

  msgwriter_writeraw(mw,str,2);

  mw->inbody = 1;
}

void msgwriter_format(msgwriter *mw, const char *format, ...)
{
  va_list ap;
  stringbuf *sb = stringbuf_new();

  va_start(ap,format);
  stringbuf_vformat(sb,format,ap);
  va_end(ap);

  msgwriter_write(mw,sb->data,sb->size-1);

  stringbuf_free(sb);
}

void msgwriter_write(msgwriter *mw, const char *data, int size)
{
  if (!mw->inbody)
    msgwriter_startbody(mw);

  if (mw->chunked) {
    char tempstr[100];
    sprintf(tempstr,"%X\r\n",size);
    msgwriter_writeraw(mw,tempstr,strlen(tempstr));
  }

  msgwriter_writeraw(mw,data,size);

  if (mw->chunked)
    msgwriter_writeraw(mw,"\r\n",2);
}

void msgwriter_end(msgwriter *mw)
{
  if (!mw->inbody)
    msgwriter_startbody(mw);

  if (mw->chunked) {
    char *tempstr = "0\r\n\r\n";
    msgwriter_writeraw(mw,tempstr,5);
  }
  if (mw->bufferall) {
    circbuf *bodycb = mw->cb;
    int content_length = circbuf_size(bodycb)-2;
    char tempstr[100];
    char buf[BUFSIZE+1];
    int r;

    mw->cb = mw->headcb;
    mw->headcb = NULL;

    sprintf(tempstr,"Content-Length: %d\r\n",content_length);
    circbuf_append(mw->cb,tempstr,strlen(tempstr));

    debug("========== appending body ===========\n");
    while (0 < (r = circbuf_read(bodycb,buf,BUFSIZE))) {
      circbuf_append(mw->cb,buf,r);
      buf[r] = '\0';
      debug("%s",buf);
    }
    debug("=====================================\n");

    circbuf_free(bodycb);
  }
  mw->finished = 1;
}

int msgwriter_writepending(msgwriter *mw)
{
  if ((!mw->bufferall || mw->finished) && (0 < circbuf_size(mw->cb))) {
    char buf[BUFSIZE];
    int r = circbuf_get(mw->cb,buf,BUFSIZE);
    int w = write(mw->fd,buf,r);

    if ((0 > w) && (EAGAIN == errno))
      return 1;
    else if (0 > w)
      mw->write_error = errno;
    else
      circbuf_mvstart(mw->cb,w);

    return 1;
  }
  else {
    return 0;
  }
}

int msgwriter_done(msgwriter *mw)
{
  return (mw->finished && (0 == circbuf_size(mw->cb)));
}





char *msgparser_get_header(msgparser *r, const char *name)
{
  list *l;
  for (l = r->headers; l; l = l->next) {
    header *h = (header*)l->data;
    if (!strcasecmp(h->name,name))
      return h->value;
  }
  return NULL;
}

int msgparser_count_header(msgparser *r, const char *name)
{
  list *l;
  int count = 0;
  for (l = r->headers; l; l = l->next) {
    header *h = (header*)l->data;
    if (!strcasecmp(h->name,name))
      count++;
  }
  return count;
}

void header_free(header *h)
{
  free(h->name);
  free(h->value);
  list_free(h->values,free);
  free(h);
}

void msgparser_reset(msgparser *mp)
{
  list_free(mp->headers,(void*)header_free);
  if (NULL != mp->cb)
    circbuf_free(mp->cb);
  memset(mp,0,sizeof(msgparser));
  mp->cb = circbuf_new();
}

#define STATE_METHOD                   0
#define STATE_REQUEST_URI_START        1
#define STATE_REQUEST_URI              2
#define STATE_HTTP_VERSION_START       3
#define STATE_HTTP_VERSION             4
#define STATE_REQLINE_END              5
#define STATE_HEADER_NAME              6
#define STATE_HEADER_FIELD             7
#define STATE_HEADER_FIELD_GOTCR       8
#define STATE_HEADER_FIELD_GOTLF       9
#define STATE_HEADER_SPACELINE_GOTCR   10
#define STATE_BODY                     11

void msgparser_got_reqline(msgparser *mp)
{
  mp->method[mp->methodlen] = '\0';
  mp->uri[mp->urilen] = '\0';
  mp->version[mp->versionlen] = '\0';
  mp->handler.request(&mp->handler,mp->method,mp->uri,mp->version);
}

void msgparser_got_header(msgparser *mp, const char *name, const char *value)
{
  header *h = (header*)calloc(1,sizeof(header));
  h->name = strdup(name);
  h->value = strdup(value);
  list_push(&mp->headers,h);

  /* Note: header names are case-insensitive */
  debug("Header \"%s\" = \"%s\"\n",name,value);
}

int msgparser_parse(msgparser *mp, const char *buf, int size)
{
  const char *s;
  int rc = 0;

  for (s = buf; (s < buf+size) && (0 == rc); s++) {

/*     if (32 <= *s) */
/*       debug("state = %d, char %c (%d), hname \"%s\", hvalue \"%s\"\n",mp->state,*s,*s, */
/*              mp->hname,mp->hvalue); */
/*     else */
/*       debug("state = %d, char #%d (%d), hname \"%s\", hvalue \"%s\"\n",mp->state,*s,*s, */
/*              mp->hname,mp->hvalue); */

    switch (mp->state) {
    case STATE_METHOD:
      if (' ' == *s)
        mp->state = STATE_REQUEST_URI_START;
      else if (('\r' == *s) || ('\n' == *s))
        rc = HTTP_BAD_REQUEST;
      else if (mp->methodlen >= METHOD_MAXLEN)
        rc = HTTP_BAD_REQUEST;
      else
        mp->method[mp->methodlen++] = *s;
      break;
    case STATE_REQUEST_URI_START:
      if (('\r' == *s) || ('\n' == *s)) {
        rc = HTTP_BAD_REQUEST;
      }
      else if (' ' != *s) {
        mp->uri[mp->urilen++] = *s;
        mp->state = STATE_REQUEST_URI;
      }
      break;
    case STATE_REQUEST_URI:
      if (' ' == *s)
        mp->state = STATE_HTTP_VERSION_START;
      else if (('\r' == *s) || ('\n' == *s))
        rc = HTTP_BAD_REQUEST;
      else if (mp->urilen >= URI_MAXLEN)
        rc = HTTP_REQUEST_URI_TOO_LONG;
      else
        mp->uri[mp->urilen++] = *s;
      break;

    case STATE_HTTP_VERSION_START:
      if (('\r' == *s) || ('\n' == *s)) {
        rc = HTTP_BAD_REQUEST;
      }
      else if (' ' != *s) {
        mp->version[mp->versionlen++] = *s;
        mp->state = STATE_HTTP_VERSION;
      }
      break;

    case STATE_HTTP_VERSION:
      if ('\r' == *s) {
        mp->state = STATE_REQLINE_END;
      }
      else if ('\n' == *s) {
        msgparser_got_reqline(mp);
        mp->state = STATE_HEADER_NAME;
      }
      else if (mp->versionlen >= VERSION_MAXLEN) {
        rc = HTTP_BAD_REQUEST;
      }
      else {
        mp->version[mp->versionlen++] = *s;
      }
      break;

    case STATE_REQLINE_END:
      if ('\n' != *s) {
        rc = HTTP_BAD_REQUEST;
      }
      else {
        msgparser_got_reqline(mp);
        mp->state = STATE_HEADER_NAME;
      }
      break;

    case STATE_HEADER_NAME:
      if (':' == *s)
        mp->state = STATE_HEADER_FIELD;
      else if ('\r' == *s)
        mp->state = STATE_HEADER_SPACELINE_GOTCR;
      else if ('\n' == *s) {
        mp->handler.headers(&mp->handler,mp->headers);
        debug("msgparser_parse: returning (1) after reading %d bytes\n",s+1-buf);
        return s+1-buf;
      }
      else if (mp->hnamelen >= HNAME_MAXLEN)
        rc = HTTP_BAD_REQUEST;
      else
        mp->hname[mp->hnamelen++] = *s;
      break;
    case STATE_HEADER_FIELD:
      if ('\r' == *s) {
        mp->state = STATE_HEADER_FIELD_GOTCR;
      }
      else if ('\n' == *s) {
        mp->state = STATE_HEADER_FIELD_GOTLF;
      }
      else if (!isspace(*s) ||
               ((0 < mp->hvaluelen) && (!isspace(mp->hvalue[mp->hvaluelen-1])))) {
        if (mp->hvaluelen >= HVALUE_MAXLEN)
          rc = HTTP_BAD_REQUEST;
        else if (isspace(*s))
          mp->hvalue[mp->hvaluelen++] = ' ';
        else
          mp->hvalue[mp->hvaluelen++] = *s;
      }
      break;
    case STATE_HEADER_FIELD_GOTCR:
      if ('\n' == *s)
        mp->state = STATE_HEADER_FIELD_GOTLF;
      else
        rc = HTTP_BAD_REQUEST;
      break;
    case STATE_HEADER_FIELD_GOTLF: {
      if ((' ' == *s) || ('\t' == *s)) {
        /* field continues */

        if ((0 < mp->hvaluelen) && (!isspace(mp->hvalue[mp->hvaluelen-1]))) {
          if (mp->hvaluelen >= HVALUE_MAXLEN)
            rc = HTTP_BAD_REQUEST;
          else
            mp->hvalue[mp->hvaluelen++] = ' ';
        }
        mp->state = STATE_HEADER_FIELD;
      }
      else {
        if ('\r' == *s) {
          mp->state = STATE_HEADER_SPACELINE_GOTCR;
        }
        else if ('\n' == *s) {
          mp->state = STATE_BODY;
        }

        /* got header */
        while ((0 < mp->hvaluelen) && (' ' == mp->hvalue[mp->hvaluelen-1]))
          mp->hvaluelen--;
        mp->hname[mp->hnamelen] = '\0';
        mp->hvalue[mp->hvaluelen] = '\0';
        msgparser_got_header(mp,mp->hname,mp->hvalue);
        mp->hnamelen = 0;
        mp->hvaluelen = 0;
        memset(mp->hname,0,HNAME_MAXLEN+1); /* temp */
        memset(mp->hvalue,0,HVALUE_MAXLEN+1); /* temp */

        if ('\n' == *s) {
          mp->handler.headers(&mp->handler,mp->headers);
          debug("msgparser_parse: returning (2) after reading %d bytes\n",s+1-buf);
          return s+1-buf;
        }
        else if ('\r' != *s) {
          mp->hname[mp->hnamelen++] = *s;
          mp->state = STATE_HEADER_NAME;
        }
      }

      break;
    }
    case STATE_HEADER_SPACELINE_GOTCR:
      if ('\n' == *s) {
        mp->state = STATE_BODY;
        mp->handler.headers(&mp->handler,mp->headers);
        debug("msgparser_parse: returning (3) after reading %d bytes\n",s+1-buf);
        return s+1-buf;
      }
      else {
        rc = HTTP_BAD_REQUEST;
      }
      break;
    case STATE_BODY:
      break;
    default:
      assert(0);
    }
  }

  if (0 != rc) {
    debug("msgparser_parse: error code %d\n",rc);
    mp->handler.error(&mp->handler,rc,"");
  }

  debug("msgparser_parse: returning (normal) after reading %d bytes\n",s-buf);
  return s-buf;
}

int msgparser_readavail(msgparser *mp, int fd)
{
  char buf[1024];
  int r;
  int r2;
  r = circbuf_suck(mp->cb,fd,BUFSIZE);
  if (0 > r) {
    if (EAGAIN == errno) {
      return 0;
    }
    else {
      fprintf(stderr,"Error reading from client: %s\n",strerror(errno));
      return 1;
    }
  }
  else if (0 == r) {
    fprintf(stderr,"Client closed connection\n");
    return 1;
  }

  while (0 < (r2 = circbuf_get(mp->cb,buf,1024))) {
    int cr = msgparser_parse(mp,buf,r2);
    circbuf_mvstart(mp->cb,cr);
  }
  return 0;
}

