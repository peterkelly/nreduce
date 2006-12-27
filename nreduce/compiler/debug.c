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

#define DEBUG_C

#include "compiler/bytecode.h"
#include "src/nreduce.h"
#include "compiler/source.h"
#include "runtime/runtime.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <assert.h>
#include <ctype.h>
#include <stdarg.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <errno.h>

int compileinfo = 0;

void fatal(const char *format, ...)
{
  va_list ap;
  va_start(ap,format);
  vfprintf(stderr,format,ap);
  va_end(ap);
  fprintf(stderr,"\n");
  abort();
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
  if (!compileinfo)
    return;
  for (i = 0; i < npad/2; i++)
    printf("=");
  printf(" %s ",name);
  for (; i < npad; i++)
    printf("=");
  printf("\n");
}

int count_args(snode *c)
{
  int napps = 0;
  while (SNODE_APPLICATION == c->type) {
    c = c->left;
    napps++;
  }
  return napps;
}

snode *get_arg(snode *c, int argno)
{
  argno = count_args(c)-argno-1;
  assert(0 <= argno);
  while (0 < argno) {
    assert(SNODE_APPLICATION == c->type);
    c = c->left;
    argno--;
  }
  return c->right;
}

static void print1(char *prefix, snode *c, int indent)
{
  int i;

  printf("%s%p    %11s ",prefix,c,snode_types[c->type]);
  for (i = 0; i < indent; i++)
    printf("  ");

  switch (c->type) {
  case SNODE_EMPTY:
    printf("empty\n");
    break;
  case SNODE_APPLICATION:
    if (c->strict)
      printf("@S\n");
    else
      printf("@\n");
    print1(prefix,c->left,indent+1);
    print1(prefix,c->right,indent+1);
    break;
  case SNODE_LAMBDA:
    printf("lambda %s\n",c->name);
    print1(prefix,c->body,indent+1);
    break;
  case SNODE_BUILTIN:
    printf("%s\n",builtin_info[c->bif].name);
    break;
  case SNODE_SYMBOL:
    printf("%s\n",c->name);
    break;
  case SNODE_SCREF:
    printf("%s\n",c->sc->name);
    break;
  case SNODE_LETREC: {
    letrec *rec;
    printf("LETREC\n");
    for (rec = c->bindings; rec; rec = rec->next) {
      printf("%s             %11s ",prefix,"def");
      for (i = 0; i < indent+1; i++)
        printf("  ");
      if (rec->strict)
        printf("!%s\n",rec->name);
      else
        printf(" %s\n",rec->name);
      print1(prefix,rec->value,indent+2);
    }
    print1(prefix,c->body,indent+1);
    break;
  }
  case SNODE_NIL:
    printf("nil\n"); break;
  case SNODE_NUMBER:
    print_double(stdout,c->num);
    printf("\n");
    break;
  case SNODE_STRING:
    printf("%s\n",c->value);
    break;
  default:
    printf("**** INVALID\n");
    abort();
    break;
  }
}

void print(snode *c)
{
  print1("",c,0);
}

static void down_line(FILE *f, int *line, int col)
{
  int i;
  (*line)++;
  fprintf(f,"\n");
  for (i = 0; i < col; i++)
    fprintf(f," ");
}

const char *real_varname(source *src, const char *sym)
{
#ifdef SHOW_SUBSTITUTED_NAMES
  return sym;
#else
  if (!strncmp(sym,GENVAR_PREFIX,strlen(GENVAR_PREFIX))) {
    int varno = atoi(sym+2);
    assert(src->oldnames);
    assert(varno < array_count(src->oldnames));
    return array_item(src->oldnames,varno,char*);
  }
  else {
    return sym;
  }
#endif
}

char *real_scname(source *src, const char *sym)
{
#ifdef SHOW_SUBSTITUTED_NAMES
  return strdup(sym);
#else
  if (!strncmp(sym,GENVAR_PREFIX,strlen(GENVAR_PREFIX))) {
    char *end = NULL;
    int varno = strtol(sym+2,&end,10);
    char *old;
    char *fullname;
    int oldlen;
    int symlen;
    assert(end);
    assert(src->oldnames);
    assert(varno < array_count(src->oldnames));
    old = array_item(src->oldnames,varno,char*);
    oldlen = strlen(old);
    symlen = strlen(sym);
    fullname = (char*)malloc(oldlen+symlen+1);
    sprintf(fullname,"%s%s",old,end);
    return fullname;
  }
  else {
    return strdup(sym);
  }
#endif
}

