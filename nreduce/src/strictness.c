#define STRICTNESS_C

#include "nreduce.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <assert.h>
#include <ctype.h>
#include <stdarg.h>
#include <math.h>

scomb *sc_lallstrict = NULL;
scomb *sc_allstrict = NULL;
scomb *sc_lnostrict = NULL;
scomb *sc_nostrict = NULL;
scomb *sc_spint = NULL;
scomb *sc_builtins[NUM_BUILTINS];

int tempdebug = 0;
int ntestreductions = 0;
int strictdebug = 0;

cell *mkcell(int type, void *field1, void *field2)
{
  cell *c = alloc_cell();
  c->tag = type;
  c->field1 = field1;
  c->field2 = field2;
  return c;
}

cell *mkbapp(int bif, cell *arg1, cell *arg2)
{
  cell *bifcell = mkcell(TYPE_BUILTIN,(void*)bif,NULL);
  cell *app = mkcell(TYPE_APPLICATION,bifcell,arg1);
  if (arg2)
    app = mkcell(TYPE_APPLICATION,app,arg2);
  return app;
}

#define def(name,value,prev) mkcell(TYPE_VARLNK,mkcell(TYPE_VARDEF,strdup(name),value),prev);
#define var(var) mkcell(TYPE_SYMBOL,strdup(var),NULL)
#define scref(sc) mkcell(TYPE_SCREF,sc,NULL)
#define letrec(a,b) mkcell(TYPE_LETREC,a,b)
#define in NULL
#define cons(a,b) mkbapp(B_CONS,a,b)
#define un(a,b) mkbapp(B_UNION,a,b)
#define intersect(a,b) mkbapp(B_INTERSECT,a,b)
#define spintersect(a,b) app(app(scref(sc_spint),a),b)
#define lambda(varname,expr) mkcell(TYPE_LAMBDA,strdup(varname),expr)
#define Sv(a) mkbapp(B_HEAD,a,NULL)
#define Sf(a) mkbapp(B_TAIL,a,NULL)
#define string(x) mkcell(TYPE_STRING,strdup(x),NULL)
#define app(a,b) mkcell(TYPE_APPLICATION,a,b)

void addsc(const char *s)
{
}

void addallstrict()
{
  scomb *sc = add_scomb("lallstrict");
  sc_lallstrict = sc;
  sc->nargs = 1;
  sc->argnames = (char**)malloc(sizeof(char*));
  sc->argnames[0] = strdup("x");
  sc->body = cons(string("{*}"),scref(sc_lallstrict));
  sc->internal = 1;
  scomb_calc_cells(sc);

  sc = add_scomb("allstrict");
  sc_allstrict = sc;
  sc->body = cons(string("{*}"),scref(sc_lallstrict));
  sc->internal = 1;
  scomb_calc_cells(sc);
}

void addnostrict()
{
  scomb *sc = add_scomb("lnostrict");
  sc_lnostrict = sc;
  sc->nargs = 1;
  sc->argnames = (char**)malloc(sizeof(char*));
  sc->argnames[0] = strdup("x");
  sc->body = cons(string("{}"),scref(sc_lnostrict));
  sc->internal = 1;
  scomb_calc_cells(sc);

  sc = add_scomb("nostrict");
  sc_nostrict = sc;
  sc->body = cons(string("{}"),scref(sc_lnostrict));
  sc->internal = 1;
  scomb_calc_cells(sc);
}

void addspint()
{
  scomb *sc = add_scomb("spint");
  sc_spint = sc;
  sc->nargs = 3;
  sc->argnames = (char**)malloc(sc->nargs*sizeof(char*));
  sc->argnames[0] = strdup("e1");
  sc->argnames[1] = strdup("e2");
  sc->argnames[2] = strdup("x");

  cell *e1x = app(var("e1"),var("x"));
  cell *e2x = app(var("e2"),var("x"));

  sc->body = cons(intersect(Sv(e1x),Sv(e2x)),app(app(scref(sc_spint),Sf(e1x)),Sf(e2x)));
  sc->internal = 1;
  scomb_calc_cells(sc);
}

