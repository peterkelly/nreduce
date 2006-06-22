#if 0
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
  print_comp("ES",c);
  int oldcount = stackcount;
  assert(TYPE_IND != celltype(c));

  cdepth++;
  switch (celltype(c)) {
  case TYPE_NIL:
  case TYPE_INT:
  case TYPE_DOUBLE:
  case TYPE_STRING: /* E[I] */
    c->tag |= FLAG_PINNED;
    gprogram_add_ginstr(gp,OP_PUSHCELL,(int)c,0,si);
    gprogram_add_ginstr(gp,OP_MKAP,n,0,si);
    gprogram_add_ginstr(gp,OP_EVAL,0,0,si);
    break;
  case TYPE_VARIABLE: /* E[x] */
    push_varref(gp,(char*)c->field1,d,pm,si);
    gprogram_add_ginstr(gp,OP_MKAP,n,0,si);
    gprogram_add_ginstr(gp,OP_EVAL,0,0,si);
    break;
  case TYPE_SCREF: /* E[f] */
    push_scref(gp,(scomb*)c->field1,d,pm,si);
    gprogram_add_ginstr(gp,OP_MKAP,n,0,si);
    gprogram_add_ginstr(gp,OP_EVAL,0,0,si);
    break;
  case TYPE_BUILTIN: /* another form of E[f] */
    gprogram_add_ginstr(gp,OP_PUSHCELL,(int)globcells[(int)c->field1],0,si);
    gprogram_add_ginstr(gp,OP_MKAP,n,0,si);
    gprogram_add_ginstr(gp,OP_EVAL,0,0,si);
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
        gprogram_add_ginstr(gp,OP_JFALSE,0,0,si);

        tempsi = stackinfo_new(si);
        ES(gp,et,d,pm,n,tempsi);
        jendaddr = gp->count;
        gprogram_add_ginstr(gp,OP_JUMP,0,0,tempsi);
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
            C(gp,stack[stackcount-nargs+i],d+i,pm,si,0);
        }
        gprogram_add_ginstr(gp,OP_BIF,bif,0,si);
        gprogram_add_ginstr(gp,OP_MKAP,n,0,si);
        gprogram_add_ginstr(gp,OP_EVAL,0,0,si);
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
        gprogram_add_ginstr(gp,OP_CALL,NUM_BUILTINS+sc->index,0,si);
        gprogram_add_ginstr(gp,OP_MKAP,n,0,si);
        gprogram_add_ginstr(gp,OP_EVAL,0,0,si);
        break;
      }
    }
#endif

    if (c->tag & FLAG_STRICT)
      E(gp,(cell*)c->field2,d,pm,si);
    else
      C(gp,(cell*)c->field2,d,pm,si,0);
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
  print_comp("E",c);
  int oldcount = stackcount;
  assert(TYPE_IND != celltype(c));

  cdepth++;
  switch (celltype(c)) {
  case TYPE_NIL:
  case TYPE_INT:
  case TYPE_DOUBLE:
  case TYPE_STRING: /* E[I] */
    c->tag |= FLAG_PINNED;
    gprogram_add_ginstr(gp,OP_PUSHCELL,(int)c,0,si);
    break;
  case TYPE_VARIABLE: /* E[x] */
    push_varref(gp,(char*)c->field1,d,pm,si);
    gprogram_add_ginstr(gp,OP_EVAL,0,0,si);
    break;
  case TYPE_SCREF: /* E[f] */
    push_scref(gp,(scomb*)c->field1,d,pm,si);
    gprogram_add_ginstr(gp,OP_EVAL,0,0,si);
    break;
  case TYPE_BUILTIN: /* another form of E[f] */
    gprogram_add_ginstr(gp,OP_PUSHCELL,(int)globcells[(int)c->field1],0,si);
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
        gprogram_add_ginstr(gp,OP_JFALSE,0,0,si);

        tempsi = stackinfo_new(si);
        E(gp,arg1,d,pm,tempsi);
        jendaddr = gp->count;
        gprogram_add_ginstr(gp,OP_JUMP,0,0,tempsi);
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
        gprogram_add_ginstr(gp,OP_BIF,bif,0,si);
        if (!builtin_info[bif].strictres)
          gprogram_add_ginstr(gp,OP_EVAL,0,0,si);
        break;
      }
    }
