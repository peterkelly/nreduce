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

#ifndef _RUNTIME_H
#define _RUNTIME_H

#include "src/nreduce.h"
#include "compiler/source.h"
#include "compiler/bytecode.h"
#include <stdio.h>

#define MAX_FRAME_SIZE   1024
#define MAX_CAP_SIZE     1024

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

#define B_SQRT           20
#define B_FLOOR          21
#define B_CEIL           22

#define B_SEQ            23
#define B_PAR            24
#define B_PARHEAD        25

#define B_ECHO           26
#define B_PRINT          27

#define B_IF             28
#define B_CONS           29
#define B_HEAD           30
#define B_TAIL           31

#define B_ARRAYSIZE      32
#define B_ARRAYSKIP      33
#define B_ARRAYPREFIX    34

#define B_ISVALARRAY     35
#define B_PRINTARRAY     36

#define B_NUMTOSTRING    37
#define B_OPENFD         38
#define B_READCHUNK      39

#define NUM_BUILTINS     40

#define checkcell(_c) ({ if (CELL_EMPTY == (_c)->type) \
                          fatal("access to free'd cell %p",(_c)); \
                        (_c); })

//#define celltype(_c) ((_c)->type)
#define celltype(_c) (checkcell(_c)->type)
#define pntrtype(p) (is_pntr(p) ? celltype(get_pntr(p)) : CELL_NUMBER)

typedef double pntr;

#define CELL_EMPTY       0x00
#define CELL_APPLICATION 0x01  /* left: function (cell*)   right: argument (cell*) */
#define CELL_BUILTIN     0x02  /* left: bif (int)                                  */
#define CELL_CONS        0x03  /* left: head (cell*)       right: tail (cell*)     */
#define CELL_REMOTEREF   0x04  /*                                                  */
#define CELL_IND         0x05  /* left: tgt (cell*)                                */
#define CELL_SCREF       0x06  /* left: scomb (scomb*)                             */
#define CELL_AREF        0x07  /* left: array (cell*)      right: index            */
#define CELL_HOLE        0x08  /*                                                  */
#define CELL_FRAME       0x09  /* left: frame (frame*)                             */
#define CELL_CAP         0x0A  /* left: cap (cap*)                                 */
#define CELL_NIL         0x0B  /*                                                  */
#define CELL_NUMBER      0x0C  /*                                                  */
#define CELL_ARRAY       0x0D  /* left: array (carray*)                            */
#define CELL_COUNT       0x0E

typedef struct cell {
  short flags;
  short type;
  pntr field1;
  pntr field2;
} cell;

#define PNTR_VALUE 0xFFF80000
//#define MAX_ARRAY_SIZE (1 << 19)
#define MAX_ARRAY_SIZE 2000

#define pfield1(__p) (get_pntr(__p)->field1)
#define pfield2(__p) (get_pntr(__p)->field2)
#define ppfield1(__p) (get_pntr(pfield1(__p)))
#define ppfield2(__p) (get_pntr(pfield2(__p)))
#define pglobal(__p) ((global*)ppfield1(__p))
#define pframe(__p) ((frame*)ppfield1(__p))

#define is_pntr(__p) ((*(((unsigned int*)&(__p))+1) & PNTR_VALUE) == PNTR_VALUE)
#define make_pntr(__p,__c) { *(((unsigned int*)&(__p))+0) = (unsigned int)(__c); \
                            *(((unsigned int*)&(__p))+1) = PNTR_VALUE; }

#define make_aref_pntr(__p,__c,__i) { assert((__i) < MAX_ARRAY_SIZE); \
                            *(((unsigned int*)&(__p))+0) = (unsigned int)(__c); \
                            *(((unsigned int*)&(__p))+1) = (PNTR_VALUE | (__i)); }

#define aref_index(__p) (*(((unsigned int*)&(__p))+1) & ~PNTR_VALUE)
#define aref_array(__p) ((carray*)get_pntr(get_pntr(__p)->field1))


#define get_pntr(__p) (assert(is_pntr(__p)), ((cell*)(*((unsigned int*)&(__p)))))

#define pntrequal(__a,__b) ((*(((unsigned int*)&(__a))+0) == *(((unsigned int*)&(__b))+0)) && \
                            (*(((unsigned int*)&(__a))+1) == *(((unsigned int*)&(__b))+1)))

#define is_nullpntr(__p) (is_pntr(__p) && (NULL == get_pntr(__p)))
#ifndef NO_STATEMENT_EXPRS
#define INLINE_RESOLVE_PNTR
#endif

#ifdef INLINE_RESOLVE_PNTR
#define resolve_pntr(x) ({ pntr __x = (x);        \
                           while (CELL_IND == pntrtype(__x)) \
                             __x = get_pntr(__x)->field1; \
                           __x; })
#else
pntr resolve_pntr(pntr p);
#endif


#define check_global(_g) (assert(!(_g)->freed))












typedef struct message {
  int from;
  int to;
  int tag;
  int size;
  char *data;
} message;

typedef struct group {
  struct process **procs;
  int nprocs;
} group;

typedef struct waitqueue {
  list *frames;
  list *fetchers;
} waitqueue;

