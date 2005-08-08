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

#include "util/stringbuf.h"
#include "util/namespace.h"
#include "xmlschema/xmlschema.h"
#include "xpath.h"
#include "xslt.h"
#include <stdio.h>
#include <argp.h>
#include <string.h>
#include <stdlib.h>
#include <assert.h>

const char *argp_program_version =
  "testxpath 0.1";

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

error_t parse_opt (int key, char *arg, struct argp_state *state)
{
  struct arguments *arguments = state->input;

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

int main(int argc, char **argv)
{
  struct arguments arguments;
  stringbuf *exprstr;
  stringbuf *input;
  char buf[1024];
  xs_globals *g = xs_globals_new();
  xs_schema *s = NULL;
  xl_snode *stmt = xl_snode_new(XSLT_INSTR_SEQUENCE);
  xp_expr *expr;
  error_info ei;
  list *l;

  setbuf(stdout,NULL);

  memset(&arguments,0,sizeof(arguments));
  argp_parse (&argp, argc, argv, 0, 0, &arguments);

  memset(&ei,0,sizeof(error_info));

  if (NULL == arguments.schema)
    s = xs_schema_new(g);
  else if (NULL == (s = parse_xmlschema_file(arguments.schema,g)))
    exit(1);

  exprstr = stringbuf_new();
  input = stringbuf_new();

/*   stringbuf_printf(input,"__just_expr "); */
  if (arguments.normalized_seqtypes)
    stringbuf_printf(input,"0 instance of ");


  if (NULL != arguments.filename) {
    FILE *in;
    int r;
    if (NULL == (in = fopen(arguments.filename,"r"))) {
      perror(arguments.filename);
      exit(1);
    }

    while (0 < (r = fread(buf,1,1024,in)))
      stringbuf_append(input,buf,r);
    fclose(in);
  }
  else {
    assert(NULL != arguments.expr);
    stringbuf_printf(input,"%s",arguments.expr);
  }

  if (NULL == (expr = xp_expr_parse(input->data,arguments.filename,1,&ei))) {
    error_info_print(&ei,stderr);
    error_info_free_vals(&ei);
    exit(1);
  }

  xp_set_parent(expr,NULL,stmt);
  /* add default namespaces declared in schema */
  for (l = s->globals->namespaces->defs; l; l = l->next) {
    ns_def *ns = (ns_def*)l->data;
    ns_add(stmt->namespaces,ns->href,ns->prefix);
  }

  if (0 != xp_expr_resolve(expr,s,arguments.filename,&ei)) {
    error_info_print(&ei,stderr);
    error_info_free_vals(&ei);
    exit(1);
  }

  if (arguments.normalized_seqtypes) {
    stringbuf *buf = stringbuf_new();
    assert(XPATH_EXPR_INSTANCE_OF == expr->type);
    assert(NULL != expr->seqtype);
    df_seqtype_print_fs(buf,expr->seqtype,s->globals->namespaces->defs);
    printf("%s\n",buf->data);
    stringbuf_free(buf);
  }
  else {
    xp_expr_serialize(exprstr,expr,0);
    printf("%s\n",exprstr->data);
  }

  if (arguments.tree)
    xp_expr_print_tree(expr,0);
  xp_expr_free(expr);
  stringbuf_free(exprstr);
  stringbuf_free(input);

  xl_snode_free(stmt);
  xs_schema_free(s);
  xs_globals_free(g);

  return 0;
}
