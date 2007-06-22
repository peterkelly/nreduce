#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <ctype.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#include <pthread.h>

#define BUFSIZE 1024

typedef struct {
  int repeat;
  char *bg;
  char *cmd;
  double min;
  double max;
  double incr;
} params;

int verbose = 0;

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

int timeval_ms(struct timeval tv)
{
  return tv.tv_sec*1000 + tv.tv_usec/1000;
}

void parse_pair(const char *key, const char *value, params *p)
{
  if (!strcmp(key,"repeat")) {
    p->repeat = atoi(value);
  }
  else if (!strcmp(key,"bg")) {
    p->bg = strdup(value);
  }
  else if (!strcmp(key,"cmd")) {
    p->cmd = strdup(value);
  }
  else if (!strcmp(key,"min")) {
    p->min = atof(value);
  }
  else if (!strcmp(key,"max")) {
    p->max = atof(value);
  }
  else if (!strcmp(key,"incr")) {
    p->incr = atof(value);
  }
  else {
    fprintf(stderr,"Unknown key: %s\n",key);
    exit(1);
  }
}

void process_line(char *line, int len, params *p)
{
  char *equals;
  char *value;
  char *end;

  equals = strchr(line,'=');
  if (!equals || (equals == line)) {
    fprintf(stderr,"invalid line: \"%s\"\n",line);
    exit(1);
  }

  value = equals+1;
  while ((value < &line[len]) && isspace(*value))
    value++;

  end = &line[len];
  while ((end-1 > value) && isspace(*(end-1)))
    end--;
  *end = '\0';

  while ((equals-1 > line) && isspace(*(equals-1)))
    equals--;
  *equals = '\0';

  parse_pair(line,value,p);
}

void read_paramfile(const char *filename, params *p)
{
  FILE *f = fopen(filename,"r");
  if (NULL == f) {
    perror(filename);
    exit(1);
  }
  else {
    int size = 0;
    char *data = (char*)malloc(BUFSIZE);
    int r;

    while (0 < (r = fread(&data[size],1,BUFSIZE,f))) {
      size += r;
      data = realloc(data,size+BUFSIZE);
    }
    if (0 > r) {
      perror(filename);
      exit(1);
    }

    int start = 0;
    int pos = 0;
    for (; pos <= size; pos++) {
      if ((pos == size) || ('\n' == data[pos])) {
        char *line = &data[start];

        if (pos == start)
          continue;

        data[pos] = '\0';
        process_line(line,pos-start,p);
        start = pos+1;
      }
    }

    free(data);
  }
  fclose(f);
}

void check_params(params *p)
{
  if (0 >= p->repeat) {
    fprintf(stderr,"Invalid repeat count %d: must be > 0\n",p->repeat);
    exit(1);
  }

  if (p->min > p->max) {
    fprintf(stderr,"Invalid range %f-%f: min must be <= max\n",p->min,p->max);
    exit(1);
  }

  if (0 >= p->incr) {
    fprintf(stderr,"Invalid increment %f: must be > 0\n",p->incr);
    exit(1);
  }

}

void kill_background(pid_t bgpid, struct rusage *ru)
{
  int status;
  if (0 > kill(bgpid,SIGKILL)) {
    perror("kill");
    exit(1);
  }
  if (0 > wait4(bgpid,&status,0,ru)) {
    perror("wait4");
    exit(1);
  }
}

void *read_rest(void *arg)
{
  int fd = (int)arg;
  int r;
  char buf[BUFSIZE];
  while (0 < (r = read(fd,buf,BUFSIZE)))
    fwrite(buf,1,r,stdout);
  if (0 > r) {
    perror("read_rest: read");
    exit(1);
  }
  close(fd);
  return NULL;
}

pid_t start_background(params *p)
{
  pid_t bgpid;
  int fds[2];

  if (0 > pipe(fds)) {
    perror("pipe");
    exit(1);
  }

  bgpid = fork();
  if (0 > bgpid) {
    perror("fork");
    exit(1);
  }

  if (0 == bgpid) {
    /* child */
    dup2(fds[1],STDOUT_FILENO);
    close(fds[0]);
    execl("/bin/sh","sh","-c",p->bg,NULL);
    perror("execl");
    abort();
  }

  /* parent */
  close(fds[1]);

  /* wait for a line saying "ready" */

  int size = 0;
  char *data = (char*)malloc(BUFSIZE);
  int r;
  int pos = 0;
  int start = 0;
  int ready = 0;

  while (!ready && (0 < (r = read(fds[0],&data[size],BUFSIZE)))) {
    size += r;
    data = realloc(data,size+BUFSIZE);
    if (verbose)
      fwrite(&data[size-r],1,r,stdout);

    for (; pos < size; pos++) {
      if ('\n' == data[pos]) {
        if (!strncmp(&data[start],"ready",5)) {
          if (verbose)
            printf("Background process is ready\n");
          ready = 1;
          break;
        }
        start = pos+1;
      }
    }
  }
  if (0 > r) {
    fprintf(stderr,"Error reading from background process: %s\n",strerror(errno));
    kill_background(bgpid,NULL);
    exit(1);
  }
  free(data);

  if (verbose) {
    pthread_t thread;
    if (0 != pthread_create(&thread,NULL,read_rest,(void*)fds[0])) {
      perror("pthread_create");
      exit(1);
    }
    if (0 != pthread_detach(thread)) {
      perror("pthread_detach");
      exit(1);
    }
  }
  else {
    close(fds[0]);
  }

  return bgpid;
}

