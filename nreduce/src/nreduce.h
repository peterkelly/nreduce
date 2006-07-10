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

#ifndef _NREDUCE_H
#define _NREDUCE_H

#include <stdio.h>

//#define INLINE_RESOLVE_IND

//#define NDEBUG
#define VERIFICATION

//#define DEBUG_ABSTRACTION
//#define DEBUG_SHOW_CREATED_SUPERCOMBINATORS
//#define DEBUG_DUMP_TREE_AFTER_CREATING_SUPERCOMBINATOR
//#define DEBUG_GCODE_COMPILATION
#define DEBUG_SHOW_STRICTNESS_ANALYSIS
//#define DEBUG_SHOW_STRICTNESS_ANALYSIS2
//#define DEBUG_REDUCTION
//#define STACK_MODEL_SANITY_CHECK
//#define DEBUG_FRONTIERS

#define STACK_LIMIT 10240



// Optimizations that work

#define DONT_ABSTRACT_PARTIAL_BUILTINS

// Optimizations that don't work (yet)

//#define REMOVE_REDUNDANT_SUPERCOMBINATORS
#define STRICTNESS_ANALYSIS

// Misc

//#define LAMBDAS_ARE_MAXFREE_BEFORE_PROCESSING
//#define CHECK_FOR_SUPERCOMBINATOR_SHARED_CELLS

#define HISTOGRAM_SIZE 10

#define BLOCK_SIZE 1024
/* #define COLLECT_THRESHOLD 1024000 */
#define COLLECT_THRESHOLD 102400
//#define COLLECT_THRESHOLD 100

#define celldouble(c) (*(double*)(&((c)->field1)))
#define hasfield1str(type) ((TYPE_STRING == (type)) ||   \
                            (TYPE_SYMBOL == (type)) || \
                            (TYPE_LAMBDA == (type)) ||   \
                            (TYPE_VARDEF == (type)))

#define TYPE_EMPTY       0x00
#define TYPE_APPLICATION 0x01  /* left: function (cell*)   right: argument (cell*) */
#define TYPE_LAMBDA      0x02  /* left: name (char*)       right: body (cell*)     */
#define TYPE_BUILTIN     0x03  /* left: bif (int)                                  */
#define TYPE_CONS        0x04  /* left: head (cell*)       right: tail (cell*)     */
#define TYPE_SYMBOL      0x05  /* left: name (char*)  - a free variable            */
#define TYPE_LETREC      0x06  /* left: vars (cell*)       right: body (cell*)     */
#define TYPE_VARDEF      0x07  /* left: name (char*)       right: body (cell*)     */
#define TYPE_VARLNK      0x08  /* left: def (cell*)        right: next (cell*)     */
#define TYPE_IND         0x09  /* left: tgt (cell*)                                */
#define TYPE_FUNCTION    0x0A  /* left: nargs(int)         right: address          */
#define TYPE_SCREF       0x0B  /* left: scomb (scomb*)                             */
#define TYPE_AREF        0x0C  /* left: array (cell*)      right: index            */
#define TYPE_RES4        0x0D  /*                                                  */
#define TYPE_RES5        0x0E  /*                                                  */
#define TYPE_RES6        0x0F  /*                                                  */
#define TYPE_NIL         0x10  /*                                                  */
#define TYPE_INT         0x11  /*                                                  */
#define TYPE_DOUBLE      0x12  /*                                                  */
#define TYPE_STRING      0x13  /*                                                  */
#define TYPE_ARRAY       0x14  /* left: array (carray*)                            */
#define NUM_CELLTYPES    0x15

#define VALUE_FIELD      0x10

#define isvalue(c) ((c)->tag & VALUE_FIELD)

#define B_ADD            0
#define B_SUBTRACT       1
#define B_MULTIPLY       2
#define B_DIVIDE         3
#define B_MOD            4

#define B_EQ             5
#define B_NE             6
#define B_LT             7
#define B_LE             8
#define B_GT             9
#define B_GE             10

