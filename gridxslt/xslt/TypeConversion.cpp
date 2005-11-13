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
#include "TypeConversion.h"
#include <math.h>
#include <stdlib.h>
#include <errno.h>

using namespace GridXSLT;

#define XSNS XS_NAMESPACE
#define XDTNS XDT_NAMESPACE

// See conversion table in XQuery/XPath functions and operators section 17.1

// Note: when casting from a numeric type to itself, we create a new value even though it's the
// same type. This is because the argument may be a subtype and we want the cast operator to
// return the actual type itself

// FIXME: Here we just use the native C conversions for numeric values; the spec gives more detail
// about the conversion process. Should really confirm that we conform to these. The numeric
// coversions are marked as "done" for now, assuming that the compiler will take care of this.
// Should be right most cases.

// FIXME: we should probably raise an error when an attempt to cast from an invalid type is
// made... e.g. converting from one of the date types to a number

// FIXME: we also need to support constructor functions for user-defined datatypes. This will
// require special handling of functions since the set of available functions won't be known
// until runtime.

// FIXME: the spec gives detailed instructions for how to convert numeric types to strings...
// this is handled by printDouble() in SequenceType.cpp. I've marked theses as done but
// should really verify that we're following the spec exactly... there may be cases where C's
// printf() doesn't correctly format the numbers.


// @implements(xpath-functions:casting-1) @end
// @implements(xpath-functions:casting-3) @end

// @implements(xpath-functions:casting-from-primitive-to-primitive-1) @end
// @implements(xpath-functions:casting-from-primitive-to-primitive-2) @end
// @implements(xpath-functions:casting-from-primitive-to-primitive-3) @end
// @implements(xpath-functions:casting-from-primitive-to-primitive-4) @end
// @implements(xpath-functions:casting-from-primitive-to-primitive-5) @end
// @implements(xpath-functions:casting-from-primitive-to-primitive-6) @end
// @implements(xpath-functions:casting-from-primitive-to-primitive-7) @end
// @implements(xpath-functions:casting-from-primitive-to-primitive-8) @end
// @implements(xpath-functions:casting-from-primitive-to-primitive-9) @end

// @implements(xpath-functions:casting-to-binary-1) @end

// @implements(xpath-functions:casting-from-strings-1) @end
// @implements(xpath-functions:casting-from-strings-2) @end
// @implements(xpath-functions:casting-from-strings-3) @end
// @implements(xpath-functions:casting-from-strings-4) @end
// @implements(xpath-functions:casting-from-strings-5) @end

// @implements(xpath-functions:casting-to-string-1) @end
// @implements(xpath-functions:casting-to-string-2) @end
// @implements(xpath-functions:casting-to-string-3) @end
// @implements(xpath-functions:casting-to-string-4) @end
// @implements(xpath-functions:casting-to-string-5) @end

// @implements(xpath-functions:casting-to-durations-1) status { deferred } @end
// @implements(xpath-functions:casting-to-durations-2) status { deferred } @end
// @implements(xpath-functions:casting-to-durations-3) status { deferred } @end
// @implements(xpath-functions:casting-to-datetimes-1) status { deferred } @end
// @implements(xpath-functions:casting-to-datetimes-2) status { deferred } @end
// @implements(xpath-functions:casting-to-datetimes-3) status { deferred } @end
// @implements(xpath-functions:casting-to-datetimes-4) status { deferred } @end
// @implements(xpath-functions:casting-to-datetimes-5) status { deferred } @end
// @implements(xpath-functions:casting-to-datetimes-6) status { deferred } @end
// @implements(xpath-functions:casting-to-datetimes-7) status { deferred } @end
// @implements(xpath-functions:casting-to-datetimes-8) status { deferred } @end
// @implements(xpath-functions:casting-to-datetimes-9) status { deferred } @end

