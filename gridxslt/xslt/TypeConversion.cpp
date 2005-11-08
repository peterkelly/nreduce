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
#include <math.h>
#include <stdlib.h>
#include <errno.h>

using namespace GridXSLT;

#define XSNS XS_NAMESPACE

// See conversion table in XQuery/XPath functions and operators section 17.1

// Note: when casting from a numeric type to itself, we create a new value even though
// it's the same type. This is because the argument may be a subtype and we want the
// cast operator to return the actual type itself

// FIXME: Here we just use the native C conversions; the spec gives more detail about
// the conversion process. Should really confirm that we conform to these.

// FIXME: we should probably raise an error when an attempt to cast from an invalid type
// is made... e.g. converting from one of the date types to a number

static Value cvstring(Environment *env, List<Value> &args)
{
  /* FIXME */
  ASSERT(0);
  return Value::null();
}

static Value cvboolean(Environment *env, List<Value> &args)
{
  /* FIXME */
  ASSERT(0);
  return Value::null();
}

static Value cvdecimal(Environment *env, List<Value> &args)
{
  if (args[0].isNumeric() && !args[0].isInteger()) {
    double d;
    if (args[0].isFloat())
      d = args[0].asFloat();
    else if (args[0].isDouble())
      d = args[0].asDouble();
    else 
      d = args[0].asDecimal();
    if (isinf(d) || isnan(d)) {
      error(env->ei,env->sloc.uri,env->sloc.line,"FOCA0002","invalid lexical value");
      return Value::null();
    }
  }

  if (args[0].isFloat())
    return Value::decimal(double(args[0].asFloat()));

  if (args[0].isDouble())
    return Value::decimal(args[0].asDouble());

  if (args[0].isInteger())
    return Value::decimal(double(args[0].asInteger()));

  if (args[0].isDecimal())
    return Value::decimal(double(args[0].asDecimal()));

  if (args[0].isBoolean())
    return Value::decimal(double(args[0].asBoolean() ? 1.0 : 0.0));

  String str = args[0].convertToString().collapseWhitespace();
  if (0 == str.length()) {
    error(env->ei,env->sloc.uri,env->sloc.line,"FORG0001","invalid value for cast/constructor");
    return Value::null();
  }

  char *s = str.cstring();
  char *end = NULL;
  errno = 0;
  double d = strtod(s,&end);
  Value v;
  if ('\0' != *end)
    error(env->ei,env->sloc.uri,env->sloc.line,"FORG0001","invalid value for cast/constructor");
  else if (0 != errno)
    error(env->ei,env->sloc.uri,env->sloc.line,"FOCA0003","input value too large for double");
  else
    v = Value::decimal(d);
  free(s);
  return v;
}

static Value cvfloat(Environment *env, List<Value> &args)
{
  if (args[0].isFloat())
    return args[0].asFloat();

  if (args[0].isDouble())
    return float(args[0].asDouble());

  if (args[0].isInteger())
    return float(args[0].asInteger());

  if (args[0].isDecimal())
    return float(args[0].asDecimal());

  if (args[0].isBoolean())
    return float(args[0].asBoolean() ? 1.0 : 0.0);

  String str = args[0].convertToString().collapseWhitespace();
  if (0 == str.length()) {
    error(env->ei,env->sloc.uri,env->sloc.line,"FORG0001","invalid value for cast/constructor");
    return Value::null();
  }

  char *s = str.cstring();
  char *end = NULL;
  errno = 0;
  float f = strtof(s,&end);
  Value v;
  if ('\0' != *end)
    error(env->ei,env->sloc.uri,env->sloc.line,"FORG0001","invalid value for cast/constructor");
  else if (0 != errno)
    error(env->ei,env->sloc.uri,env->sloc.line,"FOCA0003","input value too large for float");
  else
    v = Value(f);
  free(s);
  return v;
}

