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

#include "grammar.tab.h"
#include "gcode.h"
#include "nreduce.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <assert.h>
#include <ctype.h>
#include <stdarg.h>
#include <math.h>

#define GCODE_C

#define MKCAP(_s,_a,_b)       add_instruction(gp,_s,OP_MKCAP,(_a),(_b))
#define MKFRAME(_s,_a,_b)     add_instruction(gp,_s,OP_MKFRAME,(_a),(_b))
#define JFUN(_s,_a)           add_instruction(gp,_s,OP_JFUN,(_a),0)
#define UPDATE(_s,_a)         add_instruction(gp,_s,OP_UPDATE,(_a),0)
#define DO(_s,_a)             add_instruction(gp,_s,OP_DO,_a,0)
#define EVAL(_s,_a)           { int arg0 = (_a);                \
                                if (!gp->si || !statusat(gp->si,gp->si->count-1-arg0)) { \
                                   add_instruction(gp,_s,OP_EVAL,arg0,0); \
                                   add_instruction(gp,_s,OP_RESOLVE,arg0,0); \
                                }}
#define RESOLVE(_s,_a)        add_instruction(gp,_s,OP_RESOLVE,(_a),0)
#define RETURN(_s)            add_instruction(gp,_s,OP_RETURN,0,0)
#define PUSH(_s,_a)           add_instruction(gp,_s,OP_PUSH,(_a),0)
#define LAST(_s)              add_instruction(gp,_s,OP_LAST,0,0)
#define BIF(_s,_a)            add_instruction(gp,_s,OP_BIF,(_a),0)
#define JUMPrel(_s,_a)        add_instruction(gp,_s,OP_JUMP,(_a),0)
#define GLOBSTART(_s,_a,_b)   add_instruction(gp,_s,OP_GLOBSTART,(_a),(_b))
#define END(_s)               add_instruction(gp,_s,OP_END,0,0)
#define BEGIN(_s)             add_instruction(gp,_s,OP_BEGIN,0,0)
#define CALL(_s,_a)           add_instruction(gp,_s,OP_CALL,(_a),0)
#define ALLOC(_s,_a)          add_instruction(gp,_s,OP_ALLOC,(_a),0)
#define PUSHNIL(_s)           add_instruction(gp,_s,OP_PUSHNIL,0,0)
#define PUSHNUMBER(_s,_a,_b)  add_instruction(gp,_s,OP_PUSHNUMBER,(_a),(_b))
#define PUSHSTRING(_s,_a)     add_instruction(gp,_s,OP_PUSHSTRING,(_a),0)
#define SQUEEZE(_s,_a,_b)     add_instruction(gp,_s,OP_SQUEEZE,(_a),(_b))

#define JFALSE(_s,addr)       { addr = gp->count; add_instruction(gp,_s,OP_JFALSE,0,0); }
#define JUMP(_s,addr)         { addr = gp->count; add_instruction(gp,_s,OP_JUMP,0,0); }
#define LABEL(addr)           { gp->ginstrs[addr].arg0 = gp->count-addr; }

static stackinfo *stackinfo_new(stackinfo *source)
{
  stackinfo *si = (stackinfo*)calloc(1,sizeof(stackinfo));
  if (NULL != source) {
    si->alloc = source->alloc;
    si->count = source->count;
    si->status = (int*)malloc(si->alloc*sizeof(int));
    memcpy(si->status,source->status,si->count*sizeof(int));
  }
  return si;
}

static void stackinfo_free(stackinfo *si)
{
  free(si->status);
  free(si);
}

static int statusat(stackinfo *si, int index)
{
  assert(index < si->count);
  assert(0 <= index);
  return si->status[index];
}

static void setstatusat(stackinfo *si, int index, int i)
{
  assert(index < si->count);
  assert(0 <= index);
  si->status[index] = i;
}

static void pushstatus(stackinfo *si, int i)
{
  if (si->alloc == si->count) {
    if (0 == si->alloc)
      si->alloc = 16;
    else
      si->alloc *= 2;
    si->status = (int*)realloc(si->status,si->alloc*sizeof(snode*));
  }
  si->status[si->count++] = i;
}

static void popstatus(stackinfo *si, int n)
{
  si->count -= n;
  assert(0 <= si->count);
}

static void stackinfo_newswap(stackinfo **si, stackinfo **tmp)
{
  *tmp = *si;
  *si = stackinfo_new(*si);
}

static void stackinfo_freeswap(stackinfo **si, stackinfo **tmp)
{
  stackinfo_free(*si);
  *si = *tmp;
}

