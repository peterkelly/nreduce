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

#ifdef WIN32
#define YY_NO_UNISTD_H
#include <float.h>
#define isnan _isnan
#define isinf(__x) (!_finite(__x))
#define NO_STATEMENT_EXPRS
#else
#define TIMING
#endif

#define INLINE_PTRFUNS
#define UNBOXED_NUMBERS

//#define NDEBUG

//#define DEBUG_GCODE_COMPILATION
//#define STACK_MODEL_SANITY_CHECK
//#define SHOW_SUBSTITUTED_NAMES
//#define EXECUTION_TRACE
//#define PROFILING

// Misc

#define BLOCK_SIZE 1024
#define COLLECT_THRESHOLD 102400
//#define COLLECT_THRESHOLD 1024
#define STACK_LIMIT 10240

#ifdef UNBOXED_NUMBERS
#define pntrdouble(p) (p)
#else
#define pntrdouble(p) ({assert(is_pntr(p)); get_pntr(p)->field1; })
#endif

#define TYPE_EMPTY       0x00
#define TYPE_APPLICATION 0x01  /* left: function (snode*)   right: argument (snode*) */
#define TYPE_LAMBDA      0x02  /* left: name (char*)       right: body (snode*)     */
#define TYPE_BUILTIN     0x03  /* left: bif (int)                                  */
#define TYPE_CONS        0x04  /* left: head (snode*)       right: tail (snode*)     */
#define TYPE_SYMBOL      0x05  /* left: name (char*)                               */
#define TYPE_LETREC      0x06  /* left: defs (letrec*)     right: body (snode*)     */
#define TYPE_RES2        0x07  /*                                                  */
#define TYPE_RES3        0x08  /*                                                  */
#define TYPE_IND         0x09  /* left: tgt (snode*)                                */
#define TYPE_RES1        0x0A  /*                                                  */
#define TYPE_SCREF       0x0B  /* left: scomb (scomb*)                             */
#define TYPE_AREF        0x0C  /* left: array (snode*)      right: index            */
#define TYPE_HOLE        0x0D  /*                                                  */
#define TYPE_FRAME       0x0E  /* left: frame (frame*)                             */
#define TYPE_CAP         0x0F  /* left: cap (cap*)                                 */
#define TYPE_NIL         0x10  /*                                                  */
#define TYPE_NUMBER      0x11  /*                                                  */
#define TYPE_STRING      0x12  /*                                                  */
#define TYPE_ARRAY       0x13  /* left: array (carray*)                            */
#define NUM_CELLTYPES    0x14

#define VALUE_FIELD      0x10

#define isvalue(c) ((c)->tag & VALUE_FIELD)
#define isvaluetype(t) ((t) & VALUE_FIELD)
#define isvaluefun(c) (isvalue(c) || (TYPE_BUILTIN == snodetype(c) || (TYPE_SCREF == snodetype(c))))

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
#define B_ISNUMBER       28
#define B_ISSTRING       29

#define B_ARRAYITEM      30
#define B_ARRAYHAS       31
#define B_ARRAYREMSIZE   32
#define B_ARRAYREM       33
#define B_ARRAYOPTLEN    34

#define B_ECHO           35
#define B_PRINT          36

#define B_SQRT           37
#define B_FLOOR          38
#define B_CEIL           39

#define NUM_BUILTINS     40

#define TAG_MASK         0xFF

#define FLAG_MARKED      0x100
#define FLAG_PROCESSED   0x200
#define FLAG_TMP         0x400
#define FLAG_MAXFREE     0x800
#define FLAG_PINNED     0x1000
#define FLAG_STRICT     0x2000
#define FLAG_NEEDCLEAR  0x4000

struct letrec;
struct scomb;

typedef struct sourceloc {
  int fileno;
  int lineno;
} sourceloc;

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

#define snodetype(_c) ((_c)->tag & TAG_MASK)
#define celltype snodetype
#define pntrtype(p) (is_pntr(p) ? snodetype(get_pntr(p)) : TYPE_NUMBER)

