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

#include "util/String.h"
#include "Reducer.h"
#include <argp.h>
#include <string.h>
#include <errno.h>

//#define STREAMING

using namespace GridXSLT;

static char doc[] =
  "ml";

static char args_doc[] = "FILENAME\n";

static struct argp_option options[] = {
  {"dot",                      'd', "FILE",  0, "Output graph in dot format to FILE" },
  {"trace",                    't', NULL,    0, "Enable tracing" },
  { 0 }
};

struct arguments {
  char *filename;
  char *dot;
  bool trace;
};

static error_t parse_opt (int key, char *arg, struct argp_state *state)
{
  struct arguments *arguments = (struct arguments*)state->input;

  switch (key) {
  case 'd':
    arguments->dot = arg;
    break;
  case 't':
    arguments->trace = true;
    break;
  case ARGP_KEY_ARG:
    if (0 == state->arg_num)
      arguments->filename = arg;
    else
      argp_usage (state);
    break;
  case ARGP_KEY_END:
    if (1 > state->arg_num)
      argp_usage (state);
    break;
  default:
    return ARGP_ERR_UNKNOWN;
  }
  return 0;
}

static struct argp argp = { options, parse_opt, args_doc, doc };

extern int mldebug;

static void streamingEvaluate(Graph *g, Cell *c)
{
  c = g->evaluate(c);
  int cs = 0;
  if (ML_NIL == c->m_type) {
    c->deref();
    cs = 1;
  }
  else if (ML_CONSTANT == c->m_type) {
    message("%*",&c->m_value);
    c->deref();
    cs = 2;
  }
  else if (ML_CONS == c->m_type) {
    Cell *left = c->m_left->ref();
    Cell *right = c->m_right->ref();
    c->deref();
    streamingEvaluate(g,left);
    streamingEvaluate(g,right);
    cs = 3;
  }
  else {
    g->outputTree(c,stdout);
    c->deref();
    cs = 4;
  }
}

static void normalEvaluate(Graph *g, Cell *c)
{
  c = g->evaluate(c);
  if (ML_CONS == c->m_type) {
    normalEvaluate(g,c->m_left);
    normalEvaluate(g,c->m_right);
  }
}

static void solidify(Graph *g, Cell *c)
{
  g->rt = c;
  normalEvaluate(g,c);
}

int main(int argc, char **argv)
{
  struct arguments arguments;
  Cell *root = NULL;

  setbuf(stdout,NULL);
//  mldebug = 1;

  memset(&arguments,0,sizeof(arguments));
  argp_parse (&argp, argc, argv, 0, 0, &arguments);

  char buf[1024];
  int r;
  FILE *f;
  if (NULL == (f = fopen(arguments.filename,"r"))) {
    perror(arguments.dot);
    return 1;
  }

  stringbuf *input = stringbuf_new();
  while (0 < (r = fread(buf,1,1024,f)))
    stringbuf_append(input,buf,r);

  fclose(f);

  String code(input->data);
  stringbuf_free(input);

  Graph *g = parseGraph(code,&root);

  if (NULL == g)
    return 1;

  if (NULL != arguments.dot) {
    FILE *dotfile;
    if (NULL == (dotfile = fopen(arguments.dot,"w"))) {
      perror(arguments.dot);
      return 1;
    }
    g->outputDot(root,dotfile,true);
    fclose(dotfile);
  }

//   g->outputTree(stdout);
//   message("---------------\n");
  g->trace = arguments.trace;
  g->numberCells(root);
//   while (g->reduce(g->root)) {
//     // keep reducing
//   }



#ifdef STREAMING
  streamingEvaluate(g,root);
#else


  g->rt = root;
  List<Cell*> args;
  g->outputStep(args," - start");
  solidify(g,root);
  g->outputTree(root,stdout);
  root->deref();

#endif


  delete g;

//   message("numCells = %d\n",Cell::m_numCells);
//   message("maxCells = %d\n",Cell::m_maxCells);
//   message("totalCells = %d\n",Cell::m_totalCells);

  return 0;
}

