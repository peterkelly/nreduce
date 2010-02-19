/*
 * This file is part of the nreduce project
 * Copyright (C) 2006-2009 Peter Kelly (pmk@cs.adelaide.edu.au)
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
#include "events.h"
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

extern int opt_postpone;

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

static void aref_resolve_tail(task *tsk, cell *c)
{
  if (CELL_IND == pntrtype(c->field2)) {
    c->field2 = resolve_pntr(c->field2);
    write_barrier_ifnew(tsk,c,c->field2);
  }
}

static void aref_resolve_element(task *tsk, cell *refcell, int pos)
{
  carray *arr = (carray*)get_pntr(refcell->field1);
  assert(sizeof(pntr) == arr->elemsize);
  pntr *elements = (pntr*)arr->elements;
  if (CELL_IND == pntrtype(elements[pos])) {
    elements[pos] = resolve_pntr(elements[pos]);
    write_barrier_ifnew(tsk,refcell,elements[pos]);
  }
}

static void aref_set_tail(task *tsk, cell *ref, pntr newtail)
{
  ref->field2 = newtail;
  write_barrier_ifnew(tsk,ref,newtail);
}

carray *carray_new(task *tsk, int dsize, int alloc)
{
  carray *arr;

  if (MAX_ARRAY_SIZE < alloc)
    alloc = MAX_ARRAY_SIZE;
  else if (0 >= alloc)
    alloc = 8;
  unsigned int sizereq = sizeof(carray)+alloc*dsize;
  if (0 != sizereq%8)
    sizereq += 8-(sizereq%8);
  arr = (carray*)alloc_mem(tsk,sizereq);
  arr->type = CELL_O_ARRAY;
  arr->nbytes = sizereq;
  arr->alloc = alloc;
  arr->size = 0;
  arr->elemsize = dsize;
  arr->multiref = 0;
  arr->nchars = 0;

  #ifdef PROFILING
  tsk->stats.array_allocs++;
  #endif

  return arr;
}

/* Can be called from native code */
cell *create_array_cell(task *tsk, int dsize, int alloc)
{
  carray *carr = carray_new(tsk,dsize,alloc);

  cell *refcell = alloc_cell(tsk);
  refcell->type = CELL_AREF;
  make_pntr(refcell->field1,carr);
  refcell->field2 = tsk->globnilpntr;

  return refcell;
}

