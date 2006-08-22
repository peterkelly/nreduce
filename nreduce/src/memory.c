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

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#define MEMORY_C

#include "grammar.tab.h"
#include "nreduce.h"
#include "gcode.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <assert.h>
#include <ctype.h>
#include <stdarg.h>
#include <math.h>

int trace = 0;

array *lexstring = NULL;
array *oldnames = NULL;
array *parsedfiles = NULL;

int maxstack = 0;
int nallocs = 0;
int totalallocs = 0;
int nscombappls = 0;
int nreductions = 0;
int nframes = 0;
int maxdepth = 0;
int maxframes = 0;
int ncollections = 0;

snode *pinned = NULL;
pntr globnilpntr = 0.0;
pntr globtruepntr = 0.0;
pntr globzeropntr = 0.0;

typedef struct block {
  struct block *next;
  cell values[BLOCK_SIZE];
} block;

static int framecount = 0;
static int active_framecount = 0;
static frame *freeframe = NULL;

frame *active_frames = NULL;
pntrstack *streamstack = NULL;

block *blocks = NULL;
cell *freeptr = NULL;

#ifndef INLINE_PTRFUNS
int is_pntr(pntr p)
{
  return (*(((unsigned int*)&p)+1) == 0xFFFFFFF1);
}

cell *get_pntr(pntr p)
{
  assert(is_pntr(p));
  return (cell*)(*((unsigned int*)&p));
}

int is_string(pntr p)
{
  return (*(((unsigned int*)&p)+1) == 0xFFFFFFF2);
}

char *get_string(pntr p)
{
  assert(is_string(p));
  return (char*)(*((unsigned int*)&p));
}

int pntrequal(pntr a, pntr b)
{
  return 
    ((*(((unsigned int*)&(a))+0) == *(((unsigned int*)&(b))+0)) &&
     (*(((unsigned int*)&(a))+1) == *(((unsigned int*)&(b))+1)));
}

int is_nullpntr(pntr p)
{
  return (is_pntr(p) && (NULL == get_pntr(p)));
}

#endif

#ifndef INLINE_RESOLVE_PNTR
pntr resolve_pntr(pntr p)
{
  while (TYPE_IND == pntrtype(p))
    p = get_pntr(p)->field1;
  return p;
}
#endif

#ifndef UNBOXED_NUMBERS
pntr make_number(double d)
{
  cell *v = alloc_cell();
  pntr p;
  v->tag = TYPE_NUMBER;
  v->field1 = d;
  make_pntr(p,v);
  return p;
}
#endif

void initmem()
{
  cell *globnilvalue;

  memset(&op_usage,0,OP_COUNT*sizeof(int));

  globnilvalue = alloc_cell();
  globnilvalue->tag = TYPE_NIL | FLAG_PINNED;

  make_pntr(globnilpntr,globnilvalue);
  globtruepntr = make_number(1.0);
  globzeropntr = make_number(0.0);

  if (is_pntr(globtruepntr))
    get_pntr(globtruepntr)->tag |= FLAG_PINNED;
  if (is_pntr(globzeropntr))
    get_pntr(globzeropntr)->tag |= FLAG_PINNED;
}

void mark(pntr p);

void mark_frame(frame *f)
{
/*   printf("mark_frame %p (cell %p)\n",f,f->c); */
  int i;
  if (f->completed) {
    assert(!"completed frame referenced from here");
  }
  if (f->c) {
    pntr p;
    make_pntr(p,f->c);
    mark(p);
  };
  for (i = 0; i < f->count; i++)
    mark(f->data[i]);
}

void mark_cap(cap *c)
{
  int i;
  for (i = 0; i < c->count; i++)
    mark(c->data[i]);
}

