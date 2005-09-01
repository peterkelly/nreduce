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
#include "util/list.h"
#include "util/macros.h"
#include "util/debug.h"
#include "dataflow/funops.h"
#include "dataflow/optimization.h"
#include "dataflow/validity.h"
#include <assert.h>
#include <string.h>

typedef struct template template;
typedef struct compilation compilation;

struct compilation {
  error_info *ei;
  xslt_source *source;
  df_program *program;

  xs_globals *globals;
  xs_schema *schema;

  list *templates;
  int nextanonid;
};

struct template {
  xp_expr *pattern;
  df_function *fun;
  int builtin;
};

static void template_free(template *t)
{
  if (t->builtin)
    xp_expr_free(t->pattern);
  free(t);
}

#define ANON_TEMPLATE_NAMESPACE "http://gridxslt.sourceforge.net/anon-template"

static int xslt_compile_expr(compilation *comp, df_function *fun, xp_expr *e,
                  df_outport **cursor);
static int xslt_compile_sequence(compilation *comp, df_function *fun, xl_snode *parent,
                       df_outport **cursor);
static int xslt_compile_funcontents(compilation *comp, df_function *fun, xl_snode *parent,
                                df_outport **cursor);
static int xslt_compile_apply_templates(compilation *comp, df_function *fun,
                                    df_outport **cursor,
                                    list *templates, const char *mode, sourceloc sloc);

static void set_and_move_cursor(df_outport **cursor, df_instruction *destinstr, int destp,
                                df_instruction *newpos, int newdestno)
{
  newpos->outports[newdestno].dest = (*cursor)->dest;
  newpos->outports[newdestno].destp = (*cursor)->destp;
  (*cursor)->dest = destinstr;
  (*cursor)->destp = destp;
  *cursor = &newpos->outports[newdestno];
}

static int xslt_compile_binary_op(compilation *comp, df_function *fun, xp_expr *e,
                              df_outport **cursor, int opcode)
{
  df_instruction *dup = df_add_instruction(comp->program,fun,OP_SPECIAL_DUP,e->sloc);
  df_instruction *binop = df_add_instruction(comp->program,fun,opcode,e->sloc);
  df_outport *left_cursor = &dup->outports[0];
  df_outport *right_cursor = &dup->outports[1];

  left_cursor->dest = binop;
  left_cursor->destp = 0;
  right_cursor->dest = binop;
  right_cursor->destp = 1;
  set_and_move_cursor(cursor,dup,0,binop,0);

  CHECK_CALL(xslt_compile_expr(comp,fun,e->left,&left_cursor))
  CHECK_CALL(xslt_compile_expr(comp,fun,e->right,&right_cursor))

  return 0;
}

static int stmt_in_conditional(xl_snode *stmt)
{
  xl_snode *sn;
  for (sn = stmt->parent; sn; sn = sn->parent)
    if ((XSLT_INSTR_CHOOSE == sn->type) ||
        (XSLT_INSTR_IF == sn->type))
      return 1;
  return 0;
}

static int expr_in_conditional(xp_expr *e)
{
  xp_expr *p;
  xp_expr *prev = e;
  for (p = e->parent; p; p = p->parent) {
    if ((XPATH_EXPR_IF == p->type) && (prev != p->conditional))
      return 1;
    prev = p;
  }
  return stmt_in_conditional(e->stmt);
}

static void df_use_var(compilation *comp, df_function *fun, df_outport *outport,
                       df_outport **cursor, int need_gate, sourceloc sloc)
{
  df_instruction *dest = (*cursor)->dest;
  int destp = (*cursor)->destp;
  df_outport *finalout = NULL;

  if (need_gate) {
    df_instruction *gate = df_add_instruction(comp->program,fun,OP_SPECIAL_GATE,sloc);
    set_and_move_cursor(cursor,gate,1,gate,0);
    dest = gate;
    destp = 0;
  }

  if (OP_SPECIAL_SWALLOW == outport->dest->opcode) {
    df_delete_instruction(comp->program,fun,outport->dest);
    outport->dest = dest;
    outport->destp = destp;
    finalout = outport;
  }
  else {
    df_instruction *dup = df_add_instruction(comp->program,fun,OP_SPECIAL_DUP,sloc);
    dup->outports[0].dest = outport->dest;
    dup->outports[0].destp = outport->destp;
    dup->outports[1].dest = dest;
    dup->outports[1].destp = destp;
    finalout = &dup->outports[1];
    outport->dest = dup;
    outport->destp = 0;
  }

  if (!need_gate) {
    df_instruction *swallow = df_add_instruction(comp->program,fun,OP_SPECIAL_SWALLOW,sloc);
    (*cursor)->dest = swallow;
    (*cursor)->destp = 0;
  }
  *cursor = finalout;
}

static df_outport *resolve_var_to_outport(df_function *fun, xp_expr *varref)
{
  xp_expr *defexpr = NULL;
  xl_snode *defnode = NULL;
  df_outport *outport = NULL;
  int i;

  xp_expr_resolve_var(varref,varref->qn,&defexpr,&defnode);
  if (NULL != defexpr)
    outport = defexpr->outp;
  else if (NULL != defnode)
    outport = defnode->outp;
  else
    return NULL;

  for (i = 0; i < fun->nparams; i++)
    if (fun->params[i].varsource == outport)
      return fun->params[i].outport;

  return outport;
}

static void xslt_compile_string(compilation *comp, df_function *fun, char *str,
                                df_outport **cursor, sourceloc sloc)
{
  df_instruction *instr = df_add_instruction(comp->program,fun,OP_SPECIAL_CONST,sloc);
  instr->outports[0].seqtype = df_seqtype_new_atomic(comp->globals->string_type);
  instr->cvalue = df_value_new(instr->outports[0].seqtype);
  instr->cvalue->value.s = strdup(str);

  /* FIXME: this type is probably not broad enough... we should really accept sequences
     here too */
  instr->inports[0].seqtype = df_seqtype_new_atomic(comp->globals->complex_ur_type);
  set_and_move_cursor(cursor,instr,0,instr,0);
}

static int stmt_below(xl_snode *sn, xl_snode *below)
{
  xl_snode *p;
  for (p = sn; p; p = p->parent)
    if (p == below)
      return 1;
  return 0;
}

typedef struct var_reference var_reference;

