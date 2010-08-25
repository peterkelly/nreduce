/*
 * This file is part of the NReduce project
 * Copyright (C) 2006-2010 Peter Kelly <kellypmk@gmail.com>
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
 * $Id: util.c 870 2009-12-09 17:55:04Z pmkelly $
 *
 */

#define _MALLOC_C
#include "malloc.h"

#define _GNU_SOURCE /* for PTHREAD_ERRORCHECK_MUTEX_INITIALIZER_NP */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <unistd.h>
#include <string.h>
#include <stdlib.h>

void *try_malloc(size_t size)
{
  void *p = malloc(size);
  if (NULL == p) {
    char *str = "malloc() failed; aborting\n";
    write(STDERR_FILENO,str,strlen(str));
    exit(1);
  }
  return p;
}

void *try_calloc(size_t nmemb, size_t size)
{
  void *p = calloc(nmemb,size);
  if (NULL == p) {
    char *str = "calloc() failed; aborting\n";
    write(STDERR_FILENO,str,strlen(str));
    exit(1);
  }
  return p;
}

void *try_realloc(void *ptr, size_t size)
{
  void *p = realloc(ptr,size);
  if (NULL == p) {
    char *str = "realloc() failed; aborting\n";
    write(STDERR_FILENO,str,strlen(str));
    exit(1);
  }
  return p;
}
