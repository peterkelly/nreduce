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
 * $Id: runtime.h 613 2007-08-23 11:40:12Z pmkelly $
 *
 */

#ifndef _RUNTIME_H
#define _RUNTIME_H

#include "src/nreduce.h"
#include "compiler/source.h"
#include <stdio.h>
#include <sys/time.h>
#include <netdb.h>
#include <pthread.h>
#include <assert.h>
#include <setjmp.h>
#include <signal.h>

struct task;

//// B indicates Build-in functions, these numbers are used as snode->bif

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

#define B_BITSHL         11
#define B_BITSHR         12
#define B_BITAND         13
#define B_BITOR          14
#define B_BITXOR         15
#define B_BITNOT         16

#define B_SQRT           17
#define B_FLOOR          18
#define B_CEIL           19
#define B_NUMTOSTRING    20
#define B_STRINGTONUM1   21

#define B_IF             22
#define B_AND            23
#define B_OR             24
#define B_NOT            25

#define B_CONS           26
#define B_HEAD           27
#define B_TAIL           28

#define B_SEQ            29
#define B_ERROR1         30
#define B_ABS            31
#define B_ISCONS         32

#define B_RANDOMNUM      33

//#define B_ZZIP_DIR_REAL  34
//#define B_ZZIP_VERSION   35
//#define B_ZZIP_READ_DIRENT 36
//#define B_ZZIP_READ		 37

//// number of total buildin functions
#define NUM_BUILTINS     34

//// Check if the cell _c is empty or not
#define checkcell(_c) ({ if (CELL_EMPTY == (_c)->type) \
                          fatal("access to free'd cell %p",(_c)); \
                        (_c); })

//// Check the vadility of the cell and then return the cell type
#define celltype(_c) (checkcell(_c)->type)

//// return the cell type of p, or return CELL_NUMBER, if the cell is not a pointer(pntr)
//// pntr type == cell type, since pntr always points to cells
#define pntrtype(p) (is_pntr(p) ? celltype(get_pntr(p)) : CELL_NUMBER)

//// unsigned int = 4 bytes => a pointer hold 4 bytes
//// data[1] is used to indicate if it's a pntr or number(CELL_NUMBER)
typedef struct {
  unsigned int data[2];
} pntr;

#define CELL_EMPTY       0x00  //// empty cell
#define CELL_APPLICATION 0x01  /* left: function (cell*)   right: argument (cell*) */
#define CELL_BUILTIN     0x02  /* left: bif (int)                                  */
#define CELL_CONS        0x03  /* left: head (cell*)       right: tail (cell*)     */
#define CELL_IND         0x04  /* left: tgt (cell*) //// IND means INDirection, 
							cells are set to this type after they are instantiated */
#define CELL_SCREF       0x05  /* left: scomb (scomb*)                             */
							   //// this kind of cells store a pntr, pointing to the SuperCombinator
							   //// so it is called supercombinator REFerence
#define CELL_HOLE        0x06  /*                                                  */
#define CELL_NIL         0x07  /*  NULL CELL                                       */
#define CELL_NUMBER      0x08  //// cell containing a number, rather than pntr
#define CELL_COUNT       0x09
#define CELL_EXTFUNC	 0x0A  //// extension function

typedef struct cell {
  int type;		//// the type of cell, see the above cell types, like CELL_APPLICATION
  int flags;	//// used in garbage collection
  pntr field1;	
  pntr field2;
} cell;


#define PNTR_MASK  0xFFF80000   //// == (binary) 11111111111110000000000000000000
#define INDEX_MASK 0x0003FFFF   //// == (binary)               111111111111111111
#define PNTR_VALUE 0xFFF00000	//// == (binary) 11111111111100000000000000000000
								//// used to indicate the specific pntr is a valid pntr structure (see is_pntr() macro)

#define NULL_PNTR (*(pntr*)NULL_PNTR_BITS)
#define MAX_ARRAY_SIZE (1 << 18)

#define pfield1(__p) (get_pntr(__p)->field1)
#define pfield2(__p) (get_pntr(__p)->field2)
#define ppfield1(__p) (get_pntr(pfield1(__p)))
#define ppfield2(__p) (get_pntr(pfield2(__p)))
#define pntrdouble(__p) (*(double*)&(__p))	//// turn a pntr (__p) into double type, that eliminate p->data[1]

