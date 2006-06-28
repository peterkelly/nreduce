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

void print_comp(char *fname, cell *c)
{
#ifdef DEBUG_GCODE_COMPILATION
  debug(cdepth,"%s [ ",fname);
  print_code(c);
  printf(" ]\n");
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

void gprogram_add_ginstr(gprogram *gp, stackinfo *si, int opcode, int arg0, int arg1)
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
    setstatusat(si,si->count-1,builtin_info[arg0].reswhnf);
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
  gprogram_add_ginstr(gp,si,OP_PUSH,d-p(pm,name),(int)strdup(name));
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
    gprogram_add_ginstr(gp,si,OP_PUSHGLOBAL,(int)globcells[fno],0);
  else
    gprogram_add_ginstr(gp,si,OP_PUSHCELL,(int)globcells[fno],0);
}

void C(gprogram *gp, cell *c, int d, pmap *pm, stackinfo *si);
void E(gprogram *gp, cell *c, int d, pmap *pm, stackinfo *si);

void Cletrec(gprogram *gp, cell *c, int d, pmap *pm, stackinfo *si)
{
  cell *lnk;
  int n = 1;

  for (lnk = (cell*)c->field1; lnk; lnk = (cell*)lnk->field2)
    n++;

  gprogram_add_ginstr(gp,si,OP_ALLOC,n,0);
  for (lnk = (cell*)c->field1; lnk; lnk = (cell*)lnk->field2) {
    cell *def = (cell*)lnk->field1;
    C(gp,(cell*)def->field2,d,pm,si);
    gprogram_add_ginstr(gp,si,OP_UPDATE,d+1-p(pm,(char*)def->field1),0);
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

void C(gprogram *gp, cell *c, int d, pmap *pm, stackinfo *si)
{
  print_comp("C",c);
  cdepth++;
  assert(TYPE_IND != celltype(c));
  switch (celltype(c)) {
  case TYPE_APPLICATION: {
    C(gp,(cell*)c->field2,d,pm,si);
    C(gp,(cell*)c->field1,d+1,pm,si);
    gprogram_add_ginstr(gp,si,OP_MKAP,1,0);
/*     gprogram_add_ginstr(gp,si,OP_ANNOTATE,(int)source->filename,source->lineno); */
    break;
  }
  case TYPE_LAMBDA:
    break;
  case TYPE_BUILTIN:
    gprogram_add_ginstr(gp,si,OP_PUSHCELL,(int)globcells[(int)c->field1],0);
    break;
  case TYPE_CONS:
    assert(!"cons shouldn't occur in a supercombinator body (or should it?...)");
    break;
  case TYPE_NIL:
    gprogram_add_ginstr(gp,si,OP_PUSHCELL,(int)globnil,0);
    break;
  case TYPE_INT:
  case TYPE_DOUBLE:
  case TYPE_STRING:
    c->tag |= FLAG_PINNED;
    gprogram_add_ginstr(gp,si,OP_PUSHCELL,(int)c,0);
    break;
  case TYPE_SYMBOL:
    push_varref(gp,(char*)c->field1,d,pm,si);
    break;
  case TYPE_SCREF:
    push_scref(gp,(scomb*)c->field1,d,pm,si);
    break;
  case TYPE_LETREC: {
    pmap pprime;
    int top = d+1;
    int ndefs = 0;
    int dprime = 0;

    Xr(c,pm,d,&pprime,&dprime,&ndefs);
    Cletrec(gp,c,dprime,&pprime,si);
    C(gp,(cell*)c->field2,dprime,&pprime,si);

    gprogram_add_ginstr(gp,si,OP_UPDATE,dprime+1-top,0);
    gprogram_add_ginstr(gp,si,OP_POP,ndefs,0);

    free(pprime.varnames);
    free(pprime.indexes);
    break;
  }
  default:
    assert(0);
    break;
  }
  cdepth--;
}

/* ES [ E ] p d n
 *
 * Completes the evaluation of an expression, the top n ribs of which have 
 * already been put on the stack.
 *
 * ES constructs instances of the ribs of E, putting them on the stack, and
 * then completes the evaluation in the same way as E.
 */
void ES(gprogram *gp, cell *c, int d, pmap *pm, int n, stackinfo *si)
{
/*   print_comp("ES",c); */
  int oldcount = stackcount;
  assert(TYPE_IND != celltype(c));

  cdepth++;
  switch (celltype(c)) {
  case TYPE_NIL:
  case TYPE_INT:
  case TYPE_DOUBLE:
  case TYPE_STRING: // E[I]
    c->tag |= FLAG_PINNED;
    gprogram_add_ginstr(gp,si,OP_PUSHCELL,(int)c,0);
    gprogram_add_ginstr(gp,si,OP_MKAP,n,0);
    gprogram_add_ginstr(gp,si,OP_EVAL,0,0);
    break;
  case TYPE_SYMBOL: // E[x]
    push_varref(gp,(char*)c->field1,d,pm,si);
    gprogram_add_ginstr(gp,si,OP_MKAP,n,0);
    gprogram_add_ginstr(gp,si,OP_EVAL,0,0);
    break;
  case TYPE_SCREF: // E[f]
    push_scref(gp,(scomb*)c->field1,d,pm,si);
    gprogram_add_ginstr(gp,si,OP_MKAP,n,0);
    gprogram_add_ginstr(gp,si,OP_EVAL,0,0);
    break;
  case TYPE_BUILTIN: // another form of E[f]
    gprogram_add_ginstr(gp,si,OP_PUSHCELL,(int)globcells[(int)c->field1],0);
    gprogram_add_ginstr(gp,si,OP_MKAP,n,0);
    gprogram_add_ginstr(gp,si,OP_EVAL,0,0);
    break;
  case TYPE_APPLICATION: {
    cell *fun = c;
    int nargs;

    while (TYPE_APPLICATION == celltype(fun)) {
      push((cell*)fun->field2);
      fun = (cell*)fun->field1;
      assert(TYPE_IND != celltype(fun));
    }
    nargs = stackcount-oldcount;

    if (TYPE_BUILTIN == celltype(fun)) {
      int bif = (int)fun->field1;

      if (0) {}
      else if ((B_IF == bif) && (3 == nargs)) {
        cell *ef = stack[stackcount-3];
        cell *et = stack[stackcount-2];
        cell *ec = stack[stackcount-1];
        int jfalseaddr;
        int jendaddr;
        stackinfo *tempsi;

        E(gp,ec,d,pm,si);
        jfalseaddr = gp->count;
        gprogram_add_ginstr(gp,si,OP_JFALSE,0,0);

        tempsi = stackinfo_new(si);
        ES(gp,et,d,pm,n,tempsi);
        jendaddr = gp->count;
        gprogram_add_ginstr(gp,tempsi,OP_JUMP,0,0);
        stackinfo_free(tempsi);

        gp->ginstrs[jfalseaddr].arg0 = gp->count-jfalseaddr;
        ES(gp,ef,d,pm,n,si);

        gp->ginstrs[jendaddr].arg0 = gp->count-jendaddr;
        break;
      }
      else if (nargs == builtin_info[bif].nargs) {
        int i;
        for (i = 0; i < nargs; i++) {
          if (i < builtin_info[bif].nstrict)
            E(gp,stack[stackcount-nargs+i],d+i,pm,si);
          else
            C(gp,stack[stackcount-nargs+i],d+i,pm,si);
        }
        gprogram_add_ginstr(gp,si,OP_BIF,bif,0);
        gprogram_add_ginstr(gp,si,OP_MKAP,n,0);
        gprogram_add_ginstr(gp,si,OP_EVAL,0,0);
        break;
      }
    }

#ifdef ALLSTRICT
    else if (TYPE_SCREF == celltype(fun)) {
      scomb *sc = (scomb*)fun->field1;
/*       assert(sc); */
      if (sc && (nargs == sc->nargs)) {
        int i;
        for (i = 0; i < nargs; i++)
          E(gp,stack[stackcount-nargs+i],d+i,pm,si);
        gprogram_add_ginstr(gp,si,OP_CALL,NUM_BUILTINS+sc->index,0);
        gprogram_add_ginstr(gp,si,OP_MKAP,n,0);
        gprogram_add_ginstr(gp,si,OP_EVAL,0,0);
        break;
      }
    }
#endif

    if (c->tag & FLAG_STRICT)
      E(gp,(cell*)c->field2,d,pm,si);
    else
      C(gp,(cell*)c->field2,d,pm,si);
    ES(gp,(cell*)c->field1,d+1,pm,n+1,si);
    break;
  }
  case TYPE_LETREC:
    assert(!"ES cannot encounter a letrec");
    break;
  default:
    assert(0);
    break;
  }
  cdepth--;
  stackcount = oldcount;
}

void E(gprogram *gp, cell *c, int d, pmap *pm, stackinfo *si)
{
/*   print_comp("E",c); */
  int oldcount = stackcount;
  assert(TYPE_IND != celltype(c));

  cdepth++;
  switch (celltype(c)) {
  case TYPE_NIL:
  case TYPE_INT:
  case TYPE_DOUBLE:
  case TYPE_STRING: // E[I]
    c->tag |= FLAG_PINNED;
    gprogram_add_ginstr(gp,si,OP_PUSHCELL,(int)c,0);
    break;
  case TYPE_SYMBOL: // E[x]
    push_varref(gp,(char*)c->field1,d,pm,si);
    gprogram_add_ginstr(gp,si,OP_EVAL,0,0);
    break;
  case TYPE_SCREF: // E[f]
    push_scref(gp,(scomb*)c->field1,d,pm,si);
    gprogram_add_ginstr(gp,si,OP_EVAL,0,0);
    break;
  case TYPE_BUILTIN: // another form of E[f]
    gprogram_add_ginstr(gp,si,OP_PUSHCELL,(int)globcells[(int)c->field1],0);
    break;
  case TYPE_APPLICATION: {
    cell *fun = c;
    int nargs;

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

        E(gp,arg2,d,pm,si);
        jfalseaddr = gp->count;
        gprogram_add_ginstr(gp,si,OP_JFALSE,0,0);

        tempsi = stackinfo_new(si);
        E(gp,arg1,d,pm,tempsi);
        jendaddr = gp->count;
        gprogram_add_ginstr(gp,tempsi,OP_JUMP,0,0);
        stackinfo_free(tempsi);

        gp->ginstrs[jfalseaddr].arg0 = gp->count-jfalseaddr;
        E(gp,arg0,d,pm,si);

        gp->ginstrs[jendaddr].arg0 = gp->count-jendaddr;
        break;
      }
      else if (nargs == builtin_info[bif].nargs) {
        int i;
        for (i = 0; i < nargs; i++) {
          if (i < builtin_info[bif].nstrict)
            E(gp,stack[stackcount-nargs+i],d+i,pm,si);
          else
            C(gp,stack[stackcount-nargs+i],d+i,pm,si);
        }
        gprogram_add_ginstr(gp,si,OP_BIF,bif,0);
        if (!builtin_info[bif].reswhnf)
          gprogram_add_ginstr(gp,si,OP_EVAL,0,0);
        break;
      }
    }
#ifdef ALLSTRICT
    else if (TYPE_SYMBOL == celltype(fun)) {
      scomb *sc = (scomb*)fun->field1;
/*       assert(sc); */
      if (sc && (nargs == sc->nargs)) {
        int i;
        for (i = 0; i < nargs; i++)
          E(gp,stack[stackcount-nargs+i],d+i,pm,si);
        gprogram_add_ginstr(gp,si,OP_CALL,NUM_BUILTINS+sc->index,0);
        gprogram_add_ginstr(gp,si,OP_EVAL,0,0);
        break;
      }
    }
#endif

#ifdef ESOPT
    ES(gp,c,d,pm,0,si);
#else
    C(gp,c,d,pm,si);
    gprogram_add_ginstr(gp,si,OP_EVAL,0,0);
#endif
    break;
  }
  default:
    assert(0);
    break;
  }
  cdepth--;
  stackcount = oldcount;
}

