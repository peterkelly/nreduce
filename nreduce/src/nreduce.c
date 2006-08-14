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
"                   (if (__printer (head val))"
"                       nil"
"                       (__printer (tail val)))"
"                   (print val)))"
"(__start () (__printer main))";

extern char *yyfilename;
extern cell *parse_root;
extern char *code_start;
extern int resolve_ind_offset;
extern array *oldnames;
extern stack *streamstack;

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

void usage()
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

extern int yyparse();
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

void stream(cell *c)
{
  streamstack = stack_new();
  stack_push(streamstack,c);
  while (0 < streamstack->count) {
    cell *c;
    reduce(streamstack);

    c = stack_pop(streamstack);
    c = resolve_ind(c);
    if (TYPE_NIL == celltype(c)) {
      /* nothing */
    }
    else if (TYPE_NUMBER == celltype(c)) {
      printf("%f",celldouble(c));
    }
    else if (TYPE_STRING == celltype(c)) {
      printf("%s",(char*)c->field1);
    }
    else if (TYPE_CONS == celltype(c)) {
      stack_push(streamstack,(cell*)c->field2);
      stack_push(streamstack,(cell*)c->field1);
    }
    else if (TYPE_APPLICATION == celltype(c)) {
      fprintf(stderr,"Too many arguments applied to function\n");
      exit(1);
    }
    else {
      fprintf(stderr,"Bad cell type returned to printing mechanism: %s\n",cell_types[celltype(c)]);
      exit(1);
    }
  }
  stack_free(streamstack);
  streamstack = NULL;
}

void print_compiled(gprogram *gp, array *cpucode)
{
  FILE *f = fopen("compiled.s","w");
  int i;
  int prev = 0;
  int *codesizes;
  int gaddr;

  codesizes = (int*)calloc(gp->count,sizeof(int));

  gaddr = 1;
  for (i = 0; i < cpucode->size; i++) {
    if (gp->ginstrs[gaddr].codeaddr == i) {
      codesizes[gaddr-1] = i-prev;
      prev = i;
      gaddr++;
    }
  }

  fprintf(f,"\t.file \"compiled.c\"\n");
  fprintf(f,".globl _start\n");
  fprintf(f,"\t.section\t.rodata\n");
  fprintf(f,"\t.type\t_start,@object\n");
  fprintf(f,"\t.size\t_start,%d\n",resolve_ind_offset);
  fprintf(f,"_start:\n");

  gaddr = 0;
  for (i = 0; i < cpucode->size; i++) {
    char x = ((char*)cpucode->data)[i] & 0xFF;
    if ((gp->ginstrs[gaddr].codeaddr == i) && (0 < i)) {
      assert(OP_LAST != gp->ginstrs[gaddr].opcode);

      fprintf(f,".globl i%06d\n",gaddr);
      fprintf(f,"\t.type\ti%06d,@object\n",gaddr);
      fprintf(f,"\t.size\ti%06d,%d\n",gaddr,codesizes[gaddr]);
      fprintf(f,"i%06d:\n",gaddr);

      gaddr++;
    }
    else if (resolve_ind_offset == i) {
      fprintf(f,".globl resolve_ind\n");
      fprintf(f,"\t.type\tresolve_ind,@object\n");
      fprintf(f,"\t.size\tresolve_ind,%d\n",gp->ginstrs[0].codeaddr-resolve_ind_offset);
      fprintf(f,"resolve_ind:\n");
    }
    fprintf(f,"\t.byte\t%d\n",x);
  }

  fclose(f);
}

int conslist_length(cell *list)
{
  int count = 0;
  while (TYPE_CONS == celltype(list)) {
    count++;
    list = (cell*)list->field2;
  }
  if (TYPE_NIL != celltype(list))
    return -1;
  return count;
}

