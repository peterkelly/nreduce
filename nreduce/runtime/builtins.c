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

static const char *numnames[4] = {"first", "second", "third", "fourth"};

#define CHECK_ARG(_argno,_type,_bif) {                                  \
    if ((_type) != pntrtype(argstack[(_argno)])) {                      \
      set_error(proc,"%s: %s argument must be of type %s, but got %s",  \
                builtin_info[(_bif)].name,                              \
                numnames[builtin_info[(_bif)].nargs-1-(_argno)],        \
                cell_types[(_type)],                                    \
                cell_types[pntrtype(argstack[(_argno)])]);              \
      return;                                                           \
    }                                                                   \
  }





void arrdebug(process *proc, const char *format, ...)
{
#ifdef ARRAY_DEBUG
  va_list ap;
  va_start(ap,format);
  vfprintf(proc->output,format,ap);
  va_end(ap);
#endif
}



const builtin builtin_info[NUM_BUILTINS];

static void setnumber(pntr *cptr, double val)
{
  *cptr = val;
}

static void setbool(process *proc, pntr *cptr, int b)
{
  *cptr = (b ? proc->globtruepntr : proc->globnilpntr);
}

static void doubleop(process *proc, pntr *cptr, int bif, double a, double b)
{
  switch (bif) {
  case B_ADD:       setnumber(cptr,a +  b);  break;
  case B_SUBTRACT:  setnumber(cptr,a -  b);  break;
  case B_MULTIPLY:  setnumber(cptr,a *  b);  break;
  case B_DIVIDE:    if (0.0 == b) {
                      fprintf(stderr,"Divide by zero\n");
                      exit(1);
                    }
                    setnumber(cptr,a /  b);  break;
  case B_MOD:       setnumber(cptr,fmod(a,b));  break;
  case B_EQ:       setbool(proc,cptr,a == b);  break;
  case B_NE:       setbool(proc,cptr,a != b);  break;
  case B_LT:       setbool(proc,cptr,a <  b);  break;
  case B_LE:       setbool(proc,cptr,a <= b);  break;
  case B_GT:       setbool(proc,cptr,a >  b);  break;
  case B_GE:       setbool(proc,cptr,a >= b);  break;
  default:         abort();        break;
  }
}

static void b_other(process *proc, pntr *argstack, int bif)
{
  pntr val2 = argstack[0];
  pntr val1 = argstack[1];

  switch (bif) {
  case B_EQ:
  case B_NE:
  case B_LT:
  case B_LE:
  case B_GT:
  case B_GE:
    /* FIXME: make this work for string/array/list comparisons */
/*     if ((CELL_STRING == pntrtype(val1)) && (CELL_STRING == pntrtype(val2))) { */
/*       const char *str1 = (const char*)get_pntr(get_pntr(val1)->field1); */
/*       const char *str2 = (const char*)get_pntr(get_pntr(val2)->field1); */
/*       int cmp = strcmp(str1,str2); */
/*       switch (bif) { */
/*       case B_EQ: setbool(proc,&argstack[0],0 == cmp); break; */
/*       case B_NE: setbool(proc,&argstack[0],0 != cmp); break; */
/*       case B_LT: setbool(proc,&argstack[0],0 > cmp); break; */
/*       case B_LE: setbool(proc,&argstack[0],0 >= cmp); break; */
/*       case B_GT: setbool(proc,&argstack[0],0 < cmp); break; */
/*       case B_GE: setbool(proc,&argstack[0],0 <= cmp); break; */
/*       } */
/*       return; */
/*     } */
    /* fall through */
  case B_ADD:
  case B_SUBTRACT:
  case B_MULTIPLY:
  case B_DIVIDE:
  case B_MOD:
    if ((CELL_NUMBER == pntrtype(val1)) && (CELL_NUMBER == pntrtype(val2))) {
      doubleop(proc,&argstack[0],bif,val1,val2);
    }
    else {
      int t1 = pntrtype(val1);
      int t2 = pntrtype(val2);
      set_error(proc,"%s: incompatible arguments: (%s,%s)",
                      builtin_info[bif].name,cell_types[t1],cell_types[t2]);
    }
    break;
  case B_AND:
    setbool(proc,&argstack[0],(CELL_NIL != pntrtype(val1)) && (CELL_NIL != pntrtype(val2)));
    break;
  case B_OR:
    setbool(proc,&argstack[0],(CELL_NIL != pntrtype(val1)) || (CELL_NIL != pntrtype(val2)));
    break;
  default:
    abort();
  }
}