void mark(pntr p)
{
  cell *c;
  assert(TYPE_EMPTY != pntrtype(p));
  if (!is_pntr(p))
    return;
  c = get_pntr(p);
  if (c->tag & FLAG_MARKED)
    return;
  c->tag |= FLAG_MARKED;
  switch (pntrtype(p)) {
  case TYPE_IND:
    mark(c->field1);
    break;
  case TYPE_APPLICATION:
  case TYPE_CONS:
    c->field1 = resolve_pntr(c->field1);
    c->field2 = resolve_pntr(c->field2);
    mark(c->field1);
    mark(c->field2);
    break;
  case TYPE_AREF: {
    cell *arrholder = get_pntr(c->field1);
    carray *arr = (carray*)get_pntr(arrholder->field1);
    int index = (int)get_pntr(c->field2);
    assert(index < arr->size);
    assert(get_pntr(arr->refs[index]) == c);

    mark(c->field1);
    break;
  }
  case TYPE_ARRAY: {
    carray *arr = (carray*)get_pntr(c->field1);
    int i;
    for (i = 0; i < arr->size; i++) {
      mark(arr->elements[i]);
      if (!is_nullpntr(arr->refs[i]))
        mark(arr->refs[i]);
    }
    mark(arr->tail);
    break;
  }
  case TYPE_FRAME:
    mark_frame((frame*)get_pntr(c->field1));
    break;
  case TYPE_CAP:
    mark_cap((cap*)get_pntr(c->field1));
    break;
  case TYPE_BUILTIN:
  case TYPE_SCREF:
  case TYPE_NIL:
  case TYPE_NUMBER:
  case TYPE_STRING:
  case TYPE_HOLE:
    break;
  default:
    assert(0);
    break;
  }
}

cell *alloc_cell()
{
  cell *v;
  if (NULL == freeptr) {
    block *bl = (block*)calloc(1,sizeof(block));
    int i;
    bl->next = blocks;
    blocks = bl;
    for (i = 0; i < BLOCK_SIZE-1; i++)
      make_pntr(bl->values[i].field1,&bl->values[i+1]);
    make_pntr(bl->values[i].field1,NULL);

    freeptr = &bl->values[0];
  }
  v = freeptr;
  freeptr = (cell*)get_pntr(freeptr->field1);
  nallocs++;
  totalallocs++;
  return v;
}

void free_cell_fields(cell *v)
{
  switch (celltype(v)) {
  case TYPE_STRING:
    free(get_string(v->field1));
    break;
  case TYPE_ARRAY: {
    carray *arr = (carray*)get_pntr(v->field1);
    free(arr->elements);
    free(arr->refs);
    free(arr);
    break;
  }
  case TYPE_FRAME: {
/*     printf("here at frame\n"); */
    /* note: we don't necessarily free frames here... this is only safe to do after the frame
       has finished executing */
    frame *f = (frame*)get_pntr(v->field1);
    f->c = NULL;
    if (!f->active)
      frame_dealloc(f);
    else printf("free_cell_fields: frame still active\n");

    break;
  }
  case TYPE_CAP: {
    cap *cp = (cap*)get_pntr(v->field1);
    cap_dealloc(cp);
  }
  }
}

extern pntr *gstack;
extern int gstackcount;

void collect()
{
  block *bl;
  int i;
  frame *f;

  ncollections++;
  nallocs = 0;

  /* clear all marks */
  for (bl = blocks; bl; bl = bl->next)
    for (i = 0; i < BLOCK_SIZE; i++)
      bl->values[i].tag &= ~FLAG_MARKED;

  /* mark phase */

  if (streamstack)
    for (i = 0; i < streamstack->count; i++)
      mark(streamstack->data[i]);

  if (gstack)
    for (i = 0; i < gstackcount; i++)
      mark(gstack[i]);

  for (f = active_frames; f; f = f->next)
    mark_frame(f);

  /* sweep phase */
  for (bl = blocks; bl; bl = bl->next) {
    for (i = 0; i < BLOCK_SIZE; i++) {
      if (!(bl->values[i].tag & FLAG_MARKED) &&
          (TYPE_EMPTY != celltype(&bl->values[i]))) {

        
        if (!(bl->values[i].tag & FLAG_PINNED)) {
          free_cell_fields(&bl->values[i]);
          bl->values[i].tag = TYPE_EMPTY;
          make_pntr(bl->values[i].field1,freeptr);
          freeptr = &bl->values[i];
        }
      }
    }
  }
}

void add_active_frame(frame *f)
{
  assert(!f->active);
  assert(NULL == f->next);
  f->active = 1;
  f->next = active_frames;
  active_frames = f;
  active_framecount++;
}

void remove_active_frame(frame *f)
{
  frame **ptr;
  assert(f->active);
  f->active = 0;
  f->completed = 1;
  ptr = &active_frames;
  while (*ptr != f)
    ptr = &((*ptr)->next);
  *ptr = f->next;
  f->next = NULL;
  active_framecount--;

  assert(NULL == f->c);
  frame_dealloc(f);

#if 0
  if (NULL == f->c) {
    frame_dealloc(f);
  }
  else {
    printf("remove_active_frame %p (cell %p): still referenced\n",f,f->c);
    assert(TYPE_FRAME == celltype(f->c));
    assert(f == f->c->field1);
    assert(!(f->c->tag & FLAG_PINNED));
/*     f->c = NULL; */
/*     frame_dealloc(f); */
  }
#endif
}

