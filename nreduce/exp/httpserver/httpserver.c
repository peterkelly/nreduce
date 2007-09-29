#include "network/util.h"
#include "network/node.h"
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <ctype.h>

#define PS_SRCHNAME     0
#define PS_NAME         1
#define PS_SRCHVALUE    2
#define PS_VALUE        3

typedef struct header {
  char *name;
  char *value;
  struct header *next;
} header;

typedef struct request {
  char *method;
  char *uri;
  char *version;
  header *headers;
} request;

typedef struct httpconn {
  socketid sockid;
  array *readbuf;
  struct httpconn *prev;
  struct httpconn *next;
} httpconn;

typedef struct httpconn_list {
  httpconn *first;
  httpconn *last;
} httpconn_list;

static void free_request(request *req)
{
  free(req->method);
  free(req->uri);
  free(req->version);
  while (req->headers) {
    header *next = req->headers->next;
    free(req->headers->name);
    free(req->headers->value);
    free(req->headers);
    req->headers = next;
  }
  free(req);
}

static void parse_headers(header **headers, char *data, int len, int pos)
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

      if (nl && ((pos+1 >= len) || !isspace(data[pos+1]))) {
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
        h->next = *headers;
        *headers = h;

        namestart = pos+1;
        state = PS_NAME;
      }
      break;
    }
    }
    pos++;
  }
}

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

static request *parse_request(httpconn *hc, char *data, int len)
{
  int pos = 0;
  request *req;
  if (!end_of_line(data,len,&pos))
    return NULL;

  req = (request*)calloc(1,sizeof(request));
  req->method = (char*)malloc(1025);
  req->uri = (char*)malloc(1025);
  req->version = (char*)malloc(1025);
  if (3 != sscanf(data,"%1024s %1024s %1024ss",req->method,req->uri,req->version)) {
    free_request(req);
    return NULL;
  }

  if ((pos < len) && ('\n' == data[pos]))
    pos++;
  else if ((pos+1 < len) && ('\r' == data[pos]) && ('\n' == data[pos+1]))
    pos += 2;

  parse_headers(&req->headers,data,len,pos);
  return req;
}

static void handle_request(endpoint *endpt, httpconn *hc,
                           const char *method, const char *uri, const char *version,
                           header *headers)
{
  header *h;
  array *response = array_new(1,0);
  array_printf(response,"HTTP/1.1 200 OK\r\n");
  array_printf(response,"Content-Type: text/html\r\n");
  array_printf(response,"Connection: close\r\n");
  array_printf(response,"\r\n");

  array_printf(response,"<html><head><title>Request info</title></head><body>\n");
  array_printf(response,"<h1>Request line</h1>\n");
  array_printf(response,"<table border=\"1\" width=\"100%\">\n");
  array_printf(response,"<tr><td>Method</td><td>%s</td></tr>\n",method);
  array_printf(response,"<tr><td>URI</td><td>%s</td></tr>\n",uri);
  array_printf(response,"<tr><td>Version</td><td>%s</td></tr>\n",version);
  array_printf(response,"</table>\n");
  array_printf(response,"<h1>Headers</h1>\n");
  array_printf(response,"<table border=\"1\" width=\"100%\">\n");
  for (h = headers; h; h = h->next) {
    array_printf(response,"<tr><td>%s</td><td>%s</td></tr>\n",h->name,h->value);
  }
  array_printf(response,"</table>\n");
  array_printf(response,"</body></html>\n");
  send_write(endpt,hc->sockid,0,response->data,response->nbytes);
  send_write(endpt,hc->sockid,0,NULL,0);
  array_free(response);
}

static void send_error(endpoint *endpt, httpconn *hc,
                       int code, const char *str, const char *msg)
{
  array *response = array_new(1,0);
  array_printf(response,"HTTP/1.1 %d %s\r\n",code,str);
  array_printf(response,"Content-Type: text/plain\r\n");
  array_printf(response,"Content-Length: %d\r\n",strlen(msg)+1);
  array_printf(response,"Connection: close\r\n");
  array_printf(response,"\r\n");
  array_printf(response,"%s\n",msg);
  send_write(endpt,hc->sockid,0,response->data,response->nbytes);
  send_write(endpt,hc->sockid,0,NULL,0);
  array_free(response);
}

