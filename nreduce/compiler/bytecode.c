/*
 * This file is part of the nreduce project
 * Copyright (C) 2006-2008 Peter Kelly (pmk@cs.adelaide.edu.au)
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

#define STATUS_UNKNOWN     0
#define STATUS_SPARKED     1
#define STATUS_EVALUATED   2

#define MKCAP(_s,_a,_b)       add_instruction(comp,_s,OP_MKCAP,(_a),(_b))
//#define MKFRAME(_s,_a,_b)     add_instruction(comp,_s,OP_MKFRAME,(_a),(_b))
#define MKFRAME(_s,_a,_b)     domkframe(src,comp,(_s),(_a),(_b))
#define JFUN(_s,_a)           { if (have_resvals(src,(_a),comp->si)) \
                                  add_instruction(comp,_s,OP_JFUN,(_a),2); \
                                else if (have_reqsparks(src,(_a),comp->si)) \
                                  add_instruction(comp,_s,OP_JFUN,(_a),1); \
                                else \
                                  add_instruction(comp,_s,OP_JFUN,(_a),0); }
#define UPDATE(_s,_a)         add_instruction(comp,_s,OP_UPDATE,comp->si->count-1-(_a),0)
#define DO(_s,_a)             add_instruction(comp,_s,OP_DO,_a,0);
#define SPARK(_s,_a)          { int arg0 = (_a);                \
                                int pos = comp->si->count-1-arg0; \
                                if (!comp->si || (STATUS_SPARKED > statusat(comp->si,pos))) { \
                                   add_instruction(comp,_s,OP_SPARK,pos,0); \
                              }}
#define EVAL(_s,_a)           { int arg0 = (_a);                \
                                int pos = comp->si->count-1-arg0; \
                                if (!comp->si || (STATUS_EVALUATED > statusat(comp->si,pos))) \
                                  add_instruction(comp,_s,OP_EVAL,pos,0); \
                              }
#define RETURN(_s)            add_instruction(comp,_s,OP_RETURN,0,0)
#define PUSH(_s,_a)           add_instruction(comp,_s,OP_PUSH,comp->si->count-1-(_a),0)
#define POP(_s,_a)            add_instruction(comp,_s,OP_POP,(_a),0)
#define BIF(_s,_a)            { int _v; \
                                for (_v = 0; _v < builtin_info[(_a)].nstrict; _v++) \
                                  EVAL(_s,_v); \
                                add_instruction(comp,_s,OP_BIF,(_a),0); \
                              }
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
#define CONSN(_s,_a)          add_instruction(comp,_s,OP_CONSN,(_a),0)
#define ITEMN(_s,_a,_b)       add_instruction(comp,_s,OP_ITEMN,(_a),(_b))
#define ERROR(_s)             add_instruction(comp,_s,OP_ERROR,0,0)
#define INVALID(_s)           add_instruction(comp,_s,OP_INVALID,0,0)

#define JFALSE(_s,addr)       { EVAL(_s,0); \
                                addr = array_count(comp->instructions); \
                                add_instruction(comp,_s,OP_JFALSE,0,0); }
#define JEQ(_s,addr,v)        { EVAL(_s,0);                            \
                                addr = array_count(comp->instructions); \
                                add_instruction(comp,_s,OP_JEQ,0,v); }
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
"SPARK",
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
"POP",
"ERROR",
"EVAL",
"CALL",
"JCMP",
"CONSN",
"ITEMN",
"JEQ",
"INVALID",
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
  int count = array_count(comp->stringmap);
  int pos;
  char *copy;
  for (pos = 0; pos < count; pos++) {
    const char *existing = array_item(comp->stringmap,pos,const char*);
    if (!strcmp(existing,str))
      return pos;
  }

  copy = strdup(str);
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
  instr->code = 0;

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
  case OP_SPARK:
    assert(0 <= arg0);
    if (STATUS_SPARKED > statusat(comp->si,arg0))
      setstatusat(comp->si,arg0,STATUS_SPARKED);
    break;
  case OP_RETURN:
    // After RETURN we don't know what the stack size will be... but since it's always the
    // last instruction this is ok. Mark the stackinfo object as invalid to indicate the
    // information in it can't be relied upon anymore.
    comp->si->invalid = 1;
    break;
  case OP_DO:
/*     comp->si->invalid = 1; */
    break;
  case OP_JFUN:
    comp->si->invalid = 1;
    break;
  case OP_JFALSE:
    popstatus(comp->si,1);
    break;
  case OP_JEQ:
    break;
  case OP_JUMP:
    break;
  case OP_PUSH:
    assert(0 <= arg0);
    pushstatus(comp->si,statusat(comp->si,arg0));
    break;
  case OP_POP:
    assert(arg0 <= comp->si->count);
    popstatus(comp->si,arg0);
    break;
  case OP_UPDATE:
    assert(0 <= arg0);
    setstatusat(comp->si,arg0,statusat(comp->si,comp->si->count-1));
    popstatus(comp->si,1);
    break;
  case OP_ALLOC: {
    int i;
    for (i = 0; i < arg0; i++)
      pushstatus(comp->si,STATUS_UNKNOWN);
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
    pushstatus(comp->si,STATUS_EVALUATED);
    break;
  case OP_MKFRAME:
    assert(0 <= comp->si->count-arg1);
    popstatus(comp->si,arg1);
    pushstatus(comp->si,STATUS_UNKNOWN);
    break;
  case OP_BIF:
    popstatus(comp->si,builtin_info[arg0].nargs-1);
    if (builtin_info[arg0].reswhnf)
      setstatusat(comp->si,comp->si->count-1,STATUS_EVALUATED);
    else
      setstatusat(comp->si,comp->si->count-1,STATUS_UNKNOWN);
    break;
  case OP_PUSHNIL:
    pushstatus(comp->si,STATUS_EVALUATED);
    break;
  case OP_PUSHNUMBER:
    pushstatus(comp->si,STATUS_EVALUATED);
    break;
  case OP_PUSHSTRING:
    pushstatus(comp->si,STATUS_EVALUATED);
    break;
  case OP_ERROR:
    comp->si->invalid = 1;
    break;
  case OP_INVALID:
    comp->si->invalid = 1;
    break;
  case OP_EVAL:
    setstatusat(comp->si,arg0,STATUS_EVALUATED);
    break;
  case OP_CALL:
    /* Should only be added during peephole optimisation */
    abort();
    break;
  case OP_JCMP:
    /* Should only be added during peephole optimisation */
    abort();
    break;
  case OP_CONSN:
    popstatus(comp->si,arg0-1);
    setstatusat(comp->si,comp->si->count-1,STATUS_EVALUATED);
    break;
  case OP_ITEMN:
    setstatusat(comp->si,comp->si->count-1,STATUS_UNKNOWN);
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

int function_strictin(source *src, int fno, int argno)
{
  assert(0 <= fno);
  assert(0 <= argno);
  assert(argno < function_nargs(src,fno));
  if (NUM_BUILTINS > fno)
    return (argno < builtin_info[fno].nstrict);
  else
    return get_scomb_index(src,fno-NUM_BUILTINS)->strictin[argno];
}

int have_reqsparks(source *src, int fno, stackinfo *si)
{
  int nargs = function_nargs(src,fno);
  int i;
  assert(nargs <= si->count);
  for (i = 0; i < nargs; i++)
    if ((STATUS_SPARKED > statusat(si,si->count-1-i)) && function_strictin(src,fno,i))
      return 0;
  return 1;
}

int have_resvals(source *src, int fno, stackinfo *si)
{
  int nargs = function_nargs(src,fno);
  int i;
  assert(nargs <= si->count);
  for (i = 0; i < nargs; i++)
    if ((STATUS_EVALUATED > statusat(si,si->count-1-i)) && function_strictin(src,fno,i))
      return 0;
  return 1;
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

static void domkframe(source *src, compilation *comp, sourceloc sl, int fno, int n)
{
  if (NUM_BUILTINS <= fno) {
    scomb *sc = get_scomb_index(src,fno-NUM_BUILTINS);
    if (0 == sc->nargs) {
      snode *c = sc->body;
      assert(0 == n);
      if (SNODE_NUMBER == c->type) {
        PUSHNUMBER(c->sl,((int*)&c->num)[0],((int*)&c->num)[1]);
        return;
      }
      else if (SNODE_STRING == c->type) {
        PUSHSTRING(c->sl,add_string(comp,c->value));
        return;
      }
      else if (SNODE_NIL == c->type) {
        PUSHNIL(c->sl);
        return;
      }
      else if (SNODE_SCREF == c->type) {
        if (0 == function_nargs(src,snode_fno(c)))
          MKFRAME(c->sl,snode_fno(c),0);
        else
          MKCAP(c->sl,snode_fno(c),0);
        return;
      }
    }
  }
  else if (B_CONS == fno) {
    assert(2 == n);
    BIF(sl,B_CONS);
    return;
  }

  add_instruction(comp,sl,OP_MKFRAME,fno,n);
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

static int iscons(snode *c)
{
  return ((SNODE_APPLICATION == c->type) &&
          (SNODE_APPLICATION == c->left->type) &&
          (SNODE_BUILTIN == c->left->left->type) &&
          (B_CONS == c->left->left->bif));
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

      if ((SNODE_APPLICATION == cond->type) &&
          (SNODE_APPLICATION == cond->left->type) &&
          (SNODE_BUILTIN == cond->left->left->type) &&
          (B_EQ == cond->left->left->bif) &&
          (SNODE_NUMBER == cond->right->type) &&
          (floor(cond->right->num) == cond->right->num)) {


        E(src,comp,cond->left->right,p,n);
        JEQ(cond->sl,label,(int)cond->right->num);

        stackinfo_newswap(&comp->si,&oldsi);
        POP(cond->sl,1);
        E(src,comp,falsebranch,p,n);
        JUMP(cond->sl,end);
        stackinfo_freeswap(&comp->si,&oldsi);

        LABEL(label);
        stackinfo_newswap(&comp->si,&oldsi);
        POP(cond->sl,1);
        E(src,comp,truebranch,p,n);
        stackinfo_freeswap(&comp->si,&oldsi);

        popstatus(comp->si,1); /* for the POP instructions */

        LABEL(end);
        pushstatus(comp->si,STATUS_SPARKED);

      }
      else {
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
        pushstatus(comp->si,STATUS_SPARKED);
      }
    }
    else if ((SNODE_BUILTIN == app->type) &&
        (B_SEQ == app->bif) &&
        (2 == m)) {
      snode *expr2 = c->right;
      snode *expr1 = c->left->right;
      E(src,comp,expr1,p,n);
      EVAL(expr1->sl,0);
      POP(expr1->sl,1);
      E(src,comp,expr2,p,n);
    }
    else if ((m == k) &&
             (m == 2) &&
             (SNODE_SCREF == app->type) &&
             !strcmp(app->sc->name,"item") &&
             (SNODE_NUMBER == c->left->right->type) &&
             (c->left->right->num == floor(c->left->right->num))) {
      int index = (int)c->left->right->num;
      E(src,comp,c->right,p,n);
      ITEMN(app->sl,index,0);
      SPARK(app->sl,0);
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
          /* Only execute the BIF instruction directly if the function is pure; otherwise, it
             may be a function that can migrate, and must always be called from a wrapper frame */
          /* FIXME: don't do this for par, because it prevents parlist from releasing references
             to earlier parts of the list, making garbage collection less effective... see for
             example what happens with samples/httpclient.elc */
          if (builtin_info[fno].pure) {
            BIF(app->sl,fno);
            if (!builtin_info[fno].reswhnf)
              SPARK(app->sl,0);
          }
          else {
            MKFRAME(app->sl,fno,k);
            EVAL(app->sl,0);
          }
        }
        else {
          MKFRAME(app->sl,fno,k);
          SPARK(app->sl,0);
        }
      }
      else {
        MKCAP(app->sl,fno,m);
        SPARK(app->sl,0);
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
    SPARK(c->sl,n-presolve(src,p,c->name));
    PUSH(c->sl,n-presolve(src,p,c->name));
    break;
  default:
    C(src,comp,c,p,n);
    SPARK(c->sl,0);
    break;
  }
  comp->cdepth--;
}

static void S2(source *src, compilation *comp, snode *source, stack *exprs,
              stack *strict, pmap *p, int n)
{
  int m;
  print_comp2_stack(src,comp,"S2",exprs,n,"");

  for (m = 0; m < exprs->count; m++) {
    if (strict->data[m])
      E(src,comp,(snode*)exprs->data[m],p,n+m);
    else
      C(src,comp,(snode*)exprs->data[m],p,n+m);
  }
}

static void S(source *src, compilation *comp, sourceloc sl, stack *exprs,
              stack *strict, pmap *p, int n)
{
  int m = 0;
  int count = exprs->count;
  int remove = n;
  print_comp2_stack(src,comp,"S",exprs,n,"");

  /* If the first few expressions simply pass the function's arguments unchanged, we can
     avoid adding PUSH instructions for them, and only squeeze out the portion of the
     stack that will actually be modified. */
  for (; m < exprs->count; m++) {
    snode *sn = (snode*)exprs->data[m];
    if (SNODE_SYMBOL != sn->type)
      break;
    if (presolve(src,p,sn->name)-1 != m)
      break;
    if (strict->data[m] && (STATUS_UNKNOWN == statusat(comp->si,m)))
      break;
    count--;
    remove--;
  }

  for (; m < exprs->count; m++) {
    if (strict->data[m])
      E(src,comp,(snode*)exprs->data[m],p,n);
    else
      C(src,comp,(snode*)exprs->data[m],p,n);
    n++;
  }

  if (0 < remove)
    SQUEEZE(sl,count,remove);
}

static void Ccons(source *src, compilation *comp, snode *c, pmap *p, int n)
{
  stack *s = stack_new();
  snode *app = c;
  int count = 1;

  while (iscons(app)) {
    stack_push(s,app->left->right);
    count++;
    app = app->right;
  }
  stack_push(s,app);

  stack *exprs = stack_new();
  stack *strict = stack_new();
  int m;
  for (m = s->count-1; m >= 0; m--) {
    stack_push(exprs,s->data[m]);
    stack_push(strict,0);
  }

  S(src,comp,c->sl,exprs,strict,p,n);

  CONSN(c->sl,count);

  stack_free(s);
  stack_free(exprs);
  stack_free(strict);
}

static void R(source *src, compilation *comp, snode *c, pmap *p, int n)
{
  comp->cdepth++;
  print_comp2(src,comp,"R",c,n,"");
  switch (c->type) {
  case SNODE_APPLICATION: {
    stack *args;
    stack *argstrict;
    snode *app;
    int m;

    if (iscons(c) && iscons(c->right)) {
      Ccons(src,comp,c,p,n);
      RETURN(c->sl);
      return;
    }

    args = stack_new();
    argstrict = stack_new();

    for (app = c; SNODE_APPLICATION == app->type; app = app->left) {
      stack_push(args,app->right);
      stack_push(argstrict,(void*)app->strict);
    }
    m = args->count;
    if (SNODE_SYMBOL == app->type) {
      stack_push(args,app);
      stack_push(argstrict,0);
      S(src,comp,app->sl,args,argstrict,p,n);
      SPARK(app->sl,0);
      EVAL(app->sl,0);
      DO(app->sl,0);
    }
    else if ((SNODE_BUILTIN == app->type) ||
             (SNODE_SCREF == app->type)) {
      int fno = snode_fno(app);
      int k = function_nargs(src,fno);
      if (m > k) {
        S(src,comp,app->sl,args,argstrict,p,n);
        MKFRAME(app->sl,fno,k);
        SPARK(app->sl,0);
        EVAL(app->sl,0);
        DO(app->sl,0);
      }
      else if (m == k) {
        if (SNODE_BUILTIN == app->type) {
          if (B_IF == app->bif) {
            snode *falsebranch = (snode*)args->data[0];
            snode *truebranch = (snode*)args->data[1];
            snode *cond = (snode*)args->data[2];
            int label;
            stackinfo *oldsi;

            if ((SNODE_APPLICATION == cond->type) &&
                (SNODE_APPLICATION == cond->left->type) &&
                (SNODE_BUILTIN == cond->left->left->type) &&
                (B_EQ == cond->left->left->bif) &&
                (SNODE_NUMBER == cond->right->type) &&
                (floor(cond->right->num) == cond->right->num)) {

              E(src,comp,cond->left->right,p,n);
              JEQ(cond->sl,label,(int)cond->right->num);

              stackinfo_newswap(&comp->si,&oldsi);
              POP(cond->sl,1);
              R(src,comp,falsebranch,p,n);
              stackinfo_freeswap(&comp->si,&oldsi);

              LABEL(label);
              stackinfo_newswap(&comp->si,&oldsi);
              POP(cond->sl,1);
              R(src,comp,truebranch,p,n);
              stackinfo_freeswap(&comp->si,&oldsi);
            }
            else {
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
          }
          else if (B_SEQ == app->bif) {
            snode *expr2 = args->data[0];
            snode *expr1 = args->data[1];
            E(src,comp,expr1,p,n);
            EVAL(expr1->sl,0);
            POP(expr1->sl,1);
            R(src,comp,expr2,p,n);
          }
          else {
            int bif;
            const builtin *bi;
            int argno;
            S2(src,comp,app,args,argstrict,p,n);
            bif = app->bif;
            bi = &builtin_info[bif];
            assert(comp->si->count >= bi->nstrict);

            for (argno = 0; argno < bi->nstrict; argno++)
              assert(STATUS_SPARKED <= statusat(comp->si,comp->si->count-1-argno));

            /* Only execute the BIF instruction directly if the function is pure; otherwise, it
               may be a function that can migrate, and must always be called from a wrapper frame */
            if (!builtin_info[bif].pure) {
              MKFRAME(app->sl,bif,builtin_info[bif].nargs);
              EVAL(app->sl,0);
            }
            else {
              BIF(app->sl,bif);
            }

            if (0 < n)
              SQUEEZE(app->sl,1,n);
            assert(1 == comp->si->count);

            if (!bi->reswhnf) {
              EVAL(app->sl,0);
              DO(app->sl,1);
            }
            else {
              RETURN(app->sl);
            }
          }
        }
        else if ((m == 2) &&
                 (SNODE_SCREF == app->type) &&
                 !strcmp(app->sc->name,"item") &&
                 (SNODE_NUMBER == c->left->right->type) &&
                 (c->left->right->num == floor(c->left->right->num))) {
          int index = (int)c->left->right->num;
          E(src,comp,c->right,p,n);
          ITEMN(app->sl,index,1);
          if (0 < n)
            SQUEEZE(app->sl,1,n);
          EVAL(app->sl,0);
          DO(app->sl,1);
        }
        else {
          S(src,comp,app->sl,args,argstrict,p,n);
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
    SPARK(c->sl,0);
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

static int is_jump(int opcode)
{
  switch (opcode) {
  case OP_JFALSE:
  case OP_JUMP:
  case OP_JCMP:
  case OP_JEQ:
    return 1;
  default:
    return 0;
  }
}

static void peephole(compilation *comp, int start)
{
  int addr1;
  instruction *instrs = (instruction*)comp->instructions->data;
  int count = array_count(comp->instructions);
  int *map = (int*)calloc(count,sizeof(int));
  int changed;

  for (addr1 = start; addr1 < count; addr1++) {
    if (is_jump(instrs[addr1].opcode)) {
      instrs[addr1].arg0 += addr1;
      assert(instrs[addr1].arg0 >= start);
      assert(instrs[addr1].arg0 < count);
    }
  }

  do {
    int source = start;
    int dest = start;

    changed = 0;

    for (addr1 = start; addr1 < count; addr1++)
      map[addr1] = addr1;

    while (source < count) {
      if ((source+1 < count) &&
          (OP_SPARK == instrs[source].opcode) &&
          (OP_EVAL == instrs[source+1].opcode) &&
          (instrs[source].arg0 == instrs[source+1].arg0)) {
        instrs[dest] = instrs[source+1];
        map[source] = dest;
        map[source+1] = dest;
        dest++;
        source += 2;
        changed = 1;
      }
      else if ((source+1 < count) &&
          (OP_MKFRAME == instrs[source].opcode) &&
          (OP_EVAL == instrs[source+1].opcode) &&
          (instrs[source+1].expcount-1 == instrs[source+1].arg0)) {
        instrs[dest] = instrs[source];
        instrs[dest].opcode = OP_CALL;
        map[source] = dest;
        dest++;
        source++;
      }
      else if ((source+1 < count) &&
               (OP_SQUEEZE == instrs[source].opcode) &&
               (OP_RETURN == instrs[source+1].opcode)) {
        assert(0 < instrs[source].arg1);
        instrs[source+1].expcount = instrs[source].expcount;
        instrs[dest] = instrs[source+1];
        map[source] = dest;
        map[source+1] = dest;
        dest++;
        source += 2;
        changed = 1;
      }
      else if ((source+1 < count) &&
               (OP_BIF == instrs[source].opcode) &&
               (OP_JFALSE == instrs[source+1].opcode) &&
               ((B_EQ == instrs[source].arg0) ||
                (B_NE == instrs[source].arg0) ||
                (B_LT == instrs[source].arg0) ||
                (B_LE == instrs[source].arg0) ||
                (B_GT == instrs[source].arg0) ||
                (B_GE == instrs[source].arg0))) {
        instrs[dest] = instrs[source];
        instrs[dest].opcode = OP_JCMP;
        instrs[dest].arg1 = instrs[dest].arg0;
        instrs[dest].arg0 = instrs[source+1].arg0;
        map[source] = dest;
        map[source+1] = dest;
        dest++;
        source += 2;
        changed = 1;
      }
      /* JEQ optimisation */
      else if ((source+3 < count) &&
               (OP_PUSH == instrs[source].opcode) &&
               (OP_JEQ == instrs[source+1].opcode) &&
               (OP_POP == instrs[source+2].opcode) &&
               (OP_PUSH == instrs[source+3].opcode) &&
               (instrs[source].arg0 == instrs[source+3].arg0)) {

        int arg0 = instrs[source].arg0;

        map[source] = dest;
        instrs[dest++] = instrs[source++];

        map[source] = dest;
        instrs[dest++] = instrs[source++];

        while ((source+2 < count) &&
               (OP_POP == instrs[source].opcode) &&
               (OP_PUSH == instrs[source+1].opcode) &&
               (OP_JEQ == instrs[source+2].opcode) &&
               (arg0 == instrs[source+1].arg0)) {
          map[source++] = dest;
          map[source++] = dest;
          map[source] = dest;
          instrs[dest++] = instrs[source++];
        }
      }
      /* JEQ optimisation */
      else if ((source+4 < count) &&
               (OP_PUSH == instrs[source].opcode) &&
               (OP_EVAL == instrs[source+1].opcode) &&
               (instrs[source+1].arg0 == instrs[source].expcount) &&
               (OP_JEQ == instrs[source+2].opcode) &&
               (OP_POP == instrs[source+3].opcode) &&
               (OP_PUSH == instrs[source+4].opcode) &&
               (instrs[source].arg0 == instrs[source+4].arg0)) {

        int arg0 = instrs[source].arg0;

        map[source] = dest;
        instrs[dest++] = instrs[source++];

        map[source] = dest;
        instrs[dest++] = instrs[source++];

        map[source] = dest;
        instrs[dest++] = instrs[source++];

        while ((source+3 < count) &&
               (OP_POP == instrs[source].opcode) &&
               (OP_PUSH == instrs[source+1].opcode) &&
               (OP_EVAL == instrs[source+2].opcode) &&
               (instrs[source+2].arg0 == instrs[source+1].expcount) &&
               (OP_JEQ == instrs[source+3].opcode) &&
               (arg0 == instrs[source+1].arg0)) {
          map[source++] = dest;
          map[source++] = dest;
          map[source++] = dest;
          map[source] = dest;
          instrs[dest++] = instrs[source++];
        }
      }
      else if (OP_POP == instrs[source].opcode) {
        map[source++] = dest;
      }
      else {
        map[source] = dest;
        instrs[dest++] = instrs[source++];
      }
    }
    count = dest;

    for (addr1 = start; addr1 < count; addr1++)
      if (is_jump(instrs[addr1].opcode))
        instrs[addr1].arg0 = map[instrs[addr1].arg0];

  } while (changed);

  for (addr1 = start; addr1 < count; addr1++) {
    if (is_jump(instrs[addr1].opcode)) {
      assert(instrs[addr1].arg0 >= start);
      assert(instrs[addr1].arg0 < count);
      instrs[addr1].arg0 -= addr1;
    }
  }

  free(map);
  comp->instructions->nbytes = count*sizeof(instruction);
}

static void F(source *src, compilation *comp, int fno, scomb *sc)
{
  int i;
  snode *copy = sc->body;
  stackinfo *oldsi;
  pmap pm;
  char *namestr;
  int bodystart;

  if (!strcmp(sc->name,"item"))
    comp->bch.itemfno = fno;

  comp->finfo[NUM_BUILTINS+sc->index].address = array_count(comp->instructions);
  comp->finfo[NUM_BUILTINS+sc->index].arity = sc->nargs;

  namestr = real_scname(src,sc->name);
  comp->finfo[NUM_BUILTINS+sc->index].name = add_string(comp,namestr);
  free(namestr);

  oldsi = comp->si;
  comp->si = stackinfo_new(NULL);

  if (sc->nospark) {
    /* For supercombinators created during the non-strict expression lifting stage, assume
       the arguments have already been sparked. This should be handled by the expression which
       calls this supercombinator. */
    for (i = 0; i < sc->nargs; i++) {
      if (sc->strictin[i])
        pushstatus(comp->si,STATUS_SPARKED);
      else
        pushstatus(comp->si,STATUS_UNKNOWN);
    }
    GLOBSTART(sc->sl,fno,sc->nargs);
    comp->finfo[NUM_BUILTINS+sc->index].addressne = array_count(comp->instructions);
    comp->finfo[NUM_BUILTINS+sc->index].addressed = array_count(comp->instructions);
  }
  else {
    /* Otherwise, evaluate all parameters that we know will be needed. It's not worth sparking
       these here, since the sparking will be done at the call site (unless the function call
       is a partial application, which only happens rarely). */
    for (i = 0; i < sc->nargs; i++)
      pushstatus(comp->si,STATUS_UNKNOWN);

    GLOBSTART(sc->sl,fno,sc->nargs);
    comp->finfo[NUM_BUILTINS+sc->index].addressne = array_count(comp->instructions);
    for (i = 0; i < sc->nargs; i++)
      if (sc->strictin && sc->strictin[i])
        EVAL(sc->sl,i);
    comp->finfo[NUM_BUILTINS+sc->index].addressed = array_count(comp->instructions);
  }
  bodystart = array_count(comp->instructions);

#ifdef DEBUG_BYTECODE_COMPILATION
  printf("\n");
  printf("Compiling supercombinator ");
  print_scomb_code(src,stdout,sc);
  printf("\n");
#endif

  pm.names = stack_new();
  pm.indexes = stack_new();
  for (i = 0; i < sc->nargs; i++) {
    stack_push(pm.names,sc->argnames[i]);
    stack_push(pm.indexes,(void*)(sc->nargs-i));
  }

  R(src,comp,copy,&pm,sc->nargs);
  peephole(comp,bodystart);

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
  pushstatus(comp->si,STATUS_UNKNOWN);
  BEGIN(startsc->sl);
  MKFRAME(startsc->sl,startsc->index+NUM_BUILTINS,1);
  SPARK(startsc->sl,0);
  EVAL(startsc->sl,0);
  END(startsc->sl);
  stackinfo_free(comp->si);

  comp->bch.erroraddr = array_count(comp->instructions);
  comp->si = stackinfo_new(NULL);
  pushstatus(comp->si,STATUS_UNKNOWN);
  ERROR(nosl);
  stackinfo_free(comp->si);
  comp->si = NULL;
}

static void compile_evaldo(compilation *comp, source *src)
{
  int ninstrs = array_count(comp->instructions);
  int maxcount = -1;
  int addr;
  int i;
  sourceloc nosl;
  nosl.fileno = -1;
  nosl.lineno = -1;
  for (addr = 0; addr < ninstrs; addr++) {
    instruction *instr = &array_item(comp->instructions,addr,instruction);
    if ((-1 == maxcount) || (maxcount < instr->expcount))
      maxcount = instr->expcount;
  }

  comp->bch.evaldoaddr = ninstrs;
  for (i = 1; i < maxcount; i++) {
    int j;
    comp->si = stackinfo_new(NULL);
    for (j = 0; j < i; j++)
      pushstatus(comp->si,STATUS_UNKNOWN);
    EVAL(nosl,0);
    DO(nosl,0);
    stackinfo_free(comp->si);
    assert(array_count(comp->instructions) == comp->bch.evaldoaddr+i*EVALDO_SEQUENCE_SIZE);
  }
  comp->si = NULL;
}

static void compile_scombs(compilation *comp, source *src)
{
  int scno;
  int count = array_count(src->scombs);
  for (scno = 0; scno < count; scno++) {
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
      pushstatus(comp->si,STATUS_UNKNOWN);

    comp->finfo[i].address = array_count(comp->instructions);
    comp->finfo[i].addressne = array_count(comp->instructions);
    comp->finfo[i].addressed = array_count(comp->instructions);
    comp->finfo[i].arity = bi->nargs;
    comp->finfo[i].name = add_string(comp,bi->name);
    GLOBSTART(nosl,i,bi->nargs);

    if (B_IF == i) {
      int label1;
      stackinfo *oldsi;
      int label2;
      PUSH(nosl,0);
      SPARK(nosl,0);
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
      SPARK(nosl,0);
      RETURN(nosl);
    }
    else {
      for (argno = 0; argno < bi->nstrict; argno++)
        SPARK(nosl,argno);
      BIF(nosl,i);
      if (!bi->reswhnf)
        SPARK(nosl,0);
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
  int count = array_count(src->parsedfiles);
  int strcount;
  sourceloc nosl;
  nosl.fileno = -1;
  nosl.lineno = -1;

  memcpy(comp->bch.signature,"NREDUCE BYTECODE",16);
  comp->instructions = array_new(sizeof(instruction),0);
  comp->si = NULL;
  comp->stringmap = array_new(sizeof(char*),0);
  comp->cdepth = -1;

  assert(0 == array_count(comp->stringmap));
  for (i = 0; i < count; i++) {
    char *filename = array_item(src->parsedfiles,i,char*);
    add_string(comp,filename);
  }

  comp->bch.nfunctions = NUM_BUILTINS+array_count(src->scombs);
  comp->finfo = (funinfo*)calloc(comp->bch.nfunctions,sizeof(funinfo));

  compile_prologue(comp,src);
  compile_scombs(comp,src);
  compile_builtins(comp);
  compile_evaldo(comp,src);
  INVALID(nosl);

  compute_stacksizes(comp);

  gen_bytecode(comp,bcdata,bcsize);

  strcount = array_count(comp->stringmap);
  for (i = 0; i < strcount; i++)
    free(array_item(comp->stringmap,i,char*));
  array_free(comp->stringmap);

  free(comp->finfo);
  array_free(comp->instructions);
  free(comp);
}

void bc_print(const char *bcdata, FILE *f, source *src, int builtins, int *usage)
{
  int i;
  int addr;
  int fno = -1;
  const bcheader *bch = (const bcheader*)bcdata;
  const instruction *instructions = bc_instructions(bcdata);
  const funinfo *finfo = bc_funinfo(bcdata);

  fprintf(f,"nops = %d\n",bch->nops);
  fprintf(f,"nfunctions = %d\n",bch->nfunctions);
  fprintf(f,"nstrings = %d\n",bch->nstrings);
  fprintf(f,"evaldoaddr = %d\n",bch->evaldoaddr);
  fprintf(f,"erroraddr = %d\n",bch->erroraddr);

  fprintf(f,"\n");
  fprintf(f,"Ops:\n");
  fprintf(f,"\n");
  fprintf(f,"%8s %-12s %-12s %-12s %s\n",
          "address","opcode","arg0","arg1","fileno/lineno");
  fprintf(f,"%8s %-12s %-12s %-12s %s\n",
          "-------","------","----","----","-------------");
  for (addr = 0; addr < bch->nops; addr++) {
    const instruction *instr = &instructions[addr];
    const char *filename = (0 <= instr->fileno) ? bc_string(bcdata,instr->fileno) : "_";

    if ((OP_GLOBSTART == instr->opcode) && (fno != instr->arg0)) {
      if (NUM_BUILTINS <= instr->arg0) {
        fprintf(f,"\n");
        if (src) {
          scomb *sc = get_scomb_index(src,instr->arg0-NUM_BUILTINS);
          print_scomb_code(src,f,sc);
          fprintf(f,"\n");
        }
        else {
          fprintf(f,"Function: %s\n",bc_function_name(bcdata,instr->arg0));
        }
      }
      else if (!builtins) {
        break;
      }
      else {
        fprintf(f,"\n");
        fprintf(f,"Builtin: %s\n",builtin_info[instr->arg0].name);
      }
      fprintf(f,"\n");
      fno = instr->arg0;
    }

/*     fprintf(f,"%-3d/",instr->expcount); */
/*     fprintf(f,"%4d %-12s %-12d %-12d",addr,opcodes[instr->opcode],instr->arg0,instr->arg1); */
/*     fprintf(f,"%s:%d",filename,instr->lineno); */

    fprintf(f,"%-3d/%4d %-12s",instr->expcount,addr,opcodes[instr->opcode]);
    if (OP_PUSHNUMBER == instr->opcode) {
      char str[100];
      format_double(str,100,*((double*)&instr->arg0));
      fprintf(f," %-25s",str);
    }
    else if ((OP_MKFRAME == instr->opcode) ||
             (OP_CALL == instr->opcode) ||
             (OP_MKCAP == instr->opcode) ||
             (OP_JFUN == instr->opcode)) {
      fprintf(f," %-18s %-6d",bc_function_name(bcdata,instr->arg0),instr->arg1);
    }
    else if (OP_BIF == instr->opcode) {
      fprintf(f," %-25s",builtin_info[instr->arg0].name);
    }
    else if (OP_JCMP == instr->opcode) {
      fprintf(f," %-18d %-6s",instr->arg0,builtin_info[instr->arg1].name);
    }
    else if ((OP_PUSH == instr->opcode) ||
             (OP_EVAL == instr->opcode) ||
             (OP_SPARK == instr->opcode)) {
#if 0
      int done = 0;
      if (src && (NUM_BUILTINS <= fno)) {
        scomb *sc = get_scomb_index(src,fno-NUM_BUILTINS);
        assert(sc);
        if (instr->arg0 < sc->nargs) {
          fprintf(f," %-25s",sc->argnames[sc->nargs-1-instr->arg0]);
          done = 1;
        }
      }
      if (!done)
#endif
        fprintf(f," %-18d %-6d",instr->arg0,instr->arg1);
    }
    else {
      fprintf(f," %-18d %-6d",instr->arg0,instr->arg1);
    }
    fprintf(f," %s:%d",filename,instr->lineno);
    fprintf(f,"\n");
  }


  fprintf(f,"\n");
  fprintf(f,"Functions:\n");
  fprintf(f,"\n");
  fprintf(f,"%-8s %-8s %-8s %-8s %-8s %s\n","fno","address","noeval","arity","stacksize","name");
  fprintf(f,"%-8s %-8s %-8s %-8s %-8s %s\n","---","-------","------","-----","---------","----");
  for (fno = 0; fno < bch->nfunctions; fno++) {
    const funinfo *fi = &finfo[fno];
    const char *name = bc_function_name(bcdata,fno);
    fprintf(f,"%-8d %-8d %-8d %-8d %-8d %s\n",
            fno,fi->address,fi->addressne,fi->arity,fi->stacksize,name);
  }
  fprintf(f,"\n");
  fprintf(f,"Strings:\n");
  fprintf(f,"\n");
  for (i = 0; i < bch->nstrings; i++) {
    fprintf(f,"%4d: ",i);
    fprintf(f,"\"");
    print_escaped(f,bc_string(bcdata,i));
    fprintf(f,"\"\n");
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
  if (0 > fno)
    return "(unknown)";
  else
    return bc_string(bcdata,finfo[fno].name);
}
