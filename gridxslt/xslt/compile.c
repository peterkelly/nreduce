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

static int df_build_expr(df_state *s, df_function *fun, xp_expr *e,
                  df_outport **cursor);
static int df_build_sequence(df_state *state, df_function *fun, xl_snode *parent,
                       df_outport **cursor);
static int df_build_funcontents(df_state *state, df_function *fun, xl_snode *parent,
                                df_outport **cursor);
static int df_build_apply_templates(df_state *state, df_function *fun,
                                    xl_snode *root, df_outport **cursor,
                                    list *templates, const char *mode);

static void set_and_move_cursor(df_outport **cursor, df_instruction *destinstr, int destp,
                                df_instruction *newpos, int newdestno)
{
  newpos->outports[newdestno].dest = (*cursor)->dest;
  newpos->outports[newdestno].destp = (*cursor)->destp;
  (*cursor)->dest = destinstr;
  (*cursor)->destp = destp;
  *cursor = &newpos->outports[newdestno];
}

static int df_build_binary_op(df_state *state, df_function *fun, xp_expr *e,
                              df_outport **cursor, int opcode)
{
  df_instruction *dup = df_add_instruction(state,fun,OP_SPECIAL_DUP);
  df_instruction *binop = df_add_instruction(state,fun,opcode);
  df_outport *left_cursor = &dup->outports[0];
  df_outport *right_cursor = &dup->outports[1];

  left_cursor->dest = binop;
  left_cursor->destp = 0;
  right_cursor->dest = binop;
  right_cursor->destp = 1;
  set_and_move_cursor(cursor,dup,0,binop,0);

  CHECK_CALL(df_build_expr(state,fun,e->left,&left_cursor))
  CHECK_CALL(df_build_expr(state,fun,e->right,&right_cursor))

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

static void df_use_var(df_state *state, df_function *fun, df_outport *outport,
                       df_outport **cursor, int need_gate)
{
  df_instruction *dest = (*cursor)->dest;
  int destp = (*cursor)->destp;
  df_outport *finalout = NULL;

  if (need_gate) {
    df_instruction *gate = df_add_instruction(state,fun,OP_SPECIAL_GATE);
    set_and_move_cursor(cursor,gate,1,gate,0);
    dest = gate;
    destp = 0;
  }

  if (OP_SPECIAL_SWALLOW == outport->dest->opcode) {
    df_delete_instruction(state,fun,outport->dest);
    outport->dest = dest;
    outport->destp = destp;
    finalout = outport;
  }
  else {
    df_instruction *dup = df_add_instruction(state,fun,OP_SPECIAL_DUP);
    dup->outports[0].dest = outport->dest;
    dup->outports[0].destp = outport->destp;
    dup->outports[1].dest = dest;
    dup->outports[1].destp = destp;
    finalout = &dup->outports[1];
    outport->dest = dup;
    outport->destp = 0;
  }

  if (!need_gate) {
    df_instruction *swallow = df_add_instruction(state,fun,OP_SPECIAL_SWALLOW);
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

static void df_build_string(df_state *state, df_function *fun, char *str, df_outport **cursor)
{
  df_instruction *instr = df_add_instruction(state,fun,OP_SPECIAL_CONST);
  instr->outports[0].seqtype = df_seqtype_new_item(ITEM_ATOMIC);
  instr->outports[0].seqtype->item->type = state->schema->globals->string_type;
  instr->cvalue = df_value_new(instr->outports[0].seqtype);
  instr->cvalue->value.s = strdup(str);

  /* FIXME: this type is probably not broad enough... we should really accept sequences
     here too */
  instr->inports[0].seqtype = df_seqtype_new_item(ITEM_ATOMIC);
  instr->inports[0].seqtype->item->type = state->schema->globals->complex_ur_type;
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

static int find_referenced_vars_expr(df_state *state, df_function *fun, xp_expr *e,
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

  CHECK_CALL(find_referenced_vars_expr(state,fun,e->conditional,below,vars))
  CHECK_CALL(find_referenced_vars_expr(state,fun,e->left,below,vars))
  CHECK_CALL(find_referenced_vars_expr(state,fun,e->right,below,vars))

  return 0;
}

static int find_referenced_vars(df_state *state, df_function *fun, xl_snode *sn,
                                xl_snode *below, list **vars)
{
  for (; sn; sn = sn->next) {
    CHECK_CALL(find_referenced_vars_expr(state,fun,sn->select,below,vars))
    CHECK_CALL(find_referenced_vars_expr(state,fun,sn->expr1,below,vars))
    CHECK_CALL(find_referenced_vars_expr(state,fun,sn->expr2,below,vars))
    CHECK_CALL(find_referenced_vars_expr(state,fun,sn->name_expr,below,vars))
    CHECK_CALL(find_referenced_vars(state,fun,sn->child,below,vars))
    CHECK_CALL(find_referenced_vars(state,fun,sn->param,below,vars))
    CHECK_CALL(find_referenced_vars(state,fun,sn->sort,below,vars))
  }

  return 0;
}

static void init_param_instructions(df_state *state, df_function *fun,
                                    df_seqtype **seqtypes, df_outport **outports)
{
  int paramno;

  for (paramno = 0; paramno < fun->nparams; paramno++) {
    df_instruction *swallow = df_add_instruction(state,fun,OP_SPECIAL_SWALLOW);
    df_instruction *pass = df_add_instruction(state,fun,OP_SPECIAL_PASS);


    if ((NULL != seqtypes) && (NULL != seqtypes[paramno])) {
      fun->params[paramno].seqtype = df_seqtype_ref(seqtypes[paramno]);
    }
    else {
      fun->params[paramno].seqtype = df_seqtype_new_item(ITEM_ATOMIC);
      fun->params[paramno].seqtype->item->type = state->schema->globals->complex_ur_type;
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

static int df_build_innerfun(df_state *state, df_function *fun, xl_snode *sn,
                             df_instruction **mapout, df_outport **cursor)
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
  sprintf(name,"anon%dfun",state->nextanonid++);
  innerfun = df_new_function(state,nsname_temp(ANON_TEMPLATE_NAMESPACE,name));

  /* Set the return type of the function. FIXME: this should be derived from the sequence
     constructor within the function */
  innerfun->rtype = df_seqtype_new_item(ITEM_ATOMIC);
  innerfun->rtype->item->type = state->schema->globals->complex_ur_type;

  /* Determine all variables and parameters of the parent function that are referenced
     in this block of code. These must all be declared as parameters to the inner function
     and passed in whenever it is called */
  CHECK_CALL(find_referenced_vars(state,fun,sn->child,sn,&vars))

  innerfun->nparams = list_count(vars);
  innerfun->params = (df_parameter*)calloc(innerfun->nparams,sizeof(df_parameter));

  /* Create the map instruction which will be used to activate the inner function for
     each item in the input sequence. The number of input ports of the instruction
     corresponds to the number of parameters of the inner function, i.e. the number of
     variables and parameters from *outside* the code block that it references */
  map = df_add_instruction(state,fun,OP_SPECIAL_MAP);
  map->cfun = innerfun;
  df_free_instruction_inports(map);
  map->ninports = innerfun->nparams+1;
  map->inports = (df_inport*)calloc(map->ninports,sizeof(df_inport));

  /* For each parameter and variable referenced from within the code block, connect the
     source to the appropriate input port of the map instruction */
  varno = 0;
  for (l = vars; l; l = l->next) {
    var_reference *vr = (var_reference*)l->data;
    df_instruction *dup = df_add_instruction(state,fun,OP_SPECIAL_DUP);
    df_outport *varcur = &dup->outports[1];

    assert(NULL != vr->top_outport);
    assert(NULL != vr->local_outport);
    innerfun->params[varno].varsource = vr->top_outport;

    set_and_move_cursor(cursor,dup,0,dup,0);
    varcur->dest = map;
    varcur->destp = varno+1;
    varno++;
    df_use_var(state,fun,vr->local_outport,&varcur,need_gate);
  }

  /* FIXME: pass types for each parameter */
  init_param_instructions(state,innerfun,NULL,NULL);

  innerfun->ret = df_add_instruction(state,innerfun,OP_SPECIAL_RETURN);
  innerfun->start = df_add_instruction(state,innerfun,OP_SPECIAL_PASS);

  /* FIXME: set correct type based on cursor's type */
  innerfun->start->inports[0].seqtype = df_seqtype_new_item(ITEM_ATOMIC);
  innerfun->start->inports[0].seqtype->item->type = state->schema->globals->int_type;
      
  innerfuncur = &innerfun->start->outports[0];
  innerfuncur->dest = innerfun->ret;
  innerfuncur->destp = 0;

  CHECK_CALL(df_build_funcontents(state,innerfun,sn,&innerfuncur))
  assert(NULL != innerfuncur);

  *mapout = map;
  list_free(vars,free);

  return 0;
}










static int df_build_expr(df_state *state, df_function *fun, xp_expr *e, df_outport **cursor)
{
  switch (e->type) {
  case XPATH_EXPR_INVALID:
    break;
  case XPATH_EXPR_FOR:
    break;
  case XPATH_EXPR_SOME:
    break;
  case XPATH_EXPR_EVERY:
    break;
  case XPATH_EXPR_IF: {
    df_instruction *dup = df_add_instruction(state,fun,OP_SPECIAL_DUP);
    df_instruction *split = df_add_instruction(state,fun,OP_SPECIAL_SPLIT);
    df_instruction *merge = df_add_instruction(state,fun,OP_SPECIAL_MERGE);
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

    CHECK_CALL(df_build_expr(state,fun,e->right,&false_cursor))
    CHECK_CALL(df_build_expr(state,fun,e->left,&true_cursor))
    CHECK_CALL(df_build_expr(state,fun,e->conditional,&cond_cursor))

    cond_cursor->dest = split;
    cond_cursor->destp = 0;
    dup->outports[1].dest = split;
    dup->outports[1].destp = 1;
    break;
  }
  case XPATH_EXPR_VAR_IN:
    break;
  case XPATH_EXPR_OR:
    break;
  case XPATH_EXPR_AND:
    break;
  case XPATH_EXPR_COMPARE_VALUES:
    switch (e->compare) {
    case XPATH_VALUE_COMP_EQ:
      break;
    case XPATH_VALUE_COMP_NE:
      break;
    case XPATH_VALUE_COMP_LT:
      break;
    case XPATH_VALUE_COMP_LE:
      break;
    case XPATH_VALUE_COMP_GT:
      break;
    case XPATH_VALUE_COMP_GE:
      break;
    default:
      assert(0);
      break;
    }
    break;
  case XPATH_EXPR_COMPARE_GENERAL:
    switch (e->compare) {
    case XPATH_GENERAL_COMP_EQ:
      CHECK_CALL(df_build_binary_op(state,fun,e,cursor,OP_NUMERIC_EQUAL))
      break;
    case XPATH_GENERAL_COMP_NE:
/*       CHECK_CALL(df_build_binary_op(state,fun,e,cursor,OP_NE)) */
      break;
    case XPATH_GENERAL_COMP_LT:
      CHECK_CALL(df_build_binary_op(state,fun,e,cursor,OP_NUMERIC_LESS_THAN))
      break;
    case XPATH_GENERAL_COMP_LE:
/*       CHECK_CALL(df_build_binary_op(state,fun,e,cursor,OP_LE)) */
      break;
    case XPATH_GENERAL_COMP_GT:
      CHECK_CALL(df_build_binary_op(state,fun,e,cursor,OP_NUMERIC_GREATER_THAN))
      break;
    case XPATH_GENERAL_COMP_GE:
/*       CHECK_CALL(df_build_binary_op(state,fun,e,cursor,OP_GE)) */
      break;
    default:
      assert(0);
      break;
    }
    break;
  case XPATH_EXPR_COMPARE_NODES:
    switch (e->compare) {
    case XPATH_NODE_COMP_IS:
      break;
    case XPATH_NODE_COMP_PRECEDES:
      break;
    case XPATH_NODE_COMP_FOLLOWS:
      break;
    default:
      assert(0);
      break;
    }
    break;
  case XPATH_EXPR_TO:
    break;
  case XPATH_EXPR_ADD:
    CHECK_CALL(df_build_binary_op(state,fun,e,cursor,OP_NUMERIC_ADD))
    break;
  case XPATH_EXPR_SUBTRACT:
    CHECK_CALL(df_build_binary_op(state,fun,e,cursor,OP_NUMERIC_SUBTRACT))
    break;
  case XPATH_EXPR_MULTIPLY:
    CHECK_CALL(df_build_binary_op(state,fun,e,cursor,OP_NUMERIC_MULTIPLY))
    break;
  case XPATH_EXPR_DIVIDE:
    CHECK_CALL(df_build_binary_op(state,fun,e,cursor,OP_NUMERIC_DIVIDE))
    break;
  case XPATH_EXPR_IDIVIDE:
    break;
  case XPATH_EXPR_MOD:
    CHECK_CALL(df_build_binary_op(state,fun,e,cursor,OP_NUMERIC_MOD))
    break;
  case XPATH_EXPR_UNION:
    break;
  case XPATH_EXPR_UNION2:
    break;
  case XPATH_EXPR_INTERSECT:
    break;
  case XPATH_EXPR_EXCEPT:
    break;
  case XPATH_EXPR_INSTANCE_OF:
    break;
  case XPATH_EXPR_TREAT:
    break;
  case XPATH_EXPR_CASTABLE:
    break;
  case XPATH_EXPR_CAST:
    break;
  case XPATH_EXPR_UNARY_MINUS:
    break;
  case XPATH_EXPR_UNARY_PLUS:
    break;
  case XPATH_EXPR_ROOT:
    break;
  case XPATH_EXPR_STRING_LITERAL: {
    df_build_string(state,fun,e->strval,cursor);
    break;
  }
  case XPATH_EXPR_INTEGER_LITERAL: {
    df_instruction *instr = df_add_instruction(state,fun,OP_SPECIAL_CONST);
    instr->outports[0].seqtype = df_seqtype_new_item(ITEM_ATOMIC);
    instr->outports[0].seqtype->item->type = state->schema->globals->int_type;
    instr->cvalue = df_value_new(instr->outports[0].seqtype);
    instr->cvalue->value.i = e->ival;

    /* FIXME: this type is probably not broad enough... we should really accept sequences
       here too */
    instr->inports[0].seqtype = df_seqtype_new_item(ITEM_ATOMIC);
    instr->inports[0].seqtype->item->type = state->schema->globals->complex_ur_type;
    set_and_move_cursor(cursor,instr,0,instr,0);
    break;
  }
  case XPATH_EXPR_DECIMAL_LITERAL:
    break;
  case XPATH_EXPR_DOUBLE_LITERAL:
    break;
  case XPATH_EXPR_VAR_REF: {
    df_outport *varoutport;
    int need_gate = expr_in_conditional(e);

    if (NULL == (varoutport = resolve_var_to_outport(fun,e))) {
      fprintf(stderr,"No such variable \"%s\"\n",e->qn.localpart);
      return -1;
    }

    df_use_var(state,fun,varoutport,cursor,need_gate);

    break;
  }
  case XPATH_EXPR_EMPTY:
    break;
  case XPATH_EXPR_CONTEXT_ITEM:
    /* just gets passed through */
    break;
  case XPATH_EXPR_NODE_TEST: {
    df_instruction *select = df_add_instruction(state,fun,OP_SPECIAL_SELECT);
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
    CHECK_CALL(df_build_expr(state,fun,e->left,cursor))
    break;
  case XPATH_EXPR_FUNCTION_CALL: {
    df_instruction *call = NULL;
    df_outport *source = *cursor;
    xp_expr *p;
    char *name = NULL;
    int nparams = 0;

    /* FIXME: we shouldn't hard-code this prefix; it may be bound to something else, or
       there may be a different prefix bound to the functions and operators namespace. We
       should do the check based on FN_NAMESPACES according to the in-scope namespaces for
       this node in the parse tree */
    if ((NULL != e->qn.prefix) && !strcmp(e->qn.prefix,"fn")) {
      int opid = df_get_op_id(e->qn.localpart);
      if (0 > opid) {
        fprintf(stderr,"No such function \"%s\"\n",e->qn.localpart);
        return -1;
      }
      call = df_add_instruction(state,fun,opid);
      nparams = df_opdefs[opid].nparams;
      name = df_opdefs[opid].name;
    }
    else {
      call = df_add_instruction(state,fun,OP_SPECIAL_CALL);
      /* FIXME: namespace support */
      if (NULL == (call->cfun = df_lookup_function(state,nsname_temp(NULL,e->qn.localpart)))) {
        fprintf(stderr,"No such function \"%s\"\n",e->qn.localpart);
        return -1;
      }

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
          df_instruction *dup = df_add_instruction(state,fun,OP_SPECIAL_DUP);
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
                  p->defline,name,nparams);
          return 0;
        }

        CHECK_CALL(df_build_expr(state,fun,p->left,&param_cursor))

        paramno++;
      }
    }
    break;
  }
  case XPATH_EXPR_PAREN:
    CHECK_CALL(df_build_expr(state,fun,e->left,cursor))
    break;


  case XPATH_EXPR_ATOMIC_TYPE:
    assert(!"not yet implemented");
    break;
  case XPATH_EXPR_ITEM_TYPE:
    assert(!"not yet implemented");
    break;
  case XPATH_EXPR_SEQUENCE: {
    df_instruction *dup = df_add_instruction(state,fun,OP_SPECIAL_DUP);
    df_instruction *seq = df_add_instruction(state,fun,OP_SPECIAL_SEQUENCE);
    df_outport *leftcur = &dup->outports[0];
    df_outport *rightcur = &dup->outports[1];

    set_and_move_cursor(cursor,dup,0,seq,0);

    dup->outports[0].dest = seq;
    dup->outports[0].destp = 0;
    dup->outports[1].dest = seq;
    dup->outports[1].destp = 1;

    CHECK_CALL(df_build_expr(state,fun,e->left,&leftcur))
    CHECK_CALL(df_build_expr(state,fun,e->right,&rightcur))

    break;
  }
  case XPATH_EXPR_STEP: {
    CHECK_CALL(df_build_expr(state,fun,e->left,cursor))
    CHECK_CALL(df_build_expr(state,fun,e->right,cursor))
    break;
  }
  case XPATH_EXPR_VARINLIST:
    assert(!"not yet implemented");
    break;
  case XPATH_EXPR_PARAMLIST:
    assert(!"not yet implemented");
    break;
  case XPATH_EXPR_FILTER:
    assert(!"not yet implemented");
    break;

  default:
    assert(0);
    break;
  }

  return 0;
}

static int df_build_conditional(df_state *state, df_function *fun, df_outport **cursor,
                                df_outport **condcur, df_outport **truecur, df_outport **falsecur)
{
  df_instruction *dup = df_add_instruction(state,fun,OP_SPECIAL_DUP);
  df_instruction *split = df_add_instruction(state,fun,OP_SPECIAL_SPLIT);
  df_instruction *merge = df_add_instruction(state,fun,OP_SPECIAL_MERGE);

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

static int df_build_sequence_item(df_state *state, df_function *fun, xl_snode *sn,
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
        xl_snode *root = sn;
        while (NULL != root->parent)
          root = root->parent;

        if (NULL != sn->select) {
          CHECK_CALL(df_build_expr(state,fun,sn->select,cursor))
        }
        else {
          df_instruction *select = df_add_instruction(state,fun,OP_SPECIAL_SELECT);
          select->axis = AXIS_CHILD;
          select->seqtypetest = df_normalize_itemnode(0);
          set_and_move_cursor(cursor,select,0,select,0);
        }

        CHECK_CALL(df_build_apply_templates(state,fun,root,cursor,root->templates,NULL))
      }
      break;
    case XSLT_INSTR_ATTRIBUTE: {
      df_instruction *dup = df_add_instruction(state,fun,OP_SPECIAL_DUP);
      df_instruction *attr = df_add_instruction(state,fun,OP_SPECIAL_ATTRIBUTE);
      df_outport *namecur = &dup->outports[0];
      df_outport *childcur = &dup->outports[1];
      if (NULL != sn->name_expr) {
        assert(!"attribute name expressions not yet implemented");
      }
      assert(NULL != sn->qn.localpart);

      namecur->dest = attr;
      namecur->destp = 1;
      df_build_string(state,fun,sn->qn.localpart,&namecur);

      childcur->dest = attr;
      childcur->destp = 0;

      if (NULL != sn->select)
        CHECK_CALL(df_build_expr(state,fun,sn->select,&childcur))
      else
        CHECK_CALL(df_build_sequence(state,fun,sn,&childcur))

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
          df_build_conditional(state,fun,branchcur,&condcur,&truecur,&falsecur);

          CHECK_CALL(df_build_expr(state,fun,c->select,&condcur))
          CHECK_CALL(df_build_sequence(state,fun,c,&truecur))

          branchcur = &falsecur;
        }
        else {
          /* otherwise */
          CHECK_CALL(df_build_sequence(state,fun,c,branchcur))
          otherwise = 1;
        }
      }

      if (!otherwise) {
        df_instruction *empty = df_add_instruction(state,fun,OP_SPECIAL_EMPTY);
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
      df_instruction *dup = df_add_instruction(state,fun,OP_SPECIAL_DUP);
      df_instruction *elem = df_add_instruction(state,fun,OP_SPECIAL_ELEMENT);
      df_outport *namecur = &dup->outports[0];
      df_outport *childcur = &dup->outports[1];
      if (NULL != sn->name_expr) {
        assert(!"element name expressions not yet implemented");
      }
      assert(NULL != sn->qn.localpart);

      namecur->dest = elem;
      namecur->destp = 1;
      df_build_string(state,fun,sn->qn.localpart,&namecur);

      childcur->dest = elem;
      childcur->destp = 0;
      CHECK_CALL(df_build_sequence(state,fun,sn,&childcur))

      set_and_move_cursor(cursor,dup,0,elem,0);

      break;
    }
    case XSLT_INSTR_FALLBACK:
      break;
    case XSLT_INSTR_FOR_EACH: {
      df_instruction *map = NULL;
      CHECK_CALL(df_build_innerfun(state,fun,sn,&map,cursor))

      map->outports[0].dest = (*cursor)->dest;
      map->outports[0].destp = (*cursor)->destp;
      (*cursor)->dest = map;
      (*cursor)->destp = 0;

      CHECK_CALL(df_build_expr(state,fun,sn->select,cursor))
      *cursor = &map->outports[0];
      break;
    }
    case XSLT_INSTR_FOR_EACH_GROUP:
      break;
    case XSLT_INSTR_IF:
      break;
    case XSLT_INSTR_MESSAGE:
      break;
    case XSLT_INSTR_NAMESPACE:
      break;
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
      CHECK_CALL(df_build_expr(state,fun,sn->select,cursor))
      break;
    case XSLT_INSTR_TEXT:
      break;
    case XSLT_INSTR_VALUE_OF:
      break;
    default:
      assert(0);
      break;
    }
  return 0;
}

static int df_build_sequence(df_state *state, df_function *fun, xl_snode *parent,
                             df_outport **cursor)
{
  xl_snode *sn;
  int havecontent = 0;

  for (sn = parent->child; sn; sn = sn->next) {

    if (XSLT_VARIABLE == sn->type) {
      df_instruction *swallow = df_add_instruction(state,fun,OP_SPECIAL_SWALLOW);
      df_instruction *pass = df_add_instruction(state,fun,OP_SPECIAL_PASS);
      df_instruction *dup = df_add_instruction(state,fun,OP_SPECIAL_DUP);
      df_outport *varcur = &dup->outports[1];
/*       printf("variable declaration: %s\n",sn->qn.localpart); */
      set_and_move_cursor(cursor,dup,0,dup,0);
      varcur->dest = pass;
      varcur->destp = 0;
      pass->outports[0].dest = swallow;
      pass->outports[0].destp = 0;
      if (NULL != sn->select) {
        CHECK_CALL(df_build_expr(state,fun,sn->select,&varcur))
      }
      else {
        df_instruction *docinstr = df_add_instruction(state,fun,OP_SPECIAL_DOCUMENT);
        docinstr->outports[0].dest = pass;
        docinstr->outports[0].destp = 0;
        varcur->dest = docinstr;
        varcur->destp = 0;
        CHECK_CALL(df_build_sequence(state,fun,sn,&varcur))
      }
      sn->outp = &pass->outports[0];
    }
    else if (sn->next) {
      df_instruction *dup = df_add_instruction(state,fun,OP_SPECIAL_DUP);
      df_instruction *seq = df_add_instruction(state,fun,OP_SPECIAL_SEQUENCE);

      seq->outports[0].dest = (*cursor)->dest;
      seq->outports[0].destp = (*cursor)->destp;

      set_and_move_cursor(cursor,dup,0,dup,0);
      (*cursor)->dest = seq;
      (*cursor)->destp = 0;

      CHECK_CALL(df_build_sequence_item(state,fun,sn,cursor))

      *cursor = &dup->outports[1];
      (*cursor)->dest = seq;
      (*cursor)->destp = 1;

      havecontent = 1;
    }
    else {
      CHECK_CALL(df_build_sequence_item(state,fun,sn,cursor))

      havecontent = 1;
    }
  }

  if (!havecontent) {
    df_instruction *empty = df_add_instruction(state,fun,OP_SPECIAL_EMPTY);
    set_and_move_cursor(cursor,empty,0,empty,0);
  }

  return 0;
}

static int df_build_funcontents(df_state *state, df_function *fun, xl_snode *parent,
                                df_outport **cursor)
{
  return df_build_sequence(state,fun,parent,cursor);
}

static xl_snode *df_next_decl(xl_snode *sn)
{
  if ((NULL != sn->child) &&
      (XSLT_DECL_FUNCTION != sn->type) &&
      (XSLT_DECL_TEMPLATE != sn->type))
    return sn->child;

  while ((NULL != sn) && (NULL == sn->next))
    sn = sn->parent;

  if (NULL != sn)
    sn = sn->next;

  return sn;
}

static int df_build_function(df_state *state, xl_snode *sn, df_function *fun)
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

  init_param_instructions(state,fun,seqtypes,outports);

  for (p = sn->param, paramno = 0; p; p = p->next, paramno++)
    p->outp = outports[paramno];

  free(seqtypes);
  free(outports);

  /* Compute return type */
  fun->rtype = sn->seqtype ? df_seqtype_ref(sn->seqtype) : NULL;
  if (NULL == fun->rtype) {
    fun->rtype = df_seqtype_new_item(ITEM_ATOMIC);
    fun->rtype->item->type = state->schema->globals->complex_ur_type;
    assert(NULL != fun->rtype->item->type);
  }

  /* Add wrapper instructions */
  fun->ret = df_add_instruction(state,fun,OP_SPECIAL_RETURN);
  fun->start = df_add_instruction(state,fun,OP_SPECIAL_PASS);
  fun->start->inports[0].seqtype = df_seqtype_new_item(ITEM_ATOMIC);
  fun->start->inports[0].seqtype->item->type = state->schema->globals->int_type;

  cursor = &fun->start->outports[0];
  cursor->dest = fun->ret;
  cursor->destp = 0;

  CHECK_CALL(df_build_funcontents(state,fun,sn,&cursor))
  assert(NULL != cursor);

  return 0;
}

static int df_build_apply_function(df_state *state, xl_snode *root,
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

  sprintf(name,"anon%dfun",state->nextanonid++);
  innerfun = df_new_function(state,nsname_temp(ANON_TEMPLATE_NAMESPACE,name));

  df_init_function(state,innerfun);

  innerfun->isapply = 1;
  innerfun->mode = mode ? strdup(mode) : NULL;


  cursor = &innerfun->start->outports[0];

  for (l = templates; l; l = l->next) {
    template *t = (template*)l->data;
    df_instruction *call = df_add_instruction(state,innerfun,OP_SPECIAL_CALL);

    df_instruction *dup = df_add_instruction(state,innerfun,OP_SPECIAL_DUP);
    df_instruction *contains = df_add_instruction(state,innerfun,OP_SPECIAL_CONTAINS_NODE);
    df_instruction *root = df_add_instruction(state,innerfun,FN_ROOT2);
    df_instruction *select = df_add_instruction(state,innerfun,OP_SPECIAL_SELECT);
    select->axis = AXIS_DESCENDANT_OR_SELF;
    select->seqtypetest = df_normalize_itemnode(0);

    df_build_conditional(state,innerfun,branchcur,&condcur,&truecur,&falsecur);

    /* FIXME: transform expression as described in XSLT 2.0 section 5.5.3 */

    set_and_move_cursor(&condcur,dup,0,dup,0);
    set_and_move_cursor(&condcur,root,0,root,0);
    set_and_move_cursor(&condcur,select,0,select,0);
    CHECK_CALL(df_build_expr(state,innerfun,t->pattern,&condcur))

    set_and_move_cursor(&condcur,contains,0,contains,0);

    dup->outports[1].dest = contains;
    dup->outports[1].destp = 1;

    set_and_move_cursor(&truecur,call,0,call,0);

    call->cfun = t->fun;

    branchcur = &falsecur;
  }

  /* default template for document and element nodes */
  {
    df_instruction *select = df_add_instruction(state,innerfun,OP_SPECIAL_SELECT);
    df_instruction *isempty = df_add_instruction(state,innerfun,FN_EMPTY);
    df_instruction *not = df_add_instruction(state,innerfun,FN_NOT);
    df_instruction *childsel = df_add_instruction(state,innerfun,OP_SPECIAL_SELECT);
    df_seqtype *doctype = df_seqtype_new_item(ITEM_DOCUMENT);
    df_seqtype *elemtype = df_seqtype_new_item(ITEM_ELEMENT);

    df_build_conditional(state,innerfun,branchcur,&condcur,&truecur,&falsecur);

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
    CHECK_CALL(df_build_apply_templates(state,innerfun,root,&truecur,templates,mode))

    branchcur = &falsecur;
  }

  /* default template for text and attribute nodes */
  {
    df_instruction *select = df_add_instruction(state,innerfun,OP_SPECIAL_SELECT);
    df_instruction *isempty = df_add_instruction(state,innerfun,FN_EMPTY);
    df_instruction *not = df_add_instruction(state,innerfun,FN_NOT);
    df_instruction *string = df_add_instruction(state,innerfun,FN_STRING2);
    df_seqtype *texttype = df_seqtype_new_item(ITEM_TEXT);
    df_seqtype *attrtype = df_seqtype_new_item(ITEM_ATTRIBUTE);

    df_build_conditional(state,innerfun,branchcur,&condcur,&truecur,&falsecur);

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

  empty = df_add_instruction(state,innerfun,OP_SPECIAL_EMPTY);
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
static int df_build_apply_templates(df_state *state, df_function *fun,
                                    xl_snode *root, df_outport **cursor,
                                    list *templates, const char *mode)
{
  list *l;
  df_function *applyfun = NULL;
  df_instruction *map = NULL;

  /* Do we already have an anonymous function for this mode? */
  for (l = state->functions; l; l = l->next) {
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
    CHECK_CALL(df_build_apply_function(state,root,templates,&applyfun,mode))

  /* Add the map instruction */
  map = df_add_instruction(state,fun,OP_SPECIAL_MAP);
  map->cfun = applyfun;
  set_and_move_cursor(cursor,map,0,map,0);

  return 0;
}

static list *df_build_ordered_template_list(df_state *state, xl_snode *root)
{
  xl_snode *sn;
  list *templates = NULL;

  for (sn = root->child; sn; sn = df_next_decl(sn)) {
    if (XSLT_DECL_TEMPLATE == sn->type) {
      sn->tmpl = (template*)calloc(1,sizeof(template));
      sn->tmpl->pattern = sn->select;
      list_push(&templates,sn->tmpl);
    }
  }

  return templates;
}

static int df_build_default_template(df_state *state, xl_snode *root, list *templates)
{
  df_instruction *pass;
  df_instruction *ret;
  df_outport *atcursor;
  df_function *fun;

  fun = df_new_function(state,nsname_temp(NULL,"default"));
  pass = df_add_instruction(state,fun,OP_SPECIAL_PASS);
  ret = df_add_instruction(state,fun,OP_SPECIAL_RETURN);

  pass->outports[0].dest = ret;
  pass->outports[0].destp = 0;
  fun->rtype = df_seqtype_new_item(ITEM_ATOMIC);
  fun->rtype->item->type = state->schema->globals->any_atomic_type;
  pass->inports[0].seqtype = df_seqtype_new_item(ITEM_ATOMIC);
  pass->inports[0].seqtype->item->type = state->schema->globals->any_atomic_type;

  atcursor = &pass->outports[0];
  CHECK_CALL(df_build_apply_templates(state,fun,root,&atcursor,templates,NULL))

  fun->start = pass;

  state->deftmpl = fun;

  return 0;
}

static int df_program_from_xl2(df_state *state, xl_snode *root, list *templates)
{
  xl_snode *sn;
  list *l;
  int templatecount = 0;

  /* create empty functions first, so that they can be referenced */

  for (sn = root->child; sn; sn = df_next_decl(sn)) {
    if (XSLT_DECL_FUNCTION == sn->type) {
      if (NULL != df_lookup_function(state,sn->ident)) {
        fprintf(stderr,"Function \"%s\" already defined\n",sn->qn.localpart);
        return -1;
      }
      df_new_function(state,sn->ident);
    }
    else if (XSLT_DECL_TEMPLATE == sn->type) {
      sn->ident.name = (char*)malloc(100);
      sprintf(sn->ident.name,"template%d",templatecount++);
      sn->ident.ns = strdup(ANON_TEMPLATE_NAMESPACE);
      assert(NULL != sn->tmpl);
      sn->tmpl->fun = df_new_function(state,sn->ident);
    }
  }

  CHECK_CALL(df_build_default_template(state,root,templates))

  /* now process the contents of the functions etc */

  for (sn = root->child; sn; sn = df_next_decl(sn)) {
    switch (sn->type) {
    case XSLT_DECL_ATTRIBUTE_SET:
      break;
    case XSLT_DECL_CHARACTER_MAP:
      break;
    case XSLT_DECL_DECIMAL_FORMAT:
      break;
    case XSLT_DECL_FUNCTION: {
      df_function *fun = df_lookup_function(state,sn->ident);
      CHECK_CALL(df_build_function(state,sn,fun))
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
      CHECK_CALL(df_build_function(state,sn,sn->tmpl->fun))
      break;
    default:
      assert(0);
      break;
    }
  }

  for (l = state->functions; l; l = l->next) {
    df_function *fun = (df_function*)l->data;
/*     stringbuf *buf; */
    if (fun == state->init)
      continue;

    if (0 != df_check_function_connected(fun))
      return -1;
    df_remove_redundant(state,fun);

    if (0 != df_compute_types(fun))
      return -1;


/*     buf = stringbuf_new(); */
/*     df_seqtype_print_fs(buf,fun->ret->outports[0].seqtype,state->schema->globals->namespaces); */
/*     stringbuf_free(buf); */
  }

  return 0;
}

int df_program_from_xl(df_state *state, xl_snode *root)
{
  int r;
  root->templates = df_build_ordered_template_list(state,root);
  r = df_program_from_xl2(state,root,root->templates);
  list_free(root->templates,(void*)template_free);
  root->templates = NULL;
  return r;
}
