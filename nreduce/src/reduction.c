#include "grammar.tab.h"
#include "nreduce.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <assert.h>
#include <ctype.h>
#include <stdarg.h>
#include <math.h>

int insdepth = 0;

int resolve_var(int oldcount, scomb *sc, cell **k)
{
  int varno;
  cell *var = *k;

  assert(TYPE_IND != celltype(var)); /* var comes from the scomb */

  if (TYPE_VARIABLE != celltype(var))
    return 0;
  varno = get_scomb_var(sc,(char*)var->field1);
/*   assert(NULL == var->field2); */

  if (0 <= varno) {
    int stackpos = oldcount-1-varno;
    assert(0 <= stackpos);
    *k = (cell*)stackat(stackpos);
    return 1;
  }
  return 0;
}

void inschild(int oldcount, scomb *sc, int base, cell **dt, cell *source)
{
  cell *oldsource;
  assert(TYPE_IND != celltype(source)); /* source comes from the scomb */

  oldsource = source;

  insdepth++;

  resolve_var(oldcount,sc,&source);
  if (oldsource != source) {
    (*dt) = source;
  }
  else if (source->tag & FLAG_PROCESSED) {
#if 0
    int b;
    int found = 0;
    for (b = oldcount; b < base; b += 2) {
      cell *d = stackat(b);
      cell *s = stackat(b+1);
      assert(s->tag & FLAG_PROCESSED);
      if (s == source) {
        (*dt) = d;
        found = 1;
        break;
      }
    }
    assert(found);
#else
    assert(source->dest);
    (*dt) = source->dest;
#endif
  }
  else {
    (*dt) = alloc_cell();
    insert(source,base);
    insert((*dt),base);
    source->dest = (*dt);
  }
  insdepth--;
}

void instantiate_scomb(cell *dest, cell *source, scomb *sc)
{
  int oldcount = stackcount;
  int base = stackcount;
  assert(TYPE_IND != celltype(source)); /* source comes from the scomb */
  assert(TYPE_EMPTY != celltype(source));

  insdepth++;
  push(dest);
  push(source);
  source->dest = dest;
  assert(sc->cells);
  clearflag_scomb(FLAG_PROCESSED,sc);
  while (base < stackcount) {
    int wasarg;

    dest = stackat(base++);

    assert(TYPE_IND != celltype(stack[base])); /* stack[base] comes from the scomb */
    wasarg = resolve_var(oldcount,sc,&stack[base]);
    source = stackat(base++);
    assert(TYPE_IND != celltype(source));

    source->tag |= FLAG_PROCESSED;
    dest->source = source;

    if (wasarg) {
      free_cell_fields(dest);
      dest->tag = TYPE_IND;
      dest->field1 = source;
    }
    else {

      switch (celltype(source)) {
      case TYPE_IND:
        assert(0);
        break;
      case TYPE_APPLICATION:
      case TYPE_CONS:
        free_cell_fields(dest);
        dest->tag = celltype(source);

        inschild(oldcount,sc,base,(cell**)(&dest->field1),(cell*)source->field1);
        inschild(oldcount,sc,base,(cell**)(&dest->field2),(cell*)source->field2);

        break;
      case TYPE_LAMBDA:
        free_cell_fields(dest);
        dest->tag = TYPE_LAMBDA;
        dest->field1 = strdup((char*)source->field1);
        inschild(oldcount,sc,base,(cell**)(&dest->field2),(cell*)source->field2);
        break;
      case TYPE_BUILTIN:
      case TYPE_NIL:
      case TYPE_INT:
      case TYPE_DOUBLE:
      case TYPE_STRING:
      case TYPE_VARIABLE:
      case TYPE_SCREF:
        copy_cell(dest,source);
        break;
      default:
        printf("instantiate_scomb: unknown cell type %d\n",celltype(source));
        assert(0);
        break;
      }
    }
  }

  stackcount = oldcount;
  insdepth--;
}






























void instantiate_scomb2a(cell *dest, cell *source, scomb *sc);

void inschild2(int oldcount, scomb *sc, int base, cell **dt, cell *source)
{
  cell *oldsource;
  assert(TYPE_IND != celltype(source)); /* source comes from the scomb */

  oldsource = source;


  resolve_var(oldcount,sc,&source);
  if (oldsource != source) {
    (*dt) = source;
  }
  else if (source->tag & FLAG_PROCESSED) {
    assert(source->dest);
    (*dt) = source->dest;
  }
  else {
    (*dt) = alloc_cell();
    instantiate_scomb2a(*dt,source,sc);
  }
}





