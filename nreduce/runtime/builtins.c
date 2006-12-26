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
#include "runtime/runtime.h"
#include "compiler/bytecode.h"
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

#define CHECK_ARG(_argno,_type,_bif) {                                  \
    if ((_type) != pntrtype(argstack[(_argno)])) {                      \
      set_error(tsk,"%s: %s argument must be of type %s, but got %s",  \
                builtin_info[(_bif)].name,                              \
                numnames[builtin_info[(_bif)].nargs-1-(_argno)],        \
                cell_types[(_type)],                                    \
                cell_types[pntrtype(argstack[(_argno)])]);              \
      return;                                                           \
    }                                                                   \
  }

void arrdebug(task *tsk, const char *format, ...)
{
#ifdef ARRAY_DEBUG
  va_list ap;
  va_start(ap,format);
  vfprintf(tsk->output,format,ap);
  va_end(ap);
#endif
}

const builtin builtin_info[NUM_BUILTINS];

static void setnumber(pntr *cptr, double val)
{
  *cptr = val;
}

static void setbool(task *tsk, pntr *cptr, int b)
{
  *cptr = (b ? tsk->globtruepntr : tsk->globnilpntr);
}

#define CHECK_NUMERIC_ARGS(bif)                                         \
  if ((CELL_NUMBER != pntrtype(argstack[1])) || (CELL_NUMBER != pntrtype(argstack[0]))) { \
    int t1 = pntrtype(argstack[1]); \
    int t2 = pntrtype(argstack[0]); \
    set_error(tsk,"%s: incompatible arguments: (%s,%s)", \
              builtin_info[bif].name,cell_types[t1],cell_types[t2]); \
    argstack[0] = tsk->globnilpntr; \
    return; \
  }

static void b_add(task *tsk, pntr *argstack)
{
  CHECK_NUMERIC_ARGS(B_ADD);
  argstack[0] = argstack[1] + argstack[0];
}

static void b_subtract(task *tsk, pntr *argstack)
{
  CHECK_NUMERIC_ARGS(B_SUBTRACT);
  argstack[0] = argstack[1] - argstack[0];
}

static void b_multiply(task *tsk, pntr *argstack)
{
  CHECK_NUMERIC_ARGS(B_MULTIPLY);
  argstack[0] = argstack[1] * argstack[0];
}

static void b_divide(task *tsk, pntr *argstack)
{
  CHECK_NUMERIC_ARGS(B_DIVIDE);
  /* FIXME: handle divide by zero -> NaN properly, i.e. without confusing
     a true NaN value with a pointer */
  argstack[0] = argstack[1] / argstack[0];
}

static void b_mod(task *tsk, pntr *argstack)
{
  CHECK_NUMERIC_ARGS(B_MOD);
  argstack[0] = fmod(argstack[1],argstack[0]);
}

static void b_eq(task *tsk, pntr *argstack)
{
  CHECK_NUMERIC_ARGS(B_EQ);
  setbool(tsk,&argstack[0],argstack[1] == argstack[0]);
}

static void b_ne(task *tsk, pntr *argstack)
{
  CHECK_NUMERIC_ARGS(B_NE);
  setbool(tsk,&argstack[0],argstack[1] != argstack[0]);
}

static void b_lt(task *tsk, pntr *argstack)
{
  CHECK_NUMERIC_ARGS(B_LT);
  setbool(tsk,&argstack[0],argstack[1] <  argstack[0]);
}

static void b_le(task *tsk, pntr *argstack)
{
  CHECK_NUMERIC_ARGS(B_LE);
  setbool(tsk,&argstack[0],argstack[1] <= argstack[0]);
}

static void b_gt(task *tsk, pntr *argstack)
{
  CHECK_NUMERIC_ARGS(B_GT);
  setbool(tsk,&argstack[0],argstack[1] >  argstack[0]);
}

static void b_ge(task *tsk, pntr *argstack)
{
  CHECK_NUMERIC_ARGS(B_GE);
  setbool(tsk,&argstack[0],argstack[1] >= argstack[0]);
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

  a = (int)val1;
  b = (int)val2;

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
  val = (int)arg;
  setnumber(&argstack[0],(double)(~val));
}

