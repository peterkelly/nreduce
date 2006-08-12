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

static int framecount = 0;
static int active_framecount = 0;
static int framecellcount = 0;
static frame *freeframe = NULL;

extern array *lexstring;
extern array *oldnames;

frame *active_frames = NULL;
cell *freeptr = NULL;
stack *streamstack = NULL;

block *blocks = NULL;
int nblocks = 0;

int maxstack = 0;
int ncells = 0;
int maxcells = 0;
int nallocs = 0;
int totalallocs = 0;
int nscombappls = 0;
int nresindnoch = 0;
int nresindch = 0;
int nreductions = 0;
int nframes = 0;
int maxdepth = 0;
int maxframes = 0;

cell *pinned = NULL;
cell *globnil = NULL;
cell *globtrue = NULL;
cell *globzero = NULL;

void initmem()
{
  memset(&op_usage,0,OP_COUNT*sizeof(int));
  globnil = alloc_cell();
  globnil->tag = TYPE_NIL | FLAG_PINNED;
  globtrue = alloc_cell();
  globtrue->tag = TYPE_INT | FLAG_PINNED;
  globtrue->field1 = (void*)1;
  globzero = alloc_cell();
  globzero->tag = TYPE_INT | FLAG_PINNED;
  globzero->field1 = (void*)0;
}

cell *alloc_cell()
{
  cell *c;
  if (NULL == freeptr) {
    block *bl = (block*)calloc(1,sizeof(block));
    int i;
    bl->next = blocks;
    blocks = bl;
    for (i = 0; i < BLOCK_SIZE-1; i++)
      bl->cells[i].field1 = &bl->cells[i+1];

    freeptr = &bl->cells[0];
    nblocks++;
  }
  c = freeptr;
  freeptr = (cell*)freeptr->field1;
  ncells++;
  if (maxcells < ncells)
    maxcells = ncells;
  nallocs++;
  totalallocs++;
  return c;
}

cell *alloc_cell2(int tag, void *field1, void *field2)
{
  cell *c = alloc_cell();
  c->tag = tag;
  c->field1 = field1;
  c->field2 = field2;
  if (TYPE_FRAME == tag)
    framecellcount++;
  return c;
}

cell *alloc_sourcecell(const char *filename, int lineno)
{
  return alloc_cell();
}

void mark(cell *c);
void mark_frame(frame *f)
{
/*   printf("mark_frame %p (cell %p)\n",f,f->c); */
  int i;
  if (f->completed) {
    assert(!"completed frame referenced from here");
  }
  if (f->c)
    mark(f->c);
  for (i = 0; i < f->count; i++)
    mark(f->data[i]);
}

void mark_cap(cap *c)
{
  int i;
  for (i = 0; i < c->count; i++)
    mark(c->data[i]);
}

int nreachable = 0;
void mark(cell *c)
{
  assert(TYPE_EMPTY != celltype(c));
  if (c->tag & FLAG_MARKED)
    return;
  c->tag |= FLAG_MARKED;
  nreachable++;
  switch (celltype(c)) {
  case TYPE_IND:
    mark((cell*)c->field1);
    break;
  case TYPE_APPLICATION:
  case TYPE_CONS:
    c->field1 = resolve_ind((cell*)c->field1);
    c->field2 = resolve_ind((cell*)c->field2);
    mark((cell*)c->field1);
    mark((cell*)c->field2);
    break;
  case TYPE_LAMBDA:
    mark((cell*)c->field2);
    break;
  case TYPE_LETREC: {
    letrec *rec;
    for (rec = (letrec*)c->field1; rec; rec = rec->next)
      mark(rec->value);
    mark((cell*)c->field2);
    break;
  }
  case TYPE_AREF: {
    cell *arrcell = (cell*)c->field1;
    carray *arr = (carray*)arrcell->field1;
    int index = (int)c->field2;
    assert(index < arr->size);
    assert(arr->refs[index] == c);

    mark((cell*)c->field1);
    break;
  }
  case TYPE_ARRAY: {
    carray *arr = (carray*)c->field1;
    int i;
    for (i = 0; i < arr->size; i++) {
      mark(arr->cells[i]);
      if (arr->refs[i])
        mark(arr->refs[i]);
    }
    mark(arr->tail);
    if (arr->sizecell)
      mark(arr->sizecell);
    break;
  }
  case TYPE_FRAME:
    mark_frame((frame*)c->field1);
    break;
  case TYPE_CAP:
    mark_cap((cap*)c->field1);
    break;
  default:
    break;
  }
}