#define B_AND            11
#define B_OR             12
#define B_NOT            13

#define B_BITSHL         14
#define B_BITSHR         15
#define B_BITAND         16
#define B_BITOR          17
#define B_BITXOR         18
#define B_BITNOT         19

#define B_IF             20
#define B_CONS           21
#define B_HEAD           22
#define B_TAIL           23

#define B_ISLAMBDA       24
#define B_ISVALUE        25
#define B_ISCONS         26
#define B_ISNIL          27
#define B_ISINT          28
#define B_ISDOUBLE       29
#define B_ISSTRING       30

#define B_CONVERTARRAY   31
#define B_ARRAYITEM      32
#define B_ARRAYHAS       33
#define B_ARRAYEXT       34
#define B_ARRAYSIZE      35
#define B_ARRAYTAIL      36
#define B_ARRAYOPTLEN    37

#define NUM_BUILTINS     38

#define TAG_MASK         0xFF

#define FLAG_MARKED      0x100
#define FLAG_PROCESSED   0x200
#define FLAG_PRINTED     0x400
#define FLAG_MAXFREE     0x800
#define FLAG_PINNED     0x1000
#define FLAG_STRICT     0x2000
#define FLAG_NEEDCLEAR  0x4000

typedef void (jitfun)();

#define CELL_TAG    ((int)&((cell*)0)->tag)
#define CELL_FIELD1 ((int)&((cell*)0)->field1)
#define CELL_FIELD2 ((int)&((cell*)0)->field2)

typedef struct cell {
  int tag;
  void *field1;
  void *field2;
  int id;
  struct cell *source;
  const char *filename;
  int lineno;
  int level;

  struct cell *dest;
} cell;

#define celltype(_c) ((_c)->tag & TAG_MASK)

typedef struct carray {
  int alloc;
  int size;
  cell **cells;
  cell **refs;
  cell *tail;
  cell *sizecell;
} carray;

typedef struct array {
  int alloc;
  int size;
  void *data;
} array;

typedef struct abstraction {
  struct abstraction *next;
  cell *body;
  char *name;
  cell *replacement;
  int level;
} abstraction;

typedef struct flist {
  int **item;
  int alloc;
  int count;
  int nargs;
} flist;

flist *flist_new(int nargs);
void flist_free(flist *fl);
void flist_add(flist *fl, int *front);
int flist_contains(flist *fl, int *front);
flist *flist_copy(flist *src);

typedef struct scomb {
  char *name;
  int nargs;
  char **argnames;
  int *mayterminate;
  cell *lambda;
  cell *body;
  cell *origlambda;
  abstraction *abslist;
  int ncells;
  cell **cells;
  int index;
  int iscopy;
  int internal;

  int *strictin;

  struct scomb *next;
} scomb;

typedef struct block {
  struct block *next;
  cell cells[BLOCK_SIZE];
} block;

typedef void (*builtin_f)();

typedef struct builtin {
  char *name;
  int nargs;
  int nstrict;
  int reswhnf;
  builtin_f f;
} builtin;

/* memory */

struct dumpentry *pushdump();

void initmem();
cell *alloc_cell();
cell *alloc_sourcecell(const char *filename, int lineno);
void collect();
void free_scomb(scomb *sc);
void cleanup();
void push(cell *c);
void insert(cell *c, int pos);
cell *pop();
cell *top();
cell *stackat(int s);
void statistics(FILE *f);
cell *resolve_source(cell *c);
void copy_raw(cell *dest, cell *source);
void copy_cell(cell *redex, cell *source);
void clearflag(int flag);
void cleargraph(cell *root, int flag);
void free_cell_fields(cell *c);
void print_stack(int redex, cell **stk, int size, int dir);
void adjust_stack(int nargs);

cell *resolve_ind2(cell *c);
#ifdef INLINE_RESOLVE_IND
#define resolve_ind(expr) ({ cell *_rc = (expr); \
                             while (TYPE_IND == celltype(_rc)) \
                               _rc = (cell*)_rc->field1; \
                             _rc; })