void addabsbuiltins()
{
  /* FIXME: ideally this should be extended to support arbitrary nargs/nstrict, to allow
     additional builtin functions to be added in the future */
  int i;
  for (i = 0; i < NUM_BUILTINS; i++) {
    const builtin *info = &builtin_info[i];
    cell *body = NULL;
    if (B_IF == i) {
      body = cons(string("{}"),lambda("a",
             cons(string("{}"),lambda("b",
             cons(string("{}"),lambda("c",
               cons(un(Sv(var("a")),intersect(Sv(var("b")),Sv(var("c")))),
                    spintersect(Sf(var("b")),Sf(var("c"))))))))));
    }
    else if ((2 == info->nargs) && (2 == info->nstrict)) {
      body = cons(string("{}"),lambda("a",
             cons(string("{}"),lambda("b",
               cons(un(Sv(var("a")),Sv(var("b"))),string("serr"))))));
    }
    else if ((2 == info->nargs) && (0 == info->nstrict)) {
      body = cons(string("{}"),lambda("a",
             cons(string("{}"),lambda("b",
               cons(string("{}"),string("serr"))))));
    }
    else if (1 == info->nargs) {
      assert(1 == info->nstrict);
      body = cons(string("{}"),lambda("a",cons(Sv(var("a")),string("serr"))));
    }
    else {
      assert(0); /* ? */
    }
    sc_builtins[i] = build_scomb(body,0,NULL,0,1,"%s#",info->name);
  }
}

cell *S(cell *c, int iteration)
{
  switch (celltype(c)) {
  case TYPE_APPLICATION: {
    cell *e1pair = S((cell*)c->field1,iteration);
    cell *e2pair = S((cell*)c->field2,iteration);
    cell *app = mkcell(TYPE_APPLICATION,Sf(e1pair),e2pair);
    return cons(un(Sv(e1pair),Sv(app)),Sf(app));
    break;
  }
  case TYPE_LAMBDA:
    return cons(string("{}"),lambda((char*)c->field1,S((cell*)c->field2,iteration)));
    break;
  case TYPE_BUILTIN:
    return scref(sc_builtins[(int)c->field1]);
    break;
  case TYPE_CONS:
    assert(0);
    break;
  case TYPE_SYMBOL: {
    return mkcell(TYPE_SYMBOL,strdup((char*)c->field1),(void*)1);
    break;
  }
  case TYPE_SCREF: {
    scomb *vsc = (scomb*)c->field1;
    scomb *realsc = get_fscomb("%s#%d",vsc->name,iteration);
    assert(realsc);
    return mkcell(TYPE_SCREF,realsc,NULL);
    break;
  }
  case TYPE_NIL:
  case TYPE_INT:
  case TYPE_DOUBLE:
  case TYPE_STRING:
    return cons(string("{}"),string("serr"));
    break;
  default:
    break;
  }
  assert(0);
  return NULL;
}

void compute_strictness(scomb *approx, scomb *orig)
{
  int i;
  cell *app = scref(approx);
  scomb **oldlastsc = lastsc;

  for (i = 0; i < orig->nargs; i++) {
    char *str = (char*)malloc(strlen(orig->argnames[i])+3);
    sprintf(str,"{%s}",orig->argnames[i]);
    cell *arg = cons(string(str),globnil);
    arg->field2 = lambda("x",arg);
    free(str);
/*     assert(NULL == get_fscomb("%s_arg%d",approx->name,i)); */

    scomb *argsc = build_scomb(arg,0,NULL,1,0,"%s_arg%d",approx->name,i);
    
    app = mkcell(TYPE_APPLICATION,Sf(app),scref(argsc));
  }
  app = Sv(app);
  global_root = app;
  push(app);

  global_root = app;
  ntestreductions++;
  reduce();
  app = pop();

  assert(TYPE_STRING == celltype(app));

  lastsc = oldlastsc;
  scomb_free_list(lastsc);
  assert(NULL == *lastsc);

/*   printf("%s",(char*)app->field1); */

  approx->strictin = (int*)malloc(orig->nargs*sizeof(int));
  memset(approx->strictin,0,orig->nargs*sizeof(int));
  for (i = 0; i < orig->nargs; i++) {
    if (set_contains((char*)app->field1,orig->argnames[i]))
      approx->strictin[i] = 1;
  }

/*   printf(" ("); */
/*   for (i = 0; i < orig->nargs; i++) { */
/*     printf("%d",approx->strictin[i]); */
/*   } */
/*   printf(") "); */
}

flist *flist_new(int nargs)
{
  flist *fl = (flist*)calloc(1,sizeof(flist));
  fl->nargs = nargs;
  return fl;
}

void flist_free(flist *fl)
{
  if (!fl)
    return;
  int i;
  for (i = 0; i < fl->count; i++)
    free(fl->item[i]);
  free(fl);
}

