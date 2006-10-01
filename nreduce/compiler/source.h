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
 * $Id: gcode.h 339 2006-09-21 01:18:34Z pmkelly $
 *
 */

#ifndef _SOURCE_H
#define _SOURCE_H

#include "src/nreduce.h"

typedef struct snode {
  int tag;

  struct snode *left;
  struct snode *right;

  char *value;
  char *name;
  struct letrec *bindings;
  struct snode *body;
  struct scomb *sc;
  int bif;
  double num;

  sourceloc sl;
} snode;

typedef struct letrec {
  char *name;
  snode *value;
  int strict;
  struct scomb *sc;
  struct letrec *next;
} letrec;

typedef struct scomb {
  char *name;
  int nargs;
  char **argnames;
  snode *body;
  int index;
  int *strictin;
  sourceloc sl;
  struct scomb *next;
} scomb;

typedef struct source {
  scomb *scombs;
  scomb **lastsc;
  int genvar;
  int varno;
  array *oldnames;
  array *parsedfiles;
} source;

snode *snode_new(int fileno, int lineno);
void snode_free(snode *c);
void parse_check(source *src, int cond, snode *c, char *msg);
void free_letrec(letrec *rec);

source *source_new();
int source_parse_string(source *src, const char *str, const char *filename);
int source_parse_file(source *src, const char *filename);
int source_process(source *src);
int source_compile(source *src, char **bcdata, int *bcsize);
void source_free(source *src);

const char *lookup_parsedfile(source *src, int fileno);
int add_parsedfile(source *src, const char *filename);
void print_sourceloc(source *src, FILE *f, sourceloc sl);

/* resolve */

void resolve_refs(source *src, scomb *sc);

/* reorder */

void reorder_letrecs(snode *c);

/* super */

scomb *get_scomb_index(source *src, int index);
scomb *get_scomb(source *src, const char *name);
int get_scomb_var(scomb *sc, const char *name);
scomb *add_scomb(source *src, const char *name1);
void scomb_free_list(scomb **list);
void fix_partial_applications(source *src);
void check_scombs_nosharing(source *src);

/* lifting */

void lift(source *src, scomb *sc);
void applift(source *src, scomb *sc);

/* graph */

void cleargraph(snode *root, int flag);
void find_snodes(snode ***nodes, int *nnodes, snode *root);

/* new */

void rename_variables(source *src, scomb *sc);

/* debug */

void fatal(const char *format, ...);
int debug(int depth, const char *format, ...);
void debug_stage(const char *name);
void print_hex(int c);
void print_hexbyte(unsigned char val);
void print_bin(void *ptr, int nbytes);
void print_bin_rev(void *ptr, int nbytes);
int count_args(snode *c);
snode *get_arg(snode *c, int argno);
void print(snode *c);
void print_codef2(source *src, FILE *f, snode *c, int pos);
void print_codef(source *src, FILE *f, snode *c);
void print_code(source *src, snode *c);
void print_scomb_code(source *src, scomb *sc);
void print_scombs1(source *src);
void print_scombs2(source *src);
const char *real_varname(source *src, const char *sym);
char *real_scname(source *src, const char *sym);
void print_stack(FILE *f, pntr *stk, int size, int dir);

/* strictness */

void dump_strictinfo(source *src);
void strictness_analysis(source *src);

#endif
