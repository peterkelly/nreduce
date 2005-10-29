/*
 * This file is part of the GridXSLT project
 * Copyright (C) 2005 Peter Kelly (pmk@cs.adelaide.edu.au)
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

#include "compile.h"
#include "builtins.h"
#include "util/List.h"
#include "util/macros.h"
#include "util/debug.h"
#include "dataflow/optimization.h"
#include "dataflow/validity.h"
#include <assert.h>
#include <string.h>

namespace GridXSLT {

class Compilation;
class Template;

class Compilation {
  DISABLE_COPY(Compilation)
public:
  Compilation();
  ~Compilation();

  void set_and_move_cursor(OutputPort **cursor, Instruction *destinstr, int destp,
                                Instruction *newpos, int newdestno);

  Instruction *specialop(Function *fun, const String &ns,
                              const String &name, int nargs, sourceloc sloc);
  int compileBinaryOp(Function *fun, Expression *e,
                              OutputPort **cursor, const String &ns, const String &name);
  int stmtInConditional(Statement *stmt);
  int exprInConditional(Expression *e);
  void useVar(Function *fun, OutputPort *outport,
                       OutputPort **cursor, int need_gate, sourceloc sloc);
  OutputPort *resolveVar(Function *fun, Expression *varref);
  void compileString(Function *fun, const String &str,
                                OutputPort **cursor, sourceloc sloc);
  int stmtBelow(Statement *sn, Statement *below);
  int haveVarReference(list *l, OutputPort *top_outport);
  int findReferencedVarsExpr(Function *fun, Expression *e,
                                     Statement *below, list **vars);
  int findReferencedVarsStmt(Function *fun, Statement *sn,
                                Statement *below, list **vars);
  void initParamInstructions(Function *fun,
                                    List<SequenceType> &seqtypes, OutputPort **outports);
  int compileInnerFunction(Function *fun, Statement *sn,
                                 Instruction **mapout, OutputPort **cursor, sourceloc sloc);
  void compileConditional(Function *fun, OutputPort **cursor,
                                    OutputPort **condcur, OutputPort **truecur,
                                    OutputPort **falsecur, sourceloc sloc);
  int compileExpr(Function *fun, Expression *e, OutputPort **cursor);
  int compileSequenceItem(Function *fun, Statement *sn,
                                  OutputPort **cursor);
  int compileSequence(Function *fun, Statement *parent,
                             OutputPort **cursor);
  int compileFunctionContents(Function *fun, Statement *parent,
                                OutputPort **cursor);
  int compileFunction(Statement *sn, Function *fun);
  int compileApplyFunction(Function **ifout, const char *mode);
  int compileApplyTemplates(Function *fun,
                                    OutputPort **cursor,
                                    const char *mode, sourceloc sloc);
  void compileOrderedTemplateList();
  int compileDefaultTemplate();
  int compile2();






  Error *m_ei;
  xslt_source *m_source;
  Program *m_program;

  Schema *m_schema;

  ManagedPtrList<Template*> m_templates;
  int m_nextanonid;
};

class Template {
public:
  Template();
  ~Template() ;

  Expression *pattern;
  Function *fun;
  bool builtin;
};

};

using namespace GridXSLT;

Template::Template()
  : pattern(NULL), fun(NULL), builtin(false)
{
}

Template::~Template()
{
  if (builtin)
    delete pattern;
}

Compilation::Compilation()
  : m_ei(NULL),
    m_source(NULL),
    m_program(NULL),
    m_schema(NULL),
    m_nextanonid(0)
{
}

Compilation::~Compilation()
{
}








#define ANON_TEMPLATE_NAMESPACE "http://gridxslt.sourceforge.net/anon-template"

void Compilation::set_and_move_cursor(OutputPort **cursor, Instruction *destinstr, int destp,
                                Instruction *newpos, int newdestno)
{
  newpos->m_outports[newdestno].dest = (*cursor)->dest;
  newpos->m_outports[newdestno].destp = (*cursor)->destp;
  (*cursor)->dest = destinstr;
  (*cursor)->destp = destp;
  *cursor = &newpos->m_outports[newdestno];
}

Instruction *Compilation::specialop(Function *fun, const String &ns,
                              const String &name, int nargs, sourceloc sloc)
{
  return fun->addBuiltinInstruction(NSName(ns,name),nargs,sloc);
}

int Compilation::compileBinaryOp(Function *fun, Expression *e,
                              OutputPort **cursor, const String &ns, const String &name)
{
  Instruction *dup = fun->addInstruction(OP_DUP,e->m_sloc);
  Instruction *binop = specialop(fun,ns,name,2,e->m_sloc);
  OutputPort *left_cursor = &dup->m_outports[0];
  OutputPort *right_cursor = &dup->m_outports[1];

  left_cursor->dest = binop;
  left_cursor->destp = 0;
  right_cursor->dest = binop;
  right_cursor->destp = 1;
  set_and_move_cursor(cursor,dup,0,binop,0);

  CHECK_CALL(compileExpr(fun,e->m_left,&left_cursor))
  CHECK_CALL(compileExpr(fun,e->m_right,&right_cursor))

  return 0;
}

int Compilation::stmtInConditional(Statement *stmt)
{
  Statement *sn;
  for (sn = stmt->m_parent; sn; sn = sn->m_parent)
    if ((XSLT_INSTR_CHOOSE == sn->m_type) ||
        (XSLT_INSTR_IF == sn->m_type))
      return 1;
  return 0;
}

int Compilation::exprInConditional(Expression *e)
{
  Expression *p;
  Expression *prev = e;
  for (p = e->m_parent; p; p = p->m_parent) {
    if ((XPATH_EXPR_IF == p->m_type) && (prev != p->m_conditional))
      return 1;
    prev = p;
  }
  return stmtInConditional(e->m_stmt);
}

void Compilation::useVar(Function *fun, OutputPort *outport,
                       OutputPort **cursor, int need_gate, sourceloc sloc)
{
  Instruction *dest = (*cursor)->dest;
  int destp = (*cursor)->destp;
  OutputPort *finalout = NULL;

  if (need_gate) {
    Instruction *gate = fun->addInstruction(OP_GATE,sloc);
    set_and_move_cursor(cursor,gate,1,gate,0);
    dest = gate;
    destp = 0;
  }

  if (OP_SWALLOW == outport->dest->m_opcode) {
    fun->deleteInstruction(outport->dest);
    outport->dest = dest;
    outport->destp = destp;
    finalout = outport;
  }
  else {
    Instruction *dup = fun->addInstruction(OP_DUP,sloc);
    dup->m_outports[0].dest = outport->dest;
    dup->m_outports[0].destp = outport->destp;
    dup->m_outports[1].dest = dest;
    dup->m_outports[1].destp = destp;
    finalout = &dup->m_outports[1];
    outport->dest = dup;
    outport->destp = 0;
  }

  if (!need_gate) {
    Instruction *swallow = fun->addInstruction(OP_SWALLOW,sloc);
    (*cursor)->dest = swallow;
    (*cursor)->destp = 0;
  }
  *cursor = finalout;
}

OutputPort *Compilation::resolveVar(Function *fun, Expression *varref)
{
  Expression *defexpr = NULL;
  Statement *defnode = NULL;
  OutputPort *outport = NULL;
  int i;

  Expression_resolve_var(varref,varref->m_qn,&defexpr,&defnode);
  if (NULL != defexpr)
    outport = defexpr->m_outp;
  else if (NULL != defnode)
    outport = defnode->m_outp;
  else
    return NULL;

  for (i = 0; i < fun->m_nparams; i++)
    if (fun->m_params[i].varsource == outport)
      return fun->m_params[i].outport;

  return outport;
}

void Compilation::compileString(Function *fun, const String &str,
                                OutputPort **cursor, sourceloc sloc)
{
  Instruction *instr = fun->addInstruction(OP_CONST,sloc);
  instr->m_outports[0].st = SequenceType(xs_g->string_type);
  instr->m_cvalue = Value(str);

  /* FIXME: this type is probably not broad enough... we should really accept sequences
     here too */
