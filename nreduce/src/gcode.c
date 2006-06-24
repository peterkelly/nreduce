//#define USE_DISPATCH

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

#define RSOPT
#define ESOPT
//#define ALLSTRICT

cell **globcells = NULL;
int *addressmap = NULL;
int nfunctions = 0;

int op_usage[OP_COUNT];
int cdepth = 0;


typedef struct stackinfo {
  int alloc;
  int count;
  int *status;
} stackinfo;

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








const char *op_names[OP_COUNT] = {
"BEGIN",
"END",
"GLOBSTART",
"EVAL",
"UNWIND",
"RETURN",
"PUSH",
"PUSHGLOBAL",
"PUSHCELL",
"MKAP",
"UPDATE",
"POP",
"LAST",
"PRINT",
"BIF",
"JFALSE",
"JUMP",
"JEMPTY",
"ISTYPE",
"ANNOTATE",
"ALLOC",
"SQUEEZE",
"DISPATCH"
};

void print_comp(cell *c, int n, int endfun, int doeval)
{
#ifdef DEBUG_GCODE_COMPILATION
  debug(cdepth,"C [ ");
  print_code(c);
  printf(" ]");
  if (0 < n)
    printf(" n=%d",n);
  if (endfun)
    printf(" endfun");
  if (doeval)
    printf(" doeval");
  printf("\n");
#endif
}

gprogram *gprogram_new()
{
  gprogram *gp = (gprogram*)calloc(1,sizeof(gprogram));
  gp->count = 0;
  gp->alloc = 2;
  gp->ginstrs = (ginstr*)malloc(gp->alloc*sizeof(ginstr));
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

  free(gp->ginstrs);
  free(gp);
}

