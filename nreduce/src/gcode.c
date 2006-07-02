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

#define MKAP(_a)           add_instruction(gp,OP_MKAP,(_a),0)
#define UPDATE(_a)         add_instruction(gp,OP_UPDATE,(_a),0)
#define POP(_a)            add_instruction(gp,OP_POP,(_a),0)
#define UNWIND()           add_instruction(gp,OP_UNWIND,0,0)
#define EVAL(_a)           add_instruction(gp,OP_EVAL,(_a),0)
#define RETURN()           add_instruction(gp,OP_RETURN,0,0)
#define PUSH(_a)           add_instruction(gp,OP_PUSH,(_a),0)
#define LAST()             add_instruction(gp,OP_LAST,0,0)
#define BIF(_a)            add_instruction(gp,OP_BIF,(_a),0)
#define JUMP(_a)           add_instruction(gp,OP_JUMP,(_a),0)
#define JFALSE(_a)         add_instruction(gp,OP_JFALSE,(_a),0)
#define GLOBSTART(_a,_b)   add_instruction(gp,OP_GLOBSTART,(_a),(_b))
#define END()              add_instruction(gp,OP_END,0,0)
#define JEMPTY(_a)         add_instruction(gp,OP_JEMPTY,(_a),0)
#define PRINT()            add_instruction(gp,OP_PRINT,0,0)
#define ISTYPE(_a)         add_instruction(gp,OP_ISTYPE,(_a),0)
#define PUSHGLOBAL(_a,_b)  add_instruction(gp,OP_PUSHGLOBAL,(_a),(_b))
#define BEGIN()            add_instruction(gp,OP_BEGIN,0,0)
#define CALL(_a)           add_instruction(gp,OP_CALL,(_a),0)
#define ALLOC(_a)          add_instruction(gp,OP_ALLOC,(_a),0)
#define PUSHNIL()          add_instruction(gp,OP_PUSHNIL,0,0)
#define PUSHINT(_a)        add_instruction(gp,OP_PUSHINT,(_a),0)
#define PUSHDOUBLE(_a,_b)  add_instruction(gp,OP_PUSHDOUBLE,(_a),(_b))
#define PUSHSTRING(_a)     add_instruction(gp,OP_PUSHSTRING,(_a),0)
#define CHECKEVAL(_a)      add_instruction(gp,OP_CHECKEVAL,(_a),0)

cell **globcells = NULL;
int *addressmap = NULL;
int *noevaladdressmap = NULL;
int nfunctions = 0;

int op_usage[OP_COUNT];
int cdepth = 0;


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
"DISPATCH",
"PUSHNIL",
"PUSHINT",
"PUSHDOUBLE",
"PUSHSTRING",
"CHECKEVAL",
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

  for (i = 0; i < gp->stringmap->size/sizeof(char*); i++) {
    char *str = ((char**)gp->stringmap->data)[i];
    free(str);
  }
  array_free(gp->stringmap);

  free(gp->ginstrs);
  free(gp);
}

int add_string(gprogram *gp, char *str)
{
  int pos = gp->stringmap->size/sizeof(char*);
  char *copy = strdup(str);
  array_append(gp->stringmap,&copy,sizeof(char*));
  return pos;
}

