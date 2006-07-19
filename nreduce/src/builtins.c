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

void setint(cell **cptr, int val)
{
  *cptr = alloc_cell();
  (*cptr)->tag = TYPE_INT;
  (*cptr)->field1 = (void*)val;
}

void setdouble(cell **cptr, double val)
{
  *cptr = alloc_cell();
  (*cptr)->tag = TYPE_DOUBLE;
  celldouble(*cptr) = val;
}

void setbool(cell **cptr, int b)
{
  *cptr = (b ? globtrue : globnil);
}

void intop(cell **cptr, int bif, int a, int b)
{
  switch (bif) {
  case B_ADD:       setint(cptr,a +  b);  break;
  case B_SUBTRACT:  setint(cptr,a -  b);  break;
  case B_MULTIPLY:  setint(cptr,a *  b);  break;
  case B_DIVIDE:    if (0 == b) {
                      fprintf(stderr,"Divide by zero\n");
                      exit(1);
                    }
                    setint(cptr,a /  b);  break;
  case B_MOD:       setint(cptr,a %  b);  break;
  case B_EQ:       setbool(cptr,a == b);  break;
  case B_NE:       setbool(cptr,a != b);  break;
  case B_LT:       setbool(cptr,a <  b);  break;
  case B_LE:       setbool(cptr,a <= b);  break;
  case B_GT:       setbool(cptr,a >  b);  break;
  case B_GE:       setbool(cptr,a >= b);  break;
  default:         assert(0);        break;
  }
}

void doubleop(cell **cptr, int bif, double a, double b)
{
  switch (bif) {
  case B_ADD:       setdouble(cptr,a +  b);  break;
  case B_SUBTRACT:  setdouble(cptr,a -  b);  break;
  case B_MULTIPLY:  setdouble(cptr,a *  b);  break;
  case B_DIVIDE:    if (0.0 == b) {
                      fprintf(stderr,"Divide by zero\n");
                      exit(1);
                    }
                    setdouble(cptr,a /  b);  break;
  case B_MOD:       setdouble(cptr,fmod(a,b));  break;
  case B_EQ:       setbool(cptr,a == b);  break;
  case B_NE:       setbool(cptr,a != b);  break;
  case B_LT:       setbool(cptr,a <  b);  break;
  case B_LE:       setbool(cptr,a <= b);  break;
  case B_GT:       setbool(cptr,a >  b);  break;
  case B_GE:       setbool(cptr,a >= b);  break;
  default:         assert(0);        break;
  }
}