static void b_sqrt(task *tsk, pntr *argstack)
{
  CHECK_ARG(0,CELL_NUMBER,B_SQRT);
  setnumber(&argstack[0],sqrt(argstack[0]));
  /* FIXME: handle NaN result properly (it's not a pointer!) */
}

static void b_floor(task *tsk, pntr *argstack)
{
  CHECK_ARG(0,CELL_NUMBER,B_FLOOR);
  setnumber(&argstack[0],floor(argstack[0]));
}

static void b_ceil(task *tsk, pntr *argstack)
{
  CHECK_ARG(0,CELL_NUMBER,B_CEIL);
  setnumber(&argstack[0],ceil(argstack[0]));
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

static void b_echo(task *tsk, pntr *argstack)
{
  char *str;
  CHECK_ARG(0,CELL_AREF,B_OPENFD);

  str = array_to_string(argstack[0]);
  fprintf(tsk->output,"%s",str);
  free(str);
  argstack[0] = tsk->globnilpntr;
}

void printp(FILE *f, pntr p)
{
  if (CELL_NUMBER == pntrtype(p)) {
    double d = p;
    if ((d == floor(d)) && (0 < d) && (128 > d)) {
      fprintf(f,"%c",(int)d);
    }
    else {
      fprintf(f,"?");
    }
  }
  else if (CELL_NIL == pntrtype(p)) {
  }
  else if ((CELL_CONS == pntrtype(p)) || (CELL_AREF == pntrtype(p))) {
    fprintf(f,"(list)");
  }
  else {
    fprintf(f,"print: encountered %s\n",cell_types[pntrtype(p)]);
    abort();
  }
}

static void b_print(task *tsk, pntr *argstack)
{
  pntr p = argstack[0];
#ifdef PRINT_DEBUG
  fprintf(tsk->output,"p"); /* FIXME: temp */
#endif
  printp(tsk->output,p);
  fflush(tsk->output);

  argstack[0] = tsk->globnilpntr;
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

carray *carray_new(task *tsk, int dsize, carray *oldarr, cell *usewrapper)
{
  carray *arr = (carray*)calloc(1,sizeof(carray));
  arr->alloc = 8;
  arr->size = 0;
  arr->elemsize = dsize;
  arr->elements = (pntr*)calloc(arr->alloc,arr->elemsize);
  arr->tail = tsk->globnilpntr;

  arr->wrapper = usewrapper ? usewrapper : alloc_cell(tsk);
  arr->wrapper->type = CELL_AREF;
  make_pntr(arr->wrapper->field1,arr);

  if (oldarr)
    make_aref_pntr(oldarr->tail,arr->wrapper,0);

  return arr;
}

int pntr_is_char(pntr p)
{
  if (CELL_NUMBER == pntrtype(p)) {
    double d = p;
    if ((floor(d) == d) && (0 <= d) && (255 >= d))
      return 1;
  }
  return 0;
}

void convert_to_string(task *tsk, carray *arr)
{
  unsigned char *newelements;
  int i;
  assert(sizeof(pntr) == arr->elemsize);
  assert(arr->nchars == arr->size);

  newelements = (unsigned char*)malloc(arr->size);
  for (i = 0; i < arr->size; i++)
    newelements[i] = (unsigned char)((pntr*)arr->elements)[i];

  free(arr->elements);
  arr->elements = newelements;
  arr->elemsize = 1;
}

int check_array_convert(task *tsk, carray *arr, const char *from)
{
  int printed = 0;
  if ((sizeof(pntr) == arr->elemsize) &&
      ((MAX_ARRAY_SIZE == arr->size) || (CELL_NIL == pntrtype(arr->tail)))) {
    int oldchars = arr->nchars;

    while ((arr->nchars < arr->size) &&
           pntr_is_char(((pntr*)arr->elements)[arr->nchars]))
      arr->nchars++;

    if (arr->nchars != oldchars) {
      arrdebug(tsk,"%s: now arr->nchars = %d, was %d; array size = %d\n",
               from,arr->nchars,oldchars,arr->size);
      printed = 1;
    }

    if (arr->nchars == arr->size) {
      arrdebug(tsk,"%s: converting %d-element array to string\n",from,arr->size);
      printed = 1;

      convert_to_string(tsk,arr);
    }
  }
  return printed;
}

void carray_append(task *tsk, carray **arr, const void *data, int totalcount, int dsize)
{
  assert(dsize == (*arr)->elemsize); /* sanity check */

  while (1) {
    int count = totalcount;
    if ((*arr)->size+count > MAX_ARRAY_SIZE)
      count = MAX_ARRAY_SIZE - (*arr)->size;

    if ((*arr)->alloc < (*arr)->size+count) {
      while ((*arr)->alloc < (*arr)->size+count)
        (*arr)->alloc *= 2;
      (*arr)->elements = (pntr*)realloc((*arr)->elements,(*arr)->alloc*(*arr)->elemsize);
    }

    memcpy(((char*)(*arr)->elements)+(*arr)->size*(*arr)->elemsize,data,count*(*arr)->elemsize);
    (*arr)->size += count;

    data += count;
    totalcount -= count;

    if (0 == totalcount)
      break;

    check_array_convert(tsk,*arr,"append");

    *arr = carray_new(tsk,dsize,*arr,NULL);
  }
}

void maybe_expand_array(task *tsk, pntr p)
{
  int printed = 0;
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

      arrdebug(tsk,"expand: replacing double cons %s, %s with array (secondtail is %s)\n",
               cell_types[pntrtype(firsthead)],cell_types[pntrtype(secondhead)],
               cell_types[pntrtype(secondtail)]);
      printed = 1;

      arr = carray_new(tsk,sizeof(pntr),NULL,firstcell);
      carray_append(tsk,&arr,&firsthead,1,sizeof(pntr));
      carray_append(tsk,&arr,&secondhead,1,sizeof(pntr));
      arr->tail = secondtail;
    }
  }

  if (CELL_AREF == pntrtype(p)) {
    carray *arr = aref_array(p);
    int count = 0;
    arr->tail = resolve_pntr(arr->tail);

    while (1) {

      if (CELL_CONS == pntrtype(arr->tail)) {
        cell *tailcell = get_pntr(arr->tail);
        pntr tailhead = resolve_pntr(tailcell->field1);
        carray_append(tsk,&arr,&tailhead,1,sizeof(pntr));
        arr->tail = resolve_pntr(tailcell->field2);

        count++;
      }
      else {
        break;
      }
    }

    if (0 < count) {
      arrdebug(tsk,"expand: appended %d new values from conses; nchars = %d, elemsize = %d\n",
               count,arr->nchars,arr->elemsize);
      printed = 1;
    }


    if (check_array_convert(tsk,arr,"expand"))
      printed = 1;
  }


  if (printed) {
    arrdebug(tsk,"expand: done\n");
  }
}