static Value cvdouble(Environment *env, List<Value> &args)
{
  if (args[0].isFloat())
    return double(args[0].asFloat());

  if (args[0].isDouble())
    return args[0].asDouble();

  if (args[0].isInteger())
    return double(args[0].asInteger());

  if (args[0].isDecimal())
    return double(args[0].asDecimal());

  if (args[0].isBoolean())
    return double(args[0].asBoolean() ? 1.0 : 0.0);

  String str = args[0].convertToString().collapseWhitespace();
  if (0 == str.length()) {
    error(env->ei,env->sloc.uri,env->sloc.line,"FORG0001","invalid value for cast/constructor");
    return Value::null();
  }

  char *s = str.cstring();
  char *end = NULL;
  errno = 0;
  double d = strtod(s,&end);
  Value v;
  if ('\0' != *end)
    error(env->ei,env->sloc.uri,env->sloc.line,"FORG0001","invalid value for cast/constructor");
  else if (0 != errno)
    error(env->ei,env->sloc.uri,env->sloc.line,"FOCA0003","input value too large for double");
  else
    v = Value(d);
  free(s);
  return v;
}

static Value cvduration(Environment *env, List<Value> &args)
{
  /* FIXME */
  ASSERT(0);
  return Value::null();
}

static Value cvdateTime(Environment *env, List<Value> &args)
{
  /* FIXME */
  ASSERT(0);
  return Value::null();
}

static Value cvtime(Environment *env, List<Value> &args)
{
  /* FIXME */
  ASSERT(0);
  return Value::null();
}

static Value cvdate(Environment *env, List<Value> &args)
{
  /* FIXME */
  ASSERT(0);
  return Value::null();
}

static Value cvgYearMonth(Environment *env, List<Value> &args)
{
  /* FIXME */
  ASSERT(0);
  return Value::null();
}

static Value cvgYear(Environment *env, List<Value> &args)
{
  /* FIXME */
  ASSERT(0);
  return Value::null();
}

static Value cvgMonthDay(Environment *env, List<Value> &args)
{
  /* FIXME */
  ASSERT(0);
  return Value::null();
}

static Value cvgDay(Environment *env, List<Value> &args)
{
  /* FIXME */
  ASSERT(0);
  return Value::null();
}

static Value cvgMonth(Environment *env, List<Value> &args)
{
  /* FIXME */
  ASSERT(0);
  return Value::null();
}

static Value cvhexBinary(Environment *env, List<Value> &args)
{
  /* FIXME */
  ASSERT(0);
  return Value::null();
}

static Value cvbase64Binary(Environment *env, List<Value> &args)
{
  /* FIXME */
  ASSERT(0);
  return Value::null();
}

static Value cvanyURI(Environment *env, List<Value> &args)
{
  /* FIXME */
  ASSERT(0);
  return Value::null();
}

static Value cvQName(Environment *env, List<Value> &args)
{
  /* FIXME */
  ASSERT(0);
  return Value::null();
}

static Value cvnormalizedString(Environment *env, List<Value> &args)
{
  /* FIXME */
  ASSERT(0);
  return Value::null();
}

static Value cvtoken(Environment *env, List<Value> &args)
{
  /* FIXME */
  ASSERT(0);
  return Value::null();
}

static Value cvlanguage(Environment *env, List<Value> &args)
{
  /* FIXME */
  ASSERT(0);
  return Value::null();
}

static Value cvNMTOKEN(Environment *env, List<Value> &args)
{
  /* FIXME */
  ASSERT(0);
  return Value::null();
}

static Value cvName(Environment *env, List<Value> &args)
{
  /* FIXME */
  ASSERT(0);
  return Value::null();
}

static Value cvNCName(Environment *env, List<Value> &args)
{
  /* FIXME */
  ASSERT(0);
  return Value::null();
}

static Value cvID(Environment *env, List<Value> &args)
{
  /* FIXME */
  ASSERT(0);
  return Value::null();
}

static Value cvIDREF(Environment *env, List<Value> &args)
{
  /* FIXME */
  ASSERT(0);
  return Value::null();
}

static Value cvENTITY(Environment *env, List<Value> &args)
{
  /* FIXME */
  ASSERT(0);
  return Value::null();
}