static void b_add(process *proc, pntr *argstack) { b_other(proc,argstack,B_ADD); }
static void b_subtract(process *proc, pntr *argstack) { b_other(proc,argstack,B_SUBTRACT); }
static void b_multiply(process *proc, pntr *argstack) { b_other(proc,argstack,B_MULTIPLY); }
static void b_divide(process *proc, pntr *argstack) { b_other(proc,argstack,B_DIVIDE); }
static void b_mod(process *proc, pntr *argstack) { b_other(proc,argstack,B_MOD); }
static void b_eq(process *proc, pntr *argstack) { b_other(proc,argstack,B_EQ); }
static void b_ne(process *proc, pntr *argstack) { b_other(proc,argstack,B_NE); }
static void b_lt(process *proc, pntr *argstack) { b_other(proc,argstack,B_LT); }
static void b_le(process *proc, pntr *argstack) { b_other(proc,argstack,B_LE); }
static void b_gt(process *proc, pntr *argstack) { b_other(proc,argstack,B_GT); }
static void b_ge(process *proc, pntr *argstack) { b_other(proc,argstack,B_GE); }
static void b_and(process *proc, pntr *argstack) { b_other(proc,argstack,B_AND); }
static void b_or(process *proc, pntr *argstack) { b_other(proc,argstack,B_OR); }

static void b_not(process *proc, pntr *argstack)
{
  if (CELL_NIL == pntrtype(argstack[0]))
    argstack[0] = proc->globtruepntr;
  else
    argstack[0] = proc->globnilpntr;
}

static void b_bitop(process *proc, pntr *argstack, int bif)
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

static void b_bitshl(process *proc, pntr *argstack) { b_bitop(proc,argstack,B_BITSHL); }
static void b_bitshr(process *proc, pntr *argstack) { b_bitop(proc,argstack,B_BITSHR); }
static void b_bitand(process *proc, pntr *argstack) { b_bitop(proc,argstack,B_BITAND); }
static void b_bitor(process *proc, pntr *argstack) { b_bitop(proc,argstack,B_BITOR); }
static void b_bitxor(process *proc, pntr *argstack) { b_bitop(proc,argstack,B_BITXOR); }

static void b_bitnot(process *proc, pntr *argstack)
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

static void b_sqrt(process *proc, pntr *argstack)
{
  CHECK_ARG(0,CELL_NUMBER,B_SQRT);
  setnumber(&argstack[0],sqrt(argstack[0]));
  /* FIXME: handle NaN result properly (it's not a pointer!) */
}

static void b_floor(process *proc, pntr *argstack)
{
  CHECK_ARG(0,CELL_NUMBER,B_FLOOR);
  setnumber(&argstack[0],floor(argstack[0]));
}

static void b_ceil(process *proc, pntr *argstack)
{
  CHECK_ARG(0,CELL_NUMBER,B_CEIL);
  setnumber(&argstack[0],ceil(argstack[0]));
}

static void b_seq(process *proc, pntr *argstack)
{
}

static void b_par(process *proc, pntr *argstack)
{
  pntr p = resolve_pntr(argstack[1]);
  if (CELL_FRAME == pntrtype(p)) {
    frame *f = (frame*)get_pntr(get_pntr(p)->field1);
    spark_frame(proc,f);
  }
  else {
/*     printf("not sparking - it's a %s\n",cell_types[pntrtype(p)]); */
  }
}

static void b_head(process *proc, pntr *argstack);
static void b_parhead(process *proc, pntr *argstack)
{
  b_head(proc,&argstack[1]);
  b_par(proc,argstack);
}

