#define _GNU_SOURCE /* for asprintf */

#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/select.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/time.h>
#include <time.h>
#include <unistd.h>
#include <curses.h>
#include <signal.h>

#define CHUNKSIZE 4
#define DELAYMS 1000
#define HOSTWIDTH 12

FILE *logfile = NULL;
struct timeval start_time;
int rows = 0;
int cols = 0;
char *shell = "rsh";
int quiet = 0;

typedef struct host {
  char *hostname;
  int pid;
  int fd;
  int count;
  int load;
} host;

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

struct timeval timeval_addms(struct timeval t, int ms)
{
  t.tv_sec += ms/1000;
  t.tv_usec += (ms%1000)*1000;
  if (t.tv_usec >= 1000000) {
    t.tv_usec -= 1000000;
    t.tv_sec++;
  }
  return t;
}

char *readfile(FILE *f)
{
  char buf[CHUNKSIZE];
  char *data = NULL;
  int size = 0;
  int r;
  while (0 < (r = fread(buf,1,CHUNKSIZE,f))) {
    data = (char*)realloc(data,size+r+1);
    memcpy(&data[size],buf,r);
    size += r;
  }
  data[size++] = '\0';
  return data;
}

char *readfd(int fd)
{
  char buf[CHUNKSIZE];
  char *data = NULL;
  int size = 0;
  int r;
  while (0 < (r = read(fd,buf,CHUNKSIZE))) {
    data = (char*)realloc(data,size+r+1);
    memcpy(&data[size],buf,r);
    size += r;
  }
  data[size++] = '\0';
  return data;
}

char *read_hosts(const char *filename)
{
  char *data;
  int fd;

  if (0 > (fd = open(filename,O_RDONLY))) {
    perror(filename);
    exit(-1);
  }

  data = readfd(fd);
  close(fd);
  return data;
}

int parse_hosts(const char *data, host **hout)
{
  int nhosts;
  host *hosts;
  int start;
  int pos;
  int h;

  nhosts = 0;
  start = 0;
  for (pos = 0; '\0' != data[pos]; pos++) {
    if (('\n' == data[pos]) || ('\0' == data[pos])) {
      if (0 < pos-start)
        nhosts++;
      start = pos+1;
    }
  }

  hosts = (host*)calloc(nhosts,sizeof(host));
  h = 0;
  start = 0;
  for (pos = 0; '\0' != data[pos]; pos++) {
    if (('\n' == data[pos]) || ('\0' == data[pos])) {
      if (0 < pos-start) {
        hosts[h].hostname = (char*)malloc(pos-start+1);
        memcpy(hosts[h].hostname,&data[start],pos-start);
        hosts[h].hostname[pos-start] = '\0';
        h++;
      }
      start = pos+1;
    }
  }

  *hout = hosts;
  return nhosts;
}

int run_process(pid_t *pid, ...)
{
  va_list ap;
  int nargs = 0;
  int fds[2];

  va_start(ap,pid);
  while (NULL != va_arg(ap,char*))
    nargs++;
  va_end(ap);

  if (0 > pipe(fds)) {
    perror("pipe");
    exit(-1);
  }

  *pid = fork();
  if (0 > *pid) {
    /* fork failed */
    perror("fork");
    exit(-1);
  }
  else if (0 == *pid) {
    /* child */
    char **args = (char**)malloc((nargs+1)*sizeof(char*));
    int argno = 0;

    va_start(ap,pid);
    for (argno = 0; argno < nargs; argno++)
      args[argno] = va_arg(ap,char*);
    va_end(ap);

    args[argno] = NULL;

    dup2(fds[1],STDOUT_FILENO);
    dup2(fds[1],STDERR_FILENO);
    close(fds[0]);

    execvp(args[0],args);
    printf("Can't execute %s: %s\n",args[0],strerror(errno));
    exit(-1);
  }
  else {
    /* parent */
    close(fds[1]);
    return fds[0];
  }
  return -1;
}

