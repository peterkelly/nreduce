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

void setint(stack *s, int val)
{
  s->data[s->count-1] = alloc_cell();
  s->data[s->count-1]->tag = TYPE_INT;
  s->data[s->count-1]->field1 = (void*)val;
}

void setdouble(stack *s, double val)
{
  s->data[s->count-1] = alloc_cell();
  s->data[s->count-1]->tag = TYPE_DOUBLE;
  celldouble(s->data[s->count-1]) = val;
}

void setbool(stack *s, int b)
{
  s->data[s->count-1] = (b ? globtrue : globnil);
}

void intop(stack *s, int bif, int a, int b)
{
  switch (bif) {
  case B_ADD:       setint(s,a +  b);  break;
  case B_SUBTRACT:  setint(s,a -  b);  break;
  case B_MULTIPLY:  setint(s,a *  b);  break;
  case B_DIVIDE:    if (0 == b) {
                      fprintf(stderr,"Divide by zero\n");
                      exit(1);
                    }
                    setint(s,a /  b);  break;
  case B_MOD:       setint(s,a %  b);  break;
  case B_EQ:       setbool(s,a == b);  break;
  case B_NE:       setbool(s,a != b);  break;
  case B_LT:       setbool(s,a <  b);  break;
  case B_LE:       setbool(s,a <= b);  break;
  case B_GT:       setbool(s,a >  b);  break;
  case B_GE:       setbool(s,a >= b);  break;
  default:         assert(0);        break;
  }
}

void doubleop(stack *s, int bif, double a, double b)
{
  switch (bif) {
  case B_ADD:       setdouble(s,a +  b);  break;
  case B_SUBTRACT:  setdouble(s,a -  b);  break;
  case B_MULTIPLY:  setdouble(s,a *  b);  break;
  case B_DIVIDE:    if (0.0 == b) {
                      fprintf(stderr,"Divide by zero\n");
                      exit(1);
                    }
                    setdouble(s,a /  b);  break;
  case B_MOD:       setdouble(s,fmod(a,b));  break;
  case B_EQ:       setbool(s,a == b);  break;
  case B_NE:       setbool(s,a != b);  break;
  case B_LT:       setbool(s,a <  b);  break;
  case B_LE:       setbool(s,a <= b);  break;
  case B_GT:       setbool(s,a >  b);  break;
  case B_GE:       setbool(s,a >= b);  break;
  default:         assert(0);        break;
  }
}

void b_other(stack *s, int bif)
{
  cell *cell2 = s->data[s->count-2];
  cell *cell1 = s->data[s->count-1];

  assert(TYPE_IND != celltype(cell2));
  assert(TYPE_IND != celltype(cell1));


  s->count--;

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
      case B_EQ: setbool(s,0 == cmp); break;
      case B_NE: setbool(s,0 != cmp); break;
      case B_LT: setbool(s,0 > cmp); break;
      case B_LE: setbool(s,0 >= cmp); break;
      case B_GT: setbool(s,0 < cmp); break;
      case B_GE: setbool(s,0 <= cmp); break;
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
      intop(s,bif,(int)cell1->field1,(int)cell2->field1);
    }
    else if ((TYPE_DOUBLE == celltype(cell1)) && (TYPE_DOUBLE == celltype(cell2))) {
      doubleop(s,bif,celldouble(cell1),celldouble(cell2));
    }
    else if ((TYPE_INT == celltype(cell1)) && (TYPE_DOUBLE == celltype(cell2))) {
      doubleop(s,bif,(double)((int)cell1->field1),celldouble(cell2));
    }
    else if ((TYPE_DOUBLE == celltype(cell1)) && (TYPE_INT == celltype(cell2))) {
      doubleop(s,bif,celldouble(cell1),(double)((int)cell2->field1));
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
    setbool(s,(TYPE_NIL != celltype(cell1)) && (TYPE_NIL != celltype(cell2)));
    break;
  case B_OR:
    setbool(s,(TYPE_NIL != celltype(cell1)) || (TYPE_NIL != celltype(cell2)));
    break;
  default:
    assert(0);
  }
}

void b_add(stack *s) { b_other(s,B_ADD); }
void b_subtract(stack *s) { b_other(s,B_SUBTRACT); }
void b_multiply(stack *s) { b_other(s,B_MULTIPLY); }
void b_divide(stack *s) { b_other(s,B_DIVIDE); }
void b_mod(stack *s) { b_other(s,B_MOD); }
void b_eq(stack *s) { b_other(s,B_EQ); }
void b_ne(stack *s) { b_other(s,B_NE); }
void b_lt(stack *s) { b_other(s,B_LT); }
void b_le(stack *s) { b_other(s,B_LE); }
void b_gt(stack *s) { b_other(s,B_GT); }
void b_ge(stack *s) { b_other(s,B_GE); }
void b_and(stack *s) { b_other(s,B_AND); }
void b_or(stack *s) { b_other(s,B_OR); }

