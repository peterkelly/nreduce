#ifndef _PORTSET_H
#define _PORTSET_H

/* Keeps track of ports we have previously used, to aid in selection of client-side ports
   when establishing outgoing connections. */
typedef struct portset {
  int min;
  int max;
  int upto;

  int size;
  int count;
  int *ports;
  int start;
  int end;
} portset;

void portset_init(portset *pb, int min, int max);
void portset_destroy(portset *pb);
int portset_allocport(portset *pb);
void portset_releaseport(portset *pb, int port);
void bind_client_port(int sock, portset *ps, int *clientport);

#endif
