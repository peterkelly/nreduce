#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <netdb.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <sys/types.h>
#include <time.h>
#include <ucontext.h>
#include <unistd.h>

#define STACK_SIZE 65536
#define SCHEDULE_MS 250
#define BACKLOG 10
#define MAXNAME 100
#define CHUNKSIZE 1024

/* Macros */

#define llist_append(_ll,_obj) do {                       \
    assert(!(_obj)->prev && !(_obj)->next);               \
    if ((_ll)->last) {                                    \
      (_obj)->prev = (_ll)->last;                         \
      (_ll)->last->next = (_obj);                         \
      (_ll)->last = (_obj);                               \
    }                                                     \
    else {                                                \
      (_ll)->first = (_ll)->last = (_obj);                \
    }                                                     \
  }  while(0)

#define llist_remove(_ll,_obj) do {                       \
    assert((_obj)->prev || ((_ll)->first == (_obj)));     \
    assert((_obj)->next || ((_ll)->last == (_obj)));      \
    if ((_ll)->first == (_obj))                           \
      (_ll)->first = (_obj)->next;                        \
    if ((_ll)->last == (_obj))                            \
      (_ll)->last = (_obj)->prev;                         \
    if ((_obj)->next)                                     \
      (_obj)->next->prev = (_obj)->prev;                  \
    if ((_obj)->prev)                                     \
      (_obj)->prev->next = (_obj)->next;                  \
    (_obj)->next = NULL;                                  \
    (_obj)->prev = NULL;                                  \
  } while(0)

#define RETRY(expression)                                 \
    ({ long int __result;                                 \
       do __result = (long int) (expression);             \
       while (__result == -1L && errno == EINTR);         \
       __result; })

/* Type definitions */

typedef struct buffer {
  char *data;
  int alloc;
  int size;
} buffer;

typedef struct message {
  int from;
  int to;
  int size;
  int tag;
  buffer data;
  struct message *prev;
  struct message *next;
} message;

typedef struct messagelist {
  message *first;
  message *last;
} messagelist;

typedef struct thread {
  int id;
  int blocked;
  ucontext_t uc;
  messagelist messages;
  struct thread *prev;
  struct thread *next;
} thread;

typedef struct threadlist {
  thread *first;
  thread *last;
} threadlist;

typedef struct connection {
  int sock;
  buffer recvbuf;
  char name[MAXNAME];
  message *msg;
  struct connection *prev;
  struct connection *next;
} connection;

typedef struct connectionlist {
  connection *first;
  connection *last;
} connectionlist;

/* Global variables */

connectionlist connections;
threadlist running;
threadlist blocked;
thread *selectthread = NULL;
int listensock = -1;
int nextthreadid = 0;

/* Global variable used to indicate that the current thread is in a critical
   section. If a SIGALRM is received, the first thing it does it to check if
   this is set and if so, it just returns without switching threads.

   This is safe, because assigning to the variable is an atomic operation. So
   when a thread is about to enter it, one of two things will happen:

   1. The SIGALRM handler will be activated before critical is set to 1,
      in which case it is safe to switch to another thread because it's not
      yet inside the critical section.
   2. The handler will be activated *after* the variable is set, in which case
      it will see that the thread is in a critical section and avoid switching.

   In the case of SIGIO, if the thread is in a critical section it will set
   iopending to 1, and the next SIGARLM handler activation will pick this up
   and deal with the pending IO then.

   FIXME: implement the above

*/

int critical = 0;
int iopending = 0;

#define BEGIN_CRITICAL { critical = 1; {
#define END_CRITICAL critical = 0; } }

/* Utility functions */

void buffer_append(buffer *buf, const char *data, int size)
{
  if (buf->alloc < buf->size+size) {
    buf->alloc = buf->size+size;
    buf->data = realloc(buf->data,buf->alloc);
  }
  memmove(&buf->data[buf->size],data,size);
  buf->size += size;
}

void buffer_remove(buffer *buf, int count)
{
  memmove(&buf->data[0],&buf->data[count],buf->size-count);
  buf->size -= count;
}

/* Thread management */

void setup_timer(int ms)
{
  struct itimerval timer;
  timer.it_value.tv_sec = ms/1000;
  timer.it_value.tv_usec = (ms%1000)*1000;
  timer.it_interval.tv_sec = ms/1000;;
  timer.it_interval.tv_usec = (ms%1000)*1000;
  setitimer(ITIMER_REAL,&timer,NULL);
}

void *malloc_protected(int size)
{
  unsigned int pagesize = sysconf(_SC_PAGESIZE);
  unsigned int ptr;

  if (0 != size % pagesize)
    size = size+pagesize-size%pagesize;

  ptr = (unsigned int)calloc(1,size+pagesize*3);
  if (0 != ptr % pagesize)
    ptr = ptr+pagesize-ptr%pagesize;

  mprotect((void*)ptr,pagesize,PROT_NONE);
  mprotect((void*)(ptr+pagesize+size),pagesize,PROT_NONE);
  return (void*)(ptr+pagesize);
}

