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

#include "engine.h"
#include "functions.h"
#include "serialization.h"
#include "util/macros.h"
#include "util/debug.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

typedef struct df_activity df_activity;
typedef struct df_actdest df_actdest;

struct df_actdest {
  df_activity *a;
  int p;
};

struct df_activity {
  df_instruction *instr;
  int remaining;
  df_value **values;
  df_actdest *outports;
  int actno;
  int refs;
  int fired;
};

df_state *df_state_new(df_program *program)
{
  df_state *state = (df_state*)calloc(1,sizeof(df_state));
  state->program = program;
  state->intype = df_seqtype_new_atomic(program->schema->globals->int_type);
  return state;
}

void df_state_free(df_state *state)
{
  df_seqtype_deref(state->intype);
  free(state);
}

void free_activity(df_state *state, df_activity *a)
{
  if (!a->fired) {
    int i;
    for (i = 0; i < a->instr->ninports; i++)
      if (NULL != a->values[i])
        df_value_deref(state->program->schema->globals,a->values[i]);
  }
  free(a->values);
  free(a->outports);
  free(a);
}

df_activity *df_activate_function(df_state *state, df_function *fun,
                                    df_activity *dest, int destp,
                                    df_activity **starts, int startslen)
{
  list *l;
  df_activity *start = NULL;
  int actno = state->actno++;
  int i;
  for (l = fun->instructions; l; l = l->next) {
    df_instruction *instr = (df_instruction*)l->data;
    df_activity *a;

    if (instr->internal)
      continue;

    a = (df_activity*)calloc(1,sizeof(df_activity));

    a->actno = actno;
    a->instr = instr;
    instr->act = a;
    list_push(&state->activities,a);
  }
  start = fun->start->act;
  start->refs++;

  assert(startslen == fun->nparams);
  for (i = 0; i < fun->nparams; i++) {
    starts[i] = fun->params[i].start->act;
    starts[i]->refs++;
  }

  for (l = fun->instructions; l; l = l->next) {
    df_instruction *instr = (df_instruction*)l->data;
    df_activity *a;
    int i;

    if (instr->internal)
      continue;

    a = instr->act;
    a->remaining = instr->ninports;
    a->outports = (df_actdest*)calloc(a->instr->noutports,sizeof(df_actdest));
    a->values = (df_value**)calloc(a->instr->ninports,sizeof(df_value*));

    if (OP_SPECIAL_RETURN == instr->opcode) {
      a->outports[0].a = dest;
      a->outports[0].p = destp;
    }
    else {
      for (i = 0; i < instr->noutports; i++) {
        if (NULL == instr->outports[i].dest) {
          fprintf(stderr,"Instruction %s:%d has no destination assigned to output port %d\n",
                  fun->ident.name,instr->id,i);
        }
        assert(NULL != instr->outports[i].dest);
        a->outports[i].a = instr->outports[i].dest->act;
        a->outports[i].a->refs++;
        a->outports[i].p = instr->outports[i].destp;
      }
    }
  }
  for (l = fun->instructions; l; l = l->next)
    ((df_instruction*)l->data)->act = NULL;
  return start;
}

void df_set_input(df_activity *a, int p, df_value *v)
{
  a->values[p] = v;
  a->remaining--;
  a->refs--;
  assert(0 <= a->remaining);
  assert(0 <= a->refs);
}

void df_output_value(df_activity *source, int sourcep, df_value *v)
{
  df_activity *dest = source->outports[sourcep].a;
  int destp = source->outports[sourcep].p;
  assert(NULL == dest->values[destp]);
  df_set_input(dest,destp,v);
}

void df_print_actmsg(const char *prefix, df_activity *a)
{
  printf("%s [%d] %s.%d - %s\n",prefix,
         a->actno,a->instr->fun->ident.name,a->instr->id,df_opstr(a->instr->opcode));
}