static void b_echo(process *proc, pntr *argstack)
{
  /* FIXME: remove */
/*   if (CELL_STRING == pntrtype(argstack[0])) */
/*     fprintf(proc->output,"%s",(char*)get_pntr(get_pntr(argstack[0])->field1)); */
/*   else */
/*     print_pntr(proc->output,argstack[0]); */
  argstack[0] = proc->globnilpntr;
}

static void printp(FILE *f, pntr p)
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

static void b_print(process *proc, pntr *argstack)
{
  pntr p = argstack[0];
#ifdef PRINT_DEBUG
  fprintf(proc->output,"p"); /* FIXME: temp */
#endif
  printp(proc->output,p);
  fflush(proc->output);

  argstack[0] = proc->globnilpntr;
}

static void b_if(process *proc, pntr *argstack)
{
  pntr ifc = argstack[2];
  pntr source;

  if (CELL_NIL == pntrtype(ifc))
    source = argstack[0];
  else
    source = argstack[1];

  argstack[0] = source;
}




























































carray *carray_new(process *proc, int dsize, carray *oldarr, cell *usewrapper)
{
  carray *arr = (carray*)calloc(1,sizeof(carray));
  arr->alloc = 8;
  arr->size = 0;
  arr->elemsize = dsize;
  arr->elements = (pntr*)calloc(arr->alloc,arr->elemsize);
  arr->tail = proc->globnilpntr;

  arr->wrapper = usewrapper ? usewrapper : alloc_cell(proc);
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

void convert_to_string(process *proc, carray *arr)
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

int check_array_convert(process *proc, carray *arr, const char *from)
{
  int printed = 0;
  if ((sizeof(pntr) == arr->elemsize) &&
      ((MAX_ARRAY_SIZE == arr->size) || (CELL_NIL == pntrtype(arr->tail)))) {
    int oldchars = arr->nchars;

    while ((arr->nchars < arr->size) &&
           pntr_is_char(((pntr*)arr->elements)[arr->nchars]))
      arr->nchars++;

    if (arr->nchars != oldchars) {
      arrdebug(proc,"%s: now arr->nchars = %d, was %d; array size = %d\n",
               from,arr->nchars,oldchars,arr->size);
      printed = 1;
    }

    if (arr->nchars == arr->size) {
      arrdebug(proc,"%s: converting %d-element array to string\n",from,arr->size);
      printed = 1;

      convert_to_string(proc,arr);
    }
  }
  return printed;
}

void carray_append(process *proc, carray **arr, const void *data, int totalcount, int dsize)
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

    check_array_convert(proc,*arr,"append");

    *arr = carray_new(proc,dsize,*arr,NULL);
  }
}

void maybe_expand_array(process *proc, pntr p)
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

      arrdebug(proc,"expand: replacing double cons %s, %s with array (secondtail is %s)\n",
               cell_types[pntrtype(firsthead)],cell_types[pntrtype(secondhead)],
               cell_types[pntrtype(secondtail)]);
      printed = 1;

      arr = carray_new(proc,sizeof(pntr),NULL,firstcell);
      carray_append(proc,&arr,&firsthead,1,sizeof(pntr));
      carray_append(proc,&arr,&secondhead,1,sizeof(pntr));
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
        carray_append(proc,&arr,&tailhead,1,sizeof(pntr));
        arr->tail = resolve_pntr(tailcell->field2);

        count++;
      }
      else {
        break;
      }
    }

    if (0 < count) {
      arrdebug(proc,"expand: appended %d new values from conses; nchars = %d, elemsize = %d\n",
               count,arr->nchars,arr->elemsize);
      printed = 1;
    }


    if (check_array_convert(proc,arr,"expand"))
      printed = 1;


  }


  if (printed) {
    arrdebug(proc,"expand: done\n");
  }
}