String GridXSLT::typestr(const SequenceType &st)
{
  if ((SEQTYPE_ITEM == st.type()) && (ITEM_ATOMIC == st.itemType()->m_kind) &&
      ((st.itemType()->m_type->def.ident.m_ns == XS_NAMESPACE) ||
       (st.itemType()->m_type->def.ident.m_ns == XDT_NAMESPACE))) {
    return st.itemType()->m_type->def.ident.m_name;
  }
  else {
    StringBuffer buf;
    SequenceType(st).print(buf);
    return buf;
  }
}

Value GridXSLT::invalidTypeConversion(Environment *env, const Value &source, const String &target)
{
  String st = typestr(source.type());
  error(env->ei,env->sloc.uri,env->sloc.line,"FORG0001","Cannot convert %* to %*",&st,&target);
  return Value::null();
}

Value GridXSLT::invalidValueConversion(Environment *env, const Value &source, const String &target)
{
  String st = typestr(source.type());
  error(env->ei,env->sloc.uri,env->sloc.line,"FOCA0002","Cannot convert %* %* to %*",
        &st,&source,&target);
  return Value::null();
}

Value GridXSLT::invalidString(Environment *env, const String &source, const String &target)
{
  error(env->ei,env->sloc.uri,env->sloc.line,"FORG0001",
        "The string \"%*\" cannot be cast to a %*",&source,&target);
  return Value::null();
}

static Value cvstring(Environment *env, List<Value> &args)
{
  // FIXME: check this more according to the spec... the logic should actually be implemented
  // in convertToString()
  // see also cvuntypedAtomic()
  return args[0].convertToString();
}

static Value cvboolean(Environment *env, List<Value> &args)
{
  // @implements(xpath-functions:casting-boolean-1) @end
  // @implements(xpath-functions:casting-boolean-2) @end

  if (args[0].isBoolean())
    return args[0].asBoolean();

  if (args[0].isFloat())
    return !((0.0 == args[0].asFloat()) || isnan(args[0].asFloat()));

  if (args[0].isDouble())
    return !((0.0 == args[0].asDouble()) || isnan(args[0].asDouble()));

  if (args[0].isDecimal())
    return !((0.0 == args[0].asDecimal()) || isnan(args[0].asDecimal()));

  if (args[0].isInteger())
    return !(0 == args[0].asInteger());

  if (args[0].isString() || args[0].isUntypedAtomic()) {
    String str;
    if (args[0].isString())
      str = args[0].asString().collapseWhitespace();
    else
      str = args[0].asUntypedAtomic().collapseWhitespace();
    if (("true" == str) || ("1" == str))
      return true;
    else if (("false" == str) || ("0" == str))
      return false;
    else
      return invalidString(env,str,"boolean");
  }

  return invalidTypeConversion(env,args[0],"boolean");
}

