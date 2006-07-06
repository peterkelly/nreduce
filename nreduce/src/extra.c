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

const char *cell_types[NUM_CELLTYPES] = {
"EMPTY",
"APPLICATION",
"LAMBDA",
"BUILTIN",
"CONS",
"SYMBOL",
"LETREC",
"VARDEF",
"VARLNK",
"IND",
"FUNCTION",
"SCREF",
"AREF",
"RES4",
"RES5",
"RES6",
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


void cleardir(char *dirname)
{
  DIR *dir;
  struct dirent *entry;
  struct stat statbuf;

  if (NULL == (dir = opendir(dirname)))
    return;

  while (NULL != (entry = readdir(dir))) {
    char *path = (char*)malloc(strlen(dirname)+1+strlen(entry->d_name)+1);
    sprintf(path,"%s/%s",dirname,entry->d_name);
    if (0 != stat(path,&statbuf)) {
      fprintf(stderr,"Can't stat %s: %s\n",path,strerror(errno));
      exit(1);
    }
    if (S_ISREG(statbuf.st_mode)) {
      if (0 != unlink(path)) {
        fprintf(stderr,"Can't delete %s: %s\n",path,strerror(errno));
        exit(1);
      }
    }
    free(path);
  }

  closedir(dir);

  if (0 != rmdir(dirname)) {
    fprintf(stderr,"Can't remove directory %s: %s\n",dirname,strerror(errno));
    exit(1);
  }
}








void cellmsg(FILE *f, cell *c, const char *format, ...)
{
  cell *source = resolve_source(c);
  va_list ap;
  if (source && source->filename)
    fprintf(f,"%s:%d: ",source->filename,source->lineno);
  va_start(ap,format);
  vfprintf(f,format,ap);
  va_end(ap);
}

void clearprinted()
{
  block *bl;
  int i;
  for (bl = blocks; bl; bl = bl->next)
    for (i = 0; i < BLOCK_SIZE; i++)
      bl->cells[i].tag &= ~FLAG_PRINTED;
}

void print1(char *prefix, cell *c, int indent)
{
  int i;

/*   if (c->filename) { */
/*     printf("%-16s:%-3d ",c->filename,c->lineno); */
/*   } */
/*   else { */
/*     printf("%-16s:%-3s ","",""); */
/*   } */

  push(c);
  printf("%s#%05d    %11s ",prefix,c->id,cell_types[celltype(c)]);
  for (i = 0; i < indent; i++)
    printf("  ");

  if (c->tag & FLAG_PRINTED) {
    printf("#%05d\n",c->id);
  }
  else {
    c->tag |= FLAG_PRINTED;
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
      cell *lnk;
      printf("LETREC\n");
      for (lnk = (cell*)c->field1; lnk; lnk = (cell*)lnk->field2) {
        print1(prefix,(cell*)lnk->field1,indent+1);
      }
      print1(prefix,(cell*)c->field2,indent+1);
      break;
    }
    case TYPE_VARDEF:
      printf("%s\n",(char*)c->field1);
      print1(prefix,(cell*)c->field2,indent+1);
      break;
    case TYPE_VARLNK:
      assert(0);
      break;
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
    case TYPE_FUNCTION:
      printf("f/%d/%d\n",(int)c->field1,(int)c->field2);
      break;
    default:
      printf("**** INVALID\n");
      assert(0);
      break;
    }
  }
  pop();
}

void print(cell *c)
{
  clearprinted();
  print1("",c,0);
}

void print_escaped(char *t, const char *s)
{
  for (; *s; s++) {
    if ('\n' == *s) {
      *(t++) = '\\';
      *(t++) = '\n';
    }
    else if (('\"' == *s) || ('<' == *s) || ('>' == *s)) {
      *(t++) = '\\';
      *(t++) = *s;
    }
    else  {
      *(t++) = *s;
    }
  }
  *t = '\0';
}

