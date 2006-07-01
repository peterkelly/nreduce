//#define PRINT_DISPATCH
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

extern cell **globcells;

int get_fno_from_address(int address)
{
  int fno = 0;
  while ((fno < nfunctions) && (addressmap[fno] != address))
    fno++;
  assert(fno < nfunctions);
  return fno;
}

void trace_address(ginstr *program, int address, int nargs)
{
  int fno = get_fno_from_address(address);

  if (trace) {
    debug(0,"\n");
    if (NUM_BUILTINS > fno)
      debug(0,"0      Function %d = %s",fno,builtin_info[fno].name);
    else
      debug(0,"0      Function %d = %s",fno,get_scomb_index(fno-NUM_BUILTINS)->name);
    debug(0,", address %d, nargs = %d, stackbase = %d\n",
          address,nargs,stackbase);
  }
}

void insufficient_args(ginstr *program, int address, cell *target, int nargs)
{
  int fno = get_fno_from_address(address);
  cell *source = resolve_source(target);
  int reqargs = program[address].arg1;
  if (source && source->filename)
    fprintf(stderr,"%s:%d: ",source->filename,source->lineno);
  if (NUM_BUILTINS > fno)
    fprintf(stderr,"Built-in function %s requires %d args; have only %d\n",
            builtin_info[fno].name,reqargs,nargs);
  else
    fprintf(stderr,"Supercombinator %s requires %d args; have only %d\n",
    get_scomb_index(fno-NUM_BUILTINS)->name,reqargs,nargs);
  exit(1);
}

extern int repl_histogram[NUM_CELLTYPES];

int is_whnf(cell *c)
{
  c = resolve_ind(c);
  if ((TYPE_APPLICATION == celltype(c)) ||
      (TYPE_FUNCTION == celltype(c))) {
    int nargs = 0;
    while (TYPE_APPLICATION == celltype(c)) {
      c = resolve_ind((cell*)c->field1);
      nargs++;
    }
    if (TYPE_FUNCTION == celltype(c)) {
      if (nargs >= (int)c->field1)
        return 0;
    }
  }
  return 1;
}

cell *unwind_part1()
{
  stackcount = stackbase+1;
  cell *ebx = stack[stackcount-1];
  ebx = resolve_ind(ebx);
  while (TYPE_APPLICATION == celltype(ebx)) {
    ebx = (cell*)ebx->field1;
    ebx = resolve_ind(ebx);
    push(ebx);
  }
  return ebx;
}

int address = 0;
int exec_stackbase = 0;

int unwind_part2(cell *ebx)
{
  if (TYPE_FUNCTION == celltype(ebx)) {
    int argspos = stackcount-(int)ebx->field1;
    if (argspos >= stackbase+1) {
      int funaddr = (int)ebx->field2;
      int edx;

      assert(!is_whnf(stack[argspos-1]));

      for (edx = stackcount-1; edx >= argspos; edx--) {
        ebx = stack[edx-1];
        ebx = resolve_ind(ebx);

        ebx = (cell*)ebx->field2;
        ebx = resolve_ind(ebx);
        stack[edx] = ebx;
      }

      ebx = stack[argspos-1];
      ebx = resolve_ind(ebx);
      stack[argspos-1] = ebx;

      address = funaddr;
      exec_stackbase = argspos-1;
      return 1;
    }
  }
  return 0;
}

void do_return()
{
  dumpentry *edx = &dumpstack[dumpcount-1];
/**/  if (trace) {
/**/    debug(0,"!!!!! UNWIND: result is in WHNF; dump: next address %d, next stackcount %d\n",
/**/          dumpstack[dumpcount-1].address+1,dumpstack[dumpcount-1].stackcount);
/**/  }

  assert(is_whnf(stack[stackcount-1]));

  stackbase = edx->stackbase;
  stackcount = edx->stackcount;
  address = edx->address;
  exec_stackbase = edx->sb;
  dumpcount--;

  assert(0 <= dumpcount);

  cell *ebx = stack[stackcount-1];
  ebx = resolve_ind(ebx);
}

