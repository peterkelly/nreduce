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

#include "grammar.tab.h"
#include "gcode.h"
#include "nreduce.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <assert.h>
#include <ctype.h>
#include <stdarg.h>
#include <math.h>
#ifdef TIMING
#include <sys/time.h>
#include <time.h>
#endif

#define NREDUCE_C

#define ENGINE_INTERPRETER 0
#define ENGINE_NATIVE      1
#define ENGINE_REDUCER     2

const char *internal_functions =
"(__printer (val) (if (cons? val)"
"                   (seq (print (head val))"
"                       (__printer (tail val)))"
"                   nil))"
"(sparklist (lst) (if lst"
"                     (parhead lst (sparklist (tail lst)))"
"                     nil))"
"(spark (val) (seq (sparklist val) val))"
"(__start () (__printer (spark main)))";

extern char *yyfilename;
extern int yyfileno;
extern char *code_start;

snode *parse_root = NULL;
FILE *statsfile = NULL;

gprogram *global_program = NULL;
array *global_cpucode = NULL;

char *exec_modes[3] = { "interpreter", "native", "reducer" };

struct arguments {
  int trace;
  char *statistics;
  int profiling;
  int juststrict;
  int justgcode;
  int engine;
  char *filename;
  int strictdebug;
  int lambdadebug;
  int reorderdebug;
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
"  -g, --just-gcode        Just print compiled G-code and exit\n"
"  -e, --engine ENGINE     Use execution engine:\n"
"                          (r)educer|(i)nterpreter|(n)ative\n"
"                          (default: interpreter)\n"
"  -r, --strictness-debug  Do not run program; show strictness information for\n"
"                          all supercombinators\n"
"  -l, --lambdadebug       Do not run program; just show results of lambda\n"
"                          lifting\n"
"  -o, --reorder-debug     Do not run program; just show results of letrec\n"
"                          reordering\n");
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
      args.justgcode = 1;
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
    else {
      args.filename = argv[i];
      if (++i < argc)
        usage();
    }
  }

  if (NULL == args.filename)
    usage();
}

extern int yyparse(void);
extern FILE *yyin;

#define YY_BUF_SIZE 16384

typedef struct yy_buffer_state *YY_BUFFER_STATE;
YY_BUFFER_STATE yy_create_buffer(FILE *file,int size);
YY_BUFFER_STATE yy_scan_string(const char *str);
void yy_switch_to_buffer(YY_BUFFER_STATE new_buffer);
void yy_delete_buffer(YY_BUFFER_STATE buffer);
int yylex_destroy(void);
#if HAVE_YYLEX_DESTROY
int yylex_destroy(void);
#endif

static void close_statistics()
{
  fclose(statsfile);
  statsfile = NULL;
}

static void stream(process *proc, pntr lst)
{
  proc->streamstack = pntrstack_new();
  pntrstack_push(proc->streamstack,lst);
  while (0 < proc->streamstack->count) {
    pntr p;
    reduce(proc,proc->streamstack);

    p = pntrstack_pop(proc->streamstack);
    p = resolve_pntr(p);
    if (TYPE_NIL == pntrtype(p)) {
      /* nothing */
    }
    else if (TYPE_NUMBER == pntrtype(p)) {
      printf("%f",pntrdouble(p));
    }
    else if (TYPE_STRING == pntrtype(p)) {
      printf("%s",(char*)get_string(get_pntr(p)->field1));
    }
    else if (TYPE_CONS == pntrtype(p)) {
      pntrstack_push(proc->streamstack,get_pntr(p)->field2);
      pntrstack_push(proc->streamstack,get_pntr(p)->field1);
    }
    else if (TYPE_APPLICATION == pntrtype(p)) {
      fprintf(stderr,"Too many arguments applied to function\n");
      exit(1);
    }
    else {
      fprintf(stderr,"Bad cell type returned to printing mechanism: %s\n",cell_types[pntrtype(p)]);
      exit(1);
    }
  }
  pntrstack_free(proc->streamstack);
  proc->streamstack = NULL;
}

static int conslist_length(snode *list)
{
  int count = 0;
  while (TYPE_CONS == snodetype(list)) {
    count++;
    list = list->right;
  }
  if (TYPE_NIL != snodetype(list))
    return -1;
  return count;
}

