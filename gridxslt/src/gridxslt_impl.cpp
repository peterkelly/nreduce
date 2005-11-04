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
 */

#include "xslt/Parse.h"
#include "xslt/Statement.h"
#include "util/XMLUtils.h"
#include "util/Macros.h"
#include "util/Debug.h"
#include "http/HTTP.h"
#include "dataflow/Program.h"
#include "dataflow/SequenceType.h"
#include "dataflow/Engine.h"
#include "dataflow/Serialization.h"
#include "xslt/Compilation.h"
#include <stdio.h>
#include <string.h>
#include <argp.h>
#include <unistd.h>

using namespace GridXSLT;

extern FILE *yyin;
extern int lex_lineno;
int yyparse();

/* static const char *argp_program_version = */
/*   "gridxslt 0.1"; */

static char doc[] =
  "GridXSLT execution engine";

static char args_doc[] = "[INPUT] SOURCEFILE\n"
                         "-s DOCROOT\n";

static struct argp_option options[] = {
  {"dot",                      'd', "FILE",  0, "Output graph in dot format to FILE" },
  {"internal",                 'i', NULL,    0, "Include internal functions in output" },
  {"types",                    'y', NULL,    0, "Show port types in dot output" },
  {"anon-functions",           'a', NULL,    0, "Show anonymous functions separately" },
  {"df",                       'f', NULL,    0, "Just print compiled dataflow graph; do not "
                                                "execute" },
  {"trace",                    't', NULL,    0, "Enable tracing" },
  {"parse-tree",               'p', NULL,    0, "Dump parse tree" },
  {"server",                   's', NULL,    0, "Run as HTTP server" },
  { 0 }
};

struct arguments {
  char *filename;
  char *dot;
  bool internal;
  bool types;
  bool anonfuns;
  char *input;
  bool df;
  bool trace;
  bool print_tree;
  bool server;
};

static error_t parse_opt (int key, char *arg, struct argp_state *state)
{
  struct arguments *arguments = (struct arguments*)state->input;

  switch (key) {
  case 'd':
    arguments->dot = arg;
    break;
  case 'i':
    arguments->internal = true;
    break;
  case 'y':
    arguments->types = true;
    break;
  case 'a':
    arguments->anonfuns = true;
    break;
  case 'f':
    arguments->df = true;
    break;
  case 't':
    arguments->trace = true;
    break;
  case 'p':
    arguments->print_tree = true;
    break;
  case 's':
    arguments->server = true;
    break;
  case ARGP_KEY_ARG:
    if (0 == state->arg_num) {
      arguments->filename = arg;
    }
    else if (1 == state->arg_num) {
      arguments->input = arguments->filename;
      arguments->filename = arg;
    }
    else {
      argp_usage (state);
    }
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

static Value load_input_doc(Error *ei, const char *filename, list *space_decls)
{
  FILE *f;
  xmlDocPtr doc;
  xmlNodePtr root;
  Node *docnode;
  Node *rootelem;
  SequenceType doctype;
  Value context;

  if (NULL == (f = fopen(filename,"r"))) {
    error(ei,filename,-1,String::null(),"%s",strerror(errno));
    return Value::null();
  }

  if (NULL == (doc = xmlReadFd(fileno(f),NULL,NULL,0))) {
    fclose(f);
    error(ei,filename,-1,String::null(),"XML parse error");
    return Value::null();
  }
  fclose(f);

  if (NULL == (root = xmlDocGetRootElement(doc))) {
    error(ei,filename,-1,String::null(),"No root element");
    xmlFreeDoc(doc);
    return Value::null();
  }

  docnode = new Node(NODE_DOCUMENT);
  rootelem = Node::fromXMLNode(root);
  df_strip_spaces(rootelem,space_decls);
  docnode->addChild(rootelem);

  xmlFreeDoc(doc);
  return Value(docnode);
}

int gridxslt_main(int argc, char **argv)
{
  struct arguments arguments;
  int r = 0;
  Error ei;
  xslt_source *source = NULL;
  Program *program = NULL;

  setbuf(stdout,NULL);

  memset(&arguments,0,sizeof(arguments));
  argp_parse (&argp, argc, argv, 0, 0, &arguments);

  xs_init();

  if (arguments.server)
    return http_main(arguments.filename);

  if (NULL != arguments.dot)
    unlink(arguments.dot);

  if (0 != xslt_parse(&ei,arguments.filename,&source)) {
    ei.fprint(stderr);
    ei.clear();
    return 1;
  }

  if (arguments.print_tree)
    source->root->printTree(0);

  if (0 != xslt_compile(&ei,source,&program)) {
    ei.fprint(stderr);
    ei.clear();
    xslt_source_free(source);
    return 1;
  }

  if (NULL != arguments.dot) {
    FILE *dotfile;
    if (NULL == (dotfile = fopen(arguments.dot,"w"))) {
      perror(arguments.dot);
      return 1;
    }
    program->outputDot(dotfile,arguments.types,arguments.internal,arguments.anonfuns);
    fclose(dotfile);
  }

  if (arguments.df) {
    program->outputDF(stdout,arguments.internal);
  }
  else {

    Value context;

    if ((NULL != arguments.input) &&
        ((context = load_input_doc(&ei,arguments.input,program->m_space_decls)).isNull())) {
      ei.fprint(stderr);
      ei.clear();
      r = 1;
    }
    else if (0 != df_execute(program,arguments.trace,&ei,context)) {
      ei.fprint(stderr);
      ei.clear();
      r = 1;
    }
  }

  delete program;
  xslt_source_free(source);
  ValueImpl::printRemaining();
  xs_cleanup();

  return r;
}
