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

const builtin builtin_info[NUM_BUILTINS];

void setnumber(pntr *cptr, double val)
{
  *cptr = make_number(val);
}

void setbool(pntr *cptr, int b)
{
  *cptr = (b ? globtruepntr : globnilpntr);
}

void doubleop(pntr *cptr, int bif, double a, double b)
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
  case B_EQ:       setbool(cptr,a == b);  break;
  case B_NE:       setbool(cptr,a != b);  break;
  case B_LT:       setbool(cptr,a <  b);  break;
  case B_LE:       setbool(cptr,a <= b);  break;
  case B_GT:       setbool(cptr,a >  b);  break;
  case B_GE:       setbool(cptr,a >= b);  break;
  default:         assert(0);        break;
  }
}

void b_other(pntr *argstack, int bif)
{
  pntr cell2 = argstack[0];
  pntr cell1 = argstack[1];

  switch (bif) {
  case B_EQ:
  case B_NE:
  case B_LT:
  case B_LE:
  case B_GT:
  case B_GE:
    if ((TYPE_STRING == pntrtype(cell1)) && (TYPE_STRING == pntrtype(cell2))) {
      int cmp = strcmp(get_string(get_pntr(cell1)->cmp1),get_string(get_pntr(cell2)->cmp2));
      switch (bif) {
      case B_EQ: setbool(&argstack[0],0 == cmp); break;
      case B_NE: setbool(&argstack[0],0 != cmp); break;
      case B_LT: setbool(&argstack[0],0 > cmp); break;
      case B_LE: setbool(&argstack[0],0 >= cmp); break;
      case B_GT: setbool(&argstack[0],0 < cmp); break;
      case B_GE: setbool(&argstack[0],0 <= cmp); break;
      }
      return;
    }
    /* fall through */
  case B_ADD:
  case B_SUBTRACT:
  case B_MULTIPLY:
  case B_DIVIDE:
  case B_MOD:
    if ((TYPE_NUMBER == pntrtype(cell1)) && (TYPE_NUMBER == pntrtype(cell2))) {
      doubleop(&argstack[0],bif,pntrdouble(cell1),pntrdouble(cell2));
    }
    else {
      fprintf(stderr,"%s: incompatible arguments: (%s,%s)\n",
                      builtin_info[bif].name,
                      cell_types[pntrtype(cell1)],
                      cell_types[pntrtype(cell2)]);
      exit(1);
    }
    break;
  case B_AND:
    setbool(&argstack[0],(TYPE_NIL != pntrtype(cell1)) && (TYPE_NIL != pntrtype(cell2)));
    break;
  case B_OR:
    setbool(&argstack[0],(TYPE_NIL != pntrtype(cell1)) || (TYPE_NIL != pntrtype(cell2)));
    break;
  default:
    assert(0);
  }
}

void b_add(pntr *argstack) { b_other(argstack,B_ADD); }
void b_subtract(pntr *argstack) { b_other(argstack,B_SUBTRACT); }
void b_multiply(pntr *argstack) { b_other(argstack,B_MULTIPLY); }
void b_divide(pntr *argstack) { b_other(argstack,B_DIVIDE); }
void b_mod(pntr *argstack) { b_other(argstack,B_MOD); }
void b_eq(pntr *argstack) { b_other(argstack,B_EQ); }
void b_ne(pntr *argstack) { b_other(argstack,B_NE); }
void b_lt(pntr *argstack) { b_other(argstack,B_LT); }
void b_le(pntr *argstack) { b_other(argstack,B_LE); }
void b_gt(pntr *argstack) { b_other(argstack,B_GT); }
void b_ge(pntr *argstack) { b_other(argstack,B_GE); }
void b_and(pntr *argstack) { b_other(argstack,B_AND); }
void b_or(pntr *argstack) { b_other(argstack,B_OR); }

void b_not(pntr *argstack)
{
  if (TYPE_NIL == pntrtype(argstack[0]))
    argstack[0] = globtruepntr;
  else
    argstack[0] = globnilpntr;
}