const char *op_names[OP_COUNT] = {
"BEGIN",
"END",
"LAST",
"GLOBSTART",
"EVAL",
"RETURN",
"DO",
"JFUN",
"JFALSE",
"JUMP",
"PUSH",
"UPDATE",
"ALLOC",
"SQUEEZE",
"MKCAP",
"MKFRAME",
"BIF",
"PUSHNIL",
"PUSHNUMBER",
"PUSHSTRING",
"RESOLVE",
};

static void print_comp2(gprogram *gp, char *fname, snode *c, int n, const char *format, ...)
{
#ifdef DEBUG_GCODE_COMPILATION
  va_list ap;
  debug(gp->cdepth,"%s [ ",fname);
  print_code(c);
  printf(" ]");
  printf(" %d",n);
  va_start(ap,format);
  vprintf(format,ap);
  va_end(ap);
  printf("\n");
#endif
}

static void print_comp2_stack(gprogram *gp, char *fname, stack *exprs, int n,
                              const char *format, ...)
{
#ifdef DEBUG_GCODE_COMPILATION
  int i;
  va_list ap;
  debug(gp->cdepth,"%s [ ",fname);
  for (i = exprs->count-1; 0 <= i; i--) {
    if (i < exprs->count-1)
      printf(", ");
    print_code((snode*)exprs->data[i]);
  }
  printf(" ]");
  printf(" %d",n);
  va_start(ap,format);
  vprintf(format,ap);
  va_end(ap);
  printf("\n");
#endif
}

gprogram *gprogram_new(void)
{
  gprogram *gp = (gprogram*)calloc(1,sizeof(gprogram));
  gp->count = 0;
  gp->alloc = 2;
  gp->ginstrs = (ginstr*)malloc(gp->alloc*sizeof(ginstr));
  gp->si = NULL;
  gp->stringmap = array_new();
  gp->cdepth = -1;
  return gp;
}

void gprogram_free(gprogram *gp)
{
  int i;
  for (i = 0; i < gp->count; i++) {
    if (OP_PUSH == gp->ginstrs[i].opcode)
      free((char*)gp->ginstrs[i].arg1);
    free(gp->ginstrs[i].expstatus);
  }

  for (i = 0; i < (int)(gp->stringmap->size/sizeof(char*)); i++)
    free(((char**)gp->stringmap->data)[i]);
  array_free(gp->stringmap);

  for (i = 0; i < gp->nfunctions; i++)
    free(gp->fnames[i]);
  free(gp->fnames);

  free(gp->addressmap);
  free(gp->noevaladdressmap);
  free(gp->ginstrs);
  free(gp->stacksizes);
  free(gp);
}

static int add_string(gprogram *gp, const char *str)
{
  int pos = gp->stringmap->size/sizeof(char*);
  char *copy = strdup(str);
  array_append(gp->stringmap,&copy,sizeof(cell*));
  return pos;
}

