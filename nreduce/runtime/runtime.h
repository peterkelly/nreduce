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
#include <sys/time.h>
#include <netdb.h>
#include <pthread.h>

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
  int indsource;
  char *msg;
} cell;

#define PNTR_VALUE 0xFFF80000
//#define MAX_ARRAY_SIZE (1 << 19)
#define MAX_ARRAY_SIZE 65536
#define READBUFSIZE 16384

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

typedef struct taskid {
  struct in_addr nodeip;
  short nodeport;
  short localid;
} taskid;

typedef struct msgheader {
  int sourceindex;
  int destlocalid;
  int rsize;
  int rtag;
} msgheader;

typedef struct message {
  char *rawdata;
  int rawsize;
  msgheader hdr;
  struct message *next;
  struct message *prev;
} message;

typedef struct messagelist {
  message *first;
  message *last; 
  pthread_mutex_t lock;
  pthread_cond_t cond;
} messagelist;

/* FIXME: remove */
typedef struct group {
  struct task **procs;
  int nprocs;
} group;

typedef struct waitqueue {
  list *frames;
  list *fetchers;
} waitqueue;

typedef void (*builtin_f)(struct task *tsk, pntr *argstack);

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
  pntr *data;

  int state;

  struct frame *freelnk;
  struct frame *next;
  struct frame *prev;

  struct frame *waitframe;
  struct global *waitglo;
} frame;

typedef struct frameq {
  frame *first;
  frame *last;
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
  int sparks;
  int sparksused;
  int fetches;
} procstats;

typedef struct gaddr {
  int pid;
  int lid;
} gaddr;

/* For "entry items" (globally visible objects that this task owns):
     tsk = this task id
     id = (tsk)-unique id for the object
     p = pointer to the actual object

  For "exit items" (globally visible objects on another task that we have a ref to):
     tsk = the task (not us) that owns the object
     id = (tsk)-unique id for the object      -- may be -1
     p = pointer to a REMOTE_REF object

  For "replicas" (local copies we've made of objects that other tasks own):
     tsk = the task (not us) that owns the object
     id = (tsk)-unique id for the object
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

/* task */

typedef struct block {
  struct block *next;
  cell values[BLOCK_SIZE];
} block;

struct task;
typedef void (*send_fun)(struct task *tsk, int dest, int tag, char *data, int size);
typedef int (*recv_fun)(struct task *tsk, int *tag, char **data, int *size, int block,
                        int delayms);

