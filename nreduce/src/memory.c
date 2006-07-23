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

cell *global_root = NULL;
int trace = 0;

extern cell **globcells;
extern array *lexstring;

stack *active_stacks = NULL;

int repl_histogram[NUM_CELLTYPES];



cell *freeptr = NULL;

/* dumpentry *dump = NULL; */

dumpentry *dumpstack = NULL;
int dumpalloc = 0;
int dumpcount = 0;

dumpentry *pushdump()
{
  if (dumpalloc <= dumpcount) {
    if (0 == dumpalloc)
      dumpalloc = 2;
    else
      dumpalloc *= 2;
    dumpstack = (dumpentry*)realloc(dumpstack,dumpalloc*sizeof(dumpentry));
  }
  dumpcount++;
/*   printf("pushdump: dumpstack = %p, dumpcount = %d\n",dumpstack,dumpcount); */
  return &dumpstack[dumpcount-1];
}

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
int nALLOCs = 0;
int nALLOCcells = 0;
int ndumpallocs = 0;
int nunwindsvar = 0;
int nunwindswhnf = 0;
int nreductions = 0;
int ndispexact = 0;
int ndispless = 0;
int ndispgreater = 0;
int ndisp0 = 0;
int ndispother = 0;

cell *pinned = NULL;
cell *globnil = NULL;
cell *globtrue = NULL;
cell *globzero = NULL;

void initmem()
{
  memset(&repl_histogram,0,NUM_CELLTYPES*sizeof(int));
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
  c->level = -1;
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
  case TYPE_LETREC:
    mark((cell*)c->field1);
    mark((cell*)c->field2);
    break;
  case TYPE_VARDEF:
    mark((cell*)c->field2);
    break;
  case TYPE_VARLNK:
    mark((cell*)c->field1);
    mark((cell*)c->field2);
    break;
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
  default:
    break;
  }
}

void free_cell_fields(cell *c)
{
/*   if (hasfield1str(celltype(c))) */
/*     free((char*)c->field1); */
  switch (celltype(c)) {
  case TYPE_STRING:
  case TYPE_SYMBOL:
  case TYPE_LAMBDA:
  case TYPE_VARDEF:
    free((char*)c->field1);
    break;
  case TYPE_ARRAY: {
    carray *arr = (carray*)c->field1;
    free(arr->cells);
    free(arr->refs);
    free(arr);
    break;
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
/*   int npinned = 0; */
/*   int ninuse = 0; */
/*   int nind = 0; */
/*   int histogram[NUM_CELLTYPES]; */
/*   memset(&histogram,0,NUM_CELLTYPES*sizeof(int)); */

/*   printf("collect()\n"); */

  ncollections++;
  nallocs = 0;

  /* clear all marks */
  for (bl = blocks; bl; bl = bl->next)
    for (i = 0; i < BLOCK_SIZE; i++)
      bl->cells[i].tag &= ~FLAG_MARKED;
  nreachable = 0;

  /* mark phase */
/*   mark(global_root); */

  if (NULL != collect_ebp) {
    char *p;
    /* native stack */
    assert(collect_ebp >= collect_esp);
/*     printf("collect(): stack bytes = %d\n",collect_ebp-collect_esp); */
    for (p = collect_ebp-4; p >= collect_esp; p -= 4) {
      cell *cp;

      *((cell**)p) = resolve_ind(*((cell**)p));

      cp = *((cell**)p);
/*       printf("collect(): p = %p, cp = %p\n",p,cp); */
      mark(cp);
    }
  }
  else {
    stack *s;
    for (s = active_stacks; s; s = s->next) {
      for (i = 0; i < s->count; i++) {
        s->data[i] = resolve_ind(s->data[i]);
        mark(s->data[i]);
      }
    }
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
/*           printf("  free #%05d\n",bl->cells[i].id); */
/*           debug(0,"collect(): freeing #%05d\n",bl->cells[i].id); */
          free_cell_fields(&bl->cells[i]);
          bl->cells[i].tag = TYPE_EMPTY;
          bl->cells[i].field1 = freeptr;
          freeptr = &bl->cells[i];
          ncells--;
        }
/*         else { */
/*           npinned++; */
/*           printf("pinned: "); */
/*           print_code(&bl->cells[i]); */
/*           printf("\n"); */
/*         } */
      }
/*       else if (TYPE_EMPTY != celltype(&bl->cells[i])) { */
/*         if (TYPE_IND == celltype(&bl->cells[i])) */
/*           nind++; */
/*         ninuse++; */
/*         histogram[celltype(&bl->cells[i])]++; */
/*       } */
    }
  }