/*   instr->m_inports[0].st = SequenceType(xs_g->complex_ur_type); */
  set_and_move_cursor(cursor,instr,0,instr,0);
}

int Compilation::stmtBelow(Statement *sn, Statement *below)
{
  Statement *p;
  for (p = sn; p; p = p->m_parent)
    if (p == below)
      return 1;
  return 0;
}

typedef struct var_reference var_reference;

struct var_reference {
  Expression *ref;
  Expression *defexpr;
  Statement *defnode;
  OutputPort *top_outport;
  OutputPort *local_outport;
};

int Compilation::haveVarReference(list *l, OutputPort *top_outport)
{
  for (; l; l = l->next) {
    var_reference *vr = (var_reference*)l->data;
    if (vr->top_outport == top_outport)
      return 1;
  }
  return 0;
}

int Compilation::findReferencedVarsExpr(Function *fun, Expression *e,
                                     Statement *below, list **vars)
{
  if (NULL == e)
    return 0;

  if (XPATH_EXPR_VAR_REF == e->m_type) {
    OutputPort *outport = NULL;
    Expression *defexpr = NULL;
    Statement *defnode = NULL;
    setbuf(stdout,NULL);

    Expression_resolve_var(e,e->m_qn,&defexpr,&defnode);
    if (NULL != defexpr) {
      if (!stmtBelow(defexpr->m_stmt,below))
        outport = defexpr->m_outp;
    }
    else if (NULL != defnode) {
      if (!stmtBelow(defnode,below))
        outport = defnode->m_outp;
    }
    else {
      fmessage(stderr,"No such variable \"%*\"\n",&e->m_qn.m_localPart);
      return -1;
    }

    if (NULL != outport) {

      if (!haveVarReference(*vars,outport)) {
        var_reference *vr = (var_reference*)calloc(1,sizeof(var_reference));
        OutputPort *local_outport;

        local_outport = resolveVar(fun,e); /* get the local version */
        assert(NULL != local_outport);

        vr->ref = e;
        vr->defexpr = defexpr;
        vr->defnode = defnode;
        vr->top_outport = outport;
        vr->local_outport = local_outport;

        list_push(vars,vr);
      }
    }
  }

  CHECK_CALL(findReferencedVarsExpr(fun,e->m_conditional,below,vars))
  CHECK_CALL(findReferencedVarsExpr(fun,e->m_left,below,vars))
  CHECK_CALL(findReferencedVarsExpr(fun,e->m_right,below,vars))

  return 0;
}

int Compilation::findReferencedVarsStmt(Function *fun, Statement *sn,
                                Statement *below, list **vars)
{
  for (; sn; sn = sn->m_next) {
    CHECK_CALL(findReferencedVarsExpr(fun,sn->m_select,below,vars))
    CHECK_CALL(findReferencedVarsExpr(fun,sn->m_expr1,below,vars))
    CHECK_CALL(findReferencedVarsExpr(fun,sn->m_expr2,below,vars))
    CHECK_CALL(findReferencedVarsExpr(fun,sn->m_name_expr,below,vars))
    CHECK_CALL(findReferencedVarsStmt(fun,sn->m_child,below,vars))
    CHECK_CALL(findReferencedVarsStmt(fun,sn->m_param,below,vars))
    CHECK_CALL(findReferencedVarsStmt(fun,sn->m_sort,below,vars))
  }

  return 0;
}

void Compilation::initParamInstructions(Function *fun,
                                    List<SequenceType> &seqtypes, OutputPort **outports)
{
  int paramno;

  for (paramno = 0; paramno < fun->m_nparams; paramno++) {
    Instruction *swallow = fun->addInstruction(OP_SWALLOW,nosourceloc);
    Instruction *pass = fun->addInstruction(OP_PASS,nosourceloc);

    if (!seqtypes.isEmpty() && (!seqtypes[paramno].isNull()))
      fun->m_params[paramno].st = seqtypes[paramno];
    else
      fun->m_params[paramno].st = SequenceType(xs_g->complex_ur_type);

    pass->m_outports[0].dest = swallow;
    pass->m_outports[0].destp = 0;
    pass->m_inports[0].st = fun->m_params[paramno].st;
    assert(!fun->m_params[paramno].st.isNull());

    fun->m_params[paramno].start = pass;
    fun->m_params[paramno].outport = &pass->m_outports[0];

    if (outports)
      outports[paramno] = &pass->m_outports[0];
  }
}

int Compilation::compileInnerFunction(Function *fun, Statement *sn,
                                 Instruction **mapout, OutputPort **cursor, sourceloc sloc)
{
  char name[100];
  Function *innerfun;
  int varno;
  int need_gate = stmtInConditional(sn);
  list *vars = NULL;
  Instruction *map;
  list *l;
  OutputPort *innerfuncur;

  /* Create the anonymous inner function */
  sprintf(name,"anon%dfun",m_nextanonid++);
  innerfun = m_program->addFunction(NSName(ANON_TEMPLATE_NAMESPACE,name));

  /* Set the return type of the function. FIXME: this should be derived from the sequence
     constructor within the function */
  innerfun->m_rtype = SequenceType(xs_g->complex_ur_type);

  /* Determine all variables and parameters of the parent function that are referenced
     in this block of code. These must all be declared as parameters to the inner function
     and passed in whenever it is called */
  CHECK_CALL(findReferencedVarsStmt(fun,sn->m_child,sn,&vars))

  innerfun->m_nparams = list_count(vars);
  innerfun->m_params = new Parameter[innerfun->m_nparams];

  /* Create the map instruction which will be used to activate the inner function for
     each item in the input sequence. The number of input ports of the instruction
     corresponds to the number of parameters of the inner function, i.e. the number of
     variables and parameters from *outside* the code block that it references */
  map = fun->addInstruction(OP_MAP,sloc);
  map->m_cfun = innerfun;
  map->freeInports();
  map->m_ninports = innerfun->m_nparams+1;
  map->m_inports = new InputPort[map->m_ninports];

  /* For each parameter and variable referenced from within the code block, connect the
     source to the appropriate input port of the map instruction */
  varno = 0;
  for (l = vars; l; l = l->next) {
    var_reference *vr = (var_reference*)l->data;
    Instruction *dup = fun->addInstruction(OP_DUP,sloc);
    OutputPort *varcur = &dup->m_outports[1];

    assert(NULL != vr->top_outport);
    assert(NULL != vr->local_outport);
    innerfun->m_params[varno].varsource = vr->top_outport;

    set_and_move_cursor(cursor,dup,0,dup,0);
    varcur->dest = map;
    varcur->destp = varno+1;
    varno++;
    useVar(fun,vr->local_outport,&varcur,need_gate,sloc);
  }

  /* FIXME: pass types for each parameter */
  List<SequenceType> types;
  initParamInstructions(innerfun,types,NULL);

  innerfun->m_ret = innerfun->addInstruction(OP_RETURN,sloc);
  innerfun->m_start = innerfun->addInstruction(OP_PASS,sloc);

  /* FIXME: set correct type based on cursor's type */
  innerfun->m_start->m_inports[0].st = SequenceType::item();
      
  innerfuncur = &innerfun->m_start->m_outports[0];
  innerfuncur->dest = innerfun->m_ret;
  innerfuncur->destp = 0;

  CHECK_CALL(compileFunctionContents(innerfun,sn,&innerfuncur))
  assert(NULL != innerfuncur);

  *mapout = map;
  list_free(vars,(list_d_t)free);

  return 0;
}







