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

#include "Compilation.h"
#include "Builtins.h"
#include "util/List.h"
#include "util/Macros.h"
#include "util/Debug.h"
#include "dataflow/Optimization.h"
#include "dataflow/Validity.h"
#include <string.h>

/*

Expression types remaining to be implemented:

XPATH_INVALID
XPATH_FOR
XPATH_SOME
XPATH_EVERY
XPATH_VAR_IN
XPATH_VALUE_EQ
XPATH_VALUE_NE
XPATH_VALUE_LT
XPATH_VALUE_LE
XPATH_VALUE_GT
XPATH_VALUE_GE
XPATH_NODE_IS
XPATH_NODE_PRECEDES
XPATH_NODE_FOLLOWS
XPATH_UNION
XPATH_UNION2
XPATH_INTERSECT
XPATH_EXCEPT
XPATH_TREAT
XPATH_CASTABLE
XPATH_CAST
XPATH_ATOMIC_TYPE
XPATH_ITEM_TYPE
XPATH_VARINLIST
XPATH_PARAMLIST

Statement types remaining to be implemented:

XSLT_INSTR_ANALYZE_STRING
XSLT_INSTR_APPLY_IMPORTS
XSLT_INSTR_CALL_TEMPLATE
XSLT_INSTR_COMMENT
XSLT_INSTR_COPY
XSLT_INSTR_COPY_OF
XSLT_INSTR_FALLBACK
XSLT_INSTR_FOR_EACH_GROUP
XSLT_INSTR_MESSAGE
XSLT_INSTR_NEXT_MATCH
XSLT_INSTR_NUMBER
XSLT_INSTR_PERFORM_SORT
XSLT_INSTR_PROCESSING_INSTRUCTION
XSLT_INSTR_RESULT_DOCUMENT

*/

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





int BinaryExpr::compile(Compilation *comp, Function *fun, OutputPort **cursor)
{
  NSName funName;
  bool negate = false;

  switch (m_type) {
  case XPATH_OR:
    funName = NSName(SPECIAL_NAMESPACE,"or");
    break;
  case XPATH_AND:
    funName = NSName(SPECIAL_NAMESPACE,"and");
    break;
  case XPATH_GENERAL_EQ:
    funName = NSName(FN_NAMESPACE,"numeric-equal");
    break;
  case XPATH_GENERAL_NE:
    funName = NSName(FN_NAMESPACE,"numeric-equal");
    negate = true;
    break;
  case XPATH_GENERAL_LT:
    funName = NSName(FN_NAMESPACE,"numeric-less-than");
    break;
  case XPATH_GENERAL_LE:
    funName = NSName(FN_NAMESPACE,"numeric-greater-than");
    negate = true;
    break;
  case XPATH_GENERAL_GT:
    funName = NSName(FN_NAMESPACE,"numeric-greater-than");
    break;
  case XPATH_GENERAL_GE:
    funName = NSName(FN_NAMESPACE,"numeric-less-than");
    negate = true;
    break;
  case XPATH_TO:
    funName = NSName(SPECIAL_NAMESPACE,"range");
    break;
  case XPATH_ADD:
    funName = NSName(FN_NAMESPACE,"numeric-add");
    break;
  case XPATH_SUBTRACT:
    funName = NSName(FN_NAMESPACE,"numeric-subtract");
    break;
  case XPATH_MULTIPLY:
    funName = NSName(FN_NAMESPACE,"numeric-multiply");
    break;
  case XPATH_DIVIDE:
    funName = NSName(FN_NAMESPACE,"numeric-divide");
    break;
  case XPATH_IDIVIDE:
    funName = NSName(FN_NAMESPACE,"numeric-integer-divide");
    break;
  case XPATH_MOD:
    funName = NSName(FN_NAMESPACE,"numeric-mod");
    break;
  default:
    ASSERT("!not a binary operator");
    break;
  }

  Instruction *dup = fun->addInstruction(OP_DUP,m_sloc);
  Instruction *binop = comp->specialop(fun,funName.m_ns,funName.m_name,2,m_sloc);
  OutputPort *left_cursor = &dup->m_outports[0];
  OutputPort *right_cursor = &dup->m_outports[1];

  left_cursor->dest = binop;
  left_cursor->destp = 0;
  right_cursor->dest = binop;
  right_cursor->destp = 1;
  comp->set_and_move_cursor(cursor,dup,0,binop,0);

  CHECK_CALL(m_left->compile(comp,fun,&left_cursor))
  CHECK_CALL(m_right->compile(comp,fun,&right_cursor))

  if (negate) {
    Instruction *not1 = comp->specialop(fun,FN_NAMESPACE,"not",1,m_sloc);
    comp->set_and_move_cursor(cursor,not1,0,not1,0);
  }

  return 0;
}

int UnaryExpr::compile(Compilation *comp, Function *fun, OutputPort **cursor)
{
  CHECK_CALL(m_source->compile(comp,fun,cursor))
  if (XPATH_UNARY_PLUS == m_type) {
    Instruction *plus = comp->specialop(fun,FN_NAMESPACE,"numeric-unary-plus",1,m_sloc);
    comp->set_and_move_cursor(cursor,plus,0,plus,0);
  }
  else {
    Instruction *minus = comp->specialop(fun,FN_NAMESPACE,"numeric-unary-minus",1,m_sloc);
    comp->set_and_move_cursor(cursor,minus,0,minus,0);
  }
  return 0;
}

