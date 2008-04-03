/*
 * This file is part of the nreduce project
 * Copyright (C) 2006-2007 Peter Kelly (pmk@cs.adelaide.edu.au)
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
 * $Id: source.c 626 2007-08-28 06:14:44Z pmkelly $
 *
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#define SOURCE_C

#include "source.h"
#include "src/nreduce.h"
#include "grammar.tab.h"
#include "runtime/runtime.h"
#include "modules/modules.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <assert.h>
#include <ctype.h>
#include <stdarg.h>
#include <math.h>

extern const module_info *modules;

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
  "WRAP",
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
const char *parse_modname = NULL;

snode *snode_new(int fileno, int lineno)
{
  snode *c = calloc(1,sizeof(snode));
  c->sl.fileno = fileno;
  c->sl.lineno = lineno;
  return c;
}

void free_letrec(letrec *rec)
{
  snode_free(rec->value);
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
  case SNODE_EXTFUNC:
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
  src->scombs = array_new(sizeof(scomb*),0);
  list_push(&src->newimports,strdup("prelude"));
  return src;
}
//// check if main supercombinator exist
static int check_for_main(source *src)
{
  int gotmain = 0;
  int scno;
  int count = array_count(src->scombs);

  for (scno = 0; scno < count; scno++) {
    scomb *sc = array_item(src->scombs,scno,scomb*);
    if (!strcmp(sc->name,"main")) {
      gotmain = 1;

      if (2 <= sc->nargs) {
        fprintf(stderr,"Supercombinator \"main\" must take either 0 or 1 arguments\n");
        return -1;
      }
      else if (0 == sc->nargs) {	//// add an predefined argument to main supercombinator, if no arguments is specified
        sc->nargs = 1;
        free(sc->argnames);
        free(sc->strictin);
        sc->argnames = (char**)malloc(sizeof(char*));
        sc->argnames[0] = strdup("__args");	//// the added argument
        sc->strictin = (int*)malloc(sizeof(int));
        sc->strictin[0] = 0;
      }
    }
  }

  if (!gotmain) {
    fprintf(stderr,"No \"main\" function defined\n");
    return -1;
  }

  return 0;
}

int source_parse_string(source *src, const char *str, const char *filename, const char *modname)
{
  YY_BUFFER_STATE bufstate;
  int r;
  yyfilename = filename;
  yyfileno = add_parsedfile(src,filename);
  yylloc.first_line = 1;

  bufstate = yy_scan_string(str);
  yy_switch_to_buffer(bufstate);

  parse_src = src;
  parse_modname = modname ? modname : "";
  r = yyparse();

  yy_delete_buffer(bufstate);
#if HAVE_YYLEX_DESTROY
  yylex_destroy();
#endif

  return r;
}

int source_parse_file(source *src, const char *filename, const char *modname)
{
  YY_BUFFER_STATE bufstate;
  int r;

  if (NULL == (yyin = fopen(filename,"r"))) {
    perror(filename);
    return -1;
  }

  yyfilename = filename;
  yyfileno = add_parsedfile(src,filename);    ////yyfileno is the index in the src->parsedfiles
  yylloc.first_line = 1;

  bufstate = yy_create_buffer(yyin,YY_BUF_SIZE);
  yy_switch_to_buffer(bufstate);
  parse_src = src;
  parse_modname = modname ? modname : "";
  r = yyparse();
  yy_delete_buffer(bufstate);
#if HAVE_YYLEX_DESTROY
  yylex_destroy();
#endif

  if (yyin)
    fclose(yyin);

  return r;
}

void add_import(source *src, const char *name)
{
  list *l;
  for (l = src->imports; l; l = l->next)
    if (!strcmp(name,(char*)l->data))
      return;
  for (l = src->newimports; l; l = l->next)
    if (!strcmp(name,(char*)l->data))
      return;
  list_push(&src->newimports,strdup(name));
}
//// process the imported modules
static int process_imports(source *src)
{
  int r = 0;

  while ((NULL != src->newimports) && (0 == r)) {
    list *l;
    list *newimports = src->newimports;
    src->newimports = NULL;

    for (l = newimports; l && (0 == r); l = l->next)
      list_push(&src->imports,strdup((char*)l->data));

    for (l = newimports; l && (0 == r); l = l->next) {
      char *modname = (char*)l->data;
      const module_info *mod;
      int found = 0;

      for (mod = modules; mod->name; mod++) {
        if (!strcmp(modname,mod->name)) {
          const char *prefix = (!strcmp(mod->name,"prelude") ? NULL : mod->name);
          r = source_parse_string(src,mod->source,mod->filename,prefix);
          found = 1;
          break;
        }
      }

      if (!found) {
        char *modfilename = (char*)malloc(strlen(modname)+strlen(MODULE_EXTENSION)+1);
        sprintf(modfilename,"%s%s",modname,MODULE_EXTENSION);
        r = source_parse_file(src,modfilename,modname);
        free(modfilename);
      }
    }
    list_free(newimports,free);
  }
  return r;
}

int is_from_prelude(source *src, scomb *sc)
{
  return ((0 <= sc->sl.fileno) &&
          !strcmp(array_item(src->parsedfiles,sc->sl.fileno,char*),
                  MODULE_FILENAME_PREFIX"prelude.elc"));
}

int handle_unbound(source *src, list *unbound)
{
  list *l;
  int count = 0;

  for (l = unbound; l; l = l->next) {
    unboundvar *ubv = (unboundvar*)l->data;
    print_sourceloc(src,stderr,ubv->sl);
    fprintf(stderr,"Unbound variable: %s\n",ubv->name);
    free(ubv->name);
    count++;
  }
  list_free(unbound,free);

  if (0 == count)
    return 0;
  else
    return -1;
}

int source_process(source *src)
{
  int sccount;
  int scno;
  list *unbound = NULL;

  if (0 != process_imports(src))
    return -1;

  if (0 != check_for_main(src))
    return -1;

  sccount = array_count(src->scombs);

  /* Compile stage: Variable renaming */
  for (scno = 0; scno < sccount; scno++) {
    scomb *sc = array_item(src->scombs,scno,scomb*);
    rename_variables(src,sc);
  }

  /* Compile stage: Symbol resolution */
  for (scno = 0; scno < sccount; scno++) {
    scomb *sc = array_item(src->scombs,scno,scomb*);
    resolve_refs(src,sc,&unbound);
  }

  if (0 != handle_unbound(src,unbound))
    return -1;

  /* Compile stage: Lambda lifting */
  for (scno = 0; scno < sccount; scno++)
    lift(src,array_item(src->scombs,scno,scomb*));
  sccount = array_count(src->scombs); /* lift() may have added some */

  return 0;
}

