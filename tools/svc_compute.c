#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <netdb.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <math.h>
#include <netinet/tcp.h>

#define BACKLOG 100

long long comp_per_ms = 0;

int start_listening(int port)
{
  int yes = 1;
  struct sockaddr_in local_addr;
  int sock;

  if (0 > (sock = socket(AF_INET,SOCK_STREAM,0))) {
    perror("socket");
    return -1;
  }

  if (0 > setsockopt(sock,SOL_SOCKET,SO_REUSEADDR,&yes,sizeof(int))) {
    perror("setsockopt");
    close(sock);
    return -1;
  }

  local_addr.sin_family = AF_INET;
  local_addr.sin_port = htons(port);
  local_addr.sin_addr.s_addr = INADDR_ANY;
  memset(&local_addr.sin_zero,0,8);

  if (0 > bind(sock,(struct sockaddr*)&local_addr,sizeof(struct sockaddr))) {
    perror("bind");
    close(sock);
    return -1;
  }

  if (0 > listen(sock,BACKLOG)) {
    perror("listen");
    close(sock);
    return -1;
  }

  return sock;
}

pthread_mutex_t lock;
pthread_cond_t cond;
int nthreads = 0;

#define BUFSIZE 1024

void run(long long work, long long iterations)
{
  long long total = work*iterations;
  long long i;
  for (i = 0; i < total; i++) {
  }
}

#define DEFAULT_WORK 1000000
#define CALIB_ITERATIONS 1000

long long get_micro_time()
{
  struct timeval tv;
  gettimeofday(&tv,NULL);
  return (long long)tv.tv_sec*1000000 + (long long)tv.tv_usec;
}

long calibrate()
{
  printf("Warmup\n");
  run(DEFAULT_WORK,CALIB_ITERATIONS);
  long long start = get_micro_time();

  printf("Calibration\n");
  run(DEFAULT_WORK,CALIB_ITERATIONS);
  long long end = get_micro_time();
  long long ms = (end-start)/1000;
  printf("%d iterations took %lldms\n",CALIB_ITERATIONS,ms);
  long long comp_per_ms = DEFAULT_WORK*CALIB_ITERATIONS/ms;
  printf("comp per ms = %lld\n",comp_per_ms);

  printf("Testing\n");
  int i;
  for (i = 1; i <= 16384; i *= 2) {
    long long start = get_micro_time();
    run(comp_per_ms,i);
    long long end = get_micro_time();
    double ratio = (end-start)/(i*1000.0);
    printf("Goal=%dms Actual=%lldms (%.3f%%)\n",
           i,(end-start)/1000,100.0*ratio);
  }

  return 0;
}

void handle(int sock)
{
  /* Read all data from client */
  int alloc = 1024;
  int size = 0;
  char *data = (char*)malloc(alloc);
  int r;
  while (0 < (r = read(sock,&data[size],1024))) {
    alloc += r;
    size += r;
    data = (char*)realloc(data,alloc);
  }
  data[size] = '\0';

  /* Get value & computation time */
  int value;
  int ms;
  if (2 == sscanf(data,"%d %d",&value,&ms)) {

    /* Compute for requested time */
    run(comp_per_ms,ms);

    /* Send back value */
    snprintf(data,1024,"%d",value);
    write(sock,data,strlen(data));
  }

  free(data);
}

void *connection_handler(void *arg)
{
  int sock = (int)arg;
  handle(sock);
  close(sock);

  pthread_mutex_lock(&lock);
  nthreads--;
  pthread_cond_broadcast(&cond);
  pthread_mutex_unlock(&lock);

  return NULL;
}

long long get_comp_per_ms()
{
  char *cpmstr = getenv("COMP_PER_MS");
  if (NULL == cpmstr) {
    fprintf(stderr,"Environment variable COMP_PER_MS not set; run with -calibrate to determine\n");
    exit(-1);
  }
  char *end = NULL;
  long long cpm = strtol(cpmstr,&end,10);
  if (('\0' == cpmstr[0]) || ('\0' != *end)) {
    fprintf(stderr,"Environment variable COMP_PER_MS invalid (\"%s\"); must be a number\n",
            cpmstr);
    exit(1);
  }
  return cpm;
}

void compute_service(int port, int direct)
{
  int listensock;
  int sock;
  int maxthreads = 3;
  int nconnections = 0;

  if (direct) {
    printf("Using single thread for all responses\n");
  }
  else {
    printf("Using separate threads for each response\n");
  }

  comp_per_ms = get_comp_per_ms();

  printf("COMP_PER_MS = %lld\n",comp_per_ms);

  printf("Starting compute service on port %d\n",port);

  /* Create the listening socket */
  if (0 > (listensock = start_listening(port)))
    exit(-1);

  pthread_mutex_init(&lock,NULL);
  pthread_cond_init(&cond,NULL);

  /* Repeatedly accept connections from clients */
  printf("Waiting for connection...\n");
  while (1) {
    if (!direct) {
      /* Wait until there is a thread available */
      pthread_mutex_lock(&lock);
      while (nthreads+1 > maxthreads) {
        pthread_cond_wait(&cond,&lock);
      }
      nthreads++;
      pthread_mutex_unlock(&lock);
    }

    /* Accept the connection */
    if (0 > (sock = accept(listensock,NULL,0))) {
      perror("accept");
      exit(-1);
    }

    int yes = 1;
    if (0 > setsockopt(sock,IPPROTO_TCP,TCP_NODELAY,&yes,sizeof(int))) {
      perror("setsockopt TCP_NODELAY");
      exit(-1);
    }

    nconnections++;
    printf("# connections = %d\n",nconnections);

    if (direct) {
      handle(sock);
      close(sock);
    }
    else {

      /* Start a new thread to handle the connection */
      pthread_t thread;
      if (0 != pthread_create(&thread,NULL,connection_handler,(void*)sock)) {
        perror("pthread_create");
        exit(-1);
      }
      if (0 != pthread_detach(thread)) {
        perror("pthread_detach");
        exit(-1);
      }
    }
  }

  pthread_cond_destroy(&cond);
  pthread_mutex_destroy(&lock);
}

int main(int argc, char **argv)
{
  int port;

  setbuf(stdout,NULL);

  if (2 > argc) {
    fprintf(stderr,"Usage: svc_compute <port>\n");
    fprintf(stderr,"       svc_compute -calibrate\n");
    return -1;
  }

  if (!strcmp(argv[1],"-calibrate")) {
    calibrate();
    exit(0);
  }
  else {
    char *end = NULL;
    port = strtol(argv[1],&end,10);
    if (('\0' == argv[1][0]) || ('\0' != *end)) {
      fprintf(stderr,"Invalid port number\n");
      exit(1);
    }
    int direct = (getenv("DIRECT") != NULL);
    compute_service(port,direct);
  }

  return 0;
}
