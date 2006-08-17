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

#define celldouble(c) (*(double*)(&((c)->field1)))
#ifdef UNBOXED_NUMBERS
#define pntrdouble(p) (p)
#else
#define pntrdouble(p) ({assert(is_pntr(p)); get_pntr(p)->cmp1; })
#endif

#define TYPE_EMPTY       0x00
#define TYPE_APPLICATION 0x01  /* left: function (cell*)   right: argument (cell*) */
#define TYPE_LAMBDA      0x02  /* left: name (char*)       right: body (cell*)     */
#define TYPE_BUILTIN     0x03  /* left: bif (int)                                  */
#define TYPE_CONS        0x04  /* left: head (cell*)       right: tail (cell*)     */
#define TYPE_SYMBOL      0x05  /* left: name (char*)                               */
#define TYPE_LETREC      0x06  /* left: defs (letrec*)     right: body (cell*)     */
#define TYPE_RES2        0x07  /*                                                  */
#define TYPE_RES3        0x08  /*                                                  */
#define TYPE_IND         0x09  /* left: tgt (cell*)                                */
#define TYPE_RES1        0x0A  /*                                                  */
#define TYPE_SCREF       0x0B  /* left: scomb (scomb*)                             */
#define TYPE_AREF        0x0C  /* left: array (cell*)      right: index            */
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
#define isvaluefun(c) (isvalue(c) || (TYPE_BUILTIN == celltype(c) || (TYPE_SCREF == celltype(c))))

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

#define B_CONVERTARRAY   30
#define B_ARRAYITEM      31
#define B_ARRAYHAS       32
#define B_ARRAYEXT       33
#define B_ARRAYSIZE      34
#define B_ARRAYTAIL      35
#define B_ARRAYOPTLEN    36

#define B_ECHO           37
#define B_PRINT          38

#define B_SQRT           39
#define B_FLOOR          40
#define B_CEIL           41

#define NUM_BUILTINS     42

#define TAG_MASK         0xFF

#define FLAG_MARKED      0x100
#define FLAG_PROCESSED   0x200
#define FLAG_TMP         0x400
#define FLAG_MAXFREE     0x800
#define FLAG_PINNED     0x1000
#define FLAG_STRICT     0x2000
#define FLAG_NEEDCLEAR  0x4000

#define CELL_TAG    ((int)&((cell*)0)->tag)
#define CELL_FIELD1 ((int)&((cell*)0)->field1)
#define CELL_FIELD2 ((int)&((cell*)0)->field2)

typedef struct cell {
  int tag;
  void *field1;
  void *field2;
} cell;

#define celltype(_c) ((_c)->tag & TAG_MASK)
#define pntrtype(p) (is_pntr(p) ? celltype(get_pntr(p)) : TYPE_NUMBER)

typedef double pntr;

typedef struct rtvalue {
  int tag;
  pntr cmp1;
  pntr cmp2;
} rtvalue;

#ifdef INLINE_PTRFUNS
#define is_pntr(__p) (*(((unsigned int*)&(__p))+1) == 0xFFFFFFF1)
#define make_pntr(__c) ({ double __d;                                     \
                        *(((unsigned int*)&__d)+0) = (unsigned int)(__c); \
                        *(((unsigned int*)&__d)+1) = 0xFFFFFFF1;          \
                        __d; })
#define get_pntr(__p) ({assert(is_pntr(__p)); ((rtvalue*)(*((unsigned int*)&(__p)))); })

#define is_string(__p) (*(((unsigned int*)&(__p))+1) == 0xFFFFFFF2)
#define make_string(__c) ({ double __d;                                       \
                            *(((unsigned int*)&__d)+0) = (unsigned int)(__c);  \
                            *(((unsigned int*)&__d)+1) = 0xFFFFFFF2;      \
                            __d; })
#define get_string(__p) ({assert(is_string(__p)); ((char*)(*((unsigned int*)&(__p)))); })
#define resolve_pntr(x) ({ pntr __x = (x);        \
                           while (TYPE_IND == pntrtype(__x)) \
                             __x = get_pntr(__x)->cmp1; \
                           __x; })