void b_bitop(pntr *argstack, int bif)
{
  pntr cell2 = argstack[0];
  pntr cell1 = argstack[1];
  int a;
  int b;

  if ((TYPE_NUMBER != pntrtype(cell1)) || (TYPE_NUMBER != pntrtype(cell2))) {
    fprintf(stderr,"%s: incompatible arguments (must be numbers)\n",builtin_info[bif].name);
    exit(1);
  }

  a = (int)pntrdouble(cell1);
  b = (int)pntrdouble(cell2);

  switch (bif) {
  case B_BITSHL:  setnumber(&argstack[0],(double)(a << b));  break;
  case B_BITSHR:  setnumber(&argstack[0],(double)(a >> b));  break;
  case B_BITAND:  setnumber(&argstack[0],(double)(a & b));   break;
  case B_BITOR:   setnumber(&argstack[0],(double)(a | b));   break;
  case B_BITXOR:  setnumber(&argstack[0],(double)(a ^ b));   break;
  default:        assert(0);       break;
  }
}

void b_bitshl(pntr *argstack) { b_bitop(argstack,B_BITSHL); }
void b_bitshr(pntr *argstack) { b_bitop(argstack,B_BITSHR); }
void b_bitand(pntr *argstack) { b_bitop(argstack,B_BITAND); }
void b_bitor(pntr *argstack) { b_bitop(argstack,B_BITOR); }
void b_bitxor(pntr *argstack) { b_bitop(argstack,B_BITXOR); }

void b_bitnot(pntr *argstack)
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

void b_if(pntr *argstack)
{
  pntr ifc = argstack[2];
  pntr source;

  if (TYPE_NIL == pntrtype(ifc))
    source = argstack[0];
  else
    source = argstack[1];

  argstack[0] = source;
}

void b_cons(pntr *argstack)
{
  pntr head = argstack[1];
  pntr tail = argstack[0];

  rtvalue *res = alloc_rtvalue();
  res->tag = TYPE_CONS;
  res->cmp1 = head;
  res->cmp2 = tail;

  make_pntr(argstack[0],res);
}

void b_head(pntr *argstack)
{
  pntr arg = argstack[0];
  if (TYPE_CONS == pntrtype(arg)) {
    argstack[0] = get_pntr(arg)->cmp1;
  }
  else if (TYPE_AREF == pntrtype(arg)) {
    rtvalue *argval = get_pntr(arg);
    rtvalue *arrcell = (rtvalue*)get_pntr(argval->cmp1);
    carray *arr = (carray*)get_pntr(arrcell->cmp1);
    int index = (int)get_pntr(argval->cmp2);
    assert(index < arr->size);
    argstack[0] = arr->cells[index];
  }
  else {
    assert(!"head: expected a cons cell");
  }
}

pntr get_tail(rtvalue *arrcell, int index)
{
  carray *arr = (carray*)get_pntr(arrcell->cmp1);
  assert(index < arr->size);
  if (index+1 == arr->size) {
    return arr->tail;
  }
  else {
    pntr ref = arr->refs[index+1];
    if (is_nullpntr(ref)) {
      rtvalue *refval = alloc_rtvalue();
      refval->tag = TYPE_AREF;
      make_pntr(refval->cmp1,arrcell);
      make_pntr(refval->cmp2,(void*)(index+1));
      make_pntr(ref,refval);
      arr->refs[index+1] = ref;
      assert(index+1 < arr->size);
    }
    return ref;
  }
}

void b_tail(pntr *argstack)
{
  pntr arg = argstack[0];
  if (TYPE_CONS == pntrtype(arg)) {
    argstack[0] = get_pntr(arg)->cmp2;
  }
  else if (TYPE_AREF == pntrtype(arg)) {
    rtvalue *argval = get_pntr(arg);
    rtvalue *arrcell = (rtvalue*)get_pntr(argval->cmp1);
    int index = (int)get_pntr(argval->cmp2);
    argstack[0] = get_tail(arrcell,index);
  }
  else {
    assert(!"tail: expected a cons cell");
  }
}

void b_islambda(pntr *argstack)
{
  pntr val = argstack[0];
  setbool(&argstack[0],TYPE_LAMBDA == pntrtype(val));
}

