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
 * $Id: nreduce.h 613 2007-08-23 11:40:12Z pmkelly $
 *
 */

#ifndef _NREDUCE_H
#define _NREDUCE_H

#include <stdio.h>
#include <stdarg.h>
#include <sys/time.h>
#include <time.h>
#include "compiler/util.h"

#define ERRMSG_MAX 256
#define BLOCK_SIZE 102400
#define COLLECT_THRESHOLD 8192000
#define GLOBAL_HASH_SIZE 256

//// the flags are used for garbage collection
#define FLAG_MARKED         0x1
#define FLAG_NEW            0x2
#define FLAG_PINNED         0x8		//// cells with this flag would not be collected, like global nil pntr

#endif /* _NREDUCE_H */
