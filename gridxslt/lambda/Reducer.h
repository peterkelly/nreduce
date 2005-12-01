#ifndef _LAMBDA_REDUCER_H
#define _LAMBDA_REDUCER_H

#define YYDEBUG 1

#include "util/String.h"
#include <stdio.h>

namespace GridXSLT {

class Cell;

typedef int (builtinfun)(Cell *c, List<Cell*> &args);

struct builtindef {
  char *name;
  builtinfun *fun;
  unsigned int nargs;
};

class Graph {
public:
  Graph();
  ~Graph();

  void clearPrinted();
  void numberCells();
  void outputDotContents(FILE *f, bool numbers);
  void outputDot(FILE *f, bool numbers);
  void outputTree(FILE *f);
  bool reduce(Cell *r);
  void outputStep(String msg);

  Cell *root;

  int step;
  int reduction;
  bool trace;
};

Graph *parseGraph(String s);

enum CellType {
  ML_INVALID     = 0,
  ML_APPLICATION = 1,
  // things that can be applied
  ML_LAMBDA      = 2,
  ML_BUILTIN     = 3,
  // data objects
  ML_CONS        = 4,
  ML_CONSTANT    = 5,
  ML_NIL         = 6,
  ML_VARIABLE    = 7
};

class Cell : public Shared<Cell> {
public:

  Cell();
  ~Cell();
  void clear();

  Cell *instantiate(String varname, Cell *varval);

  void setApplication(Cell *left, Cell *right);
  void setLambda(String varname, Cell *body);
  void setBuiltin(String bname);
  void setCons(Cell *left, Cell *right);
  void setConstant(String value);
  void setNil();
  void setVariable(String varname);
  void replace(Cell *source);

  void printDot(FILE *f, bool numbers);
  void printTree(FILE *f, int indent);

  int num;

  CellType m_type;
  Cell *m_left;
  Cell *m_right;
  String m_value;

  bool m_current;
  bool m_active;
  bool m_printed;

  int m_x;
  int m_y;
  int m_span;
  int m_id;

  static int m_nextId;
};

};

#endif /* _LAMBDA_REDUCER_H */
