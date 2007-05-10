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

#define BUILTINS_C

#include "src/nreduce.h"
#include "compiler/source.h"
#include "compiler/bytecode.h"
#include "compiler/util.h"
#include "runtime.h"
#include "node.h"
#include "messages.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <assert.h>
#include <ctype.h>
#include <stdarg.h>
#include <math.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <dirent.h>

static const char *numnames[4] = {"first", "second", "third", "fourth"};

static unsigned char NAN_BITS[8] = { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xF8, 0xFF };

#define CHECK_ARG(_argno,_type) {                                       \
    if ((_type) != pntrtype(argstack[(_argno)])) {                      \
      int bif = ((*tsk->runptr)->instr-1)->arg0;                        \
      assert(OP_BIF == ((*tsk->runptr)->instr-1)->opcode);              \
      invalid_arg(tsk,argstack[(_argno)],bif,(_argno),(_type));         \
      return;                                                           \
    }                                                                   \
  }

#define CHECK_SYSOBJECT_ARG(_argno,_sotype) {                           \
    CHECK_ARG(_argno,CELL_SYSOBJECT);                                   \
    if ((_sotype) != psysobject(argstack[(_argno)])->type) {            \
      int bif = ((*tsk->runptr)->instr-1)->arg0;                        \
      assert(OP_BIF == ((*tsk->runptr)->instr-1)->opcode);              \
      set_error(tsk,"%s: %s arg must be CONNECTION, got %s",            \
                builtin_info[(bif)].name,                               \
                numnames[builtin_info[(bif)].nargs-1-(_argno)],         \
                sysobject_types[psysobject(argstack[(_argno)])->type]); \
      return;                                                           \
    }                                                                   \
  }

#define CHECK_NUMERIC_ARGS(bif)                                         \
  if ((CELL_NUMBER != pntrtype(argstack[1])) ||                         \
      (CELL_NUMBER != pntrtype(argstack[0]))) {                         \
    invalid_binary_args(tsk,argstack,bif);                              \
    return;                                                             \
  }

const builtin builtin_info[NUM_BUILTINS];

static void setnumber(pntr *cptr, double val)
{
  if (isnan(val))
    *cptr = *(pntr*)NAN_BITS;
  else
    set_pntrdouble(*cptr,val);
}

static void setbool(task *tsk, pntr *cptr, int b)
{
  *cptr = (b ? tsk->globtruepntr : tsk->globnilpntr);
}

void invalid_arg(task *tsk, pntr arg, int bif, int argno, int type)
{
  if (CELL_CAP == pntrtype(arg))
    cap_error(tsk,arg);
  else
    set_error(tsk,"%s: %s argument must be of type %s, but got %s",
              builtin_info[bif].name,numnames[builtin_info[bif].nargs-1-argno],
              cell_types[type],cell_types[pntrtype(arg)]);
}

void invalid_binary_args(task *tsk, pntr *argstack, int bif)
{
  if (CELL_CAP == pntrtype(argstack[1]))
    cap_error(tsk,argstack[1]);
  else if (CELL_CAP == pntrtype(argstack[0]))
    cap_error(tsk,argstack[0]);
  else
    set_error(tsk,"%s: incompatible arguments: (%s,%s)",builtin_info[bif].name,
              cell_types[pntrtype(argstack[1])],cell_types[pntrtype(argstack[0])]);
  argstack[0] = tsk->globnilpntr;
}

static void b_add(task *tsk, pntr *argstack)
{
  CHECK_NUMERIC_ARGS(B_ADD);
  setnumber(&argstack[0],pntrdouble(argstack[1]) + pntrdouble(argstack[0]));
}

static void b_subtract(task *tsk, pntr *argstack)
{
  CHECK_NUMERIC_ARGS(B_SUBTRACT);
  setnumber(&argstack[0],pntrdouble(argstack[1]) - pntrdouble(argstack[0]));
}

static void b_multiply(task *tsk, pntr *argstack)
{
  CHECK_NUMERIC_ARGS(B_MULTIPLY);
  setnumber(&argstack[0],pntrdouble(argstack[1]) * pntrdouble(argstack[0]));
}

static void b_divide(task *tsk, pntr *argstack)
{
  CHECK_NUMERIC_ARGS(B_DIVIDE);
  setnumber(&argstack[0],pntrdouble(argstack[1]) / pntrdouble(argstack[0]));
}

static void b_mod(task *tsk, pntr *argstack)
{
  CHECK_NUMERIC_ARGS(B_MOD);
  setnumber(&argstack[0],fmod(pntrdouble(argstack[1]),pntrdouble(argstack[0])));
}

static void b_eq(task *tsk, pntr *argstack)
{
  CHECK_NUMERIC_ARGS(B_EQ);
  setbool(tsk,&argstack[0],pntrdouble(argstack[1]) == pntrdouble(argstack[0]));
}

static void b_ne(task *tsk, pntr *argstack)
{
  CHECK_NUMERIC_ARGS(B_NE);
  setbool(tsk,&argstack[0],pntrdouble(argstack[1]) != pntrdouble(argstack[0]));
}

static void b_lt(task *tsk, pntr *argstack)
{
  CHECK_NUMERIC_ARGS(B_LT);
  setbool(tsk,&argstack[0],pntrdouble(argstack[1]) <  pntrdouble(argstack[0]));
}

static void b_le(task *tsk, pntr *argstack)
{
  CHECK_NUMERIC_ARGS(B_LE);
  setbool(tsk,&argstack[0],pntrdouble(argstack[1]) <= pntrdouble(argstack[0]));
}

static void b_gt(task *tsk, pntr *argstack)
{
  CHECK_NUMERIC_ARGS(B_GT);
  setbool(tsk,&argstack[0],pntrdouble(argstack[1]) >  pntrdouble(argstack[0]));
}

static void b_ge(task *tsk, pntr *argstack)
{
  CHECK_NUMERIC_ARGS(B_GE);
  setbool(tsk,&argstack[0],pntrdouble(argstack[1]) >= pntrdouble(argstack[0]));
}

static void b_and(task *tsk, pntr *argstack)
{
  setbool(tsk,&argstack[0],
          (CELL_NIL != pntrtype(argstack[0])) && (CELL_NIL != pntrtype(argstack[1])));
}

static void b_or(task *tsk, pntr *argstack)
{
  setbool(tsk,&argstack[0],
          (CELL_NIL != pntrtype(argstack[0])) || (CELL_NIL != pntrtype(argstack[1])));
}

static void b_not(task *tsk, pntr *argstack)
{
  if (CELL_NIL == pntrtype(argstack[0]))
    argstack[0] = tsk->globtruepntr;
  else
    argstack[0] = tsk->globnilpntr;
}