#else
cell *resolve_ind(cell *c);
#endif


/* code */

cell *suball_letrecs(cell *root, scomb *sc);

/* super */

scomb *get_scomb_index(int index);
scomb *get_scomb(const char *name);
scomb *get_fscomb(const char *name_format, ...);
scomb *get_scomb_origlambda(cell *lambda);
int get_scomb_var(scomb *sc, const char *name);
int scomb_count();
scomb *add_scomb(char *name, char *prefix);
scomb *build_scomb(cell *body, int nargs, char **argnames, int iscopy, int internal, 
                   char *name_format, ...);
void scomb_free_list(scomb **list);
void scomb_free(scomb *sc);
cell *check_for_lambda(cell *c);
void check_scombs();
void check_scombs_nosharing();
void scomb_calc_cells(scomb *sc);
void print_scomb_code(scomb *sc);
void print_scombs1();
void print_scombs2();
int scomb_isgraph(scomb *sc);
void lift(cell **k, int iscopy, int calccells, char *prefix);
void clearflag_scomb(int flag, scomb *sc);
cell *super_to_letrec(scomb *sc);
void remove_redundant_scombs();
void fix_partial_applications();
void resolve_scvars(cell *c);

/* reduction */

void too_many_arguments(cell *target);
void reduce();

/* extra */

void fatal(const char *msg);
void debug(int depth, const char *format, ...);
void debug_stage(const char *name);
void cellmsg(FILE *f, cell *c, const char *format, ...);
void print(cell *c);
void print_dot(FILE *f, cell *c);
void print_graphdot(char *filename, cell *root);
void print_codef(FILE *f, cell *c);
void print_code(cell *c);

/* verify */
int verify_noneedclear();

/* jit */
void print_cell(cell *c);

/* strictness */

void dump_strictinfo();
void strictness_analysis();

/* util */

array *array_new();
int array_equals(array *a, array *b);
void array_mkroom(array *arr, const int size);
void array_append(array *arr, const void *data, int size);
void array_free(array *arr);

int is_set(char *s);
int set_contains(char *s, char *item);
char *set_empty();
char *set_intersection(char *a, char *b);
char *set_union(char *a, char *b);

void print_quoted_string(FILE *f, const char *str);

typedef struct list list;

typedef void (*list_d_t)(void *a);
typedef void* (*list_copy_t)(void *a);

struct list {
  void *data;
  list *next;
};

list *list_copy(list *orig, list_copy_t copy);
void list_append(list **l, void *data);
void list_push(list **l, void *data);
void *list_pop(list **l);
int list_count(list *l);
void list_free(list *l, list_d_t d);

int list_contains_string(list *l, const char *str);
int list_contains_ptr(list *l, const void *data);
void list_remove_ptr(list **l, void *ptr);



#ifndef EXTRA_C
extern const char *cell_types[NUM_CELLTYPES];
#endif

#ifndef MEMORY_C
extern struct dumpentry *dumpstack;
extern int dumpalloc;
extern int dumpcount;

extern int trace;
extern cell *global_root;
extern struct dumpentry *dump;
extern cell **stack;
extern int stackalloc;
extern int stackcount;
extern int stackbase;
extern block *blocks;
extern int nblocks;
extern int nallocs;
extern int nscombappls;
extern int nALLOCs;
extern int nALLOCcells;
extern int ndumpallocs;
extern int nunwindsvar;
extern int nunwindswhnf;
extern int nreductions;
extern int ndispexact;
extern int ndispless;
extern int ndispgreater;
extern int ndisp0;
extern int ndispother;

extern cell *globnil;
extern cell *globtrue;
extern cell *globzero;
extern char *collect_ebp;
extern char *collect_esp;
#endif

#ifndef SUPER_C
extern scomb *scombs;
extern scomb **lastsc;
extern int nscombs;
#endif

#ifndef BUILTINS_C
extern const builtin builtin_info[NUM_BUILTINS];
#endif

#endif /* _NREDUCE_H */
