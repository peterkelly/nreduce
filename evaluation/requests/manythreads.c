#include <stdio.h>
#include <pthread.h>
#include <string.h>
#include <errno.h>
#include <sys/time.h>
#include <stdlib.h>

pthread_mutex_t lock;

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

void *empty_thread(void *arg)
{
  return NULL;
}

void *thread_func(void *arg)
{
/*   int threadno = (int)arg; */
/*   printf("Thread %d has begun\n",threadno); */
  pthread_mutex_lock(&lock);
  pthread_mutex_unlock(&lock);
  return NULL;
}

void time_threadspawn()
{
  int i;
  struct timeval start;
  struct timeval end;
  int nthreads = 100000;

  gettimeofday(&start,NULL);

  for (i = 0; i < nthreads; i++) {
    pthread_t th;
    if (0 != pthread_create(&th,NULL,empty_thread,NULL)) {
      fprintf(stderr,"pthread_create: %s\n",strerror(errno));
      exit(1);
    }
    pthread_join(th,NULL);
  }

  gettimeofday(&end,NULL);
  int ms = timeval_diffms(start,end);
  double rate = 1000.0*((double)nthreads)/((double)ms);
  printf("Creating + destroying %d threads took %dms; rate = %.0f threads/sec\n",
         nthreads,ms,rate);
}

void test_maxthreads()
{
  int threadno = 0;
  int i;
  pthread_t threads[10000];
  pthread_mutex_init(&lock,NULL);
  pthread_mutex_lock(&lock);
  while (threadno < 10000) {
    if (0 == pthread_create(&threads[threadno],NULL,thread_func,(void*)threadno)) {
/*       printf("Created thread %d\n",threadno); */
      threadno++;
    }
    else {
      printf("Can't create any more threads: %s\n",strerror(errno));
      break;
    }
  }
  pthread_mutex_unlock(&lock);
  for (i = 0; i < threadno; i++)
    pthread_join(threads[i],NULL);
  printf("Max threads: %d\n",threadno);
  pthread_mutex_destroy(&lock);
}

int main(int argc, char **argv)
{

  time_threadspawn();
  test_maxthreads();

  return 0;
}
