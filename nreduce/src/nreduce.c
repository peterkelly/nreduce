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
#include <argp.h>
#include <sys/time.h>
#include <time.h>

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

static char doc[] = "nreduce";

const char *argp_program_version = "nreduce 0.1";

const char *argp_program_bug_address = "pmk@cs.adelaide.edu.au";

static char args_doc[] = "FILENAME\n";

static struct argp_option options[] = {
  {"trace",            't', NULL,    0, "Enable tracing" },
  {"statistics",       's', "FILE",  0, "Write statistics to FILE" },
  {"profiling",        'p', NULL,    0, "Show profiling information" },
  {"just-strictness",  'j', NULL,    0, "Just display strictness information" },
  {"just-gcode",       'g', NULL,    0, "Just print compiled G-code and exit" },
  {"engine",           'e', "ENGINE",0, "Use execution engine: (r)educer|(i)nterpreter|(n)ative\n"
                                        "(default: interpreter)" },
  {"strictness-debug", 'r', NULL,    0, "Do not run program; show strictness information for all "
                                        "supercombinators" },
  {"lambda-debug",     'l', NULL,    0, "Do not run program; just show results of lambda lifting" },
  { 0 }
};

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
};

struct arguments args;

static error_t parse_opt (int key, char *arg, struct argp_state *state)
{
  struct arguments *arguments = (struct arguments*)state->input;

  switch (key) {
  case 't':
    arguments->trace = 1;
    break;
  case 's':
    arguments->statistics = arg;
    break;
  case 'p':
    arguments->profiling = 1;
    break;
  case 'j':
    arguments->juststrict = 1;
    break;
  case 'g':
    arguments->justgcode = 1;
    break;
  case 'e':
    if (!strncasecmp(arg,"i",1))
      arguments->engine = ENGINE_INTERPRETER;
    else if (!strncasecmp(arg,"n",1))
      arguments->engine = ENGINE_NATIVE;
    else if (!strncasecmp(arg,"r",1))
      arguments->engine = ENGINE_REDUCER;
    else
      argp_usage (state);
    break;
  case 'r':
    arguments->strictdebug = 1;
    break;
  case 'l':
    arguments->lambdadebug = 1;
    break;
  case ARGP_KEY_ARG:
    if (0 == state->arg_num)
      arguments->filename = arg;
    else
      argp_usage (state);
    break;
  case ARGP_KEY_END:
    if (1 > state->arg_num)
      argp_usage (state);
    break;
  default:
    return ARGP_ERR_UNKNOWN;
  }
  return 0;
}

static struct argp argp = { options, parse_opt, args_doc, doc };

extern int yyparse();
extern FILE *yyin;

#define YY_BUF_SIZE 16384

typedef struct yy_buffer_state *YY_BUFFER_STATE;
YY_BUFFER_STATE yy_create_buffer(FILE *file,int size);
YY_BUFFER_STATE yy_scan_string(const char *str);
void yy_switch_to_buffer(YY_BUFFER_STATE new_buffer);
void yy_delete_buffer(YY_BUFFER_STATE buffer);
int yylex_destroy(void);

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
    else if (TYPE_INT == celltype(c)) {
      printf("%d",(int)c->field1);
    }
    else if (TYPE_DOUBLE == celltype(c)) {
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
    char *namestr = (char*)name->field1;

    int nargs = conslist_length(args);
    if (0 > nargs) {
      fprintf(stderr,"Invalid argument list\n");
      exit(1);
    }

    if (NULL != get_scomb(namestr)) {
      fprintf(stderr,"Duplicate supercombinator: %s\n",namestr);
      exit(1);
    }

    scomb *sc = add_scomb(namestr);
    sc->nargs = nargs;
    sc->argnames = (char**)calloc(sc->nargs,sizeof(char*));
    int i = 0;
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
  if (NULL == (yyin = fopen(filename,"r"))) {
    perror(filename);
    exit(1);
  }

  yyfilename = filename;

  YY_BUFFER_STATE bufstate = yy_create_buffer(yyin,YY_BUF_SIZE);
  yy_switch_to_buffer(bufstate);
  if (0 != yyparse())
    exit(1);
  yy_delete_buffer(bufstate);
  yylex_destroy();

  fclose(yyin);

  parse_post_processing(parse_root);
}