pntr string_to_array(task *tsk, const char *str)
{
  /* FIXME: handle case where strlen(str) > MAX_ARRAY_SIZE */
  if (0 == strlen(str)) {
    return tsk->globnilpntr;
  }
  else {
    carray *arr = carray_new(tsk,1,NULL,NULL);
    pntr p;
    carray_append(tsk,&arr,str,strlen(str),1);
    make_aref_pntr(p,arr->wrapper,0);
    return p;
  }
}

char *array_to_string(pntr refpntr)
{
  /* FIXME: handle the case of index > 0 */
  /* FIXME: test in both dsize=1 and dsize=8 cases */
  /* FIXME: handle chained arrays */
  carray *arr = aref_array(refpntr);
  char *str = malloc(arr->size+1);

  if (1 == arr->elemsize) {
    memcpy(str,arr->elements,arr->size);
    str[arr->size] = '\0';
  }
  else if (sizeof(pntr) == arr->elemsize) {
    int i;
    int pos = 0;
    for (i = 0; i < arr->size; i++) {
      pntr val = resolve_pntr(((pntr*)arr->elements)[i]);
      if (CELL_NUMBER == pntrtype(val))
        str[pos++] = ((int)val) & 0xff;
    }
    str[pos++] = '\0';
  }
  else
    fatal("array_to_string: invalid array size");

  return str;
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
      argstack[0] = (double)(((char*)arr->elements)[index]);
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

void b_arraysize(task *tsk, pntr *argstack)
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

void b_arrayskip(task *tsk, pntr *argstack)
{
  pntr npntr = argstack[1];
  pntr refpntr = argstack[0];
  int n;

  CHECK_ARG(1,CELL_NUMBER,B_ARRAYSIZE);

  maybe_expand_array(tsk,refpntr);

  n = (int)npntr;
  assert(0 <= n);


  if (CELL_CONS == pntrtype(refpntr)) {
    #ifdef ARRAY_DEBUG2
    fprintf(tsk->output,"[as %d,cons]\n",n);
    #endif
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
    #ifdef ARRAY_DEBUG2
    fprintf(tsk->output,"[as %d,%d]\n",n,arr->size);
    #endif

    if (index+n == arr->size)
      argstack[0] = arr->tail;
    else
      make_aref_pntr(argstack[0],arr->wrapper,index+n);
  }
  else {
    set_error(tsk,"arrayskip: expected aref or cons, got %s",cell_types[pntrtype(refpntr)]);
  }
}

void b_arrayprefix(task *tsk, pntr *argstack)
{
  pntr npntr = argstack[2];
  pntr refpntr = argstack[1];
  pntr restpntr = argstack[0];
  int n;
  carray *prefix;

  CHECK_ARG(2,CELL_NUMBER,B_ARRAYPREFIX);
  n = (int)npntr;

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

    arrdebug(tsk,"arrayprefix: index = %d, arr->size = %d, n = %d\n",index,arr->size,n);

    if (0 >= n) {
      argstack[0] = tsk->globnilpntr;
      return;
    }

    if (n > arr->size-index)
      n = arr->size-index;

    prefix = carray_new(tsk,arr->elemsize,NULL,NULL);
    prefix->tail = restpntr;
    make_aref_pntr(argstack[0],prefix->wrapper,0);
    carray_append(tsk,&prefix,arr->elements+index*arr->elemsize,n,arr->elemsize);
  }
  else {
    set_error(tsk,"arrayprefix: expected aref or cons, got %s",cell_types[pntrtype(refpntr)]);
  }
}

