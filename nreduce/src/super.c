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

#define SUPER_C

#include "grammar.tab.h"
#include "nreduce.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <assert.h>
#include <ctype.h>
#include <stdarg.h>
#include <math.h>

scomb *scombs = NULL;
scomb **lastsc = &scombs;

int genvar = 0;
int nscombs = 0;

scomb *get_scomb_index(int index)
{
  scomb *sc = scombs;
  while (0 < index) {
    index--;
    sc = sc->next;
  }
  return sc;
}

scomb *get_scomb(const char *name)
{
  scomb *sc;
  for (sc = scombs; sc; sc = sc->next)
    if (!strcmp(sc->name,name))
      return sc;
  return NULL;
}

scomb *get_fscomb(const char *name_format, ...)
{
  va_list ap;
  int len;
  char *name;
  scomb *sc;

  va_start(ap,name_format);
  len = vsnprintf(NULL,0,name_format,ap);
  va_end(ap);

  name = (char*)malloc(len+1);
  va_start(ap,name_format);
  vsprintf(name,name_format,ap);
  va_end(ap);

  sc = get_scomb(name);
  free(name);
  return sc;
}

scomb *get_scomb_origlambda(cell *lambda)
{
  scomb *sc;
  for (sc = scombs; sc; sc = sc->next)
    if (sc->origlambda == lambda)
      return sc;
  return NULL;
}

int get_scomb_var(scomb *sc, const char *name)
{
  int i;
  for (i = 0; i < sc->nargs; i++)
    if (sc->argnames[i] && !strcmp(sc->argnames[i],name))
      return i;
  return -1;
}

cell *check_for_lambda(cell *c)
{
  if (c->tag & FLAG_PROCESSED)
    return NULL;
  c->tag |= FLAG_PROCESSED;

  if (TYPE_LAMBDA == celltype(c))
    return c;

  if ((TYPE_APPLICATION == celltype(c)) || (TYPE_CONS == celltype(c))) {
    cell *lambda;
    if (NULL != (lambda = check_for_lambda((cell*)c->field1)))
      return lambda;
    if (NULL != (lambda = check_for_lambda((cell*)c->field2)))
      return lambda;
  }

  return NULL;
}

void check_r(cell *c, scomb *sc)
{
  if (c->tag & FLAG_PROCESSED)
    return;
  c->tag |= FLAG_PROCESSED;

  switch (celltype(c)) {
  case TYPE_IND:
    assert(0);
    break;
  case TYPE_APPLICATION:
  case TYPE_CONS:
    check_r((cell*)c->field1,sc);
    check_r((cell*)c->field2,sc);
    break;
  case TYPE_LAMBDA:
    assert(!"lambdas should be removed by this point");
    break;
  case TYPE_SYMBOL: {
    int found = 0;
    int i;
    for (i = 0; (i < sc->nargs) && !found; i++) {
      if (!strcmp(sc->argnames[i],(char*)c->field1))
        found = 1;
    }
    if (!found) {
      fprintf(stderr,"Supercombinator %s contains free variable %s\n",sc->name,(char*)c->field1);
      assert(0);
    }
    break;
  }
  case TYPE_SCREF:
  case TYPE_BUILTIN:
  case TYPE_NIL:
  case TYPE_INT:
  case TYPE_DOUBLE:
  case TYPE_STRING:
    break;
  }
}

void check_scombs()
{
  scomb *sc;
  for (sc = scombs; sc; sc = sc->next) {
    cleargraph(sc->body,FLAG_PROCESSED);
    check_r(sc->body,sc);
  }
  /* FIXME: check that cells are not shared between supercombinators... each should
     have its own copy */
}

int sc_has_cell(scomb *sc, cell *c)
{
  int i;
  for (i = 0; i < sc->ncells; i++)
    if (sc->cells[i] == c)
      return 1;
  return 0;
}

void add_cells(scomb *sc, cell *c, int *alloc)
{
  if (sc_has_cell(sc,c))
    return;

  if (sc->ncells == *alloc) {
    (*alloc) *= 2;
    sc->cells = (cell**)realloc(sc->cells,(*alloc)*sizeof(cell*));
  }
  sc->cells[sc->ncells++] = c;

  switch (celltype(c)) {
  case TYPE_IND:
    assert(0);
    break;
  case TYPE_APPLICATION:
  case TYPE_CONS:
    add_cells(sc,(cell*)c->field1,alloc);
    add_cells(sc,(cell*)c->field2,alloc);
    break;
  case TYPE_LAMBDA:
    add_cells(sc,(cell*)c->field2,alloc);
    break;
  case TYPE_BUILTIN:
  case TYPE_NIL:
  case TYPE_INT:
  case TYPE_DOUBLE:
  case TYPE_STRING:
  case TYPE_SYMBOL:
  case TYPE_SCREF:
    break;
  default:
    assert(0);
    break;
  }
}