static void add_instruction(gprogram *gp, sourceloc sl, int opcode, int arg0, int arg1)
{
  ginstr *instr;
  assert(!gp->si || !gp->si->invalid);

  if (gp->alloc <= gp->count) {
    gp->alloc *= 2;
    gp->ginstrs = (ginstr*)realloc(gp->ginstrs,gp->alloc*sizeof(ginstr));
  }
  instr = &gp->ginstrs[gp->count++];

  memset(instr,0,sizeof(ginstr));
  instr->opcode = opcode;
  instr->arg0 = arg0;
  instr->arg1 = arg1;
  instr->sl = sl;
  if (gp->si) {
    instr->expcount = gp->si->count;
    instr->expstatus = (int*)malloc(gp->si->count*sizeof(int));
    memcpy(instr->expstatus,gp->si->status,gp->si->count*sizeof(int));
  }
  else {
    instr->expcount = -1;
  }

  #ifdef DEBUG_GCODE_COMPILATION
  if (gp->si && (0 <= gp->cdepth)) {
    int i;
    for (i = 0; i < gp->cdepth; i++)
      printf("  ");
    printf("==> ");
    for (i = 0; i < 6-gp->cdepth; i++)
      printf("  ");
    printf("%d ",gp->si->count);
/*     printf("stackinfo"); */
/*     int i; */
/*     for (i = 0; i < gp->si->count; i++) { */
/*       printf(" %d",gp->si->status[i]); */
/*     } */
/*     printf("\n"); */
  }
  print_ginstr(stdout,gp,gp->count-1,instr);
  #endif

  if (!gp->si)
    return;

  switch (opcode) {
  case OP_BEGIN:
    break;
  case OP_END:
    break;
  case OP_LAST:
    break;
  case OP_GLOBSTART:
    break;
  case OP_EVAL:
    assert(0 <= gp->si->count-1-arg0);
    setstatusat(gp->si,gp->si->count-1-arg0,1);
    break;
  case OP_RETURN:
    // After RETURN we don't know what the stack size will be... but since it's always the
    // last instruction this is ok. Mark the stackinfo object as invalid to indicate the
    // information in it can't be relied upon anymore.
    gp->si->invalid = 1;
    break;
  case OP_DO:
    gp->si->invalid = 1;
    break;
  case OP_JFUN:
    gp->si->invalid = 1;
    break;
  case OP_JFALSE:
    popstatus(gp->si,1);
    break;
  case OP_JUMP:
    break;
  case OP_PUSH:
    assert(0 <= gp->si->count-1-arg0);
    pushstatus(gp->si,statusat(gp->si,gp->si->count-1-arg0));
    break;
  case OP_UPDATE:
    assert(0 <= gp->si->count-1-arg0);
    setstatusat(gp->si,gp->si->count-1-arg0,statusat(gp->si,gp->si->count-1));
    popstatus(gp->si,1);
    break;
  case OP_ALLOC: {
    int i;
    for (i = 0; i < arg0; i++)
      pushstatus(gp->si,1);
    break;
  }
  case OP_SQUEEZE:
    assert(0 <= gp->si->count-arg0-arg1);
    memmove(&gp->si->status[gp->si->count-arg0-arg1],
            &gp->si->status[gp->si->count-arg0],
            arg0*sizeof(int));
    gp->si->count -= arg1;
    break;
  case OP_MKCAP:
    assert(0 <= gp->si->count-arg1);
    popstatus(gp->si,arg1);
    pushstatus(gp->si,0);
    break;
  case OP_MKFRAME:
    assert(0 <= gp->si->count-arg1);
    popstatus(gp->si,arg1);
    pushstatus(gp->si,0);
    break;
  case OP_BIF:
    popstatus(gp->si,builtin_info[arg0].nargs-1);
    setstatusat(gp->si,gp->si->count-1,builtin_info[arg0].reswhnf);
    break;
  case OP_PUSHNIL:
    pushstatus(gp->si,1);
    break;
  case OP_PUSHNUMBER:
    pushstatus(gp->si,1);
    break;
  case OP_PUSHSTRING:
    pushstatus(gp->si,1);
    break;
  case OP_RESOLVE:
    assert(statusat(gp->si,gp->si->count-1-arg0));
    break;
  default:
    abort();
    break;
  }
}

static int snode_fno(snode *c)
{
  assert((TYPE_BUILTIN == snodetype(c)) ||
         (TYPE_SCREF == snodetype(c)));
  if (TYPE_BUILTIN == snodetype(c))
    return c->bif;
  else if (TYPE_SCREF == snodetype(c))
    return c->sc->index+NUM_BUILTINS;
  else
    return -1;
}

char *get_function_name(int fno)
{
  if (0 > fno)
    return strdup("(unknown)");
  else if (NUM_BUILTINS > fno)
    return strdup(builtin_info[fno].name);
  else
    return real_scname(get_scomb_index(fno-NUM_BUILTINS)->name);
}

const char *function_name(gprogram *gp, int fno)
{
  if (0 > fno)
    return "(unknown)";
  else if (fno >= gp->nfunctions)
    return "(invalid)";
  else
    return gp->fnames[fno];
}

int function_nargs(int fno)
{
  assert(0 <= fno);
  if (NUM_BUILTINS > fno)
    return builtin_info[fno].nargs;
  else
    return get_scomb_index(fno-NUM_BUILTINS)->nargs;
}