void free_letrec(letrec *rec)
{
  free(rec->name);
  free(rec);
}

void free_cell_fields(cell *c)
{
  switch (celltype(c)) {
  case TYPE_STRING:
  case TYPE_SYMBOL:
  case TYPE_LAMBDA:
    free((char*)c->field1);
    break;
  case TYPE_LETREC: {
    letrec *rec = (letrec*)c->field1;
    while (rec) {
      letrec *next = rec->next;
      free_letrec(rec);
      rec = next;
    }
    break;
  }
  case TYPE_ARRAY: {
    carray *arr = (carray*)c->field1;
    free(arr->cells);
    free(arr->refs);
    free(arr);
    break;
  }
  case TYPE_FRAME: {
/*     printf("here at frame\n"); */
    /* note: we don't necessarily free frames here... this is only safe to do after the frame
       has finished executing */
    frame *f = (frame*)c->field1;
    f->c = NULL;
    if (!f->active)
      frame_dealloc(f);
    else printf("free_cell_fields: frame still active\n");

    framecellcount--;
    break;
  }
  case TYPE_CAP: {
    cap *cp = (cap*)c->field1;
    cap_dealloc(cp);
  }
  }
}

char *collect_ebp = NULL;
char *collect_esp = NULL;

extern cell **gstack;
extern int gstackcount;

int ncollections = 0;
void collect()
{
  block *bl;
  int i;
  scomb *sc;

  ncollections++;
  nallocs = 0;

  /* clear all marks */
  for (bl = blocks; bl; bl = bl->next)
    for (i = 0; i < BLOCK_SIZE; i++)
      bl->cells[i].tag &= ~FLAG_MARKED;
  nreachable = 0;

  /* mark phase */

  if (streamstack)
    for (i = 0; i < streamstack->count; i++)
      mark((cell*)streamstack->data[i]);

  if (gstack)
    for (i = 0; i < gstackcount; i++)
      mark(gstack[i]);

  if (NULL != collect_ebp) {
    char *p;
    /* native stack */
    assert(collect_ebp >= collect_esp);
    for (p = collect_ebp-4; p >= collect_esp; p -= 4) {
      cell *cp;
      *((cell**)p) = resolve_ind(*((cell**)p));
      cp = *((cell**)p);
      mark(cp);
    }
  }
  else {
    frame *f;
    for (f = active_frames; f; f = f->next)
      mark_frame(f);
  }
  for (sc = scombs; sc; sc = sc->next)
    mark(sc->body);
/*   printf("\ncollect(): %d/%d cells still reachable\n",nreachable,ncells); */

  /* sweep phase */
  for (bl = blocks; bl; bl = bl->next) {
    for (i = 0; i < BLOCK_SIZE; i++) {
      if (!(bl->cells[i].tag & FLAG_MARKED) &&
          (TYPE_EMPTY != celltype(&bl->cells[i]))) {

        
        if (!(bl->cells[i].tag & FLAG_PINNED)) {
          free_cell_fields(&bl->cells[i]);
          bl->cells[i].tag = TYPE_EMPTY;
          bl->cells[i].field1 = freeptr;
          freeptr = &bl->cells[i];
          ncells--;
        }
      }
    }
  }
}