void scomb_calc_cells(scomb *sc)
{
  assert(NULL == sc->cells);

  int alloc = 4;
  sc->ncells = 0;
  sc->cells = (cell**)malloc(alloc*sizeof(cell*));
  add_cells(sc,sc->body,&alloc);
}

void check_scombs_nosharing()
{
#ifdef CHECK_FOR_SUPERCOMBINATOR_SHARED_CELLS
  scomb *sc;
  clearflag(FLAG_PROCESSED);
  for (sc = scombs; sc; sc = sc->next) {
    int i;
    for (i = 0; i < sc->ncells; i++) {
      cell *c = sc->cells[i];
      if (c->tag & FLAG_PROCESSED) {
        scomb *clasher;
        printf("Supercombinator %s contains cell %p shared with another "
                "supercombinator\n",sc->name,c);

        for (clasher = scombs; clasher; clasher = clasher->next) {
          int j;
          for (j = 0; j < clasher->ncells; j++) {
            if (clasher->cells[j] == c) {
              printf("\n");
              printf("%s\n",clasher->name);
              print(clasher->body);
              break;
            }
          }
        }

        exit(1);
      }
      c->tag |= FLAG_PROCESSED;
    }
  }
#endif
}

int liftdepth = 0;

int scomb_count()
{
  int count = 0;
  scomb *sc;
  for (sc = scombs; sc; sc = sc->next)
    count++;
  return count;
}

scomb *add_scomb(char *name, char *prefix)
{
  scomb *sc = (scomb*)calloc(1,sizeof(scomb));

  if (NULL != name) {
    sc->name = strdup(name);
    nscombs++;
  }
  else {
    sc->name = (char*)malloc(strlen(prefix)+20);
    sprintf(sc->name,"%s__%d",prefix,nscombs++);
    assert(NULL == get_scomb(sc->name));
  }

/*   sc->next = scombs; */
/*   scombs = sc; */

/*   scomb **ptr = &scombs; */
/*   while (*ptr) */
/*     ptr = &((*ptr)->next); */
/*   *ptr = sc; */
  *lastsc = sc;
  lastsc = &sc->next;

  return sc;
}

scomb *build_scomb(cell *body, int nargs, char **argnames, int iscopy, int internal, 
                   char *name_format, ...)
{
  va_list ap;
  int len;
  char *name;
  scomb *sc;
  int i;

  va_start(ap,name_format);
  len = vsnprintf(NULL,0,name_format,ap);
  va_end(ap);

  name = (char*)malloc(len+1);
  va_start(ap,name_format);
  vsprintf(name,name_format,ap);
  va_end(ap);

  sc = add_scomb(name,NULL);
  free(name);

  sc->nargs = nargs;
  sc->argnames = (char**)calloc(nargs,sizeof(char*));
  for (i = 0; i < nargs; i++)
    sc->argnames[i] = strdup(argnames[i]);
  sc->lambda = NULL;
  sc->body = body;
  sc->origlambda = NULL;
  sc->abslist = NULL;
  sc->index = -1;
  sc->iscopy = iscopy;
  sc->internal = internal;
  lift(&sc->body,1,1,"__sc__");
  scomb_calc_cells(sc);

  return sc;
}

void scomb_free_list(scomb **list)
{
  while (*list) {
    scomb *sc = *list;
    *list = sc->next;
    scomb_free(sc);
  }
}

void scomb_free(scomb *sc)
{
  abstraction *a;
  int i;
  free(sc->name);
  for (i = 0; i < sc->nargs; i++)
    free(sc->argnames[i]);
  free(sc->argnames);
  free(sc->mayterminate);

  a = sc->abslist;
  while (a) {
    abstraction *next = a->next;
    free(a->name);
    free(a);
    a = next;
  }

  free(sc->strictin);
  free(sc->cells);
  free(sc);
}