#define pntrequal(__a,__b) ((*(((unsigned int*)&(__a))+0) == *(((unsigned int*)&(__b))+0)) && \
                            (*(((unsigned int*)&(__a))+1) == *(((unsigned int*)&(__b))+1)))

#define is_nullpntr(__p) (is_pntr(__p) && (NULL == get_pntr(__p)))

#else
int is_pntr(pntr p);
pntr make_pntr(void *c);
rtvalue *get_pntr(pntr p);

int is_string(pntr p);
pntr make_string(char *c);
char *get_string(pntr p);
pntr resolve_pntr(pntr p);
int pntrequal(pntr a, pntr b);
int is_nullpntr(pntr p);
#endif


#ifdef UNBOXED_NUMBERS
#define make_number(d) (d)
#else
pntr make_number(double d);
#endif

typedef struct letrec {
  char *name;
  cell *value;
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
  pntr *cells;
  pntr *refs;
  pntr tail;
  pntr sizecell;
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
  cell *body;
  int index;
  int *strictin;
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
} cap;

typedef struct frame {
  int address;
  struct frame *d;

  int alloc;
  int count;
  pntr *data;

  rtvalue *c;
  int fno; /* temp */
  int active;
  int completed;
  struct frame *next;
  struct frame *freelnk;
} frame;

/* memory */

void initmem();
cell *alloc_cell(void);
cell *alloc_cell2(int tag, void *field1, void *field2);
cell *alloc_sourcecell(const char *filename, int lineno);
void collect();
void free_letrec(letrec *rec);
void free_scomb(scomb *sc);
void cleanup();

pntrstack *pntrstack_new(void);
void pntrstack_push(pntrstack *s, pntr p);
pntr pntrstack_at(pntrstack *s, int pos);
pntr pntrstack_pop(pntrstack *s);
void pntrstack_free(pntrstack *s);

stack *stack_new(void);
stack *stack_new2(int alloc);
void stack_free(stack *s);
void stack_push2(stack *s, void *c);
void stack_push(stack *s, void *c);
void stack_insert(stack *s, void *c, int pos);
cell *stack_pop(stack *s);
cell *stack_top(stack *s);
cell *stack_at(stack *s, int pos);
void pntrstack_grow(int *alloc, pntr **data, int size);

void statistics(FILE *f);
void copy_raw(cell *dest, cell *source);
void copy_cell(cell *redex, cell *source);
void free_cell_fields(cell *c);
void print_stack(pntr *stk, int size, int dir);

cell *resolve_ind(cell *c);

/* rtvalue */

rtvalue *alloc_rtvalue(void);
void free_rtvalue_fields(rtvalue *v);

void rtcollect();

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

void reorder_letrecs(cell *c);

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

void cleargraph(cell *root, int flag);
void find_graph_cells(cell ***cells, int *ncells, cell *root);

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
int count_args(cell *c);
cell *get_arg(cell *c, int argno);
void print(cell *c);
void print_codef2(FILE *f, cell *c, int pos);
void print_codef(FILE *f, cell *c);
void print_code(cell *c);
void print_scomb_code(scomb *sc);
void print_scombs1();
void print_scombs2();
const char *real_varname(const char *sym);
char *real_scname(const char *sym);

/* jit */
void print_double(FILE *f, double d);
void output_pntr(pntr p);

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
void parse_check(int cond, cell *c, char *msg);

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

#ifndef EXTRA_C
extern const char *cell_types[NUM_CELLTYPES];
#endif

#ifndef MEMORY_C
extern int trace;
extern int nblocks;
extern int nallocs;
extern int ncollections;
extern int totalrtallocs;
extern int nscombappls;
extern int nreductions;
extern int nframes;
extern int maxdepth;
extern int maxframes;

extern cell *globnil;
extern cell *globtrue;
extern cell *globzero;
extern pntr globnilpntr;
extern pntr globtruepntr;
extern pntr globzeropntr;
extern char *collect_ebp;
extern char *collect_esp;
#endif

#ifndef SUPER_C
extern scomb *scombs;
extern scomb **lastsc;
#endif

#ifndef BUILTINS_C
extern const builtin builtin_info[NUM_BUILTINS];
#endif

#endif /* _NREDUCE_H */