void b_isvalue(pntr *argstack)
{
  pntr val = argstack[0];
  setbool(&argstack[0],isvaluetype(pntrtype(val)));
}

void b_iscons(pntr *argstack)
{
  pntr val = argstack[0];
  setbool(&argstack[0],(TYPE_CONS == pntrtype(val)) || (TYPE_AREF == pntrtype(val)));
}

void b_isnil(pntr *argstack)
{
  pntr val = argstack[0];
  setbool(&argstack[0],TYPE_NIL == pntrtype(val));
}

void b_isnumber(pntr *argstack)
{
  pntr val = argstack[0];
  setbool(&argstack[0],TYPE_NUMBER == pntrtype(val));
}

void b_isstring(pntr *argstack)
{
  pntr val = argstack[0];
  setbool(&argstack[0],TYPE_STRING == pntrtype(val));
}

void b_convertarray(pntr *argstack)
{
  pntr ret = argstack[1];
  pntr top;
  rtvalue *topval;
  int size;
  pntr p;
  carray *arr;
  int i;
  rtvalue *arrcell;

  if (TYPE_NUMBER != pntrtype(ret)) {
    printf("convertarray: first arg should be a number, got ");
    print_pntr(ret);
    printf("\n");
    abort();
  }

  // FIXME: this doesn't correctly handle cyclic lists at present!
  top = argstack[0];
  size = 0;

  if (TYPE_AREF == pntrtype(top)) {
    argstack[0] = ret;
    return;
  }

  p = top;
  while (TYPE_CONS == pntrtype(p)) {
    p = resolve_pntr(get_pntr(p)->cmp2);
    size++;
  }

  if (TYPE_APPLICATION == pntrtype(p)) {
    printf("convertarray: list is not fully evaluated\n");
    abort();
  }
  else if (TYPE_NIL != pntrtype(p)) {
    printf("convertarray: found non-cons within list: ");
    print_pntr(p);
    printf("\n");
    abort();
  }

  arr = (carray*)calloc(1,sizeof(carray));
  arr->alloc = size;
  arr->size = size;
  arr->cells = (pntr*)calloc(arr->alloc,sizeof(pntr));
  arr->refs = (pntr*)calloc(arr->alloc,sizeof(pntr));
  for (i = 0; i < arr->alloc; i++) {
    make_pntr(arr->cells[i],NULL);
    make_pntr(arr->refs[i],NULL);
  }
  arr->tail = globnilpntr;
  make_pntr(arr->sizecell,NULL);

  p = top;
  for (i = 0; i < arr->size; i++) {
    arr->cells[i] = get_pntr(p)->cmp1;
    p = resolve_pntr(get_pntr(p)->cmp2);
  }

  arrcell = alloc_rtvalue();
  arrcell->tag = TYPE_ARRAY;
  make_pntr(arrcell->cmp1,arr);

  // return value is what is passed in, but now it points to an array
  assert(is_pntr(top));
  topval = get_pntr(top);
  topval->tag = TYPE_AREF;
  make_pntr(topval->cmp1,arrcell);
  make_pntr(topval->cmp2,0);
  make_pntr(arr->refs[0],topval);

  argstack[0] = ret;
}

void b_arrayitem(pntr *argstack)
{
  pntr refpntr = argstack[0];
  pntr indexpntr = argstack[1];
  int index;
  rtvalue *arrcell;
  carray *arr;
  int refindex;

  if (TYPE_NUMBER != pntrtype(indexpntr)) {
    printf("arrayitem: index must be a number, got ");
    print_pntr(indexpntr);
    printf("\n");
    abort();
  }
  if (TYPE_AREF != pntrtype(refpntr)) {
    printf("arrayitem: expected an array reference, got ");
    print_pntr(refpntr);
    printf("\n");
    abort();
  }

  index = (int)pntrdouble(indexpntr);

  arrcell = get_pntr(get_pntr(refpntr)->cmp1);
  arr = (carray*)get_pntr(arrcell->cmp1);
  refindex = (int)get_pntr(get_pntr(refpntr)->cmp2);
  assert(refindex+index < arr->size);
  argstack[0] = arr->cells[refindex+index];
}

