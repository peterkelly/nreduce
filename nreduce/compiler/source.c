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
 * $Id: memory.c 326 2006-08-22 06:11:26Z pmkelly $
 *
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "source.h"
#include "gcode.h"
#include "src/nreduce.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <assert.h>
#include <ctype.h>
#include <stdarg.h>
#include <math.h>

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
extern int yylineno;
snode *parse_root = NULL;

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
  switch (snodetype(c)) {
  case TYPE_APPLICATION:
  case TYPE_CONS:
    snode_free(c->left);
    snode_free(c->right);
    break;
  case TYPE_LAMBDA:
    snode_free(c->body);
    break;
  case TYPE_LETREC: {
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
  case TYPE_BUILTIN:
  case TYPE_SYMBOL:
  case TYPE_SCREF:
  case TYPE_NIL:
  case TYPE_NUMBER:
  case TYPE_STRING:
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
  src->scombs = NULL;
  src->lastsc = &src->scombs;
  src->genvar = 0;
  return src;
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

static void parse_post_processing(source *src, snode *root)
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

    if (NULL != get_scomb(src,name->name)) {
      fprintf(stderr,"Duplicate supercombinator: %s\n",name->name);
      exit(1);
    }

    sc = add_scomb(src,name->name);
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

static void check_for_main(source *src)
{
  int gotmain = 0;

  scomb *sc;

  for (sc = src->scombs; sc; sc = sc->next) {
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
  yylineno = 1;

  bufstate = yy_scan_string(str);
  yy_switch_to_buffer(bufstate);

  if (0 != yyparse())
    exit(1);

  yy_delete_buffer(bufstate);
#if HAVE_YYLEX_DESTROY
  yylex_destroy();
#endif

  parse_post_processing(src,parse_root);
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
  yylineno = 1;

  bufstate = yy_create_buffer(yyin,YY_BUF_SIZE);
  yy_switch_to_buffer(bufstate);
  if (0 != yyparse())
    exit(1);
  yy_delete_buffer(bufstate);
#if HAVE_YYLEX_DESTROY
  yylex_destroy();
#endif

  fclose(yyin);

  parse_post_processing(src,parse_root);
  return 0;
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

static void line_fixing(source *src)
{
  scomb *sc;
  debug_stage("Line number fixing");

  for (sc = src->scombs; sc; sc = sc->next)
    fix_linenos(sc->body);

  if (trace)
    print_scombs1(src);
}

static void create_letrecs_r(source *src, snode *c)
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
      parse_check(src,TYPE_CONS == snodetype(let1),let1,"let takes 2 parameters");

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
        parse_check(src,TYPE_CONS == snodetype(deflist),deflist,"let entry not a cons node");
        symbol = deflist->left;
        parse_check(src,TYPE_SYMBOL == snodetype(symbol),symbol,"let definition expects a symbol");
        link1 = deflist->right;
        parse_check(src,TYPE_CONS == snodetype(link1),link1,"let definition should be list of 2");
        value = link1->left;
        link1->left = NULL; /* don't free value */
        link2 = link1->right;
        parse_check(src,TYPE_NIL == snodetype(link2),link2,"let definition should be list of 2");

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
      parse_check(src,TYPE_CONS == snodetype(let2),let2,"let takes 2 parameters");
      body = let2->left;
      let2->left = NULL; /* don't free body */

      for (lnk = defs; lnk; lnk = lnk->next)
        create_letrecs_r(src,lnk->value);

      snode_free(c->left);
      snode_free(c->right);
      c->tag = (TYPE_LETREC | (c->tag & ~TAG_MASK));
      c->bindings = defs;
      c->body = body;

      create_letrecs_r(src,body);
    }
    else {

      create_letrecs_r(src,c->left);
      create_letrecs_r(src,c->right);
    }

    break;
  case TYPE_LAMBDA:
    create_letrecs_r(src,c->body);
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

static void create_letrecs(source *src, snode *c)
{
  create_letrecs_r(src,c);
}

static void letrec_creation(source *src)
{
  scomb *sc;
  debug_stage("Letrec creation");

  for (sc = src->scombs; sc; sc = sc->next)
    create_letrecs(src,sc->body);

  if (trace)
    print_scombs1(src);
}

static void variable_renaming(source *src)
{
  scomb *sc;
  debug_stage("Variable renaming");

  for (sc = src->scombs; sc; sc = sc->next)
    rename_variables(src,sc);

  if (trace)
    print_scombs1(src);
}

static void substitute_apps_r(source *src, snode **k)
{
  switch (snodetype(*k)) {
  case TYPE_APPLICATION:
    substitute_apps_r(src,&(*k)->left);
    substitute_apps_r(src,&(*k)->right);
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
      parse_check(src,TYPE_CONS == snodetype(right),right,"expected a cons here");

      lst = right;
      app = left;
      next = lst->right;

      while (TYPE_NIL != snodetype(next)) {
        snode *item;
        snode *newapp;
        parse_check(src,TYPE_CONS == snodetype(next),next,"expected a cons here");
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

      substitute_apps_r(src,k);
    }
    else {
      snode_free(*k);
      *k = left;
      substitute_apps_r(src,k);
    }
    break;
  }
  case TYPE_LAMBDA:
    substitute_apps_r(src,&(*k)->body);
    break;
  case TYPE_LETREC: {
    letrec *rec;
    for (rec = (*k)->bindings; rec; rec = rec->next)
      substitute_apps_r(src,&rec->value);
    substitute_apps_r(src,&(*k)->body);
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

static void substitute_apps(source *src, snode **k)
{
  substitute_apps_r(src,k);
}

static void app_substitution(source *src)
{
  scomb *sc;
  debug_stage("Application substitution");

  for (sc = src->scombs; sc; sc = sc->next)
    substitute_apps(src,&sc->body);

  if (trace)
    print_scombs1(src);
}

static void symbol_resolution(source *src)
{
  scomb *sc;
  debug_stage("Symbol resolution");

  for (sc = src->scombs; sc; sc = sc->next)
    resolve_refs(src,sc);

  if (trace)
    print_scombs1(src);
}


static void lambda_lifting(source *src)
{
  scomb *sc;
  scomb *last;
  scomb *prev;
  debug_stage("Lambda lifting");

  last = NULL;
  for (sc = src->scombs; sc; sc = sc->next)
    last = sc;

  prev = NULL;
  for (sc = src->scombs; prev != last; sc = sc->next) {
    lift(src,sc);
    prev = sc;
  }

  if (trace)
    print_scombs1(src);
}

static void app_lifting(source *src)
{
  scomb *sc;
  scomb *last;
  scomb *prev;
  debug_stage("Application lifting");

  last = NULL;
  for (sc = src->scombs; sc; sc = sc->next)
    last = sc;

  prev = NULL;
  for (sc = src->scombs; prev != last; sc = sc->next) {
    applift(src,sc);
    prev = sc;
  }

  if (trace)
    print_scombs1(src);
}

static void letrec_reordering(source *src)
{
  scomb *sc;
  debug_stage("Letrec reordering");

  for (sc = src->scombs; sc; sc = sc->next)
    reorder_letrecs(sc->body);

  if (trace)
    print_scombs1(src);
}

#if 0
static void graph_optimisation(source *src)
{
  debug_stage("Graph optimisation");

  fix_partial_applications(src);

  if (trace)
    print_scombs1(src);
}
#endif

int source_process(source *src)
{
  check_for_main(src);

  line_fixing(src);
  letrec_creation(src);
  variable_renaming(src);

  app_substitution(src);

  symbol_resolution(src);
  lambda_lifting(src);
  check_scombs_nosharing(src);
/*   if (args.lambdadebug) { */
/*     print_scombs1(src); */
/*     exit(0); */
/*   } */

  check_scombs_nosharing(src);
  app_lifting(src);
  check_scombs_nosharing(src);

  letrec_reordering(src);

  return 0;
}

int source_compile(source *src, char **bcdata, int *bcsize)
{
  gprogram *gp;

  strictness_analysis(src);

  debug_stage("G-code compilation");

  gp = gprogram_new();
  compile(src,gp);
  gen_bytecode(gp,bcdata,bcsize);
  gprogram_free(gp);

  return 0;
}

void source_free(source *src)
{
  int i;
  scomb_free_list(&src->scombs);
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

