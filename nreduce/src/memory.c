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

typedef struct block {
  struct block *next;
  cell cells[BLOCK_SIZE];
} block;

int trace = 0;

extern array *lexstring;
extern array *oldnames;

cell *freeptr = NULL;

block *blocks = NULL;
int nblocks = 0;

int maxstack = 0;
int ncells = 0;
int maxcells = 0;
int nallocs = 0;
int totalallocs = 0;
int totalrtallocs = 0;
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
pntr globnilpntr = 0.0;
pntr globtruepntr = 0.0;
pntr globzeropntr = 0.0;

#ifndef INLINE_PTRFUNS
int is_pntr(pntr p)
{
  return (*(((unsigned int*)&p)+1) == 0xFFFFFFF1);
}

rtvalue *get_pntr(pntr p)
{
  assert(is_pntr(p));
  return (rtvalue*)(*((unsigned int*)&p));
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
    p = get_pntr(p)->cmp1;
  return p;
}
#endif

#ifndef UNBOXED_NUMBERS
pntr make_number(double d)
{
  rtvalue *v = alloc_rtvalue();
  pntr p;
  v->tag = TYPE_NUMBER;
  v->cmp1 = d;
  make_pntr(p,v);
  return p;
}
#endif

void initmem()
{
  rtvalue *globnilvalue;

  memset(&op_usage,0,OP_COUNT*sizeof(int));
  globnil = alloc_cell();
  globnil->tag = TYPE_NIL | FLAG_PINNED;
  globtrue = alloc_cell();
  globtrue->tag = TYPE_NUMBER | FLAG_PINNED;
  celldouble(globtrue) = 1.0;
  globzero = alloc_cell();
  globzero->tag = TYPE_NUMBER | FLAG_PINNED;
  celldouble(globzero) = 0.0;

  globnilvalue = alloc_rtvalue();
  globnilvalue->tag = TYPE_NIL | FLAG_PINNED;

  make_pntr(globnilpntr,globnilvalue);
  globtruepntr = make_number(1.0);
  globzeropntr = make_number(0.0);

  if (is_pntr(globtruepntr))
    get_pntr(globtruepntr)->tag |= FLAG_PINNED;
  if (is_pntr(globzeropntr))
    get_pntr(globzeropntr)->tag |= FLAG_PINNED;
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
  return c;
}

cell *alloc_sourcecell(const char *filename, int lineno)
{
  return alloc_cell();
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
  case TYPE_AREF:
  case TYPE_ARRAY:
  case TYPE_FRAME:
  case TYPE_CAP:
    assert(0);
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

  rtcleanup();

  scomb_free_list(&scombs);

  for (bl = blocks; bl; bl = bl->next)
    for (i = 0; i < BLOCK_SIZE; i++)
      bl->cells[i].tag &= ~FLAG_PINNED;

  collect();

  bl = blocks;
  while (bl) {
    block *next = bl->next;
    free(bl);
    bl = next;
  }
  free(funcalls);
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

void pntrstack_grow(int *alloc, pntr **data, int size)
{
  if (*alloc < size) {
    *alloc = size;
    *data = (pntr*)realloc(*data,(*alloc)*sizeof(pntr));
  }
}

static void stack_grow(int *alloc, void ***data, int size)
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
  fprintf(f,"totalrtallocs = %d\n",totalrtallocs);
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

void print_stack(pntr *stk, int size, int dir)
{
  int i;

  if (dir)
    i = size-1;
  else
    i = 0;


  while (1) {
    pntr p;
    int pos = dir ? (size-1-i) : i;

    if (dir && i < 0)
      break;

    if (!dir && (i >= size))
      break;

    p = resolve_pntr(stk[i]);
    if (TYPE_IND == pntrtype(stk[i]))
      debug(0,"%2d: [i] %12s ",pos,cell_types[pntrtype(p)]);
    else
      debug(0,"%2d:     %12s ",pos,cell_types[pntrtype(p)]);
    print_pntr(p);
    debug(0,"\n");

    if (dir)
      i--;
    else
      i++;
  }
}