void print_ginstr(FILE *f, gprogram *gp, int address, ginstr *instr)
{
  assert(OP_COUNT > instr->opcode);

  fprintf(f,"%-5d %-12s %-10d %-10d",
         address,op_names[instr->opcode],instr->arg0,instr->arg1);
  if (0 <= instr->expcount)
    fprintf(f," %-4d",instr->expcount);
  else
    fprintf(f,"       ");
  fprintf(f,"    ");

  if (0 <= instr->sl.fileno)
    fprintf(f,"%20s:%-4d ",lookup_parsedfile(instr->sl.fileno),instr->sl.lineno);
  else
    fprintf(f,"%20s%-5s ","","");

  switch (instr->opcode) {
  case OP_GLOBSTART:
    if (NUM_BUILTINS > instr->arg0) {
      fprintf(f,"; Builtin %s",builtin_info[instr->arg0].name);
    }
    else {
      char *name = real_scname(get_scomb_index(instr->arg0-NUM_BUILTINS)->name);
      fprintf(f,"; Supercombinator %s",name);
      free(name);
    }
    break;
  case OP_PUSH:
    if (0 <= instr->expcount) {
      ginstr *program = instr-address;
      int funstart = address;
      while ((0 <= funstart) && (OP_GLOBSTART != program[funstart].opcode))
        funstart--;
      if ((OP_GLOBSTART == program[funstart].opcode) &&
          (NUM_BUILTINS <= program[funstart].arg0)) {
        scomb *sc = get_scomb_index(program[funstart].arg0-NUM_BUILTINS);
        int stackpos = instr->expcount-instr->arg0-1;
        if (stackpos < sc->nargs)
          fprintf(f,"; PUSH %s",real_varname(sc->argnames[sc->nargs-1-stackpos]));
      }
    }
    break;
  case OP_PUSHSTRING: {
    char *str = ((char**)gp->stringmap->data)[instr->arg0];
    fprintf(f,"; PUSHSTRING \"%s\"",str);
    break;
  }
  case OP_MKFRAME:
    fprintf(f,"; MKFRAME %s %d",function_name(gp,instr->arg0),instr->arg1);
    break;
  case OP_MKCAP:
    fprintf(f,"; MKCAP %s %d",function_name(gp,instr->arg0),instr->arg1);
    break;
  case OP_JFUN:
    fprintf(f,"; JFUN %s",function_name(gp,instr->arg0));
    break;
  case OP_BIF:
    fprintf(f,"; %s",builtin_info[instr->arg0].name);
    break;
  default:
    break;
  }

  fprintf(f,"\n");
}

void print_program(gprogram *gp, int builtins)
{
  int i;
  int prevfun = -1;
  for (i = 0; i < gp->count; i++) {
    if ((OP_GLOBSTART == gp->ginstrs[i].opcode) &&
        (prevfun != gp->ginstrs[i].arg0)) {
      if (NUM_BUILTINS <= gp->ginstrs[i].arg0) {
        scomb *sc = get_scomb_index(gp->ginstrs[i].arg0-NUM_BUILTINS);
        printf("\n");
        print_scomb_code(sc);
        printf("\n");
      }
      else if (!builtins) {
        break;
      }
      printf("\n");
      prevfun = gp->ginstrs[i].arg0;
    }
    print_ginstr(stdout,gp,i,&gp->ginstrs[i]);
  }

  printf("\n");
  printf("String map:\n");
  for (i = 0; i < (int)(gp->stringmap->size/sizeof(char*)); i++) {
    char *str = ((char**)gp->stringmap->data)[i];
    printf("%d: ",i);
    print_quoted_string(stdout,str);
    printf("\n");
  }
}

void print_profiling(process *proc, gprogram *gp)
{
  int index = 0;
  scomb *sc;
  int *usage;
  int prevfun = -1;
  int fno = -1;
  int curusage = 0;
  int totalusage = 0;
  int addr = 0;

  print_program(gp,1);

  for (sc = scombs; sc; sc = sc->next)
    sc->index = index++;

  usage = (int*)calloc((index+1)+NUM_BUILTINS,sizeof(int));

  while (addr <= gp->count) {
    if ((addr == gp->count) ||
        ((OP_GLOBSTART == gp->ginstrs[addr].opcode) &&
         (prevfun != gp->ginstrs[addr].arg0))) {
      if (0 <= fno) {
        usage[fno] = curusage;
      }
      else {
        assert(-1 == prevfun);
        usage[NUM_BUILTINS+index] = curusage;
      }
      curusage = 0;
      fno = gp->ginstrs[addr].arg0;
      prevfun = fno;
    }
    if (addr < gp->count) {
      curusage += proc->stats.usage[addr];
      totalusage += proc->stats.usage[addr];
    }
    addr++;
  }

  printf("#calls   #instrs   instr%%\n");
  printf("------   -------   ------\n");
  for (fno = 0; fno <= NUM_BUILTINS+index; fno++) {
    double dusage = (double)usage[fno];
    double dtotalusage = (double)totalusage;
    double portion = (dusage/dtotalusage)*100.0;
    printf("%-9d",proc->stats.funcalls[fno]);
    printf("%-9d",usage[fno]);
    printf(" %-6.2f",portion);
    if (fno == NUM_BUILTINS+index) {
      printf("start code");
    }
    else if (fno < NUM_BUILTINS) {
      printf(" %s",builtin_info[fno].name);
    }
    else {
      char *name = real_scname(get_scomb_index(fno-NUM_BUILTINS)->name);
      printf(" %s",name);
      free(name);
    }
    printf("\n");
  }

  free(usage);
}

