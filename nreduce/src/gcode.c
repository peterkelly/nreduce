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

#define MKCAP(_a,_b)       add_instruction(gp,OP_MKCAP,(_a),(_b))
#define MKFRAME(_a,_b)     add_instruction(gp,OP_MKFRAME,(_a),(_b))
#define JFUN(_a)           add_instruction(gp,OP_JFUN,(_a),0)
#define UPDATE(_a)         add_instruction(gp,OP_UPDATE,(_a),0)
#define REPLACE(_a)        add_instruction(gp,OP_REPLACE,(_a),0)
#define POP(_a)            add_instruction(gp,OP_POP,(_a),0)
#define DO()               add_instruction(gp,OP_DO,0,0)
#define EVAL(_a)           add_instruction(gp,OP_EVAL,(_a),0)
#define RETURN()           add_instruction(gp,OP_RETURN,0,0)
#define PUSH(_a)           add_instruction(gp,OP_PUSH,(_a),0)
#define LAST()             add_instruction(gp,OP_LAST,0,0)
#define BIF(_a)            add_instruction(gp,OP_BIF,(_a),0)
#define JUMPrel(_a)        add_instruction(gp,OP_JUMP,(_a),0)
#define GLOBSTART(_a,_b)   add_instruction(gp,OP_GLOBSTART,(_a),(_b))
#define END()              add_instruction(gp,OP_END,0,0)
#define BEGIN()            add_instruction(gp,OP_BEGIN,0,0)
#define CALL(_a)           add_instruction(gp,OP_CALL,(_a),0)
#define ALLOC(_a)          add_instruction(gp,OP_ALLOC,(_a),0)
#define PUSHNIL()          add_instruction(gp,OP_PUSHNIL,0,0)
#define PUSHINT(_a)        add_instruction(gp,OP_PUSHINT,(_a),0)
#define PUSHDOUBLE(_a,_b)  add_instruction(gp,OP_PUSHDOUBLE,(_a),(_b))
#define PUSHSTRING(_a)     add_instruction(gp,OP_PUSHSTRING,(_a),0)
#define SQUEEZE(_a,_b)     add_instruction(gp,OP_SQUEEZE,(_a),(_b))

#define JFALSE(addr)       { addr = gp->count; add_instruction(gp,OP_JFALSE,0,0); }
#define JUMP(addr)         { addr = gp->count; add_instruction(gp,OP_JUMP,0,0); }
#define JEMPTY(addr)       { addr = gp->count; add_instruction(gp,OP_JEMPTY,0,0); }
#define LABEL(addr)        { gp->ginstrs[addr].arg0 = gp->count-addr; }

int *addressmap = NULL;
int *noevaladdressmap = NULL;
int nfunctions = 0;
int evaldoaddr = 0;

int op_usage[OP_COUNT];
int cdepth = -1;


stackinfo *stackinfo_new(stackinfo *source)
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

void stackinfo_free(stackinfo *si)
{
  free(si->status);
  free(si);
}

int statusat(stackinfo *si, int index)
{
  assert(index < si->count);
  assert(0 <= index);
  return si->status[index];
}

void setstatusat(stackinfo *si, int index, int i)
{
  assert(index < si->count);
  assert(0 <= index);
  si->status[index] = i;
}

void pushstatus(stackinfo *si, int i)
{
  if (si->alloc == si->count) {
    if (0 == si->alloc)
      si->alloc = 16;
    else
      si->alloc *= 2;
    si->status = (int*)realloc(si->status,si->alloc*sizeof(cell*));
  }
  si->status[si->count++] = i;
}

void popstatus(stackinfo *si, int n)
{
  si->count -= n;
  assert(0 <= si->count);
}

void stackinfo_newswap(stackinfo **si, stackinfo **tmp)
{
  *tmp = *si;
  *si = stackinfo_new(*si);
}

