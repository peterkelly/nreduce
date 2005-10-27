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
#include "xslt/Statement.h"
#include "dataflow/serialization.h"
#include "xmlschema/xmlschema.h"
#include "util/stringbuf.h"

#include <stdio.h>
#include <assert.h>
#include <stdlib.h>
#include <math.h>
#include <string.h>
#include <argp.h>

using namespace GridXSLT;

/* static const char *argp_program_version = */
/*   "testxslt 0.1"; */

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

static error_t parse_opt (int key, char *arg, struct argp_state *state)
{
  struct arguments *arguments = (struct arguments*)state->input;

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

static void dump_output_defs(xslt_source *source)
{
  list *l;
  for (l = source->output_defs; l; l = l->next) {
    df_seroptions *options = (df_seroptions*)l->data;
    list *nl;
    if (options->m_ident.isNull())
      message("Output definition: (unnamed)\n");
    else
      message("Output definition: %*\n",&options->m_ident);

      message("  byte-order-mark = %s\n",options->m_byte_order_mark ? "yes" : "no");

      message("  cdata-section-elements = ");
      Iterator<NSName> cdsit;
      for (cdsit = options->m_cdata_section_elements; cdsit.haveCurrent(); cdsit++) {
        NSName nn = *cdsit;
        message("%*",&nn);
        if (nl->next)
          message("  ");
      }
      message("\n");

      if (options->m_doctype_public.isNull())
        message("  doctype-public = (none)\n");
      else
        message("  doctype-public = %*\n",&options->m_doctype_public);

      if (options->m_doctype_system.isNull())
        message("  doctype-system = (none)\n");
      else
        message("  doctype-system = %*\n",&options->m_doctype_system);

      if (options->m_encoding.isNull())
        message("  encoding = (none)\n");
      else
        message("  encoding = %*\n",&options->m_encoding);

      message("  escape-uri-attributes = %s\n",options->m_escape_uri_attributes ? "yes" : "no");

      message("  include-content-type = %s\n",options->m_include_content_type ? "yes" : "no");

      message("  indent = %s\n",options->m_indent ? "yes" : "no");

      if (options->m_media_type.isNull())
        message("  media-type = (none)\n");
      else
        message("  media-type = %*\n",&options->m_media_type);

      message("  method = %*\n",&options->m_method);

      if (options->m_normalization_form.isNull())
        message("  normalization-form = (none)\n");
      else
        message("  normalization-form = %*\n",&options->m_normalization_form);

      message("  omit-xml-declaration = %s\n",options->m_omit_xml_declaration ? "yes" : "no");

      if (STANDALONE_YES == options->m_standalone)
        message("  standalone = yes\n");
      else if (STANDALONE_NO == options->m_standalone)
        message("  standalone = no\n");
      else
        message("  standalone = omit\n");

      message("  undeclare-prefixes = %s\n",options->m_undeclare_prefixes ? "yes" : "no");

      message("  use-character-maps = ");
      Iterator<NSName> ucmit;
      for (ucmit = options->m_use_character_maps; ucmit.haveCurrent(); ucmit++) {
        NSName nn = *ucmit;
        message("%*",&nn);
        if (nl->next)
          message("  ");
      }
      message("\n");

      if (options->m_version.isNull())
        message("  version = (none)\n");
      else
        message("  version = %*\n",&options->m_version);


    message("\n");
  }
}

static void dump_space_decls(xslt_source *source)
{
  list *l;
  stringbuf *buf = stringbuf_new();
  for (l = source->space_decls; l; l = l->next) {
    space_decl *decl = (space_decl*)l->data;
    if (decl->nnt->wcns && decl->nnt->wcname)
      stringbuf_format(buf,"*");
    else if (decl->nnt->wcns)
      stringbuf_format(buf,"*:%*",&decl->nnt->nn.m_name);
    else if (decl->nnt->wcname)
      stringbuf_format(buf,"%*:*",&decl->nnt->nn.m_ns);
    else
      stringbuf_format(buf,"%*",&decl->nnt->nn);
    stringbuf_format(buf,"%#i",30-(buf->size-1));
    if (decl->preserve)
      stringbuf_format(buf,"preserve");
    else
      stringbuf_format(buf,"strip   ");
    stringbuf_format(buf," %d %f",decl->importpred,decl->priority);
    message("%s\n",buf->data);
    stringbuf_clear(buf);
  }
  stringbuf_free(buf);
}

int testxslt_main(int argc, char **argv)
{
  struct arguments arguments;
  Error ei;
  int r = 0;
  xslt_source *source;

  setbuf(stdout,NULL);

  memset(&arguments,0,sizeof(arguments));
  argp_parse (&argp, argc, argv, 0, 0, &arguments);

  if (0 != xslt_parse(&ei,arguments.filename,&source)) {
    ei.fprint(stderr);
    ei.clear();
    return 1;
  }

  if (arguments.tree)
    source->root->printTree(0);
  else if (arguments.output_defs)
    dump_output_defs(source);
  else if (arguments.space_decls)
    dump_space_decls(source);
  else
    output_xslt(stdout,source->root);

  xslt_source_free(source);

  return r;
}