typedef struct pmap {
  stack *names;
  stack *indexes;
} pmap;

static int presolve(pmap *pm, char *varname)
{
  int i;
  assert(pm->names->count == pm->indexes->count);
  for (i = 0; i < pm->names->count; i++)
    if (!strcmp((char*)pm->names->data[i],varname))
      return (int)pm->indexes->data[i];
  printf("unknown variable: %s\n",real_varname(varname));
  abort();
  return -1;
}

static void presize(pmap *pm, int count)
{
  assert(pm->names->count == pm->indexes->count);
  assert(count <= pm->names->count);
  pm->names->count = count;
  pm->indexes->count = count;
}

static int pcount(pmap *pm)
{
  assert(pm->names->count == pm->indexes->count);
  return pm->names->count;
}

static void getusage(snode *c, list **used)
{
  switch (snodetype(c)) {
  case TYPE_APPLICATION:
    getusage(c->left,used);
    getusage(c->right,used);
    break;
  case TYPE_LETREC: {
    letrec *rec;
    for (rec = c->bindings; rec; rec = rec->next)
      getusage(rec->value,used);
    getusage(c->body,used);
    break;
  }
  case TYPE_SYMBOL:
    if (!list_contains_string(*used,c->name))
      list_push(used,c->name);
    break;
  case TYPE_BUILTIN:
  case TYPE_SCREF:
  case TYPE_NIL:
  case TYPE_NUMBER:
  case TYPE_STRING:
    break;
  }
}

static int letrecs_used(snode *expr, letrec *first)
{
  int count = 0;
  list *used = NULL;
  letrec *rec;
  getusage(expr,&used);

  for (rec = first; rec; rec = rec->next)
    if (list_contains_string(used,rec->name))
      count++;

  list_free(used,NULL);
  return count;
}

static void C(gprogram *gp, snode *c, pmap *p, int n);
static void E(gprogram *gp, snode *c, pmap *p, int n);
static void Cletrec(gprogram *gp, snode *c, int n, pmap *p, int strictcontext)
{
  letrec *rec;
  int count = 0;
  for (rec = c->bindings; rec; rec = rec->next) {
    count++;
    stack_push(p->names,rec->name);
    stack_push(p->indexes,(void*)(n+count));
  }

  rec = c->bindings;
  for (; rec; rec = rec->next) {
    if (strictcontext && rec->strict && (0 == letrecs_used(rec->value,rec))) {
      E(gp,rec->value,p,n);
      n++;
      count--;
    }
    else {
      break;
    }
  }

  if (0 == count)
    return;

  ALLOC(rec->value->sl,count);
  n += count;

  for (; rec; rec = rec->next) {
    if (strictcontext && rec->strict && (0 == letrecs_used(rec->value,rec)))
      E(gp,rec->value,p,n);
    else
      C(gp,rec->value,p,n);
    UPDATE(rec->value->sl,n+1-presolve(p,rec->name));
  }
}

static void E(gprogram *gp, snode *c, pmap *p, int n)
{
  gp->cdepth++;
  print_comp2(gp,"E",c,n,"");
  switch (snodetype(c)) {
  case TYPE_APPLICATION: {
    int m = 0;
    snode *app;
    int fno;
    int k;
    for (app = c; TYPE_APPLICATION == snodetype(app); app = app->left)
      m++;

    assert(TYPE_SYMBOL != snodetype(app)); /* should be lifted into separate scomb otherwise */
    parse_check((TYPE_BUILTIN == snodetype(app)) || (TYPE_SCREF == snodetype(app)),
                app,CONSTANT_APP_MSG);

    fno = snode_fno(app);
    k = function_nargs(fno);
    assert(m <= k); /* should be lifted into separate supercombinator otherwise */

    if ((TYPE_BUILTIN == snodetype(app)) &&
        (B_IF == app->bif) &&
        (3 == m)) {
      snode *falsebranch;
      snode *truebranch;
      snode *cond;
      int label;
      stackinfo *oldsi;
      int end;
      app = c;
      falsebranch = app->right;
      app = app->left;
      truebranch = app->right;
      app = app->left;
      cond = app->right;

      E(gp,cond,p,n);
      JFALSE(cond->sl,label);

      stackinfo_newswap(&gp->si,&oldsi);
      E(gp,truebranch,p,n);
      JUMP(cond->sl,end);
      stackinfo_freeswap(&gp->si,&oldsi);

      LABEL(label);
      stackinfo_newswap(&gp->si,&oldsi);
      E(gp,falsebranch,p,n);
      stackinfo_freeswap(&gp->si,&oldsi);

      LABEL(end);
      pushstatus(gp->si,1);
    }
    else {
      m = 0;
      for (app = c; TYPE_APPLICATION == snodetype(app); app = app->left) {
        if (app->tag & FLAG_STRICT)
          E(gp,app->right,p,n+m);
        else
          C(gp,app->right,p,n+m);
        m++;
      }

      if (m == k) {

        if (TYPE_BUILTIN == snodetype(app)) {
          BIF(app->sl,fno);
          if (!builtin_info[fno].reswhnf)
            EVAL(app->sl,0);
        }
        else {
          MKFRAME(app->sl,fno,k);
          EVAL(app->sl,0);
        }
      }
      else {
        MKCAP(app->sl,fno,m);
        EVAL(app->sl,0);
      }
    }
    break;
  }
  case TYPE_LETREC: {
    int oldcount = p->names->count;
    Cletrec(gp,c,n,p,1);
    n += pcount(p)-oldcount;
    E(gp,c->body,p,n);
    SQUEEZE(c->sl,1,pcount(p)-oldcount);
    presize(p,oldcount);
    break;
  }
  case TYPE_SYMBOL:
    EVAL(c->sl,n-presolve(p,c->name));
    PUSH(c->sl,n-presolve(p,c->name));
    break;
  default:
    C(gp,c,p,n);
    EVAL(c->sl,0);
    break;
  }
  gp->cdepth--;
}

