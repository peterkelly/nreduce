#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "network/util.h"
#include "network/node.h"
#include <stdlib.h>
#include <string.h>
#include <assert.h>

static void echo_thread(node *n, endpoint *endpt, void *arg)
{
  int done = 0;
  int port = *(int*)arg;
  socketid listen_sockid;

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
      assert(sizeof(accept_response_msg) == msg->size);
      printf("got connection from %s\n",arm->hostname);
      send_accept(endpt,listen_sockid,1);
      send_read(endpt,arm->sockid,1);
      break;
    }
    case MSG_READ_RESPONSE: {
      read_response_msg *rrm = (read_response_msg*)msg->data;
      assert(sizeof(read_response_msg) <= msg->size);
/*       printf("read %d bytes\n",rrm->len); */
      if (0 < rrm->len) {
        send_read(endpt,rrm->sockid,1);
        /* FIXME: it should be ok to set the ioid to anything, but currently it is not... see
           comment in manager_write(). When that problem is fixed this can be set to 1 (not that
           it matters for this case, but it should at least be accepted). Actually the ioid field
           shouldn't be needed at all once sockids are used to correlate I/O requests (the only
           exception begin connect) */
        send_write(endpt,rrm->sockid,0,rrm->data,rrm->len);
      }
      else {
        send_write(endpt,rrm->sockid,0,NULL,0);
      }
      break;
    }
    case MSG_WRITE_RESPONSE:
      /* ignore */
      break;
    case MSG_CONNECTION_CLOSED:
      break;
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
  int bufsize;

  setbuf(stdout,NULL);

  if (3 > argc) {
    fprintf(stderr,"Usage: echo2 <port> <bufsize>\n");
    return -1;
  }

  port = atoi(argv[1]);
  bufsize = atoi(argv[2]);

  n = node_start(LOG_ERROR,0);
  if (NULL == n)
    exit(1);
  n->iosize = bufsize;
  node_add_thread(n,"echo",echo_thread,&port,NULL);
  node_run(n);

  return 0;
}
