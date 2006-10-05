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

#include "bytecode.h"
#include "src/nreduce.h"
#include "compiler/source.h"
#include "runtime/runtime.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <assert.h>
#include <ctype.h>
#include <stdarg.h>
#include <math.h>

#define BYTECODE_C

#define MKCAP(_s,_a,_b)       add_instruction(comp,_s,OP_MKCAP,(_a),(_b))
#define MKFRAME(_s,_a,_b)     add_instruction(comp,_s,OP_MKFRAME,(_a),(_b))
#define JFUN(_s,_a)           add_instruction(comp,_s,OP_JFUN,(_a),0)
#define UPDATE(_s,_a)         add_instruction(comp,_s,OP_UPDATE,(_a),0)
#define DO(_s,_a)             add_instruction(comp,_s,OP_DO,_a,0)
#define EVAL(_s,_a)           { int arg0 = (_a);                \
                                if (!comp->si || !statusat(comp->si,comp->si->count-1-arg0)) { \
                                   add_instruction(comp,_s,OP_EVAL,arg0,0); \
                                   add_instruction(comp,_s,OP_RESOLVE,arg0,0); \
                                }}
#define RESOLVE(_s,_a)        add_instruction(comp,_s,OP_RESOLVE,(_a),0)
#define RETURN(_s)            add_instruction(comp,_s,OP_RETURN,0,0)
#define PUSH(_s,_a)           add_instruction(comp,_s,OP_PUSH,(_a),0)
#define BIF(_s,_a)            add_instruction(comp,_s,OP_BIF,(_a),0)
#define JUMPrel(_s,_a)        add_instruction(comp,_s,OP_JUMP,(_a),0)
#define GLOBSTART(_s,_a,_b)   add_instruction(comp,_s,OP_GLOBSTART,(_a),(_b))
#define END(_s)               add_instruction(comp,_s,OP_END,0,0)
#define BEGIN(_s)             add_instruction(comp,_s,OP_BEGIN,0,0)
#define CALL(_s,_a)           add_instruction(comp,_s,OP_CALL,(_a),0)
#define ALLOC(_s,_a)          add_instruction(comp,_s,OP_ALLOC,(_a),0)
#define PUSHNIL(_s)           add_instruction(comp,_s,OP_PUSHNIL,0,0)
#define PUSHNUMBER(_s,_a,_b)  add_instruction(comp,_s,OP_PUSHNUMBER,(_a),(_b))
#define PUSHSTRING(_s,_a)     add_instruction(comp,_s,OP_PUSHSTRING,(_a),0)
#define SQUEEZE(_s,_a,_b)     add_instruction(comp,_s,OP_SQUEEZE,(_a),(_b))

#define JFALSE(_s,addr)       { addr = array_count(comp->instructions); \
                                add_instruction(comp,_s,OP_JFALSE,0,0); }
#define JUMP(_s,addr)         { addr = array_count(comp->instructions); \
                                add_instruction(comp,_s,OP_JUMP,0,0); }
#define LABEL(addr)           { array_item(comp->instructions,addr,instruction).arg0 = \
                                array_count(comp->instructions)-addr; }

typedef struct stackinfo {
  int alloc;
  int count;
  int *status;
  int invalid;
} stackinfo;

typedef struct compilation {
  bcheader bch;
  array *instructions;
  funinfo *finfo;
  array *stringmap;
  stackinfo *si;
  int cdepth;
} compilation;