static Value cvdecimal(Environment *env, List<Value> &args)
{
  // @implements(xpath-functions:casting-to-numerics-3) @end

  if (args[0].isNumeric() && !args[0].isInteger()) {
    double d;
    if (args[0].isFloat())
      d = args[0].asFloat();
    else if (args[0].isDouble())
      d = args[0].asDouble();
    else 
      d = args[0].asDecimal();
    if (isinf(d) || isnan(d))
      return invalidValueConversion(env,args[0],"decimal");
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

  if (args[0].isString() || args[0].isUntypedAtomic()) {
    String str;
    if (args[0].isString())
      str = args[0].asString().collapseWhitespace();
    else
      str = args[0].asUntypedAtomic().collapseWhitespace();

    if (0 == str.length())
      return invalidString(env,str,"decimal");

    char *s = str.cstring();
    char *end = NULL;
    errno = 0;
    double d = strtod(s,&end);
    Value v;
    if ('\0' != *end)
      invalidString(env,str,"decimal");
    else if (0 != errno)
      error(env->ei,env->sloc.uri,env->sloc.line,"FOCA0003","input value too large for decimal");
    else
      v = Value::decimal(d);
    free(s);
    return v;
  }

  return invalidTypeConversion(env,args[0],"decimal");
}

static Value cvfloat(Environment *env, List<Value> &args)
{
  // @implements(xpath-functions:casting-to-numerics-1) @end

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

  if (args[0].isString() || args[0].isUntypedAtomic()) {
    String str;
    if (args[0].isString())
      str = args[0].asString().collapseWhitespace();
    else
      str = args[0].asUntypedAtomic().collapseWhitespace();

    if (0 == str.length())
      return invalidString(env,str,"float");

    char *s = str.cstring();
    char *end = NULL;
    errno = 0;
    float f = strtof(s,&end);
    Value v;
    if ('\0' != *end)
      invalidString(env,str,"float");
    else if (0 != errno)
      error(env->ei,env->sloc.uri,env->sloc.line,"FOCA0003","input value too large for float");
    else
      v = Value(f);
    free(s);
    return v;
  }

  return invalidTypeConversion(env,args[0],"float");
}

static Value cvdouble(Environment *env, List<Value> &args)
{
  // @implements(xpath-functions:casting-to-numerics-2) @end

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

  if (args[0].isString() || args[0].isUntypedAtomic()) {
    String str;
    if (args[0].isString())
      str = args[0].asString().collapseWhitespace();
    else
      str = args[0].asUntypedAtomic().collapseWhitespace();

    if (0 == str.length())
      return invalidString(env,str,"double");

    char *s = str.cstring();
    char *end = NULL;
    errno = 0;
    double d = strtod(s,&end);
    Value v;
    if ('\0' != *end)
      invalidString(env,str,"double");
    else if (0 != errno)
      error(env->ei,env->sloc.uri,env->sloc.line,"FOCA0003","input value too large for double");
    else
      v = Value(d);
    free(s);
    return v;
  }

  return invalidTypeConversion(env,args[0],"double");
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
  if (args[0].isString() || args[0].isUntypedAtomic()) {
    String str;
    if (args[0].isString())
      str = args[0].asString().collapseWhitespace();
    else
      str = args[0].asUntypedAtomic().collapseWhitespace();
    BinaryArray arr = BinaryArray::fromHex(str);
    if (arr.isNull()) {
      error(env->ei,env->sloc.uri,env->sloc.line,"FORG0001","invalid hex string");
      return Value::null();
    }
    return Value::hexBinary(arr);
  }

  if (args[0].isHexBinary())
    return Value::hexBinary(args[0].asHexBinary());

  if (args[0].isBase64Binary())
    return Value::hexBinary(args[0].asBase64Binary());

  return invalidTypeConversion(env,args[0],"hexBinary");
}

static Value cvbase64Binary(Environment *env, List<Value> &args)
{
  if (args[0].isString() || args[0].isUntypedAtomic()) {
    String str;
    if (args[0].isString())
      str = args[0].asString().collapseWhitespace();
    else
      str = args[0].asUntypedAtomic().collapseWhitespace();
    BinaryArray arr = BinaryArray::fromBase64(str);
    if (arr.isNull()) {
      error(env->ei,env->sloc.uri,env->sloc.line,"FORG0001","invalid base64 string");
      return Value::null();
    }
    return Value::base64Binary(arr);
  }

  if (args[0].isHexBinary())
    return Value::base64Binary(args[0].asHexBinary());

  if (args[0].isBase64Binary())
    return Value::base64Binary(args[0].asBase64Binary());

  return invalidTypeConversion(env,args[0],"base64Binary");
}

static Value cvanyURI(Environment *env, List<Value> &args)
{
  // @implements(xpath-functions:casting-to-anyuri-1) @end
  // @implements(xpath-functions:casting-to-anyuri-2) @end
  // @implements(xpath-functions:casting-to-anyuri-3) @end

  if (args[0].isString())
    return URI(args[0].asString().collapseWhitespace());

  if (args[0].isUntypedAtomic())
    return URI(args[0].asUntypedAtomic().collapseWhitespace());

  if (args[0].isAnyURI())
    return URI(args[0].asAnyURI());

  return invalidTypeConversion(env,args[0],"anyURI");
}

static Value cvQName(Environment *env, List<Value> &args)
{
  /* FIXME */
  ASSERT(0);
  return Value::null();
}

// static Value cvnormalizedString(Environment *env, List<Value> &args)
// {
//   /* FIXME */
//   ASSERT(0);
//   return Value::null();
// }

// static Value cvtoken(Environment *env, List<Value> &args)
// {
//   /* FIXME */
//   ASSERT(0);
//   return Value::null();
// }

// static Value cvlanguage(Environment *env, List<Value> &args)
// {
//   /* FIXME */
//   ASSERT(0);
//   return Value::null();
// }

// static Value cvNMTOKEN(Environment *env, List<Value> &args)
// {
//   /* FIXME */
//   ASSERT(0);
//   return Value::null();
// }

// static Value cvName(Environment *env, List<Value> &args)
// {
//   /* FIXME */
//   ASSERT(0);
//   return Value::null();
// }

// static Value cvNCName(Environment *env, List<Value> &args)
// {
//   /* FIXME */
//   ASSERT(0);
//   return Value::null();
// }

// static Value cvID(Environment *env, List<Value> &args)
// {
//   /* FIXME */
//   ASSERT(0);
//   return Value::null();
// }

// static Value cvIDREF(Environment *env, List<Value> &args)
// {
//   /* FIXME */
//   ASSERT(0);
//   return Value::null();
// }

// static Value cvENTITY(Environment *env, List<Value> &args)
// {
//   /* FIXME */
//   ASSERT(0);
//   return Value::null();
// }

static Value cvinteger(Environment *env, List<Value> &args)
{
  // @implements(xpath-functions:casting-to-numerics-4) @end

  if (args[0].isNumeric() && !args[0].isInteger()) {
    double d;
    if (args[0].isFloat())
      d = args[0].asFloat();
    else if (args[0].isDouble())
      d = args[0].asDouble();
    else 
      d = args[0].asDecimal();
    if (isinf(d) || isnan(d))
      return invalidValueConversion(env,args[0],"integer");
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

  if (args[0].isString() || args[0].isUntypedAtomic()) {
    String str;
    if (args[0].isString())
      str = args[0].asString().collapseWhitespace();
    else
      str = args[0].asUntypedAtomic().collapseWhitespace();

    if (0 == str.length())
      return invalidString(env,str,"integer");

    char *s = str.cstring();
    char *end = NULL;
    errno = 0;
    int i = strtol(s,&end,10);
    Value v;
    if ('\0' != *end)
      invalidString(env,str,"integer");
    else if (0 != errno)
      error(env->ei,env->sloc.uri,env->sloc.line,"FOCA0003","input value too large for integer");
    else
      v = Value(i);
    free(s);
    return v;
  }

  return invalidTypeConversion(env,args[0],"integer");
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
  String str = args[0].convertToString();
  return Value::untypedAtomic(str);
}




FunctionDefinition TypeConversion_fundefs[35] = {
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
//   { cvnormalizedString, XSNS, "normalizedString", "xdt:anyAtomicType?",
//                                                             "xsd:normalizedString?", false },
//   { cvtoken,        XSNS, "token",        "xdt:anyAtomicType?", "xsd:token?",        false },
//   { cvlanguage,     XSNS, "language",     "xdt:anyAtomicType?", "xsd:language?",     false },
//   { cvNMTOKEN,      XSNS, "NMTOKEN",      "xdt:anyAtomicType?", "xsd:NMTOKEN?",      false },
//   { cvName,         XSNS, "Name",         "xdt:anyAtomicType?", "xsd:Name?",         false },
//   { cvNCName,       XSNS, "NCName",       "xdt:anyAtomicType?", "xsd:NCName?",       false },
//   { cvID,           XSNS, "ID",           "xdt:anyAtomicType?", "xsd:ID?",           false },
//   { cvIDREF,        XSNS, "IDREF",        "xdt:anyAtomicType?", "xsd:IDREF?",        false },
//   { cvENTITY,       XSNS, "ENTITY",       "xdt:anyAtomicType?", "xsd:ENTITY?",       false },
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
  { cvuntypedAtomic, XDTNS, "untypedAtomic", "xdt:anyAtomicType?",
                                                              "xdt:untypedAtomic?", false },
  { NULL },
};

FunctionDefinition *TypeConversion_module = TypeConversion_fundefs;