#ifdef ALLSTRICT
    else if (TYPE_VARIABLE == celltype(fun)) {
      scomb *sc = (scomb*)fun->field1;
/*       assert(sc); */
      if (sc && (nargs == sc->nargs)) {
        int i;
        for (i = 0; i < nargs; i++)
          E(gp,stack[stackcount-nargs+i],d+i,pm,si);
        gprogram_add_ginstr(gp,OP_CALL,NUM_BUILTINS+sc->index,0,si);
        gprogram_add_ginstr(gp,OP_EVAL,0,0,si);
        break;
      }
    }
#endif

#ifdef ESOPT
    ES(gp,c,d,pm,0,si);
#else
    C(gp,c,d,pm,si);
    gprogram_add_ginstr(gp,OP_EVAL,0,0,si);
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
  print_comp("RS",c);
  int oldcount = stackcount;
  assert(TYPE_IND != celltype(c));

  cdepth++;
  switch (celltype(c)) {
  case TYPE_NIL:
  case TYPE_INT:
  case TYPE_DOUBLE:
  case TYPE_STRING:
    c->tag |= FLAG_PINNED;
    gprogram_add_ginstr(gp,OP_PUSHCELL,(int)c,0,si);
    gprogram_add_ginstr(gp,OP_UPDATE,d-n+1,0,si);
    gprogram_add_ginstr(gp,OP_POP,d-n,0,si);
    gprogram_add_ginstr(gp,OP_UNWIND,0,0,si);
    break;
  case TYPE_VARIABLE: /* RS[x] p d n */
    push_varref(gp,(char*)c->field1,d,pm,si);
    gprogram_add_ginstr(gp,OP_MKAP,n,0,si);
    gprogram_add_ginstr(gp,OP_UPDATE,d-n+1,0,si);
    gprogram_add_ginstr(gp,OP_POP,d-n,0,si);
    gprogram_add_ginstr(gp,OP_UNWIND,0,0,si);
    break;
  case TYPE_SCREF: /* RS[f] p d n */
    push_scref(gp,(scomb*)c->field1,d,pm,si);
    gprogram_add_ginstr(gp,OP_MKAP,n,0,si);
    gprogram_add_ginstr(gp,OP_UPDATE,d-n+1,0,si);
    gprogram_add_ginstr(gp,OP_POP,d-n,0,si);
    gprogram_add_ginstr(gp,OP_UNWIND,0,0,si);
    break;
  case TYPE_BUILTIN: /* another form of R[f] p d */
    gprogram_add_ginstr(gp,OP_PUSHCELL,(int)globcells[(int)c->field1],0,si);
    gprogram_add_ginstr(gp,OP_MKAP,n,0,si);
    gprogram_add_ginstr(gp,OP_UPDATE,d-n+1,0,si);
    gprogram_add_ginstr(gp,OP_POP,d-n,0,si);
    gprogram_add_ginstr(gp,OP_UNWIND,0,0,si);
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
        gprogram_add_ginstr(gp,OP_JFALSE,0,0,si);

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
        gprogram_add_ginstr(gp,OP_BIF,bif,0,si);
        if (!builtin_info[bif].strictres)
          gprogram_add_ginstr(gp,OP_EVAL,0,0,si);
        gprogram_add_ginstr(gp,OP_MKAP,n,0,si);
        gprogram_add_ginstr(gp,OP_UPDATE,d-n+1,0,si);
        gprogram_add_ginstr(gp,OP_POP,d-n,0,si);
        gprogram_add_ginstr(gp,OP_UNWIND,0,0,si);
        break;
      }
    }
