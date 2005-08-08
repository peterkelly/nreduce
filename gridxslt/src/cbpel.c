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

#include "syntax_bpel.h"

#include <stdio.h>
#include <assert.h>
#include <stdlib.h>
#include <math.h>
#include <string.h>

extern FILE *yyin;
extern int lex_lineno;
int yyparse();

int main(int argc, char **argv)
{
  char *infilename;
  char *dotfilename = NULL;
  bp_scope *bpel;
  FILE *in;

  setbuf(stdout,NULL);

  if (2 > argc) {
    fprintf(stderr,"Usage: cbpel <infilename> [dotfilename]\n");
    exit(1);
  }

  infilename = argv[1];
  if (2 <= argc)
    dotfilename = argv[2];

  if (NULL == (in = fopen(infilename,"r"))) {
    perror(infilename);
    exit(1);
  }

  printf("Parsing BPEL file\n");
  if (NULL == (bpel = parse_bpel(in)))
    exit(1);
  printf("Parsed ok\n");

  output_bpel(stdout,bpel);

/*   if (NULL != dotfilename) { */
/*     FILE *dot; */
/*     if (NULL == (dot = fopen(dotfilename,"w"))) { */
/*       perror(dotfilename); */
/*       exit(1); */
/*     } */

/*     output_dot(dot,parse_tree); */

/*     fclose(dot); */
/*   } */

  bp_scope_free(bpel);
  fclose(in);

  return 0;
}
