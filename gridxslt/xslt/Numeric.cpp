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

#include "dataflow/SequenceType.h"
#include "dataflow/Program.h"
#include "util/XMLUtils.h"
#include "util/Debug.h"
#include <libxml/parser.h>
#include <libxml/tree.h>
#include <string.h>
#include <errno.h>
#include <math.h>

using namespace GridXSLT;

#define FNS FN_NAMESPACE


// @implements(xpath-functions:numeric-functions-1) @end
// @implements(xpath-functions:d1e1156-1) @end
// @implements(xpath-functions:d1e1156-2) @end
// @implements(xpath-functions:d1e1156-3) @end
// @implements(xpath-functions:d1e1156-4) @end
// @implements(xpath-functions:op.numeric-1) @end
// @implements(xpath-functions:op.numeric-2) @end
// @implements(xpath-functions:op.numeric-3) @end
// @implements(xpath-functions:op.numeric-4) @end
// @implements(xpath-functions:op.numeric-5) @end
// @implements(xpath-functions:op.numeric-6) @end
// @implements(xpath-functions:op.numeric-7) @end
// @implements(xpath-functions:op.numeric-8) @end
// @implements(xpath-functions:op.numeric-9) @end
// @implements(xpath-functions:op.numeric-10) @end
// @implements(xpath-functions:op.numeric-11) @end
// @implements(xpath-functions:op.numeric-12) @end
// @implements(xpath-functions:op.numeric-13) @end
// @implements(xpath-functions:op.numeric-14) @end
// @implements(xpath-functions:op.numeric-15) @end
// @implements(xpath-functions:op.numeric-16) @end
// @implements(xpath-functions:op.numeric-17) @end

// @implements(xpath-functions:func-numeric-add-1) @end
// @implements(xpath-functions:func-numeric-add-2) @end
// @implements(xpath-functions:func-numeric-add-3) @end
// @implements(xpath-functions:func-numeric-subtract-1) @end
// @implements(xpath-functions:func-numeric-subtract-2) @end
// @implements(xpath-functions:func-numeric-subtract-3) @end
// @implements(xpath-functions:func-numeric-multiply-1) @end
// @implements(xpath-functions:func-numeric-multiply-2) @end
// @implements(xpath-functions:func-numeric-multiply-3) @end
// @implements(xpath-functions:func-numeric-divide-1) @end
// @implements(xpath-functions:func-numeric-divide-2) @end
// @implements(xpath-functions:func-numeric-divide-3) @end
// @implements(xpath-functions:func-numeric-divide-4) @end
// @implements(xpath-functions:func-numeric-integer-divide-1) @end
// @implements(xpath-functions:func-numeric-integer-divide-2) @end
// @implements(xpath-functions:func-numeric-integer-divide-3) @end
// @implements(xpath-functions:func-numeric-integer-divide-4) @end
// @implements(xpath-functions:func-numeric-integer-divide-5) @end
// @implements(xpath-functions:func-numeric-integer-divide-6) @end
// @implements(xpath-functions:func-numeric-mod-1) @end
// @implements(xpath-functions:func-numeric-mod-2) @end
// @implements(xpath-functions:func-numeric-mod-3) @end
// @implements(xpath-functions:func-numeric-mod-4) @end
// @implements(xpath-functions:func-numeric-mod-5) @end
// @implements(xpath-functions:func-numeric-mod-6) @end
// @implements(xpath-functions:func-numeric-unary-plus-1) @end
// @implements(xpath-functions:func-numeric-unary-plus-2) @end
// @implements(xpath-functions:func-numeric-unary-minus-1) @end
// @implements(xpath-functions:func-numeric-unary-minus-2) @end
// @implements(xpath-functions:func-numeric-unary-minus-3) @end

// @implements(xpath-functions:comp.numeric-1) @end
// @implements(xpath-functions:comp.numeric-2) @end
// @implements(xpath-functions:func-numeric-equal-1) @end
// @implements(xpath-functions:func-numeric-equal-2) @end
// @implements(xpath-functions:func-numeric-equal-3) @end
// @implements(xpath-functions:func-numeric-less-than-1) @end
// @implements(xpath-functions:func-numeric-less-than-2) @end
// @implements(xpath-functions:func-numeric-less-than-3) @end
// @implements(xpath-functions:func-numeric-greater-than-1) @end
// @implements(xpath-functions:func-numeric-greater-than-2) @end
// @implements(xpath-functions:func-numeric-greater-than-3) @end

