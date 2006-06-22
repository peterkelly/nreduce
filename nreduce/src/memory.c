#define MEMORY_C

#include "grammar.tab.h"
#include "nreduce.h"
#include "gcode.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <assert.h>
#include <ctype.h>
#include <stdarg.h>
#include <math.h>

cell *global_root = NULL;
int trace = 0;

extern cell **globcells;
extern int *addressmap;

int repl_histogram[NUM_CELLTYPES];



cell *freeptr = NULL;

/* dumpentry *dump = NULL; */

dumpentry *dumpstack = NULL;
int dumpalloc = 0;
int dumpcount = 0;

dumpentry *pushdump()
{
  if (dumpalloc <= dumpcount) {
    if (0 == dumpalloc)
      dumpalloc = 2;
    else
      dumpalloc *= 2;
    dumpstack = (dumpentry*)realloc(dumpstack,dumpalloc*sizeof(dumpentry));
  }
  dumpcount++;
/*   printf("pushdump: dumpstack = %p, dumpcount = %d\n",dumpstack,dumpcount); */
  return &dumpstack[dumpcount-1];
}

cell **stack = NULL;
int stackalloc = 0;
int stackcount = 0;
int stackbase = 0;

block *blocks = NULL;
int nblocks = 0;

int maxstack = 0;
int ncells = 0;
int maxcells = 0;
int nallocs = 0;
int totalallocs = 0;
int nscombappls = 0;
int nresindnoch = 0;
int nresindch = 0;
int nALLOCs = 0;
int nALLOCcells = 0;
int ndumpallocs = 0;
int nunwindsvar = 0;
int nunwindswhnf = 0;
int nreductions = 0;

cell *pinned = NULL;
cell *globnil = NULL;
cell *globtrue = NULL;

void initmem()
{
  memset(&repl_histogram,0,NUM_CELLTYPES*sizeof(int));
  memset(&op_usage,0,OP_COUNT*sizeof(int));
  globnil = alloc_cell();
  globnil->tag = TYPE_NIL | FLAG_PINNED;
  globtrue = alloc_cell();
  globtrue->tag = TYPE_INT | FLAG_PINNED;
  globtrue->field1 = (void*)1;
}

cell *alloc_cell()
{
  cell *c;
  if (NULL == freeptr) {
    block *bl = (block*)calloc(1,sizeof(block));
    int i;
    bl->next = blocks;
    blocks = bl;
    for (i = 0; i < BLOCK_SIZE-1; i++)
      bl->cells[i].field1 = &bl->cells[i+1];

    freeptr = &bl->cells[0];
    nblocks++;
  }
  c = freeptr;
  freeptr = (cell*)freeptr->field1;
  ncells++;
  if (maxcells < ncells)
    maxcells = ncells;
  nallocs++;
  c->id = totalallocs;
  c->source = NULL;
  c->filename = NULL;
  c->lineno = -1;
  c->level = -1;
  totalallocs++;
/*   printf("allocated #%05d\n",c->id); */
  return c;
}

cell *alloc_sourcecell(const char *filename, int lineno)
{
  cell *c = alloc_cell();
  c->filename = filename;
  c->lineno = lineno;
  return c;
}

int nreachable = 0;
void mark(cell *c)
{
  assert(TYPE_EMPTY != celltype(c));
  if (!c || (c->tag & FLAG_MARKED))
    return;
  c->tag |= FLAG_MARKED;
  nreachable++;
  switch (celltype(c)) {
  case TYPE_IND:
    mark((cell*)c->field1);
    break;
  case TYPE_APPLICATION:
  case TYPE_CONS:
    c->field1 = resolve_ind((cell*)c->field1);
    c->field2 = resolve_ind((cell*)c->field2);
    mark((cell*)c->field1);
    mark((cell*)c->field2);
    break;
  case TYPE_LAMBDA:
    mark((cell*)c->field2);
    break;
  case TYPE_LETREC:
    mark((cell*)c->field1);
    mark((cell*)c->field2);
    break;
  case TYPE_VARDEF:
    mark((cell*)c->field2);
    break;
  case TYPE_VARLNK:
    mark((cell*)c->field1);
    mark((cell*)c->field2);
    break;
  default:
    break;
  }
}

