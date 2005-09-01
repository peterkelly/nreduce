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

#include "xslt/parse.h"
#include "xslt/xslt.h"
#include "util/xmlutils.h"
#include "util/macros.h"
#include "util/debug.h"
#include "dataflow/dataflow.h"
#include "dataflow/sequencetype.h"
#include "dataflow/engine.h"
#include "dataflow/serialization.h"
#include "xslt/compile.h"
#include <stdio.h>
#include <assert.h>
#include <string.h>
#include <argp.h>
#include <unistd.h>

extern FILE *yyin;
extern int lex_lineno;
int yyparse();

const char *argp_program_version =
  "gridxslt 0.1";

static char doc[] =
  "GridXSLT execution engine";

static char args_doc[] = "[INPUT] SOURCEFILE";

static struct argp_option options[] = {
  {"dot",                      'd', "FILE",  0, "Output graph in dot format to FILE" },
  {"df",                       'f', NULL,    0, "Just print compiled dataflow graph; do not "
                                                "execute" },
  {"trace",                    't', NULL,    0, "Enable tracing" },
  {"parse-tree",               's', NULL,    0, "Dump parse tree" },
  {"o",                        'o', NULL,    0, "Decompile functions" },
  { 0 }
};

struct arguments {
  char *filename;
  char *dot;
  char *input;
  int df;
  int trace;
  int print_tree;
  int decompile;
};

error_t parse_opt (int key, char *arg, struct argp_state *state)
{
  struct arguments *arguments = state->input;

  switch (key) {
  case 'd':
    arguments->dot = arg;
    break;
  case 'f':
    arguments->df = 1;
    break;
  case 't':
    arguments->trace = 1;
    break;
  case 'o':
    arguments->decompile = 1;
    break;
  case 's':
    arguments->print_tree = 1;
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

df_value *load_input_doc(error_info *ei, const char *filename, list *space_decls)
{
  FILE *f;
  xmlDocPtr doc;
  xmlNodePtr root;
  df_node *docnode;
  df_node *rootelem;
  df_seqtype *doctype;
  df_value *context;

  if (NULL == (f = fopen(filename,"r"))) {
    error(ei,filename,-1,NULL,"%s",strerror(errno));
    return NULL;
  }

  if (NULL == (doc = xmlReadFd(fileno(f),NULL,NULL,0))) {
    fclose(f);
    error(ei,filename,-1,NULL,"XML parse error");
    return NULL;
  }
  fclose(f);

  if (NULL == (root = xmlDocGetRootElement(doc))) {
    error(ei,filename,-1,NULL,"No root element");
    xmlFreeDoc(doc);
    return NULL;
  }

  docnode = df_node_new(NODE_DOCUMENT);
  rootelem = df_node_from_xmlnode(root);
  df_strip_spaces(rootelem,space_decls);
  df_node_add_child(docnode,rootelem);

  doctype = df_seqtype_new_item(ITEM_DOCUMENT);
  context = df_value_new(doctype);
  df_seqtype_deref(doctype);
  context->value.n = docnode;
  context->value.n->refcount++;

  xmlFreeDoc(doc);
  return context;
}

int main(int argc, char **argv)
{
  struct arguments arguments;
  int r = 0;
  error_info ei;
  xslt_source *source = NULL;
  df_program *program = NULL;

  setbuf(stdout,NULL);
  memset(&ei,0,sizeof(error_info));

  memset(&arguments,0,sizeof(arguments));
  argp_parse (&argp, argc, argv, 0, 0, &arguments);

  if (NULL != arguments.dot)
    unlink(arguments.dot);

  if (0 != xslt_parse(&ei,arguments.filename,&source)) {
    error_info_print(&ei,stderr);
    error_info_free_vals(&ei);
    return 1;
  }

  if (arguments.print_tree)
    xl_snode_print_tree(source->root,0);

  if (0 != xslt_compile(&ei,source,&program)) {
    error_info_print(&ei,stderr);
    error_info_free_vals(&ei);
    xslt_source_free(source);
    return 1;
  }

  if (NULL != arguments.dot) {
    FILE *dotfile;
    if (NULL == (dotfile = fopen(arguments.dot,"w"))) {
      perror(arguments.dot);
      exit(1);
    }
    df_output_dot(program,dotfile);
    fclose(dotfile);
  }

  if (arguments.df) {
    df_output_df(program,stdout);
  }
  else {

    df_value *context = NULL;

    if ((NULL != arguments.input) &&
        (NULL == (context = load_input_doc(&ei,arguments.input,program->space_decls)))) {
      error_info_print(&ei,stderr);
      error_info_free_vals(&ei);
      r = 1;
    }
    else if (0 != df_execute(program,arguments.trace,&ei,context)) {
      error_info_print(&ei,stderr);
      error_info_free_vals(&ei);
      r = 1;
    }
  }

  df_program_free(program);
  df_print_remaining(source->schema->globals);
  xslt_source_free(source);

  return r;
}
