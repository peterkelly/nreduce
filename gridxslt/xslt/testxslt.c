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
#include "xmlschema/xmlschema.h"
#include "util/stringbuf.h"

#include <stdio.h>
#include <assert.h>
#include <stdlib.h>
#include <math.h>
#include <string.h>

int ends_with(char *filename, char *ext)
{
  return ((strlen(ext) <= strlen(filename)) &&
          (!strcasecmp(filename+strlen(filename)-strlen(ext),ext)));
}

int main(int argc, char **argv)
{
  char *infilename;
  FILE *in;
  xl_snode *tree;
  error_info ei;
  xs_globals *globals;
  xs_schema *schema;
  int r = 0;

  setbuf(stdout,NULL);
  memset(&ei,0,sizeof(error_info));

  if (2 > argc) {
    fprintf(stderr,"Usage: testxslt <infilename>\n");
    exit(1);
  }

  infilename = argv[1];

  if (NULL == (in = fopen(infilename,"r"))) {
    perror(infilename);
    exit(1);
  }

  if (ends_with(infilename,".xl")) {
    stringbuf *input = stringbuf_new();
    char buf[1024];
    int r;

    while (0 < (r = fread(buf,1,1024,in)))
      stringbuf_append(input,buf,r);

    if (NULL == (tree = xl_snode_parse(input->data,infilename,0,&ei))) {
      error_info_print(&ei,stderr);
      error_info_free_vals(&ei);
      fclose(in);
      exit(1);
    }

    stringbuf_free(input);
  }
  else if (ends_with(infilename,".xsl") ||
           ends_with(infilename,".xslt") ||
           ends_with(infilename,".xml")) {
/*     printf("Parsing XSLT file\n"); */
    tree = xl_snode_new(XSLT_TRANSFORM);
    if (0 != parse_xslt(in,tree,&ei,infilename)) {
      error_info_print(&ei,stderr);
      error_info_free_vals(&ei);
      xl_snode_free(tree);
      fclose(in);
      exit(1);
    }
  }
  else {
    fprintf(stderr,"Unknown file type: %s\n",infilename);
    fclose(in);
    exit(1);
  }

  fclose(in);

  globals = xs_globals_new();
  schema = xs_schema_new(globals);

  if (0 != xl_snode_resolve(tree,schema,infilename,&ei)) {
    error_info_print(&ei,stderr);
    error_info_free_vals(&ei);
    r = 1;
  }
  else {
    output_xslt(stdout,tree);
  }

  xl_snode_free(tree);
  xs_schema_free(schema);
  xs_globals_free(globals);

  return r;
}
