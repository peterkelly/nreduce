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
 * $Id: builtins.c 613 2007-08-23 11:40:12Z pmkelly $
 *
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#define BUILTINS_C

#include "runtime/builtins.h"

#ifndef O_BINARY
#define O_BINARY 0
#endif

const char *numnames[4] = {"first", "second", "third", "fourth"};
unsigned char NAN_BITS[8] = { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xF8, 0xFF };

const builtin builtin_info[NUM_BUILTINS];

void setnumber(pntr *cptr, double val)
{
  if (isnan(val))	//// nan: NotANumber, which is a c func
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
  set_error(tsk,"%s: %s argument must be of type %s, but got %s",
            builtin_info[bif].name,numnames[builtin_info[bif].nargs-1-argno],
            cell_types[type],cell_types[pntrtype(arg)]);
}

void invalid_binary_args(task *tsk, pntr *argstack, int bif)
{
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

int pntr_is_char(pntr p)
{
  if (CELL_NUMBER == pntrtype(p)) {
    double d = pntrdouble(p);
    if ((floor(d) == d) && (0 <= d) && (255 >= d))
      return 1;
  }
  return 0;
}

//// make list using embeded cons with the given data
//// e.g. str == (cons str[0] (cons str[1] (cons str[2] nil)))
//// return a pntr, pointing to the list

pntr data_to_list(task *tsk, const char *data, int size, pntr tail)
{
  pntr p = tsk->globnilpntr;
  pntr *prev = &p;
  int i;
  for (i = 0; i < size; i++) {
    cell *ch = alloc_cell(tsk);
    ch->type = CELL_CONS;
    set_pntrdouble(ch->field1,data[i]);
    make_pntr(*prev,ch); //// when i==0, *prev == p, then p is pointing to the result list
    prev = &ch->field2;	//// ch->field2 will be used as the address of next cons, or the tail 
  }
  *prev = tail;
  return p;
}

//// convert a string into array list (cons cons...)
pntr string_to_array(task *tsk, const char *str)
{
  return data_to_list(tsk,str,strlen(str),tsk->globnilpntr);
}

//// convert a list(cons') to a string
int array_to_string(pntr refpntr, char **str)
{
  pntr p = refpntr;
  array *buf = array_new(1,0);
  char zero = '\0';
  int badtype = -1;

  *str = NULL;

  while (badtype < 0) {

    if (CELL_CONS == pntrtype(p)) {
      cell *c = get_pntr(p);
      pntr head = resolve_pntr(c->field1);
//      cell *headcell = get_pntr(head);
//      cell *Lheadcell = get_pntr(headcell->field1);
//      cell *LLheadcell = get_pntr(Lheadcell->field1);
      pntr tail = resolve_pntr(c->field2);
//      cell *tailcell = get_pntr(tail);
//      cell *Rtailcell = get_pntr(tailcell->field2);
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
    else if (CELL_NIL == pntrtype(p)) {
      break;
    }
    else {
      badtype = pntrtype(p);
      break;
    }
  }

  if (badtype >= 0) {
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
  else {
    set_error(tsk,"head: expected cons, got %s",cell_types[pntrtype(arg)]);
  }
}

static void b_tail(task *tsk, pntr *argstack)
{
  pntr arg = argstack[0];

  if (CELL_CONS == pntrtype(arg)) {
    cell *conscell = get_pntr(arg);
    argstack[0] = conscell->field2;
  }
  else {
    set_error(tsk,"tail: expected cons, got %s",cell_types[pntrtype(arg)]);
  }
}

static void b_numtostring(task *tsk, pntr *argstack)
{
  pntr p = argstack[0];
  char str[100];

  CHECK_ARG(0,CELL_NUMBER);
  format_double(str,100,pntrdouble(p));
  assert(strlen(str) > 0);
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

static void b_abs(task *tsk, pntr *argstack)
{
  CHECK_ARG(0,CELL_NUMBER);
  setnumber(&argstack[0],fabs(pntrdouble(argstack[0])));
}

static void b_iscons(task *tsk, pntr *argstack)
{
  setbool(tsk,&argstack[0],CELL_CONS == pntrtype(argstack[0]));
}

int get_builtin(const char *name)
{
  int i;
  for (i = 0; i < NUM_BUILTINS; i++)
    if (!strcmp(name,builtin_info[i].name))
      return i;
  return -1;
}

//// a generate random number
static void b_randomnum(task *tsk, pntr *argstack)
{
	CHECK_ARG(0, CELL_NUMBER);
	setnumber(&argstack[0], Uniform(1.0, 9.0));
//    setnumber(&argstack[0], zzip_file_real(3));
}

//// Initialization of builtin functions' information
const builtin builtin_info[NUM_BUILTINS] = {
/* Arithmetic operations */
{ "+",              2, 2, b_add            },
{ "-",              2, 2, b_subtract       },
{ "*",              2, 2, b_multiply       },
{ "/",              2, 2, b_divide         },
{ "%",              2, 2, b_mod            },

/* Numeric comparison */
{ "==",             2, 2, b_eq             },
{ "!=",             2, 2, b_ne             },
{ "<",              2, 2, b_lt             },
{ "<=",             2, 2, b_le             },
{ ">",              2, 2, b_gt             },
{ ">=",             2, 2, b_ge             },

/* Bit operations */
{ "<<",             2, 2, b_bitshl         },
{ ">>",             2, 2, b_bitshr         },
{ "&",              2, 2, b_bitand         },
{ "|",              2, 2, b_bitor          },
{ "^",              2, 2, b_bitxor         },
{ "~",              1, 1, b_bitnot         },

/* Numeric functions */
{ "sqrt",           1, 1, b_sqrt           },
{ "floor",          1, 1, b_floor          },
{ "ceil",           1, 1, b_ceil           },
{ "numtostring",    1, 1, b_numtostring    },
{ "stringtonum1",   1, 1, b_stringtonum1   },

/* Logical operations */
{ "if",             3, 1, b_if             },
{ "and",            2, 2, b_and            },
{ "or",             2, 2, b_or             },
{ "not",            1, 1, b_not            },

/* Lists and arrays */
{ "cons",           2, 0, b_cons           },
{ "head",           1, 1, b_head           },
{ "tail",           1, 1, b_tail           },

/* Sequential/parallel directives */
{ "seq",            2, 1, b_seq            },

/* Other */
{ "error1",         1, 1, b_error1         },

{ "abs",            1, 1, b_abs            },
{ "iscons",         1, 1, b_iscons         },

//// my builtin functions
{ "randomnum",      1, 1, b_randomnum      },

};