void df_deref_activity(df_activity *a, int indent, df_activity *merge)
{
  int i;
  a->refs--;
  assert(0 <= a->refs);
  assert(OP_SPECIAL_MERGE == merge->instr->opcode);
  if ((OP_SPECIAL_RETURN == a->instr->opcode) || (merge == a))
    return;
  if (0 == a->refs) {
    for (i = 0; i < a->instr->noutports; i++)
      df_deref_activity(a->outports[i].a,indent+1,merge);
  }
}

static int is_text_node(df_value *v)
{
  return ((SEQTYPE_ITEM == v->seqtype->type) &&
          (ITEM_TEXT == v->seqtype->item->kind));
}

static char *df_construct_simple_content(xs_globals *g, df_value *v)
{
  char *str;
  stringbuf *buf = stringbuf_new();
  list *values = df_sequence_to_list(v);
  list *strings = NULL;
  list *l;
  char *separator = " "; /* FIXME: allow custom separators */
  char *nextsep = "";

  for (l = values; l; l = l->next) {
    df_value *v = (df_value*)l->data;
    stringbuf_format(buf,"%s",nextsep);
    if (is_text_node(v)) {
      if (0 == strlen(v->value.n->value))
        continue;
      stringbuf_format(buf,v->value.n->value);
      if (l->next && (is_text_node((df_value*)l->next->data)))
        nextsep = "";
      else
        nextsep = separator;
    }
    else {
      char *str = df_value_as_string(g,v);
      stringbuf_format(buf,str);
      free(str);
      nextsep = separator;
    }
  }

  list_free(values,NULL);
  df_value_deref_list(g,strings);
  list_free(strings,NULL);

  str = strdup(buf->data);
  stringbuf_free(buf);
  return str;
}

static int nodetest_matches(df_node *n, char *nametest, df_seqtype *seqtypetest)
{
  if (NULL != nametest) {
    return (((NODE_ELEMENT == n->type) || (NODE_ATTRIBUTE == n->type)) &&
            !strcmp(n->ident.name,nametest));
  }
  else {
    assert(NULL != seqtypetest);
    if (SEQTYPE_ITEM == seqtypetest->type) {
      switch (seqtypetest->item->kind) {
      case ITEM_ATOMIC:
        /* FIXME (does this even apply here?) */
        break;
      case ITEM_DOCUMENT:
        /* FIXME: support checks based on document type, e.g. document(element(foo)) */
        return (n->type == NODE_DOCUMENT);
        break;
      case ITEM_ELEMENT:
        if (NULL != seqtypetest->item->elem) {
          /* FIXME: support checks based on type annotation of node, e.g. element(*,xs:string) */
          return ((n->type == NODE_ELEMENT) &&
                  nsname_equals(n->ident,seqtypetest->item->elem->def.ident));
        }
        else {
          return (n->type == NODE_ELEMENT);
        }
        break;
      case ITEM_ATTRIBUTE:
        /* FIXME */
        break;
      case ITEM_PI:
        return (n->type == NODE_PI);
        break;
      case ITEM_COMMENT:
        return (n->type == NODE_COMMENT);
        break;
      case ITEM_TEXT:
        return (n->type == NODE_TEXT);
        break;
      case ITEM_NAMESPACE:
        return (n->type == NODE_NAMESPACE);
        break;
      default:
        assert(!"invalid item type");
        break;
      }
    }
    else if (SEQTYPE_CHOICE == seqtypetest->type) {
      return (nodetest_matches(n,NULL,seqtypetest->left) ||
              nodetest_matches(n,NULL,seqtypetest->right));
    }
  }

  return 0;
}

