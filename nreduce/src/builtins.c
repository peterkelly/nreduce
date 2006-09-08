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

#include "grammar.tab.h"
#include "nreduce.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <assert.h>
#include <ctype.h>
#include <stdarg.h>
#include <math.h>
#include <gcode.h>

const builtin builtin_info[NUM_BUILTINS];

static void setnumber(pntr *cptr, double val)
{
  *cptr = make_number(val);
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
    if ((TYPE_STRING == pntrtype(val1)) && (TYPE_STRING == pntrtype(val2))) {
      const char *str1 = get_string(get_pntr(val1)->field1);
      const char *str2 = get_string(get_pntr(val2)->field1);
      int cmp = strcmp(str1,str2);
      switch (bif) {
      case B_EQ: setbool(proc,&argstack[0],0 == cmp); break;
      case B_NE: setbool(proc,&argstack[0],0 != cmp); break;
      case B_LT: setbool(proc,&argstack[0],0 > cmp); break;
      case B_LE: setbool(proc,&argstack[0],0 >= cmp); break;
      case B_GT: setbool(proc,&argstack[0],0 < cmp); break;
      case B_GE: setbool(proc,&argstack[0],0 <= cmp); break;
      }
      return;
    }
    /* fall through */
  case B_ADD:
  case B_SUBTRACT:
  case B_MULTIPLY:
  case B_DIVIDE:
  case B_MOD:
    if ((TYPE_NUMBER == pntrtype(val1)) && (TYPE_NUMBER == pntrtype(val2))) {
      doubleop(proc,&argstack[0],bif,pntrdouble(val1),pntrdouble(val2));
    }
    else {
      int t1 = pntrtype(val1);
      int t2 = pntrtype(val2);
      fprintf(stderr,"%s: incompatible arguments: (%s,%s)\n",
                      builtin_info[bif].name,cell_types[t1],cell_types[t2]);
      exit(1);
    }
    break;
  case B_AND:
    setbool(proc,&argstack[0],(TYPE_NIL != pntrtype(val1)) && (TYPE_NIL != pntrtype(val2)));
    break;
  case B_OR:
    setbool(proc,&argstack[0],(TYPE_NIL != pntrtype(val1)) || (TYPE_NIL != pntrtype(val2)));
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
  if (TYPE_NIL == pntrtype(argstack[0]))
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

  if ((TYPE_NUMBER != pntrtype(val1)) || (TYPE_NUMBER != pntrtype(val2))) {
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

static void b_bitshl(process *proc, pntr *argstack) { b_bitop(proc,argstack,B_BITSHL); }
static void b_bitshr(process *proc, pntr *argstack) { b_bitop(proc,argstack,B_BITSHR); }
static void b_bitand(process *proc, pntr *argstack) { b_bitop(proc,argstack,B_BITAND); }
static void b_bitor(process *proc, pntr *argstack) { b_bitop(proc,argstack,B_BITOR); }
static void b_bitxor(process *proc, pntr *argstack) { b_bitop(proc,argstack,B_BITXOR); }

static void b_bitnot(process *proc, pntr *argstack)
{
  pntr arg = argstack[0];
  int val;
  if (TYPE_NUMBER != pntrtype(arg)) {
    fprintf(stderr,"~: incompatible argument (must be a number)\n");
    exit(1);
  }
  val = (int)pntrdouble(arg);
  setnumber(&argstack[0],(double)(~val));
}

static void b_if(process *proc, pntr *argstack)
{
  pntr ifc = argstack[2];
  pntr source;

  if (TYPE_NIL == pntrtype(ifc))
    source = argstack[0];
  else
    source = argstack[1];

  argstack[0] = source;
}

static void b_cons(process *proc, pntr *argstack)
{
  pntr head = argstack[1];
  pntr tail = argstack[0];

  cell *res = alloc_cell(proc);
  res->type = TYPE_CONS;
  res->field1 = head;
  res->field2 = tail;

  make_pntr(argstack[0],res);
}

static void b_head(process *proc, pntr *argstack)
{
  pntr arg = argstack[0];
  if (TYPE_CONS == pntrtype(arg)) {
    argstack[0] = get_pntr(arg)->field1;
  }
  else if (TYPE_AREF == pntrtype(arg)) {
    cell *argval = get_pntr(arg);
    cell *arrholder = (cell*)get_pntr(argval->field1);
    carray *arr = (carray*)get_pntr(arrholder->field1);
    int index = (int)get_pntr(argval->field2);
    assert(index < arr->size);
    argstack[0] = arr->elements[index];
  }
  else {
    fatal("head: expected a cons value");
  }
}

void make_aref(cell *refval, cell *arrholder, int index)
{
  carray *arr = (carray*)get_pntr(arrholder->field1);
  refval->type = TYPE_AREF;
  make_pntr(refval->field1,arrholder);
  make_pntr(refval->field2,(void*)index);
  make_pntr(arr->refs[index],refval);
}

pntr get_aref(process *proc, cell *arrholder, int index)
{
  carray *arr = (carray*)get_pntr(arrholder->field1);
  if (is_nullpntr(arr->refs[index])) {
    cell *refval = alloc_cell(proc);
    make_aref(refval,arrholder,index);
  }
  return arr->refs[index];
}

static void b_tail(process *proc, pntr *argstack)
{
  pntr arg = argstack[0];
  if (TYPE_CONS == pntrtype(arg)) {
    cell *consval = get_pntr(arg);
    pntr conshead = resolve_pntr(consval->field1);
    pntr constail = resolve_pntr(consval->field2);
    cell *otherval;
    pntr otherhead;
    pntr othertail;
    carray *arr;
    cell *arrholder;

    /* TODO: handle the case where the tail is another array */
    if (TYPE_CONS != pntrtype(constail)) {
      argstack[0] = constail;
      return;
    }

    otherval = get_pntr(constail);
    otherhead = resolve_pntr(otherval->field1);
    othertail = resolve_pntr(otherval->field2);

    arr = (carray*)calloc(1,sizeof(carray));
    arr->alloc = 8;
    arr->size = 2;
    arr->elements = (pntr*)calloc(arr->alloc,sizeof(pntr));
    arr->refs = (pntr*)calloc(arr->alloc,sizeof(pntr));

    arr->elements[0] = conshead;
    arr->elements[1] = otherhead;
    arr->tail = othertail;

    arrholder = alloc_cell(proc);
    arrholder->type = TYPE_ARRAY;
    make_pntr(arrholder->field1,arr);

    make_aref(consval,arrholder,0);
    make_aref(otherval,arrholder,1);

    argstack[0] = arr->refs[1];
  }
  else if (TYPE_AREF == pntrtype(arg)) {
    cell *argval = get_pntr(arg);
    cell *arrholder = (cell*)get_pntr(argval->field1);
    int index = (int)get_pntr(argval->field2);
    carray *arr = (carray*)get_pntr(arrholder->field1);
    assert(index < arr->size);
    if (index+1 == arr->size) {
      pntr other = resolve_pntr(arr->tail);
      cell *otherval;
      pntr otherhead;
      pntr othertail;

      /* TODO: handle the case where the tail is another array */
      if (TYPE_CONS != pntrtype(other)) {
        argstack[0] = other;
        return;
      }

      otherval = get_pntr(other);
      otherhead = otherval->field1;
      othertail = otherval->field2;

      if (arr->alloc < arr->size+1) {
        arr->alloc *= 2;
        arr->elements = (pntr*)realloc(arr->elements,arr->alloc*sizeof(pntr));
        arr->refs = (pntr*)realloc(arr->refs,arr->alloc*sizeof(pntr));
      }

      arr->elements[arr->size] = otherhead;
      arr->tail = othertail;
      make_aref(otherval,arrholder,arr->size);
      arr->size++;

      argstack[0] = other;
      return;
    }
    else {
      argstack[0] = get_aref(proc,arrholder,index+1);
      return;
    }
  }
  else {
    fatal("tail: expected a cons value");
  }
}

static void b_islambda(process *proc, pntr *argstack)
{
  setbool(proc,&argstack[0],0);
}

static void b_isvalue(process *proc, pntr *argstack)
{
  pntr val = argstack[0];
  setbool(proc,&argstack[0],isvaluetype(pntrtype(val)));
}

static void b_iscons(process *proc, pntr *argstack)
{
  pntr val = argstack[0];
  setbool(proc,&argstack[0],(TYPE_CONS == pntrtype(val)) || (TYPE_AREF == pntrtype(val)));
}

static void b_isnil(process *proc, pntr *argstack)
{
  pntr val = argstack[0];
  setbool(proc,&argstack[0],TYPE_NIL == pntrtype(val));
}

static void b_isnumber(process *proc, pntr *argstack)
{
  pntr val = argstack[0];
  setbool(proc,&argstack[0],TYPE_NUMBER == pntrtype(val));
}

static void b_isstring(process *proc, pntr *argstack)
{
  pntr val = argstack[0];
  setbool(proc,&argstack[0],TYPE_STRING == pntrtype(val));
}

static void b_arrayitem(process *proc, pntr *argstack)
{
  pntr refpntr = argstack[0];
  pntr indexpntr = argstack[1];
  int index;
  cell *arrholder;
  carray *arr;
  int refindex;

  if (TYPE_NUMBER != pntrtype(indexpntr)) {
    printf("arrayitem: index must be a number, got ");
    print_pntr(proc->output,indexpntr);
    printf("\n");
    abort();
  }
  if (TYPE_AREF != pntrtype(refpntr)) {
    printf("arrayitem: expected an array reference, got ");
    print_pntr(proc->output,refpntr);
    printf("\n");
    abort();
  }

  index = (int)pntrdouble(indexpntr);

  arrholder = get_pntr(get_pntr(refpntr)->field1);
  arr = (carray*)get_pntr(arrholder->field1);
  refindex = (int)get_pntr(get_pntr(refpntr)->field2);
  assert(refindex+index < arr->size);
  argstack[0] = arr->elements[refindex+index];
}

static void b_arrayhas(process *proc, pntr *argstack)
{
  pntr refpntr = argstack[0];
  pntr indexpntr = argstack[1];
  int index;
  cell *arrholder;
  carray *arr;
  int refindex;

  if (TYPE_NUMBER != pntrtype(indexpntr)) {
    printf("arrayhas: index must be a number, got ");
    print_pntr(proc->output,indexpntr);
    printf("\n");
    abort();
  }

  if (TYPE_AREF != pntrtype(refpntr)) {
    argstack[0] = proc->globnilpntr;
    return;
  }

  index = (int)pntrdouble(indexpntr);

  arrholder = get_pntr(get_pntr(refpntr)->field1);
  arr = (carray*)get_pntr(arrholder->field1);
  refindex = (int)get_pntr(get_pntr(refpntr)->field2);
  if (refindex+index < arr->size)
    argstack[0] = proc->globtruepntr;
  else
    argstack[0] = proc->globnilpntr;
}

static void b_arrayremsize(process *proc, pntr *argstack)
{
  pntr refpntr = argstack[1];
  pntr npntr = argstack[0];
  int n;

  if (TYPE_NUMBER != pntrtype(npntr)) {
    printf("arrayremsize: index must be a number, got ");
    print_pntr(proc->output,npntr);
    printf("\n");
    abort();
  }

  n = (int)pntrdouble(npntr);

  if (TYPE_AREF == pntrtype(refpntr)) {
    cell *arrholder = get_pntr(get_pntr(refpntr)->field1);
    carray *arr;
    int result;
    assert(0 == (int)get_pntr(get_pntr(refpntr)->field2));
    arr = (carray*)get_pntr(arrholder->field1);
    result = n-arr->size+1;

    assert(0 < arr->size);
    assert(0 <= result);
    argstack[0] = make_number(result);
  }
  else if (TYPE_CONS == pntrtype(refpntr)) {
    argstack[0] = make_number(n);
  }
  else {
    printf("arrayremsize: expected aref or cons, got ");
    print_pntr(proc->output,refpntr);
    printf("\n");
    abort();
  }
}

static void b_arrayrem(process *proc, pntr *argstack)
{
  pntr refpntr = argstack[0];

  if (TYPE_AREF == pntrtype(refpntr)) {
    cell *arrholder = get_pntr(get_pntr(refpntr)->field1);
    carray *arr;
    assert(0 == (int)get_pntr(get_pntr(refpntr)->field2));
    arr = (carray*)get_pntr(arrholder->field1);
    assert(0 < arr->size);

    argstack[0] = arr->refs[arr->size-1];
  }
  else if (TYPE_CONS == pntrtype(refpntr)) {
    argstack[0] = refpntr;
  }
  else {
    printf("arrayrem: expected aref or cons, got ");
    print_pntr(proc->output,refpntr);
    printf("\n");
    abort();
  }
}

static void b_arrayoptlen(process *proc, pntr *argstack)
{
  pntr refpntr = argstack[0];
  if (TYPE_AREF == pntrtype(refpntr)) {
    cell *arrholder = get_pntr(get_pntr(refpntr)->field1);
    carray *arr;
    assert(0 == (int)get_pntr(get_pntr(refpntr)->field2));
    arr = (carray*)get_pntr(arrholder->field1);

    arr->tail = resolve_pntr(arr->tail);

    if (TYPE_NIL == pntrtype(arr->tail))
      argstack[0] = make_number(arr->size);
    else
      argstack[0] = proc->globnilpntr;
  }
  else if (TYPE_CONS == pntrtype(refpntr)) {
    argstack[0] = proc->globnilpntr;
  }
  else {
    printf("arrayoptlen: expected aref or cons, got ");
    print_pntr(proc->output,refpntr);
    printf("\n");
    abort();
  }
}

static void b_echo(process *proc, pntr *argstack)
{
  if (TYPE_STRING == pntrtype(argstack[0]))
    fprintf(proc->output,"%s",get_string(get_pntr(argstack[0])->field1));
  else
    print_pntr(proc->output,argstack[0]);
  argstack[0] = proc->globnilpntr;
}

static void b_print(process *proc, pntr *argstack)
{
  pntr p = argstack[0];

  //  fprintf(proc->output,"print: "); /* TEMP - distinguish output from debug */

  if (trace)
    fprintf(proc->output,"<============ ");

  if (TYPE_AREF == pntrtype(p)) {
    print_pntr(proc->output,p);
    fprintf(proc->output,"\n");
    return;
  }

  if (!isvaluetype(pntrtype(p))) {
    fprintf(proc->output,"print: encountered %s\n",cell_types[pntrtype(p)]);
    abort();
  }
  assert(isvaluetype(pntrtype(p)));
  switch (pntrtype(p)) {
  case TYPE_NIL:
    break;
  case TYPE_NUMBER:
    print_double(proc->output,pntrdouble(p));
    break;
  case TYPE_STRING:
    fprintf(proc->output,"%s",get_string(get_pntr(p)->field1));
    break;
  }
  if (trace)
    fprintf(proc->output,"\n");
  fflush(proc->output);

  //  fprintf(proc->output,"\n"); /* TEMP - distinguish output from debug */
  argstack[0] = proc->globnilpntr;
}

static void b_sqrt(process *proc, pntr *argstack)
{
  double d = 0.0;

  pntr p = argstack[0];
  if (TYPE_NUMBER == pntrtype(p)) {
    d = pntrdouble(p);
  }
  else {
    fprintf(stderr,"sqrt: incompatible argument: %s\n",cell_types[pntrtype(p)]);
    exit(1);
  }

  setnumber(&argstack[0],sqrt(d));
}

static void b_floor(process *proc, pntr *argstack)
{
  double d = 0.0;

  pntr p = argstack[0];
  if (TYPE_NUMBER == pntrtype(p)) {
    d = pntrdouble(p);
  }
  else {
    fprintf(stderr,"floor: incompatible argument: %s\n",cell_types[pntrtype(p)]);
    exit(1);
  }

  setnumber(&argstack[0],floor(d));
}

static void b_ceil(process *proc, pntr *argstack)
{
  double d = 0.0;

  pntr p = argstack[0];
  if (TYPE_NUMBER == pntrtype(p)) {
    d = pntrdouble(p);
  }
  else {
    fprintf(stderr,"ceil: incompatible argument: %s\n",cell_types[pntrtype(p)]);
    exit(1);
  }

  setnumber(&argstack[0],ceil(d));
}

static void b_seq(process *proc, pntr *argstack)
{
}

static void b_par(process *proc, pntr *argstack)
{
  pntr p = resolve_pntr(argstack[1]);
  if (TYPE_FRAME == pntrtype(p)) {
    frame *f = (frame*)get_pntr(get_pntr(p)->field1);
    spark_frame(proc,f);
  }
  else {
/*     printf("not sparking - it's a %s\n",cell_types[pntrtype(p)]); */
  }
}

static void b_parhead(process *proc, pntr *argstack)
{
  b_head(proc,&argstack[1]);
  b_par(proc,argstack);
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

{ "if",             3, 1, 0, b_if             },
{ "cons",           2, 0, 1, b_cons           },
{ "head",           1, 1, 0, b_head           },
{ "tail",           1, 1, 0, b_tail           },

{ "lambda?",        1, 1, 1, b_islambda       },
{ "value?",         1, 1, 1, b_isvalue        },
{ "cons?",          1, 1, 1, b_iscons         },
{ "nil?",           1, 1, 1, b_isnil          },
{ "number?",        1, 1, 1, b_isnumber       },
{ "string?",        1, 1, 1, b_isstring       },

{ "arrayitem",      2, 2, 0, b_arrayitem      },
{ "arrayhas",       2, 2, 1, b_arrayhas       },
{ "arrayremsize",   2, 2, 1, b_arrayremsize   },
{ "arrayrem",       1, 1, 1, b_arrayrem       },
{ "arrayoptlen",    1, 1, 1, b_arrayoptlen    },

{ "echo",           1, 1, 1, b_echo           },
{ "print",          1, 1, 1, b_print          },

{ "sqrt",           1, 1, 1, b_sqrt           },
{ "floor",          1, 1, 1, b_floor          },
{ "ceil",           1, 1, 1, b_ceil           },

{ "seq",            2, 1, 0, b_seq            },
{ "par",            2, 0, 0, b_par            },
{ "parhead",        2, 1, 0, b_parhead        },

};

