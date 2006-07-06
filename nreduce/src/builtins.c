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

void setint(int val)
{
  stack[stackcount-1] = alloc_cell();
  stack[stackcount-1]->tag = TYPE_INT;
  stack[stackcount-1]->field1 = (void*)val;
}

void setdouble(double val)
{
  stack[stackcount-1] = alloc_cell();
  stack[stackcount-1]->tag = TYPE_DOUBLE;
  celldouble(stack[stackcount-1]) = val;
}

void setbool(int b)
{
  stack[stackcount-1] = alloc_cell();
  if (b) {
    stack[stackcount-1]->tag = TYPE_INT;
    stack[stackcount-1]->field1 = (void*)1;
  }
  else {
    stack[stackcount-1]->tag = TYPE_NIL;
  }
}


void intop(int bif, int a, int b)
{
  switch (bif) {
  case B_ADD:       setint(a +  b);  break;
  case B_SUBTRACT:  setint(a -  b);  break;
  case B_MULTIPLY:  setint(a *  b);  break;
  case B_DIVIDE:    if (0 == b) {
                      fprintf(stderr,"Divide by zero\n");
                      exit(1);
                    }
                    setint(a /  b);  break;
  case B_MOD:       setint(a %  b);  break;
  case B_EQ:       setbool(a == b);  break;
  case B_NE:       setbool(a != b);  break;
  case B_LT:       setbool(a <  b);  break;
  case B_LE:       setbool(a <= b);  break;
  case B_GT:       setbool(a >  b);  break;
  case B_GE:       setbool(a >= b);  break;
  default:         assert(0);        break;
  }
}

void doubleop(int bif, double a, double b)
{
  switch (bif) {
  case B_ADD:       setdouble(a +  b);  break;
  case B_SUBTRACT:  setdouble(a -  b);  break;
  case B_MULTIPLY:  setdouble(a *  b);  break;
  case B_DIVIDE:    if (0.0 == b) {
                      fprintf(stderr,"Divide by zero\n");
                      exit(1);
                    }
                    setdouble(a /  b);  break;
  case B_MOD:       setdouble(fmod(a,b));  break;
  case B_EQ:       setbool(a == b);  break;
  case B_NE:       setbool(a != b);  break;
  case B_LT:       setbool(a <  b);  break;
  case B_LE:       setbool(a <= b);  break;
  case B_GT:       setbool(a >  b);  break;
  case B_GE:       setbool(a >= b);  break;
  default:         assert(0);        break;
  }
}

void b_other(int bif)
{
  cell *cell2 = stack[stackcount-2];
  cell *cell1 = stack[stackcount-1];

  assert(TYPE_IND != celltype(cell2));
  assert(TYPE_IND != celltype(cell1));


  stackcount--;

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
      case B_EQ: setbool(0 == cmp); break;
      case B_NE: setbool(0 != cmp); break;
      case B_LT: setbool(0 > cmp); break;
      case B_LE: setbool(0 >= cmp); break;
      case B_GT: setbool(0 < cmp); break;
      case B_GE: setbool(0 <= cmp); break;
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
      intop(bif,(int)cell1->field1,(int)cell2->field1);
    }
    else if ((TYPE_DOUBLE == celltype(cell1)) && (TYPE_DOUBLE == celltype(cell2))) {
      doubleop(bif,celldouble(cell1),celldouble(cell2));
    }
    else if ((TYPE_INT == celltype(cell1)) && (TYPE_DOUBLE == celltype(cell2))) {
      doubleop(bif,(double)((int)cell1->field1),celldouble(cell2));
    }
    else if ((TYPE_DOUBLE == celltype(cell1)) && (TYPE_INT == celltype(cell2))) {
      doubleop(bif,celldouble(cell1),(double)((int)cell2->field1));
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
    setbool((TYPE_NIL != celltype(cell1)) && (TYPE_NIL != celltype(cell2)));
    break;
  case B_OR:
    setbool((TYPE_NIL != celltype(cell1)) || (TYPE_NIL != celltype(cell2)));
    break;

  case B_AP:
    if ((TYPE_STRING == celltype(cell1)) && (TYPE_STRING == celltype(cell2))) {
      char *a1 = (char*)cell1->field1;
      char *a2 = (char*)cell2->field1;
      stack[stackcount-1] = alloc_cell();
      stack[stackcount-1]->tag = TYPE_STRING;
      stack[stackcount-1]->field1 = malloc(strlen(a1)+strlen(a2)+1);
      sprintf((char*)stack[stackcount-1]->field1,"%s%s",a1,a2);
    }
    else {
      fprintf(stderr,"%s: incompatible arguments: (%s,%s)\n",
                      builtin_info[bif].name,
                      cell_types[celltype(cell1)],
                      cell_types[celltype(cell2)]);
      exit(1);
    }
    break;
  }
}

