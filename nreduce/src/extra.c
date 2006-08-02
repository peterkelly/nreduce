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

#define EXTRA_C

#include "grammar.tab.h"
#include "gcode.h"
#include "nreduce.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <assert.h>
#include <ctype.h>
#include <stdarg.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <dirent.h>
#include <errno.h>

cell *parse_root = NULL;
extern gprogram *cur_program;
extern array *oldnames;

const char *cell_types[NUM_CELLTYPES] = {
"EMPTY",
"APPLICATION",
"LAMBDA",
"BUILTIN",
"CONS",
"SYMBOL",
"LETREC",
"RES2",
"RES3",
"IND",
"RES1",
"SCREF",
"AREF",
"HOLE",
"FRAME",
"CAP",
"NIL",
"INT",
"DOUBLE",
"STRING",
"ARRAY",
 };

void fatal(const char *msg)
{
  fprintf(stderr,"%s\n",msg);
  assert(0);
  exit(1);
}

void debug(int depth, const char *format, ...)
{
  va_list ap;
  int i;
  for (i = 0; i < depth; i++)
    printf("  ");
  va_start(ap,format);
  vprintf(format,ap);
  va_end(ap);
}

void debug_stage(const char *name)
{
  if (!trace)
    return;
  int npad = 80-2-strlen(name);
  int i;
  for (i = 0; i < npad/2; i++)
    printf("=");
  printf(" %s ",name);
  for (; i < npad; i++)
    printf("=");
  printf("\n");
}

static void print1(char *prefix, cell *c, int indent)
{
  int i;

  printf("%s%p    %11s ",prefix,c,cell_types[celltype(c)]);
  for (i = 0; i < indent; i++)
    printf("  ");

  if (c->tag & FLAG_TMP) {
    printf("%p\n",c);
  }
  else {
    c->tag |= FLAG_TMP;
    switch (celltype(c)) {
    case TYPE_IND:
      printf("IND\n");
      print1(prefix,(cell*)c->field1,indent+1);
      break;
    case TYPE_EMPTY:
      printf("empty\n");
      break;
    case TYPE_APPLICATION:
      if (c->tag & FLAG_STRICT)
        printf("@S\n");
      else
        printf("@\n");
      print1(prefix,(cell*)c->field1,indent+1);
      print1(prefix,(cell*)c->field2,indent+1);
      break;
    case TYPE_LAMBDA:
      printf("lambda %s\n",(char*)c->field1);
      print1(prefix,(cell*)c->field2,indent+1);
      break;
    case TYPE_BUILTIN:
      printf("%s\n",builtin_info[(int)c->field1].name);
      break;
    case TYPE_CONS:
      printf(":\n");
      print1(prefix,(cell*)c->field1,indent+1);
      print1(prefix,(cell*)c->field2,indent+1);
      break;
    case TYPE_SYMBOL:
      printf("%s\n",(char*)c->field1);
      break;
    case TYPE_SCREF:
      printf("%s\n",((scomb*)c->field1)->name);
      break;
    case TYPE_LETREC: {
      printf("LETREC\n");
      letrec *rec;
      for (rec = (letrec*)c->field1; rec; rec = rec->next) {
        printf("%s             %11s ",prefix,"def");
        for (i = 0; i < indent+1; i++)
          printf("  ");
        printf("%s\n",rec->name);
        print1(prefix,rec->value,indent+2);
      }
      print1(prefix,(cell*)c->field2,indent+1);
      break;
    }
    case TYPE_HOLE:
      printf("(hole)\n"); break;
    case TYPE_NIL:
      printf("nil\n"); break;
    case TYPE_INT:
      printf("%d\n",(int)c->field1);
      break;
    case TYPE_DOUBLE:
      printf("%f\n",celldouble(c));
      break;
    case TYPE_STRING:
      printf("%s\n",(char*)c->field1);
      break;
    default:
      printf("**** INVALID\n");
      assert(0);
      break;
    }
  }
}

void print(cell *c)
{
  cleargraph(c,FLAG_TMP);
  print1("",c,0);
}

static void print_code_indent(FILE *f, int indent)
{
  int i;
  for (i = 0; i < indent; i++)
    fprintf(f,"  ");
}

const char *real_varname(const char *sym)
{
#ifdef SHOW_SUBSTITUTED_NAMES
  return sym;
#else
  if (!strncmp(sym,"_v",2)) {
    int varno = atoi(sym+2);
    assert(oldnames);
    assert(varno <= (oldnames->size-1)*sizeof(char*));
    return ((char**)oldnames->data)[varno];
  }
  else {
    return sym;
  }
#endif
}

