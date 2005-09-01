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

#include "functions.h"
#include "dataflow.h"
#include "funops.h"
#include "serialization.h"
#include "util/macros.h"
#include "util/namespace.h"
#include "util/debug.h"
#include <assert.h>
#include <string.h>
#include <errno.h>
#include <ctype.h>
#include <libxml/parser.h>
#include <libxml/tree.h>

static int df_check_is_atomic_type(df_seqtype *st, xs_type *type)
{
  return ((SEQTYPE_ITEM == st->type) &&
          (ITEM_ATOMIC == st->item->kind) &&
          (st->item->type == type));
}

int df_execute_op(df_state *state, df_instruction *instr, int opcode,
                  df_value **values, df_value **result)
{
  xs_globals *g = state->program->schema->globals;

  switch (opcode) {

  case FN_NODE_NAME: assert(!"not yet implemented"); break;
  case FN_NILLED: assert(!"not yet implemented"); break;
  case FN_STRING: assert(!"not yet implemented"); break;
  case FN_STRING2: {
    char *str;
    if (SEQTYPE_EMPTY == values[0]->seqtype->type) {
      *result = df_value_new_string(g,"");
    }
    else {
      assert(SEQTYPE_ITEM == values[0]->seqtype->type);
      str = df_value_as_string(g,values[0]);
      *result = df_value_new_string(g,str);
      free(str);
    }
    break;
  }
  case FN_DATA: assert(!"not yet implemented"); break;
  case FN_BASE_URI: assert(!"not yet implemented"); break;
  case FN_BASE_URI2: assert(!"not yet implemented"); break;
  case FN_DOCUMENT_URI: assert(!"not yet implemented"); break;
  case FN_ERROR: assert(!"not yet implemented"); break;
  case FN_ERROR2: assert(!"not yet implemented"); break;
  case FN_ERROR3: assert(!"not yet implemented"); break;
  case FN_ERROR4: assert(!"not yet implemented"); break;
  case FN_TRACE: assert(!"not yet implemented"); break;
  case FN_DATETIME: assert(!"not yet implemented"); break;
  case OP_DECIMAL_ADD: assert(!"not yet implemented"); break;
  case OP_DECIMAL_SUBTRACT: assert(!"not yet implemented"); break;
  case OP_DECIMAL_MULTIPLY: assert(!"not yet implemented"); break;
  case OP_DECIMAL_DIVIDE: assert(!"not yet implemented"); break;
  case OP_DECIMAL_INTEGER_DIVIDE: assert(!"not yet implemented"); break;
  case OP_DECIMAL_MOD: assert(!"not yet implemented"); break;
  case OP_DECIMAL_UNARY_PLUS: assert(!"not yet implemented"); break;
  case OP_DECIMAL_UNARY_MINUS: assert(!"not yet implemented"); break;
  case OP_DECIMAL_EQUAL: assert(!"not yet implemented"); break;
  case OP_DECIMAL_LESS_THAN: assert(!"not yet implemented"); break;
  case OP_DECIMAL_GREATER_THAN: assert(!"not yet implemented"); break;
  case FN_DECIMAL_ABS: assert(!"not yet implemented"); break;
  case FN_DECIMAL_CEILING: assert(!"not yet implemented"); break;
  case FN_DECIMAL_FLOOR: assert(!"not yet implemented"); break;
  case FN_DECIMAL_ROUND: assert(!"not yet implemented"); break;
  case FN_DECIMAL_ROUND_HALF_TO_EVEN: assert(!"not yet implemented"); break;
  case FN_DECIMAL_ROUND_HALF_TO_EVEN2: assert(!"not yet implemented"); break;
  case OP_INTEGER_ADD: assert(!"not yet implemented"); break;
  case OP_INTEGER_SUBTRACT: assert(!"not yet implemented"); break;
  case OP_INTEGER_MULTIPLY: assert(!"not yet implemented"); break;
  case OP_INTEGER_DIVIDE: assert(!"not yet implemented"); break;
  case OP_INTEGER_INTEGER_DIVIDE: assert(!"not yet implemented"); break;
  case OP_INTEGER_MOD: assert(!"not yet implemented"); break;
  case OP_INTEGER_UNARY_PLUS: assert(!"not yet implemented"); break;
  case OP_INTEGER_UNARY_MINUS: assert(!"not yet implemented"); break;
  case OP_INTEGER_EQUAL: assert(!"not yet implemented"); break;
  case OP_INTEGER_LESS_THAN: assert(!"not yet implemented"); break;
  case OP_INTEGER_GREATER_THAN: assert(!"not yet implemented"); break;
  case FN_INTEGER_ABS: assert(!"not yet implemented"); break;
  case FN_INTEGER_CEILING: assert(!"not yet implemented"); break;
  case FN_INTEGER_FLOOR: assert(!"not yet implemented"); break;
  case FN_INTEGER_ROUND: assert(!"not yet implemented"); break;
  case FN_INTEGER_ROUND_HALF_TO_EVEN: assert(!"not yet implemented"); break;
  case FN_INTEGER_ROUND_HALF_TO_EVEN2: assert(!"not yet implemented"); break;
  case OP_FLOAT_ADD: assert(!"not yet implemented"); break;
  case OP_FLOAT_SUBTRACT: assert(!"not yet implemented"); break;
  case OP_FLOAT_MULTIPLY: assert(!"not yet implemented"); break;
  case OP_FLOAT_DIVIDE: assert(!"not yet implemented"); break;
  case OP_FLOAT_INTEGER_DIVIDE: assert(!"not yet implemented"); break;
  case OP_FLOAT_MOD: assert(!"not yet implemented"); break;
  case OP_FLOAT_UNARY_PLUS: assert(!"not yet implemented"); break;
  case OP_FLOAT_UNARY_MINUS: assert(!"not yet implemented"); break;
  case OP_FLOAT_EQUAL: assert(!"not yet implemented"); break;
  case OP_FLOAT_LESS_THAN: assert(!"not yet implemented"); break;
  case OP_FLOAT_GREATER_THAN: assert(!"not yet implemented"); break;
  case FN_FLOAT_ABS: assert(!"not yet implemented"); break;
  case FN_FLOAT_CEILING: assert(!"not yet implemented"); break;
  case FN_FLOAT_FLOOR: assert(!"not yet implemented"); break;
  case FN_FLOAT_ROUND: assert(!"not yet implemented"); break;
  case FN_FLOAT_ROUND_HALF_TO_EVEN: assert(!"not yet implemented"); break;
  case FN_FLOAT_ROUND_HALF_TO_EVEN2: assert(!"not yet implemented"); break;
  case OP_DOUBLE_ADD: assert(!"not yet implemented"); break;
  case OP_DOUBLE_SUBTRACT: assert(!"not yet implemented"); break;
  case OP_DOUBLE_MULTIPLY: assert(!"not yet implemented"); break;
  case OP_DOUBLE_DIVIDE: assert(!"not yet implemented"); break;
  case OP_DOUBLE_INTEGER_DIVIDE: assert(!"not yet implemented"); break;
  case OP_DOUBLE_MOD: assert(!"not yet implemented"); break;
  case OP_DOUBLE_UNARY_PLUS: assert(!"not yet implemented"); break;
  case OP_DOUBLE_UNARY_MINUS: assert(!"not yet implemented"); break;
  case OP_DOUBLE_EQUAL: assert(!"not yet implemented"); break;
  case OP_DOUBLE_LESS_THAN: assert(!"not yet implemented"); break;
  case OP_DOUBLE_GREATER_THAN: assert(!"not yet implemented"); break;
  case FN_DOUBLE_ABS: assert(!"not yet implemented"); break;
  case FN_DOUBLE_CEILING: assert(!"not yet implemented"); break;
  case FN_DOUBLE_FLOOR: assert(!"not yet implemented"); break;
  case FN_DOUBLE_ROUND: assert(!"not yet implemented"); break;
  case FN_DOUBLE_ROUND_HALF_TO_EVEN: assert(!"not yet implemented"); break;
  case FN_DOUBLE_ROUND_HALF_TO_EVEN2: assert(!"not yet implemented"); break;

  case OP_NUMERIC_ADD:
    /* FIXME: type promotion and use appropriate typed operator */
    assert(df_check_derived_atomic_type(values[0],g->int_type));
    assert(df_check_derived_atomic_type(values[1],g->int_type));
    assert(df_check_is_atomic_type(instr->outports[0].seqtype,g->int_type));
    *result = df_value_new(instr->outports[0].seqtype);
    (*result)->value.i = values[0]->value.i + values[1]->value.i;
    break;

  case OP_NUMERIC_SUBTRACT:
    /* FIXME: type promotion and use appropriate typed operator */
    assert(df_check_derived_atomic_type(values[0],g->int_type));
    assert(df_check_derived_atomic_type(values[1],g->int_type));
    assert(df_check_is_atomic_type(instr->outports[0].seqtype,g->int_type));
    *result = df_value_new(instr->outports[0].seqtype);
    (*result)->value.i = values[0]->value.i - values[1]->value.i;
    break;

  case OP_NUMERIC_MULTIPLY:
    /* FIXME: type promotion and use appropriate typed operator */
    assert(df_check_derived_atomic_type(values[0],g->int_type));
    assert(df_check_derived_atomic_type(values[1],g->int_type));
    assert(df_check_is_atomic_type(instr->outports[0].seqtype,g->int_type));
    *result = df_value_new(instr->outports[0].seqtype);
    (*result)->value.i = values[0]->value.i * values[1]->value.i;
    break;

  case OP_NUMERIC_DIVIDE:
    /* FIXME: type promotion and use appropriate typed operator */
    assert(df_check_derived_atomic_type(values[0],g->int_type));
    assert(df_check_derived_atomic_type(values[1],g->int_type));
    assert(df_check_is_atomic_type(instr->outports[0].seqtype,g->int_type));
    *result = df_value_new(instr->outports[0].seqtype);
    (*result)->value.i = values[0]->value.i / values[1]->value.i;
    break;

  case OP_NUMERIC_INTEGER_DIVIDE:
    /* FIXME: type promotion and use appropriate typed operator */
    assert(df_check_derived_atomic_type(values[0],g->int_type));
    assert(df_check_derived_atomic_type(values[1],g->int_type));
    assert(df_check_is_atomic_type(instr->outports[0].seqtype,g->int_type));
    *result = df_value_new(instr->outports[0].seqtype);
    (*result)->value.i = values[0]->value.i / values[1]->value.i;
    break;

  case OP_NUMERIC_MOD: assert(!"not yet implemented"); break;
  case OP_NUMERIC_UNARY_PLUS: assert(!"not yet implemented"); break;
  case OP_NUMERIC_UNARY_MINUS: assert(!"not yet implemented"); break;

  case OP_NUMERIC_EQUAL:
    /* FIXME: type promotion and use appropriate typed operator */
    assert(df_check_is_atomic_type(instr->outports[0].seqtype,g->boolean_type)); 
    assert(df_check_derived_atomic_type(values[0],g->int_type));
    assert(df_check_derived_atomic_type(values[1],g->int_type));
    assert(df_check_is_atomic_type(instr->outports[0].seqtype,g->boolean_type));

    *result = df_value_new(instr->outports[0].seqtype);
    if (values[0]->value.i == values[1]->value.i)
      (*result)->value.b = 1;
    else
      (*result)->value.b = 0;
    break;

  case OP_NUMERIC_LESS_THAN:
    /* FIXME: type promotion and use appropriate typed operator */
    assert(df_check_derived_atomic_type(values[0],g->int_type));
    assert(df_check_derived_atomic_type(values[1],g->int_type));
    assert(df_check_is_atomic_type(instr->outports[0].seqtype,g->boolean_type));

    *result = df_value_new(instr->outports[0].seqtype);
    if (values[0]->value.i < values[1]->value.i)
      (*result)->value.b = 1;
    else
      (*result)->value.b = 0;
    break;

  case OP_NUMERIC_GREATER_THAN:
    /* FIXME: type promotion and use appropriate typed operator */
    assert(df_check_derived_atomic_type(values[0],g->int_type));
    assert(df_check_derived_atomic_type(values[1],g->int_type));
    assert(df_check_is_atomic_type(instr->outports[0].seqtype,g->boolean_type));

    *result = df_value_new(instr->outports[0].seqtype);
    if (values[0]->value.i > values[1]->value.i)
      (*result)->value.b = 1;
    else
      (*result)->value.b = 0;
    break;
  case FN_ABS: assert(!"not yet implemented"); break;
  case FN_CEILING: assert(!"not yet implemented"); break;
  case FN_FLOOR: assert(!"not yet implemented"); break;
  case FN_ROUND: assert(!"not yet implemented"); break;
  case FN_ROUND_HALF_TO_EVEN: assert(!"not yet implemented"); break;
  case FN_ROUND_HALF_TO_EVEN2: assert(!"not yet implemented"); break;
  case FN_CODEPOINTS_TO_STRING: assert(!"not yet implemented"); break;
  case FN_STRING_TO_CODEPOINTS: assert(!"not yet implemented"); break;
  case FN_COMPARE: assert(!"not yet implemented"); break;
  case FN_COMPARE2: assert(!"not yet implemented"); break;
  case FN_CODEPOINT_EQUAL: assert(!"not yet implemented"); break;
  case FN_CONCAT: assert(!"not yet implemented"); break;
  case FN_STRING_JOIN: {
    list *seq = df_sequence_to_list(values[0]);
    stringbuf *buf = stringbuf_new();
    const char *separator;
    list *l;

    assert(df_check_derived_atomic_type(values[1],g->string_type));
    separator = values[1]->value.s;

    for (l = seq; l; l = l->next) {
      df_value *v = (df_value*)l->data;
      assert(df_check_derived_atomic_type(v,g->string_type));
      stringbuf_format(buf,"%s",v->value.s);
      if (l->next)
        stringbuf_format(buf,"%s",separator);
    }

    *result = df_value_new_string(g,buf->data);

    stringbuf_free(buf);
    list_free(seq,NULL);
    break;
  }
  case FN_SUBSTRING: assert(!"not yet implemented"); break;
  case FN_SUBSTRING2: assert(!"not yet implemented"); break;
  case FN_STRING_LENGTH: assert(!"not yet implemented"); break;
  case FN_STRING_LENGTH2: assert(!"not yet implemented"); break;
  case FN_NORMALIZE_SPACE: assert(!"not yet implemented"); break;
  case FN_NORMALIZE_SPACE2: assert(!"not yet implemented"); break;
  case FN_NORMALIZE_UNICODE: assert(!"not yet implemented"); break;
  case FN_NORMALIZE_UNICODE2: assert(!"not yet implemented"); break;
  case FN_UPPER_CASE: assert(!"not yet implemented"); break;
  case FN_LOWER_CASE: assert(!"not yet implemented"); break;
  case FN_TRANSLATE: assert(!"not yet implemented"); break;
  case FN_ESCAPE_URI: assert(!"not yet implemented"); break;
  case FN_CONTAINS: assert(!"not yet implemented"); break;
  case FN_CONTAINS2: assert(!"not yet implemented"); break;
  case FN_STARTS_WITH: assert(!"not yet implemented"); break;
  case FN_STARTS_WITH2: assert(!"not yet implemented"); break;
  case FN_ENDS_WITH: assert(!"not yet implemented"); break;
  case FN_ENDS_WITH2: assert(!"not yet implemented"); break;
  case FN_SUBSTRING_BEFORE: assert(!"not yet implemented"); break;
  case FN_SUBSTRING_BEFORE2: assert(!"not yet implemented"); break;
  case FN_SUBSTRING_AFTER: assert(!"not yet implemented"); break;
  case FN_SUBSTRING_AFTER2: assert(!"not yet implemented"); break;
  case FN_MATCHES: assert(!"not yet implemented"); break;
  case FN_MATCHES2: assert(!"not yet implemented"); break;
  case FN_REPLACE: assert(!"not yet implemented"); break;
  case FN_REPLACE2: assert(!"not yet implemented"); break;
  case FN_TOKENIZE: assert(!"not yet implemented"); break;
  case FN_TOKENIZE2: assert(!"not yet implemented"); break;
  case FN_RESOLVE_URI: assert(!"not yet implemented"); break;
  case FN_RESOLVE_URI2: assert(!"not yet implemented"); break;
  case FN_TRUE: assert(!"not yet implemented"); break;
  case FN_FALSE: assert(!"not yet implemented"); break;
  case OP_BOOLEAN_EQUAL: assert(!"not yet implemented"); break;
  case OP_BOOLEAN_LESS_THAN: assert(!"not yet implemented"); break;
  case OP_BOOLEAN_GREATER_THAN: assert(!"not yet implemented"); break;
  case FN_NOT:
    /* FIXME: we can't assume the input is a boolean! arg def is item()*; we have to reduce it
       to a boolean ourselves */
    *result = df_value_new(instr->outports[0].seqtype);
    (*result)->value.b = !values[0]->value.b;
    break;
  case OP_YEARMONTHDURATION_EQUAL: assert(!"not yet implemented"); break;
  case OP_YEARMONTHDURATION_LESS_THAN: assert(!"not yet implemented"); break;
  case OP_YEARMONTHDURATION_GREATER_THAN: assert(!"not yet implemented"); break;
  case OP_DAYTIMEDURATION_EQUAL: assert(!"not yet implemented"); break;
  case OP_DAYTIMEDURATION_LESS_THAN: assert(!"not yet implemented"); break;
  case OP_DAYTIMEDURATION_GREATER_THAN: assert(!"not yet implemented"); break;
  case OP_DATETIME_EQUAL: assert(!"not yet implemented"); break;
  case OP_DATETIME_LESS_THAN: assert(!"not yet implemented"); break;
  case OP_DATETIME_GREATER_THAN: assert(!"not yet implemented"); break;
  case OP_DATE_EQUAL: assert(!"not yet implemented"); break;
  case OP_DATE_LESS_THAN: assert(!"not yet implemented"); break;
  case OP_DATE_GREATER_THAN: assert(!"not yet implemented"); break;
  case OP_TIME_EQUAL: assert(!"not yet implemented"); break;
  case OP_TIME_LESS_THAN: assert(!"not yet implemented"); break;
  case OP_TIME_GREATER_THAN: assert(!"not yet implemented"); break;
  case OP_GYEARMONTH_EQUAL: assert(!"not yet implemented"); break;
  case OP_GYEAR_EQUAL: assert(!"not yet implemented"); break;
  case OP_GMONTHDAY_EQUAL: assert(!"not yet implemented"); break;
  case OP_GMONTH_EQUAL: assert(!"not yet implemented"); break;
  case OP_GDAY_EQUAL: assert(!"not yet implemented"); break;
  case FN_YEARS_FROM_DURATION: assert(!"not yet implemented"); break;
  case FN_MONTHS_FROM_DURATION: assert(!"not yet implemented"); break;
  case FN_DAYS_FROM_DURATION: assert(!"not yet implemented"); break;
  case FN_HOURS_FROM_DURATION: assert(!"not yet implemented"); break;
  case FN_MINUTES_FROM_DURATION: assert(!"not yet implemented"); break;
  case FN_SECONDS_FROM_DURATION: assert(!"not yet implemented"); break;
  case FN_YEAR_FROM_DATETIME: assert(!"not yet implemented"); break;
  case FN_MONTH_FROM_DATETIME: assert(!"not yet implemented"); break;
  case FN_DAY_FROM_DATETIME: assert(!"not yet implemented"); break;
  case FN_HOURS_FROM_DATETIME: assert(!"not yet implemented"); break;
  case FN_MINUTES_FROM_DATETIME: assert(!"not yet implemented"); break;
  case FN_SECONDS_FROM_DATETIME: assert(!"not yet implemented"); break;
  case FN_TIMEZONE_FROM_DATETIME: assert(!"not yet implemented"); break;
  case FN_YEAR_FROM_DATE: assert(!"not yet implemented"); break;
  case FN_MONTH_FROM_DATE: assert(!"not yet implemented"); break;
  case FN_DAY_FROM_DATE: assert(!"not yet implemented"); break;
  case FN_TIMEZONE_FROM_DATE: assert(!"not yet implemented"); break;
  case FN_HOURS_FROM_TIME: assert(!"not yet implemented"); break;
  case FN_MINUTES_FROM_TIME: assert(!"not yet implemented"); break;
  case FN_SECONDS_FROM_TIME: assert(!"not yet implemented"); break;
  case FN_TIMEZONE_FROM_TIME: assert(!"not yet implemented"); break;
  case OP_ADD_YEARMONTHDURATIONS: assert(!"not yet implemented"); break;
  case OP_SUBTRACT_YEARMONTHDURATIONS: assert(!"not yet implemented"); break;
  case OP_MULTIPLY_YEARMONTHDURATION: assert(!"not yet implemented"); break;
  case OP_DIVIDE_YEARMONTHDURATION: assert(!"not yet implemented"); break;
  case OP_DIVIDE_YEARMONTHDURATION_BY_YEARMONTHDURATION: assert(!"not yet implemented"); break;
  case OP_ADD_DAYTIMEDURATIONS: assert(!"not yet implemented"); break;
  case OP_SUBTRACT_DAYTIMEDURATIONS: assert(!"not yet implemented"); break;
  case OP_MULTIPLY_DAYTIMEDURATION: assert(!"not yet implemented"); break;
  case OP_DIVIDE_DAYTIMEDURATION: assert(!"not yet implemented"); break;
  case OP_DIVIDE_DAYTIMEDURATION_BY_DAYTIMEDURATION: assert(!"not yet implemented"); break;
  case FN_ADJUST_DATETIME_TO_TIMEZONE: assert(!"not yet implemented"); break;
  case FN_ADJUST_DATETIME_TO_TIMEZONE2: assert(!"not yet implemented"); break;
  case FN_ADJUST_DATE_TO_TIMEZONE: assert(!"not yet implemented"); break;
  case FN_ADJUST_DATE_TO_TIMEZONE2: assert(!"not yet implemented"); break;
  case FN_ADJUST_TIME_TO_TIMEZONE: assert(!"not yet implemented"); break;
  case FN_ADJUST_TIME_TO_TIMEZONE2: assert(!"not yet implemented"); break;
  case OP_SUBTRACT_DATETIMES_YIELDING_DAYTIMEDURATION: assert(!"not yet implemented"); break;
  case OP_SUBTRACT_DATES_YIELDING_DAYTIMEDURATION: assert(!"not yet implemented"); break;
  case OP_SUBTRACT_TIMES: assert(!"not yet implemented"); break;
  case OP_ADD_YEARMONTHDURATION_TO_DATETIME: assert(!"not yet implemented"); break;
  case OP_ADD_DAYTIMEDURATION_TO_DATETIME: assert(!"not yet implemented"); break;
  case OP_SUBTRACT_YEARMONTHDURATION_FROM_DATETIME: assert(!"not yet implemented"); break;
  case OP_SUBTRACT_DAYTIMEDURATION_FROM_DATETIME: assert(!"not yet implemented"); break;
  case OP_ADD_YEARMONTHDURATION_TO_DATE: assert(!"not yet implemented"); break;
  case OP_ADD_DAYTIMEDURATION_TO_DATE: assert(!"not yet implemented"); break;
  case OP_SUBTRACT_YEARMONTHDURATION_FROM_DATE: assert(!"not yet implemented"); break;
  case OP_SUBTRACT_DAYTIMEDURATION_FROM_DATE: assert(!"not yet implemented"); break;
  case OP_ADD_DAYTIMEDURATION_TO_TIME: assert(!"not yet implemented"); break;
  case OP_SUBTRACT_DAYTIMEDURATION_FROM_TIME: assert(!"not yet implemented"); break;
  case FN_RESOLVE_QNAME: assert(!"not yet implemented"); break;
  case FN_QNAME: assert(!"not yet implemented"); break;
  case OP_QNAME_EQUAL: assert(!"not yet implemented"); break;
  case FN_PREFIX_FROM_QNAME: assert(!"not yet implemented"); break;
  case FN_LOCAL_NAME_FROM_QNAME: assert(!"not yet implemented"); break;
  case FN_NAMESPACE_URI_FROM_QNAME: assert(!"not yet implemented"); break;
  case FN_NAMESPACE_URI_FOR_PREFIX: assert(!"not yet implemented"); break;
  case FN_IN_SCOPE_PREFIXES: assert(!"not yet implemented"); break;
  case OP_HEXBINARY_EQUAL: assert(!"not yet implemented"); break;
  case OP_BASE64BINARY_EQUAL: assert(!"not yet implemented"); break;
  case OP_NOTATION_EQUAL: assert(!"not yet implemented"); break;
  case FN_NAME: assert(!"not yet implemented"); break;
  case FN_NAME2: assert(!"not yet implemented"); break;
  case FN_LOCAL_NAME: assert(!"not yet implemented"); break;
  case FN_LOCAL_NAME2: assert(!"not yet implemented"); break;
  case FN_NAMESPACE_URI: assert(!"not yet implemented"); break;
  case FN_NAMESPACE_URI2: assert(!"not yet implemented"); break;
  case FN_NUMBER: assert(!"not yet implemented"); break;
  case FN_NUMBER2: assert(!"not yet implemented"); break;
  case FN_LANG: assert(!"not yet implemented"); break;
  case FN_LANG2: assert(!"not yet implemented"); break;
  case OP_IS_SAME_NODE: assert(!"not yet implemented"); break;
  case OP_NODE_BEFORE: assert(!"not yet implemented"); break;
  case OP_NODE_AFTER: assert(!"not yet implemented"); break;
  case FN_ROOT: assert(!"not yet implemented"); break;
  case FN_ROOT2: {
    df_node *n;
    assert(values[0]->seqtype->type == SEQTYPE_ITEM);
    assert(values[0]->seqtype->item->kind != ITEM_ATOMIC);
    n = values[0]->value.n;
    while (NULL != n->parent)
      n = n->parent;

    *result = df_node_to_value(n);
    break;
  }
  case FN_BOOLEAN: assert(!"not yet implemented"); break;
  case OP_CONCATENATE: assert(!"not yet implemented"); break;
  case FN_INDEX_OF: assert(!"not yet implemented"); break;
  case FN_INDEX_OF2: assert(!"not yet implemented"); break;
  case FN_EMPTY: {
    *result = df_value_new(instr->outports[0].seqtype);
    (*result)->value.b = (SEQTYPE_EMPTY == values[0]->seqtype->type);
    break;
  }
  case FN_EXISTS: assert(!"not yet implemented"); break;
  case FN_DISTINCT_VALUES: assert(!"not yet implemented"); break;
  case FN_DISTINCT_VALUES2: assert(!"not yet implemented"); break;
  case FN_INSERT_BEFORE: assert(!"not yet implemented"); break;
  case FN_REMOVE: assert(!"not yet implemented"); break;
  case FN_REVERSE: assert(!"not yet implemented"); break;
  case FN_SUBSEQUENCE: assert(!"not yet implemented"); break;
  case FN_SUBSEQUENCE2: assert(!"not yet implemented"); break;
  case FN_UNORDERED: assert(!"not yet implemented"); break;
  case FN_ZERO_OR_ONE: assert(!"not yet implemented"); break;
  case FN_ONE_OR_MORE: assert(!"not yet implemented"); break;
  case FN_EXACTLY_ONE: assert(!"not yet implemented"); break;
  case FN_DEEP_EQUAL: assert(!"not yet implemented"); break;
  case FN_DEEP_EQUAL2: assert(!"not yet implemented"); break;
  case OP_UNION: assert(!"not yet implemented"); break;
  case OP_INTERSECT: assert(!"not yet implemented"); break;
  case OP_EXCEPT: assert(!"not yet implemented"); break;
  case FN_COUNT: {
    list *seq = df_sequence_to_list(values[0]);
    int count = list_count(seq);
    list_free(seq,NULL);
    *result = df_value_new_int(g,count);
    break;
  }
  case FN_AVG: assert(!"not yet implemented"); break;
  case FN_MAX: assert(!"not yet implemented"); break;
  case FN_MAX2: assert(!"not yet implemented"); break;
  case FN_MIN: assert(!"not yet implemented"); break;
  case FN_MIN2: assert(!"not yet implemented"); break;
  case FN_SUM: assert(!"not yet implemented"); break;
  case FN_SUM2: assert(!"not yet implemented"); break;
  case OP_TO: assert(!"not yet implemented"); break;
  case FN_ID: assert(!"not yet implemented"); break;
  case FN_ID2: assert(!"not yet implemented"); break;
  case FN_IDREF: assert(!"not yet implemented"); break;
  case FN_IDREF2: assert(!"not yet implemented"); break;
  case FN_DOC: {
    char *uri;
    FILE *f;
    xmlDocPtr doc;
    xmlNodePtr root;
    df_node *docnode;
    df_node *rootelem;
    df_seqtype *restype;
    assert(df_check_derived_atomic_type(values[0],g->string_type));
    uri = values[0]->value.s;
/*     debugl("Loading document \"%s\"...",uri); */

    if (NULL == (f = fopen(uri,"r"))) {
      fprintf(stderr,"Can't open %s: %s\n",uri,strerror(errno));
      return -1; /* FIXME: raise an error here */
    }

    if (NULL == (doc = xmlReadFd(fileno(f),NULL,NULL,0))) {
      fclose(f);
      fprintf(stderr,"XML parse error.\n");
      return -1; /* FIXME: raise an error here */
    }
    fclose(f);

    if (NULL == (root = xmlDocGetRootElement(doc))) {
      fprintf(stderr,"No root element.\n");
      xmlFreeDoc(doc);
      return -1; /* FIXME: raise an error here */
    }

    docnode = df_node_new(NODE_DOCUMENT);
    rootelem = df_node_from_xmlnode(root);
    df_strip_spaces(rootelem,state->program->space_decls);
    df_node_add_child(docnode,rootelem);

    xmlFreeDoc(doc);

    restype = df_seqtype_new_item(ITEM_DOCUMENT);
    *result = df_value_new(restype);
    df_seqtype_deref(restype);
    (*result)->value.n = docnode;
    assert(0 == (*result)->value.n->refcount);
    (*result)->value.n->refcount++;

    break;
  }
  case FN_DOC_AVAILABLE: assert(!"not yet implemented"); break;
  case FN_COLLECTION: assert(!"not yet implemented"); break;
  case FN_COLLECTION2: assert(!"not yet implemented"); break;
  case FN_POSITION: assert(!"not yet implemented"); break;
  case FN_LAST: assert(!"not yet implemented"); break;
  case FN_CURRENT_DATETIME: assert(!"not yet implemented"); break;
  case FN_CURRENT_DATE: assert(!"not yet implemented"); break;
  case FN_CURRENT_TIME: assert(!"not yet implemented"); break;
  case FN_IMPLICIT_TIMEZONE: assert(!"not yet implemented"); break;
  case FN_DEFAULT_COLLATION: assert(!"not yet implemented"); break;
  case FN_STATIC_BASE_URI: assert(!"not yet implemented"); break;

  /* Special operations: handled in df_fire_activity() */

  default:
    assert(!"invalid opcode");
    break;
  }

  return 0;
}


