#include "util/String.h"
#include "util/Debug.h"
#include "dataflow/SequenceType.h"
#include "Reducer.h"
#include <stdlib.h>
#include <math.h>

#define CELL_HEIGHT 50

using namespace GridXSLT;

extern FILE *mlin;
extern int lex_lineno;
int mlparse();

typedef struct yy_buffer_state *YY_BUFFER_STATE;
YY_BUFFER_STATE ml_scan_string(const char *str);
void ml_switch_to_buffer(YY_BUFFER_STATE new_buffer);
void ml_delete_buffer(YY_BUFFER_STATE buffer);
int mllex_destroy(void);

Cell *rootCell = NULL;

extern builtindef *builtin_functions;

Graph::Graph()
{
  step = 0;
  reduction = 0;
  trace = false;
}

Graph::~Graph()
{
  root->deref();
}

static void numberRecursive(Cell *c, int *num)
{
  ASSERT(c);
  c->num = (*num)++;
  switch (c->m_type) {
  case ML_APPLICATION:
    numberRecursive(c->m_left,num);
    numberRecursive(c->m_right,num);
    break;
  case ML_LAMBDA:
    numberRecursive(c->m_left,num);
    break;
  case ML_CONS:
    numberRecursive(c->m_left,num);
    numberRecursive(c->m_right,num);
    break;
  default:
    break;
  }
}

static void clearPrintedRecursive(Cell *c)
{
  ASSERT(c);
  c->m_printed = false;
  switch (c->m_type) {
  case ML_APPLICATION:
    clearPrintedRecursive(c->m_left);
    clearPrintedRecursive(c->m_right);
    break;
  case ML_LAMBDA:
    clearPrintedRecursive(c->m_left);
    break;
  case ML_CONS:
    clearPrintedRecursive(c->m_left);
    clearPrintedRecursive(c->m_right);
    break;
  default:
    break;
  }
}

void Graph::clearPrinted()
{
  clearPrintedRecursive(root);
}

void Graph::numberCells()
{
  int num = 1;
  numberRecursive(root,&num);
}

void Graph::outputDotContents(FILE *f, bool numbers)
{
  fmessage(f,"  node[shape=record,fontsize=10,ranksep=0.1,nodesep=0.1]\n");
  clearPrinted();
  root->printDot(f,numbers);
}

void Graph::outputDot(FILE *f, bool numbers)
{
  fmessage(f,"digraph {\n");
  outputDotContents(f,numbers);
  fmessage(f,"}");
}

void Graph::outputTree(FILE *f)
{
  clearPrinted();
  root->printTree(f,0);
}

bool Graph::reduce(Cell *r)
{
  reduction++;
  r->m_current = true;
//  outputStep(String::format(" - start reducing cell %d",r->num));
  debug("********* reduce(%d): r is:\n",r->num);
  // Unwind the spine
  List<Cell*> stack;
  Cell *c = r;
  while (ML_APPLICATION == c->m_type) {
    stack.push(c);
    c = c->m_left;
  }

  c->m_active = true;
  outputStep(String::format(" - found non-application cell %d",c->num));

  switch (c->m_type) {
  case ML_LAMBDA: {
    debug("lambda expression %*: has argument? %d\n",&c->m_value,!stack.isEmpty());
    if (stack.isEmpty()) { // i.e. c == r
      ASSERT(c == r);
      outputStep(String::format(" - lambda %* @%d has no arguments, leaving as-is",
                 &c->m_value,c->num));
      r->m_current = false;
      c->m_active = false;
      return false;
    }

    Cell *appl = stack.pop();
    ASSERT(ML_APPLICATION == appl->m_type);
    Cell *value = appl->m_right;

    String lambdaname = c->m_value;
//    outputStep(String::format(" - before instantiation of lambda %* (appl=%d)",
//               &lambdaname,appl->num));
    Cell *ins = c->m_left->instantiate(c->m_value,value);

    appl->replace(ins);
    outputStep(String::format(" - after instantiation of lambda %* (appl=%d)",
               &lambdaname,appl->num));
    ins->deref();

    return true;
  }
  case ML_BUILTIN: {
    builtindef *def;
    for (def = builtin_functions; def->fun; def++) {
      if (def->name == c->m_value)
        break;
    }
    if (!def) {
      fmessage(stderr,"Unknown builtin function: %*\n",&c->m_value);
      exit(1);
    }

    if (stack.count() >= def->nargs) {
      List<Cell*> args;
      Cell *appl = NULL;
      for (unsigned int i = 0; i < def->nargs; i++) {
        appl = stack.pop();
        ASSERT(ML_APPLICATION == appl->m_type);
        Cell *value = appl->m_right;
        while (reduce(value)) {
          // keep reducing
        }
        args.append(value);
      }
      ASSERT(appl);

      debug("calling builtin function %*\n",&c->m_value);
      if (def->fun(appl,args))
        exit(1);
    }

    // otherwise, application is in WHNF (but unapplied). STOP.

    return true;
  }
  default: // cons, constant, or variable
    if (!stack.isEmpty()) {
      fmessage(stderr,"Attempt to apply argument(s) to a data object (type %d)\n",c->m_type);
      if (ML_CONSTANT == c->m_type)
        fmessage(stderr,"Value: %*\n",&c->m_value);
      exit(1);
    }
    if (ML_VARIABLE == c->m_type) {
      fmessage(stderr,"Free variable: %*\n",&c->m_value);
      exit(1);
    }
    // otherwise, we are in WHNF. STOP.
  }
//  outputStep(String::format(" - done reducing cell %d",r->num));
  r->m_current = false;
  c->m_active = false;
  return false;
}