typedef double pntr;

typedef struct cell {
  int tag;
  pntr field1;
  pntr field2;
} cell;


#ifdef INLINE_PTRFUNS
#define is_pntr(__p) (*(((unsigned int*)&(__p))+1) == 0xFFFFFFF1)
#define make_pntr(__p,__c) { *(((unsigned int*)&(__p))+0) = (unsigned int)(__c); \
                            *(((unsigned int*)&(__p))+1) = 0xFFFFFFF1; }
#define get_pntr(__p) (assert(is_pntr(__p)), ((cell*)(*((unsigned int*)&(__p)))))

#define is_string(__p) (*(((unsigned int*)&(__p))+1) == 0xFFFFFFF2)
#define make_string(__p,__c) { *(((unsigned int*)&(__p))+0) = (unsigned int)(__c); \
                              *(((unsigned int*)&(__p))+1) = 0xFFFFFFF2; }
#define get_string(__p) (assert(is_string(__p)), ((char*)(*((unsigned int*)&(__p)))))
#define pntrequal(__a,__b) ((*(((unsigned int*)&(__a))+0) == *(((unsigned int*)&(__b))+0)) && \
                            (*(((unsigned int*)&(__a))+1) == *(((unsigned int*)&(__b))+1)))

#define is_nullpntr(__p) (is_pntr(__p) && (NULL == get_pntr(__p)))
#ifndef NO_STATEMENT_EXPRS
#define INLINE_RESOLVE_PNTR
#endif
#else
int is_pntr(pntr p);
cell *get_pntr(pntr p);

int is_string(pntr p);
char *get_string(pntr p);
int pntrequal(pntr a, pntr b);
int is_nullpntr(pntr p);
#endif

#ifdef INLINE_RESOLVE_PNTR
#define resolve_pntr(x) ({ pntr __x = (x);        \
                           while (TYPE_IND == pntrtype(__x)) \
                             __x = get_pntr(__x)->field1; \
                           __x; })
#else
pntr resolve_pntr(pntr p);
#endif


#ifdef UNBOXED_NUMBERS
#define make_number(d) (d)
#else
pntr make_number(double d);
#endif

typedef struct letrec {
  char *name;
  snode *value;
  int strict;
  struct scomb *sc;
  struct letrec *next;
} letrec;

typedef struct list list;
struct list {
  void *data;
  list *next;
};

typedef struct carray {
  int alloc;
  int size;
  pntr *elements;
  pntr *refs;
  pntr tail;
} carray;

typedef struct stack {
  int alloc;
  int count;
  void **data;
} stack;

typedef struct pntrstack {
  int alloc;
  int count;
  pntr *data;
} pntrstack;

typedef struct array {
  int alloc;
  int size;
  void *data;
} array;

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

typedef void (*builtin_f)(pntr *argstack);

typedef struct builtin {
  char *name;
  int nargs;
  int nstrict;
  int reswhnf;
  builtin_f f;
} builtin;

typedef struct cap {
  int arity;
  int address;
  int fno; /* temp */

  pntr *data;
  int count;
  sourceloc sl;
} cap;

typedef struct frame {
  int address;
  struct frame *d;

  int alloc;
  int count;
  pntr *data;

  cell *c;
  int fno; /* temp */
  int active;
  int completed;
  struct frame *next;
  struct frame *freelnk;
} frame;

/* memory */

void initmem();
snode *snode_new(int fileno, int lineno);
void snode_free(snode *c);
void free_letrec(letrec *rec);
void free_scomb(scomb *sc);
void cleanup();

const char *lookup_parsedfile(int fileno);
int add_parsedfile(const char *filename);

pntrstack *pntrstack_new(void);
void pntrstack_push(pntrstack *s, pntr p);
pntr pntrstack_at(pntrstack *s, int pos);
pntr pntrstack_pop(pntrstack *s);
void pntrstack_free(pntrstack *s);

