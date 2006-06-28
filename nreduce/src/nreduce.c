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

#define NREDUCE_C

#define ENGINE_INTERPRETER 0
#define ENGINE_NATIVE      1
#define ENGINE_REDUCER     2

extern char *yyfilename;
extern cell *parse_root;
extern char *code_start;
extern int resolve_ind_offset;

gprogram *global_program = NULL;
array *global_cpucode = NULL;

char *exec_modes[3] = { "interpreter", "native", "reducer" };

static char doc[] = "nreduce";

const char *argp_program_version = "nreduce 0.1";

const char *argp_program_bug_address = "pmk@cs.adelaide.edu.au";

static char args_doc[] = "FILENAME\n";

static struct argp_option options[] = {
  {"trace",            't', NULL,    0, "Enable tracing" },
  {"statistics",       's', NULL,    0, "Show statistics" },
  {"just-strictness",  'j', NULL,    0, "Just display strictness information" },
  {"engine",           'e', "ENGINE",0, "Use execution engine: (r)educer|(i)nterpreter|(n)ative\n"
                                        "(default: interpreter)" },
  {"strictness-debug", 'r', NULL,    0, "Do not run program; show strictness information for all "
                                        "supercombinators" },
  { 0 }
};

struct arguments {
  int trace;
  int statistics;
  int juststrict;
  int engine;
  char *filename;
  int strictdebug;
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
    arguments->statistics = 1;
    break;
  case 'j':
    arguments->juststrict = 1;
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

void stream(cell *c)
{
  push(c);
  while (0 < stackcount) {
    cell *c;
    reduce();

    c = pop();
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
      push((cell*)c->field2);
      push((cell*)c->field1);
    }
    else if (TYPE_APPLICATION == celltype(c)) {
/*       too_many_arguments(c); */ /* + 2 3 4 causes this to infinitely loop in resolve_source() */
      fprintf(stderr,"Too many arguments applied to function\n");
      exit(1);
    }
    else {
      fprintf(stderr,"Bad cell type returned to printing mechanism: %s\n",cell_types[celltype(c)]);
      exit(1);
    }
  }
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

void source_code_parsing()
{
  debug_stage("Source code parsing");

  if (NULL == (yyin = fopen(args.filename,"r"))) {
    perror(args.filename);
    exit(1);
  }

  yyfilename = args.filename;

  if (0 != yyparse())
    exit(1);

  fclose(yyin);

  global_root = parse_root;

  scomb *sc;
  for (sc = scombs; sc; sc = sc->next) {
    cell *lambda;
    clearflag_scomb(FLAG_PROCESSED,sc);
    if (NULL != (lambda = check_for_lambda(sc->body))) {
      cellmsg(stderr,lambda,"Supercombinators cannot contain lambda expressions\n");
      exit(1);
    }
  }

  if (trace) {
    print_code(global_root);
    printf("\n");
  }
}

void parse_check(int cond, cell *c, char *msg)
{
  if (cond)
    return;
  if (NULL != c->filename)
    fprintf(stderr,"%s:%d: %s\n",c->filename,c->lineno,msg);
  else
    fprintf(stderr,"%s\n",msg);
  exit(1);
}

void create_letrecs_r(cell *c)
{
  if (c->tag & FLAG_PROCESSED)
    return;
  c->tag |= FLAG_PROCESSED;
  switch (celltype(c)) {
  case TYPE_APPLICATION:
  case TYPE_CONS:

    if ((TYPE_CONS == celltype(c)) &&
        (TYPE_SYMBOL == celltype((cell*)c->field1)) &&
        !strcmp("let",(char*)((cell*)c->field1)->field1)) {

      cell *defs = NULL;
      cell **lnkptr = NULL;

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

        cell *vardef = alloc_cell();
        vardef->tag = TYPE_VARDEF;
        vardef->field1 = strdup((char*)symbol->field1);
        vardef->field2 = value;

        cell *newlnk = alloc_cell();
        newlnk->tag = TYPE_VARLNK;
        newlnk->field1 = vardef;
        newlnk->field2 = NULL;

        if (NULL == defs)
          defs = newlnk;
        else
          *lnkptr = newlnk;
        lnkptr = (cell**)&newlnk->field2;

        iter = (cell*)iter->field2;
      }

      cell *let2 = (cell*)let1->field2;
      parse_check(TYPE_CONS == celltype(let2),let2,"let takes 2 parameters");
      cell *body = (cell*)let2->field1;

      cell *lnk;
      for (lnk = defs; lnk; lnk = (cell*)lnk->field2) {
        assert(TYPE_VARLNK == celltype(lnk));
        cell *def = (cell*)lnk->field1;
        assert(TYPE_VARDEF == celltype(def));
        create_letrecs_r((cell*)def->field2);
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
  create_letrecs(global_root);

  scomb *sc;
  for (sc = scombs; sc; sc = sc->next)
    create_letrecs(sc->body);

  if (trace) {
    print_code(global_root);
    printf("\n");
  }
}

void letrec_substitution()
{
  debug_stage("Letrec substitution");

  global_root = suball_letrecs(global_root,NULL);
  resolve_scvars(global_root);

  clearflag(FLAG_PROCESSED);
  clearflag(FLAG_MAXFREE);

  scomb *sc;
  for (sc = scombs; sc; sc = sc->next) {
    sc->body = suball_letrecs(sc->body,sc);
    resolve_scvars(sc->body);
  }

  assert(0 == stackcount);
}

void substitute_apps_r(cell *c)
{
  if (c->tag & FLAG_PROCESSED)
    return;
  c->tag |= FLAG_PROCESSED;
  switch (celltype(c)) {
  case TYPE_APPLICATION:
    substitute_apps_r((cell*)c->field1);
    substitute_apps_r((cell*)c->field2);
    break;
  case TYPE_CONS: {
    cell *left = (cell*)c->field1;
    cell *right = (cell*)c->field2;

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
      c->tag = TYPE_APPLICATION;
      c->field1 = app;
      c->field2 = lastitem;

      substitute_apps_r(c);
    }
    else {
      copy_cell(c,(cell*)c->field1);
      substitute_apps_r(c);
    }
    break;
  }
  case TYPE_LAMBDA:
    substitute_apps_r((cell*)c->field2);
    break;
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

void substitute_apps(cell *c)
{
  cleargraph(c,FLAG_PROCESSED);
  substitute_apps_r(c);
}

void app_substitution()
{
  debug_stage("Application substitution");

  substitute_apps(global_root);

  scomb *sc;
  for (sc = scombs; sc; sc = sc->next)
    substitute_apps(sc->body);

  if (trace) {
    print_code(global_root);
    printf("\n");
  }
}

void lambda_lifting()
{
  debug_stage("Lambda lifting");

  lift(&global_root,0,0);
  assert(0 == stackcount);

  mkprogsuper(global_root);

  if (trace)
    print_scombs1();
}

void calc_all_scomb_cells()
{
  scomb *sc;
  for (sc = scombs; sc; sc = sc->next) {
    if (!sc->cells) {
      scomb_calc_cells(sc);
    }
  }
}

void graph_optimisation()
{
  debug_stage("Graph optimisation");

/*   fix_partial_applications(); */
  check_scombs_nosharing();
  check_scombs();
  remove_redundant_scombs();
  calc_all_scomb_cells();

  if (trace)
    print_scombs1();
}

void gcode_compilation()
{
  debug_stage("G-code compilation");

  global_program = gprogram_new();
  compile(global_program);
  if (trace)
    print_program(global_program);
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
  calc_all_scomb_cells();
  stream(global_root);
  printf("\n");
}

void gcode_interpreter()
{
  debug_stage("G-code interpreter");
  execute(global_program->ginstrs);
  printf("\n");
  gprogram_free(global_program);
}

void native_execution_engine()
{
  debug_stage("Native execution engine");
  ((jitfun*)global_cpucode->data)();;
  printf("\n");

  free(global_cpucode);
  gprogram_free(global_program);
}

int main(int argc, char **argv)
{
  setbuf(stdout,NULL);
  initmem();

  memset(&args,0,sizeof(args));
  argp_parse (&argp, argc, argv, 0, 0, &args);
  trace = args.trace;
  strictdebug = args.strictdebug;

  source_code_parsing();
  letrec_creation();
  letrec_substitution();
  app_substitution();
  lambda_lifting();

  if (ENGINE_REDUCER == args.engine) {
    reduction_engine();
  }
  else {
    graph_optimisation();
    strictness_analysis();
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
  if (args.statistics)
    statistics();

  cleanup();
  return 0;
}