void b_not(stack *s)
{
  cell *arg = s->data[s->count-1];
  assert(TYPE_IND != celltype(arg));
  if (TYPE_NIL == celltype(s->data[s->count-1]))
    s->data[s->count-1] = globtrue;
  else
    s->data[s->count-1] = globnil;
}

void b_bitop(stack *s, int bif)
{
  cell *cell2 = s->data[s->count-2];
  cell *cell1 = s->data[s->count-1];

  assert(TYPE_IND != celltype(cell2));
  assert(TYPE_IND != celltype(cell1));

  if ((TYPE_INT != celltype(cell1)) || (TYPE_INT != celltype(cell2))) {
    fprintf(stderr,"%s: incompatible arguments (must be int)\n",builtin_info[bif].name);
    exit(1);
  }

  int a = (int)cell1->field1;
  int b = (int)cell2->field1;
  s->count--;

  switch (bif) {
  case B_BITSHL:  setint(s,a << b);  break;
  case B_BITSHR:  setint(s,a >> b);  break;
  case B_BITAND:  setint(s,a & b);   break;
  case B_BITOR:   setint(s,a | b);   break;
  case B_BITXOR:  setint(s,a ^ b);   break;
  default:        assert(0);       break;
  }
}

void b_bitshl(stack *s) { b_bitop(s,B_BITSHL); }
void b_bitshr(stack *s) { b_bitop(s,B_BITSHR); }
void b_bitand(stack *s) { b_bitop(s,B_BITAND); }
void b_bitor(stack *s) { b_bitop(s,B_BITOR); }
void b_bitxor(stack *s) { b_bitop(s,B_BITXOR); }

void b_bitnot(stack *s)
{
  cell *arg = s->data[s->count-1];
  assert(TYPE_IND != celltype(arg));
  if (TYPE_INT != celltype(arg)) {
    fprintf(stderr,"~: incompatible argument (must be an int)\n");
    exit(1);
  }
  int val = (int)arg->field1;
  setint(s,~val);
}

void b_if(stack *s)
{
  cell *ifc = s->data[s->count-1];
  cell *source;

  assert(TYPE_IND != celltype(ifc));

  if (TYPE_NIL == celltype(ifc))
    source = s->data[s->count-3];
  else
    source = s->data[s->count-2];

  s->count -= 2;
  s->data[s->count-1] = source;
}

void b_cons(stack *s)
{
  cell *head = s->data[s->count-1];
  cell *tail = s->data[s->count-2];
/*   assert(TYPE_IND != celltype(head)); */
/*   assert(TYPE_IND != celltype(tail)); */

  s->count -= 1;
  s->data[s->count-1] = alloc_cell();
  s->data[s->count-1]->tag = TYPE_CONS;
  s->data[s->count-1]->field1 = head;
  s->data[s->count-1]->field2 = tail;
}