/*   printf("collect(): npinned = %d\n",npinned); */
/*   printf("collect(): ninuse = %d\n",ninuse); */
/*   printf("collect(): nind = %d\n",nind); */
/*   for (i = 0; i < NUM_CELLTYPES; i++) { */
/*     printf("# remaining %-12s = %d\n",cell_types[i],histogram[i]); */
/*   } */
}

void cleanup()
{
  block *bl;
  int i;
/*   scomb *sc = scombs; */
/*   while (sc) { */
/*     scomb *next = sc->next; */
/*     scomb_free(sc); */
/*     sc = next; */
/*   } */
/*   scombs = NULL; */
   scomb_free_list(&scombs);

  global_root = NULL;

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
  free(addressmap);
  free(noevaladdressmap);
  free(globcells);
  free(dumpstack);
  if (lexstring)
    array_free(lexstring);
}

stack *stack_new()
{
  stack *s = (stack*)calloc(1,sizeof(stack));
  s->alloc = 4;
  s->count = 0;
  s->base = 0;
  s->data = malloc(s->alloc*sizeof(void*));
  s->next = active_stacks;
  active_stacks = s;
  return s;
}

void stack_free(stack *s)
{
  stack **ptr = &active_stacks;
  while (*ptr != s)
    ptr = &((*ptr)->next);
  *ptr = s->next;
  free(s->data);
  free(s);
}

void stack_grow(stack *s)
{
  s->alloc *= 2;
  s->data = realloc(s->data,s->alloc*sizeof(void*));
}

void stack_push(stack *s, void *c)
{
  if (s->count == s->alloc) {
    if (s->count >= STACK_LIMIT) {
      fprintf(stderr,"Out of stack space\n");
      exit(1);
    }
    stack_grow(s);
  }
  s->data[s->count++] = c;
}

void stack_insert(stack *s, void *c, int pos)
{
  if (s->count == s->alloc)
    stack_grow(s);
  memmove(&s->data[pos+1],&s->data[pos],(s->count-pos)*sizeof(cell*));
  s->count++;
  s->data[pos] = c;
}

void *stack_pop(stack *s)
{
  assert(0 < s->count);
  return resolve_ind(s->data[--s->count]);
}

void *stack_top(stack *s)
{
  assert(0 < s->count);
  return resolve_ind(s->data[s->count-1]);
}

void *stack_at(stack *s, int pos)
{
  assert(0 <= pos);
  assert(pos < s->count);
  return resolve_ind(s->data[pos]);
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
  fprintf(f,"nALLOCs = %d\n",nALLOCs);
  fprintf(f,"nALLOCcells = %d\n",nALLOCcells);
  fprintf(f,"resolve_ind() total = %d\n",nresindch+nresindnoch);
  fprintf(f,"resolve_ind() ratio = %f\n",((double)nresindch)/((double)nresindnoch));
  fprintf(f,"ndumpallocs = %d\n",ndumpallocs);
  fprintf(f,"nunwindsvar = %d\n",nunwindsvar);
  fprintf(f,"nunwindswhnf = %d\n",nunwindswhnf);
  fprintf(f,"nreductions = %d\n",nreductions);
  fprintf(f,"ndispexact = %d\n",ndispexact);
  fprintf(f,"ndispless = %d\n",ndispless);
  fprintf(f,"ndispgreater = %d\n",ndispgreater);
  fprintf(f,"ndisp0 = %d\n",ndisp0);
  fprintf(f,"ndispother = %d\n",ndispother);

  for (i = 0; i < NUM_CELLTYPES; i++)
    fprintf(f,"repl_histogram[%s] = %d\n",cell_types[i],repl_histogram[i]);

  total = 0;
  for (i = 0; i < OP_COUNT; i++) {
    fprintf(f,"usage(%s) = %d\n",op_names[i],op_usage[i]);
    total += op_usage[i];
  }
  fprintf(f,"usage total = %d\n",total);
}

#ifndef INLINE_RESOLVE_IND
cell *resolve_ind(cell *c)
{
/*   cell *orig = c; */
/*   int n = 0; */
  while (TYPE_IND == celltype(c)) {
/*     assert(c->field1 != c); */
    c = (cell*)c->field1;
/*     n++; */
  }

/*   if (orig == c) */
/*     nresindnoch++; */
/*   else */
/*     nresindch++; */

/*   printf("resolve_ind: n = %d\n",n); */
  return c;
}
#endif

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
      (TYPE_LAMBDA == celltype(source)) ||
      (TYPE_VARDEF == celltype(source)))
    redex->field1 = strdup((char*)redex->field1);
}