void unmarkprocessed(cell *c)
{
  if (TYPE_LAMBDA == celltype(c))
    return;

  if (!(c->tag & FLAG_PROCESSED))
    return;
  c->tag &= ~FLAG_PROCESSED;

  liftdepth++;
  switch (celltype(c)) {
  case TYPE_IND:
    assert(0);
    break;
  case TYPE_APPLICATION:
  case TYPE_CONS:
    unmarkprocessed((cell*)c->field1);
    unmarkprocessed((cell*)c->field2);
    break;
  case TYPE_LAMBDA:
    assert(0);
    break;
  case TYPE_BUILTIN:
  case TYPE_NIL:
  case TYPE_INT:
  case TYPE_DOUBLE:
  case TYPE_STRING:
  case TYPE_SYMBOL:
  case TYPE_SCREF:
    break;
  }
  liftdepth--;
}

void abstract(cell **k, scomb *lambdasc)
{
  abstraction *a;
  abstraction **ptr = &lambdasc->abslist;
//#ifdef DONT_ABSTRACT_PARTIAL_APPLICATIONS
  cell *c;
  int nargs = 0;
  int justdoargs = 0;
//#endif
  int level = (*k)->level;
  assert(0 <= level);

  if (!lambdasc ||
      (TYPE_BUILTIN == celltype((*k))) ||
      (isvalue((*k))) ||
      (TYPE_NIL == celltype((*k))) ||
      (TYPE_SCREF == celltype((*k))))
    return;

  /* skip duplicate variable abstraction */
  if (TYPE_SYMBOL == celltype((*k))) {
    for (a = lambdasc->abslist; a; a = a->next) {
      if (!strcmp(a->name,(char*)(*k)->field1))
        return;
    }
  }

  for (c = *k; TYPE_APPLICATION == celltype(c); c = (cell*)c->field1)
    nargs++;
  #ifdef DONT_ABSTRACT_PARTIAL_BUILTINS
  if (TYPE_BUILTIN == celltype(c)) {
    if (nargs < builtin_info[(int)c->field1].nargs) {
      /* FIXME: can we do this for supercombinators also? They may not have their nargs
         computed yet... */
      #ifdef DEBUG_ABSTRACTION
      printf("%s: Not abstracting builtin %s applied to %d args\n",lambdasc->name,
             builtin_info[(int)c->field1].name,nargs);
      #endif
      justdoargs = 1;
    }
  }
  #endif

  if (justdoargs) {
    /* instead abstract the arguments */
    for (c = *k; TYPE_APPLICATION == celltype(c); c = (cell*)c->field1) {
      #ifdef DEBUG_ABSTRACTION
      printf("  %s: Abstracting arg %p ",lambdasc->name,c->field2);
      print_code((cell*)c->field2);
      printf("\n");
      #endif
      abstract((cell**)&c->field2,lambdasc);
    }
    return;
  }

  #ifdef DEBUG_ABSTRACTION
  printf("%s: Abstracting expr %p ",lambdasc->name,*k);
  print_code(*k);
  printf("\n");
  #endif

  a = (abstraction*)calloc(1,sizeof(abstraction));
  while (*ptr && ((*ptr)->level < level))
    ptr = &((*ptr)->next);
  if (*ptr)
    a->next = *ptr;
  *ptr = a;

  if (TYPE_SYMBOL == celltype((*k))) {
    a->name = strdup((char*)(*k)->field1);
  }
  else if (TYPE_SCREF == celltype((*k))) {
    a->name = strdup(((scomb*)(*k)->field1)->name);
  }
  else {
    a->name = (char*)malloc(20);
    sprintf(a->name,"V%d",genvar++);
  }

  a->body = (*k);
  a->replacement = alloc_cell();
  a->replacement->tag = TYPE_SYMBOL;
  a->replacement->field1 = strdup(a->name);
  a->replacement->field2 = NULL;
  a->replacement->level = level;
  a->level = level;

  (*k) = a->replacement;

  unmarkprocessed(a->body);

  /* FIXME: still have the problem that if we encounter this original cell again we
     will need to replace... */
}

void print_level(cell *c, int level)
{
/*   debug(liftdepth,"%p: level %d: ",c,level); */
/*   print_code(c); */
/*   debug(0,"\n"); */
}


