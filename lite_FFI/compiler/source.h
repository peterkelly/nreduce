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
 * $Id: source.h 613 2007-08-23 11:40:12Z pmkelly $
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
#define SNODE_EXTFUNC     0x0C

#define MODULE_EXTENSION ".elc"
#define MODULE_FILENAME_PREFIX "(module) "
#define MODULE_REF '.'

#define GENVAR_PREFIX "_v"

typedef struct sourceloc {
  int fileno;
  int lineno;
} sourceloc;

//// snode: (s)upercombinator (node)
typedef struct snode {
  int type;	//// see the above type definition, like SNODE_APPLICATION

  struct snode *left;
  struct snode *right;

  char *value;
  char *name;
  struct letrec *bindings;
  struct snode *body;
  struct scomb *sc;
  int bif;	//// (b)uild(i)n (f)unction
  int extf; //// external functions
  double num;	//// the number value (type: double), used when the snode is a number node
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
  char *name;		   //// name of this supercombinator, like "main"
  int nargs;           //// number of arguments for this supercombinator
  char **argnames;	   //// arg+names: names of all arguments
  snode *body;         //// the body of this supercombinator (type snode)
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
  scomb *schash[GLOBAL_HASH_SIZE];    ////sc (supercombinator) hash
  int varno;	//// variable NO
  array *scombs;
  array *oldnames;
  array *parsedfiles;	//// source files that have been parsed
  list *imports;	//// modules have been imported 
  list *newimports;	//// the imported modules, like prelude modules
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
source *source_new();
int source_parse_string(source *src, const char *str, const char *filename, const char *modname);
int source_parse_file(source *src, const char *filename, const char *modname);
void add_import(source *src, const char *name);
int source_process(source *src);
int source_compile(source *src, char **bcdata, int *bcsize);
void source_free(source *src);

const char *lookup_parsedfile(source *src, int fileno);
int add_parsedfile(source *src, const char *filename);
void print_sourceloc(source *src, FILE *f, sourceloc sl);

char *make_varname(const char *want);
int create_scomb(source *src, const char *modname, char *name, list *argnames,
                 snode *body, int fileno, int lineno);
////Make a symbol, used in Yacc
snode *makesym(int fileno, int lineno, const char *name);

/* resolve */

void resolve_refs(source *src, scomb *sc, list **unbound);

/* super */

scomb *get_scomb(source *src, const char *name);
scomb *add_scomb(source *src, const char *name1);
void scomb_free(scomb *sc);

/* lifting */

void lift(source *src, scomb *sc);

/* renaming */

char *next_var(source *src, const char *oldname);
void rename_variables(source *src, scomb *sc);

#ifndef SOURCE_C
extern const char *snode_types[SNODE_COUNT];
#endif

#endif
