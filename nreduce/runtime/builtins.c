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
#include "network/util.h"
#include "runtime.h"
#include "network/node.h"
#include "messages.h"
#include "cxslt/xslt.h"
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
#include <arpa/inet.h>

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

builtin builtin_info[NUM_BUILTINS];

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
  default:        fatal("Invalid bit operation %d",bif);     break;
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
  }
}

static void b_head(task *tsk, pntr *argstack);
static void b_parhead(task *tsk, pntr *argstack)
{
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
  unsigned char *charp = (unsigned char*)arr->elements;
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
  if ((tsk->alloc_bytes >= COLLECT_THRESHOLD) && tsk->endpt)
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

    memmove(((unsigned char*)(*arr)->elements)+(*arr)->size*(*arr)->elemsize,
            data,count*(*arr)->elemsize);
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
  int ret;

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
        array_append(buf,&((unsigned char*)arr->elements)[index],arr->size-index);
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
    ret = -1;
  }
  else {
    ret = buf->nbytes;
    array_append(buf,&zero,1);
    *str = buf->data;
  }

  free(buf);
  return ret;
}

int flatten_list(pntr refpntr, pntr **data)
{
  pntr p = refpntr;
  array *buf = array_new(sizeof(pntr),0);
  int badtype = -1;

  *data = NULL;

  while (0 > badtype) {

    if (CELL_CONS == pntrtype(p)) {
      cell *c = get_pntr(p);
      pntr head = resolve_pntr(c->field1);
      pntr tail = resolve_pntr(c->field2);
      array_append(buf,&head,sizeof(pntr));
      p = tail;
    }
    else if (CELL_AREF == pntrtype(p)) {
      carray *arr = aref_array(p);
      int index = aref_index(p);
      int i;

      if (1 == arr->elemsize) {
        for (i = index; i < arr->size; i++) {
          double d = (double)(((unsigned char*)arr->elements)[i]);
          pntr p;
          set_pntrdouble(p,d);
          array_append(buf,&p,sizeof(pntr));
        }
      }
      else {
        pntr *pelements = (pntr*)arr->elements;
        for (i = index; i < arr->size; i++)
          pelements[i] = resolve_pntr(pelements[i]);
        array_append(buf,&pelements[index],(arr->size-index)*sizeof(pntr));
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
    *data = NULL;
    array_free(buf);
    return -1;
  }
  else {
    int size = array_count(buf);
    *data = (pntr*)buf->data;
    free(buf);
    return size;
  }
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
      set_pntrdouble(argstack[0],(double)(((unsigned char*)arr->elements)[index]));
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

static void b_stringtonum(task *tsk, pntr *argstack)
{
  pntr p = argstack[0];
  char *str;
  char *end = NULL;

  if (0 > array_to_string(p,&str)) {
    set_error(tsk,"stringtonum: argument is not a string");
    return;
  }

  setnumber(&argstack[0],strtod(str,&end));
  if (('\0' == *str) || ('\0' != *end))
    set_error(tsk,"stringtonum: \"%s\" is not a valid number",str);

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
  sysobject *so;

  if (0 > array_to_string(filenamepntr,&filename)) {
    set_error(tsk,"openfd: filename is not a string");
    return;
  }

  if (0 > (fd = open(filename,O_RDONLY))) {
    set_error(tsk,"%s: %s",filename,strerror(errno));
    free(filename);
    return;
  }

  so = new_sysobject(tsk,SYSOBJECT_FILE);
  so->fd = fd;
  make_pntr(argstack[0],so->c);

  free(filename);
}

static void b_readchunk(task *tsk, pntr *argstack)
{
  pntr sopntr = argstack[1];
  pntr nextpntr = argstack[0];
  int r;
  char buf[DEFAULT_IOSIZE];
  sysobject *so;
  cell *c;
  int doclose = 0;

  CHECK_SYSOBJECT_ARG(1,SYSOBJECT_FILE);
  c = get_pntr(sopntr);
  so = psysobject(sopntr);

  /* FIXME: add migration check here? */
  if (so->ownertid != tsk->tid) {
    fatal("readchunk: should migrate to task %d",so->ownertid);
  }

  r = read(so->fd,buf,DEFAULT_IOSIZE);
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

static void b_readdir(task *tsk, pntr *argstack)
{
  DIR *dir;
  char *path;
  pntr filenamepntr = argstack[0];
  struct dirent tmp;
  struct dirent *entry;
  carray *arr = NULL;
  int count = 0;

  if (0 > array_to_string(filenamepntr,&path)) {
    set_error(tsk,"readdir: path is not a string");
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

static void b_exists(task *tsk, pntr *argstack)
{
  pntr pathpntr = argstack[0];
  char *path;
  int s;
  struct stat statbuf;

  if (0 > array_to_string(pathpntr,&path)) {
    set_error(tsk,"exists: filename is not a string");
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

static void b_isdir(task *tsk, pntr *argstack)
{
  pntr pathpntr = argstack[0];
  char *path;
  struct stat statbuf;

  if (0 > array_to_string(pathpntr,&path)) {
    set_error(tsk,"isdir: filename is not a string");
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

static void migrate_to(task *tsk, int desttsk)
{
  frame *curf = *tsk->runptr;
  array *newmsg;

  curf->instr--;
  done_frame(tsk,curf);
  check_runnable(tsk);

  node_log(tsk->n,LOG_DEBUG1,"send(%d->%d) SCHEDULE (migration: %s)",tsk->tid,desttsk,
           bc_function_name(tsk->bcdata,frame_fno(tsk,curf)));

  newmsg = write_start();
  schedule_frame(tsk,curf,desttsk,newmsg);
  msg_send(tsk,desttsk,MSG_SCHEDULE,newmsg->data,newmsg->nbytes);
  write_end(newmsg);
}

static void b_connect(task *tsk, pntr *argstack)
{
  frame *curf = *tsk->runptr;
  if (0 == curf->resume) {
    pntr hostnamepntr = argstack[2];
    pntr portpntr = argstack[1];
    int port;
    char *hostname;
    sysobject *so;
    int ioid;
    in_addr_t ip;
    struct in_addr ipt;

    CHECK_ARG(1,CELL_NUMBER);
    port = (int)pntrdouble(portpntr);

    if (0 > array_to_string(hostnamepntr,&hostname)) {
      set_error(tsk,"connect: hostname is not a string");
      return;
    }
    if (1 != inet_aton(hostname,&ipt)) {
      set_error(tsk,"connect: invalid IP %s",hostname);
      free(hostname);
      return;
    }
    ip = ipt.s_addr;

    char ipstr[100];
    snprintf(ipstr,100,IP_FORMAT,IP_ARGS(tsk->endpt->epid.ip));

    int i;
    int found = -1;
    for (i = 0; i < tsk->groupsize; i++) {
      if (ip == tsk->idmap[i].ip) {
        found = i;
      }
    }
    if ((0 <= found) && (found != tsk->tid)) {
      migrate_to(tsk,found);
      free(hostname);
      return;
    }

    /* Create sysobject cell */
    so = new_sysobject(tsk,SYSOBJECT_CONNECTION);
    so->hostname = strdup(hostname);
    so->port = port;
    so->len = 0;

    gettimeofday(&so->start,NULL);
    so->local = !strcmp(hostname,"127.0.0.1");
    if (so->local) {
      assert(0 <= so->tsk->svcbusy);
      tsk->svcbusy++;
    }

    make_pntr(argstack[2],so->c);

    node_log(tsk->n,LOG_DEBUG1,"%d: CONNECT1 (%s:%d) svcbusy = %d",
             tsk->tid,hostname,port,tsk->svcbusy);

    ioid = suspend_current_frame(tsk,*tsk->runptr);
    send_connect(tsk->endpt,tsk->n->iothid,ip,port,tsk->endpt->epid,ioid);
    so->frameids[CONNECT_FRAMEADDR] = ioid;

    free(hostname);
  }
  else {
    sysobject *so;
    cell *c;

    assert(1 == curf->resume);
    assert(CELL_SYSOBJECT == pntrtype(argstack[2]));
    c = get_pntr(argstack[2]);
    so = (sysobject*)get_pntr(c->field1);
    assert(so->ownertid == tsk->tid);
    assert(SYSOBJECT_CONNECTION == so->type);
    curf->resume = 0;

    if (!so->connected) {
      node_log(tsk->n,LOG_DEBUG1,"%d: CONNECT2 (%s:%d) failed",tsk->tid,so->hostname,so->port);
      set_error(tsk,"%s:%d: %s",so->hostname,so->port,so->errmsg);
      sysobject_done_reading(so);
      sysobject_done_writing(so);
      return;
    }
    else {
      pntr printer;
      node_log(tsk->n,LOG_DEBUG1,"%d: CONNECT2 (%s:%d) connected",tsk->tid,so->hostname,so->port);

      /* Start printing output to the connection */
      printer = resolve_pntr(argstack[0]);
      if (CELL_FRAME == pntrtype(printer)) {
        run_frame(tsk,pframe(printer));
      }
      else if (CELL_REMOTEREF == pntrtype(printer)) {
        /* Send a FETCH request for the printer, so it will migrate here and start running,
           but don't need to block the current frame since they're supposed to run concurrently */
        global *target = (global*)get_pntr(get_pntr(printer)->field1);
        assert(target->addr.tid != tsk->tid);

        gaddr storeaddr = get_physical_address(tsk,printer);
        assert(storeaddr.tid == tsk->tid);
        node_log(tsk->n,LOG_DEBUG1,"send(%d->%d) FETCH targetaddr=%d@%d storeaddr=%d@%d (connect)",
                 tsk->tid,target->addr.tid,
                 target->addr.lid,target->addr.tid,storeaddr.lid,storeaddr.tid);
        msg_fsend(tsk,target->addr.tid,MSG_FETCH,"aa",target->addr,storeaddr);
        assert(0 <= tsk->nfetching);
        tsk->nfetching++;
        target->fetching = 1;
      }

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
5. Workers sends CONNECTION_CLOSED message with event either CONN_IOERROR or CONN_CLOSED
6. Task handles CONNECTION_CLOSED, marks so as closed, sets error = 1 or 0, unblocks frame
7. b_readcon() invoked again with curf->resume == 1, reports error or close condition to caller

Error occurs before call to read
1. Workers sends CONNECTION_CLOSED message with event either CONN_IOERROR or CONN_CLOSED
2. Task handles CONNECTION_CLOSED, marks so as closed, sets error = 1 or 0,
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

  /* Migration check */
  if (so->ownertid != tsk->tid) {
    migrate_to(tsk,so->ownertid);
    return;
  }

  assert(so->connected);

  if (so->closed) {
    if (so->error)
      set_error(tsk,"readcon %s:%d: %s",so->hostname,so->port,so->errmsg);
    else
      argstack[0] = tsk->globnilpntr;
    sysobject_done_reading(so);
    curf->resume = 0;
    return;
  }

  if (0 == curf->resume) {
    int ioid = suspend_current_frame(tsk,*tsk->runptr);
    send_read(tsk->endpt,so->sockid,ioid);

    assert(0 == so->frameids[READ_FRAMEADDR]);
    so->frameids[READ_FRAMEADDR] = ioid;
  }
  else {
    curf->resume = 0;
    if (0 == so->len) {
      argstack[0] = tsk->globnilpntr;
      sysobject_done_reading(so);
    }
    else {
      carray *arr = carray_new(tsk,1,so->len,NULL,NULL);
      make_aref_pntr(argstack[0],arr->wrapper,0);
      carray_append(tsk,&arr,so->buf,so->len,1);
      arr->tail = nextpntr;
      free(so->buf);
      so->buf = NULL;
      so->len = 0;
    }
  }
}

static void b_listen(task *tsk, pntr *argstack)
{
  frame *curf = *tsk->runptr;

  if (0 == curf->resume) {
    pntr portpntr = argstack[0];
    int port;
    sysobject *so;
    in_addr_t ip = INADDR_ANY;
    int ioid;

    CHECK_ARG(0,CELL_NUMBER);
    port = (int)pntrdouble(portpntr);

    /* Create sysobject cell */
    so = new_sysobject(tsk,SYSOBJECT_LISTENER);
    so->hostname = strdup("0.0.0.0");
    so->port = port;

    make_pntr(argstack[0],so->c);

    ioid = suspend_current_frame(tsk,curf);
    send_listen(tsk->endpt,tsk->n->iothid,ip,port,tsk->endpt->epid,ioid);
    so->frameids[LISTEN_FRAMEADDR] = ioid;

  }
  else {
    sysobject *so;
    CHECK_SYSOBJECT_ARG(0,SYSOBJECT_LISTENER);
    so = psysobject(argstack[0]);
    assert(so->ownertid == tsk->tid);
    curf->resume = 0;
    if (so->error) {
      set_error(tsk,"listen: %s",so->errmsg);
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

  /* FIXME: add migration check here? */
  if (so->ownertid != tsk->tid) {
    fatal("accept: should migrate to task %d",so->ownertid);
  }

  if (0 == curf->resume) {
    int ioid = suspend_current_frame(tsk,curf);
    send_accept(tsk->endpt,so->sockid,ioid);
    assert(0 == so->frameids[ACCEPT_FRAMEADDR]);
    so->frameids[ACCEPT_FRAMEADDR] = ioid;
  }
  else {
    curf->resume = 0;

    pntr printer;
    sysobject *newso = so->newso;
    assert(newso);
    so->newso = NULL;
    assert(so->ownertid == tsk->tid);
    assert(newso->ownertid == tsk->tid);

    /* Start printing output to the connection */
    printer = resolve_pntr(argstack[0]);
    node_log(tsk->n,LOG_DEBUG1,"accept %s:%d: printer is %s",
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

  c = (cell*)get_pntr(destpntr);
  so = (sysobject*)get_pntr(c->field1);

  /* Migration check */
  if (so->ownertid != tsk->tid) {
    migrate_to(tsk,so->ownertid);
    return;
  }

  if (SYSOBJECT_CONNECTION != so->type) {
    set_error(tsk,"write_data: first arg must be CONNECTION, got %s",sysobject_types[so->type]);
    return;
  }
  assert(so->connected);

  if (so->closed) {
    if (0 < len) {
      if (so->error)
        set_error(tsk,"writecon %s:%d: %s",so->hostname,so->port,so->errmsg);
      else
        set_error(tsk,"writecon %s:%d: Connection is closed",so->hostname,so->port);
    }
    curf->resume = 0;
    return;
  }

  if (1 == so->sockid.sid) {
    /* sid can only have the value 1 in standalone mode; we can take a shortcut here and write
       the data directly to the task's output stream */
    fwrite(data,1,len,stdout);
    argstack[0] = tsk->globnilpntr;
    return;
  }

  if (curf->resume) {
    argstack[0] = tsk->globnilpntr; /* normal return */
    curf->resume = 0;
    if (0 == len)
      sysobject_done_writing(so);
  }
  else {
    int ioid = suspend_current_frame(tsk,curf);
    send_write(tsk->endpt,so->sockid,ioid,data,len);
    assert(0 == so->frameids[WRITE_FRAMEADDR]);
    so->frameids[WRITE_FRAMEADDR] = ioid;
  }
}

static void b_print(task *tsk, pntr *argstack)
{
  pntr destpntr = argstack[1];
  double d;
  char c;

  CHECK_ARG(0,CELL_NUMBER);
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
  pntr destpntr = argstack[0];
  if (CELL_NIL == pntrtype(destpntr))
    return;
  CHECK_SYSOBJECT_ARG(0,SYSOBJECT_CONNECTION);
  write_data(tsk,argstack,NULL,0,destpntr);
}

static void b_error(task *tsk, pntr *argstack)
{
  pntr p = argstack[0];
  char *str;

  if (0 > array_to_string(p,&str)) {
    set_error(tsk,"error: argument is not a string");
    return;
  }

  set_error(tsk,"%s",str);
  free(str);
  return;
}

static void b_getoutput(task *tsk, pntr *argstack)
{
  assert(tsk->out_so);
  argstack[0] = tsk->out_so->p;
}

static void b_genid(task *tsk, pntr *argstack)
{
  double id;
  id = tsk->nextid++;
  id += ((double)(tsk->tid+1))*pow(2,32);
  setnumber(&argstack[0],id);
}

static void b_exit(task *tsk, pntr *argstack)
{
  tsk->done = 1;
  argstack[0] = tsk->globnilpntr;
  endpoint_interrupt(tsk->endpt);
}

static void b_abs(task *tsk, pntr *argstack)
{
  CHECK_ARG(0,CELL_NUMBER);
  setnumber(&argstack[0],fabs(pntrdouble(argstack[0])));
}

static void b_jnew(task *tsk, pntr *argstack)
{
  pntr classn = argstack[1];
  pntr args = argstack[0];
  frame *curf = *tsk->runptr;

  if (0 == curf->resume) {
    char *classname = NULL;

    if (0 > array_to_string(classn,&classname)) {
      set_error(tsk,"jnew: class name is not a string");
    }
    else {
      array *arr = array_new(1,0);
      array_printf(arr,"new %s",classname);

      if (serialise_args(tsk,args,arr)) {
        int ioid = suspend_current_frame(tsk,curf);
        send_jcmd(tsk->endpt,ioid,0,arr->data,arr->nbytes);
      }
      array_free(arr);
    }

    free(classname);
  }
  else {
    curf->resume = 0;
  }
}

static void b_jcall(task *tsk, pntr *argstack)
{
  pntr target = argstack[2];
  pntr method = argstack[1];
  pntr args = argstack[0];
  frame *curf = *tsk->runptr;

  if (0 == curf->resume) {
    char *methodname = NULL;
    char *targetname = NULL;

    if (get_callinfo(tsk,target,method,&targetname,&methodname)) {
      array *arr = array_new(1,0);
      array_printf(arr,"call %s %s",targetname,methodname);

      if (serialise_args(tsk,args,arr)) {
        int ioid = suspend_current_frame(tsk,curf);
        send_jcmd(tsk->endpt,ioid,0,arr->data,arr->nbytes);
      }
      array_free(arr);
    }

    free(methodname);
    free(targetname);
  }
  else {
    curf->resume = 0;
  }
}

static void b_iscons(task *tsk, pntr *argstack)
{
  setbool(tsk,&argstack[0],
          (CELL_CONS == pntrtype(argstack[0])) ||
          (CELL_AREF == pntrtype(argstack[0])));
}

static void b_cxslt(task *tsk, pntr *argstack)
{
  pntr sourcepntr = argstack[1];
  pntr urlpntr = argstack[0];
  char *source;
  char *url;
  char *compiled;

  if (0 > array_to_string(sourcepntr,&source)) {
    set_error(tsk,"cxslt: source is not a string");
    return;
  }

  if (0 > array_to_string(urlpntr,&url)) {
    set_error(tsk,"cxslt: url is not a string");
    free(source);
    return;
  }

/*   printf("------------- cxslt: url ------------------------\n"); */
/*   printf("%s\n",url); */
/*   printf("------------- cxslt: source ---------------------\n"); */
/*   printf("%s\n",source); */
/*   printf("-------------------------------------------------\n"); */

  /* FIXME: need to handle compile errors. Currently the compiler does this by calling exit(),
     which is really bad. Need to instead have it return an error code and messages, so that
     we can call set_error() here. */
  if (cxslt(source,url,&compiled)) {
    argstack[0] = string_to_array(tsk,compiled);
  }
  else {
    set_error(tsk,"%s",compiled);
  }
  free(compiled);

  free(source);
  free(url);
}

static void b_cache(task *tsk, pntr *argstack)
{
  assert(!"not yet implemented");
}

pntr socketid_string(task *tsk, socketid sockid)
{
  char ident[100];
  snprintf(ident,100,IP_FORMAT" %d %d %d",IP_ARGS(sockid.coordid.ip),
           sockid.coordid.port,sockid.coordid.localid,sockid.sid);
  return string_to_array(tsk,ident);
}

pntr mkcons(task *tsk, pntr head, pntr tail)
{
  pntr p;
  cell *pair = alloc_cell(tsk);
  pair->type = CELL_CONS;
  pair->field1 = head;
  pair->field2 = tail;
  make_pntr(p,pair);
  return p;
}

static void b_connpair(task *tsk, pntr *argstack)
{
  frame *curf = *tsk->runptr;

  if (0 == curf->resume) {
    int ioid = suspend_current_frame(tsk,*tsk->runptr);
    send_connpair(tsk->endpt,tsk->n->iothid,ioid);
  }
  else {
    /* Response processing done in interpreter_connpair_response() */
    curf->resume = 0;
  }
}

static void b_mkconn(task *tsk, pntr *argstack)
{
  pntr identpntr = argstack[0];
  char *ident;
  char ipstr[21];
  int port;
  int localid;
  int sid;
  sysobject *so;

  if (0 > array_to_string(identpntr,&ident)) {
    set_error(tsk,"mkconn: identifier is not a string");
    return;
  }

  if (4 != sscanf(ident,"%20s %d %d %d",ipstr,&port,&localid,&sid)) {
    set_error(tsk,"mkconn: invalid identifier");
    free(ident);
    return;
  }

  so = new_sysobject(tsk,SYSOBJECT_CONNECTION);
  so->hostname = strdup("(pair)");
  so->port = 0;
  so->sockid.coordid.ip = inet_addr(ipstr);
  so->sockid.coordid.port = port;
  so->sockid.coordid.localid = localid;
  so->sockid.sid = sid;
  so->connected = 1;

  make_pntr(argstack[0],so->c);

  free(ident);
}

static void b_spawn(task *tsk, pntr *argstack)
{
  pntr bcpntr = argstack[1];
  pntr identpntr = argstack[0];
  char *ident;
  int bcsize;
  char *bcdata;
  node *n = tsk->n;
  const char *argv[2];
  endpointid managerid = { ip: n->listenip, port: n->listenport, localid: MANAGER_ID };

  if (0 > (bcsize = array_to_string(bcpntr,&bcdata))) {
    set_error(tsk,"spawn: bytecode is not a byte array");
    return;
  }

  if ((sizeof(bcheader) > bcsize) ||
      memcmp(((bcheader*)bcdata)->signature,"NREDUCE BYTECODE",16)) {
    set_error(tsk,"spawn: invalid bytecode");
    free(bcdata);
    return;
  }

  if (0 > array_to_string(identpntr,&ident)) {
    set_error(tsk,"spawn: identifier is not a string");
    free(bcdata);
    return;
  }

  argv[0] = "_connection";
  argv[1] = ident;

  start_launcher(n,bcdata,bcsize,&managerid,1,NULL,tsk->out_sockid,2,argv);
  free(bcdata);
  free(ident);

  argstack[0] = tsk->globnilpntr;
}

static void b_compile(task *tsk, pntr *argstack)
{
  source *src;
  pntr codepntr = argstack[1];
  pntr filenamepntr = argstack[0];
  char *code;
  char *filename;
  int bcsize;
  char *bcdata = NULL;

  if (0 > array_to_string(codepntr,&code)) {
    set_error(tsk,"compile: code is not a string");
    return;
  }

  if (0 > array_to_string(filenamepntr,&filename)) {
    set_error(tsk,"compile: filename is not a string");
    free(code);
    return;
  }

  src = source_new();
  if ((0 != source_parse_string(src,code,"(unknown)","")) ||
      (0 != source_process(src,0,0,0,0)) ||
      (0 != source_compile(src,&bcdata,&bcsize))) {
    set_error(tsk,"compile error");
  }
  else {
    carray *arr = carray_new(tsk,1,0,NULL,NULL);
    make_pntr(argstack[0],arr->wrapper);
    carray_append(tsk,&arr,bcdata,bcsize,1);
  }

  free(bcdata);
  free(code);
  free(filename);
  source_free(src);
}

static void b_isspace(task *tsk, pntr *argstack)
{
  CHECK_ARG(0,CELL_NUMBER);
  setbool(tsk,&argstack[0],isspace((int)pntrdouble(argstack[0])));
}

static void b_lookup(task *tsk, pntr *argstack)
{
  pntr hostnamepntr = argstack[0];
  char *hostname;
  in_addr_t ip;
  int error;
  char ipstr[100];

  if (0 > array_to_string(hostnamepntr,&hostname)) {
    set_error(tsk,"lookup: hostname is not a string");
    return;
  }

  if (0 > lookup_address(hostname,&ip,&error)) {
    set_error(tsk,"lookup: %s",hstrerror(error));
    free(hostname);
    return;
  }

  snprintf(ipstr,100,IP_FORMAT,IP_ARGS(ip));
  argstack[0] = string_to_array(tsk,ipstr);
  free(hostname);
}

int get_builtin(const char *name)
{
  int i;
  for (i = 0; i < NUM_BUILTINS; i++)
    if (!strcmp(name,builtin_info[i].name))
      return i;
  return -1;
}

builtin builtin_info[NUM_BUILTINS] = {
/* Arithmetic operations */
{ "+",              2, 2, ALWAYS_VALUE, ALWAYS_TRUE,   PURE, b_add            },
{ "-",              2, 2, ALWAYS_VALUE, ALWAYS_TRUE,   PURE, b_subtract       },
{ "*",              2, 2, ALWAYS_VALUE, ALWAYS_TRUE,   PURE, b_multiply       },
{ "/",              2, 2, ALWAYS_VALUE, ALWAYS_TRUE,   PURE, b_divide         },
{ "%",              2, 2, ALWAYS_VALUE, ALWAYS_TRUE,   PURE, b_mod            },

/* Numeric comparison */
{ "==",             2, 2, ALWAYS_VALUE, MAYBE_FALSE,   PURE, b_eq             },
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
{ "_stringtonum",   1, 1, ALWAYS_VALUE, ALWAYS_TRUE,   PURE, b_stringtonum    },

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
{ "_readdir",       1, 1, MAYBE_UNEVAL, MAYBE_FALSE, IMPURE, b_readdir        },
{ "_exists",        1, 1, ALWAYS_VALUE, MAYBE_FALSE, IMPURE, b_exists         },
{ "_isdir",         1, 1, ALWAYS_VALUE, MAYBE_FALSE, IMPURE, b_isdir          },

/* Networking */
{ "_connect",       3, 2, ALWAYS_VALUE, MAYBE_FALSE, IMPURE, b_connect        },
{ "readcon",        2, 1, ALWAYS_VALUE, MAYBE_FALSE, IMPURE, b_readcon        },
{ "_listen",        1, 1, ALWAYS_VALUE, MAYBE_FALSE, IMPURE, b_listen         },
{ "accept",         2, 1, ALWAYS_VALUE, MAYBE_FALSE, IMPURE, b_accept         },

/* Terminal and network output */
{ "nchars",         1, 1, ALWAYS_VALUE, MAYBE_FALSE,   PURE, b_nchars         },
{ "print",          2, 2, ALWAYS_VALUE, MAYBE_FALSE, IMPURE, b_print          },
{ "printarray",     3, 3, ALWAYS_VALUE, MAYBE_FALSE, IMPURE, b_printarray     },
{ "printend",       1, 1, ALWAYS_VALUE, MAYBE_FALSE, IMPURE, b_printend       },

/* Other */
{ "_error",         1, 1, ALWAYS_VALUE, MAYBE_FALSE, IMPURE, b_error          },
{ "getoutput",      1, 1, ALWAYS_VALUE, MAYBE_FALSE, IMPURE, b_getoutput      },
{ "genid",          1, 1, ALWAYS_VALUE, ALWAYS_TRUE, IMPURE, b_genid          },
{ "exit",           1, 1, ALWAYS_VALUE, MAYBE_FALSE, IMPURE, b_exit           },

{ "abs",            1, 1, ALWAYS_VALUE, ALWAYS_TRUE,   PURE, b_abs            },
{ "_jnew",          2, 2, ALWAYS_VALUE, MAYBE_FALSE, IMPURE, b_jnew           },
{ "_jcall",         3, 3, ALWAYS_VALUE, MAYBE_FALSE, IMPURE, b_jcall          },
{ "iscons",         1, 1, ALWAYS_VALUE, MAYBE_FALSE,   PURE, b_iscons         },

{ "_cxslt",         2, 2, ALWAYS_VALUE, MAYBE_FALSE,   PURE, b_cxslt          },
{ "_cache",         2, 1, ALWAYS_VALUE, MAYBE_FALSE, IMPURE, b_cache          },

{ "connpair",       1, 1, ALWAYS_VALUE, MAYBE_FALSE, IMPURE, b_connpair       },
{ "mkconn",         1, 1, ALWAYS_VALUE, MAYBE_FALSE, IMPURE, b_mkconn         },
{ "_spawn",         2, 2, ALWAYS_VALUE, MAYBE_FALSE, IMPURE, b_spawn          },
{ "_compile",       2, 2, ALWAYS_VALUE, MAYBE_FALSE,   PURE, b_compile        },

{ "isspace",        1, 1, ALWAYS_VALUE, MAYBE_FALSE,   PURE, b_isspace        },
{ "_lookup",        1, 1, ALWAYS_VALUE, MAYBE_FALSE, IMPURE, b_lookup         },

{ "lcons",          2, 0, ALWAYS_VALUE, ALWAYS_TRUE,   PURE, b_cons           },

};
