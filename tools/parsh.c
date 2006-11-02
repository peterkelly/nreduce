#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/select.h>
#include <sys/time.h>
#include <errno.h>

#define MAXHOSTS 1024

struct buffer {
  int size;
  int allocated;
  char *data;
};

void buffer_append(struct buffer *buf, const char *data, int size)
{
  while (buf->size+size >= buf->allocated) {
    buf->allocated += 1024;
    buf->data = (char*)realloc(buf->data,buf->allocated);
  }
  memcpy(buf->data+buf->size,data,size);
  buf->size += size;
}

int outfds[MAXHOSTS];
pid_t pids[MAXHOSTS];
struct buffer hostbufs[MAXHOSTS];
char *hostnames[MAXHOSTS];
int nhosts = 0;

struct arguments {
  char *hosts;
  char *shell;
  int oneline;
  char *cmd;
  char **args;
  int nargs;
};

struct arguments arguments;

void usage()
{
  printf(
"Usage: parsh -h HOSTFILE [OPTIONS] COMMAND\n"
"\n"
"  -h, --hosts FILE         Read host list from FILE\n"
"  -s, --shell SHELL        Use SHELL instead of rsh\n"
"  -o, --oneline            Print each result on one line\n"
"\n"
"If either of -h or -s is missing, the values will be taken\n"
"from $PARSH_HOSTS and $PARSH_SHELL, respectively. The default\n"
"shell is rsh.\n");
  exit(1);
}

void parse_args(int argc, char **argv)
{
  int i;
  if (1 >= argc)
    usage();

  for (i = 1; i < argc; i++) {
    if (!strcmp(argv[i],"-h") || !strcmp(argv[i],"--hosts")) {
      if (++i >= argc)
        usage();
      arguments.hosts = argv[i];
    }
    else if (!strcmp(argv[i],"-s") || !strcmp(argv[i],"--shell")) {
      if (++i >= argc)
        usage();
      arguments.shell = argv[i];
    }
    else if (!strcmp(argv[i],"-o") || !strcmp(argv[i],"--oneline")) {
      arguments.oneline = 1;
    }
    else {
      break;
    }
  }

  arguments.args = &argv[i];
  arguments.nargs = argc-i;
  if ((0 >= arguments.nargs) || (NULL == arguments.hosts))
    usage();
}

void run_shell(char *host, int argc, char **argv)
{
  int fds[2];
  pid_t pid;

  if (0 > pipe(fds)) {
    perror("pipe");
    exit(1);
  }

  if (0 > (pid = fork())) {
    perror("fork");
    exit(1);
  }
  else if (0 == pid) {
    /* child */
    char **args = (char**)malloc((3+argc)*sizeof(char*));
    int argno = 0;
    int i;

    args[argno++] = arguments.shell;
    args[argno++] = host;
    for (i = 0; i < argc; i++)
      args[argno++] = argv[i];
    args[argno++] = NULL;

    dup2(fds[1],STDOUT_FILENO);
    dup2(fds[1],STDERR_FILENO);
    close(fds[0]);

    execvp(args[0],args);
    perror(args[0]);
    exit(-1);
  }
  else {
    /* parent */
    close(fds[1]);

    outfds[nhosts] = fds[0];
    pids[nhosts] = pid;
  }
}

int main(int argc, char **argv)
{
  char *host;
  FILE *hf;
  int i;

  memset(&arguments,0,sizeof(struct arguments));

  setbuf(stdout,NULL);

  arguments.hosts = getenv("PARSH_HOSTS");
  arguments.shell = getenv("PARSH_SHELL");
  if (NULL == arguments.shell)
    arguments.shell = "rsh";

  parse_args(argc,argv);

  memset(hostbufs,0,sizeof(MAXHOSTS*sizeof(struct buffer)));
  memset(hostnames,0,sizeof(MAXHOSTS*sizeof(struct buffer)));

  if (NULL == (hf = fopen(arguments.hosts,"r"))) {
    perror(arguments.hosts);
    exit(1);
  }

  while ((1 == fscanf(hf,"%as",&host)) && (MAXHOSTS > nhosts)) {
    hostnames[nhosts] = host;
    run_shell(host,arguments.nargs,arguments.args);
    fcntl(outfds[nhosts],F_SETFL,fcntl(outfds[nhosts],F_GETFL)|O_NONBLOCK);
    nhosts++;
  }

  while (1) {
    fd_set readfds;
    fd_set exceptfds;
    int highest = 0;
    int found = 0;
    int r;
    struct timeval tv = { tv_sec: 5, tv_usec: 0 };

    FD_ZERO(&readfds);
    FD_ZERO(&exceptfds);
    for (i = 0; i < nhosts; i++) {
      if (0 > outfds[i])
        continue;
      FD_SET(outfds[i],&readfds);
      FD_SET(outfds[i],&exceptfds);
      if (highest < outfds[i])
        highest = outfds[i];
      found++;
    }

    if (!found)
      break;

    if (0 > (r = select(highest+1,&readfds,NULL,&exceptfds,&tv))) {
      perror("select");
      exit(1);
    }

    if (0 == r) {
      printf("Waiting on:\n");
      for (i = 0; i < nhosts; i++) {
        if (0 <= outfds[i])
          printf("  %s\n",hostnames[i]);
      }
    }

    for (i = 0; i < nhosts; i++) {
      if (0 > outfds[i])
        continue;
      if (FD_ISSET(outfds[i],&readfds) || FD_ISSET(outfds[i],&exceptfds)) {
        char tmpbuf[1024];
        while (0 < (r = read(outfds[i],tmpbuf,1024))) {
          buffer_append(&hostbufs[i],tmpbuf,r);
        }
        if ((0 != r) && (EAGAIN != errno)) {
          printf("%s: error: %s\n",hostnames[i],strerror(errno));
          close(outfds[i]);
          outfds[i] = -1;
        }
        if (0 == r) {
          char zero = '\0';
          buffer_append(&hostbufs[i],&zero,1);

          close(outfds[i]);
          outfds[i] = -1;

          if (arguments.oneline) {
            int c;
            for (c = 0; c < hostbufs[i].size; c++) {
              if ('\n' == hostbufs[i].data[c])
                hostbufs[i].data[c] = ' ';
            }
            printf("%-30s %s",hostnames[i],hostbufs[i].data);
            printf("\n");
          }
          else {
            printf("=============== %s ===============\n",hostnames[i]);
            if (NULL != hostbufs[i].data)
              printf("%s",hostbufs[i].data);
          }

        }
      }
    }
  }

  fclose(hf);

  return 0;
}