void run_experiment(params *p)
{
  double val;
  int iteration;
  int values = 0;
  int *totalreal;
  int *totaluser;
  int *totalsys;
  int *totalcs;
  int valno;

  for (val = p->min; val <= p->max; val += p->incr)
    values++;

  totalreal = (int*)calloc(sizeof(int),values);
  totaluser = (int*)calloc(sizeof(int),values);
  totalsys = (int*)calloc(sizeof(int),values);
  totalcs = (int*)calloc(sizeof(int),values);

  for (iteration = 0; iteration < p->repeat; iteration++) {
    valno = 0;
    for (val = p->min; val <= p->max; val += p->incr) {
      struct timeval start;
      struct timeval end;
      int real = 0;
      int user = 0;
      int sys = 0;
      int cs = 0;
      char valstr[100];
      pid_t pid;
      pid_t bgpid = 0;
      int status;
      struct rusage ru;
      struct rusage bgru;

      sprintf(valstr,"%f",val);
      setenv("val",valstr,1);

      if (p->bg)
        bgpid = start_background(p);

      gettimeofday(&start,NULL);

      pid = fork();
      if (0 > pid) {
        perror("fork");
        exit(1);
      }

      if (0 == pid) {
        /* child */
        if (!verbose)
          close(STDOUT_FILENO);
        execl("/bin/sh","sh","-c",p->cmd,NULL);
        perror("execl");
        abort();
      }

      /* parent */
      if (0 > wait4(pid,&status,0,&ru)) {
        perror("wait4");
        exit(1);
      }
      if (!WIFEXITED(status)) {
        fprintf(stderr,"Child process had abnormal exit\n");
        exit(1);
      }
      if (0 != WEXITSTATUS(status)) {
        fprintf(stderr,"Child process exited with return code %d\n",WEXITSTATUS(status));
        if (p->bg)
          kill_background(bgpid,NULL);
        exit(1);
      }

      gettimeofday(&end,NULL);

      memset(&bgru,0,sizeof(bgru));
      if (p->bg)
        kill_background(bgpid,&bgru);

      real = timeval_diffms(start,end);
      user = timeval_ms(ru.ru_utime);
      sys = timeval_ms(ru.ru_stime);
      cs = ru.ru_nvcsw + ru.ru_nivcsw + bgru.ru_nvcsw + bgru.ru_nivcsw;

      printf("# iteration %d val %f real %d user %d sys %d cs %d\n",iteration,val,real,user,sys,cs);

      totalreal[valno] += real;
      totaluser[valno] += user;
      totalsys[valno] += sys;
      totalcs[valno] += cs;
      valno++;
    }
  }

  valno = 0;
  for (val = p->min; val <= p->max; val += p->incr) {
    double avgreal = ((double)totalreal[valno])/((double)p->repeat);
    double avguser = ((double)totaluser[valno])/((double)p->repeat);
    double avgsys = ((double)totalsys[valno])/((double)p->repeat);
    double avgcs = ((double)totalcs[valno])/((double)p->repeat);
    printf("%.3f %.3f %.3f %.3f %.3f\n",val,avgreal/1000.0,avguser/1000.0,avgsys/1000.0,avgcs);
    valno++;
  }

  free(totalreal);
  free(totaluser);
  free(totalsys);
  free(totalcs);
}

int main(int argc, char **argv)
{
  char *filename;
  params p;

  setbuf(stdout,NULL);
  signal(SIGPIPE,SIG_IGN);

  verbose = (getenv("VERBOSE") != NULL);

  if (2 > argc) {
    fprintf(stderr,"Usage: measure <paramfile>\n");
    exit(1);
  }

  filename = argv[1];

  memset(&p,0,sizeof(p));
  read_paramfile(filename,&p);

  check_params(&p);
  run_experiment(&p);

  return 0;
}