void b_isvalarray(task *tsk, pntr *argstack)
{
  pntr p = argstack[0];

  maybe_expand_array(tsk,p);

  if ((CELL_AREF == pntrtype(p)) && (1 == aref_array(p)->elemsize))
    argstack[0] = tsk->globtruepntr;
  else
    argstack[0] = tsk->globnilpntr;
}

void b_printarray(task *tsk, pntr *argstack)
{
  carray *arr;
  int index;
  int i;
  int n;

  CHECK_ARG(1,CELL_NUMBER,B_PRINTARRAY);
  CHECK_ARG(0,CELL_AREF,B_PRINTARRAY);

  n = (int)argstack[1];
  arr = aref_array(argstack[0]);
  index = aref_index(argstack[0]);

  assert(1 <= n);
  assert(index+n <= arr->size);

#ifdef PRINT_DEBUG
  printf("\n        >>>>>>>>>>> printarray: index = %d, arr->size = %d <<<<<<<<<<<\n",
         index,arr->size);
#endif

  if (1 == arr->elemsize) {
    char *str = (char*)malloc(n+1);
    memcpy(str,arr->elements+index,n);
    str[n] = '\0';
    fprintf(tsk->output,"%s",str);
    free(str);
  }
  else if (sizeof(pntr) == arr->elemsize) {
    for (i = 0; i < n; i++) {
      pntr p = ((pntr*)arr->elements)[index+i];
      printp(tsk->output,p);
    }
  }
  else {
    fatal("printarray: invalid array size");
  }
  fflush(tsk->output);

  argstack[0] = tsk->globnilpntr;
}

void b_numtostring(task *tsk, pntr *argstack)
{
  pntr p = argstack[0];
  char str[100];

  CHECK_ARG(0,CELL_NUMBER,B_NUMTOSTRING);
  format_double(str,100,p);
  argstack[0] = string_to_array(tsk,str);
}