void b_head(stack *s)
{
  cell *arg = s->data[s->count-1];
  assert(TYPE_IND != celltype(arg));
  if (TYPE_CONS == celltype(arg)) {
    s->data[s->count-1] = (cell*)arg->field1;
  }
  else if (TYPE_AREF == celltype(arg)) {
    cell *arrcell = (cell*)arg->field1;
    carray *arr = (carray*)arrcell->field1;
    int index = (int)arg->field2;
    assert(index < arr->size);
    s->data[s->count-1] = (cell*)arr->cells[index];
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

void b_tail(stack *s)
{
  cell *arg = s->data[s->count-1];
  assert(TYPE_IND != celltype(arg));
  if (TYPE_CONS == celltype(arg)) {
    s->data[s->count-1] = (cell*)arg->field2;
  }
  else if (TYPE_AREF == celltype(arg)) {
    cell *arrcell = (cell*)arg->field1;
    int index = (int)arg->field2;
    s->data[s->count-1] = get_tail(arrcell,index);
  }
  else {
    assert(!"tail: expected a cons cell");
  }
}

void b_islambda(stack *s)
{
  cell *val = s->data[s->count-1];
  assert(TYPE_IND != celltype(val));
  setbool(s,TYPE_LAMBDA == celltype(val));
}

void b_isvalue(stack *s)
{
  cell *val = s->data[s->count-1];
  assert(TYPE_IND != celltype(val));
  setbool(s,isvalue(val));
}

void b_iscons(stack *s)
{
  cell *val = s->data[s->count-1];
  assert(TYPE_IND != celltype(val));
  setbool(s,(TYPE_CONS == celltype(val)) || (TYPE_AREF == celltype(val)));
}

void b_isnil(stack *s)
{
  cell *val = s->data[s->count-1];
  assert(TYPE_IND != celltype(val));
  setbool(s,TYPE_NIL == celltype(val));
}

void b_isint(stack *s)
{
  cell *val = s->data[s->count-1];
  assert(TYPE_IND != celltype(val));
  setbool(s,TYPE_INT == celltype(val));
}

void b_isdouble(stack *s)
{
  cell *val = s->data[s->count-1];
  assert(TYPE_IND != celltype(val));
  setbool(s,TYPE_DOUBLE == celltype(val));
}

void b_isstring(stack *s)
{
  cell *val = s->data[s->count-1];
  assert(TYPE_IND != celltype(val));
  setbool(s,TYPE_STRING == celltype(val));
}

void b_convertarray(stack *s)
{
  cell *retcell = (cell*)s->data[s->count-1];
  s->count--;

  if (TYPE_INT != celltype(retcell)) {
    printf("convertarray: first arg should be an int, got ");
    print_code(retcell);
    printf("\n");
    abort();
  }

  // FIXME: this doesn't correctly handle cyclic lists at present!
  cell *top = s->data[s->count-1];
  assert(TYPE_IND != celltype(top));
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

  s->data[s->count-1] = retcell;
}

void b_arrayitem(stack *s)
{
  cell *refcell = s->data[s->count-2];
  cell *indexcell = s->data[s->count-1];
  assert(TYPE_IND != celltype(refcell));
  assert(TYPE_IND != celltype(indexcell));

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
  s->count--;

  cell *arrcell = (cell*)refcell->field1;
  carray *arr = (carray*)arrcell->field1;
  int refindex = (int)refcell->field2;
  assert(refindex+index < arr->size);
  s->data[s->count-1] = (cell*)arr->cells[refindex+index];
}

void b_arrayhas(stack *s)
{
  cell *refcell = s->data[s->count-2];
  cell *indexcell = s->data[s->count-1];
  assert(TYPE_IND != celltype(refcell));
  assert(TYPE_IND != celltype(indexcell));
  s->count--;

  if (TYPE_INT != celltype(indexcell)) {
    printf("arrayhas: index must be an integer, got ");
    print_code(indexcell);
    printf("\n");
    abort();
  }

  if (TYPE_AREF != celltype(refcell)) {
    s->data[s->count-1] = globnil;
    return;
  }

  int index = (int)indexcell->field1;

  cell *arrcell = (cell*)refcell->field1;
  carray *arr = (carray*)arrcell->field1;
  int refindex = (int)refcell->field2;
  if (refindex+index < arr->size)
    s->data[s->count-1] = globtrue;
  else
    s->data[s->count-1] = globnil;
}

void b_arrayext(stack *s)
{
  cell *lstcell = s->data[s->count-2];
  cell *ncell = s->data[s->count-1];
  assert(TYPE_IND != celltype(lstcell));
  assert(TYPE_IND != celltype(ncell));
  s->count--;

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

  s->data[s->count-1] = arr->cells[n];
}

void b_arraysize(stack *s)
{
  cell *refcell = s->data[s->count-1];
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

    s->data[s->count-1] = arr->sizecell;
  }
  else if (TYPE_CONS == celltype(refcell)) {
    s->data[s->count-1] = globzero; // FIXME: should this be nil, like arrayoptlen?
  }
  else {
    printf("arraysize: expected aref or cons, got ");
    print_code(refcell);
    printf("\n");
    abort();
  }
}

void b_arraytail(stack *s)
{
  cell *refcell = s->data[s->count-1];
  if (TYPE_AREF == celltype(refcell)) {
    cell *arrcell = (cell*)refcell->field1;
    assert(0 == (int)refcell->field2);
    carray *arr = (carray*)arrcell->field1;

    s->data[s->count-1] = arr->tail;
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

void b_arrayoptlen(stack *s)
{
  cell *refcell = s->data[s->count-1];
  if (TYPE_AREF == celltype(refcell)) {
    cell *arrcell = (cell*)refcell->field1;
    assert(0 == (int)refcell->field2);
    carray *arr = (carray*)arrcell->field1;

    if (TYPE_NIL == celltype(arr->tail))
      b_arraysize(s);
    else
      s->data[s->count-1] = globnil;
  }
  else if (TYPE_CONS == celltype(refcell)) {
    s->data[s->count-1] = globnil;
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

