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
 * $Id: strictness.c 267 2006-07-06 14:07:53Z pmkelly $
 *
 */

/**
 * \file
 *
 * Strictness analyser
 *
 * The strictness analyser is responsible for determining which arguments a function is "strict"
 * in, i.e. which of its arguments will definitely be evaluated if the function is called,
 * regardless of which code path is taken. This information is important for generating efficient
 * code, as it allows the compiler to evaluate expressions directly in many cases, rather than
 * generating a set of application nodes at run-time.
 *
 * We use a fairly simple analysis procedure here, which will detect obvious cases of strictness.
 * Examples of these cases include:
 * - An argument which is passed to a built-in function which is strict in all its arguments
 *   e.g. f x y = + x y will be strict in both arguments since + definitely requires them to
 *   be evaluated
 * - An argument which is evaluated by the conditional expression of an if statement, or is used
 *   in a strict context in *both* the true and false branches (not just one)
 *   e.g. f a b c d = if (= 0 a) (+ b c) (- c d) will be strict in a and c, but not b or d
 * - An argument which is passed to a supercombinator which is strict in the relevant argument
 *   position, in the same manner as for built-in functions. This relies on the strictness
 *   information having already been discovered in a previous iteration of the analysis process.
 *
 * In the above examples the term "strict context" means an expression that definitely needs to
 * be evaluated. For example, in (head E), the expression E is a strict context and any variables
 * within it must be evaluated if the function call occurs. In (cons E1 E2), neither expression
 * is a strict context, as cons doesn't require either of its arguments to be evaluated.
 *
 * Performing strictness analysis in the general case is a very complex problem, and has been
 * treated extensively in the literature. A more sophisticated algorithm based on abstract
 * interpretation was previously used in nreduce, but this proved too slow for practical usage as
 * its execution time for a given supercombinator was exponential in the number of arguments. For
 * the time being we content ourselves with a faster but less complete method which should catch
 * most of the cases we are interested in.
 *
 * Note that it's safe to assume a function is not strict in an argument when it actually is, but
 * not the other way round. If there is any chance that the value of an argument may not be needed
 * by a function, then it should not be evaluated, as doing so could result in expressions being
 * lazily evaluated when they should not be. This could lead to infinite loops in some cases, e.g.
 * the following function:
 *
 *   f x y = if (x) (len y) 0
 *
 * where nil (false) is passed for x, and y is an infinite list. Lazy evaluation would not cause
 * y to be evaluated in this case (as the false branch is taken), and therefore we would be
 * changing the semantics of evaluation by trying to evaluate (len y) before we call the function.
 * So if there is any doubt about whether a function is strict in a particular argument, we assume
 * that it is not, and thus the G-code compiler would generate MKFRAME/MKCAP instructions instead
 * of evaluating the expression for that argument directly.
 */

#include "nreduce.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <assert.h>
#include <ctype.h>
#include <stdarg.h>
#include <math.h>

/**
 * Determine the number of arguments required by the function referenced by the specified cell.
 *
 * @param c Cell representing a function. Must be either a supercombinator reference or built-in
 *          function
 */
static int fun_nargs(cell *c)
{
  if (TYPE_SCREF == celltype(c))
    return ((scomb*)(c->field1))->nargs;
  else if (TYPE_BUILTIN == celltype(c))
    return builtin_info[(int)c->field1].nargs;
  else
    abort();
}

/**
 * Determine whether or not the function referenced by the cell is strict in the specified argument
 *
 * @param c Cell representing a function. Must be either a supercombinator reference or built-in
 *          function
 */
static int fun_strictin(cell *c, int argno)
{
  assert(argno < fun_nargs(c));
  if (TYPE_SCREF == celltype(c))
    return ((scomb*)(c->field1))->strictin[argno];
  else if (TYPE_BUILTIN == celltype(c))
    return (argno < builtin_info[(int)c->field1].nstrict);
  else
    abort();
}