static void b_bitop(task *tsk, pntr *argstack, int bif)
{
  pntr val2 = argstack[0];
  pntr val1 = argstack[1];
  int a;
  int b;

  if ((CELL_NUMBER != pntrtype(val1)) || (CELL_NUMBER != pntrtype(val2))) {
    fprintf(stderr,"%s: incompatible arguments (must be numbers)\n",builtin_info[bif].name);
    exit(1);
  }

  a = (int)pntrdouble(val1);
  b = (int)pntrdouble(val2);

  switch (bif) {
  case B_BITSHL:  setnumber(&argstack[0],(double)(a << b));  break;
  case B_BITSHR:  setnumber(&argstack[0],(double)(a >> b));  break;
  case B_BITAND:  setnumber(&argstack[0],(double)(a & b));   break;
  case B_BITOR:   setnumber(&argstack[0],(double)(a | b));   break;
  case B_BITXOR:  setnumber(&argstack[0],(double)(a ^ b));   break;
  default:        abort();       break;
  }
}

static void b_bitshl(task *tsk, pntr *argstack) { b_bitop(tsk,argstack,B_BITSHL); }
static void b_bitshr(task *tsk, pntr *argstack) { b_bitop(tsk,argstack,B_BITSHR); }
static void b_bitand(task *tsk, pntr *argstack) { b_bitop(tsk,argstack,B_BITAND); }
static void b_bitor(task *tsk, pntr *argstack) { b_bitop(tsk,argstack,B_BITOR); }
static void b_bitxor(task *tsk, pntr *argstack) { b_bitop(tsk,argstack,B_BITXOR); }

static void b_bitnot(task *tsk, pntr *argstack)
{
  pntr arg = argstack[0];
  int val;
  if (CELL_NUMBER != pntrtype(arg)) {
    fprintf(stderr,"~: incompatible argument (must be a number)\n");
    exit(1);
  }
  val = (int)pntrdouble(arg);
  setnumber(&argstack[0],(double)(~val));
}

static void b_sqrt(task *tsk, pntr *argstack)
{
  CHECK_ARG(0,CELL_NUMBER);
  setnumber(&argstack[0],sqrt(pntrdouble(argstack[0])));
}

static void b_floor(task *tsk, pntr *argstack)
{
  CHECK_ARG(0,CELL_NUMBER);
  setnumber(&argstack[0],floor(pntrdouble(argstack[0])));
}

static void b_ceil(task *tsk, pntr *argstack)
{
  CHECK_ARG(0,CELL_NUMBER);
  setnumber(&argstack[0],ceil(pntrdouble(argstack[0])));
}

static void b_seq(task *tsk, pntr *argstack)
{
}

static void b_par(task *tsk, pntr *argstack)
{
  pntr p = resolve_pntr(argstack[1]);
  if (CELL_FRAME == pntrtype(p)) {
    frame *f = (frame*)get_pntr(get_pntr(p)->field1);
    spark_frame(tsk,f);
/*     fprintf(tsk->output,"sparking\n"); */
  }
  else {
/*     fprintf(tsk->output,"not sparking - it's a %s\n",cell_types[pntrtype(p)]); */
  }
}

static void b_head(task *tsk, pntr *argstack);
static void b_parhead(task *tsk, pntr *argstack)
{
/*   fprintf(tsk->output,"b_parhead: first arg is a %s\n",cell_types[pntrtype(argstack[1])]); */
  b_head(tsk,&argstack[1]);
  b_par(tsk,argstack);
}

static void b_if(task *tsk, pntr *argstack)
{
  pntr ifc = argstack[2];
  pntr source;

  if (CELL_NIL == pntrtype(ifc))
    source = argstack[0];
  else
    source = argstack[1];

  argstack[0] = source;
}

 /* Can be called from native code */
carray *carray_new(task *tsk, int dsize, int alloc, carray *oldarr, cell *usewrapper)
{
  carray *arr;

  if (MAX_ARRAY_SIZE < alloc)
    alloc = MAX_ARRAY_SIZE;
  else if (0 >= alloc)
    alloc = 8;

  arr = (carray*)malloc(sizeof(carray)+alloc*dsize);
  arr->alloc = alloc;
  arr->size = 0;
  arr->elemsize = dsize;
  arr->tail = tsk->globnilpntr;
  arr->nchars = 0;
  arr->wrapper = usewrapper ? usewrapper : alloc_cell(tsk);
  arr->wrapper->type = CELL_AREF;
  make_pntr(arr->wrapper->field1,arr);

  if (oldarr)
    make_aref_pntr(oldarr->tail,arr->wrapper,0);

  #ifdef PROFILING
  tsk->stats.array_allocs++;
  #endif

  return arr;
}

int pntr_is_char(pntr p)
{
  if (CELL_NUMBER == pntrtype(p)) {
    double d = pntrdouble(p);
    if ((floor(d) == d) && (0 <= d) && (255 >= d))
      return 1;
  }
  return 0;
}

static void convert_to_string(task *tsk, carray *arr)
{
  pntr *elemp = (pntr*)arr->elements;
  char *charp = (char*)arr->elements;
  int i;

  assert(sizeof(pntr) == arr->elemsize);
  assert(arr->nchars == arr->size);

  for (i = 0; i < arr->size; i++)
    charp[i] = (unsigned char)pntrdouble(elemp[i]);
  arr->elemsize = 1;
}

static void check_array_convert(task *tsk, carray *arr, const char *from)
{
#ifndef DISABLE_ARRAYS
  arr->tail = resolve_pntr(arr->tail);
  if ((sizeof(pntr) == arr->elemsize) &&
      ((MAX_ARRAY_SIZE == arr->size) || (CELL_FRAME != pntrtype(arr->tail)))) {
    pntr *pelements = (pntr*)arr->elements;

    while (arr->nchars < arr->size) {
      pelements[arr->nchars] = resolve_pntr(pelements[arr->nchars]);
      if (pntr_is_char(pelements[arr->nchars]))
        arr->nchars++;
      else
        break;
    }

    if (arr->nchars == arr->size)
      convert_to_string(tsk,arr);
  }
#endif
}

void carray_append(task *tsk, carray **arr, const void *data, int totalcount, int dsize)
{
  assert(dsize == (*arr)->elemsize); /* sanity check */

  tsk->alloc_bytes += totalcount*dsize;
  if ((tsk->alloc_bytes >= COLLECT_THRESHOLD) && tsk->endpt && tsk->endpt->interruptptr)
    endpoint_interrupt(tsk->endpt);

  while (1) {
    int count = totalcount;
    if ((*arr)->size+count > MAX_ARRAY_SIZE)
      count = MAX_ARRAY_SIZE - (*arr)->size;

    if ((*arr)->alloc < (*arr)->size+count) {
      cell *wrapper = (*arr)->wrapper;
      assert(wrapper);
      while ((*arr)->alloc < (*arr)->size+count)
        (*arr)->alloc *= 2;
      (*arr) = (carray*)realloc((*arr),sizeof(carray)+(*arr)->alloc*(*arr)->elemsize);
      make_pntr(wrapper->field1,*arr);
      #ifdef PROFILING
      tsk->stats.array_resizes++;
      #endif
    }

    memmove(((char*)(*arr)->elements)+(*arr)->size*(*arr)->elemsize,data,count*(*arr)->elemsize);
    (*arr)->size += count;

    data += count;
    totalcount -= count;

    if (0 == totalcount)
      break;

    check_array_convert(tsk,*arr,"append");

    *arr = carray_new(tsk,dsize,totalcount,*arr,NULL);
  }
}