//// store a double value in the pntr (8 bytes). 
//// firstly: get the pntr address: &(__p)
//// secondly: turn the data type to double (double *), a double requires 8 bytes as well
//// at last: set the value(__val) to *p
//// Once a pntr is used to store a double number, the is_pntr() test will return false to indicate that it's not a pntr
#define set_pntrdouble(__p,__val) ({ (*(double*)&(__p)) = (__val); })
#define is_pntr(__p) (((__p).data[1] & PNTR_MASK) == PNTR_VALUE)
//// make pointer __P, the first element of __p is the same as __c
#define make_pntr(__p,__c) { (__p).data[0] = (unsigned int)(__c); \
                             (__p).data[1] = PNTR_VALUE; }

//// get the CELL pointer which is pointed by the pntr __p (in its data[0])
//// firstly: get the pntr address: &(__p)
//// secondly: turn the __p type into unsigned int, in this way, the data[1] of __p would be abandoned.
//// finally: fetch the content of __p->data[0] by (*(&....p))
#define get_pntr(__p) (assert(is_pntr(__p)), ((cell*)(*((unsigned int*)&(__p)))))
#define pntrequal(__a,__b) (((__a).data[0] == (__b).data[0]) && ((__a).data[1] == (__b).data[1]))

#define is_nullpntr(__p) (is_pntr(__p) && ((cell*)1 == get_pntr(__p)))

//// retrieve the correct pointer (pntr) from x, once the cell has been instantiated, return its body
#define resolve_pntr(x) ({ pntr __x = (x);        \
                           while (CELL_IND == pntrtype(__x)) \
                             __x = get_pntr(__x)->field1; \
                           __x; })

//// buildin functions pointer
typedef void (*builtin_f)(struct task *tsk, pntr *argstack);

typedef struct builtin {
  char *name;	//// the name of the buildin function
  int nargs;	//// number of arguments needed for this buildin f
  int nstrict;	//// 
  builtin_f f;	//// function pointer
} builtin, extfunc;

typedef struct pntrstack {
  int alloc;  //// Number of all allocated stack space, including used and unused space
  int count;  //// Number of used stack space, "count" point to the top of the stack
  pntr *data; //// Stored pointers
  int limit;  //// The maximum space 
} pntrstack;

//// a block of cells, like a pool used to store lot of cells
typedef struct block {
  struct block *next;	//// next block
  int pad;
  cell values[BLOCK_SIZE];	//// cells in this block
} block;

//// there may be lot of tasks running simutaneously
typedef struct task {
  int done;
  char *error;				 //// NULL default, otherwise some error occured, and the err messages is stored here
  block *blocks;			 //// blocks of cells, a block can store BLOCK_SIZE cells
  cell *freeptr;			 //// the pointer points to the first available cell, which is in the block (see task_new())
  pntr globnilpntr;          //// global nil pointer, which points to a CELL_NIL cell
  pntr globtruepntr;         //// global true pointer (true)
  pntrstack *streamstack;    //// stack used to store application nodes to be processed
  pntrstack *markstack;
  int newcellflags;			 //// used to initialize the new cells' flag, which is used for garbage collection
  int inmark;
  int alloc_bytes;	         //// Memory has been used by this task
  int framesize;
  int framesperblock;
} task;

task *task_new(int tid, int groupsize, const char *bcdata, int bcsize);
void task_free(task *tsk);

int set_error(task *tsk, const char *format, ...);

/* reduction */

pntr instantiate_scomb(task *tsk, pntrstack *s, scomb *sc);
void reduce(task *h, pntrstack *s);
void run_reduction(source *src);

/* builtin */

void invalid_arg(task *tsk, pntr arg, int bif, int argno, int type);
void invalid_binary_args(task *tsk, pntr *argstack, int bif);

pntr data_to_list(task *tsk, const char *data, int size, pntr tail);

int get_builtin(const char *name);
pntr string_to_array(task *tsk, const char *str);
int array_to_string(pntr refpntr, char **str);

//// external functions 
int get_extfunc(const char *name);

/* cell */

cell *alloc_cell(task *tsk);
void clear_marks(task *tsk, short bit);
void mark_roots(task *tsk, short bit);
void sweep(task *tsk, int all);
void local_collect(task *tsk);

/* memory */

pntrstack *pntrstack_new(void);
void pntrstack_push(pntrstack *s, pntr p);
pntr pntrstack_at(pntrstack *s, int pos);
pntr pntrstack_pop(pntrstack *s);
pntr pntrstack_top(pntrstack *s);
void pntrstack_free(pntrstack *s);

void pntrstack_grow(int *alloc, pntr **data, int size);

char *cell_type(pntr cell_pntr);

#ifndef BUILTINS_C
extern const builtin builtin_info[NUM_BUILTINS];
#endif

#ifndef MEMORY_C
extern unsigned char NULL_PNTR_BITS[8];
extern const char *cell_types[CELL_COUNT];
#endif

#endif