void pntrstack_grow(int *alloc, pntr **data, int size);

/* cell */

cell *alloc_cell(void);
void free_cell_fields(cell *v);

void collect();

void add_active_frame(frame *f);
void remove_active_frame(frame *f);
frame *frame_new();
void frame_free(frame *f);
frame *frame_alloc();
void frame_dealloc(frame *f);
int frame_depth(frame *f);

cap *cap_alloc(int arity, int address, int fno);
void cap_dealloc(cap *c);

void rtcleanup();

void print_pntr(pntr p);

/* resolve */

void resolve_refs(scomb *sc);

/* reorder */

void reorder_letrecs(snode *c);

/* super */

scomb *get_scomb_index(int index);
scomb *get_scomb(const char *name);
int get_scomb_var(scomb *sc, const char *name);
scomb *add_scomb(const char *name);
void scomb_free_list(scomb **list);
void fix_partial_applications();
void check_scombs_nosharing();

/* lifting */

void lift(scomb *sc);
void applift(scomb *sc);

/* reduction */

void reduce(pntrstack *s);

/* graph */

void cleargraph(snode *root, int flag);
void find_snodes(snode ***nodes, int *nnodes, snode *root);

/* new */

void rename_variables(scomb *sc);

/* extra */

void fatal(const char *msg);
int debug(int depth, const char *format, ...);
void debug_stage(const char *name);
void print_hex(int c);
void print_hexbyte(unsigned char val);
void print_bin(void *ptr, int nbytes);
void print_bin_rev(void *ptr, int nbytes);
int count_args(snode *c);
snode *get_arg(snode *c, int argno);
void print(snode *c);
void print_codef2(FILE *f, snode *c, int pos);
void print_codef(FILE *f, snode *c);
void print_code(snode *c);
void print_scomb_code(scomb *sc);
void print_scombs1();
void print_scombs2();
const char *real_varname(const char *sym);
char *real_scname(const char *sym);
void print_stack(pntr *stk, int size, int dir);
void statistics(FILE *f);

/* strictness */

void dump_strictinfo();
void strictness_analysis();

/* builtin */

int get_builtin(const char *name);

/* util */

array *array_new();
int array_equals(array *a, array *b);
void array_mkroom(array *arr, const int size);
void array_append(array *arr, const void *data, int size);
void array_free(array *arr);

void print_quoted_string(FILE *f, const char *str);
void parse_check(int cond, snode *c, char *msg);
void print_sourceloc(FILE *f, sourceloc sl);

typedef void (*list_d_t)(void *a);
typedef void* (*list_copy_t)(void *a);

list *list_copy(list *orig, list_copy_t copy);
void list_append(list **l, void *data);
void list_push(list **l, void *data);
void *list_pop(list **l);
int list_count(list *l);
void *list_item(list *l, int item);
void list_free(list *l, list_d_t d);

int list_contains_string(list *l, const char *str);
int list_contains_ptr(list *l, const void *data);
void list_remove_ptr(list **l, void *ptr);

stack *stack_new(void);
void stack_free(stack *s);
void stack_push(stack *s, void *c);

void print_double(FILE *f, double d);

#ifndef EXTRA_C
extern const char *snode_types[NUM_CELLTYPES];
extern const char *cell_types[NUM_CELLTYPES];
#endif

#ifndef MEMORY_C
extern int maxstack;
extern int trace;
extern int nallocs;
extern int ncollections;
extern int totalallocs;
extern int nscombappls;
extern int nreductions;
extern int nframes;
extern int maxdepth;
extern int maxframes;

extern pntr globnilpntr;
extern pntr globtruepntr;
extern pntr globzeropntr;
#endif

#ifndef SUPER_C
extern scomb *scombs;
extern scomb **lastsc;
#endif

#ifndef BUILTINS_C
extern const builtin builtin_info[NUM_BUILTINS];
#endif

#endif /* _NREDUCE_H */