void b_arrayhas(pntr *argstack)
{
  pntr refcell = argstack[0];
  pntr indexcell = argstack[1];
  int index;
  rtvalue *arrcell;
  carray *arr;
  int refindex;

  if (TYPE_NUMBER != pntrtype(indexcell)) {
    printf("arrayhas: index must be a number, got ");
    print_pntr(indexcell);
    printf("\n");
    abort();
  }

  if (TYPE_AREF != pntrtype(refcell)) {
    argstack[0] = globnilpntr;
    return;
  }

  index = (int)pntrdouble(indexcell);

  arrcell = get_pntr(get_pntr(refcell)->cmp1);
  arr = (carray*)get_pntr(arrcell->cmp1);
  refindex = (int)get_pntr(get_pntr(refcell)->cmp2);
  if (refindex+index < arr->size)
    argstack[0] = globtruepntr;
  else
    argstack[0] = globnilpntr;
}

void b_arrayext(pntr *argstack)
{
  pntr lstcell = argstack[0];
  pntr ncell = argstack[1];
  int n;
  rtvalue *arrcell = NULL;
  carray *arr = NULL;
  int base = 0; // FIXME: make sure this is respected
  int pos = 0;
  pntr cons;
  char *mode = NULL;
  int existing = 0;
  int oldalloc;
  int i;

  make_pntr(cons,NULL);

  if (TYPE_NUMBER != pntrtype(ncell)) {
    printf("arrayext: n must be a number, got ");
    print_pntr(ncell);
    printf("\n");
    abort();
  }
  n = (int)pntrdouble(ncell);
  assert(0 <= n);

  if (TYPE_AREF == pntrtype(lstcell)) {
    mode = "existing";
    arrcell = get_pntr(get_pntr(lstcell)->cmp1);
    arr = (carray*)get_pntr(arrcell->cmp1);
    base = (int)get_pntr(get_pntr(lstcell)->cmp2);

    assert(base < arr->size);
    assert(0 == base); // FIXME: lift this restriction

    pos = arr->size;
    cons = arr->tail;
    existing = 1;
  }
  else {
    mode = "new";
    assert(TYPE_CONS == pntrtype(lstcell));
    arr = (carray*)calloc(1,sizeof(carray));
    arr->alloc = 16; // FIXME
    arr->size = 0;
    arr->cells = (pntr*)calloc(arr->alloc,sizeof(pntr));
    arr->refs = (pntr*)calloc(arr->alloc,sizeof(pntr));
    for (i = 0; i < arr->alloc; i++) {
      make_pntr(arr->cells[i],NULL);
      make_pntr(arr->refs[i],NULL);
    }
    make_pntr(arr->tail,NULL);
    make_pntr(arr->sizecell,NULL);

    arrcell = alloc_rtvalue();
    arrcell->tag = TYPE_ARRAY;
    make_pntr(arrcell->cmp1,arr);
    cons = lstcell;
  }

  oldalloc = arr->alloc;
  while (base+n >= arr->alloc)
    arr->alloc *= 2;
  if (oldalloc != arr->alloc) {
    arr->cells = (pntr*)realloc(arr->cells,arr->alloc*sizeof(pntr));
    arr->refs = (pntr*)realloc(arr->refs,arr->alloc*sizeof(pntr));
    for (i = oldalloc; i < arr->alloc; i++) {
      make_pntr(arr->cells[i],NULL);
      make_pntr(arr->refs[i],NULL);
    }
  }

  assert(pos == arr->size);
  while (pos <= n) {
    cons = resolve_pntr(cons);
    assert(TYPE_CONS == pntrtype(cons));

    assert(pos < arr->alloc);

    arr->cells[pos] = get_pntr(cons)->cmp1;
    arr->tail = get_pntr(cons)->cmp2;
    cons = get_pntr(cons)->cmp2;

    pos++;
    arr->size++;
    assert(pos == arr->size);
  }

  if (TYPE_CONS == pntrtype(lstcell)) {
    rtvalue *lstvalue = get_pntr(lstcell);
    lstvalue->tag = TYPE_AREF;
    make_pntr(lstvalue->cmp1,arrcell);
    make_pntr(lstvalue->cmp2,0);
    make_pntr(arr->refs[0],lstvalue);
  }

  assert(n == (int)pntrdouble(ncell));
  assert(pos == n+1);
  assert(!is_nullpntr(arr->cells[n]));

  argstack[0] = arr->cells[n];
}

