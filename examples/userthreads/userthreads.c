/*

Demonstration of how to implement pre-emptive multithreading in user space.

Peter Kelly <pmk@cs.adelaide.edu.au>

Linux provides a set of functions for setting and retrieving the "user context",
which contains information about the state of a running process. This consists
of the value of all CPU registers, plus other information like the base and
size of the stack. This information is maintained by the kernel and CPU while
a thread is running, but if there are multiple threads in existence then in
order to switch between them it is necessary to save a thread's context in
memory, and then load a different context.

The functions for doing this are:

int getcontext(ucontext_t *ucp);
  Stores the context of the current thread in a ucontext_t structure

int setcontext(const ucontext_t *ucp);
  Loads the specified context from the structure into the CPU registers and
  continues execution from that point. This function does not "return" as such,
  it just transfers control to the other thread.

void makecontext(ucontext_t *ucp, void *func(), int argc, ...);
  Creates a new user context, based on values taken from an existing context
  plus modifications by the user. It is necessary to set up a stack before
  calling this function. The specified function and arguments are what the
  CPU will start executing when you jump to this context.

int swapcontext (ucontext_t *oucp, ucontext_t *ucp);
  Stores the current context into oucp and loads the context stored in ucp.
  This is used to switch between threads.

The functions in the following code implement a very simple user space threading
library. An array of ucontext_t structures, threads, is used to store the
context for all suspended threads, i.e. all except the currently executing
one. When the process switches to a different thread using yield(), the current
thread's context is saved in the appropriate position in the array, and the
context from the next thread is loaded into the CPU registers.

A new thread is created by calling the newthread() function, which takes
the initial function to execute in the thread as well as a single argument.
This sets up a new user context structure and allocates a stack for the thread,
whose size is determined by STACK_SIZE. The thread will not actually begin
execution until init() has been called.

As a precautionary measure, we want to be able to catch stack overflows within
a thread. In order to do this, we actually allocate a slightly larger chunk
of memory for the stack, and divide it into three sections. The middle section
is the actual stack used by the thread, and the first and third sections are
"no go" areas which we mark using mprotect(). If a thread tries to access memory
just above or below the stack area, the process will get a SIGSEGV. The
malloc_protected() function handles this allocation, and makes sure that the
allocation occurs on page size boundaries, as required by mprotect().
Note that no free_protected() is included here, but in a real application this
would be needed, and it would need to unprotect the appropriate address ranges
and then call free() on the *original address* returned by malloc().

The yield() function is used to switch between threads. This can either be
called directly by a thread (co-operative multithreading), or from the SIGALRM
handler (pre-emptive multithreading).

The init() function sets up a timer which requests the kernel to deliver
SIGALRM to the process at a regular interval, as determined by SCHEDULE_MS.
When the signal handler is activated, the sigalrm() function is called (on
the currently thread's stack). It then calls yield() to switch to another
thread. Technically, the signal handler is still "active", and it will only
return once that thread is switched to again via another call to yield(), after
which it will continue execution of whatever that thread was doing before the
signal was received.

This example does not provide any of the other primitives generally associated
with multi-threading environments, such as locks, semaphores, condition
variables etc. These could be implemented on top of the structure here. It also
does not handle the case where a thread's top-level function returns; in a real
implementation it would need to be removed from the list of active threads and
its stack freed.

Note: When running this program with valgrind, you should use the following
command to avoid it getting confused about stack changes and reporting bogus
errors:

  valgrind --max-stackframe=32768 ./userthreads

It may also be necessary to increase the scheduling interval (e.g. to 100+ms)
to avoid spurious(?) errors from valgrind.

*/

#include <stdio.h>
#include <ucontext.h>
#include <stdlib.h>
#include <signal.h>
#include <sys/time.h>
#include <time.h>
#include <unistd.h>
#include <string.h>
#include <sys/mman.h>

#define MAXTHREADS 64
#define STACK_SIZE 65536
#define SCHEDULE_MS 10

ucontext_t threads[MAXTHREADS];
int curthread = -1;
int numthreads = 0;

void yield()
{
  int old = curthread;
  curthread = (curthread+1) % numthreads;

  if (0 > old)
    setcontext(&threads[curthread]);
  else
    swapcontext(&threads[old],&threads[curthread]);
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

void newthread(void (*func)(void), void *arg)
{
  ucontext_t *uc = &threads[numthreads];
  getcontext(uc);
  uc->uc_link = NULL;
  uc->uc_stack.ss_sp = malloc_protected(STACK_SIZE);
  uc->uc_stack.ss_size = STACK_SIZE;
  uc->uc_stack.ss_flags = 0;
  makecontext(uc,func,1,arg);
  numthreads++;
}

void sigalrm(int sig)
{
  printf("Context switch...\n");
  if (0 < numthreads)
    yield();
}

void setup_timer(int ms)
{
  struct itimerval timer;
  timer.it_value.tv_sec = ms/1000;
  timer.it_value.tv_usec = (ms%1000)*1000;
  timer.it_interval.tv_sec = ms/1000;;
  timer.it_interval.tv_usec = (ms%1000)*1000;
  setitimer(ITIMER_REAL,&timer,NULL);
}

void init()
{
  signal(SIGALRM,sigalrm);
  setup_timer(SCHEDULE_MS);
}

void userfun(int id)
{
  int x = 0;
  while (1) {
    int i;
    for (i = 0; i < 1000000; i++) {
    }
    for (i = 0; i < id; i++)
      printf("                 ");
    printf("Thread %d: x = %d\n",id,x);
    x++;
  }
}

int main(int argc, char **argv)
{
  int i;
  setbuf(stdout,NULL);

  for (i = 0; i < 5; i++)
    newthread((void*)userfun,(void*)i);

  init();
  raise(SIGALRM);
  return 0;
}
