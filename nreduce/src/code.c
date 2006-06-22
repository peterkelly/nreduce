#include "grammar.tab.h"
#include "nreduce.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <assert.h>
#include <ctype.h>
#include <stdarg.h>
#include <math.h>

cell *sub_letrecs(cell *c, scomb *sc)
{
  cell *res = c;

  if (!c || (c->tag & FLAG_PROCESSED))
    return res;

  c->tag |= FLAG_PROCESSED;

  push(c);
  switch (celltype(c)) {
  case TYPE_IND:
    assert(0);
    break;
  case TYPE_EMPTY:
    break;
  case TYPE_APPLICATION:
    c->field1 = sub_letrecs((cell*)c->field1,sc);
    c->field2 = sub_letrecs((cell*)c->field2,sc);
    break;
  case TYPE_LAMBDA:
    c->field2 = sub_letrecs((cell*)c->field2,sc);
    break;
  case TYPE_BUILTIN:
    break;
  case TYPE_CONS:
    c->field1 = sub_letrecs((cell*)c->field1,sc);
    c->field2 = sub_letrecs((cell*)c->field2,sc);
    break;
  case TYPE_NIL:
    break;
  case TYPE_INT:
    break;
  case TYPE_DOUBLE:
    break;
  case TYPE_STRING:
    break;
  case TYPE_VARIABLE: {
    int chain = 0;
    int bletrec;

    cell *prevres = NULL;
    do {
      int st = stackcount-2;
      int found = 0;

      bletrec = 0;

      while ((0 <= st) && !found) {
        cell *p = stackat(st);
        if (TYPE_LAMBDA == celltype(p)) {
          if (!strcmp((char*)p->field1,(char*)res->field1))
            found = 1;
        }
        else if (TYPE_LETREC == celltype(p)) {
          cell *lnk;
          for (lnk = (cell*)p->field1; lnk; lnk = (cell*)lnk->field2) {
            cell *def = (cell*)lnk->field1;
            if (!strcmp((char*)def->field1,(char*)res->field1)) {
              res = (cell*)def->field2;
              found = 1;
              bletrec = 1;
            }
          }
        }
        st--;
      }
      if (!found && (NULL != sc)) {
        int argno;
        for (argno = 0; argno < sc->nargs; argno++)
          if (!strcmp((char*)res->field1,sc->argnames[argno]))
            found = 1;
      }
      if (!found) {
        if (NULL != get_scomb((char*)res->field1))
          break;

        if (res->filename)
          fprintf(stderr,"%s:%d: ",res->filename,res->lineno);
        fprintf(stderr,"Variable not bound: %s\n",(char*)res->field1);

  /* FIXME: disable this to support supercombinators */
        exit(1);
      }
      chain++;

      if (prevres == res) {
        fprintf(stderr,"Invalid letrec recursion: ");
        print_codef(stderr,c);
        fprintf(stderr,"\n");
        exit(1);
      }
      prevres = res;
    } while ((TYPE_VARIABLE == celltype(res)) && bletrec);

    break;
  }
  case TYPE_SCREF:
    break;
  case TYPE_LETREC: {
    cell *lnk;
    for (lnk = (cell*)c->field1; lnk; lnk = (cell*)lnk->field2) {
      cell *def = (cell*)lnk->field1;
      sub_letrecs((cell*)def->field2,sc);
    }
    res = sub_letrecs((cell*)c->field2,sc);
    break;
  }
  case TYPE_VARDEF:
    assert(0);
    break;
  case TYPE_VARLNK:
    assert(0);
    break;
  }
  pop();
  return res;
}

cell *suball_letrecs(cell *root, scomb *sc)
{
  /* FIXME: check variable usage to make sure all variables are bound in the letrec
     or above.
     How should be handle situations like ths:
       letrec
          foo (+ x 1),
          x 4,
          bar: (!x.* foo 2)
     -- what is the x in foo bound to? I think it should be the letrec x (not bar's !x)
     -- solution: rename variables to give them unique names before calling suball_letrecs()
     Same issue for:
     !x.letrec foo (+ x 1)
               x 4
        in * foo 2
  */
  clearflag(FLAG_PROCESSED);
  root = sub_letrecs(root,sc);
  return root;
}
