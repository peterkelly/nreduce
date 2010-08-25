/*
 * This file is part of the NReduce project
 * Copyright (C) 2006-2010 Peter Kelly <kellypmk@gmail.com>
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
 * that it is not, and thus the bytecode compiler would generate MKFRAME/MKCAP instructions instead
 * of evaluating the expression for that argument directly.
 *
 * We also try to detect the "strictness" of letrec bindings, so that when constructing a letrec
 * graph during bytecode execution we can in some cases evaluate expressions directly rather than
 * creating FRAME or CAP cells for them. For this reason, struct letrec contains a "strict" field
 * which indicates that the expression is definitely used in the body of the letrec expression.
 * Note however that we can only evaluate it directly if it does not depend on later bindings
 * in the letrec expression; graphs constructed using a letrec must be evaluated lazily.
 */

#include "src/nreduce.h"
#include "runtime/runtime.h"
#include "source.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <assert.h>
#include <ctype.h>
#include <stdarg.h>
#include <math.h>

/**
 * Determine the number of arguments required by the function referenced by the specified node
 *
 * @param c Node representing a function. Must be either a supercombinator reference or built-in
 *          function
 */
static int fun_nargs(snode *c)
{
  if (SNODE_SCREF == c->type)
    return c->sc->nargs;
  else if (SNODE_BUILTIN == c->type)
    return builtin_info[c->bif].nargs;
  else
    abort();
}

/**
 * Determine whether or not the function referenced by the node is strict in the specified argument
 *
 * @param c Node representing a function. Must be either a supercombinator reference or built-in
 *          function
 */
static int fun_strictin(snode *c, int argno)
{
  assert(argno < fun_nargs(c));
  if (SNODE_SCREF == c->type)
    return c->sc->strictin[argno];
  else if (SNODE_BUILTIN == c->type)
    return (argno < builtin_info[c->bif].nstrict);
  else
    abort();
}

/**
 * Add the specified variable to a list, maintaining the correct sort order
 *
 * @param vars The list to add to
 * @param name The name to add
 */
static void add_var(list **vars, char *name)
{
  list **ptr = vars;
  int cmp = -1;
  while (*ptr && (0 > (cmp = strcmp((char*)(*ptr)->data,name)))) {
    ptr = &(*ptr)->next;
  }
  if (cmp)
    list_push(ptr,name);
}

/**
 * Add all names which appear in *both* a and b to the specified list.
 *
 * @param vars The list to add to
 * @param a The first list to take names from. This list must be sorted.
 * @param b The second list to take names from. This list must be sorted.
 */
static void add_union(list **dest, list *a, list *b)
{
  while (a && b) {
    int cmp = strcmp((char*)a->data,(char*)b->data);

    if (cmp < 0) {
      a = a->next;
    }
    else if (cmp > 0) {
      b = b->next;
    }
    else {
      add_var(dest,(char*)a->data);
      a = a->next;
      b = b->next;
    }
  }
}

/**
 * Recursively perform strictness analysis on an expression.
 *
 * This function does three things:
 * - Records which arguments will definitely be evaluated, assuming this expression is evaluated
 * - Marks any application nodes corresponding to an expression being passed as an argument to
 *   a supercombinator or built-in function which is strict in that argument
 * - Marks any letrec bindings used within the expression as strict
 *
 * @param sc      The supercombinator being processed
 *
 * @param c       The expression to analysed
 *
 * @param used    A (sorted) list of strings indicating the variables that are used in a strict
 *                context within the expression. This list is updated by the function if any new
 *                ones are encountered.
 *
 * @param changed A pointer to an integer that is set to true if a change was made to the
 *                strictness flag of one or more application nodes. Changes to used are not
 *                recorded here; this is the responsibility of check_strictness().
 */
