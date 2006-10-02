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

//#define NDEBUG

/* #define DEBUG_BYTECODE_COMPILATION */
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

/* #define ARRAY_DEBUG */
/* #define PRINT_DEBUG */
/* #define ARRAY_DEBUG2 */


// Misc

#define BLOCK_SIZE 1024
#define COLLECT_THRESHOLD 102400
//#define COLLECT_THRESHOLD 1024
#define COLLECT_INTERVAL 100000
//#define COLLECT_INTERVAL 1000
#define STACK_LIMIT 10240
#define GLOBAL_HASH_SIZE 256

#define FLAG_MARKED      0x100
#define FLAG_NEW         0x200
#define FLAG_TMP         0x400
#define FLAG_DMB         0x800
#define FLAG_PINNED     0x1000
#define FLAG_NEEDCLEAR  0x4000

#endif /* _NREDUCE_H */