void flist_add(flist *fl, int *front)
{
  if (fl->alloc == fl->count) {
    if (0 == fl->alloc)
      fl->alloc = 2;
    else
      fl->alloc *= 2;
    fl->item = (int**)realloc(fl->item,fl->alloc*sizeof(int*));
  }

  int *copy = (int*)malloc(fl->nargs*sizeof(int));
  memcpy(copy,front,fl->nargs*sizeof(int));

  fl->item[fl->count++] = copy;
}

int flist_contains(flist *fl, int *front)
{
  int i;
  for (i = 0; i < fl->count; i++) {
    if (!memcmp(fl->item[i],front,fl->nargs*sizeof(int)))
      return 1;
  }
  return 0;
}

flist *flist_copy(flist *src)
{
  flist *dst = flist_new(src->nargs);
  int i;
  dst->count = src->count;
  dst->alloc = src->alloc;
  dst->item = (int**)malloc(dst->alloc*sizeof(int*));
  for (i = 0; i < src->count; i++) {
    dst->item[i] = malloc(dst->nargs*sizeof(int));
    memcpy(dst->item[i],src->item[i],dst->nargs*sizeof(int));
  }
  return dst;
}


int check_mayterminate(scomb *sc, int nargs, int *args)
{
  int i;
  cell *app = scref(sc);
  for (i = 0; i < nargs; i++) {
    cell *arg;
    if (args[i])
      arg = scref(sc_nostrict);
    else
      arg = scref(sc_allstrict);
    app = mkcell(TYPE_APPLICATION,Sf(app),arg);
  }
  app = Sv(app);
  global_root = app;
  push(app);

  global_root = app;
  ntestreductions++;
  reduce();
  app = pop();

  assert(TYPE_STRING == celltype(app));

  if (strcmp((char*)app->field1,"{}")) {
    /* the set of variables in which this function is strict is not empty - so this is
       non-termination */
    return 0;
  }
  else
    return 1;
}

void print_front(int nargs, int *args)
{
  int i;
  for (i = 0; i < nargs; i++)
    printf("%d",args[i]);
}

void print_frontier(flist *fl, int nargs)
{
  int f;
  for (f = 0; f < fl->count; f++) {
    if (0 < f)
      printf(" ");
    print_front(fl->nargs,fl->item[f]);
  }
}


int within_frontier(flist *fl, int nargs, int *args)
{
  int i;
  for (i = 0; i < fl->count; i++) {
    if (!memcmp(fl->item[i],args,nargs*sizeof(int)))
      return 1;
  }
  for (i = 0; i < nargs; i++) {
    if (!args[i]) {
      args[i] = 1;
      int checked = within_frontier(fl,nargs,args);
      args[i] = 0;
      if (checked)
        return 1;
    }
  }
  return 0;
}

void calc_single(flist *fl, scomb *sc, int nargs, flist *tocheck, int *args, flist *checked)
{
  /* Note: A node is only checked if its parent was a 1-node */

  /* Have we already checked this node? If so, we don't want to check it again. */
  if (flist_contains(checked,args)) {
#ifdef DEBUG_FRONTIERS
    printf("  %s: ",sc->name);
    print_front(nargs,args);
    printf(" already checked\n");
#endif
    return;
  }
  flist_add(checked,args);

  /* Is this node below and existing 0-node? If so, we know its value will be 0 so we can
     skip checking */
  if (within_frontier(fl,nargs,args)) {
#ifdef DEBUG_FRONTIERS
    printf("  %s: ",sc->name);
    print_front(nargs,args);
    printf(" already within frontier\n");
#endif
    return;
  }

  /* Find out if this node is an 0-node or a 1-node */
  int checkres = check_mayterminate(sc,nargs,args);

#ifdef DEBUG_FRONTIERS
  printf("  %s: ",sc->name);
  print_front(nargs,args);
  printf(" -> %d",checkres);
#endif

  if (0 == checkres) {
    /* All nodes below this will be 0 nodes, so we don't need to check any of the children */
#ifdef DEBUG_FRONTIERS
    printf(", adding to frontier\n");
#endif
    flist_add(fl,args);
  }
  else {
    /* It is a 1 node - therefore we want to check all of its children to see if they are
       0-nodes */
    int i;
#ifdef DEBUG_FRONTIERS
    printf(", adding children\n");
#endif
    for (i = 0; i < nargs; i++) {
      if (args[i]) {
        args[i] = 0;
        if (!within_frontier(fl,nargs,args))
          flist_add(tocheck,args);
        args[i] = 1;
      }
    }
  }
}

