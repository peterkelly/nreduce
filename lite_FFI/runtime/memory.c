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
 * $Id: memory.c 613 2007-08-23 11:40:12Z pmkelly $
 *
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#define MEMORY_C

#include "src/nreduce.h"
#include "compiler/source.h"
#include "runtime/runtime.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <assert.h>
#include <ctype.h>
#include <stdarg.h>
#include <math.h>
#include <unistd.h>

unsigned char NULL_PNTR_BITS[8] = { 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0xF0, 0xFF };

const char *cell_types[CELL_COUNT] = {
  "EMPTY",
  "APPLICATION",
  "BUILTIN",
  "CONS",
  "IND",
  "SCREF",
  "HOLE",
  "NIL",
  "NUMBER",
};

static void mark(task *tsk, pntr p, short bit)
{
  cell *c;
  assert(CELL_EMPTY != pntrtype(p));

  if (tsk->markstack) {
    if (is_pntr(p) && !(get_pntr(p)->flags & bit))
      pntrstack_push(tsk->markstack,p);
    return;
  }

  tsk->markstack = pntrstack_new();
  tsk->markstack->limit = -1;
  pntrstack_push(tsk->markstack,p);

  while (0 < tsk->markstack->count) {
    int cont = 0;

    p = tsk->markstack->data[--tsk->markstack->count];

    /* handle CONS and specially - process the "spine" iteratively */
    while (CELL_CONS == pntrtype(p)) {
      c = get_pntr(p);
      if (c->flags & bit) {
        cont = 1;
        break;
      }
      c->flags |= bit;
      c->field1 = resolve_pntr(c->field1);
      c->field2 = resolve_pntr(c->field2);
      mark(tsk,c->field1,bit);
      p = c->field2;
    }

    if (cont)
      continue;

    /* handle other types */
    if (!is_pntr(p))
      continue;

    c = get_pntr(p);
    if (c->flags & bit)
      continue;

    c->flags |= bit;
    switch (pntrtype(p)) {
    case CELL_IND:
      mark(tsk,c->field1,bit);
      break;
    case CELL_APPLICATION:
      c->field1 = resolve_pntr(c->field1);
      c->field2 = resolve_pntr(c->field2);
      mark(tsk,c->field1,bit);
      mark(tsk,c->field2,bit);
      break;
    case CELL_BUILTIN:
    case CELL_SCREF:
    case CELL_NIL:
    case CELL_NUMBER:
    case CELL_HOLE:
    case CELL_EXTFUNC:
      break;
    default:
      fatal("Invalid pntr type %d",pntrtype(p));
      break;
    }
  }

  assert(tsk->markstack);
  pntrstack_free(tsk->markstack);
  tsk->markstack = NULL;
}

//// allocate an free cell from the task (tsk), if there is no free cell in the tsk, create new block containing cells.
cell *alloc_cell(task *tsk)
{
  cell *v;
  assert(tsk);
  //// if (tsk->freeprt == 0x0000001) is true, then the task dosen't have free cells, so creat a new block
  if (tsk->freeptr == (cell*)1) { /* 64 bit pntrs use 1 in second byte for null */
    block *bl = (block*)calloc(1,sizeof(block));	//// create a new block of cells
    int i;
    bl->next = tsk->blocks;	//// link the new block to the head of existing block
    tsk->blocks = bl;	//// replace the original blocks in the task (tsk)
    for (i = 0; i < BLOCK_SIZE-1; i++)
      //// here, make_pntr is an macro, because the BLOCK_SIZE is large, it would be much slow if using function call.
      make_pntr(bl->values[i].field1,&bl->values[i+1]);	//// initialize all the cells in the block
      													//// initially, CELL(n)(field1) points to CELL(n+1)
    bl->values[i].field1 = NULL_PNTR;	//// the last cell in the block should points to NULL

    tsk->freeptr = &bl->values[0];	//// the first free pointer is initialized
  }
  v = tsk->freeptr;
  v->flags = tsk->newcellflags;
  tsk->freeptr = (cell*)get_pntr(tsk->freeptr->field1);	//// redirect free pntr to next cell, since current free cell will be used
  tsk->alloc_bytes += sizeof(cell);	//// adjust the used space for this task
  return v;
}