static void S(gprogram *gp, snode *source, stack *exprs, stack *strict, pmap *p, int n)
{
  int m;
  print_comp2_stack(gp,"S",exprs,n,"");
  for (m = 0; m < exprs->count; m++) {
    if (strict->data[m])
      E(gp,(snode*)exprs->data[m],p,n+m);
    else
      C(gp,(snode*)exprs->data[m],p,n+m);
  }
  SQUEEZE(source->sl,m,n);
}

static void R(gprogram *gp, snode *c, pmap *p, int n)
{
  gp->cdepth++;
  print_comp2(gp,"R",c,n,"");
  switch (snodetype(c)) {
  case TYPE_APPLICATION: {
    stack *args = stack_new();
    stack *argstrict = stack_new();
    snode *app;
    int m;
    for (app = c; TYPE_APPLICATION == snodetype(app); app = app->left) {
      stack_push(args,app->right);
      stack_push(argstrict,(void*)(app->tag & FLAG_STRICT));
    }
    m = args->count;
    if (TYPE_SYMBOL == snodetype(app)) {
      stack_push(args,app);
      stack_push(argstrict,0);
      S(gp,app,args,argstrict,p,n);
      EVAL(app->sl,0);
      DO(app->sl,0);
    }
    else if ((TYPE_BUILTIN == snodetype(app)) ||
             (TYPE_SCREF == snodetype(app))) {
      int fno = snode_fno(app);
      int k = function_nargs(fno);
      if (m > k) {
        S(gp,app,args,argstrict,p,n);
        MKFRAME(app->sl,fno,k);
        EVAL(app->sl,0);
        DO(app->sl,0);
      }
      else if (m == k) {
        if ((TYPE_BUILTIN == snodetype(app)) &&
            (B_IF == app->bif)) {
          snode *falsebranch = (snode*)args->data[0];
          snode *truebranch = (snode*)args->data[1];
          snode *cond = (snode*)args->data[2];
          int label;
          stackinfo *oldsi;

          E(gp,cond,p,n);
          JFALSE(cond->sl,label);

          stackinfo_newswap(&gp->si,&oldsi);
          R(gp,truebranch,p,n);
          stackinfo_freeswap(&gp->si,&oldsi);

          LABEL(label);
          stackinfo_newswap(&gp->si,&oldsi);
          R(gp,falsebranch,p,n);
          stackinfo_freeswap(&gp->si,&oldsi);
        }
        else if ((TYPE_BUILTIN == snodetype(app)) && (B_IF != app->bif)) {
          int bif;
          const builtin *bi;
          int argno;
          S(gp,app,args,argstrict,p,n);
          bif = app->bif;
          bi = &builtin_info[bif];
          assert(gp->si->count >= bi->nstrict);


          /* FIXME: temp: this is only because strictness analysis is disabled */
/*           for (argno = 0; argno < bi->nstrict; argno++) */
/*             EVAL(app->sl,argno); */






          for (argno = 0; argno < bi->nstrict; argno++)
            assert(statusat(gp->si,gp->si->count-1-argno));

          BIF(app->sl,bif);

/*           if (!bi->reswhnf) */
/*             EVAL(app->sl,0); */
/*           RETURN(app->sl); */

          if (!bi->reswhnf) {
            DO(app->sl,1);
          }
          else {
            RETURN(app->sl);
          }


        }
        else {
          S(gp,app,args,argstrict,p,n);
          JFUN(app->sl,fno);
        }
      }
      else { /* m < k */
        for (m = 0; m < args->count; m++)
          C(gp,(snode*)args->data[m],p,n+m);
        MKCAP(app->sl,fno,m);
        RETURN(app->sl);
      }
    }
    else {
      print_sourceloc(stderr,app->sl);
      fprintf(stderr,CONSTANT_APP_MSG"\n");
      exit(1);
    }
    stack_free(args);
    stack_free(argstrict);
    break;
  }
  case TYPE_SYMBOL:
    C(gp,c,p,n);
    SQUEEZE(c->sl,1,n);
    EVAL(c->sl,0);
    RETURN(c->sl);
    break;
  case TYPE_BUILTIN:
  case TYPE_SCREF:
    if (0 == function_nargs(snode_fno(c))) {
      JFUN(c->sl,snode_fno(c));
    }
    else {
      MKCAP(c->sl,snode_fno(c),0);
      RETURN(c->sl);
    }
    break;
  case TYPE_NIL:
  case TYPE_NUMBER:
  case TYPE_STRING:
    C(gp,c,p,n);
    RETURN(c->sl);
    break;
  case TYPE_LETREC: {
    int oldcount = p->names->count;
    Cletrec(gp,c,n,p,1);
    n += pcount(p)-oldcount;
    R(gp,c->body,p,n);
    presize(p,oldcount);
    break;
  }
  default:
    abort();
    break;
  }
  gp->cdepth--;
}