thread *newthread(void (*func)(thread*,void*), void *arg)
{
  thread *th = (thread*)calloc(1,sizeof(thread));
  th->id = nextthreadid++;
  getcontext(&th->uc);
  th->uc.uc_link = NULL;
  th->uc.uc_stack.ss_sp = malloc_protected(STACK_SIZE*2)+STACK_SIZE;
  th->uc.uc_stack.ss_size = STACK_SIZE;
  th->uc.uc_stack.ss_flags = 0;
  makecontext(&th->uc,(void*)func,2,th,arg);
  llist_append(&running,th);
  return th;
}

void block_thread(thread *th)
{
  assert(critical);
  assert(!th->blocked);
  llist_remove(&running,th);
  llist_append(&blocked,th);
  th->blocked = 1;

  assert(running.first);
  if (NULL == running.first->next)
    setup_timer(0);
}

void unblock_thread(thread *th)
{
  assert(critical);
  assert(th->blocked);
  assert(running.first);

  if (NULL == running.first->next)
    setup_timer(SCHEDULE_MS);

  llist_remove(&blocked,th);
  llist_append(&running,th);
  th->blocked = 0;
}

thread *curthread = NULL;

void sigalrm(int sig)
{
  thread *oldthread;

  printf("SIGALRM\n");
  if (critical)
    return;

  oldthread = curthread;
  if (NULL == running.last) {
    fprintf(stderr,"No running threads!\n");
    exit(-1);
  }

  if (NULL != curthread) {
    if (curthread->blocked)
      curthread = NULL;
    else
      curthread = curthread->prev;
  }
  if (NULL == curthread)
    curthread = running.last;

  printf("Switching to thread %d\n",curthread->id);
  if (curthread == oldthread)
    return;
  else if (NULL == oldthread)
    setcontext(&curthread->uc);
  else
    swapcontext(&oldthread->uc,&curthread->uc);
}

void sigio(int sig)
{
  printf("SIGIO\n");
  BEGIN_CRITICAL

    if (selectthread->blocked)
      unblock_thread(selectthread);

  END_CRITICAL
  raise(SIGALRM);
}

void init()
{
  signal(SIGALRM,sigalrm);
  signal(SIGIO,sigio);
  setup_timer(SCHEDULE_MS);
}

/* Communication */

int setnonblock(int fd)
{
  int flags;
  if (0 > (flags = fcntl(fd,F_GETFL))) {
    perror("fcntl(F_GETFL)");
    return -1;
  }

  flags |= O_NONBLOCK;

  if (0 > fcntl(fd,F_SETFL,flags)) {
    perror("fcntl(F_SETFL)");
    return -1;
  }

  return 0;
}

int setasync(int fd)
{
  int flags;
  if (0 > (flags = fcntl(fd,F_GETFL))) {
    perror("fcntl(F_GETFL)");
    return -1;
  }

  flags |= O_ASYNC;

  if (0 > fcntl(fd,F_SETFL,flags)) {
    perror("fcntl(F_SETFL)");
    return -1;
  }

  if (0 > fcntl(fd,F_SETOWN,getpid())) {
    perror("fcntl(F_SETOWN)");
    return -1;
  }

  return 0;
}

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

void handle_new_connection()
{
  struct sockaddr_in rmtaddr;
  int sin_size = sizeof(struct sockaddr_in);
  int sock;
  int port;
  unsigned char *addrbytes;
  struct hostent *he;
  connection *conn;

  if (0 > (sock = accept(listensock,(struct sockaddr*)&rmtaddr,&sin_size))) {
    perror("accept");
    return;
  }

  if ((0 > setnonblock(sock)) || (0 > setasync(sock))) {
    close(sock);
    return;
  }

  conn = (connection*)calloc(1,sizeof(connection));
  conn->sock = sock;

  addrbytes = (unsigned char*)&rmtaddr.sin_addr;
  port = ntohs(rmtaddr.sin_port);

  he = gethostbyaddr(&rmtaddr.sin_addr,sizeof(struct in_addr),AF_INET);
  if (NULL == he)
    snprintf(conn->name,MAXNAME,"%u.%u.%u.%u:%d",
           addrbytes[0],addrbytes[1],addrbytes[2],addrbytes[3],port);
  else
    snprintf(conn->name,MAXNAME,"%s:%d",he->h_name,port);

  llist_append(&connections,conn);

  return;
}

void close_connection(connection *conn)
{
  close(conn->sock);
  free(conn->recvbuf.data);
  llist_remove(&connections,conn);
}

int getmsg(thread *th, int *from, int *tag, int *size, char **data)
{
  message *msg;

  BEGIN_CRITICAL

    msg = th->messages.first;
    if (msg)
      llist_remove(&th->messages,msg);
    else
      block_thread(th);

  END_CRITICAL


  if (!msg) {
    /* the thread will block here until it's awoken */
    raise(SIGALRM);

    BEGIN_CRITICAL

      msg = th->messages.first;
      if (msg)
        llist_remove(&th->messages,msg);

    END_CRITICAL
  }

  if (msg) {
    *from = msg->from;
    *tag = msg->tag;
    *size = msg->size;
    *data = msg->data.data;
    free(msg);
    return 0;
  }
  else {
    return -1;
  }
}

