#include <fcntl.h>
#include <errno.h>
#include <netdb.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/time.h>
#include <pthread.h>

char *request;
char *hostname;
int port;
int nrequests;
int size;
int nthreads;

struct timeval timeval_diff(struct timeval from, struct timeval to)
{
  struct timeval diff;
  diff.tv_sec = to.tv_sec - from.tv_sec;
  diff.tv_usec = to.tv_usec - from.tv_usec;
  if (0 > diff.tv_usec) {
    diff.tv_sec -= 1;
    diff.tv_usec += 1000000;
  }
  return diff;
}

int timeval_diffms(struct timeval from, struct timeval to)
{
  struct timeval diff = timeval_diff(from,to);
  return diff.tv_sec*1000 + diff.tv_usec/1000;
}

int open_connection(const char *hostname, int port)
{
  int sockfd;
  struct hostent *he;
  struct sockaddr_in addr;

  if (NULL == (he = gethostbyname(hostname))) {
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

void do_request()
{
  int sock;
  if (0 > (sock = open_connection(hostname,port)))
    exit(1);
  int w = write(sock,request,size);
  if (w != size) {
    fprintf(stderr,"write() returned %d, expected %d\n",w,size);
    if (0 > w)
      perror("write");
    exit(1);
  }
  shutdown(sock,SHUT_WR);
  int r;
  char buf[1024];
  int total = 0;
  while (0 < (r = read(sock,buf,1024))) {
    total += r;
  }
  close(sock);
  if (total != size+1) {
    fprintf(stderr,"read %d total bytes, expected %d\n",total,size+1);
    exit(1);
  }
}

int count = 0;
pthread_mutex_t count_lock;

void *request_thread(void *arg)
{
  while (1) {
    pthread_mutex_lock(&count_lock);
    if (count >= nrequests) {
      pthread_mutex_unlock(&count_lock);
      return NULL;
    }
    count++;
    pthread_mutex_unlock(&count_lock);

    do_request();
  }
}

int main(int argc, char **argv)
{
  setbuf(stdout,NULL);

  if (6 > argc) {
    fprintf(stderr,"Usage: client <host> <port> <nrequests> <size> <nthreads>\n");
    return -1;
  }

  hostname = argv[1];
  port = atoi(argv[2]);
  nrequests = atoi(argv[3]);
  size = atoi(argv[4]);
  nthreads = atoi(argv[5]);

  if (0 == size)
    size = 1;
  if (0 == nthreads)
    nthreads = 1;

  printf("Data transfer benchmark\n");
  printf("hostname = %s\n",hostname);
  printf("port = %d\n",port);
  printf("nrequests = %d\n",nrequests);
  printf("size = %d\n",size);
  printf("nthreads = %d\n",nthreads);

  request = (char*)malloc(size);
  memset(request,' ',size);
  request[0] = '0'; /* 0 comp ms */

  pthread_mutex_init(&count_lock,NULL);

  struct timeval start_time;
  gettimeofday(&start_time,NULL);

  pthread_t threads[nthreads];
  int i;
  for (i = 0; i < nthreads; i++) {
    if (0 != pthread_create(&threads[i],NULL,request_thread,NULL)) {
      perror("pthread_create");
      exit(1);
    }
  }

  for (i = 0; i < nthreads; i++) {
    if (0 != pthread_join(threads[i],NULL)) {
      perror("pthread_join");
      exit(1);
    }
  }

  struct timeval end_time;
  gettimeofday(&end_time,NULL);

  int ms = timeval_diffms(start_time,end_time);
  double seconds = ((double)ms)/1000.0;
  printf("\n");
  printf("Done\n");
  printf("\n");
  printf("Time = %.3f\n",seconds);
  printf("Request rate = %.3f requests/second\n",nrequests/seconds);
  printf("Transfer rate = %.3f kb/second\n",nrequests*size*2/seconds/1024.0);

  free(request);
  pthread_mutex_destroy(&count_lock);

  return 0;
}