cell *conslist_item(cell *list, int index)
{
  while ((0 < index) && (TYPE_CONS == celltype(list))) {
    index--;
    list = (cell*)list->field2;
  }
  if (TYPE_CONS != celltype(list))
    return NULL;
  return (cell*)list->field1;
}

void parse_post_processing(cell *root)
{
  cell *funlist = parse_root;

  while (TYPE_CONS == celltype(funlist)) {
    char *namestr;
    int nargs;
    scomb *sc;
    int i;
    cell *fundef = (cell*)funlist->field1;

    cell *name = conslist_item(fundef,0);
    cell *args = conslist_item(fundef,1);
    cell *body = conslist_item(fundef,2);
    if ((NULL == name) || (NULL == args) || (NULL == body)) {
      fprintf(stderr,"Invalid function definition\n");
      exit(1);
    }

    if (TYPE_SYMBOL != celltype(name)) {
      fprintf(stderr,"Invalid function name\n");
      exit(1);
    }
    namestr = (char*)name->field1;

    nargs = conslist_length(args);
    if (0 > nargs) {
      fprintf(stderr,"Invalid argument list\n");
      exit(1);
    }

    if (NULL != get_scomb(namestr)) {
      fprintf(stderr,"Duplicate supercombinator: %s\n",namestr);
      exit(1);
    }

    sc = add_scomb(namestr);
    sc->nargs = nargs;
    sc->argnames = (char**)calloc(sc->nargs,sizeof(char*));
    i = 0;
    while (TYPE_CONS == celltype(args)) {
      cell *argname = (cell*)args->field1;
      if (TYPE_SYMBOL != celltype(argname)) {
        fprintf(stderr,"Invalid argument name\n");
        exit(1);
      }
      sc->argnames[i++] = (char*)strdup((char*)argname->field1);
      args = (cell*)args->field2;
    }

    sc->body = body;

    funlist = (cell*)funlist->field2;
  }
}

void check_for_main()
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