#define HEADER_SIZE (3*sizeof(int))

thread *find_thread(int id)
{
  thread *th;
  for (th = blocked.first; th; th = th->next)
    if (th->id == id)
      return th;

  for (th = running.first; th; th = th->next)
    if (th->id == id)
      return th;

  return NULL;
}

void add_message(thread *th, message *msg)
{
  BEGIN_CRITICAL
    llist_append(&th->messages,msg);
    if (th->blocked)
      unblock_thread(th);
  END_CRITICAL
}

void process_incoming(connection *conn)
{
  int start = 0;
  while (0 < conn->recvbuf.size-start) {
    if (NULL != conn->msg) {
      int got = conn->msg->data.size;
      int remaining = conn->msg->size - got;
      int newdata = conn->recvbuf.size-start;
      int take = (newdata < remaining) ? newdata : remaining;
      buffer_append(&conn->msg->data,&conn->recvbuf.data[start],take);
      start += take;

      if (conn->msg->size == conn->msg->data.size) {
        thread *recipient = find_thread(conn->msg->to);

        if (NULL == recipient) {
          printf("Message received for unknown thread %d\n",conn->msg->to);
          free(conn->msg->data.data);
          free(conn->msg);
        }
        else {
          add_message(recipient,conn->msg);
        }

        conn->msg = NULL;
      }
    }
    else if (HEADER_SIZE < conn->recvbuf.size-start) {
      char *data = &conn->recvbuf.data[start];
      conn->msg = (message*)calloc(1,sizeof(message));
      memcpy(&conn->msg->to,data,sizeof(int));
      memcpy(&conn->msg->size,data+sizeof(int),sizeof(int));
      memcpy(&conn->msg->tag,data+2*sizeof(int),sizeof(int));
      start += HEADER_SIZE;
    }
    else {
      break;
    }
  }

  buffer_remove(&conn->recvbuf,start);
}

void select_loop(thread *th, void *arg)
{
  while (1) {
    fd_set readfds;
    fd_set writefds;
    int highest = listensock;
    connection *conn;

    FD_ZERO(&readfds);
    FD_ZERO(&writefds);

    FD_SET(listensock,&readfds);

    for (conn = connections.first; conn; conn = conn->next) {
      FD_SET(conn->sock,&readfds);
      if (highest < conn->sock)
        highest = conn->sock;
    }

    if (0 > RETRY(select(highest+1,&readfds,&writefds,NULL,NULL))) {
      perror("select");
      exit(-1);
    }

    if (FD_ISSET(listensock,&readfds))
      handle_new_connection();

    for (conn = connections.first; conn; conn = conn->next) {
      if (FD_ISSET(conn->sock,&readfds)) {
        char buf[CHUNKSIZE];
        int r = read(conn->sock,buf,CHUNKSIZE);
        if (0 > r) {
          fprintf(stderr,"read from %s: %s\n",conn->name,strerror(errno));
          close_connection(conn);
        }
        else if (0 == r) {
          printf("%s closed connection\n",conn->name);
          close_connection(conn);
        }
        else {
          buffer_append(&conn->recvbuf,buf,r);
          process_incoming(conn);
        }
      }
    }
  }
}

void test_thread(thread *th, void *arg)
{
/*   int i; */
/*   while (1) { */
/*     printf("Thread %d\n",th->id); */
/*     for (i = 0; i < 10000000; i++); */
/*   } */

  while (1) {
    int from;
    int tag;
    int size;
    char *data;
    if (0 > getmsg(th,&from,&tag,&size,&data)) {
      printf("Thread %d: no message :(\n",th->id);
    }
    else {
      printf("Thread %d: got message from %d, tag = %d, size = %d, data = \"",
             th->id,from,tag,size);
      write(STDOUT_FILENO,data,size);
      printf("\"\n");
      free(data);
    }
  }
}

void sleeper(thread *th, void *arg)
{
  while (1) {
    struct timespec t;
    t.tv_sec = 1;
    t.tv_nsec = 1;
    printf("Sleeping\n");
    nanosleep(&t,NULL);
  }
}

/* Application */

int main(int argc, char **argv)
{
  int port;
  int numthreads = 4;
  int i;
  setbuf(stdout,NULL);
  memset(&running,0,sizeof(threadlist));
  memset(&blocked,0,sizeof(threadlist));

  if (2 > argc) {
    fprintf(stderr,"Usage: threadcom <port>\n");
    return -1;
  }

  port = atoi(argv[1]);
  if (0 > (listensock = start_listening(port)))
    return -1;

  if (0 > setasync(listensock)) {
    close(listensock);
    return -1;
  }

  selectthread = newthread(select_loop,NULL);

  for (i = 0; i < numthreads; i++)
    newthread(test_thread,NULL);

  init();
  raise(SIGALRM);

/*   select_loop(NULL,NULL); */
  return 0;
}