/* static snode *conslist_item(snode *list, int index) */
/* { */
/*   while ((0 < index) && (TYPE_CONS == snodetype(list))) { */
/*     index--; */
/*     list = list->right; */
/*   } */
/*   if (TYPE_CONS != snodetype(list)) */
/*     return NULL; */
/*   return list->left; */
/* } */

static void parse_post_processing(snode *root)
{
  snode *funlist = parse_root;

  while (TYPE_CONS == snodetype(funlist)) {
    int nargs;
    scomb *sc;
    int i;
    snode *fundef = funlist->left;

/*     snode *name = conslist_item(fundef,0); */
/*     snode *args = conslist_item(fundef,1); */
/*     snode *body = conslist_item(fundef,2); */

    snode *name = fundef->left;
    snode *args = fundef->right->left;
    snode *body = fundef->right->right->left;
    fundef->right->right->left = NULL; /* don't free body */

    if ((NULL == name) || (NULL == args) || (NULL == body)) {
      fprintf(stderr,"Invalid function definition\n");
      exit(1);
    }

    if (TYPE_SYMBOL != snodetype(name)) {
      fprintf(stderr,"Invalid function name\n");
      exit(1);
    }

    nargs = conslist_length(args);
    if (0 > nargs) {
      fprintf(stderr,"Invalid argument list\n");
      exit(1);
    }

    if (NULL != get_scomb(name->name)) {
      fprintf(stderr,"Duplicate supercombinator: %s\n",name->name);
      exit(1);
    }

    sc = add_scomb(name->name);
    sc->sl = name->sl;
    sc->nargs = nargs;
    sc->argnames = (char**)calloc(sc->nargs,sizeof(char*));
    i = 0;
    while (TYPE_CONS == snodetype(args)) {
      snode *argname = args->left;
      if (TYPE_SYMBOL != snodetype(argname)) {
        fprintf(stderr,"Invalid argument name\n");
        exit(1);
      }
      sc->argnames[i++] = strdup(argname->name);
      args = args->right;
    }

    sc->body = body;

    funlist = funlist->right;
  }

  snode_free(root);
}

static void check_for_main()
{
  int gotmain = 0;

  scomb *sc;

  for (sc = scombs; sc; sc = sc->next) {
    if (!strcmp(sc->name,"main")) {
      gotmain = 1;

      if (0 != sc->nargs) {
        fprintf(stderr,"Supercombinator \"main\" must have 0 arguments\n");
        exit(1);
      }
    }
  }

  if (!gotmain) {
    fprintf(stderr,"No \"main\" function defined\n");
    exit(1);
  }
}

static void parse_file(char *filename)
{
  YY_BUFFER_STATE bufstate;
  if (NULL == (yyin = fopen(filename,"r"))) {
    perror(filename);
    exit(1);
  }

  yyfilename = filename;
  yyfileno = add_parsedfile(filename);

  bufstate = yy_create_buffer(yyin,YY_BUF_SIZE);
  yy_switch_to_buffer(bufstate);
  if (0 != yyparse())
    exit(1);
  yy_delete_buffer(bufstate);
#if HAVE_YYLEX_DESTROY
  yylex_destroy();
#endif

  fclose(yyin);

  parse_post_processing(parse_root);
}

static void parse_string(const char *str)
{
  YY_BUFFER_STATE bufstate;
  yyfilename = "(string)";
  yyfileno = add_parsedfile("(string)");

  bufstate = yy_scan_string(str);
  yy_switch_to_buffer(bufstate);

  if (0 != yyparse())
    exit(1);

  yy_delete_buffer(bufstate);
#if HAVE_YYLEX_DESTROY
  yylex_destroy();
#endif

  parse_post_processing(parse_root);
}

static void source_code_parsing()
{
  debug_stage("Source code parsing");

  parse_string(internal_functions);
  parse_file(args.filename);
  check_for_main();

  if (trace)
    print_scombs1();
}