void stackinfo_freeswap(stackinfo **si, stackinfo **tmp)
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
"JEMPTY",
"PUSH",
"POP",
"UPDATE",
"REPLACE",
"ALLOC",
"SQUEEZE",
"MKCAP",
"MKFRAME",
"BIF",
"PUSHNIL",
"PUSHINT",
"PUSHDOUBLE",
"PUSHSTRING",
};

void print_comp(char *fname, cell *c, int d, int isresult, int needseval, int n)
{
#ifdef DEBUG_GCODE_COMPILATION
  debug(cdepth,"%s [ ",fname);
  print_code(c);
  printf(" ]");
  printf(" %d",d);
  if (isresult)
    printf(" (r)");
  if (needseval)
    printf(" (e)");
  if (0 < n)
    printf(" %d",n);
  printf("\n");
#endif
}

void print_comp2(char *fname, cell *c, int n, const char *format, ...)
{
#ifdef DEBUG_GCODE_COMPILATION
  debug(cdepth,"%s [ ",fname);
  print_code(c);
  printf(" ]");
  printf(" %d",n);
  va_list ap;
  va_start(ap,format);
  vprintf(format,ap);
  va_end(ap);
  printf("\n");
#endif
}

void print_comp2_stack(char *fname, stack *exprs, int n, const char *format, ...)
{
#ifdef DEBUG_GCODE_COMPILATION
  debug(cdepth,"%s [ ",fname);
  int i;
  for (i = exprs->count-1; 0 <= i; i--) {
    if (i < exprs->count-1)
      printf(", ");
    print_code((cell*)exprs->data[i]);
  }
  printf(" ]");
  printf(" %d",n);
  va_list ap;
  va_start(ap,format);
  vprintf(format,ap);
  va_end(ap);
  printf("\n");
#endif
}

gprogram *gprogram_new()
{
  gprogram *gp = (gprogram*)calloc(1,sizeof(gprogram));
  gp->count = 0;
  gp->alloc = 2;
  gp->ginstrs = (ginstr*)malloc(gp->alloc*sizeof(ginstr));
  gp->si = NULL;
  gp->stringmap = array_new();
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

  array_free(gp->stringmap);

  free(gp->ginstrs);
  free(gp);
}

int add_string(gprogram *gp, char *str)
{
  int pos = gp->stringmap->size/sizeof(cell*);
  cell *c = alloc_cell();
  c->tag = TYPE_STRING | FLAG_PINNED;
  c->field1 = strdup(str);
  array_append(gp->stringmap,&c,sizeof(cell*));
  return pos;
}

void add_instruction(gprogram *gp, int opcode, int arg0, int arg1)
{
  ginstr *instr;
  assert(!gp->si || !gp->si->invalid);

#if 1
  /* Skip the EVAL instruction if we know this stack location has already been evaluated */
  if ((OP_EVAL == opcode) && gp->si && statusat(gp->si,gp->si->count-1-arg0)) {
/*     printf("skipping eval (%d)\n",arg0); */
    return;
  }
#endif

  if (gp->alloc <= gp->count) {
    gp->alloc *= 2;
    gp->ginstrs = (ginstr*)realloc(gp->ginstrs,gp->alloc*sizeof(ginstr));
  }
  instr = &gp->ginstrs[gp->count++];

  memset(instr,0,sizeof(ginstr));
  instr->opcode = opcode;
  instr->arg0 = arg0;
  instr->arg1 = arg1;
  if (gp->si) {
    instr->expcount = gp->si->count;
    instr->expstatus = (int*)malloc(gp->si->count*sizeof(int));
    memcpy(instr->expstatus,gp->si->status,gp->si->count*sizeof(int));
  }
  else {
    instr->expcount = -1;
  }

  #ifdef DEBUG_GCODE_COMPILATION
  if (gp->si && (0 <= cdepth)) {
    int i;
    for (i = 0; i < cdepth; i++)
      printf("  ");
    printf("==> ");
    for (i = 0; i < 6-cdepth; i++)
      printf("  ");
    printf("%d ",gp->si->count);
/*     printf("stackinfo"); */
/*     int i; */
/*     for (i = 0; i < gp->si->count; i++) { */
/*       printf(" %d",gp->si->status[i]); */
/*     } */
/*     printf("\n"); */
  }
  print_ginstr(gp,gp->count-1,instr,0);
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
  case OP_JEMPTY:
    break;
  case OP_PUSH:
    assert(0 <= gp->si->count-1-arg0);
    pushstatus(gp->si,statusat(gp->si,gp->si->count-1-arg0));
    break;
  case OP_POP:
    assert(0 <= gp->si->count-1-arg0);
    popstatus(gp->si,arg0);
    break;
  case OP_UPDATE:
    assert(0 <= gp->si->count-1-arg0);
    setstatusat(gp->si,gp->si->count-1-arg0,statusat(gp->si,gp->si->count-1));
    popstatus(gp->si,1);
    break;
  case OP_REPLACE:
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
  case OP_PUSHINT:
    pushstatus(gp->si,1);
    break;
  case OP_PUSHDOUBLE:
    pushstatus(gp->si,1);
    break;
  case OP_PUSHSTRING:
    pushstatus(gp->si,1);
    break;
  default:
    assert(0);
    break;
  }
}