void source_free(source *src)
{
  int i;
  if (src->scombs) {
    int count = array_count(src->scombs);
    for (i = 0; i < count; i++)
      scomb_free(array_item(src->scombs,i,scomb*));
    array_free(src->scombs);
  }
  if (src->oldnames) {
    int count = array_count(src->oldnames);
    for (i = 0; i < count; i++)
      free(array_item(src->oldnames,i,char*));
    array_free(src->oldnames);
  }
  if (src->parsedfiles) {
    int count = array_count(src->parsedfiles);
    for (i = 0; i < count; i++)
      free(array_item(src->parsedfiles,i,char*));
    array_free(src->parsedfiles);
  }
  list_free(src->imports,free);
  list_free(src->newimports,free);
  free(src);
}

const char *lookup_parsedfile(source *src, int fileno)
{
  assert(src->parsedfiles);
  assert(0 <= fileno);
  assert(fileno < array_count(src->parsedfiles));
  return array_item(src->parsedfiles,fileno,char*);
}

//// add the pointer of parsed file to src
int add_parsedfile(source *src, const char *filename)
{
  char *copy = strdup(filename);
  if (src->parsedfiles == NULL)
    src->parsedfiles = array_new(sizeof(char*),0);
  array_append(src->parsedfiles,&copy,sizeof(char*));
  return array_count(src->parsedfiles)-1;
}

void print_sourceloc(source *src, FILE *f, sourceloc sl)
{
  if (0 <= sl.fileno)
    fprintf(f,"%s:%d: ",lookup_parsedfile(src,sl.fileno),sl.lineno);
}

char *make_varname(const char *want)
{
  char *name;
  if (!strncmp(want,GENVAR_PREFIX,strlen(GENVAR_PREFIX))) {
    name = (char*)malloc(strlen(want)+2);
    sprintf(name,"%s_",want);
  }
  else {
    name = strdup(want);
  }
  return name;
}

int create_scomb(source *src, const char *modname, char *name, list *argnames,
                 snode *body, int fileno, int lineno)
{
  list *l;
  int argno = 0;
  scomb *sc;
  char *scname;

  if (0 < strlen(modname)) {
    scname = (char*)malloc(strlen(modname)+2+
                           strlen(name)+1);
    sprintf(scname,"%s::%s",modname,name);
  }
  else {
    scname = strdup(name);
  }

  if (NULL != get_scomb(src,scname)) {
    sourceloc sl;
    sl.fileno = fileno;
    sl.lineno = lineno;
    print_sourceloc(src,stderr,sl);
    fprintf(stderr,"Duplicate supercombinator: %s\n",name);
    free(scname);
    return -1;
  }

  sc = add_scomb(src,scname);
  sc->sl.fileno = fileno;
  sc->sl.lineno = lineno;
  sc->nargs = list_count(argnames);
  sc->argnames = (char**)calloc(sc->nargs,sizeof(char*));
  sc->strictin = (int*)calloc(sc->nargs,sizeof(int));
  if (0 < strlen(modname))
    sc->modname = strdup(modname);
  for (l = argnames; l; l = l->next) {
    char *argname = (char*)l->data;
    if ('!' == argname[0]) {
      argname++;
      sc->strictin[argno] = 1;
    }
    sc->argnames[argno++] = strdup(argname);
  }
  free(scname);
  sc->body = body;
  return 0;
}

snode *makesym(int fileno, int lineno, const char *name)
{
  snode *sym = snode_new(fileno,lineno);
  sym->type = SNODE_SYMBOL;
  sym->name = strdup(name);
  return sym;
}