int CallExpr::compile(Compilation *comp, Function *fun, OutputPort **cursor)
{
  Instruction *call = NULL;
  ActualParamExpr *p;
  String name;
  int nparams = 0;
  int found = 0;
  int supparam = 0;

  for (p = m_left; p; p = p->right()) {
    supparam++;
  }

  if ((m_ident.m_ns.isNull()) ||
      (m_ident.m_ns == FN_NAMESPACE) ||
      (m_ident.m_ns == XS_NAMESPACE) ||
      (m_ident.m_ns == XDT_NAMESPACE)) {
    NSName ident = m_ident;
    if (ident.m_ns.isNull())
      ident.m_ns = FN_NAMESPACE;
    // FIXME: shouldn't allow direct access to operators (as distinct from builtin functions)...
    // must use namespace to access these (e.g. op:numeric-integer-divide)
    call = fun->addBuiltinInstruction(ident,supparam,m_sloc);
    if (NULL != call) {
      nparams = supparam;
      name = m_qn.m_prefix;
      found = 1;
    }
  }

  if (!found) {
    Function *cfun;

    if (NULL == (cfun = comp->m_program->getFunction(m_ident)))
      return error(comp->m_ei,m_sloc.uri,m_sloc.line,String::null(),
                   "No such function %* with %d args",&m_ident,supparam);

    call = fun->addInstruction(OP_CALL,m_sloc);
    call->m_cfun = cfun;

    unsigned int i;
    call->freeInports();
    call->m_ninports = call->m_cfun->m_nparams;
    call->m_inports = new InputPort[call->m_ninports];
    for (i = 0; i < call->m_cfun->m_nparams; i++) {
      call->m_inports[i].st = call->m_cfun->m_params[i].st;
      ASSERT(!call->m_inports[i].st.isNull());
    }

    nparams = call->m_cfun->m_nparams-1;
    name = call->m_cfun->m_ident.m_name;
  }

  unsigned int paramno = 0;
  unsigned int userparams = call->m_ninports;

  OutputPort **psource = (OutputPort**)alloca(call->m_ninports*sizeof(OutputPort*));

  for (paramno = 0; paramno < call->m_ninports; paramno++) {
    if (paramno < call->m_ninports-1) {
      Instruction *dup = fun->addInstruction(OP_DUP,m_sloc);
      dup->m_outports[0].dest = call;
      dup->m_outports[0].destp = paramno;
      psource[paramno] = &dup->m_outports[0];
      comp->set_and_move_cursor(cursor,dup,0,dup,1);
    }
    else {
      psource[paramno] = *cursor;
      comp->set_and_move_cursor(cursor,call,paramno,call,0);
    }
  }

  paramno = 0;

  if ((OP_BUILTIN != call->m_opcode) || call->m_bif->m_context) {
    paramno++;
    userparams--;
  }


  p = m_left;
  while (paramno < call->m_ninports) {

    if (NULL == p) {
      fmessage(stderr,"%*:%d: Insufficient parameters (expected %u)\n",
               &m_sloc.uri,m_sloc.line,userparams);
      return -1;
    }

    OutputPort *pcur = psource[paramno];
    CHECK_CALL(p->left()->compile(comp,fun,&pcur))

    paramno++;
    p = p->right();
  }

  if (NULL != p) {
    fmessage(stderr,"%*:%d: Too many parameters (expected %u)\n",
             &p->m_sloc.uri,p->m_sloc.line,userparams);
    return -1;
  }

  return 0;
}

int XPathIfExpr::compile(Compilation *comp, Function *fun, OutputPort **cursor)
{
  OutputPort *falsecur;
  OutputPort *truecur;
  OutputPort *condcur;
  Instruction *ebv = comp->specialop(fun,SPECIAL_NAMESPACE,"ebv",1,m_sloc);
  comp->compileConditional(fun,cursor,&condcur,&truecur,&falsecur,m_sloc);
  CHECK_CALL(m_falseExpr->compile(comp,fun,&falsecur))
  CHECK_CALL(m_trueExpr->compile(comp,fun,&truecur))
  CHECK_CALL(m_conditional->compile(comp,fun,&condcur))
  comp->set_and_move_cursor(&condcur,ebv,0,ebv,0);
  return 0;
}

int TypeExpr::compile(Compilation *comp, Function *fun, OutputPort **cursor)
{
  switch (m_type) {
  case XPATH_INSTANCE_OF: {
    CHECK_CALL(m_source->compile(comp,fun,cursor))
    Instruction *instanceof = comp->specialop(fun,SPECIAL_NAMESPACE,"instanceof",1,m_sloc);
    instanceof->m_seqtypetest = m_st;
    comp->set_and_move_cursor(cursor,instanceof,0,instanceof,0);
    break;
  }
  case XPATH_TREAT:
    ASSERT(!"not implemented");
    break;
  case XPATH_CASTABLE:
    ASSERT(!"not implemented");
    break;
  case XPATH_CAST:
    ASSERT(!"not implemented");
    break;
  default:
    ASSERT(0);
    break;
  }
  return 0;
}

int FilterExpr::compile(Compilation *comp, Function *fun, OutputPort **cursor)
{
  Instruction *dup = fun->addInstruction(OP_DUP,m_sloc);
  comp->set_and_move_cursor(cursor,dup,0,dup,0);

  CHECK_CALL(m_left->compile(comp,fun,cursor))

  Instruction *listdup = fun->addInstruction(OP_DUP,m_sloc);
  comp->set_and_move_cursor(cursor,listdup,0,listdup,0);

  Instruction *map;
  CHECK_CALL(comp->compileInnerFunction(fun,m_right,&map,cursor,m_sloc))
  comp->set_and_move_cursor(cursor,map,0,map,0);
  dup->m_outports[1].dest = map;
  dup->m_outports[1].destp = 1;

  Instruction *filter = comp->specialop(fun,SPECIAL_NAMESPACE,"filter",2,m_sloc);
  comp->set_and_move_cursor(cursor,filter,1,filter,0);
  listdup->m_outports[1].dest = filter;
  listdup->m_outports[1].destp = 0;

  return 0;
}