static Value cvinteger(Environment *env, List<Value> &args)
{
  if (args[0].isNumeric() && !args[0].isInteger()) {
    double d;
    if (args[0].isFloat())
      d = args[0].asFloat();
    else if (args[0].isDouble())
      d = args[0].asDouble();
    else 
      d = args[0].asDecimal();
    if (isinf(d) || isnan(d)) {
      error(env->ei,env->sloc.uri,env->sloc.line,"FOCA0002","invalid lexical value");
      return Value::null();
    }
  }

  if (args[0].isFloat())
    return int(args[0].asFloat());

  if (args[0].isDouble())
    return int(args[0].asDouble());

  if (args[0].isInteger())
    return args[0].asInteger();

  if (args[0].isDecimal())
    return int(args[0].asDecimal());

  if (args[0].isBoolean())
    return args[0].asBoolean() ? 1 : 0;

  String str = args[0].convertToString().collapseWhitespace();
  if (0 == str.length()) {
    error(env->ei,env->sloc.uri,env->sloc.line,"FORG0001","invalid value for cast/constructor");
    return Value::null();
  }

  char *s = str.cstring();
  char *end = NULL;
  errno = 0;
  int i = strtol(s,&end,10);
  Value v;
  if ('\0' != *end)
    error(env->ei,env->sloc.uri,env->sloc.line,"FORG0001","invalid value for cast/constructor");
  else if (0 != errno)
    error(env->ei,env->sloc.uri,env->sloc.line,"FOCA0003","input value too large for integer");
  else
    v = Value(i);
  free(s);
  return v;
}

static Value cvnonPositiveInteger(Environment *env, List<Value> &args)
{
  /* FIXME */
  ASSERT(0);
  return Value::null();
}

static Value cvnegativeInteger(Environment *env, List<Value> &args)
{
  /* FIXME */
  ASSERT(0);
  return Value::null();
}

static Value cvlong(Environment *env, List<Value> &args)
{
  /* FIXME */
  ASSERT(0);
  return Value::null();
}

static Value cvint(Environment *env, List<Value> &args)
{
  /* FIXME */
  ASSERT(0);
  return Value::null();
}

static Value cvshort(Environment *env, List<Value> &args)
{
  /* FIXME */
  ASSERT(0);
  return Value::null();
}

static Value cvbyte(Environment *env, List<Value> &args)
{
  /* FIXME */
  ASSERT(0);
  return Value::null();
}

static Value cvnonNegativeInteger(Environment *env, List<Value> &args)
{
  /* FIXME */
  ASSERT(0);
  return Value::null();
}

static Value cvunsignedLong(Environment *env, List<Value> &args)
{
  /* FIXME */
  ASSERT(0);
  return Value::null();
}

static Value cvunsignedInt(Environment *env, List<Value> &args)
{
  /* FIXME */
  ASSERT(0);
  return Value::null();
}

static Value cvunsignedShort(Environment *env, List<Value> &args)
{
  /* FIXME */
  ASSERT(0);
  return Value::null();
}

static Value cvunsignedByte(Environment *env, List<Value> &args)
{
  /* FIXME */
  ASSERT(0);
  return Value::null();
}

static Value cvpositiveInteger(Environment *env, List<Value> &args)
{
  /* FIXME */
  ASSERT(0);
  return Value::null();
}

static Value cvyearMonthDuration(Environment *env, List<Value> &args)
{
  /* FIXME */
  ASSERT(0);
  return Value::null();
}

static Value cvdayTimeDuration(Environment *env, List<Value> &args)
{
  /* FIXME */
  ASSERT(0);
  return Value::null();
}

static Value cvuntypedAtomic(Environment *env, List<Value> &args)
{
  /* FIXME */
  ASSERT(0);
  return Value::null();
}




