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

#include "parse.h"
#include "xslt.h"
#include "util/xmlutils.h"
#include "util/macros.h"
#include "util/debug.h"
#include "dataflow/dataflow.h"
#include "dataflow/sequencetype.h"
#include "dataflow/engine.h"
#include "xslt/compile.h"
#include <stdio.h>
#include <assert.h>
#include <string.h>
#include <argp.h>
#include <unistd.h>

extern FILE *yyin;
extern int lex_lineno;
int yyparse();

int ends_with(char *filename, char *ext)
{
  return ((strlen(ext) <= strlen(filename)) &&
          (!strcasecmp(filename+strlen(filename)-strlen(ext),ext)));
}

const char *argp_program_version =
  "dftest 0.1";

static char doc[] =
  "Dataflow execution engine";

static char args_doc[] = "FILENAME1 FILENAME2 ...";

static struct argp_option options[] = {
  {"dot",                      'd', "FILE",  0, "Output graph in dot format to FILE" },
  {"input",                    'i', "FILE",  0, "Input file" },
  {"df",                       'f', NULL,    0, "Just print compiled dataflow graph; do not "
                                                "execute" },
  {"trace",                    't', NULL,    0, "Enable tracing" },
  {"parse-tree",               's', NULL,    0, "Dump parse tree" },
  {"o",                        'o', NULL,    0, "Decompile functions" },
  { 0 }
};

struct arguments {
  list *filenames;
  char *dot;
  char *input;
  int df;
  int trace;
  int parse_tree;
  int decompile;
};

error_t parse_opt (int key, char *arg, struct argp_state *state)
{
  struct arguments *arguments = state->input;

  switch (key) {
  case 'd':
    arguments->dot = arg;
    break;
  case 'i':
    arguments->input = arg;
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
    arguments->parse_tree = 1;
    break;
  case ARGP_KEY_ARG:
    list_append(&arguments->filenames,arg);
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

static int add_file(df_state *state, xs_schema *schema, char *filename, int print_tree)
{
  FILE *f;
  error_info ei;
  int res = 0;
  xl_snode *tree = NULL;
  memset(&ei,0,sizeof(error_info));

  if (NULL == (f = fopen(filename,"r"))) {
    perror(filename);
    exit(1);
  }

  if (ends_with(filename,".xl")) {
    stringbuf *input = stringbuf_new();
    char buf[1024];
    int r;
    list *l;

    while (0 < (r = fread(buf,1,1024,f)))
      stringbuf_append(input,buf,r);

    if (NULL == (tree = xl_snode_parse(input->data,filename,0,&ei))) {
      error_info_print(&ei,stderr);
      error_info_free_vals(&ei);
      exit(1);
    }

    /* add default namespaces declared in schema */
    for (l = state->schema->globals->namespaces->defs; l; l = l->next) {
      ns_def *ns = (ns_def*)l->data;
      ns_add_direct(tree->namespaces,ns->href,ns->prefix);
    }

    /* temp - functions and operators namespace */
    ns_add_direct(tree->namespaces,FN_NAMESPACE,"fn");

    stringbuf_free(input);
  }
  else if (ends_with(filename,".xsl") ||
           ends_with(filename,".xslt") ||
           ends_with(filename,".xml")) {
    char *uri = get_real_uri(filename);
    tree = xl_snode_new(XSLT_TRANSFORM);
    if (0 != parse_xslt_relative_uri(&ei,NULL,0,NULL,uri,uri,tree)) {
      error_info_print(&ei,stderr);
      error_info_free_vals(&ei);
      xl_snode_free(tree);
      free(uri);
      exit(1);
    }
    free(uri);
  }

  if (0 != xl_snode_resolve(tree,schema,filename,&ei)) {
    error_info_print(&ei,stderr);
    exit(1);
  }

  if (print_tree)
    xl_snode_print_tree(tree,0);

  if (0 != df_program_from_xl(state,tree)) {
    res = 1;
  }
  xl_snode_free(tree);

  fclose(f);
  return res;
}

int main(int argc, char **argv)
{
  struct arguments arguments;
  int r = 0;
  error_info ei;
  xs_globals *globals = xs_globals_new();
  xs_schema *schema = xs_schema_new(globals);
  df_state *state = df_state_new(schema);
  list *l;

  setbuf(stdout,NULL);
  memset(&ei,0,sizeof(error_info));

  memset(&arguments,0,sizeof(arguments));
  argp_parse (&argp, argc, argv, 0, 0, &arguments);

  if (NULL != arguments.dot)
    unlink(arguments.dot);

  for (l = arguments.filenames; l; l = l->next) {
    char *filename = (char*)l->data;
    if (0 != add_file(state,schema,filename,arguments.parse_tree)) {
      r = 1;
      break;
    }
  }

  if (0 == r) {
    if (NULL != arguments.dot) {
      FILE *dotfile;
      if (NULL == (dotfile = fopen(arguments.dot,"w"))) {
        perror(arguments.dot);
        exit(1);
      }
      df_output_dot(state,dotfile);
      fclose(dotfile);
    }

    if (arguments.df) {
      df_output_df(state,stdout);
    }
    else {

      df_value *context = NULL;

      if (NULL != arguments.input) {
        FILE *f;
        xmlDocPtr doc;
        xmlNodePtr root;
        df_node *docnode;
        df_node *rootelem;
        df_seqtype *doctype;

        if (NULL == (f = fopen(arguments.input,"r"))) {
          fprintf(stderr,"Can't open %s: %s\n",arguments.input,strerror(errno));
          return -1; /* FIXME: raise an error here */
        }

        if (NULL == (doc = xmlReadFd(fileno(f),NULL,NULL,0))) {
          fclose(f);
          fprintf(stderr,"XML parse error.\n");
          return -1; /* FIXME: raise an error here */
        }
        fclose(f);

        if (NULL == (root = xmlDocGetRootElement(doc))) {
          fprintf(stderr,"No root element.\n");
          xmlFreeDoc(doc);
          return -1; /* FIXME: raise an error here */
        }

        docnode = df_node_new(NODE_DOCUMENT);
        rootelem = df_node_from_xmlnode(root);
        df_node_add_child(docnode,rootelem);

        doctype = df_seqtype_new_item(ITEM_DOCUMENT);
        context = df_value_new(doctype);
        df_seqtype_deref(doctype);
        context->value.n = docnode;
        context->value.n->refcount++;
/*         printf("context node is %p (type %d)\n",context->value.n,context->value.n->type); */

        xmlFreeDoc(doc);
      }

      if (0 != df_execute(state,arguments.trace,&ei,context)) {
        error_info_print(&ei,stderr);
        error_info_free_vals(&ei);
        r = 1;
      }

/*       if (NULL != context) */
/*         df_value_deref(state->schema->globals,context); */
    }
  }

  if (state)
    df_state_free(state);
  list_free(arguments.filenames,NULL);

  df_print_remaining(globals);

  xs_schema_free(schema);
  xs_globals_free(globals);

  return r;
}