void Compilation::compileConditional(Function *fun, OutputPort **cursor,
                                    OutputPort **condcur, OutputPort **truecur,
                                    OutputPort **falsecur, sourceloc sloc)
{
  Instruction *dup = fun->addInstruction(OP_DUP,sloc);
  Instruction *split = fun->addInstruction(OP_SPLIT,sloc);
  Instruction *merge = fun->addInstruction(OP_MERGE,sloc);

  set_and_move_cursor(cursor,dup,0,merge,0);

  *falsecur = &split->m_outports[0];
  *truecur = &split->m_outports[1];
  *condcur = &dup->m_outports[0];

  dup->m_outports[0].dest = split;
  dup->m_outports[0].destp = 0;
  dup->m_outports[1].dest = split;
  dup->m_outports[1].destp = 1;

  split->m_outports[0].dest = merge;
  split->m_outports[0].destp = 0;
  split->m_outports[1].dest = merge;
  split->m_outports[1].destp = 0;

  split->m_outports[2].dest = merge;
  split->m_outports[2].destp = 1;
}




int Compilation::compileExpr(Function *fun, Expression *e, OutputPort **cursor)
{
  switch (e->m_type) {
  case XPATH_EXPR_INVALID:
    assert(!"not yet implemented"); /* FIXME */
    break;
  case XPATH_EXPR_FOR:
    assert(!"not yet implemented"); /* FIXME */
    break;
  case XPATH_EXPR_SOME:
    assert(!"not yet implemented"); /* FIXME */
    break;
  case XPATH_EXPR_EVERY:
    assert(!"not yet implemented"); /* FIXME */
    break;
  case XPATH_EXPR_IF: {
    OutputPort *falsecur;
    OutputPort *truecur;
    OutputPort *condcur;
    Instruction *ebv = specialop(fun,SPECIAL_NAMESPACE,"ebv",1,e->m_sloc);
    compileConditional(fun,cursor,&condcur,&truecur,&falsecur,e->m_sloc);
    CHECK_CALL(compileExpr(fun,e->m_right,&falsecur))
    CHECK_CALL(compileExpr(fun,e->m_left,&truecur))
    CHECK_CALL(compileExpr(fun,e->m_conditional,&condcur))
    set_and_move_cursor(&condcur,ebv,0,ebv,0);
    break;
  }
  case XPATH_EXPR_VAR_IN:
    assert(!"not yet implemented"); /* FIXME */
    break;
  case XPATH_EXPR_OR:
    CHECK_CALL(compileBinaryOp(fun,e,cursor,SPECIAL_NAMESPACE,"or"))
    break;
  case XPATH_EXPR_AND:
    CHECK_CALL(compileBinaryOp(fun,e,cursor,SPECIAL_NAMESPACE,"and"))
    break;
  case XPATH_EXPR_COMPARE_VALUES:
    switch (e->m_compare) {
    case XPATH_VALUE_COMP_EQ:
      assert(!"not yet implemented"); /* FIXME */
      break;
    case XPATH_VALUE_COMP_NE:
      assert(!"not yet implemented"); /* FIXME */
      break;
    case XPATH_VALUE_COMP_LT:
      assert(!"not yet implemented"); /* FIXME */
      break;
    case XPATH_VALUE_COMP_LE:
      assert(!"not yet implemented"); /* FIXME */
      break;
    case XPATH_VALUE_COMP_GT:
      assert(!"not yet implemented"); /* FIXME */
      break;
    case XPATH_VALUE_COMP_GE:
      assert(!"not yet implemented"); /* FIXME */
      break;
    default:
      assert(0);
      break;
    }
    break;
  case XPATH_EXPR_COMPARE_GENERAL:
    switch (e->m_compare) {
    case XPATH_GENERAL_COMP_EQ:
      CHECK_CALL(compileBinaryOp(fun,e,cursor,FN_NAMESPACE,"numeric-equal"))
      break;
    case XPATH_GENERAL_COMP_NE: {
      Instruction *not1 = specialop(fun,FN_NAMESPACE,"not",1,e->m_sloc);
      CHECK_CALL(compileBinaryOp(fun,e,cursor,FN_NAMESPACE,"numeric-equal"))
      set_and_move_cursor(cursor,not1,0,not1,0);
      break;
    }
    case XPATH_GENERAL_COMP_LT:
      CHECK_CALL(compileBinaryOp(fun,e,cursor,FN_NAMESPACE,"numeric-less-than"))
      break;
    case XPATH_GENERAL_COMP_LE: {
      Instruction *not1 = specialop(fun,FN_NAMESPACE,"not",1,e->m_sloc);
      CHECK_CALL(compileBinaryOp(fun,e,cursor,FN_NAMESPACE,"numeric-greater-than"))
      set_and_move_cursor(cursor,not1,0,not1,0);
      break;
    }
    case XPATH_GENERAL_COMP_GT:
      CHECK_CALL(compileBinaryOp(fun,e,cursor,FN_NAMESPACE,"numeric-greater-than"))
      break;
    case XPATH_GENERAL_COMP_GE: {
      Instruction *not1 = specialop(fun,FN_NAMESPACE,"not",1,e->m_sloc);
      CHECK_CALL(compileBinaryOp(fun,e,cursor,FN_NAMESPACE,"numeric-less-than"))
      set_and_move_cursor(cursor,not1,0,not1,0);
      break;
    }
    default:
      assert(0);
      break;
    }
    break;
  case XPATH_EXPR_COMPARE_NODES:
    switch (e->m_compare) {
    case XPATH_NODE_COMP_IS:
      assert(!"not yet implemented"); /* FIXME */
      break;
    case XPATH_NODE_COMP_PRECEDES:
      assert(!"not yet implemented"); /* FIXME */
      break;
    case XPATH_NODE_COMP_FOLLOWS:
      assert(!"not yet implemented"); /* FIXME */
      break;
    default:
      assert(0);
      break;
    }
    break;
  case XPATH_EXPR_TO:
    CHECK_CALL(compileBinaryOp(fun,e,cursor,SPECIAL_NAMESPACE,"range"))
    break;
  case XPATH_EXPR_ADD:
    CHECK_CALL(compileBinaryOp(fun,e,cursor,FN_NAMESPACE,"numeric-add"))
    break;
  case XPATH_EXPR_SUBTRACT:
    CHECK_CALL(compileBinaryOp(fun,e,cursor,FN_NAMESPACE,"numeric-subtract"))
    break;
  case XPATH_EXPR_MULTIPLY:
    CHECK_CALL(compileBinaryOp(fun,e,cursor,FN_NAMESPACE,"numeric-multiply"))
    break;
  case XPATH_EXPR_DIVIDE:
    CHECK_CALL(compileBinaryOp(fun,e,cursor,FN_NAMESPACE,"numeric-divide"))
    break;
  case XPATH_EXPR_IDIVIDE:
    break;
  case XPATH_EXPR_MOD:
    CHECK_CALL(compileBinaryOp(fun,e,cursor,FN_NAMESPACE,"numeric-mode"))
    break;
  case XPATH_EXPR_UNION:
    assert(!"not yet implemented"); /* FIXME */
    break;
  case XPATH_EXPR_UNION2:
    assert(!"not yet implemented"); /* FIXME */
    break;
  case XPATH_EXPR_INTERSECT:
    assert(!"not yet implemented"); /* FIXME */
    break;
  case XPATH_EXPR_EXCEPT:
    assert(!"not yet implemented"); /* FIXME */
    break;
  case XPATH_EXPR_INSTANCE_OF:
    assert(!"not yet implemented"); /* FIXME */
    break;
  case XPATH_EXPR_TREAT:
    assert(!"not yet implemented"); /* FIXME */
    break;
  case XPATH_EXPR_CASTABLE:
    assert(!"not yet implemented"); /* FIXME */
    break;
  case XPATH_EXPR_CAST:
    assert(!"not yet implemented"); /* FIXME */
    break;
  case XPATH_EXPR_UNARY_MINUS:
    assert(!"not yet implemented"); /* FIXME */
    break;
  case XPATH_EXPR_UNARY_PLUS:
    assert(!"not yet implemented"); /* FIXME */
    break;
  case XPATH_EXPR_ROOT: {
    Instruction *root = specialop(fun,SPECIAL_NAMESPACE,"select-root",1,e->m_sloc);
    set_and_move_cursor(cursor,root,0,root,0);
    if (NULL != e->m_left)
      CHECK_CALL(compileExpr(fun,e->m_left,cursor))
    break;
  }
  case XPATH_EXPR_STRING_LITERAL: {
    compileString(fun,e->m_strval,cursor,e->m_sloc);
    break;
  }
  case XPATH_EXPR_INTEGER_LITERAL: {
    Instruction *instr = fun->addInstruction(OP_CONST,e->m_sloc);
    instr->m_outports[0].st = SequenceType(xs_g->int_type);
    instr->m_cvalue = Value(e->m_ival);
/*     instr->m_inports[0].st = SequenceType::item(); */
    set_and_move_cursor(cursor,instr,0,instr,0);
    break;
  }
  case XPATH_EXPR_DECIMAL_LITERAL: {
    Instruction *instr = fun->addInstruction(OP_CONST,e->m_sloc);
    instr->m_outports[0].st = SequenceType(xs_g->decimal_type);
    instr->m_cvalue = Value(e->m_dval);
/*     instr->m_inports[0].st = SequenceType::item(); */
    set_and_move_cursor(cursor,instr,0,instr,0);
    break;
  }
  case XPATH_EXPR_DOUBLE_LITERAL: {
    Instruction *instr = fun->addInstruction(OP_CONST,e->m_sloc);
    instr->m_outports[0].st = SequenceType(xs_g->double_type);
    instr->m_cvalue = Value(e->m_dval);
/*     instr->m_inports[0].st = SequenceType::item(); */
    set_and_move_cursor(cursor,instr,0,instr,0);
    break;
  }
  case XPATH_EXPR_VAR_REF: {
    OutputPort *varoutport;
    int need_gate = exprInConditional(e);

    if (NULL == (varoutport = resolveVar(fun,e))) {
      fmessage(stderr,"No such variable \"%*\"\n",&e->m_qn.m_localPart);
      return -1;
    }

    useVar(fun,varoutport,cursor,need_gate,e->m_sloc);

    break;
  }
  case XPATH_EXPR_EMPTY: {
    Instruction *empty = specialop(fun,SPECIAL_NAMESPACE,"empty",1,e->m_sloc);
    set_and_move_cursor(cursor,empty,0,empty,0);
    break;
  }
  case XPATH_EXPR_CONTEXT_ITEM:
    /* just gets passed through */
    break;
  case XPATH_EXPR_NODE_TEST: {
    Instruction *select = specialop(fun,SPECIAL_NAMESPACE,"select",1,e->m_sloc);
    select->m_axis = e->m_axis;

    if (XPATH_NODE_TEST_NAME == e->m_nodetest) {
      select->m_nametest = e->m_qn.m_localPart;
    }
    else if (XPATH_NODE_TEST_SEQUENCETYPE == e->m_nodetest) {
      select->m_seqtypetest = e->m_st;
    }
    else {
      /* FIXME: support other types (should there even by others?) */
      debug("e->m_nodetest = %d\n",e->m_nodetest);
      assert(!"node test type not supported");
    }

    set_and_move_cursor(cursor,select,0,select,0);
    break;
  }
  case XPATH_EXPR_ACTUAL_PARAM:
    CHECK_CALL(compileExpr(fun,e->m_left,cursor))
    break;
  case XPATH_EXPR_FUNCTION_CALL: {
    Instruction *call = NULL;
    OutputPort *source = *cursor;
    Expression *p;
    String name;
    int nparams = 0;
    int found = 0;
    int supparam = 0;

    for (p = e->m_left; p; p = p->m_right)
      supparam++;

    /* FIXME: we shouldn't hard-code this prefix; it may be bound to something else, or
       there may be a different prefix bound to the functions and operators namespace. We
       should do the check based on FN_NAMESPACES according to the in-scope namespaces for
       this node in the parse tree */



    if ((e->m_qn.m_prefix.isNull()) || (e->m_qn.m_prefix == "fn")) {
      call = fun->addBuiltinInstruction(NSName(FN_NAMESPACE,e->m_qn.m_localPart),
                                        supparam,e->m_sloc);
      if (NULL != call) {
        nparams = supparam;
        name = e->m_qn.m_prefix;
        found = 1;
      }
    }

    if (!found) {
      Function *cfun;

      if (NULL == (cfun = m_program->getFunction(e->m_ident)))
        return error(m_ei,e->m_sloc.uri,e->m_sloc.line,String::null(),
                     "No such function %*",&e->m_ident);

      call = fun->addInstruction(OP_CALL,e->m_sloc);
      call->m_cfun = cfun;

      nparams = call->m_cfun->m_nparams;
      name = call->m_cfun->m_ident.m_name;
    }

    set_and_move_cursor(cursor,call,0,call,0);

    if (0 < nparams) {
      int paramno = 0;

      if (OP_BUILTIN != call->m_opcode) {
        int i;
        call->freeInports();
        call->m_ninports = nparams;
        call->m_inports = new InputPort[call->m_ninports];
        for (i = 0; i < call->m_cfun->m_nparams; i++)
          call->m_inports[i].st = call->m_cfun->m_params[i].st;
      }

      for (p = e->m_left; p; p = p->m_right) {
        OutputPort *param_cursor = NULL; /* FIXME */

        if (paramno < nparams-1) {
          Instruction *dup = fun->addInstruction(OP_DUP,e->m_sloc);
          source->dest = dup;
          source->destp = 0;
          source = &dup->m_outports[0];
          source->dest = call;
          source->destp = 0;
          param_cursor = &dup->m_outports[1];
        }
        else {
          param_cursor = source;
        }

        param_cursor->dest = call;
        param_cursor->destp = paramno;

        assert(XPATH_EXPR_ACTUAL_PARAM == p->m_type);
        if (paramno >= nparams) {
          fmessage(stderr,"%d: Too many parameters; %* only accepts %d parameters\n",
                  p->m_sloc.line,&name,nparams);
          return 0;
        }

        CHECK_CALL(compileExpr(fun,p->m_left,&param_cursor))

        paramno++;
      }
    }
    break;
  }
  case XPATH_EXPR_PAREN:
    CHECK_CALL(compileExpr(fun,e->m_left,cursor))
    break;


  case XPATH_EXPR_ATOMIC_TYPE:
    assert(!"not yet implemented"); /* FIXME */
    break;
  case XPATH_EXPR_ITEM_TYPE:
    assert(!"not yet implemented"); /* FIXME */
    break;
  case XPATH_EXPR_SEQUENCE: {
    Instruction *dup = fun->addInstruction(OP_DUP,e->m_sloc);
    Instruction *seq = specialop(fun,SPECIAL_NAMESPACE,"sequence",2,e->m_sloc);
    OutputPort *leftcur = &dup->m_outports[0];
    OutputPort *rightcur = &dup->m_outports[1];

    set_and_move_cursor(cursor,dup,0,seq,0);

    dup->m_outports[0].dest = seq;
    dup->m_outports[0].destp = 0;
    dup->m_outports[1].dest = seq;
    dup->m_outports[1].destp = 1;

    CHECK_CALL(compileExpr(fun,e->m_left,&leftcur))
    CHECK_CALL(compileExpr(fun,e->m_right,&rightcur))

    break;
  }
  case XPATH_EXPR_STEP: {
    CHECK_CALL(compileExpr(fun,e->m_left,cursor))
    CHECK_CALL(compileExpr(fun,e->m_right,cursor))
    break;
  }
  case XPATH_EXPR_VARINLIST:
    assert(!"not yet implemented"); /* FIXME */
    break;
  case XPATH_EXPR_PARAMLIST:
    assert(!"not yet implemented"); /* FIXME */
    break;
  case XPATH_EXPR_FILTER:
    assert(!"not yet implemented"); /* FIXME */
    break;

  default:
    assert(0);
    break;
  }

  return 0;
}

