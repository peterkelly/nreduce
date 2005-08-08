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

#include "engine.h"
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

void free_activity(df_state *state, df_activity *a)
{
  if (!a->fired) {
    int i;
    for (i = 0; i < a->instr->ninports; i++)
      if (NULL != a->values[i])
        df_value_deref(state->schema->globals,a->values[i]);
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
                  fun->name,instr->id,i);
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
         a->actno,a->instr->fun->name,a->instr->id,df_opstr(a->instr->opcode));
}

void df_deref_activity(df_activity *a, int indent)
{
  int i;
  a->refs--;
  assert(0 <= a->refs);
  if ((OP_SPECIAL_RETURN == a->instr->opcode) || (OP_SPECIAL_MERGE == a->instr->opcode))
    return;
  if (0 == a->refs) {
    for (i = 0; i < a->instr->noutports; i++)
      df_deref_activity(a->outports[i].a,indent+1);
  }
}

static void get_seq_values(df_value *v, list **values)
{
  if (SEQTYPE_SEQUENCE == v->seqtype->type) {
    get_seq_values(v->value.pair.left,values);
    get_seq_values(v->value.pair.right,values);
  }
  else if (SEQTYPE_ITEM == v->seqtype->type) {
    list_append(values,v);
  }
  else {
    assert(SEQTYPE_EMPTY == v->seqtype->type);
  }
}

void df_node_to_string(df_node *n, stringbuf *buf)
{
  switch (n->type) {
  case NODE_DOCUMENT:
    /* FIXME */
    break;
  case NODE_ELEMENT: {
    df_node *c;
    for (c = n->first_child; c; c = c->next)
      df_node_to_string(c,buf);
    break;
  }
  case NODE_ATTRIBUTE:
    stringbuf_printf(buf,n->value);
    break;
  case NODE_PI:
    /* FIXME */
    break;
  case NODE_COMMENT:
    /* FIXME */
    break;
  case NODE_TEXT:
    stringbuf_printf(buf,n->value);
    break;
  default:
    assert(!"invalid node type");
    break;
  }
}

df_value *df_atomize(xs_globals *g, df_value *v)
{
  if (SEQTYPE_SEQUENCE == v->seqtype->type) {
    df_seqtype *atomicseq;
    df_value *atom;
    df_value *left = df_atomize(g,v->value.pair.left);
    df_value *right = df_atomize(g,v->value.pair.right);

    atomicseq = df_seqtype_new(SEQTYPE_SEQUENCE);
    atomicseq->left = df_seqtype_ref(left->seqtype);
    atomicseq->right = df_seqtype_ref(right->seqtype);

    atom = df_value_new(atomicseq);
    atom->value.pair.left = left;
    atom->value.pair.right = right;
    return atom;
  }
  else if (SEQTYPE_ITEM == v->seqtype->type) {
    if (ITEM_ATOMIC == v->seqtype->item->kind) {
      return df_value_ref(v);
    }
    else {
      /* FIXME: this is just a quick and dirty implementation of node atomization... need to
         follow the rules set out in XPath 2.0 section 2.5.2 */
      df_seqtype *strtype = df_seqtype_new_item(ITEM_ATOMIC);
      df_value *atom = df_value_new(strtype);
      stringbuf *buf = stringbuf_new();
      strtype->item->type = g->string_type;
      df_node_to_string(v->value.n,buf);
      atom->value.s = strdup(buf->data);
      stringbuf_free(buf);
      return atom;
    }
  }
  else {
    assert(SEQTYPE_EMPTY == v->seqtype->type);
    /* FIXME: is this safe? are there any situations in which df_atomize could be called with
       an empty sequence? */
    assert(!"can't atomize an empty sequence");
    return NULL;
  }
}