static void fix_linenos(snode *c)
{
  switch (snodetype(c)) {
  case TYPE_CONS: {
    snode *other = c;
    while (TYPE_CONS == snodetype(other))
      other = other->left;
    c->sl.fileno = other->sl.fileno;
    c->sl.lineno = other->sl.lineno;


    fix_linenos(c->left);
    fix_linenos(c->right);
    break;
  }
  case TYPE_LAMBDA:
    fix_linenos(c->body);
    break;
  case TYPE_LETREC: {
    letrec *rec;
    for (rec = c->bindings; rec; rec = rec->next)
      fix_linenos(rec->value);
    fix_linenos(c->body);
    break;
  }
  case TYPE_BUILTIN:
  case TYPE_SYMBOL:
  case TYPE_SCREF:
  case TYPE_NIL:
  case TYPE_NUMBER:
  case TYPE_STRING:
    break;
  default:
    abort();
    break;
  }
}

static void line_fixing()
{
  scomb *sc;
  debug_stage("Line number fixing");

  for (sc = scombs; sc; sc = sc->next)
    fix_linenos(sc->body);

  if (trace)
    print_scombs1();
}

static void create_letrecs_r(snode *c)
{
  switch (snodetype(c)) {
  case TYPE_APPLICATION:
  case TYPE_CONS:

    if ((TYPE_CONS == snodetype(c)) &&
        (TYPE_SYMBOL == snodetype(c->left)) &&
        (!strcmp("let",c->left->name) ||
         !strcmp("letrec",c->left->name))) {

      letrec *defs = NULL;
      letrec **lnkptr = NULL;
      snode *iter;
      snode *let1;
      snode *let2;
      snode *body;
      letrec *lnk;

      let1 = c->right;
      parse_check(TYPE_CONS == snodetype(let1),let1,"let takes 2 parameters");

      iter = let1->left;
      while (TYPE_CONS == snodetype(iter)) {
        letrec *check;
        letrec *newlnk;

        snode *deflist;
        snode *symbol;
        snode *link1;
        snode *value;
        snode *link2;

        deflist = iter->left;
        parse_check(TYPE_CONS == snodetype(deflist),deflist,"let entry not a cons node");
        symbol = deflist->left;
        parse_check(TYPE_SYMBOL == snodetype(symbol),symbol,"let definition expects a symbol");
        link1 = deflist->right;
        parse_check(TYPE_CONS == snodetype(link1),link1,"let definition should be list of 2");
        value = link1->left;
        link1->left = NULL; /* don't free value */
        link2 = link1->right;
        parse_check(TYPE_NIL == snodetype(link2),link2,"let definition should be list of 2");

        for (check = defs; check; check = check->next) {
          if (!strcmp(check->name,symbol->name)) {
            fprintf(stderr,"Duplicate letrec definition: %s\n",check->name);
            exit(1);
          }
        }

        newlnk = (letrec*)calloc(1,sizeof(letrec));
        newlnk->name = strdup(symbol->name);
        newlnk->value = value;

        if (NULL == defs)
          defs = newlnk;
        else
          *lnkptr = newlnk;
        lnkptr = &newlnk->next;

        iter = iter->right;
      }

      let2 = let1->right;
      parse_check(TYPE_CONS == snodetype(let2),let2,"let takes 2 parameters");
      body = let2->left;
      let2->left = NULL; /* don't free body */

      for (lnk = defs; lnk; lnk = lnk->next)
        create_letrecs_r(lnk->value);

      snode_free(c->left);
      snode_free(c->right);
      c->tag = (TYPE_LETREC | (c->tag & ~TAG_MASK));
      c->bindings = defs;
      c->body = body;

      create_letrecs_r(body);
    }
    else {

      create_letrecs_r(c->left);
      create_letrecs_r(c->right);
    }

    break;
  case TYPE_LAMBDA:
    create_letrecs_r(c->body);
    break;
  case TYPE_SYMBOL:
  case TYPE_NIL:
  case TYPE_NUMBER:
  case TYPE_STRING:
  case TYPE_SCREF:
    break;
  default:
    fatal("invalid node type");
    break;
  }
}

static void create_letrecs(snode *c)
{
  create_letrecs_r(c);
}

static void letrec_creation()
{
  scomb *sc;
  debug_stage("Letrec creation");

  for (sc = scombs; sc; sc = sc->next)
    create_letrecs(sc->body);

  if (trace)
    print_scombs1();
}

