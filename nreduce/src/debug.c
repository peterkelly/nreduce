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

const char *snode_types[NUM_CELLTYPES] = {
"EMPTY",
"APPLICATION",
"LAMBDA",
"BUILTIN",
"CONS",
"SYMBOL",
"LETREC",
"REMOTEREF",
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

const char *cell_types[NUM_CELLTYPES] = {
"EMPTY",
"APPLICATION",
"LAMBDA",
"BUILTIN",
"CONS",
"SYMBOL",
"LETREC",
"REMOTEREF",
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

const char *msg_names[MSG_COUNT] = {
"ISTATS",
"ALLSTATS",
"DUMP_INFO",
"DONE",
"PAUSE",
"RESUME",
"FISH",
"FETCH",
"TRANSFER",
"ACK",
"MARKROOTS",
"MARKENTRY",
"SWEEP",
"SWEEPACK",
"UPDATE",
"RESPOND",
"SCHEDULE",
"UPDATEREF",
"TEST",
"STARTDISTGC",
};

const char *frame_states[4] = {
"NEW",
"SPARKED",
"RUNNING",
"DONE",
};

void fatal(const char *msg)
{
  fprintf(stderr,"%s\n",msg);
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
  if (!trace)
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
  while (TYPE_APPLICATION == snodetype(c)) {
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
    assert(TYPE_APPLICATION == snodetype(c));
    c = c->left;
    argno--;
  }
  return c->right;
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
  unsigned char hi = (unsigned char)((val & 0xF0) >> 4);
  unsigned char lo = (unsigned char)(val & 0x0F);
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

static void print1(char *prefix, snode *c, int indent)
{
  int i;

  printf("%s%p    %11s ",prefix,c,snode_types[snodetype(c)]);
  for (i = 0; i < indent; i++)
    printf("  ");

  if (c->tag & FLAG_TMP) {
    printf("%p\n",c);
  }
  else {
    c->tag |= FLAG_TMP;
    switch (snodetype(c)) {
    case TYPE_IND:
      abort();
      break;
    case TYPE_EMPTY:
      printf("empty\n");
      break;
    case TYPE_APPLICATION:
      if (c->tag & FLAG_STRICT)
        printf("@S\n");
      else
        printf("@\n");
      print1(prefix,c->left,indent+1);
      print1(prefix,c->right,indent+1);
      break;
    case TYPE_LAMBDA:
      printf("lambda %s\n",c->name);
      print1(prefix,c->body,indent+1);
      break;
    case TYPE_BUILTIN:
      printf("%s\n",builtin_info[c->bif].name);
      break;
    case TYPE_CONS:
      abort();
      break;
    case TYPE_SYMBOL:
      printf("%s\n",c->name);
      break;
    case TYPE_SCREF:
      printf("%s\n",c->sc->name);
      break;
    case TYPE_LETREC: {
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
    case TYPE_HOLE:
      printf("(hole)\n"); break;
    case TYPE_NIL:
      printf("nil\n"); break;
    case TYPE_NUMBER:
      print_double(stdout,c->num);
      printf("\n");
      break;
    case TYPE_STRING:
      printf("%s\n",c->value);
      break;
    default:
      printf("**** INVALID\n");
      abort();
      break;
    }
  }
}

void print(snode *c)
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
    assert(varno < array_count(oldnames));
    return array_item(oldnames,varno,char*);
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
    int oldlen;
    int symlen;
    assert(end);
    assert(oldnames);
    assert(varno < array_count(oldnames));
    old = array_item(oldnames,varno,char*);
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

static void print_code1(FILE *f, snode *c, int needbr, snode *parent, int *line, int *col)
{
  switch (snodetype(c)) {
  case TYPE_IND:
    abort();
    break;
  case TYPE_EMPTY:
    *col += fprintf(f,"empty");
    break;
  case TYPE_APPLICATION: {
    snode *tmp;
    list *l;
    int addedline = 0;
    int argscol;
    int argno;
    int isif;
    list *apps = NULL;
    if (needbr)
      *col += fprintf(f,"(");
    for (tmp = c; TYPE_APPLICATION == snodetype(tmp); tmp = tmp->left)
      list_push(&apps,tmp);

    print_code1(f,tmp,0,c,line,col);
    *col += fprintf(f," ");
    argscol = *col;
    argno = 0;
    isif = (((TYPE_BUILTIN == snodetype(tmp)) && (B_IF == tmp->bif)) ||
            ((TYPE_SYMBOL == snodetype(tmp)) && !strcmp(tmp->name,"if")));
    for (l = apps; l; l = l->next) {
      snode *app = (snode*)l->data;
      snode *arg = app->right;
      int oldcol;
      int oldline;

      if (!addedline && (0 < argno) && (isif || (TYPE_LAMBDA == snodetype(arg)))) {
        *col = argscol;
        down_line(f,line,col);
      }
      addedline = 0;

      oldcol = *col;
      oldline = *line;
/*       if (app->tag & FLAG_STRICT) */
/*         *col += fprintf(f,"!"); */
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
    if (parent && (TYPE_LAMBDA != snodetype(parent)) && (TYPE_LETREC != snodetype(parent)))
      *col += fprintf(f,"(");
    *col += fprintf(f,"!%s.",real_varname(c->name));
    print_code1(f,c->body,0,c,line,col);
    if (parent && (TYPE_LAMBDA != snodetype(parent)) && (TYPE_LETREC != snodetype(parent)))
      *col += fprintf(f,")");
    break;
  case TYPE_BUILTIN:
    *col += fprintf(f,"%s",builtin_info[c->bif].name);
    break;
  case TYPE_CONS: {
    int pos = 0;
    snode *item;
    *col += fprintf(f,"[");
    for (item = c; TYPE_CONS == snodetype(item); item = item->right) {
      if (0 < pos++)
        *col += fprintf(f,",");
      print_code1(f,item->left,1,c,line,col);
    }
    *col += fprintf(f,"]");
    if (TYPE_NIL != snodetype(item))
      print_code1(f,item,1,c,line,col);
    break;
  }
  case TYPE_SYMBOL: {
    char *sym = c->name;
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
    char *scname = real_scname(c->sc->name);
    *col += fprintf(f,"%s",scname);
    free(scname);
    break;
  }
  case TYPE_LETREC: {
    int count = 0;
    letrec *rec = c->bindings;
    if (parent && (TYPE_LAMBDA != snodetype(parent)) && (TYPE_LETREC != snodetype(parent)))
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
    print_code1(f,c->body,1,c,line,col);
    if (parent && (TYPE_LAMBDA != snodetype(parent)) && (TYPE_LETREC != snodetype(parent)))
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
    print_double(f,c->num);
    break;
  case TYPE_STRING: {
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
  case TYPE_AREF:
    abort();
    break;
  case TYPE_ARRAY:
    abort();
    break;
  case TYPE_FRAME:
    abort();
    break;
  case TYPE_CAP:
    abort();
    break;
  default:
    abort();
    break;
  }
}

void print_codef2(FILE *f, snode *c, int pos)
{
  int line = 0;
  int col = pos;
  cleargraph(c,FLAG_TMP);
  print_code1(f,c,0,NULL,&line,&col);
}

void print_codef(FILE *f, snode *c)
{
  int line = 0;
  int col = 0;
  cleargraph(c,FLAG_TMP);
  print_code1(f,c,0,NULL,&line,&col);
}
void print_code(snode *c)
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

void print_scombs1(void)
{
  scomb *sc;
  for (sc = scombs; sc; sc = sc->next) {
    if (strncmp(sc->name,"__",2)) {
      print_scomb_code(sc);
      debug(0,"\n");
    }
  }
}

void print_scombs2(void)
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

void print_stack(FILE *f, pntr *stk, int size, int dir)
{
  int i;

  if (dir)
    i = size-1;
  else
    i = 0;


  while (1) {
    pntr p;
    int pos = dir ? (size-1-i) : i;

    if (dir && i < 0)
      break;

    if (!dir && (i >= size))
      break;

    p = resolve_pntr(stk[i]);
    if (TYPE_IND == pntrtype(stk[i]))
      fprintf(f,"%2d: [i] %12s ",pos,snode_types[pntrtype(p)]);
    else
      fprintf(f,"%2d:     %12s ",pos,snode_types[pntrtype(p)]);
    print_pntr(f,p);
    fprintf(f,"\n");

    if (dir)
      i--;
    else
      i++;
  }
}

void statistics(process *proc, FILE *f)
{
  int i;
  int total;
  fprintf(f,"totalallocs = %d\n",proc->stats.totalallocs);
  fprintf(f,"ncollections = %d\n",proc->stats.ncollections);
  fprintf(f,"nscombappls = %d\n",proc->stats.nscombappls);
  fprintf(f,"nreductions = %d\n",proc->stats.nreductions);
  fprintf(f,"nsparks = %d\n",proc->stats.nsparks);

  total = 0;
  for (i = 0; i < OP_COUNT; i++) {
    fprintf(f,"usage(%s) = %d\n",op_names[i],proc->stats.op_usage[i]);
    total += proc->stats.op_usage[i];
  }
  fprintf(f,"usage total = %d\n",total);
}

void print_pntr_tree(FILE *f, pntr p, int indent)
{
  cell *c;
  int i;
  short oldtype;
  int type = pntrtype(p);

  if (TYPE_EMPTY == type)
    return; /* cycle */

  if (TYPE_NUMBER == type)
    fprintf(f,"%-12s","(number)");
  else
    fprintf(f,"%-12p",get_pntr(p));

  for (i = 0; i < indent; i++) {
    fprintf(f,"  ");
  }
  fprintf(f,"%-12s",cell_types[type]);

  if (TYPE_NUMBER == type) {
    assert(!is_pntr(p));
    print_double(f,p);
    fprintf(f,"\n");
    return;
  }

  assert(is_pntr(p));
  c = get_pntr(p);

  oldtype = c->type;
  c->type = TYPE_EMPTY; /* to avoid loops */
  switch (type) {
  case TYPE_CONS:
    fprintf(f,"(cons)\n");
    print_pntr_tree(f,c->field1,indent+1);
    print_pntr_tree(f,c->field2,indent+1);
    break;
  case TYPE_REMOTEREF: {
    global *glo = (global*)get_pntr(c->field1);
    fprintf(f,"%d@%d\n",glo->addr.lid,glo->addr.pid);
    break;
  }
  case TYPE_IND:
    fprintf(f,"(ind)\n");
    print_pntr_tree(f,c->field1,indent+1);
    break;
  case TYPE_AREF: {
    cell *arrcell = get_pntr(c->field1);
    carray *arr = (carray*)get_pntr(arrcell->field1);
    int index = (int)get_pntr(c->field2);
    fprintf(f,"(aref %d/%d)\n",index,arr->size);
    break;
  }
  case TYPE_HOLE:
    fprintf(f,"(hole)\n");
    break;
  case TYPE_FRAME: {
    frame *fr = (frame*)get_pntr(c->field1);
    char *name = get_function_name(fr->fno);
    fprintf(f,"%s (%d)\n",name,fr->count);
    free(name);
    for (i = 0; i < fr->count; i++)
      print_pntr_tree(f,fr->data[i],indent+1);
    break;
  }
  case TYPE_CAP: {
    cap *cp = (cap*)get_pntr(c->field1);
    char *name = get_function_name(cp->fno);
    fprintf(f,"%s (%d)\n",name,cp->count);
    free(name);
    for (i = 0; i < cp->count; i++)
      print_pntr_tree(f,cp->data[i],indent+1);
    break;
  }
  case TYPE_NIL:
    fprintf(f,"(nil)\n");
    break;
  case TYPE_STRING:
    fprintf(f,"\"%s\"\n",get_string(c->field1));
    break;
  default:
    abort();
  }
  c->type = oldtype;
}

void dump_info(process *proc)
{
  block *bl;
  int i;
  frame *f;

  fprintf(proc->output,"Frames with things waiting on them:\n");
  fprintf(proc->output,"%-12s %-20s %-12s %-12s\n","frame*","function","frames","fetchers");
  fprintf(proc->output,"%-12s %-20s %-12s %-12s\n","------","--------","------","--------");

  for (bl = proc->blocks; bl; bl = bl->next) {
    for (i = 0; i < BLOCK_SIZE; i++) {
      cell *c = &bl->values[i];
      if (TYPE_FRAME == c->type) {
        f = (frame*)get_pntr(c->field1);
        if (f->wq.frames || f->wq.fetchers) {
          const char *fname = function_name(proc->gp,f->fno);
          int nframes = list_count(f->wq.frames);
          int nfetchers = list_count(f->wq.fetchers);
          fprintf(proc->output,"%-12p %-20s %-12d %-12d\n",
                  f,fname,nframes,nfetchers);
        }
      }
    }
  }

  fprintf(proc->output,"\n");
  fprintf(proc->output,"Runnable queue (size %d):\n",proc->runnable.size);
  fprintf(proc->output,"%-12s %-20s %-12s %-12s %-16s\n",
          "frame*","function","frames","fetchers","state");
  fprintf(proc->output,"%-12s %-20s %-12s %-12s %-16s\n",
          "------","--------","------","--------","-------------");
  for (f = proc->runnable.first; f; f = f->qnext) {
          const char *fname = function_name(proc->gp,f->fno);
          int nframes = list_count(f->wq.frames);
          int nfetchers = list_count(f->wq.fetchers);
    fprintf(proc->output,"%-12p %-20s %-12d %-12d %-16s\n",
            f,fname,nframes,nfetchers,frame_states[f->state]);
  }

}

void dump_globals(process *proc)
{
  int h;
  global *glo;

  fprintf(proc->output,"\n");
  fprintf(proc->output,"%-9s %-12s %-12s\n","Address","Type","Cell");
  fprintf(proc->output,"%-9s %-12s %-12s\n","-------","----","----");
  for (h = 0; h < GLOBAL_HASH_SIZE; h++) {
    for (glo = proc->pntrhash[h]; glo; glo = glo->pntrnext) {
      fprintf(proc->output,"%4d@%-4d %-12s %-12p\n",
              glo->addr.lid,glo->addr.pid,cell_types[pntrtype(glo->p)],
              is_pntr(glo->p) ? get_pntr(glo->p) : NULL);
    }
  }
}