int StepExpr::compile(Compilation *comp, Function *fun, OutputPort **cursor)
{
  Instruction *dup = fun->addInstruction(OP_DUP,m_sloc);
  comp->set_and_move_cursor(cursor,dup,0,dup,0);

  CHECK_CALL(m_left->compile(comp,fun,cursor))

  Instruction *map;
  CHECK_CALL(comp->compileInnerFunction(fun,m_right,&map,cursor,m_sloc))
  comp->set_and_move_cursor(cursor,map,0,map,0);
  dup->m_outports[1].dest = map;
  dup->m_outports[1].destp = 1;
  return 0;
}

int RootExpr::compile(Compilation *comp, Function *fun, OutputPort **cursor)
{
  Instruction *dup = NULL;

  if (NULL != m_source) {
    dup = fun->addInstruction(OP_DUP,m_sloc);
    comp->set_and_move_cursor(cursor,dup,0,dup,0);
  }

  Instruction *dot = comp->specialop(fun,SPECIAL_NAMESPACE,"dot",0,m_sloc);
  comp->set_and_move_cursor(cursor,dot,0,dot,0);

  Instruction *root = comp->specialop(fun,SPECIAL_NAMESPACE,"select-root",1,m_sloc);
  comp->set_and_move_cursor(cursor,root,0,root,0);
  if (NULL != m_source) {
//   CHECK_CALL(m_source->compile(this,fun,cursor))

    Instruction *map;
    CHECK_CALL(comp->compileInnerFunction(fun,m_source,&map,cursor,m_sloc))
    comp->set_and_move_cursor(cursor,map,0,map,0);
    dup->m_outports[1].dest = map;
    dup->m_outports[1].destp = 1;
  }

  return 0;
}

int IntegerExpr::compile(Compilation *comp, Function *fun, OutputPort **cursor)
{
  Instruction *instr = fun->addInstruction(OP_CONST,m_sloc);
  instr->m_outports[0].st = SequenceType(xs_g->integer_type);
  instr->m_cvalue = Value(m_ival);
  comp->set_and_move_cursor(cursor,instr,0,instr,0);
  return 0;
}

int DecimalExpr::compile(Compilation *comp, Function *fun, OutputPort **cursor)
{
  Instruction *instr = fun->addInstruction(OP_CONST,m_sloc);
  instr->m_outports[0].st = SequenceType(xs_g->decimal_type);
  instr->m_cvalue = Value::decimal(m_dval);
  comp->set_and_move_cursor(cursor,instr,0,instr,0);
  return 0;
}

int DoubleExpr::compile(Compilation *comp, Function *fun, OutputPort **cursor)
{
  Instruction *instr = fun->addInstruction(OP_CONST,m_sloc);
  instr->m_outports[0].st = SequenceType(xs_g->double_type);
  instr->m_cvalue = Value(m_dval);
  comp->set_and_move_cursor(cursor,instr,0,instr,0);
  return 0;
}

int StringExpr::compile(Compilation *comp, Function *fun, OutputPort **cursor)
{
  comp->compileString(fun,m_strval,cursor,m_sloc);
  return 0;
}

int VarRefExpr::compile(Compilation *comp, Function *fun, OutputPort **cursor)
{
  OutputPort *varoutport;
  int need_gate = inConditional();

  if (NULL == (varoutport = comp->resolveVar(fun,this))) {
    fmessage(stderr,"No such variable \"%*\"\n",&m_qn.m_localPart);
    return -1;
  }

  comp->useVar(fun,varoutport,cursor,need_gate,m_sloc);
  return 0;
}

int EmptyExpr::compile(Compilation *comp, Function *fun, OutputPort **cursor)
{
  Instruction *empty = comp->specialop(fun,SPECIAL_NAMESPACE,"empty",1,m_sloc);
  comp->set_and_move_cursor(cursor,empty,0,empty,0);
  return 0;
}

int ContextItemExpr::compile(Compilation *comp, Function *fun, OutputPort **cursor)
{
  Instruction *dot = comp->specialop(fun,SPECIAL_NAMESPACE,"dot",0,m_sloc);
  comp->set_and_move_cursor(cursor,dot,0,dot,0);
  return 0;
}

int NodeTestExpr::compile(Compilation *comp, Function *fun, OutputPort **cursor)
{
  Instruction *dot = comp->specialop(fun,SPECIAL_NAMESPACE,"dot",0,m_sloc);
  comp->set_and_move_cursor(cursor,dot,0,dot,0);

  Instruction *select = comp->specialop(fun,SPECIAL_NAMESPACE,"select",1,m_sloc);
  select->m_axis = m_axis;

  if (XPATH_NODE_TEST_NAME == m_nodetest) {
    select->m_nametest = m_qn.m_localPart;
  }
  else if (XPATH_NODE_TEST_SEQUENCETYPE == m_nodetest) {
    select->m_seqtypetest = m_st;
  }
  else {
    // FIXME: support other types (should there even by others?)
    debug("m_nodetest = %d\n",m_nodetest);
    ASSERT(!"node test type not supported");
  }

  comp->set_and_move_cursor(cursor,select,0,select,0);

  return 0;
}

int SequenceExpr::compile(Compilation *comp, Function *fun, OutputPort **cursor)
{
  Instruction *dup = fun->addInstruction(OP_DUP,m_sloc);
  Instruction *seq = comp->specialop(fun,SPECIAL_NAMESPACE,"sequence",2,m_sloc);
  OutputPort *leftcur = &dup->m_outports[0];
  OutputPort *rightcur = &dup->m_outports[1];

  comp->set_and_move_cursor(cursor,dup,0,seq,0);

  dup->m_outports[0].dest = seq;
  dup->m_outports[0].destp = 0;
  dup->m_outports[1].dest = seq;
  dup->m_outports[1].destp = 1;

  CHECK_CALL(m_left->compile(comp,fun,&leftcur))
  CHECK_CALL(m_right->compile(comp,fun,&rightcur))

  return 0;
}