void add_instruction(gprogram *gp, int opcode, int arg0, int arg1)
{
  ginstr *instr;

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
  if (gp->si) {
    printf(":%d ",gp->si->count);
/*     printf("stackinfo"); */
/*     int i; */
/*     for (i = 0; i < gp->si->count; i++) { */
/*       printf(" %d",gp->si->status[i]); */
/*     } */
/*     printf("\n"); */
  }
  print_ginstr(gp->count-1,instr);
  #endif

  if (!gp->si)
    return;

  switch (opcode) {
  case OP_BEGIN:
    break;
  case OP_END:
    break;
  case OP_GLOBSTART:
    break;
  case OP_EVAL:
    assert(0 <= gp->si->count-1-arg0);
    setstatusat(gp->si,gp->si->count-1-arg0,1);
    break;
  case OP_UNWIND:
    // after UNWIND we don't know what the stack size will be... but since it's always the
    // last instruction this is ok
    break;
  case OP_RETURN:
    // after RETURN we don't know what the stack size will be... but since it's always the
    // last instruction this is ok
    break;
  case OP_PUSH:
    assert(0 <= gp->si->count-1-arg0);
    pushstatus(gp->si,statusat(gp->si,gp->si->count-1-arg0));
    break;
  case OP_PUSHGLOBAL: {
    int fno = arg0;
    cell *src = globcells[fno];
    assert(TYPE_FUNCTION == celltype(src));
    if ((fno >= NUM_BUILTINS) &&
        (0 == get_scomb_index(fno-NUM_BUILTINS)->nargs)) {
      pushstatus(gp->si,0);
    }
    else {
      pushstatus(gp->si,1);
    }
    break;
  }
  case OP_MKAP:
    assert(0 <= gp->si->count-1-arg0);
    popstatus(gp->si,arg0);
    setstatusat(gp->si,gp->si->count-1,0);
    break;
  case OP_UPDATE:
    assert(0 <= gp->si->count-1-arg0);
    setstatusat(gp->si,gp->si->count-1-arg0,statusat(gp->si,gp->si->count-1));
    popstatus(gp->si,1);
    break;
  case OP_POP:
    assert(0 <= gp->si->count-1-arg0);
    popstatus(gp->si,arg0);
    break;
  case OP_LAST:
    break;
  case OP_PRINT:
    popstatus(gp->si,1);
    break;
  case OP_BIF:
    popstatus(gp->si,builtin_info[arg0].nargs-1);
    setstatusat(gp->si,gp->si->count-1,builtin_info[arg0].reswhnf);
    break;
  case OP_JFALSE:
    popstatus(gp->si,1);
    break;
  case OP_JUMP:
    break;
  case OP_JEMPTY:
    break;
  case OP_ISTYPE:
    setstatusat(gp->si,gp->si->count-1,1);
    break;
  case OP_ANNOTATE:
    break;
  case OP_ALLOC: {
    int i;
    for (i = 0; i < arg0; i++)
      pushstatus(gp->si,1);
    break;
  }
  case OP_SQUEEZE:
    assert(0 <= gp->si->count-arg0-arg1);
    memcpy(&gp->si->status[gp->si->count-arg0-arg1],
           &gp->si->status[gp->si->count-arg0],
           arg1);
    gp->si->count -= arg1;
    break;
  case OP_DISPATCH:
    /* FIXME: what to do here? */
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
  case OP_CHECKEVAL:
    assert(statusat(gp->si,gp->si->count-1-arg0));
    break;
  default:
    assert(0);
    break;
  }
}

void print_ginstr(int address, ginstr *instr)
{
  assert(OP_COUNT > instr->opcode);

  printf("%-6d %-12s %-11d %-11d",
         address,op_names[instr->opcode],instr->arg0,instr->arg1);
  if (0 <= instr->expcount)
    printf(" %-11d",instr->expcount);
  else
    printf("            ");
  printf("    ");

  if (OP_GLOBSTART == instr->opcode) {
    if (NUM_BUILTINS > instr->arg0)
      printf("; Builtin %s",builtin_info[instr->arg0].name);
    else
      printf("; Supercombinator %s",get_scomb_index(instr->arg0-NUM_BUILTINS)->name);
  }
  else if (OP_PUSH == instr->opcode) {

    if (0 <= instr->expcount) {
      ginstr *program = instr-address;
      int funstart = address;
      while ((0 <= funstart) && (OP_GLOBSTART != program[funstart].opcode))
        funstart--;
      if ((OP_GLOBSTART == program[funstart].opcode) &&
          (NUM_BUILTINS <= program[funstart].arg0)) {
        scomb *sc = get_scomb_index(program[funstart].arg0-NUM_BUILTINS);
        int stackpos = instr->expcount-instr->arg0-1;
        if ((stackpos > 0) && (stackpos <= sc->nargs))
          printf("; PUSH %s",sc->argnames[sc->nargs-stackpos]);
      }
    }
  }
  else if (OP_PUSHGLOBAL == instr->opcode) {
    if (instr->arg0 < NUM_BUILTINS)
      printf("; PUSHGLOBAL %s",builtin_info[instr->arg0].name);
    else
      printf("; PUSHGLOBAL %s",get_scomb_index(instr->arg0-NUM_BUILTINS)->name);
  }
  else if (OP_BIF == instr->opcode) {
    printf("; %s",builtin_info[instr->arg0].name);
  }
  else if (OP_ISTYPE == instr->opcode) {
    printf("; ISTYPE %s",cell_types[instr->arg0]);
  }
  else if (OP_ANNOTATE == instr->opcode) {
    printf("; ANNOTATE %s:%d",(char*)instr->arg0,instr->arg1);
  }

  printf("\n");
}