void free_cell_fields(cell *c)
{
/*   if (hasfield1str(celltype(c))) */
/*     free((char*)c->field1); */
  switch (celltype(c)) {
  case TYPE_STRING:
  case TYPE_VARIABLE:
  case TYPE_LAMBDA:
  case TYPE_VARDEF:
    free((char*)c->field1);
    break;
  }
}

char *collect_ebp = NULL;
char *collect_esp = NULL;

int ncollections = 0;
void collect()
{
  block *bl;
  int i;
  scomb *sc;
/*   int npinned = 0; */
/*   int ninuse = 0; */
/*   int nind = 0; */
/*   int histogram[NUM_CELLTYPES]; */
/*   memset(&histogram,0,NUM_CELLTYPES*sizeof(int)); */

/*   printf("collect()\n"); */

  ncollections++;
  nallocs = 0;

  /* clear all marks */
  for (bl = blocks; bl; bl = bl->next)
    for (i = 0; i < BLOCK_SIZE; i++)
      bl->cells[i].tag &= ~FLAG_MARKED;
  nreachable = 0;

  /* mark phase */
/*   mark(global_root); */

  if (NULL != collect_ebp) {
    char *p;
    /* native stack */
    assert(collect_ebp >= collect_esp);
/*     printf("collect(): stack bytes = %d\n",collect_ebp-collect_esp); */
    for (p = collect_ebp-4; p >= collect_esp; p -= 4) {
      cell *cp;

      *((cell**)p) = resolve_ind(*((cell**)p));

      cp = *((cell**)p);
/*       printf("collect(): p = %p, cp = %p\n",p,cp); */
      mark(cp);
    }
  }
  else {
    for (i = 0; i < stackcount; i++) {
      stack[i] = resolve_ind(stack[i]);
      mark(stack[i]);
    }
  }
  for (sc = scombs; sc; sc = sc->next)
    mark(sc->body);
/*   printf("\ncollect(): %d/%d cells still reachable\n",nreachable,ncells); */

  /* sweep phase */
  for (bl = blocks; bl; bl = bl->next) {
    for (i = 0; i < BLOCK_SIZE; i++) {
      if (!(bl->cells[i].tag & FLAG_MARKED) &&
          (TYPE_EMPTY != celltype(&bl->cells[i]))) {

        
        if (!(bl->cells[i].tag & FLAG_PINNED)) {
/*           printf("  free #%05d\n",bl->cells[i].id); */
/*           debug(0,"collect(): freeing #%05d\n",bl->cells[i].id); */
          free_cell_fields(&bl->cells[i]);
          bl->cells[i].tag = TYPE_EMPTY;
          bl->cells[i].field1 = freeptr;
          freeptr = &bl->cells[i];
          ncells--;
        }
/*         else { */
/*           npinned++; */
/*           printf("pinned: "); */
/*           print_code(&bl->cells[i]); */
/*           printf("\n"); */
/*         } */
      }
/*       else if (TYPE_EMPTY != celltype(&bl->cells[i])) { */
/*         if (TYPE_IND == celltype(&bl->cells[i])) */
/*           nind++; */
/*         ninuse++; */
/*         histogram[celltype(&bl->cells[i])]++; */
/*       } */
    }
  }
/*   printf("collect(): npinned = %d\n",npinned); */
/*   printf("collect(): ninuse = %d\n",ninuse); */
/*   printf("collect(): nind = %d\n",nind); */
/*   for (i = 0; i < NUM_CELLTYPES; i++) { */
/*     printf("# remaining %-12s = %d\n",cell_types[i],histogram[i]); */
/*   } */
}