/*





  @implements(xpath-functions:durations-dates-times-1) status { deferred } @end
  @implements(xpath-functions:date-time-values-1) status { deferred } @end
  @implements(xpath-functions:d1e3810-1) status { deferred } @end
  @implements(xpath-functions:d1e3828-1) status { deferred } @end
  @implements(xpath-functions:d1e3828-2) status { deferred } @end
  @implements(xpath-functions:d1e3828-3) status { deferred } @end
  @implements(xpath-functions:d1e3828-4) status { deferred } @end
  @implements(xpath-functions:d1e3828-5) status { deferred } @end
  @implements(xpath-functions:d1e3828-6) status { deferred } @end
  @implements(xpath-functions:date-time-duration-conformance-1) status { deferred } @end
  @implements(xpath-functions:date-time-duration-conformance-2) status { deferred } @end
  @implements(xpath-functions:date-time-duration-conformance-3) status { deferred } @end
  @implements(xpath-functions:date-time-duration-conformance-4) status { deferred } @end
  @implements(xpath-functions:duration-subtypes-1) status { deferred } @end
  @implements(xpath-functions:dt-yearMonthDuration-1) status { deferred } @end
  @implements(xpath-functions:dt-yearMonthDuration-2) status { deferred } @end
  @implements(xpath-functions:dt-yearMonthDuration-3) status { deferred } @end
  @implements(xpath-functions:dt-yearMonthDuration-4) status { deferred } @end
  @implements(xpath-functions:dt-yearMonthDuration-5) status { deferred } @end
  @implements(xpath-functions:dt-yearMonthDuration-6) status { deferred } @end
  @implements(xpath-functions:dt-yearMonthDuration-7) status { deferred } @end
  @implements(xpath-functions:dt-dayTimeDuration-1) status { deferred } @end
  @implements(xpath-functions:dt-dayTimeDuration-2) status { deferred } @end
  @implements(xpath-functions:dt-dayTimeDuration-3) status { deferred } @end
  @implements(xpath-functions:dt-dayTimeDuration-4) status { deferred } @end
  @implements(xpath-functions:dt-dayTimeDuration-5) status { deferred } @end
  @implements(xpath-functions:dt-dayTimeDuration-6) status { deferred } @end
  @implements(xpath-functions:comp.duration.datetime-1) status { deferred } @end
  @implements(xpath-functions:comp.duration.datetime-2) status { deferred } @end
  @implements(xpath-functions:comp.duration.datetime-3) status { deferred } @end
  @implements(xpath-functions:comp.duration.datetime-4) status { deferred } @end
  @implements(xpath-functions:comp.duration.datetime-5) status { deferred } @end
  @implements(xpath-functions:comp.duration.datetime-6) status { deferred } @end
  @implements(xpath-functions:comp.duration.datetime-7) status { deferred } @end
  @implements(xpath-functions:comp.duration.datetime-8) status { deferred } @end
  @implements(xpath-functions:comp.duration.datetime-9) status { deferred } @end
  @implements(xpath-functions:func-yearMonthDuration-equal-1) status { deferred } @end
  @implements(xpath-functions:func-yearMonthDuration-equal-2) status { deferred } @end
  @implements(xpath-functions:func-yearMonthDuration-equal-3) status { deferred } @end
  @implements(xpath-functions:func-yearMonthDuration-less-than-1) status { deferred } @end
  @implements(xpath-functions:func-yearMonthDuration-less-than-2) status { deferred } @end
  @implements(xpath-functions:func-yearMonthDuration-less-than-3) status { deferred } @end
  @implements(xpath-functions:func-yearMonthDuration-greater-than-1) status { deferred } @end
  @implements(xpath-functions:func-yearMonthDuration-greater-than-2) status { deferred } @end
  @implements(xpath-functions:func-yearMonthDuration-greater-than-3) status { deferred } @end
  @implements(xpath-functions:func-dayTimeDuration-equal-1) status { deferred } @end
  @implements(xpath-functions:func-dayTimeDuration-equal-2) status { deferred } @end
  @implements(xpath-functions:func-dayTimeDuration-equal-3) status { deferred } @end
  @implements(xpath-functions:func-dayTimeDuration-less-than-1) status { deferred } @end
  @implements(xpath-functions:func-dayTimeDuration-less-than-2) status { deferred } @end
  @implements(xpath-functions:func-dayTimeDuration-less-than-3) status { deferred } @end
  @implements(xpath-functions:func-dayTimeDuration-greater-than-1) status { deferred } @end
  @implements(xpath-functions:func-dayTimeDuration-greater-than-2) status { deferred } @end
  @implements(xpath-functions:func-dayTimeDuration-greater-than-3) status { deferred } @end
  @implements(xpath-functions:func-dateTime-equal-1) status { deferred } @end
  @implements(xpath-functions:func-dateTime-equal-2) status { deferred } @end
  @implements(xpath-functions:func-dateTime-equal-3) status { deferred } @end
  @implements(xpath-functions:func-dateTime-equal-4) status { deferred } @end
  @implements(xpath-functions:func-dateTime-less-than-1) status { deferred } @end
  @implements(xpath-functions:func-dateTime-less-than-2) status { deferred } @end
  @implements(xpath-functions:func-dateTime-less-than-3) status { deferred } @end
  @implements(xpath-functions:func-dateTime-greater-than-1) status { deferred } @end
  @implements(xpath-functions:func-dateTime-greater-than-2) status { deferred } @end
  @implements(xpath-functions:func-dateTime-greater-than-3) status { deferred } @end
  @implements(xpath-functions:func-date-equal-1) status { deferred } @end
  @implements(xpath-functions:func-date-equal-2) status { deferred } @end
  @implements(xpath-functions:func-date-equal-3) status { deferred } @end
  @implements(xpath-functions:func-date-equal-4) status { deferred } @end
  @implements(xpath-functions:func-date-equal-5) status { deferred } @end
  @implements(xpath-functions:func-date-equal-6) status { deferred } @end
  @implements(xpath-functions:func-date-less-than-1) status { deferred } @end
  @implements(xpath-functions:func-date-less-than-2) status { deferred } @end
  @implements(xpath-functions:func-date-less-than-3) status { deferred } @end
  @implements(xpath-functions:func-date-less-than-4) status { deferred } @end
  @implements(xpath-functions:func-date-less-than-5) status { deferred } @end
  @implements(xpath-functions:func-date-less-than-6) status { deferred } @end
  @implements(xpath-functions:func-date-greater-than-1) status { deferred } @end
  @implements(xpath-functions:func-date-greater-than-2) status { deferred } @end
  @implements(xpath-functions:func-date-greater-than-3) status { deferred } @end
  @implements(xpath-functions:func-date-greater-than-4) status { deferred } @end
  @implements(xpath-functions:func-date-greater-than-5) status { deferred } @end
  @implements(xpath-functions:func-date-greater-than-6) status { deferred } @end
  @implements(xpath-functions:func-time-equal-1) status { deferred } @end
  @implements(xpath-functions:func-time-equal-2) status { deferred } @end
  @implements(xpath-functions:func-time-equal-3) status { deferred } @end
  @implements(xpath-functions:func-time-equal-4) status { deferred } @end
  @implements(xpath-functions:func-time-equal-5) status { deferred } @end
  @implements(xpath-functions:func-time-less-than-1) status { deferred } @end
  @implements(xpath-functions:func-time-less-than-2) status { deferred } @end
  @implements(xpath-functions:func-time-less-than-3) status { deferred } @end
  @implements(xpath-functions:func-time-less-than-4) status { deferred } @end
  @implements(xpath-functions:func-time-less-than-5) status { deferred } @end
  @implements(xpath-functions:func-time-greater-than-1) status { deferred } @end
  @implements(xpath-functions:func-time-greater-than-2) status { deferred } @end
  @implements(xpath-functions:func-time-greater-than-3) status { deferred } @end
  @implements(xpath-functions:func-time-greater-than-4) status { deferred } @end
  @implements(xpath-functions:func-time-greater-than-5) status { deferred } @end
  @implements(xpath-functions:func-gYearMonth-equal-1) status { deferred } @end
  @implements(xpath-functions:func-gYearMonth-equal-2) status { deferred } @end
  @implements(xpath-functions:func-gYearMonth-equal-3) status { deferred } @end
  @implements(xpath-functions:func-gYearMonth-equal-4) status { deferred } @end
  @implements(xpath-functions:func-gYearMonth-equal-5) status { deferred } @end
  @implements(xpath-functions:func-gYear-equal-1) status { deferred } @end
  @implements(xpath-functions:func-gYear-equal-2) status { deferred } @end
  @implements(xpath-functions:func-gYear-equal-3) status { deferred } @end
  @implements(xpath-functions:func-gYear-equal-4) status { deferred } @end
  @implements(xpath-functions:func-gYear-equal-5) status { deferred } @end
  @implements(xpath-functions:func-gMonthDay-equal-1) status { deferred } @end
  @implements(xpath-functions:func-gMonthDay-equal-2) status { deferred } @end
  @implements(xpath-functions:func-gMonthDay-equal-3) status { deferred } @end
  @implements(xpath-functions:func-gMonthDay-equal-4) status { deferred } @end
  @implements(xpath-functions:func-gMonthDay-equal-5) status { deferred } @end
  @implements(xpath-functions:func-gMonth-equal-1) status { deferred } @end
  @implements(xpath-functions:func-gMonth-equal-2) status { deferred } @end
  @implements(xpath-functions:func-gMonth-equal-3) status { deferred } @end
  @implements(xpath-functions:func-gMonth-equal-4) status { deferred } @end
  @implements(xpath-functions:func-gMonth-equal-5) status { deferred } @end
  @implements(xpath-functions:func-gDay-equal-1) status { deferred } @end
  @implements(xpath-functions:func-gDay-equal-2) status { deferred } @end
  @implements(xpath-functions:func-gDay-equal-3) status { deferred } @end
  @implements(xpath-functions:func-gDay-equal-4) status { deferred } @end
  @implements(xpath-functions:func-gDay-equal-5) status { deferred } @end
  @implements(xpath-functions:d1e4771-1) status { deferred } @end
  @implements(xpath-functions:d1e4771-2) status { deferred } @end
  @implements(xpath-functions:func-years-from-duration-1) status { deferred } @end
  @implements(xpath-functions:func-years-from-duration-2) status { deferred } @end
  @implements(xpath-functions:func-years-from-duration-3) status { deferred } @end
  @implements(xpath-functions:func-years-from-duration-4) status { deferred } @end
  @implements(xpath-functions:func-months-from-duration-1) status { deferred } @end
  @implements(xpath-functions:func-months-from-duration-2) status { deferred } @end
  @implements(xpath-functions:func-months-from-duration-3) status { deferred } @end
  @implements(xpath-functions:func-months-from-duration-4) status { deferred } @end
  @implements(xpath-functions:func-days-from-duration-1) status { deferred } @end
  @implements(xpath-functions:func-days-from-duration-2) status { deferred } @end
  @implements(xpath-functions:func-days-from-duration-3) status { deferred } @end
  @implements(xpath-functions:func-days-from-duration-4) status { deferred } @end
  @implements(xpath-functions:func-hours-from-duration-1) status { deferred } @end
  @implements(xpath-functions:func-hours-from-duration-2) status { deferred } @end
  @implements(xpath-functions:func-hours-from-duration-3) status { deferred } @end
  @implements(xpath-functions:func-hours-from-duration-4) status { deferred } @end
  @implements(xpath-functions:func-minutes-from-duration-1) status { deferred } @end
  @implements(xpath-functions:func-minutes-from-duration-2) status { deferred } @end
  @implements(xpath-functions:func-minutes-from-duration-3) status { deferred } @end
  @implements(xpath-functions:func-minutes-from-duration-4) status { deferred } @end
  @implements(xpath-functions:func-seconds-from-duration-1) status { deferred } @end
  @implements(xpath-functions:func-seconds-from-duration-2) status { deferred } @end
  @implements(xpath-functions:func-seconds-from-duration-3) status { deferred } @end
  @implements(xpath-functions:func-seconds-from-duration-4) status { deferred } @end
  @implements(xpath-functions:func-year-from-dateTime-1) status { deferred } @end
  @implements(xpath-functions:func-year-from-dateTime-2) status { deferred } @end
  @implements(xpath-functions:func-year-from-dateTime-3) status { deferred } @end
  @implements(xpath-functions:func-year-from-dateTime-4) status { deferred } @end
  @implements(xpath-functions:func-month-from-dateTime-1) status { deferred } @end
  @implements(xpath-functions:func-month-from-dateTime-2) status { deferred } @end
  @implements(xpath-functions:func-month-from-dateTime-3) status { deferred } @end
  @implements(xpath-functions:func-month-from-dateTime-4) status { deferred } @end
  @implements(xpath-functions:func-day-from-dateTime-1) status { deferred } @end
  @implements(xpath-functions:func-day-from-dateTime-2) status { deferred } @end
  @implements(xpath-functions:func-day-from-dateTime-3) status { deferred } @end
  @implements(xpath-functions:func-day-from-dateTime-4) status { deferred } @end
  @implements(xpath-functions:func-hours-from-dateTime-1) status { deferred } @end
  @implements(xpath-functions:func-hours-from-dateTime-2) status { deferred } @end
  @implements(xpath-functions:func-hours-from-dateTime-3) status { deferred } @end
  @implements(xpath-functions:func-hours-from-dateTime-4) status { deferred } @end
  @implements(xpath-functions:func-minutes-from-dateTime-1) status { deferred } @end
  @implements(xpath-functions:func-minutes-from-dateTime-2) status { deferred } @end
  @implements(xpath-functions:func-minutes-from-dateTime-3) status { deferred } @end
  @implements(xpath-functions:func-minutes-from-dateTime-4) status { deferred } @end
  @implements(xpath-functions:func-seconds-from-dateTime-1) status { deferred } @end
  @implements(xpath-functions:func-seconds-from-dateTime-2) status { deferred } @end
  @implements(xpath-functions:func-seconds-from-dateTime-3) status { deferred } @end
  @implements(xpath-functions:func-seconds-from-dateTime-4) status { deferred } @end
  @implements(xpath-functions:func-timezone-from-dateTime-1) status { deferred } @end
  @implements(xpath-functions:func-timezone-from-dateTime-2) status { deferred } @end
  @implements(xpath-functions:func-timezone-from-dateTime-3) status { deferred } @end
  @implements(xpath-functions:func-timezone-from-dateTime-4) status { deferred } @end
  @implements(xpath-functions:func-year-from-date-1) status { deferred } @end
  @implements(xpath-functions:func-year-from-date-2) status { deferred } @end
  @implements(xpath-functions:func-year-from-date-3) status { deferred } @end
  @implements(xpath-functions:func-year-from-date-4) status { deferred } @end
  @implements(xpath-functions:func-month-from-date-1) status { deferred } @end
  @implements(xpath-functions:func-month-from-date-2) status { deferred } @end
  @implements(xpath-functions:func-month-from-date-3) status { deferred } @end
  @implements(xpath-functions:func-month-from-date-4) status { deferred } @end
  @implements(xpath-functions:func-day-from-date-1) status { deferred } @end
  @implements(xpath-functions:func-day-from-date-2) status { deferred } @end
  @implements(xpath-functions:func-day-from-date-3) status { deferred } @end
  @implements(xpath-functions:func-day-from-date-4) status { deferred } @end
  @implements(xpath-functions:func-timezone-from-date-1) status { deferred } @end
  @implements(xpath-functions:func-timezone-from-date-2) status { deferred } @end
  @implements(xpath-functions:func-timezone-from-date-3) status { deferred } @end
  @implements(xpath-functions:func-timezone-from-date-4) status { deferred } @end
  @implements(xpath-functions:func-hours-from-time-1) status { deferred } @end
  @implements(xpath-functions:func-hours-from-time-2) status { deferred } @end
  @implements(xpath-functions:func-hours-from-time-3) status { deferred } @end
  @implements(xpath-functions:func-hours-from-time-4) status { deferred } @end
  @implements(xpath-functions:func-minutes-from-time-1) status { deferred } @end
  @implements(xpath-functions:func-minutes-from-time-2) status { deferred } @end
  @implements(xpath-functions:func-minutes-from-time-3) status { deferred } @end
  @implements(xpath-functions:func-minutes-from-time-4) status { deferred } @end
  @implements(xpath-functions:func-seconds-from-time-1) status { deferred } @end
  @implements(xpath-functions:func-seconds-from-time-2) status { deferred } @end
  @implements(xpath-functions:func-seconds-from-time-3) status { deferred } @end
  @implements(xpath-functions:func-seconds-from-time-4) status { deferred } @end
  @implements(xpath-functions:func-timezone-from-time-1) status { deferred } @end
  @implements(xpath-functions:func-timezone-from-time-2) status { deferred } @end
  @implements(xpath-functions:func-timezone-from-time-3) status { deferred } @end
  @implements(xpath-functions:func-timezone-from-time-4) status { deferred } @end
  @implements(xpath-functions:duration-arithmetic-1) status { deferred } @end
  @implements(xpath-functions:func-add-yearMonthDurations-1) status { deferred } @end
  @implements(xpath-functions:func-add-yearMonthDurations-2) status { deferred } @end
  @implements(xpath-functions:func-add-yearMonthDurations-3) status { deferred } @end
  @implements(xpath-functions:func-subtract-yearMonthDurations-1) status { deferred } @end
  @implements(xpath-functions:func-subtract-yearMonthDurations-2) status { deferred } @end
  @implements(xpath-functions:func-subtract-yearMonthDurations-3) status { deferred } @end
  @implements(xpath-functions:func-multiply-yearMonthDuration-1) status { deferred } @end
  @implements(xpath-functions:func-multiply-yearMonthDuration-2) status { deferred } @end
  @implements(xpath-functions:func-multiply-yearMonthDuration-3) status { deferred } @end
  @implements(xpath-functions:func-multiply-yearMonthDuration-4) status { deferred } @end
  @implements(xpath-functions:func-multiply-yearMonthDuration-5) status { deferred } @end
  @implements(xpath-functions:func-divide-yearMonthDuration-1) status { deferred } @end
  @implements(xpath-functions:func-divide-yearMonthDuration-2) status { deferred } @end
  @implements(xpath-functions:func-divide-yearMonthDuration-3) status { deferred } @end
  @implements(xpath-functions:func-divide-yearMonthDuration-4) status { deferred } @end
  @implements(xpath-functions:func-divide-yearMonthDuration-5) status { deferred } @end
  @implements(xpath-functions:func-divide-yearMonthDuration-by-yearMonthDuration-1) status { deferred } @end
  @implements(xpath-functions:func-divide-yearMonthDuration-by-yearMonthDuration-2) status { deferred } @end
  @implements(xpath-functions:func-divide-yearMonthDuration-by-yearMonthDuration-3) status { deferred } @end
  @implements(xpath-functions:func-divide-yearMonthDuration-by-yearMonthDuration-4) status { deferred } @end
  @implements(xpath-functions:func-add-dayTimeDurations-1) status { deferred } @end
  @implements(xpath-functions:func-add-dayTimeDurations-2) status { deferred } @end
  @implements(xpath-functions:func-add-dayTimeDurations-3) status { deferred } @end
  @implements(xpath-functions:func-subtract-dayTimeDurations-1) status { deferred } @end
  @implements(xpath-functions:func-subtract-dayTimeDurations-2) status { deferred } @end
  @implements(xpath-functions:func-subtract-dayTimeDurations-3) status { deferred } @end
  @implements(xpath-functions:func-multiply-dayTimeDuration-1) status { deferred } @end
  @implements(xpath-functions:func-multiply-dayTimeDuration-2) status { deferred } @end
  @implements(xpath-functions:func-multiply-dayTimeDuration-3) status { deferred } @end
  @implements(xpath-functions:func-multiply-dayTimeDuration-4) status { deferred } @end
  @implements(xpath-functions:func-multiply-dayTimeDuration-5) status { deferred } @end
  @implements(xpath-functions:func-divide-dayTimeDuration-1) status { deferred } @end
  @implements(xpath-functions:func-divide-dayTimeDuration-2) status { deferred } @end
  @implements(xpath-functions:func-divide-dayTimeDuration-3) status { deferred } @end
  @implements(xpath-functions:func-divide-dayTimeDuration-4) status { deferred } @end
  @implements(xpath-functions:func-divide-dayTimeDuration-5) status { deferred } @end
  @implements(xpath-functions:func-divide-dayTimeDuration-by-dayTimeDuration-1) status { deferred } @end
  @implements(xpath-functions:func-divide-dayTimeDuration-by-dayTimeDuration-2) status { deferred } @end
  @implements(xpath-functions:func-divide-dayTimeDuration-by-dayTimeDuration-3) status { deferred } @end
  @implements(xpath-functions:func-divide-dayTimeDuration-by-dayTimeDuration-4) status { deferred } @end
  @implements(xpath-functions:timezone.functions-1) status { deferred } @end
  @implements(xpath-functions:timezone.functions-2) status { deferred } @end
  @implements(xpath-functions:func-adjust-dateTime-to-timezone-1) status { deferred } @end
  @implements(xpath-functions:func-adjust-dateTime-to-timezone-2) status { deferred } @end
  @implements(xpath-functions:func-adjust-dateTime-to-timezone-3) status { deferred } @end
  @implements(xpath-functions:func-adjust-dateTime-to-timezone-4) status { deferred } @end
  @implements(xpath-functions:func-adjust-dateTime-to-timezone-5) status { deferred } @end
  @implements(xpath-functions:func-adjust-dateTime-to-timezone-6) status { deferred } @end
  @implements(xpath-functions:func-adjust-dateTime-to-timezone-7) status { deferred } @end
  @implements(xpath-functions:func-adjust-dateTime-to-timezone-8) status { deferred } @end
  @implements(xpath-functions:func-adjust-dateTime-to-timezone-9) status { deferred } @end
  @implements(xpath-functions:func-adjust-dateTime-to-timezone-10) status { deferred } @end
  @implements(xpath-functions:func-adjust-dateTime-to-timezone-11) status { deferred } @end
  @implements(xpath-functions:func-adjust-date-to-timezone-1) status { deferred } @end
  @implements(xpath-functions:func-adjust-date-to-timezone-2) status { deferred } @end
  @implements(xpath-functions:func-adjust-date-to-timezone-3) status { deferred } @end
  @implements(xpath-functions:func-adjust-date-to-timezone-4) status { deferred } @end
  @implements(xpath-functions:func-adjust-date-to-timezone-5) status { deferred } @end
  @implements(xpath-functions:func-adjust-date-to-timezone-6) status { deferred } @end
  @implements(xpath-functions:func-adjust-date-to-timezone-7) status { deferred } @end
  @implements(xpath-functions:func-adjust-date-to-timezone-8) status { deferred } @end
  @implements(xpath-functions:func-adjust-date-to-timezone-9) status { deferred } @end
  @implements(xpath-functions:func-adjust-date-to-timezone-10) status { deferred } @end
  @implements(xpath-functions:func-adjust-date-to-timezone-11) status { deferred } @end
  @implements(xpath-functions:func-adjust-date-to-timezone-12) status { deferred } @end
  @implements(xpath-functions:func-adjust-time-to-timezone-1) status { deferred } @end
  @implements(xpath-functions:func-adjust-time-to-timezone-2) status { deferred } @end
  @implements(xpath-functions:func-adjust-time-to-timezone-3) status { deferred } @end
  @implements(xpath-functions:func-adjust-time-to-timezone-4) status { deferred } @end
  @implements(xpath-functions:func-adjust-time-to-timezone-5) status { deferred } @end
  @implements(xpath-functions:func-adjust-time-to-timezone-6) status { deferred } @end
  @implements(xpath-functions:func-adjust-time-to-timezone-7) status { deferred } @end
  @implements(xpath-functions:func-adjust-time-to-timezone-8) status { deferred } @end
  @implements(xpath-functions:func-adjust-time-to-timezone-9) status { deferred } @end
  @implements(xpath-functions:func-adjust-time-to-timezone-10) status { deferred } @end
  @implements(xpath-functions:func-adjust-time-to-timezone-11) status { deferred } @end
  @implements(xpath-functions:func-adjust-time-to-timezone-12) status { deferred } @end
  @implements(xpath-functions:dateTime-arithmetic-1) status { deferred } @end
  @implements(xpath-functions:dateTime-arithmetic-2) status { deferred } @end
  @implements(xpath-functions:func-subtract-dateTimes-yielding-dayTimeDuration-1) status { deferred } @end
  @implements(xpath-functions:func-subtract-dateTimes-yielding-dayTimeDuration-2) status { deferred } @end
  @implements(xpath-functions:func-subtract-dateTimes-yielding-dayTimeDuration-3) status { deferred } @end
  @implements(xpath-functions:func-subtract-dateTimes-yielding-dayTimeDuration-4) status { deferred } @end
  @implements(xpath-functions:func-subtract-dateTimes-yielding-dayTimeDuration-5) status { deferred } @end
  @implements(xpath-functions:func-subtract-dates-yielding-dayTimeDuration-1) status { deferred } @end
  @implements(xpath-functions:func-subtract-dates-yielding-dayTimeDuration-2) status { deferred } @end
  @implements(xpath-functions:func-subtract-dates-yielding-dayTimeDuration-3) status { deferred } @end
  @implements(xpath-functions:func-subtract-dates-yielding-dayTimeDuration-4) status { deferred } @end
  @implements(xpath-functions:func-subtract-dates-yielding-dayTimeDuration-5) status { deferred } @end
  @implements(xpath-functions:func-subtract-dates-yielding-dayTimeDuration-6) status { deferred } @end
  @implements(xpath-functions:func-subtract-dates-yielding-dayTimeDuration-7) status { deferred } @end
  @implements(xpath-functions:func-subtract-times-1) status { deferred } @end
  @implements(xpath-functions:func-subtract-times-2) status { deferred } @end
  @implements(xpath-functions:func-subtract-times-3) status { deferred } @end
  @implements(xpath-functions:func-subtract-times-4) status { deferred } @end
  @implements(xpath-functions:func-subtract-times-5) status { deferred } @end
  @implements(xpath-functions:func-subtract-times-6) status { deferred } @end
  @implements(xpath-functions:func-add-yearMonthDuration-to-dateTime-1) status { deferred } @end
  @implements(xpath-functions:func-add-yearMonthDuration-to-dateTime-2) status { deferred } @end
  @implements(xpath-functions:func-add-yearMonthDuration-to-dateTime-3) status { deferred } @end
  @implements(xpath-functions:func-add-yearMonthDuration-to-dateTime-4) status { deferred } @end
  @implements(xpath-functions:func-add-yearMonthDuration-to-dateTime-5) status { deferred } @end
  @implements(xpath-functions:func-add-dayTimeDuration-to-dateTime-1) status { deferred } @end
  @implements(xpath-functions:func-add-dayTimeDuration-to-dateTime-2) status { deferred } @end
  @implements(xpath-functions:func-add-dayTimeDuration-to-dateTime-3) status { deferred } @end
  @implements(xpath-functions:func-add-dayTimeDuration-to-dateTime-4) status { deferred } @end
  @implements(xpath-functions:func-add-dayTimeDuration-to-dateTime-5) status { deferred } @end
  @implements(xpath-functions:func-subtract-yearMonthDuration-from-dateTime-1) status { deferred } @end
  @implements(xpath-functions:func-subtract-yearMonthDuration-from-dateTime-2) status { deferred } @end
  @implements(xpath-functions:func-subtract-yearMonthDuration-from-dateTime-3) status { deferred } @end
  @implements(xpath-functions:func-subtract-yearMonthDuration-from-dateTime-4) status { deferred } @end
  @implements(xpath-functions:func-subtract-yearMonthDuration-from-dateTime-5) status { deferred } @end
  @implements(xpath-functions:func-subtract-dayTimeDuration-from-dateTime-1) status { deferred } @end
  @implements(xpath-functions:func-subtract-dayTimeDuration-from-dateTime-2) status { deferred } @end
  @implements(xpath-functions:func-subtract-dayTimeDuration-from-dateTime-3) status { deferred } @end
  @implements(xpath-functions:func-subtract-dayTimeDuration-from-dateTime-4) status { deferred } @end
  @implements(xpath-functions:func-subtract-dayTimeDuration-from-dateTime-5) status { deferred } @end
  @implements(xpath-functions:func-add-yearMonthDuration-to-date-1) status { deferred } @end
  @implements(xpath-functions:func-add-yearMonthDuration-to-date-2) status { deferred } @end
  @implements(xpath-functions:func-add-yearMonthDuration-to-date-3) status { deferred } @end
  @implements(xpath-functions:func-add-yearMonthDuration-to-date-4) status { deferred } @end
  @implements(xpath-functions:func-add-yearMonthDuration-to-date-5) status { deferred } @end
  @implements(xpath-functions:func-add-yearMonthDuration-to-date-6) status { deferred } @end
  @implements(xpath-functions:func-add-dayTimeDuration-to-date-1) status { deferred } @end
  @implements(xpath-functions:func-add-dayTimeDuration-to-date-2) status { deferred } @end
  @implements(xpath-functions:func-add-dayTimeDuration-to-date-3) status { deferred } @end
  @implements(xpath-functions:func-add-dayTimeDuration-to-date-4) status { deferred } @end
  @implements(xpath-functions:func-add-dayTimeDuration-to-date-5) status { deferred } @end
  @implements(xpath-functions:func-add-dayTimeDuration-to-date-6) status { deferred } @end
  @implements(xpath-functions:func-subtract-yearMonthDuration-from-date-1) status { deferred } @end
  @implements(xpath-functions:func-subtract-yearMonthDuration-from-date-2) status { deferred } @end
  @implements(xpath-functions:func-subtract-yearMonthDuration-from-date-3) status { deferred } @end
  @implements(xpath-functions:func-subtract-yearMonthDuration-from-date-4) status { deferred } @end
  @implements(xpath-functions:func-subtract-yearMonthDuration-from-date-5) status { deferred } @end
  @implements(xpath-functions:func-subtract-yearMonthDuration-from-date-6) status { deferred } @end
  @implements(xpath-functions:func-subtract-dayTimeDuration-from-date-1) status { deferred } @end
  @implements(xpath-functions:func-subtract-dayTimeDuration-from-date-2) status { deferred } @end
  @implements(xpath-functions:func-subtract-dayTimeDuration-from-date-3) status { deferred } @end
  @implements(xpath-functions:func-subtract-dayTimeDuration-from-date-4) status { deferred } @end
  @implements(xpath-functions:func-subtract-dayTimeDuration-from-date-5) status { deferred } @end
  @implements(xpath-functions:func-subtract-dayTimeDuration-from-date-6) status { deferred } @end
  @implements(xpath-functions:func-add-dayTimeDuration-to-time-1) status { deferred } @end
  @implements(xpath-functions:func-add-dayTimeDuration-to-time-2) status { deferred } @end
  @implements(xpath-functions:func-add-dayTimeDuration-to-time-3) status { deferred } @end
  @implements(xpath-functions:func-add-dayTimeDuration-to-time-4) status { deferred } @end
  @implements(xpath-functions:func-add-dayTimeDuration-to-time-5) status { deferred } @end
  @implements(xpath-functions:func-subtract-dayTimeDuration-from-time-1) status { deferred } @end
  @implements(xpath-functions:func-subtract-dayTimeDuration-from-time-2) status { deferred } @end
  @implements(xpath-functions:func-subtract-dayTimeDuration-from-time-3) status { deferred } @end
  @implements(xpath-functions:func-subtract-dayTimeDuration-from-time-4) status { deferred } @end
  @implements(xpath-functions:func-subtract-dayTimeDuration-from-time-5) status { deferred } @end

  @implements(xpath-functions:examples-1) status { info } @end
  @implements(xpath-functions:examples-2) status { info } @end
  @implements(xpath-functions:examples-3) status { info } @end
  @implements(xpath-functions:if-empty-if-absent-1) status { info } @end
  @implements(xpath-functions:if-empty-1) status { info } @end
  @implements(xpath-functions:if-empty-2) status { info } @end
  @implements(xpath-functions:if-empty-3) status { info } @end
  @implements(xpath-functions:if-empty-4) status { info } @end
  @implements(xpath-functions:if-empty-5) status { info } @end
  @implements(xpath-functions:if-empty-6) status { info } @end
  @implements(xpath-functions:if-absent-1) status { info } @end
  @implements(xpath-functions:if-absent-2) status { info } @end
  @implements(xpath-functions:if-absent-3) status { info } @end
  @implements(xpath-functions:if-absent-4) status { info } @end
  @implements(xpath-functions:if-absent-5) status { info } @end
  @implements(xpath-functions:if-absent-6) status { info } @end
  @implements(xpath-functions:value-union-1) status { info } @end
  @implements(xpath-functions:value-union-2) status { info } @end
  @implements(xpath-functions:value-union-3) status { info } @end
  @implements(xpath-functions:value-union-4) status { info } @end
  @implements(xpath-functions:value-union-5) status { info } @end
  @implements(xpath-functions:value-union-6) status { info } @end
  @implements(xpath-functions:value-intersect-1) status { info } @end
  @implements(xpath-functions:value-intersect-2) status { info } @end
  @implements(xpath-functions:value-intersect-3) status { info } @end
  @implements(xpath-functions:value-intersect-4) status { info } @end
  @implements(xpath-functions:value-intersect-5) status { info } @end
  @implements(xpath-functions:value-intersect-6) status { info } @end
  @implements(xpath-functions:value-except-1) status { info } @end
  @implements(xpath-functions:value-except-2) status { info } @end
  @implements(xpath-functions:value-except-3) status { info } @end
  @implements(xpath-functions:value-except-4) status { info } @end
  @implements(xpath-functions:value-except-5) status { info } @end
  @implements(xpath-functions:value-except-6) status { info } @end
  @implements(xpath-functions:index-of-node-1) status { info } @end
  @implements(xpath-functions:index-of-node-2) status { info } @end
  @implements(xpath-functions:index-of-node-3) status { info } @end
  @implements(xpath-functions:index-of-node-4) status { info } @end
  @implements(xpath-functions:index-of-node-5) status { info } @end
  @implements(xpath-functions:index-of-node-6) status { info } @end
  @implements(xpath-functions:index-of-node-7) status { info } @end
  @implements(xpath-functions:index-of-node-8) status { info } @end
  @implements(xpath-functions:index-of-node-9) status { info } @end
  @implements(xpath-functions:index-of-node-10) status { info } @end
  @implements(xpath-functions:string-pad-1) status { info } @end
  @implements(xpath-functions:string-pad-2) status { info } @end
  @implements(xpath-functions:string-pad-3) status { info } @end
  @implements(xpath-functions:string-pad-4) status { info } @end
  @implements(xpath-functions:string-pad-5) status { info } @end
  @implements(xpath-functions:string-pad-6) status { info } @end
  @implements(xpath-functions:string-pad-7) status { info } @end
  @implements(xpath-functions:func-distinct-nodes-stable-1) status { info } @end
  @implements(xpath-functions:func-distinct-nodes-stable-2) status { info } @end
  @implements(xpath-functions:func-distinct-nodes-stable-3) status { info } @end
  @implements(xpath-functions:func-distinct-nodes-stable-4) status { info } @end
  @implements(xpath-functions:func-distinct-nodes-stable-5) status { info } @end
  @implements(xpath-functions:func-distinct-nodes-stable-6) status { info } @end
  @implements(xpath-functions:func-distinct-nodes-stable-7) status { info } @end
  @implements(xpath-functions:func-distinct-nodes-stable-8) status { info } @end
  @implements(xpath-functions:working-with-duration-values-1) status { info } @end
  @implements(xpath-functions:working-with-duration-values-2) status { info } @end
  @implements(xpath-functions:working-with-duration-values-3) status { info } @end
  @implements(xpath-functions:working-with-duration-values-4) status { info } @end
  @implements(xpath-functions:working-with-duration-values-5) status { info } @end
  @implements(xpath-functions:working-with-duration-values-6) status { info } @end


  @implements(xpath-functions:normative-biblio-1) status { info } @end
  @implements(xpath-functions:non-normative-biblio-1) status { info } @end

  @implements(xpath-functions:quickref-section-1) status { info } @end

  @implements(xpath-functions:quickref-alpha-1) status { info } @end
  @implements(xpath-functions:quickref-alpha-2) status { info } @end
  @implements(xpath-functions:quickref-alpha-3) status { info } @end
  @implements(xpath-functions:quickref-alpha-4) status { info } @end
  @implements(xpath-functions:quickref-alpha-5) status { info } @end
  @implements(xpath-functions:quickref-alpha-6) status { info } @end
  @implements(xpath-functions:quickref-alpha-7) status { info } @end
  @implements(xpath-functions:quickref-alpha-8) status { info } @end
  @implements(xpath-functions:quickref-alpha-9) status { info } @end
  @implements(xpath-functions:quickref-alpha-10) status { info } @end
  @implements(xpath-functions:quickref-alpha-11) status { info } @end
  @implements(xpath-functions:quickref-alpha-12) status { info } @end
  @implements(xpath-functions:quickref-alpha-13) status { info } @end
  @implements(xpath-functions:quickref-alpha-14) status { info } @end
  @implements(xpath-functions:quickref-alpha-15) status { info } @end
  @implements(xpath-functions:quickref-alpha-16) status { info } @end
  @implements(xpath-functions:quickref-alpha-17) status { info } @end
  @implements(xpath-functions:quickref-alpha-18) status { info } @end
  @implements(xpath-functions:quickref-alpha-19) status { info } @end
  @implements(xpath-functions:quickref-alpha-20) status { info } @end
  @implements(xpath-functions:quickref-alpha-21) status { info } @end
  @implements(xpath-functions:quickref-alpha-22) status { info } @end
  @implements(xpath-functions:quickref-alpha-23) status { info } @end
  @implements(xpath-functions:quickref-alpha-24) status { info } @end
  @implements(xpath-functions:quickref-alpha-25) status { info } @end
  @implements(xpath-functions:quickref-alpha-26) status { info } @end
  @implements(xpath-functions:quickref-alpha-27) status { info } @end
  @implements(xpath-functions:quickref-alpha-28) status { info } @end
  @implements(xpath-functions:quickref-alpha-29) status { info } @end
  @implements(xpath-functions:quickref-alpha-30) status { info } @end
  @implements(xpath-functions:quickref-alpha-31) status { info } @end
  @implements(xpath-functions:quickref-alpha-32) status { info } @end
  @implements(xpath-functions:quickref-alpha-33) status { info } @end
  @implements(xpath-functions:quickref-alpha-34) status { info } @end
  @implements(xpath-functions:quickref-alpha-35) status { info } @end
  @implements(xpath-functions:quickref-alpha-36) status { info } @end
  @implements(xpath-functions:quickref-alpha-37) status { info } @end
  @implements(xpath-functions:quickref-alpha-38) status { info } @end
  @implements(xpath-functions:quickref-alpha-39) status { info } @end
  @implements(xpath-functions:quickref-alpha-40) status { info } @end
  @implements(xpath-functions:quickref-alpha-41) status { info } @end
  @implements(xpath-functions:quickref-alpha-42) status { info } @end
  @implements(xpath-functions:quickref-alpha-43) status { info } @end
  @implements(xpath-functions:quickref-alpha-44) status { info } @end
  @implements(xpath-functions:quickref-alpha-45) status { info } @end
  @implements(xpath-functions:quickref-alpha-46) status { info } @end
  @implements(xpath-functions:quickref-alpha-47) status { info } @end
  @implements(xpath-functions:quickref-alpha-48) status { info } @end
  @implements(xpath-functions:quickref-alpha-49) status { info } @end
  @implements(xpath-functions:quickref-alpha-50) status { info } @end
  @implements(xpath-functions:quickref-alpha-51) status { info } @end
  @implements(xpath-functions:quickref-alpha-52) status { info } @end
  @implements(xpath-functions:quickref-alpha-53) status { info } @end
  @implements(xpath-functions:quickref-alpha-54) status { info } @end
  @implements(xpath-functions:quickref-alpha-55) status { info } @end
  @implements(xpath-functions:quickref-alpha-56) status { info } @end
  @implements(xpath-functions:quickref-alpha-57) status { info } @end
  @implements(xpath-functions:quickref-alpha-58) status { info } @end
  @implements(xpath-functions:quickref-alpha-59) status { info } @end
  @implements(xpath-functions:quickref-alpha-60) status { info } @end
  @implements(xpath-functions:quickref-alpha-61) status { info } @end
  @implements(xpath-functions:quickref-alpha-62) status { info } @end
  @implements(xpath-functions:quickref-alpha-63) status { info } @end
  @implements(xpath-functions:quickref-alpha-64) status { info } @end
  @implements(xpath-functions:quickref-alpha-65) status { info } @end
  @implements(xpath-functions:quickref-alpha-66) status { info } @end
  @implements(xpath-functions:quickref-alpha-67) status { info } @end
  @implements(xpath-functions:quickref-alpha-68) status { info } @end
  @implements(xpath-functions:quickref-alpha-69) status { info } @end
  @implements(xpath-functions:quickref-alpha-70) status { info } @end
  @implements(xpath-functions:quickref-alpha-71) status { info } @end
  @implements(xpath-functions:quickref-alpha-72) status { info } @end
  @implements(xpath-functions:quickref-alpha-73) status { info } @end
  @implements(xpath-functions:quickref-alpha-74) status { info } @end
  @implements(xpath-functions:quickref-alpha-75) status { info } @end
  @implements(xpath-functions:quickref-alpha-76) status { info } @end
  @implements(xpath-functions:quickref-alpha-77) status { info } @end
  @implements(xpath-functions:quickref-alpha-78) status { info } @end
  @implements(xpath-functions:quickref-alpha-79) status { info } @end
  @implements(xpath-functions:quickref-alpha-80) status { info } @end
  @implements(xpath-functions:quickref-alpha-81) status { info } @end
  @implements(xpath-functions:quickref-alpha-82) status { info } @end
  @implements(xpath-functions:quickref-alpha-83) status { info } @end
  @implements(xpath-functions:quickref-alpha-84) status { info } @end
  @implements(xpath-functions:quickref-alpha-85) status { info } @end
  @implements(xpath-functions:quickref-alpha-86) status { info } @end
  @implements(xpath-functions:quickref-alpha-87) status { info } @end
  @implements(xpath-functions:quickref-alpha-88) status { info } @end
  @implements(xpath-functions:quickref-alpha-89) status { info } @end
  @implements(xpath-functions:quickref-alpha-90) status { info } @end
  @implements(xpath-functions:quickref-alpha-91) status { info } @end
  @implements(xpath-functions:quickref-alpha-92) status { info } @end
  @implements(xpath-functions:quickref-alpha-93) status { info } @end
  @implements(xpath-functions:quickref-alpha-94) status { info } @end
  @implements(xpath-functions:quickref-alpha-95) status { info } @end
  @implements(xpath-functions:quickref-alpha-96) status { info } @end
  @implements(xpath-functions:quickref-alpha-97) status { info } @end
  @implements(xpath-functions:quickref-alpha-98) status { info } @end
  @implements(xpath-functions:quickref-alpha-99) status { info } @end
  @implements(xpath-functions:quickref-alpha-100) status { info } @end
  @implements(xpath-functions:quickref-alpha-101) status { info } @end
  @implements(xpath-functions:quickref-alpha-102) status { info } @end
  @implements(xpath-functions:quickref-alpha-103) status { info } @end
  @implements(xpath-functions:quickref-alpha-104) status { info } @end
  @implements(xpath-functions:quickref-alpha-105) status { info } @end
  @implements(xpath-functions:quickref-alpha-106) status { info } @end
  @implements(xpath-functions:quickref-alpha-107) status { info } @end
  @implements(xpath-functions:quickref-alpha-108) status { info } @end
  @implements(xpath-functions:quickref-alpha-109) status { info } @end
  @implements(xpath-functions:quickref-alpha-110) status { info } @end
  @implements(xpath-functions:quickref-alpha-111) status { info } @end
  @implements(xpath-functions:quickref-alpha-112) status { info } @end
  @implements(xpath-functions:quickref-alpha-113) status { info } @end
  @implements(xpath-functions:quickref-alpha-114) status { info } @end
  @implements(xpath-functions:quickref-alpha-115) status { info } @end
  @implements(xpath-functions:quickref-alpha-116) status { info } @end
  @implements(xpath-functions:quickref-alpha-117) status { info } @end
  @implements(xpath-functions:quickref-alpha-118) status { info } @end
  @implements(xpath-functions:quickref-alpha-119) status { info } @end
  @implements(xpath-functions:quickref-alpha-120) status { info } @end
  @implements(xpath-functions:quickref-alpha-121) status { info } @end
  @implements(xpath-functions:quickref-alpha-122) status { info } @end
  @implements(xpath-functions:quickref-alpha-123) status { info } @end
  @implements(xpath-functions:quickref-alpha-124) status { info } @end
  @implements(xpath-functions:quickref-alpha-125) status { info } @end
  @implements(xpath-functions:quickref-alpha-126) status { info } @end
  @implements(xpath-functions:quickref-alpha-127) status { info } @end
  @implements(xpath-functions:quickref-alpha-128) status { info } @end
  @implements(xpath-functions:quickref-alpha-129) status { info } @end
  @implements(xpath-functions:quickref-alpha-130) status { info } @end
  @implements(xpath-functions:quickref-alpha-131) status { info } @end
  @implements(xpath-functions:quickref-alpha-132) status { info } @end
  @implements(xpath-functions:quickref-alpha-133) status { info } @end
  @implements(xpath-functions:quickref-alpha-134) status { info } @end
  @implements(xpath-functions:quickref-alpha-135) status { info } @end
  @implements(xpath-functions:quickref-alpha-136) status { info } @end
  @implements(xpath-functions:quickref-alpha-137) status { info } @end
  @implements(xpath-functions:quickref-alpha-138) status { info } @end
  @implements(xpath-functions:quickref-alpha-139) status { info } @end
  @implements(xpath-functions:quickref-alpha-140) status { info } @end
  @implements(xpath-functions:quickref-alpha-141) status { info } @end
  @implements(xpath-functions:quickref-alpha-142) status { info } @end
  @implements(xpath-functions:quickref-alpha-143) status { info } @end
  @implements(xpath-functions:quickref-alpha-144) status { info } @end
  @implements(xpath-functions:quickref-alpha-145) status { info } @end
  @implements(xpath-functions:quickref-alpha-146) status { info } @end
  @implements(xpath-functions:quickref-alpha-147) status { info } @end
  @implements(xpath-functions:quickref-alpha-148) status { info } @end
  @implements(xpath-functions:quickref-alpha-149) status { info } @end
  @implements(xpath-functions:quickref-alpha-150) status { info } @end
  @implements(xpath-functions:quickref-alpha-151) status { info } @end
  @implements(xpath-functions:quickref-alpha-152) status { info } @end
  @implements(xpath-functions:quickref-alpha-153) status { info } @end
  @implements(xpath-functions:quickref-alpha-154) status { info } @end
  @implements(xpath-functions:quickref-alpha-155) status { info } @end
  @implements(xpath-functions:quickref-alpha-156) status { info } @end
  @implements(xpath-functions:quickref-alpha-157) status { info } @end
  @implements(xpath-functions:quickref-alpha-158) status { info } @end
  @implements(xpath-functions:quickref-alpha-159) status { info } @end
  @implements(xpath-functions:quickref-alpha-160) status { info } @end
  @implements(xpath-functions:quickref-alpha-161) status { info } @end
  @implements(xpath-functions:quickref-alpha-162) status { info } @end
  @implements(xpath-functions:quickref-alpha-163) status { info } @end
  @implements(xpath-functions:quickref-alpha-164) status { info } @end
  @implements(xpath-functions:quickref-alpha-165) status { info } @end
  @implements(xpath-functions:quickref-alpha-166) status { info } @end
  @implements(xpath-functions:quickref-alpha-167) status { info } @end
  @implements(xpath-functions:quickref-alpha-168) status { info } @end
  @implements(xpath-functions:quickref-alpha-169) status { info } @end
  @implements(xpath-functions:quickref-alpha-170) status { info } @end
  @implements(xpath-functions:quickref-alpha-171) status { info } @end
  @implements(xpath-functions:quickref-alpha-172) status { info } @end
  @implements(xpath-functions:quickref-alpha-173) status { info } @end
  @implements(xpath-functions:quickref-alpha-174) status { info } @end
  @implements(xpath-functions:quickref-alpha-175) status { info } @end
  @implements(xpath-functions:quickref-alpha-176) status { info } @end
  @implements(xpath-functions:quickref-alpha-177) status { info } @end
  @implements(xpath-functions:quickref-alpha-178) status { info } @end
  @implements(xpath-functions:quickref-alpha-179) status { info } @end
  @implements(xpath-functions:quickref-alpha-180) status { info } @end
  @implements(xpath-functions:quickref-alpha-181) status { info } @end
  @implements(xpath-functions:quickref-alpha-182) status { info } @end
  @implements(xpath-functions:quickref-alpha-183) status { info } @end
  @implements(xpath-functions:quickref-alpha-184) status { info } @end
  @implements(xpath-functions:quickref-alpha-185) status { info } @end
  @implements(xpath-functions:quickref-alpha-186) status { info } @end
  @implements(xpath-functions:quickref-alpha-187) status { info } @end
  @implements(xpath-functions:quickref-alpha-188) status { info } @end
  @implements(xpath-functions:quickref-alpha-189) status { info } @end
  @implements(xpath-functions:quickref-alpha-190) status { info } @end
  @implements(xpath-functions:quickref-alpha-191) status { info } @end
  @implements(xpath-functions:quickref-alpha-192) status { info } @end
  @implements(xpath-functions:quickref-alpha-193) status { info } @end
  @implements(xpath-functions:quickref-alpha-194) status { info } @end
  @implements(xpath-functions:quickref-alpha-195) status { info } @end
  @implements(xpath-functions:quickref-alpha-196) status { info } @end
  @implements(xpath-functions:quickref-alpha-197) status { info } @end
  @implements(xpath-functions:quickref-alpha-198) status { info } @end
  @implements(xpath-functions:quickref-alpha-199) status { info } @end
  @implements(xpath-functions:quickref-alpha-200) status { info } @end
  @implements(xpath-functions:quickref-alpha-201) status { info } @end
  @implements(xpath-functions:quickref-alpha-202) status { info } @end
  @implements(xpath-functions:quickref-alpha-203) status { info } @end
  @implements(xpath-functions:quickref-alpha-204) status { info } @end
  @implements(xpath-functions:quickref-alpha-205) status { info } @end
  @implements(xpath-functions:quickref-alpha-206) status { info } @end
  @implements(xpath-functions:quickref-alpha-207) status { info } @end
  @implements(xpath-functions:quickref-alpha-208) status { info } @end
  @implements(xpath-functions:quickref-alpha-209) status { info } @end
  @implements(xpath-functions:quickref-alpha-210) status { info } @end
  @implements(xpath-functions:quickref-alpha-211) status { info } @end
  @implements(xpath-functions:quickref-alpha-212) status { info } @end
  @implements(xpath-functions:quickref-alpha-213) status { info } @end
  @implements(xpath-functions:quickref-alpha-214) status { info } @end
  @implements(xpath-functions:quickref-alpha-215) status { info } @end
  @implements(xpath-functions:quickref-alpha-216) status { info } @end
  @implements(xpath-functions:quickref-alpha-217) status { info } @end

*/