void login_hosts(host *hosts, int nhosts, const char *cmd)
{
  char dir[PATH_MAX];
  int h;

  if (NULL == getcwd(dir,PATH_MAX)) {
    perror("getcwd");
    exit(-1);
  }

  for (h = 0; h < nhosts; h++) {
    char *rshcmd;
    asprintf(&rshcmd,"cd %s && %s SLAVE",dir,cmd);
    hosts[h].fd = run_process(&hosts[h].pid,shell,hosts[h].hostname,rshcmd,NULL);
  }
}

void finish(int sig)
{
  if (!quiet)
    endwin();
  if (NULL != logfile)
    fclose(logfile);
  exit(0);
}

void update_screen(host *hosts, int nhosts, int h, int load)
{
  if (quiet)
    return;

  int i;
  double ratio = ((double)(cols-HOSTWIDTH))/100.0;
  int nchars = (int)(((double)load)*ratio);
  struct timeval now;

  gettimeofday(&now,NULL);

  mvaddstr(h+2,0,hosts[h].hostname);
  if (0 < hosts[h].count) {

    if (0 > load) {
      mvaddstr(h+2,HOSTWIDTH,"?????????????????????????????????????????");
    }
    else {
      for (i = 0; i < cols-HOSTWIDTH; i++) {
        if (i < nchars) {
          attron(A_REVERSE);
        }
        else {
          attroff(A_REVERSE);
        }
        mvaddstr(h+2,HOSTWIDTH+i," ");
      }
      attroff(A_REVERSE);
    }
  }
  move(0,0);

  refresh();
}

void update_log(host *hosts, int nhosts, int h, int load)
{
  if (NULL == logfile)
    return;

  struct timeval now;
  gettimeofday(&now,NULL);
  int total_ms = timeval_diffms(start_time,now);
  int time_sec = total_ms/1000;
  int time_ms = total_ms%1000;
  if (0 < hosts[h].count) {
    if (0 > load) {
      fprintf(logfile,"%s %d.%03d error\n",
              hosts[h].hostname,time_sec,time_ms);
    }
    else {
      fprintf(logfile,"%s %d.%03d %d\n",
              hosts[h].hostname,time_sec,time_ms,load);
    }
  }
}

void update_log_total(host *hosts, int nhosts)
{
  if (NULL == logfile)
    return;

  int total = 0;

  struct timeval now;
  gettimeofday(&now,NULL);
  int total_ms = timeval_diffms(start_time,now);
  int time_sec = total_ms/1000;
  int time_ms = total_ms%1000;

  int h;
  for (h = 0; h < nhosts; h++) {
    if ((0 < hosts[h].count) && (0 <= hosts[h].load))
      total += hosts[h].load;
  }

  fprintf(logfile,"total %d.%03d %.3f\n",
          time_sec,time_ms,total/(double)nhosts);
}

void loop(host *hosts, int nhosts)
{
  int next_updatems = 0;

  while (1) {
    fd_set rfds;
    int h;
    int highest = -1;
    int s;
    int ms;
    struct timeval wait_time = { 0, 0 };
    struct timeval now;

    FD_ZERO(&rfds);
    for (h = 0; h < nhosts; h++) {
      FD_SET(hosts[h].fd,&rfds);
      if ((0 > highest) || (highest < hosts[h].fd))
        highest = hosts[h].fd;
    }

    gettimeofday(&now,NULL);
    ms = timeval_diffms(start_time,now);
    if (ms < next_updatems)
      wait_time = timeval_addms(wait_time,next_updatems-ms);

    while ((0 > (s = select(highest+1,&rfds,NULL,NULL,&wait_time))) &&
           (EINTR == errno));

    if (0 > s) {
      perror("select");
      exit(-1);
    }

    gettimeofday(&now,NULL);
    ms = timeval_diffms(start_time,now);
    if (ms >= next_updatems) {
      update_log_total(hosts,nhosts);
      next_updatems += 500;
    }

    for (h = 0; h < nhosts; h++) {
      if (FD_ISSET(hosts[h].fd,&rfds)) {
        char c;
        int r;
        while ((0 > (r = read(hosts[h].fd,&c,1))) &&
               (EINTR == errno));
        if (0 >= r)
          c = -1;
        hosts[h].count++;
        if (0 <= c)
          hosts[h].load = c;
        update_screen(hosts,nhosts,h,c);
        update_log(hosts,nhosts,h,c);
      }
    }
  }
}