void parse_file(char *filename)
{
  YY_BUFFER_STATE bufstate;
  if (NULL == (yyin = fopen(filename,"r"))) {
    perror(filename);
    exit(1);
  }

  yyfilename = filename;

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

void parse_string(const char *str)
{
  YY_BUFFER_STATE bufstate;
  yyfilename = "(string)";

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

void source_code_parsing()
{
  debug_stage("Source code parsing");

  parse_string(internal_functions);
  parse_file(args.filename);
  check_for_main();

  if (trace)
    print_scombs1();
}

void create_letrecs_r(cell *c)
{
  assert((c == globnil) || !(c->tag & FLAG_PROCESSED)); /* should not be a graph */
  c->tag |= FLAG_PROCESSED;
  switch (celltype(c)) {
  case TYPE_APPLICATION:
  case TYPE_CONS:

    if ((TYPE_CONS == celltype(c)) &&
        (TYPE_SYMBOL == celltype((cell*)c->field1)) &&
        (!strcmp("let",(char*)((cell*)c->field1)->field1) ||
         !strcmp("letrec",(char*)((cell*)c->field1)->field1))) {

      letrec *defs = NULL;
      letrec **lnkptr = NULL;
      cell *iter;
      cell *let1;
      cell *let2;
      cell *body;
      letrec *lnk;

      let1 = (cell*)c->field2;
      parse_check(TYPE_CONS == celltype(let1),let1,"let takes 2 parameters");

      iter = (cell*)let1->field1;
      while (TYPE_CONS == celltype(iter)) {
        letrec *check;
        letrec *newlnk;

        cell *deflist;
        cell *symbol;
        cell *link1;
        cell *value;
        cell *link2;

        deflist = (cell*)iter->field1;
        parse_check(TYPE_CONS == celltype(deflist),deflist,"let entry not a cons cell");
        symbol = (cell*)deflist->field1;
        parse_check(TYPE_SYMBOL == celltype(symbol),symbol,"let definition expects a symbol");
        link1 = (cell*)deflist->field2;
        parse_check(TYPE_CONS == celltype(link1),link1,"let definition should be list of 2");
        value = (cell*)link1->field1;
        link2 = (cell*)link1->field2;
        parse_check(TYPE_NIL == celltype(link2),link2,"let definition should be list of 2");

        for (check = defs; check; check = check->next) {
          if (!strcmp(check->name,(char*)symbol->field1)) {
            fprintf(stderr,"Duplicate letrec definition: %s\n",check->name);
            exit(1);
          }
        }

        newlnk = (letrec*)calloc(1,sizeof(letrec));
        newlnk->name = strdup((char*)symbol->field1);
        newlnk->value = value;

        if (NULL == defs)
          defs = newlnk;
        else
          *lnkptr = newlnk;
        lnkptr = &newlnk->next;

        iter = (cell*)iter->field2;
      }

      let2 = (cell*)let1->field2;
      parse_check(TYPE_CONS == celltype(let2),let2,"let takes 2 parameters");
      body = (cell*)let2->field1;

      for (lnk = defs; lnk; lnk = lnk->next) {
        create_letrecs_r(lnk->value);
      }

      c->tag = (TYPE_LETREC | (c->tag & ~TAG_MASK));
      c->field1 = defs;
      c->field2 = body;

      create_letrecs_r(body);
    }
    else {

      create_letrecs_r((cell*)c->field1);
      create_letrecs_r((cell*)c->field2);
    }

    break;
  case TYPE_LAMBDA:
    create_letrecs_r((cell*)c->field2);
    break;
  case TYPE_SYMBOL:
  case TYPE_NIL:
  case TYPE_NUMBER:
  case TYPE_STRING:
  case TYPE_SCREF:
    break;
  default:
    assert(!"invalid cell type");
    break;
  }
}

void create_letrecs(cell *c)
{
  cleargraph(c,FLAG_PROCESSED);
  create_letrecs_r(c);
}

void letrec_creation()
{
  scomb *sc;
  debug_stage("Letrec creation");

  for (sc = scombs; sc; sc = sc->next)
    create_letrecs(sc->body);

  if (trace)
    print_scombs1();
}

void variable_renaming()
{
  scomb *sc;
  debug_stage("Variable renaming");

  for (sc = scombs; sc; sc = sc->next)
    rename_variables(sc);

  if (trace)
    print_scombs1();
}

void symbol_resolution()
{
  scomb *sc;
  debug_stage("Symbol resolution");

  for (sc = scombs; sc; sc = sc->next)
    resolve_refs(sc);

  if (trace)
    print_scombs1();
}

void substitute_apps_r(cell **k)
{
  assert((*k == globnil) || !((*k)->tag & FLAG_PROCESSED)); /* should not be a graph */
  (*k)->tag |= FLAG_PROCESSED;
  switch (celltype(*k)) {
  case TYPE_APPLICATION:
    substitute_apps_r((cell**)&(*k)->field1);
    substitute_apps_r((cell**)&(*k)->field2);
    break;
  case TYPE_CONS: {
    cell *left = (cell*)(*k)->field1;
    cell *right = (cell*)(*k)->field2;

    if (TYPE_NIL != celltype(right)) {
      cell *lst;
      cell *app;
      cell *next;
      cell *lastitem;
      parse_check(TYPE_CONS == celltype(right),right,"expected a cons here");

      lst = right;
      app = left;
      next = (cell*)lst->field2;

      while (TYPE_NIL != celltype(next)) {
        cell *item;
        cell *newapp;
        parse_check(TYPE_CONS == celltype(next),next,"expected a cons here");
        item = (cell*)lst->field1;
        newapp = alloc_cell();
        newapp->tag = TYPE_APPLICATION;
        newapp->field1 = app;
        newapp->field2 = item;
        app = newapp;

        lst = next;
        next = (cell*)lst->field2;
      }

      lastitem = (cell*)lst->field1;
      (*k)->tag = TYPE_APPLICATION;
      (*k)->field1 = app;
      (*k)->field2 = lastitem;

      substitute_apps_r(k);
    }
    else {
      (*k)->tag &= ~FLAG_PROCESSED;
      *k = (cell*)(*k)->field1;
      substitute_apps_r(k);
    }
    break;
  }
  case TYPE_LAMBDA:
    substitute_apps_r((cell**)&(*k)->field2);
    break;
  case TYPE_LETREC: {
    letrec *rec;
    for (rec = (letrec*)(*k)->field1; rec; rec = rec->next)
      substitute_apps_r(&rec->value);
    substitute_apps_r((cell**)&(*k)->field2);
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
    assert(!"invalid cell type");
    break;
  }
}

void substitute_apps(cell **k)
{
  cleargraph(*k,FLAG_PROCESSED);
  substitute_apps_r(k);
}

void app_substitution()
{
  scomb *sc;
  debug_stage("Application substitution");

  for (sc = scombs; sc; sc = sc->next)
    substitute_apps(&sc->body);

  if (trace)
    print_scombs1();
}

void lambda_lifting()
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

void app_lifting()
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

void letrec_reordering()
{
  scomb *sc;
  debug_stage("Letrec reordering");

  for (sc = scombs; sc; sc = sc->next)
    reorder_letrecs(sc->body);

  if (trace)
    print_scombs1();
}

void graph_optimisation()
{
  debug_stage("Graph optimisation");

  fix_partial_applications();

  if (trace)
    print_scombs1();
}

void gcode_compilation()
{
  debug_stage("G-code compilation");

  global_program = gprogram_new();
  compile(global_program);
  if (args.justgcode) {
    print_program(global_program,0,0);
    exit(0);
  }
  else if (trace) {
    print_program(global_program,1,0);
  }
}

void machine_code_generation()
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

void reduction_engine()
{
  scomb *mainsc;
  cell *root;
#ifdef TIMING
  struct timeval start;
  struct timeval end;
  int ms;
#endif

  debug_stage("Reduction engine");
  mainsc = get_scomb("main");

  root = alloc_cell2(TYPE_SCREF,mainsc,NULL);
#ifdef TIMING
  gettimeofday(&start,NULL);
#endif
  stream(root);
#ifdef TIMING
  gettimeofday(&end,NULL);
  ms = (end.tv_sec - start.tv_sec)*1000 +
       (end.tv_usec - start.tv_usec)/1000;
  if (args.statistics)
    fprintf(statsfile,"Execution time: %.3fs\n",((double)ms)/1000.0);
#endif

  printf("\n");
}

void gcode_interpreter()
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
  execute(global_program);
#ifdef TIMING
  gettimeofday(&end,NULL);
  ms = (end.tv_sec - start.tv_sec)*1000 +
       (end.tv_usec - start.tv_usec)/1000;
  if (args.statistics)
    fprintf(statsfile,"Execution time: %.3fs\n",((double)ms)/1000.0);
#endif

  printf("\n");
  if (args.profiling)
    print_profiling(global_program);
  gprogram_free(global_program);
}

typedef void (jitfun)();
void native_execution_engine()
{
  debug_stage("Native execution engine");
  ((jitfun*)global_cpucode->data)();;
  printf("\n");

  free(global_cpucode);
  gprogram_free(global_program);
}

void open_statistics()
{
  if (NULL == (statsfile = fopen(args.statistics,"w"))) {
    perror(args.statistics);
    exit(1);
  }
}

void close_statistics()
{
  fclose(statsfile);
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
      exit(0);
    }

    gcode_compilation();

    if (ENGINE_INTERPRETER == args.engine) {
      gcode_interpreter();
    }
    else {
      machine_code_generation();
      native_execution_engine();
    }
  }

  collect();
  if (args.statistics) {
    statistics(statsfile);
    close_statistics();
  }

  cleanup();
  return 0;
}