char *real_scname(const char *sym)
{
#ifdef SHOW_SUBSTITUTED_NAMES
  return strdup(sym);
#else
  if (!strncmp(sym,"_v",2)) {
    char *end = NULL;
    int varno = strtol(sym+2,&end,10);
    assert(end);
    assert(oldnames);
    assert(varno <= (oldnames->size-1)*sizeof(char*));
    char *old = ((char**)oldnames->data)[varno];
    char *fullname = (char*)malloc(strlen(old)+strlen(sym)+1);
    sprintf(fullname,"%s%s",old,end);
    return fullname;
  }
  else {
    return strdup(sym);
  }
#endif
}

static void print_abbrev_stack(stack *s)
{
  int i;
  for (i = 0; i < s->count; i++) {
    if (0 < i)
      printf(" ");
    cell *c = resolve_ind((cell*)s->data[i]);
    if ((TYPE_NIL == celltype(c)) ||
        (TYPE_INT == celltype(c)) ||
        (TYPE_DOUBLE == celltype(c)) ||
        (TYPE_STRING == celltype(c)))
      print_code(c);
    else
      printf("?");
  }
}

static void print_code1(FILE *f, cell *c, int needbr, int inlist, int indent, cell *parent)
{
  c = resolve_ind(c);
  if (1 && (c->tag & FLAG_TMP) &&
      (c != globnil) &&
      (TYPE_NIL != celltype(c)) &&
      (TYPE_INT != celltype(c)) &&
      (TYPE_DOUBLE != celltype(c)) &&
      (TYPE_STRING != celltype(c)) &&
      (TYPE_BUILTIN != celltype(c)) &&
      (TYPE_SYMBOL != celltype(c)) &&
      (TYPE_SCREF != celltype(c)) &&
      (TYPE_BUILTIN != celltype(c)) &&
      (TYPE_SYMBOL != celltype(c)) &&
      (!isvalue(c))) {
    fprintf(f,"###");
  }
  else {
    c->tag |= FLAG_TMP;

/*     if (TYPE_APPLICATION != celltype(c)) { */
/*       if (c->tag & FLAG_STRICT) */
/*         fprintf(f,"#"); */
/*       else */
/*         fprintf(f," "); */
/*     } */

    switch (celltype(c)) {
    case TYPE_IND:
      assert(0);
      break;
    case TYPE_EMPTY:
      fprintf(f,"empty");
    break;
    case TYPE_APPLICATION:
/*       if (needbr) { */
/*         printf("\n"); */
/*         print_code_indent(f,indent); */
/*       } */

      if (needbr)
        fprintf(f,"(");
      print_code1(f,(cell*)c->field1,0,0,indent,c);
      fprintf(f," ");
      if (c->tag & FLAG_STRICT)
        fprintf(f,"!");
      print_code1(f,(cell*)c->field2,1,0,indent+1,c);
      if (needbr)
        fprintf(f,")");
      break;
    case TYPE_LAMBDA:
      if (parent && (TYPE_LAMBDA != celltype(parent)) && (TYPE_LETREC != celltype(parent)))
        fprintf(f,"(");
      fprintf(f,"!%s.",real_varname((char*)c->field1));
      print_code1(f,(cell*)c->field2,0,0,indent,c);
      if (parent && (TYPE_LAMBDA != celltype(parent)) && (TYPE_LETREC != celltype(parent)))
        fprintf(f,")");
      break;
    case TYPE_BUILTIN:
      fprintf(f,"%s",builtin_info[(int)c->field1].name);
      break;
    case TYPE_CONS:
      if (!inlist)
        fprintf(f,"[");
      print_code1(f,(cell*)c->field1,0,0,indent,c);
      if (TYPE_NIL != celltype((cell*)c->field2)) {
        fprintf(f,",");
        print_code1(f,(cell*)c->field2,0,1,indent,c);
      }
      if (!inlist)
        fprintf(f,"]");
      break;
    case TYPE_SYMBOL: {
      char *sym = (char*)c->field1;
      if (!strncmp(sym,"__code",6)) {
        int fno = atoi(sym+6);
        assert(0 <= fno);
        if (NUM_BUILTINS > fno)
          fprintf(f,"%s",builtin_info[fno].name);
        else
          fprintf(f,"%s",get_scomb_index(fno-NUM_BUILTINS)->name);
      }
      else {
        fprintf(f,"%s",real_varname(sym));
      }
      break;
    }
    case TYPE_SCREF: {
      char *scname = real_scname(((scomb*)c->field1)->name);
      fprintf(f,"%s",scname);
      free(scname);
      break;
    }
    case TYPE_LETREC: {
      letrec *rec = (letrec*)c->field1;
      if (parent && (TYPE_LAMBDA != celltype(parent)) && (TYPE_LETREC != celltype(parent)))
        fprintf(f,"(");
      fprintf(f,"\n");
      print_code_indent(f,indent);
      fprintf(f,"letrec ");
      while (rec) {
        fprintf(f,"\n");
        print_code_indent(f,indent+1);
        fprintf(f,"%s (",real_varname(rec->name));
        print_code1(f,rec->value,0,0,indent+2,c);

        rec = rec->next;
        fprintf(f,")");
      }
      fprintf(f,"\n");
      print_code_indent(f,indent);
      fprintf(f,"in ");
      fprintf(f,"\n");
      print_code_indent(f,indent+1);
      print_code1(f,(cell*)c->field2,0,0,indent+1,c);
      if (parent && (TYPE_LAMBDA != celltype(parent)) && (TYPE_LETREC != celltype(parent)))
        fprintf(f,")");
      break;
    case TYPE_HOLE:
      fprintf(f,"(hole)");
      break;
    case TYPE_NIL:
      fprintf(f,"nil");
      break;
    case TYPE_INT:
      fprintf(f,"%d",(int)c->field1);
      break;
    case TYPE_DOUBLE:
      fprintf(f,"%f",celldouble(c));
      break;
    case TYPE_STRING: {
      char *ch;
      fprintf(f,"\"");
      for (ch = (char*)c->field1; *ch; ch++) {
        if ('\n' == *ch)
          fprintf(f,"\\n");
        else if ('"' == *ch)
          fprintf(f,"\\\"");
        else
          fprintf(f,"%c",*ch);
      }
      fprintf(f,"\"");
      break;
    }
    case TYPE_AREF: {
      cell *arrcell = (cell*)c->field1;
      carray *arr = (carray*)arrcell->field1;
      int index = (int)c->field2;
      fprintf(f,"(array ref %d/%d)",index,arr->size);
      break;
    }
    case TYPE_ARRAY: {
      carray *arr = (carray*)c->field1;
      printf("ARRAY[");
      int i;
      for (i = 0; i < arr->size; i++) {
        if (i > 0)
          printf(",");
        print_code1(f,arr->cells[i],0,0,indent,c);
      }
      printf("]");
      break;
    }
    default:
      assert(0);
      break;
    }
    case TYPE_FRAME: {
      frame *f = (frame*)c->field1;
/*       printf("(FRAME %d %s)",f->s->count,function_name(f->fno)); */
      printf("(FRAME (");
      print_abbrev_stack(f->s);
      printf(") %s)",function_name(f->fno));
      break;
    }
    case TYPE_CAP: {
      cap *cp = (cap*)c->field1;
/*       printf("(CAP %d %s)",cp->s->count,function_name(cp->fno)); */
      printf("(CAP (");
      print_abbrev_stack(cp->s);
      printf(") %s)",function_name(cp->fno));
      break;
    }
    }
  }
}