int getload()
{
  static unsigned long long int oldtotal = 0;
  static unsigned long long int oldidle = 0;
  static unsigned long long int load = 0;

  FILE *f;
  int count;
  unsigned long long int user = 0;
  unsigned long long int nice = 0;
  unsigned long long int system = 0;
  unsigned long long int idle = 0;
  unsigned long long int iowait = 0;
  unsigned long long int irq = 0;
  unsigned long long int softirq = 0;
  unsigned long long int total;
  unsigned long long int totaldiff;
  unsigned long long int idlediff;

  if (NULL == (f = fopen("/proc/stat","r")))
    return -1;

  count = fscanf(f,"cpu %llu %llu %llu %llu %llu %llu %llu",
                 &user,&nice,&system,&idle,&iowait,&irq,&softirq);
  fclose(f);

  if ((4 != count) && (7 != count))
    return -1;

  total = user+nice+system+idle+iowait+irq+softirq;

  if (0 > total) {
    oldtotal = total;
    oldidle = idle;
    return -1;
  }

  if ((total < oldtotal) || (idle < oldidle))
    return load; /* happens sometimes; rounding error in kernel? */

  totaldiff = total-oldtotal;
  idlediff = idle-oldidle;

  oldtotal = total;
  oldidle = idle;

  if (0 == totaldiff)
    return -1;

  load = 100-100*idlediff/totaldiff;
  return (int)load;
}

void slave()
{
  while (1) {
    int load = getload();
    char c = (char)load;
    struct timespec time;

    write(STDOUT_FILENO,&c,1);

    time.tv_sec = DELAYMS/1000;
    time.tv_nsec = (DELAYMS%1000)*1000000;
    nanosleep(&time,NULL);

    load++;
  }
}

void usage()
{
  fprintf(stderr,"Usage: showload [OPTION] <hosts>\n");
  fprintf(stderr,"  -l FILENAME       Log load data to FILENAME\n");
  fprintf(stderr,"  -q                Quiet mode; don't show display\n");
  exit(1);
}

int main(int argc, char **argv)
{
  char *data;
  int nhosts;
  host *hosts;
  char *logfilename = NULL;
  char *hostsfilename = NULL;

  setbuf(stdout,NULL);

  if (2 > argc)
    usage();

  if (!strcmp("SLAVE",argv[1])) {
    slave();
    return 0;
  }

  int argpos;
  for (argpos = 1; argpos < argc; argpos++) {
    if (!strcmp(argv[argpos],"-l")) {
      if (argpos+1 >= argc)
        usage();
      logfilename = argv[++argpos];
    }
    else if (!strcmp(argv[argpos],"-q")) {
      quiet = 1;
    }
    else {
      hostsfilename = argv[argpos];
    }
  }

  if (NULL == hostsfilename)
    usage();

  if (NULL != getenv("SHOWLOAD_SHELL"))
    shell = getenv("SHOWLOAD_SHELL");

  data = read_hosts(hostsfilename);

  if ((NULL != logfilename) &&
      (NULL == (logfile = fopen(logfilename,"w")))) {
    perror(logfilename);
    return -1;
  }
  if (NULL != logfile)
    setbuf(logfile,NULL);

  gettimeofday(&start_time,NULL);

  signal(SIGINT,finish);

  if (!quiet) {
    initscr();
    keypad(stdscr,1);
    nonl();
    cbreak();
    noecho();
    curs_set(0);
    getmaxyx(stdscr,rows,cols);

    mvaddstr(0,0,"Host");
    mvaddstr(0,HOSTWIDTH,"0");
    mvaddstr(0,HOSTWIDTH+(cols-HOSTWIDTH)/2-1,"50");
    mvaddstr(0,cols-4,"100");

    refresh();
  }

  nhosts = parse_hosts(data,&hosts);
  login_hosts(hosts,nhosts,argv[0]);
  loop(hosts,nhosts);

  finish(0);

  return 0;
}
