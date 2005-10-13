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

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <signal.h>
#include <errno.h>
#include <unistd.h>
#include "util/network.h"
#include "http/http.h"
#include "http/message.h"

typedef struct client_data client_data;

struct client_data {
  msgwriter *mw;
};

int client_read(eventman *em, fdhandler *h)
{
  char buf[1025];
  int r = read(h->fd,buf,1024);
  if ((0 > r) && (EAGAIN == errno)) {
    return 0;
  }
  else if (0 > r) {
    fprintf(stderr,"Error reading from server: %s\n",strerror(errno));
    return 1;
  }
  else {
    buf[r] = '\0';
    printf("%s",buf);
  }
  return (0 == r);
}

int client_write(eventman *em, fdhandler *h)
{
  client_data *cd = (client_data*)h->data;

  if (0 != cd->mw->write_error) {
    printf("Error writing to server: %s\n",strerror(cd->mw->write_error));
    return 1;
  }

  if (msgwriter_done(cd->mw)) {
    h->reading = 1;
    h->writing = 0;
    return 0;
  }

  msgwriter_writepending(cd->mw);

  return 0;
}

void client_cleanup(eventman *em, fdhandler *h)
{
  client_data *cd = (client_data*)h->data;
  msgwriter_free(cd->mw);
}



/*
int client_response(msghandler *mr, int rc, const char *version)
{
  return 0;
}

int client_headers(msghandler *mr, list *hl)
{
  return 0;
}

int client_data(msghandler *mr, const char *buf, int size)
{
  return 0;
}

int client_end(msghandler *mr)
{
  return 0;
}
*/




int main(int argc, char **argv)
{
  fdhandler *client_handler;
  int fd;
  eventman em;
  client_data cd;
  msghandler mh;

  setbuf(stdout,NULL);
  signal(SIGPIPE,SIG_IGN);
  memset(&em,0,sizeof(eventman));
  memset(&mh,0,sizeof(msghandler));

  if (0 > (fd = connect_host("localhost",80)))
    exit(1);

  cd.mw = msgwriter_new(fd);

  msgwriter_request(cd.mw,"GET","/",0);
  msgwriter_header(cd.mw,"Host","localhost");
  msgwriter_end(cd.mw);

  client_handler = (fdhandler*)calloc(1,sizeof(fdhandler));
  client_handler->fd = fd;
  client_handler->writing = 1;
  client_handler->readhandler = client_read;
  client_handler->writehandler = client_write;
  client_handler->cleanup = client_cleanup;
  client_handler->data = &cd;
  eventman_add_handler(&em,client_handler);

  printf("Waiting\n");
  eventman_loop(&em);
  eventman_clear(&em);

  return 0;
}
