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

#include "util/stringbuf.h"
#include "util/debug.h"
#include "xmlschema.h"
#include "validation.h"

#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <string.h>
#include <signal.h>
#include <unistd.h>
#include <argp.h>
#include <sys/types.h>
#include <libxml/tree.h>

using namespace GridXSLT;

/* static const char *argp_program_version = */
/*   "binxml 0.1"; */

static char doc[] =
  "XML binary encoder/decoder";

static char args_doc[] = "SCHEMA INPUT OUTPUT";

static struct argp_option options[] = {
  {"encode",                  'e', 0,      0,  "Encode XML file into binary" },
  {"decode",                  'd', 0,      0,  "Decode binary file into XML" },
  {"printable",               'p', 0,      0,  "Use printable binary representation" },
  { 0 }
};

struct arguments {
  int encode;
  int decode;
  int printable;
  char *schema;
  char *input;
  char *output;
};

static error_t parse_opt (int key, char *arg, struct argp_state *state)
{
  struct arguments *arguments = (struct arguments*)state->input;

  switch (key) {
  case 'e':
    arguments->encode = 1;
    break;
  case 'd':
    arguments->decode = 1;
    break;
  case 'p':
    arguments->printable = 1;
  case ARGP_KEY_ARG:
    if (0 == state->arg_num)
      arguments->schema = arg;
    else if (1 == state->arg_num)
      arguments->input = arg;
    else if (2 == state->arg_num)
      arguments->output = arg;
    else
      argp_usage (state);
    break;
  case ARGP_KEY_END:
    if ((2 > state->arg_num) || (!arguments->decode && !arguments->encode))
      argp_usage (state);
    break;
  default:
    return ARGP_ERR_UNKNOWN;
  }
  return 0;
}

static struct argp argp = { options, parse_opt, args_doc, doc };

#if 0
static int decode(FILE *out, xs_validator *v, xmlNodePtr n, stringbuf *encoded)
{
  xmlOutputBuffer *buf = xmlOutputBufferCreateFile(out,NULL);
  xmlTextWriter *writer = xmlNewTextWriter(buf);
  NSName nn = NSName(String::null(),n->name);
  Type *t = xs_lookup_type(v->s,nn);
  ASSERT(t);

  xmlTextWriterSetIndent(writer,1);
  xmlTextWriterSetIndentString(writer,"  ");
  xmlTextWriterStartDocument(writer,NULL,NULL,NULL);

  if (0 != xs_decode(v,writer,t,encoded,0)) {
    fmessage(stderr,"decoding failed!\n");
    return 1;
  }

  xmlTextWriterEndDocument(writer);
  xmlTextWriterFlush(writer);
  xmlFreeTextWriter(writer);
  return 0;
}
#endif

static void print_hex(FILE *f, stringbuf *buf)
{
  int pos;

  for (pos = 0; pos < buf->size-1; pos += 16) {
    int p;
    fmessage(f,"%08x  ",pos);
    for (p = pos; (p < pos+16); p++) {
      if (p < buf->size-1)
        fmessage(f,"%02hhx ",buf->data[p]);
      else
        fmessage(f,"   ");
      if (7 == p % 8)
        fmessage(f," ");
    }
    fmessage(f,"|");
    for (p = pos; (p < pos+16) && (p < buf->size-1); p++) {
      fmessage(f,"%c",isprint(buf->data[p]) ? buf->data[p] : '.');
    }
    fmessage(f,"|");
    fmessage(f,"\n");
  }
  if (0 < buf->size-1)
    fmessage(f,"%08x\n",buf->size-1);
}

static xmlDocPtr encode(xs_validator *v, char *filename, stringbuf *encoded)
{
  FILE *f;
  xmlDocPtr doc;
  xmlNodePtr root;

  if (NULL == (f = fopen(filename,"r"))) {
    perror(filename);
    return NULL;
  }

  if (NULL == (doc = xmlReadFd(fileno(f),NULL,NULL,0))) {
    fclose(f);
    fmessage(stderr,"XML parse error.\n");
    return NULL;
  }
  fclose(f);

  if (NULL == (root = xmlDocGetRootElement(doc))) {
    fmessage(stderr,"No root element.\n");
    return NULL;
  }

  if (0 == validate_root(v,filename,root,encoded)) {
/*     message("Document valid!\n"); */
  }
  else {
    v->ei.fprint(stderr);
    return NULL;
  }

/*   message("encoded: %d bytes\n",encoded->size-1); */
  return doc;
}



int binxml_main(int argc, char **argv)
{
  Schema *s;
  BuiltinTypes *g = new BuiltinTypes();
  struct arguments arguments;
  xs_validator *v;
  stringbuf *encoded = stringbuf_new();
  stringbuf *decoded = stringbuf_new();

  setbuf(stdout,NULL);
  memset(&arguments,0,sizeof(struct arguments));
  argp_parse (&argp, argc, argv, 0, 0, &arguments);

  if (NULL == (s = parse_xmlschema_file(arguments.schema,g)))
    return 1;
  v = xs_validator_new(s);

  if (arguments.encode) {
    FILE *encout = stdout;
    xmlDocPtr doc = encode(v,arguments.input,encoded);

    if (NULL == doc)
      return 1;

/*     if (arguments.decode) { */
/*       decode(stdout,v,xmlDocGetRootElement(doc),encoded); */
/*     } */

    if ((NULL != arguments.output) &&
        (NULL == (encout = fopen(arguments.output,"w")))) {
      perror(arguments.output);
      return 1;
    }
    if (arguments.printable)
      print_hex(encout,encoded);
    else
      fwrite(encoded->data,1,encoded->size-1,encout);
    if (NULL != arguments.output)
      fclose(encout);

    xmlFreeDoc(doc);
  }
  else {
    ASSERT(!"not yet implemented");
#if 0
    FILE *f;
    int r;
    char buf[1024];
    if (NULL == (f = fopen(arguments.input,"r"))) {
      perror(arguments.input);
      return 1;
    }
    while (0 < (r = fread(buf,1,1024,f)))
      stringbuf_append(encoded,buf,r);
    fclose(f);
#endif
  }


  stringbuf_free(encoded);
  stringbuf_free(decoded);

  delete s;
  delete g;
  xs_validator_free(v);

  return 0;
}