void cleanup()
{
  block *bl;
  int i;
/*   scomb *sc = scombs; */
/*   while (sc) { */
/*     scomb *next = sc->next; */
/*     scomb_free(sc); */
/*     sc = next; */
/*   } */
/*   scombs = NULL; */
   scomb_free_list(&scombs);

  stackcount = 0;
  global_root = NULL;

  for (bl = blocks; bl; bl = bl->next)
    for (i = 0; i < BLOCK_SIZE; i++)
      bl->cells[i].tag &= ~FLAG_PINNED;

  collect();

  bl = blocks;
  while (bl) {
    block *next = bl->next;
    free(bl);
    bl = next;
  }
  free(stack);
  free(addressmap);
  free(globcells);
  free(dumpstack);
}

void growstack()
{
  if (0 == stackalloc)
    stackalloc = 16;
  else
    stackalloc *= 2;
  stack = (cell**)realloc(stack,stackalloc*sizeof(cell*));
}

void push(cell *c)
{
/*   printf("PUSH %p\n",c); */
  if (stackcount == stackalloc) {
    if (stackcount >= STACK_LIMIT) {
      fprintf(stderr,"Out of stack space\n");
      exit(1);
    }
    growstack();
  }
  stack[stackcount++] = c;
  if (maxstack < stackcount)
    maxstack = stackcount;
}

void insert(cell *c, int pos)
{
  if (stackcount == stackalloc)
    growstack();
  memmove(&stack[pos+1],&stack[pos],(stackcount-pos)*sizeof(cell*));
  stackcount++;
  stack[pos] = c;
}

cell *pop()
{
  assert(0 < stackcount);
/*   printf("POP %p\n",stack[stackcount-1]); */
  return resolve_ind(stack[--stackcount]);
}

cell *top()
{
  assert(0 < stackcount);
  return resolve_ind(stack[stackcount-1]);
}

cell *stackat(int s)
{
  assert(0 <= s);
  assert(s < stackcount);
  return resolve_ind(stack[s]);
}

void statistics()
{
  int i;
  int total;
  printf("maxstack = %d\n",maxstack);
  printf("totalallocs = %d\n",totalallocs);
  printf("ncells = %d\n",ncells);
  printf("maxcells = %d\n",maxcells);
  printf("nblocks = %d\n",nblocks);
  printf("ncollections = %d\n",ncollections);
  printf("nscombappls = %d\n",nscombappls);
  printf("nresindnoch = %d\n",nresindnoch);
  printf("nresindch = %d\n",nresindch);
  printf("nALLOCs = %d\n",nALLOCs);
  printf("nALLOCcells = %d\n",nALLOCcells);
  printf("resolve_ind() total = %d\n",nresindch+nresindnoch);
  printf("resolve_ind() ratio = %f\n",((double)nresindch)/((double)nresindnoch));
  printf("ndumpallocs = %d\n",ndumpallocs);
  printf("nunwindsvar = %d\n",nunwindsvar);
  printf("nunwindswhnf = %d\n",nunwindswhnf);
  printf("nreductions = %d\n",nreductions);

  for (i = 0; i < NUM_CELLTYPES; i++)
    printf("repl_histogram[%s] = %d\n",cell_types[i],repl_histogram[i]);

  total = 0;
  for (i = 0; i < OP_COUNT; i++) {
    printf("usage(%s) = %d\n",op_names[i],op_usage[i]);
    total += op_usage[i];
  }
  printf("usage total = %d\n",total);
}


cell *resolve_ind2(cell *c)
{
  while (TYPE_IND == celltype(c))
    c = (cell*)c->field1;
  return c;
}

#ifndef INLINE_RESOLVE_IND
cell *resolve_ind(cell *c)
{
/*   cell *orig = c; */
/*   int n = 0; */
  while (TYPE_IND == celltype(c)) {
/*     assert(c->field1 != c); */
    c = (cell*)c->field1;
/*     n++; */
  }

/*   if (orig == c) */
/*     nresindnoch++; */
/*   else */
/*     nresindch++; */

/*   printf("resolve_ind: n = %d\n",n); */
  return c;
}
#endif

cell *resolve_source(cell *c)
{
  while (c->source) {
    assert(c->source != c);
    c = c->source;
  }
  return c;
}