/**
 * Recursively perform strictness analysis on an expression.
 *
 * This function does two things:
 * - Records which arguments will definitely be evaluated, assuming this expression is evaluated
 * - Marks any application nodes corresponding to an expression being passed as an argument to
 *   a supercombinator or built-in function which is strict in that argument
 *
 * @param sc      The supercombinator being processed
 *
 * @param c       The expression to analysed
 *
 * @param used    An array of sc->nargs integers which indicate which arguments are used by the
 *                expression
 *
 * @param changed A pointer to an integer that is set to true if a change was made to the
 *                strictness flag of one or more application nodes. Changes to used are not
 *                recorded here; this is the responsibility of check_strictness().
 */
static void check_strictness_r(scomb *sc, cell *c, int *used, int *changed)
{
  if (c->tag & FLAG_PROCESSED)
    return;
  c->tag |= FLAG_PROCESSED;
  switch (celltype(c)) {
  case TYPE_APPLICATION: {
    cell *fun;
    int nargs = 0;
    for (fun = c; TYPE_APPLICATION == celltype(fun); fun = (cell*)fun->field1)
      nargs++;

    /* We have discovered the item at the bottom of the spine, which is the function to be called.
       However we can only perform strictness analysis if we know statically what that function
       is - i.e. if the cell is a supercombinator reference of built-in function.

       It is also possible that it could be a symbol, corresponding to a higher-order function
       passed in as an argument, but since we don't know anything about it we skip this case.

       Additionally, we will wait until we have an application to the right number of arguments
       before doing the analysis - if this is not the case yet we'll just do a recursive call
       to the next item in the application chain and handle it later. */
    if (((TYPE_SCREF == celltype(fun)) || (TYPE_BUILTIN == celltype(fun))) &&
        (nargs == fun_nargs(fun))) {

      /* Follow the left branches down the tree, inspecting each argument and treating it as
         strict or not depending on what we know about the appropriate argument to the function. */
      cell *app = c;
      int argno;
      for (argno = nargs-1; 0 <= argno; argno--) {

        /* If the function is strict in this argument, mark the application node with FLAG_STRICT.
           This will provide a hint to the G-code compiler that it may compile the expression
           directly instead of adding MKAP instructions to create application nodes at runtime. */
        if (fun_strictin(fun,argno)) {
          *changed = (*changed || !(app->tag & FLAG_STRICT));
          app->tag |= FLAG_STRICT;

          /* The expression will definitely need to be evaluated, i.e. it is in a strictness
             context. Perform the analysis recursively. */
          check_strictness_r(sc,(cell*)app->field2,used,changed);
        }

        app = (cell*)app->field1;
      }

      /* If statements need special treatment. The first argument (i.e. the conditional) is strict
         since this is always evaluated. But the second and third arguments (i.e. true and false)
         branches are not strict, since we don't know which will be needed until after the
         conditional is evaluated at runtime. A variable is only considered strict on the branches
         of an if in the case where it is used in a strict context in *both* branches.

         Despite 2nd and 3rd arguments to if not being strict, we treat the *contents* of these
         expressions as being so. Whichever branch is taken will definitely need to return a value,
         so they are treated as a strict context and analysed with another recursive call to
         check_strictness_r(). This information is still relevant to the G-code compiler due to
         the optimised way in which it compiles if statements using JFALSE and JUMP instructions.

         Note that due to the fact that the supercombinator body is a graph and the recursive
         calls to this function may find it's already evaluated, we may miss some variable uses -
         but that's ok. It just means potentially missing out on a few (rare) opportunitites for
         optimisation. */
      if ((TYPE_BUILTIN == celltype(fun)) && (B_IF == (int)fun->field1)) {
        cell *falsebranch = (cell*)c->field2;
        cell *truebranch = (cell*)((cell*)c->field1)->field2;

        int *trueused = (int*)alloca(sc->nargs*sizeof(int));
        int *falseused = (int*)alloca(sc->nargs*sizeof(int));
        memset(trueused,0,sc->nargs*sizeof(int));
        memset(falseused,0,sc->nargs*sizeof(int));

        check_strictness_r(sc,truebranch,trueused,changed);
        check_strictness_r(sc,falsebranch,falseused,changed);

        /* Merge the argument usage information from both branches, keeping only those arguments
           which appear in both. */
        int i;
        for (i = 0; i < sc->nargs; i++)
          if (trueused[i] && falseused[i])
            used[i] = 1;
      }
    }

    /* The expression representing the thing being called is in a strict context, as we definitely
       need the function. */
    check_strictness_r(sc,(cell*)c->field1,used,changed);
    break;
  }
  case TYPE_SYMBOL: {
    /* We are in a strict context and have an encountered a symbol, which must correspond to
       one of the supercombinator's arguments. Mark the appropriate entry in the usage array to
       indicate that this argument will definitely be evaluated. */
    char *symbol = (char*)c->field1;
    int i;
    for (i = 0; i < sc->nargs; i++)
      if (!strcmp(sc->argnames[i],symbol))
        used[i] = 1;
    break;
  }
  case TYPE_BUILTIN:
  case TYPE_SCREF:
  case TYPE_NIL:
  case TYPE_INT:
  case TYPE_DOUBLE:
  case TYPE_STRING:
    break;
  default:
    assert(0);
    break;
  }
}