int Compilation::compileSequenceItem(Function *fun, Statement *sn,
                                  OutputPort **cursor)
{
  switch (sn->m_type) {
  case XSLT_VARIABLE: {
    assert(0); /* handled previosuly */
    break;
  }
  case XSLT_INSTR_ANALYZE_STRING:
    break;
  case XSLT_INSTR_APPLY_IMPORTS:
    break;
  case XSLT_INSTR_APPLY_TEMPLATES: {
    if (NULL != sn->m_select) {
      CHECK_CALL(compileExpr(fun,sn->m_select,cursor))
    }
    else {
      Instruction *select = specialop(fun,SPECIAL_NAMESPACE,"select",1,sn->m_sloc);
      select->m_axis = AXIS_CHILD;
      select->m_seqtypetest = SequenceType::node();
      set_and_move_cursor(cursor,select,0,select,0);
    }

    CHECK_CALL(compileApplyTemplates(fun,cursor,NULL,sn->m_sloc))
    break;
  }
  case XSLT_INSTR_ATTRIBUTE: {
    Instruction *dup = fun->addInstruction(OP_DUP,sn->m_sloc);
    Instruction *attr = specialop(fun,SPECIAL_NAMESPACE,"attribute",2,sn->m_sloc);
    OutputPort *namecur = &dup->m_outports[0];
    OutputPort *childcur = &dup->m_outports[1];
    if (NULL != sn->m_name_expr) {
      assert(!"attribute name expressions not yet implemented");
    }
    assert(!sn->m_qn.isNull());

    namecur->dest = attr;
    namecur->destp = 1;
    compileString(fun,sn->m_qn.m_localPart,&namecur,sn->m_sloc);

    childcur->dest = attr;
    childcur->destp = 0;

    if (NULL != sn->m_select)
      CHECK_CALL(compileExpr(fun,sn->m_select,&childcur))
    else
      CHECK_CALL(compileSequence(fun,sn,&childcur))

    set_and_move_cursor(cursor,dup,0,attr,0);
    break;
  }
  case XSLT_INSTR_CALL_TEMPLATE:
    break;
  case XSLT_INSTR_CHOOSE: {
    Statement *c;
    OutputPort *falsecur = NULL;
    OutputPort **branchcur = cursor;
    int otherwise = 0;

    for (c = sn->m_child; c; c = c->m_next) {
      if (NULL != c->m_select) {
        /* when */
        OutputPort *truecur = NULL;
        OutputPort *condcur = NULL;
        Instruction *ebv = specialop(fun,SPECIAL_NAMESPACE,"ebv",1,c->m_sloc);
        compileConditional(fun,branchcur,&condcur,&truecur,&falsecur,c->m_sloc);

        CHECK_CALL(compileExpr(fun,c->m_select,&condcur))
        set_and_move_cursor(&condcur,ebv,0,ebv,0);
        CHECK_CALL(compileSequence(fun,c,&truecur))

        branchcur = &falsecur;
      }
      else {
        /* otherwise */
        CHECK_CALL(compileSequence(fun,c,branchcur))
        otherwise = 1;
      }
    }

    if (!otherwise) {
      Instruction *empty = specialop(fun,SPECIAL_NAMESPACE,"empty",1,sn->m_sloc);
      set_and_move_cursor(branchcur,empty,0,empty,0);
    }

    break;
  }
  case XSLT_INSTR_COMMENT:
    break;
  case XSLT_INSTR_COPY:
    break;
  case XSLT_INSTR_COPY_OF:
    break;
  case XSLT_INSTR_ELEMENT: {
    Instruction *dup = fun->addInstruction(OP_DUP,sn->m_sloc);
    Instruction *elem = specialop(fun,SPECIAL_NAMESPACE,"element",2,sn->m_sloc);
    OutputPort *namecur = &dup->m_outports[0];
    OutputPort *childcur = &dup->m_outports[1];
    if (NULL != sn->m_name_expr) {
      assert(!"element name expressions not yet implemented");
    }
    assert(!sn->m_qn.isNull());

    if (sn->m_includens) {
      NamespaceMap *m;
      for (m = sn->m_namespaces; m; m = m->parent) {
        Iterator<ns_def*> nsit;
        for (nsit = m->defs; nsit.haveCurrent(); nsit++) {
          ns_def *def = *nsit;
          int have = 0;
          list *exl;
          for (exl = elem->m_nsdefs; exl && !have; exl = exl->next) {
            ns_def *exdef = (ns_def*)exl->data;
            if (exdef->prefix == def->prefix)
              have = 1;
          }
          if (!have) {
            ns_def *copy = new ns_def(def->prefix,def->href);
            list_append(&elem->m_nsdefs,copy);
          }
        }
      }
    }

    namecur->dest = elem;
    namecur->destp = 1;
    compileString(fun,sn->m_qn.m_localPart,&namecur,sn->m_sloc);

    childcur->dest = elem;
    childcur->destp = 0;
    CHECK_CALL(compileSequence(fun,sn,&childcur))

    set_and_move_cursor(cursor,dup,0,elem,0);

    break;
  }
  case XSLT_INSTR_FALLBACK:
    break;
  case XSLT_INSTR_FOR_EACH: {
    Instruction *map = NULL;
    CHECK_CALL(compileInnerFunction(fun,sn,&map,cursor,sn->m_sloc))

    map->m_outports[0].dest = (*cursor)->dest;
    map->m_outports[0].destp = (*cursor)->destp;
    (*cursor)->dest = map;
    (*cursor)->destp = 0;

    CHECK_CALL(compileExpr(fun,sn->m_select,cursor))
    *cursor = &map->m_outports[0];
    break;
  }
  case XSLT_INSTR_FOR_EACH_GROUP:
    break;
  case XSLT_INSTR_IF: {
    OutputPort *falsecur;
    OutputPort *truecur;
    OutputPort *condcur;
    Instruction *ebv = specialop(fun,SPECIAL_NAMESPACE,"ebv",1,sn->m_sloc);
    Instruction *empty = specialop(fun,SPECIAL_NAMESPACE,"empty",1,sn->m_sloc);
    compileConditional(fun,cursor,&condcur,&truecur,&falsecur,sn->m_sloc);
    set_and_move_cursor(&falsecur,empty,0,empty,0);
    CHECK_CALL(compileSequence(fun,sn,&truecur))
    CHECK_CALL(compileExpr(fun,sn->m_select,&condcur))
    set_and_move_cursor(&condcur,ebv,0,ebv,0);
    break;
  }
  case XSLT_INSTR_MESSAGE:
    break;
  case XSLT_INSTR_NAMESPACE: {
    Instruction *dup = fun->addInstruction(OP_DUP,sn->m_sloc);
    Instruction *ns = specialop(fun,SPECIAL_NAMESPACE,"namespace",2,sn->m_sloc);
    OutputPort *prefixcur = &dup->m_outports[0];
    OutputPort *hrefcur = &dup->m_outports[1];

    dup->m_outports[0].dest = ns;
    dup->m_outports[0].destp = 0;
    dup->m_outports[1].dest = ns;
    dup->m_outports[1].destp = 1;

    set_and_move_cursor(cursor,dup,0,ns,0);

    CHECK_CALL(compileExpr(fun,sn->m_name_expr,&prefixcur))
    if (NULL != sn->m_select)
      CHECK_CALL(compileExpr(fun,sn->m_select,&hrefcur))
    else
      CHECK_CALL(compileSequence(fun,sn,&hrefcur))
    break;
  }
  case XSLT_INSTR_NEXT_MATCH:
    break;
  case XSLT_INSTR_NUMBER:
    break;
  case XSLT_INSTR_PERFORM_SORT:
    break;
  case XSLT_INSTR_PROCESSING_INSTRUCTION:
    break;
  case XSLT_INSTR_RESULT_DOCUMENT:
    break;
  case XSLT_INSTR_SEQUENCE:
    CHECK_CALL(compileExpr(fun,sn->m_select,cursor))
    break;
  case XSLT_INSTR_TEXT: {
    Instruction *text = specialop(fun,SPECIAL_NAMESPACE,"text",1,sn->m_sloc);
    text->m_str = sn->m_strval;
    set_and_move_cursor(cursor,text,0,text,0);
    break;
  }
  case XSLT_INSTR_VALUE_OF: {
    Instruction *dup = fun->addInstruction(OP_DUP,sn->m_sloc);
    Instruction *valueof = specialop(fun,SPECIAL_NAMESPACE,"value-of",2,sn->m_sloc);
    OutputPort *valuecur = &dup->m_outports[0];
    OutputPort *sepcur = &dup->m_outports[1];

    set_and_move_cursor(cursor,dup,0,valueof,0);

    dup->m_outports[0].dest = valueof;
    dup->m_outports[0].destp = 0;
    dup->m_outports[1].dest = valueof;
    dup->m_outports[1].destp = 1;

    if (NULL != sn->m_expr1) /* "select" attribute */
      CHECK_CALL(compileExpr(fun,sn->m_expr1,&sepcur))
    else if (NULL != sn->m_select)
      compileString(fun," ",&sepcur,sn->m_sloc);
    else
      compileString(fun,"",&sepcur,sn->m_sloc);

    if (NULL != sn->m_select)
      CHECK_CALL(compileExpr(fun,sn->m_select,&valuecur))
    else
      CHECK_CALL(compileSequence(fun,sn,&valuecur))

    break;
  }
  default:
    assert(0);
    break;
  }
  return 0;
}

