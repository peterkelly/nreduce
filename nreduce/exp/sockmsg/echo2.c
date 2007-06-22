#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "compiler/util.h"
#include "runtime/node.h"
#include "runtime/messages.h"
#include <stdlib.h>
#include <string.h>

static void send_accept(endpoint *endpt, socketid listen_sockid)
{
  accept_msg am;
  am.sockid = listen_sockid;
  am.ioid = 1;
  endpoint_send(endpt,endpt->n->managerid,MSG_ACCEPT,&am,sizeof(am));
}

static void send_read(endpoint *endpt, socketid sockid)
{
  read_msg rm;
  rm.sockid = sockid;
  rm.ioid = 1;
  endpoint_send(endpt,sockid.managerid,MSG_READ,&rm,sizeof(rm));
}

static void send_write(endpoint *endpt, socketid sockid, const char *data, int len)
{
  int msglen = sizeof(write_msg)+len;
  write_msg *wm = (write_msg*)malloc(msglen);
  wm->sockid = sockid;
  /* FIXME: it should be ok to set this to anything, but currently it is not... see comment
     in manager_write(). When that problem is fixed this can be set to 1 (not that it matters for
     this case, but it should at least be accepted). Actually the ioid field shouldn't be needed
     at all once sockids are used to correlate I/O requests (the only exception begin connect) */
  wm->ioid = 0;
  wm->len = len;
  memcpy(wm->data,data,len);
  endpoint_send(endpt,sockid.managerid,MSG_WRITE,wm,msglen);
  free(wm);
}

static void send_finwrite(endpoint *endpt, socketid sockid)
{
  finwrite_msg fwm;

  fwm.sockid = sockid;
  fwm.ioid = 1;

  endpoint_send(endpt,sockid.managerid,MSG_FINWRITE,&fwm,sizeof(fwm));
}

static void echo_thread(node *n, endpoint *endpt, void *arg)
{
  int done = 0;
  int port = *(int*)arg;
  listen_msg lm;
  socketid listen_sockid;

  memset(&listen_sockid,0,sizeof(listen_sockid));

  lm.ip = INADDR_ANY;
  lm.port = port;
  lm.owner = endpt->epid;
  lm.ioid = 1;

  endpoint_send(endpt,n->managerid,MSG_LISTEN,&lm,sizeof(lm));

  while (!done) {
    message *msg = endpoint_receive(endpt,-1);
    switch (msg->hdr.tag) {
    case MSG_LISTEN_RESPONSE: {
      listen_response_msg *lrm = (listen_response_msg*)msg->data;
      assert(sizeof(listen_response_msg) == msg->hdr.size);
      if (lrm->error) {
        fprintf(stderr,"listen failed: %s\n",lrm->errmsg);
        done = 1;
      }
      else {
        printf("listen succeeded\n");
        printf("ready\n");
        listen_sockid = lrm->sockid;
        send_accept(endpt,listen_sockid);
      }
      break;
    }
    case MSG_ACCEPT_RESPONSE: {
      accept_response_msg *arm = (accept_response_msg*)msg->data;
      assert(sizeof(accept_response_msg) == msg->hdr.size);
      printf("got connection from %s\n",arm->hostname);
      send_accept(endpt,listen_sockid);
      send_read(endpt,arm->sockid);
      break;
    }
    case MSG_READ_RESPONSE: {
      read_response_msg *rrm = (read_response_msg*)msg->data;
      assert(sizeof(read_response_msg) <= msg->hdr.size);
/*       printf("read %d bytes\n",rrm->len); */
      if (0 < rrm->len) {
        send_read(endpt,rrm->sockid);
        send_write(endpt,rrm->sockid,rrm->data,rrm->len);
      }
      else {
        send_finwrite(endpt,rrm->sockid);
      }
      break;
    }
    case MSG_WRITE_RESPONSE:
    case MSG_FINWRITE_RESPONSE:
      /* ignore */
      break;
    case MSG_CONNECTION_CLOSED:
      lock_node(n);
      printf("Connection coded: total sent = %d, total received = %d\n",
             n->total_sent,n->total_received);
      unlock_node(n);
      break;
    case MSG_KILL:
      done = 1;
      break;
    default:
      fatal("echo: unexpected message %s",msg_names[msg->hdr.tag]);
      break;
    }
    message_free(msg);
  }
}

int main(int argc, char **argv)
{
  node *n;
  int port;
  int bufsize;

  setbuf(stdout,NULL);

  if (3 > argc) {
    fprintf(stderr,"Usage: echo2 <port> <bufsize>\n");
    return -1;
  }

  port = atoi(argv[1]);
  bufsize = atoi(argv[2]);

  n = node_start(LOG_ERROR,0);
  n->iosize = bufsize;
  if (NULL == n)
    exit(1);
  node_add_thread(n,0,TEST_ENDPOINT,0,echo_thread,&port,NULL);
  node_run(n);

  return 0;
}