void maybe_expand_array(task *tsk, pntr p)
{
#ifndef DISABLE_ARRAYS
  if (CELL_CONS == pntrtype(p)) {
    cell *firstcell = get_pntr(p);
    pntr firsthead = resolve_pntr(firstcell->field1);
    pntr firsttail = resolve_pntr(firstcell->field2);

    if (CELL_CONS == pntrtype(firsttail)) {

      /* Case 1: Two consecutive CONS cells - convert to an array */

      cell *secondcell = get_pntr(firsttail);
      pntr secondhead = resolve_pntr(secondcell->field1);
      pntr secondtail = resolve_pntr(secondcell->field2);
      carray *arr;

      arr = carray_new(tsk,sizeof(pntr),0,NULL,firstcell);
      carray_append(tsk,&arr,&firsthead,1,sizeof(pntr));
      carray_append(tsk,&arr,&secondhead,1,sizeof(pntr));
      arr->tail = secondtail;
    }
  }

  if ((CELL_AREF == pntrtype(p)) && (sizeof(pntr) == aref_array(p)->elemsize)) {
    carray *arr = aref_array(p);
    arr->tail = resolve_pntr(arr->tail);

    /* FIXME: need to optimise this code by making sure it adjusts references to the
       old tails to point to this array, wherver possible. We can't just replace the
       old tails with INDs, because the execution engine assumes that once a cell is
       non-IND, it remains non-IND for the rest of its lifetime. */

    while (MAX_ARRAY_SIZE > arr->size) {
      if (CELL_CONS == pntrtype(arr->tail)) {
        cell *tailcell = get_pntr(arr->tail);
        pntr tailhead = resolve_pntr(tailcell->field1);
        carray_append(tsk,&arr,&tailhead,1,sizeof(pntr));
        arr->tail = resolve_pntr(tailcell->field2);

        /* FIXME: is there a way to do this safely? */
/*         tailcell->type = CELL_IND; */
/*         make_aref_pntr(tailcell->field1,arr->wrapper,arr->size-1); */
      }
      else if ((CELL_AREF == pntrtype(arr->tail)) &&
               (sizeof(pntr) == aref_array(arr->tail)->elemsize)) {
        carray *otharr = aref_array(arr->tail);
        int othindex = aref_index(arr->tail);
        carray_append(tsk,&arr,&((pntr*)otharr->elements)[othindex],
                      otharr->size-othindex,sizeof(pntr));
        arr->tail = otharr->tail;
      }
      else {
        break;
      }
    }

    check_array_convert(tsk,arr,"expand");
  }


#endif
}

pntr data_to_list(task *tsk, const char *data, int size, pntr tail)
{
  #ifdef DISABLE_ARRAYS
  pntr p = tsk->globnilpntr;
  pntr *prev = &p;
  int i;
  for (i = 0; i < size; i++) {
    cell *ch = alloc_cell(tsk);
    ch->type = CELL_CONS;
    ch->field1 = data[i];
    make_pntr(*prev,ch);
    prev = &ch->field2;
  }
  *prev = tail;
  return p;
  #else
  if (0 == size) {
    return tail;
  }
  else {
    carray *arr = carray_new(tsk,1,size,NULL,NULL);
    pntr p;
    make_aref_pntr(p,arr->wrapper,0);
    carray_append(tsk,&arr,data,size,1);
    arr->tail = tail;
    return p;
  }
  #endif
}

pntr string_to_array(task *tsk, const char *str)
{
  return data_to_list(tsk,str,strlen(str),tsk->globnilpntr);
}

int array_to_string(pntr refpntr, char **str)
{
  pntr p = refpntr;
  array *buf = array_new(1,0);
  char zero = '\0';
  int badtype = -1;

  *str = NULL;

  while (0 > badtype) {

    if (CELL_CONS == pntrtype(p)) {
      cell *c = get_pntr(p);
      pntr head = resolve_pntr(c->field1);
      pntr tail = resolve_pntr(c->field2);
      if (pntr_is_char(head)) {
        char cc = (char)pntrdouble(head);
        array_append(buf,&cc,1);
      }
      else {
        badtype = pntrtype(head);
        break;
      }
      p = tail;
    }
    else if (CELL_AREF == pntrtype(p)) {
      carray *arr = aref_array(p);
      int index = aref_index(p);

      if (1 == arr->elemsize) {
        array_append(buf,&((char*)arr->elements)[index],arr->size-index);
      }
      else {
        pntr *pelements = (pntr*)arr->elements;
        int i;
        for (i = index; i < arr->size; i++) {
          pntr elem = resolve_pntr(pelements[i]);
          if (pntr_is_char(elem)) {
            char cc = (char)pntrdouble(elem);
            array_append(buf,&cc,1);
          }
          else {
            badtype = pntrtype(elem);
            break;
          }
        }
      }
      p = resolve_pntr(arr->tail);
    }
    else if (CELL_NIL == pntrtype(p)) {
      break;
    }
    else {
      badtype = pntrtype(p);
      break;
    }
  }

  if (0 <= badtype) {
    *str = NULL;
    free(buf->data);
  }
  else {
    array_append(buf,&zero,1);
    *str = buf->data;
  }

  free(buf);
  return badtype;
}

static void b_cons(task *tsk, pntr *argstack)
{
  pntr head = argstack[1];
  pntr tail = argstack[0];

  cell *res = alloc_cell(tsk);
  res->type = CELL_CONS;
  res->field1 = head;
  res->field2 = tail;

  make_pntr(argstack[0],res);
}

static void b_head(task *tsk, pntr *argstack)
{
  pntr arg = argstack[0];

  if (CELL_CONS == pntrtype(arg)) {
    argstack[0] = get_pntr(arg)->field1;
  }
  else if (CELL_AREF == pntrtype(arg)) {
    carray *arr = aref_array(arg);
    int index = aref_index(arg);
    assert(index < arr->size);

    if (sizeof(pntr) == arr->elemsize)
      argstack[0] = ((pntr*)arr->elements)[index];
    else if (1 == arr->elemsize)
      set_pntrdouble(argstack[0],(double)(((char*)arr->elements)[index]));
    else
      set_error(tsk,"head: invalid array size");
  }
  else {
    set_error(tsk,"head: expected aref or cons, got %s",cell_types[pntrtype(arg)]);
  }
}

static void b_tail(task *tsk, pntr *argstack)
{
  pntr arg = argstack[0];

  maybe_expand_array(tsk,arg);

  if (CELL_CONS == pntrtype(arg)) {
    cell *conscell = get_pntr(arg);
    argstack[0] = conscell->field2;
  }
  else if (CELL_AREF == pntrtype(arg)) {
    int index = aref_index(arg);
    carray *arr = aref_array(arg);
    assert(index < arr->size);

    if (index+1 < arr->size) {
      make_aref_pntr(argstack[0],arr->wrapper,index+1);
    }
    else {
      argstack[0] = arr->tail;
    }
  }
  else {
    set_error(tsk,"tail: expected aref or cons, got %s",cell_types[pntrtype(arg)]);
  }
}

