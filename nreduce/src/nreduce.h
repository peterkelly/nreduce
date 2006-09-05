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
#include <pthread.h>
#include <stdarg.h>
#include <sys/time.h>
#include <time.h>

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
#define EXECUTION_TRACE
#define PROFILING
//#define QUEUE_CHECKS
//#define PAREXEC_DEBUG
//#define MSG_DEBUG
//#define SHOW_FRAME_COMPLETION
//#define COLLECTION_DEBUG


// Misc

#define BLOCK_SIZE 1024
//#define COLLECT_THRESHOLD 102400
#define COLLECT_THRESHOLD 1024
#define COLLECT_INTERVAL 100000
#define STACK_LIMIT 10240
#define GLOBAL_HASH_SIZE 256

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
#define TYPE_REMOTEREF   0x07  /*                                                  */
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

#define isvalue(c) ((c)->type & VALUE_FIELD)
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

#define B_SEQ            40
#define B_PAR            41
#define B_PARHEAD        42

#define NUM_BUILTINS     43

#define TAG_MASK         0xFF

#define FLAG_MARKED      0x100
#define FLAG_PROCESSED   0x200
#define FLAG_TMP         0x400
#define FLAG_DMB         0x800
#define FLAG_PINNED     0x1000
#define FLAG_STRICT     0x2000
#define FLAG_NEEDCLEAR  0x4000

#define MSG_ISTATS       0
#define MSG_ALLSTATS     1
#define MSG_DUMP_INFO    2
#define MSG_DONE         3
#define MSG_PAUSE        4
#define MSG_RESUME       5
#define MSG_FISH         6
#define MSG_FETCH        7
#define MSG_TRANSFER     8
#define MSG_ACK          9
#define MSG_MARKROOTS    10
#define MSG_MARKENTRY    11
#define MSG_SWEEP        12
#define MSG_SWEEPACK     13
#define MSG_UPDATE       14
#define MSG_RESPOND      15
#define MSG_SCHEDULE     16
#define MSG_UPDATEREF    17
#define MSG_TEST         18
#define MSG_STARTDISTGC  19
#define MSG_COUNT        20

#define MAX_FRAME_SIZE   1024
#define MAX_CAP_SIZE     1024

struct letrec;
struct scomb;
struct gprogram;
struct process;

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

#define checkcell(_c) ({ if (TYPE_EMPTY == (_c)->type) \
                          fatal("access to free'd cell"); \
                        (_c); })

#define snodetype(_c) ((_c)->tag & TAG_MASK)
//#define celltype(_c) ((_c)->type)
#define celltype(_c) (checkcell(_c)->type)
#define pntrtype(p) (is_pntr(p) ? (get_pntr(p))->type : TYPE_NUMBER)

typedef double pntr;

typedef struct cell {
  short flags;
  short type;
  pntr field1;
  pntr field2;

  short oldtype; /* TEMP */
} cell;

#define pfield1(__p) (get_pntr(__p)->field1)
#define pfield2(__p) (get_pntr(__p)->field2)
#define ppfield1(__p) (get_pntr(pfield1(__p)))
#define ppfield2(__p) (get_pntr(pfield2(__p)))
#define pglobal(__p) ((global*)ppfield1(__p))
#define pframe(__p) ((frame*)ppfield1(__p))

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

typedef void (*builtin_f)(struct process *proc, pntr *argstack);

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
  sourceloc sl;
  int count;

  pntr *data;
} cap;

typedef struct waitqueue {
  list *frames;
  list *fetchers;
} waitqueue;

#define STATE_NEW       0
#define STATE_SPARKED   1
#define STATE_RUNNING   2
#define STATE_DONE      3

typedef struct frame {
  int address;
  waitqueue wq;
/*   int id; */

  cell *c;
  int fno; /* temp */

  int alloc;
  int count;
  pntr *data;

  int state;

  struct frame *freelnk;
  struct frame *qnext;
  struct frame *qprev;

  struct frame *waitframe;
  struct global *waitglo;
} frame;

typedef struct block {
  struct block *next;
  cell values[BLOCK_SIZE];
} block;

typedef struct message {
  int from;
  int to;
  int size;
  char *data;
} message;

typedef struct group {
  struct process **procs;
  int nprocs;
} group;

typedef struct frameq {
  frame *first;
  frame *last;
  int size;
} frameq;