void b_openfd(task *tsk, pntr *argstack)
{
  pntr filenamepntr = argstack[0];
  char *filename;
  int fd;

  /* FIXME: need to associate the fd with a garbage collectable wrapper so if the whole
     file isn't read it still gets closed */

  CHECK_ARG(0,CELL_AREF,B_OPENFD);

  filename = array_to_string(filenamepntr);

  if (0 > (fd = open(filename,O_RDONLY))) {
    set_error(tsk,"%s: %s",filename,strerror(errno));
    free(filename);
    return;
  }

/*   fprintf(tsk->output,"Opened %s for reading\n",filename); */
  argstack[0] = fd;
  free(filename);
}

void b_readchunk(task *tsk, pntr *argstack)
{
  pntr fdpntr = argstack[1];
  pntr nextpntr = argstack[0];
  int fd;
  int r;
  unsigned char buf[READBUFSIZE];
  carray *arr;

  CHECK_ARG(1,CELL_NUMBER,B_READCHUNK);
  fd = (int)fdpntr;

  r = read(fd,buf,READBUFSIZE);
  if (0 == r) {
/*     fprintf(tsk->output,"End of file\n"); */
    close(fd);
    argstack[0] = tsk->globnilpntr;
    return;
  }

/*   fprintf(tsk->output,"Read %d bytes\n",r); */
  arr = carray_new(tsk,1,NULL,NULL);
  make_aref_pntr(argstack[0],arr->wrapper,0);

  carray_append(tsk,&arr,buf,r,1);
  arr->tail = nextpntr;
}

void b_readdir(task *tsk, pntr *argstack)
{
  DIR *dir;
  char *path;
  pntr filenamepntr = argstack[0];
  struct dirent tmp;
  struct dirent *entry;
  carray *arr;

  CHECK_ARG(0,CELL_AREF,B_OPENFD);
  path = array_to_string(filenamepntr);

  if (NULL == (dir = opendir(path))) {
    set_error(tsk,"%s: %s",path,strerror(errno));
    free(path);
    return;
  }

  arr = carray_new(tsk,sizeof(pntr),NULL,NULL);
  make_aref_pntr(argstack[0],arr->wrapper,0);

  while ((0 == readdir_r(dir,&tmp,&entry)) && (NULL != entry)) {
    char *fullpath = (char*)malloc(strlen(path)+1+strlen(entry->d_name)+1);
    pntr namep;
    pntr typep;
    pntr sizep;
    pntr entryp;
    carray *entryarr;
    struct stat statbuf;
    char *type;

    if (!strcmp(entry->d_name,".") || !strcmp(entry->d_name,".."))
      continue;

    sprintf(fullpath,"%s/%s",path,entry->d_name);
    if (0 != stat(fullpath,&statbuf)) {
      set_error(tsk,"%s: %s",path,strerror(errno));
      closedir(dir);
      free(fullpath);
      free(path);
      return;
    }

    if (S_ISREG(statbuf.st_mode))
      type = "file";
    else if (S_ISDIR(statbuf.st_mode))
      type = "directory";
    else
      type = "other";

    namep = string_to_array(tsk,entry->d_name);
    typep = string_to_array(tsk,type);
    sizep = statbuf.st_size;

    entryarr = carray_new(tsk,sizeof(pntr),NULL,NULL);
    carray_append(tsk,&entryarr,&namep,1,sizeof(pntr));
    carray_append(tsk,&entryarr,&typep,1,sizeof(pntr));
    carray_append(tsk,&entryarr,&sizep,1,sizeof(pntr));
    make_aref_pntr(entryp,entryarr->wrapper,0);

    carray_append(tsk,&arr,&entryp,1,sizeof(pntr));

    free(fullpath);
  }
  closedir(dir);

  free(path);

}