static void b_arraysize(task *tsk, pntr *argstack)
{
  pntr refpntr = argstack[0];
  maybe_expand_array(tsk,refpntr);
  if (CELL_CONS == pntrtype(refpntr)) {
    setnumber(&argstack[0],1);
  }
  else if (CELL_AREF == pntrtype(refpntr)) {
    carray *arr = aref_array(refpntr);
    int index = aref_index(refpntr);
    assert(index < arr->size);
    setnumber(&argstack[0],arr->size-index);
  }
  else {
    set_error(tsk,"arraysize: expected aref or cons, got %s",cell_types[pntrtype(refpntr)]);
  }
}

static void b_arrayskip(task *tsk, pntr *argstack)
{
  pntr npntr = argstack[1];
  pntr refpntr = argstack[0];
  int n;

  CHECK_ARG(1,CELL_NUMBER);

  maybe_expand_array(tsk,refpntr);

  n = (int)pntrdouble(npntr);
  assert(0 <= n);


  if (CELL_CONS == pntrtype(refpntr)) {
    assert((0 == n) || (1 == n));

    if (0 == n)
      argstack[0] = refpntr;
    else
      argstack[0] = get_pntr(refpntr)->field2;
  }
  else if (CELL_AREF == pntrtype(refpntr)) {
    carray *arr = aref_array(refpntr);
    int index = aref_index(refpntr);
    assert(index < arr->size);
    assert(index+n <= arr->size);

    if (index+n == arr->size)
      argstack[0] = arr->tail;
    else
      make_aref_pntr(argstack[0],arr->wrapper,index+n);
  }
  else {
    set_error(tsk,"arrayskip: expected aref or cons, got %s",cell_types[pntrtype(refpntr)]);
  }
}

static void b_arrayprefix(task *tsk, pntr *argstack)
{
  pntr npntr = argstack[2];
  pntr refpntr = argstack[1];
  pntr restpntr = argstack[0];
  int n;
  carray *prefix;

  CHECK_ARG(2,CELL_NUMBER);
  n = (int)pntrdouble(npntr);

  if (CELL_CONS == pntrtype(refpntr)) {
    cell *newcons;
    assert(1 == n);

    newcons = alloc_cell(tsk);
    newcons->type = CELL_CONS;
    newcons->field1 = get_pntr(refpntr)->field1;
    newcons->field2 = restpntr;

    make_pntr(argstack[0],newcons);
  }
  else if (CELL_AREF == pntrtype(refpntr)) {
    carray *arr = aref_array(refpntr);
    int index = aref_index(refpntr);

    if (0 >= n) {
      argstack[0] = tsk->globnilpntr;
      return;
    }

    if (n > arr->size-index)
      n = arr->size-index;

    prefix = carray_new(tsk,arr->elemsize,n,NULL,NULL);
    prefix->tail = restpntr;
    make_aref_pntr(argstack[0],prefix->wrapper,0);
    carray_append(tsk,&prefix,arr->elements+index*arr->elemsize,n,arr->elemsize);
  }
  else {
    set_error(tsk,"arrayprefix: expected aref or cons, got %s",cell_types[pntrtype(refpntr)]);
  }
}

static void b_arraystrcmp(task *tsk, pntr *argstack)
{
  pntr a = argstack[1];
  pntr b = argstack[0];

  /* If a and b are are both arrays with elemsize=1, and a nil tail, we can do a normal string
     comparison */
  if ((CELL_AREF == pntrtype(a)) && (CELL_AREF == pntrtype(b))) {
    carray *aarr = aref_array(a);
    carray *barr = aref_array(b);
    int aindex = aref_index(a);
    int bindex = aref_index(b);
    aarr->tail = resolve_pntr(aarr->tail);
    barr->tail = resolve_pntr(barr->tail);
    if ((1 == aarr->elemsize) && (1 == barr->elemsize) &&
        (CELL_NIL == pntrtype(aarr->tail)) && (CELL_NIL == pntrtype(barr->tail))) {

      /* Compare character by character until we reach the end of either or both strings */
      while ((aindex < aarr->size) && (bindex < barr->size)) {
        if (aarr->elements[aindex] != barr->elements[bindex]) {
          set_pntrdouble(argstack[0],(double)(aarr->elements[aindex] - barr->elements[bindex]));
          return;
        }
        aindex++;
        bindex++;
      }

      if (aindex < aarr->size)
        set_pntrdouble(argstack[0],1);
      else if (bindex < barr->size)
        set_pntrdouble(argstack[0],-1);
      else
        set_pntrdouble(argstack[0],0);
      return;
    }
  }

  /* Otherwise, have to fall back and treat them as cons lists */
  argstack[0] = tsk->globnilpntr;
}

static void b_numtostring(task *tsk, pntr *argstack)
{
  pntr p = argstack[0];
  char str[100];

  CHECK_ARG(0,CELL_NUMBER);
  format_double(str,100,pntrdouble(p));
  assert(0 < strlen(str));
  argstack[0] = string_to_array(tsk,str);
}

static void b_stringtonum1(task *tsk, pntr *argstack)
{
  pntr p = argstack[0];
  int badtype;
  char *str;
  char *end = NULL;

  if (0 <= (badtype = array_to_string(p,&str))) {
    set_error(tsk,"stringtonum1: argument is not a string (contains non-char: %s)",
              cell_types[badtype]);
    return;
  }

  setnumber(&argstack[0],strtod(str,&end));
  if (('\0' == *str) || ('\0' != *end))
    set_error(tsk,"stringtonum1: \"%s\" is not a valid number",str);

  free(str);
}

static void b_teststring(task *tsk, pntr *argstack)
{
  pntr tail = argstack[0];
  pntr p = string_to_array(tsk,"this part shouldn't be here! test");
  carray *arr = aref_array(p);
  arr->tail = tail;
  make_aref_pntr(argstack[0],arr->wrapper,29);
}

static void b_testarray(task *tsk, pntr *argstack)
{
  pntr content = argstack[1];
  pntr tail = argstack[0];
  carray *arr = carray_new(tsk,sizeof(pntr),0,NULL,NULL);
  pntr p;
  int i;
  make_aref_pntr(p,arr->wrapper,2);
  for (i = 0; i < 5; i++)
    carray_append(tsk,&arr,&content,1,sizeof(pntr));
  arr->tail = tail;
  argstack[0] = p;
}

static void b_openfd(task *tsk, pntr *argstack)
{
  pntr filenamepntr = argstack[0];
  char *filename;
  int fd;
  int badtype;
  sysobject *so;
  cell *c;

  if (0 <= (badtype = array_to_string(filenamepntr,&filename))) {
    set_error(tsk,"openfd: filename is not a string (contains non-char: %s)",cell_types[badtype]);
    return;
  }

  if (0 > (fd = open(filename,O_RDONLY))) {
    set_error(tsk,"%s: %s",filename,strerror(errno));
    free(filename);
    return;
  }

  so = new_sysobject(tsk,SYSOBJECT_FILE,&c);
  so->fd = fd;
  make_pntr(argstack[0],c);

  free(filename);
}

