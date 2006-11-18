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
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <assert.h>
#include <ctype.h>
#include <stdarg.h>
#include <math.h>
#include <unistd.h>
#include <errno.h>
#ifdef TIMING
#include <sys/time.h>
#include <time.h>
#endif

#define NREDUCE_C

#define ENGINE_INTERPRETER 0
#define ENGINE_NATIVE      1
#define ENGINE_REDUCER     2

extern const char *prelude;

char *exec_modes[3] = { "interpreter", "native", "reducer" };

struct arguments {
  int trace;
  char *statistics;
  int profiling;
  int juststrict;
  int bytecode;
  int engine;
  char *filename;
  int strictdebug;
  int lambdadebug;
  int reorderdebug;
  char *hostsfile;
  char *myaddr;
  char *masteraddr;
  int worker;
};

struct arguments args;

static void usage()
{
  printf(
"Usage: nreduce [OPTIONS] FILENAME\n"
"\n"
"where OPTIONS includes zero or more of the following:\n"
"\n"
"  -h, --help              Help (this message)\n"
"  -t, --trace             Enable tracing\n"
"  -s, --statistics FILE   Write statistics to FILE\n"
"  -p, --profiling         Show profiling information\n"
"  -j, --just-strictness   Just display strictness information\n"
"  -g, --just-bytecode     Just print compiled bytecode and exit\n"
"  -e, --engine ENGINE     Use execution engine:\n"
"                          (r)educer|(i)nterpreter|(n)ative\n"
"                          (default: interpreter)\n"
"  -r, --strictness-debug  Do not run program; show strictness information for\n"
"                          all supercombinators\n"
"  -l, --lambdadebug       Do not run program; just show results of lambda\n"
"                          lifting\n"
"  -o, --reorder-debug     Do not run program; just show results of letrec\n"
"                          reordering\n"
"  -m, --master HOSTS MYADDR  Run as master\n"
"  -w, --worker HOSTS MASTER  Run as worker\n");
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
    else if (!strcmp(argv[i],"-t") || !strcmp(argv[i],"--trace")) {
      args.trace = 1;
    }
    else if (!strcmp(argv[i],"-s") || !strcmp(argv[i],"--statistics")) {
      if (++i >= argc)
        usage();
      args.statistics = argv[i];
    }
    else if (!strcmp(argv[i],"-p") || !strcmp(argv[i],"--profiling")) {
      args.profiling = 1;
    }
    else if (!strcmp(argv[i],"-j") || !strcmp(argv[i],"--just-strictness")) {
      args.juststrict = 1;
    }
    else if (!strcmp(argv[i],"-g") || !strcmp(argv[i],"--just-gcode")) {
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
    else if (!strcmp(argv[i],"-m") || !strcmp(argv[i],"--master")) {
      if (++i >= argc)
        usage();
      args.hostsfile = argv[i];
      if (++i >= argc)
        usage();
      args.myaddr = argv[i];
    }
    else if (!strcmp(argv[i],"-w") || !strcmp(argv[i],"--worker")) {
      args.worker = 1;
    }
    else {
      args.filename = argv[i];
      if (++i < argc)
        usage();
    }
  }

  if ((NULL == args.filename) && (NULL == args.masteraddr) && !args.worker)
    usage();
}

void get_progname(const char *cmd)
{
  char *cwd = getcwd_alloc();
  printf("cwd = \"%s\"\n",cwd);
  printf("cmd = \"%s\"\n",cmd);
}

extern int arefs;
int main(int argc, char **argv)
{
  int bufsize = 1024*1024;
  void *buf = malloc(bufsize);
  source *src;
  int bcsize;
  char *bcdata;
  FILE *statsfile = NULL;

  setbuf(stdout,NULL);

  memset(&args,0,sizeof(args));
  parse_args(argc,argv);

  trace = args.trace;

  if (args.worker)
    return worker();

  if (args.myaddr)
    return master(args.hostsfile,args.myaddr,args.filename,argv[0]);

  if (args.statistics && (NULL == (statsfile = fopen(args.statistics,"w")))) {
    perror(args.statistics);
    exit(1);
  }

  src = source_new();

/*   debug_stage("Source code parsing"); */

  if (0 != source_parse_string(src,prelude,"prelude.l"))
    return -1;
  if (0 != source_parse_file(src,args.filename,""))
    return -1;

/*   if (trace) */
/*     print_scombs1(src); */

  if (0 != source_process(src))
    return -1;





  if (args.reorderdebug) {
    print_scombs1(src);
    source_free(src);
    exit(0);
  }

  if (ENGINE_REDUCER == args.engine) {
    run_reduction(src,statsfile);

    if (args.statistics)
      fclose(statsfile);
  }
  else {
    if (0 != source_compile(src,&bcdata,&bcsize))
      return -1;

    if (args.strictdebug) {
      dump_strictinfo(src);
      source_free(src);
      exit(0);
    }

    if (args.bytecode) {
      bc_print(bcdata,stdout,src,0,NULL);
      exit(0);
    }

    if (ENGINE_INTERPRETER == args.engine) {
      const bcheader *bch = (const bcheader*)bcdata;
      int *usage = NULL;
      debug_stage("Interpreter");

      if (args.profiling)
        usage = (int*)malloc(bch->nops*sizeof(int));

      run(bcdata,bcsize,statsfile,usage);

      if (args.profiling) {
        fprintf(statsfile,"\n");
        bc_print(bcdata,statsfile,src,1,usage);
        free(usage);
      }

      if (args.statistics)
        fclose(statsfile);
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
  free(buf);
  return 0;
}
