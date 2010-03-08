/*
 * This file is part of the nreduce project
 * Copyright (C) 2006-2009 Peter Kelly (pmk@cs.adelaide.edu.au)
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 *
 * $Id$
 *
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "compiler/source.h"
#include "compiler/bytecode.h"
#include "nreduce.h"
#include "runtime/runtime.h"
#include "network/node.h"
#include "runtime/chord.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <assert.h>
#include <ctype.h>
#include <stdarg.h>
#include <math.h>
#include <unistd.h>
#include <errno.h>
#include <sys/time.h>
#include <time.h>
#include <signal.h>

#ifdef HAVE_EXECINFO_H
#include <execinfo.h>
#endif

#define NREDUCE_C

extern int opt_postpone;
extern int opt_fishframes;
extern int opt_buildarray;
extern int opt_maxheap;

char *exec_modes[3] = { "interpreter", "native", "reducer" };

int max_array_size = (1 << 18);

struct arguments {
  int compileinfo;
  int nosink;
  int bytecode;
  char *filename;
  int strictdebug;
  int lambdadebug;
  int reorderdebug;
  int appendoptdebug;
  int worker;
  char *trace;
  int trace_type;
  char *port;
  char *initial;
  char *client;
  int chordtest;
  array *extra;
  char *evaluation;
};

struct arguments args;

static void usage()
{
  printf(
"Usage: nreduce [OPTIONS] FILENAME\n"
"\n"
"where OPTIONS includes zero or more of the following:\n"
"\n"
"  -h, --help               Help (this message)\n"
"  -c, --compile-stages     Print debug info about each compilation stage\n"
"  -n, --no-sinking         Disable letrec sinking\n"
"  -t, --trace DIR          Reduction engine: Print trace data to stdout and DIR\n"
"  -T, --Trace DIR          Same as -t but uses \"landscape\" mode\n"
"  -e, --engine ENGINE      Use execution engine:\n"
"                           (r)educer|(i)nterpreter|(n)ative\n"
"                           (default: interpreter)\n"
"  -v, --evaluation MODE    Use strict or lazy evaluation mode\n"
"  -w, --worker             Run as worker\n"
"  -p, --port PORT          Worker mode: listen on the specified port\n"
"  -i, --initial HOST:PORT  Worker/client mode: initial node to connect to\n"
"  --client NODESFILE CMD   Run program CMD on nodes read from NODESFILE\n"
"  --chordtest NODESFILE    Test chord implementation ('' for single-host test)\n"
"\n"
"Options for printing output of compilation stages:\n"
"(these do not actually run the program)\n"
"\n"
"  -j, --just-strictness    Print strictness information\n"
"  -g, --just-bytecode      Print compiled bytecode and exit\n"
"  -l, --lambdadebug        Print results of lambda lifting\n"
"  -o, --reorder-debug      Print results of letrec reordering\n"
"      --appopt-debug       Print results of append optimisation\n"
"  -r, --strictness-debug   Print supercombinators strictness information\n");
  exit(1);
}

void set_engine(const char *str)
{
  if (!strcmp(str,"i") || !strcmp(str,"interpreter")) {
    engine_type = ENGINE_INTERPRETER;
  }
  else if (!strcmp(str,"n") || !strcmp(str,"native")) {
    engine_type = ENGINE_NATIVE;
  }
  else if (!strcmp(str,"r") || !strcmp(str,"reducer")) {
    engine_type = ENGINE_REDUCER;
  }
  else {
    fprintf(stderr,"Invalid execution engine: %s\n",str);
    exit(1);
  }
}

void parse_args(int argc, char **argv)
{
  int i;
  if (1 >= argc)
    usage();

  for (i = 1; i < argc; i++) {
    if (!strcmp(argv[i],"-h") || !strcmp(argv[i],"--help")) {
      usage();
    }
    else if (!strcmp(argv[i],"-c") || !strcmp(argv[i],"--compile-stages")) {
      args.compileinfo = 1;
    }
    else if (!strcmp(argv[i],"-n") || !strcmp(argv[i],"--no-sinking")) {
      args.nosink = 1;
    }
    else if (!strcmp(argv[i],"-g") || !strcmp(argv[i],"--just-bytecode")) {
      args.bytecode = 1;
    }
    else if (!strcmp(argv[i],"-e") || !strcmp(argv[i],"--engine")) {
      if (++i >= argc)
        usage();
      set_engine(argv[i]);
    }
    else if (!strcmp(argv[i],"-v") || !strcmp(argv[i],"--evaluation")) {
      if (++i >= argc)
        usage();
      args.evaluation = argv[i];
    }
    else if (!strcmp(argv[i],"-r") || !strcmp(argv[i],"--strictness-debug")) {
      args.strictdebug = 1;
    }
    else if (!strcmp(argv[i],"-l") || !strcmp(argv[i],"--lambda-debug")) {
      args.lambdadebug = 1;
    }
    else if (!strcmp(argv[i],"-o") || !strcmp(argv[i],"--reorder-debug")) {
      args.reorderdebug = 1;
    }
    else if (!strcmp(argv[i],"--appopt-debug")) {
      args.appendoptdebug = 1;
    }
    else if (!strcmp(argv[i],"-w") || !strcmp(argv[i],"--worker")) {
      args.worker = 1;
    }
    else if (!strcmp(argv[i],"-t") || !strcmp(argv[i],"--trace")) {
      if (++i >= argc)
        usage();
      args.trace = argv[i];
      args.trace_type = TRACE_NORMAL;
    }
    else if (!strcmp(argv[i],"-T") || !strcmp(argv[i],"--Trace")) {
      if (++i >= argc)
        usage();
      args.trace = argv[i];
      args.trace_type = TRACE_LANDSCAPE;
    }
    else if (!strcmp(argv[i],"-p") || !strcmp(argv[i],"--port")) {
      if (++i >= argc)
        usage();
      args.port = argv[i];
    }
    else if (!strcmp(argv[i],"-i") || !strcmp(argv[i],"--initial")) {
      if (++i >= argc)
        usage();
      args.initial = argv[i];
    }
    else if (!strcmp(argv[i],"--client")) {
      if (++i >= argc)
        usage();
      args.client = argv[i];
    }
    else if (!strcmp(argv[i],"--chordtest")) {
      args.chordtest = 1;
    }
    else {
      array_append(args.extra,&argv[i],sizeof(char*));
    }
  }
}

static int chordtest_mode()
{
  run_chordtest(array_count(args.extra),(char**)args.extra->data);
  return 0;
}

static int client_mode()
{
  int r;

  if (1 > array_count(args.extra)) {
    fprintf(stderr,"No program specified\n");
    array_free(args.extra);
    return -1;
  }

  r = do_client(args.client,array_count(args.extra),(const char**)args.extra->data);
  array_free(args.extra);
  return r;
}

static int worker_mode()
{
  int r;
  int port = WORKER_PORT;

  if (NULL != args.port) {
    char *end = NULL;
    port = strtol(args.port,&end,10);
    if (('\0' == *args.port) || ('\0' != *end)) {
      fprintf(stderr,"Invalid port: %s\n",args.port);
      array_free(args.extra);
      return -1;
    }
  }

  r = worker(port,args.initial);
  array_free(args.extra);
  return r;
}

static void show_backtrace()
{
  #ifdef HAVE_EXECINFO_H
  void *btarray[1024];
  int size = backtrace(btarray,1024);
  backtrace_symbols_fd(btarray,size,STDERR_FILENO);
  #endif
}

static void sigabrt(int sig)
{
  char *str = "abort()\n";
  write(STDERR_FILENO,str,strlen(str));
  show_backtrace();
}

static void sigsegv(int sig)
{
  char *str = "Segmentation fault\n";
  write(STDERR_FILENO,str,strlen(str));
  show_backtrace();
  exit(1);
}

static void sigterm(int sig)
{
  char *str = "Received SIGTERM\n";
  write(STDERR_FILENO,str,strlen(str));
  exit(1);
}

static void init_evaluation()
{
  char *evaluation = args.evaluation;
  if (NULL == evaluation)
    evaluation = getenv("EVALUATION");
  if (NULL != evaluation) {
    if (!strcasecmp(evaluation,"strict")) {
      strict_evaluation = 1;
    }
    else if (!strcasecmp(evaluation,"lazy")) {
      strict_evaluation = 0;
    }
    else {
      fprintf(stderr,"Invalid evaluation mode: %s\n",evaluation);
      exit(1);
    }
  }

  if (strict_evaluation) {
    builtin_info[B_CONS].nstrict = builtin_info[B_CONS].nargs;
    builtin_info[B_ARRAYPREFIX].nstrict = builtin_info[B_ARRAYPREFIX].nargs;
  }
}

void parse_optimisations()
{
  char *postpone = getenv("OPT_POSTPONE");
  if (NULL != postpone)
    opt_postpone = atoi(postpone);

  char *fishframes = getenv("OPT_FISHFRAMES");
  if (NULL != fishframes)
    opt_fishframes = atoi(fishframes);

  char *buildarray = getenv("OPT_BUILDARRAY");
  if (NULL != buildarray)
    opt_buildarray = atoi(buildarray);

  char *maxheap = getenv("OPT_MAXHEAP");
  if (NULL != maxheap)
    opt_maxheap = atoi(maxheap)*1024*1024;
}

int main(int argc, char **argv)
{
  source *src;
  int bcsize;
  char *bcdata;
  struct timeval time;
  int r = 0;

  setbuf(stdout,NULL);
  signal(SIGABRT,sigabrt);
  signal(SIGSEGV,sigsegv);
  signal(SIGTERM,sigterm);
  signal(SIGPIPE,SIG_IGN);

  enable_invalid_fpe();

  if (0 != pthread_key_create(&task_key,NULL)) {
    perror("pthread_key_create");
    exit(1);
  }

  assert(0 == sizeof(frame)%8);
  assert(0 == ((int)&((frameblock*)0)->mem)%8);
  assert(0 == sizeof(cell)%8);
  assert(0 == BLOCK_START%8);
  assert(0 == BLOCK_END%8);
  assert(0 == ((int)&((carray*)0)->elements)%8);
  assert(sizeof(block) == BLOCK_END);
  assert(MAX_ARRAY_SIZE*8 <= (BLOCK_END-BLOCK_START));

  gettimeofday(&time,NULL);
  srand(time.tv_usec);

  if (getenv("ENGINE"))
    set_engine(getenv("ENGINE"));

#ifdef DISABLE_SPARKS
  printf("Sparks disabled\n");
#endif

  memset(&args,0,sizeof(args));
  args.extra = array_new(sizeof(char*),0);
  parse_args(argc,argv);
  parse_optimisations();

  init_evaluation();

  if (ENGINE_NATIVE == engine_type) {
    struct sigaction act;
    sigemptyset(&act.sa_mask);
    sigaddset(&act.sa_mask,SIGUSR1);
    sigaddset(&act.sa_mask,SIGFPE);
    act.sa_flags = SA_SIGINFO;

    act.sa_sigaction = native_sigusr1;
    sigaction(SIGUSR1,&act,NULL);

    act.sa_sigaction = native_sigfpe;
    sigaction(SIGFPE,&act,NULL);

    act.sa_sigaction = native_sigsegv;
    sigaction(SIGSEGV,&act,NULL);
  }
  else if (ENGINE_INTERPRETER == engine_type) {
    struct sigaction act;
    sigemptyset(&act.sa_mask);
    act.sa_flags = SA_SIGINFO;
    act.sa_sigaction = interpreter_sigfpe;
    sigaction(SIGFPE,&act,NULL);
  }

  if (getenv("MAX_ARRAY_SIZE"))
    max_array_size = atoi(getenv("MAX_ARRAY_SIZE"));

  compileinfo = args.compileinfo;

  if (args.chordtest)
    return chordtest_mode();

  if (args.client)
    return client_mode();

  if (args.worker)
    return worker_mode();

  if (1 > array_count(args.extra))
    usage();
  args.filename = array_item(args.extra,0,char*);
  array_remove_items(args.extra,1);

  src = source_new();

/*   debug_stage("Source code parsing"); */

  if (0 != source_parse_file(src,args.filename,""))
    return -1;

/*   if (compileinfo) */
/*     print_scombs1(src); */

  if (0 != source_process(src,args.lambdadebug,
                          args.nosink,
                          args.reorderdebug,
                          args.appendoptdebug))
    return -1;

  if (args.reorderdebug || args.lambdadebug || args.appendoptdebug) {
    print_scombs1(src);
    source_free(src);
    exit(0);
  }

  if (ENGINE_REDUCER == engine_type) {
    run_reduction(src,args.trace,args.trace_type,args.extra);
  }
  else {
    if (0 != source_compile(src,&bcdata,&bcsize))
      return -1;

    if (args.strictdebug) {
      dump_strictinfo(src);
      free(bcdata);
      source_free(src);
      exit(0);
    }

    if (args.bytecode) {
      bc_print(bcdata,stdout,src,0,NULL);
      exit(0);
    }

    debug_stage("Execution");
    r = standalone(bcdata,bcsize,array_count(args.extra),(const char**)args.extra->data);
    free(bcdata);
  }

  source_free(src);
  array_free(args.extra);

  return r;
}