flist *calc_frontier(scomb *sc, flist *prevfrontier)
{
  flist *tocheck = flist_copy(prevfrontier);
  int nargs = prevfrontier->nargs;
  flist *checked = flist_new(nargs);
  flist *fl = flist_new(nargs);

  int pos;
  for (pos = 0; pos < tocheck->count; pos++) {
    int *check = tocheck->item[pos];
    calc_single(fl,sc,nargs,tocheck,check,checked);
  }
  return fl;
}

int flist_equals(flist *a, flist *b)
{
  assert(a->nargs == b->nargs);
  if (a->count != b->count)
    return 0;
  int i;
  for (i = 0; i < a->count; i++)
    if (memcmp(a->item[i],b->item[i],a->nargs*sizeof(int)))
      return 0;
  return 1;
}

void annotate_r(cell *c)
{
  if (c->tag & FLAG_PROCESSED)
    return;
  c->tag |= FLAG_PROCESSED;

  switch (celltype(c)) {
  case TYPE_APPLICATION: {
    cell *target = (cell*)c->field1;
    int count = 0;
    while (TYPE_APPLICATION == celltype(target)) {
      target = (cell*)target->field1;
      count++;
    }
    if (TYPE_SCREF == celltype(target)) {
      scomb *fun = (scomb*)target->field1;
      if ((count < fun->nargs) && fun->strictin[count])
        c->tag |= FLAG_STRICT;
    }

    annotate_r((cell*)c->field1);
    annotate_r((cell*)c->field2);
    break;
  }
  case TYPE_CONS:
    assert(0);
    break;
  case TYPE_BUILTIN:
  case TYPE_SYMBOL:
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

void annotate(scomb *sc)
{
/*   int i; */
/*   printf("annotate(%s): strict in",sc->name); */
/*   if (sc->strictin) { */
/*     for (i = 0; i < sc->nargs; i++) */
/*       if (sc->strictin[i]) */
/*         printf(" %s",sc->argnames[i]); */
/*   } */
/*   printf("\n"); */

#if 0
  clearflag_scomb(FLAG_PROCESSED,sc);
  annotate_r(sc->body);
#endif
}

#define MAXITER 15
void strictness_analysis()
{
#ifdef STRICTNESS_ANALYSIS
  scomb *sc;
  int iteration;
  int nscombs = 0;
  scomb **oldlast = lastsc;

  if (trace)
    debug_stage("Strictness analysis");

  /* FIXME: just calculate isgraph once for each scomb! */

  for (sc = scombs; sc; sc = sc->next)
    nscombs++;

  scomb **sctotest = (scomb**)calloc(nscombs,sizeof(scomb*));
  int *isgr = (int*)calloc(nscombs,sizeof(int));

  int scno = 0;
  for (sc = scombs; sc; sc = sc->next) {
    sctotest[scno] = sc;
    isgr[scno] = scomb_isgraph(sc);
/*     if (isgr[scno]) */
/*       printf("WARNING: skipping strictness analysis for %s; it's a graph\n",sc->name); */
    scno++;
  }

  addallstrict();
  addnostrict();
  addspint();
  addabsbuiltins();

  /* Print strictness information for initial approximation (#0), i.e. which is strict
     in all args */
  if (strictdebug) {
    for (scno = 0; scno < nscombs; scno++) {
      sc = sctotest[scno];
      if (!isgr[scno] && strcmp(sc->name,"PROG")) {
/*         printf("%s#0 is strict in {*}\n",sc->name); */

/*         printf("%s#0 is strict in {",sc->name); */
/*         int i; */
/*         for (i = 0; i < sc->nargs; i++) */
/*           printf("%s%s",i > 0 ? "," : "",sc->argnames[i]); */
/*         printf("}\n"); */
      }
    }
  }

  /* Create the initial frontier for each supercombinator */
  flist **frontiers = (flist**)calloc(1,MAXITER*nscombs*sizeof(flist*));
  int **strictins = (int**)calloc(1,MAXITER*nscombs*sizeof(int*));
  for (scno = 0; scno < nscombs; scno++) {
    sc = sctotest[scno];
    cell *body = scref(sc_allstrict);
    scomb *approx = build_scomb(body,0,NULL,1,0,"%s#0",sc->name);
    int *initfront = (int*)alloca(sc->nargs*sizeof(int));
    int i;
    approx->strictin = (int*)malloc(sc->nargs*sizeof(int));
    strictins[scno] = approx->strictin;
    for (i = 0; i < sc->nargs; i++) {
      initfront[i] = 1;
      approx->strictin[i] = 1;
    }
    frontiers[scno] = flist_new(sc->nargs);
    flist_add(frontiers[scno],initfront);
  }

  /* Create successive approximations in the ascending kleene chain, and compute the
     frontier for each */
  for (iteration = 1; iteration < MAXITER; iteration++) {
    int converged = 1;
    for (scno = 0; scno < nscombs; scno++) {
      sc = sctotest[scno];
      int i;
      cell *body = sc->body;
      for (i = sc->nargs-1; i >= 0; i--)
        body = lambda(sc->argnames[i],body);
      cell *spfun = isgr[scno] ? scref(sc_nostrict) : S(body,iteration-1);
      scomb *approx = build_scomb(spfun,0,NULL,1,0,"%s#%d",sc->name,iteration);

/*       if (trace) */
/*         printf("Built approximation %s\n",approx->name); */


/*       frontiers[iteration*nscombs+scno] = */
/*         calc_frontier(approx,frontiers[(iteration-1)*nscombs+scno]); */

/*       if (!flist_equals(frontiers[iteration*nscombs+scno], */
/*                         frontiers[(iteration-1)*nscombs+scno])) */
/*         converged = 0; */


      if (!isgr[scno] && strcmp(sc->name,"PROG")) {
        compute_strictness(approx,sc);


        if (trace) {
          printf("%s is strict in {",approx->name);
          int a;
          int nstrict = 0;
          for (a = 0; a < sc->nargs; a++) {
            if (approx->strictin[a]) {
              if (0 < nstrict++)
                printf(",");
              printf("%s",sc->argnames[a]);
            }
          }
          printf("}");
          printf("\n");
        }

        strictins[iteration*nscombs+scno] = approx->strictin;

        int nprev = 0;
        while (nprev < iteration) {
          if (!memcmp(strictins[iteration*nscombs+scno],
                      strictins[(iteration-nprev-1)*nscombs+scno],
                      sc->nargs*sizeof(int))) {
            nprev++;
          }
          else {
            break;
          }
        }
/*         printf(" - same as %d prev iterations",nprev); */
/*         printf("\n"); */

        if (nprev < 2)
          converged = 0;


/*         if (NULL != frontiers[iteration*nscombs+scno]) { */
/*           printf(" -- frontier "); */
/*           print_frontier(frontiers[iteration*nscombs+scno],sc->nargs); */
/*         } */
      }
    }

#if 1
    if (converged) {
/*       printf("Frontiers have converged\n"); */
      iteration++;
      break;
    }
#endif
  }
  int usediter = iteration-1;

  if (strictdebug) {
    for (scno = 0; scno < nscombs; scno++) {
      sc = sctotest[scno];
      if (!isgr[scno] && strcmp(sc->name,"PROG")) {
        scomb *approx = get_fscomb("%s#%d",sc->name,usediter);
        printf("%s is strict in {",approx->name);
        int a;
        int nstrict = 0;
        for (a = 0; a < sc->nargs; a++) {
          if (approx->strictin[a]) {
            if (0 < nstrict++)
              printf(",");
            printf("%s",sc->argnames[a]);
          }
        }
        printf("}\n");
      }
    }
  }

  for (scno = 0; scno < nscombs; scno++) {
    sc = sctotest[scno];
    assert(!sc->strictin);
    sc->strictin = (int*)malloc(sc->nargs*sizeof(int));
    scomb *approx = get_fscomb("%s#%d",sc->name,usediter);
    assert(approx);

    if (approx->strictin)
      memcpy(sc->strictin,approx->strictin,sc->nargs*sizeof(int));
    else
      memset(sc->strictin,0,sc->nargs*sizeof(int));
  }

  if (strictdebug)
    exit(0);

  for (iteration--; 0 <= iteration; iteration--) {
    for (scno = 0; scno < nscombs; scno++)
      flist_free(frontiers[iteration*nscombs+scno]);
  }

/*   printf("ntestreductions = %d\n",ntestreductions); */
/*   printf("# scombs = %d\n",scomb_count()); */
  free(frontiers);
  free(sctotest);
  free(isgr);

  scomb_free_list(oldlast);
  lastsc = oldlast;

  for (sc = scombs; sc; sc = sc->next)
    annotate(sc);
/*   if (trace) */
/*     print_scombs2(); */

#endif
}