int cell_fno(cell *c)
{
  assert((TYPE_BUILTIN == celltype(c)) ||
         (TYPE_SCREF == celltype(c)));
  if (TYPE_BUILTIN == celltype(c))
    return (int)c->field1;
  else if (TYPE_SCREF == celltype(c))
    return ((scomb*)c->field1)->index+NUM_BUILTINS;
  else
    return -1;
}

const char *function_name(int fno)
{
  /* FIXME: this leaks memory, since real_scname makes a copy */
  if (0 > fno)
    return "(unknown)";
  else if (NUM_BUILTINS > fno)
    return builtin_info[fno].name;
  else
    return real_scname(get_scomb_index(fno-NUM_BUILTINS)->name);
}

int function_nargs(int fno)
{
  assert(0 <= fno);
  if (NUM_BUILTINS > fno)
    return builtin_info[fno].nargs;
  else
    return get_scomb_index(fno-NUM_BUILTINS)->nargs;
}

void print_ginstr(gprogram *gp, int address, ginstr *instr, int usage)
{
  assert(OP_COUNT > instr->opcode);

  printf("%-6d %-12s %-6d %-6d",
         address,op_names[instr->opcode],instr->arg0,instr->arg1);
  if (0 <= instr->expcount)
    printf(" %-4d",instr->expcount);
  else
    printf("       ");
  if (usage)
    printf(" u:%-6d",instr->usage);
  printf("    ");

  switch (instr->opcode) {
  case OP_GLOBSTART:
    if (NUM_BUILTINS > instr->arg0) {
      printf("; Builtin %s",builtin_info[instr->arg0].name);
    }
    else {
      char *name = real_scname(get_scomb_index(instr->arg0-NUM_BUILTINS)->name);
      printf("; Supercombinator %s",name);
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
          printf("; PUSH %s",real_varname(sc->argnames[sc->nargs-1-stackpos]));
      }
    }
    break;
  case OP_PUSHSTRING:
    printf("; PUSHSTRING \"%s\"",(char*)(((cell**)gp->stringmap->data)[instr->arg0])->field1);
    break;
  case OP_MKFRAME:
    printf("; MKFRAME %s %d",function_name(instr->arg0),instr->arg1);
    break;
  case OP_MKCAP:
    printf("; MKCAP %s %d",function_name(instr->arg0),instr->arg1);
    break;
  case OP_JFUN:
    printf("; JFUN %s",function_name(instr->arg0));
    break;
  case OP_BIF:
    printf("; %s",builtin_info[instr->arg0].name);
    break;
  default:
    break;
  }

  printf("\n");
}

