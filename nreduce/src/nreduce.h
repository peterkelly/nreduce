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
#include "compiler/util.h"

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

/* #define DEBUG_GCODE_COMPILATION */
/* #define SHOW_SUBSTITUTED_NAMES */
/* #define EXECUTION_TRACE */
/* #define PROFILING */
/* #define QUEUE_CHECKS */
/* #define PAREXEC_DEBUG */
/* #define MSG_DEBUG */
/* #define CONTINUOUS_DISTGC */
/* #define DISTGC_DEBUG */
/* #define SHOW_FRAME_COMPLETION */
/* #define COLLECTION_DEBUG */


// Misc

#define BLOCK_SIZE 1024
#define COLLECT_THRESHOLD 102400
//#define COLLECT_THRESHOLD 1024
#define COLLECT_INTERVAL 100000
//#define COLLECT_INTERVAL 1000
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

#define TAG_MASK         0xFF

#define FLAG_MARKED      0x100
#define FLAG_PROCESSED   0x200
#define FLAG_NEW         FLAG_PROCESSED
#define FLAG_TMP         0x400
#define FLAG_DMB         0x800
#define FLAG_PINNED     0x1000
#define FLAG_STRICT     0x2000
#define FLAG_NEEDCLEAR  0x4000

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

#define checkcell(_c) ({ if (TYPE_EMPTY == (_c)->type) \
                          fatal("access to free'd cell %p",(_c)); \
                        (_c); })

#define snodetype(_c) ((_c)->tag & TAG_MASK)
//#define celltype(_c) ((_c)->type)
#define celltype(_c) (checkcell(_c)->type)
#define pntrtype(p) (is_pntr(p) ? celltype(get_pntr(p)) : TYPE_NUMBER)

typedef double pntr;

typedef struct cell {
  short flags;
  short type;
  pntr field1;
  pntr field2;
} cell;

#define PNTR_VALUE 0xFFF80000
//#define PNTR_VALUE 0xFFF00000
//#define MAX_ARRAY_SIZE (1 << 19)
//#define MAX_ARRAY_SIZE 30
#define MAX_ARRAY_SIZE 2000
//#define MAX_ARRAY_SIZE 20




//#define ARRAY_DEBUG
//#define PRINT_DEBUG

//#define ARRAY_DEBUG2






#define pfield1(__p) (get_pntr(__p)->field1)
#define pfield2(__p) (get_pntr(__p)->field2)
#define ppfield1(__p) (get_pntr(pfield1(__p)))
#define ppfield2(__p) (get_pntr(pfield2(__p)))
#define pglobal(__p) ((global*)ppfield1(__p))
#define pframe(__p) ((frame*)ppfield1(__p))

#ifdef INLINE_PTRFUNS
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
#else
int is_pntr(pntr p);
cell *get_pntr(pntr p);

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

#define check_global(_g) (assert(!(_g)->freed))

#ifndef DEBUG_C
extern const char *snode_types[NUM_CELLTYPES];
extern const char *cell_types[NUM_CELLTYPES];
extern const char *frame_states[5];
#endif

#ifndef MEMORY_C
extern int trace;
#endif

#endif /* _NREDUCE_H */