static void b_readchunk(task *tsk, pntr *argstack)
{
  pntr sopntr = argstack[1];
  pntr nextpntr = argstack[0];
  int r;
  char buf[IOSIZE];
  sysobject *so;
  cell *c;
  int doclose = 0;

  CHECK_SYSOBJECT_ARG(1,SYSOBJECT_FILE);
  c = get_pntr(sopntr);
  so = psysobject(sopntr);

  r = read(so->fd,buf,IOSIZE);
  if (0 == r) {
    argstack[0] = tsk->globnilpntr;
    doclose = 1;
  }
  if (0 > r) {
    set_error(tsk,"Error reading file: %s",strerror(errno));
    doclose = 1;
  }

  if (doclose) {
    close(so->fd);
    free(so);
    c->type = CELL_IND;
    c->field1 = tsk->globnilpntr;
    return;
  }

  argstack[0] = data_to_list(tsk,buf,r,nextpntr);
}

static void b_readdir1(task *tsk, pntr *argstack)
{
  DIR *dir;
  char *path;
  pntr filenamepntr = argstack[0];
  struct dirent tmp;
  struct dirent *entry;
  carray *arr = NULL;
  int badtype;
  int count = 0;

  if (0 <= (badtype = array_to_string(filenamepntr,&path))) {
    set_error(tsk,"readdir: path is not a string (contains non-char: %s)",cell_types[badtype]);
    return;
  }

  if (NULL == (dir = opendir(path))) {
    set_error(tsk,"%s: %s",path,strerror(errno));
    free(path);
    return;
  }

  argstack[0] = tsk->globnilpntr;

  while ((0 == readdir_r(dir,&tmp,&entry)) && (NULL != entry)) {
    char *fullpath;
    pntr namep;
    pntr typep;
    pntr sizep;
    pntr entryp;
    carray *entryarr;
    struct stat statbuf;
    char *type;

    if (!strcmp(entry->d_name,".") || !strcmp(entry->d_name,".."))
      continue;

    fullpath = (char*)malloc(strlen(path)+1+strlen(entry->d_name)+1);
    sprintf(fullpath,"%s/%s",path,entry->d_name);
    if (0 == stat(fullpath,&statbuf)) {

      if (S_ISREG(statbuf.st_mode))
        type = "file";
      else if (S_ISDIR(statbuf.st_mode))
        type = "directory";
      else
        type = "other";

      namep = string_to_array(tsk,entry->d_name);
      typep = string_to_array(tsk,type);
      set_pntrdouble(sizep,statbuf.st_size);

      entryarr = carray_new(tsk,sizeof(pntr),0,NULL,NULL);
      carray_append(tsk,&entryarr,&namep,1,sizeof(pntr));
      carray_append(tsk,&entryarr,&typep,1,sizeof(pntr));
      carray_append(tsk,&entryarr,&sizep,1,sizeof(pntr));
      make_aref_pntr(entryp,entryarr->wrapper,0);

      if (0 == count) {
        arr = carray_new(tsk,sizeof(pntr),0,NULL,NULL);
        make_aref_pntr(argstack[0],arr->wrapper,0);
      }

      carray_append(tsk,&arr,&entryp,1,sizeof(pntr));
      count++;
    }

    free(fullpath);
  }
  closedir(dir);

  free(path);
}

static void b_fexists(task *tsk, pntr *argstack)
{
  pntr pathpntr = argstack[0];
  char *path;
  int badtype;
  int s;
  struct stat statbuf;

  if (0 <= (badtype = array_to_string(pathpntr,&path))) {
    set_error(tsk,"fexists: filename is not a string (contains non-char: %s)",cell_types[badtype]);
    return;
  }

  s = stat(path,&statbuf);
  if (0 == s) {
    argstack[0] = tsk->globtruepntr;
  }
  else if (ENOENT == errno) {
    argstack[0] = tsk->globnilpntr;
  }
  else {
    set_error(tsk,"%s: %s",path,strerror(errno));
  }

  free(path);
}

static void b_fisdir(task *tsk, pntr *argstack)
{
  pntr pathpntr = argstack[0];
  char *path;
  int badtype;
  struct stat statbuf;

  if (0 <= (badtype = array_to_string(pathpntr,&path))) {
    set_error(tsk,"fisdir: filename is not a string (contains non-char: %s)",cell_types[badtype]);
    return;
  }

  if (0 > stat(path,&statbuf)) {
    set_error(tsk,"stat(%s): %s",path,strerror(errno));
  }
  else if (S_ISDIR(statbuf.st_mode)) {
    argstack[0] = tsk->globtruepntr;
  }
  else {
    argstack[0] = tsk->globnilpntr;
  }

  free(path);
}

static int suspend_current_frame(task *tsk, frame *f)
{
  int count = tsk->iocount;
  int ioid;
  assert(SAFE_TO_ACCESS_TASK(tsk));
  assert(f == *tsk->runptr);

  /* Allocate ioid */
  if (0 >= tsk->iofree) {
    ioid = count;

    if (count == tsk->ioalloc) {
      tsk->ioalloc *= 2;
      tsk->ioframes = (ioframe*)realloc(tsk->ioframes,tsk->ioalloc*sizeof(ioframe));
    }

    tsk->iocount++;

    tsk->ioframes[ioid].f = f;
    tsk->ioframes[ioid].freelnk = 0;
  }
  else {
    ioid = tsk->iofree;
    assert(ioid < count);
    assert(NULL == tsk->ioframes[ioid].f);
    tsk->iofree = tsk->ioframes[ioid].freelnk;

    tsk->ioframes[ioid].f = f;
    tsk->ioframes[ioid].freelnk = 0;
  }

  /* Suspend frame */
  tsk->netpending++;
  f->resume = 1;
  f->instr--;
  block_frame(tsk,f);
  check_runnable(tsk);

  return ioid;
}