struct var_reference {
  xp_expr *ref;
  xp_expr *defexpr;
  xl_snode *defnode;
  df_outport *top_outport;
  df_outport *local_outport;
};

static int have_var_reference(list *l, df_outport *top_outport)
{
  for (; l; l = l->next) {
    var_reference *vr = (var_reference*)l->data;
    if (vr->top_outport == top_outport)
      return 1;
  }
  return 0;
}

static int find_referenced_vars_expr(df_function *fun, xp_expr *e,
                                     xl_snode *below, list **vars)
{
  if (NULL == e)
    return 0;

  if (XPATH_EXPR_VAR_REF == e->type) {
    df_outport *outport = NULL;
    xp_expr *defexpr = NULL;
    xl_snode *defnode = NULL;
    setbuf(stdout,NULL);

    xp_expr_resolve_var(e,e->qn,&defexpr,&defnode);
    if (NULL != defexpr) {
      if (!stmt_below(defexpr->stmt,below))
        outport = defexpr->outp;
    }
    else if (NULL != defnode) {
      if (!stmt_below(defnode,below))
        outport = defnode->outp;
    }
    else {
      fprintf(stderr,"No such variable \"%s\"\n",e->qn.localpart);
      return -1;
    }

    if (NULL != outport) {

      if (!have_var_reference(*vars,outport)) {
        var_reference *vr = (var_reference*)calloc(1,sizeof(var_reference));
        df_outport *local_outport;

        local_outport = resolve_var_to_outport(fun,e); /* get the local version */
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

  CHECK_CALL(find_referenced_vars_expr(fun,e->conditional,below,vars))
  CHECK_CALL(find_referenced_vars_expr(fun,e->left,below,vars))
  CHECK_CALL(find_referenced_vars_expr(fun,e->right,below,vars))

  return 0;
}

static int find_referenced_vars(df_function *fun, xl_snode *sn,
                                xl_snode *below, list **vars)
{
  for (; sn; sn = sn->next) {
    CHECK_CALL(find_referenced_vars_expr(fun,sn->select,below,vars))
    CHECK_CALL(find_referenced_vars_expr(fun,sn->expr1,below,vars))
    CHECK_CALL(find_referenced_vars_expr(fun,sn->expr2,below,vars))
    CHECK_CALL(find_referenced_vars_expr(fun,sn->name_expr,below,vars))
    CHECK_CALL(find_referenced_vars(fun,sn->child,below,vars))
    CHECK_CALL(find_referenced_vars(fun,sn->param,below,vars))
    CHECK_CALL(find_referenced_vars(fun,sn->sort,below,vars))
  }

  return 0;
}

static void init_param_instructions(compilation *comp, df_function *fun,
                                    df_seqtype **seqtypes, df_outport **outports)
{
  int paramno;

  for (paramno = 0; paramno < fun->nparams; paramno++) {
    df_instruction *swallow = df_add_instruction(comp->program,fun,OP_SPECIAL_SWALLOW,nosourceloc);
    df_instruction *pass = df_add_instruction(comp->program,fun,OP_SPECIAL_PASS,nosourceloc);


    if ((NULL != seqtypes) && (NULL != seqtypes[paramno])) {
      fun->params[paramno].seqtype = df_seqtype_ref(seqtypes[paramno]);
    }
    else {
      fun->params[paramno].seqtype = df_seqtype_new_atomic(comp->globals->complex_ur_type);
    }

    pass->outports[0].dest = swallow;
    pass->outports[0].destp = 0;
    pass->inports[0].seqtype = df_seqtype_ref(fun->params[paramno].seqtype);
    assert(NULL != fun->params[paramno].seqtype);

    fun->params[paramno].start = pass;
    fun->params[paramno].outport = &pass->outports[0];

    if (outports)
      outports[paramno] = &pass->outports[0];
  }
}

static int xslt_compile_innerfun(compilation *comp, df_function *fun, xl_snode *sn,
                                 df_instruction **mapout, df_outport **cursor, sourceloc sloc)
{
  char name[100];
  df_function *innerfun;
  int varno;
  int need_gate = stmt_in_conditional(sn);
  list *vars = NULL;
  df_instruction *map;
  list *l;
  df_outport *innerfuncur;

  /* Create the anonymous inner function */
  sprintf(name,"anon%dfun",comp->nextanonid++);
  innerfun = df_new_function(comp->program,nsname_temp(ANON_TEMPLATE_NAMESPACE,name));

  /* Set the return type of the function. FIXME: this should be derived from the sequence
     constructor within the function */
  innerfun->rtype = df_seqtype_new_atomic(comp->globals->complex_ur_type);

  /* Determine all variables and parameters of the parent function that are referenced
     in this block of code. These must all be declared as parameters to the inner function
     and passed in whenever it is called */
  CHECK_CALL(find_referenced_vars(fun,sn->child,sn,&vars))

  innerfun->nparams = list_count(vars);
  innerfun->params = (df_parameter*)calloc(innerfun->nparams,sizeof(df_parameter));

  /* Create the map instruction which will be used to activate the inner function for
     each item in the input sequence. The number of input ports of the instruction
     corresponds to the number of parameters of the inner function, i.e. the number of
     variables and parameters from *outside* the code block that it references */
  map = df_add_instruction(comp->program,fun,OP_SPECIAL_MAP,sloc);
  map->cfun = innerfun;
  df_free_instruction_inports(map);
  map->ninports = innerfun->nparams+1;
  map->inports = (df_inport*)calloc(map->ninports,sizeof(df_inport));

  /* For each parameter and variable referenced from within the code block, connect the
     source to the appropriate input port of the map instruction */
  varno = 0;
  for (l = vars; l; l = l->next) {
    var_reference *vr = (var_reference*)l->data;
    df_instruction *dup = df_add_instruction(comp->program,fun,OP_SPECIAL_DUP,sloc);
    df_outport *varcur = &dup->outports[1];

    assert(NULL != vr->top_outport);
    assert(NULL != vr->local_outport);
    innerfun->params[varno].varsource = vr->top_outport;

    set_and_move_cursor(cursor,dup,0,dup,0);
    varcur->dest = map;
    varcur->destp = varno+1;
    varno++;
    df_use_var(comp,fun,vr->local_outport,&varcur,need_gate,sloc);
  }

  /* FIXME: pass types for each parameter */
  init_param_instructions(comp,innerfun,NULL,NULL);

  innerfun->ret = df_add_instruction(comp->program,innerfun,OP_SPECIAL_RETURN,sloc);
  innerfun->start = df_add_instruction(comp->program,innerfun,OP_SPECIAL_PASS,sloc);

  /* FIXME: set correct type based on cursor's type */
  innerfun->start->inports[0].seqtype = df_seqtype_new_atomic(comp->globals->int_type);
      
  innerfuncur = &innerfun->start->outports[0];
  innerfuncur->dest = innerfun->ret;
  innerfuncur->destp = 0;

  CHECK_CALL(xslt_compile_funcontents(comp,innerfun,sn,&innerfuncur))
  assert(NULL != innerfuncur);

  *mapout = map;
  list_free(vars,free);

  return 0;
}










static int xslt_compile_expr(compilation *comp, df_function *fun, xp_expr *e, df_outport **cursor)
{
  switch (e->type) {
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
    df_instruction *dup = df_add_instruction(comp->program,fun,OP_SPECIAL_DUP,e->sloc);
    df_instruction *split = df_add_instruction(comp->program,fun,OP_SPECIAL_SPLIT,e->sloc);
    df_instruction *merge = df_add_instruction(comp->program,fun,OP_SPECIAL_MERGE,e->sloc);
    df_outport *false_cursor = &split->outports[0];
    df_outport *true_cursor = &split->outports[1];
    df_outport *cond_cursor = &dup->outports[0];

    set_and_move_cursor(cursor,dup,0,merge,0);

    split->outports[0].dest = merge;
    split->outports[0].destp = 0;
    split->outports[1].dest = merge;
    split->outports[1].destp = 0;

    split->outports[2].dest = merge;
    split->outports[2].destp = 1;

    CHECK_CALL(xslt_compile_expr(comp,fun,e->right,&false_cursor))
    CHECK_CALL(xslt_compile_expr(comp,fun,e->left,&true_cursor))
    CHECK_CALL(xslt_compile_expr(comp,fun,e->conditional,&cond_cursor))

    cond_cursor->dest = split;
    cond_cursor->destp = 0;
    dup->outports[1].dest = split;
    dup->outports[1].destp = 1;
    break;
  }
  case XPATH_EXPR_VAR_IN:
    assert(!"not yet implemented"); /* FIXME */
    break;
  case XPATH_EXPR_OR:
    assert(!"not yet implemented"); /* FIXME */
    break;
  case XPATH_EXPR_AND:
    assert(!"not yet implemented"); /* FIXME */
    break;
  case XPATH_EXPR_COMPARE_VALUES:
    switch (e->compare) {
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
    switch (e->compare) {
    case XPATH_GENERAL_COMP_EQ:
      CHECK_CALL(xslt_compile_binary_op(comp,fun,e,cursor,OP_NUMERIC_EQUAL))
      break;
    case XPATH_GENERAL_COMP_NE:
      assert(!"not yet implemented"); /* FIXME */
/*       CHECK_CALL(xslt_compile_binary_op(comp,fun,e,cursor,OP_NE)) */
      break;
    case XPATH_GENERAL_COMP_LT:
      CHECK_CALL(xslt_compile_binary_op(comp,fun,e,cursor,OP_NUMERIC_LESS_THAN))
      break;
    case XPATH_GENERAL_COMP_LE:
      assert(!"not yet implemented"); /* FIXME */
/*       CHECK_CALL(xslt_compile_binary_op(comp,fun,e,cursor,OP_LE)) */
      break;
    case XPATH_GENERAL_COMP_GT:
      CHECK_CALL(xslt_compile_binary_op(comp,fun,e,cursor,OP_NUMERIC_GREATER_THAN))
      break;
    case XPATH_GENERAL_COMP_GE:
      assert(!"not yet implemented"); /* FIXME */
/*       CHECK_CALL(xslt_compile_binary_op(comp,fun,e,cursor,OP_GE)) */
      break;
    default:
      assert(0);
      break;
    }
    break;
  case XPATH_EXPR_COMPARE_NODES:
    switch (e->compare) {
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
    CHECK_CALL(xslt_compile_binary_op(comp,fun,e,cursor,OP_SPECIAL_RANGE))
    break;
  case XPATH_EXPR_ADD:
    CHECK_CALL(xslt_compile_binary_op(comp,fun,e,cursor,OP_NUMERIC_ADD))
    break;
  case XPATH_EXPR_SUBTRACT:
    CHECK_CALL(xslt_compile_binary_op(comp,fun,e,cursor,OP_NUMERIC_SUBTRACT))
    break;
  case XPATH_EXPR_MULTIPLY:
    CHECK_CALL(xslt_compile_binary_op(comp,fun,e,cursor,OP_NUMERIC_MULTIPLY))
    break;
  case XPATH_EXPR_DIVIDE:
    CHECK_CALL(xslt_compile_binary_op(comp,fun,e,cursor,OP_NUMERIC_DIVIDE))
    break;
  case XPATH_EXPR_IDIVIDE:
    break;
  case XPATH_EXPR_MOD:
    CHECK_CALL(xslt_compile_binary_op(comp,fun,e,cursor,OP_NUMERIC_MOD))
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
    df_instruction *root = df_add_instruction(comp->program,fun,OP_SPECIAL_SELECTROOT,e->sloc);
    set_and_move_cursor(cursor,root,0,root,0);
    if (NULL != e->left)
      CHECK_CALL(xslt_compile_expr(comp,fun,e->left,cursor))
    break;
  }
  case XPATH_EXPR_STRING_LITERAL: {
    xslt_compile_string(comp,fun,e->strval,cursor,e->sloc);
    break;
  }
  case XPATH_EXPR_INTEGER_LITERAL: {
    df_instruction *instr = df_add_instruction(comp->program,fun,OP_SPECIAL_CONST,e->sloc);
    instr->outports[0].seqtype = df_seqtype_new_atomic(comp->globals->int_type);
    instr->cvalue = df_value_new(instr->outports[0].seqtype);
    instr->cvalue->value.i = e->ival;

    /* FIXME: this type is probably not broad enough... we should really accept sequences
       here too */
    instr->inports[0].seqtype = df_seqtype_new_atomic(comp->globals->complex_ur_type);
    set_and_move_cursor(cursor,instr,0,instr,0);
    break;
  }
  case XPATH_EXPR_DECIMAL_LITERAL:
    assert(!"not yet implemented"); /* FIXME */
    break;
  case XPATH_EXPR_DOUBLE_LITERAL:
    assert(!"not yet implemented"); /* FIXME */
    break;
  case XPATH_EXPR_VAR_REF: {
    df_outport *varoutport;
    int need_gate = expr_in_conditional(e);

    if (NULL == (varoutport = resolve_var_to_outport(fun,e))) {
      fprintf(stderr,"No such variable \"%s\"\n",e->qn.localpart);
      return -1;
    }

    df_use_var(comp,fun,varoutport,cursor,need_gate,e->sloc);

    break;
  }
  case XPATH_EXPR_EMPTY:
    assert(!"not yet implemented"); /* FIXME */
    break;
  case XPATH_EXPR_CONTEXT_ITEM:
    /* just gets passed through */
    break;
  case XPATH_EXPR_NODE_TEST: {
    df_instruction *select = df_add_instruction(comp->program,fun,OP_SPECIAL_SELECT,e->sloc);
    select->axis = e->axis;

    if (XPATH_NODE_TEST_NAME == e->nodetest) {
      select->nametest = strdup(e->qn.localpart);
    }
    else if (XPATH_NODE_TEST_SEQTYPE == e->nodetest) {
      select->seqtypetest = df_seqtype_ref(e->seqtype);
    }
    else {
      /* FIXME: support other types (should there even by others?) */
      debug("e->nodetest = %d\n",e->nodetest);
      assert(!"node test type not supported");
    }

    set_and_move_cursor(cursor,select,0,select,0);
    break;
  }
  case XPATH_EXPR_ACTUAL_PARAM:
    CHECK_CALL(xslt_compile_expr(comp,fun,e->left,cursor))
    break;
  case XPATH_EXPR_FUNCTION_CALL: {
    df_instruction *call = NULL;
    df_outport *source = *cursor;
    xp_expr *p;
    char *name = NULL;
    int nparams = 0;
    int found = 0;

    /* FIXME: we shouldn't hard-code this prefix; it may be bound to something else, or
       there may be a different prefix bound to the functions and operators namespace. We
       should do the check based on FN_NAMESPACES according to the in-scope namespaces for
       this node in the parse tree */



    if ((NULL == e->qn.prefix) || !strcmp(e->qn.prefix,"fn")) {
      int opid = df_get_op_id(e->qn.localpart);
      if (0 <= opid) {
        call = df_add_instruction(comp->program,fun,opid,e->sloc);
        nparams = df_opdefs[opid].nparams;
        name = df_opdefs[opid].name;
        found = 1;
      }
    }

    if (!found) {
      df_function *cfun;
      /* FIXME: namespace support */
      if (NULL == (cfun = df_lookup_function(comp->program,
                                                   nsname_temp(NULL,e->qn.localpart)))) {
        fprintf(stderr,"No such function \"%s\"\n",e->qn.localpart);
        return -1;
      }

      call = df_add_instruction(comp->program,fun,OP_SPECIAL_CALL,e->sloc);
      call->cfun = cfun;

      nparams = call->cfun->nparams;
      name = call->cfun->ident.name;
    }

    set_and_move_cursor(cursor,call,0,call,0);

    if (0 < nparams) {
      int paramno = 0;
      df_free_instruction_inports(call);
      call->ninports = nparams;
      call->inports = (df_inport*)calloc(call->ninports,sizeof(df_inport));

      for (p = e->left; p; p = p->right) {
        df_outport *param_cursor = NULL; /* FIXME */

        if (paramno < nparams-1) {
          df_instruction *dup = df_add_instruction(comp->program,fun,OP_SPECIAL_DUP,e->sloc);
          source->dest = dup;
          source->destp = 0;
          source = &dup->outports[0];
          source->dest = call;
          source->destp = 0;
          param_cursor = &dup->outports[1];
        }
        else {
          param_cursor = source;
        }

        param_cursor->dest = call;
        param_cursor->destp = paramno;

        assert(XPATH_EXPR_ACTUAL_PARAM == p->type);
        if (paramno >= nparams) {
          fprintf(stderr,"%d: Too many parameters; %s only accepts %d parameters\n",
                  p->sloc.line,name,nparams);
          return 0;
        }

        CHECK_CALL(xslt_compile_expr(comp,fun,p->left,&param_cursor))

        paramno++;
      }
    }
    break;
  }
  case XPATH_EXPR_PAREN:
    CHECK_CALL(xslt_compile_expr(comp,fun,e->left,cursor))
    break;


  case XPATH_EXPR_ATOMIC_TYPE:
    assert(!"not yet implemented"); /* FIXME */
    break;
  case XPATH_EXPR_ITEM_TYPE:
    assert(!"not yet implemented"); /* FIXME */
    break;
  case XPATH_EXPR_SEQUENCE: {
    df_instruction *dup = df_add_instruction(comp->program,fun,OP_SPECIAL_DUP,e->sloc);
    df_instruction *seq = df_add_instruction(comp->program,fun,OP_SPECIAL_SEQUENCE,e->sloc);
    df_outport *leftcur = &dup->outports[0];
    df_outport *rightcur = &dup->outports[1];

    set_and_move_cursor(cursor,dup,0,seq,0);

    dup->outports[0].dest = seq;
    dup->outports[0].destp = 0;
    dup->outports[1].dest = seq;
    dup->outports[1].destp = 1;

    CHECK_CALL(xslt_compile_expr(comp,fun,e->left,&leftcur))
    CHECK_CALL(xslt_compile_expr(comp,fun,e->right,&rightcur))

    break;
  }
  case XPATH_EXPR_STEP: {
    CHECK_CALL(xslt_compile_expr(comp,fun,e->left,cursor))
    CHECK_CALL(xslt_compile_expr(comp,fun,e->right,cursor))
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

static int xslt_compile_conditional(compilation *comp, df_function *fun, df_outport **cursor,
                                    df_outport **condcur, df_outport **truecur,
                                    df_outport **falsecur, sourceloc sloc)
{
  df_instruction *dup = df_add_instruction(comp->program,fun,OP_SPECIAL_DUP,sloc);
  df_instruction *split = df_add_instruction(comp->program,fun,OP_SPECIAL_SPLIT,sloc);
  df_instruction *merge = df_add_instruction(comp->program,fun,OP_SPECIAL_MERGE,sloc);

  set_and_move_cursor(cursor,dup,0,merge,0);

  *falsecur = &split->outports[0];
  *truecur = &split->outports[1];
  *condcur = &dup->outports[0];

  dup->outports[0].dest = split;
  dup->outports[0].destp = 0;
  dup->outports[1].dest = split;
  dup->outports[1].destp = 1;

  split->outports[0].dest = merge;
  split->outports[0].destp = 0;
  split->outports[1].dest = merge;
  split->outports[1].destp = 0;

  split->outports[2].dest = merge;
  split->outports[2].destp = 1;

  return 0;
}

static int xslt_compile_sequence_item(compilation *comp, df_function *fun, xl_snode *sn,
                                  df_outport **cursor)
{
    switch (sn->type) {
    case XSLT_VARIABLE: {
      assert(0); /* handled previosuly */
      break;
    }
    case XSLT_INSTR_ANALYZE_STRING:
      break;
    case XSLT_INSTR_APPLY_IMPORTS:
      break;
    case XSLT_INSTR_APPLY_TEMPLATES: {
        if (NULL != sn->select) {
          CHECK_CALL(xslt_compile_expr(comp,fun,sn->select,cursor))
        }
        else {
          df_instruction *select = df_add_instruction(comp->program,fun,OP_SPECIAL_SELECT,sn->sloc);
          select->axis = AXIS_CHILD;
          select->seqtypetest = df_normalize_itemnode(0);
          set_and_move_cursor(cursor,select,0,select,0);
        }

        CHECK_CALL(xslt_compile_apply_templates(comp,fun,cursor,comp->templates,NULL,sn->sloc))
      }
      break;
    case XSLT_INSTR_ATTRIBUTE: {
      df_instruction *dup = df_add_instruction(comp->program,fun,OP_SPECIAL_DUP,sn->sloc);
      df_instruction *attr = df_add_instruction(comp->program,fun,OP_SPECIAL_ATTRIBUTE,sn->sloc);
      df_outport *namecur = &dup->outports[0];
      df_outport *childcur = &dup->outports[1];
      if (NULL != sn->name_expr) {
        assert(!"attribute name expressions not yet implemented");
      }
      assert(NULL != sn->qn.localpart);

      namecur->dest = attr;
      namecur->destp = 1;
      xslt_compile_string(comp,fun,sn->qn.localpart,&namecur,sn->sloc);

      childcur->dest = attr;
      childcur->destp = 0;

      if (NULL != sn->select)
        CHECK_CALL(xslt_compile_expr(comp,fun,sn->select,&childcur))
      else
        CHECK_CALL(xslt_compile_sequence(comp,fun,sn,&childcur))

      set_and_move_cursor(cursor,dup,0,attr,0);
      break;
    }
    case XSLT_INSTR_CALL_TEMPLATE:
      break;
    case XSLT_INSTR_CHOOSE: {
      xl_snode *c;
      df_outport *falsecur = NULL;
      df_outport **branchcur = cursor;
      int otherwise = 0;

      /* FIXME: when no otherwise, just have a nil in its place */
      for (c = sn->child; c; c = c->next) {
        if (NULL != c->select) {
          /* when */
          df_outport *truecur = NULL;
          df_outport *condcur = NULL;
          xslt_compile_conditional(comp,fun,branchcur,&condcur,&truecur,&falsecur,c->sloc);

          CHECK_CALL(xslt_compile_expr(comp,fun,c->select,&condcur))
          CHECK_CALL(xslt_compile_sequence(comp,fun,c,&truecur))

          branchcur = &falsecur;
        }
        else {
          /* otherwise */
          CHECK_CALL(xslt_compile_sequence(comp,fun,c,branchcur))
          otherwise = 1;
        }
      }

      if (!otherwise) {
        df_instruction *empty = df_add_instruction(comp->program,fun,OP_SPECIAL_EMPTY,sn->sloc);
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
      df_instruction *dup = df_add_instruction(comp->program,fun,OP_SPECIAL_DUP,sn->sloc);
      df_instruction *elem = df_add_instruction(comp->program,fun,OP_SPECIAL_ELEMENT,sn->sloc);
      df_outport *namecur = &dup->outports[0];
      df_outport *childcur = &dup->outports[1];
      if (NULL != sn->name_expr) {
        assert(!"element name expressions not yet implemented");
      }
      assert(NULL != sn->qn.localpart);

      if (sn->includens) {
        ns_map *m;
        for (m = sn->namespaces; m; m = m->parent) {
          list *l;
          for (l = m->defs; l; l = l->next) {
            ns_def *def = (ns_def*)l->data;
            int have = 0;
            list *exl;
            for (exl = elem->nsdefs; exl && !have; exl = exl->next) {
              ns_def *exdef = (ns_def*)exl->data;
              if (nullstr_equals(exdef->prefix,def->prefix))
                have = 1;
            }
            if (!have) {
              ns_def *copy = (ns_def*)calloc(1,sizeof(ns_def));
              copy->prefix = def->prefix ? strdup(def->prefix) : NULL;
              copy->href = strdup(def->href);
              list_append(&elem->nsdefs,copy);
            }
          }
        }
      }

      namecur->dest = elem;
      namecur->destp = 1;
      xslt_compile_string(comp,fun,sn->qn.localpart,&namecur,sn->sloc);

      childcur->dest = elem;
      childcur->destp = 0;
      CHECK_CALL(xslt_compile_sequence(comp,fun,sn,&childcur))

      set_and_move_cursor(cursor,dup,0,elem,0);

      break;
    }
    case XSLT_INSTR_FALLBACK:
      break;
    case XSLT_INSTR_FOR_EACH: {
      df_instruction *map = NULL;
      CHECK_CALL(xslt_compile_innerfun(comp,fun,sn,&map,cursor,sn->sloc))

      map->outports[0].dest = (*cursor)->dest;
      map->outports[0].destp = (*cursor)->destp;
      (*cursor)->dest = map;
      (*cursor)->destp = 0;

      CHECK_CALL(xslt_compile_expr(comp,fun,sn->select,cursor))
      *cursor = &map->outports[0];
      break;
    }
    case XSLT_INSTR_FOR_EACH_GROUP:
      break;
    case XSLT_INSTR_IF:
      break;
    case XSLT_INSTR_MESSAGE:
      break;
    case XSLT_INSTR_NAMESPACE: {
      df_instruction *dup = df_add_instruction(comp->program,fun,OP_SPECIAL_DUP,sn->sloc);
      df_instruction *ns = df_add_instruction(comp->program,fun,OP_SPECIAL_NAMESPACE,sn->sloc);
      df_outport *prefixcur = &dup->outports[0];
      df_outport *hrefcur = &dup->outports[1];

      dup->outports[0].dest = ns;
      dup->outports[0].destp = 0;
      dup->outports[1].dest = ns;
      dup->outports[1].destp = 1;

      set_and_move_cursor(cursor,dup,0,ns,0);

      CHECK_CALL(xslt_compile_expr(comp,fun,sn->name_expr,&prefixcur))
      if (NULL != sn->select)
        CHECK_CALL(xslt_compile_expr(comp,fun,sn->select,&hrefcur))
      else
        CHECK_CALL(xslt_compile_sequence(comp,fun,sn,&hrefcur))
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
      CHECK_CALL(xslt_compile_expr(comp,fun,sn->select,cursor))
      break;
    case XSLT_INSTR_TEXT: {
      df_instruction *text = df_add_instruction(comp->program,fun,OP_SPECIAL_TEXT,sn->sloc);
      text->str = strdup(sn->strval);
      set_and_move_cursor(cursor,text,0,text,0);
      break;
    }
    case XSLT_INSTR_VALUE_OF: {
      df_instruction *dup = df_add_instruction(comp->program,fun,OP_SPECIAL_DUP,sn->sloc);
      df_instruction *valueof = df_add_instruction(comp->program,fun,OP_SPECIAL_VALUE_OF,sn->sloc);
      df_outport *valuecur = &dup->outports[0];
      df_outport *sepcur = &dup->outports[1];

      set_and_move_cursor(cursor,dup,0,valueof,0);

      dup->outports[0].dest = valueof;
      dup->outports[0].destp = 0;
      dup->outports[1].dest = valueof;
      dup->outports[1].destp = 1;

      if (NULL != sn->expr1) /* "select" attribute */
        CHECK_CALL(xslt_compile_expr(comp,fun,sn->expr1,&sepcur))
      else if (NULL != sn->select)
        xslt_compile_string(comp,fun," ",&sepcur,sn->sloc);
      else
        xslt_compile_string(comp,fun,"",&sepcur,sn->sloc);

      if (NULL != sn->select)
        CHECK_CALL(xslt_compile_expr(comp,fun,sn->select,&valuecur))
      else
        CHECK_CALL(xslt_compile_sequence(comp,fun,sn,&valuecur))

      break;
    }
    default:
      assert(0);
      break;
    }
  return 0;
}

static int xslt_compile_sequence(compilation *comp, df_function *fun, xl_snode *parent,
                             df_outport **cursor)
{
  xl_snode *sn;
  int havecontent = 0;

  for (sn = parent->child; sn; sn = sn->next) {

    if (XSLT_VARIABLE == sn->type) {
      df_instruction *swallow = df_add_instruction(comp->program,fun,OP_SPECIAL_SWALLOW,sn->sloc);
      df_instruction *pass = df_add_instruction(comp->program,fun,OP_SPECIAL_PASS,sn->sloc);
      df_instruction *dup = df_add_instruction(comp->program,fun,OP_SPECIAL_DUP,sn->sloc);
      df_outport *varcur = &dup->outports[1];
/*       printf("variable declaration: %s\n",sn->qn.localpart); */
      set_and_move_cursor(cursor,dup,0,dup,0);
      varcur->dest = pass;
      varcur->destp = 0;
      pass->outports[0].dest = swallow;
      pass->outports[0].destp = 0;
      if (NULL != sn->select) {
        CHECK_CALL(xslt_compile_expr(comp,fun,sn->select,&varcur))
      }
      else {
        df_instruction *docinstr = df_add_instruction(comp->program,fun,OP_SPECIAL_DOCUMENT,sn->sloc);
        docinstr->outports[0].dest = pass;
        docinstr->outports[0].destp = 0;
        varcur->dest = docinstr;
        varcur->destp = 0;
        CHECK_CALL(xslt_compile_sequence(comp,fun,sn,&varcur))
      }
      sn->outp = &pass->outports[0];
    }
    else if (sn->next) {
      df_instruction *dup = df_add_instruction(comp->program,fun,OP_SPECIAL_DUP,sn->sloc);
      df_instruction *seq = df_add_instruction(comp->program,fun,OP_SPECIAL_SEQUENCE,sn->sloc);

      seq->outports[0].dest = (*cursor)->dest;
      seq->outports[0].destp = (*cursor)->destp;

      set_and_move_cursor(cursor,dup,0,dup,0);
      (*cursor)->dest = seq;
      (*cursor)->destp = 0;

      CHECK_CALL(xslt_compile_sequence_item(comp,fun,sn,cursor))

      *cursor = &dup->outports[1];
      (*cursor)->dest = seq;
      (*cursor)->destp = 1;

      havecontent = 1;
    }
    else {
      CHECK_CALL(xslt_compile_sequence_item(comp,fun,sn,cursor))

      havecontent = 1;
    }
  }

  if (!havecontent) {
    df_instruction *empty = df_add_instruction(comp->program,fun,OP_SPECIAL_EMPTY,parent->sloc);
    set_and_move_cursor(cursor,empty,0,empty,0);
  }

  return 0;
}

static int xslt_compile_funcontents(compilation *comp, df_function *fun, xl_snode *parent,
                                df_outport **cursor)
{
  return xslt_compile_sequence(comp,fun,parent,cursor);
}

static int xslt_compile_function(compilation *comp, xl_snode *sn, df_function *fun)
{
  df_outport *cursor;
  df_seqtype **seqtypes;
  df_outport **outports;
  xl_snode *p;
  int paramno;

  /* Compute parameters */
  fun->nparams = 0;
  for (p = sn->param; p; p = p->next)
    fun->nparams++;

  if (0 != fun->nparams)
    fun->params = (df_parameter*)calloc(fun->nparams,sizeof(df_parameter));

  seqtypes = (df_seqtype**)calloc(fun->nparams,sizeof(df_seqtype*));
  outports = (df_outport**)calloc(fun->nparams,sizeof(df_outport*));

  for (p = sn->param, paramno = 0; p; p = p->next, paramno++)
    seqtypes[paramno] = p->seqtype;

  init_param_instructions(comp,fun,seqtypes,outports);

  for (p = sn->param, paramno = 0; p; p = p->next, paramno++)
    p->outp = outports[paramno];

  free(seqtypes);
  free(outports);

  /* Compute return type */
  fun->rtype = sn->seqtype ? df_seqtype_ref(sn->seqtype) : NULL;
  if (NULL == fun->rtype) {
    fun->rtype = df_seqtype_new_atomic(comp->globals->complex_ur_type);
    assert(NULL != fun->rtype->item->type);
  }

  /* Add wrapper instructions */
  fun->ret = df_add_instruction(comp->program,fun,OP_SPECIAL_RETURN,sn->sloc);
  fun->start = df_add_instruction(comp->program,fun,OP_SPECIAL_PASS,sn->sloc);
  fun->start->inports[0].seqtype = df_seqtype_new_atomic(comp->globals->int_type);

  cursor = &fun->start->outports[0];
  cursor->dest = fun->ret;
  cursor->destp = 0;

  CHECK_CALL(xslt_compile_funcontents(comp,fun,sn,&cursor))
  assert(NULL != cursor);

  return 0;
}

static int xslt_compile_apply_function(compilation *comp,
                                   list *templates, df_function **ifout, const char *mode)
{
  list *l;
  df_outport *cursor = NULL;
  df_outport *truecur = NULL;
  df_outport *falsecur = NULL;
  df_outport *condcur = NULL;
  df_outport **branchcur = &cursor;
  df_instruction *empty;
  char name[100];
  df_function *innerfun;

  sprintf(name,"anon%dfun",comp->nextanonid++);
  innerfun = df_new_function(comp->program,nsname_temp(ANON_TEMPLATE_NAMESPACE,name));

  df_init_function(comp->program,innerfun,nosourceloc);

  innerfun->isapply = 1;
  innerfun->mode = mode ? strdup(mode) : NULL;


  cursor = &innerfun->start->outports[0];

  for (l = templates; l; l = l->next) {
    template *t = (template*)l->data;
    df_instruction *call = df_add_instruction(comp->program,innerfun,OP_SPECIAL_CALL,nosourceloc);

    df_instruction *dup = df_add_instruction(comp->program,innerfun,OP_SPECIAL_DUP,nosourceloc);
    df_instruction *contains = df_add_instruction(comp->program,innerfun,OP_SPECIAL_CONTAINS_NODE,
                                                  nosourceloc);
    df_instruction *root = df_add_instruction(comp->program,innerfun,FN_ROOT2,nosourceloc);
    df_instruction *select = df_add_instruction(comp->program,innerfun,OP_SPECIAL_SELECT,
                                                nosourceloc);
    select->axis = AXIS_DESCENDANT_OR_SELF;
    select->seqtypetest = df_normalize_itemnode(0);

    xslt_compile_conditional(comp,innerfun,branchcur,&condcur,&truecur,&falsecur,nosourceloc);

    /* FIXME: transform expression as described in XSLT 2.0 section 5.5.3 */

    set_and_move_cursor(&condcur,dup,0,dup,0);
    set_and_move_cursor(&condcur,root,0,root,0);
    set_and_move_cursor(&condcur,select,0,select,0);
    CHECK_CALL(xslt_compile_expr(comp,innerfun,t->pattern,&condcur))

    set_and_move_cursor(&condcur,contains,0,contains,0);

    dup->outports[1].dest = contains;
    dup->outports[1].destp = 1;

    set_and_move_cursor(&truecur,call,0,call,0);

    call->cfun = t->fun;

    branchcur = &falsecur;
  }

  /* default template for document and element nodes */
  {
    df_instruction *select = df_add_instruction(comp->program,innerfun,OP_SPECIAL_SELECT,nosourceloc);
    df_instruction *isempty = df_add_instruction(comp->program,innerfun,FN_EMPTY,nosourceloc);
    df_instruction *not = df_add_instruction(comp->program,innerfun,FN_NOT,nosourceloc);
    df_instruction *childsel = df_add_instruction(comp->program,innerfun,OP_SPECIAL_SELECT,nosourceloc);
    df_seqtype *doctype = df_seqtype_new_item(ITEM_DOCUMENT);
    df_seqtype *elemtype = df_seqtype_new_item(ITEM_ELEMENT);

    xslt_compile_conditional(comp,innerfun,branchcur,&condcur,&truecur,&falsecur,nosourceloc);

    select->axis = AXIS_SELF;
    select->seqtypetest = df_seqtype_new(SEQTYPE_CHOICE);
    select->seqtypetest->left = doctype;
    select->seqtypetest->right = elemtype;

    set_and_move_cursor(&condcur,select,0,select,0);
    set_and_move_cursor(&condcur,isempty,0,isempty,0);
    set_and_move_cursor(&condcur,not,0,not,0);

    childsel->axis = AXIS_CHILD;
    childsel->seqtypetest = df_normalize_itemnode(0);
    set_and_move_cursor(&truecur,childsel,0,childsel,0);
    CHECK_CALL(xslt_compile_apply_templates(comp,innerfun,&truecur,templates,mode,nosourceloc))

    branchcur = &falsecur;
  }

  /* default template for text and attribute nodes */
  {
    df_instruction *select = df_add_instruction(comp->program,innerfun,OP_SPECIAL_SELECT,nosourceloc);
    df_instruction *isempty = df_add_instruction(comp->program,innerfun,FN_EMPTY,nosourceloc);
    df_instruction *not = df_add_instruction(comp->program,innerfun,FN_NOT,nosourceloc);
    df_instruction *string = df_add_instruction(comp->program,innerfun,FN_STRING2,nosourceloc);
    df_seqtype *texttype = df_seqtype_new_item(ITEM_TEXT);
    df_seqtype *attrtype = df_seqtype_new_item(ITEM_ATTRIBUTE);

    xslt_compile_conditional(comp,innerfun,branchcur,&condcur,&truecur,&falsecur,nosourceloc);

    select->axis = AXIS_SELF;
    select->seqtypetest = df_seqtype_new(SEQTYPE_CHOICE);
    select->seqtypetest->left = texttype;
    select->seqtypetest->right = attrtype;

    set_and_move_cursor(&condcur,select,0,select,0);
    set_and_move_cursor(&condcur,isempty,0,isempty,0);
    set_and_move_cursor(&condcur,not,0,not,0);

    set_and_move_cursor(&truecur,string,0,string,0);

    branchcur = &falsecur;
  }

  empty = df_add_instruction(comp->program,innerfun,OP_SPECIAL_EMPTY,nosourceloc);
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
static int xslt_compile_apply_templates(compilation *comp, df_function *fun,
                                    df_outport **cursor,
                                    list *templates, const char *mode, sourceloc sloc)
{
  list *l;
  df_function *applyfun = NULL;
  df_instruction *map = NULL;

  /* Do we already have an anonymous function for this mode? */
  for (l = comp->program->functions; l; l = l->next) {
    df_function *candidate = (df_function*)l->data;
    if (candidate->isapply) {
      if (((NULL == mode) && (NULL == candidate->mode)) ||
          ((NULL != mode) && (NULL != candidate->mode) && !strcmp(mode,candidate->mode))) {
        applyfun = candidate;
        break;
      }
    }
  }

  /* Create the anonymous function if it doesn't already exist */
  if (NULL == applyfun)
    CHECK_CALL(xslt_compile_apply_function(comp,templates,&applyfun,mode))

  /* Add the map instruction */
  map = df_add_instruction(comp->program,fun,OP_SPECIAL_MAP,sloc);
  map->cfun = applyfun;
  set_and_move_cursor(cursor,map,0,map,0);

  return 0;
}

static list *xslt_compile_ordered_template_list(compilation *comp)
{
  xl_snode *sn;
  list *templates = NULL;

  for (sn = comp->source->root->child; sn; sn = xl_next_decl(sn)) {
    if (XSLT_DECL_TEMPLATE == sn->type) {
      sn->tmpl = (template*)calloc(1,sizeof(template));
      sn->tmpl->pattern = sn->select;
      list_push(&templates,sn->tmpl);
    }
  }

  return templates;
}

static int xslt_compile_default_template(compilation *comp)
{
  df_function *fun = df_new_function(comp->program,nsname_temp(NULL,"default"));
  df_instruction *pass = df_add_instruction(comp->program,fun,OP_SPECIAL_PASS,nosourceloc);
  df_instruction *ret = df_add_instruction(comp->program,fun,OP_SPECIAL_RETURN,nosourceloc);
  df_instruction *output = df_add_instruction(comp->program,fun,OP_SPECIAL_OUTPUT,nosourceloc);
  df_outport *atcursor;
  df_seroptions *options;

  pass->outports[0].dest = ret;
  pass->outports[0].destp = 0;
  fun->rtype = df_seqtype_new_atomic(comp->globals->any_atomic_type);
  pass->inports[0].seqtype = df_seqtype_new_atomic(comp->globals->any_atomic_type);

  options = xslt_get_output_def(comp->source,nsname_temp(NULL,NULL));
  assert(NULL != options);
  output->seroptions = df_seroptions_copy(options);

  atcursor = &pass->outports[0];
  CHECK_CALL(xslt_compile_apply_templates(comp,fun,&atcursor,comp->templates,NULL,nosourceloc))
  set_and_move_cursor(&atcursor,output,0,output,0);

  fun->start = pass;

  return 0;
}

static int xslt_compile2(compilation *comp)
{
  xl_snode *sn;
  list *l;
  int templatecount = 0;

  comp->program->space_decls = list_copy(comp->source->space_decls,(list_copy_t)space_decl_copy);

  /* create empty functions first, so that they can be referenced */

  for (sn = comp->source->root->child; sn; sn = xl_next_decl(sn)) {
    if (XSLT_DECL_FUNCTION == sn->type) {
      if (NULL != df_lookup_function(comp->program,sn->ident)) {
        fprintf(stderr,"Function \"%s\" already defined\n",sn->qn.localpart);
        return -1;
      }
      df_new_function(comp->program,sn->ident);
    }
    else if (XSLT_DECL_TEMPLATE == sn->type) {
      sn->ident.name = (char*)malloc(100);
      sprintf(sn->ident.name,"template%d",templatecount++);
      sn->ident.ns = strdup(ANON_TEMPLATE_NAMESPACE);
      assert(NULL != sn->tmpl);
      sn->tmpl->fun = df_new_function(comp->program,sn->ident);
    }
  }

  CHECK_CALL(xslt_compile_default_template(comp))

  /* now process the contents of the functions etc */

  for (sn = comp->source->root->child; sn; sn = xl_next_decl(sn)) {
    switch (sn->type) {
    case XSLT_DECL_ATTRIBUTE_SET:
      break;
    case XSLT_DECL_CHARACTER_MAP:
      break;
    case XSLT_DECL_DECIMAL_FORMAT:
      break;
    case XSLT_DECL_FUNCTION: {
      df_function *fun = df_lookup_function(comp->program,sn->ident);
      CHECK_CALL(xslt_compile_function(comp,sn,fun))
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
      assert(NULL != sn->tmpl->fun);
      CHECK_CALL(xslt_compile_function(comp,sn,sn->tmpl->fun))
      break;
    default:
      assert(0);
      break;
    }
  }

  for (l = comp->program->functions; l; l = l->next) {
    df_function *fun = (df_function*)l->data;
    CHECK_CALL(df_check_function_connected(fun))
    df_remove_redundant(comp->program,fun);
    CHECK_CALL(df_compute_types(fun))
  }

  return 0;
}

int xslt_compile(error_info *ei, xslt_source *source, df_program **program)
{
  int r;
  compilation comp;

  memset(&comp,0,sizeof(comp));
  *program = df_program_new(source->schema);

  comp.ei = ei;
  comp.source = source;
  comp.program = *program;
  comp.globals = source->globals;
  comp.schema = source->schema;
  comp.templates = xslt_compile_ordered_template_list(&comp);

  r = xslt_compile2(&comp);

  list_free(comp.templates,(void*)template_free);

  if (0 != r) {
    df_program_free(*program);
    *program = NULL;
  }

  return r;
}