void b_add() { b_other(B_ADD); }
void b_subtract() { b_other(B_SUBTRACT); }
void b_multiply() { b_other(B_MULTIPLY); }
void b_divide() { b_other(B_DIVIDE); }
void b_mod() { b_other(B_MOD); }
void b_eq() { b_other(B_EQ); }
void b_ne() { b_other(B_NE); }
void b_lt() { b_other(B_LT); }
void b_le() { b_other(B_LE); }
void b_gt() { b_other(B_GT); }
void b_ge() { b_other(B_GE); }
void b_and() { b_other(B_AND); }
void b_or() { b_other(B_OR); }

void b_not()
{
  cell *arg = stack[stackcount-1];
  assert(TYPE_IND != celltype(arg));
  if (TYPE_NIL == celltype(stack[stackcount-1]))
    stack[stackcount-1] = globtrue;
  else
    stack[stackcount-1] = globnil;
}

void b_ap() { b_other(B_AP); }

void b_if()
{
  cell *ifc = stack[stackcount-1];
  cell *source;

  assert(TYPE_IND != celltype(ifc));

  if (TYPE_NIL == celltype(ifc))
    source = stack[stackcount-3];
  else
    source = stack[stackcount-2];

  stackcount -= 2;
  stack[stackcount-1] = source;
}

void b_cons()
{
  cell *head = stack[stackcount-1];
  cell *tail = stack[stackcount-2];
/*   assert(TYPE_IND != celltype(head)); */
/*   assert(TYPE_IND != celltype(tail)); */

  stackcount -= 1;
  stack[stackcount-1] = alloc_cell();
  stack[stackcount-1]->tag = TYPE_CONS;
  stack[stackcount-1]->field1 = head;
  stack[stackcount-1]->field2 = tail;
}