void Graph::outputStep(String msg)
{
  if (!trace) {
    step++;
    return;
  }

  message("Step %d%*\n",step,&msg);
  char filename[255];
  sprintf(filename,"steps/step%03d.dot",step);
  FILE *f;
  if (NULL == (f = fopen(filename,"w"))) {
    perror(filename);
    exit(1);
  }

  fmessage(f,"digraph {\n");
  fmessage(f,"  label=\"Step %d%*\";\n",step,&msg);
  outputDotContents(f,true);
  fmessage(f,"}");

  fclose(f);

  step++;
}

Graph *GridXSLT::parseGraph(String s)
{
  YY_BUFFER_STATE bufstate;
  int r;
  rootCell = NULL;

//   message("parseGraph(): s = \"%*\"\n",&s);

//   char *cstr = s.collapseWhitespace().cstring();
  char *cstr = s.cstring();
  bufstate = ml_scan_string(cstr);
  free(cstr);
  ml_switch_to_buffer(bufstate);

  r = mlparse();

  ml_delete_buffer(bufstate);
  mllex_destroy();

  if (0 != r)
    return NULL;

  Graph *g = new Graph();
  g->root = rootCell;
  return g;
}

Cell::Cell()
  : num(0),
    m_type(ML_INVALID), m_left(NULL), m_right(NULL),
    m_current(false), m_active(false), m_printed(false),
    m_x(0), m_y(0), m_span(0), m_id(m_nextId++)
{
}

Cell::~Cell()
{
  debug("Cell::~Cell() %p\n",this);
  clear();
}

void Cell::clear()
{
  switch (m_type) {
  case ML_APPLICATION:
    ASSERT(m_left);
    ASSERT(m_right);
    ASSERT(1 <= m_left->refCount());
    ASSERT(1 <= m_right->refCount());
    m_left->deref();
    m_right->deref();
    break;
  case ML_LAMBDA:
    ASSERT(m_left);
    ASSERT(1 <= m_left->refCount());
    m_left->deref();
    break;
  case ML_CONS:
    ASSERT(m_left);
    ASSERT(m_right);
    ASSERT(1 <= m_left->refCount());
    ASSERT(1 <= m_right->refCount());
    m_left->deref();
    m_right->deref();
    break;
  default:
    break;
  }
  m_type = ML_INVALID;
  m_value = String::null();
  m_left = NULL;
  m_right = NULL;
}

Cell *Cell::instantiate(String varname, Cell *varval)
{
  switch (m_type) {
  case ML_VARIABLE:
    if (m_value == varname)
      return varval->ref();
    else
      return this->ref();
  case ML_CONSTANT:
    return this->ref();
  case ML_NIL:
    return this->ref();
  case ML_BUILTIN:
    return this->ref();
  case ML_CONS: {
    Cell *copy = new Cell();
    copy->m_type = ML_CONS;
    copy->m_left = m_left->instantiate(varname,varval);
    copy->m_right = m_right->instantiate(varname,varval);
    return copy->ref();
  }
  case ML_APPLICATION: {
    Cell *copy = new Cell();
    copy->m_type = ML_APPLICATION;
    copy->m_left = m_left->instantiate(varname,varval);
    copy->m_right = m_right->instantiate(varname,varval);
    return copy->ref();
  }
  case ML_LAMBDA: {
    if (m_value == varname)
      return this->ref();

    Cell *copy = new Cell();
    copy->m_type = ML_LAMBDA;
    copy->m_value = m_value;
    copy->m_left = m_left->instantiate(varname,varval);
    return copy->ref();
  }
  default:
    ASSERT(!"invalid cell type");
    break;
  }
  ASSERT(0);
  return NULL;
}