/**
 * Perform strictness analysis for the specified supercombinator.
 *
 * The main thing this does is to call through to call through to check_strictness_r(). It also
 * compares the new argument usage information with the old, and sets *changed to true of there
 * is any difference.
 *
 * @param sc      The supercombinator being processed
 *
 * @param changed Pointer to an integer which will be set to true if there is any change to
 *                the strictness flags or usage information (thus necessitating another iteration)
 */
static void check_strictness(scomb *sc, int *changed)
{
  int *used = (int*)alloca(sc->nargs*sizeof(int));
  memset(used,0,sc->nargs*sizeof(int));

  cleargraph(sc->body,FLAG_PROCESSED);
  check_strictness_r(sc,sc->body,used,changed);

  if (memcmp(sc->strictin,used,sc->nargs*sizeof(int)))
    *changed = 1;

  memcpy(sc->strictin,used,sc->nargs*sizeof(int));
}

/**
 * Print strictness information about all supercobminators to standard output
 */
void dump_strictinfo()
{
  scomb *sc;
  for (sc = scombs; sc; sc = sc->next) {
    printf("%s ",sc->name);
    int i;
    for (i = 0; i < sc->nargs; i++) {
      if (sc->strictin) {
        if (sc->strictin[i])
          printf("!");
        else
          printf(" ");
      }
      printf("%s ",sc->argnames[i]);
    }
    printf("\n");
  }
}

/**
 * Perform strictness analysis on a set of supercombinators.
 *
 * The analysis process operates in an iterative manner. On each iteration, all supercombinators
 * are examined to determine which arguments they are strict in, and to mark appropriate
 * application nodes with the strictness flag. If any changes to the strictness information or
 * flag values occurs during this process, then another iteration will be performed, as additional
 * cases of strictness may become apparent where arguments are applied to a supercombinator that
 * is now known to be strict in some of its arguments.
 *
 * The process terminates when no changes to the argument strictness or application flags have
 * occurred.
 */
void strictness_analysis()
{
  if (trace)
    debug_stage("Strictness analysis (new version)");

  scomb *sc;
  for (sc = scombs; sc; sc = sc->next)
    sc->strictin = (int*)calloc(sc->nargs,sizeof(int));

  for (sc = scombs; sc; sc = sc->next)
    letrecs_to_graph(&sc->body,sc);

  /* Begin the first iteration. At this stage, none of the arguments to any supercombinators are
     known to be strict. This will change as we perform the analysis. */
  int changed;
  int iteration = 0;
  do {
    changed = 0;
    for (sc = scombs; sc; sc = sc->next)
      check_strictness(sc,&changed);
    iteration++;

    /* If there was any changes, we have to go back through and check the graphs again. One or
       more of the supercombinators is now known to be strict in one of its arguments that
       we didn't know about before. Applications of this supercombinator will now be marked where
       appropriate. We have to keep doing this until there are no more changes, as the effects
       can trickle up to other supercombinators which call the one that changed, and then others
       that call them and so forth. */
  } while (changed);
}
