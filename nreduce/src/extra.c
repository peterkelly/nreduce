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
"NUMBER",
"STRING",
"ARRAY",
 };

void fatal(const char *msg)
{
  fprintf(stderr,"%s\n",msg);
  assert(0);
  exit(1);
}

int debug(int depth, const char *format, ...)
{
  va_list ap;
  int i;
  int r;
  for (i = 0; i < depth; i++)
    printf("  ");
  va_start(ap,format);
  r = vprintf(format,ap);
  va_end(ap);
  return r;
}

void debug_stage(const char *name)
{
  int npad = 80-2-strlen(name);
  int i;
  if (!trace)
    return;
  for (i = 0; i < npad/2; i++)
    printf("=");
  printf(" %s ",name);
  for (; i < npad; i++)
    printf("=");
  printf("\n");
}

int count_args(cell *c)
{
  int napps = 0;
  c = resolve_ind(c);
  while (TYPE_APPLICATION == celltype(c)) {
    c = resolve_ind((cell*)c->field1);
    napps++;
  }
  return napps;
}

cell *get_arg(cell *c, int argno)
{
  argno = count_args(c)-argno-1;
  assert(0 <= argno);
  c = resolve_ind(c);
  while (0 < argno) {
    assert(TYPE_APPLICATION == celltype(c));
    c = resolve_ind((cell*)c->field1);
    argno--;
  }
  return (cell*)c->field2;
}

void print_hex(int c)
{
  if (0xA > c)
    printf("%c",'0'+c);
  else
    printf("%c",'A'+(c-0xA));
}

void print_hexbyte(unsigned char val)
{
  unsigned char hi = ((val & 0xF0) >> 4);
  unsigned char lo = (val & 0x0F);
  print_hex(hi);
  print_hex(lo);
}

void print_bin(void *ptr, int nbytes)
{
  int i;
  unsigned char *data = (unsigned char*)ptr;
  printf("   ");
  for (i = 0; i < nbytes; i++) {
    int bit;
    for (bit = 7; 0 <= bit; bit--)
      printf("%c",(data[i] & (0x1 << bit)) ? '1' : '0');
    printf(" ");
  }
  printf("\n0x ");
  for (i = 0; i < nbytes; i++) {
    print_hexbyte(data[i]);
    printf("       ");
  }
  printf("\n");
}

void print_bin_rev(void *ptr, int nbytes)
{
  int i;
  unsigned char *data = (unsigned char*)ptr;
  printf("   ");
  for (i = nbytes-1; 0 <= i; i--) {
    int bit;
    for (bit = 7; 0 <= bit; bit--)
      printf("%c",(data[i] & (0x1 << bit)) ? '1' : '0');
    printf(" ");
  }
  printf("\n0x ");
  for (i = nbytes-1; 0 <= i; i--) {
    print_hexbyte(data[i]);
    printf("       ");
  }
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
      letrec *rec;
      printf("LETREC\n");
      for (rec = (letrec*)c->field1; rec; rec = rec->next) {
        printf("%s             %11s ",prefix,"def");
        for (i = 0; i < indent+1; i++)
          printf("  ");
        if (rec->strict)
          printf("!%s\n",rec->name);
        else
          printf(" %s\n",rec->name);
        print1(prefix,rec->value,indent+2);
      }
      print1(prefix,(cell*)c->field2,indent+1);
      break;
    }
    case TYPE_HOLE:
      printf("(hole)\n"); break;
    case TYPE_NIL:
      printf("nil\n"); break;
    case TYPE_NUMBER:
      print_double(stdout,celldouble(c));
      printf("\n");
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

static void down_line(FILE *f, int *line, int *col)
{
  int i;
  (*line)++;
  fprintf(f,"\n");
  for (i = 0; i < *col; i++)
    fprintf(f," ");
}

const char *real_varname(const char *sym)
{
#ifdef SHOW_SUBSTITUTED_NAMES
  return sym;
#else
  if (!strncmp(sym,"_v",2)) {
    int varno = atoi(sym+2);
    assert(oldnames);
    assert(varno <= (int)((oldnames->size-1)*sizeof(char*)));
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
    char *old;
    char *fullname;
    assert(end);
    assert(oldnames);
    assert(varno <= (int)((oldnames->size-1)*sizeof(char*)));
    old = ((char**)oldnames->data)[varno];
    fullname = (char*)malloc(strlen(old)+strlen(sym)+1);
    sprintf(fullname,"%s%s",old,end);
    return fullname;
  }
  else {
    return strdup(sym);
  }
#endif
}

