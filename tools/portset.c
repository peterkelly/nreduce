#include "portset.h"

#include <errno.h>
#include <netdb.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <assert.h>

void portset_init(portset *pb, int min, int max)
{
  pb->min = min;
  pb->max = max;
  pb->upto = min;

  pb->size = max+1-min;
  pb->ports = (int*)calloc(pb->size,sizeof(int));
  pb->count = 0;
  pb->start = 0;
  pb->end = 0;
}

void portset_destroy(portset *pb)
{
  free(pb->ports);
}

/* Choose a port to use for an outgoing connection. If we have not yet used up all ports in the
   range, the next port is picked. Otherwise, we choose the least recently used port. */
int portset_allocport(portset *pb)
{
  assert(0 <= pb->count);
  if (pb->upto <= pb->max) {
    /* Pick a port that has not yet been used, in preference to an existing one (which may still
       be be in TIME_WAIT) */
    return pb->upto++;
  }
  else if (0 < pb->count) {
    /* Pick a previously used port. It may still be in TIME_WAIT, but by picking the least
       recently used, we maximise the amount of time connections can stay in TIME_WAIT. */
    int r = pb->ports[pb->start];
    pb->start = (pb->start+1) % pb->size;
    pb->count--;
    return r;
  }
  else {
    return -1;
  }
}

/* Indicate that we have finished using a port, and it can be re-used again for a subsequent
   connection. */
void portset_releaseport(portset *pb, int port)
{
  pb->ports[pb->end] = port;
  pb->end = (pb->end+1) % pb->size;
  pb->count++;
  assert(pb->count <= pb->size);
}

void bind_client_port(int sock, portset *ps, int *clientport)
{
  while (1) {

    *clientport = portset_allocport(ps);
    if (0 > *clientport) {
      fprintf(stderr,"No more ports available!\n");
      exit(-1);
    }

    struct sockaddr_in local_addr;
    local_addr.sin_family = AF_INET;
    local_addr.sin_port = htons(*clientport);
    local_addr.sin_addr.s_addr = INADDR_ANY;
    memset(&local_addr.sin_zero,0,8);

    if (0 == bind(sock,(struct sockaddr*)&local_addr,sizeof(struct sockaddr))) {
      /* Port successfully bound */
      break;
    }

    if (EADDRINUSE == errno) {
      /* Port in use by another application. We don't put this back in the port set, since we
         don't want it to be used again. */
      printf("bind: port %d already in use, trying with another\n",*clientport);
      *clientport = -1;
    }
    else {
      perror("bind");
      exit(-1);
    }
  }
}