pntr create_array(task *tsk, int dsize, int alloc)
{
  cell *refcell = create_array_cell(tsk,dsize,alloc);
  pntr p;
  make_pntr(p,refcell);
  return p;
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

static void check_array_convert(task *tsk, cell *refcell, const char *from)
{
  carray *arr = (carray*)get_pntr(refcell->field1);
  assert(!arr->multiref);
  aref_resolve_tail(tsk,refcell);
  if ((sizeof(pntr) == arr->elemsize) &&
      ((MAX_ARRAY_SIZE == arr->size) || (CELL_FRAME != pntrtype(refcell->field2)))) {
    pntr *pelements = (pntr*)arr->elements;

    while (arr->nchars < arr->size) {
      aref_resolve_element(tsk,refcell,arr->nchars);
      if (pntr_is_char(pelements[arr->nchars]))
        arr->nchars++;
      else
        break;
    }

    if (arr->nchars == arr->size)
      convert_to_string(tsk,arr);
  }
}

static void carray_append(task *tsk, cell **refcell, const void *data, int totalcount, int dsize)
{
  carray *arr = (carray*)get_pntr((*refcell)->field1);
  assert(dsize == arr->elemsize); /* sanity check */
  assert(!arr->multiref); /* can only append if there's a single reference */

  while (1) {
    int count = totalcount;
    if (arr->size+count > MAX_ARRAY_SIZE)
      count = MAX_ARRAY_SIZE - arr->size;

    if (arr->alloc < arr->size+count) {
      while (arr->alloc < arr->size+count)
        arr->alloc *= 4;
      unsigned int sizereq = sizeof(carray)+arr->alloc*arr->elemsize;

      if (0 != sizereq%8)
        sizereq += 8-(sizereq%8);

      if (sizereq > arr->nbytes) {
        arr = (carray*)realloc_mem(tsk,arr,sizereq);
        arr->nbytes = sizereq;
      }

      write_barrier(tsk,*refcell);
      make_pntr((*refcell)->field1,arr);
      #ifdef PROFILING
      tsk->stats.array_resizes++;
      #endif
    }

    write_barrier(tsk,(cell*)arr);
    memmove(((unsigned char*)arr->elements)+arr->size*arr->elemsize,
            data,count*arr->elemsize);
    arr->size += count;

    data += count;
    totalcount -= count;

    if (0 == totalcount)
      break;

    /* Still more data to append - create a new chain in the array */

    check_array_convert(tsk,*refcell,"append");

    cell *oldref = *refcell;

    arr = carray_new(tsk,dsize,totalcount);

    *refcell = alloc_cell(tsk);
    (*refcell)->type = CELL_AREF;
    make_pntr((*refcell)->field1,arr);
    (*refcell)->field2 = tsk->globnilpntr;

    pntr p;
    make_aref_pntr(p,*refcell,0);
    aref_set_tail(tsk,oldref,p);
  }
}

void maybe_expand_array(task *tsk, pntr p)
{
  if (CELL_CONS == pntrtype(p)) {
    cell *firstcell = get_pntr(p);
    pntr firsthead = resolve_pntr(firstcell->field1);
    pntr firsttail = resolve_pntr(firstcell->field2);

    if (CELL_CONS == pntrtype(firsttail)) {

      /* Case 1: Two consecutive CONS cells - convert to an array */

      cell *secondcell = get_pntr(firsttail);
      pntr secondhead = resolve_pntr(secondcell->field1);
      pntr secondtail = resolve_pntr(secondcell->field2);

      carray *arr = carray_new(tsk,sizeof(pntr),8); /* allocate 8 to allow for expansion */

      cell *refcell = firstcell;
      write_barrier(tsk,refcell);
      refcell->type = CELL_AREF;
      make_pntr(refcell->field1,arr);
      refcell->field2 = tsk->globnilpntr;

      ((pntr*)arr->elements)[0] = firsthead;
      ((pntr*)arr->elements)[1] = secondhead;
      arr->size = 2;

      aref_set_tail(tsk,refcell,secondtail);
    }
  }

  if (CELL_AREF == pntrtype(p)) {
    cell *refcell = (cell*)get_pntr(p);
    carray *arr = aref_array(p);

    /* If the array object potentially has multiple AREF cells referencing it, then it
       might be part of several lists and thus we must avoid expanding it as otherwise
       we could end up with extra data appearing in the middle of existing lists. */
    if (arr->multiref)
      return;

    aref_resolve_tail(tsk,refcell);

    if (sizeof(pntr) == aref_array(p)->elemsize) {
      while (MAX_ARRAY_SIZE > arr->size) {
        if (CELL_CONS == pntrtype(refcell->field2)) {
          cell *tailcell = get_pntr(refcell->field2);
          pntr tailhead = resolve_pntr(tailcell->field1);
          carray_append(tsk,&refcell,&tailhead,1,sizeof(pntr));
          aref_set_tail(tsk,refcell,resolve_pntr(tailcell->field2));
        }
        else if ((CELL_AREF == pntrtype(refcell->field2)) &&
                 (sizeof(pntr) == aref_array(refcell->field2)->elemsize)) {
          carray *otharr = aref_array(refcell->field2);
          pntr othtail = aref_tail(refcell->field2);
          int othindex = aref_index(refcell->field2);
          carray_append(tsk,&refcell,&((pntr*)otharr->elements)[othindex],
                         otharr->size-othindex,sizeof(pntr));
          aref_set_tail(tsk,refcell,othtail);
        }
        else {
          break;
        }
      }
    }
    else {
      /* string */
      while (1) {
        if (CELL_AREF != pntrtype(refcell->field2))
          break;
        carray *tail_arr = aref_array(refcell->field2);
        pntr tail_tail = aref_tail(refcell->field2);
        int tail_index = aref_index(refcell->field2);
        int tail_size = tail_arr->size-tail_index;
        if (tail_arr->elemsize != arr->elemsize)
          break;
        if (arr->size+tail_size > MAX_ARRAY_SIZE)
          break;
        carray_append(tsk,&refcell,&tail_arr->elements[tail_index],tail_size,arr->elemsize);
        refcell->field2 = tail_tail;
        write_barrier_ifnew(tsk,refcell,refcell->field2);
      }
    }

    check_array_convert(tsk,refcell,"expand");
  }
}

pntr pointers_to_list(task *tsk, pntr *data, int size, pntr tail)
{
  if (0 == size) {
    return tail;
  }
  else {
    pntr aref = create_array(tsk,sizeof(pntr),size);
    cell *refcell = get_pntr(aref);
    carray_append(tsk,&refcell,data,size,sizeof(pntr));
    aref_set_tail(tsk,refcell,tail);
    return aref;
  }
}

pntr binary_data_to_list(task *tsk, const char *data, int size, pntr tail)
{
  if (0 == size) {
    return tail;
  }
  else {
    pntr aref = create_array(tsk,1,size);
    cell *refcell = get_pntr(aref);
    carray_append(tsk,&refcell,data,size,1);
    aref_set_tail(tsk,refcell,tail);
    return aref;
  }
}

pntr string_to_array(task *tsk, const char *str)
{
  return binary_data_to_list(tsk,str,strlen(str),tsk->globnilpntr);
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
      p = aref_tail(p);
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
      p = aref_tail(p);
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
    cell *refcell = get_pntr(arg);
    int index = aref_index(arg);
    carray *arr = aref_array(arg);
    assert(index < arr->size);

    if (index+1 < arr->size) {
      make_aref_pntr(argstack[0],refcell,index+1);
    }
    else {
      argstack[0] = aref_tail(arg);
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
    cell *refcell = get_pntr(refpntr);
    carray *arr = aref_array(refpntr);
    int index = aref_index(refpntr);
    assert(index < arr->size);
    assert(index+n <= arr->size);

    if (index+n == arr->size)
      argstack[0] = aref_tail(refpntr);
    else
      make_aref_pntr(argstack[0],refcell,index+n);
  }
  else {
    set_error(tsk,"arrayskip: expected aref or cons, got %s",cell_types[pntrtype(refpntr)]);
  }
}

static void b_arrayitem(task *tsk, pntr *argstack)
{
  pntr npntr = argstack[1];
  pntr refpntr = argstack[0];
  int n;

  CHECK_ARG(1,CELL_NUMBER);

  maybe_expand_array(tsk,refpntr);

  n = (int)pntrdouble(npntr);
  assert(0 <= n);


  if (CELL_CONS == pntrtype(refpntr)) {
    assert(0 == n);
    argstack[0] = get_pntr(refpntr)->field1;
  }
  else if (CELL_AREF == pntrtype(refpntr)) {
    carray *arr = aref_array(refpntr);
    int index = aref_index(refpntr);
    assert(index < arr->size);
    assert(index+n < arr->size);

    if (sizeof(pntr) == arr->elemsize) {
      argstack[0] = ((pntr*)arr->elements)[index+n];
    }
    else {
      set_pntrdouble(argstack[0],(double)(((unsigned char*)arr->elements)[index+n]));
    }
  }
  else {
    set_error(tsk,"arrayitem: expected aref or cons, got %s",cell_types[pntrtype(refpntr)]);
  }
}

static void b_restring(task *tsk, pntr *argstack)
{
  /* This function is an ugly hack necessary to get around a race condition in Java's
     built-in web server used for publishing web services. The race condition causes
     it to "miss" some of the data contained in a HTTP request if the request line
     and headers are not received as a single unit by the server. When sending a
     request built out of multiple arrays, NReduce issues one write operation for each,
     exposing the race condition in the web server. This function joins a set of arrays
     into a single array; it is called from xq::post2 to ensure that the request is
     sent as a single unit. */

  /* FIXME: this function fails when the argument contains a REMOTEREF to a later portion
     of the string. This could be fixed by sending a fetch request for the appropriate
     tail, and then suspending the calling frame so that this function is woken up when
     the object arrives. */

  char *str = NULL;
  if (0 > array_to_string(argstack[0],&str)) {
    set_error(tsk,"restring: argument is not a string");
    return;
  }
  argstack[0] = string_to_array(tsk,str);
  free(str);
}

static void b_buildarray(task *tsk, pntr *argstack)
{
  /* When an attempt is made to send an AREF between machines, what is actually sent is
     a frame which is a call to b_buildarray, containing parameters for the carray object
     and the tail. When this frame is arrives, the receiver will think that the object
     associated with the global address it requested is not an AREF, but rather a frame.
     It will execute this frame, which will call buildarray. When buildarray returns,
     the frame cell will change to an IND cell which points to the AREF cell. Thus, the
     object associated with the global address will be an AREF, which is what we originally
     wanted.

     This approach enables us to do the two transfers (aref and carray object) without having
     to modify the logic that deals with object reception. We rely on the existing
     evaluation mechanism to send a FETCH request for the carray object (which it will do
     before it calls the buildarray function).

     As an optimisation, we could perhaps send the full array if it is less than a particular
     size. */
  pntr arrayp = argstack[2];
  pntr indexp = argstack[1];
  pntr tailp = argstack[0];

  CHECK_ARG(2,CELL_O_ARRAY); /* first arg */
  CHECK_ARG(1,CELL_NUMBER); /* second arg */

  carray *carr = (carray*)get_pntr(arrayp);
  int index = (int)pntrdouble(indexp);

  assert((1 == carr->elemsize) || (sizeof(pntr) == carr->elemsize));
  assert(MAX_ARRAY_SIZE >= carr->size);
  assert(index < carr->size);

  cell *refcell = alloc_cell(tsk);
  refcell->type = CELL_AREF;
  make_pntr(refcell->field1,carr);
  refcell->field2 = tailp;
  make_aref_pntr(argstack[0],refcell,index);
}

static void b_arrayprefix(task *tsk, pntr *argstack)
{
  pntr npntr = argstack[2];
  pntr refpntr = argstack[1];
  pntr restpntr = argstack[0];
  int n;

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
      argstack[0] = restpntr;
      return;
    }

    if (n > arr->size-index)
      n = arr->size-index;

    if ((0 == index) && (n == arr->size)) {
      /* Optimisation: Instead of creating a new array with a copy of the data, simply
         create a new aref cell which references the existing array data. This means that
         the data can be shared, which is useful if the array is large and the copy is
         part of another list. */

      cell *new_aref = alloc_cell(tsk);
      new_aref->type = CELL_AREF;
      make_pntr(new_aref->field1,arr);
      aref_set_tail(tsk,new_aref,restpntr);
      make_aref_pntr(argstack[0],new_aref,index);

      /* We set the multiref flag here to ensure that this array never gets expanded. If
         this wasn't done, then we could get data unexpectedly appearing in existing lists. */
      arr->multiref = 1;
    }
    else {
      /* Create a new array with a copy of the old data */
      if (sizeof(pntr) == arr->elemsize)
        argstack[0] = pointers_to_list(tsk,(pntr*)(arr->elements+index*arr->elemsize),n,restpntr);
      else
        argstack[0] = binary_data_to_list(tsk,arr->elements+index*arr->elemsize,n,restpntr);
    }
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
    pntr atail = aref_tail(a);
    pntr btail = aref_tail(b);
    int aindex = aref_index(a);
    int bindex = aref_index(b);
    aref_resolve_tail(tsk,get_pntr(a));
    aref_resolve_tail(tsk,get_pntr(b));
    if ((1 == aarr->elemsize) && (1 == barr->elemsize) &&
        (CELL_NIL == pntrtype(atail)) &&
        (CELL_NIL == pntrtype(btail))) {

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

static void b_ntos(task *tsk, pntr *argstack)
{
  pntr p = argstack[0];
  char str[100];

  CHECK_ARG(0,CELL_NUMBER);
  format_double(str,100,pntrdouble(p));
  assert(0 < strlen(str));
  argstack[0] = string_to_array(tsk,str);
}

static void b_ston(task *tsk, pntr *argstack)
{
  pntr p = argstack[0];
  char *str;
  char *end = NULL;

  if (0 > array_to_string(p,&str)) {
    set_error(tsk,"ston: argument is not a string");
    return;
  }

  setnumber(&argstack[0],strtod(str,&end));
  if (('\0' == *str) || ('\0' != *end))
    set_error(tsk,"ston: \"%s\" is not a valid number",str);

  free(str);
}

static void b_teststring(task *tsk, pntr *argstack)
{
  pntr tail = argstack[0];
  pntr p = string_to_array(tsk,"this part shouldn't be here! test");
  cell *refcell = get_pntr(p);
  aref_set_tail(tsk,refcell,tail);
  make_aref_pntr(argstack[0],refcell,29);
}

static void b_testarray(task *tsk, pntr *argstack)
{
  /* no longer used; could be safely removed */
  argstack[0] = tsk->globnilpntr;
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
    free_sysobject(tsk,so);
    cell_make_ind(tsk,c,tsk->globnilpntr);
    return;
  }

  argstack[0] = binary_data_to_list(tsk,buf,r,nextpntr);
}

static void b_readdir(task *tsk, pntr *argstack)
{
  char *path;
  pntr filenamepntr = argstack[0];

  if (0 > array_to_string(filenamepntr,&path)) {
    set_error(tsk,"readdir: path is not a string");
    return;
  }

  DIR *dir;
  if (NULL == (dir = opendir(path))) {
    set_error(tsk,"%s: %s",path,strerror(errno));
    free(path);
    return;
  }

  argstack[0] = tsk->globnilpntr;

  array *dircontents = array_new(sizeof(pntr),0);

  struct dirent tmp;
  struct dirent *entry;
  while ((0 == readdir_r(dir,&tmp,&entry)) && (NULL != entry)) {
    char *fullpath;
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

      pntr fields[3];
      fields[0] = string_to_array(tsk,entry->d_name);
      fields[1] = string_to_array(tsk,type);
      set_pntrdouble(fields[2],statbuf.st_size);
      pntr entryp = pointers_to_list(tsk,fields,3,tsk->globnilpntr);

      array_append(dircontents,&entryp,sizeof(pntr));
    }

    free(fullpath);
  }

  argstack[0] = pointers_to_list(tsk,(pntr*)dircontents->data,
                                 array_count(dircontents),tsk->globnilpntr);

  closedir(dir);
  array_free(dircontents);
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

static void start_frame_running(task *tsk, pntr framep)
{
  framep = resolve_pntr(framep);
  if (CELL_FRAME == pntrtype(framep)) {
    run_frame(tsk,pframe(framep));
  }
  else if (CELL_REMOTEREF == pntrtype(framep)) {
    /* Send a FETCH request for the frame, so it will migrate here and start running,
       but don't need to block the current frame since they're supposed to run concurrently */
    global *target = (global*)get_pntr(get_pntr(framep)->field1);
    assert(target->addr.tid != tsk->tid);

    gaddr storeaddr = get_physical_address(tsk,framep);
    assert(storeaddr.tid == tsk->tid);
    event_send_fetch(tsk,target->addr.tid,target->addr,storeaddr,FUN_START_FRAME_RUNNING);
    msg_fsend(tsk,target->addr.tid,MSG_FETCH,"aa",target->addr,storeaddr);
    assert(0 <= tsk->nfetching);
    tsk->nfetching++;
    target->fetching = 1;
  }
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

    CHECK_ARG(1,CELL_NUMBER);
    port = (int)pntrdouble(portpntr);

    if (0 > array_to_string(hostnamepntr,&hostname)) {
      set_error(tsk,"connect: hostname is not a string");
      return;
    }

    int error;
    if (0 > lookup_address(hostname,&ip,&error)) {
      set_error(tsk,"%s, %s",hostname,hstrerror(error));
      free(hostname);
      return;
    }

    int local = (ntohl(ip) == 0x7f000001); /* 127.0.0.1 */
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

    if (opt_postpone &&
        ((MAX_TOTAL_CONNECTIONS <= tsk->total_conns) ||
         ((MAX_LOCAL_CONNECTIONS <= tsk->local_conns) && local))) {
      /* This machine is already processing the max. no of allowed service requests. Put
         the frame back in the SPARKED state, and mark it as postponed. This will prevent
         the frame from being run again locally until the number of active local service
         requests drops below the maximum. The frame can however be exported to other machines
         in response to a FISH request. */
      int fno = frame_fno(tsk,curf);
      curf->instr = bc_instructions(tsk->bcdata)+bc_funinfo(tsk->bcdata)[fno].address+1;

      done_frame(tsk,curf);
      curf->postponed = 1;
      curf->state = STATE_SPARKED;
      append_spark(tsk,curf);
      check_runnable(tsk);

      free(hostname);
      return;
    }

    /* Create sysobject cell */
    so = new_sysobject(tsk,SYSOBJECT_CONNECTION);
    so->hostname = strdup(hostname);
    so->port = port;
    so->len = 0;
    so->outgoing_connection = 1;

    gettimeofday(&so->start,NULL);
    so->local = local;
    if (so->local) {
      assert(0 <= so->tsk->local_conns);
      tsk->local_conns++;
    }
    assert(0 <= so->tsk->total_conns);
    tsk->total_conns++;

    make_pntr(argstack[2],so->c);

    node_log(tsk->n,LOG_DEBUG1,"%d: CONNECT1 (%s:%d) local_conns = %d",
             tsk->tid,hostname,port,tsk->local_conns);

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
      if (((EISCONN == so->errn) || (EMFILE == so->errn) || (ENFILE == so->errn)) &&
          (0 == so->retry)){
        node_log(tsk->n,LOG_DEBUG1,
                 "%d: CONNECT2 (%s:%d) failed (out of sockets); will retry after GC",
                 tsk->tid,so->hostname,so->port);
        /* Force a major garbage collection and retry */
        so->retry++;
        argstack[2] = string_to_array(tsk,so->hostname);
        int ioid = suspend_current_frame(tsk,*tsk->runptr);
        curf->resume = 0;
        int *copy = (int*)malloc(sizeof(int));
        memcpy(copy,&ioid,sizeof(int));
        list_push(&tsk->wakeup_after_collect,copy);
        fprintf(stderr,"suspended; ioid %d\n",ioid);
        force_major_collection(tsk);
        return;
      }
      else {
        node_log(tsk->n,LOG_DEBUG1,"%d: CONNECT2 (%s:%d) failed",tsk->tid,so->hostname,so->port);
        set_error(tsk,"%s:%d: %s",so->hostname,so->port,so->errmsg);
        sysobject_done_connect(so);
        sysobject_done_reading(so);
        sysobject_done_writing(so);
        sysobject_delete_if_finished(so);
        return;
      }
    }
    else {
      sysobject_done_connect(so);
      node_log(tsk->n,LOG_DEBUG1,"%d: CONNECT2 (%s:%d) connected",tsk->tid,so->hostname,so->port);

      /* Start printing output to the connection */
      start_frame_running(tsk,argstack[0]);

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
  pntr nextpntr = resolve_pntr(argstack[0]);
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
    curf->resume = 0;
    sysobject_done_reading(so);
    sysobject_delete_if_finished(so);
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
      sysobject_delete_if_finished(so);
    }
    else {

      argstack[0] = binary_data_to_list(tsk,so->buf,so->len,nextpntr);

      free(so->buf);
      so->buf = NULL;
      so->len = 0;

      if (strict_evaluation) {
        /* Force the next part of the stream to be read immediately */
        start_frame_running(tsk,nextpntr);
      }
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
    sysobject_done_writing(so);
    sysobject_delete_if_finished(so);
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
    if (0 == len) {
      sysobject_done_writing(so);
      sysobject_delete_if_finished(so);
    }
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
  so->done_connect = 1;

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
    argstack[0] = binary_data_to_list(tsk,bcdata,bcsize,tsk->globnilpntr);
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
{ "ntos",           1, 1, ALWAYS_VALUE, ALWAYS_TRUE,   PURE, b_ntos           },
{ "_ston",          1, 1, ALWAYS_VALUE, ALWAYS_TRUE,   PURE, b_ston           },

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
{ "arrayitem",      2, 2, MAYBE_UNEVAL, MAYBE_FALSE,   PURE, b_arrayitem      },
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
{ "print",          2, 2, MAYBE_UNEVAL, MAYBE_FALSE, IMPURE, b_print          },
{ "printarray",     3, 3, MAYBE_UNEVAL, MAYBE_FALSE, IMPURE, b_printarray     },
{ "printend",       1, 1, MAYBE_UNEVAL, MAYBE_FALSE, IMPURE, b_printend       },

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

{ "lcons",          2, 0, ALWAYS_VALUE, ALWAYS_TRUE,   PURE, b_cons           },
{ "restring",       1, 1, ALWAYS_VALUE, MAYBE_FALSE,   PURE, b_restring       },
{ "buildarray",     3, 2, ALWAYS_VALUE, MAYBE_FALSE,   PURE, b_buildarray     },

};