void Cell::setApplication(Cell *_left, Cell *_right)
{
  _left->ref();
  _right->ref();

  clear();
  m_type = ML_APPLICATION;
  m_left = _left;
  m_right = _right;
}

void Cell::setLambda(String varname, Cell *body)
{
  clear();
  m_type = ML_LAMBDA;
  m_value = varname;
  m_left = body->ref();
}

void Cell::setBuiltin(String bname)
{
  clear();
  m_type = ML_BUILTIN;
  m_value = bname;
}

void Cell::setCons(Cell *_left, Cell *_right)
{
  _left->ref();
  _right->ref();

  clear();
  m_type = ML_CONS;
  m_left = _left;
  m_right = _right;
}

void Cell::setConstant(String _value)
{
  clear();
  m_type = ML_CONSTANT;
  m_value = _value;
}

void Cell::setNil()
{
  clear();
  m_type = ML_NIL;
}

void Cell::setVariable(String varname)
{
  clear();
  m_type = ML_VARIABLE;
  m_value = varname;
}

void Cell::replace(Cell *source)
{
  source->ref();

  debug("replacing %d (%d) from %d/%*\n",num,m_type,source->m_type,&source->m_value);
  clear();

  m_type = source->m_type;
  m_value = source->m_value;
  m_left = NULL;
  m_right = NULL;

  switch (source->m_type) {
  case ML_APPLICATION:
    m_left = source->m_left->ref();
    m_right = source->m_right->ref();
    break;
  case ML_LAMBDA:
    m_left = source->m_left->ref();
    break;
  case ML_CONS:
    m_left = source->m_left->ref();
    m_right = source->m_right->ref();
  default:
    break;
  }

  source->deref();
}

void Cell::printDot(FILE *f, bool numbers)
{
  if (m_printed)
    return;
  m_printed = true;
  String label;
  switch (m_type) {
  case ML_APPLICATION:
    label = "@";
    break;
  case ML_LAMBDA:
    label = String::format("L %*",&m_value);
    break;
  case ML_BUILTIN:
    label = m_value;
    break;
  case ML_CONS:
    label = ":";
    break;
  case ML_CONSTANT:
    label = m_value;
    break;
  case ML_NIL:
    label = "nil";
    break;
  case ML_VARIABLE:
    label = m_value;
    break;
  default:
    ASSERT(!"invalid cell type");
    break;
  }

  if (m_left) {
    m_left->printDot(f,numbers);
    fmessage(f,"  cell%05d:left -> cell%05d\n",m_id,m_left->m_id);
  }

  if (m_right) {
    m_right->printDot(f,numbers);
    fmessage(f,"  cell%05d:right -> cell%05d\n",m_id,m_right->m_id);
  }

  fmessage(f,"  cell%05d [label=\"{",m_id);
  if (numbers)
    fmessage(f,"[%d] ",num);
  fmessage(f,"%*|{<left>|<right>}}\"",&label);
  if (m_current)
    fmessage(f,",color=red,fillcolor=\"#FFC0C0\",style=filled");
  else if (m_active)
    fmessage(f,",color=blue,fillcolor=\"#C0C0FF\",style=filled");
  fmessage(f,"];\n");
}

void Cell::printTree(FILE *f, int indent)
{
  fmessage(f,"%#i",indent*2);
  if (m_printed) {
    fmessage(f,"%%%%%\n");
    return;
  }
  m_printed = true;
  switch (m_type) {
  case ML_APPLICATION:
    fmessage(f,"@\n");
    break;
  case ML_LAMBDA:
    fmessage(f,"L\n",&m_value);
    break;
  case ML_BUILTIN:
    fmessage(f,"%*\n",&m_value);
    break;
  case ML_CONS:
    fmessage(f,":\n");
    break;
  case ML_CONSTANT:
    fmessage(f,"%*\n",&m_value);
    break;
  case ML_NIL:
    fmessage(f,"nil\n");
    break;
  case ML_VARIABLE:
    fmessage(f,"%*\n",&m_value);
    break;
  default:
    ASSERT(!"invalid cell type");
    break;
  }

  if (m_left)
    m_left->printTree(f,indent+1);
  if (m_right)
    m_right->printTree(f,indent+1);
}

int Cell::m_nextId = 0;