const char *opcodes[OP_COUNT] = {
"BEGIN",
"END",
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

static void parse_check(source *src, int cond, snode *c, char *msg)
{
  if (cond)
    return;
  if (0 <= c->sl.fileno)
    fprintf(stderr,"%s:%d: ",lookup_parsedfile(src,c->sl.fileno),c->sl.lineno);
  fprintf(stderr,"%s\n",msg);
  exit(1);
}

static void print_comp2(source *src, compilation *comp, char *fname, snode *c, int n,
                        const char *format, ...)
{
#ifdef DEBUG_BYTECODE_COMPILATION
  va_list ap;
  debug(comp->cdepth,"%s [ ",fname);
  print_code(src,c);
  printf(" ]");
  printf(" %d",n);
  va_start(ap,format);
  vprintf(format,ap);
  va_end(ap);
  printf("\n");
#endif
}

static void print_comp2_stack(source *src, compilation *comp, char *fname, stack *exprs,
                              int n, const char *format, ...)
{
#ifdef DEBUG_BYTECODE_COMPILATION
  int i;
  va_list ap;
  debug(comp->cdepth,"%s [ ",fname);
  for (i = exprs->count-1; 0 <= i; i--) {
    if (i < exprs->count-1)
      printf(", ");
    print_code(src,(snode*)exprs->data[i]);
  }
  printf(" ]");
  printf(" %d",n);
  va_start(ap,format);
  vprintf(format,ap);
  va_end(ap);
  printf("\n");
#endif
}

static int add_string(compilation *comp, const char *str)
{
  int pos = array_count(comp->stringmap);
  char *copy = strdup(str);
  array_append(comp->stringmap,&copy,sizeof(cell*));
  return pos;
}

static void add_instruction(compilation *comp, sourceloc sl, int opcode, int arg0, int arg1)
{
  instruction *instr;
  instruction tmp;
  int addr = array_count(comp->instructions);
  assert(!comp->si || !comp->si->invalid);

  memset(&tmp,0,sizeof(instruction));
  array_append(comp->instructions,&instr,sizeof(instruction));

  instr = &array_item(comp->instructions,addr,instruction);

  instr->opcode = opcode;
  instr->arg0 = arg0;
  instr->arg1 = arg1;
  instr->fileno = sl.fileno;
  instr->lineno = sl.lineno;
  if (comp->si) {
    instr->expcount = comp->si->count;
  }
  else {
    instr->expcount = -1;
  }

  #ifdef DEBUG_BYTECODE_COMPILATION
  if (comp->si && (0 <= comp->cdepth)) {
    int i;
    for (i = 0; i < comp->cdepth; i++)
      printf("  ");
    printf("==> ");
    for (i = 0; i < 6-comp->cdepth; i++)
      printf("  ");
    printf("%d ",comp->si->count);
/*     printf("stackinfo"); */
/*     int i; */
/*     for (i = 0; i < comp->si->count; i++) { */
/*       printf(" %d",comp->si->status[i]); */
/*     } */
/*     printf("\n"); */
  }
  printf("%8d %-12s %-12d %-12d\n",addr,opcodes[instr->opcode],instr->arg0,instr->arg1);
  #endif

  if (!comp->si)
    return;

  switch (opcode) {
  case OP_BEGIN:
    break;
  case OP_END:
    break;
  case OP_GLOBSTART:
    break;
  case OP_EVAL:
    assert(0 <= comp->si->count-1-arg0);
    setstatusat(comp->si,comp->si->count-1-arg0,1);
    break;
  case OP_RETURN:
    // After RETURN we don't know what the stack size will be... but since it's always the
    // last instruction this is ok. Mark the stackinfo object as invalid to indicate the
    // information in it can't be relied upon anymore.
    comp->si->invalid = 1;
    break;
  case OP_DO:
    comp->si->invalid = 1;
    break;
  case OP_JFUN:
    comp->si->invalid = 1;
    break;
  case OP_JFALSE:
    popstatus(comp->si,1);
    break;
  case OP_JUMP:
    break;
  case OP_PUSH:
    assert(0 <= comp->si->count-1-arg0);
    pushstatus(comp->si,statusat(comp->si,comp->si->count-1-arg0));
    break;
  case OP_UPDATE:
    assert(0 <= comp->si->count-1-arg0);
    setstatusat(comp->si,comp->si->count-1-arg0,statusat(comp->si,comp->si->count-1));
    popstatus(comp->si,1);
    break;
  case OP_ALLOC: {
    int i;
    for (i = 0; i < arg0; i++)
      pushstatus(comp->si,1);
    break;
  }
  case OP_SQUEEZE:
    assert(0 <= comp->si->count-arg0-arg1);
    memmove(&comp->si->status[comp->si->count-arg0-arg1],
            &comp->si->status[comp->si->count-arg0],
            arg0*sizeof(int));
    comp->si->count -= arg1;
    break;
  case OP_MKCAP:
    assert(0 <= comp->si->count-arg1);
    popstatus(comp->si,arg1);
    pushstatus(comp->si,0);
    break;
  case OP_MKFRAME:
    assert(0 <= comp->si->count-arg1);
    popstatus(comp->si,arg1);
    pushstatus(comp->si,0);
    break;
  case OP_BIF:
    popstatus(comp->si,builtin_info[arg0].nargs-1);
    setstatusat(comp->si,comp->si->count-1,builtin_info[arg0].reswhnf);
    break;
  case OP_PUSHNIL:
    pushstatus(comp->si,1);
    break;
  case OP_PUSHNUMBER:
    pushstatus(comp->si,1);
    break;
  case OP_PUSHSTRING:
    pushstatus(comp->si,1);
    break;
  case OP_RESOLVE:
    assert(statusat(comp->si,comp->si->count-1-arg0));
    break;
  default:
    abort();
    break;
  }
}

static int snode_fno(snode *c)
{
  assert((SNODE_BUILTIN == c->type) ||
         (SNODE_SCREF == c->type));
  if (SNODE_BUILTIN == c->type)
    return c->bif;
  else if (SNODE_SCREF == c->type)
    return c->sc->index+NUM_BUILTINS;
  else
    return -1;
}

int function_nargs(source *src, int fno)
{
  assert(0 <= fno);
  if (NUM_BUILTINS > fno)
    return builtin_info[fno].nargs;
  else
    return get_scomb_index(src,fno-NUM_BUILTINS)->nargs;
}

typedef struct pmap {
  stack *names;
  stack *indexes;
} pmap;

static int presolve(source *src, pmap *pm, char *varname)
{
  int i;
  assert(pm->names->count == pm->indexes->count);
  for (i = 0; i < pm->names->count; i++)
    if (!strcmp((char*)pm->names->data[i],varname))
      return (int)pm->indexes->data[i];
  printf("unknown variable: %s\n",real_varname(src,varname));
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
  switch (c->type) {
  case SNODE_APPLICATION:
    getusage(c->left,used);
    getusage(c->right,used);
    break;
  case SNODE_LETREC: {
    letrec *rec;
    for (rec = c->bindings; rec; rec = rec->next)
      getusage(rec->value,used);
    getusage(c->body,used);
    break;
  }
  case SNODE_SYMBOL:
    if (!list_contains_string(*used,c->name))
      list_push(used,c->name);
    break;
  case SNODE_BUILTIN:
  case SNODE_SCREF:
  case SNODE_NIL:
  case SNODE_NUMBER:
  case SNODE_STRING:
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

static void C(source *src, compilation *comp, snode *c, pmap *p, int n);
static void E(source *src, compilation *comp, snode *c, pmap *p, int n);
static void Cletrec(source *src, compilation *comp, snode *c, int n, pmap *p, int strictcontext)
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
      E(src,comp,rec->value,p,n);
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
      E(src,comp,rec->value,p,n);
    else
      C(src,comp,rec->value,p,n);
    UPDATE(rec->value->sl,n+1-presolve(src,p,rec->name));
  }
}

static void E(source *src, compilation *comp, snode *c, pmap *p, int n)
{
  comp->cdepth++;
  print_comp2(src,comp,"E",c,n,"");
  switch (c->type) {
  case SNODE_APPLICATION: {
    int m = 0;
    snode *app;
    int fno;
    int k;
    for (app = c; SNODE_APPLICATION == app->type; app = app->left)
      m++;

    assert(SNODE_SYMBOL != app->type); /* should be lifted into separate scomb otherwise */
    parse_check(src,(SNODE_BUILTIN == app->type) || (SNODE_SCREF == app->type),
                app,CONSTANT_APP_MSG);

    fno = snode_fno(app);
    k = function_nargs(src,fno);
    assert(m <= k); /* should be lifted into separate supercombinator otherwise */

    if ((SNODE_BUILTIN == app->type) &&
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

      E(src,comp,cond,p,n);
      JFALSE(cond->sl,label);

      stackinfo_newswap(&comp->si,&oldsi);
      E(src,comp,truebranch,p,n);
      JUMP(cond->sl,end);
      stackinfo_freeswap(&comp->si,&oldsi);

      LABEL(label);
      stackinfo_newswap(&comp->si,&oldsi);
      E(src,comp,falsebranch,p,n);
      stackinfo_freeswap(&comp->si,&oldsi);

      LABEL(end);
      pushstatus(comp->si,1);
    }
    else {
      m = 0;
      for (app = c; SNODE_APPLICATION == app->type; app = app->left) {
        if (app->strict)
          E(src,comp,app->right,p,n+m);
        else
          C(src,comp,app->right,p,n+m);
        m++;
      }

      if (m == k) {

        if (SNODE_BUILTIN == app->type) {
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
  case SNODE_LETREC: {
    int oldcount = p->names->count;
    Cletrec(src,comp,c,n,p,1);
    n += pcount(p)-oldcount;
    E(src,comp,c->body,p,n);
    SQUEEZE(c->sl,1,pcount(p)-oldcount);
    presize(p,oldcount);
    break;
  }
  case SNODE_SYMBOL:
    EVAL(c->sl,n-presolve(src,p,c->name));
    PUSH(c->sl,n-presolve(src,p,c->name));
    break;
  default:
    C(src,comp,c,p,n);
    EVAL(c->sl,0);
    break;
  }
  comp->cdepth--;
}

static void S(source *src, compilation *comp, snode *source, stack *exprs,
              stack *strict, pmap *p, int n)
{
  int m;
  print_comp2_stack(src,comp,"S",exprs,n,"");
  for (m = 0; m < exprs->count; m++) {
    if (strict->data[m])
      E(src,comp,(snode*)exprs->data[m],p,n+m);
    else
      C(src,comp,(snode*)exprs->data[m],p,n+m);
  }
  SQUEEZE(source->sl,m,n);
}

static void R(source *src, compilation *comp, snode *c, pmap *p, int n)
{
  comp->cdepth++;
  print_comp2(src,comp,"R",c,n,"");
  switch (c->type) {
  case SNODE_APPLICATION: {
    stack *args = stack_new();
    stack *argstrict = stack_new();
    snode *app;
    int m;
    for (app = c; SNODE_APPLICATION == app->type; app = app->left) {
      stack_push(args,app->right);
      stack_push(argstrict,(void*)app->strict);
    }
    m = args->count;
    if (SNODE_SYMBOL == app->type) {
      stack_push(args,app);
      stack_push(argstrict,0);
      S(src,comp,app,args,argstrict,p,n);
      EVAL(app->sl,0);
      DO(app->sl,0);
    }
    else if ((SNODE_BUILTIN == app->type) ||
             (SNODE_SCREF == app->type)) {
      int fno = snode_fno(app);
      int k = function_nargs(src,fno);
      if (m > k) {
        S(src,comp,app,args,argstrict,p,n);
        MKFRAME(app->sl,fno,k);
        EVAL(app->sl,0);
        DO(app->sl,0);
      }
      else if (m == k) {
        if ((SNODE_BUILTIN == app->type) &&
            (B_IF == app->bif)) {
          snode *falsebranch = (snode*)args->data[0];
          snode *truebranch = (snode*)args->data[1];
          snode *cond = (snode*)args->data[2];
          int label;
          stackinfo *oldsi;

          E(src,comp,cond,p,n);
          JFALSE(cond->sl,label);

          stackinfo_newswap(&comp->si,&oldsi);
          R(src,comp,truebranch,p,n);
          stackinfo_freeswap(&comp->si,&oldsi);

          LABEL(label);
          stackinfo_newswap(&comp->si,&oldsi);
          R(src,comp,falsebranch,p,n);
          stackinfo_freeswap(&comp->si,&oldsi);
        }
        else if ((SNODE_BUILTIN == app->type) && (B_IF != app->bif)) {
          int bif;
          const builtin *bi;
          int argno;
          S(src,comp,app,args,argstrict,p,n);
          bif = app->bif;
          bi = &builtin_info[bif];
          assert(comp->si->count >= bi->nstrict);

          for (argno = 0; argno < bi->nstrict; argno++)
            assert(statusat(comp->si,comp->si->count-1-argno));

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
          S(src,comp,app,args,argstrict,p,n);
          JFUN(app->sl,fno);
        }
      }
      else { /* m < k */
        for (m = 0; m < args->count; m++)
          C(src,comp,(snode*)args->data[m],p,n+m);
        MKCAP(app->sl,fno,m);
        RETURN(app->sl);
      }
    }
    else {
      print_sourceloc(src,stderr,app->sl);
      fprintf(stderr,CONSTANT_APP_MSG"\n");
      exit(1);
    }
    stack_free(args);
    stack_free(argstrict);
    break;
  }
  case SNODE_SYMBOL:
    C(src,comp,c,p,n);
    SQUEEZE(c->sl,1,n);
    EVAL(c->sl,0);
    RETURN(c->sl);
    break;
  case SNODE_BUILTIN:
  case SNODE_SCREF:
    if (0 == function_nargs(src,snode_fno(c))) {
      JFUN(c->sl,snode_fno(c));
    }
    else {
      MKCAP(c->sl,snode_fno(c),0);
      RETURN(c->sl);
    }
    break;
  case SNODE_NIL:
  case SNODE_NUMBER:
  case SNODE_STRING:
    C(src,comp,c,p,n);
    RETURN(c->sl);
    break;
  case SNODE_LETREC: {
    int oldcount = p->names->count;
    Cletrec(src,comp,c,n,p,1);
    n += pcount(p)-oldcount;
    R(src,comp,c->body,p,n);
    presize(p,oldcount);
    break;
  }
  default:
    abort();
    break;
  }
  comp->cdepth--;
}

static void C(source *src, compilation *comp, snode *c, pmap *p, int n)
{
  comp->cdepth++;
  print_comp2(src,comp,"C",c,n,"");
  switch (c->type) {
  case SNODE_APPLICATION: {
    int m = 0;
    snode *app;
    int fno;
    int k;
    for (app = c; SNODE_APPLICATION == app->type; app = app->left) {
      C(src,comp,app->right,p,n+m);
      m++;
    }

    assert(SNODE_SYMBOL != app->type); /* should be lifted into separate scomb otherwise */
    parse_check(src,(SNODE_BUILTIN == app->type) || (SNODE_SCREF == app->type),
                app,CONSTANT_APP_MSG);

    fno = snode_fno(app);
    k = function_nargs(src,fno);
    assert(m <= k); /* should be lifted into separate supercombinator otherwise */

    if (m == k)
      MKFRAME(app->sl,fno,k);
    else
      MKCAP(app->sl,fno,m);
    break;
  }
  case SNODE_BUILTIN:
  case SNODE_SCREF:   if (0 == function_nargs(src,snode_fno(c)))
                       MKFRAME(c->sl,snode_fno(c),0);
                     else
                       MKCAP(c->sl,snode_fno(c),0);
                     break;
  case SNODE_SYMBOL:  PUSH(c->sl,n-presolve(src,p,c->name));                    break;
  case SNODE_NIL:     PUSHNIL(c->sl);                                           break;
  case SNODE_NUMBER:  PUSHNUMBER(c->sl,((int*)&c->num)[0],((int*)&c->num)[1]);  break;
  case SNODE_STRING:  PUSHSTRING(c->sl,add_string(comp,c->value));                break;
  case SNODE_LETREC: {
    int oldcount = p->names->count;
    Cletrec(src,comp,c,n,p,0);
    n += pcount(p)-oldcount;
    C(src,comp,c->body,p,n);
    SQUEEZE(c->sl,1,pcount(p)-oldcount);
    presize(p,oldcount);
    break;
  }
  default:           abort();                                    break;
  }
  comp->cdepth--;
}



static void F(source *src, compilation *comp, int fno, scomb *sc)
{
  int i;
  snode *copy = sc->body;
  stackinfo *oldsi;
  pmap pm;
  char *namestr;

  comp->finfo[NUM_BUILTINS+sc->index].address = array_count(comp->instructions);
  comp->finfo[NUM_BUILTINS+sc->index].arity = sc->nargs;

  namestr = real_scname(src,sc->name);
  comp->finfo[NUM_BUILTINS+sc->index].name = add_string(comp,namestr);
  free(namestr);

  oldsi = comp->si;
  comp->si = stackinfo_new(NULL);

  for (i = 0; i < sc->nargs; i++)
    pushstatus(comp->si,0);

  GLOBSTART(sc->sl,fno,sc->nargs);
  for (i = 0; i < sc->nargs; i++)
    if (sc->strictin && sc->strictin[i])
      EVAL(sc->sl,i);

  comp->finfo[NUM_BUILTINS+sc->index].addressne = array_count(comp->instructions);
  GLOBSTART(sc->sl,fno,sc->nargs);

#ifdef DEBUG_BYTECODE_COMPILATION
  printf("\n");
  printf("Compiling supercombinator ");
  print_scomb_code(src,sc);
  printf("\n");
#endif

  pm.names = stack_new();
  pm.indexes = stack_new();
  for (i = 0; i < sc->nargs; i++) {
    stack_push(pm.names,sc->argnames[i]);
    stack_push(pm.indexes,(void*)(sc->nargs-i));
  }

  R(src,comp,copy,&pm,sc->nargs);

  stack_free(pm.names);
  stack_free(pm.indexes);

  stackinfo_free(comp->si);
  comp->si = oldsi;
#ifdef DEBUG_BYTECODE_COMPILATION
  printf("\n\n");
#endif
}

static void compute_stacksizes(compilation *comp)
{
  int fno = -1;
  int addr = 0;
  int stacksize = 0;
  int nops = array_count(comp->instructions);
  while (addr <= array_count(comp->instructions)) {
    instruction *instr = &array_item(comp->instructions,addr,instruction);
    if ((addr == nops) || ((OP_GLOBSTART == instr->opcode) && (fno != instr->arg0))) {
      if (0 <= fno)
        comp->finfo[fno].stacksize = stacksize;
      if (addr == nops)
        return;
      fno = instr->arg0;
      stacksize = instr->expcount;
    }
    else if (stacksize < instr->expcount) {
      stacksize = instr->expcount;
    }
    addr++;
  }
}

void gen_bytecode(compilation *comp, char **bcdata, int *bcsize)
{
  char *data;
  int size;
  int i;
  int pos;
  int strpos;

  comp->bch.nops = array_count(comp->instructions);
  comp->bch.nstrings = array_count(comp->stringmap);

  size = sizeof(bcheader) +
         comp->bch.nops*sizeof(instruction) +
         comp->bch.nfunctions*sizeof(funinfo) +
         comp->bch.nstrings*sizeof(int);

  for (i = 0; i < comp->bch.nstrings; i++) { /* string data */
    char *str = array_item(comp->stringmap,i,char*);
    size += strlen(str)+1;
  }

  data = (char*)malloc(size);
  memcpy(data,&comp->bch,sizeof(bcheader));
  pos = sizeof(bcheader);

  assert(comp->bch.nops*sizeof(instruction) == comp->instructions->nbytes);
  memcpy(&data[pos],comp->instructions->data,comp->instructions->nbytes);
  pos += comp->instructions->nbytes;

  memcpy(&data[pos],comp->finfo,comp->bch.nfunctions*sizeof(funinfo));
  pos += comp->bch.nfunctions*sizeof(funinfo);

  strpos = pos+comp->bch.nstrings*sizeof(int);
  for (i = 0; i < comp->bch.nstrings; i++) {
    char *str = array_item(comp->stringmap,i,char*);

    *(int*)&data[pos] = strpos;
    pos += sizeof(int);

    memcpy(&data[strpos],str,strlen(str)+1);
    strpos += strlen(str)+1;
  }

  assert(strpos == size);

  *bcdata = data;
  *bcsize = size;
}

static void compile_prologue(compilation *comp, source *src)
{
  scomb *startsc = get_scomb(src,"__start");
  sourceloc nosl;
  nosl.fileno = -1;
  nosl.lineno = -1;

  assert(startsc);

  comp->si = stackinfo_new(NULL);
  BEGIN(startsc->sl);
  MKFRAME(startsc->sl,startsc->index+NUM_BUILTINS,0);
  EVAL(startsc->sl,0);
  END(startsc->sl);
  stackinfo_free(comp->si);
  comp->si = NULL;

  comp->bch.evaldoaddr = array_count(comp->instructions);
  EVAL(nosl,0);
  DO(nosl,0);
}

static void compile_scombs(compilation *comp, source *src)
{
  int scno;
  for (scno = 0; scno < array_count(src->scombs); scno++) {
    scomb *sc = array_item(src->scombs,scno,scomb*);
    F(src,comp,NUM_BUILTINS+sc->index,sc);
  }
}

static void compile_builtins(compilation *comp)
{
  int i;
  stackinfo *topsi = comp->si;
  sourceloc nosl;
  nosl.fileno = -1;
  nosl.lineno = -1;
  for (i = 0; i < NUM_BUILTINS; i++) {
    int argno;
    const builtin *bi = &builtin_info[i];
    comp->si = stackinfo_new(NULL);

    for (argno = 0; argno < bi->nargs; argno++)
      pushstatus(comp->si,0);

    comp->finfo[i].address = array_count(comp->instructions);
    comp->finfo[i].addressne = array_count(comp->instructions);
    comp->finfo[i].arity = bi->nargs;
    comp->finfo[i].name = add_string(comp,bi->name);
    GLOBSTART(nosl,i,bi->nargs);

    if (B_IF == i) {
      int label1;
      stackinfo *oldsi;
      int label2;
      PUSH(nosl,0);
      EVAL(nosl,0);
      JFALSE(nosl,label1);

      oldsi = comp->si;
      comp->si = stackinfo_new(oldsi);
      PUSH(nosl,1);
      JUMP(nosl,label2);
      stackinfo_free(comp->si);
      comp->si = oldsi;

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

    stackinfo_free(comp->si);
  }
  comp->si = topsi;
}

void compile(source *src, char **bcdata, int *bcsize)
{
  int i;
  compilation *comp = (compilation*)calloc(1,sizeof(compilation));


  comp->instructions = array_new(sizeof(instruction));
  comp->si = NULL;
  comp->stringmap = array_new(sizeof(char*));
  comp->cdepth = -1;

  assert(0 == array_count(comp->stringmap));
  for (i = 0; i < array_count(src->parsedfiles); i++) {
    char *filename = array_item(src->parsedfiles,i,char*);
    add_string(comp,filename);
  }

  comp->bch.nfunctions = NUM_BUILTINS+array_count(src->scombs);
  comp->finfo = (funinfo*)calloc(comp->bch.nfunctions,sizeof(funinfo));

  compile_prologue(comp,src);
  compile_scombs(comp,src);
  compile_builtins(comp);

  compute_stacksizes(comp);

  gen_bytecode(comp,bcdata,bcsize);

  for (i = 0; i < array_count(comp->stringmap); i++)
    free(array_item(comp->stringmap,i,char*));
  array_free(comp->stringmap);

  free(comp->finfo);
  array_free(comp->instructions);
  free(comp);
}

void bc_print(const char *bcdata, FILE *f, source *src, int builtins)
{
  int i;
  int addr;
  int fno;
  int prevfun = -1;
  const bcheader *bch = (const bcheader*)bcdata;
  const instruction *instructions = bc_instructions(bcdata);
  const funinfo *finfo = bc_funinfo(bcdata);

  fprintf(f,"nops = %d\n",bch->nops);
  fprintf(f,"nfunctions = %d\n",bch->nfunctions);
  fprintf(f,"nstrings = %d\n",bch->nstrings);
  fprintf(f,"evaldoaddr = %d\n",bch->evaldoaddr);

  fprintf(f,"\n");
  fprintf(f,"Ops:\n");
  fprintf(f,"\n");
  fprintf(f,"%8s %-12s %-12s %-12s %s\n","address","opcode","arg0","arg1","fileno/lineno");
  fprintf(f,"%8s %-12s %-12s %-12s %s\n","-------","------","----","----","-------------");
  for (addr = 0; addr < bch->nops; addr++) {
    const instruction *instr = &instructions[addr];
    const char *filename = (0 <= instr->fileno) ? bc_string(bcdata,instr->fileno) : "_";

    if ((OP_GLOBSTART == instr->opcode) && (prevfun != instr->arg0)) {
      if (NUM_BUILTINS <= instr->arg0) {
        scomb *sc = get_scomb_index(src,instr->arg0-NUM_BUILTINS);
        printf("\n");
        print_scomb_code(src,sc);
        printf("\n");
      }
      else if (!builtins) {
        break;
      }
      else {
        printf("\n");
        printf("Builtin: %s\n",builtin_info[instr->arg0].name);
      }
      printf("\n");
      prevfun = instr->arg0;
    }

    fprintf(f,"%8d %-12s %-12d %-12d %s:%d\n",
            addr,opcodes[instr->opcode],instr->arg0,instr->arg1,filename,instr->lineno);
  }


  fprintf(f,"\n");
  fprintf(f,"Functions:\n");
  fprintf(f,"\n");
  fprintf(f,"%-8s %-8s %-8s %-8s %s\n","fno","address","arity","stacksize","name");
  fprintf(f,"%-8s %-8s %-8s %-8s %s\n","---","-------","-----","---------","----");
  for (fno = 0; fno < bch->nfunctions; fno++) {
    const funinfo *fi = &finfo[fno];
    const char *name = bc_function_name(bcdata,fno);
    fprintf(f,"%-8d %-8d %-8d %-8d %s\n",fno,fi->address,fi->arity,fi->stacksize,name);
  }
  fprintf(f,"\n");
  fprintf(f,"Strings:\n");
  fprintf(f,"\n");
  for (i = 0; i < bch->nstrings; i++) {
    fprintf(f,"%4d: ",i);
    print_quoted_string(f,bc_string(bcdata,i));
    fprintf(f,"\n");
  }
}

const instruction *bc_instructions(const char *bcdata)
{
  return (const instruction*)&bcdata[sizeof(bcheader)];
}

const funinfo *bc_funinfo(const char *bcdata)
{
  return (const funinfo*)&bcdata[sizeof(bcheader)+
                                 ((bcheader*)bcdata)->nops*sizeof(instruction)];
}

const char *bc_string(const char *bcdata, int sno)
{
  const bcheader *bch = (const bcheader*)bcdata;
  int pos = sizeof(bcheader)+
            bch->nops*sizeof(instruction)+
            bch->nfunctions*sizeof(funinfo);
  const int *offsets = (const int*)&bcdata[pos];
  return bcdata+offsets[sno];
}

const char *bc_function_name(const char *bcdata, int fno)
{
  const funinfo *finfo = bc_funinfo(bcdata);
  return bc_string(bcdata,finfo[fno].name);
}