void print_dot(FILE *f, cell *c)
{
  char label[1024];
  c = resolve_ind(c);

  if (!c || (c->tag & FLAG_PRINTED))
    return;

  c->tag |= FLAG_PRINTED;

  push(c);

  switch (celltype(c)) {
  case TYPE_IND:
    assert(0);
    break;
  case TYPE_EMPTY:
    sprintf(label,"empty");
    break;
  case TYPE_APPLICATION:
    sprintf(label,"@");
    fprintf(f,"  cell%p:left -> cell%p:top\n",c,(cell*)c->field1);
    fprintf(f,"  cell%p:right -> cell%p:top\n",c,(cell*)c->field2);
    print_dot(f,(cell*)c->field1);
    print_dot(f,(cell*)c->field2);
    break;
  case TYPE_LAMBDA:
    sprintf(label,"!%s",(char*)c->field1);
    fprintf(f,"  cell%p:right -> cell%p:top\n",c,(cell*)c->field2);
    print_dot(f,(cell*)c->field2);
    break;
  case TYPE_BUILTIN:
    print_escaped(label,builtin_info[(int)c->field1].name);
    break;
  case TYPE_CONS:
    sprintf(label,":");
    fprintf(f,"  cell%p:left -> cell%p:top\n",c,(cell*)c->field1);
    fprintf(f,"  cell%p:right -> cell%p:top\n",c,(cell*)c->field2);
    print_dot(f,(cell*)c->field1);
    print_dot(f,(cell*)c->field2);
    break;
  case TYPE_SYMBOL:
    sprintf(label,"VAR: %s",(char*)c->field1);
    break;
  case TYPE_SCREF:
    sprintf(label,"SCOMB: %s",((scomb*)c->field1)->name);
    break;
  case TYPE_LETREC: {
    cell *lnk;

    sprintf(label,"LETREC");
    for (lnk = (cell*)c->field1; lnk; lnk = (cell*)lnk->field2) {
      print_dot(f,(cell*)lnk->field1);
      fprintf(f,"  cell%p:left -> cell%p:top\n",c,(cell*)lnk->field1);
    }

    print_dot(f,(cell*)c->field2);
    fprintf(f,"  cell%p:right -> cell%p:top\n",c,(cell*)c->field2);

    break;
  }
  case TYPE_VARDEF:
    sprintf(label,"%s",(char*)c->field1);
    print_dot(f,(cell*)c->field2);
    fprintf(f,"  cell%p:left -> cell%p:top\n",c,(cell*)c->field2);
    break;
  case TYPE_NIL:
    sprintf(label,"nil");
    break;
  case TYPE_INT:
    sprintf(label,"%d",(int)c->field1);
    break;
  case TYPE_DOUBLE:
    sprintf(label,"%f",celldouble(c));
    break;
  case TYPE_STRING:
    print_escaped(label,(char*)c->field1);
    break;
  case TYPE_VARLNK:
    sprintf(label,"VARLNK");
    break;
  default:
    assert(0);
    break;
  }

/*   fprintf(f,"  cell%p [label=\"{{<top>%p\\n%s}|{<left>|<right>}}\"",c,c,label); */
/*   fprintf(f,"  cell%p [label=\"{{<top>%d\\n%s}|{<left>|<right>}}\"",c,c->id,label); */
/*   fprintf(f,"  cell%p [label=\"{{<top>%s}|{<left>|<right>}}\"",c,label); */
  fprintf(f,"  cell%p [label=\"%s\"",c,label);
  if (c->tag & FLAG_STRICT)
    fprintf(f,",fontcolor=\"#FF0000\",color=\"#FF0000\"");
  fprintf(f,"];\n");

  pop();
}

