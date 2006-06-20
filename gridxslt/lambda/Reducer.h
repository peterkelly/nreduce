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

  void clearPrinted(Cell *c);
  void numberCells(Cell *c);
  void outputDotContents(Cell *c, FILE *f, bool numbers);
  void outputDot(Cell *c, FILE *f, bool numbers);
  void outputTree(Cell *c, FILE *f);
  bool reduce(Cell *r);
  Cell *evaluate(Cell *r);
  void outputStep(List<Cell*> &stack, String msg);

/*   Cell *root; */

  int step;
  int reduction;
  bool trace;
  Cell *rt;
};

Graph *parseGraph(String s, Cell **rt);

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

  bool isRedex();

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
  static int m_numCells;
  static int m_maxCells;
  static int m_totalCells;
};

};

#endif /* _LAMBDA_REDUCER_H */
