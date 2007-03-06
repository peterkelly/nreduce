#include <assert.h>
#include <fcntl.h>
#include <netdb.h>
#include <netinet/tcp.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include "util.h"

#define LISTEN_BACKLOG 10

int start_listening(struct in_addr ip, int port)
{
  int yes = 1;
  struct sockaddr_in local_addr;
  int listenfd;

  if (-1 == (listenfd = socket(AF_INET,SOCK_STREAM,0))) {
    perror("socket");
    return -1;
  }

  if (-1 == setsockopt(listenfd,SOL_SOCKET,SO_REUSEADDR,&yes,sizeof(int))) {
    perror("setsockopt");
    return -1;
  }

  local_addr.sin_family = AF_INET;
  local_addr.sin_port = htons(port);
  local_addr.sin_addr = ip;
  memset(&local_addr.sin_zero,0,8);

  if (-1 == bind(listenfd,(struct sockaddr*)&local_addr,sizeof(struct sockaddr))) {
    perror("bind");
    return -1;
  }

  if (-1 == listen(listenfd,LISTEN_BACKLOG)) {
    perror("listen");
    return -1;
  }

  return listenfd;
}

int start_listening_host(const char *host, int port)
{
  struct hostent *he = gethostbyname(host);
  struct in_addr ip;
  if (NULL == he) {
    fprintf(stderr,"%s: %s\n",host,hstrerror(h_errno));
    return -1;
  }

  if (4 != he->h_length) {
    fprintf(stderr,"%s: unknown address type (length %d)\n",host,he->h_length);
    return -1;
  }

  ip = *((struct in_addr*)he->h_addr_list[0]);
  return start_listening(ip,port);
}

int fdsetflag(int fd, int flag, int on)
{
  int flags;
  if (0 > (flags = fcntl(fd,F_GETFL))) {
    perror("fcntl(F_GETFL)");
    return -1;
  }

  if (on)
    flags |= flag;
  else
    flags &= ~flag;

  if (0 > fcntl(fd,F_SETFL,flags)) {
    perror("fcntl(F_SETFL)");
    return -1;
  }
  return 0;
}

int fdsetblocking(int fd, int blocking)
{
  return fdsetflag(fd,O_NONBLOCK,!blocking);
}

array *array_new(int elemsize)
{
  array *arr = (array*)malloc(sizeof(array));
  arr->elemsize = elemsize;
  arr->alloc = 120;
  arr->nbytes = 0;
  arr->data = malloc(arr->alloc);
  return arr;
}

void array_mkroom(array *arr, const int size)
{
  while (arr->nbytes+size > arr->alloc) {
    arr->alloc *= 2;
    arr->data = realloc(arr->data,arr->alloc);
  }
}

void array_append(array *arr, const void *data, int size)
{
  array_mkroom(arr,size);
  memmove((char*)(arr->data)+arr->nbytes,data,size);
  arr->nbytes += size;
}

void array_free(array *arr)
{
  free(arr->data);
  free(arr);
}