static void check_strictness_r(scomb *sc, snode *c, list **used, int *changed)
{
  switch (c->type) {
  case SNODE_LETREC: {
    letrec *rec;
    list *bodyused = NULL;
    list *l;

    int again;
    do {
      again = 0;
      for (rec = c->bindings; rec; rec = rec->next)
        if (rec->strict)
          check_strictness_r(sc,rec->value,&bodyused,changed);
      check_strictness_r(sc,c->body,&bodyused,changed);

      for (rec = c->bindings; rec; rec = rec->next) {
        if (!rec->strict && list_contains_string(bodyused,rec->name)) {
          rec->strict = 1;
          again = 1;
          *changed = 1;
        }
      }
    } while (again);

    for (l = bodyused; l; l = l->next) {
      int isrec = 0;
      for (rec = c->bindings; rec && !isrec; rec = rec->next)
        if (!strcmp((char*)l->data,rec->name))
          isrec = 1;
      if (!isrec) {
        add_var(used,(char*)l->data);
      }
    }
    list_free(bodyused,NULL);
    break;
  }
  case SNODE_APPLICATION: {
    snode *fun;
    int nargs = 0;
    for (fun = c; SNODE_APPLICATION == fun->type; fun = fun->left)
      nargs++;

    /* We have discovered the item at the bottom of the spine, which is the function to be called.
       However we can only perform strictness analysis if we know statically what that function
       is - i.e. if the node is a supercombinator reference of built-in function.

       It is also possible that it could be a symbol, corresponding to a higher-order function
       passed in as an argument, but since we don't know anything about it we skip this case.

       Additionally, we will wait until we have an application to the right number of arguments
       before doing the analysis - if this is not the case yet we'll just do a recursive call
       to the next item in the application chain and handle it later. */
    if (((SNODE_SCREF == fun->type) || (SNODE_BUILTIN == fun->type)) &&
        (nargs == fun_nargs(fun))) {

      /* Follow the left branches down the tree, inspecting each argument and treating it as
         strict or not depending on what we know about the appropriate argument to the function. */
      snode *app = c;
      int argno;
      for (argno = nargs-1; 0 <= argno; argno--) {

        /* If the function is strict in this argument, mark the application node as strict.
           This will provide a hint to the bytecode compiler that it may compile the expression
           directly instead of adding MKAP instructions to create application nodes at runtime. */
        if (fun_strictin(fun,argno)) {
          *changed = (*changed || !app->strict);
          app->strict = 1;

          /* The expression will definitely need to be evaluated, i.e. it is in a strictness
             context. Perform the analysis recursively. */
          check_strictness_r(sc,app->right,used,changed);
        }

        app = app->left;
      }

      /* If statements need special treatment. The first argument (i.e. the conditional) is strict
         since this is always evaluated. But the second and third arguments (i.e. true and false)
         branches are not strict, since we don't know which will be needed until after the
         conditional is evaluated at runtime. A variable is only considered strict on the branches
         of an if in the case where it is used in a strict context in *both* branches.

         Despite the 2nd and 3rd arguments to if not being strict, we treat the *contents* of these
         expressions as being so. Whichever branch is taken will definitely need to return a value,
         so they are treated as a strict context and analysed with another recursive call to
         check_strictness_r(). This information is still relevant to the bytecode compiler due to
         the optimised way in which it compiles if statements using JFALSE and JUMP instructions.
         We also annotate application nodes within the true/false branches with strictness
         where appropriate. */
      if ((SNODE_BUILTIN == fun->type) && (B_IF == fun->bif)) {
        snode *falsebranch = c->right;
        snode *truebranch = c->left->right;

        list *trueused = NULL;
        list *falseused = NULL;

        check_strictness_r(sc,truebranch,&trueused,changed);
        check_strictness_r(sc,falsebranch,&falseused,changed);

        /* Merge the argument usage information from both branches, keeping only those arguments
           which appear in both. */
        add_union(used,trueused,falseused);
        list_free(trueused,NULL);
        list_free(falseused,NULL);
      }
      /* As with the true/false branches of an if call, we can also treat the contents of the
         second argument to seq as strict. The bytecode compiler also contains an optimisation
         for seq where it will evaluate the first argument, discard the result, and then evaluate
         the second argument. So we know that any function applications within the second argument
         will definitely be needed. However, we do not add variables within the expression to our
         used list, as we need to ensure that things referenced by the second argument are not
         evaluated until *after* the first argument (like with if). */
      if ((SNODE_BUILTIN == fun->type) && (B_SEQ == fun->bif)) {
        snode *after = c->right;
        list *afterused = NULL;
        check_strictness_r(sc,after,&afterused,changed);
        list_free(afterused,NULL);
      }
    }

    /* The expression representing the thing being called is in a strict context, as we definitely
       need the function. */
    check_strictness_r(sc,c->left,used,changed);
    break;
  }
  case SNODE_SYMBOL:
    /* We are in a strict context and have an encountered a symbol, which must correspond to
       one of the supercombinator's arguments or a letrec binding. Add the variable to the list
       to indicate that this argument will definitely be evaluated. */
    add_var(used,c->name);
    break;
  case SNODE_BUILTIN:
  case SNODE_SCREF:
  case SNODE_NIL:
  case SNODE_NUMBER:
  case SNODE_STRING:
    break;
  default:
    abort();
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
void check_strictness(scomb *sc, int *changed)
{
  list *used = NULL;
  int i;
  int *strictin;

  check_strictness_r(sc,sc->body,&used,changed);

  strictin = (int*)malloc(sc->nargs*sizeof(int));
  for (i = 0; i < sc->nargs; i++)
    strictin[i] = sc->strictin[i] || list_contains_string(used,sc->argnames[i]);

  if (memcmp(sc->strictin,strictin,sc->nargs*sizeof(int)))
    *changed = 1;

  free(sc->strictin);
  sc->strictin = strictin;
  list_free(used,NULL);
}

/**
 * Print strictness information about all supercobminators to standard output
 */
void dump_strictinfo(source *src)
{
  int scno;
  int count = array_count(src->scombs);
  for (scno = 0; scno < count; scno++) {
    scomb *sc = array_item(src->scombs,scno,scomb*);
    if (is_from_prelude(src,sc))
      continue;
    print_scomb_code(src,stdout,sc);
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
void strictness_analysis(source *src)
{
  int scno;
  int changed;
  int sccount = array_count(src->scombs);

  for (scno = 0; scno < sccount; scno++) {
    scomb *sc = array_item(src->scombs,scno,scomb*);
    if (NULL == sc->strictin)
      sc->strictin = (int*)calloc(sc->nargs,sizeof(int));
  }

  /* Begin the first iteration. At this stage, none of the arguments to any supercombinators are
     known to be strict. This will change as we perform the analysis. */
  do {
    changed = 0;
    for (scno = 0; scno < sccount; scno++) {
      scomb *sc = array_item(src->scombs,scno,scomb*);
      check_strictness(sc,&changed);
    }

    /* If there was any changes, we have to go back through and check the graphs again. One or
       more of the supercombinators is now known to be strict in one of its arguments that
       we didn't know about before. Applications of this supercombinator will now be marked where
       appropriate. We have to keep doing this until there are no more changes, as the effects
       can trickle up to other supercombinators which call the one that changed, and then others
       that call them and so forth. */
  } while (changed);
}