typedef void (*builtin_f)(struct process *proc, pntr *argstack);

typedef struct builtin {
  char *name;
  int nargs;
  int nstrict;
  int reswhnf;
  builtin_f f;
} builtin;

typedef struct carray {
  int alloc;
  int size;
  int elemsize;
  void *elements;
  pntr tail;
  cell *wrapper;
  int nchars;
} carray;

typedef struct pntrstack {
  int alloc;
  int count;
  pntr *data;
} pntrstack;

typedef struct cap {
  int arity;
  int address;
  int fno; /* temp */
  sourceloc sl;
  int count;

  pntr *data;
} cap;

/* frame states */
#define STATE_NEW       0
#define STATE_SPARKED   1
#define STATE_RUNNING   2
#define STATE_BLOCKED   3
#define STATE_DONE      4

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
  int *fusage;
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
  struct global *pntrnext;
  struct global *addrnext;
} global;

/* process */

typedef struct block {
  struct block *next;
  cell values[BLOCK_SIZE];
} block;

typedef struct process {
  int memdebug;

  /* bytecode */
  const char *bcdata;
  int bcsize;

  /* communication */
  group *grp;
  list *msgqueue;
  int pid;
  int groupsize;
  pthread_mutex_t msglock;
  pthread_cond_t msgcond;
  int ackmsgsize;
  int naddrsread;
  array *ackmsg;
  array **distmarks;

  /* globals */
  global **pntrhash;
  global **addrhash;

  /* runtime info */
  int done;
  int paused;
  frameq sparked;
  frameq runnable;
  frameq blocked;
  int nextentryid;
  int nextblackholeid;
  int nextlid;
  int *gcsent;
  list *inflight;
  char *error;

  gaddr **infaddrs;
  int *infcount;
  int *infalloc;

  /* Each element of inflight_addrs is an array containing the addresses that have been sent
     to a particular process but not yet acknowledged. */
  array **inflight_addrs;

  /* Each element of unack_msg_acount is an array containing the number of addresses in each
     outstanding message to a particular process. An outstanding message is a message containing
     addresses that has been sent but not yet acknowledged. */
  array **unack_msg_acount;

  /* For each unacknowledged message, the number of addresses that were in that message */
  int **unackaddrs;
  int *unackcount;
  int *unackalloc;

  /* memory */
  block *blocks;
  cell *freeptr;
  pntr globnilpntr;
  pntr globtruepntr;
  pntr *strings;
  int nstrings;
  pntrstack *streamstack;
  int indistgc;
  int inmark;

  /* general */
  FILE *output;
  procstats stats;
} process;

process *process_new(void);
void process_init(process *proc);
void process_free(process *proc);

global *pntrhash_lookup(process *proc, pntr p);
global *addrhash_lookup(process *proc, gaddr addr);
void pntrhash_add(process *proc, global *glo);
void addrhash_add(process *proc, global *glo);
void pntrhash_remove(process *proc, global *glo);
void addrhash_remove(process *proc, global *glo);

global *add_global(process *proc, gaddr addr, pntr p);
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
void block_frame(process *proc, frame *f);
void unblock_frame(process *proc, frame *f);
void done_frame(process *proc, frame *f);

void set_error(process *proc, const char *format, ...);

/* data */

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

typedef struct reader {
  const char *data;
  int size;
  int pos;
} reader;

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

void msg_send(process *proc, int dest, int tag, char *data, int size);
void msg_fsend(process *proc, int dest, int tag, const char *fmt, ...);
int msg_recv(process *proc, int *tag, char **data, int *size);
int msg_recvb(process *proc, int *tag, char **data, int *size);
int msg_recvbt(process *proc, int *tag, char **data, int *size, struct timespec *abstime);

/* reduction */

void reduce(process *h, pntrstack *s);
void run_reduction(source *src, FILE *stats);

/* console */

void console(process *proc);

/* builtin */

int get_builtin(const char *name);
pntr string_to_array(process *proc, const char *str);
char *array_to_string(pntr refpntr);

/* master */

int master(const char *hostsfile, const char *myaddr, const char *filename, const char *cmd);

/* worker */

int worker(const char *hostsfile, const char *masteraddr);

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

void statistics(process *proc, FILE *f);
void dump_info(process *proc);
void dump_globals(process *proc);

/* interpreter */

void print_stack(FILE *f, pntr *stk, int size, int dir);
void add_pending_mark(process *proc, gaddr addr);
void spark(process *proc, frame *f);
void run(const char *bcdata, int bcsize, FILE *statsfile);

/* memory */

pntrstack *pntrstack_new(void);
void pntrstack_push(pntrstack *s, pntr p);
pntr pntrstack_at(pntrstack *s, int pos);
pntr pntrstack_pop(pntrstack *s);
void pntrstack_free(pntrstack *s);

void pntrstack_grow(int *alloc, pntr **data, int size);


#ifndef BUILTINS_C
extern const builtin builtin_info[NUM_BUILTINS];
#endif

#ifndef MEMORY_C
extern const char *cell_types[CELL_COUNT];
extern const char *msg_names[MSG_COUNT];
extern const char *frame_states[5];
#endif

#endif