void print_graphdot(char *filename, cell *root)
{
  FILE *f = fopen(filename,"w");
  if (NULL == f) {
    perror(filename);
    exit(1);
  }
  fprintf(f,"digraph {\n");
/*   fprintf(f,"  node [shape=record,color=\"#E0E0E0\",fontname=courier,fontsize=8];\n"); */
  fprintf(f,"  node [shape=box,color=\"#FFFFFF\",fontname=helvetica,fontsize=24];\n");
/*   fprintf(f,"  ranksep=0.1;\n"); */
/*   fprintf(f,"  edge [arrowsize=0.6];\n"); */
  clearflag(FLAG_PRINTED);
  print_dot(f,root);
  fprintf(f,"}\n");
  fclose(f);
}

void print_code_indent(FILE *f, int indent)
{
  int i;
  for (i = 0; i < indent; i++)
    fprintf(f,"  ");
}

void print_code1(FILE *f, cell *c, int needbr, int inlist, int indent, cell *parent)
{
  c = resolve_ind(c);
/*   push(c); */
  if (1 && (c->tag & FLAG_PRINTED) &&
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
    c->tag |= FLAG_PRINTED;

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
      fprintf(f,"!%s.",(char*)c->field1);
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
    case TYPE_SYMBOL:
      if ((TYPE_SYMBOL == celltype(c)) && !strncmp((char*)c->field1,"__code",6)) {
        int fno = atoi((char*)c->field1+6);
        assert(0 <= fno);
        assert(fno < NUM_BUILTINS+nscombs);
/*         fprintf(f,"%s [",(char*)c->fiedl1); */
        if (NUM_BUILTINS > fno)
          fprintf(f,"%s",builtin_info[fno].name);
        else
          fprintf(f,"%s",get_scomb_index(fno-NUM_BUILTINS)->name);
/*         fprintf(f,"]"); */
      }
      else {
        fprintf(f,"%s",(char*)c->field1);
      }
      break;
    case TYPE_SCREF:
      fprintf(f,"%s",((scomb*)c->field1)->name);
      break;
    case TYPE_LETREC: {
      cell *lnk = (cell*)c->field1;
      if (parent && (TYPE_LAMBDA != celltype(parent)) && (TYPE_LETREC != celltype(parent)))
        fprintf(f,"(");
      fprintf(f,"\n");
      print_code_indent(f,indent);
      fprintf(f,"letrec ");
      while (lnk) {
        cell *def = (cell*)lnk->field1;
        char *name = (char*)def->field1;
        cell *expr = (cell*)def->field2;

        fprintf(f,"\n");
        print_code_indent(f,indent+1);
        fprintf(f,"%s (",name);
        print_code1(f,expr,0,0,indent+2,c);

        lnk = (cell*)lnk->field2;
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
    case TYPE_FUNCTION:
      if (NULL != cur_program) {
        ginstr *instr = &cur_program->ginstrs[(int)c->field2];
        assert(OP_GLOBSTART == instr->opcode);
        if (NUM_BUILTINS > instr->arg0)
          fprintf(f,"[%s]",builtin_info[instr->arg0].name);
        else
          fprintf(f,"[%s]",get_scomb_index(instr->arg0-NUM_BUILTINS)->name);
      }
      else {
        fprintf(f,"[fun %d/%d]",(int)c->field1,(int)c->field2);
      }
      break;
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
    }
  }
/*   pop(); */
}

void print_codef(FILE *f, cell *c)
{
  clearprinted();
  print_code1(f,c,0,0,0,NULL);
}
void print_code(cell *c)
{
  print_codef(stdout,c);
}

void print_scomb_code(scomb *sc)
{
  int i;
  debug(0,"%s ",sc->name);
  for (i = 0; i < sc->nargs; i++) {
    if (sc->strictin) {
      if (sc->strictin[i])
        debug(0,"!");
    }
    debug(0,"%s ",sc->argnames[i]);
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
/*     debug(0,"Cells:"); */
/*     for (i = 0; i < sc->ncells; i++) */
/*       debug(0," #%05d",sc->cells[i]->id); */
/*     debug(0,"\n"); */
    print(sc->body);
    debug(0,"\n");
  }
  debug(0,"\n");
}