void instantiate_scomb2a(cell *dest, cell *source, scomb *sc)
{
  int oldcount = stackcount;
  int base = stackcount;
  assert(TYPE_IND != celltype(source)); /* source comes from the scomb */
  assert(TYPE_EMPTY != celltype(source));

  source->dest = dest;

  int wasarg = resolve_var(oldcount,sc,&source);
  assert(TYPE_IND != celltype(source));

  source->tag |= FLAG_PROCESSED;
  dest->source = source;

  if (wasarg) {
    free_cell_fields(dest);
    dest->tag = TYPE_IND;
    dest->field1 = source;
  }
  else {

    switch (celltype(source)) {
    case TYPE_IND:
      assert(0);
      break;
    case TYPE_APPLICATION:
    case TYPE_CONS:
      free_cell_fields(dest);
      dest->tag = celltype(source);

      inschild2(oldcount,sc,base,(cell**)(&dest->field1),(cell*)source->field1);
      inschild2(oldcount,sc,base,(cell**)(&dest->field2),(cell*)source->field2);

      break;
    case TYPE_LAMBDA:
      free_cell_fields(dest);
      dest->tag = TYPE_LAMBDA;
      dest->field1 = strdup((char*)source->field1);
      inschild2(oldcount,sc,base,(cell**)(&dest->field2),(cell*)source->field2);
      break;
    case TYPE_BUILTIN:
    case TYPE_NIL:
    case TYPE_INT:
    case TYPE_DOUBLE:
    case TYPE_STRING:
    case TYPE_VARIABLE:
    case TYPE_SCREF:
      copy_cell(dest,source);
      break;
    default:
      printf("instantiate_scomb2a: unknown cell type %d\n",celltype(source));
      assert(0);
      break;
    }
  }

}




void instantiate_scomb2(cell *dest, cell *source, scomb *sc)
{
  int i;
  assert(sc->cells);
  clearflag_scomb(FLAG_PROCESSED,sc);
  for (i = 0; i < sc->ncells; i++)
    sc->cells[i]->dest = NULL; /* shouldn't be strictly necessary */
  instantiate_scomb2a(dest,source,sc);
}

























void instantiate(cell *dest, cell *source, char *varname, cell *varval, int indent, int *base,
                 int *processed)
{
/*   int i; */
  assert(0);
/*   source = resolve_ind(source); */
  assert(TYPE_IND != celltype(source));

/*   printf("source %p",source); */
/*   printf(" %s\n",cell_types[celltype(source)]); */

  insdepth++;
/*   debug(insdepth,"%p <- %p(%s)\n",dest,source,cell_types[celltype(source)]); */


  /* has this source been processed yet? */
  if (source->tag & FLAG_PROCESSED) {
#if 0
    for (i = *base; i < stackcount; i += 2) {
/*       debug(insdepth,"  check %p == %p ?\n",stack[i],source); */
      cell *maybesource = resolve_ind(stack[i+1]);
      assert(TYPE_IND != celltype(maybesource));
      if (maybesource == source) {
/*         printf("adding ind "); */
/*         print_code(stack[i]); */
/*         printf("\n"); */
        dest->tag = TYPE_IND;
        dest->field1 = stack[i];
        return;
      }
    }
    assert(0);
#else
    assert(source->dest);
    dest->tag = TYPE_IND;
    dest->field1 = source->dest;
    return;
#endif
  }

/*   push(dest); */
/*   push(source); */

  source->dest = dest;

  (*processed)++;

  source->tag |= FLAG_PROCESSED;

  switch (source->tag & TAG_MASK) {
  case TYPE_APPLICATION:
  case TYPE_CONS:
    dest->tag = (source->tag & TAG_MASK);
    dest->field1 = alloc_cell();
    dest->field2 = alloc_cell();
    instantiate((cell*)dest->field1,source->field1,varname,varval,indent+1,base,processed);
    instantiate((cell*)dest->field2,source->field2,varname,varval,indent+1,base,processed);
    break;
  case TYPE_LAMBDA:
    if (!strcmp((char*)source->field1,varname)) {
      copy_cell(dest,source);
    }
    else {
      dest->tag = (source->tag & TAG_MASK);
      dest->field2 = alloc_cell();
      instantiate((cell*)dest->field2,source->field2,varname,varval,indent+1,base,processed);
      dest->field1 = strdup((char*)source->field1);
    }
    break;
  case TYPE_BUILTIN:
  case TYPE_NIL:
  case TYPE_INT:
  case TYPE_DOUBLE:
  case TYPE_STRING:
    copy_cell(dest,source);
    break;
  case TYPE_VARIABLE:
    if (!strcmp((char*)source->field1,varname))
      copy_cell(dest,varval);
    else
      copy_cell(dest,source);
    break;
  case TYPE_SCREF:
    copy_cell(dest,source);
    break;
  default:
    assert(0);
    break;
  }
  insdepth--;
}