static void variable_renaming()
{
  scomb *sc;
  debug_stage("Variable renaming");

  for (sc = scombs; sc; sc = sc->next)
    rename_variables(sc);

  if (trace)
    print_scombs1();
}

static void symbol_resolution()
{
  scomb *sc;
  debug_stage("Symbol resolution");

  for (sc = scombs; sc; sc = sc->next)
    resolve_refs(sc);

  if (trace)
    print_scombs1();
}

static void substitute_apps_r(snode **k)
{
  switch (snodetype(*k)) {
  case TYPE_APPLICATION:
    substitute_apps_r(&(*k)->left);
    substitute_apps_r(&(*k)->right);
    break;
  case TYPE_CONS: {
    snode *left = (*k)->left;
    snode *right = (*k)->right;

    (*k)->left = NULL; /* don't free it */

    if (TYPE_NIL != snodetype(right)) {
      snode *lst;
      snode *app;
      snode *next;
      snode *lastitem;
      parse_check(TYPE_CONS == snodetype(right),right,"expected a cons here");

      lst = right;
      app = left;
      next = lst->right;

      while (TYPE_NIL != snodetype(next)) {
        snode *item;
        snode *newapp;
        parse_check(TYPE_CONS == snodetype(next),next,"expected a cons here");
        item = lst->left;
        lst->left = NULL; /* don't free it */
        newapp = snode_new(-1,-1);
        newapp->tag = TYPE_APPLICATION;
        newapp->left = app;
        newapp->right = item;
        newapp->sl = lst->sl;
        app = newapp;

        lst = next;
        next = lst->right;
      }

      lastitem = lst->left;
      lst->left = NULL; /* don't free it */

      snode_free(*k);
      *k = snode_new(-1,-1);
      (*k)->tag = TYPE_APPLICATION;
      (*k)->left = app;
      (*k)->right = lastitem;
      (*k)->sl = lastitem->sl;

      substitute_apps_r(k);
    }
    else {
      snode_free(*k);
      *k = left;
      substitute_apps_r(k);
    }
    break;
  }
  case TYPE_LAMBDA:
    substitute_apps_r(&(*k)->body);
    break;
  case TYPE_LETREC: {
    letrec *rec;
    for (rec = (*k)->bindings; rec; rec = rec->next)
      substitute_apps_r(&rec->value);
    substitute_apps_r(&(*k)->body);
    break;
  }
  case TYPE_BUILTIN:
  case TYPE_SYMBOL:
  case TYPE_NIL:
  case TYPE_NUMBER:
  case TYPE_STRING:
  case TYPE_SCREF:
    break;
  default:
    fatal("invalid node type");
    break;
  }
}

static void substitute_apps(snode **k)
{
  substitute_apps_r(k);
}

static void app_substitution()
{
  scomb *sc;
  debug_stage("Application substitution");

  for (sc = scombs; sc; sc = sc->next)
    substitute_apps(&sc->body);

  if (trace)
    print_scombs1();
}

static void lambda_lifting()
{
  scomb *sc;
  scomb *last;
  scomb *prev;
  debug_stage("Lambda lifting");

  last = NULL;
  for (sc = scombs; sc; sc = sc->next)
    last = sc;

  prev = NULL;
  for (sc = scombs; prev != last; sc = sc->next) {
    lift(sc);
    prev = sc;
  }

  if (trace)
    print_scombs1();
}

static void app_lifting()
{
  scomb *sc;
  scomb *last;
  scomb *prev;
  debug_stage("Application lifting");

  last = NULL;
  for (sc = scombs; sc; sc = sc->next)
    last = sc;

  prev = NULL;
  for (sc = scombs; prev != last; sc = sc->next) {
    applift(sc);
    prev = sc;
  }

  if (trace)
    print_scombs1();
}

static void letrec_reordering()
{
  scomb *sc;
  debug_stage("Letrec reordering");

  for (sc = scombs; sc; sc = sc->next)
    reorder_letrecs(sc->body);

  if (trace)
    print_scombs1();
}

#if 0
static void graph_optimisation()
{
  debug_stage("Graph optimisation");

  fix_partial_applications();

  if (trace)
    print_scombs1();
}
#endif