/* RS [ E ] p d n
 *
 * Completes a supercombinator reduction, in which the top n ribs of the body have
 * already been put on the stack
 *
 * RS constructs instances of the ribs of E, putting them on the stack, and then
 * completes the reduction in the same way as R.
 */
void RS(gprogram *gp, cell *c, int d, pmap *pm, int n, stackinfo *si)
{
/*   print_comp("RS",c); */
  int oldcount = stackcount;
  assert(TYPE_IND != celltype(c));

  cdepth++;
  switch (celltype(c)) {
  case TYPE_NIL:
  case TYPE_INT:
  case TYPE_DOUBLE:
  case TYPE_STRING:
    // note: book mentions constant shouldn't appear here, as that would mean
    // it was being applied to something
    c->tag |= FLAG_PINNED;
    gprogram_add_ginstr(gp,si,OP_PUSHCELL,(int)c,0);
    gprogram_add_ginstr(gp,si,OP_UPDATE,d-n+1,0);
    gprogram_add_ginstr(gp,si,OP_POP,d-n,0);
    gprogram_add_ginstr(gp,si,OP_UNWIND,0,0);
    break;
  case TYPE_SYMBOL: // RS[x] p d n
    push_varref(gp,(char*)c->field1,d,pm,si);
    gprogram_add_ginstr(gp,si,OP_MKAP,n,0);
    gprogram_add_ginstr(gp,si,OP_UPDATE,d-n+1,0);
    gprogram_add_ginstr(gp,si,OP_POP,d-n,0);
    gprogram_add_ginstr(gp,si,OP_UNWIND,0,0);
    break;
  case TYPE_SCREF: // RS[f] p d n
    push_scref(gp,(scomb*)c->field1,d,pm,si);
    gprogram_add_ginstr(gp,si,OP_MKAP,n,0);
    gprogram_add_ginstr(gp,si,OP_UPDATE,d-n+1,0);
    gprogram_add_ginstr(gp,si,OP_POP,d-n,0);
    gprogram_add_ginstr(gp,si,OP_UNWIND,0,0);
    break;
  case TYPE_BUILTIN: // another form of R[f] p d
    gprogram_add_ginstr(gp,si,OP_PUSHCELL,(int)globcells[(int)c->field1],0);
    gprogram_add_ginstr(gp,si,OP_MKAP,n,0);
    gprogram_add_ginstr(gp,si,OP_UPDATE,d-n+1,0);
    gprogram_add_ginstr(gp,si,OP_POP,d-n,0);
    gprogram_add_ginstr(gp,si,OP_UNWIND,0,0);
    break;
  case TYPE_CONS:
    assert(!"cons shouldn't occur in a supercombinator body (or should it?...)");
    break;
  case TYPE_APPLICATION: {
    cell *fun = c;
    int nargs;

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
        stackinfo *tempsi;

        E(gp,arg2,d,pm,si);
        jfalseaddr = gp->count;
        gprogram_add_ginstr(gp,si,OP_JFALSE,0,0);

        tempsi = stackinfo_new(si);
        RS(gp,arg1,d,pm,n,tempsi);
        stackinfo_free(tempsi);

        gp->ginstrs[jfalseaddr].arg0 = gp->count-jfalseaddr;
        RS(gp,arg0,d,pm,n,si);
        break;
      }
      else if (nargs == builtin_info[bif].nargs) {
        int i;
        for (i = 0; i < nargs; i++) {
          if (i < builtin_info[bif].nstrict)
            E(gp,stack[stackcount-nargs+i],d+i,pm,si);
          else
            C(gp,stack[stackcount-nargs+i],d+i,pm,si);
        }
        gprogram_add_ginstr(gp,si,OP_BIF,bif,0);
        if (!builtin_info[bif].reswhnf)
          gprogram_add_ginstr(gp,si,OP_EVAL,0,0);
        gprogram_add_ginstr(gp,si,OP_MKAP,n,0);
        gprogram_add_ginstr(gp,si,OP_UPDATE,d-n+1,0);
        gprogram_add_ginstr(gp,si,OP_POP,d-n,0);
        gprogram_add_ginstr(gp,si,OP_UNWIND,0,0);
        break;
      }
    }
#ifdef ALLSTRICT
    else if (TYPE_SYMBOL == celltype(fun)) {
      scomb *sc = (scomb*)fun->field1;
/*       assert(sc); */
      if (sc && (nargs == sc->nargs)) {
        int i;
        for (i = 0; i < nargs; i++)
          E(gp,stack[stackcount-nargs+i],d+i,pm,si);
        gprogram_add_ginstr(gp,si,OP_CALL,NUM_BUILTINS+sc->index,0);
        gprogram_add_ginstr(gp,si,OP_EVAL,0,0);
        gprogram_add_ginstr(gp,si,OP_MKAP,n,0);
        gprogram_add_ginstr(gp,si,OP_UPDATE,d-n+1,0);
        gprogram_add_ginstr(gp,si,OP_POP,d-n,0);
        gprogram_add_ginstr(gp,si,OP_UNWIND,0,0);
        break;
      }
    }
#endif

    if (c->tag & FLAG_STRICT)
      E(gp,(cell*)c->field2,d,pm,si);
    else
      C(gp,(cell*)c->field2,d,pm,si);
    RS(gp,(cell*)c->field1,d+1,pm,n+1,si);
    break;
  }
  case TYPE_LETREC:
    assert(!"RS cannot encounter a letrec");
    break;
  }
  cdepth--;
  stackcount = oldcount;
}




