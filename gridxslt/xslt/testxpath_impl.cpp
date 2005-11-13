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
#include "util/Namespace.h"
#include "util/Debug.h"
#include "xmlschema/XMLSchema.h"
#include "Expression.h"
#include "Statement.h"
#include <stdio.h>
#include <argp.h>
#include <string.h>
#include <stdlib.h>

using namespace GridXSLT;

/* static const char *argp_program_version = */
/*   "testxpath 0.1"; */

static char doc[] =
  "XPath test program";

static char args_doc[] = "FILENAME";

static struct argp_option options[] = {
  {"tree",                  't', 0,      0,  "Print expression tree" },
  {"schema",                's', "FILE", 0,  "Read schema definitions from FILE" },
  {"normalized-seqtypes",   'n', 0,      0,  "Print normalized versions of sequence types" },
  {"expr",                  'e', "EXPR", 0,  "Parse EXPR specified on command-line instead of "
                                             "reading from file" },
  { 0 }
};

struct arguments {
  int tree;
  int normalized_seqtypes;
  char *schema;
  char *filename;
  char *expr;
};

static error_t parse_opt (int key, char *arg, struct argp_state *state)
{
  struct arguments *arguments = (struct arguments*)state->input;

  switch (key) {
  case 't':
    arguments->tree = 1;
    break;
  case 's':
    arguments->schema = arg;
    break;
  case 'n':
    arguments->normalized_seqtypes = 1;
    break;
  case 'e':
    arguments->expr = arg;
    break;
  case ARGP_KEY_ARG:
    if (1 <= state->arg_num)
      argp_usage (state);
    arguments->filename = arg;
    break;
  case ARGP_KEY_END:
    if ((1 > state->arg_num) && (NULL == arguments->expr))
      argp_usage (state);
    break;
  default:
    return ARGP_ERR_UNKNOWN;
  }
  return 0;
}

static struct argp argp = { options, parse_opt, args_doc, doc };

int testxpath_main(int argc, char **argv)
{
  struct arguments arguments;
  StringBuffer input;
  char buf[1025];
  Schema *s = NULL;
  XSLTSequenceExpr *stmt = new XSLTSequenceExpr(NULL);
  Expression *expr;
  Error ei;
  int r = 0;

  setbuf(stdout,NULL);

  memset(&arguments,0,sizeof(arguments));
  argp_parse (&argp, argc, argv, 0, 0, &arguments);

  xs_init();

  if (NULL == arguments.schema)
    s = new Schema(xs_g);
  else if (NULL == (s = parse_xmlschema_file(arguments.schema,xs_g)))
    return 1;

/*   stringbuf_format(input,"__just_expr "); */
  if (arguments.normalized_seqtypes)
    input.append("0 instance of ");


  if (NULL != arguments.filename) {
    FILE *in;
    int r;
    if (NULL == (in = fopen(arguments.filename,"r"))) {
      perror(arguments.filename);
      return 1;
    }

    while (0 < (r = fread(buf,1,1024,in))) {
      buf[r] = '\0';
      input.append(buf);
    }
    fclose(in);
  }
  else {
    ASSERT(NULL != arguments.expr);
    input.append(arguments.expr);
  }

  if (NULL == (expr = Expression_parse(input.contents(),arguments.filename,1,&ei,0))) {
    ei.fprint(stderr);
    ei.clear();
    return 1;
  }

  int importpred = 1;
  expr->setParent(stmt,&importpred);
  /* add default namespaces declared in schema */
  Iterator<ns_def*> nsit;
  for (nsit = xs_g->namespaces->defs; nsit.haveCurrent(); nsit++) {
    ns_def *ns = *nsit;
    stmt->m_namespaces->add_direct(ns->href,ns->prefix);
  }

  if (0 != expr->resolve(s,arguments.filename,&ei)) {
    ei.fprint(stderr);
    ei.clear();
    r = 1;
  }
  else {
    if (arguments.normalized_seqtypes) {
      StringBuffer buf;
      ASSERT(XPATH_INSTANCE_OF == expr->m_type);
      TypeExpr *iof = static_cast<TypeExpr*>(expr);
      ASSERT(!iof->m_st.isNull());
      iof->m_st.printFS(buf,xs_g->namespaces,false);
      message("%*\n",&buf);
    }
    else {
      message("%*\n",expr);
    }

    if (arguments.tree)
      expr->printTree(0);
  }

  delete expr;

  delete stmt;
  delete s;
  xs_cleanup();

  return r;
}
