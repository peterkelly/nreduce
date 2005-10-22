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

#include "fsm.h"
#include "util/list.h"
#include "xmlschema/xmlschema.h"

#include <stdio.h>
#include <assert.h>
#include <stdlib.h>
#include <stdarg.h>
#include <math.h>
#include <string.h>
#include <signal.h>
#include <unistd.h>
#include <argp.h>
#include <sys/types.h>
#include <libxml/xmlreader.h>


int trace_depth = 0;

void trace(int print_indent, const char *format, ...)
{
  int i;
  va_list ap;
  if (print_indent)
    for (i = 0; i < trace_depth; i++)
      printf("  ");
  va_start(ap,format);
  vprintf(format,ap);
  va_end(ap);
}

void build_model_group_fsm(xs_schema *s, xs_model_group *mg, fsm *f,
                           fsm_state *start, int startid, fsm_state *end, int endid,
                           list **allocated_inputs, int userel);

/* const char *argp_program_version = */
/*   "validate 0.1"; */

static char doc[] =
  "XML Validator";

static char args_doc[] = "SCHEMA DOTFILENAME";

static struct argp_option options[] = {
  {"dump",  'd', 0,      0,  "Dump output" },
  { 0 }
};

struct arguments {
  int dump;
  char *schemafilename;
  char *dotfilename;
};