int lift_r(stack *s, cell **k, char *lambdavar, cell *lroot, scomb *lambdasc, int lambdadepth,
           int iscopy, int *level, list **newscombs, char *prefix)
{
  int maxfree = 1;
  abstraction *a;
  *level = 0;

  /* FIXME: experiment with this enabled/disabled to see if it works */
  if (NULL != lambdasc) {
    for (a = lambdasc->abslist; a; a = a->next) {
      if (a->body == *k) {
        *k = a->replacement;
        return ((*k)->tag & FLAG_MAXFREE);
      }
    }
  }

  if ((*k)->tag & FLAG_PROCESSED)
    return ((*k)->tag & FLAG_MAXFREE);
  (*k)->tag |= FLAG_PROCESSED;

  liftdepth++;
  switch (celltype(*k)) {
  case TYPE_IND:
    assert(0);
    break;
  case TYPE_APPLICATION:
  case TYPE_CONS: {
    int level1 = 0;
    int level2 = 0;
    int mf1 = lift_r(s,(cell**)&((*k)->field1),lambdavar,lroot,lambdasc,lambdadepth,iscopy,&level1,
                     newscombs,prefix);
    int mf2 = lift_r(s,(cell**)&((*k)->field2),lambdavar,lroot,lambdasc,lambdadepth,iscopy,&level2,
                     newscombs,prefix);
    if (level1 > level2)
      *level = level1;
    else
      *level = level2;
    print_level(*k,*level);

    maxfree = (mf1 && mf2);

    if (!maxfree) {
      if (mf1)
        abstract((cell**)&((*k)->field1),lambdasc);
      if (mf2)
        abstract((cell**)&((*k)->field2),lambdasc);
    }
    break;
  }
  case TYPE_LAMBDA: {
    scomb *sc = add_scomb(NULL,prefix);
    char *name = strdup((char*)(*k)->field1);
    abstraction *a;
    int argno;

    sc->iscopy = iscopy;
    list_append(newscombs,sc);

    #ifdef LAMBDAS_ARE_MAXFREE_BEFORE_PROCESSING
    (*k)->level = 0;
    (*k)->tag |= FLAG_MAXFREE;
    #else
    (*k)->tag &= ~FLAG_MAXFREE;
    #endif

    stack_push(s,*k);
    if (lift_r(s,(cell**)&((*k)->field2),name,*k,sc,lambdadepth+1,iscopy,level,newscombs,prefix)) {
      abstract((cell**)&(*k)->field2,sc);
    }
    stack_pop(s);
    *level = lambdadepth+1;
    print_level(*k,*level);

    sc->nargs = 1;
    for (a = sc->abslist; a; a = a->next)
      sc->nargs++;
    sc->argnames = (char**)malloc(sc->nargs*sizeof(char*));

    argno = 0;
    for (a = sc->abslist; a; a = a->next)
      sc->argnames[argno++] = strdup(a->name);
    sc->argnames[argno++] = strdup(name);

    sc->body = (cell*)(*k)->field2;

    free((*k)->field1);
    (*k)->tag = TYPE_SCREF;
    (*k)->field1 = sc;
    (*k)->field2 = NULL;

    for (a = sc->abslist; a; a = a->next) {
      cell *fun = alloc_cell();
      copy_raw(fun,*k);
      (*k)->tag = TYPE_APPLICATION;
      (*k)->field1 = fun;
      (*k)->field2 = a->body;
    }

    #ifdef DEBUG_SHOW_CREATED_SUPERCOMBINATORS
    printf("Created supercombinator ");
    print_scomb_code(sc);
    printf("\n");

    argno = 0;
    for (a = sc->abslist; a; a = a->next, argno++) {
      printf("  %s -> ",sc->argnames[argno]);
      print_code(a->body);
      printf("\n");
    }

    #endif

    #ifdef DEBUG_DUMP_TREE_AFTER_CREATING_SUPERCOMBINATOR
    print(global_root);
    #endif

    maxfree = lift_r(s,k,lambdavar,lroot,lambdasc,lambdadepth,iscopy,level,newscombs,prefix);

    free(name);
    break;
  }
  case TYPE_SYMBOL: {
    char *name = (char*)(*k)->field1;
    assert(NULL == get_scomb((char*)(*k)->field1));
    maxfree = ((NULL == lambdavar) || strcmp(name,lambdavar));
    int i;
    for (i = s->count-1; i >= 0; i--) {
      assert(TYPE_LAMBDA == celltype(s->data[i]));
      if (!strcmp((char*)s->data[i]->field1,name)) {
        *level = i+1;
        break;
      }
    }
    print_level(*k,*level);
    break;
  }
  case TYPE_SCREF:
  case TYPE_BUILTIN:
  case TYPE_NIL:
  case TYPE_INT:
  case TYPE_DOUBLE:
  case TYPE_STRING:
    print_level(*k,*level);
    break;
  }
  liftdepth--;

  if (maxfree)
    (*k)->tag |= FLAG_MAXFREE;
  else
    (*k)->tag &= ~FLAG_MAXFREE;

  (*k)->level = *level;

  return maxfree;
}

