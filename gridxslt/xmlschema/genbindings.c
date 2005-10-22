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

#include "xmlschema.h"
#include "validation.h"

#include <stdio.h>
#include <assert.h>
#include <stdlib.h>
#include <math.h>
#include <string.h>
#include <signal.h>
#include <unistd.h>
#include <argp.h>
#include <sys/types.h>
#include <libxml/tree.h>

const char *argp_program_version =
  "genbindings 0.1";

static char doc[] =
  "XML Schema C binding generator";

static char args_doc[] = "SCHEMA";

static struct argp_option options[] = {
  {"comments",                  'c', 0,      0,  "Include comments describing each structure" },
  { 0 }
};

struct arguments {
  char *schema;
  int comments;
};

error_t parse_opt (int key, char *arg, struct argp_state *state)
{
  struct arguments *arguments = (struct arguments*)state->input;

  switch (key) {
  case 'c':
    arguments->comments = 1;
    break;
  case ARGP_KEY_ARG:
    if (0 == state->arg_num)
      arguments->schema = arg;
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

int main(int argc, char **argv)
{
  xs_schema *s;
  xs_globals *g = xs_globals_new();
  struct arguments arguments;
  xs_validator *v;

  setbuf(stdout,NULL);
  memset(&arguments,0,sizeof(struct arguments));
  argp_parse (&argp, argc, argv, 0, 0, &arguments);

  if (NULL == (s = parse_xmlschema_file(arguments.schema,g)))
    exit(1);
  v = xs_validator_new(s);
  xs_print_defs(s);

  xs_validator_free(v);
  xs_schema_free(s);
  xs_globals_free(g);

  return 0;
}