static void b_opencon(task *tsk, pntr *argstack)
{
  frame *curf = *tsk->runptr;
  if (0 == curf->resume) {
    pntr hostnamepntr = argstack[2];
    pntr portpntr = argstack[1];
    int port;
    int badtype;
    char *hostname;
    sysobject *so;
    cell *c;
    connect_msg cm;

    CHECK_ARG(1,CELL_NUMBER);
    port = (int)pntrdouble(portpntr);

    if (0 <= (badtype = array_to_string(hostnamepntr,&hostname))) {
      set_error(tsk,"opencon: hostname is not a string (contains non-char: %s)",
                cell_types[badtype]);
      return;
    }

    /* Create sysobject cell */
    so = new_sysobject(tsk,SYSOBJECT_CONNECTION,&c);
    so->hostname = strdup(hostname);
    so->port = port;
    so->len = 0;

    make_pntr(argstack[2],c);

    node_log(tsk->n,LOG_DEBUG1,"opencon %s:%d: Initiated connection",hostname,port);

    snprintf(cm.hostname,HOSTNAME_MAX,"%s",hostname);
    cm.hostname[HOSTNAME_MAX] = '\0';
    cm.port = port;
    cm.owner = tsk->endpt->epid;
    cm.ioid = suspend_current_frame(tsk,*tsk->runptr);
    so->frameids[CONNECT_FRAMEADDR] = cm.ioid;
    node_send(tsk->n,tsk->endpt->epid.localid,tsk->n->managerid,MSG_CONNECT,&cm,sizeof(cm));

    free(hostname);
  }
  else {
    sysobject *so;
    cell *c;

    assert(1 == curf->resume);
    assert(CELL_SYSOBJECT == pntrtype(argstack[2]));
    c = get_pntr(argstack[2]);
    so = (sysobject*)get_pntr(c->field1);
    assert(SYSOBJECT_CONNECTION == so->type);
    curf->resume = 0;

    if (!so->connected) {
      node_log(tsk->n,LOG_DEBUG1,"opencon %s:%d: Connection failed",so->hostname,so->port);
      set_error(tsk,"%s:%d: %s",so->hostname,so->port,so->errmsg);
      return;
    }
    else {
      pntr printer;
      node_log(tsk->n,LOG_DEBUG1,"opencon %s:%d: Connection successful",so->hostname,so->port);

      /* Start printing output to the connection */
      printer = resolve_pntr(argstack[0]);
      node_log(tsk->n,LOG_DEBUG2,"opencon %s:%d: printer is %s",
               so->hostname,so->port,cell_types[pntrtype(printer)]);
      if (CELL_FRAME == pntrtype(printer))
        run_frame(tsk,pframe(printer));

      /* Return the sysobject */
      argstack[0] = argstack[2];
    }
  }
}

/*

Readcon process

Normal operation

1. b_readcon() called; curf->resume will be 0 since it has not yet blocked
2. Task sends a READ message to manager and blocks frame (setting curf->resume = 1)
3. Manager records the ioid of the frame and enables reading on the connection
4. Data arrives and is read from connection
5. Worker sends READ_RESPONSE message to task
6. Task handles READ_RESPONSE message, stores data in so->buf, and unblocks frame
7. b_readcon() invoked again with curf->resume == 1, returns data to caller

Error occurs during read
1-3: as above
4. Connection has error or is closed by peer
5. Workers sends CONNECTION_EVENT message with event either CONN_IOERROR or CONN_CLOSED
6. Task handles CONNECTION_EVENT, marks so as closed, sets error = 1 or 0, unblocks frame
7. b_readcon() invoked again with curf->resume == 1, reports error or close condition to caller

Error occurs before call to read
1. Workers sends CONNECTION_EVENT message with event either CONN_IOERROR or CONN_CLOSED
2. Task handles CONNECTION_EVENT, marks so as closed, sets error = 1 or 0,
3. b_readcon() called, detects so->closed, reports error or close condition to caller

*/

static void b_readcon(task *tsk, pntr *argstack)
{
  frame *curf = *tsk->runptr;
  pntr sopntr = argstack[1];
  pntr nextpntr = argstack[0];
  sysobject *so;

  CHECK_SYSOBJECT_ARG(1,SYSOBJECT_CONNECTION);
  so = psysobject(sopntr);
  assert(so->connected);

  if (so->closed) {
    if (so->error) {
      node_log(tsk->n,LOG_DEBUG1,"readcon %s:%d: %s",so->hostname,so->port,so->errmsg);
      set_error(tsk,"readcon %s:%d: %s",so->hostname,so->port,so->errmsg);
    }
    else {
      node_log(tsk->n,LOG_DEBUG1,"readcon %s:%d: Connection is closed",so->hostname,so->port);
      argstack[0] = tsk->globnilpntr;
    }
    curf->resume = 0;
    return;
  }

  if (0 == curf->resume) {
    read_msg rm;
    rm.sockid = so->sockid;
    rm.ioid = suspend_current_frame(tsk,*tsk->runptr);
    assert(0 == so->frameids[READ_FRAMEADDR]);
    so->frameids[READ_FRAMEADDR] = rm.ioid;
    node_send(tsk->n,tsk->endpt->epid.localid,so->sockid.managerid,MSG_READ,&rm,sizeof(rm));
    node_log(tsk->n,LOG_DEBUG1,"readcon %s:%d: Waiting for data",so->hostname,so->port);
  }
  else {
    curf->resume = 0;
    if (0 == so->len) {
      node_log(tsk->n,LOG_DEBUG1,"readcon %s:%d: Finished reading",so->hostname,so->port);
      argstack[0] = tsk->globnilpntr;
    }
    else {
      carray *arr = carray_new(tsk,1,so->len,NULL,NULL);
      make_aref_pntr(argstack[0],arr->wrapper,0);
      carray_append(tsk,&arr,so->buf,so->len,1);
      arr->tail = nextpntr;
      node_log(tsk->n,LOG_DEBUG1,"readcon %s:%d: Read %d bytes",so->hostname,so->port,so->len);
      free(so->buf);
      so->buf = NULL;
      so->len = 0;
    }
  }
}

/* FIXME: remove the IP address parameter to listen... it's a bit pointless for our case */
static void b_startlisten(task *tsk, pntr *argstack)
{
  frame *curf = *tsk->runptr;

  if (0 == curf->resume) {
    pntr hostnamepntr = argstack[1];
    pntr portpntr = argstack[0];
    char *hostname;
    int port;
    int badtype;
    sysobject *so;
    cell *c;
    in_addr_t ip = INADDR_ANY;
    listen_msg lm;

    CHECK_ARG(0,CELL_NUMBER);
    port = (int)pntrdouble(portpntr);

    if (0 <= (badtype = array_to_string(hostnamepntr,&hostname))) {
      set_error(tsk,"startlisten: hostname is not a string (contains non-char: %s)",
                cell_types[badtype]);
      return;
    }

    if (0 > lookup_address(tsk->n,hostname,&ip,NULL)) {
      set_error(tsk,"startlisten: could not resolve hostname %s",hostname);
      free(hostname);
      return;
    }

    /* Create sysobject cell */
    so = new_sysobject(tsk,SYSOBJECT_LISTENER,&c);
    so->hostname = strdup(hostname);
    so->port = port;
    so->tsk = tsk;

    make_pntr(argstack[0],c);
    make_pntr(argstack[1],c);

    lm.ip = ip;
    lm.port = port;
    lm.owner = tsk->endpt->epid;
    lm.ioid = suspend_current_frame(tsk,curf);
    so->frameids[LISTEN_FRAMEADDR] = lm.ioid;

    node_send(tsk->n,tsk->endpt->epid.localid,tsk->n->managerid,MSG_LISTEN,&lm,sizeof(lm));

    free(hostname);
  }
  else {
    sysobject *so;
    CHECK_SYSOBJECT_ARG(1,SYSOBJECT_LISTENER);
    so = psysobject(argstack[1]);
    curf->resume = 0;
    if (so->error) {
      set_error(tsk,"startlisten: %s",so->errmsg);
      return;
    }
  }
}