void b_head()
{
  cell *arg = stack[stackcount-1];
  assert(TYPE_IND != celltype(arg));
  if (TYPE_CONS == celltype(arg)) {
    stack[stackcount-1] = (cell*)arg->field1;
  }
  else if (TYPE_AREF == celltype(arg)) {
    cell *arrcell = (cell*)arg->field1;
    carray *arr = (carray*)arrcell->field1;
    int index = (int)arg->field2;
    assert(index < arr->size);
    stack[stackcount-1] = (cell*)arr->cells[index];
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

void b_tail()
{
  cell *arg = stack[stackcount-1];
  assert(TYPE_IND != celltype(arg));
  if (TYPE_CONS == celltype(arg)) {
    stack[stackcount-1] = (cell*)arg->field2;
  }
  else if (TYPE_AREF == celltype(arg)) {
    cell *arrcell = (cell*)arg->field1;
    int index = (int)arg->field2;
    stack[stackcount-1] = get_tail(arrcell,index);
  }
  else {
    assert(!"tail: expected a cons cell");
  }
}

void b_islambda()
{
  cell *val = stack[stackcount-1];
  assert(TYPE_IND != celltype(val));
  setbool(TYPE_LAMBDA == celltype(val));
}

void b_isvalue()
{
  cell *val = stack[stackcount-1];
  assert(TYPE_IND != celltype(val));
  setbool(isvalue(val));
}

void b_iscons()
{
  cell *val = stack[stackcount-1];
  assert(TYPE_IND != celltype(val));
  setbool((TYPE_CONS == celltype(val)) || (TYPE_AREF == celltype(val)));
}

void b_isnil()
{
  cell *val = stack[stackcount-1];
  assert(TYPE_IND != celltype(val));
  setbool(TYPE_NIL == celltype(val));
}

void b_isint()
{
  cell *val = stack[stackcount-1];
  assert(TYPE_IND != celltype(val));
  setbool(TYPE_INT == celltype(val));
}

void b_isdouble()
{
  cell *val = stack[stackcount-1];
  assert(TYPE_IND != celltype(val));
  setbool(TYPE_DOUBLE == celltype(val));
}

void b_isstring()
{
  cell *val = stack[stackcount-1];
  assert(TYPE_IND != celltype(val));
  setbool(TYPE_STRING == celltype(val));
}

void b_sqrt()
{
  cell *num = stack[stackcount-1];
  assert(TYPE_IND != celltype(num));
/*   printf("sqrt\n"); */

  if (TYPE_INT == celltype(num)) {
    setdouble(sqrt((double)((int)num->field1)));
  }
  else if (TYPE_DOUBLE == celltype(num)) {
    setdouble(sqrt(celldouble(num)));
  }
  else {
    fprintf(stderr,"sqrt: invalid argument type: (%s)\n",
            cell_types[celltype(num)]);
    exit(1);
  }
}

void b_neg()
{
  cell *num = stack[stackcount-1];
  assert(TYPE_IND != celltype(num));

  if (TYPE_INT == celltype(num)) {
    setint(-((int)num->field1));
  }
  else if (TYPE_DOUBLE == celltype(num)) {
    setdouble(-celldouble(num));
  }
  else {
    fprintf(stderr,"neg: invalid argument type: (%s)\n",
            cell_types[celltype(num)]);
    exit(1);
  }
}

void check_strlist(cell *lst)
{
  cell *c;
  for (c = lst; TYPE_NIL != celltype(c); c = (cell*)c->field2) {
    printf("type %s\n",cell_types[celltype(c)]);
    assert(TYPE_CONS == celltype(c));
    assert(TYPE_STRING == celltype((cell*)c->field1));
  }
}

int have_str(cell *lst, char *str)
{
  cell *c;
  for (c = lst; TYPE_NIL != celltype(c); c = (cell*)c->field2) {
    cell *strcell = (cell*)c->field1;
    if (!strcmp((char*)strcell->field1,str))
      return 1;
  }
  return 0;
}

void add_str(cell ***ptr, char *str)
{
  cell *newstr = alloc_cell();
  newstr->tag = TYPE_STRING;
  newstr->field1 = strdup(str);
  (**ptr) = alloc_cell();
  (**ptr)->tag = TYPE_CONS;
  (**ptr)->field1 = newstr;
  *ptr = (cell**)&((**ptr)->field2);
}

void b_union()
{
  cell *set1 = stack[stackcount-1];
  cell *set2 = stack[stackcount-2];

  if ((TYPE_STRING != celltype(set1)) || !is_set((char*)set1->field1)) {
    fprintf(stderr,"Invalid first argument for union: ");
    print_codef(stderr,set1);
    fprintf(stderr,"\n");
    exit(1);
  }

  if ((TYPE_STRING != celltype(set2)) || !is_set((char*)set2->field1)) {
    fprintf(stderr,"Invalid second argument for union: ");
    print_codef(stderr,set2);
    fprintf(stderr,"\n");
    exit(1);
  }

  stackcount -= 1;
  stack[stackcount-1] = alloc_cell();
  stack[stackcount-1]->tag = TYPE_STRING;
  stack[stackcount-1]->field1 = set_union((char*)set1->field1,(char*)set2->field1);
}

void b_intersect()
{
  cell *set1 = stack[stackcount-1];
  cell *set2 = stack[stackcount-2];

  if ((TYPE_STRING != celltype(set1)) || !is_set((char*)set1->field1)) {
    fprintf(stderr,"Invalid first argument for intersection: ");
    print_codef(stderr,set1);
    fprintf(stderr,"\n");
    exit(1);
  }

  if ((TYPE_STRING != celltype(set2)) || !is_set((char*)set2->field1)) {
    fprintf(stderr,"Invalid second argument for intersection: ");
    print_codef(stderr,set2);
    fprintf(stderr,"\n");
    exit(1);
  }

  stackcount -= 1;
  stack[stackcount-1] = alloc_cell();
  stack[stackcount-1]->tag = TYPE_STRING;
  stack[stackcount-1]->field1 = set_intersection((char*)set1->field1,(char*)set2->field1);
}

void b_convertarray()
{
  cell *retcell = (cell*)stack[stackcount-1];
  stackcount--;

  if (TYPE_INT != celltype(retcell)) {
    printf("convertarray: first arg should be an int, got ");
    print_code(retcell);
    printf("\n");
    abort();
  }

  // FIXME: this doesn't correctly handle cyclic lists at present!
  cell *top = stack[stackcount-1];
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

  stack[stackcount-1] = retcell;
}

void b_arrayitem()
{
  cell *refcell = stack[stackcount-2];
  cell *indexcell = stack[stackcount-1];
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
  stackcount--;

  cell *arrcell = (cell*)refcell->field1;
  carray *arr = (carray*)arrcell->field1;
  int refindex = (int)refcell->field2;
  assert(refindex+index < arr->size);
  stack[stackcount-1] = (cell*)arr->cells[refindex+index];
}

void b_arrayhas()
{
  cell *refcell = stack[stackcount-2];
  cell *indexcell = stack[stackcount-1];
  assert(TYPE_IND != celltype(refcell));
  assert(TYPE_IND != celltype(indexcell));
  stackcount--;

  if (TYPE_INT != celltype(indexcell)) {
    printf("arrayhas: index must be an integer, got ");
    print_code(indexcell);
    printf("\n");
    abort();
  }

  if (TYPE_AREF != celltype(refcell)) {
    stack[stackcount-1] = globnil;
    return;
  }

  int index = (int)indexcell->field1;

  cell *arrcell = (cell*)refcell->field1;
  carray *arr = (carray*)arrcell->field1;
  int refindex = (int)refcell->field2;
  if (refindex+index < arr->size)
    stack[stackcount-1] = globtrue;
  else
    stack[stackcount-1] = globnil;
}

void b_arrayext()
{
  cell *lstcell = stack[stackcount-2];
  cell *ncell = stack[stackcount-1];
  assert(TYPE_IND != celltype(lstcell));
  assert(TYPE_IND != celltype(ncell));
  stackcount--;

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

  stack[stackcount-1] = arr->cells[n];
}

void b_arraysize()
{
  cell *refcell = stack[stackcount-1];
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

    stack[stackcount-1] = arr->sizecell;
  }
  else if (TYPE_CONS == celltype(refcell)) {
    stack[stackcount-1] = globzero; // FIXME: should this be nil, like arrayoptlen?
  }
  else {
    printf("arraysize: expected aref or cons, got ");
    print_code(refcell);
    printf("\n");
    abort();
  }
}

void b_arraytail()
{
  cell *refcell = stack[stackcount-1];
  if (TYPE_AREF == celltype(refcell)) {
    cell *arrcell = (cell*)refcell->field1;
    assert(0 == (int)refcell->field2);
    carray *arr = (carray*)arrcell->field1;

    stack[stackcount-1] = arr->tail;
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

void b_arrayoptlen()
{
  cell *refcell = stack[stackcount-1];
  if (TYPE_AREF == celltype(refcell)) {
    cell *arrcell = (cell*)refcell->field1;
    assert(0 == (int)refcell->field2);
    carray *arr = (carray*)arrcell->field1;

    if (TYPE_NIL == celltype(arr->tail))
      b_arraysize();
    else
      stack[stackcount-1] = globnil;
  }
  else if (TYPE_CONS == celltype(refcell)) {
    stack[stackcount-1] = globnil;
  }
  else {
    printf("arrayoptlen: expected aref or cons, got ");
    print_code(refcell);
    printf("\n");
    abort();
  }
}

const builtin builtin_info[NUM_BUILTINS] = {
{ "+",          2, 2, 1, b_add      },
{ "-",          2, 2, 1, b_subtract },
{ "*",          2, 2, 1, b_multiply },
{ "/",          2, 2, 1, b_divide   },
{ "%",          2, 2, 1, b_mod      },
{ "=",          2, 2, 1, b_eq       },
{ "!=",         2, 2, 1, b_ne       },
{ "<",          2, 2, 1, b_lt       },
{ "<=",         2, 2, 1, b_le       },
{ ">",          2, 2, 1, b_gt       },
{ ">=",         2, 2, 1, b_ge       },
{ "&&",         2, 2, 1, b_and      },
{ "||",         2, 2, 1, b_or       },
{ "not",        1, 1, 1, b_not      },
{ "ap",         2, 2, 1, b_ap       },

{ "if",         3, 1, 0, b_if       },
{ "cons",       2, 0, 1, b_cons     },
{ "head",       1, 1, 0, b_head     },
{ "tail",       1, 1, 0, b_tail     },

{ "lambda?",    1, 1, 1, b_islambda },
{ "value?",     1, 1, 1, b_isvalue  },
{ "cons?",      1, 1, 1, b_iscons   },
{ "nil?",       1, 1, 1, b_isnil    },
{ "int?",       1, 1, 1, b_isint    },
{ "double?",    1, 1, 1, b_isdouble },
{ "string?",    1, 1, 1, b_isstring },
{ "sqrt",       1, 1, 1, b_sqrt     },

{ "neg",        1, 1, 1, b_neg      },

{ "union",      2, 2, 1, b_union    },
{ "intersect",  2, 2, 1, b_intersect},

{ "convertarray",   2, 2, 1, b_convertarray   },
{ "arrayitem",      2, 2, 0, b_arrayitem      },
{ "arrayhas",       2, 2, 1, b_arrayhas       },
{ "arrayext",       2, 2, 1, b_arrayext       },
{ "arraysize",      1, 1, 1, b_arraysize      },
{ "arraytail",      1, 1, 1, b_arraytail      },
{ "arrayoptlen",    1, 1, 1, b_arrayoptlen    },

};