int Compilation::compileSequence(Function *fun, Statement *parent,
                             OutputPort **cursor)
{
  Statement *sn;
  int havecontent = 0;

  for (sn = parent->m_child; sn; sn = sn->m_next) {

    if (XSLT_VARIABLE == sn->m_type) {
      Instruction *swallow = fun->addInstruction(OP_SWALLOW,sn->m_sloc);
      Instruction *pass = fun->addInstruction(OP_PASS,sn->m_sloc);
      Instruction *dup = fun->addInstruction(OP_DUP,sn->m_sloc);
      OutputPort *varcur = &dup->m_outports[1];
/*       message("variable declaration: %s\n",sn->m_qn.localpart); */
      set_and_move_cursor(cursor,dup,0,dup,0);
      varcur->dest = pass;
      varcur->destp = 0;
      pass->m_outports[0].dest = swallow;
      pass->m_outports[0].destp = 0;
      if (NULL != sn->m_select) {
        CHECK_CALL(compileExpr(fun,sn->m_select,&varcur))
      }
      else {
        Instruction *docinstr = specialop(fun,SPECIAL_NAMESPACE,"document",1,sn->m_sloc);
        docinstr->m_outports[0].dest = pass;
        docinstr->m_outports[0].destp = 0;
        varcur->dest = docinstr;
        varcur->destp = 0;
        CHECK_CALL(compileSequence(fun,sn,&varcur))
      }
      sn->m_outp = &pass->m_outports[0];
    }
    else if (sn->m_next) {
      Instruction *dup = fun->addInstruction(OP_DUP,sn->m_sloc);
      Instruction *seq = specialop(fun,SPECIAL_NAMESPACE,"sequence",2,sn->m_sloc);

      seq->m_outports[0].dest = (*cursor)->dest;
      seq->m_outports[0].destp = (*cursor)->destp;

      set_and_move_cursor(cursor,dup,0,dup,0);
      (*cursor)->dest = seq;
      (*cursor)->destp = 0;

      CHECK_CALL(compileSequenceItem(fun,sn,cursor))

      *cursor = &dup->m_outports[1];
      (*cursor)->dest = seq;
      (*cursor)->destp = 1;

      havecontent = 1;
    }
    else {
      CHECK_CALL(compileSequenceItem(fun,sn,cursor))

      havecontent = 1;
    }
  }

  if (!havecontent) {
    Instruction *empty = specialop(fun,SPECIAL_NAMESPACE,"empty",1,parent->m_sloc);
    set_and_move_cursor(cursor,empty,0,empty,0);
  }

  return 0;
}