static void b_accept(task *tsk, pntr *argstack)
{
  frame *curf = *tsk->runptr;
  sysobject *so;
  cell *c;
  pntr sopntr = argstack[1];

  CHECK_SYSOBJECT_ARG(1,SYSOBJECT_LISTENER);
  c = get_pntr(sopntr);
  so = psysobject(sopntr);

  if (0 == curf->resume) {
    accept_msg am;
    am.sockid = so->sockid;
    am.ioid = suspend_current_frame(tsk,curf);
    assert(0 == so->frameids[ACCEPT_FRAMEADDR]);
    so->frameids[ACCEPT_FRAMEADDR] = am.ioid;
    node_send(tsk->n,tsk->endpt->epid.localid,tsk->n->managerid,MSG_ACCEPT,&am,sizeof(am));
  }
  else {
    curf->resume = 0;

    pntr printer;
    sysobject *newso = so->newso;
    assert(newso);
    so->newso = NULL;
    node_log(tsk->n,LOG_DEBUG1,"accept %s:%d: Got new connection",so->hostname,so->port);

    /* Start printing output to the connection */
    printer = resolve_pntr(argstack[0]);
    node_log(tsk->n,LOG_DEBUG2,"accept %s:%d: printer is %s",
             so->hostname,so->port,cell_types[pntrtype(printer)]);
    if (CELL_FRAME == pntrtype(printer))
      run_frame(tsk,pframe(printer));

    /* Return the sysobject */
    make_pntr(argstack[0],newso->c);
  }
}

static void b_nchars(task *tsk, pntr *argstack)
{
  pntr p = argstack[0];
  if ((CELL_AREF == pntrtype(p)) && (1 == aref_array(p)->elemsize))
    set_pntrdouble(argstack[0],aref_array(p)->size - aref_index(p));
  else
    argstack[0] = tsk->globnilpntr;
}

static void write_data(task *tsk, pntr *argstack, const char *data, int len, pntr destpntr)
{
  frame *curf = *tsk->runptr;
  cell *c;
  sysobject *so;

  if (CELL_NIL == pntrtype(destpntr)) {
    /* Write to task output file handle */
    if (tsk->output) {
      fwrite(data,1,len,tsk->output);
      fflush(tsk->output);
    }
    argstack[0] = tsk->globnilpntr;
    return;
  }

  c = (cell*)get_pntr(destpntr);
  so = (sysobject*)get_pntr(c->field1);
  if (SYSOBJECT_CONNECTION != so->type) {
    set_error(tsk,"write_data: first arg must be CONNECTION, got %s",sysobject_types[so->type]);
    return;
  }
  assert(so->connected);

  if (so->closed) {
    if (so->error)
      set_error(tsk,"writecon %s:%d: %s",so->hostname,so->port,so->errmsg);
    else
      set_error(tsk,"writecon %s:%d: Connection is closed",so->hostname,so->port);
    curf->resume = 0;
    return;
  }

  if (curf->resume) {
    argstack[0] = tsk->globnilpntr; /* normal return */
    curf->resume = 0;
  }
  else {
    int msglen = sizeof(write_msg)+len;
    write_msg *wm = (write_msg*)malloc(msglen);
    wm->sockid = so->sockid;
    wm->ioid = suspend_current_frame(tsk,curf);
    wm->len = len;
    assert(0 == so->frameids[WRITE_FRAMEADDR]);
    so->frameids[WRITE_FRAMEADDR] = wm->ioid;
    memcpy(wm->data,data,len);
    node_send(tsk->n,tsk->endpt->epid.localid,so->sockid.managerid,MSG_WRITE,wm,msglen);
    free(wm);
  }
}

static void b_print(task *tsk, pntr *argstack)
{
  pntr destpntr = argstack[1];
  double d;
  char c;

  CHECK_ARG(0,CELL_NUMBER);
  if (CELL_NIL != pntrtype(destpntr))
    CHECK_SYSOBJECT_ARG(1,SYSOBJECT_CONNECTION);

  d = pntrdouble(argstack[0]);
  if ((d == floor(d)) && (0 < d) && (128 > d))
    c = (int)d;
  else
    c = '?';
  write_data(tsk,argstack,&c,1,destpntr);
}

static void b_printarray(task *tsk, pntr *argstack)
{
  carray *arr;
  int index;
  int n;
  pntr destpntr = argstack[2];
  pntr npntr = argstack[1];
  pntr valpntr = argstack[0];

  CHECK_ARG(0,CELL_AREF);
  CHECK_ARG(1,CELL_NUMBER);
  if (CELL_NIL != pntrtype(destpntr))
    CHECK_SYSOBJECT_ARG(2,SYSOBJECT_CONNECTION);

  n = (int)pntrdouble(npntr);

  if ((CELL_AREF != pntrtype(valpntr)) && (CELL_CONS != pntrtype(valpntr))) {
    set_error(tsk,"printarray: expected aref or cons, got %s",cell_types[pntrtype(valpntr)]);
    return;
  }

  arr = aref_array(valpntr);
  index = aref_index(valpntr);

  assert(1 <= n);
  assert(index+n <= arr->size);
  assert(1 == arr->elemsize);

  write_data(tsk,argstack,arr->elements+index,n,destpntr);
}

static void b_printend(task *tsk, pntr *argstack)
{
  frame *curf = *tsk->runptr;
  if (CELL_NIL == pntrtype(argstack[0]))
    return;
  if (0 == curf->resume) {
    sysobject *so;
    finwrite_msg fwm;

    CHECK_SYSOBJECT_ARG(0,SYSOBJECT_CONNECTION);
    so = psysobject(argstack[0]);
    assert(so->connected);

    fwm.sockid = so->sockid;
    fwm.ioid = suspend_current_frame(tsk,curf);
    assert(0 == so->frameids[FINWRITE_FRAMEADDR]);
    so->frameids[FINWRITE_FRAMEADDR] = fwm.ioid;

    node_send(tsk->n,tsk->endpt->epid.localid,so->sockid.managerid,MSG_FINWRITE,&fwm,sizeof(fwm));
  }
  else {
    curf->resume = 0;
    argstack[0] = tsk->globnilpntr;
  }
}

static void b_echo1(task *tsk, pntr *argstack)
{
  char *str;
  int badtype;

  if (0 <= (badtype = array_to_string(argstack[0],&str))) {
    set_error(tsk,"echo: argument is not a string (contains non-char: %s)",cell_types[badtype]);
    return;
  }

  fprintf(tsk->output,"%s",str);
  free(str);
  argstack[0] = tsk->globnilpntr;
}

static void b_error1(task *tsk, pntr *argstack)
{
  pntr p = argstack[0];
  int badtype;
  char *str;

  if (0 <= (badtype = array_to_string(p,&str))) {
    set_error(tsk,"error1: argument is not a string (contains non-char: %s)",cell_types[badtype]);
    return;
  }

  set_error(tsk,"%s",str);
  free(str);
  return;
}

static void b_getoutput(task *tsk, pntr *argstack)
{
  if (tsk->out_so)
    argstack[0] = tsk->out_so->p;
  else
    argstack[0] = tsk->globnilpntr;
}