/*
void alloc_some(cell *redex, int n)
{
  cell *app = alloc_cell();
  free_cell_fields(redex);
  redex->tag = TYPE_IND;
  redex->field1 = app;

  int base = stackcount-1-n;

  app->tag = TYPE_APPLICATION;
  app->field1 = globnil;
  app->field2 = stack[base];

  int i;
  for (i = 1; i < n; i++) {
    cell *newapp = alloc_cell();
    newapp->tag = TYPE_APPLICATION;
    newapp->field1 = globnil;
    newapp->field2 = stack[base+i];
    stack[base+i-1] = newapp;
    app->field1 = newapp;
    app = newapp;
  }
  app->field1 = stack[base+i];
  stack[base+i-1] = stack[base+i];
  stackcount--;
}
*/

void alloc_some(cell *redex, int actual, int n)
{
  cell *app = alloc_cell();
  free_cell_fields(redex);
  redex->tag = TYPE_IND;
  redex->field1 = app;

  int base = stackcount-1-actual;

  app->tag = TYPE_APPLICATION;
  app->field1 = globnil;
  app->field2 = stack[base];

  int i;
  for (i = 1; i < n; i++) {
    cell *newapp = alloc_cell();
    newapp->tag = TYPE_APPLICATION;
    newapp->field1 = globnil;
    newapp->field2 = stack[base+i];
    stack[base+i-1] = newapp;
    app->field1 = newapp;
    app = newapp;
  }
  if (n == actual) {
    app->field1 = stack[base+i];
    stack[base+i-1] = stack[base+i];
    stackcount--;
  }
  else {
    cell *hole = alloc_cell();
/*     hole->tag = TYPE_STRING; */
/*     hole->field1 = strdup("HOLE"); */

    hole->tag = TYPE_NIL;

    app->field1 = hole;
    stack[base+i-1] = hole;
  }
}