static void check_readbuf(endpoint *endpt, httpconn *hc)
{
  char *data = hc->readbuf->data;
  int len = hc->readbuf->nbytes;
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
    request *req;
    if (NULL != (req = parse_request(hc,data,pos))) {
      handle_request(endpt,hc,req->method,req->uri,req->version,req->headers);
      free_request(req);
    }
    else {
      send_error(endpt,hc,400,"Bad Request","Invalid request line");
    }
  }
}

static httpconn *find_connection(httpconn_list *connections, socketid sockid)
{
  httpconn *hc;
  for (hc = connections->first; hc; hc = hc->next)
    if (socketid_equals(&hc->sockid,&sockid))
      return hc;
  return NULL;
}

static void http_thread(node *n, endpoint *endpt, void *arg)
{
  int done = 0;
  int port = *(int*)arg;
  socketid listen_sockid;
  httpconn_list connections;

  memset(&connections,0,sizeof(connections));
  memset(&listen_sockid,0,sizeof(listen_sockid));

  send_listen(endpt,n->iothid,INADDR_ANY,port,endpt->epid,1);
  while (!done) {
    message *msg = endpoint_receive(endpt,-1);
    switch (msg->tag) {
    case MSG_LISTEN_RESPONSE: {
      listen_response_msg *lrm = (listen_response_msg*)msg->data;
      assert(sizeof(listen_response_msg) == msg->size);
      if (lrm->error) {
        fprintf(stderr,"listen failed: %s\n",lrm->errmsg);
        done = 1;
      }
      else {
        printf("listen succeeded\n");
        printf("ready\n");
        listen_sockid = lrm->sockid;
        send_accept(endpt,listen_sockid,1);
      }
      break;
    }
    case MSG_ACCEPT_RESPONSE: {
      accept_response_msg *arm = (accept_response_msg*)msg->data;
      httpconn *hc;
      assert(sizeof(accept_response_msg) == msg->size);
      printf("got connection from %s\n",arm->hostname);
      send_accept(endpt,listen_sockid,1);
      send_read(endpt,arm->sockid,1);

      hc = (httpconn*)calloc(1,sizeof(httpconn));
      hc->sockid = arm->sockid;
      hc->readbuf = array_new(1,0);
      llist_append(&connections,hc);

      break;
    }
    case MSG_READ_RESPONSE: {
      read_response_msg *rrm = (read_response_msg*)msg->data;
      httpconn *hc;
      assert(sizeof(read_response_msg) <= msg->size);
      hc = find_connection(&connections,rrm->sockid);
      assert(hc);

      if (0 < rrm->len) {
        array_append(hc->readbuf,rrm->data,rrm->len);
        check_readbuf(endpt,hc);
        send_read(endpt,rrm->sockid,1);
      }
      break;
    }
    case MSG_WRITE_RESPONSE:
      /* ignore */
      break;
    case MSG_CONNECTION_CLOSED: {
      connection_event_msg *cem = (connection_event_msg*)msg->data;
      httpconn *hc;
      assert(sizeof(connection_event_msg) <= msg->size);
      hc = find_connection(&connections,cem->sockid);
      assert(hc);

      llist_remove(&connections,hc);
      array_free(hc->readbuf);
      free(hc);

      break;
    }
    case MSG_KILL:
      done = 1;
      break;
    default:
      fatal("echo: unexpected message %d",msg->tag);
      break;
    }
    message_free(msg);
  }
}

int main(int argc, char **argv)
{
  node *n;
  int port;

  setbuf(stdout,NULL);

  if (2 > argc) {
    fprintf(stderr,"Usage: httpserver <port>\n");
    return -1;
  }

  port = atoi(argv[1]);

  n = node_start(LOG_ERROR,0);
  if (NULL == n)
    exit(1);
  node_add_thread(n,"echo",http_thread,&port,NULL);
  node_run(n);

  return 0;
}