static int append_matching_nodes(df_node *self, char *nametest, df_seqtype *seqtypetest, int axis,
                                 list **matches)
{
  df_node *c;
  list *l;
  switch (axis) {
  case AXIS_CHILD:
    for (c = self->first_child; c; c = c->next)
      if (nodetest_matches(c,nametest,seqtypetest))
        list_append(matches,df_node_to_value(c));
    break;
  case AXIS_DESCENDANT:
    for (c = self->first_child; c; c = c->next)
      CHECK_CALL(append_matching_nodes(c,nametest,seqtypetest,AXIS_DESCENDANT_OR_SELF,matches))
    break;
  case AXIS_ATTRIBUTE:
    for (l = self->attributes; l; l = l->next) {
      df_node *attr = (df_node*)l->data;
      if (nodetest_matches(attr,nametest,seqtypetest)) {
        df_value *val = df_node_to_value(attr);
        list_append(matches,val);
        debugl("adding matching attribute %p; root %p refcount is now %d",
              val->value.n,df_node_root(val->value.n),df_node_root(val->value.n)->refcount);
      }
    }
    break;
  case AXIS_SELF:
    if (nodetest_matches(self,nametest,seqtypetest))
      list_append(matches,df_node_to_value(self));
    break;
  case AXIS_DESCENDANT_OR_SELF:
    CHECK_CALL(append_matching_nodes(self,nametest,seqtypetest,AXIS_SELF,matches))
    CHECK_CALL(append_matching_nodes(self,nametest,seqtypetest,AXIS_DESCENDANT,matches))
    break;
  case AXIS_FOLLOWING_SIBLING:
    /* FIXME */
    assert(!"not yet implemented");
    break;
  case AXIS_FOLLOWING:
    /* FIXME */
    assert(!"not yet implemented");
    break;
  case AXIS_NAMESPACE:
    /* FIXME */
    assert(!"not yet implemented");
    break;
  case AXIS_PARENT:
    /* FIXME */
    assert(!"not yet implemented");
    break;
  case AXIS_ANCESTOR:
    /* FIXME */
    assert(!"not yet implemented");
    break;
  case AXIS_PRECEDING_SIBLING:
    /* FIXME */
    assert(!"not yet implemented");
    break;
  case AXIS_PRECEDING:
    /* FIXME */
    assert(!"not yet implemented");
    break;
  case AXIS_ANCESTOR_OR_SELF:
    /* FIXME */
    assert(!"not yet implemented");
    break;
  default:
    assert(!"invalid axis");
    break;
  }
  return 0;
}

static void add_sequence_as_child_nodes(xs_globals *g, list *values, df_node *parent)
{
  list *l;

/*   debugl("add_sequence_as_child_nodes: values are:"); */

/*   for (l = values; l; l = l->next) { */
/*     df_value *v = (df_value*)l->data; */
/*     assert(SEQTYPE_ITEM == v->seqtype->type); */
/*     debugl("  %s",df_item_kinds[v->seqtype->item->kind]); */
/*   } */

  for (l = values; l; l = l->next) {
    df_value *v = (df_value*)l->data;
    assert(SEQTYPE_ITEM == v->seqtype->type);
    switch (v->seqtype->item->kind) {
    case ITEM_ATOMIC: {
      df_node *textnode = df_atomic_value_to_text_node(g,v);
      debugl("created text node %p for atomic value %p (which has %d refs)",textnode,v,v->refcount);
      df_node_add_child(parent,textnode);
      break;
    }
    case ITEM_DOCUMENT: {
      df_node *doc = v->value.n;
      df_node *c = doc->first_child;

      while (c) {
        df_node *next = c->next;
        if ((1 == v->refcount) && (1 == doc->refcount)) {
          c->parent = NULL;
          c->next = NULL;
          c->prev = NULL;
          assert(0 == c->refcount);
          df_node_add_child(parent,c);
        }
        else {
          df_node_add_child(parent,df_node_deep_copy(c));
        }
        c = next;
      }

      if ((1 == v->refcount) && (1 == doc->refcount)) {
        doc->first_child = NULL;
        doc->last_child = NULL;
      }

      break;
    }
    case ITEM_ATTRIBUTE:
      if ((1 == v->refcount) && (1 == v->value.n->refcount)) {
        debugl("***** adding existing attribute node");
        v->value.n->refcount = 0;
        df_node_add_attribute(parent,v->value.n);
        v->value.n = NULL;
      }
      else {
        debugl("!!! copying attribute node");
        df_node_add_attribute(parent,df_node_deep_copy(v->value.n));
      }
      break;
    case ITEM_ELEMENT:
    case ITEM_PI:
    case ITEM_COMMENT:
    case ITEM_TEXT:
    case ITEM_NAMESPACE:
      if ((1 == v->refcount) && (1 == v->value.n->refcount)) {
        debugl("***** adding existing text node");
        v->value.n->refcount = 0;
        df_node_add_child(parent,v->value.n);
        v->value.n = NULL;
      }
      else {
        df_node_add_child(parent,df_node_deep_copy(v->value.n));
      }
      break;
    default:
      assert(!"invalid item type");
      break;
    }
  }
}