void print_codef(FILE *f, cell *c)
{
  cleargraph(c,FLAG_TMP);
  print_code1(f,c,0,0,0,NULL);
}
void print_code(cell *c)
{
  print_codef(stdout,c);
}

void print_scomb_code(scomb *sc)
{
  int i;
  char *scname = real_scname(sc->name);
  debug(0,"%s ",scname);
  free(scname);
  for (i = 0; i < sc->nargs; i++) {
    if (sc->strictin) {
      if (sc->strictin[i])
        debug(0,"!");
    }
    debug(0,"%s ",real_varname(sc->argnames[i]));
  }
  debug(0,"= ");
  print_code(sc->body);
}

void print_scombs1()
{
  scomb *sc;
  for (sc = scombs; sc; sc = sc->next) {
    print_scomb_code(sc);
    debug(0,"\n");
  }
}

void print_scombs2()
{
  scomb *sc;
  debug(0,"\n--------------------\n\n");
  for (sc = scombs; sc; sc = sc->next) {
    int i;
    debug(0,"%s ",sc->name);
    for (i = 0; i < sc->nargs; i++)
      debug(0,"%s ",sc->argnames[i]);
    debug(0,"= ");
    print_code(sc->body);
    debug(0,"\n");
    print(sc->body);
    debug(0,"\n");
  }
  debug(0,"\n");
}
