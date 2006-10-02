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

#define SOURCE_C

#include "source.h"
#include "bytecode.h"
#include "src/nreduce.h"
#include "grammar.tab.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <assert.h>
#include <ctype.h>
#include <stdarg.h>
#include <math.h>

const char *snode_types[SNODE_COUNT] = {
  "EMPTY",
  "APPLICATION",
  "LAMBDA",
  "BUILTIN",
  "SYMBOL",
  "LETREC",
  "SCREF",
  "NIL",
  "NUMBER",
  "STRING",
};

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

extern const char *yyfilename;
extern int yyfileno;
source *parse_src = NULL;

snode *snode_new(int fileno, int lineno)
{
  snode *c = calloc(1,sizeof(snode));
  c->sl.fileno = fileno;
  c->sl.lineno = lineno;
  return c;
}

void free_letrec(letrec *rec)
{
  free(rec->name);
  free(rec);
}

void snode_free(snode *c)
{
  if (NULL == c)
    return;
  switch (c->type) {
  case SNODE_APPLICATION:
    snode_free(c->left);
    snode_free(c->right);
    break;
  case SNODE_LAMBDA:
    snode_free(c->body);
    break;
  case SNODE_LETREC: {
    letrec *rec = c->bindings;
    while (rec) {
      letrec *next = rec->next;
      snode_free(rec->value);
      free_letrec(rec);
      rec = next;
    }
    snode_free(c->body);
    break;
  }
  case SNODE_BUILTIN:
  case SNODE_SYMBOL:
  case SNODE_SCREF:
  case SNODE_NIL:
  case SNODE_NUMBER:
  case SNODE_STRING:
    break;
  default:
    abort();
  }
  free(c->value);
  free(c->name);
  free(c);
}

source *source_new()
{
  source *src = (source*)calloc(1,sizeof(source));
  src->scombs = array_new(sizeof(scomb*));
  src->genvar = 0;
  return src;
}

static void check_for_main(source *src)
{
  int gotmain = 0;
  int scno;

  for (scno = 0; scno < array_count(src->scombs); scno++) {
    scomb *sc = array_item(src->scombs,scno,scomb*);
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

void parse_check(source *src, int cond, snode *c, char *msg)
{
  if (cond)
    return;
  if (0 <= c->sl.fileno)
    fprintf(stderr,"%s:%d: ",lookup_parsedfile(src,c->sl.fileno),c->sl.lineno);
  fprintf(stderr,"%s\n",msg);
  exit(1);
}

int source_parse_string(source *src, const char *str, const char *filename)
{
  YY_BUFFER_STATE bufstate;
  yyfilename = filename;
  yyfileno = add_parsedfile(src,filename);
  yylloc.first_line = 1;

  bufstate = yy_scan_string(str);
  yy_switch_to_buffer(bufstate);

  parse_src = src;
  if (0 != yyparse())
    exit(1);

  yy_delete_buffer(bufstate);
#if HAVE_YYLEX_DESTROY
  yylex_destroy();
#endif

  return 0;
}

int source_parse_file(source *src, const char *filename)
{
  YY_BUFFER_STATE bufstate;
  if (NULL == (yyin = fopen(filename,"r"))) {
    perror(filename);
    exit(1);
  }

  yyfilename = filename;
  yyfileno = add_parsedfile(src,filename);
  yylloc.first_line = 1;

  bufstate = yy_create_buffer(yyin,YY_BUF_SIZE);
  yy_switch_to_buffer(bufstate);
  parse_src = src;
  if (0 != yyparse())
    exit(1);
  yy_delete_buffer(bufstate);
#if HAVE_YYLEX_DESTROY
  yylex_destroy();
#endif

  fclose(yyin);

  return 0;
}

void compile_stage(source *src, const char *name)
{
  static int stageno = 0;
  if ((0 < stageno++) && trace)
    print_scombs1(src);
  debug_stage(name);
}

int source_process(source *src)
{
  int sccount = array_count(src->scombs);
  int scno;

  check_for_main(src);

  compile_stage(src,"Variable renaming"); /* renaming.c */
  for (scno = 0; scno < sccount; scno++) {
    scomb *sc = array_item(src->scombs,scno,scomb*);
    rename_variables(src,sc);
  }

  compile_stage(src,"Symbol resolution"); /* resolve.c */
  for (scno = 0; scno < sccount; scno++) {
    scomb *sc = array_item(src->scombs,scno,scomb*);
    resolve_refs(src,sc);
  }

  compile_stage(src,"Lambda lifting"); /* lifting.c */
  for (scno = 0; scno < sccount; scno++)
    lift(src,array_item(src->scombs,scno,scomb*));
  sccount = array_count(src->scombs); /* lift() may have added some */

/*   if (args.lambdadebug) { */
/*     print_scombs1(src); */
/*     exit(0); */
/*   } */

  compile_stage(src,"Application lifting"); /* lifting.c */
  for (scno = 0; scno < sccount; scno++)
    applift(src,array_item(src->scombs,scno,scomb*));
  sccount = array_count(src->scombs); /* applift() may have added some */

  compile_stage(src,"Letrec reordering"); /* reorder.c */
  for (scno = 0; scno < array_count(src->scombs); scno++) {
    scomb *sc = array_item(src->scombs,scno,scomb*);
    reorder_letrecs(sc->body);
  }

  return 0;
}

int source_compile(source *src, char **bcdata, int *bcsize)
{

  strictness_analysis(src);

  compile_stage(src,"Bytecode compilation");
  compile(src,bcdata,bcsize);

  if (trace)
    bc_print(*bcdata,stdout,src,1);

  return 0;
}

void source_free(source *src)
{
  int i;
  if (src->scombs) {
    for (i = 0; i < array_count(src->scombs); i++)
      scomb_free(array_item(src->scombs,i,scomb*));
    array_free(src->scombs);
  }
  if (src->oldnames) {
    for (i = 0; i < array_count(src->oldnames); i++)
      free(array_item(src->oldnames,i,char*));
    array_free(src->oldnames);
  }
  if (src->parsedfiles) {
    for (i = 0; i < array_count(src->parsedfiles); i++)
      free(array_item(src->parsedfiles,i,char*));
    array_free(src->parsedfiles);
  }
  free(src);
}

const char *lookup_parsedfile(source *src, int fileno)
{
  assert(src->parsedfiles);
  assert(0 <= fileno);
  assert(fileno < array_count(src->parsedfiles));
  return array_item(src->parsedfiles,fileno,char*);
}

int add_parsedfile(source *src, const char *filename)
{
  char *copy = strdup(filename);
  if (NULL == src->parsedfiles)
    src->parsedfiles = array_new(sizeof(char*));
  array_append(src->parsedfiles,&copy,sizeof(char*));
  return array_count(src->parsedfiles)-1;
}

void print_sourceloc(source *src, FILE *f, sourceloc sl)
{
  if (0 <= sl.fileno)
    fprintf(f,"%s:%d: ",lookup_parsedfile(src,sl.fileno),sl.lineno);
}

