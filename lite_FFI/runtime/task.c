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
 * $Id: task.c 613 2007-08-23 11:40:12Z pmkelly $
 *
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#define TASK_C

#include "src/nreduce.h"
#include "compiler/source.h"
#include "runtime/runtime.h"
#include "modules/modules.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <assert.h>
#include <ctype.h>
#include <stdarg.h>
#include <math.h>
#include <errno.h>
#include <unistd.h>

extern const module_info *modules;

int set_error(task *tsk, const char *format, ...)
{
  va_list ap;
  int len;
  va_start(ap,format);
  len = vsnprintf(NULL,0,format,ap);
  va_end(ap);

  assert(NULL == tsk->error);
  tsk->error = (char*)malloc(len+1);
  va_start(ap,format);
  len = vsnprintf(tsk->error,len+1,format,ap);
  va_end(ap);

  return 0;
}

task *task_new(int tid, int groupsize, const char *bcdata, int bcsize)
{
  task *tsk = (task*)calloc(1,sizeof(task));
  cell *globnilvalue;

  tsk->freeptr = (cell*)1;

  globnilvalue = alloc_cell(tsk);
  globnilvalue->type = CELL_NIL;
  globnilvalue->flags |= FLAG_PINNED;

  make_pntr(tsk->globnilpntr,globnilvalue);
  set_pntrdouble(tsk->globtruepntr,1.0);

  if (is_pntr(tsk->globtruepntr))
    get_pntr(tsk->globtruepntr)->flags |= FLAG_PINNED;

  if (NULL == bcdata)
    return tsk; /* no bytecode; we must be using the reduction engine */

  return tsk;
}

void task_free(task *tsk)
{
  block *bl;

  sweep(tsk,1);

  bl = tsk->blocks;
  while (bl) {
    block *next = bl->next;
    free(bl);
    bl = next;
  }

  free(tsk->error);

  free(tsk);
}
