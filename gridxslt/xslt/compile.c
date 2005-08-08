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

int df_build_expr(df_state *s, df_function *fun, xp_expr *e,
                  df_outport **cursor);
int df_build_sequence(df_state *state, df_function *fun, xl_snode *parent,
                       df_outport **cursor);
int df_build_function(df_state *state, df_function *fun, xl_snode *parent, df_outport **cursor);

static void set_and_move_cursor(df_outport **cursor, df_instruction *destinstr, int destp,
                                df_instruction *newpos, int newdestno)
{
  newpos->outports[newdestno].dest = (*cursor)->dest;
  newpos->outports[newdestno].destp = (*cursor)->destp;
  (*cursor)->dest = destinstr;
  (*cursor)->destp = destp;
  *cursor = &newpos->outports[newdestno];
}

int df_build_binary_op(df_state *state, df_function *fun, xp_expr *e,
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

void df_use_var(df_state *state, df_function *fun, df_outport *outport,
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

static df_outport *resolve_var_to_outport_local(df_function *fun, xp_expr *varref)
{
  xp_expr *defexpr = NULL;
  xl_snode *defnode = NULL;
  xp_expr_resolve_var(varref,varref->qname,&defexpr,&defnode);
  if (NULL != defexpr)
    return defexpr->outp;
  else if (NULL != defnode)
    return defnode->outp;
  else
    return NULL;
}

static df_outport *resolve_var_to_outport(df_function *fun, xp_expr *varref)
{
  xl_snode *sn;
  int in_for_each = 0;
  df_outport *outport = NULL;
  int i;

  if (NULL == (outport = resolve_var_to_outport_local(fun,varref)))
    return NULL;

  /* Are we inside a for-each statement (i.e. compiling to an anonymous function)? */
  for (sn = varref->stmt->parent; sn; sn = sn->parent) {
    if (XSLT_INSTR_FOR_EACH == sn->type) {
      in_for_each = 1;
      break;
    }
  }

  if (!in_for_each)
    return outport;

  for (i = 0; i < fun->nparams; i++) {
    if (fun->params[i].varsource == outport)
      return fun->params[i].outport;
  }

  fprintf(stderr,"FATAL: no parameter in anonymous function corresponding to variable %s\n",
          varref->qname.localpart);
  assert(!"no parameter in anonymous function corresponding to this variable");
  return NULL;
}

void df_build_string(df_state *state, df_function *fun, char *str, df_outport **cursor)
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

int df_build_expr(df_state *state, df_function *fun, xp_expr *e, df_outport **cursor)
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

    CHECK_CALL(df_build_expr(state,fun,e->right,&false_cursor))
    CHECK_CALL(df_build_expr(state,fun,e->left,&true_cursor))
    CHECK_CALL(df_build_expr(state,fun,e->conditional,&cond_cursor))

    false_cursor->dest = merge;
    false_cursor->destp = 0;
    true_cursor->dest = merge;
    true_cursor->destp = 0;

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
      fprintf(stderr,"No such variable \"%s\"\n",e->qname.localpart);
      return -1;
    }

    df_use_var(state,fun,varoutport,cursor,need_gate);

    break;
  }
  case XPATH_EXPR_EMPTY:
    break;
  case XPATH_EXPR_CONTEXT_ITEM:
    debug("*** context item reference ***");
    /* just gets passed through */
    break;
  case XPATH_EXPR_NODE_TEST: {
    df_instruction *select = df_add_instruction(state,fun,OP_SPECIAL_SELECT);
    select->axis = e->axis;

    if (XPATH_NODE_TEST_NAME == e->nodetest) {
      debug("name test: %s",e->qname.localpart);
      select->nametest = strdup(e->qname.localpart);
    }
    else if (XPATH_NODE_TEST_SEQTYPE == e->nodetest) {
      debug("sequence type test");
      select->seqtypetest = df_seqtype_ref(e->seqtype);
    }
    else {
      /* FIXME: support other types (should there even by others?) */
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
    if ((NULL != e->qname.prefix) && !strcmp(e->qname.prefix,"fn")) {
      int opid = df_get_op_id(e->qname.localpart);
      if (0 > opid) {
        fprintf(stderr,"No such function \"%s\"\n",e->qname.localpart);
        return -1;
      }
      call = df_add_instruction(state,fun,opid);
      nparams = df_opdefs[opid].nparams;
      name = df_opdefs[opid].name;
    }
    else {
      call = df_add_instruction(state,fun,OP_SPECIAL_CALL);
      if (NULL == (call->cfun = df_lookup_function(state,e->qname.localpart))) {
        fprintf(stderr,"No such function \"%s\"\n",e->qname.localpart);
        return -1;
      }

      nparams = call->cfun->nparams;
      name = call->cfun->name;
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

static int find_referenced_vars_expr(df_state *state, df_function *fun, xp_expr *e, list **vars)
{
  if (XPATH_EXPR_VAR_REF == e->type) {
    df_outport *outport;

    if (NULL == (outport = resolve_var_to_outport_local(fun,e))) {
      fprintf(stderr,"No such variable \"%s\"\n",e->qname.localpart);
      return -1;
    }

    if (!list_contains_ptr(*vars,outport)) {
      debug("inner function: var reference %s",e->qname.localpart);
      list_push(vars,outport);
    }
  }

  if (NULL != e->conditional)
    CHECK_CALL(find_referenced_vars_expr(state,fun,e->conditional,vars))
  if (NULL != e->left)
    CHECK_CALL(find_referenced_vars_expr(state,fun,e->left,vars))
  if (NULL != e->right)
    CHECK_CALL(find_referenced_vars_expr(state,fun,e->right,vars))

  return 0;
}

static int find_referenced_vars(df_state *state, df_function *fun, xl_snode *parent, list **vars)
{
  xl_snode *sn;

  for (sn = parent->child; sn; sn = sn->next) {

    if (NULL != sn->select)
      CHECK_CALL(find_referenced_vars_expr(state,fun,sn->select,vars))
    if (NULL != sn->expr1)
      CHECK_CALL(find_referenced_vars_expr(state,fun,sn->expr1,vars))
    if (NULL != sn->expr2)
      CHECK_CALL(find_referenced_vars_expr(state,fun,sn->expr2,vars))
    if (NULL != sn->name_expr)
      CHECK_CALL(find_referenced_vars_expr(state,fun,sn->name_expr,vars))

    if (NULL != sn->child)
      CHECK_CALL(find_referenced_vars(state,fun,sn,vars))


    /* FIXME: param and sort (?) */

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

int df_build_sequence_item(df_state *state, df_function *fun, xl_snode *sn, df_outport **cursor)
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
    case XSLT_INSTR_APPLY_TEMPLATES:
      break;
    case XSLT_INSTR_ATTRIBUTE: {
      df_instruction *dup = df_add_instruction(state,fun,OP_SPECIAL_DUP);
      df_instruction *attr = df_add_instruction(state,fun,OP_SPECIAL_ATTRIBUTE);
      df_outport *namecur = &dup->outports[0];
      df_outport *childcur = &dup->outports[1];
      if (NULL != sn->name_expr) {
        assert(!"attribute name expressions not yet implemented");
      }
      assert(NULL != sn->qname.localpart);

      namecur->dest = attr;
      namecur->destp = 1;
      df_build_string(state,fun,sn->qname.localpart,&namecur);

      childcur->dest = attr;
      childcur->destp = 0;

      if (NULL != sn->select) {
        debug("attribute %s: content is xpath expression",sn->qname.localpart);
        CHECK_CALL(df_build_expr(state,fun,sn->select,&childcur))
      }
      else {
        debug("attribute %s: content is sequence constructor",sn->qname.localpart);
        CHECK_CALL(df_build_sequence(state,fun,sn,&childcur))
      }

      set_and_move_cursor(cursor,dup,0,attr,0);
      break;
    }
    case XSLT_INSTR_CALL_TEMPLATE:
      break;
    case XSLT_INSTR_CHOOSE: {
      xl_snode *c;

      /* FIXME: when no otherwise, just have a nil in its place */
      /* FIXME: complete this! I don't think it works for multiple <when> occurrences yet */
      for (c = sn->child; c; c = c->next) {

        if (NULL != c->select) {
          /* when */

          df_instruction *split = df_add_instruction(state,fun,OP_SPECIAL_SPLIT);
          df_instruction *merge = df_add_instruction(state,fun,OP_SPECIAL_MERGE);
          df_instruction *dup = df_add_instruction(state,fun,OP_SPECIAL_DUP);

          df_outport *condcur = &dup->outports[0];
          df_outport *tbcur = &split->outports[0];
/*           df_outport *fbcur = &split->outports[1]; */


          dup->outports[0].dest = split;
          dup->outports[0].destp = 0;
          dup->outports[1].dest = split;
          dup->outports[1].destp = 1;

          split->outports[0].dest = merge;
          split->outports[0].destp = 0;
          split->outports[1].dest = merge;
          split->outports[1].destp = 1;

          CHECK_CALL(df_build_expr(state,fun,c->select,&condcur))

          CHECK_CALL(df_build_sequence(state,fun,c,&tbcur))

          set_and_move_cursor(cursor,dup,0,merge,0);
          debug("compiled when");
        }
        else {
          /* otherwise */
          debug("compiled otherwise");
        }
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
      assert(NULL != sn->qname.localpart);

      namecur->dest = elem;
      namecur->destp = 1;
      df_build_string(state,fun,sn->qname.localpart,&namecur);

      childcur->dest = elem;
      childcur->destp = 0;
      CHECK_CALL(df_build_sequence(state,fun,sn,&childcur))

      set_and_move_cursor(cursor,dup,0,elem,0);

      break;
    }
    case XSLT_INSTR_FALLBACK:
      break;
    case XSLT_INSTR_FOR_EACH: {
      char name[100];
      df_function *innerfun;
      df_instruction *map = df_add_instruction(state,fun,OP_SPECIAL_MAP);
      int need_gate = stmt_in_conditional(sn);
      list *vars = NULL;
      list *l;
      int varno;
      df_outport *innerfuncur;

      debug("for-each within function %s - BEGIN",fun->name);

      sprintf(name,"_anon%d",state->nextanonid++);
      innerfun = df_new_function(state,name);

      /* FIXME: set correct return type */
      innerfun->rtype = df_seqtype_new_item(ITEM_ATOMIC);
      innerfun->rtype->item->type = state->schema->globals->complex_ur_type;
      

      debug("before find_referenced_vars");
      CHECK_CALL(find_referenced_vars(state,fun,sn,&vars))
      debug("after find_referenced_vars");
      innerfun->nparams = list_count(vars);
      innerfun->params = (df_parameter*)calloc(innerfun->nparams,sizeof(df_parameter));

      map->cfun = innerfun;

      df_free_instruction_inports(map);
      map->ninports = innerfun->nparams+1;
      map->inports = (df_inport*)calloc(map->ninports,sizeof(df_inport));

      varno = 0;
      for (l = vars; l; l = l->next) {
        df_outport *varoutport = (df_outport*)l->data;
        df_instruction *dup = df_add_instruction(state,fun,OP_SPECIAL_DUP);
        df_outport *varcur = &dup->outports[1];

        assert(NULL != varoutport);
        innerfun->params[varno].varsource = varoutport;

        set_and_move_cursor(cursor,dup,0,dup,0);
        varcur->dest = map;
        varcur->destp = varno+1;
        varno++;
        df_use_var(state,fun,varoutport,&varcur,need_gate);
      }

      map->outports[0].dest = (*cursor)->dest;
      map->outports[0].destp = (*cursor)->destp;
      (*cursor)->dest = map;
      (*cursor)->destp = 0;

      debug("for-each within function %s - building inner anon function %s",fun->name,
             innerfun->name);
/*       CHECK_CALL(df_build_expr(state,innerfun,sn->select,cursor)) */
      /* FIXME!!!!!!!!!!!: this should be innerfun: */
      CHECK_CALL(df_build_expr(state,fun,sn->select,cursor))

      *cursor = &map->outports[0];


      /* FIXME: pass types for each parameter */
      init_param_instructions(state,innerfun,NULL,NULL);

      if (NULL == fun->mapseq) {
        fun->mapseq = df_add_instruction(state,fun,OP_SPECIAL_SEQUENCE);
        fun->mapseq->internal = 1;
        /* FIXME: use appropriate types here */
        fun->mapseq->inports[0].seqtype = df_seqtype_new_item(ITEM_ATOMIC);
        fun->mapseq->inports[0].seqtype->item->type = state->schema->globals->complex_ur_type;
        fun->mapseq->inports[1].seqtype = df_seqtype_new_item(ITEM_ATOMIC);
        fun->mapseq->inports[1].seqtype->item->type = state->schema->globals->complex_ur_type;
        fun->mapseq->outports[0].seqtype = df_seqtype_new(SEQTYPE_SEQUENCE);
        fun->mapseq->outports[0].seqtype->left = df_seqtype_ref(fun->mapseq->inports[0].seqtype);
        fun->mapseq->outports[0].seqtype->right = df_seqtype_ref(fun->mapseq->inports[1].seqtype);
      }

      innerfun->ret = df_add_instruction(state,innerfun,OP_SPECIAL_RETURN);
      innerfun->start = df_add_instruction(state,innerfun,OP_SPECIAL_PASS);

      /* FIXME: set correct type based on cursor's type */
      innerfun->start->inports[0].seqtype = df_seqtype_new_item(ITEM_ATOMIC);
      innerfun->start->inports[0].seqtype->item->type = state->schema->globals->int_type;
      
      innerfuncur = &innerfun->start->outports[0];
      innerfuncur->dest = innerfun->ret;
      innerfuncur->destp = 0;

      debug("for-each: before building sequence constructor");
      CHECK_CALL(df_build_function(state,innerfun,sn,&innerfuncur))
      debug("for-each: after building sequence constructor");
      assert(NULL != innerfuncur);

      list_free(vars,NULL);
      debug("for-each within function %s - END",fun->name);

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


int df_build_sequence(df_state *state, df_function *fun, xl_snode *parent,
                       df_outport **cursor)
{
  xl_snode *sn;

  for (sn = parent->child; sn; sn = sn->next) {

    if (XSLT_VARIABLE == sn->type) {
      df_instruction *swallow = df_add_instruction(state,fun,OP_SPECIAL_SWALLOW);
      df_instruction *pass = df_add_instruction(state,fun,OP_SPECIAL_PASS);
      df_instruction *dup = df_add_instruction(state,fun,OP_SPECIAL_DUP);
      df_outport *varcur = &dup->outports[1];
/*       printf("variable declaration: %s\n",sn->qname.localpart); */
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
    }
    else {
      CHECK_CALL(df_build_sequence_item(state,fun,sn,cursor))
    }
  }

  return 0;
}


int df_build_function(df_state *state, df_function *fun, xl_snode *parent, df_outport **cursor)
{
  return df_build_sequence(state,fun,parent,cursor);
}

int df_program_from_xl(df_state *state, xl_snode *root)
{
  xl_snode *sn;
  list *l;

  /* create empty functions first, so that they can be referenced */

  for (sn = root->child; sn; sn = sn->next) {
    if (XSLT_DECL_FUNCTION == sn->type) {
      if (NULL != df_lookup_function(state,sn->qname.localpart)) {
        fprintf(stderr,"Function \"%s\" already defined\n",sn->qname.localpart);
        return -1;
      }
      df_new_function(state,sn->qname.localpart);
    }
  }

  /* now process the contents of the functions etc */

  for (sn = root->child; sn; sn = sn->next) {
    switch (sn->type) {
    case XSLT_DECL_ATTRIBUTE_SET:
      break;
    case XSLT_DECL_CHARACTER_MAP:
      break;
    case XSLT_DECL_DECIMAL_FORMAT:
      break;
    case XSLT_DECL_FUNCTION: {
      df_function *fun = df_lookup_function(state,sn->qname.localpart);
      df_outport *cursor;
      df_seqtype **seqtypes;
      df_outport **outports;
      xl_snode *p;
      int paramno;

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

      fun->rtype = sn->seqtype ? df_seqtype_ref(sn->seqtype) : NULL;
      if (NULL == fun->rtype) {
        fun->rtype = df_seqtype_new_item(ITEM_ATOMIC);
        fun->rtype->item->type = state->schema->globals->complex_ur_type;
        assert(NULL != fun->rtype->item->type);
      }

      for (p = sn->param, paramno = 0; p; p = p->next, paramno++)
        p->outp = outports[paramno];

      free(seqtypes);
      free(outports);


      fun->ret = df_add_instruction(state,fun,OP_SPECIAL_RETURN);
      fun->start = df_add_instruction(state,fun,OP_SPECIAL_PASS);
      fun->start->inports[0].seqtype = df_seqtype_new_item(ITEM_ATOMIC);
      fun->start->inports[0].seqtype->item->type = state->schema->globals->int_type;

      cursor = &fun->start->outports[0];
      cursor->dest = fun->ret;
      cursor->destp = 0;

      CHECK_CALL(df_build_function(state,fun,sn,&cursor))
      assert(NULL != cursor);

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
