/*
 * This file is part of the nreduce project
 * Copyright (C) 2006-2009 Peter Kelly (pmk@cs.adelaide.edu.au)
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

#define _XOPEN_SOURCE /* for ucontext routines on snow leopard */

#include <stdio.h>
#include <stdarg.h>
#include <sys/time.h>
#include <time.h>
#include "network/util.h"

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
/* #define PROFILING */
/* #define CONTINUOUS_DISTGC */
/* #define DISABLE_ARRAYS */

// Misc

#define MB 1048576
#define BLOCK_BYTES (32*MB-1024)
#define FRAMEBLOCK_SIZE 1048576
#define COLLECT_THRESHOLD 8192000
#define GLOBAL_HASH_SIZE 4096
#define PROFILE_FILENAME "profile.out"
#define MAX_LOCAL_CONNECTIONS 3
#define MAX_TOTAL_CONNECTIONS 128
/* #define DISABLE_SPARKS */

#define FLAG_MARKED         0x1
#define FLAG_NEW            0x2
#define FLAG_DMB            0x4
#define FLAG_REDUCED       0x10
#define FLAG_MATURE        0x20
#define FLAG_INRSET        0x40
#define FLAG_ALTSPACE      0x80

#endif /* _NREDUCE_H */