void lift(cell **k, int iscopy, int calccells, char *prefix)
{
  int level = 0;
  list *newscombs = NULL;
  list *l;
  stack *s = stack_new();

  cleargraph(*k,FLAG_PROCESSED);
  lift_r(s,k,NULL,NULL,NULL,0,iscopy,&level,&newscombs,prefix);
  if (calccells) {
    for (l = newscombs; l; l = l->next)
      scomb_calc_cells((scomb*)l->data);
  }
  list_free(newscombs,NULL);
  stack_free(s);
}

void find_shared(cell *c, scomb *sc, int *shared, int *nshared)
{
  if (c->tag & FLAG_PROCESSED) {
    int i;
    for (i = 0; i < sc->ncells; i++) {
      if (c == sc->cells[i]) {
        if (!shared[i]) {
          shared[i] = ++(*nshared);
          nshared++;
        }
        return;
      }
    }
    assert(0);
    return;
  }
  c->tag |= FLAG_PROCESSED;

  if ((TYPE_APPLICATION == celltype(c)) || (TYPE_CONS == celltype(c))) {
    find_shared((cell*)c->field1,sc,shared,nshared);
    find_shared((cell*)c->field2,sc,shared,nshared);
  }
}

cell *make_varref(char *varname)
{
  cell *ref = alloc_cell();
  ref->tag = TYPE_SYMBOL;
  ref->field1 = strdup(varname);
  ref->field2 = NULL;
  return ref;
}

cell *copy_shared(cell *c, scomb *sc, int *shared, cell **defbodies,
                  char **varnames, int *pos)
{
  int thispos = *pos;
  cell *copy;
  cell *ret;

  if (c->tag & FLAG_PROCESSED) {
    int i;
    for (i = 0; i < *pos; i++) {
      if (sc->cells[i] == c) {
        assert(shared[i]);
        return make_varref(varnames[shared[i]-1]);
      }
    }
    assert(!"can't find copy");
    return NULL;
  }

  c->tag |= FLAG_PROCESSED;
  (*pos)++;

  copy = alloc_cell();

  if (shared[thispos]) {
    defbodies[shared[thispos]-1] = copy;
    ret = make_varref(varnames[shared[thispos]-1]);
  }
  else {
    ret = copy;
  }

  copy_cell(copy,c);

  if ((TYPE_APPLICATION == celltype(c)) || (TYPE_CONS == celltype(c))) {
    copy->field1 = copy_shared((cell*)c->field1,sc,shared,defbodies,varnames,pos);
    copy->field2 = copy_shared((cell*)c->field2,sc,shared,defbodies,varnames,pos);
  }

  return ret;
}

// Note: if you consider getting rid of this and just keeping the letrecs during all
// stages of the compilation process (i.e. no letrec substitution), there is another
// transformation necessary. See the bottom of IFPL p. 345 (358 in the pdf) which
// explains that the RS scheme can't handle deep letrecs, and thus all letrecs should
// be floated up to the top level of a supercombinator body.
cell *super_to_letrec(scomb *sc)
{
  int pos = 0;
  cell *root;
  int *shared = (int*)alloca(sc->ncells*sizeof(int));
  int nshared = 0;
  cell **defbodies;
  char **varnames;
  cell *letrec;
  cell **lnkptr;
  int i;

  cleargraph(sc->body,FLAG_PROCESSED);
  memset(shared,0,sc->ncells*sizeof(int));
  find_shared(sc->body,sc,shared,&nshared);

  if (0 == nshared)
    return sc->body;

  defbodies = (cell**)calloc(nshared,sizeof(cell*));
  varnames = (char**)calloc(nshared,sizeof(char*));
  for (i = 0; i < nshared; i++) {
    varnames[i] = (char*)malloc(20);
    sprintf(varnames[i],"L%d",genvar++);
  }

  cleargraph(sc->body,FLAG_PROCESSED);
  root = copy_shared(sc->body,sc,shared,defbodies,varnames,&pos);

  letrec = alloc_cell();
  letrec->tag = TYPE_LETREC;
  letrec->field1 = NULL;
  letrec->field2 = root;
  lnkptr = (cell**)&letrec->field1;

  for (i = 0; i < nshared; i++) {
    cell *def = alloc_cell();
    cell *lnk = alloc_cell();

    def->tag = TYPE_VARDEF;
    def->field1 = varnames[i];
    def->field2 = defbodies[i];

    lnk->tag = TYPE_VARLNK;
    lnk->field1 = def;
    lnk->field2 = NULL;
    *lnkptr = lnk;
    lnkptr = (cell**)&lnk->field2;
  }

  free(defbodies);
  free(varnames);
  return letrec;
}