void copy_raw(cell *dest, cell *source)
{
  dest->tag = source->tag & TAG_MASK;
  dest->field1 = source->field1;
  dest->field2 = source->field2;
}

void copy_cell(cell *redex, cell *source)
{
  assert(redex != source);
  copy_raw(redex,source);
  if ((TYPE_STRING == celltype(source)) ||
      (TYPE_VARIABLE == celltype(source)) ||
      (TYPE_LAMBDA == celltype(source)) ||
      (TYPE_VARDEF == celltype(source)))
    redex->field1 = strdup((char*)redex->field1);
}

void clearflag(int flag)
{
  block *bl;
  int i;
  int count = 0;
  for (bl = blocks; bl; bl = bl->next) {
    for (i = 0; i < BLOCK_SIZE; i++) {
      bl->cells[i].tag &= ~flag;
      count++;
    }
  }
/*   printf("clearflag: processed %d cells\n",count); */
}

void setneedclear_r(cell *c)
{
  if (c->tag & FLAG_NEEDCLEAR)
    return;
  c->tag |= FLAG_NEEDCLEAR;

  switch (celltype(c)) {
  case TYPE_APPLICATION:
  case TYPE_CONS:
    setneedclear_r((cell*)c->field1);
    setneedclear_r((cell*)c->field2);
    break;
  case TYPE_LAMBDA:
    setneedclear_r((cell*)c->field2);
    break;
  case TYPE_IND:
    setneedclear_r((cell*)c->field1);
    break;
  case TYPE_LETREC:
  case TYPE_VARDEF:
  case TYPE_VARLNK:
    assert(0);
    break;
  }
}

void cleargraph_r(cell *c, int flag)
{
  if (!(c->tag & FLAG_NEEDCLEAR))
    return;
  c->tag &= ~FLAG_NEEDCLEAR;
  c->tag &= ~flag;

  switch (celltype(c)) {
  case TYPE_APPLICATION:
  case TYPE_CONS:
    cleargraph_r((cell*)c->field1,flag);
    cleargraph_r((cell*)c->field2,flag);
    break;
  case TYPE_LAMBDA:
    cleargraph_r((cell*)c->field2,flag);
    break;
  case TYPE_IND:
    cleargraph_r((cell*)c->field1,flag);
    break;
  case TYPE_LETREC:
  case TYPE_VARDEF:
  case TYPE_VARLNK:
    assert(0);
    break;
  }
}

void cleargraph(cell *root, int flag)
{
  setneedclear_r(root);
  cleargraph_r(root,flag);
}

void print_stack(int redex, cell **stk, int size, int dir)
{
  int i;
/*   dumpentry *de; */
/*   int dumpcount = 0; */

/*   for (de = dump; de; de = de->next) */
/*     dumpcount++; */

  if (dir)
    i = size-1;
  else
    i = 0;


  while (1) {
    int wasdump = 0;
    cell *c;
    int pos = dir ? (size-1-i) : i;
    int dpos;

    if (dir && i < 0)
      break;

    if (!dir && (i >= size))
      break;

    c = resolve_ind(stk[i]);

    for (dpos = 0; dpos < dumpcount; dpos++) {
      if (dumpstack[dpos].stackcount-1 == i) {
        debug(0,"  D%-2d ",dumpcount-1-dpos);
        wasdump = 1;
        break;
      }
    }

    if (!wasdump)
      debug(0,"      ");

/*     debug(0,"%2d: %p/#%05d #%05d %12s ",pos,stk[i],stk[i]->id,c->id,cell_types[celltype(c)]); */
    debug(0,"%2d: #%05d %12s ",pos,c->id,cell_types[celltype(c)]);
    print_code(c);
    if (i == redex)
      debug(0," <----- redex");
    debug(0,"\n");

    if (dir)
      i--;
    else
      i++;
  }
}

void adjust_stack(int nargs)
{
  int i;
  for (i = stackcount-1; i >= stackcount-nargs; i--) {
    assert(TYPE_APPLICATION == celltype(stackat(i-1)));
    stack[i] = (cell*)stackat(i-1)->field2;
  }
}