void print_program(gprogram *gp, int builtins, int usage)
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
    print_ginstr(gp,i,&gp->ginstrs[i],usage);
  }

  printf("\n");
  printf("String map:\n");
  for (i = 0; i < gp->stringmap->size/sizeof(cell*); i++) {
    cell *strcell = ((cell**)gp->stringmap->data)[i];
    assert(TYPE_STRING == celltype(strcell));
    printf("%d: ",i);
    print_quoted_string(stdout,(char*)strcell->field1);
    printf("\n");
  }
}

void print_profiling(gprogram *gp)
{
  print_program(gp,1,1);

  int index = 0;
  scomb *sc;
  for (sc = scombs; sc; sc = sc->next)
    sc->index = index++;

  int *usage = (int*)calloc((index+1)+NUM_BUILTINS,sizeof(int));

  int prevfun = -1;
  int fno = -1;
  int curusage = 0;
  int totalusage = 0;

  int addr = 0;
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
      curusage += gp->ginstrs[addr].usage;
      totalusage += gp->ginstrs[addr].usage;
    }
    addr++;
  }

  for (fno = 0; fno <= NUM_BUILTINS+index; fno++) {
    printf("%-9d",usage[fno]);
    double portion = (((double)usage[fno])/((double)totalusage))*100.0;
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

int presolve(pmap *pm, char *varname)
{
  assert(pm->names->count == pm->indexes->count);
  int i;
  for (i = 0; i < pm->names->count; i++)
    if (!strcmp((char*)pm->names->data[i],varname))
      return (int)pm->indexes->data[i];
  printf("unknown variable: %s\n",real_varname(varname));
  abort();
  return -1;
}

void presize(pmap *pm, int count)
{
  assert(pm->names->count == pm->indexes->count);
  assert(count <= pm->names->count);
  pm->names->count = count;
  pm->indexes->count = count;
}

int pcount(pmap *pm)
{
  assert(pm->names->count == pm->indexes->count);
  return pm->names->count;
}

void getusage(cell *c, list **used)
{
  switch (celltype(c)) {
  case TYPE_APPLICATION:
    getusage((cell*)c->field1,used);
    getusage((cell*)c->field2,used);
    break;
  case TYPE_LETREC: {
    letrec *rec;
    for (rec = (letrec*)c->field1; rec; rec = rec->next)
      getusage(rec->value,used);
    getusage((cell*)c->field2,used);
    break;
  }
  case TYPE_SYMBOL:
    if (!list_contains_string(*used,(char*)c->field1))
      list_push(used,(char*)c->field1);
    break;
  case TYPE_BUILTIN:
  case TYPE_SCREF:
  case TYPE_NIL:
  case TYPE_INT:
  case TYPE_DOUBLE:
  case TYPE_STRING:
    break;
  }
}

int letrecs_used(cell *expr, letrec *first)
{
  int count = 0;
  list *used = NULL;
  getusage(expr,&used);

  letrec *rec;
  for (rec = first; rec; rec = rec->next)
    if (list_contains_string(used,rec->name))
      count++;

  list_free(used,NULL);
  return count;
}

void C(gprogram *gp, cell *c, pmap *p, int n);
void E(gprogram *gp, cell *c, pmap *p, int n);
void Cletrec(gprogram *gp, cell *c, int n, pmap *p, int strictcontext)
{
  letrec *rec;
  int count = 0;
  for (rec = (letrec*)c->field1; rec; rec = rec->next) {
    count++;
    stack_push(p->names,rec->name);
    stack_push(p->indexes,(void*)(n+count));
  }

  rec = (letrec*)c->field1;
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

  ALLOC(count);
  n += count;

  for (; rec; rec = rec->next) {
    if (strictcontext && rec->strict && (0 == letrecs_used(rec->value,rec)))
      E(gp,rec->value,p,n);
    else
      C(gp,rec->value,p,n);
    UPDATE(n+1-presolve(p,rec->name));
  }
}

void E(gprogram *gp, cell *c, pmap *p, int n)
{
  cdepth++;
  print_comp2("E",c,n,"");
  switch (celltype(c)) {
  case TYPE_APPLICATION: {
    int m = 0;
    cell *app;
    for (app = c; TYPE_APPLICATION == celltype(app); app = (cell*)app->field1)
      m++;

    assert(TYPE_SYMBOL != celltype(app)); /* should be lifted into separate scomb otherwise */
    parse_check((TYPE_BUILTIN == celltype(app)) || (TYPE_SCREF == celltype(app)),
                app,"Non-function applied to args");

    int fno = cell_fno(app);
    int k = function_nargs(fno);
    assert(m <= k); /* should be lifted into separate supercombinator otherwise */

    if ((TYPE_BUILTIN == celltype(app)) &&
        (B_IF == (int)app->field1) &&
        (3 == m)) {
      cell *app = c;
      cell *falsebranch = (cell*)app->field2;
      app = (cell*)app->field1;
      cell *truebranch = (cell*)app->field2;
      app = (cell*)app->field1;
      cell *cond = (cell*)app->field2;

      E(gp,cond,p,n);
      int label;
      JFALSE(label);

      stackinfo *oldsi;
      stackinfo_newswap(&gp->si,&oldsi);
      E(gp,truebranch,p,n);
      int end;
      JUMP(end);
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
      for (app = c; TYPE_APPLICATION == celltype(app); app = (cell*)app->field1) {
        if (app->tag & FLAG_STRICT)
          E(gp,(cell*)app->field2,p,n+m);
        else
          C(gp,(cell*)app->field2,p,n+m);
        m++;
      }

      if (m == k) {

        if (TYPE_BUILTIN == celltype(app)) {
          BIF(fno);
          if (!builtin_info[fno].reswhnf)
            EVAL(0);
        }
        else {
          MKFRAME(fno,k);
          EVAL(0);
        }
      }
      else {
        MKCAP(fno,m);
        EVAL(0);
      }
    }
    break;
  }
  case TYPE_LETREC: {
    int oldcount = p->names->count;
    Cletrec(gp,c,n,p,1);
    n += pcount(p)-oldcount;
    E(gp,(cell*)c->field2,p,n);
    SQUEEZE(1,pcount(p)-oldcount);
    presize(p,oldcount);
    break;
  }
  default:
    C(gp,c,p,n);
    EVAL(0);
    break;
  }
  cdepth--;
}

void S(gprogram *gp, stack *exprs, stack *strict, pmap *p, int n)
{
  print_comp2_stack("S",exprs,n,"");
  int m;
  for (m = 0; m < exprs->count; m++) {
    if (strict->data[m])
      E(gp,(cell*)exprs->data[m],p,n+m);
    else
      C(gp,(cell*)exprs->data[m],p,n+m);
  }
  SQUEEZE(m,n);
}

void R(gprogram *gp, cell *c, pmap *p, int n)
{
  cdepth++;
  print_comp2("R",c,n,"");
  switch (celltype(c)) {
  case TYPE_APPLICATION: {
    stack *args = stack_new();
    stack *argstrict = stack_new();
    cell *app;
    for (app = c; TYPE_APPLICATION == celltype(app); app = (cell*)app->field1) {
      stack_push(args,app->field2);
      stack_push(argstrict,(void*)(app->tag & FLAG_STRICT));
    }
    int m = args->count;
    if (TYPE_SYMBOL == celltype(app)) {
      stack_push(args,app);
      stack_push(argstrict,0);
      S(gp,args,argstrict,p,n);
      EVAL(0);
      DO();
    }
    else if ((TYPE_BUILTIN == celltype(app)) ||
             (TYPE_SCREF == celltype(app))) {
      int fno = cell_fno(app);
      int k = function_nargs(fno);
      if (m > k) {
        S(gp,args,argstrict,p,n);
        MKFRAME(fno,k);
        EVAL(0);
        DO();
      }
      else if (m == k) {
        if ((TYPE_BUILTIN == celltype(app)) &&
            (B_IF == (int)app->field1)) {
          cell *falsebranch = args->data[0];
          cell *truebranch = args->data[1];
          cell *cond = args->data[2];

          E(gp,cond,p,n);
          int label;
          JFALSE(label);

          stackinfo *oldsi;
          stackinfo_newswap(&gp->si,&oldsi);
          R(gp,truebranch,p,n);
          stackinfo_freeswap(&gp->si,&oldsi);

          LABEL(label);
          stackinfo_newswap(&gp->si,&oldsi);
          R(gp,falsebranch,p,n);
          stackinfo_freeswap(&gp->si,&oldsi);
        }
        else if ((TYPE_BUILTIN == celltype(app)) && (B_IF != (int)app->field1)) {
          S(gp,args,argstrict,p,n);
          int bif = (int)app->field1;
          const builtin *bi = &builtin_info[bif];
          int argno;
          assert(gp->si->count >= bi->nstrict);
          for (argno = 0; argno < bi->nstrict; argno++)
            assert(statusat(gp->si,gp->si->count-1-argno));
          BIF(bif);
          if (!bi->reswhnf)
            EVAL(0);
          RETURN();
        }
        else {
          S(gp,args,argstrict,p,n);
          JFUN(fno);
        }
      }
      else { /* m < k */
        for (m = 0; m < args->count; m++)
          C(gp,(cell*)args->data[m],p,n+m);
        MKCAP(fno,m);
        RETURN();
      }
    }
    else {
      fprintf(stderr,"%s can't be applied to anything\n",cell_types[celltype(app)]);
      assert(0);
    }
    stack_free(args);
    stack_free(argstrict);
    break;
  }
  case TYPE_SYMBOL:
    C(gp,c,p,n);
    SQUEEZE(1,n);
    EVAL(0);
    RETURN();
    break;
  case TYPE_BUILTIN:
  case TYPE_SCREF:
    if (0 == function_nargs(cell_fno(c))) {
      JFUN(cell_fno(c));
    }
    else {
      MKCAP(cell_fno(c),0);
      RETURN();
    }
    break;
  case TYPE_NIL:
  case TYPE_INT:
  case TYPE_DOUBLE:
  case TYPE_STRING:
    C(gp,c,p,n);
    RETURN();
    break;
  case TYPE_LETREC: {
    int oldcount = p->names->count;
    Cletrec(gp,c,n,p,1);
    n += pcount(p)-oldcount;
    R(gp,(cell*)c->field2,p,n);
    presize(p,oldcount);
    break;
  }
  default:
    assert(0);
    break;
  }
  cdepth--;
}

void C(gprogram *gp, cell *c, pmap *p, int n)
{
  cdepth++;
  print_comp2("C",c,n,"");
  switch (celltype(c)) {
  case TYPE_APPLICATION: {
    int m = 0;
    cell *app;
    for (app = c; TYPE_APPLICATION == celltype(app); app = (cell*)app->field1) {
      C(gp,(cell*)app->field2,p,n+m);
      m++;
    }

    assert(TYPE_SYMBOL != celltype(app)); /* should be lifted into separate scomb otherwise */
    parse_check((TYPE_BUILTIN == celltype(app)) || (TYPE_SCREF == celltype(app)),
                app,"Non-function applied to args");

    int fno = cell_fno(app);
    int k = function_nargs(fno);
    assert(m <= k); /* should be lifted into separate supercombinator otherwise */

    if (m == k)
      MKFRAME(fno,k);
    else
      MKCAP(fno,m);
    break;
  }
  case TYPE_BUILTIN:
  case TYPE_SCREF:   if (0 == function_nargs(cell_fno(c)))
                       MKFRAME(cell_fno(c),0);
                     else
                       MKCAP(cell_fno(c),0);
                     break;
  case TYPE_SYMBOL:  PUSH(n-presolve(p,(char*)c->field1));         break;
  case TYPE_NIL:     PUSHNIL();                                    break;
  case TYPE_INT:     PUSHINT((int)c->field1);                      break;
  case TYPE_DOUBLE:  PUSHDOUBLE((int)c->field1,(int)c->field2);    break;
  case TYPE_STRING:  PUSHSTRING(add_string(gp,(char*)c->field1));  break;
  case TYPE_LETREC: {
    int oldcount = p->names->count;
    Cletrec(gp,c,n,p,0);
    n += pcount(p)-oldcount;
    C(gp,(cell*)c->field2,p,n);
    SQUEEZE(1,pcount(p)-oldcount);
    presize(p,oldcount);
    break;
  }
  default:           assert(0);                                    break;
  }
  cdepth--;
}



void F(gprogram *gp, int fno, scomb *sc)
{
  int i;
  cell *copy = sc->body;

  addressmap[NUM_BUILTINS+sc->index] = gp->count;

  stackinfo *oldsi = gp->si;
  gp->si = stackinfo_new(NULL);

/*   pushstatus(gp->si,0); // redex */
  for (i = 0; i < sc->nargs; i++) {
    /* FIXME: add evals here */
    pushstatus(gp->si,0);
  }

  GLOBSTART(fno,sc->nargs);
  for (i = 0; i < sc->nargs; i++)
    if (sc->strictin && sc->strictin[i])
      EVAL(i);

  noevaladdressmap[NUM_BUILTINS+sc->index] = gp->count;
  GLOBSTART(fno,sc->nargs);

#ifdef DEBUG_GCODE_COMPILATION
  printf("\n");
  printf("Compiling supercombinator ");
  print_scomb_code(sc);
  printf("\n");
#endif

  pmap pm;

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

gprogram *cur_program = NULL;
void compile(gprogram *gp)
{
  scomb *sc;
  int index = 0;
  int i;
  stackinfo *topsi = NULL; /* stackinfo_new(NULL); */

  scomb *startsc = get_scomb("__start");
  assert(startsc);

  for (sc = scombs; sc; sc = sc->next)
    sc->index = index++;

  nfunctions = NUM_BUILTINS+index;

  addressmap = (int*)calloc(nfunctions,sizeof(int));
  noevaladdressmap = (int*)calloc(nfunctions,sizeof(int));

  gp->si = stackinfo_new(NULL);
  BEGIN();
  MKFRAME(startsc->index+NUM_BUILTINS,0);
  EVAL(0);
  POP(0);
  END();
  stackinfo_free(gp->si);
  gp->si = NULL;

  evaldoaddr = gp->count;
  EVAL(0);
  DO();

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

    addressmap[i] = gp->count;
    GLOBSTART(i,bi->nargs);

    if (B_IF == i) {
      PUSH(0);
      EVAL(0);
      int label1;
      JFALSE(label1);

      stackinfo *oldsi = gp->si;
      gp->si = stackinfo_new(oldsi);
      PUSH(1);
      int label2;
      JUMP(label2);
      stackinfo_free(gp->si);
      gp->si = oldsi;

      /* label l1 */
      LABEL(label1);
      PUSH(2);

      /* label l2 */
      LABEL(label2);
      EVAL(0);
      RETURN();
    }
    else {
      for (argno = 0; argno < bi->nstrict; argno++)
        EVAL(argno);
      BIF(i);
      if (!bi->reswhnf)
        EVAL(0);
      RETURN();
    }

    stackinfo_free(gp->si);
  }
  gp->si = topsi;

  LAST();
/*   stackinfo_free(gp->si); */

  cur_program = gp;
}