int df_fire_activity(df_state *state, df_activity *a)
{
  xs_globals *g = state->program->schema->globals;
  if (state->trace)
    df_print_actmsg("Firing",a);

  switch (a->instr->opcode) {
  case OP_SPECIAL_CONST:
    df_output_value(a,0,df_value_ref(a->instr->cvalue));
    df_value_deref(g,a->values[0]);
    break;
  case OP_SPECIAL_DUP:
    df_output_value(a,0,df_value_ref(a->values[0]));
    df_output_value(a,1,df_value_ref(a->values[0]));
    df_value_deref(g,a->values[0]);
    break;
  case OP_SPECIAL_SPLIT:
    if (0 == a->values[0]->value.b) {
      df_output_value(a,0,a->values[1]);
      df_deref_activity(a->outports[1].a,0,a->outports[2].a);
    }
    else {
      df_output_value(a,1,a->values[1]);
      df_deref_activity(a->outports[0].a,0,a->outports[2].a);
    }
    df_output_value(a,2,a->values[0]);
    break;
  case OP_SPECIAL_MERGE:
    df_output_value(a,0,a->values[0]);
    df_value_deref(state->program->schema->globals,a->values[1]);
    break;
  case OP_SPECIAL_CALL: {
    df_activity **acts = (df_activity**)alloca(a->instr->cfun->nparams*sizeof(df_activity*));
    df_activity *start;
    int i;
    memset(acts,0,a->instr->cfun->nparams*sizeof(df_activity*));
    start = df_activate_function(state,a->instr->cfun,a->outports[0].a,a->outports[0].p,
                                  acts,a->instr->cfun->nparams);
    for (i = 0; i < a->instr->cfun->nparams; i++) {
      assert(NULL != a->values[i]);
      assert(NULL != acts[i]);
      df_set_input(acts[i],0,a->values[i]);
    }

    if (0 == a->instr->cfun->nparams)
      df_set_input(start,0,a->values[0]);
    else
      df_set_input(start,0,df_value_ref(a->values[0]));

    break;
  }
  case OP_SPECIAL_MAP: {
    df_activity **acts = (df_activity**)alloca(a->instr->cfun->nparams*sizeof(df_activity*));
    list *values = df_sequence_to_list(a->values[0]);
    list *l;
    int i;
    df_activity *ultimate_desta = a->outports[0].a;
    int ultimate_destp = a->outports[0].p;

    if (NULL == values) {
      df_seqtype *empty = df_seqtype_new(SEQTYPE_EMPTY);
      df_value *ev = df_value_new(empty);
      df_seqtype_deref(empty);
      df_output_value(a,0,ev);
    }

/*     printf("Map values:\n"); */
    for (l = values; l; l = l->next) {
      df_value *v = (df_value*)l->data;

      df_activity *start;

      df_activity *desta = ultimate_desta;
      int destp = ultimate_destp;

      if (l->next) {
        df_activity *seq = (df_activity*)calloc(1,sizeof(df_activity));
        list_push(&state->activities,seq);
        assert(NULL != a->instr->fun->mapseq);
        seq->instr = a->instr->fun->mapseq;
        seq->remaining = 2;
        seq->values = (df_value**)calloc(2,sizeof(df_value*));
        seq->outports = (df_actdest*)calloc(1,sizeof(df_actdest));
        seq->outports[0].a = ultimate_desta;
        seq->outports[0].p = ultimate_destp;
        seq->actno = a->actno;
        seq->refs = 2;

        desta = seq;
        destp = 0;
        ultimate_desta = seq;
        ultimate_destp = 1;
      }

      memset(acts,0,a->instr->cfun->nparams*sizeof(df_activity*));
      start = df_activate_function(state,a->instr->cfun,desta,destp,
                                    acts,a->instr->cfun->nparams);

      df_set_input(start,0,df_value_ref(v));
      for (i = 0; i < a->instr->cfun->nparams; i++) {
        assert(NULL != a->values[i+1]);
        assert(NULL != acts[i]);
        df_set_input(acts[i],0,df_value_ref(a->values[i+1]));
      }

/*       printf("  "); */
/*       df_value_print(g,stdout,v); */
/*       printf("\n"); */
    }

    for (i = 0; i < a->instr->ninports; i++)
      df_value_deref(g,a->values[i]);

    list_free(values,NULL);
    break;
  }
  case OP_SPECIAL_PASS:
    df_output_value(a,0,a->values[0]);
    break;
  case OP_SPECIAL_SWALLOW:
    df_value_deref(g,a->values[0]);
    break;
  case OP_SPECIAL_RETURN:
    df_output_value(a,0,a->values[0]);
    break;
  case OP_SPECIAL_PRINT: {

    if (SEQTYPE_EMPTY != a->values[0]->seqtype->type) {
      stringbuf *buf = stringbuf_new();
      df_seroptions *options = df_seroptions_new(nsname_temp(NULL,"xml"));

      if (0 != df_serialize(g,a->values[0],buf,options,&state->ei)) {
        df_value_deref(g,a->values[0]);
        df_seroptions_free(options);
        return -1;
      }

      printf("%s",buf->data);

      stringbuf_free(buf);
      df_seroptions_free(options);
    }

    df_value_deref(g,a->values[0]);
    break;
  }
  case OP_SPECIAL_OUTPUT: {
    stringbuf *buf = stringbuf_new();
    df_value *empty;

    assert(NULL != a->instr->seroptions);

    if (0 != df_serialize(g,a->values[0],buf,a->instr->seroptions,&state->ei)) {
      df_value_deref(g,a->values[0]);
      return -1;
    }

    printf("%s",buf->data);

    stringbuf_free(buf);
    df_value_deref(g,a->values[0]);

    empty = df_list_to_sequence(g,NULL);
    df_output_value(a,0,empty);
    break;
  }
  case OP_SPECIAL_SEQUENCE: {
    df_value *pair;
    assert(SEQTYPE_SEQUENCE == a->instr->outports[0].seqtype->type);
    pair = df_value_new(a->instr->outports[0].seqtype);
    pair->value.pair.left = a->values[0];
    pair->value.pair.right = a->values[1];
    df_output_value(a,0,pair);

/*     printf("OP_SPECIAL_SEQUENCE: created "); */
/*     df_value_print(g,stdout,pair); */
/*     printf("\n"); */
    break;
  }
  case OP_SPECIAL_GATE:
    df_output_value(a,0,a->values[0]);
    df_value_deref(g,a->values[1]);
    break;
  case OP_SPECIAL_DOCUMENT: {
    df_node *docnode = df_node_new(NODE_DOCUMENT);
    df_value *docval = df_value_new(a->instr->outports[0].seqtype);
    list *values = df_sequence_to_list(a->values[0]);
    docval->value.n = docnode;
    assert(0 == docval->value.n->refcount);
    docval->value.n->refcount++;

    add_sequence_as_child_nodes(g,values,docnode);

    if (NULL != docnode->attributes) {
      error(&state->ei,"",0,"XTDE0420",
            "Attribute nodes cannot be added directly as children of a document");
      df_value_deref(g,docval);
      list_free(values,NULL);
      return -1;
    }

    df_output_value(a,0,docval);

    df_value_deref(g,a->values[0]);
    list_free(values,NULL);
    break;
  }
  case OP_SPECIAL_ELEMENT: {
    list *values = df_sequence_to_list(a->values[0]);
    df_value *elemvalue = df_value_new(a->instr->outports[0].seqtype);
    df_node *elem = df_node_new(NODE_ELEMENT);
    assert(a->instr->outports[0].seqtype->type == SEQTYPE_ITEM);
    assert(a->instr->outports[0].seqtype->item->kind == ITEM_ELEMENT);

    elemvalue->value.n = elem;
    assert(0 == elemvalue->value.n->refcount);
    elemvalue->value.n->refcount++;

    assert(df_check_derived_atomic_type(a->values[1],g->string_type));
    elem->ident.name = strdup(a->values[1]->value.s);

    /* FIXME: namespace (should be received on an input port) - what about prefix? */

    debug("OP_SPECIAL_ELEMENT %p (\"%s\"): %d items in child sequence\n",
          elemvalue,elem->ident.name,list_count(values));
    add_sequence_as_child_nodes(g,values,elem);

    df_output_value(a,0,elemvalue);
    df_value_deref(g,a->values[0]);
    df_value_deref(g,a->values[1]);

    list_free(values,NULL);
    break;
  }
  case OP_SPECIAL_ATTRIBUTE: {
    df_value *attrvalue = df_value_new(a->instr->outports[0].seqtype);
    df_node *attr = df_node_new(NODE_ATTRIBUTE);
    assert(a->instr->outports[0].seqtype->type == SEQTYPE_ITEM);
    assert(a->instr->outports[0].seqtype->item->kind == ITEM_ATTRIBUTE);
    attrvalue->value.n = attr;
    assert(0 == attrvalue->value.n->refcount);
    attrvalue->value.n->refcount++;

    assert(df_check_derived_atomic_type(a->values[1],g->string_type));
    attr->ident.name = strdup(a->values[1]->value.s);
    attr->value = df_construct_simple_content(g,a->values[0]);

    df_output_value(a,0,attrvalue);
    df_value_deref(g,a->values[0]);
    df_value_deref(g,a->values[1]);
    break;
  }
  case OP_SPECIAL_PI:
    /* FIXME */
    break;
  case OP_SPECIAL_COMMENT:
    /* FIXME */
    break;
  case OP_SPECIAL_VALUE_OF:
    /* FIXME */
    break;
  case OP_SPECIAL_TEXT:
    /* FIXME */
    break;
  case OP_SPECIAL_NAMESPACE:
    /* FIXME */
    break;
  case OP_SPECIAL_SELECT: {
    list *values = df_sequence_to_list(a->values[0]);
    list *output = NULL;
    list *l;

    for (l = values; l; l = l->next) {
      df_value *v = (df_value*)l->data;
      /* FIXME: raise a dynamic error here instead of asserting */
      if ((SEQTYPE_ITEM != v->seqtype->type) || (ITEM_ATOMIC == v->seqtype->item->kind)) {
        fprintf(stderr,"ERROR: non-node in select input\n");
        assert(0);
      }
      if (ITEM_ATOMIC != v->seqtype->item->kind) {
        CHECK_CALL(append_matching_nodes(v->value.n,a->instr->nametest,a->instr->seqtypetest,
                                         a->instr->axis,&output))
      }
    }

    list_free(values,NULL);
    df_output_value(a,0,df_list_to_sequence(g,output));
    df_value_deref(g,a->values[0]);
    df_value_deref_list(g,output);
    list_free(output,NULL);
    break;
  }
  case OP_SPECIAL_FILTER: {
#if 0
    list *values = df_sequence_to_list(a->values[0]);
    list *mask = df_sequence_to_list(a->values[1]);
    list *output = NULL;
    list *vl;
    list *ml;

    assert(list_count(values) == list_count(mask));

    /* FIXME */
    vl = values;
    ml = mask;
    while (vl) {
      df_value *v = (df_value*)vl->data;
      df_value *m = (df_value*)ml->data;

      vl = vl->next;
      ml = ml->next;
    }
#endif

    break;
  }
  case OP_SPECIAL_EMPTY:
    df_output_value(a,0,df_value_new(a->instr->outports[0].seqtype));
    df_value_deref(g,a->values[0]);
    break;
  case OP_SPECIAL_CONTAINS_NODE: {
    list *nodevals = df_sequence_to_list(a->values[0]);
    df_value *result = df_value_new(a->instr->outports[0].seqtype);
    df_node *n = a->values[1]->value.n;
    int found = 0;
    list *l;
    assert(SEQTYPE_ITEM == a->values[1]->seqtype->type);
    assert(ITEM_ATOMIC != a->values[1]->seqtype->item->kind);

    for (l = nodevals; l; l = l->next) {
      df_value *nv = (df_value*)l->data;
      assert(SEQTYPE_ITEM == nv->seqtype->type);
      assert(ITEM_ATOMIC != nv->seqtype->item->kind);
      if (nv->value.n == n)
        found = 1;
    }

    result->value.b = found;
    df_output_value(a,0,result);
    list_free(nodevals,NULL);
    df_value_deref(g,a->values[0]);
    df_value_deref(g,a->values[1]);
    break;
  }
  default: {
    df_value *result = NULL;
    int i;
    CHECK_CALL(df_execute_op(state,a->instr,a->instr->opcode,a->values,&result))
    for (i = 0; i < a->instr->ninports; i++)
      df_value_deref(state->program->schema->globals,a->values[i]);
    assert(NULL != result);
    df_output_value(a,0,result);
    break;
  }
  }

  return 0;
}

