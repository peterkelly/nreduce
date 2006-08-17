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
 * $Id: memory.c 315 2006-08-14 09:23:32Z pmkelly $
 *
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

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

typedef struct rtblock {
  struct rtblock *next;
  rtvalue values[BLOCK_SIZE];
} rtblock;

static int framecount = 0;
static int active_framecount = 0;
static frame *freeframe = NULL;

frame *active_frames = NULL;
pntrstack *streamstack = NULL;

rtblock *rtblocks = NULL;
int nrtrtblocks = 0;
rtvalue *rtfreeptr = NULL;

void rtmark(pntr p);

void rtmark_frame(frame *f)
{
/*   printf("rtmark_frame %p (cell %p)\n",f,f->c); */
  int i;
  if (f->completed) {
    assert(!"completed frame referenced from here");
  }
  if (f->c)
    rtmark(make_pntr(f->c));
  for (i = 0; i < f->count; i++)
    rtmark(f->data[i]);
}

void rtmark_cap(cap *c)
{
  int i;
  for (i = 0; i < c->count; i++)
    rtmark(c->data[i]);
}








void rtmark(pntr p)
{
  rtvalue *c;
  if (!is_pntr(p))
    return;
  c = get_pntr(p);
  assert(TYPE_EMPTY != celltype(c));
  if (c->tag & FLAG_MARKED)
    return;
  c->tag |= FLAG_MARKED;
  switch (celltype(c)) {
  case TYPE_IND:
    rtmark(c->cmp1);
    break;
  case TYPE_APPLICATION:
  case TYPE_CONS:
    c->cmp1 = resolve_pntr(c->cmp1);
    c->cmp2 = resolve_pntr(c->cmp2);
    rtmark(c->cmp1);
    rtmark(c->cmp2);
    break;
  case TYPE_AREF: {
    rtvalue *arrcell = get_pntr(c->cmp1);
    carray *arr = (carray*)get_pntr(arrcell->cmp1);
    int index = (int)get_pntr(c->cmp2);
    assert(index < arr->size);
    assert(get_pntr(arr->refs[index]) == c);

    rtmark(c->cmp1);
    break;
  }
  case TYPE_ARRAY: {
    carray *arr = (carray*)get_pntr(c->cmp1);
    int i;
    for (i = 0; i < arr->size; i++) {
      rtmark(arr->cells[i]);
      if (!is_nullpntr(arr->refs[i]))
        rtmark(arr->refs[i]);
    }
    rtmark(arr->tail);
    if (!is_nullpntr(arr->sizecell))
      rtmark(arr->sizecell);
    break;
  }
  case TYPE_FRAME:
    rtmark_frame((frame*)get_pntr(c->cmp1));
    break;
  case TYPE_CAP:
    rtmark_cap((cap*)get_pntr(c->cmp1));
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

rtvalue *alloc_rtvalue()
{
  rtvalue *v;
  if (NULL == rtfreeptr) {
    rtblock *bl = (rtblock*)calloc(1,sizeof(rtblock));
    int i;
    bl->next = rtblocks;
    rtblocks = bl;
    for (i = 0; i < BLOCK_SIZE-1; i++)
      bl->values[i].cmp1 = make_pntr(&bl->values[i+1]);
    bl->values[i].cmp1 = make_pntr(NULL);

    rtfreeptr = &bl->values[0];
    nrtrtblocks++;
  }
  v = rtfreeptr;
  rtfreeptr = (rtvalue*)get_pntr(rtfreeptr->cmp1);
  nallocs++;
  totalrtallocs++;
  return v;
}

void free_rtvalue_fields(rtvalue *v)
{
  switch (celltype(v)) {
  case TYPE_STRING:
  case TYPE_SYMBOL:
  case TYPE_LAMBDA:
    free(get_string(v->cmp1));
    break;
  case TYPE_ARRAY: {
    carray *arr = (carray*)get_pntr(v->cmp1);
    free(arr->cells);
    free(arr->refs);
    free(arr);
    break;
  }
  case TYPE_FRAME: {
/*     printf("here at frame\n"); */
    /* note: we don't necessarily free frames here... this is only safe to do after the frame
       has finished executing */
    frame *f = (frame*)get_pntr(v->cmp1);
    f->c = NULL;
    if (!f->active)
      frame_dealloc(f);
    else printf("free_cell_fields: frame still active\n");

    break;
  }
  case TYPE_CAP: {
    cap *cp = (cap*)get_pntr(v->cmp1);
    cap_dealloc(cp);
  }
  }
}

extern pntr *gstack;
extern int gstackcount;

void rtcollect()
{
  rtblock *bl;
  int i;
  frame *f;

  ncollections++;
  nallocs = 0;

  /* clear all marks */
  for (bl = rtblocks; bl; bl = bl->next)
    for (i = 0; i < BLOCK_SIZE; i++)
      bl->values[i].tag &= ~FLAG_MARKED;

  /* mark phase */

  if (streamstack)
    for (i = 0; i < streamstack->count; i++)
      rtmark(streamstack->data[i]);

  if (gstack)
    for (i = 0; i < gstackcount; i++)
      rtmark(gstack[i]);

  for (f = active_frames; f; f = f->next)
    rtmark_frame(f);

  /* sweep phase */
  for (bl = rtblocks; bl; bl = bl->next) {
    for (i = 0; i < BLOCK_SIZE; i++) {
      if (!(bl->values[i].tag & FLAG_MARKED) &&
          (TYPE_EMPTY != celltype(&bl->values[i]))) {

        
        if (!(bl->values[i].tag & FLAG_PINNED)) {
          free_rtvalue_fields(&bl->values[i]);
          bl->values[i].tag = TYPE_EMPTY;
          bl->values[i].cmp1 = make_pntr(rtfreeptr);
          rtfreeptr = &bl->values[i];
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

void rtcleanup()
{
  frame *f;
  rtblock *bl;
  int i;

  for (bl = rtblocks; bl; bl = bl->next)
    for (i = 0; i < BLOCK_SIZE; i++)
      bl->values[i].tag &= ~FLAG_PINNED;

  rtcollect();

  f = freeframe;
  while (f) {
    frame *next = f->freelnk;
    f->c = NULL;
    frame_free(f);
    f = next;
  }

  bl = rtblocks;
  while (bl) {
    rtblock *next = bl->next;
    free(bl);
    bl = next;
  }
}

void print_pntr(pntr p)
{
  printf("(value)");
}