// FIXME: ensure floating point operations are conformant with IEEE 754-1985 as mentioned in
// the spec. For now we just assume the underlying hardware provides this assurance but
// need to confirm; there may be certain cases where we e.g. need to raise an error

// FIXME: technically we should be using arbitrary position integers for the xs:integer type,
// and arbitrary precision decimal numbers for the xs:decimal type. For now we just use int and
// double respectively for these to simplify implementation.

// FIXME: the type conversion code in the execution engine (when implemented) needs to take
// into account that arguments need to be converted into one of the numeric types, even though
// the functions specify item() as their parameter types. Ultimately the code relating to the
// "Non-numeric value used with numeric operator" shouldn't be needed as this error should be
// caught when the converion is attempted (if it is not possible to perform the conversion) - and
// if it is possible (e.g. a string or attribute with a valid numeric value is passed in), then
// this should be allowed.

enum NumericOp {
  NUMERIC_ADD,
  NUMERIC_SUBTRACT,
  NUMERIC_MULTIPLY,
  NUMERIC_DIVIDE,
  NUMERIC_INTEGER_DIVIDE,
  NUMERIC_MOD,
  NUMERIC_EQUAL,
  NUMERIC_LESS_THAN,
  NUMERIC_GREATER_THAN
//   NUMERIC_UNARY_PLUS,
//   NUMERIC_UNARY_MINUS
};


static Value float_op(Environment *env, NumericOp op, float a, float b)
{
  switch (op) {
  case NUMERIC_ADD:
    return a + b;
  case NUMERIC_SUBTRACT:
    return a - b;
  case NUMERIC_MULTIPLY:
    return a * b;
  case NUMERIC_DIVIDE:
    return a / b;
  case NUMERIC_INTEGER_DIVIDE:
    if (isnan(a) || isnan(b) || isinf(a)) {
      error(env->ei,env->sloc.uri,env->sloc.line,"FOAR0002","numeric operation overflow/underflow");
      return Value::null();
    }
    if (0.0 == b) {
      error(env->ei,env->sloc.uri,env->sloc.line,"FOAR0001","division by zero");
      return Value::null();
    }
    return int(a / b);
  case NUMERIC_MOD:
    return fmodf(a,b);
  case NUMERIC_EQUAL:
    return a == b;
  case NUMERIC_LESS_THAN:
    return a < b;
  case NUMERIC_GREATER_THAN:
    return a > b;
  }
  ASSERT(0);
  return Value::null();
}

static Value double_op(Environment *env, NumericOp op, double a, double b)
{
  switch (op) {
  case NUMERIC_ADD:
    return a + b;
  case NUMERIC_SUBTRACT:
    return a - b;
  case NUMERIC_MULTIPLY:
    return a * b;
  case NUMERIC_DIVIDE:
    return a / b;
  case NUMERIC_INTEGER_DIVIDE:
    if (isnan(a) || isnan(b) || isinf(a)) {
      error(env->ei,env->sloc.uri,env->sloc.line,"FOAR0002","numeric operation overflow/underflow");
      return Value::null();
    }
    if (0.0 == b) {
      error(env->ei,env->sloc.uri,env->sloc.line,"FOAR0001","division by zero");
      return Value::null();
    }
    return int(a / b);
  case NUMERIC_MOD:
    return fmod(a,b);
  case NUMERIC_EQUAL:
    return a == b;
  case NUMERIC_LESS_THAN:
    return a < b;
  case NUMERIC_GREATER_THAN:
    return a > b;
  }
  ASSERT(0);
  return Value::null();
}

