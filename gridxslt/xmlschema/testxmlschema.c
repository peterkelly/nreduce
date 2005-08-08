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

#include "util/namespace.h"
#include "util/macros.h"
#include "xmlschema.h"
#include "parse.h"
#include "output.h"
#include "derivation.h"

#include <stdio.h>
#include <assert.h>
#include <stdlib.h>
#include <math.h>
#include <string.h>
#include <signal.h>
#include <unistd.h>
#include <argp.h>
#include <sys/types.h>

const char *argp_program_version =
  "testxmlschema 0.1";

static char doc[] =
  "XML Schema test program";

static char args_doc[] = "FILENAME";

static struct argp_option options[] = {
  {"dump",                  'd', 0,      0,  "Dump entire schema" },
  {"hierarchy",             'h', 0,      0,  "Print type hierarchy" },
  {"types",                 't', 0,      0,  "Dump top-level type definitions" },
  {"attributes",            'a', 0,      0,  "Dump top-level attributes" },
  {"elements",              'e', 0,      0,  "Dump top-level elements" },
  {"attribute-groups"    ,  'g', 0,      0,  "Dump attribute groups" },
  {"identity-constraints",  'i', 0,      0,  "Dump identity constraints" },
  {"model-groups",          'm', 0,      0,  "Dump model group defintions" },
  {"notations",             'n', 0,      0,  "Dump notations" },
  {"wildcards",             'w', 0,      0,  "Dump wildcards in all top-level types and attribute "
                                             "groups" },
  {"wildcard-union",        'u', "O1,O2",0,  "Print the union of the attribute wildcards of "
                                             "types O1 and O2" },
  {"wildcard-intersection", 's', "O1,O2",0,  "Print the intersection of the attribute wildcards of "
                                             "types O1 and O2" },
  {"wildcard-subset",       'b', "sub,super",0,"Checks to see if the attribute wildcard of type "
                                             "sub is a subset of the attribute wildcard of type "
                                             "super" },
  {"range",                 'r', "type", 0,  "Print the effective total range of the content model "
                                             "particle of a complex type" },
  {"particle-restriction",  'p', "p1,p2",0,  "Test if content model of type t1 can validly "
                                             "restrict that of t2" },
  {"type-derivation",       'v', "t1,t2",0,  "Test if deriving type t1 from t2 is ok" },
  {"final",                 'f', "SET",  0,  "Use SET as the subset of (extension,restriction,list,"
                                             "union) when checking particle or type derivation" },
  {"non-pointless",         'o', "type",  0, "Display a version of the content model of type that "
                                             "has all non-pointless particles ignored" },
  { 0 }
};

struct arguments {
  int dump;
  int hierarchy;
  int types;
  int attributes;
  int elements;
  int attribute_groups;
  int identity_constraints;
  int model_groups;
  int notations;
  int wildcards;
  int wildcard_union;
  int wildcard_intersection;
  int wildcard_subset;
  int range;
  int particle_derivation;
  int type_derivation;
  int non_pointless;
  char *final_set;
  char *typenames;
  char *filename;
};


