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

#include "xslt/parse.h"
#include "xslt/output.h"
#include "xslt/xslt.h"
#include "dataflow/serialization.h"
#include "xmlschema/xmlschema.h"
#include "util/stringbuf.h"

#include <stdio.h>
#include <assert.h>
#include <stdlib.h>
#include <math.h>
#include <string.h>
#include <argp.h>

const char *argp_program_version =
  "testxslt 0.1";

static char doc[] =
  "XSLT syntax test program";

static char args_doc[] = "[INPUT] SOURCEFILE";

static struct argp_option options[] = {
  {"parse-tree",               't', NULL,    0, "Dump parse tree" },
  {"output-defs",              'o', NULL,    0, "Dump output definitions" },
  {"space-decls",              's', NULL,    0, "Dump strip/preserve space declarations" },
  { 0 }
};

struct arguments {
  char *filename;
  int tree;
  int output_defs;
  int space_decls;
};

error_t parse_opt (int key, char *arg, struct argp_state *state)
{
  struct arguments *arguments = state->input;

  switch (key) {
  case 't':
    arguments->tree = 1;
    break;
  case 'o':
    arguments->output_defs = 1;
    break;
  case 's':
    arguments->space_decls = 1;
    break;
  case ARGP_KEY_ARG:
    if (1 <= state->arg_num)
      argp_usage (state);
    arguments->filename = arg;
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

void dump_output_defs(xslt_source *source)
{
  list *l;
  for (l = source->output_defs; l; l = l->next) {
    df_seroptions *options = (df_seroptions*)l->data;
    list *nl;
    if (nsname_isnull(options->ident))
      print("Output definition: (unnamed)\n");
    else
      print("Output definition: %#n\n",options->ident);

      print("  byte-order-mark = %s\n",options->byte_order_mark ? "yes" : "no");

      print("  cdata-section-elements = ");
      for (nl = options->cdata_section_elements; nl; nl = nl->next) {
        print("%#n",(nsname*)nl->data);
        if (nl->next)
          print("  ");
      }
      print("\n");

      print("  doctype-public = %s\n",options->doctype_public ? options->doctype_public : "(none)");

      print("  doctype-system = %s\n",options->doctype_system ? options->doctype_system : "(none)");

      print("  encoding = %s\n",options->encoding ? options->encoding : "(none)");

      print("  escape-uri-attributes = %s\n",options->escape_uri_attributes ? "yes" : "no");

      print("  include-content-type = %s\n",options->include_content_type ? "yes" : "no");

      print("  indent = %s\n",options->indent ? "yes" : "no");

      print("  media-type = %s\n",options->media_type ? options->media_type : "(none)");

      print("  method = %#n\n",options->method);

      print("  normalization-form = %s\n",
            options->normalization_form ? options->normalization_form : "(none)");

      print("  omit-xml-declaration = %s\n",options->omit_xml_declaration ? "yes" : "no");

      if (STANDALONE_YES == options->standalone)
        print("  standalone = yes\n");
      else if (STANDALONE_NO == options->standalone)
        print("  standalone = no\n");
      else
        print("  standalone = omit\n");

      print("  undeclare-prefixes = %s\n",options->undeclare_prefixes ? "yes" : "no");

      print("  use-character-maps = ");
      for (nl = options->use_character_maps; nl; nl = nl->next) {
        print("%#n",(nsname*)nl->data);
        if (nl->next)
          print("  ");
      }
      print("\n");

      print("  version = %s\n",options->version ? options->version : "(none)");


    print("\n");
  }
}

void dump_space_decls(xslt_source *source)
{
  list *l;
  stringbuf *buf = stringbuf_new();
  for (l = source->space_decls; l; l = l->next) {
    space_decl *decl = (space_decl*)l->data;
    if (decl->nnt->wcns && decl->nnt->wcname)
      stringbuf_format(buf,"*");
    else if (decl->nnt->wcns)
      stringbuf_format(buf,"*:%s",decl->nnt->nn.name);
    else if (decl->nnt->wcname)
      stringbuf_format(buf,"%s:*",decl->nnt->nn.ns);
    else
      stringbuf_format(buf,"%#n",decl->nnt->nn);
    stringbuf_format(buf,"%#i",30-(buf->size-1));
    if (decl->preserve)
      stringbuf_format(buf,"preserve");
    else
      stringbuf_format(buf,"strip   ");
    stringbuf_format(buf," %d %f",decl->importpred,decl->priority);
    print("%s\n",buf->data);
    stringbuf_clear(buf);
  }
  stringbuf_free(buf);
}

int main(int argc, char **argv)
{
  struct arguments arguments;
  error_info ei;
  int r = 0;
  xslt_source *source;

  setbuf(stdout,NULL);
  memset(&ei,0,sizeof(error_info));

  memset(&arguments,0,sizeof(arguments));
  argp_parse (&argp, argc, argv, 0, 0, &arguments);

  if (0 != xslt_parse(&ei,arguments.filename,&source)) {
    error_info_print(&ei,stderr);
    error_info_free_vals(&ei);
    return 1;
  }

  if (arguments.tree)
    xl_snode_print_tree(source->root,0);
  else if (arguments.output_defs)
    dump_output_defs(source);
  else if (arguments.space_decls)
    dump_space_decls(source);
  else
    output_xslt(stdout,source->root);

  xslt_source_free(source);

  return r;
}