int Compilation::compileFunctionContents(Function *fun, Statement *parent,
                                OutputPort **cursor)
{
  return compileSequence(fun,parent,cursor);
}

int Compilation::compileFunction(Statement *sn, Function *fun)
{
  OutputPort *cursor;
  List<SequenceType> seqtypes;
  OutputPort **outports;
  Statement *p;
  int paramno;

  /* Compute parameters */
  fun->m_nparams = 0;
  for (p = sn->m_param; p; p = p->m_next)
    fun->m_nparams++;

  if (0 != fun->m_nparams)
    fun->m_params = new Parameter[fun->m_nparams];

  outports = (OutputPort**)calloc(fun->m_nparams,sizeof(OutputPort*));

  for (p = sn->m_param, paramno = 0; p; p = p->m_next, paramno++)
    seqtypes.append(p->m_st);

  initParamInstructions(fun,seqtypes,outports);

  for (p = sn->m_param, paramno = 0; p; p = p->m_next, paramno++)
    p->m_outp = outports[paramno];

  free(outports);

  /* Compute return type */
  fun->m_rtype = sn->m_st;
  if (fun->m_rtype.isNull()) {
    fun->m_rtype = SequenceType(xs_g->complex_ur_type);
    assert(NULL != fun->m_rtype.itemType()->m_type);
  }

  /* Add wrapper instructions */
  fun->m_ret = fun->addInstruction(OP_RETURN,sn->m_sloc);
  fun->m_start = fun->addInstruction(OP_PASS,sn->m_sloc);
  fun->m_start->m_inports[0].st = SequenceType::item();

  cursor = &fun->m_start->m_outports[0];
  cursor->dest = fun->m_ret;
  cursor->destp = 0;

  CHECK_CALL(compileFunctionContents(fun,sn,&cursor))
  assert(NULL != cursor);

  return 0;
}