static Value decimal_op(Environment *env, NumericOp op, double a, double b)
{
  switch (op) {
  case NUMERIC_ADD:
    return Value::decimal(a + b);
  case NUMERIC_SUBTRACT:
    return Value::decimal(a - b);
  case NUMERIC_MULTIPLY:
    return Value::decimal(a * b);
  case NUMERIC_DIVIDE:
    if (0.0 == b) {
      error(env->ei,env->sloc.uri,env->sloc.line,"FOAR0001","division by zero");
      return Value::null();
    }
    return Value::decimal(a / b);
  case NUMERIC_INTEGER_DIVIDE:
    if (isnan(a) || isnan(b) || isinf(a)) {
      error(env->ei,env->sloc.uri,env->sloc.line,"FOAR0002","numeric operation overflow/underflow");
      return Value::null();
    }
    if (0.0 == b) {
      error(env->ei,env->sloc.uri,env->sloc.line,"FOAR0001","division by zero");
      return Value::null();
    }
    return int(a / b);
  case NUMERIC_MOD:
    if (0 == b) {
      error(env->ei,env->sloc.uri,env->sloc.line,"FOAR0001","division by zero");
      return Value::null();
    }
    return Value::decimal(fmod(a,b));
  case NUMERIC_EQUAL:
    return a == b;
  case NUMERIC_LESS_THAN:
    return a < b;
  case NUMERIC_GREATER_THAN:
    return a > b;
  }
  ASSERT(0);
  return Value::null();
}

static Value integer_op(Environment *env, NumericOp op, int a, int b)
{
  switch (op) {
  case NUMERIC_ADD:
    return a + b;
  case NUMERIC_SUBTRACT:
    return a - b;
  case NUMERIC_MULTIPLY:
    return a * b;
  case NUMERIC_DIVIDE:
    if (0 == b) {
      error(env->ei,env->sloc.uri,env->sloc.line,"FOAR0001","division by zero");
      return Value::null();
    }
    return Value::decimal(double(a) / double(b));
  case NUMERIC_INTEGER_DIVIDE:
    if (0 == b) {
      error(env->ei,env->sloc.uri,env->sloc.line,"FOAR0001","division by zero");
      return Value::null();
    }
    return a / b;
  case NUMERIC_MOD:
    if (0 == b) {
      error(env->ei,env->sloc.uri,env->sloc.line,"FOAR0001","division by zero");
      return Value::null();
    }
    return fmod(a,b);
  case NUMERIC_EQUAL:
    return a == b;
  case NUMERIC_LESS_THAN:
    return a < b;
  case NUMERIC_GREATER_THAN:
    return a > b;
  }
  ASSERT(0);
  return Value::null();
}

static Value numeric_op(Environment *env, NumericOp op, const Value &a, const Value &b)
{
  if (!a.isNumeric()) {
    String s = a.convertToString();
    error(env->ei,env->sloc.uri,env->sloc.line,String::null(),
          "Non-numeric value used with numeric operator: %*",&s);
    return Value::null();
  }
  if (!b.isNumeric()) {
    String s = b.convertToString();
    error(env->ei,env->sloc.uri,env->sloc.line,String::null(),
          "Non-numeric value used with numeric operator: %*",&s);
    return Value::null();
  }
  if (a.isFloat()) {
    if (b.isFloat())
      return float_op(env,op,a.asFloat(),b.asFloat());
    else if (b.isDouble())
      return double_op(env,op,double(a.asFloat()),b.asDouble());
    else if (b.isInteger())
      return float_op(env,op,a.asFloat(),float(b.asInteger()));
    else // b.isDecimal()
      return float_op(env,op,a.asFloat(),float(b.asDecimal()));
  }
  else if (a.isDouble()) {
    if (b.isFloat())
      return double_op(env,op,a.asDouble(),double(b.asFloat()));
    else if (b.isDouble())
      return double_op(env,op,a.asDouble(),b.asDouble());
    else if (b.isInteger())
      return double_op(env,op,a.asDouble(),double(b.asInteger()));
    else // b.isDecimal()
      return double_op(env,op,a.asDouble(),double(b.asDecimal()));
  }
  else if (a.isInteger()) {
    if (b.isFloat())
      return float_op(env,op,float(a.asInteger()),b.asFloat());
    else if (b.isDouble())
      return double_op(env,op,double(a.asInteger()),b.asDouble());
    else if (b.isInteger())
      return integer_op(env,op,a.asInteger(),b.asInteger());
    else // b.isDecimal()
      return decimal_op(env,op,double(a.asInteger()),b.asDecimal());
  }
  else { // a.isDecimal()
    if (b.isFloat())
      return float_op(env,op,float(a.asDecimal()),b.asFloat());
    else if (b.isDouble())
      return double_op(env,op,double(a.asDecimal()),b.asDouble());
    else if (b.isInteger())
      return decimal_op(env,op,a.asDecimal(),double(b.asInteger()));
    else // b.isDecimal()
      return decimal_op(env,op,a.asDecimal(),b.asDecimal());
  }

  return Value::null();
}

