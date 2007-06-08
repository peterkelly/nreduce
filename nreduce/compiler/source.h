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

#ifndef _SOURCE_H
#define _SOURCE_H

#include "src/nreduce.h"

#define SNODE_EMPTY       0x00
#define SNODE_APPLICATION 0x01
#define SNODE_LAMBDA      0x02
#define SNODE_BUILTIN     0x03
#define SNODE_SYMBOL      0x04
#define SNODE_LETREC      0x05
#define SNODE_SCREF       0x06
#define SNODE_NIL         0x07
#define SNODE_NUMBER      0x08
#define SNODE_STRING      0x09
#define SNODE_WRAP        0x0A
#define SNODE_COUNT       0x0B

#define MODULE_EXTENSION ".elc"
#define MODULE_FILENAME_PREFIX "(module) "
#define MODULE_REF '.'

#define GENVAR_PREFIX "_v"

typedef struct sourceloc {
  int fileno;
  int lineno;
} sourceloc;

typedef struct snode {
  int type;

  struct snode *left;
  struct snode *right;

  char *value;
  char *name;
  struct letrec *bindings;
  struct snode *body;
  struct scomb *sc;
  int bif;
  double num;
  int strict;
  sourceloc sl;
  struct snode *target;

  struct snode *parent;
  struct snode *nextuser;
} snode;

typedef struct letrec {
  char *name;
  snode *value;
  int strict;
  struct scomb *sc;
  struct letrec *next;
  snode *users;
} letrec;

typedef struct scomb {
  char *name;
  int nargs;
  char **argnames;
  snode *body;
  int index;
  int *strictin;
  sourceloc sl;
  char *modname;
  int used;
  int nospark;
  struct scomb *hashnext;
  int doesappend;
  struct scomb *appendver;
  int caninline;
} scomb;

typedef struct source {
  scomb *schash[GLOBAL_HASH_SIZE];
  int varno;
  array *scombs;
  array *oldnames;
  array *parsedfiles;
  list *imports;
  list *newimports;
  char *curmodname;
} source;

typedef struct unboundvar {
  sourceloc sl;
  char *name;
} unboundvar;

snode *snode_new(int fileno, int lineno);
void snode_free(snode *c);
void free_letrec(letrec *rec);

int is_from_prelude(source *src, scomb *sc);
void compile_stage(source *src, const char *name);
source *source_new();
int source_parse_string(source *src, const char *str, const char *filename, const char *modname);
int source_parse_file(source *src, const char *filename, const char *modname);
void add_import(source *src, const char *name);
int source_process(source *src, int stopafterlambda, int nosink, int disstrict, int appendoptdebug);
int source_compile(source *src, char **bcdata, int *bcsize);
void source_free(source *src);

const char *lookup_parsedfile(source *src, int fileno);
int add_parsedfile(source *src, const char *filename);
void print_sourceloc(source *src, FILE *f, sourceloc sl);

char *make_varname(const char *want);

/* resolve */

void resolve_refs(source *src, scomb *sc, list **unbound);

/* reorder */

void reorder_letrecs(snode *c);

/* sinking */

void remove_wrappers(snode *s);
void sink_letrecs(source *src, snode *s);

/* super */

scomb *get_scomb_index(source *src, int index);
scomb *get_scomb(source *src, const char *name);
int get_scomb_var(scomb *sc, const char *name);
scomb *add_scomb(source *src, const char *name1);
void scomb_free(scomb *sc);
void schash_rebuild(source *src);
int schash_check(source *src);

/* lifting */

void lift(source *src, scomb *sc);
void applift(source *src, scomb *sc);
void nonstrict_lift(source *src, scomb *sc);

/* inlining */

void inlining(source *src);

/* appendopt */

snode *snode_copy(snode *s);
void appendopt(source *src);

/* renaming */

char *next_var(source *src, const char *oldname);
void rename_variables(source *src, scomb *sc);

/* debug */

void fatal(const char *format, ...);
int debug(int depth, const char *format, ...);
void debug_stage(const char *name);
int count_args(snode *c);
snode *get_arg(snode *c, int argno);
void print(snode *c);
void print_codef2(source *src, FILE *f, snode *c, int pos);
void print_codef(source *src, FILE *f, snode *c, int needbr);
void print_code(source *src, snode *c);
void print_scomb_code(source *src, FILE *f, scomb *sc);
void print_scombs1(source *src);
void print_scombs2(source *src);
const char *real_varname(source *src, const char *sym);
char *real_scname(source *src, const char *sym);

/* strictness */

void check_strictness(scomb *sc, int *changed);
void dump_strictinfo(source *src);
void strictness_analysis(source *src);

#ifndef DEBUG_C
extern int compileinfo;
#endif

#ifndef SOURCE_C
extern const char *snode_types[SNODE_COUNT];
#endif

#endif