typedef struct task {
  int memdebug;

  /* bytecode */
  char *bcdata;
  int bcsize;

  /* communication */
  group *grp;
  int pid;
  int groupsize;
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
  sourceloc errorsl;
  frame **curfptr;
  int newfish;

  gaddr **infaddrs;
  int *infcount;
  int *infalloc;

  /* Each element of inflight_addrs is an array containing the addresses that have been sent
     to a particular task but not yet acknowledged. */
  array **inflight_addrs;

  /* Each element of unack_msg_acount is an array containing the number of addresses in each
     outstanding message to a particular task. An outstanding message is a message containing
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

  void *commdata;
  taskid *idmap;
  int localid;

  struct task *prev;
  struct task *next;

  messagelist mailbox;
  int checkmsg;
  pthread_t thread;
} task;

typedef struct tasklist {
  task *first;
  task *last;
} tasklist;

task *task_new(int pid, int groupsize, const char *bcdata, int bcsize);
void task_free(task *tsk);
void task_add_message(task *tsk, message *msg);
message *task_next_message(task *tsk, int delayms);

global *pntrhash_lookup(task *tsk, pntr p);
global *addrhash_lookup(task *tsk, gaddr addr);
void pntrhash_add(task *tsk, global *glo);
void addrhash_add(task *tsk, global *glo);
void pntrhash_remove(task *tsk, global *glo);
void addrhash_remove(task *tsk, global *glo);

global *add_global(task *tsk, gaddr addr, pntr p);
pntr global_lookup_existing(task *tsk, gaddr addr);
pntr global_lookup(task *tsk, gaddr addr, pntr val);
global *make_global(task *tsk, pntr p);
gaddr global_addressof(task *tsk, pntr p);

void add_gaddr(list **l, gaddr addr);
void remove_gaddr(task *tsk, list **l, gaddr addr);

void add_frame_queue(frameq *q, frame *f);
void add_frame_queue_end(frameq *q, frame *f);
void remove_frame_queue(frameq *q, frame *f);
void transfer_waiters(waitqueue *from, waitqueue *to);

void spark_frame(task *tsk, frame *f);
void unspark_frame(task *tsk, frame *f);
void run_frame(task *tsk, frame *f);
void block_frame(task *tsk, frame *f);
void unblock_frame(task *tsk, frame *f);
void done_frame(task *tsk, frame *f);

void set_error(task *tsk, const char *format, ...);

/* data */

#define MSG_DONE         0
#define MSG_FISH         1
#define MSG_FETCH        2
#define MSG_TRANSFER     3
#define MSG_ACK          4
#define MSG_MARKROOTS    5
#define MSG_MARKENTRY    6
#define MSG_SWEEP        7
#define MSG_SWEEPACK     8
#define MSG_UPDATE       9
#define MSG_RESPOND      10
#define MSG_SCHEDULE     11
#define MSG_UPDATEREF    12
#define MSG_STARTDISTGC  13
#define MSG_COUNT        14

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
int read_gaddr(reader *rd, task *tsk, gaddr *a);
int read_pntr(reader *rd, task *tsk, pntr *pout, int observe);
int read_vformat(reader *rd, task *tsk, int observe, const char *fmt, va_list ap);
int read_format(reader *rd, task *tsk, int observe, const char *fmt, ...);
int read_end(reader *rd);
int print_data(task *tsk, const char *data, int size);

array *write_start(void);
void write_tag(array *wr, int tag);
void write_char(array *wr, char c);
void write_int(array *wr, int i);
void write_double(array *wr, double d);
void write_string(array *wr, char *s);
void write_gaddr_noack(array *wr, gaddr a);
void write_gaddr(array *wr, task *tsk, gaddr a);
void write_ref(array *arr, task *tsk, pntr p);
void write_pntr(array *arr, task *tsk, pntr p);
void write_vformat(array *wr, task *tsk, const char *fmt, va_list ap);
void write_format(array *wr, task *tsk, const char *fmt, ...);
void write_end(array *wr);

void msg_send(task *tsk, int dest, int tag, char *data, int size);
void msg_fsend(task *tsk, int dest, int tag, const char *fmt, ...);
int msg_recv(task *tsk, int *tag, char **data, int *size, int delayms);

void msg_print(task *tsk, int dest, int tag, const char *data, int size);

/* reduction */

void reduce(task *h, pntrstack *s);
void run_reduction(source *src, FILE *stats);

/* console */

void console(task *tsk);

/* builtin */

void printp(FILE *f, pntr p);
int get_builtin(const char *name);
pntr string_to_array(task *tsk, const char *str);
char *array_to_string(pntr refpntr);

/* master */

int master(const char *hostsfile, const char *myaddr, const char *filename, const char *cmd);

/* worker */

int worker(const char *hostsfile, const char *masteraddr);

/* cell */

cell *alloc_cell(task *tsk);
void free_cell_fields(task *tsk, cell *v);

int count_alive(task *tsk);
void clear_marks(task *tsk, short bit);
void mark_roots(task *tsk, short bit);
void print_cells(task *tsk);
void sweep(task *tsk);
void mark_global(task *tsk, global *glo, short bit);
void local_collect(task *tsk);

frame *frame_alloc(task *tsk);
void frame_dealloc(task *tsk, frame *f);

cap *cap_alloc(int arity, int address, int fno);
void cap_dealloc(cap *c);

void print_pntr(FILE *f, pntr p);

void statistics(task *tsk, FILE *f);
void dump_info(task *tsk);
void dump_globals(task *tsk);

/* interpreter */

void print_stack(FILE *f, pntr *stk, int size, int dir);
void add_pending_mark(task *tsk, gaddr addr);
void spark(task *tsk, frame *f);
void *execute(task *tsk);
void run(const char *bcdata, int bcsize, FILE *statsfile, int *usage);

/* memory */

cell *pntrcell(pntr p);
global *pntrglobal(pntr p);
frame *pntrframe(pntr p);

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