void cleanup()
{
  block *bl;
  int i;
  frame *f;
  scomb_free_list(&scombs);

  for (bl = blocks; bl; bl = bl->next)
    for (i = 0; i < BLOCK_SIZE; i++)
      bl->cells[i].tag &= ~FLAG_PINNED;

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
  free(addressmap);
  free(noevaladdressmap);
  free(stacksizes);
  if (lexstring)
    array_free(lexstring);
  if (oldnames) {
    for (i = 0; i < (int)(oldnames->size/sizeof(char*)); i++)
      free(((char**)oldnames->data)[i]);
    array_free(oldnames);
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

stack *stack_new(void)
{
  stack *s = (stack*)calloc(1,sizeof(stack));
  s->alloc = 1;
  s->count = 0;
  s->data = (void**)malloc(s->alloc*sizeof(void*));
  return s;
}

stack *stack_new2(int alloc)
{
  stack *s = (stack*)calloc(1,sizeof(stack));
  s->alloc = alloc;
  s->count = 0;
  s->data = (void**)malloc(s->alloc*sizeof(void*));
  return s;
}

void stack_free(stack *s)
{
  free(s->data);
  free(s);
}

void stack_grow(int *alloc, void ***data, int size)
{
  if (*alloc < size) {
    *alloc = size;
    *data = (void**)realloc(*data,(*alloc)*sizeof(void*));
  }
}

void stack_push2(stack *s, void *c)
{
  assert(s->count+1 <= s->alloc);
  s->data[s->count++] = c;
}

void stack_push(stack *s, void *c)
{
  if (s->count == s->alloc) {
    if (s->count >= STACK_LIMIT) {
      fprintf(stderr,"Out of stack space\n");
      exit(1);
    }
    stack_grow(&s->alloc,&s->data,s->alloc*2);
  }
  s->data[s->count++] = c;
}

void stack_insert(stack *s, void *c, int pos)
{
  if (s->count == s->alloc)
    stack_grow(&s->alloc,&s->data,s->alloc*2);
  memmove(&s->data[pos+1],&s->data[pos],(s->count-pos)*sizeof(cell*));
  s->count++;
  s->data[pos] = c;
}

cell *stack_pop(stack *s)
{
  assert(0 < s->count);
  return resolve_ind((cell*)s->data[--s->count]);
}

cell *stack_top(stack *s)
{
  assert(0 < s->count);
  return resolve_ind((cell*)s->data[s->count-1]);
}

cell *stack_at(stack *s, int pos)
{
  assert(0 <= pos);
  assert(pos < s->count);
  return resolve_ind((cell*)s->data[pos]);
}

void statistics(FILE *f)
{
  int i;
  int total;
  fprintf(f,"maxstack = %d\n",maxstack);
  fprintf(f,"totalallocs = %d\n",totalallocs);
  fprintf(f,"ncells = %d\n",ncells);
  fprintf(f,"maxcells = %d\n",maxcells);
  fprintf(f,"nblocks = %d\n",nblocks);
  fprintf(f,"ncollections = %d\n",ncollections);
  fprintf(f,"nscombappls = %d\n",nscombappls);
  fprintf(f,"nresindnoch = %d\n",nresindnoch);
  fprintf(f,"nresindch = %d\n",nresindch);
  fprintf(f,"resolve_ind() total = %d\n",nresindch+nresindnoch);
  fprintf(f,"resolve_ind() ratio = %f\n",((double)nresindch)/((double)nresindnoch));
  fprintf(f,"nreductions = %d\n",nreductions);
  fprintf(f,"nframes = %d\n",nframes);
  fprintf(f,"maxdepth = %d\n",maxdepth);
  fprintf(f,"maxframes = %d\n",maxframes);

  total = 0;
  for (i = 0; i < OP_COUNT; i++) {
    fprintf(f,"usage(%s) = %d\n",op_names[i],op_usage[i]);
    total += op_usage[i];
  }
  fprintf(f,"usage total = %d\n",total);
}

cell *resolve_ind(cell *c)
{
  while (TYPE_IND == celltype(c))
    c = (cell*)c->field1;
  return c;
}

void copy_raw(cell *dest, cell *source)
{
  dest->tag = source->tag & TAG_MASK;
  dest->field1 = source->field1;
  dest->field2 = source->field2;
}

void copy_cell(cell *redex, cell *source)
{
  assert(redex != source);
  copy_raw(redex,source);
  if ((TYPE_STRING == celltype(source)) ||
      (TYPE_SYMBOL == celltype(source)) ||
      (TYPE_LAMBDA == celltype(source)))
    redex->field1 = strdup((char*)redex->field1);
}

void print_stack(cell **stk, int size, int dir)
{
  int i;

  if (dir)
    i = size-1;
  else
    i = 0;


  while (1) {
    cell *c;
    int pos = dir ? (size-1-i) : i;

    if (dir && i < 0)
      break;

    if (!dir && (i >= size))
      break;

    c = resolve_ind(stk[i]);
    if (TYPE_IND == celltype(stk[i]))
      debug(0,"%2d: [i] %12s ",pos,cell_types[celltype(c)]);
    else
      debug(0,"%2d:     %12s ",pos,cell_types[celltype(c)]);
    print_code(c);
    debug(0,"\n");

    if (dir)
      i--;
    else
      i++;
  }
}
