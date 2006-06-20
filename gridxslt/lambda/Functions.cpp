#include "util/String.h"
#include "util/Debug.h"
#include "dataflow/SequenceType.h"
#include "Reducer.h"
#include <stdlib.h>
#include <math.h>

using namespace GridXSLT;

static String numToString(double d)
{
  StringBuffer buf;
  printDouble(d,buf);
  return buf;
}

static void setBool(Cell *c, bool b)
{
  if (b)
    c->setConstant("true");
  else
    c->setNil();
}

static bool fromBool(Cell *c)
{
  return (ML_NIL != c->m_type);
}

#define CHECK_ARG(_arg,_type) { if ((_type) != (_arg)->m_type) { \
                                  fmessage(stderr,"Invalid argument: got %d, wanted %d\n", \
                                           (_arg)->m_type,(_type));     \
                                  return -1; \
                                } \
                              }

static int add(Cell *c, List<Cell*> &args)
{
  CHECK_ARG(args[0],ML_CONSTANT);
  CHECK_ARG(args[1],ML_CONSTANT);
  c->setConstant(numToString(args[0]->m_value.toDouble() + args[1]->m_value.toDouble()));
  return 0;
}

static int subtract(Cell *c, List<Cell*> &args)
{
  CHECK_ARG(args[0],ML_CONSTANT);
  CHECK_ARG(args[1],ML_CONSTANT);
  c->setConstant(numToString(args[0]->m_value.toDouble() - args[1]->m_value.toDouble()));
  return 0;
}

static int multiply(Cell *c, List<Cell*> &args)
{
  CHECK_ARG(args[0],ML_CONSTANT);
  CHECK_ARG(args[1],ML_CONSTANT);
  c->setConstant(numToString(args[0]->m_value.toDouble() * args[1]->m_value.toDouble()));
  return 0;
}

static int divide(Cell *c, List<Cell*> &args)
{
  CHECK_ARG(args[0],ML_CONSTANT);
  CHECK_ARG(args[1],ML_CONSTANT);
  c->setConstant(numToString(args[0]->m_value.toDouble() / args[1]->m_value.toDouble()));
  return 0;
}

static int mod(Cell *c, List<Cell*> &args)
{
  CHECK_ARG(args[0],ML_CONSTANT);
  CHECK_ARG(args[1],ML_CONSTANT);
  c->setConstant(numToString(fmod(args[0]->m_value.toDouble(),args[1]->m_value.toDouble())));
  return 0;
}

static int eq(Cell *c, List<Cell*> &args)
{
  CHECK_ARG(args[0],ML_CONSTANT);
  CHECK_ARG(args[1],ML_CONSTANT);
  setBool(c,args[0]->m_value.toDouble() == args[1]->m_value.toDouble());
  return 0;
}

static int neq(Cell *c, List<Cell*> &args)
{
  CHECK_ARG(args[0],ML_CONSTANT);
  CHECK_ARG(args[1],ML_CONSTANT);
  setBool(c,args[0]->m_value.toDouble() != args[1]->m_value.toDouble());
  return 0;
}

static int lt(Cell *c, List<Cell*> &args)
{
  CHECK_ARG(args[0],ML_CONSTANT);
  CHECK_ARG(args[1],ML_CONSTANT);
  setBool(c,args[0]->m_value.toDouble() < args[1]->m_value.toDouble());
  return 0;
}

static int le(Cell *c, List<Cell*> &args)
{
  CHECK_ARG(args[0],ML_CONSTANT);
  CHECK_ARG(args[1],ML_CONSTANT);
  setBool(c,args[0]->m_value.toDouble() <= args[1]->m_value.toDouble());
  return 0;
}

static int gt(Cell *c, List<Cell*> &args)
{
  CHECK_ARG(args[0],ML_CONSTANT);
  CHECK_ARG(args[1],ML_CONSTANT);
  setBool(c,args[0]->m_value.toDouble() > args[1]->m_value.toDouble());
  return 0;
}

