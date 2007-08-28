#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "network/util.h"
#include "network/node.h"
#include <stdlib.h>
#include <string.h>
#include <errno.h>

#define MSG_PING 0
#define MSG_PONG 1

typedef struct {
  int npings;
  int size;
  endpointid pongid;
} ping_arg;

static void ping_thread(node *n, endpoint *endpt, void *arg)
{
  ping_arg *pa = (ping_arg*)arg;
  int done = 0;
  char *data = (char*)calloc(1,pa->size);

  endpoint_send(endpt,pa->pongid,MSG_PING,data,pa->size);
  pa->npings--;
  printf("%d remaining\n",pa->npings);

  while (!done) {
    message *msg = endpoint_receive(endpt,-1);
    switch (msg->tag) {
    case MSG_PONG:
      endpoint_send(endpt,pa->pongid,MSG_PING,data,pa->size);
      pa->npings--;
      printf("%d remaining\n",pa->npings);
      if (0 >= pa->npings)
        done = 1;
      break;
    case MSG_KILL:
      done = 1;
      break;
    default:
      fatal("ping: unexpected message %d",msg->tag);
      break;
    }
    message_free(msg);
  }

  free(data);
}

static void pong_thread(node *n, endpoint *endpt, void *arg)
{
  int done = 0;

  while (!done) {
    message *msg = endpoint_receive(endpt,-1);
    switch (msg->tag) {
    case MSG_PING:
      endpoint_send(endpt,msg->source,MSG_PONG,msg->data,msg->size);
      break;
    case MSG_KILL:
      done = 1;
      break;
    default:
      fatal("pong: unexpected message %d",msg->tag);
      break;
    }
    message_free(msg);
  }
}

int main(int argc, char **argv)
{
  node *n;
  ping_arg pa;
  pthread_t thread;
  int msgcount;
  int size;

  setbuf(stdout,NULL);

  if (3 > argc) {
    fprintf(stderr,"Usage: pingpong <msgcount> <size>\n");
    return -1;
  }

  msgcount = atoi(argv[1]);
  size = atoi(argv[2]);

  n = node_start(LOG_ERROR,0);
  if (NULL == n)
    exit(1);
  pa.pongid = node_add_thread(n,"pong",pong_thread,NULL,NULL);
  pa.size = size;
  pa.npings = msgcount/2;
  node_add_thread(n,"ping",ping_thread,&pa,&thread);
  if (0 != pthread_join(thread,NULL))
    fatal("pthread_join: %s",strerror(errno));
  node_shutdown(n);
  node_run(n);

  return 0;
}