error_t parse_opt (int key, char *arg, struct argp_state *state)
{
  struct arguments *arguments = state->input;

  switch (key) {
  case 'd':
    arguments->dump = 1;
    break;
  case 'h':
    arguments->hierarchy = 1;
    break;
  case 't':
    arguments->types = 1;
    break;
  case 'a':
    arguments->attributes = 1;
    break;
  case 'e':
    arguments->elements = 1;
    break;
  case 'g':
    arguments->attribute_groups = 1;
    break;
  case 'i':
    arguments->identity_constraints = 1;
    break;
  case 'm':
    arguments->model_groups = 1;
    break;
  case 'n':
    arguments->notations = 1;
    break;
  case 'w':
    arguments->wildcards = 1;
    break;
  case 'u':
    arguments->wildcard_union = 1;
    arguments->typenames = arg;
    break;
  case 's':
    arguments->wildcard_intersection = 1;
    arguments->typenames = arg;
    break;
  case 'b':
    arguments->wildcard_subset = 1;
    arguments->typenames = arg;
    break;
  case 'r':
    arguments->range = 1;
    arguments->typenames = arg;
    break;
  case 'p':
    arguments->particle_derivation = 1;
    arguments->typenames = arg;
    break;
  case 'v':
    arguments->type_derivation = 1;
    arguments->typenames = arg;
    break;
  case 'f':
    arguments->final_set = arg;
    break;
  case 'o':
    arguments->non_pointless = 1;
    arguments->typenames = arg;
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

void print_hierarchy(FILE *f, xs_schema *s)
{
  fprintf(stderr,"Not yet implemented!\n");
  exit(1);
}

void dump_all_types(dumpinfo *di, xs_schema *s)
{
  fprintf(stderr,"Not yet implemented!\n");
  exit(1);
}

void dump_all_attributes(dumpinfo *di, xs_schema *s)
{
  fprintf(stderr,"Not yet implemented!\n");
  exit(1);
}

void dump_all_elements(dumpinfo *di, xs_schema *s)
{
  fprintf(stderr,"Not yet implemented!\n");
  exit(1);
}

void dump_all_attribute_groups(dumpinfo *di, xs_schema *s)
{
  fprintf(stderr,"Not yet implemented!\n");
  exit(1);
}

void dump_all_identity_constraints(dumpinfo *di, xs_schema *s)
{
  fprintf(stderr,"Not yet implemented!\n");
  exit(1);
}

void dump_all_model_groups(dumpinfo *di, xs_schema *s)
{
  fprintf(stderr,"Not yet implemented!\n");
  exit(1);
}

void dump_all_notations(dumpinfo *di, xs_schema *s)
{
  fprintf(stderr,"Not yet implemented!\n");
  exit(1);
}

void get_element_wildcards(xs_particle *p, list **wildcards)
{
  if (XS_PARTICLE_TERM_MODEL_GROUP == p->term_type) {
    list *l;
    for (l = p->term.mg->particles; l; l = l->next)
      get_element_wildcards((xs_particle*)l->data,wildcards);
  }
  else if (XS_PARTICLE_TERM_WILDCARD == p->term_type) {
    list_append(wildcards,p->term.w);
  }
}

void dump_all_wildcards(dumpinfo *di, xs_schema *s)
{
  symbol_space_entry *sse;
  int prevline = 0;

  for (sse = s->symt->ss_types->entries; sse; sse = sse->next) {
    xs_type *t = (xs_type*)sse->object;
    if (!t->builtin) {
      if (prevline)
        printf("\n");
      if (t->ns)
        printf("Attribute wildcard for type {%s}%s:\n",t->ns,t->name);
      else
        printf("Attribute wildcard for type %s:\n",t->name);
      if (t->attribute_wildcard) {
        dump_wildcard(s,NULL,di,0,t->attribute_wildcard);
        dump_wildcard(s,NULL,di,1,t->attribute_wildcard);
      }
      else {
        printf("(none)\n");
      }
      prevline = 1;
    }
    if (!t->builtin && t->content_type) {
      list *element_wildcards = NULL;
      list *l;
      get_element_wildcards(t->content_type,&element_wildcards);
      for (l = element_wildcards; l; l = l->next) {
        xs_wildcard *w = (xs_wildcard*)l->data;
        if (prevline)
          printf("\n");
        if (t->ns)
          printf("Element wildcard within type {%s}%s:\n",t->ns,t->name);
        else
          printf("Element wildcard within type %s:\n",t->name);
        dump_wildcard(s,NULL,di,0,w);
        dump_wildcard(s,NULL,di,1,w);
        prevline = 1;
      }
      list_free(element_wildcards,NULL);
    }
  }

  for (sse = s->symt->ss_attribute_groups->entries; sse; sse = sse->next) {
    xs_attribute_group *ag = (xs_attribute_group*)sse->object;
    if (prevline)
      printf("\n");
    if (ag->ns)
      printf("Attribute wildcard for attribute group {%s}%s:\n",ag->ns,ag->name);
    else
      printf("Attribute wildcard for attribute group %s:\n",ag->name);
    if (ag->attribute_wildcard) {
      dump_wildcard(s,NULL,di,0,ag->attribute_wildcard);
      dump_wildcard(s,NULL,di,1,ag->attribute_wildcard);
    }
    else {
        printf("(none)\n");
    }
    prevline = 1;
  }
}

void get_types(xs_schema *s, char *typenames, xs_type **t1, xs_type **t2)
{
  char *comma;
  char *name1 = strdup(typenames);
  char *name2;

  if (NULL == (comma = strchr(name1,','))) {
    fprintf(stderr,"Two type names must be specified");
    exit(1);
  }

  name2 = strdup(comma+1);
  *comma = '\0';

  if ((NULL == (*t1 = xs_lookup_type(s,name1,s->ns))) &&
      (NULL == (*t1 = xs_lookup_type(s,name1,XS_NAMESPACE)))) {
    fprintf(stderr,"No such type: %s\n",name1);
    exit(1);
  }

  if ((NULL == (*t2 = xs_lookup_type(s,name2,s->ns))) &&
      (NULL == (*t2 = xs_lookup_type(s,name2,XS_NAMESPACE)))) {
    fprintf(stderr,"No such type: %s\n",name2);
    exit(1);
  }

  free(name1);
  free(name2);
}
void get_type_wildcards(xs_schema *s, char *typenames, xs_wildcard **O1, xs_wildcard **O2)
{
  xs_type *t1;
  xs_type *t2;

  get_types(s,typenames,&t1,&t2);

  if (NULL == (*O1 = t1->attribute_wildcard)) {
    fprintf(stderr,"Type %s does not have an attribute wildcard\n",t1->name);
    exit(1);
  }

  if (NULL == (*O2 = t2->attribute_wildcard)) {
    fprintf(stderr,"Type %s does not have an attribute wildcard\n",t2->name);
    exit(1);
  }
}

void dump_wildcard_union(dumpinfo *di, xs_schema *s, char *typenames)
{
  xs_wildcard *O1;
  xs_wildcard *O2;
  xs_wildcard *u;
  get_type_wildcards(s,typenames,&O1,&O2);
  u = xs_wildcard_constraint_union(s,O1,O2,XS_WILDCARD_PROCESS_CONTENTS_SKIP);
  if (NULL != u) {
    dump_wildcard(s,NULL,di,0,u);
    dump_wildcard(s,NULL,di,1,u);
  }
  else {
    printf("Union is not expressible\n");
  }
}

void dump_wildcard_intersection(dumpinfo *di, xs_schema *s, char *typenames)
{
  xs_wildcard *O1;
  xs_wildcard *O2;
  xs_wildcard *i;
  get_type_wildcards(s,typenames,&O1,&O2);
  i = xs_wildcard_constraint_intersection(s,O1,O2,XS_WILDCARD_PROCESS_CONTENTS_SKIP);
  if (NULL != i) {
    dump_wildcard(s,NULL,di,0,i);
    dump_wildcard(s,NULL,di,1,i);
  }
  else {
    printf("Intersection is not expressible\n");
  }
}

void dump_wildcard_subset(dumpinfo *di, xs_schema *s, char *typenames)
{
  xs_wildcard *sub;
  xs_wildcard *super;
  int i;
  get_type_wildcards(s,typenames,&sub,&super);
  i = xs_wildcard_constraint_is_subset(s,sub,super);
  printf("Subset: %s\n",i ? "true" : "false");
}

void print_range(xs_schema *s, char *typename)
{
  xs_type *t;
  xs_range r;

  if (NULL == (t = xs_lookup_type(s,typename,s->ns))) {
    fprintf(stderr,"No such type: %s\n",typename);
    exit(1);
  }

  if (!t->complex) {
    fprintf(stderr,"Not a complex type: %s\n",typename);
    exit(1);
  }

  if (NULL == t->content_type) {
    fprintf(stderr,"No content model: %s\n",typename);
    exit(1);
  }

  r = xs_particle_effective_total_range(t->content_type);
  printf("Min: %d\n",r.min_occurs);
  if (0 > r.max_occurs)
    printf("Max: unbounded\n");
  else
    printf("Max: %d\n",r.max_occurs);
}

void test_particle_derivation(xs_schema *s, char *typenames, char *final_set)
{
  fprintf(stderr,"Not yet implemented\n");
  exit(1);
}

int test_type_derivation(xs_schema *s, char *typenames, char *final_set)
{
  xs_type *t1;
  xs_type *t2;
  int final_extension = 0;
  int final_restriction = 0;
  int final_list = 0;
  int final_union = 0;
  char *c;

  /* replace spaces with commas since runtests doesn't support command-line args with spaces */
  if (final_set)
    for (c = final_set; *c; c++)
      if (',' == *c)
        *c = ' ';

  if (final_set)
    CHECK_CALL(xs_parse_block_final(final_set,&final_extension,&final_restriction,NULL,
                                    &final_list,&final_union))

  get_types(s,typenames,&t1,&t2);

  if (t1->complex) {
    if (0 != xs_check_complex_type_derivation_ok(s,t1,t2,final_extension,final_restriction)) {
      error_info_print(&s->ei,stderr);
      exit(1);
    }
  }
  else {
    if (0 != xs_check_simple_type_derivation_ok(s,t1,t2,final_extension,final_restriction,
                                               final_list,final_union)) {
      error_info_print(&s->ei,stderr);
      exit(1);
    }
  }


  printf("Type derivation ok\n");
  return 0;
}

void print_non_pointless(xs_schema *s, char *typename)
{
  xs_type *t;
  xs_particle *p;
  ignore_pointless_data ipd;

  if (NULL == (t = xs_lookup_type(s,typename,s->ns))) {
    fprintf(stderr,"No such type: %s\n",typename);
    exit(1);
  }

  if (!t->complex) {
    fprintf(stderr,"Not a complex type: %s\n",typename);
    exit(1);
  }

  if (NULL == t->content_type) {
    fprintf(stderr,"No content model: %s\n",typename);
    exit(1);
  }

  p = ignore_pointless(s,t->content_type,&ipd);
  t->effective_content = p;
  output_xmlschema(stdout,s);
  ignore_pointless_free(&ipd);
}

int main(int argc, char **argv)
{
  xs_schema *s;
  xs_globals *g = xs_globals_new();
  struct arguments arguments;
  struct dumpinfo di;
  int r = 0;

  setbuf(stdout,NULL);

  memset(&di,0,sizeof(dumpinfo));
  di.f = stdout;
  di.found_non_builtin = 1;

  memset(&arguments,0,sizeof(arguments));
  argp_parse (&argp, argc, argv, 0, 0, &arguments);

  if (NULL == (s = parse_xmlschema_file(arguments.filename,g)))
    exit(1);

  if (arguments.dump)
    dump_xmlschema(stdout,s);
  else if (arguments.hierarchy)
    print_hierarchy(stdout,s);
  else if (arguments.types)
    dump_all_types(&di,s);
  else if (arguments.attributes)
    dump_all_attributes(&di,s);
  else if (arguments.elements)
    dump_all_elements(&di,s);
  else if (arguments.attribute_groups)
    dump_all_attribute_groups(&di,s);
  else if (arguments.identity_constraints)
    dump_all_identity_constraints(&di,s);
  else if (arguments.model_groups)
    dump_all_model_groups(&di,s);
  else if (arguments.notations)
    dump_all_notations(&di,s);
  else if (arguments.wildcards)
    dump_all_wildcards(&di,s);
  else if (arguments.wildcard_union)
    dump_wildcard_union(&di,s,arguments.typenames);
  else if (arguments.wildcard_intersection)
    dump_wildcard_intersection(&di,s,arguments.typenames);
  else if (arguments.wildcard_subset)
    dump_wildcard_subset(&di,s,arguments.typenames);
  else if (arguments.range)
    print_range(s,arguments.typenames);
  else if (arguments.particle_derivation)
    test_particle_derivation(s,arguments.typenames,arguments.final_set);
  else if (arguments.type_derivation)
    r = test_type_derivation(s,arguments.typenames,arguments.final_set);
  else if (arguments.non_pointless)
    print_non_pointless(s,arguments.typenames);
  else
    output_xmlschema(stdout,s);

  xs_schema_free(s);
  xs_globals_free(g);

  return r;
}