int ParenExpr::compile(Compilation *comp, Function *fun, OutputPort **cursor)
{
  return m_left->compile(comp,fun,cursor);
}

int ApplyTemplatesExpr::compile(Compilation *comp, Function *fun, OutputPort **cursor)
{
  if (NULL != m_select) {
    CHECK_CALL(m_select->compile(comp,fun,cursor))
  }
  else {
    Instruction *select = comp->specialop(fun,SPECIAL_NAMESPACE,"select",1,m_sloc);
    select->m_axis = AXIS_CHILD;
    select->m_seqtypetest = SequenceType::node();
    comp->set_and_move_cursor(cursor,select,0,select,0);
  }

  Instruction *map;
  CHECK_CALL(comp->compileApplyTemplates(fun,cursor,NULL,m_sloc,&map))
  // FIXME: i think we should connect this map into the graph...
  return 0;
}


int AttributeExpr::compile(Compilation *comp, Function *fun, OutputPort **cursor)
{
  Instruction *dup = fun->addInstruction(OP_DUP,m_sloc);
  Instruction *attr = comp->specialop(fun,SPECIAL_NAMESPACE,"attribute",2,m_sloc);
  OutputPort *namecur = &dup->m_outports[0];
  OutputPort *childcur = &dup->m_outports[1];
  if (NULL != m_name_expr) {
    ASSERT(!"attribute name expressions not yet implemented");
  }
  ASSERT(!m_qn.isNull());

  namecur->dest = attr;
  namecur->destp = 1;
  comp->compileString(fun,m_qn.m_localPart,&namecur,m_sloc);

  childcur->dest = attr;
  childcur->destp = 0;

  if (NULL != m_select)
    CHECK_CALL(m_select->compile(comp,fun,&childcur))
  else
    CHECK_CALL(comp->compileSequence(fun,this,&childcur))

  comp->set_and_move_cursor(cursor,dup,0,attr,0);

  return 0;
}

int ChooseExpr::compile(Compilation *comp, Function *fun, OutputPort **cursor)
{
  Statement *c;
  OutputPort *falsecur = NULL;
  OutputPort **branchcur = cursor;
  int otherwise = 0;

  for (c = m_child; c; c = c->m_next) {
    if (XSLT_WHEN == c->m_type) {
      WhenExpr *when = static_cast<WhenExpr*>(c);
      // when
      OutputPort *truecur = NULL;
      OutputPort *condcur = NULL;
      Instruction *ebv = comp->specialop(fun,SPECIAL_NAMESPACE,"ebv",1,when->m_sloc);
      comp->compileConditional(fun,branchcur,&condcur,&truecur,&falsecur,when->m_sloc);

      CHECK_CALL(when->m_select->compile(comp,fun,&condcur))
      comp->set_and_move_cursor(&condcur,ebv,0,ebv,0);
      CHECK_CALL(comp->compileSequence(fun,when,&truecur))

      branchcur = &falsecur;
    }
    else {
      // otherwise
      CHECK_CALL(comp->compileSequence(fun,c,branchcur))
      otherwise = 1;
    }
  }

  if (!otherwise) {
    Instruction *empty = comp->specialop(fun,SPECIAL_NAMESPACE,"empty",1,m_sloc);
    comp->set_and_move_cursor(branchcur,empty,0,empty,0);
  }

  return 0;
}