typedef struct procstats {
  int *funcalls;
  int *usage;
  int *op_usage;
  int totalallocs;
  int ncollections;
  int nscombappls;
  int nreductions;
  int nsparks;
  int nallocs;
  int nextframeid;
  int *framecompletions;
  int *sendcount;
  int *sendbytes;
  int *recvcount;
  int *recvbytes;
} procstats;

typedef struct gaddr {
  int pid;
  int lid;
} gaddr;

/* For "entry items" (globally visible objects that this process owns):
     proc = this proc id
     id = (proc)-unique id for the object
     p = pointer to the actual object

  For "exit items" (globally visible objects on another processor that we have a ref to):
     proc = the processor (not us) that owns the object
     id = (proc)-unique id for the object      -- may be -1
     p = pointer to a REMOTE_REF object

  For "replicas" (local copies we've made of objects that other processors own):
     proc = the processor (not us) that owns the object
     id = (proc)-unique id for the object
     p = pointer to our copy of the object
*/
typedef struct global {
  gaddr addr;
  pntr p;

  int fetching;
  waitqueue wq;
  int flags;
  int freed;
  struct global *pntrnext;
  struct global *addrnext;
} global;

typedef struct process {
  int memdebug;

  /* communication */
  group *grp;
  list *msgqueue;
  int pid;
  pthread_mutex_t msglock;
  pthread_cond_t msgcond;
  int ackmsgsize;
  array *ackmsg;
  array **markmsgs;

  /* globals */
  global **pntrhash;
  global **addrhash;

  /* runtime info */
  int done;
  int paused;
  frameq sparked;
  frameq runnable;
  int nextentryid;
  int nextblackholeid;
  int nextlid;
  int *gcsent;
  list *inflight;

  /* memory */
  block *blocks;
  cell *freeptr;
  pntr globnilpntr;
  pntr globtruepntr;
  pntr *strings;
  int nstrings;
  pntrstack *streamstack;
  int indistgc;

  /* general */
  struct gprogram *gp;
  FILE *output;
  procstats stats;
} process;

#define check_global(_g) (assert(!(_g)->freed))

typedef struct reader {
  const char *data;
  int size;
  int pos;
} reader;

/* data */

#define READER_OK                  0
#define READER_INCOMPLETE_DATA    -1
#define READER_INVALID_DATA       -2
#define READER_INCORRECT_TYPE     -3
#define READER_EXTRA_DATA         -4
#define READER_INCORRECT_CONTENTS -5

#define CHAR_TAG   0x44912234
#define INT_TAG    0xA492BC09
#define DOUBLE_TAG 0x44ABC92F
#define STRING_TAG 0x93EB1123
#define GADDR_TAG  0x85113B1C
#define PNTR_TAG   0xE901FA12
#define REF_TAG    PNTR_TAG

//#define CHECK_READ(_x) { int _r; if (READER_OK != (_r = (_x))) return _r; }
#define CHECK_READ(_x) { int _r; if (READER_OK != (_r = (_x))) { abort(); return _r; } }
//#define CHECK_EXPR(_x) { if (!(_x)) { abort(); return READER_INCORRECT_CONTENTS; } }
#define CHECK_EXPR(_x) { assert(_x); }

reader read_start(const char *data, int size);
int read_char(reader *rd, char *c);
int read_int(reader *rd, int *i);
int read_double(reader *rd, double *d);
int read_string(reader *rd, char **s);
int read_gaddr_noack(reader *rd, gaddr *a);
int read_gaddr(reader *rd, process *proc, gaddr *a);
int read_pntr(reader *rd, process *proc, pntr *pout, int observe);
int read_vformat(reader *rd, process *proc, int observe, const char *fmt, va_list ap);
int read_format(reader *rd, process *proc, int observe, const char *fmt, ...);
int read_end(reader *rd);
int print_data(process *proc, const char *data, int size);

array *write_start(void);
void write_tag(array *wr, int tag);
void write_char(array *wr, char c);
void write_int(array *wr, int i);
void write_double(array *wr, double d);
void write_string(array *wr, char *s);
void write_gaddr_noack(array *wr, gaddr a);
void write_gaddr(array *wr, process *proc, gaddr a);
void write_ref(array *arr, process *proc, pntr p);
void write_pntr(array *arr, process *proc, pntr p);
void write_vformat(array *wr, process *proc, const char *fmt, va_list ap);
void write_format(array *wr, process *proc, const char *fmt, ...);
void write_end(array *wr);

void msg_send(process *proc, int dest, const char *data, int size);
void msg_fsend(process *proc, int dest, const char *fmt, ...);
int msg_recv(process *proc, char **data, int *size);
int msg_recvb(process *proc, char **data, int *size);
int msg_recvbt(process *proc, char **data, int *size, struct timespec *abstime);