int df_execute_network(df_state *state)
{
  int found;
  do {
    list **listptr = &state->activities;
    found = 0;
    while (NULL != *listptr) {
      list *l = *listptr;
      df_activity *a = (df_activity*)((*listptr)->data);

      if ((0 == a->remaining) && !a->fired) {
        if (0 != df_fire_activity(state,a))
          return -1;
        a->fired = 1;
        found = 1;
      }

      else if (0 == a->refs) {
        /* just remove it */
        *listptr = (*listptr)->next;
        found = 1;
        free_activity(state,a);
        free(l);
        break;
      }
      listptr = &(*listptr)->next;
    }
  } while (found);


  if (NULL != state->activities) {
    list *l;
    fprintf(stderr,"The following activities remain outstanding:\n");
    for (l = state->activities; l; l = l->next) {
      df_activity *a = (df_activity*)l->data;
      fprintf(stderr,"[%d] %s.%d - %s\n",
             a->actno,a->instr->fun->ident.name,a->instr->id,df_opstr(a->instr->opcode));
    }
  }
  return 0;
}

int df_execute(df_program *program, int trace, error_info *ei, df_value *context)
{
  df_function *mainfun;
  df_function *init;
  df_activity *print;
  df_activity *start;
  df_state *state = NULL;
  int r;

  mainfun = df_lookup_function(program,nsname_temp(GX_NAMESPACE,"main"));
  if (NULL == mainfun)
    mainfun = df_lookup_function(program,nsname_temp(NULL,"main"));
  if (NULL == mainfun)
    mainfun = df_lookup_function(program,nsname_temp(NULL,"default"));

  if (NULL == mainfun) {
    fprintf(stderr,"No main function defined\n");
    return -1;
  }

  init = (df_function*)calloc(1,sizeof(df_function));
  init->ident = nsname_new(NULL,"_init");
  init->program = program;
  init->start = df_add_instruction(program,init,OP_SPECIAL_PRINT);
  init->rtype = df_seqtype_new_atomic(program->schema->globals->complex_ur_type);

  state = df_state_new(program);
  state->trace = trace;

  print = (df_activity*)calloc(1,sizeof(df_activity));
  list_push(&state->activities,print);
  print->instr = init->start;
  print->values = (df_value**)calloc(1,sizeof(df_value*));
  print->remaining = 1;
  print->refs++;

  /* FIXME: support main functions with >0 params (where do we get the values of these from?) */
  assert(0 == mainfun->nparams);

  start = df_activate_function(state,mainfun,print,0,NULL,0);

  if (NULL == context) {
    context = df_value_new(state->intype);
    context->value.i = 0;
  }
  df_set_input(start,0,context);

  r = df_execute_network(state);

  df_state_free(state);

  df_free_function(init);

  return r;
}