frame *frame_new()
{
  frame *f = (frame*)calloc(1,sizeof(frame));
  f->fno = -1;
  nframes++;
  framecount++;
/*   printf("framecount = %d, cells = %d, active = %d\n",framecount,framecellcount,active_framecount); */
  if (framecount > maxframes)
    maxframes = framecount;
  return f;
}

void frame_free(frame *f)
{
  assert(NULL == f->c);
  assert(!f->active);
  if (f->data)
    free(f->data);
  free(f);
  framecount--;
}

frame *frame_alloc()
{
  if (freeframe) {
    frame *f = freeframe;
    freeframe = freeframe->freelnk;
    f->c = NULL;
    f->fno = -1;
    f->address = 0;
    f->d = NULL;

    f->alloc = 0;
    f->count = 0;
    f->data = NULL;

    f->next = NULL;
    f->active = 0;
    f->completed = 0;
    f->freelnk = NULL;
    return f;
  }
  else {
    return frame_new();
  }
}

void frame_dealloc(frame *f)
{
  if (f->data)
    free(f->data);
  f->data = NULL;
  f->freelnk = freeframe;
  freeframe = f;
/*   frame_free(f); */
}

int frame_depth(frame *f)
{
  int depth = 0;
  while (f) {
    depth++;
    f = f->d;
  }
  return depth;
}

cap *cap_alloc(int arity, int address, int fno)
{
  cap *c = (cap*)calloc(1,sizeof(cap));
  c->arity = arity;
  c->address = address;
  c->fno = fno;
  assert(0 < c->arity); /* MKCAP should not be called for CAFs */
  return c;
}

void cap_dealloc(cap *c)
{
  if (c->data)
    free(c->data);
  free(c);
}

void cleanup()
{
  int i;
  frame *f;
  block *bl;

  for (bl = blocks; bl; bl = bl->next)
    for (i = 0; i < BLOCK_SIZE; i++)
      bl->values[i].tag &= ~FLAG_PINNED;

  collect();

  f = freeframe;
  while (f) {
    frame *next = f->freelnk;
    f->c = NULL;
    frame_free(f);
    f = next;
  }

  bl = blocks;
  while (bl) {
    block *next = bl->next;
    free(bl);
    bl = next;
  }

  scomb_free_list(&scombs);

  free(funcalls);
  free(addressmap);
  free(noevaladdressmap);
  free(stacksizes);
  if (lexstring)
    array_free(lexstring);
  if (oldnames) {
    for (i = 0; i < oldnames->size/sizeof(char*); i++)
      free(((char**)oldnames->data)[i]);
    array_free(oldnames);
  }
  if (parsedfiles) {
    for (i = 0; i < parsedfiles->size/sizeof(char*); i++)
      free(((char**)parsedfiles->data)[i]);
    array_free(parsedfiles);
  }
}

pntrstack *pntrstack_new(void)
{
  pntrstack *s = (pntrstack*)calloc(1,sizeof(pntrstack));
  s->alloc = 1;
  s->count = 0;
  s->data = (pntr*)malloc(sizeof(pntr));
  return s;
}

void pntrstack_push(pntrstack *s, pntr p)
{
  if (s->count == s->alloc) {
    if (s->count >= STACK_LIMIT) {
      fprintf(stderr,"Out of stack space\n");
      exit(1);
    }
    pntrstack_grow(&s->alloc,&s->data,s->alloc*2);
  }
  s->data[s->count++] = p;
}

pntr pntrstack_at(pntrstack *s, int pos)
{
  assert(0 <= pos);
  assert(pos < s->count);
  return resolve_pntr(s->data[pos]);
}

pntr pntrstack_pop(pntrstack *s)
{
  assert(0 < s->count);
  return resolve_pntr(s->data[--s->count]);
}

void pntrstack_free(pntrstack *s)
{
  free(s->data);
  free(s);
}

void pntrstack_grow(int *alloc, pntr **data, int size)
{
  if (*alloc < size) {
    *alloc = size;
    *data = (pntr*)realloc(*data,(*alloc)*sizeof(pntr));
  }
}

void print_pntr(pntr p)
{
  printf("(value %s)",cell_types[pntrtype(p)]);
}