int ElementExpr::compile(Compilation *comp, Function *fun, OutputPort **cursor)
{
  Instruction *dup = fun->addInstruction(OP_DUP,m_sloc);
  Instruction *elem = comp->specialop(fun,SPECIAL_NAMESPACE,"element",2,m_sloc);
  OutputPort *namecur = &dup->m_outports[0];
  OutputPort *childcur = &dup->m_outports[1];
  if (NULL != m_name_expr) {
    ASSERT(!"element name expressions not yet implemented");
  }
  ASSERT(!m_qn.isNull());

  if (m_includens) {
    NamespaceMap *m;
    for (m = m_namespaces; m; m = m->parent) {
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
  comp->compileString(fun,m_qn.m_localPart,&namecur,m_sloc);

  childcur->dest = elem;
  childcur->destp = 0;
  CHECK_CALL(comp->compileSequence(fun,this,&childcur))

  comp->set_and_move_cursor(cursor,dup,0,elem,0);

  return 0;
}

int ForEachExpr::compile(Compilation *comp, Function *fun, OutputPort **cursor)
{
  Instruction *map = NULL;
  Instruction *dup = fun->addInstruction(OP_DUP,m_sloc);

  CHECK_CALL(comp->compileInnerFunction(fun,this,&map,cursor,m_sloc))

  comp->set_and_move_cursor(cursor,dup,0,dup,0);
    
  OutputPort *selectcur = &dup->m_outports[1];
  selectcur->dest = map;
  selectcur->destp = 0;

  CHECK_CALL(m_select->compile(comp,fun,&selectcur))

  comp->set_and_move_cursor(cursor,map,1,map,0);

  return 0;
}

int XSLTIfExpr::compile(Compilation *comp, Function *fun, OutputPort **cursor)
{
  OutputPort *falsecur;
  OutputPort *truecur;
  OutputPort *condcur;
  Instruction *ebv = comp->specialop(fun,SPECIAL_NAMESPACE,"ebv",1,m_sloc);
  Instruction *empty = comp->specialop(fun,SPECIAL_NAMESPACE,"empty",1,m_sloc);
  comp->compileConditional(fun,cursor,&condcur,&truecur,&falsecur,m_sloc);
  comp->set_and_move_cursor(&falsecur,empty,0,empty,0);
  CHECK_CALL(comp->compileSequence(fun,this,&truecur))
  CHECK_CALL(m_select->compile(comp,fun,&condcur))
  comp->set_and_move_cursor(&condcur,ebv,0,ebv,0);
  return 0;
}

int NamespaceExpr::compile(Compilation *comp, Function *fun, OutputPort **cursor)
{
  Instruction *dup = fun->addInstruction(OP_DUP,m_sloc);
  Instruction *ns = comp->specialop(fun,SPECIAL_NAMESPACE,"namespace",2,m_sloc);
  OutputPort *prefixcur = &dup->m_outports[0];
  OutputPort *hrefcur = &dup->m_outports[1];

  dup->m_outports[0].dest = ns;
  dup->m_outports[0].destp = 0;
  dup->m_outports[1].dest = ns;
  dup->m_outports[1].destp = 1;

  comp->set_and_move_cursor(cursor,dup,0,ns,0);

  CHECK_CALL(m_name_expr->compile(comp,fun,&prefixcur))
  if (NULL != m_select)
    CHECK_CALL(m_select->compile(comp,fun,&hrefcur))
  else
    CHECK_CALL(comp->compileSequence(fun,this,&hrefcur))

  return 0;
}

int TextExpr::compile(Compilation *comp, Function *fun, OutputPort **cursor)
{
  Instruction *text = comp->specialop(fun,SPECIAL_NAMESPACE,"text",1,m_sloc);
  text->m_str = m_strval;
  comp->set_and_move_cursor(cursor,text,0,text,0);
  return 0;
}

int ValueOfExpr::compile(Compilation *comp, Function *fun, OutputPort **cursor)
{
  Instruction *dup = fun->addInstruction(OP_DUP,m_sloc);
  Instruction *valueof = comp->specialop(fun,SPECIAL_NAMESPACE,"value-of",2,m_sloc);
  OutputPort *valuecur = &dup->m_outports[0];
  OutputPort *sepcur = &dup->m_outports[1];

  comp->set_and_move_cursor(cursor,dup,0,valueof,0);

  dup->m_outports[0].dest = valueof;
  dup->m_outports[0].destp = 0;
  dup->m_outports[1].dest = valueof;
  dup->m_outports[1].destp = 1;

  if (NULL != m_expr1) /* "select" attribute */
    CHECK_CALL(m_expr1->compile(comp,fun,&sepcur))
  else if (NULL != m_select)
    comp->compileString(fun," ",&sepcur,m_sloc);
  else
    comp->compileString(fun,"",&sepcur,m_sloc);

  if (NULL != m_select)
    CHECK_CALL(m_select->compile(comp,fun,&valuecur))
  else
    CHECK_CALL(comp->compileSequence(fun,this,&valuecur))

  return 0;
}

int XSLTSequenceExpr::compile(Compilation *comp, Function *fun, OutputPort **cursor)
{
  return m_select->compile(comp,fun,cursor);
}

// ==================== compile end =================


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

OutputPort *Compilation::resolveVar(Function *fun, VarRefExpr *varref)
{
  OutputPort *outport = NULL;
  unsigned int i;

  SyntaxNode *def = varref->parent()->resolveVar(varref->m_qn);
  if (NULL != def)
    outport = def->m_outp;
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

int Compilation::haveVarReference(List<var_reference*> vars, OutputPort *top_outport)
{
  for (Iterator<var_reference*> it; it.haveCurrent(); it++) {
    var_reference *vr = *it;
    if (vr->top_outport == top_outport)
      return 1;
  }
  return 0;
}

int VarRefExpr::findReferencedVars(Compilation *comp, Function *fun, SyntaxNode *below,
                                   List<var_reference*> &vars)
{
  OutputPort *outport = NULL;

  SyntaxNode *def = m_parent->resolveVar(m_qn);
  if (NULL != def) {
    if (!def->isBelow(below))
      outport = def->m_outp;
  }
  else {
    fmessage(stderr,"No such variable \"%*\"\n",&m_qn.m_localPart);
    return -1;
  }

  if (NULL != outport) {

    if (!comp->haveVarReference(vars,outport)) {
      var_reference *vr = new var_reference();
      OutputPort *local_outport;

      local_outport = comp->resolveVar(fun,this); /* get the local version */
      ASSERT(NULL != local_outport);

      vr->ref = this;

      vr->top_outport = outport;
      vr->local_outport = local_outport;

      vars.push(vr);
    }
  }

  return Expression::findReferencedVars(comp,fun,below,vars);
}

void Compilation::initParamInstructions(Function *fun,
                                    List<SequenceType> &seqtypes, OutputPort **outports)
{
  unsigned int paramno;

  ASSERT(0 < fun->m_params);
  for (paramno = 1; paramno < fun->m_nparams; paramno++) {
    Instruction *swallow = fun->addInstruction(OP_SWALLOW,nosourceloc);
    Instruction *pass = fun->addInstruction(OP_PASS,nosourceloc);

    if (!seqtypes.isEmpty() && (!seqtypes[paramno].isNull()))
      fun->m_params[paramno].st = seqtypes[paramno];
    else
      fun->m_params[paramno].st = SequenceType::itemstar();

    pass->m_outports[0].dest = swallow;
    pass->m_outports[0].destp = 0;
    pass->m_inports[0].st = fun->m_params[paramno].st;
    ASSERT(!fun->m_params[paramno].st.isNull());

    fun->m_params[paramno].start = pass;
    fun->m_params[paramno].outport = &pass->m_outports[0];

    if (outports)
      outports[paramno] = &pass->m_outports[0];
  }
}

int Compilation::compileInnerFunction(Function *fun, SyntaxNode *sn,
                                 Instruction **mapout, OutputPort **cursor, sourceloc sloc)
{
  char name[100];
  Function *innerfun;
  int varno;
  /* FIXME: need to take into variable declarations within XPath expressions e.g. for */
  int need_gate = sn->inConditional();
  ManagedPtrList<var_reference*> vars;
  Instruction *map;
  OutputPort *innerfuncur;

  /* Determine all variables and parameters of the parent function that are referenced
     in this block of code. These must all be declared as parameters to the inner function
     and passed in whenever it is called */
  CHECK_CALL(sn->findReferencedVars(this,fun,sn,vars))

  /* Create the anonymous inner function */
  sprintf(name,"anon%dfun",m_nextanonid++);
  innerfun = m_program->addFunction(NSName(ANON_TEMPLATE_NAMESPACE,name),1+vars.count());

  /* Set the return type of the function. FIXME: this should be derived from the sequence
     constructor within the function */
  innerfun->m_rtype = SequenceType::itemstar();

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
  varno = 1;
  for (Iterator<var_reference*> it = vars; it.haveCurrent(); it++) {
    var_reference *vr = *it;
    Instruction *dup = fun->addInstruction(OP_DUP,sloc);
    OutputPort *varcur = &dup->m_outports[1];

    ASSERT(NULL != vr->top_outport);
    ASSERT(NULL != vr->local_outport);
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



//  innerfun->m_start = innerfun->addInstruction(OP_PASS,sloc);

  /* FIXME: set correct type based on cursor's type */
//  innerfun->m_start->m_inports[0].st = SequenceType::item();

/*   set_and_move_cursor(cursor,map,0,map,0); */

  innerfun->m_params[0].st = SequenceType(xs_g->context_type);
  innerfun->m_params[0].start = innerfun->addInstruction(OP_PASS,sn->m_sloc);
  innerfun->m_params[0].start->m_inports[0].st = innerfun->m_params[0].st;
      
  innerfuncur = &innerfun->m_params[0].start->m_outports[0];
  innerfuncur->dest = innerfun->m_ret;
  innerfuncur->destp = 0;

  CHECK_CALL(compileFunctionContents(innerfun,sn,&innerfuncur))
  ASSERT(NULL != innerfuncur);

  *mapout = map;

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




int Compilation::compileSequence(Function *fun, Statement *parent,
                             OutputPort **cursor)
{
  Statement *sn;
  int havecontent = 0;

  for (sn = parent->m_child; sn; sn = sn->m_next) {

    if (XSLT_VARIABLE == sn->m_type) {
      VariableExpr *variable = static_cast<VariableExpr*>(sn);
      Instruction *swallow = fun->addInstruction(OP_SWALLOW,variable->m_sloc);
      Instruction *pass = fun->addInstruction(OP_PASS,variable->m_sloc);
      Instruction *dup = fun->addInstruction(OP_DUP,variable->m_sloc);
      OutputPort *varcur = &dup->m_outports[1];
/*       message("variable declaration: %s\n",variable->m_qn.localpart); */
      set_and_move_cursor(cursor,dup,0,dup,0);
      varcur->dest = pass;
      varcur->destp = 0;
      pass->m_outports[0].dest = swallow;
      pass->m_outports[0].destp = 0;
      if (NULL != variable->m_select) {
        CHECK_CALL(variable->m_select->compile(this,fun,&varcur))
      }
      else {
        Instruction *docinstr = specialop(fun,SPECIAL_NAMESPACE,"document",1,variable->m_sloc);
        docinstr->m_outports[0].dest = pass;
        docinstr->m_outports[0].destp = 0;
        varcur->dest = docinstr;
        varcur->destp = 0;
        CHECK_CALL(compileSequence(fun,variable,&varcur))
      }
      variable->m_outp = &pass->m_outports[0];
    }
    else if (sn->m_next) {
      Instruction *dup = fun->addInstruction(OP_DUP,sn->m_sloc);
      Instruction *seq = specialop(fun,SPECIAL_NAMESPACE,"sequence",2,sn->m_sloc);

      seq->m_outports[0].dest = (*cursor)->dest;
      seq->m_outports[0].destp = (*cursor)->destp;

      set_and_move_cursor(cursor,dup,0,dup,0);
      (*cursor)->dest = seq;
      (*cursor)->destp = 0;

      CHECK_CALL(sn->compile(this,fun,cursor))

      *cursor = &dup->m_outports[1];
      (*cursor)->dest = seq;
      (*cursor)->destp = 1;

      havecontent = 1;
    }
    else {
      CHECK_CALL(sn->compile(this,fun,cursor))

      havecontent = 1;
    }
  }

  if (!havecontent) {
    Instruction *empty = specialop(fun,SPECIAL_NAMESPACE,"empty",1,parent->m_sloc);
    set_and_move_cursor(cursor,empty,0,empty,0);
  }

  return 0;
}

int Compilation::compileFunctionContents(Function *fun, SyntaxNode *sn, OutputPort **cursor)
{
  if (sn->isExpression())
    return sn->compile(this,fun,cursor);
  else
    return compileSequence(fun,static_cast<Statement*>(sn),cursor);
}

int Compilation::compileFunction(Statement *sn, Function *fun)
{
  OutputPort *cursor;
  List<SequenceType> seqtypes;
  OutputPort **outports;
  Statement *p;
  int paramno;

  /* Compute parameters */

  outports = (OutputPort**)calloc(fun->m_nparams,sizeof(OutputPort*));

  seqtypes.append(SequenceType(xs_g->context_type));
  for (p = sn->m_param; p; p = p->m_next)
    seqtypes.append(static_cast<ParamExpr*>(p)->m_st);
  ASSERT(seqtypes.count() == fun->m_nparams);

  initParamInstructions(fun,seqtypes,outports);

  for (p = sn->m_param, paramno = 1; p; p = p->m_next, paramno++)
    p->m_outp = outports[paramno];

  free(outports);

  /* Compute return type */
  // FIXME: return type for templates
  if (XSLT_DECL_FUNCTION == sn->m_type)
    fun->m_rtype = static_cast<FunctionExpr*>(sn)->m_st;
  if (fun->m_rtype.isNull())
    fun->m_rtype = SequenceType::itemstar();

  /* Add wrapper instructions */
  fun->m_ret = fun->addInstruction(OP_RETURN,sn->m_sloc);

  fun->m_params[0].st = SequenceType(xs_g->context_type);
  fun->m_params[0].start = fun->addInstruction(OP_PASS,sn->m_sloc);
  fun->m_params[0].start->m_inports[0].st = fun->m_params[0].st;

  cursor = &fun->m_params[0].start->m_outports[0];
  cursor->dest = fun->m_ret;
  cursor->destp = 0;

  CHECK_CALL(compileFunctionContents(fun,sn,&cursor))
  ASSERT(NULL != cursor);

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
  innerfun = m_program->addFunction(NSName(ANON_TEMPLATE_NAMESPACE,name),1);

  innerfun->init(nosourceloc);

  innerfun->m_isapply = 1;
  innerfun->m_mode = mode;
  innerfun->m_internal = true;

  innerfun->m_params[0].st = SequenceType(xs_g->context_type);
  innerfun->m_params[0].start = innerfun->addInstruction(OP_PASS,nosourceloc);
  innerfun->m_params[0].start->m_inports[0].st = innerfun->m_params[0].st;

  cursor = &innerfun->m_params[0].start->m_outports[0];
  cursor->dest = innerfun->m_ret;
  cursor->destp = 0;

  Iterator<Template*> it;
  for (it = m_templates; it.haveCurrent(); it++) {
    Template *t = *it;
    Instruction *call = innerfun->addInstruction(OP_CALL,nosourceloc);

    Instruction *dot = specialop(innerfun,SPECIAL_NAMESPACE,"dot",0,nosourceloc);
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

    Instruction *cdup = innerfun->addInstruction(OP_DUP,t->pattern->m_sloc);
    set_and_move_cursor(&condcur,cdup,0,cdup,0);


    set_and_move_cursor(&condcur,dot,0,dot,0);
    set_and_move_cursor(&condcur,dup,0,dup,0);
    set_and_move_cursor(&condcur,root,0,root,0);
    set_and_move_cursor(&condcur,select,0,select,0);



    Instruction *map;
    CHECK_CALL(compileInnerFunction(innerfun,t->pattern,&map,&condcur,t->pattern->m_sloc))
    set_and_move_cursor(&condcur,map,0,map,0);
    cdup->m_outports[1].dest = map;
    cdup->m_outports[1].destp = 1;


//    CHECK_CALL(t->pattern->compile(this,innerfun,&condcur))





    set_and_move_cursor(&condcur,contains,0,contains,0);

    dup->m_outports[1].dest = contains;
    dup->m_outports[1].destp = 1;

    set_and_move_cursor(&truecur,call,0,call,0);

    call->m_cfun = t->fun;

    branchcur = &falsecur;
  }

  /* default template for document and element nodes */
  {
    Instruction *cdot = specialop(innerfun,SPECIAL_NAMESPACE,"dot",0,nosourceloc);
    Instruction *select = specialop(innerfun,SPECIAL_NAMESPACE,"select",1,nosourceloc);
    Instruction *isempty = specialop(innerfun,FN_NAMESPACE,"empty",1,nosourceloc);
    Instruction *not1 = specialop(innerfun,FN_NAMESPACE,"not",1,nosourceloc);
    Instruction *childsel = specialop(innerfun,SPECIAL_NAMESPACE,"select",1,nosourceloc);
    SequenceType doctype = SequenceType(new ItemType(ITEM_DOCUMENT));
    SequenceType elemtype = SequenceType(new ItemType(ITEM_ELEMENT));

    compileConditional(innerfun,branchcur,&condcur,&truecur,&falsecur,nosourceloc);

    select->m_axis = AXIS_SELF;
    select->m_seqtypetest = SequenceType::choice(doctype,elemtype);

    set_and_move_cursor(&condcur,cdot,0,cdot,0);
    set_and_move_cursor(&condcur,select,0,select,0);
    set_and_move_cursor(&condcur,isempty,0,isempty,0);
    set_and_move_cursor(&condcur,not1,0,not1,0);

    Instruction *dup = innerfun->addInstruction(OP_DUP,nosourceloc);
    set_and_move_cursor(&truecur,dup,0,dup,0);

    Instruction *dot = specialop(innerfun,SPECIAL_NAMESPACE,"dot",0,nosourceloc);
    set_and_move_cursor(&truecur,dot,0,dot,0);

    childsel->m_axis = AXIS_CHILD;
    childsel->m_seqtypetest = SequenceType::node();
    set_and_move_cursor(&truecur,childsel,0,childsel,0);

    Instruction *map;
    CHECK_CALL(compileApplyTemplates(innerfun,&truecur,mode,nosourceloc,&map))

    dup->m_outports[1].dest = map;
    dup->m_outports[1].destp = 1;

    branchcur = &falsecur;
  }

  /* default template for text and attribute nodes */
  {
    Instruction *cdot = specialop(innerfun,SPECIAL_NAMESPACE,"dot",0,nosourceloc);
    Instruction *select = specialop(innerfun,SPECIAL_NAMESPACE,"select",1,nosourceloc);
    Instruction *isempty = specialop(innerfun,FN_NAMESPACE,"empty",1,nosourceloc);
    Instruction *not1 = specialop(innerfun,FN_NAMESPACE,"not",1,nosourceloc);
    Instruction *string = specialop(innerfun,FN_NAMESPACE,"string",1,nosourceloc);
    SequenceType texttype = SequenceType(new ItemType(ITEM_TEXT));
    SequenceType attrtype = SequenceType(new ItemType(ITEM_ATTRIBUTE));

    compileConditional(innerfun,branchcur,&condcur,&truecur,&falsecur,nosourceloc);

    select->m_axis = AXIS_SELF;
    select->m_seqtypetest = SequenceType::choice(texttype,attrtype);

    set_and_move_cursor(&condcur,cdot,0,cdot,0);
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
int Compilation::compileApplyTemplates(Function *fun, OutputPort **cursor, const char *mode,
                                       sourceloc sloc, Instruction **map)
{
  Function *applyfun = NULL;

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
  (*map) = fun->addInstruction(OP_MAP,sloc);
  (*map)->m_cfun = applyfun;
  (*map)->freeInports();
  (*map)->m_ninports = applyfun->m_nparams+1;
  (*map)->m_inports = new InputPort[(*map)->m_ninports];

  set_and_move_cursor(cursor,*map,0,*map,0);

  return 0;
}

void Compilation::compileOrderedTemplateList()
{
  Statement *sn;

  for (sn = m_source->root->m_child; sn; sn = static_cast<Statement*>(xl_next_decl(sn))) {
    if (XSLT_DECL_TEMPLATE == sn->m_type) {
      TemplateExpr *te = static_cast<TemplateExpr*>(sn);
      te->m_tmpl = new Template();
      te->m_tmpl->pattern = te->m_select;
      m_templates.append(te->m_tmpl);
    }
  }
}

int Compilation::compileDefaultTemplate()
{
  Function *fun = m_program->addFunction(NSName(String::null(),"default"),1);
  fun->m_internal = true;
  Instruction *pass = fun->addInstruction(OP_PASS,nosourceloc);
  Instruction *ret = fun->addInstruction(OP_RETURN,nosourceloc);
  Instruction *output = specialop(fun,SPECIAL_NAMESPACE,"output",1,nosourceloc);
  OutputPort *atcursor;
  df_seroptions *options;

  fun->m_rtype = SequenceType(xs_g->any_atomic_type);
  fun->m_params[0].st = SequenceType(xs_g->context_type);
  fun->m_params[0].start = pass;

  pass->m_outports[0].dest = ret;
  pass->m_outports[0].destp = 0;
  pass->m_inports[0].st = fun->m_params[0].st;

  options = xslt_get_output_def(m_source,NSName::null());
  ASSERT(NULL != options);
  output->m_seroptions = new df_seroptions(*options);

  atcursor = &pass->m_outports[0];

  Instruction *dup = fun->addInstruction(OP_DUP,nosourceloc);
  set_and_move_cursor(&atcursor,dup,0,dup,0);

  Instruction *dot = specialop(fun,SPECIAL_NAMESPACE,"dot",0,nosourceloc);
  set_and_move_cursor(&atcursor,dot,0,dot,0);

  Instruction *map;
  CHECK_CALL(compileApplyTemplates(fun,&atcursor,NULL,nosourceloc,&map))

  dup->m_outports[1].dest = map;
  dup->m_outports[1].destp = 1;

  set_and_move_cursor(&atcursor,output,0,output,0);

  return 0;
}

int Compilation::compile2()
{
  Statement *sn;
  int templatecount = 0;

  m_program->m_space_decls = list_copy(m_source->space_decls,(list_copy_t)space_decl_copy);

  /* create empty functions first, so that they can be referenced */

  for (sn = m_source->root->m_child; sn; sn = static_cast<Statement*>(xl_next_decl(sn))) {
    unsigned int nparams = 0;
    Statement *p;
    for (p = sn->m_param; p; p = p->m_next)
      nparams++;

    if (XSLT_DECL_FUNCTION == sn->m_type) {
      if (NULL != m_program->getFunction(sn->m_ident)) {
        fmessage(stderr,"Function \"%*\" already defined\n",&sn->m_qn.m_localPart);
        return -1;
      }
      m_program->addFunction(sn->m_ident,nparams+1);
    }
    else if (XSLT_DECL_TEMPLATE == sn->m_type) {
      sn->m_ident = NSName(ANON_TEMPLATE_NAMESPACE,String::format("template%d",templatecount++));
      ASSERT(NULL != static_cast<TemplateExpr*>(sn)->m_tmpl);
      static_cast<TemplateExpr*>(sn)->m_tmpl->fun = m_program->addFunction(sn->m_ident,nparams+1);
    }
  }

  CHECK_CALL(compileDefaultTemplate())

  /* now process the contents of the functions etc */
  for (sn = m_source->root->m_child; sn; sn = static_cast<Statement*>(xl_next_decl(sn))) {
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
      ASSERT(NULL != static_cast<TemplateExpr*>(sn)->m_tmpl->fun);
      CHECK_CALL(compileFunction(sn,static_cast<TemplateExpr*>(sn)->m_tmpl->fun))
      break;
    default:
      ASSERT(0);
      break;
    }
  }

#if 1
  for (Iterator<Function*> it(m_program->m_functions); it.haveCurrent(); it++) {
    Function *fun = *it;
    CHECK_CALL(df_check_function_connected(fun))
    df_remove_redundant(m_program,fun);
    fun->computeTypes();
  }
#endif

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
extern FunctionDefinition *TypeConversion_module;

static FunctionDefinition **modules[10] = {
  &special_module,
  &string_module,
  &sequence_module,
  &node_module,
  &boolean_module,
  &numeric_module,
  &accessor_module,
  &context_module,
  &TypeConversion_module,
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

