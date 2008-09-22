#include <fcntl.h>
#include <errno.h>
#include <netdb.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <sys/types.h>

//#define CONNECTION_LIMIT 100
#define CONNECTION_LIMIT 100000
//#define DEBUG_THREADS

char *host;
int port;
char *path;
pthread_mutex_t lock;
pthread_cond_t cond;
int active = 0;
int maxactive = 0;
int total = 0;
int remaining = 0;

int open_connection(const char *host, int port)
{
  int sockfd;
  struct hostent *he;
  struct sockaddr_in addr;

  if (NULL == (he = gethostbyname(host))) {
    herror("gethostbyname");
    return -1;
  }

  if (0 > (sockfd = socket(AF_INET,SOCK_STREAM,0))) {
    perror("socket");
    return -1;
  }

  addr.sin_family = AF_INET;
  addr.sin_port = htons(port);
  addr.sin_addr = *((struct in_addr*)he->h_addr);
  memset(&addr.sin_zero,0,8);

  if (0 > (connect(sockfd,(struct sockaddr*)&addr,sizeof(struct sockaddr)))) {
    close(sockfd);
    perror("connect");
    return -1;
  }

  return sockfd;
}

int single_request()
{
  int sock;
  if (0 > (sock = open_connection(host,port))) {
    perror("sock");
    exit(-1);
  }

  char request[1024];
  snprintf(request,1024,"GET %s HTTP/1.0\r\n\r\n",path);
  int reqlen = strlen(request);
  if (reqlen != write(sock,request,reqlen)) {
    fprintf(stderr,"Request failed\n");
    exit(-1);
  }

  int r;
  char buf[1024];
  int bytesread = 0;
  while (0 < (r = read(sock,buf,1024)))
    bytesread += r;

  #ifdef DEBUG_THREADS
  printf("Bytes read: %d\n",bytesread);
  #endif
  close(sock);
  return bytesread;
}

void *request_thread(void *arg)
{
  pthread_mutex_lock(&lock);

  while (1) {
    if (active >= CONNECTION_LIMIT)
      pthread_cond_wait(&cond,&lock);
    if (active < CONNECTION_LIMIT)
      break;
  }

  active++;
  if (maxactive < active)
    maxactive = active;
  #ifdef DEBUG_THREADS
  printf("active = %d\n",active);
  #endif
  pthread_mutex_unlock(&lock);


  int bytesread = single_request();

  pthread_mutex_lock(&lock);
  active--;
  #ifdef DEBUG_THREADS
  printf("active = %d\n",active);
  #endif
  remaining--;
  total += bytesread;
  pthread_cond_broadcast(&cond);
  pthread_mutex_unlock(&lock);


  return NULL;
}

void do_requests(int n)
{
  int i;
  pthread_t *threads = (pthread_t*)calloc(n,sizeof(pthread_t));
  remaining = n;
  for (i = 0; i < n; i++) {
    if (0 != pthread_create(&threads[i],NULL,request_thread,NULL)) {
      perror("pthread_create");
      exit(-1);
    }
  }

  pthread_mutex_lock(&lock);
  while (0 < remaining) {
    pthread_cond_wait(&cond,&lock);
  }
  pthread_mutex_unlock(&lock);

  printf("All requests completed, total = %d, maxactive = %d\n",total,maxactive);
}

int main(int argc, char **argv)
{
  int n;

  setbuf(stdout,NULL);

  if (4 > argc) {
    fprintf(stderr,"Usage: requests <n> <host> <port>\n");
    return -1;
  }

  n = atoi(argv[1]);
  host = argv[2];
  port = atoi(argv[3]);
  if (5 <= argc)
    path = argv[4];
  else
    path = "/";

  printf("Connection limit = %d\n",CONNECTION_LIMIT);

  pthread_mutex_init(&lock,NULL);
  pthread_cond_init(&cond,NULL);

  do_requests(n);

  pthread_cond_destroy(&cond);
  pthread_mutex_destroy(&lock);

  return 0;
}