void b_arraysize(pntr *argstack)
{
  pntr refcell = argstack[0];
  if (TYPE_AREF == pntrtype(refcell)) {
    rtvalue *arrcell = get_pntr(get_pntr(refcell)->cmp1);
    carray *arr;
    assert(0 == (int)get_pntr(get_pntr(refcell)->cmp2));
    arr = (carray*)get_pntr(arrcell->cmp1);

    if (is_nullpntr(arr->sizecell) ||
        (arr->size != (int)pntrdouble(arr->sizecell)))
      arr->sizecell = make_number((double)arr->size);

    argstack[0] = arr->sizecell;
  }
  else if (TYPE_CONS == pntrtype(refcell)) {
    argstack[0] = globzeropntr; // FIXME: should this be nil, like arrayoptlen?
  }
  else {
    printf("arraysize: expected aref or cons, got ");
    print_pntr(refcell);
    printf("\n");
    abort();
  }
}

void b_arraytail(pntr *argstack)
{
  pntr refcell = argstack[0];
  if (TYPE_AREF == pntrtype(refcell)) {
    rtvalue *arrcell = get_pntr(get_pntr(refcell)->cmp1);
    carray *arr;
    assert(0 == (int)get_pntr(get_pntr(refcell)->cmp2));
    arr = (carray*)get_pntr(arrcell->cmp1);

    argstack[0] = arr->tail;
  }
  else if (TYPE_CONS == pntrtype(refcell)) {
    // leave item on top of stack as is; it's the list
  }
  else {
    printf("arraytail: expected aref or cons, got ");
    print_pntr(refcell);
    printf("\n");
    abort();
  }
}

void b_arrayoptlen(pntr *argstack)
{
  pntr refcell = argstack[0];
  if (TYPE_AREF == pntrtype(refcell)) {
    rtvalue *arrcell = get_pntr(get_pntr(refcell)->cmp1);
    carray *arr;
    assert(0 == (int)get_pntr(get_pntr(refcell)->cmp2));
    arr = (carray*)get_pntr(arrcell->cmp1);

    if (TYPE_NIL == pntrtype(arr->tail))
      b_arraysize(argstack);
    else
      argstack[0] = globnilpntr;
  }
  else if (TYPE_CONS == pntrtype(refcell)) {
    argstack[0] = globnilpntr;
  }
  else {
    printf("arrayoptlen: expected aref or cons, got ");
    print_pntr(refcell);
    printf("\n");
    abort();
  }
}

void b_echo(pntr *argstack)
{
  if (TYPE_STRING == pntrtype(argstack[0]))
    printf("%s",get_string(get_pntr(argstack[0])->cmp1));
  else
    print_pntr(argstack[0]);
  argstack[0] = globnilpntr;
}

void b_print(pntr *argstack)
{
  output_pntr(argstack[0]);
  argstack[0] = globnilpntr;
}

void b_sqrt(pntr *argstack)
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

void b_floor(pntr *argstack)
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

void b_ceil(pntr *argstack)
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

{ "convertarray",   2, 2, 1, b_convertarray   },
{ "arrayitem",      2, 2, 0, b_arrayitem      },
{ "arrayhas",       2, 2, 1, b_arrayhas       },
{ "arrayext",       2, 2, 0, b_arrayext       },
{ "arraysize",      1, 1, 1, b_arraysize      },
{ "arraytail",      1, 1, 0, b_arraytail      },
{ "arrayoptlen",    1, 1, 1, b_arrayoptlen    },

{ "echo",           1, 1, 1, b_echo           },
{ "print",          1, 1, 1, b_print          },

{ "sqrt",           1, 1, 1, b_sqrt           },
{ "floor",          1, 1, 1, b_floor          },
{ "ceil",           1, 1, 1, b_ceil           },

};