/* R [ E ] p d
 *
 * Generates code to apply a supercombinator to its arguments
 *
 * Note: there are d arguments
 */
void R(gprogram *gp, cell *c, int d, pmap *pm, stackinfo *si)
{
/*   print_comp("R",c); */
  int oldcount = stackcount;
  assert(TYPE_IND != celltype(c));

  cdepth++;
  switch (celltype(c)) {
  case TYPE_NIL:
  case TYPE_INT:
  case TYPE_DOUBLE:
  case TYPE_STRING: // R[I] p d
    c->tag |= FLAG_PINNED;
    gprogram_add_ginstr(gp,si,OP_PUSHCELL,(int)c,0);
    gprogram_add_ginstr(gp,si,OP_UPDATE,d+1,0);
    gprogram_add_ginstr(gp,si,OP_POP,d,0);
    gprogram_add_ginstr(gp,si,OP_RETURN,0,0);
    break;
  case TYPE_SYMBOL: // R[x] p d
    push_varref(gp,(char*)c->field1,d,pm,si);
    gprogram_add_ginstr(gp,si,OP_EVAL,0,0);
    gprogram_add_ginstr(gp,si,OP_UPDATE,d+1,0);
    gprogram_add_ginstr(gp,si,OP_POP,d,0);
    gprogram_add_ginstr(gp,si,OP_UNWIND,0,0);
    break;
  case TYPE_SCREF: // R[f] p d
    push_scref(gp,(scomb*)c->field1,d,pm,si);
    gprogram_add_ginstr(gp,si,OP_EVAL,0,0); // could be a CAF
    gprogram_add_ginstr(gp,si,OP_UPDATE,d+1,0);
    gprogram_add_ginstr(gp,si,OP_POP,d,0);
    gprogram_add_ginstr(gp,si,OP_UNWIND,0,0);
    break;
  case TYPE_BUILTIN: // another form of R[f] p d
    gprogram_add_ginstr(gp,si,OP_PUSHCELL,(int)globcells[(int)c->field1],0);
    gprogram_add_ginstr(gp,si,OP_EVAL,0,0); // is this necessary?
    gprogram_add_ginstr(gp,si,OP_UPDATE,d+1,0);
    gprogram_add_ginstr(gp,si,OP_POP,d,0);
    gprogram_add_ginstr(gp,si,OP_UNWIND,0,0);
    break;
  case TYPE_CONS:
    assert(!"cons shouldn't occur in a supercombinator body (or should it?...)");
    break;
  case TYPE_APPLICATION: {
    cell *fun = c;
    int nargs;

    while (TYPE_APPLICATION == celltype(fun)) {
      push((cell*)fun->field2);
      fun = (cell*)fun->field1;
      assert(TYPE_IND != celltype(fun));
    }
    nargs = stackcount-oldcount;

    if (TYPE_BUILTIN == celltype(fun)) {
      int bif = (int)fun->field1;
      /* R [ IF Ec Et Ef ] p d */
      if ((B_IF == bif) && (3 == nargs)) {
        cell *ef = stack[stackcount-3];
        cell *et = stack[stackcount-2];
        cell *ec = stack[stackcount-1];
        int jfalseaddr;
        stackinfo *tempsi;

        E(gp,ec,d,pm,si);
        jfalseaddr = gp->count;
        gprogram_add_ginstr(gp,si,OP_JFALSE,0,0);

        tempsi = stackinfo_new(si);
        R(gp,et,d,pm,tempsi);
        stackinfo_free(tempsi);

        gp->ginstrs[jfalseaddr].arg0 = gp->count-jfalseaddr;
        R(gp,ef,d,pm,si);
        break;
      }
      /* R [ NEG E ] p d */
      /* R [ CONS E1 E2 ] p d */
      /* R [ HEAD E ] p d */
      else if (nargs == builtin_info[bif].nargs) {
        int i;
        for (i = 0; i < nargs; i++) {
          if (i < builtin_info[bif].nstrict)
            E(gp,stack[stackcount-nargs+i],d+i,pm,si); /* e.g. CONS */
          else
            C(gp,stack[stackcount-nargs+i],d+i,pm,si);
        }
        gprogram_add_ginstr(gp,si,OP_BIF,bif,0);
        if (!builtin_info[bif].reswhnf)
          gprogram_add_ginstr(gp,si,OP_EVAL,0,0);
        gprogram_add_ginstr(gp,si,OP_UPDATE,d+1,0);
        gprogram_add_ginstr(gp,si,OP_POP,d,0);
        gprogram_add_ginstr(gp,si,OP_RETURN,0,0);
        break;
      }
    }
#ifdef ALLSTRICT
    else if (TYPE_SYMBOL == celltype(fun)) {
      scomb *sc = (scomb*)fun->field1;
/*       assert(sc); */
      if (sc && (nargs == sc->nargs)) {
        int i;
        for (i = 0; i < nargs; i++)
          E(gp,stack[stackcount-nargs+i],d+i,pm,si);
        gprogram_add_ginstr(gp,si,OP_CALL,NUM_BUILTINS+sc->index,0);
        gprogram_add_ginstr(gp,si,OP_EVAL,0,0);
        gprogram_add_ginstr(gp,si,OP_UPDATE,d+1,0);
        gprogram_add_ginstr(gp,si,OP_POP,d,0);
        gprogram_add_ginstr(gp,si,OP_RETURN,0,0);
        break;
      }
    }
#endif

    /* R [ E1 E2 ] */
#ifdef RSOPT
    RS(gp,c,d,pm,0,si);
#else
    C(gp,c,d,pm,si);
    gprogram_add_ginstr(gp,si,OP_UPDATE,d+1,0);
    gprogram_add_ginstr(gp,si,OP_POP,d,0);
    gprogram_add_ginstr(gp,si,OP_UNWIND,0,0);
#endif
    break;
  }
  case TYPE_LETREC: {
    pmap pprime;
    int ndefs = 0;
    int dprime = 0;

    Xr(c,pm,d,&pprime,&dprime,&ndefs);
    Cletrec(gp,c,dprime,&pprime,si);
    R(gp,(cell*)c->field2,dprime,&pprime,si);

    free(pprime.varnames);
    free(pprime.indexes);
    break;
  }
  }
  cdepth--;
  stackcount = oldcount;
}