void execute(gprogram *gp)
{
  ginstr *program = gp->ginstrs;
  assert(0 == stackbase);
  while (1) {
    ginstr *instr = &program[address];
/*     int chkno; */
/*     int i; */

#ifdef STACK_MODEL_SANITY_CHECK
    if (0 <= instr->expcount) {
/*       printf("stack frame size = %d-%d = %d\n",stackcount,exec_stackbase,stackcount-exec_stackbase); */
      if (stackcount-exec_stackbase != instr->expcount) {
        printf("Instruction %d expects stack frame size %d, actually %d\n",
               address,instr->expcount,stackcount-exec_stackbase);
      }
      assert(stackcount-exec_stackbase == instr->expcount);

      int i;
      for (i = 0; i < instr->expcount; i++) {
        cell *c = stackat(exec_stackbase+i);
/*         int actualstatus = (isvalue(c) || */
/*                             (TYPE_CONS == celltype(c)) || */
/*                             (TYPE_FUNCTION == celltype(c))); */
        int actualstatus = is_whnf(c);

        if (instr->expstatus[i] && !actualstatus) {
          printf("Instruction %d expects stack frame entry %d (%d) to be evald but it's "
                 "actually a %s\n",
                 address,i,exec_stackbase+i,cell_types[celltype(c)]);
        }
        assert(!instr->expstatus[i] || actualstatus);
      }
    }
#endif

    if (trace) {
      print_ginstr(address,instr);
      print_stack(-1,stack,stackcount,0);
    }

/*     for (chkno = 0; chkno < dumpcount; chkno++) { */
/*       if (stackcount <= dumpstack[chkno].stackcount) { */
/*         debug(0,"fatal: stackcount %d is at or below dump entry %d's stackcount %d\n", */
/*               stackcount,chkno,dumpstack[chkno].stackcount); */
/*         assert(0); */
/*       } */
/*       chkno++; */
/*     } */

    if (nallocs > COLLECT_THRESHOLD)
      collect();

    op_usage[instr->opcode]++;

    switch (instr->opcode) {
    case OP_BEGIN:
      break;
    case OP_END:
      return;
    case OP_GLOBSTART:
      break;
    case OP_EVAL: {
      cell *c;
      assert(0 <= stackcount-1-instr->arg0);
      c = resolve_ind(stack[stackcount-1-instr->arg0]);
      if ((TYPE_APPLICATION == celltype(c)) ||
          (TYPE_FUNCTION == celltype(c))) {
        dumpentry *de = pushdump();
        ndumpallocs++;
        de->stackbase = stackbase;
        de->stackcount = stackcount;
        de->address = address;
        de->sb = exec_stackbase;
        push(stack[stackcount-1-instr->arg0]);
        stackbase = stackcount-1;
        exec_stackbase = stackbase;
      }
      else {
        break;
      }
      /* fall through */
    }
    case OP_UNWIND: {
      cell *ebx = unwind_part1();
      if (unwind_part2(ebx))
        break;

      /* fall through */
    }
    case OP_RETURN: {
      do_return();
      break;
    }
    case OP_PUSH: {
      assert(instr->arg0 < stackcount);
      push(resolve_ind(stack[stackcount-1-instr->arg0]));
      break;
    }
    case OP_PUSHGLOBAL: {
      int fno = instr->arg0;
      if ((fno >= NUM_BUILTINS) &&
          (0 == get_scomb_index(fno-NUM_BUILTINS)->nargs)) {
        cell *src = globcells[fno];
        assert(TYPE_FUNCTION == celltype(src));
        cell *c = alloc_cell();
        c->tag = TYPE_FUNCTION;
        c->field1 = src->field1;
        c->field2 = src->field2;
        push(c);
      }
      else {
        push(globcells[fno]);
      }
      break;
    }
    case OP_MKAP: {
      int i;
      for (i = instr->arg0-1; i >= 0; i--) {
        cell *c = alloc_cell();
        c->tag = TYPE_APPLICATION;
        c->field1 = stack[stackcount-1];
        c->field2 = stack[stackcount-2];
        stackcount--;
        stack[stackcount-1] = c;
      }
      break;
    }
    case OP_UPDATE: {
      cell *res;     /* EBX */
      cell *target;  /* ECX */
      assert(instr->arg0 < stackcount);
      assert(instr->arg0 > 0);

      res = resolve_ind(stack[stackcount-1-instr->arg0]);

      if (res->tag & FLAG_PINNED) {
        stack[stackcount-1-instr->arg0] = alloc_cell();
        res = stack[stackcount-1-instr->arg0];
      }

      assert(!(res->tag & FLAG_PINNED));
      free_cell_fields(res);
      target = res;
      repl_histogram[celltype(target)]++;

      target->tag = TYPE_IND;
      target->field1 = resolve_ind(stack[stackcount-1]);
      stackcount--;
      break;
    }
    case OP_POP:
      stackcount -= instr->arg0;
      assert(0 <= stackcount);
      break;
    case OP_PRINT: {
      cell *c = top();
      print_cell(c);
      stackcount--;
      break;
    }
    case OP_BIF: {
      int i;
      assert(0 <= builtin_info[instr->arg0].nargs); /* should we support 0-arg bifs? */
      for (i = 0; i < builtin_info[instr->arg0].nstrict; i++) {
        stack[stackcount-1-i] = resolve_ind(stack[stackcount-1-i]); /* bifs expect it */
        assert(TYPE_APPLICATION != celltype(stack[stackcount-1-i]));
      }
      if (trace)
        debug(0,"  builtin %s\n",builtin_info[instr->arg0].name);
      builtin_info[instr->arg0].f();
      break;
    }
    case OP_JFALSE: {
      cell *test = pop();
      if (TYPE_APPLICATION == celltype(test)) {
        print(test);
      }
      assert(TYPE_APPLICATION != celltype(test));
      if (TYPE_NIL == celltype(test))
        address += instr->arg0-1;
      break;
    }
    case OP_JUMP:
      address += instr->arg0-1;
      break;
    case OP_JEMPTY:
      if (0 == stackcount)
        address += instr->arg0-1;
      break;
    case OP_ISTYPE: {
      if (celltype(top()) == instr->arg0)
        stack[stackcount-1] = globtrue;
      else
        stack[stackcount-1] = globnil;
      break;
    }
    case OP_ANNOTATE: {
      cell *c;
      assert(0 < stackcount);
      c = top();
      c->filename = (char*)instr->arg0;
      c->lineno = instr->arg1;
      break;
    }
    case OP_ALLOC: {
      int i;
      /* FIXME: perhaps we should make a HOLE type instead of using nil here... then it
         should be save to use globnil everywhere we normally use TYPE_NIL */
      nALLOCs++;
      for (i = 0; i < instr->arg0; i++) {
        cell *c = alloc_cell();
        c->tag = TYPE_NIL;
        push(c);
        nALLOCcells++;
      }
      break;
    }
    case OP_SQUEEZE: {
      assert(0 <= stackcount-instr->arg0-instr->arg1);
      int base = stackcount-instr->arg0-instr->arg1;
      int i;
      for (i = 0; i <= instr->arg0; i++)
        stack[base+i] = stack[base+i+instr->arg1];
      stackcount -= instr->arg1;
      break;
    }
    case OP_DISPATCH: {
#if 1
      cell *topelem = resolve_ind(stack[stackcount-1]);
      if (TYPE_FUNCTION == celltype(topelem)) {
/*         int fno = program[(int)topelem->field2].arg0; */
        int expargs = (int)topelem->field1;

        assert(0 < expargs);

        #ifdef PRINT_DISPATCH
        if (NUM_BUILTINS > fno)
          printf("top element %s (expects %d args)",builtin_info[fno].name,expargs);
        else
          printf("top element %s (expects %d args)",
                 get_scomb_index(fno-NUM_BUILTINS)->name,expargs);
        #endif

        if (expargs == instr->arg0) {
          #ifdef PRINT_DISPATCH
          printf(" == DISPATCH %d\n",instr->arg0);
          #endif
          stackcount--;
          address = (int)topelem->field2;
          break;
        }
#if 1
        else if (expargs < instr->arg0) {
/*           printf("\n"); */
          #ifdef PRINT_DISPATCH
          printf(" < DISPATCH %d\n",instr->arg0);
          #endif

          cell *redex = resolve_ind(stack[stackcount-2-instr->arg0]);
          if (redex->tag & FLAG_PINNED) {
            stack[stackcount-2-instr->arg0] = alloc_cell();
            redex = stack[stackcount-2-instr->arg0];
          }

          alloc_some(redex,instr->arg0,instr->arg0-expargs);
          stackcount--;
          address = (int)topelem->field2;
          break;
        }
        else {
          #ifdef PRINT_DISPATCH
          printf("\n");
          #endif
/*           printf(" > DISPATCH %d\n",instr->arg0); */
        }
#endif
      }
#endif

      assert(1 <= instr->arg0);

      cell *redex = resolve_ind(stack[stackcount-2-instr->arg0]);
      if (redex->tag & FLAG_PINNED) {
        stack[stackcount-2-instr->arg0] = alloc_cell();
        redex = stack[stackcount-2-instr->arg0];
      }

      alloc_some(redex,instr->arg0,instr->arg0);




      if (trace) {
        printf("dispatch: after building spine\n");
        print_stack(-1,stack,stackcount,0);
      }
/*       exit(1); */


      cell *ebx = unwind_part1();
      if (unwind_part2(ebx))
        break;
      do_return();
      break;
    }
    case OP_PUSHNIL:
      push(globnil);
      break;
    case OP_PUSHINT: {
      cell *c = alloc_cell();
      c->tag = TYPE_INT;
      c->field1 = (void*)instr->arg0;
      push(c);
      break;
    }
    case OP_PUSHDOUBLE: {
      cell *c = alloc_cell();
      c->tag = TYPE_DOUBLE;
      c->field1 = (void*)instr->arg0;
      c->field2 = (void*)instr->arg1;
      push(c);
      break;
    }
    case OP_PUSHSTRING: {
      char *str = ((char**)gp->stringmap->data)[instr->arg0];
      cell *c = alloc_cell();
      c->tag = TYPE_STRING;
      c->field1 = (void*)strdup(str);
      push(c);
      break;
    }
    default:
      assert(0);
      break;
    }

    address++;
  }
}