char *df_value_as_string(xs_globals *g, df_value *v)
{
  df_value *atomicv;
  char *str;
  stringbuf *buf = stringbuf_new();

  if (SEQTYPE_EMPTY != v->seqtype->type) {
    if ((SEQTYPE_ITEM != v->seqtype->type) ||
        (ITEM_ATOMIC != v->seqtype->item->kind))
      atomicv = df_atomize(g,v);
    else
      atomicv = df_value_ref(v);
  }

  df_value_printbuf(g,buf,v);


  str = strdup(buf->data);

  df_value_deref(g,v);
  stringbuf_free(buf);
  return str;
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
  list *values = NULL;
  list *strings = NULL;
  list *l;
  char *separator = " "; /* FIXME: allow custom separators */
  char *nextsep = "";

  get_seq_values(v,&values);

  for (l = values; l; l = l->next) {
    df_value *v = (df_value*)l->data;
    stringbuf_printf(buf,"%s",nextsep);
    if (is_text_node(v)) {
      if (0 == strlen(v->value.n->value))
        continue;
      stringbuf_printf(buf,v->value.n->value);
      if (l->next && (is_text_node((df_value*)l->next->data)))
        nextsep = "";
      else
        nextsep = separator;
    }
    else {
      char *str = df_value_as_string(g,v);
      stringbuf_printf(buf,str);
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

static df_value *list_to_sequence(xs_globals *g, list *values)
{
  list *l;
  df_value *result = NULL;
  if (NULL == values)
    return df_value_new(df_seqtype_new(SEQTYPE_EMPTY));

  for (l = values; l; l = l->next) {
    df_value *v = (df_value*)l->data;
    if (NULL == result) {
      result = df_value_ref(v);
    }
    else {
      df_seqtype *st = df_seqtype_new(SEQTYPE_SEQUENCE);
      df_value *newresult = df_value_new(st);
      df_seqtype_deref(st);
      st->left = df_seqtype_ref(result->seqtype);
      st->right = df_seqtype_ref(v->seqtype);
      newresult->value.pair.left = result;
      newresult->value.pair.right = df_value_ref(v);
      result = newresult;
    }
  }
  return result;
}

static int nodetest_matches(df_node *n, char *nametest, df_seqtype *seqtypetest)
{
  if (NULL != nametest) {
    return (((NODE_ELEMENT == n->type) || (NODE_ATTRIBUTE == n->type)) &&
            !strcmp(n->name,nametest));
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
                  ns_name_equals(n->ns,n->name,
                                 seqtypetest->item->elem->ns,seqtypetest->item->elem->name));
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
      default:
        assert(!"invalid item type");
        break;
      }
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
    printf("append_matching_nodes: AXIS_ATTRIBUTE: element %s, have %d attributes\n",
           self->name,list_count(self->attributes));
    for (l = self->attributes; l; l = l->next) {
      df_node *attr = (df_node*)l->data;
      if (nodetest_matches(attr,nametest,seqtypetest)) {
        df_value *val = df_node_to_value(attr);
        list_append(matches,val);
        debug("adding matching attribute %p; root %p refcount is now %d",
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

/*   debug("add_sequence_as_child_nodes: values are:"); */

/*   for (l = values; l; l = l->next) { */
/*     df_value *v = (df_value*)l->data; */
/*     assert(SEQTYPE_ITEM == v->seqtype->type); */
/*     debug("  %s",itemtypes[v->seqtype->item->kind]); */
/*   } */

  for (l = values; l; l = l->next) {
    df_value *v = (df_value*)l->data;
    assert(SEQTYPE_ITEM == v->seqtype->type);
    switch (v->seqtype->item->kind) {
    case ITEM_ATOMIC: {
      df_node *textnode = df_atomic_value_to_text_node(g,v);
      debug("created text node %p for atomic value %p (which has %d refs)",textnode,v,v->refcount);
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
        debug("***** adding existing attribute node");
        v->value.n->refcount = 0;
        df_node_add_attribute(parent,v->value.n);
        v->value.n = NULL;
      }
      else {
        debug("!!! copying attribute node");
        df_node_add_attribute(parent,df_node_deep_copy(v->value.n));
      }
      break;
    case ITEM_ELEMENT:
    case ITEM_PI:
    case ITEM_COMMENT:
    case ITEM_TEXT:
      if ((1 == v->refcount) && (1 == v->value.n->refcount)) {
        debug("***** adding existing text node");
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
  xs_globals *g = state->schema->globals;
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
      df_deref_activity(a->outports[1].a,0);
    }
    else {
      df_output_value(a,1,a->values[1]);
      df_deref_activity(a->outports[0].a,0);
    }
    df_value_deref(g,a->values[0]);
    break;
  case OP_SPECIAL_MERGE:
    df_output_value(a,0,a->values[0]);
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
    list *values = NULL;
    list *l;
    int i;
    df_activity *ultimate_desta = a->outports[0].a;
    int ultimate_destp = a->outports[0].p;
    get_seq_values(a->values[0],&values);

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
  case OP_SPECIAL_PRINT:
/*     printf("Program result:\n"); */
    df_value_print(g,stdout,a->values[0]);
    if ((SEQTYPE_ITEM != a->values[0]->seqtype->type) ||
        (ITEM_ATOMIC == a->values[0]->seqtype->item->kind))
      printf("\n");

    debug("OP_SPECIAL_PRINT: before dereference of a->values[0]");
    df_value_deref(g,a->values[0]);
    debug("OP_SPECIAL_PRINT: after dereference of a->values[0]");
    break;
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
    list *values = NULL;
    docval->value.n = docnode;
    assert(0 == docval->value.n->refcount);
    docval->value.n->refcount++;

    get_seq_values(a->values[0],&values);

    add_sequence_as_child_nodes(g,values,docnode);

    if (NULL != docnode->attributes) {
      set_error_info2(&state->ei,"",0,"XTDE0420",
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
    list *values = NULL;
    df_value *elemvalue = df_value_new(a->instr->outports[0].seqtype);
    df_node *elem = df_node_new(NODE_ELEMENT);
    assert(a->instr->outports[0].seqtype->type == SEQTYPE_ITEM);
    assert(a->instr->outports[0].seqtype->item->kind == ITEM_ELEMENT);
    get_seq_values(a->values[0],&values);

    elemvalue->value.n = elem;
    assert(0 == elemvalue->value.n->refcount);
    elemvalue->value.n->refcount++;

    assert(df_check_derived_atomic_type(state,a->values[1],g->string_type));
    elem->name = strdup(a->values[1]->value.s);

    /* FIXME: namespace (should be received on an input port) - what about prefix? */

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

    assert(df_check_derived_atomic_type(state,a->values[1],g->string_type));
    attr->name = strdup(a->values[1]->value.s);
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
    list *values = NULL;
    list *output = NULL;
    list *l;
    get_seq_values(a->values[0],&values);

    printf("OP_SPECIAL_SELECT firing: %d values in input sequence\n",list_count(values));
    for (l = values; l; l = l->next) {
      df_value *v = (df_value*)l->data;
      /* FIXME: raise a dynamic error here instead of asserting */
      assert((SEQTYPE_ITEM == v->seqtype->type) && (ITEM_ATOMIC != v->seqtype->item->kind));
/*       printf("item kind: %d\n",v->seqtype->item->kind); */
      if ((ITEM_ELEMENT == v->seqtype->item->kind) ||
          (ITEM_DOCUMENT == v->seqtype->item->kind)) {
/*         printf("element name: %s\n",v->value.n->name); */

        CHECK_CALL(append_matching_nodes(v->value.n,a->instr->nametest,a->instr->seqtypetest,
                                         a->instr->axis,&output))
      }
    }

    list_free(values,NULL);
    df_output_value(a,0,list_to_sequence(g,output));
    df_value_deref(g,a->values[0]);
    df_value_deref_list(g,output);
    list_free(output,NULL);
    break;
  }
  case OP_SPECIAL_FILTER: {
#if 0
    list *values = NULL;
    list *mask = NULL;
    list *output = NULL;
    list *vl;
    list *ml;
    get_seq_values(a->values[0],&values);
    get_seq_values(a->values[0],&mask);

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
  default: {
    df_value *result = NULL;
    int i;
    CHECK_CALL(df_execute_op(state,a->instr,a->instr->opcode,a->values,&result))
    for (i = 0; i < a->instr->ninports; i++)
      df_value_deref(state->schema->globals,a->values[i]);
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
             a->actno,a->instr->fun->name,a->instr->id,df_opstr(a->instr->opcode));
    }
  }
  return 0;
}

int df_execute(df_state *state, int trace, error_info *ei)
{
  df_function *mainfun;
  df_activity *print;
  df_activity *start;
  df_value *invalue;

  state->trace = trace;

  if (NULL == (mainfun = df_lookup_function(state,"main"))) {
    fprintf(stderr,"No main function defined\n");
    return -1;
  }

  print = (df_activity*)calloc(1,sizeof(df_activity));
  list_push(&state->activities,print);
  print->instr = state->init->start;
  print->values = (df_value**)calloc(1,sizeof(df_value*));
  print->remaining = 1;
  print->refs++;

  /* FIXME: support main functions with >0 params (where do we get the values of these from?) */
  assert(0 == mainfun->nparams);

  start = df_activate_function(state,mainfun,print,0,NULL,0);

  invalue = df_value_new(state->intype);
  invalue->value.i = 0;
  df_set_input(start,0,invalue);

  return df_execute_network(state);
}