FunctionDefinition TypeConversion_fundefs[44] = {
  { cvstring,       XSNS, "string",       "xdt:anyAtomicType?", "xsd:string?",       false },
  { cvboolean,      XSNS, "boolean",      "xdt:anyAtomicType?", "xsd:boolean?",      false },
  { cvdecimal,      XSNS, "decimal",      "xdt:anyAtomicType?", "xsd:decimal?",      false },
  { cvfloat,        XSNS, "float",        "xdt:anyAtomicType?", "xsd:float?",        false },
  { cvdouble,       XSNS, "double",       "xdt:anyAtomicType?", "xsd:double?",       false },
  { cvduration,     XSNS, "duration",     "xdt:anyAtomicType?", "xsd:duration?",     false },
  { cvdateTime,     XSNS, "dateTime",     "xdt:anyAtomicType?", "xsd:dateTime?",     false },
  { cvtime,         XSNS, "time",         "xdt:anyAtomicType?", "xsd:time?",         false },
  { cvdate,         XSNS, "date",         "xdt:anyAtomicType?", "xsd:date?",         false },
  { cvgYearMonth,   XSNS, "gYearMonth",   "xdt:anyAtomicType?", "xsd:gYearMonth?",   false },
  { cvgYear,        XSNS, "gYear",        "xdt:anyAtomicType?", "xsd:gYear?",        false },
  { cvgMonthDay,    XSNS, "gMonthDay",    "xdt:anyAtomicType?", "xsd:gMonthDay?",    false },
  { cvgDay,         XSNS, "gDay",         "xdt:anyAtomicType?", "xsd:gDay?",         false },
  { cvgMonth,       XSNS, "gMonth",       "xdt:anyAtomicType?", "xsd:gMonth?",       false },
  { cvhexBinary,    XSNS, "hexBinary",    "xdt:anyAtomicType?", "xsd:hexBinary?",    false },
  { cvbase64Binary, XSNS, "base64Binary", "xdt:anyAtomicType?", "xsd:base64Binary?", false },
  { cvanyURI,       XSNS, "anyURI",       "xdt:anyAtomicType?", "xsd:anyURI?",       false },
  { cvQName,        XSNS, "QName",        "xsd:string",          "xsd:QName",         false },
  { cvnormalizedString, XSNS, "normalizedString", "xdt:anyAtomicType?",
                                                            "xsd:normalizedString?", false },
  { cvtoken,        XSNS, "token",        "xdt:anyAtomicType?", "xsd:token?",        false },
  { cvlanguage,     XSNS, "language",     "xdt:anyAtomicType?", "xsd:language?",     false },
  { cvNMTOKEN,      XSNS, "NMTOKEN",      "xdt:anyAtomicType?", "xsd:NMTOKEN?",      false },
  { cvName,         XSNS, "Name",         "xdt:anyAtomicType?", "xsd:Name?",         false },
  { cvNCName,       XSNS, "NCName",       "xdt:anyAtomicType?", "xsd:NCName?",       false },
  { cvID,           XSNS, "ID",           "xdt:anyAtomicType?", "xsd:ID?",           false },
  { cvIDREF,        XSNS, "IDREF",        "xdt:anyAtomicType?", "xsd:IDREF?",        false },
  { cvENTITY,       XSNS, "ENTITY",       "xdt:anyAtomicType?", "xsd:ENTITY?",       false },
  { cvinteger,      XSNS, "integer",      "xdt:anyAtomicType?", "xsd:integer?",      false },
  { cvnonPositiveInteger, XSNS, "nonPositiveInteger", "xdt:anyAtomicType?",
                                                          "xsd:nonPositiveInteger?", false },
  { cvnegativeInteger, XSNS, "negativeInteger", "xdt:anyAtomicType?",
                                                             "xsd:negativeInteger?", false },
  { cvlong,         XSNS, "long",         "xdt:anyAtomicType?", "xsd:long?",         false },
  { cvint,          XSNS, "int",          "xdt:anyAtomicType?", "xsd:int?",          false },
  { cvshort,        XSNS, "short",        "xdt:anyAtomicType?", "xsd:short?",        false },
  { cvbyte,         XSNS, "byte",         "xdt:anyAtomicType?", "xsd:byte?",         false },
  { cvnonNegativeInteger, XSNS, "nonNegativeInteger", "xdt:anyAtomicType?",
                                                          "xsd:nonNegativeInteger?", false },
  { cvunsignedLong, XSNS, "unsignedLong", "xdt:anyAtomicType?", "xsd:unsignedLong?", false },
  { cvunsignedInt,  XSNS, "unsignedInt",  "xdt:anyAtomicType?", "xsd:unsignedInt?",  false },
  { cvunsignedShort,XSNS, "unsignedShort","xdt:anyAtomicType?", "xsd:unsignedShort?",false },
  { cvunsignedByte, XSNS, "unsignedByte", "xdt:anyAtomicType?", "xsd:unsignedByte?", false },
  { cvpositiveInteger, XSNS, "positiveInteger", "xdt:anyAtomicType?",
                                                             "xsd:positiveInteger?", false },
  { cvyearMonthDuration, XSNS, "yearMonthDuration", "xdt:anyAtomicType?",
                                                          "xdt:yearMonthDuration?", false },
  { cvdayTimeDuration, XSNS, "dayTimeDuration", "xdt:anyAtomicType?",
                                                            "xdt:dayTimeDuration?", false },
  { cvuntypedAtomic, XSNS, "untypedAtomic", "xdt:anyAtomicType?",
                                                              "xdt:untypedAtomic?", false },
  { NULL },
};

FunctionDefinition *TypeConversion_module = TypeConversion_fundefs;