void print_program(gprogram *gp, int builtins)
{
  int i;
  for (i = 0; i < gp->count; i++) {
    if (OP_GLOBSTART == gp->ginstrs[i].opcode) {
      if (NUM_BUILTINS <= gp->ginstrs[i].arg0) {
        scomb *sc = get_scomb_index(gp->ginstrs[i].arg0-NUM_BUILTINS);
        printf("\n");
        print_scomb_code(sc);
        printf("\n");
      }
      else if (!builtins) {
        break;;
      }
      printf("\n");
    }
    print_ginstr(i,&gp->ginstrs[i]);
  }

  printf("\n");
  printf("String map:\n");
  for (i = 0; i < gp->stringmap->size/sizeof(char*); i++) {
    char *str = ((char**)gp->stringmap->data)[i];
    printf("%d: ",i);
    print_quoted_string(stdout,str);
    printf("\n");
  }
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

void C(gprogram *gp, cell *c, int d, pmap *pm, int isresult, int needseval, int n);

void Cletrec(gprogram *gp, cell *c, int d, pmap *pm)
{
  cell *lnk;
  int n = 1;

  for (lnk = (cell*)c->field1; lnk; lnk = (cell*)lnk->field2) {

/*     cell *def = (cell*)lnk->field1; */
/*     printf("generated letrec %s = ",(char*)def->field1); */
/*     print_code((cell*)def->field2); */
/*     printf("\n"); */

    n++;
  }

  ALLOC(n);
  for (lnk = (cell*)c->field1; lnk; lnk = (cell*)lnk->field2) {
    cell *def = (cell*)lnk->field1;
    C(gp,(cell*)def->field2,d,pm,0,0,0);
    UPDATE(d+1-p(pm,(char*)def->field1));
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

/**
 * Main compilation scheme. Recursively compiles the code for the given cell, applying
 * optimisations as appropriate
 *
 * @param gp Data structure representing the compiled code
 *
 * @param c The cell to recursively compile
 *
 * @param d Depth of the current stack frame
 *
 * @param p Map from variable name to position from frame start
 *
 * @param isresult If true, then the code compiled for the current cell constitutes
 *                 the results of the function, i.e. there will be no operations performed
 *                 after this. The compiler should therefore generate the instructions
 *                 necessary to complete execution of the current function, either directly
 *                 or as part of a recursive call with isresult also set to true.
 *                 This corresponds to the R and RS schemes in IFPL.
 *
 * @param needseval If true, the caller expects that after executing the code compiled by this
 *                  function, the top element in the stack will be evaluated to WHNF. In the
 *                  simplest case, this simply corresponds to executing an EVAL after the
 *                  rest of the instructions, however in many cases this can be avoided if
 *                  the result of what is compiled for the expression is known to be WHNF.
 *                  This corresponds to the E and ES schemes.
 *
 * @param n         The number of ribs that have already been pushed onto the stack. At the end
 *                  of the generated code sequence, the appropriate number of MKAPs will be
 *                  executed.
 *                  Where n > 0, this corresponds to the RS and ES schemes.
 */
void C(gprogram *gp, cell *c, int d, pmap *pm, int isresult, int needseval, int n)
{
  int resultwhnf = 0;

  print_comp("C",c,d,isresult,needseval,n);
  cdepth++;
  assert(TYPE_IND != celltype(c));
  switch (celltype(c)) {
  case TYPE_APPLICATION: {
    cell *fun = c;
    int nargs = 0;
    while (TYPE_APPLICATION == celltype(fun)) {
      fun = (cell*)fun->field1;
      assert(TYPE_IND != celltype(fun));
      nargs++;
    }

    if (needseval || isresult) {
      /* If the result of the current expression (c) definitely needs to be evaluated, and
         the expression is an application of a function to the correct number of arguments,
         then we can just execute the function directory, instead of building an intermediate
         graph structure. */
      if (TYPE_BUILTIN == celltype(fun)) {
        int bif = (int)fun->field1;

        /* We treat IF as a special case. First we evaluate the first argument, and based on
           that we jump to one code section or the other in order to evaluate the true or
           false branch. The branch called is also compiled with needseval=1, reflecting the
           fact that the result of the if expression is the result of whatever branch is taken. */
        if ((B_IF == bif) && (3 == nargs)) {
          cell *app = c;
          cell *ef = (cell*)app->field2;
          app = (cell*)app->field1;
          cell *et = (cell*)app->field2;
          app = (cell*)app->field1;
          cell *ec = (cell*)app->field2;

          int jfalseaddr;
          int jendaddr;

          /* Compile the code to compute the result of the conditional */
          C(gp,ec,d,pm,0,1,0);

          /* Add a JFALSE instruction to jump past the next bit of code (the true branch) if
             the conditional evaluates to false. Note that we can't give the instruction the
             (relative) address to jump to yet, because it will depend on how many instructions
             are taken up by the true branch. For this reason, we record the current address
             and will update the JFALSE instruction afterwards once we know how many instructions
             it needs to skip. */
          jfalseaddr = gp->count;
          JFALSE(0);

          /* Compile the true branch. Note that it's necessary to temporarily create a new
             stackinfo object here. At each point during the compilation, this records the
             expected stack depth, and because we've just had a JFALSE instruction, the
             expected depth at the point that JFALSE jumps to may be different to the depth
             had the true branch been executed. This can only happen if isresult is true,
             since in this case the VM will return from the current function. Otherwise, the
             stack depth should be the same at the end regardless of which branch is taken. */
          stackinfo *oldsi = gp->si;
          gp->si = stackinfo_new(oldsi);
          C(gp,et,d,pm,isresult,needseval,n);
          jendaddr = gp->count;
          if (!isresult)
            JUMP(0);
          stackinfo_free(gp->si);
          gp->si = oldsi;

          /* Now we know how many instructions the JFALSE needs to skip ahead, and can update
             the instruction we added earlier. */
          gp->ginstrs[jfalseaddr].arg0 = gp->count-jfalseaddr;

          /* Compile the false branch. No temporary stackinfo is used here as in the case of
             isresult = true, it won't be used again anway. */
          C(gp,ef,d,pm,isresult,needseval,n);

          /* We update the JUMP instruction with the address as before. It is *only* safe to do
             this if isresult = false, as this is the only case above where the JUMP instruction
             is added. */
          if (!isresult)
            gp->ginstrs[jendaddr].arg0 = gp->count-jendaddr;

          /* If isresult = true, then set it to false, as the true or false branch (whichever
             is executed) will have taken care of executing the UPDATE/POP/UNWIND instructions.
             We also set n to 0, as if it was > 0 then the true or falsh branch would also have
             executed the MKAP n instruction. */
          isresult = 0;
          n = 0;

          break;
        }

        /* All other built-in functions are handled by this case. If we have the correct number
           of arguments, then we just compile the expressions directly here and then execute
           the appropriate builtin function. Note that, as with IF, this will *only* be done if
           the result of this expression is definitely needed. */
        else if (nargs == builtin_info[bif].nargs) {
          int i;
          cell *app = c;
          for (i = 0; i < nargs; i++) {
            cell *arg = (cell*)app->field2;

            /* Whether or not we compile the expression with needseval = true depends on whether
               the builtin function is strict in that argument. For example, things like + and >
               require both their arguments to be evaluated, but cons does not. */
            if (nargs-1-i < builtin_info[bif].nstrict)
              C(gp,arg,d+i,pm,0,1,0);
            else
              C(gp,arg,d+i,pm,0,0,0);
            app = (cell*)app->field1;
          }

          /* Now execute the builtin function */
          BIF(bif);

          /* If the caller expects the compiled code to return an evaluated expression, add
             an EVAL instruction here. This is only necessary however if we can't guarantee
             that the builtin function returns a WHNF value. Most - such as the arithmetic
             operators - do. But some may not, such as HEAD, which in the case of a list containing
             elements that are yet to be evaluated could return one of these values. */
          if (!builtin_info[bif].reswhnf) {
            EVAL(0);
            if (0 == n)
              needseval = 0;
          }
          resultwhnf = builtin_info[bif].reswhnf;
          break;
        }
      }

    }

    if (isresult || needseval) { // R, RS, E, ES cases
      int evalarg = (c->tag & FLAG_STRICT);
      C(gp,(cell*)c->field2,d,pm,0,evalarg,0);
      C(gp,(cell*)c->field1,d+1,pm,isresult,needseval,n+1);
      isresult = 0;
      needseval = 0;
      n = 0;
    }
    else { // C case
      C(gp,(cell*)c->field2,d,pm,0,0,0);
      C(gp,(cell*)c->field1,d+1,pm,0,0,0);
      MKAP(1);
    }
    break;
  }
  case TYPE_BUILTIN:              PUSHGLOBAL((int)c->field1,0);
                                  resultwhnf = 1;
                                  break;
  case TYPE_NIL:                  PUSHNIL();
                                  resultwhnf = 1;
                                  break;
  case TYPE_INT:                  PUSHINT((int)c->field1);
                                  resultwhnf = 1;
                                  break;
  case TYPE_DOUBLE:               PUSHDOUBLE((int)c->field1,(int)c->field2);
                                  resultwhnf = 1;
                                  break;
  case TYPE_STRING:               PUSHSTRING(add_string(gp,(char*)c->field1));
                                  resultwhnf = 1;
                                  break;
  case TYPE_SYMBOL:               PUSH(d-p(pm,(char*)c->field1));
                                  /* If needseval is true, we know that the value of this
                                     variable *definitely* needs to be evaluated. Let's
                                     evaluate it now before we update the redex, so when it
                                     is used in the future the value will already be there.
                                     This is to improve the performance of supercombinators
                                     with only a variable in their body (IFPL 20.2) */
                                  if ((needseval || isresult) && (0 == n)) {
                                    EVAL(0);
                                    needseval = 0;
                                  }
                                  break;
  case TYPE_SCREF:                PUSHGLOBAL(((scomb*)c->field1)->index+NUM_BUILTINS,0);
                                  /* Evaluate the variable if the result is needed, as for
                                     TYPE_SYMBOL. This is needed in case the supercombinator
                                     is a CAF. */
                                  if ((needseval || isresult) && (0 == n)) {
                                    EVAL(0);
                                    needseval = 0;
                                  }
                                  break;
  case TYPE_LETREC: {
    pmap pprime;
    int top = d+1;
    int ndefs = 0;
    int dprime = 0;

    assert(0 == n);

    Xr(c,pm,d,&pprime,&dprime,&ndefs);
    Cletrec(gp,c,dprime,&pprime);
    C(gp,(cell*)c->field2,dprime,&pprime,isresult,needseval,0);

    if (!isresult) {
      UPDATE(dprime+1-top);
      POP(ndefs);
    }
    isresult = 0; // final instructions already added; don't want to do it here

    free(pprime.varnames);
    free(pprime.indexes);
    break;
  }
  default:
    assert(0);
    break;
  }

  if (0 < n)
    MKAP(n);

  if (needseval && (!resultwhnf || (0 < n)))
    EVAL(0);

  if (isresult) {
    /* We're at the end of the function and should generate the instructions necessary for
       the program to continue execution. The value at the top of the stack is the result
       of the function, and therefore we use UPDATE to overwrite the root of the redex. */
    UPDATE(d-n+1);
    POP(d-n);

    /* We then POP all the stack entries corresponding to the current frame, and execute
       the UNWIND operation to continue reduction of the graph based on the new value in the
       redex. The only exception to this is if the result is known to be in WHNF (e.g. if
       it's an integer or reference to a built-in function) - then we can execute RETURN
       instead, as there is no need to perform any further reductions on the redex. */
    if (resultwhnf)
      RETURN();
    else
      UNWIND();
  }

  cdepth--;
}

void R(gprogram *gp, cell *c, int d, pmap *pm)
{
  C(gp,c,d,pm,1,0,0);
}

void F(gprogram *gp, int fno, scomb *sc)
{
  pmap pm;
  int i;
  cell *copy = super_to_letrec(sc);

  addressmap[NUM_BUILTINS+sc->index] = gp->count;

  stackinfo *oldsi = gp->si;
  gp->si = stackinfo_new(NULL);
  GLOBSTART(fno,sc->nargs);

  pushstatus(gp->si,0); /* redex */
  for (i = 0; i < sc->nargs; i++) {
    /* FIXME: add evals here */
    pushstatus(gp->si,0);
  }

  for (i = 0; i < sc->nargs; i++)
    if (sc->strictin && sc->strictin[i])
      EVAL(i);

/*   for (i = 0; i < sc->nargs; i++) */
/*     if (sc->strictin && sc->strictin[i]) */
/*       CHECKEVAL(i); */


#ifdef DEBUG_GCODE_COMPILATION
  printf("\n");
  printf("Compiling supercombinator ");
  print_scomb_code(sc);
  printf("\n");
#endif

  pm.count = sc->nargs;
  pm.varnames = sc->argnames;
  pm.indexes = (int*)alloca(sc->nargs*sizeof(int));
  for (i = 0; i < sc->nargs; i++)
    pm.indexes[i] = sc->nargs-i;

  R(gp,copy,sc->nargs,&pm);

  stackinfo_free(gp->si);
  gp->si = oldsi;
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
  noevaladdressmap = (int*)calloc(nfunctions,sizeof(int));
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

  BEGIN();
  PUSHGLOBAL(prog_scomb->index+NUM_BUILTINS,0);
  evaladdr = gp->count;;
  EVAL(0);
  PUSH(0);
  ISTYPE(TYPE_CONS);
  jfalseaddr = gp->count;
  JFALSE(0);

  topsi = gp->si;
  PUSH(0);
  BIF(B_HEAD);
  PUSH(1);
  BIF(B_TAIL);
  UPDATE(2);
  JUMP(evaladdr-gp->count);
  gp->si = topsi;

  gp->ginstrs[jfalseaddr].arg0 = gp->count-jfalseaddr;
  PRINT();
  jemptyaddr = gp->count;
  JEMPTY(0);
  JUMP(evaladdr-gp->count);

  gp->ginstrs[jemptyaddr].arg0 = gp->count-jemptyaddr;
  END();

  for (sc = scombs; sc; sc = sc->next) {
    globcells[NUM_BUILTINS+sc->index]->field2 = (void*)gp->count;
    clearflag_scomb(FLAG_PROCESSED,sc);
    F(gp,NUM_BUILTINS+sc->index,sc);
  }

  topsi = gp->si;
  for (i = 0; i < NUM_BUILTINS; i++) {
    int argno;
    const builtin *bi = &builtin_info[i];
    gp->si = stackinfo_new(NULL);

    pushstatus(gp->si,0); /* redex */
    for (argno = 0; argno < bi->nargs; argno++)
      pushstatus(gp->si,0);

    addressmap[i] = gp->count;
    globcells[i]->field2 = (void*)gp->count;
    GLOBSTART(i,bi->nargs);

    if (B_CONS == i) {
      BIF(B_CONS);
      UPDATE(1);
      RETURN();
    }
    else if (B_IF == i) {
      int jfalseaddr;
      int jmpaddr;

      PUSH(0);
      EVAL(0);
      jfalseaddr = gp->count;
      JFALSE(0);

      stackinfo *oldsi = gp->si;
      gp->si = stackinfo_new(oldsi);
      PUSH(1);
      jmpaddr = gp->count;
      JUMP(0);
      stackinfo_free(gp->si);
      gp->si = oldsi;

      /* label l1 */
      gp->ginstrs[jfalseaddr].arg0 = gp->count-jfalseaddr;
      PUSH(2);

      /* label l2 */
      gp->ginstrs[jmpaddr].arg0 = gp->count-jmpaddr;
      EVAL(0);
      UPDATE(4);
      POP(3);
      UNWIND();
    }
    else if (B_HEAD == i) {
      EVAL(0);
      BIF(B_HEAD);
      EVAL(0);
      UPDATE(1);
      UNWIND();
    }
    else if (B_TAIL == i) {
      EVAL(0);
      BIF(B_TAIL);
      EVAL(0);
      UPDATE(1);
      UNWIND();
    }
    else {
      for (argno = 0; argno < bi->nstrict; argno++)
        EVAL(argno);
      BIF(i);
      UPDATE(1);
      if (bi->reswhnf) // result is known to be in WHNF and not a function
        RETURN();
      else
        UNWIND();
    }

    stackinfo_free(gp->si);
  }
  gp->si = topsi;

  LAST();
/*   stackinfo_free(gp->si); */

  cur_program = gp;
}