int Compilation::compileApplyFunction(Function **ifout, const char *mode)
{
  OutputPort *cursor = NULL;
  OutputPort *truecur = NULL;
  OutputPort *falsecur = NULL;
  OutputPort *condcur = NULL;
  OutputPort **branchcur = &cursor;
  Instruction *empty;
  char name[100];
  Function *innerfun;

  sprintf(name,"anon%dfun",m_nextanonid++);
  innerfun = m_program->addFunction(NSName(ANON_TEMPLATE_NAMESPACE,name));

  innerfun->init(nosourceloc);

  innerfun->m_isapply = 1;
  innerfun->m_mode = mode;


  cursor = &innerfun->m_start->m_outports[0];

  Iterator<Template*> it;
  for (it = m_templates; it.haveCurrent(); it++) {
    Template *t = *it;
    Instruction *call = innerfun->addInstruction(OP_CALL,nosourceloc);

    Instruction *dup = innerfun->addInstruction(OP_DUP,nosourceloc);
    Instruction *contains = specialop(innerfun,SPECIAL_NAMESPACE,"contains-node",2,
                                         nosourceloc);
    Instruction *root = specialop(innerfun,FN_NAMESPACE,"root",1,nosourceloc);
    Instruction *select = specialop(innerfun,SPECIAL_NAMESPACE,"select",1,nosourceloc);
    select->m_axis = AXIS_DESCENDANT_OR_SELF;
    select->m_seqtypetest = SequenceType::node();
    call->m_inports[0].st = SequenceType::item();

    compileConditional(innerfun,branchcur,&condcur,&truecur,&falsecur,nosourceloc);

    /* FIXME: transform expression as described in XSLT 2.0 section 5.5.3 */

    set_and_move_cursor(&condcur,dup,0,dup,0);
    set_and_move_cursor(&condcur,root,0,root,0);
    set_and_move_cursor(&condcur,select,0,select,0);
    CHECK_CALL(compileExpr(innerfun,t->pattern,&condcur))

    set_and_move_cursor(&condcur,contains,0,contains,0);

    dup->m_outports[1].dest = contains;
    dup->m_outports[1].destp = 1;

    set_and_move_cursor(&truecur,call,0,call,0);

    call->m_cfun = t->fun;

    branchcur = &falsecur;
  }

  /* default template for document and element nodes */
  {
    Instruction *select = specialop(innerfun,SPECIAL_NAMESPACE,"select",1,nosourceloc);
    Instruction *isempty = specialop(innerfun,FN_NAMESPACE,"empty",1,nosourceloc);
    Instruction *not1 = specialop(innerfun,FN_NAMESPACE,"not",1,nosourceloc);
    Instruction *childsel = specialop(innerfun,SPECIAL_NAMESPACE,"select",1,nosourceloc);
    SequenceType doctype = SequenceType(new ItemType(ITEM_DOCUMENT));
    SequenceType elemtype = SequenceType(new ItemType(ITEM_ELEMENT));

    compileConditional(innerfun,branchcur,&condcur,&truecur,&falsecur,nosourceloc);

    select->m_axis = AXIS_SELF;
    select->m_seqtypetest = SequenceType::choice(doctype,elemtype);

    set_and_move_cursor(&condcur,select,0,select,0);
    set_and_move_cursor(&condcur,isempty,0,isempty,0);
    set_and_move_cursor(&condcur,not1,0,not1,0);

    childsel->m_axis = AXIS_CHILD;
    childsel->m_seqtypetest = SequenceType::node();
    set_and_move_cursor(&truecur,childsel,0,childsel,0);
    CHECK_CALL(compileApplyTemplates(innerfun,&truecur,mode,nosourceloc))

    branchcur = &falsecur;
  }

  /* default template for text and attribute nodes */
  {
    Instruction *select = specialop(innerfun,SPECIAL_NAMESPACE,"select",1,nosourceloc);
    Instruction *isempty = specialop(innerfun,FN_NAMESPACE,"empty",1,nosourceloc);
    Instruction *not1 = specialop(innerfun,FN_NAMESPACE,"not",1,nosourceloc);
    Instruction *string = specialop(innerfun,FN_NAMESPACE,"string",1,nosourceloc);
    SequenceType texttype = SequenceType(new ItemType(ITEM_TEXT));
    SequenceType attrtype = SequenceType(new ItemType(ITEM_ATTRIBUTE));

    compileConditional(innerfun,branchcur,&condcur,&truecur,&falsecur,nosourceloc);

    select->m_axis = AXIS_SELF;
    select->m_seqtypetest = SequenceType::choice(texttype,attrtype);

    set_and_move_cursor(&condcur,select,0,select,0);
    set_and_move_cursor(&condcur,isempty,0,isempty,0);
    set_and_move_cursor(&condcur,not1,0,not1,0);

    set_and_move_cursor(&truecur,string,0,string,0);

    branchcur = &falsecur;
  }

  empty = specialop(innerfun,SPECIAL_NAMESPACE,"empty",1,nosourceloc);
  set_and_move_cursor(branchcur,empty,0,empty,0);

  *ifout = innerfun;

  return 0;
}