#ifdef ALLSTRICT
    else if (TYPE_VARIABLE == celltype(fun)) {
      scomb *sc = (scomb*)fun->field1;
/*       assert(sc); */
      if (sc && (nargs == sc->nargs)) {
        int i;
        for (i = 0; i < nargs; i++)
          E(gp,stack[stackcount-nargs+i],d+i,pm,si);
        gprogram_add_ginstr(gp,OP_CALL,NUM_BUILTINS+sc->index,0,si);
        gprogram_add_ginstr(gp,OP_EVAL,0,0,si);
        gprogram_add_ginstr(gp,OP_MKAP,n,0,si);
        gprogram_add_ginstr(gp,OP_UPDATE,d-n+1,0,si);
        gprogram_add_ginstr(gp,OP_POP,d-n,0,si);
        gprogram_add_ginstr(gp,OP_UNWIND,0,0,si);
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
  print_comp("R",c);
  int oldcount = stackcount;
  assert(TYPE_IND != celltype(c));

  cdepth++;
  switch (celltype(c)) {
  case TYPE_NIL:
  case TYPE_INT:
  case TYPE_DOUBLE:
  case TYPE_STRING: /* R[I] p d */
    c->tag |= FLAG_PINNED;
    gprogram_add_ginstr(gp,OP_PUSHCELL,(int)c,0,si);
    gprogram_add_ginstr(gp,OP_UPDATE,d+1,0,si);
    gprogram_add_ginstr(gp,OP_POP,d,0,si);
    gprogram_add_ginstr(gp,OP_RETURN,0,0,si);
    break;
  case TYPE_VARIABLE: /* R[x] p d */
    push_varref(gp,(char*)c->field1,d,pm,si);
    gprogram_add_ginstr(gp,OP_EVAL,0,0,si); /* -- is this necessary? it causes problems... */
    gprogram_add_ginstr(gp,OP_UPDATE,d+1,0,si);
    gprogram_add_ginstr(gp,OP_POP,d,0,si);
    gprogram_add_ginstr(gp,OP_UNWIND,0,0,si);
    break;
  case TYPE_SCREF: /* R[f] p d */
    push_scref(gp,(scomb*)c->field1,d,pm,si);
    gprogram_add_ginstr(gp,OP_EVAL,0,0,si); /* -- is this necessary? it causes problems... */
    gprogram_add_ginstr(gp,OP_UPDATE,d+1,0,si);
    gprogram_add_ginstr(gp,OP_POP,d,0,si);
    gprogram_add_ginstr(gp,OP_UNWIND,0,0,si);
    break;
  case TYPE_BUILTIN: /* another form of R[f] p d */
    gprogram_add_ginstr(gp,OP_PUSHCELL,(int)globcells[(int)c->field1],0,si);
    gprogram_add_ginstr(gp,OP_EVAL,0,0,si); /* -- is this necessary? it causes problems... */
    gprogram_add_ginstr(gp,OP_UPDATE,d+1,0,si);
    gprogram_add_ginstr(gp,OP_POP,d,0,si);
    gprogram_add_ginstr(gp,OP_UNWIND,0,0,si);
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
        gprogram_add_ginstr(gp,OP_JFALSE,0,0,si);

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
        gprogram_add_ginstr(gp,OP_BIF,bif,0,si);
        if (!builtin_info[bif].strictres)
          gprogram_add_ginstr(gp,OP_EVAL,0,0,si);
        gprogram_add_ginstr(gp,OP_UPDATE,d+1,0,si);
        gprogram_add_ginstr(gp,OP_POP,d,0,si);
        gprogram_add_ginstr(gp,OP_RETURN,0,0,si);
        break;
      }
    }
#ifdef ALLSTRICT
    else if (TYPE_VARIABLE == celltype(fun)) {
      scomb *sc = (scomb*)fun->field1;
/*       assert(sc); */
      if (sc && (nargs == sc->nargs)) {
        int i;
        for (i = 0; i < nargs; i++)
          E(gp,stack[stackcount-nargs+i],d+i,pm,si);
        gprogram_add_ginstr(gp,OP_CALL,NUM_BUILTINS+sc->index,0,si);
        gprogram_add_ginstr(gp,OP_EVAL,0,0,si);
        gprogram_add_ginstr(gp,OP_UPDATE,d+1,0,si);
        gprogram_add_ginstr(gp,OP_POP,d,0,si);
        gprogram_add_ginstr(gp,OP_RETURN,0,0,si);
        break;
      }
    }
#endif

    /* R [ E1 E2 ] */
#ifdef RSOPT
    RS(gp,c,d,pm,0,si);
#else
    C(gp,c,d,pm,si);
    gprogram_add_ginstr(gp,OP_UPDATE,d+1,0,si);
    gprogram_add_ginstr(gp,OP_POP,d,0,si);
    gprogram_add_ginstr(gp,OP_UNWIND,0,0,si);
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
#endif