void instantiate1(cell *dest, cell *source, char *varname, cell *varval, int indent)
{
  int base = stackcount;
  int oldcount = stackcount;
  int processed = 0;

/*   printf("instantiate1(#%05d,%s) to #%05d\n",source->id,varname,dest->id); */
/*   print("",source,0); */

  clearflag(FLAG_PROCESSED);

  instantiate(dest,source,varname,varval,indent,&base,&processed);
/*   printf("instantiate() processed %d cells\n",processed); */
  stackcount = oldcount;
}

void too_many_arguments(cell *target)
{
  cell *source = resolve_source(target);
  if (source && source->filename)
    fprintf(stderr,"%s:%d: ",source->filename,source->lineno);

  while (TYPE_APPLICATION == celltype(source))
    source = resolve_source(resolve_ind((cell*)source->field1));
  if (TYPE_VARIABLE == celltype(source))
    fprintf(stderr,"%s: Too many arguments\n",(char*)source->field1);
  else if (TYPE_SCREF == celltype(source))
    fprintf(stderr,"%s: Too many arguments\n",((scomb*)source->field1)->name);
  else if (TYPE_BUILTIN == celltype(source))
    fprintf(stderr,"%s: Too many arguments\n",builtin_info[(int)source->field1].name);
  else if (TYPE_NIL == celltype(source))
    fprintf(stderr,"nil: Can't apply parameters to a value\n");
  else if (TYPE_INT == celltype(source))
    fprintf(stderr,"%d: Can't apply parameters to a value\n",(int)source->field1);
  else if (TYPE_DOUBLE == celltype(source))
    fprintf(stderr,"%f: Can't apply parameters to a value\n",celldouble(source));
  else if (TYPE_STRING == celltype(source))
    fprintf(stderr,"%s: Can't apply parameters to a value\n",(char*)source->field1);
  else {
    fprintf(stderr,"%s: Too many arguments, or application to a data object\n",
            cell_types[celltype(source)]);
  }
}

cell *temptop = NULL;
int abstractint = 0;