static void print_code1(FILE *f, cell *c, int needbr, cell *parent, int *line, int *col);

void print_abbrev_stack(FILE *f, pntr *data, int count, cell *parent, int *line, int *col)
{
  /* DIS1
  int i;
  for (i = 0; i < count; i++) {
    rtvalue *c = resolve_value(get_pntr(data[i]));
    if (0 < i)
      fprintf(f," ");
    if ((TYPE_NIL == celltype(c)) ||
        (TYPE_NUMBER == celltype(c)) ||
        (TYPE_STRING == celltype(c)))
      print_code1(f,c,1,parent,line,col);
    else
      *col += fprintf(f,"?");
  }
  */
}

static void print_code1(FILE *f, cell *c, int needbr, cell *parent, int *line, int *col)
{
  c = resolve_ind(c);
  if (1 && (c->tag & FLAG_TMP) &&
      (c != globnil) &&
      (TYPE_NIL != celltype(c)) &&
      (TYPE_NUMBER != celltype(c)) &&
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
    switch (celltype(c)) {
    case TYPE_IND:
      assert(0);
      break;
    case TYPE_EMPTY:
      *col += fprintf(f,"empty");
      break;
    case TYPE_APPLICATION: {
      cell *tmp;
      list *l;
      int addedline = 0;
      int argscol;
      int argno;
      int isif;
      list *apps = NULL;
      if (needbr)
        *col += fprintf(f,"(");
      for (tmp = c; TYPE_APPLICATION == celltype(tmp); tmp = resolve_ind((cell*)tmp->field1))
        list_push(&apps,tmp);

      print_code1(f,tmp,0,c,line,col);
      *col += fprintf(f," ");
      argscol = *col;
      argno = 0;
      isif = (((TYPE_BUILTIN == celltype(tmp)) && (B_IF == (int)tmp->field1)) ||
              ((TYPE_SYMBOL == celltype(tmp)) && !strcmp((char*)tmp->field1,"if")));
      for (l = apps; l; l = l->next) {
        cell *app = (cell*)l->data;
        cell *arg = resolve_ind((cell*)app->field2);
        int oldcol;
        int oldline;

        if (!addedline && (0 < argno) && (isif || (TYPE_LAMBDA == celltype(resolve_ind(arg))))) {
          *col = argscol;
          down_line(f,line,col);
        }
        addedline = 0;

        oldcol = *col;
        oldline = *line;
/*         if (app->tag & FLAG_STRICT) */
/*           *col += fprintf(f,"!"); */
        print_code1(f,arg,1,app,line,col);
        if (l->next) {
          if (1 && (oldline != *line)) {
            *col = oldcol;
            down_line(f,line,col);
            addedline = 1;
          }
          else {
            *col += fprintf(f," ");
          }
        }
        argno++;
      }

      list_free(apps,NULL);
      if (needbr)
        fprintf(f,")");
      break;
    }
    case TYPE_LAMBDA:
      if (parent && (TYPE_LAMBDA != celltype(parent)) && (TYPE_LETREC != celltype(parent)))
        *col += fprintf(f,"(");
      *col += fprintf(f,"!%s.",real_varname((char*)c->field1));
      print_code1(f,(cell*)c->field2,0,c,line,col);
      if (parent && (TYPE_LAMBDA != celltype(parent)) && (TYPE_LETREC != celltype(parent)))
        *col += fprintf(f,")");
      break;
    case TYPE_BUILTIN:
      *col += fprintf(f,"%s",builtin_info[(int)c->field1].name);
      break;
    case TYPE_CONS: {
      int pos = 0;
      cell *item;
      *col += fprintf(f,"[");
      for (item = c; TYPE_CONS == celltype(item); item = resolve_ind((cell*)item->field2)) {
        if (0 < pos++)
          *col += fprintf(f,",");
        print_code1(f,(cell*)item->field1,1,c,line,col);
      }
      *col += fprintf(f,"]");
      if (TYPE_NIL != celltype(item))
        print_code1(f,item,1,c,line,col);
      break;
    }
    case TYPE_SYMBOL: {
      char *sym = (char*)c->field1;
      if (!strncmp(sym,"__code",6)) {
        int fno = atoi(sym+6);
        assert(0 <= fno);
        if (NUM_BUILTINS > fno)
          *col += fprintf(f,"%s",builtin_info[fno].name);
        else
          *col += fprintf(f,"%s",get_scomb_index(fno-NUM_BUILTINS)->name);
      }
      else {
        *col += fprintf(f,"%s",real_varname(sym));
      }
      break;
    }
    case TYPE_SCREF: {
      char *scname = real_scname(((scomb*)c->field1)->name);
      *col += fprintf(f,"%s",scname);
      free(scname);
      break;
    }
    case TYPE_LETREC: {
      int count = 0;
      letrec *rec = (letrec*)c->field1;
      if (parent && (TYPE_LAMBDA != celltype(parent)) && (TYPE_LETREC != celltype(parent)))
        *col += fprintf(f,"(");
      *col += fprintf(f,"let (");
      while (rec) {
        int col2 = *col;
        if (0 < count++)
          down_line(f,line,&col2);
        if (rec->strict)
          col2 += fprintf(f,"(%s! ",real_varname(rec->name));
        else
          col2 += fprintf(f,"(%s ",real_varname(rec->name));
        print_code1(f,rec->value,1,c,line,&col2);
        rec = rec->next;
        fprintf(f,")");
      }
      fprintf(f,")");
      (*col)--;
      down_line(f,line,col);
      print_code1(f,(cell*)c->field2,1,c,line,col);
      if (parent && (TYPE_LAMBDA != celltype(parent)) && (TYPE_LETREC != celltype(parent)))
        *col += fprintf(f,")");
      break;
    }
    case TYPE_HOLE:
      fprintf(f,"(hole)");
      break;
    case TYPE_NIL:
      fprintf(f,"nil");
      break;
    case TYPE_NUMBER:
      print_double(f,celldouble(c));
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
      /* DIS1 
      carray *arr = (carray*)c->field1;
      int i;
      *col += fprintf(f,"ARRAY[");
      for (i = 0; i < arr->size; i++) {
        if (i > 0)
          *col += fprintf(f,",");
        print_code1(f,arr->cells[i],0,c,line,col);
      }
      fprintf(f,"]");
      */
      break;
    }
    case TYPE_FRAME: {
      /* DIS1 
      frame *frm = (frame*)c->field1;
      *col += fprintf(f,"(FRAME (");
      if (frm->data)
        print_abbrev_stack(f,frm->data,frm->count,c,line,col);
      else
        *col += fprintf(f,"...");
      *col += fprintf(f,") %s)",function_name(frm->fno));
      */
      break;
    }
    case TYPE_CAP: {
      /* DIS1 
      cap *cp = (cap*)c->field1;
      *col += fprintf(f,"(CAP (");
      print_abbrev_stack(f,cp->data,cp->count,c,line,col);
      *col += fprintf(f,") %s)",function_name(cp->fno));
      */
      break;
    }
    default:
      assert(0);
      break;
    }
  }
}

void print_codef2(FILE *f, cell *c, int pos)
{
  int line = 0;
  int col = pos;
  cleargraph(c,FLAG_TMP);
  print_code1(f,c,0,NULL,&line,&col);
}

void print_codef(FILE *f, cell *c)
{
  int line = 0;
  int col = 0;
  cleargraph(c,FLAG_TMP);
  print_code1(f,c,0,NULL,&line,&col);
}
void print_code(cell *c)
{
  print_codef(stdout,c);
}

void print_scomb_code(scomb *sc)
{
  int i;
  int col = 0;
  char *scname = real_scname(sc->name);
  col += debug(0,"%s ",scname);
  free(scname);
  for (i = 0; i < sc->nargs; i++) {
    if (sc->strictin) {
      if (sc->strictin[i])
        col += debug(0,"!");
    }
    col += debug(0,"%s ",real_varname(sc->argnames[i]));
  }
  col += debug(0,"= ");
  print_codef2(stdout,sc->body,col);
}

void print_scombs1()
{
  scomb *sc;
  for (sc = scombs; sc; sc = sc->next) {
    if (strncmp(sc->name,"__",2)) {
      print_scomb_code(sc);
      debug(0,"\n");
    }
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