static void gcode_compilation()
{
  debug_stage("G-code compilation");

  global_program = gprogram_new();
  compile(global_program);
  if (args.justgcode) {
    print_program(global_program,0);
    exit(0);
  }
  else if (trace) {
    print_program(global_program,1);
  }
}

#if 0
static void machine_code_generation()
{
  int i;
  debug_stage("Machine code generation");
  global_cpucode = array_new();
  jit_compile(global_program->ginstrs,global_cpucode);

  print_compiled(global_program,global_cpucode);

  for (i = 0; i < global_program->count; i++)
    global_program->ginstrs[i].codeaddr += (int)global_cpucode->data;

  code_start = (char*)global_cpucode->data;
}
#endif

static void reduction_engine()
{
  scomb *mainsc;
  cell *root;
  pntr rootp;
  process *proc = process_new();
#ifdef TIMING
  struct timeval start;
  struct timeval end;
  int ms;
#endif

  debug_stage("Reduction engine");
  mainsc = get_scomb("main");

  root = alloc_cell(proc);
  root->type = TYPE_SCREF;
  make_pntr(root->field1,mainsc);
  make_pntr(rootp,root);

#ifdef TIMING
  gettimeofday(&start,NULL);
#endif
  stream(proc,rootp);
#ifdef TIMING
  gettimeofday(&end,NULL);
  ms = (end.tv_sec - start.tv_sec)*1000 +
       (end.tv_usec - start.tv_usec)/1000;
  if (args.statistics)
    fprintf(statsfile,"Execution time: %.3fs\n",((double)ms)/1000.0);
#endif

  printf("\n");

  if (args.statistics) {
    statistics(proc,statsfile);
    close_statistics();
  }

  process_free(proc);
}

static void gcode_interpreter()
{
#ifdef TIMING
  struct timeval start;
  struct timeval end;
  int ms;
#endif


  debug_stage("G-code interpreter");

#ifdef TIMING
  gettimeofday(&start,NULL);
#endif
  run(global_program);
#ifdef TIMING
  gettimeofday(&end,NULL);
  ms = (end.tv_sec - start.tv_sec)*1000 +
       (end.tv_usec - start.tv_usec)/1000;
  if (args.statistics)
    fprintf(statsfile,"Execution time: %.3fs\n",((double)ms)/1000.0);
#endif

/*   if (args.profiling) */
/*     print_profiling(proc,global_program); */

/*   if (args.statistics) { */
/*     statistics(proc,statsfile); */
/*     close_statistics(); */
/*   } */

  gprogram_free(global_program);
}

#if 0
typedef void (jitfun)();
static void native_execution_engine()
{
  debug_stage("Native execution engine");
  ((jitfun*)global_cpucode->data)();;
  printf("\n");

  free(global_cpucode);
  gprogram_free(global_program);
}
#endif

static void open_statistics()
{
  if (NULL == (statsfile = fopen(args.statistics,"w"))) {
    perror(args.statistics);
    exit(1);
  }
}

int main(int argc, char **argv)
{
  setbuf(stdout,NULL);
  initmem();

  memset(&args,0,sizeof(args));
  parse_args(argc,argv);

  trace = args.trace;

  if (args.statistics)
    open_statistics();

  source_code_parsing();
  line_fixing();
  letrec_creation();
  variable_renaming();

  app_substitution();

  symbol_resolution();
  lambda_lifting();
  check_scombs_nosharing();
  if (args.lambdadebug) {
    print_scombs1();
    exit(0);
  }

  check_scombs_nosharing();
  app_lifting();
  check_scombs_nosharing();

  letrec_reordering();
  if (args.reorderdebug) {
    print_scombs1();
    cleanup();
    exit(0);
  }

  if (ENGINE_REDUCER == args.engine) {
    reduction_engine();
  }
  else {
/*     graph_optimisation(); */

    strictness_analysis();
    if (args.strictdebug) {
      dump_strictinfo();
      cleanup();
      exit(0);
    }

    gcode_compilation();

    if (ENGINE_INTERPRETER == args.engine) {
      gcode_interpreter();
    }
    else {
#if 0
      machine_code_generation();
      native_execution_engine();
#endif
    }
  }

  cleanup();
  return 0;
}