int get_builtin(const char *name)
{
  int i;
  for (i = 0; i < NUM_BUILTINS; i++)
    if (!strcmp(name,builtin_info[i].name))
      return i;
  return -1;
}

const builtin builtin_info[NUM_BUILTINS] = {
/* Arithmetic operations */
{ "+",              2, 2, ALWAYS_VALUE, ALWAYS_TRUE,   PURE, b_add            },
{ "-",              2, 2, ALWAYS_VALUE, ALWAYS_TRUE,   PURE, b_subtract       },
{ "*",              2, 2, ALWAYS_VALUE, ALWAYS_TRUE,   PURE, b_multiply       },
{ "/",              2, 2, ALWAYS_VALUE, ALWAYS_TRUE,   PURE, b_divide         },
{ "%",              2, 2, ALWAYS_VALUE, ALWAYS_TRUE,   PURE, b_mod            },

/* Numeric comparison */
{ "=",              2, 2, ALWAYS_VALUE, MAYBE_FALSE,   PURE, b_eq             },
{ "!=",             2, 2, ALWAYS_VALUE, MAYBE_FALSE,   PURE, b_ne             },
{ "<",              2, 2, ALWAYS_VALUE, MAYBE_FALSE,   PURE, b_lt             },
{ "<=",             2, 2, ALWAYS_VALUE, MAYBE_FALSE,   PURE, b_le             },
{ ">",              2, 2, ALWAYS_VALUE, MAYBE_FALSE,   PURE, b_gt             },
{ ">=",             2, 2, ALWAYS_VALUE, MAYBE_FALSE,   PURE, b_ge             },

/* Bit operations */
{ "<<",             2, 2, ALWAYS_VALUE, ALWAYS_TRUE,   PURE, b_bitshl         },
{ ">>",             2, 2, ALWAYS_VALUE, ALWAYS_TRUE,   PURE, b_bitshr         },
{ "&",              2, 2, ALWAYS_VALUE, ALWAYS_TRUE,   PURE, b_bitand         },
{ "|",              2, 2, ALWAYS_VALUE, ALWAYS_TRUE,   PURE, b_bitor          },
{ "^",              2, 2, ALWAYS_VALUE, ALWAYS_TRUE,   PURE, b_bitxor         },
{ "~",              1, 1, ALWAYS_VALUE, ALWAYS_TRUE,   PURE, b_bitnot         },

/* Numeric functions */
{ "sqrt",           1, 1, ALWAYS_VALUE, ALWAYS_TRUE,   PURE, b_sqrt           },
{ "floor",          1, 1, ALWAYS_VALUE, ALWAYS_TRUE,   PURE, b_floor          },
{ "ceil",           1, 1, ALWAYS_VALUE, ALWAYS_TRUE,   PURE, b_ceil           },
{ "numtostring",    1, 1, ALWAYS_VALUE, ALWAYS_TRUE,   PURE, b_numtostring    },
{ "stringtonum1",   1, 1, ALWAYS_VALUE, ALWAYS_TRUE,   PURE, b_stringtonum1   },

/* Logical operations */
{ "if",             3, 1, MAYBE_UNEVAL, MAYBE_FALSE,   PURE, b_if             },
{ "and",            2, 2, ALWAYS_VALUE, MAYBE_FALSE,   PURE, b_and            },
{ "or",             2, 2, ALWAYS_VALUE, MAYBE_FALSE,   PURE, b_or             },
{ "not",            1, 1, ALWAYS_VALUE, MAYBE_FALSE,   PURE, b_not            },

/* Lists and arrays */
{ "cons",           2, 0, ALWAYS_VALUE, ALWAYS_TRUE,   PURE, b_cons           },
{ "head",           1, 1, MAYBE_UNEVAL, MAYBE_FALSE,   PURE, b_head           },
{ "tail",           1, 1, MAYBE_UNEVAL, MAYBE_FALSE,   PURE, b_tail           },
{ "arraysize",      1, 1, MAYBE_UNEVAL, ALWAYS_TRUE,   PURE, b_arraysize      },
{ "arrayskip",      2, 2, MAYBE_UNEVAL, MAYBE_FALSE,   PURE, b_arrayskip      },
{ "arrayprefix",    3, 2, MAYBE_UNEVAL, MAYBE_FALSE,   PURE, b_arrayprefix    },
{ "arraystrcmp",    2, 2, ALWAYS_VALUE, MAYBE_FALSE,   PURE, b_arraystrcmp    },
{ "teststring",     1, 0, ALWAYS_VALUE, ALWAYS_TRUE,   PURE, b_teststring     },
{ "testarray",      2, 0, ALWAYS_VALUE, ALWAYS_TRUE,   PURE, b_testarray      },

/* Sequential/parallel directives */
{ "seq",            2, 1, MAYBE_UNEVAL, MAYBE_FALSE, IMPURE, b_seq            },
{ "par",            2, 0, MAYBE_UNEVAL, MAYBE_FALSE, IMPURE, b_par            },
{ "parhead",        2, 1, MAYBE_UNEVAL, MAYBE_FALSE, IMPURE, b_parhead        },

/* Filesystem access */
{ "openfd",         1, 1, MAYBE_UNEVAL, MAYBE_FALSE, IMPURE, b_openfd         },
{ "readchunk",      2, 1, MAYBE_UNEVAL, MAYBE_FALSE, IMPURE, b_readchunk      },
{ "readdir1",       1, 1, MAYBE_UNEVAL, MAYBE_FALSE, IMPURE, b_readdir1       },
{ "fexists",        1, 1, ALWAYS_VALUE, MAYBE_FALSE, IMPURE, b_fexists        },
{ "fisdir",         1, 1, ALWAYS_VALUE, MAYBE_FALSE, IMPURE, b_fisdir         },

/* Networking */
{ "opencon",        3, 2, ALWAYS_VALUE, MAYBE_FALSE, IMPURE, b_opencon        },
{ "readcon",        2, 1, ALWAYS_VALUE, MAYBE_FALSE, IMPURE, b_readcon        },
{ "startlisten",    2, 2, ALWAYS_VALUE, MAYBE_FALSE, IMPURE, b_startlisten    },
{ "accept",         2, 1, ALWAYS_VALUE, MAYBE_FALSE, IMPURE, b_accept         },

/* Terminal and network output */
{ "nchars",         1, 1, ALWAYS_VALUE, MAYBE_FALSE,   PURE, b_nchars         },
{ "print",          2, 2, ALWAYS_VALUE, MAYBE_FALSE, IMPURE, b_print          },
{ "printarray",     3, 3, ALWAYS_VALUE, MAYBE_FALSE, IMPURE, b_printarray     },
{ "printend",       1, 1, ALWAYS_VALUE, MAYBE_FALSE, IMPURE, b_printend       },
{ "echo1",          1, 1, ALWAYS_VALUE, MAYBE_FALSE, IMPURE, b_echo1          },

/* Other */
{ "error1",         1, 1, ALWAYS_VALUE, MAYBE_FALSE, IMPURE, b_error1         },
{ "getoutput",      1, 1, ALWAYS_VALUE, MAYBE_FALSE, IMPURE, b_getoutput      },

};