pntr string_to_array(process *proc, const char *str)
{
  /* FIXME: handle case where strlen(str) > MAX_ARRAY_SIZE */
  if (0 == strlen(str)) {
    return proc->globnilpntr;
  }
  else {
    carray *arr = carray_new(proc,1,NULL,NULL);
    pntr p;
    carray_append(proc,&arr,str,strlen(str),1);
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




















static void b_cons(process *proc, pntr *argstack)
{
  pntr head = argstack[1];
  pntr tail = argstack[0];

  cell *res = alloc_cell(proc);
  res->type = CELL_CONS;
  res->field1 = head;
  res->field2 = tail;

  make_pntr(argstack[0],res);
}

static void b_head(process *proc, pntr *argstack)
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
      set_error(proc,"head: invalid array size");
  }
  else {
    set_error(proc,"head: expected aref or cons, got %s",cell_types[pntrtype(arg)]);
  }
}

static void b_tail(process *proc, pntr *argstack)
{
  pntr arg = argstack[0];

  maybe_expand_array(proc,arg);

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
    set_error(proc,"tail: expected aref or cons, got %s",cell_types[pntrtype(arg)]);
  }
}

void b_arraysize(process *proc, pntr *argstack)
{
  pntr refpntr = argstack[0];
  maybe_expand_array(proc,refpntr);
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
    set_error(proc,"arraysize: expected aref or cons, got %s",cell_types[pntrtype(refpntr)]);
  }
}

void b_arrayskip(process *proc, pntr *argstack)
{
  pntr npntr = argstack[1];
  pntr refpntr = argstack[0];
  int n;

  CHECK_ARG(1,CELL_NUMBER,B_ARRAYSIZE);

  maybe_expand_array(proc,refpntr);

  n = (int)npntr;
  assert(0 <= n);


  if (CELL_CONS == pntrtype(refpntr)) {
    #ifdef ARRAY_DEBUG2
    fprintf(proc->output,"[as %d,cons]\n",n);
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
    fprintf(proc->output,"[as %d,%d]\n",n,arr->size);
    #endif

    if (index+n == arr->size)
      argstack[0] = arr->tail;
    else
      make_aref_pntr(argstack[0],arr->wrapper,index+n);
  }
  else {
    set_error(proc,"arrayskip: expected aref or cons, got %s",cell_types[pntrtype(refpntr)]);
  }
}

void b_arrayprefix(process *proc, pntr *argstack)
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

    newcons = alloc_cell(proc);
    newcons->type = CELL_CONS;
    newcons->field1 = get_pntr(refpntr)->field1;
    newcons->field2 = restpntr;

    make_pntr(argstack[0],newcons);
  }
  else if (CELL_AREF == pntrtype(refpntr)) {
    carray *arr = aref_array(refpntr);
    int index = aref_index(refpntr);

    arrdebug(proc,"arrayprefix: index = %d, arr->size = %d, n = %d\n",index,arr->size,n);

    if (0 >= n) {
      argstack[0] = proc->globnilpntr;
      return;
    }

    if (n > arr->size-index)
      n = arr->size-index;

    prefix = carray_new(proc,arr->elemsize,NULL,NULL);
    prefix->tail = restpntr;
    make_aref_pntr(argstack[0],prefix->wrapper,0);
    carray_append(proc,&prefix,arr->elements+index*arr->elemsize,n,arr->elemsize);
  }
  else {
    set_error(proc,"arrayprefix: expected aref or cons, got %s",cell_types[pntrtype(refpntr)]);
  }
}

void b_isvalarray(process *proc, pntr *argstack)
{
  pntr p = argstack[0];

  maybe_expand_array(proc,p);

  if ((CELL_AREF == pntrtype(p)) && (1 == aref_array(p)->elemsize))
    argstack[0] = proc->globtruepntr;
  else
    argstack[0] = proc->globnilpntr;
}

void b_printarray(process *proc, pntr *argstack)
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
    fprintf(proc->output,"%s",str);
    free(str);
  }
  else if (sizeof(pntr) == arr->elemsize) {
    for (i = 0; i < n; i++) {
      pntr p = ((pntr*)arr->elements)[index+i];
      printp(proc->output,p);
    }
  }
  else {
    fatal("printarray: invalid array size");
  }
  fflush(proc->output);

  argstack[0] = proc->globnilpntr;
}



