static void C(gprogram *gp, snode *c, pmap *p, int n)
{
  gp->cdepth++;
  print_comp2(gp,"C",c,n,"");
  switch (snodetype(c)) {
  case TYPE_APPLICATION: {
    int m = 0;
    snode *app;
    int fno;
    int k;
    for (app = c; TYPE_APPLICATION == snodetype(app); app = app->left) {
      C(gp,app->right,p,n+m);
      m++;
    }

    assert(TYPE_SYMBOL != snodetype(app)); /* should be lifted into separate scomb otherwise */
    parse_check((TYPE_BUILTIN == snodetype(app)) || (TYPE_SCREF == snodetype(app)),
                app,CONSTANT_APP_MSG);

    fno = snode_fno(app);
    k = function_nargs(fno);
    assert(m <= k); /* should be lifted into separate supercombinator otherwise */

    if (m == k)
      MKFRAME(app->sl,fno,k);
    else
      MKCAP(app->sl,fno,m);
    break;
  }
  case TYPE_BUILTIN:
  case TYPE_SCREF:   if (0 == function_nargs(snode_fno(c)))
                       MKFRAME(c->sl,snode_fno(c),0);
                     else
                       MKCAP(c->sl,snode_fno(c),0);
                     break;
  case TYPE_SYMBOL:  PUSH(c->sl,n-presolve(p,c->name));                        break;
  case TYPE_NIL:     PUSHNIL(c->sl);                                           break;
  case TYPE_NUMBER:  PUSHNUMBER(c->sl,((int*)&c->num)[0],((int*)&c->num)[1]);  break;
  case TYPE_STRING:  PUSHSTRING(c->sl,add_string(gp,c->value));                break;
  case TYPE_LETREC: {
    int oldcount = p->names->count;
    Cletrec(gp,c,n,p,0);
    n += pcount(p)-oldcount;
    C(gp,c->body,p,n);
    SQUEEZE(c->sl,1,pcount(p)-oldcount);
    presize(p,oldcount);
    break;
  }
  default:           abort();                                    break;
  }
  gp->cdepth--;
}



static void F(gprogram *gp, int fno, scomb *sc)
{
  int i;
  snode *copy = sc->body;
  stackinfo *oldsi;
  pmap pm;

  gp->addressmap[NUM_BUILTINS+sc->index] = gp->count;

  oldsi = gp->si;
  gp->si = stackinfo_new(NULL);

/*   pushstatus(gp->si,0); // redex */
  for (i = 0; i < sc->nargs; i++) {
    /* FIXME: add evals here */
    pushstatus(gp->si,0);
  }

  GLOBSTART(sc->sl,fno,sc->nargs);
  for (i = 0; i < sc->nargs; i++)
    if (sc->strictin && sc->strictin[i])
      EVAL(sc->sl,i);

  gp->noevaladdressmap[NUM_BUILTINS+sc->index] = gp->count;
  GLOBSTART(sc->sl,fno,sc->nargs);

#ifdef DEBUG_GCODE_COMPILATION
  printf("\n");
  printf("Compiling supercombinator ");
  print_scomb_code(sc);
  printf("\n");
#endif

  pm.names = stack_new();
  pm.indexes = stack_new();
  for (i = 0; i < sc->nargs; i++) {
    stack_push(pm.names,sc->argnames[i]);
    stack_push(pm.indexes,(void*)(sc->nargs-i));
  }

/*   R(gp,copy,sc->nargs,&pm); */
  R(gp,copy,&pm,sc->nargs);

  stack_free(pm.names);
  stack_free(pm.indexes);

  stackinfo_free(gp->si);
  gp->si = oldsi;
#ifdef DEBUG_GCODE_COMPILATION
  printf("\n\n");
#endif
}