static int ge(Cell *c, List<Cell*> &args)
{
  CHECK_ARG(args[0],ML_CONSTANT);
  CHECK_ARG(args[1],ML_CONSTANT);
  setBool(c,args[0]->m_value.toDouble() >= args[1]->m_value.toDouble());
  return 0;
}

static int band(Cell *c, List<Cell*> &args)
{
  setBool(c,fromBool(args[0]) && fromBool(args[1]));
  return 0;
}

static int bor(Cell *c, List<Cell*> &args)
{
  setBool(c,fromBool(args[0]) || fromBool(args[1]));
  return 0;
}

static int bif(Cell *c, List<Cell*> &args)
{
  String component;
  if (ML_NIL == args[0]->m_type) {
    component = "F";
  }
  else {
    component = "T";
  }

  ASSERT(ML_APPLICATION == c->m_type);
  c->clear();
  c->m_type = ML_LAMBDA;
  c->m_value = "T";
  c->m_left = (new Cell())->ref();
  c->m_left->m_type = ML_LAMBDA;
  c->m_left->m_value = "F";
  c->m_left->m_left = (new Cell())->ref();
  c->m_left->m_left->m_type = ML_VARIABLE;
  c->m_left->m_left->m_value = component;

  return 0;
}

static int ap(Cell *c, List<Cell*> &args)
{
  CHECK_ARG(args[0],ML_CONSTANT);
  CHECK_ARG(args[1],ML_CONSTANT);
  StringBuffer buf;
  buf.append(args[0]->m_value);
  buf.append(args[1]->m_value);
  c->setConstant(buf);
  return 0;
}

static int seq(Cell *c, List<Cell*> &args)
{
  CHECK_ARG(args[0],ML_CONSTANT);
  CHECK_ARG(args[1],ML_CONSTANT);
  setBool(c,args[0]->m_value == args[1]->m_value);
  return 0;
}

static int cons(Cell *c, List<Cell*> &args)
{
  c->setCons(args[0],args[1]);
  return 0;
}

static int head(Cell *c, List<Cell*> &args)
{
  CHECK_ARG(args[0],ML_CONS);
  c->replace(args[0]->m_left);
  return 0;
}

static int tail(Cell *c, List<Cell*> &args)
{
  CHECK_ARG(args[0],ML_CONS);
  c->replace(args[0]->m_right);
  return 0;
}

static int islambda(Cell *c, List<Cell*> &args)
{
  setBool(c,ML_LAMBDA == args[0]->m_type);
  return 0;
}

static int isvalue(Cell *c, List<Cell*> &args)
{
  setBool(c,ML_CONSTANT == args[0]->m_type);
  return 0;
}

static int iscons(Cell *c, List<Cell*> &args)
{
  setBool(c,ML_CONS == args[0]->m_type);
  return 0;
}

static int isnil(Cell *c, List<Cell*> &args)
{
  setBool(c,ML_NIL == args[0]->m_type);
  return 0;
}

builtindef builtin_functions1[24] = {
  { "+",           add,            2 },
  { "-",           subtract,       2 },
  { "*",           multiply,       2 },
  { "/",           divide,         2 },
  { "%",           mod,            2 },
  { "=",           eq,             2 },
  { "!=",          neq,            2 },
  { "<",           lt,             2 },
  { "<=",          le,             2 },
  { ">",           gt,             2 },
  { ">=",          ge,             2 },
  { "and",         band,           2 },
  { "or",          bor,            2 },
  { "if",          bif,            1 },
  { "ap",          ap,             2 },
  { "eq",          seq,            2 },
  { "cons",        cons,           2 },
  { "head",        head,           1 },
  { "tail",        tail,           1 },
  { "lambda?",     islambda,       1 },
  { "value?",      isvalue,        1 },
  { "cons?",       iscons,         1 },
  { "nil?",        isnil,          1 },
  { NULL,          NULL,           0 }
};

builtindef *builtin_functions = builtin_functions1;
