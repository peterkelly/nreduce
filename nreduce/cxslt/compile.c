/*
 * This file is part of the nreduce project
 * Copyright (C) 2007 Peter Kelly (pmk@cs.adelaide.edu.au)
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
 * $Id: cxslt.c 603 2007-08-14 02:44:16Z pmkelly $
 *
 */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>
#include <assert.h>
#include <libxml/parser.h>
#include "cxslt.h"

int main(int argc, char **argv)
{
  char *compresult;
  array *xslt;
  char zero = '\0';

  setbuf(stdout,NULL);

  if (2 > argc) {
    fprintf(stderr,"Usage: compile <xsltfile>\n");
    exit(1);
  }

  xslt = read_file(argv[1]);
  array_append(xslt,&zero,1);

  if (cxslt(xslt->data,argv[1],&compresult)) {
    printf("%s",compresult);
    free(compresult);
    return 0;
  }
  else {
    fprintf(stderr,"%s\n",compresult);
    free(compresult);
    return 1;
  }
}