static void compute_stacksizes(gprogram *gp)
{
  int fno = -1;
  int addr = 0;
  int stacksize = 0;
  while (addr <= gp->count) {
    if ((addr == gp->count) ||
        ((OP_GLOBSTART == gp->ginstrs[addr].opcode) && (fno != gp->ginstrs[addr].arg0))) {
      if (0 <= fno)
        gp->stacksizes[fno] = stacksize;
      if (addr == gp->count)
        return;
      fno = gp->ginstrs[addr].arg0;
      stacksize = gp->ginstrs[addr].expcount;
    }
    else if (stacksize < gp->ginstrs[addr].expcount) {
      stacksize = gp->ginstrs[addr].expcount;
    }
    addr++;
  }
}

gprogram *cur_program = NULL;
void compile(gprogram *gp)
{
  scomb *sc;
  int index = 0;
  int i;
  stackinfo *topsi = NULL; /* stackinfo_new(NULL); */
  sourceloc nosl;

  scomb *startsc = get_scomb("__start");
  assert(startsc);

  nosl.fileno = -1;
  nosl.lineno = -1;

  for (sc = scombs; sc; sc = sc->next)
    sc->index = index++;

  gp->nfunctions = NUM_BUILTINS+index;
  gp->fnames = (char**)malloc(gp->nfunctions*sizeof(char*));
  for (i = 0; i < NUM_BUILTINS; i++)
    gp->fnames[i] = strdup(builtin_info[i].name);
  for (sc = scombs; sc; sc = sc->next)
    gp->fnames[i++] = real_scname(sc->name);

  gp->addressmap = (int*)calloc(gp->nfunctions,sizeof(int));
  gp->noevaladdressmap = (int*)calloc(gp->nfunctions,sizeof(int));
  gp->stacksizes = (int*)calloc(gp->nfunctions,sizeof(int));

  gp->si = stackinfo_new(NULL);
  BEGIN(startsc->sl);
  MKFRAME(startsc->sl,startsc->index+NUM_BUILTINS,0);
  EVAL(startsc->sl,0);
  END(startsc->sl);
  stackinfo_free(gp->si);
  gp->si = NULL;

  gp->evaldoaddr = gp->count;
  EVAL(nosl,0);
  DO(nosl,0);

  for (sc = scombs; sc; sc = sc->next)
    F(gp,NUM_BUILTINS+sc->index,sc);

  topsi = gp->si;
  for (i = 0; i < NUM_BUILTINS; i++) {
    int argno;
    const builtin *bi = &builtin_info[i];
    gp->si = stackinfo_new(NULL);

/*     pushstatus(gp->si,0); // redex */
    for (argno = 0; argno < bi->nargs; argno++)
      pushstatus(gp->si,0);

    gp->addressmap[i] = gp->count;
    GLOBSTART(nosl,i,bi->nargs);

    if (B_IF == i) {
      int label1;
      stackinfo *oldsi;
      int label2;
      PUSH(nosl,0);
      EVAL(nosl,0);
      JFALSE(nosl,label1);

      oldsi = gp->si;
      gp->si = stackinfo_new(oldsi);
      PUSH(nosl,1);
      JUMP(nosl,label2);
      stackinfo_free(gp->si);
      gp->si = oldsi;

      /* label l1 */
      LABEL(label1);
      PUSH(nosl,2);

      /* label l2 */
      LABEL(label2);
      EVAL(nosl,0);
      RETURN(nosl);
    }
    else {
      for (argno = 0; argno < bi->nstrict; argno++)
        EVAL(nosl,argno);
      BIF(nosl,i);
      if (!bi->reswhnf)
        EVAL(nosl,0);
      RETURN(nosl);
    }

    stackinfo_free(gp->si);
  }
  gp->si = topsi;

  LAST(nosl);
/*   stackinfo_free(gp->si); */

  compute_stacksizes(gp);

  cur_program = gp;
}