/**
 * Build the code corresponding to an apply-templates instruction for a given mode. This consists
 * of an anonymous function containing a series of conditionals which test the candidate templates
 * in turn, and makes a call to the first one that matches. The apply-templates instruction
 * then corresponds to an evaluation of the select attribute (if there is one), followed by
 * a map operator which takes the sequence returned by the select expression and applies the
 * template function to each element in the resulting sequence.
 *
 * Only one anonymous function is maintained for each mode; it will be created if it does not yet
 * exist, otherwise the existing one will be re-used.
 *
 * Note: This function does not handle the select attribute. When this function is called, the
 * cursor must be set to the output port of a subgraph which computes the sequence of items
 * corresponding to the select attribute (which defaults to "child::node()")
 */
int Compilation::compileApplyTemplates(Function *fun,
                                    OutputPort **cursor,
                                    const char *mode, sourceloc sloc)
{
  Function *applyfun = NULL;
  Instruction *map = NULL;

  /* Do we already have an anonymous function for this mode? */
  for (Iterator<Function*> it(m_program->m_functions); it.haveCurrent(); it++) {
    Function *candidate = *it;
    if (candidate->m_isapply) {
      if (((NULL == mode) && candidate->m_mode.isNull()) ||
          ((NULL != mode) && !candidate->m_mode.isNull() && (mode == candidate->m_mode))) {
        applyfun = candidate;
        break;
      }
    }
  }

  /* Create the anonymous function if it doesn't already exist */
  if (NULL == applyfun)
    CHECK_CALL(compileApplyFunction(&applyfun,mode))

  /* Add the map instruction */
  map = fun->addInstruction(OP_MAP,sloc);
  map->m_cfun = applyfun;
  set_and_move_cursor(cursor,map,0,map,0);

  return 0;
}

void Compilation::compileOrderedTemplateList()
{
  Statement *sn;

  for (sn = m_source->root->m_child; sn; sn = xl_next_decl(sn)) {
    if (XSLT_DECL_TEMPLATE == sn->m_type) {
      sn->m_tmpl = new Template();
      sn->m_tmpl->pattern = sn->m_select;
      m_templates.append(sn->m_tmpl);
    }
  }
}

int Compilation::compileDefaultTemplate()
{
  Function *fun = m_program->addFunction(NSName(String::null(),"default"));
  Instruction *pass = fun->addInstruction(OP_PASS,nosourceloc);
  Instruction *ret = fun->addInstruction(OP_RETURN,nosourceloc);
  Instruction *output = specialop(fun,SPECIAL_NAMESPACE,"output",1,nosourceloc);
  OutputPort *atcursor;
  df_seroptions *options;

  pass->m_outports[0].dest = ret;
  pass->m_outports[0].destp = 0;
  fun->m_rtype = SequenceType(xs_g->any_atomic_type);
  pass->m_inports[0].st = SequenceType::item();

  options = xslt_get_output_def(m_source,NSName::null());
  assert(NULL != options);
  output->m_seroptions = new df_seroptions(*options);

  atcursor = &pass->m_outports[0];
  CHECK_CALL(compileApplyTemplates(fun,&atcursor,NULL,nosourceloc))
  set_and_move_cursor(&atcursor,output,0,output,0);

  fun->m_start = pass;

  return 0;
}

int Compilation::compile2()
{
  Statement *sn;
  int templatecount = 0;

  m_program->m_space_decls = list_copy(m_source->space_decls,(list_copy_t)space_decl_copy);

  /* create empty functions first, so that they can be referenced */

  for (sn = m_source->root->m_child; sn; sn = xl_next_decl(sn)) {
    if (XSLT_DECL_FUNCTION == sn->m_type) {
      if (NULL != m_program->getFunction(sn->m_ident)) {
        fmessage(stderr,"Function \"%*\" already defined\n",&sn->m_qn.m_localPart);
        return -1;
      }
      m_program->addFunction(sn->m_ident);
    }
    else if (XSLT_DECL_TEMPLATE == sn->m_type) {
      sn->m_ident = NSName(ANON_TEMPLATE_NAMESPACE,String::format("template%d",templatecount++));
      assert(NULL != sn->m_tmpl);
      sn->m_tmpl->fun = m_program->addFunction(sn->m_ident);
    }
  }

  CHECK_CALL(compileDefaultTemplate())

  /* now process the contents of the functions etc */
  for (sn = m_source->root->m_child; sn; sn = xl_next_decl(sn)) {
    switch (sn->m_type) {
    case XSLT_DECL_ATTRIBUTE_SET:
      break;
    case XSLT_DECL_CHARACTER_MAP:
      break;
    case XSLT_DECL_DECIMAL_FORMAT:
      break;
    case XSLT_DECL_FUNCTION: {
      Function *fun = m_program->getFunction(sn->m_ident);
      CHECK_CALL(compileFunction(sn,fun))
      break;
    }
    case XSLT_DECL_IMPORT_SCHEMA:
      break;
    case XSLT_DECL_INCLUDE:
      break;
    case XSLT_DECL_KEY:
      break;
    case XSLT_DECL_NAMESPACE_ALIAS:
       break;
    case XSLT_DECL_OUTPUT:
      break;
    case XSLT_DECL_PRESERVE_SPACE:
      break;
    case XSLT_DECL_STRIP_SPACE:
      break;
    case XSLT_DECL_TEMPLATE:
      assert(NULL != sn->m_tmpl->fun);
      CHECK_CALL(compileFunction(sn,sn->m_tmpl->fun))
      break;
    default:
      assert(0);
      break;
    }
  }

  for (Iterator<Function*> it(m_program->m_functions); it.haveCurrent(); it++) {
    Function *fun = *it;
    CHECK_CALL(df_check_function_connected(fun))
    df_remove_redundant(m_program,fun);
    fun->computeTypes();
  }

  return 0;
}


extern FunctionDefinition *special_module;
extern FunctionDefinition *string_module;
extern FunctionDefinition *sequence_module;
extern FunctionDefinition *node_module;
extern FunctionDefinition *boolean_module;
extern FunctionDefinition *numeric_module;
extern FunctionDefinition *accessor_module;
extern FunctionDefinition *context_module;

static FunctionDefinition **modules[9] = {
  &special_module,
  &string_module,
  &sequence_module,
  &node_module,
  &boolean_module,
  &numeric_module,
  &accessor_module,
  &context_module,
  NULL
};

int xslt_compile(Error *ei, xslt_source *source, Program **program)
{
  int r = 0;
  Compilation comp;

  *program = new Program(source->schema);

  comp.m_ei = ei;
  comp.m_source = source;
  comp.m_program = *program;
  comp.m_schema = source->schema;
  comp.compileOrderedTemplateList();

  df_init_builtin_functions(comp.m_program->m_schema,comp.m_program->m_builtin_functions,modules);

  r = comp.compile2();

  if (0 != r) {
    delete *program;
    *program = NULL;
  }

  return r;
}