void reduce()
{
  int reductions = 0;

  #ifdef DEBUG_REDUCTION
  if (temptop) {
    printf("reduce() ");
    print_code(temptop);
    printf("\n");
  }
  #endif

  /* REPEAT */
  while (1) {
    int oldtop = stackcount;
    cell *target;
    cell *redex;

    nreductions++;


    /* FIXME: if we call collect() here then sometimes the redex gets collected */
    if (nallocs > COLLECT_THRESHOLD)
      collect();

    redex = stack[stackcount-1];
    reductions++;

/*     printf("==================== REDUCTION ===================\n"); */
/*     print_code(global_root); */
/*     printf("\n"); */
/*     print("",global_root,0); */

    target = resolve_ind(redex);

    /* 1. Unwind the spine until something other than an application node is encountered. */
    push(target);

    while (TYPE_APPLICATION == celltype(target)) {
      target = resolve_ind((cell*)target->field1);
      push(target);
    }

/*     printf("redex = #%05d\n",redex->id); */
/*     printf("target = #%05d\n",target->id); */

/*     printf("target %p: ",target); */
/*     print_code(target); */
/*     printf("\n"); */
/*     printf("target is of type %s\n",cell_types[celltype(target)]); */

    /* 2. Examine the cell  at the tip of the spine */
    switch (celltype(target)) {

    /* A variable. This should correspond to a supercombinator, and if it doesn't it means
       something has gone wrong. */
    case TYPE_VARIABLE:
      assert(!"variable encountered during reduction");
      break;
    case TYPE_SCREF: {
      scomb *sc = (scomb*)target->field1;

      int i;
      int destno;
      cell *dest;

      destno = stackcount-1-sc->nargs;
      dest = stackat(destno);

      #ifdef DEBUG_REDUCTION
      printf("Encountered supercombinator %s\n",sc->name);
      #endif

      nscombappls++;

      /* If there are not enough arguments to the supercombinator, we cannot instantiate it.
         The expression is in WHNF, so we can return. */
      if (stackcount-1-oldtop < sc->nargs) {
        #ifdef DEBUG_REDUCTION
        printf("Not enough arguments for supercombinator %s\n",sc->name);
        #endif
        stackcount = oldtop;
        return;
      }

      /* We have enough arguments present to instantiate the supercombinator */
      for (i = stackcount-1; i >= stackcount-sc->nargs; i--) {
        assert(i > oldtop);
        assert(TYPE_APPLICATION == celltype(stackat(i-1)));
        stack[i] = (cell*)stackat(i-1)->field2;
      }

      #ifdef DEBUG_REDUCTION
      printf("Instantiating supercombinator %s\n",sc->name);
      #endif
      instantiate_scomb2(dest,sc->body,sc);

      stackcount = oldtop;
      continue;
    }
    case TYPE_CONS:
    case TYPE_NIL:
    case TYPE_INT:
    case TYPE_DOUBLE:
    case TYPE_STRING:
      /* The item at the tip of the spine is a value; this means the expression is in WHNF.
         If there are one or more arguments "applied" to this value then it's considered an
         error, e.g. caused by an attempt to pass more arguments to a function than it requires. */

      /* HOWEVER, there is a special case when we are doing abstract interpretation. In this case,
         if there was a value passed in for a variable that is treated as a function then we
         will get this here and it will be either a 0 or a 1. In the case of 0, this represents
         a function that definitely never terminates, and we can return a 0. If it's a 1, this
         represents a function that may terminate, so we return a 1. */
      if (1 < stackcount-oldtop) {
        if (abstractint) {
          assert(TYPE_INT == celltype(target));
          assert((0 == (int)target->field1) || (1 == (int)target->field1));
          redex->tag = TYPE_INT;
          redex->field1 = target->field1;
          #ifdef DEBUG_REDUCTION
          printf("Abstract function %d being applied to some arguments\n",(int)target->field1);
          #endif
        }
        else {
          printf("Attempt to apply %d arguments to a value: ",stackcount-oldtop-1);
          print_code(target);
          printf("\n");
          exit(1);
        }
      }

      stackcount = oldtop;
      return;
      /* b. A built-in function. Check the number of arguments available. If there are too few
         arguments the expression is in WHNF so STOP. Otherwise evaluate any arguments required,
         execute the built-in function and overwrite the root of the redex with the result. */
    case TYPE_BUILTIN: {

      int bif = (int)target->field1;
      int reqargs;
      int strictargs;
      int i;
      assert(0 <= bif);
      assert(NUM_BUILTINS > bif);

      reqargs = builtin_info[bif].nargs;
      strictargs = builtin_info[bif].nstrict;


/*       printf("oldtop = %d\n",oldtop); */
/*       printf("stackcount = %d\n",stackcount); */
/*       print_stack(-1,stack,stackcount,0); */


      if (stackcount-1 < reqargs + oldtop) {
        cell *source = resolve_source(target);
        if (source && source->filename)
          fprintf(stderr,"%s:%d: ",source->filename,source->lineno);
        fprintf(stderr,"Built-in function %s requires %d args; have only %d\n",
                builtin_info[bif].name,reqargs,stackcount-1-oldtop);
        exit(1);
      }

      for (i = stackcount-1; i >= stackcount-reqargs; i--) {
        assert(i > oldtop);
        assert(TYPE_APPLICATION == celltype(stackat(i-1)));
        stack[i] = (cell*)stackat(i-1)->field2;
      }

      assert(strictargs <= reqargs);
      for (i = 0; i < strictargs; i++) {
/*         cell *r = stackat(stackcount-1-i); */
/*         reduce(r); */
/*         stack[stackcount-1-i] = resolve_ind(r); */



        push(stack[stackcount-1-i]);
        reduce();
        stack[stackcount-1-i] = pop();
/*         stack[stackcount-1-i] = resolve_ind(stack[stackcount-1-i]); */
      }

      builtin_info[bif].f();

      /* UPDATE */

      stack[stackcount-1] = resolve_ind(stack[stackcount-1]);
      stack[stackcount-1]->source = target;

      free_cell_fields(stack[stackcount-2]);
      stack[stackcount-2]->tag = TYPE_IND;
      stack[stackcount-2]->field1 = stack[stackcount-1];

      stackcount--;
      break;
    }
    case TYPE_LAMBDA: {
      assert(!"lambda expressions no longer supported");
      cell *arg;
      cell *body;
      char *varname;
      if (1 == stackcount-oldtop) { /* no argument */
        stackcount = oldtop;
        return;
      }

      arg = (cell*)stack[stackcount-2]->field2;
      body = (cell*)top()->field2;
      varname = (char*)top()->field1;

/*       printf("source is %p %s\n",body,cell_types[celltype(body)]); */
/*       printf("varname is %s\n",varname); */
      instantiate1(stack[stackcount-2],body,varname,arg,0);

/*       printf("replaced %p as %s\n",stack[stackcount-2], */
/*              cell_types[celltype(stack[stackcount-2])]); */
/*       if (TYPE_LAMBDA == celltype(stack[stackcount-2])) { */
/*         printf("replacement varname is %s\n",(char*)stack[stackcount-2]->field1); */
/*       } */
/*       printf("after instantiation: "); */
/*       print_code(global_root); */
/*       printf("\n"); */

      break;
    }
    default:
      printf("encountered cell #%05d of type %s\n",target->id,cell_types[celltype(target)]);
      assert(0);
      break;
    }

    stackcount = oldtop;
  }
  /* END */
}
