/*
 * This file is part of the GridXSLT project
 * Copyright (C) 2006 Peter Kelly (pmk@cs.adelaide.edu.au)
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
#include "runtime/node.h"
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

#define NREDUCE_C

#define ENGINE_INTERPRETER 0
#define ENGINE_NATIVE      1
#define ENGINE_REDUCER     2

#define WORKER_PORT 2000

char *exec_modes[3] = { "interpreter", "native", "reducer" };

int max_array_size = (1 << 18);

struct arguments {
  int compileinfo;
  int nopartialsink;
  int bytecode;
  int engine;
  char *filename;
  int strictdebug;
  int lambdadebug;
  int reorderdebug;
  char *partial;
  int worker;
  char *trace;
  int trace_type;
  char *address;
  char *client;
  char *nodetest;
  array *extra;
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
"  -n, --no-partial         Disable partial evaluation/letrec sinking\n"
"  -t, --trace DIR          Reduction engine: Print trace data to stdout and DIR\n"
"  -T, --Trace DIR          Same as -t but uses \"landscape\" mode\n"
"  -e, --engine ENGINE      Use execution engine:\n"
"                           (r)educer|(i)nterpreter|(n)ative\n"
"                           (default: interpreter)\n"
"  -a, --partial-eval SCOMB Perform partial evaluation of a supercombinator\n"
"  -w, --worker             Run as worker\n"
"  -d, --address IP:PORT    Listen on the specified IP address and port no\n"
"  --client NODESFILE CMD   Run program CMD on nodes read from NODESFILE\n"
"  --nodetest IP:PORT       Run as test node, listening on IP:PORT\n"
"\n"
"Options for printing output of compilation stages:\n"
"(these do not actually run the program)\n"
"\n"
"  -j, --just-strictness    Print strictness information\n"
"  -g, --just-bytecode      Print compiled bytecode and exit\n"
"  -l, --lambdadebug        Print results of lambda lifting\n"
"  -o, --reorder-debug      Print results of letrec reordering\n"
"  -r, --strictness-debug   Print supercombinators strictness information\n");
  exit(1);
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
    else if (!strcmp(argv[i],"-n") || !strcmp(argv[i],"--no-partial")) {
      args.nopartialsink = 1;
    }
    else if (!strcmp(argv[i],"-g") || !strcmp(argv[i],"--just-bytecode")) {
      args.bytecode = 1;
    }
    else if (!strcmp(argv[i],"-e") || !strcmp(argv[i],"--engine")) {
      if (++i >= argc)
        usage();
      if (!strcmp(argv[i],"i") || !strcmp(argv[i],"interpreter"))
        args.engine = ENGINE_INTERPRETER;
      else if (!strcmp(argv[i],"n") || !strcmp(argv[i],"native"))
        args.engine = ENGINE_NATIVE;
      else if (!strcmp(argv[i],"r") || !strcmp(argv[i],"reducer"))
        args.engine = ENGINE_REDUCER;
      else
        usage();
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
    else if (!strcmp(argv[i],"-a") || !strcmp(argv[i],"--partial-eval")) {
      if (++i >= argc)
        usage();
      args.partial = argv[i];
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
    else if (!strcmp(argv[i],"-d") || !strcmp(argv[i],"--address")) {
      if (++i >= argc)
        usage();
      args.address = argv[i];
    }
    else if (!strcmp(argv[i],"--client")) {
      if (++i >= argc)
        usage();
      args.client = argv[i];
    }
    else if (!strcmp(argv[i],"--nodetest")) {
      if (++i >= argc)
        usage();
      args.nodetest = argv[i];
    }
    else {
      array_append(args.extra,&argv[i],sizeof(char*));
    }
  }

/*   if ((NULL == args.filename) && !args.worker) */
/*     usage(); */
}

void get_progname(const char *cmd)
{
  char *cwd = getcwd_alloc();
  printf("cwd = \"%s\"\n",cwd);
  printf("cmd = \"%s\"\n",cmd);
}

static int nodetest_mode()
{
  int r;
  char *host = NULL;
  int port = WORKER_PORT;

  if (0 != parse_address(args.nodetest,&host,&port)) {
    fprintf(stderr,"Invalid node address: %s\n",args.nodetest);
    exit(1);
  }

  r = nodetest(host,port);

  free(host);
  array_free(args.extra);
  return r;
}

static int client_mode()
{
  int r;

  if (1 > array_count(args.extra)) {
    fprintf(stderr,"No program specified\n");
    array_free(args.extra);
    return -1;
  }

  r = do_client(args.client,array_item(args.extra,0,char*));
  array_free(args.extra);
  return r;
}

static int worker_mode()
{
  int r;
  char *host = NULL;
  int port = WORKER_PORT;

  if ((NULL != args.address) && (0 != parse_address(args.address,&host,&port))) {
    fprintf(stderr,"Invalid listening address: %s\n",args.address);
    exit(1);
  }

  r = worker(host,port,NULL,0);
  free(host);
  array_free(args.extra);
  return r;
}

extern int arefs;
int main(int argc, char **argv)
{
  source *src;
  int bcsize;
  char *bcdata;
  struct timeval time;
  int r = 0;

  setbuf(stdout,NULL);

  gettimeofday(&time,NULL);
  srand(time.tv_usec);

  memset(&args,0,sizeof(args));
  args.extra = array_new(sizeof(char*),0);
  parse_args(argc,argv);

/*   if (NULL != getenv("DISABLE_PARTIAL_EVAL")) */
/*     args.nopartialsink = 1; */

  if (getenv("MAX_ARRAY_SIZE"))
    max_array_size = atoi(getenv("MAX_ARRAY_SIZE"));

  /* TEMP: disable partial evaluation */
  args.nopartialsink = 1;

  compileinfo = args.compileinfo;

  if (args.nodetest)
    return nodetest_mode();

  if (args.client)
    return client_mode();

  if (args.worker)
    return worker_mode();

  if (1 != array_count(args.extra))
    usage();

  args.filename = array_item(args.extra,0,char*);
  array_free(args.extra);

  src = source_new();

/*   debug_stage("Source code parsing"); */

  if (0 != source_parse_file(src,args.filename,""))
    return -1;

/*   if (compileinfo) */
/*     print_scombs1(src); */

  if (0 != source_process(src,args.partial || args.lambdadebug,
                          args.nopartialsink,
                          args.reorderdebug))
    return -1;

  if (args.reorderdebug || args.lambdadebug) {
    print_scombs1(src);
    source_free(src);
    exit(0);
  }

  if (NULL != args.partial) {
    debug_partial(src,args.partial,args.trace,args.trace_type);
  }
  else if (ENGINE_REDUCER == args.engine) {
    run_reduction(src,args.trace,args.trace_type);
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

    if (ENGINE_INTERPRETER == args.engine) {
      debug_stage("Interpreter");
      r = worker("127.0.0.1",0,bcdata,bcsize);
    }
    else {
#if 0
      machine_code_generation();
      native_execution_engine();
#endif
    }
    free(bcdata);
  }

  source_free(src);
  return r;
}