static void print_code1(source *src, FILE *f, snode *c, int needbr, snode *parent,
                        int *line, int *col)
{
  switch (c->type) {
  case SNODE_EMPTY:
    *col += fprintf(f,"empty");
    break;
  case SNODE_APPLICATION: {
    snode *tmp;
    list *l;
    int addedline = 0;
    int argscol;
    int argno;
    int isif;
    list *apps = NULL;
    if (needbr)
      *col += fprintf(f,"(");
    for (tmp = c; SNODE_APPLICATION == tmp->type; tmp = tmp->left)
      list_push(&apps,tmp);

    print_code1(src,f,tmp,0,c,line,col);
    *col += fprintf(f," ");
    argscol = *col;
    argno = 0;
    isif = (((SNODE_BUILTIN == tmp->type) && (B_IF == tmp->bif)) ||
            ((SNODE_SYMBOL == tmp->type) && !strcmp(tmp->name,"if")));
    for (l = apps; l; l = l->next) {
      snode *app = (snode*)l->data;
      snode *arg = app->right;
      int oldcol;
      int oldline;

      if (!addedline && (0 < argno) && (isif || (SNODE_LAMBDA == arg->type))) {
        *col = argscol;
        down_line(f,line,*col);
      }
      addedline = 0;

      oldcol = *col;
      oldline = *line;
/*       if (app->strict) */
/*         *col += fprintf(f,"!"); */
      print_code1(src,f,arg,1,app,line,col);
      if (l->next) {
        if (1 && (oldline != *line)) {
          *col = oldcol;
          down_line(f,line,*col);
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
      (*col) += fprintf(f,")");
    break;
  }
  case SNODE_LAMBDA:
    if (parent && (SNODE_LAMBDA != parent->type) && (SNODE_LETREC != parent->type))
      *col += fprintf(f,"(");
    *col += fprintf(f,"!%s.",real_varname(src,c->name));
    print_code1(src,f,c->body,0,c,line,col);
    if (parent && (SNODE_LAMBDA != parent->type) && (SNODE_LETREC != parent->type))
      *col += fprintf(f,")");
    break;
  case SNODE_BUILTIN:
    *col += fprintf(f,"%s",builtin_info[c->bif].name);
    break;
  case SNODE_SYMBOL: {
    char *sym = c->name;
    if (!strncmp(sym,"__code",6)) {
      int fno = atoi(sym+6);
      assert(0 <= fno);
      if (NUM_BUILTINS > fno)
        *col += fprintf(f,"%s",builtin_info[fno].name);
      else
        *col += fprintf(f,"sc%d",fno-NUM_BUILTINS);
    }
    else {
      *col += fprintf(f,"%s",real_varname(src,sym));
    }
    break;
  }
  case SNODE_SCREF: {
    char *scname = real_scname(src,c->sc->name);
    *col += fprintf(f,"%s",scname);
    free(scname);
    break;
  }
  case SNODE_LETREC: {
    int count = 0;
    letrec *rec = c->bindings;
    if (needbr || (parent && (SNODE_LAMBDA != parent->type)))
      *col += fprintf(f,"(");
    fprintf(f,"letrec ");
    (*col) += 2;
    down_line(f,line,*col);
    while (rec) {
      int col2 = *col;
      if (0 < count++)
        down_line(f,line,col2);
      if (rec->strict)
        col2 += fprintf(f,"!%s = ",real_varname(src,rec->name));
      else
        col2 += fprintf(f,"%s = ",real_varname(src,rec->name));
      print_code1(src,f,rec->value,1,c,line,&col2);
      rec = rec->next;
    }
    down_line(f,line,(*col)-2);
    fprintf(f,"in");
    down_line(f,line,*col);
    print_code1(src,f,c->body,1,c,line,col);
    if (needbr || (parent && (SNODE_LAMBDA != parent->type)))
      *col += fprintf(f,")");
/*     (*col) -= 3; */
    break;
  }
  case SNODE_NIL:
    fprintf(f,"nil");
    break;
  case SNODE_NUMBER:
    print_double(f,c->num);
    break;
  case SNODE_STRING: {
    char *ch;
    fprintf(f,"\"");
    for (ch = c->value; *ch; ch++) {
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
  case SNODE_WRAP:
    print_code1(src,f,c->target,needbr,parent,line,col);
    break;
  default:
    abort();
    break;
  }
}

void print_codef2(source *src, FILE *f, snode *c, int pos)
{
  int line = 0;
  int col = pos;
  print_code1(src,f,c,1,NULL,&line,&col);
}

void print_codef(source *src, FILE *f, snode *c, int needbr)
{
  int line = 0;
  int col = 0;
  print_code1(src,f,c,needbr,NULL,&line,&col);
}
void print_code(source *src, snode *c)
{
  print_codef(src,stdout,c,1);
}

void print_scomb_code(source *src, FILE *f, scomb *sc)
{
  int i;
  int col = 0;
  char *scname = real_scname(src,sc->name);
  col += fprintf(f,"%s ",scname);
  free(scname);
  for (i = 0; i < sc->nargs; i++) {
    if (sc->strictin) {
      if (sc->strictin[i])
        col += fprintf(f,"!");
    }
    col += fprintf(f,"%s ",real_varname(src,sc->argnames[i]));
  }
  col += fprintf(f,"= ");
  print_codef2(src,f,sc->body,col);
}

void print_scombs1(source *src)
{
  int scno;
  int count = array_count(src->scombs);
  for (scno = 0; scno < count; scno++) {
    scomb *sc = array_item(src->scombs,scno,scomb*);
    if (is_from_prelude(src,sc))
      continue;
    print_scomb_code(src,stdout,sc);
    debug(0,"\n");
  }
}

void print_scombs2(source *src)
{
  int scno;
  int count = array_count(src->scombs);
  debug(0,"\n--------------------\n\n");
  for (scno = 0; scno < count; scno++) {
    scomb *sc = array_item(src->scombs,scno,scomb*);
    int i;
    debug(0,"%s ",sc->name);
    for (i = 0; i < sc->nargs; i++)
      debug(0,"%s ",sc->argnames[i]);
    debug(0,"= ");
    print_code(src,sc->body);
    debug(0,"\n");
    print(sc->body);
    debug(0,"\n");
  }
  debug(0,"\n");
}