void gprogram_add_ginstr(gprogram *gp, int opcode, int arg0, int arg1, stackinfo *si)
{
  ginstr *instr;

#if 1
  if ((OP_EVAL == opcode) && si && statusat(si,si->count-1-arg0)) {
    /* This assert is just here to see if this functionality is used... for some reason
       it doesn't get triggered. This is actually supposed to be called for some optimizations. */
/*     assert(0); */
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
  if (si) {
    instr->expcount = si->count;
    instr->expstatus = (int*)malloc(si->count*sizeof(int));
    memcpy(instr->expstatus,si->status,si->count*sizeof(int));
  }
  else {
    instr->expcount = -1;
  }

  #ifdef DEBUG_GCODE_COMPILATION
  if (si) {
    printf(":%d ",si->count);
/*     printf("stackinfo"); */
/*     int i; */
/*     for (i = 0; i < si->count; i++) { */
/*       printf(" %d",si->status[i]); */
/*     } */
/*     printf("\n"); */
  }
  print_ginstr(gp->count-1,instr);
  #endif

  if (!si)
    return;

#if 1
  switch (opcode) {
  case OP_BEGIN:
    break;
  case OP_END:
    break;
  case OP_GLOBSTART:
    break;
  case OP_EVAL:
    assert(0 <= si->count-1-arg0);
    setstatusat(si,si->count-1-arg0,1);
    break;
  case OP_UNWIND:
    /* FIXME: what to do here? */
    #ifdef DEBUG_GCODE_COMPILATION
/*     printf("\n\n\n"); */
    #endif
    break;
  case OP_RETURN:
    /* FIXME: what to do here? */
    #ifdef DEBUG_GCODE_COMPILATION
/*     printf("\n\n\n"); */
    #endif
    break;
  case OP_PUSH:
    assert(0 <= si->count-1-arg0);
    pushstatus(si,statusat(si,si->count-1-arg0));
    break;
  case OP_PUSHGLOBAL: {
    cell *src = (cell*)arg0;
    assert(TYPE_FUNCTION == celltype(src));
    if (0 == (int)src->field1)
      pushstatus(si,0);
    else
      pushstatus(si,1);
    break;
  }
  case OP_PUSHCELL:
    pushstatus(si,((cell*)arg0)->tag & FLAG_PINNED);
    break;
  case OP_MKAP:
    assert(0 <= si->count-1-arg0);
    popstatus(si,arg0);
    setstatusat(si,si->count-1,0);
    break;
  case OP_UPDATE:
    assert(0 <= si->count-1-arg0);
    setstatusat(si,si->count-1-arg0,statusat(si,si->count-1));
    popstatus(si,1);
    break;
  case OP_POP:
    assert(0 <= si->count-1-arg0);
    popstatus(si,arg0);
    break;
  case OP_LAST:
    break;
  case OP_PRINT:
    popstatus(si,1);
    break;
  case OP_BIF:
    popstatus(si,builtin_info[arg0].nargs-1);
    setstatusat(si,si->count-1,builtin_info[arg0].strictres);
    break;
  case OP_JFALSE:
    popstatus(si,1);
    break;
  case OP_JUMP:
    break;
  case OP_JEMPTY:
    break;
  case OP_ISTYPE:
    setstatusat(si,si->count-1,1);
    break;
  case OP_ANNOTATE:
    break;
  case OP_ALLOC: {
    int i;
    for (i = 0; i < arg0; i++)
      pushstatus(si,1);
    break;
  }
  case OP_SQUEEZE:
    assert(0 <= si->count-arg0-arg1);
    memcpy(&si->status[si->count-arg0-arg1],
           &si->status[si->count-arg0],
           arg1);
    si->count -= arg1;
    break;
  case OP_DISPATCH:
    /* FIXME: what to do here? */
    break;
  }
#endif

/*   debug(8,""); */
/*   print_ginstr(gp->count-1,&gp->ginstrs[gp->count-1]); */
}

void print_ginstr(int address, ginstr *instr)
{
  assert(OP_COUNT > instr->opcode);

  if (OP_GLOBSTART == instr->opcode)
    printf("\n");

/*   printf("%-6d [s %02d] %-12s %-11d %-11d", */
/*          address,instr->expcount,op_names[instr->opcode],instr->arg0,instr->arg1); */
  printf("%-6d %-12s %-11d %-11d",
         address,op_names[instr->opcode],instr->arg0,instr->arg1);
  printf("    ");

  switch (instr->opcode) {
  case OP_BEGIN:
    break;
  case OP_END:
    break;
  case OP_GLOBSTART: {
    if (NUM_BUILTINS > instr->arg0)
      printf("; Builtin %s",builtin_info[instr->arg0].name);
    else
      printf("; Supercombinator %s",get_scomb_index(instr->arg0-NUM_BUILTINS)->name);
    break;
  }
  case OP_EVAL:
    break;
  case OP_UNWIND:
    break;
  case OP_UPDATE:
    break;
  case OP_PUSH:
    if (instr->arg1)
      printf("; (%s)",(char*)instr->arg1);
    else
      printf("; (unknown)");
    break;
  case OP_PUSHGLOBAL:
    break;
  case OP_PUSHCELL:
    printf("; %-12s #%05d ","PUSHCELL",((cell*)instr->arg0)->id);
    print_code((cell*)instr->arg0);
    assert(TYPE_IND != celltype((cell*)instr->arg0));
    break;
  case OP_MKAP:
    printf("; %-12s %d","MKAP",instr->arg0);
    break;
  case OP_POP:
    break;
  case OP_RETURN:
    break;
  case OP_LAST:
    break;
  case OP_PRINT:
    break;
  case OP_BIF:
    printf("; %s",builtin_info[instr->arg0].name);
    break;
  case OP_JFALSE:
    break;
  case OP_JUMP:
    break;
  case OP_JEMPTY:
    break;
  case OP_ISTYPE:
    printf("; ISTYPE %s",cell_types[instr->arg0]);
    break;
  case OP_ANNOTATE:
    printf("; ANNOTATE %s:%d",(char*)instr->arg0,instr->arg1);
    break;
  case OP_ALLOC:
    break;
  case OP_SQUEEZE:
    break;
  case OP_DISPATCH:
    break;
  default:
    assert(0);
    break;
  }
  printf("\n");
}

void print_program(gprogram *gp)
{
  int i;
  for (i = 0; i < gp->count; i++)
    print_ginstr(i,&gp->ginstrs[i]);
}

typedef struct pmap {
  int count;
  char **varnames;
  int *indexes;
} pmap;

int p(pmap *pm, char *varname)
{
  int i;
  for (i = 0; i < pm->count; i++)
    if (!strcmp(pm->varnames[i],varname))
      return pm->indexes[i];
  printf("unknown variable: %s\n",varname);
  assert(!"unknown variable");
  return -1;
}

void push_varref(gprogram *gp, char *name, int d, pmap *pm, stackinfo *si)
{
  assert(NULL == get_scomb(name));
  gprogram_add_ginstr(gp,OP_PUSH,d-p(pm,name),(int)strdup(name),si);
}

void push_scref(gprogram *gp, scomb *vsc, int d, pmap *pm, stackinfo *si)
{
  scomb *s2 = scombs;
  int fno = NUM_BUILTINS;
  while (s2 != vsc) {
    s2 = s2->next;
    fno++;
  }
  if (0 == s2->nargs)
    gprogram_add_ginstr(gp,OP_PUSHGLOBAL,(int)globcells[fno],0,si);
  else
    gprogram_add_ginstr(gp,OP_PUSHCELL,(int)globcells[fno],0,si);
}

void C(gprogram *gp, cell *c, int d, pmap *pm, int n, stackinfo *si, int endfun, int doeval);
/* void E(gprogram *gp, cell *c, int d, pmap *pm, stackinfo *si); */

void Cletrec(gprogram *gp, cell *c, int d, pmap *pm, stackinfo *si)
{
  cell *lnk;
  int n = 1;

  for (lnk = (cell*)c->field1; lnk; lnk = (cell*)lnk->field2)
    n++;

  gprogram_add_ginstr(gp,OP_ALLOC,n,0,si);
  for (lnk = (cell*)c->field1; lnk; lnk = (cell*)lnk->field2) {
    cell *def = (cell*)lnk->field1;
    C(gp,(cell*)def->field2,d,pm,0,si,0,0);
    gprogram_add_ginstr(gp,OP_UPDATE,d+1-p(pm,(char*)def->field1),0,si);
  }
}

void Xr(cell *c, pmap *pm, int dold, pmap *pprime, int *d2, int *ndefs2)
{
  int ndefs = 0;
  int i = 0;
  cell *lnk;
  int top;

  for (lnk = (cell*)c->field1; lnk; lnk = (cell*)lnk->field2)
    ndefs++;

  *ndefs2 = ndefs;

  pprime->count = pm->count+ndefs;
  pprime->varnames = (char**)malloc(pprime->count*sizeof(char*));
  pprime->indexes = (int*)malloc(pprime->count*sizeof(int));
  memcpy(pprime->varnames,pm->varnames,pm->count*sizeof(char*));
  memcpy(pprime->indexes,pm->indexes,pm->count*sizeof(int));

  *d2 = dold;

  top = ++(*d2); /* __top - to be replaced with supercombinator body */
  for (lnk = (cell*)c->field1; lnk; lnk = (cell*)lnk->field2) {
    cell *def = (cell*)lnk->field1;
    pprime->varnames[pm->count+i] = (char*)def->field1;
    pprime->indexes[pm->count+i] = ++(*d2);
    i++;
  }
}

void C(gprogram *gp, cell *c, int d, pmap *pm, int n, stackinfo *si, int endfun, int doeval)
{
  /* endfun && (0 == n) corresponds to R */
  /* endfun && (0 < n)  corresponds to RS */
  /* doeval && (0 == n) corresponds to E */
  /* doeval && (0 < n)  corresponds to ES */

  int oldcount = stackcount;
  int evaldresult = 0;
  assert(!(endfun && doeval));
  print_comp(c,n,endfun,doeval);
  cdepth++;
  assert(TYPE_IND != celltype(c));
  switch (celltype(c)) {
  case TYPE_APPLICATION: {
    cell *fun = c;
    int nargs;

    if (endfun || doeval) {

      while (TYPE_APPLICATION == celltype(fun)) {
        push((cell*)fun->field2);
        fun = (cell*)fun->field1;
        assert(TYPE_IND != celltype(fun));
      }
      nargs = stackcount-oldcount;

      if (TYPE_BUILTIN == celltype(fun)) {
        int bif = (int)fun->field1;

        if ((B_IF == bif) && (3 == nargs)) {
          cell *arg0 = stack[stackcount-3];
          cell *arg1 = stack[stackcount-2];
          cell *arg2 = stack[stackcount-1];
          int jfalseaddr;
          int jendaddr;
          stackinfo *tempsi;

          C(gp,arg2,d,pm,0,si,0,1);
          jfalseaddr = gp->count;
          gprogram_add_ginstr(gp,OP_JFALSE,0,0,si);

          tempsi = stackinfo_new(si);
          C(gp,arg1,d,pm,0,tempsi,endfun,!endfun);
          jendaddr = gp->count;
          if (!endfun)
            gprogram_add_ginstr(gp,OP_JUMP,0,0,tempsi);
          stackinfo_free(tempsi);

          gp->ginstrs[jfalseaddr].arg0 = gp->count-jfalseaddr;
          tempsi = stackinfo_new(si);
          C(gp,arg0,d,pm,0,tempsi,endfun,!endfun);
          stackinfo_free(tempsi);

          pushstatus(si,1);

          if (!endfun)
            gp->ginstrs[jendaddr].arg0 = gp->count-jendaddr;
          evaldresult = 1;
          break;
        }
        else if (nargs == builtin_info[bif].nargs) {
          int i;
          for (i = 0; i < nargs; i++) {
            if (nargs-i <= builtin_info[bif].nstrict)
              C(gp,stack[stackcount-nargs+i],d+i,pm,0,si,0,1);
            else
              C(gp,stack[stackcount-nargs+i],d+i,pm,0,si,0,0);
          }

          gprogram_add_ginstr(gp,OP_BIF,bif,0,si);

          if (!builtin_info[bif].strictres)
            gprogram_add_ginstr(gp,OP_EVAL,0,0,si);
          evaldresult = 1;
          break;
        }
      }
      else if (TYPE_SCREF == celltype(fun)) {
/*         scomb *sc = (scomb*)fun->field1; */
        /* FIXME */
      }
    }


    if (endfun || doeval) {
/*     if (endfun) { */
      C(gp,(cell*)c->field2,d,pm,0,si,0,doeval);
      C(gp,(cell*)c->field1,d+1,pm,n+1,si,endfun,doeval);
      endfun = 0;
      doeval = 0;
    }
    else {
      C(gp,(cell*)c->field2,d,pm,0,si,0,doeval);
      C(gp,(cell*)c->field1,d+1,pm,0,si,0,doeval);
      gprogram_add_ginstr(gp,OP_MKAP,1,0,si);
    }
    break;
  }
  case TYPE_BUILTIN:
    gprogram_add_ginstr(gp,OP_PUSHCELL,(int)globcells[(int)c->field1],0,si);
#ifdef USE_DISPATCH
    if (endfun && (0 < n)) {
      gprogram_add_ginstr(gp,OP_SQUEEZE,n+1,d-n,si);
      gprogram_add_ginstr(gp,OP_DISPATCH,n,0,si);
      endfun = 0;
    }
#endif
    break;
  case TYPE_NIL:
    assert(0 == n);
    gprogram_add_ginstr(gp,OP_PUSHCELL,(int)globnil,0,si);
    evaldresult = 1;
    break;
  case TYPE_INT:
  case TYPE_DOUBLE:
  case TYPE_STRING:
    assert(0 == n);
    c->tag |= FLAG_PINNED;
    gprogram_add_ginstr(gp,OP_PUSHCELL,(int)c,0,si);
/*     evaldresult = 1; */
    break;
  case TYPE_VARIABLE:
    push_varref(gp,(char*)c->field1,d,pm,si);
#ifdef USE_DISPATCH
    if (endfun && (0 < n)) {
      gprogram_add_ginstr(gp,OP_SQUEEZE,n+1,d-n,si);
      gprogram_add_ginstr(gp,OP_DISPATCH,n,0,si);
      endfun = 0;
    }
#endif
    break;
  case TYPE_SCREF:
    push_scref(gp,(scomb*)c->field1,d,pm,si);
#ifdef USE_DISPATCH
    if (endfun && (0 < n)) {
      gprogram_add_ginstr(gp,OP_SQUEEZE,n+1,d-n,si);
      gprogram_add_ginstr(gp,OP_DISPATCH,n,0,si);
      endfun = 0;
    }
#endif
    break;
  case TYPE_LETREC: {
    assert(0 == n);
    pmap pprime;
    int top = d+1;
    int ndefs = 0;
    int dprime = 0;

    Xr(c,pm,d,&pprime,&dprime,&ndefs);
    Cletrec(gp,c,dprime,&pprime,si);
    C(gp,(cell*)c->field2,dprime,&pprime,0,si,0,0);

    gprogram_add_ginstr(gp,OP_UPDATE,dprime+1-top,0,si);
    gprogram_add_ginstr(gp,OP_POP,ndefs,0,si);

    free(pprime.varnames);
    free(pprime.indexes);
    break;
  }
  default:
    assert(0);
    break;
  }
  cdepth--;

  if (endfun || doeval) {
/*   if (endfun) { */
    if (0 < n) {
      gprogram_add_ginstr(gp,OP_MKAP,n,0,si);
      evaldresult = 0;
    }
  }

  if (doeval) {
    gprogram_add_ginstr(gp,OP_EVAL,0,0,si);
    evaldresult = 1;
  }

  if (endfun) {
    gprogram_add_ginstr(gp,OP_UPDATE,d-n+1,0,si);
    gprogram_add_ginstr(gp,OP_POP,d-n,0,si);
    /* OPT: call RETURN here if we know that the thing we just compiled is a constant */
    if (evaldresult)
      gprogram_add_ginstr(gp,OP_RETURN,0,0,si);
    else
      gprogram_add_ginstr(gp,OP_UNWIND,0,0,si);
  }
  stackcount = oldcount;
}


void F(gprogram *gp, int fno, scomb *sc)
{
  pmap pm;
  int i;
  cell *copy = super_to_letrec(sc);
  stackinfo *si = stackinfo_new(NULL);
  gprogram_add_ginstr(gp,OP_GLOBSTART,fno,sc->nargs,si);

  pushstatus(si,0); /* redex */
  for (i = 0; i < sc->nargs; i++) {
    /* FIXME: add evals here */
    pushstatus(si,0);
  }

  for (i = 0; i < sc->nargs; i++) {
    if (sc->strictin && sc->strictin[i])
      gprogram_add_ginstr(gp,OP_EVAL,i,0,si);
  }

#ifdef DEBUG_GCODE_COMPILATION
  printf("\n");
  printf("Compiling supercombinator %s ",sc->name);
  print_code(sc->body);
  printf("\n");
#endif

  pm.count = sc->nargs;
  pm.varnames = sc->argnames;
  pm.indexes = (int*)alloca(sc->nargs*sizeof(int));
  for (i = 0; i < sc->nargs; i++)
    pm.indexes[i] = sc->nargs-i;

/*   R(gp,copy,sc->nargs,&pm,si); */

  int d = sc->nargs;

  C(gp,copy,d,&pm,0,si,1,0);

  stackinfo_free(si);
}

gprogram *cur_program = NULL;
void compile(gprogram *gp)
{
  scomb *sc;
  int index = 0;
  int i;
  int evaladdr;
  int jfalseaddr;
  int jemptyaddr;
  stackinfo *topsi = NULL; /* stackinfo_new(NULL); */

  if (NULL == prog_scomb) {
    fprintf(stderr,"G-code implementation only works if supercombinators are enabled\n");
    exit(1);
  }

  for (sc = scombs; sc; sc = sc->next)
    sc->index = index++;

  nfunctions = NUM_BUILTINS+index;

  addressmap = (int*)calloc(nfunctions,sizeof(int));
  globcells = (cell**)calloc(nfunctions,sizeof(cell*));

  for (i = 0; i < NUM_BUILTINS; i++) {
    globcells[i] = alloc_cell();
    globcells[i]->tag = TYPE_FUNCTION | FLAG_PINNED;
    globcells[i]->field1 = (void*)builtin_info[i].nargs;
  }

  for (sc = scombs; sc; sc = sc->next, i++) {
    globcells[i] = alloc_cell();
    globcells[i]->tag = TYPE_FUNCTION | FLAG_PINNED;
    globcells[i]->field1 = (void*)sc->nargs;
  }

  gprogram_add_ginstr(gp,OP_BEGIN,0,0,topsi);
  gprogram_add_ginstr(gp,OP_PUSHGLOBAL,(int)globcells[prog_scomb->index+NUM_BUILTINS],1,topsi);
  evaladdr = gp->count;;
  gprogram_add_ginstr(gp,OP_EVAL,0,0,topsi);
  gprogram_add_ginstr(gp,OP_PUSH,0,0,topsi);
  gprogram_add_ginstr(gp,OP_ISTYPE,TYPE_CONS,0,topsi);
  jfalseaddr = gp->count;
  gprogram_add_ginstr(gp,OP_JFALSE,0,0,topsi);

  stackinfo *tempsi = NULL; /* stackinfo_new(topsi); */
  gprogram_add_ginstr(gp,OP_PUSH,0,0,tempsi);
  gprogram_add_ginstr(gp,OP_BIF,B_HEAD,0,tempsi);
  gprogram_add_ginstr(gp,OP_PUSH,1,0,tempsi);
  gprogram_add_ginstr(gp,OP_BIF,B_TAIL,0,tempsi);
  gprogram_add_ginstr(gp,OP_UPDATE,2,0,tempsi);
  gprogram_add_ginstr(gp,OP_JUMP,evaladdr-gp->count,0,tempsi);
/*   stackinfo_free(tempsi); */

  gp->ginstrs[jfalseaddr].arg0 = gp->count-jfalseaddr;
  gprogram_add_ginstr(gp,OP_PRINT,0,0,topsi);
  jemptyaddr = gp->count;
  gprogram_add_ginstr(gp,OP_JEMPTY,0,0,topsi);
  gprogram_add_ginstr(gp,OP_JUMP,evaladdr-gp->count,0,topsi);

  gp->ginstrs[jemptyaddr].arg0 = gp->count-jemptyaddr;
  gprogram_add_ginstr(gp,OP_END,0,0,topsi);

  for (i = 0; i < NUM_BUILTINS; i++) {
    int argno;
    const builtin *bi = &builtin_info[i];
    stackinfo *si = stackinfo_new(NULL);

    pushstatus(si,0); /* redex */
    for (argno = 0; argno < bi->nargs; argno++)
      pushstatus(si,0);

    addressmap[i] = gp->count;
    globcells[i]->field2 = (void*)gp->count;
    gprogram_add_ginstr(gp,OP_GLOBSTART,i,bi->nargs,si);

    if (B_CONS == i) {
      gprogram_add_ginstr(gp,OP_BIF,B_CONS,0,si);
      gprogram_add_ginstr(gp,OP_UPDATE,1,0,si);
      gprogram_add_ginstr(gp,OP_RETURN,0,0,si);
    }
    else if (B_IF == i) {
      int jfalseaddr;
      int jmpaddr;

      gprogram_add_ginstr(gp,OP_PUSH,0,0,si);
      gprogram_add_ginstr(gp,OP_EVAL,0,0,si);
      jfalseaddr = gp->count;
      gprogram_add_ginstr(gp,OP_JFALSE,0,0,si);

      tempsi = stackinfo_new(si);
      gprogram_add_ginstr(gp,OP_PUSH,1,0,tempsi);
      jmpaddr = gp->count;
      gprogram_add_ginstr(gp,OP_JUMP,0,0,tempsi);
      stackinfo_free(tempsi);

      /* label l1 */
      gp->ginstrs[jfalseaddr].arg0 = gp->count-jfalseaddr;
      gprogram_add_ginstr(gp,OP_PUSH,2,0,si);

      /* label l2 */
      gp->ginstrs[jmpaddr].arg0 = gp->count-jmpaddr;
/*       gprogram_add_ginstr(gp,OP_EVAL,0,0,si); */
      gprogram_add_ginstr(gp,OP_UPDATE,4,0,si);
      gprogram_add_ginstr(gp,OP_POP,3,0,si);
      gprogram_add_ginstr(gp,OP_UNWIND,0,0,si);
    }
    else if (B_HEAD == i) {
      gprogram_add_ginstr(gp,OP_EVAL,0,0,si);
      gprogram_add_ginstr(gp,OP_BIF,B_HEAD,0,si);
/*       gprogram_add_ginstr(gp,OP_EVAL,0,0,si); */
      gprogram_add_ginstr(gp,OP_UPDATE,1,0,si);
      gprogram_add_ginstr(gp,OP_UNWIND,0,0,si);
    }
    else if (B_TAIL == i) {
      gprogram_add_ginstr(gp,OP_EVAL,0,0,si);
      gprogram_add_ginstr(gp,OP_BIF,B_TAIL,0,si);
/*       gprogram_add_ginstr(gp,OP_EVAL,0,0,si); */
      gprogram_add_ginstr(gp,OP_UPDATE,1,0,si);
      gprogram_add_ginstr(gp,OP_UNWIND,0,0,si);
    }
    else {
/* FIXME: this is the new version */
      for (argno = 0; argno < bi->nstrict; argno++)
        gprogram_add_ginstr(gp,OP_EVAL,argno,0,si);
      gprogram_add_ginstr(gp,OP_BIF,i,0,si);
      gprogram_add_ginstr(gp,OP_UPDATE,1,0,si);
      gprogram_add_ginstr(gp,OP_UNWIND,0,0,si);


/* old version */
/*       for (argno = bi->nargs-1; argno >= bi->nstrict; argno--) { */
/*         gprogram_add_ginstr(gp,OP_PUSH,bi->nargs-1,0,si); */
/*       } */
/*       for (; argno >= 0; argno--) { */
/*         gprogram_add_ginstr(gp,OP_PUSH,bi->nargs-1,0,si); */
/*         gprogram_add_ginstr(gp,OP_EVAL,0,0,si); */
/*       } */

/*       gprogram_add_ginstr(gp,OP_BIF,i,0,si); */
/*       gprogram_add_ginstr(gp,OP_UPDATE,bi->nargs+1,0,si); */
/*       gprogram_add_ginstr(gp,OP_POP,bi->nargs,0,si); */
/*       gprogram_add_ginstr(gp,OP_UNWIND,0,0,si); */




    }

    stackinfo_free(si);
  }

  for (sc = scombs; sc; sc = sc->next) {
    addressmap[NUM_BUILTINS+sc->index] = gp->count;
    globcells[NUM_BUILTINS+sc->index]->field2 = (void*)gp->count;
    clearflag_scomb(FLAG_PROCESSED,sc);
    F(gp,sc->index+NUM_BUILTINS,sc);
  }

  gprogram_add_ginstr(gp,OP_LAST,0,0,topsi);
/*   stackinfo_free(topsi); */

  cur_program = gp;
}