void b_iscons(task *tsk, pntr *argstack)
{
  setbool(tsk,&argstack[0],
          (CELL_CONS == pntrtype(argstack[0])) || (CELL_AREF == pntrtype(argstack[0])));
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
{ "+",              2, 2, ALWAYS_VALUE, ALWAYS_TRUE, b_add            },
{ "-",              2, 2, ALWAYS_VALUE, ALWAYS_TRUE, b_subtract       },
{ "*",              2, 2, ALWAYS_VALUE, ALWAYS_TRUE, b_multiply       },
{ "/",              2, 2, ALWAYS_VALUE, ALWAYS_TRUE, b_divide         },
{ "%",              2, 2, ALWAYS_VALUE, ALWAYS_TRUE, b_mod            },

{ "=",              2, 2, ALWAYS_VALUE, MAYBE_FALSE, b_eq             },
{ "!=",             2, 2, ALWAYS_VALUE, MAYBE_FALSE, b_ne             },
{ "<",              2, 2, ALWAYS_VALUE, MAYBE_FALSE, b_lt             },
{ "<=",             2, 2, ALWAYS_VALUE, MAYBE_FALSE, b_le             },
{ ">",              2, 2, ALWAYS_VALUE, MAYBE_FALSE, b_gt             },
{ ">=",             2, 2, ALWAYS_VALUE, MAYBE_FALSE, b_ge             },

{ "and",            2, 2, ALWAYS_VALUE, MAYBE_FALSE, b_and            },
{ "or",             2, 2, ALWAYS_VALUE, MAYBE_FALSE, b_or             },
{ "not",            1, 1, ALWAYS_VALUE, MAYBE_FALSE, b_not            },

{ "<<",             2, 2, ALWAYS_VALUE, ALWAYS_TRUE, b_bitshl         },
{ ">>",             2, 2, ALWAYS_VALUE, ALWAYS_TRUE, b_bitshr         },
{ "&",              2, 2, ALWAYS_VALUE, ALWAYS_TRUE, b_bitand         },
{ "|",              2, 2, ALWAYS_VALUE, ALWAYS_TRUE, b_bitor          },
{ "^",              2, 2, ALWAYS_VALUE, ALWAYS_TRUE, b_bitxor         },
{ "~",              1, 1, ALWAYS_VALUE, ALWAYS_TRUE, b_bitnot         },

{ "sqrt",           1, 1, ALWAYS_VALUE, ALWAYS_TRUE, b_sqrt           },
{ "floor",          1, 1, ALWAYS_VALUE, ALWAYS_TRUE, b_floor          },
{ "ceil",           1, 1, ALWAYS_VALUE, ALWAYS_TRUE, b_ceil           },

{ "seq",            2, 1, MAYBE_UNEVAL, MAYBE_FALSE, b_seq            },
{ "par",            2, 0, MAYBE_UNEVAL, MAYBE_FALSE, b_par            },
{ "parhead",        2, 1, MAYBE_UNEVAL, MAYBE_FALSE, b_parhead        },

{ "echo",           1, 1, ALWAYS_VALUE, MAYBE_FALSE, b_echo           },
{ "print",          1, 1, ALWAYS_VALUE, MAYBE_FALSE, b_print          },

{ "if",             3, 1, MAYBE_UNEVAL, MAYBE_FALSE, b_if             },
{ "cons",           2, 0, ALWAYS_VALUE, ALWAYS_TRUE, b_cons           },
{ "head",           1, 1, MAYBE_UNEVAL, MAYBE_FALSE, b_head           },
{ "tail",           1, 1, MAYBE_UNEVAL, MAYBE_FALSE, b_tail           },

{ "arraysize",      1, 1, MAYBE_UNEVAL, ALWAYS_TRUE, b_arraysize      },
{ "arrayskip",      2, 2, MAYBE_UNEVAL, MAYBE_FALSE, b_arrayskip      },
{ "arrayprefix",    3, 2, MAYBE_UNEVAL, MAYBE_FALSE, b_arrayprefix    },

{ "valarray?",      1, 1, ALWAYS_VALUE, MAYBE_FALSE, b_isvalarray     },
{ "printarray",     2, 2, ALWAYS_VALUE, MAYBE_FALSE, b_printarray     },

{ "numtostring",    1, 1, MAYBE_UNEVAL, ALWAYS_TRUE, b_numtostring    },
{ "openfd",         1, 1, MAYBE_UNEVAL, MAYBE_FALSE, b_openfd         },
{ "readchunk",      2, 1, MAYBE_UNEVAL, MAYBE_FALSE, b_readchunk      },
{ "readdir",        1, 1, MAYBE_UNEVAL, MAYBE_FALSE, b_readdir        },

{ "cons?",          1, 1, ALWAYS_VALUE, MAYBE_FALSE, b_iscons         },

};