void b_numtostring(process *proc, pntr *argstack)
{
  pntr p = argstack[0];
  double d;
  char str[100];

  assert(CELL_NUMBER == pntrtype(p));

  d = p;

  sprintf(str,"%f",d);
  argstack[0] = string_to_array(proc,str);
}

/* FIXME: use a larger buffer size! */
#define READBUFSIZE 1024
void b_openfd(process *proc, pntr *argstack)
{
  pntr filenamepntr = argstack[0];
  char *filename;
  int fd;

  /* FIXME: need to associate the fd with a garbage collectable wrapper so if the whole
     file isn't read it still gets closed */

  CHECK_ARG(0,CELL_AREF,B_OPENFD);

  filename = array_to_string(filenamepntr);

  if (0 > (fd = open(filename,O_RDONLY))) {
    fprintf(stderr,"Can't open %s: %s\n",filename,strerror(errno));
    argstack[0] = proc->globnilpntr;
    return;
  }

/*   fprintf(proc->output,"Opened %s for reading\n",filename); */
  argstack[0] = fd;
}

void b_readchunk(process *proc, pntr *argstack)
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
/*     fprintf(proc->output,"End of file\n"); */
    close(fd);
    argstack[0] = proc->globnilpntr;
    return;
  }

/*   fprintf(proc->output,"Read %d bytes\n",r); */
  arr = carray_new(proc,1,NULL,NULL);
  make_aref_pntr(argstack[0],arr->wrapper,0);

  carray_append(proc,&arr,buf,r,1);
  arr->tail = nextpntr;
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
{ "+",              2, 2, 1, b_add            },
{ "-",              2, 2, 1, b_subtract       },
{ "*",              2, 2, 1, b_multiply       },
{ "/",              2, 2, 1, b_divide         },
{ "%",              2, 2, 1, b_mod            },

{ "=",              2, 2, 1, b_eq             },
{ "!=",             2, 2, 1, b_ne             },
{ "<",              2, 2, 1, b_lt             },
{ "<=",             2, 2, 1, b_le             },
{ ">",              2, 2, 1, b_gt             },
{ ">=",             2, 2, 1, b_ge             },

{ "and",            2, 2, 1, b_and            },
{ "or",             2, 2, 1, b_or             },
{ "not",            1, 1, 1, b_not            },

{ "<<",             2, 2, 1, b_bitshl         },
{ ">>",             2, 2, 1, b_bitshr         },
{ "&",              2, 2, 1, b_bitand         },
{ "|",              2, 2, 1, b_bitor          },
{ "^",              2, 2, 1, b_bitxor         },
{ "~",              1, 1, 1, b_bitnot         },

{ "sqrt",           1, 1, 1, b_sqrt           },
{ "floor",          1, 1, 1, b_floor          },
{ "ceil",           1, 1, 1, b_ceil           },

{ "seq",            2, 1, 0, b_seq            },
{ "par",            2, 0, 0, b_par            },
{ "parhead",        2, 1, 0, b_parhead        },

{ "echo",           1, 1, 1, b_echo           },
{ "print",          1, 1, 1, b_print          },

{ "if",             3, 1, 0, b_if             },
{ "cons",           2, 0, 1, b_cons           },
{ "head",           1, 1, 0, b_head           },
{ "tail",           1, 1, 0, b_tail           },

{ "arraysize",      1, 1, 0, b_arraysize      },
{ "arrayskip",      2, 2, 0, b_arrayskip      },
{ "arrayprefix",    3, 2, 0, b_arrayprefix    },

{ "valarray?",      1, 1, 1, b_isvalarray     },
{ "printarray",     2, 2, 1, b_printarray     },

{ "numtostring",    1, 1, 0, b_numtostring    },
{ "openfd",         1, 1, 0, b_openfd         },
{ "readchunk",      2, 1, 0, b_readchunk      },

};