void clearflag(int flag)
{
  block *bl;
  int i;
  int count = 0;
  for (bl = blocks; bl; bl = bl->next) {
    for (i = 0; i < BLOCK_SIZE; i++) {
      bl->cells[i].tag &= ~flag;
      count++;
    }
  }
/*   printf("clearflag: processed %d cells\n",count); */
}

void setneedclear_r(cell *c)
{
  if (c->tag & FLAG_NEEDCLEAR)
    return;
  c->tag |= FLAG_NEEDCLEAR;

  switch (celltype(c)) {
  case TYPE_APPLICATION:
  case TYPE_CONS:
    setneedclear_r((cell*)c->field1);
    setneedclear_r((cell*)c->field2);
    break;
  case TYPE_LAMBDA:
    setneedclear_r((cell*)c->field2);
    break;
  case TYPE_IND:
    setneedclear_r((cell*)c->field1);
    break;
  case TYPE_LETREC: {
    cell *lnk;
    for (lnk = (cell*)c->field1; lnk; lnk = (cell*)lnk->field2)
      setneedclear_r((cell*)lnk->field1);
    setneedclear_r((cell*)c->field2);
    break;
  }
  case TYPE_VARDEF:
    setneedclear_r((cell*)c->field2);
    break;
  case TYPE_VARLNK:
    assert(0);
    break;
  }
}

void cleargraph_r(cell *c, int flag)
{
  if (!(c->tag & FLAG_NEEDCLEAR))
    return;
  c->tag &= ~FLAG_NEEDCLEAR;
  c->tag &= ~flag;

  switch (celltype(c)) {
  case TYPE_APPLICATION:
  case TYPE_CONS:
    cleargraph_r((cell*)c->field1,flag);
    cleargraph_r((cell*)c->field2,flag);
    break;
  case TYPE_LAMBDA:
    cleargraph_r((cell*)c->field2,flag);
    break;
  case TYPE_IND:
    cleargraph_r((cell*)c->field1,flag);
    break;
  case TYPE_LETREC: {
    cell *lnk;
    for (lnk = (cell*)c->field1; lnk; lnk = (cell*)lnk->field2)
      cleargraph_r((cell*)lnk->field1,flag);
    cleargraph_r((cell*)c->field2,flag);
    break;
  }
  case TYPE_VARDEF:
    cleargraph_r((cell*)c->field2,flag);
    break;
  case TYPE_VARLNK:
    assert(0);
    break;
  }
}

void cleargraph(cell *root, int flag)
{
  setneedclear_r(root);
  cleargraph_r(root,flag);
}

void print_stack(int redex, cell **stk, int size, int dir)
{
  int i;
/*   dumpentry *de; */
/*   int dumpcount = 0; */

/*   for (de = dump; de; de = de->next) */
/*     dumpcount++; */

  if (dir)
    i = size-1;
  else
    i = 0;


  while (1) {
    int wasdump = 0;
    cell *c;
    int pos = dir ? (size-1-i) : i;
    int dpos;

    if (dir && i < 0)
      break;

    if (!dir && (i >= size))
      break;

    c = resolve_ind(stk[i]);

    for (dpos = 0; dpos < dumpcount; dpos++) {
      if (dumpstack[dpos].stackcount-1 == i) {
        debug(0,"  D%-2d ",dumpcount-1-dpos);
        wasdump = 1;
        break;
      }
    }

    if (!wasdump)
      debug(0,"      ");

    debug(0,"%2d: %p %12s ",pos,c,cell_types[celltype(c)]);
    print_code(c);
    if (i == redex)
      debug(0," <----- redex");
    debug(0,"\n");

    if (dir)
      i--;
    else
      i++;
  }
}

char *def_name(cell *lnk)
{
  assert(TYPE_VARLNK == celltype(lnk));
  cell *def = (cell*)lnk->field1;
  char *name = (char*)def->field1;
  return name;
}

cell *def_value(cell *lnk)
{
  assert(TYPE_VARLNK == celltype(lnk));
  cell *def = (cell*)lnk->field1;
  cell *value = (cell*)def->field2;
  return value;
}

cell *letrec_defs(cell *letrec)
{
  assert(TYPE_LETREC == celltype(letrec));
  return (cell*)letrec->field1;
}

cell *letrec_body(cell *letrec)
{
  assert(TYPE_LETREC == celltype(letrec));
  return (cell*)letrec->field2;
}