error_t parse_opt (int key, char *arg, struct argp_state *state)
{
  struct arguments *arguments = (struct arguments*)state->input;

  switch (key) {
  case 'd':
    arguments->dump = 1;
    break;
  case ARGP_KEY_ARG:
    if (0 == state->arg_num)
      arguments->schemafilename = arg;
    else if (1 == state->arg_num)
      arguments->dotfilename = arg;
    else
      argp_usage(state);
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

int validate(xmlTextReaderPtr reader, xs_schema *s)
{
  int r;
  list *type_stack = NULL;
  while (1 == (r = xmlTextReaderRead(reader))) {
    int type = xmlTextReaderNodeType(reader);
    const xmlChar *name = xmlTextReaderConstName(reader);
/*     const xmlChar *ns = xmlTextReaderConstNamespaceUri(reader); */
    if (XML_READER_TYPE_ELEMENT == type) {
      printf("begin element %s\n",name);
      if (0 == xmlTextReaderDepth(reader)) {
      }
      if (NULL == type_stack) {
      }
    }
    else if (XML_READER_TYPE_END_ELEMENT == type) {
      printf("end element %s\n",name);
    }
  }
  return 0;
}









typedef struct validation_input validation_input;

struct validation_input {
  xs_element *e;
  xs_wildcard *w;
};


int validation_input_equals(void *a, void *b)
{
  validation_input *va = (validation_input*)a;
  validation_input *vb = (validation_input*)b;
  if (va->e && vb->e && !strcmp(va->e->def.ident.name,vb->e->def.ident.name))
    return 1;
  else
    return 0;
}

void validation_input_print(FILE *f, void *input)
{
  validation_input *vi = (validation_input*)input;
  if (vi->e)
    fprintf(f,"<%s>",vi->e->def.ident.name);
  else
    fprintf(f,"*");
}




validation_input *new_validation_input(list **allocated, xs_element *e, xs_wildcard *w)
{
  validation_input *vi = (validation_input*)calloc(1,sizeof(validation_input));
  vi->e = e;
  vi->w = w;
  list_push(allocated,vi);
  return vi;
}

/**
 * Build the finite state machine for the term of the particle. Note that this deals with exactly
 * one occurrance of the term; i.e. if minOccurs or maxOccurs != 1 for the particle, the caller
 * must take care of the multiple occurances by calling this function appropriately.
 */
void build_term_fsm(xs_schema *s, xs_particle *p, fsm *f,
                    fsm_state *start, int startid, fsm_state *end, int endid,
                    list **allocated_inputs, int userel)
{
  trace_depth++;
  switch (p->term_type) {
  case XS_PARTICLE_TERM_ELEMENT: {
    trace(1,"element <%s>: start %d:%d end %d:%d",
           p->term.e->def.ident.name,start->num,startid,end->num,endid);
    if (userel) {
      trace(0," USEREL %d",userel);
    }
    trace(0,"\n");

    if (userel) {
      add_transition(start,end,new_validation_input(allocated_inputs,p->term.e,NULL),
/*                      startid,startid,-1,endid-startid); */
                     startid,startid+userel-1,-1,endid-startid);
    }
    else {
      add_transition(start,end,new_validation_input(allocated_inputs,p->term.e,NULL),
                     startid,startid,endid,0);
    }
    break;
  }
  case XS_PARTICLE_TERM_MODEL_GROUP: {
    build_model_group_fsm(s,p->term.mg,f,start,startid,end,endid,allocated_inputs,userel);
    break;
  }
  case XS_PARTICLE_TERM_WILDCARD:
    break;
  default:
    assert(0);
    break;
  }
  trace_depth--;
}

typedef struct build_model_group_data build_model_group_data;
struct build_model_group_data {
  fsm_state *intermediate;
  int intermediateid;
};

/**
 * Build the finite state machine for a single entry in the particles list of a model group. This
 * takes care of all occurrances of the entry.
 */
void build_model_group_entry_fsm(xs_schema *s, xs_particle *p, fsm *f,
                                 fsm_state *start, int startid, fsm_state *end, int endid,
                                 list **allocated_inputs, int userel)
{
/*   int i; */
  build_model_group_data *bd;

  trace_depth++;

  if (NULL == p->vdata)
    p->vdata = calloc(1,sizeof(build_model_group_data));
  bd = (build_model_group_data*)p->vdata;

  trace(1,"build entry:");
  if (XS_PARTICLE_TERM_MODEL_GROUP == p->term_type)
    trace(0," ********MG");
  else
    trace(0," <%s>",p->term.e->def.ident.name);
  trace(0," start %d:%d end %d:%d",start->num,startid,end->num,endid);
  if (userel) {
    trace(0," USEREL %d",userel);
  }
  trace(0,"\n");

  /* If minOccurs > 0, add the appropriate number of instances of the term's machine. Intermediate
     states are added between each instance (and at the end, if we're going to add more - see
     below). */
#if 0
  for (i = 0; i < p->range.min_occurs; i++) {
    fsm_state *temp = end;
    int tempid = 0;
    if ((i < p->range.min_occurs-1) || (p->range.min_occurs < p->range.max_occurs)) {
      temp = fsm_add_state(f);
      tempid = 0;
    }
    build_term_fsm(s,p,f,start,startid,temp,tempid,allocated_inputs,0);
    start = temp;
    startid = tempid;
  }
#endif

  if (1 == p->range.min_occurs) {
    build_term_fsm(s,p,f,start,startid,end,endid,allocated_inputs,userel);
  }
  else if (1 < p->range.min_occurs) {


    if (NULL == bd->intermediate) {
      bd->intermediate = fsm_add_state(f);
      bd->intermediate->count = 0;
      trace(1,"build entry: added intermediate state %d\n",bd->intermediate->num);
      bd->intermediateid = 0;
    }
    bd->intermediate->count += p->range.min_occurs-1;

    assert(start != bd->intermediate);
    build_term_fsm(s,p,f,start,startid,bd->intermediate,bd->intermediateid,allocated_inputs,userel);
    start = bd->intermediate;
    startid = bd->intermediateid;
    bd->intermediateid++;

    if (2 < p->range.min_occurs) {
/*         for (i = 1; i < p->range.min_occurs-1; i++) { */
          assert(start == bd->intermediate);
          build_term_fsm(s,p,f,start,startid,bd->intermediate,bd->intermediateid,
                         allocated_inputs,p->range.min_occurs-2);

          bd->intermediateid += p->range.min_occurs-2;
          start = bd->intermediate;
          startid = bd->intermediateid-1;

/*           start = bd->intermediate; */
/*           startid = bd->intermediateid; */
/*           bd->intermediateid++; */
/*         } */
    }
    build_term_fsm(s,p,f,start,startid,end,endid,allocated_inputs,userel);



#if 0
/*     fsm_state *temp = fsm_add_state(f); */
/*     trace(1,"added temp state %d\n",temp->num); */
    start->count += p->range.min_occurs-1;
    for (i = 0; i < p->range.min_occurs-1; i++) {
      build_term_fsm(s,p,f,start,startid,start,startid+1,allocated_inputs,0);
      startid++;
    }
    build_term_fsm(s,p,f,start,startid,end,endid,allocated_inputs,0);
/*     start = temp; */
/*     startid = 0; */
    start = end;
    startid = endid;
#endif
  }

#if 0
  /* If minOccurs < maxOccurs, add the appropriate number of term machine instances. Each will have
     a transition to the next one, and an epsilon transition to the end state, i.e. meaning that
     for each instance but the last, we can accept another input or go straight to the end. */
  for (i = p->range.min_occurs; i < p->range.max_occurs; i++) {
    fsm_state *temp = end;
    int tempid = 0;
    if ((i < p->range.max_occurs-1)) {
      temp = fsm_add_state(f);
      tempid = 0;
    }
    build_term_fsm(s,p,f,start,startid,temp,tempid,allocated_inputs,0);
    add_transition(start,end,NULL,0,0,0,0);
    start = temp;
    startid = tempid;
  }

  /* maxOccurs=unbounded - add an instance of the term's machine which starts and ends at the
     current state */
  if (0 > p->range.max_occurs) {
    build_term_fsm(s,p,f,start,startid,start,startid,allocated_inputs,0);
    if (0 == p->range.min_occurs)
      add_transition(start,end,NULL,0,0,0,0);
  }
#endif

  trace_depth--;
}


/**
 * Build the finite state machine for a model group
 */
void build_model_group_fsm(xs_schema *s, xs_model_group *mg, fsm *f,
                           fsm_state *start, int startid, fsm_state *end, int endid,
                           list **allocated_inputs, int userel)
{
  list *l;
  build_model_group_data *bd;

  switch (mg->compositor) {
  case XS_MODEL_GROUP_COMPOSITOR_ALL:
    assert(0);
    break;
  case XS_MODEL_GROUP_COMPOSITOR_CHOICE:
    for (l = mg->particles; l; l = l->next) {
      xs_particle *p2 = (xs_particle*)l->data;
      build_model_group_entry_fsm(s,p2,f,start,startid,end,endid,allocated_inputs,0);
    }
    break;
  case XS_MODEL_GROUP_COMPOSITOR_SEQUENCE:



    if (userel) {
      trace(1,"build sequence: start %d:%d end %d:%d USEREL %d\n",
            start->num,startid,end->num,endid,userel);
    }
    else {
      trace(1,"build sequence: start %d:%d end %d:%d\n",
            start->num,startid,end->num,endid);
    }
    if (NULL == mg->particles)
      break;

    if (NULL == mg->particles->next) {
      xs_particle *p = (xs_particle*)mg->particles->data;
      build_model_group_entry_fsm(s,p,f,start,startid,end,endid,allocated_inputs,userel);
    }
    else {
      xs_particle *p;
      int pcount = 0;
      int incr;
      int mul = 1;

      if (userel)
        mul = userel;

      for (l = mg->particles; l; l = l->next)
        pcount++;
      incr = mul*(pcount-1);






      for (l = mg->particles; l && l->next; l = l->next) {


        p = (xs_particle*)l->data;

        if (NULL == p->seqdata)
          p->seqdata = calloc(1,sizeof(build_model_group_data));
        bd = (build_model_group_data*)p->seqdata;

        if (NULL == bd->intermediate) {
          bd->intermediate = fsm_add_state(f);
          bd->intermediate->count = 0;
          trace(1,"build sequence - item ");
          if (XS_PARTICLE_TERM_MODEL_GROUP == p->term_type)
            trace(0,"MG");
          else
            trace(0,p->term.e->def.ident.name);
          trace(0,": added intermediate state %d\n",bd->intermediate->num);
          bd->intermediateid = 0;
        }
        trace(1,"build sequence - item ");
        if (XS_PARTICLE_TERM_MODEL_GROUP == p->term_type)
          trace(0,"MG");
        else
          trace(0,p->term.e->def.ident.name);
        trace(0,": setting intermediate state %d count to %d + %d = %d\n",
               bd->intermediate->num,
               bd->intermediate->count,mul,
               bd->intermediate->count+mul);
        bd->intermediate->count += mul;







        build_model_group_entry_fsm(s,p,f,start,startid,bd->intermediate,bd->intermediateid,
                                      allocated_inputs,userel);
        start = bd->intermediate;
        startid = bd->intermediateid;
        if (userel)
          bd->intermediateid += userel;
        else
          bd->intermediateid++;
      }

      p = (xs_particle*)l->data;
      build_model_group_entry_fsm(s,p,f,start,startid,end,endid,allocated_inputs,userel);













#if 0
      if (NULL == mg->vdata)
        mg->vdata = calloc(1,sizeof(build_model_group_data));
      bd = (build_model_group_data*)mg->vdata;

      if (NULL == bd->intermediate) {
        bd->intermediate = fsm_add_state(f);
        bd->intermediate->count = 0;
        trace(1,"build sequence: added intermediate state %d\n",bd->intermediate->num);
        bd->intermediateid = 0;
      }
      trace(1,"build sequence: setting intermediate state %d count to %d + %d*%d = %d\n",
             bd->intermediate->num,
             bd->intermediate->count,mul,incr,
             bd->intermediate->count+incr);
      bd->intermediate->count += incr;

      for (l = mg->particles; l && l->next; l = l->next) {
        p = (xs_particle*)l->data;
        build_model_group_entry_fsm(s,p,f,start,startid,bd->intermediate,bd->intermediateid,
                                      allocated_inputs,userel);
        start = bd->intermediate;
        startid = bd->intermediateid;
        if (userel)
          bd->intermediateid += userel;
        else
          bd->intermediateid++;
      }

      p = (xs_particle*)l->data;
      build_model_group_entry_fsm(s,p,f,start,startid,end,endid,allocated_inputs,userel);
#endif

    }

#if 0
    for (l = mg->particles; l; l = l->next) {
      xs_particle *p = (xs_particle*)l->data;
      if (l->next) {
        fsm_state *intermediate = fsm_add_state(f);
        int intermediateid = 0;
        trace(1,"seq %p: added intermediate state %d\n",p,intermediate->num);
        build_model_group_entry_fsm(s,p,f,start,startid,intermediate,intermediateid,
                                    allocated_inputs,0);
        start = intermediate;
        startid = intermediateid;
      }
      else {
        build_model_group_entry_fsm(s,p,f,start,startid,end,endid,allocated_inputs,0);
      }
    }
#endif

    break;
  default:
    assert(0);
    break;
  }
}






int build_fsm(xs_schema *s, fsm *f, FILE *dotfile)
{
  list *allocated_inputs = NULL;
  xs_type *t = xs_lookup_type(s,nsname_temp(NULL,"root"));
  fsm_state *start = f->start = fsm_add_state(f);
  int startid = 0;
  fsm *df = fsm_new(validation_input_equals,validation_input_print);
  if (NULL == t) {
    fprintf(stderr,"no \"root\" type\n");
    return 1;
  }
  if (NULL == t->content_type) {
    fprintf(stderr,"no content model\n");
    return 1;
  }
  if (XS_PARTICLE_TERM_MODEL_GROUP != t->content_type->term_type) {
    fprintf(stderr,"content type must be a term!\n");
    return 1;
  }

  if (t->content_type) {
    fsm_state *end = fsm_add_state(f);
    int endid = 0;
    end->accept = 1;
    build_model_group_fsm(s,t->content_type->term.mg,f,start,startid,end,endid,
                          &allocated_inputs,0);
  }

  fsm_expand_and_determinise(f,df,dotfile);

  fsm_free(df);
  list_free(allocated_inputs,(list_d_t)free);
  return 0;
}

int main(int argc, char **argv)
{
  xs_schema *s;
  xs_globals *g = xs_globals_new();
  struct arguments arguments = { dump: 0, schemafilename: NULL, dotfilename: NULL };
  int r;
/*   xmlTextReaderPtr reader; */
  fsm *f = fsm_new(validation_input_equals,validation_input_print);
  FILE *dotfile = NULL;

  setbuf(stdout,NULL);
  exit(1);

  argp_parse (&argp, argc, argv, 0, 0, &arguments);

  if (NULL == (s = parse_xmlschema_file(arguments.schemafilename,g)))
    exit(1);

  if (NULL == (dotfile = fopen(arguments.dotfilename,"w"))) {
    perror(arguments.dotfilename);
    exit(1);
  }

/*   r = validate(reader,s); */
  r = build_fsm(s,f,dotfile);
  fsm_free(f);
  fclose(dotfile);

/*   xmlFreeTextReader(reader); */
  xs_schema_free(s);
  xs_globals_free(g);

  return r;
}