void b_other(cell **argstack, int bif)
{
  cell *cell2 = argstack[0];
  cell *cell1 = argstack[1];

  switch (bif) {
  case B_EQ:
  case B_NE:
  case B_LT:
  case B_LE:
  case B_GT:
  case B_GE:
    if ((TYPE_STRING == celltype(cell1)) && (TYPE_STRING == celltype(cell2))) {
      int cmp = strcmp((char*)cell1->field1,(char*)cell2->field1);
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
    if ((TYPE_INT == celltype(cell1)) && (TYPE_INT == celltype(cell2))) {
      intop(&argstack[0],bif,(int)cell1->field1,(int)cell2->field1);
    }
    else if ((TYPE_DOUBLE == celltype(cell1)) && (TYPE_DOUBLE == celltype(cell2))) {
      doubleop(&argstack[0],bif,celldouble(cell1),celldouble(cell2));
    }
    else if ((TYPE_INT == celltype(cell1)) && (TYPE_DOUBLE == celltype(cell2))) {
      doubleop(&argstack[0],bif,(double)((int)cell1->field1),celldouble(cell2));
    }
    else if ((TYPE_DOUBLE == celltype(cell1)) && (TYPE_INT == celltype(cell2))) {
      doubleop(&argstack[0],bif,celldouble(cell1),(double)((int)cell2->field1));
    }
    else {
      fprintf(stderr,"%s: incompatible arguments: (%s,%s)\n",
                      builtin_info[bif].name,
                      cell_types[celltype(cell1)],
                      cell_types[celltype(cell2)]);
      exit(1);
    }
    break;
  case B_AND:
    setbool(&argstack[0],(TYPE_NIL != celltype(cell1)) && (TYPE_NIL != celltype(cell2)));
    break;
  case B_OR:
    setbool(&argstack[0],(TYPE_NIL != celltype(cell1)) || (TYPE_NIL != celltype(cell2)));
    break;
  default:
    assert(0);
  }
}

void b_add(cell **argstack) { b_other(argstack,B_ADD); }
void b_subtract(cell **argstack) { b_other(argstack,B_SUBTRACT); }
void b_multiply(cell **argstack) { b_other(argstack,B_MULTIPLY); }
void b_divide(cell **argstack) { b_other(argstack,B_DIVIDE); }
void b_mod(cell **argstack) { b_other(argstack,B_MOD); }
void b_eq(cell **argstack) { b_other(argstack,B_EQ); }
void b_ne(cell **argstack) { b_other(argstack,B_NE); }
void b_lt(cell **argstack) { b_other(argstack,B_LT); }
void b_le(cell **argstack) { b_other(argstack,B_LE); }
void b_gt(cell **argstack) { b_other(argstack,B_GT); }
void b_ge(cell **argstack) { b_other(argstack,B_GE); }
void b_and(cell **argstack) { b_other(argstack,B_AND); }
void b_or(cell **argstack) { b_other(argstack,B_OR); }

void b_not(cell **argstack)
{
  if (TYPE_NIL == celltype(argstack[0]))
    argstack[0] = globtrue;
  else
    argstack[0] = globnil;
}

void b_bitop(cell **argstack, int bif)
{
  cell *cell2 = argstack[0];
  cell *cell1 = argstack[1];

  if ((TYPE_INT != celltype(cell1)) || (TYPE_INT != celltype(cell2))) {
    fprintf(stderr,"%s: incompatible arguments (must be int)\n",builtin_info[bif].name);
    exit(1);
  }

  int a = (int)cell1->field1;
  int b = (int)cell2->field1;

  switch (bif) {
  case B_BITSHL:  setint(&argstack[0],a << b);  break;
  case B_BITSHR:  setint(&argstack[0],a >> b);  break;
  case B_BITAND:  setint(&argstack[0],a & b);   break;
  case B_BITOR:   setint(&argstack[0],a | b);   break;
  case B_BITXOR:  setint(&argstack[0],a ^ b);   break;
  default:        assert(0);       break;
  }
}

void b_bitshl(cell **argstack) { b_bitop(argstack,B_BITSHL); }
void b_bitshr(cell **argstack) { b_bitop(argstack,B_BITSHR); }
void b_bitand(cell **argstack) { b_bitop(argstack,B_BITAND); }
void b_bitor(cell **argstack) { b_bitop(argstack,B_BITOR); }
void b_bitxor(cell **argstack) { b_bitop(argstack,B_BITXOR); }

void b_bitnot(cell **argstack)
{
  cell *arg = argstack[0];
  if (TYPE_INT != celltype(arg)) {
    fprintf(stderr,"~: incompatible argument (must be an int)\n");
    exit(1);
  }
  int val = (int)arg->field1;
  setint(&argstack[0],~val);
}

void b_if(cell **argstack)
{
  cell *ifc = argstack[2];
  cell *source;

  if (TYPE_NIL == celltype(ifc))
    source = argstack[0];
  else
    source = argstack[1];

  argstack[0] = source;
}

void b_cons(cell **argstack)
{
  cell *head = argstack[1];
  cell *tail = argstack[0];

  argstack[0] = alloc_cell();
  argstack[0]->tag = TYPE_CONS;
  argstack[0]->field1 = head;
  argstack[0]->field2 = tail;
}

void b_head(cell **argstack)
{
  cell *arg = argstack[0];
  if (TYPE_CONS == celltype(arg)) {
    argstack[0] = (cell*)arg->field1;
  }
  else if (TYPE_AREF == celltype(arg)) {
    cell *arrcell = (cell*)arg->field1;
    carray *arr = (carray*)arrcell->field1;
    int index = (int)arg->field2;
    assert(index < arr->size);
    argstack[0] = (cell*)arr->cells[index];
  }
  else {
    assert(!"head: expected a cons cell");
  }
}

cell *get_tail(cell *arrcell, int index)
{
  carray *arr = (carray*)arrcell->field1;
  assert(index < arr->size);
  if (index+1 == arr->size) {
    return arr->tail;
  }
  else {
    cell *ref = arr->refs[index+1];
    if (NULL == ref) {
      ref = alloc_cell();
      ref->tag = TYPE_AREF;
      ref->field1 = arrcell;
      ref->field2 = (void*)(index+1);
      arr->refs[index+1] = ref;
      assert(index+1 < arr->size);
    }
    return ref;
  }
}

void b_tail(cell **argstack)
{
  cell *arg = argstack[0];
  if (TYPE_CONS == celltype(arg)) {
    argstack[0] = (cell*)arg->field2;
  }
  else if (TYPE_AREF == celltype(arg)) {
    cell *arrcell = (cell*)arg->field1;
    int index = (int)arg->field2;
    argstack[0] = get_tail(arrcell,index);
  }
  else {
    assert(!"tail: expected a cons cell");
  }
}

void b_islambda(cell **argstack)
{
  cell *val = argstack[0];
  setbool(&argstack[0],TYPE_LAMBDA == celltype(val));
}

void b_isvalue(cell **argstack)
{
  cell *val = argstack[0];
  setbool(&argstack[0],isvalue(val));
}

void b_iscons(cell **argstack)
{
  cell *val = argstack[0];
  setbool(&argstack[0],(TYPE_CONS == celltype(val)) || (TYPE_AREF == celltype(val)));
}

void b_isnil(cell **argstack)
{
  cell *val = argstack[0];
  setbool(&argstack[0],TYPE_NIL == celltype(val));
}

void b_isint(cell **argstack)
{
  cell *val = argstack[0];
  setbool(&argstack[0],TYPE_INT == celltype(val));
}

void b_isdouble(cell **argstack)
{
  cell *val = argstack[0];
  setbool(&argstack[0],TYPE_DOUBLE == celltype(val));
}

void b_isstring(cell **argstack)
{
  cell *val = argstack[0];
  setbool(&argstack[0],TYPE_STRING == celltype(val));
}

void b_convertarray(cell **argstack)
{
  cell *retcell = (cell*)argstack[1];

  if (TYPE_INT != celltype(retcell)) {
    printf("convertarray: first arg should be an int, got ");
    print_code(retcell);
    printf("\n");
    abort();
  }

  // FIXME: this doesn't correctly handle cyclic lists at present!
  cell *top = argstack[0];
  int size = 0;

  if (TYPE_AREF == celltype(top))
    return;

  cell *c = top;
  while (TYPE_CONS == celltype(c)) {
    c = resolve_ind((cell*)c->field2);
    size++;
  }

  if (TYPE_APPLICATION == celltype(c)) {
    printf("convertarray: list is not fully evaluated\n");
    abort();
  }
  else if (TYPE_NIL != celltype(c)) {
    printf("convertarray: found non-cons within list: ");
    print_code(c);
    printf("\n");
    abort();
  }

  carray *arr = (carray*)calloc(1,sizeof(carray));
  arr->alloc = size;
  arr->size = size;
  arr->cells = (cell**)calloc(arr->alloc,sizeof(cell*));
  arr->refs = (cell**)calloc(arr->alloc,sizeof(cell*));
  arr->tail = globnil;

  int i;
  c = top;
  for (i = 0; i < arr->size; i++) {
    arr->cells[i] = c->field1;
    c = resolve_ind((cell*)c->field2);
  }

  cell *arrcell = alloc_cell();
  arrcell->tag = TYPE_ARRAY;
  arrcell->field1 = arr;

  // return value is what is passed in, but now it points to an array
  top->tag = TYPE_AREF;
  top->field1 = arrcell;
  top->field2 = 0;
  arr->refs[0] = top;

  argstack[0] = retcell;
}

void b_arrayitem(cell **argstack)
{
  cell *refcell = argstack[0];
  cell *indexcell = argstack[1];

  if (TYPE_INT != celltype(indexcell)) {
    printf("arrayitem: index must be an integer, got ");
    print_code(indexcell);
    printf("\n");
    abort();
  }
  if (TYPE_AREF != celltype(refcell)) {
    printf("arrayitem: expected an array reference, got ");
    print_code(refcell);
    printf("\n");
    abort();
  }

  int index = (int)indexcell->field1;

  cell *arrcell = (cell*)refcell->field1;
  carray *arr = (carray*)arrcell->field1;
  int refindex = (int)refcell->field2;
  assert(refindex+index < arr->size);
  argstack[0] = (cell*)arr->cells[refindex+index];
}

void b_arrayhas(cell **argstack)
{
  cell *refcell = argstack[0];
  cell *indexcell = argstack[1];

  if (TYPE_INT != celltype(indexcell)) {
    printf("arrayhas: index must be an integer, got ");
    print_code(indexcell);
    printf("\n");
    abort();
  }

  if (TYPE_AREF != celltype(refcell)) {
    argstack[0] = globnil;
    return;
  }

  int index = (int)indexcell->field1;

  cell *arrcell = (cell*)refcell->field1;
  carray *arr = (carray*)arrcell->field1;
  int refindex = (int)refcell->field2;
  if (refindex+index < arr->size)
    argstack[0] = globtrue;
  else
    argstack[0] = globnil;
}

void b_arrayext(cell **argstack)
{
  cell *lstcell = argstack[0];
  cell *ncell = argstack[1];

  if (TYPE_INT != celltype(ncell)) {
    printf("arrayext: n must be an integer, got ");
    print_code(ncell);
    printf("\n");
    abort();
  }
  int n = (int)ncell->field1;
  assert(0 <= n);

  cell *arrcell = NULL;
  carray *arr = NULL;
  int base = 0; // FIXME: make sure this is respected
  int pos = 0;
  cell *cons = NULL;
  char *mode = NULL;
  int existing = 0;
  if (TYPE_AREF == celltype(lstcell)) {
    mode = "existing";
    arrcell = (cell*)lstcell->field1;
    arr = (carray*)arrcell->field1;
    base = (int)lstcell->field2;

    assert(base < arr->size);
    assert(0 == base); // FIXME: lift this restriction

    pos = arr->size;
    cons = arr->tail;
    existing = 1;
  }
  else {
    mode = "new";
    assert(TYPE_CONS == celltype(lstcell));
    arr = (carray*)calloc(1,sizeof(carray));
    arr->alloc = 16; // FIXME
    arr->size = 0;
    arr->cells = (cell**)calloc(arr->alloc,sizeof(cell*));
    arr->refs = (cell**)calloc(arr->alloc,sizeof(cell*));
    arr->tail = NULL;

    arrcell = alloc_cell();
    arrcell->tag = TYPE_ARRAY;
    arrcell->field1 = arr;
    cons = lstcell;
  }

  int oldalloc = arr->alloc;
  while (base+n >= arr->alloc)
    arr->alloc *= 2;
  if (oldalloc != arr->alloc) {
    arr->cells = (cell**)realloc(arr->cells,arr->alloc*sizeof(cell*));
    arr->refs = (cell**)realloc(arr->refs,arr->alloc*sizeof(cell*));
    memset(&arr->cells[oldalloc],0,(arr->alloc-oldalloc)*sizeof(cell*));
    memset(&arr->refs[oldalloc],0,(arr->alloc-oldalloc)*sizeof(cell*));
  }

  assert(pos == arr->size);
  while (pos <= n) {
    cons = resolve_ind(cons);
    assert(TYPE_CONS == celltype(cons));

    assert(pos < arr->alloc);

    arr->cells[pos] = (cell*)cons->field1;
    arr->tail = (cell*)cons->field2;
    cons = (cell*)cons->field2;

    pos++;
    arr->size++;
    assert(pos == arr->size);
  }

  if (TYPE_CONS == celltype(lstcell)) {
    lstcell->tag = TYPE_AREF;
    lstcell->field1 = arrcell;
    lstcell->field2 = 0;
    arr->refs[0] = lstcell;
  }

  assert(n == (int)ncell->field1);
  assert(pos == n+1);
  assert(arr->cells[n]);

  argstack[0] = arr->cells[n];
}

void b_arraysize(cell **argstack)
{
  cell *refcell = argstack[0];
  if (TYPE_AREF == celltype(refcell)) {
    cell *arrcell = (cell*)refcell->field1;
    assert(0 == (int)refcell->field2);
    carray *arr = (carray*)arrcell->field1;

    if ((NULL == arr->sizecell) ||
        (arr->size != (int)arr->sizecell->field1)) {
      arr->sizecell = alloc_cell();
      arr->sizecell->tag = TYPE_INT;
      arr->sizecell->field1 = (void*)arr->size;
    }

    argstack[0] = arr->sizecell;
  }
  else if (TYPE_CONS == celltype(refcell)) {
    argstack[0] = globzero; // FIXME: should this be nil, like arrayoptlen?
  }
  else {
    printf("arraysize: expected aref or cons, got ");
    print_code(refcell);
    printf("\n");
    abort();
  }
}

void b_arraytail(cell **argstack)
{
  cell *refcell = argstack[0];
  if (TYPE_AREF == celltype(refcell)) {
    cell *arrcell = (cell*)refcell->field1;
    assert(0 == (int)refcell->field2);
    carray *arr = (carray*)arrcell->field1;

    argstack[0] = arr->tail;
  }
  else if (TYPE_CONS == celltype(refcell)) {
    // leave item on top of stack as is; it's the list
  }
  else {
    printf("arraytail: expected aref or cons, got ");
    print_code(refcell);
    printf("\n");
    abort();
  }
}

void b_arrayoptlen(cell **argstack)
{
  cell *refcell = argstack[0];
  if (TYPE_AREF == celltype(refcell)) {
    cell *arrcell = (cell*)refcell->field1;
    assert(0 == (int)refcell->field2);
    carray *arr = (carray*)arrcell->field1;

    if (TYPE_NIL == celltype(arr->tail))
      b_arraysize(argstack);
    else
      argstack[0] = globnil;
  }
  else if (TYPE_CONS == celltype(refcell)) {
    argstack[0] = globnil;
  }
  else {
    printf("arrayoptlen: expected aref or cons, got ");
    print_code(refcell);
    printf("\n");
    abort();
  }
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
{ "int?",           1, 1, 1, b_isint          },
{ "double?",        1, 1, 1, b_isdouble       },
{ "string?",        1, 1, 1, b_isstring       },

{ "convertarray",   2, 2, 1, b_convertarray   },
{ "arrayitem",      2, 2, 0, b_arrayitem      },
{ "arrayhas",       2, 2, 1, b_arrayhas       },
{ "arrayext",       2, 2, 1, b_arrayext       },
{ "arraysize",      1, 1, 1, b_arraysize      },
{ "arraytail",      1, 1, 1, b_arraytail      },
{ "arrayoptlen",    1, 1, 1, b_arrayoptlen    },

};