static Value numeric_add(Environment *env, List<Value> &args)
{
  return numeric_op(env,NUMERIC_ADD,args[0],args[1]);
}

static Value numeric_subtract(Environment *env, List<Value> &args)
{
  return numeric_op(env,NUMERIC_SUBTRACT,args[0],args[1]);
}

static Value numeric_multiply(Environment *env, List<Value> &args)
{
  return numeric_op(env,NUMERIC_MULTIPLY,args[0],args[1]);
}

static Value numeric_divide(Environment *env, List<Value> &args)
{
  return numeric_op(env,NUMERIC_DIVIDE,args[0],args[1]);
}

static Value numeric_mod(Environment *env, List<Value> &args)
{
  return numeric_op(env,NUMERIC_MOD,args[0],args[1]);
}

static Value numeric_integer_divide(Environment *env, List<Value> &args)
{
  return numeric_op(env,NUMERIC_INTEGER_DIVIDE,args[0],args[1]);
}

static Value numeric_equal(Environment *env, List<Value> &args)
{
  return numeric_op(env,NUMERIC_EQUAL,args[0],args[1]);
}

static Value numeric_less_than(Environment *env, List<Value> &args)
{
  return numeric_op(env,NUMERIC_LESS_THAN,args[0],args[1]);
}

static Value numeric_greater_than(Environment *env, List<Value> &args)
{
  return numeric_op(env,NUMERIC_GREATER_THAN,args[0],args[1]);
}

static Value numeric_unary_plus(Environment *env, List<Value> &args)
{
  if (!args[0].isNumeric()) {
    String s = args[0].convertToString();
    error(env->ei,env->sloc.uri,env->sloc.line,String::null(),
          "Non-numeric value used with numeric operator: %*",&s);
    return Value::null();
  }

  return args[0];
}

static Value numeric_unary_minus(Environment *env, List<Value> &args)
{
  if (!args[0].isNumeric()) {
    String s = args[0].convertToString();
    error(env->ei,env->sloc.uri,env->sloc.line,String::null(),
          "Non-numeric value used with numeric operator: %*",&s);
    return Value::null();
  }

  if (args[0].isFloat()) {
    return -args[0].asFloat();
  }
  else if (args[0].isDouble()) {
    return -args[0].asDouble();
  }
  else if (args[0].isInteger()) {
    return -args[0].asInteger();
  }
  else { // args[0].isDecimal()
    double d = args[0].asDecimal();
    if (0.0 == d)
      return Value::decimal(0.0); // for decimal we don't allow -0
    else
      return Value::decimal(-args[0].asDecimal());
  }
}

static Value abs1(Environment *env, List<Value> &args)
{
  if (!args[0].isNumeric()) {
    String s = args[0].convertToString();
    error(env->ei,env->sloc.uri,env->sloc.line,String::null(),
          "Non-numeric value used with numeric operator: %*",&s);
    return Value::null();
  }

  if (args[0].isFloat())
    return fabsf(args[0].asFloat());
  else if (args[0].isDouble())
    return fabs(args[0].asDouble());
  else if (args[0].isInteger())
    return abs(args[0].asInteger());
  else // args[0].isDecimal()
    return Value::decimal(fabs(args[0].asDecimal()));
}

static Value ceiling1(Environment *env, List<Value> &args)
{
  if (!args[0].isNumeric()) {
    String s = args[0].convertToString();
    error(env->ei,env->sloc.uri,env->sloc.line,String::null(),
          "Non-numeric value used with numeric operator: %*",&s);
    return Value::null();
  }

  if (args[0].isFloat())
    return ceilf(args[0].asFloat());
  else if (args[0].isDouble())
    return ceil(args[0].asDouble());
  else if (args[0].isInteger())
    return args[0].asInteger();
  else // args[0].isDecimal()
    return Value::decimal(ceil(args[0].asDecimal()));
}

static Value floor1(Environment *env, List<Value> &args)
{
  if (!args[0].isNumeric()) {
    String s = args[0].convertToString();
    error(env->ei,env->sloc.uri,env->sloc.line,String::null(),
          "Non-numeric value used with numeric operator: %*",&s);
    return Value::null();
  }

  if (args[0].isFloat())
    return floorf(args[0].asFloat());
  else if (args[0].isDouble())
    return floor(args[0].asDouble());
  else if (args[0].isInteger())
    return args[0].asInteger();
  else // args[0].isDecimal()
    return Value::decimal(floor(args[0].asDecimal()));
}