void parse_string(const char *str)
{

  yyfilename = "(string)";

  YY_BUFFER_STATE bufstate = yy_scan_string(str);
  yy_switch_to_buffer(bufstate);

  if (0 != yyparse())
    exit(1);

  yy_delete_buffer(bufstate);
  yylex_destroy();

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

      cell *let1 = (cell*)c->field2;
      parse_check(TYPE_CONS == celltype(let1),let1,"let takes 2 parameters");

      cell *iter = (cell*)let1->field1;
      while (TYPE_CONS == celltype(iter)) {
        cell *deflist = (cell*)iter->field1;
        parse_check(TYPE_CONS == celltype(deflist),deflist,"let entry not a cons cell");
        cell *symbol = (cell*)deflist->field1;
        parse_check(TYPE_SYMBOL == celltype(symbol),symbol,"let definition expects a symbol");
        cell *link1 = (cell*)deflist->field2;
        parse_check(TYPE_CONS == celltype(link1),link1,"let definition should be list of 2");
        cell *value = (cell*)link1->field1;
        cell *link2 = (cell*)link1->field2;
        parse_check(TYPE_NIL == celltype(link2),link2,"let definition should be list of 2");

        letrec *check;
        for (check = defs; check; check = check->next) {
          if (!strcmp(check->name,(char*)symbol->field1)) {
            fprintf(stderr,"Duplicate letrec definition: %s\n",check->name);
            exit(1);
          }
        }

        letrec *newlnk = (letrec*)calloc(1,sizeof(letrec));
        newlnk->name = strdup((char*)symbol->field1);
        newlnk->value = value;

        if (NULL == defs)
          defs = newlnk;
        else
          *lnkptr = newlnk;
        lnkptr = &newlnk->next;

        iter = (cell*)iter->field2;
      }

      cell *let2 = (cell*)let1->field2;
      parse_check(TYPE_CONS == celltype(let2),let2,"let takes 2 parameters");
      cell *body = (cell*)let2->field1;

      letrec *lnk;
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
  case TYPE_INT:
  case TYPE_DOUBLE:
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
  debug_stage("Letrec creation");

  scomb *sc;
  for (sc = scombs; sc; sc = sc->next)
    create_letrecs(sc->body);

  if (trace)
    print_scombs1();
}

void variable_renaming()
{
  debug_stage("Variable renaming");

  scomb *sc;
  for (sc = scombs; sc; sc = sc->next)
    rename_variables(sc);

  if (trace)
    print_scombs1();
}

void symbol_resolution()
{
  debug_stage("Symbol resolution");

  scomb *sc;
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
      parse_check(TYPE_CONS == celltype(right),right,"expected a cons here");

      cell *lst = right;
      cell *app = left;
      cell *next = (cell*)lst->field2;

      while (TYPE_NIL != celltype(next)) {
        parse_check(TYPE_CONS == celltype(next),next,"expected a cons here");
        cell *item = (cell*)lst->field1;
        cell *newapp = alloc_cell();
        newapp->tag = TYPE_APPLICATION;
        newapp->field1 = app;
        newapp->field2 = item;
        app = newapp;

        lst = next;
        next = (cell*)lst->field2;
      }

      cell *lastitem = (cell*)lst->field1;
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
  case TYPE_INT:
  case TYPE_DOUBLE:
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
  debug_stage("Application substitution");

  scomb *sc;
  for (sc = scombs; sc; sc = sc->next)
    substitute_apps(&sc->body);

  if (trace)
    print_scombs1();
}

void lambda_lifting()
{
  debug_stage("Lambda lifting");

  scomb *sc;
  scomb *last = NULL;
  for (sc = scombs; sc; sc = sc->next)
    last = sc;

  scomb *prev = NULL;
  for (sc = scombs; prev != last; sc = sc->next) {
    lift(sc);
    prev = sc;
  }

  if (trace)
    print_scombs1();
}

void app_lifting()
{
  debug_stage("Application lifting");

  scomb *sc;
  scomb *last = NULL;
  for (sc = scombs; sc; sc = sc->next)
    last = sc;

  scomb *prev = NULL;
  for (sc = scombs; prev != last; sc = sc->next) {
    applift(sc);
    prev = sc;
  }

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
  debug_stage("Machine code generation");
  global_cpucode = array_new();
  jit_compile(global_program->ginstrs,global_cpucode);

  print_compiled(global_program,global_cpucode);

  int i;
  for (i = 0; i < global_program->count; i++)
    global_program->ginstrs[i].codeaddr += (int)global_cpucode->data;

  code_start = (char*)global_cpucode->data;
}

void reduction_engine()
{
  debug_stage("Reduction engine");
  scomb *mainsc = get_scomb("main");
  struct timeval start;
  struct timeval end;

  cell *root = alloc_cell2(TYPE_SCREF,mainsc,NULL);
  gettimeofday(&start,NULL);
  stream(root);
  gettimeofday(&end,NULL);

  printf("\n");
  int ms = (end.tv_sec - start.tv_sec)*1000 +
           (end.tv_usec - start.tv_usec)/1000;
  if (args.statistics)
    fprintf(statsfile,"Execution time: %.3fs\n",((double)ms)/1000.0);
}

void gcode_interpreter()
{
  struct timeval start;
  struct timeval end;

  debug_stage("G-code interpreter");

  gettimeofday(&start,NULL);
  execute(global_program);
  gettimeofday(&end,NULL);

  printf("\n");
  if (args.profiling)
    print_profiling(global_program);
  gprogram_free(global_program);

  int ms = (end.tv_sec - start.tv_sec)*1000 +
           (end.tv_usec - start.tv_usec)/1000;
  if (args.statistics)
    fprintf(statsfile,"Execution time: %.3fs\n",((double)ms)/1000.0);
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
  argp_parse (&argp, argc, argv, 0, 0, &args);
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