void F(gprogram *gp, int fno, scomb *sc)
{
  pmap pm;
  int i;
  cell *copy = super_to_letrec(sc);
  stackinfo *si = stackinfo_new(NULL);
  gprogram_add_ginstr(gp,si,OP_GLOBSTART,fno,sc->nargs);

  pushstatus(si,0); /* redex */
  for (i = 0; i < sc->nargs; i++) {
    /* FIXME: add evals here */
    pushstatus(si,0);
  }

  for (i = 0; i < sc->nargs; i++) {
    if (sc->strictin && sc->strictin[i])
      gprogram_add_ginstr(gp,si,OP_EVAL,i,0);
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

  R(gp,copy,sc->nargs,&pm,si);

/*   int d = sc->nargs; */
/*   C(gp,copy,d,&pm,0,si,1,0); */

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

  gprogram_add_ginstr(gp,topsi,OP_BEGIN,0,0);
  gprogram_add_ginstr(gp,topsi,OP_PUSHGLOBAL,(int)globcells[prog_scomb->index+NUM_BUILTINS],1);
  evaladdr = gp->count;;
  gprogram_add_ginstr(gp,topsi,OP_EVAL,0,0);
  gprogram_add_ginstr(gp,topsi,OP_PUSH,0,0);
  gprogram_add_ginstr(gp,topsi,OP_ISTYPE,TYPE_CONS,0);
  jfalseaddr = gp->count;
  gprogram_add_ginstr(gp,topsi,OP_JFALSE,0,0);

  stackinfo *tempsi = NULL; /* stackinfo_new(topsi); */
  gprogram_add_ginstr(gp,tempsi,OP_PUSH,0,0);
  gprogram_add_ginstr(gp,tempsi,OP_BIF,B_HEAD,0);
  gprogram_add_ginstr(gp,tempsi,OP_PUSH,1,0);
  gprogram_add_ginstr(gp,tempsi,OP_BIF,B_TAIL,0);
  gprogram_add_ginstr(gp,tempsi,OP_UPDATE,2,0);
  gprogram_add_ginstr(gp,tempsi,OP_JUMP,evaladdr-gp->count,0);
/*   stackinfo_free(tempsi); */

  gp->ginstrs[jfalseaddr].arg0 = gp->count-jfalseaddr;
  gprogram_add_ginstr(gp,topsi,OP_PRINT,0,0);
  jemptyaddr = gp->count;
  gprogram_add_ginstr(gp,topsi,OP_JEMPTY,0,0);
  gprogram_add_ginstr(gp,topsi,OP_JUMP,evaladdr-gp->count,0);

  gp->ginstrs[jemptyaddr].arg0 = gp->count-jemptyaddr;
  gprogram_add_ginstr(gp,topsi,OP_END,0,0);

  for (i = 0; i < NUM_BUILTINS; i++) {
    int argno;
    const builtin *bi = &builtin_info[i];
    stackinfo *si = stackinfo_new(NULL);

    pushstatus(si,0); /* redex */
    for (argno = 0; argno < bi->nargs; argno++)
      pushstatus(si,0);

    addressmap[i] = gp->count;
    globcells[i]->field2 = (void*)gp->count;
    gprogram_add_ginstr(gp,si,OP_GLOBSTART,i,bi->nargs);

    if (B_CONS == i) {
      gprogram_add_ginstr(gp,si,OP_BIF,B_CONS,0);
      gprogram_add_ginstr(gp,si,OP_UPDATE,1,0);
      gprogram_add_ginstr(gp,si,OP_RETURN,0,0);
    }
    else if (B_IF == i) {
      int jfalseaddr;
      int jmpaddr;

      gprogram_add_ginstr(gp,si,OP_PUSH,0,0);
      gprogram_add_ginstr(gp,si,OP_EVAL,0,0);
      jfalseaddr = gp->count;
      gprogram_add_ginstr(gp,si,OP_JFALSE,0,0);

      tempsi = stackinfo_new(si);
      gprogram_add_ginstr(gp,tempsi,OP_PUSH,1,0);
      jmpaddr = gp->count;
      gprogram_add_ginstr(gp,tempsi,OP_JUMP,0,0);
      stackinfo_free(tempsi);

      /* label l1 */
      gp->ginstrs[jfalseaddr].arg0 = gp->count-jfalseaddr;
      gprogram_add_ginstr(gp,si,OP_PUSH,2,0);

      /* label l2 */
      gp->ginstrs[jmpaddr].arg0 = gp->count-jmpaddr;
      gprogram_add_ginstr(gp,si,OP_EVAL,0,0);
      gprogram_add_ginstr(gp,si,OP_UPDATE,4,0);
      gprogram_add_ginstr(gp,si,OP_POP,3,0);
      gprogram_add_ginstr(gp,si,OP_UNWIND,0,0);
    }
    else if (B_HEAD == i) {
      gprogram_add_ginstr(gp,si,OP_EVAL,0,0);
      gprogram_add_ginstr(gp,si,OP_BIF,B_HEAD,0);
      gprogram_add_ginstr(gp,si,OP_EVAL,0,0);
      gprogram_add_ginstr(gp,si,OP_UPDATE,1,0);
      gprogram_add_ginstr(gp,si,OP_UNWIND,0,0);
    }
    else if (B_TAIL == i) {
      gprogram_add_ginstr(gp,si,OP_EVAL,0,0);
      gprogram_add_ginstr(gp,si,OP_BIF,B_TAIL,0);
      gprogram_add_ginstr(gp,si,OP_EVAL,0,0);
      gprogram_add_ginstr(gp,si,OP_UPDATE,1,0);
      gprogram_add_ginstr(gp,si,OP_UNWIND,0,0);
    }
    else {
/* FIXME: this is the new version */
      for (argno = 0; argno < bi->nstrict; argno++)
        gprogram_add_ginstr(gp,si,OP_EVAL,argno,0);
      gprogram_add_ginstr(gp,si,OP_BIF,i,0);
      gprogram_add_ginstr(gp,si,OP_UPDATE,1,0);
      if (bi->reswhnf) // result is known to be in WHNF and not a function
        gprogram_add_ginstr(gp,si,OP_RETURN,0,0);
      else
        gprogram_add_ginstr(gp,si,OP_UNWIND,0,0);

/* old version */
/*       for (argno = bi->nargs-1; argno >= bi->nstrict; argno--) { */
/*         gprogram_add_ginstr(gp,si,OP_PUSH,bi->nargs-1,0); */
/*       } */
/*       for (; argno >= 0; argno--) { */
/*         gprogram_add_ginstr(gp,si,OP_PUSH,bi->nargs-1,0); */
/*         gprogram_add_ginstr(gp,si,OP_EVAL,0,0); */
/*       } */

/*       gprogram_add_ginstr(gp,si,OP_BIF,i,0); */
/*       gprogram_add_ginstr(gp,si,OP_UPDATE,bi->nargs+1,0); */
/*       gprogram_add_ginstr(gp,si,OP_POP,bi->nargs,0); */
/*       gprogram_add_ginstr(gp,si,OP_UNWIND,0,0); */




    }

    stackinfo_free(si);
  }

  for (sc = scombs; sc; sc = sc->next) {
    addressmap[NUM_BUILTINS+sc->index] = gp->count;
    globcells[NUM_BUILTINS+sc->index]->field2 = (void*)gp->count;
    clearflag_scomb(FLAG_PROCESSED,sc);
    F(gp,sc->index+NUM_BUILTINS,sc);
  }

  gprogram_add_ginstr(gp,topsi,OP_LAST,0,0);
/*   stackinfo_free(topsi); */

  cur_program = gp;
}