//// clear marks for all the cells in all the blocks in the task (tsk)
void clear_marks(task *tsk, short bit)
{
  block *bl;
  int i;

	//// traverse all the blocks in the task
  for (bl = tsk->blocks; bl; bl = bl->next)
  	//// traverse all the cells in the block
    for (i = 0; i < BLOCK_SIZE; i++)
      bl->values[i].flags &= ~bit;
}

void mark_roots(task *tsk, short bit)
{
  int i;

  if (tsk->streamstack) {
    for (i = 0; i < tsk->streamstack->count; i++)
      mark(tsk,tsk->streamstack->data[i],bit);
  }
}

void sweep(task *tsk, int all)
{
  block *bl;
  int i;

  for (bl = tsk->blocks; bl; bl = bl->next) {
    for (i = 0; i < BLOCK_SIZE; i++) {
      if (!(bl->values[i].flags & FLAG_MARKED) &&
          !(bl->values[i].flags & FLAG_PINNED) &&
          (CELL_EMPTY != bl->values[i].type)) {
        bl->values[i].type = CELL_EMPTY;

        /* comment out these two lines to avoid reallocating cells, to check invalid accesses */
        make_pntr(bl->values[i].field1,tsk->freeptr);
        tsk->freeptr = &bl->values[i];
      }
    }
  }
}

////
void local_collect(task *tsk)
{
  tsk->alloc_bytes = 0;

  /* clear */
  clear_marks(tsk,FLAG_MARKED);

  /* mark */
  mark_roots(tsk,FLAG_MARKED);

  /* sweep */
  sweep(tsk,0);
}

//// creat a new pointer stack, Return the pointer to the pntrstack
pntrstack *pntrstack_new(void)
{
  pntrstack *s = (pntrstack*)calloc(1,sizeof(pntrstack));
  s->alloc = 1;   ////Initially, 1 pointer is allowed
  s->count = 0;   
  s->data = (pntr*)malloc(sizeof(pntr));	//// allocate space for the pointer (s->alloc)
  s->limit = STACK_LIMIT;
  return s;
}

//// push pointer p into the top of pointer stack s
void pntrstack_push(pntrstack *s, pntr p)
{
	////Test to see if the stack s is full, allocate 
  if (s->count == s->alloc) {
    if ((s->limit >= 0) && (s->count >= s->limit)) {
      fprintf(stderr,"Out of stack space\n");
      exit(1);
    }
    ////Double the pointer stack s
    pntrstack_grow(&s->alloc,&s->data,s->alloc*2);
  }
  s->data[s->count++] = p;
}

////Return the pointer at (pos), which starts from 0
pntr pntrstack_at(pntrstack *s, int pos)
{
  assert( pos >= 0);
  assert( s->count > pos);
  return resolve_pntr(s->data[pos]);
}

pntr pntrstack_pop(pntrstack *s)
{
  assert(0 < s->count);
  return resolve_pntr(s->data[--s->count]);
}

pntr pntrstack_top(pntrstack *s)
{
  assert(0 < s->count);
  return resolve_pntr(s->data[s->count]);
}

//// free the pntr stack (s)
void pntrstack_free(pntrstack *s)
{
  free(s->data);
  free(s);
}

//// grow the pointer stack space to the specified (size)
void pntrstack_grow(int *alloc, pntr **data, int size)
{
	//// make sure the requested (size) is larger than original size
  if (*alloc < size) {
    *alloc = size;
    *data = (pntr*)realloc(*data,(*alloc)*sizeof(pntr));	//// remove the original (data) to the new stack space
  }
  
}