/* memory */

void initmem(void);
snode *snode_new(int fileno, int lineno);
void snode_free(snode *c);
void free_letrec(letrec *rec);
void free_scomb(scomb *sc);
void cleanup(void);

process *process_new(void);
void process_init(process *proc, struct gprogram *gp);
void process_free(process *proc);

const char *lookup_parsedfile(int fileno);
int add_parsedfile(const char *filename);

pntrstack *pntrstack_new(void);
void pntrstack_push(pntrstack *s, pntr p);
pntr pntrstack_at(pntrstack *s, int pos);
pntr pntrstack_pop(pntrstack *s);
void pntrstack_free(pntrstack *s);

void pntrstack_grow(int *alloc, pntr **data, int size);

/* process */

global *pntrhash_lookup(process *proc, pntr p);
global *global_addrhash_lookup(process *proc, gaddr addr);
void pntrhash_add(process *proc, global *glo);
void addrhash_add(process *proc, global *glo);
void pntrhash_remove(process *proc, global *glo);
void addrhash_remove(process *proc, global *glo);

global *add_global(process *proc, gaddr addr, pntr p);
global *global_lookup_glo(process *proc, gaddr addr);
pntr global_lookup_existing(process *proc, gaddr addr);
pntr global_lookup(process *proc, gaddr addr, pntr val);
global *make_global(process *proc, pntr p);
gaddr global_addressof(process *proc, pntr p);

void add_gaddr(list **l, gaddr addr);
void remove_gaddr(process *proc, list **l, gaddr addr);

void add_frame_queue(frameq *q, frame *f);
void add_frame_queue_end(frameq *q, frame *f);
void remove_frame_queue(frameq *q, frame *f);
void transfer_waiters(waitqueue *from, waitqueue *to);

void spark_frame(process *proc, frame *f);
void unspark_frame(process *proc, frame *f);
void run_frame(process *proc, frame *f);
void done_frame(process *proc, frame *f);

/* cell */

cell *alloc_cell(process *proc);
void free_cell_fields(process *proc, cell *v);

int count_alive(process *proc);
void clear_marks(process *proc, short bit);
void mark_roots(process *proc, short bit);
void print_cells(process *proc);
void sweep(process *proc);
void mark_global(process *proc, global *glo, short bit);
void local_collect(process *proc);

frame *frame_alloc(process *proc);
void frame_dealloc(process *proc, frame *f);

cap *cap_alloc(int arity, int address, int fno);
void cap_dealloc(cap *c);

void print_pntr(FILE *f, pntr p);

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
void fix_partial_applications(void);
void check_scombs_nosharing(void);

/* lifting */

void lift(scomb *sc);
void applift(scomb *sc);

/* reduction */

void reduce(process *h, pntrstack *s);

/* graph */

void cleargraph(snode *root, int flag);
void find_snodes(snode ***nodes, int *nnodes, snode *root);

/* new */

void rename_variables(scomb *sc);

/* console */

void console(process *proc);

/* debug */

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
void print_scombs1(void);
void print_scombs2(void);
const char *real_varname(const char *sym);
char *real_scname(const char *sym);
void print_stack(FILE *f, pntr *stk, int size, int dir);
void statistics(process *proc, FILE *f);
void print_pntr_tree(FILE *f, pntr p, int indent);
void dump_info(process *proc);

/* strictness */

void dump_strictinfo(void);
void strictness_analysis(void);

/* builtin */

pntr get_aref(process *proc, cell *arrholder, int index);
int get_builtin(const char *name);

/* util */

array *array_new(void);
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

struct timeval timeval_diff(struct timeval from, struct timeval to);

int hash(void *mem, int size);

#ifndef DEBUG_C
extern const char *snode_types[NUM_CELLTYPES];
extern const char *cell_types[NUM_CELLTYPES];
extern const char *msg_names[MSG_COUNT];
extern const char *frame_states[4];
#endif

#ifndef MEMORY_C
extern int trace;
#endif

#ifndef SUPER_C
extern scomb *scombs;
extern scomb **lastsc;
#endif

#ifndef BUILTINS_C
extern const builtin builtin_info[NUM_BUILTINS];
#endif

int _pntrtype(pntr p);
const char *_pntrtname(pntr p);
global *_pglobal(pntr p);
frame *_pframe(pntr p);

#endif /* _NREDUCE_H */