void varused(cell *c, char *name, int *used)
{
  if (c->tag & FLAG_PROCESSED)
    return;
  c->tag |= FLAG_PROCESSED;

  assert(TYPE_IND != celltype(c));
  assert(TYPE_LAMBDA != celltype(c));
  if ((TYPE_SYMBOL == celltype(c)) && !strcmp((char*)c->field1,name))
    *used = 1;
  if ((TYPE_APPLICATION == celltype(c)) || (TYPE_CONS == celltype(c))) {
    varused((cell*)c->field1,name,used);
    varused((cell*)c->field2,name,used);
  }
}

void remove_redundant_scombs()
{
#ifdef REMOVE_REDUNDANT_SUPERCOMBINATORS
  scomb *sc;
  printf("Removing redundant arguments\n");

  for (sc = scombs; sc; sc = sc->next) {
    while ((0 < sc->nargs) &&
           (TYPE_APPLICATION == celltype(sc->body)) &&
           (TYPE_SYMBOL == celltype((cell*)sc->body->field2)) &&
           (!strcmp(sc->argnames[sc->nargs-1],(char*)((cell*)sc->body->field2)->field1))) {
      int used = 0;
      clearflag(FLAG_PROCESSED);
      varused((cell*)sc->body->field1,sc->argnames[sc->nargs-1],&used);

      if (used) {
        printf("%s: NOT removing %s\n",sc->name,sc->argnames[sc->nargs-1]);
        break;
      }
      else {
        printf("%s: removing %s\n",sc->name,sc->argnames[sc->nargs-1]);
        /* FIXME: we can only remove the parameter if it isn't used elsewhere in the body! */
        free(sc->argnames[sc->nargs-1]);
        sc->nargs--;
        sc->body = (cell*)sc->body->field1;
      }
    }
  }

  printf("Removing redundant supercombinators\n");
#endif
}

void fix_partial_applications()
{
#if 1
/*   printf("Fixing partial applications\n"); */
  scomb *sc;
  for (sc = scombs; sc; sc = sc->next) {
    cell *c = sc->body;
    int nargs = 0;
    scomb *other;
    while (TYPE_APPLICATION == celltype(c)) {
      c = (cell*)c->field1;
      nargs++;
    }
    if (TYPE_SCREF == celltype(c)) {
      other = (scomb*)c->field1;
      if (nargs < other->nargs) {
        int oldargs = sc->nargs;
        int extra = other->nargs - nargs;
        int i;
        sc->nargs += extra;
        sc->argnames = (char**)realloc(sc->argnames,sc->nargs*sizeof(char*));
        for (i = oldargs; i < sc->nargs; i++) {
          cell *fun = sc->body;
          cell *arg = alloc_cell();

          sc->argnames[i] = (char*)malloc(20);
          sprintf(sc->argnames[i],"N%d",genvar++);

          arg->tag = TYPE_SYMBOL;
          arg->field1 = strdup(sc->argnames[i]);
          arg->field2 = NULL;

          sc->body = alloc_cell();
          sc->body->tag = TYPE_APPLICATION;
          sc->body->field1 = fun;
          sc->body->field2 = arg;
        }
      }
    }
  }
/*   printf("Done fixing partial applications\n"); */
#endif
}

void resolve_scvars_r(cell *c)
{
  if (c->tag & FLAG_PROCESSED)
    return;
  c->tag |= FLAG_PROCESSED;

  switch (celltype(c)) {
  case TYPE_LETREC:
  case TYPE_IND:
    break;
  case TYPE_APPLICATION:
  case TYPE_CONS:
    resolve_scvars_r((cell*)c->field1);
    resolve_scvars_r((cell*)c->field2);
    break;
  case TYPE_LAMBDA:
    resolve_scvars_r((cell*)c->field2);
    break;
  case TYPE_SYMBOL: {
    scomb *sc;
    if (NULL != (sc = get_scomb((char*)c->field1))) {
      free(c->field1);
      c->tag = TYPE_SCREF;
      c->field1 = sc;
    }
    break;
  }
  }
}

void resolve_scvars(cell *c)
{
  cleargraph(c,FLAG_PROCESSED);
  resolve_scvars_r(c);
}