static Value round1(Environment *env, List<Value> &args)
{
  if (!args[0].isNumeric()) {
    String s = args[0].convertToString();
    error(env->ei,env->sloc.uri,env->sloc.line,String::null(),
          "Non-numeric value used with numeric operator: %*",&s);
    return Value::null();
  }

  if (args[0].isFloat()) {
    float f = args[0].asFloat();
    if (0.5 == (fabsf(f) - floorf(fabs(f))))
      return ceilf(f);
    else
      return roundf(f);
  }
  else if (args[0].isDouble()) {
    double d = args[0].asDouble();
    if (0.5 == (fabs(d) - floor(fabs(d))))
      return ceil(d);
    else
      return round(d);
  }
  else if (args[0].isInteger()) {
    return args[0].asInteger();
  }
  else { // args[0].isDecimal()
    double d = args[0].asDecimal();
    if (0.5 == (fabs(d) - floor(fabs(d))))
      return Value::decimal(ceil(d));
    else
      return Value::decimal(round(d));
  }
}

double roundh2e(double d, int precision)
{
  double factor = pow(10,precision);
  d *= factor;

  if (0.5 == fabs(d) - floor(fabs(d))) {
    if (0 == (int(floor(d)) % 2))
      d = floor(d);
    else
      d = ceil(d);
  }
  else {
    d = round(d);
  }

  d /= factor;
  return d;
}

static Value round_half_to_even2(Environment *env, List<Value> &args)
{
  if (!args[0].isNumeric()) {
    String s = args[0].convertToString();
    error(env->ei,env->sloc.uri,env->sloc.line,String::null(),
          "Non-numeric value used with numeric operator: %*",&s);
    return Value::null();
  }

  int precision = args[1].asInteger();

  if (args[0].isFloat()) {
    return float(roundh2e(args[0].asFloat(),precision));
  }
  else if (args[0].isDouble()) {
    return roundh2e(args[0].asDouble(),precision);
  }
  else if (args[0].isInteger()) {
    return int(roundh2e(args[0].asInteger(),precision));
  }
  else { // args[0].isDecimal()
    return Value::decimal(roundh2e(args[0].asDecimal(),precision));
  }
}

static Value round_half_to_even1(Environment *env, List<Value> &args)
{
  List<Value> args2;
  args2.append(args[0]);
  args2.append(Value(0));
  return round_half_to_even2(env,args2);
}

FunctionDefinition numeric_fundefs[18] = {
  { numeric_add,            FNS, "numeric-add",            "item(),item()", "item()",      false },
  { numeric_subtract,       FNS, "numeric-subtract",       "item(),item()", "item()",      false },
  { numeric_multiply,       FNS, "numeric-multiply",       "item(),item()", "item()",      false },
  { numeric_divide,         FNS, "numeric-divide",         "item(),item()", "item()",      false },
  { numeric_integer_divide, FNS, "numeric-integer-divide", "item(),item()", "item()",      false },
  { numeric_mod,            FNS, "numeric-mod",            "item(),item()", "item()",      false },
  { numeric_equal,          FNS, "numeric-equal",          "item(),item()", "xsd:boolean", false },
  { numeric_less_than,      FNS, "numeric-less-than",      "item(),item()", "xsd:boolean", false },
  { numeric_greater_than,   FNS, "numeric-greater-than",   "item(),item()", "xsd:boolean", false },
  { numeric_unary_plus,     FNS, "numeric-unary-plus",     "item()",        "item()",      false },
  { numeric_unary_minus,    FNS, "numeric-unary-minus",    "item()",        "item()",      false },
  { abs1,                   FNS, "abs",                    "item()",        "item()",      false },
  { ceiling1,               FNS, "ceiling",                "item()",        "item()",      false },
  { floor1,                 FNS, "floor",                  "item()",        "item()",      false },
  { round1,                 FNS, "round",                  "item()",        "item()",      false },
  { round_half_to_even2,    FNS, "round-half-to-even",     "item(),xsd:integer", "item()", false },
  { round_half_to_even1,    FNS, "round-half-to-even",     "item()",        "item()",      false },
  { NULL },
};

FunctionDefinition *numeric_module = numeric_fundefs;
