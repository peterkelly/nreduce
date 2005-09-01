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

df_state *df_state_new(df_program *program, error_info *ei)
{
  df_state *state = (df_state*)calloc(1,sizeof(df_state));
  state->program = program;
  state->intype = df_seqtype_new_atomic(program->schema->globals->int_type);
  state->ei = ei;
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


static void remove_zero_length_text_nodes(xs_globals *g, list **values)
{
  list **lptr = values;
  while (*lptr) {
    df_value *v = (df_value*)((*lptr)->data);
    if ((ITEM_TEXT == v->seqtype->item->kind) && (0 == strlen(v->value.n->value))) {
      list *del = *lptr;
      *lptr = (*lptr)->next;
      free(del);
      df_value_deref(g,v);
    }
    else {
      lptr = &((*lptr)->next);
    }
  }
}

static void merge_adjacent_text_nodes(xs_globals *g, list **values)
{
  list **lptr = values;
  while (*lptr) {
    df_value *v = (df_value*)((*lptr)->data);

    if (ITEM_TEXT == v->seqtype->item->kind) {
      stringbuf *buf = stringbuf_new();
      list **nextptr = &((*lptr)->next);
      df_node *textnode;

      stringbuf_format(buf,"%s",v->value.n->value);

      while (*nextptr && (ITEM_TEXT == ((df_value*)((*nextptr)->data))->seqtype->item->kind)) {
        df_value *v2 = (df_value*)((*nextptr)->data);
        list *del = *nextptr;
        stringbuf_format(buf,"%s",v2->value.n->value);
        df_value_deref(g,v2);
        *nextptr = (*nextptr)->next;
        free(del);
      }

      df_value_deref(g,v);

      textnode = df_node_new(NODE_TEXT);
      textnode->value = strdup(buf->data);
      (*lptr)->data = df_node_to_value(textnode);

      stringbuf_free(buf);
    }

    lptr = &((*lptr)->next);
  }
}

static int df_construct_complex_content(error_info *ei, df_instruction *instr, xs_globals *g,
                                        list *values, df_node *parent, list *ns_defs)
{
  list *l;
  list **lptr;
  int havenotnsattr = 0;

  values = list_copy(values,(list_copy_t)df_value_ref);

  /*

  @implements(xslt20:sequence-constructors-1) @end
  @implements(xslt20:sequence-constructors-2) @end
  @implements(xslt20:sequence-constructors-3) @end
  @implements(xslt20:sequence-constructors-4) @end
  @implements(xslt20:sequence-constructors-5) @end
  @implements(xslt20:sequence-constructors-6) @end
  @implements(xslt20:sequence-constructors-7) @end

  @implements(xslt20:constructing-complex-content-1) @end
  @implements(xslt20:constructing-complex-content-3) @end
  @implements(xslt20:constructing-complex-content-4)
  status { partial }
  issue { need support for namespace fixup }
  issue { need support for namespace inheritance }
  test { xslt/sequence/complex10.test }
  test { xslt/sequence/complex11.test }
  test { xslt/sequence/complex12.test }
  test { xslt/sequence/complex13.test }
  test { xslt/sequence/complex14.test }
  test { xslt/sequence/complex15.test }
  test { xslt/sequence/complex16.test }
  test { xslt/sequence/complex17.test }
  test { xslt/sequence/complex18.test }
  test { xslt/sequence/complex1.test }
  test { xslt/sequence/complex2.test }
  test { xslt/sequence/complex3.test }
  test { xslt/sequence/complex4.test }
  test { xslt/sequence/complex5.test }
  test { xslt/sequence/complex6.test }
  test { xslt/sequence/complex7.test }
  test { xslt/sequence/complex8.test }
  test { xslt/sequence/complex9.test }
  @end
  @implements(xslt20:constructing-complex-content-5)
  test { xslt/sequence/complexex.test }
  @end

  */


  /* XSLT 2.0: 5.7.1 Constructing Complex Content */

  /* 1. The containing instruction may generate attribute nodes and/or namespace nodes, as
     specified in the rules for the individual instruction. For example, these nodes may be
     produced by expanding an [xsl:]use-attribute-sets attribute, or by expanding the attributes
     of a literal result element. Any such nodes are prepended to the sequence produced by
     evaluating the sequence constructor. */

  /* FIXME */

  /* 2. Any atomic value in the sequence is cast to a string.

     Note: Casting from xs:QName or xs:NOTATION to xs:string always succeeds, because these values
     retain a prefix for this purpose. However, there is no guarantee that the prefix used will
     always be meaningful in the context where the resulting string is used. */

  /* FIXME */
  for (l = values; l; l = l->next) {
    df_value *v = (df_value*)l->data;
    assert(SEQTYPE_ITEM == v->seqtype->type);
    if (ITEM_ATOMIC == v->seqtype->item->kind) {
      char *str = df_value_as_string(g,v);
      df_value *newv = df_value_new_string(g,str);
      free(str);
      df_value_deref(g,v);
      l->data = newv;
    }
  }

  /* 3. Any consecutive sequence of strings within the result sequence is converted to a single
     text node, whose string value contains the content of each of the strings in turn, with a
     single space (#x20) used as a separator between successive strings. */
  lptr = &values;
  while (*lptr) {
    df_value *v = (df_value*)((*lptr)->data);
    if (ITEM_ATOMIC == v->seqtype->item->kind) {
      /* Must be a string; see previous step */
      stringbuf *buf = stringbuf_new();
      list **nextptr = &((*lptr)->next);
      df_node *textnode;

      stringbuf_format(buf,"%s",v->value.s);

      while (*nextptr && (ITEM_ATOMIC == ((df_value*)((*nextptr)->data))->seqtype->item->kind)) {
        df_value *v2 = (df_value*)((*nextptr)->data);
        list *del = *nextptr;
        stringbuf_format(buf," %s",v2->value.s);
        df_value_deref(g,v2);
        *nextptr = (*nextptr)->next;
        free(del);
      }

      df_value_deref(g,v);

      textnode = df_node_new(NODE_TEXT);
      textnode->value = strdup(buf->data);
      (*lptr)->data = df_node_to_value(textnode);

      stringbuf_free(buf);
    }
    lptr = &((*lptr)->next);
  }

  /* 4. Any document node within the result sequence is replaced by a sequence containing each of
     its children, in document order. */
  lptr = &values;
  while (*lptr) {
    df_value *v = (df_value*)((*lptr)->data);
    if (ITEM_DOCUMENT == v->seqtype->item->kind) {
      list *del = *lptr;
      df_node *doc = v->value.n;
      df_node *c;

      *lptr = (*lptr)->next;
      free(del);

      for (c = doc->first_child; c; c = c->next) {
        df_value *newv = df_node_to_value(c);
        list_push(lptr,newv);
        lptr = &((*lptr)->next);
      }

      df_value_deref(g,v);
    }
    else {
      lptr = &((*lptr)->next);
    }
  }

  /* 5. Zero-length text nodes within the result sequence are removed. */
  remove_zero_length_text_nodes(g,&values);

  /* 6. Adjacent text nodes within the result sequence are merged into a single text node. */
  merge_adjacent_text_nodes(g,&values);

  /* 7. Invalid namespace and attribute nodes are detected as follows. */

  for (l = values; l; l = l->next) {
    df_value *v = (df_value*)l->data;
    if ((ITEM_NAMESPACE == v->seqtype->item->kind) || (ITEM_ATTRIBUTE == v->seqtype->item->kind)) {

      int err = 0;


      /* [ERR XTDE0410] It is a non-recoverable dynamic error if the result sequence used to
         construct the content of an element node contains a namespace node or attribute node that
         is preceded in the sequence by a node that is neither a namespace node nor an attribute
         node. */
      if (havenotnsattr) {
        error(ei,instr->sloc.uri,instr->sloc.line,"XTDE0410","Namespaces and attributes can only "
              "appear at the start of a sequence used to construct complex content");
        err = 1;
      }


      /* [ERR XTDE0420] It is a non-recoverable dynamic error if the result sequence used to
         construct the content of a document node contains a namespace node or attribute node. */
      if (NODE_DOCUMENT == parent->type) {
        /* FIXME: write test for this; requires support for xsl:document */
        error(ei,instr->sloc.uri,instr->sloc.line,"XTDE0420","A sequence used to construct the "
              "content of a document node cannot contain namespace or attribute nodes");
        err = 1;
      }

      /* [ERR XTDE0430] It is a non-recoverable dynamic error if the result sequence contains two
         or more namespace nodes having the same name but different string values (that is,
         namespace nodes that map the same prefix to different namespace URIs). */
      if (ITEM_NAMESPACE == v->seqtype->item->kind) {
        list *l2;
        for (l2 = values; l2 != l; l2 = l2->next) {
          df_value *v2 = (df_value*)l2->data;
          if ((ITEM_NAMESPACE == v2->seqtype->item->kind) &&
              !strcmp(v->value.n->prefix,v2->value.n->prefix) &&
              strcmp(v->value.n->value,v2->value.n->value)) {
            error(ei,instr->sloc.uri,instr->sloc.line,"XTDE0430","Sequence contains two namespace "
                  "nodes trying to map the prefix \"%s\" to different URIs (\"%s\" and \"%s\")",
                  v->value.n->prefix,v->value.n->value,v2->value.n->value);
            err = 1;
            break;
          }
        }
      }

      /* [ERR XTDE0440] It is a non-recoverable dynamic error if the result sequence contains a
         namespace node with no name and the element node being constructed has a null namespace
         URI (that is, it is an error to define a default namespace when the element is in no
         namespace). */
      if ((NODE_ELEMENT == parent->type) && (NULL == parent->ident.ns) &&
          (ITEM_NAMESPACE == v->seqtype->item->kind) && (NULL == v->value.n->prefix)) {
        error(ei,instr->sloc.uri,instr->sloc.line,"XTDE0440","Sequence contains a namespace node "
              "with no name and the element node being constructed has a null namespace URI");
        err = 1;
      }

      if (err) {
        df_value_deref_list(g,values);
        list_free(values,NULL);
        return -1;
      }

    }
    else {
      havenotnsattr = 1;
    }
  }

  /* 8. If the result sequence contains two or more namespace nodes with the same name (or no name)
        and the same string value (that is, two namespace nodes mapping the same prefix to the same
        namespace URI), then all but one of the duplicate nodes are discarded.

      Note: Since the order of namespace nodes is undefined, it is not significant which of the
      duplicates is retained. */
  lptr = &values;
  while (*lptr) {
    df_value *v = (df_value*)((*lptr)->data);
    int removed = 0;
    if (ITEM_NAMESPACE == v->seqtype->item->kind) {
      list *l2;
      for (l2 = values; l2 != *lptr; l2 = l2->next) {
        df_value *v2 = (df_value*)l2->data;
        if ((ITEM_NAMESPACE == v2->seqtype->item->kind) &&
            !strcmp(v->value.n->prefix,v2->value.n->prefix)) {
          list *del = *lptr;
          *lptr = (*lptr)->next;
          df_value_deref(g,v);
          free(del);
          removed = 1;
          break;
        }
      }
    }
    if (!removed)
      lptr = &((*lptr)->next);
  }

  /* 9. If an attribute A in the result sequence has the same name as another attribute B that
        appears later in the result sequence, then attribute A is discarded from the result
        sequence. */
  lptr = &values;
  while (*lptr) {
    df_value *v = (df_value*)((*lptr)->data);
    int removed = 0;

    if (ITEM_ATTRIBUTE == v->seqtype->item->kind) {
      list *l2;
      for (l2 = (*lptr)->next; l2; l2 = l2->next) {
        df_value *v2 = (df_value*)l2->data;
        if ((ITEM_ATTRIBUTE == v2->seqtype->item->kind) &&
            nsname_equals(v->value.n->ident,v2->value.n->ident)) {
          list *del = *lptr;
          *lptr = (*lptr)->next;
          df_value_deref(g,v);
          free(del);
          removed = 1;
          break;
        }
      }
    }

    if (!removed)
      lptr = &((*lptr)->next);
  }

  /* FIXME */

  /* 10. Each node in the resulting sequence is attached as a namespace, attribute, or child of the
     newly constructed element or document node. Conceptually this involves making a deep copy of
     the node; in practice, however, copying the node will only be necessary if the existing node
     can be referenced independently of the parent to which it is being attached. When copying an
     element or processing instruction node, its base URI property is changed to be the same as
     that of its new parent, unless it has an xml:base attribute (see [XMLBASE]) that overrides
     this. If the element has an xml:base attribute, its base URI is the value of that attribute,
     resolved (if it is relative) against the base URI of the new parent node. */

  for (l = values; l; l = l->next) {
    df_value *v = (df_value*)l->data;
    /* FIXME: avoid copying here where possible */
    assert(ITEM_ATOMIC != v->seqtype->item->kind);

    if (ITEM_ATTRIBUTE == v->seqtype->item->kind)
      df_node_add_attribute(parent,df_node_deep_copy(v->value.n));
    else if (ITEM_NAMESPACE == v->seqtype->item->kind)
      df_node_add_namespace(parent,df_node_deep_copy(v->value.n));
    else
      df_node_add_child(parent,df_node_deep_copy(v->value.n));
  }

  /* 11. If the newly constructed node is an element node, then namespace fixup is applied to this
     node, as described in 5.7.3 Namespace Fixup. */

  /* FIXME */

  /* 12. If the newly constructed node is an element node, and if namespaces are inherited, then
     each namespace node of the newly constructed element (including any produced as a result of
     the namespace fixup process) is copied to each descendant element of the newly constructed
     element, unless that element or an intermediate element already has a namespace node with the
     same name (or absence of a name). */

  /* FIXME */


  df_value_deref_list(g,values);
  list_free(values,NULL);

  return 0;
}

static char *df_construct_simple_content(xs_globals *g, list *values, const char *separator)
{
  /* XSLT 2.0: 5.7.2 Constructing Simple Content */
  stringbuf *buf = stringbuf_new();
  char *str;
  list *l;

  values = list_copy(values,(list_copy_t)df_value_ref);

  /* @implements(xslt20:constructing-simple-content-1) @end */
  /* @implements(xslt20:constructing-simple-content-2)
     status { partial }
     issue { need support for attribute value templates with fixed and variable parts }
     @end */
  /* @implements(xslt20:constructing-simple-content-3)
     test { xslt/sequence/simple1.test }
     test { xslt/sequence/simple2.test }
     test { xslt/sequence/simple3.test }
     test { xslt/sequence/simple4.test }
     @end */
  /* @implements(xslt20:constructing-simple-content-4)
     status { partial }
     issue { need support for attribute value templates with fixed and variable parts }
     @end */

  /* 1. Zero-length text nodes in the sequence are discarded. */
  remove_zero_length_text_nodes(g,&values);

  /* 2. Adjacent text nodes in the sequence are merged into a single text node. */
  merge_adjacent_text_nodes(g,&values);

  /* 3. The sequence is atomized. */
  for (l = values; l; l = l->next) {
    df_value *v = (df_value*)l->data;
    df_value *atom = df_atomize(g,v);
    df_value_deref(g,v);
    l->data = atom;
  }

  /* 4. Every value in the atomized sequence is cast to a string. */
  for (l = values; l; l = l->next) {
    df_value *v = (df_value*)l->data;
    char *s = df_value_as_string(g,v);
    df_value *strval = df_value_new_string(g,s);
    free(s);
    df_value_deref(g,v);
    l->data = strval;
  }

  /* 5. The strings within the resulting sequence are concatenated, with a (possibly zero-length)
     separator inserted between successive strings. The default separator is a single space. In the
     case of xsl:attribute and xsl:value-of, a different separator can be specified using the
     separator attribute of the instruction; it is permissible for this to be a zero-length string,
     in which case the strings are concatenated with no separator. In the case of xsl:comment,
     xsl:processing-instruction, and xsl:namespace, and when expanding an attribute value template,
     the default separator cannot be changed. */
  for (l = values; l; l = l->next) {
    df_value *v = (df_value*)l->data;
    stringbuf_format(buf,"%s",v->value.s);
    if (l->next)
      stringbuf_format(buf,"%s",separator);
  }

  /* 6. The string that results from this concatenation forms the string value of the new
     attribute, namespace, comment, processing-instruction, or text node. */
  str = strdup(buf->data);

  stringbuf_free(buf);
  df_value_deref_list(g,values);
  list_free(values,NULL);

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

      if (0 != df_serialize(g,a->values[0],buf,options,state->ei)) {
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

    if (0 != df_serialize(g,a->values[0],buf,a->instr->seroptions,state->ei)) {
      stringbuf_free(buf);
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
/*     df_value *docval = df_value_new(a->instr->outports[0].seqtype); */
    list *values = df_sequence_to_list(a->values[0]);
/*     docval->value.n = docnode; */
/*     assert(0 == docval->value.n->refcount); */
/*     docval->value.n->refcount++; */
    df_value *docval = df_node_to_value(docnode);

    CHECK_CALL(df_construct_complex_content(state->ei,a->instr,g,values,docnode,NULL))

    if (NULL != docnode->attributes) {
      error(state->ei,"",0,"XTDE0420",
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
/*     df_value *elemvalue = df_value_new(a->instr->outports[0].seqtype); */
    df_value *elemvalue;
    df_node *elem = df_node_new(NODE_ELEMENT);
    assert(a->instr->outports[0].seqtype->type == SEQTYPE_ITEM);
    assert(a->instr->outports[0].seqtype->item->kind == ITEM_ELEMENT);

/*     elemvalue->value.n = elem; */
/*     assert(0 == elemvalue->value.n->refcount); */
/*     elemvalue->value.n->refcount++; */
    elemvalue = df_node_to_value(elem);

    assert(df_check_derived_atomic_type(a->values[1],g->string_type));
    elem->ident.name = strdup(a->values[1]->value.s);

    /* FIXME: namespace (should be received on an input port) - what about prefix? */

    debug("OP_SPECIAL_ELEMENT %p (\"%s\"): %d items in child sequence\n",
          elemvalue,elem->ident.name,list_count(values));
    if (0 != df_construct_complex_content(state->ei,a->instr,g,values,elem,a->instr->nsdefs)) {
      df_value_deref(g,elemvalue);
      return -1;
    }

    df_output_value(a,0,elemvalue);
    df_value_deref(g,a->values[0]);
    df_value_deref(g,a->values[1]);

    list_free(values,NULL);
    break;
  }
  case OP_SPECIAL_ATTRIBUTE: {
    df_value *attrvalue = df_value_new(a->instr->outports[0].seqtype);
    df_node *attr = df_node_new(NODE_ATTRIBUTE);
    list *values = df_sequence_to_list(a->values[0]);
    assert(a->instr->outports[0].seqtype->type == SEQTYPE_ITEM);
    assert(a->instr->outports[0].seqtype->item->kind == ITEM_ATTRIBUTE);
    attrvalue->value.n = attr;
    assert(0 == attrvalue->value.n->refcount);
    attrvalue->value.n->refcount++;

    assert(df_check_derived_atomic_type(a->values[1],g->string_type));
    attr->ident.name = strdup(a->values[1]->value.s);
    attr->value = df_construct_simple_content(g,values," ");

    list_free(values,NULL);

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
  case OP_SPECIAL_VALUE_OF: {

    /* @implements(xslt20:value-of-1)
       test { xslt/eval/value-of1.test }
       test { xslt/eval/value-of2.test }
       test { xslt/eval/value-of3.test }
       test { xslt/eval/value-of4.test }
       @end
       @implements(xslt20:value-of-3) @end
       @implements(xslt20:value-of-4) @end
       @implements(xslt20:value-of-6) @end
       @implements(xslt20:value-of-7) status { deferred } @end
       @implements(xslt20:value-of-8)
       test { xslt/eval/value-ofex.test }
       @end
       @implements(xslt20:value-of-9) @end
       @implements(xslt20:value-of-10) @end */

    df_node *textnode = df_node_new(NODE_TEXT);
    char *separator = df_value_as_string(g,a->values[1]);
    list *values = df_sequence_to_list(a->values[0]);
    textnode->value = df_construct_simple_content(g,values,separator);
    free(separator);
    list_free(values,NULL);
    df_output_value(a,0,df_node_to_value(textnode));
    df_value_deref(g,a->values[1]);
    df_value_deref(g,a->values[0]);
    break;
  }
  case OP_SPECIAL_TEXT: {
    df_node *textnode = df_node_new(NODE_TEXT);
    textnode->value = strdup(a->instr->str);
    df_output_value(a,0,df_node_to_value(textnode));
    df_value_deref(g,a->values[0]);
    break;
  }
  case OP_SPECIAL_NAMESPACE: {

    /* @implements(xslt20:creating-namespace-nodes-2)
       test { xslt/eval/namespace1.test }
       test { xslt/eval/namespace2.test }
       test { xslt/eval/namespace3.test }
       test { xslt/eval/namespace4.test }
       @end */

    df_node *nsnode = df_node_new(NODE_NAMESPACE);
    list *values = df_sequence_to_list(a->values[0]);
    char *prefix = df_construct_simple_content(g,values," ");
    list_free(values,NULL);
    if (0 == strlen(prefix)) {
      nsnode->prefix = NULL;
      free(prefix);
    }
    else {
      nsnode->prefix = prefix;
    }
    values = df_sequence_to_list(a->values[1]);
    nsnode->value = df_construct_simple_content(g,values," ");
    list_free(values,NULL);
    df_output_value(a,0,df_node_to_value(nsnode));
    df_value_deref(g,a->values[0]);
    df_value_deref(g,a->values[1]);
    break;
  }
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
  case OP_SPECIAL_SELECTROOT: {
    list *values = df_sequence_to_list(a->values[0]);
    list *output = NULL;
    list *l;

    for (l = values; l; l = l->next) {
      df_value *v = (df_value*)l->data;
      df_node *root;
      /* FIXME: raise a dynamic error here instead of asserting */
      if ((SEQTYPE_ITEM != v->seqtype->type) || (ITEM_ATOMIC == v->seqtype->item->kind)) {
        fprintf(stderr,"ERROR: non-node in selectroot input\n");
        assert(0);
      }
      root = v->value.n;
      while (NULL != root->parent)
        root = root->parent;
      list_append(&output,df_node_to_value(root));
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
  case OP_SPECIAL_RANGE: {
    int min;
    int max;
    list *range = NULL;
    /* FIXME: support empty sequences and conversion of other types when passed in */
    assert(df_check_derived_atomic_type(a->values[0],g->int_type));
    assert(df_check_derived_atomic_type(a->values[1],g->int_type));
    min = a->values[0]->value.i;
    max = a->values[1]->value.i;

    if (min <= max) {
      int i;
      for (i = max; i >= min; i--)
        list_push(&range,df_value_new_int(g,i));
    }

    df_output_value(a,0,df_list_to_sequence(g,range));
    df_value_deref_list(g,range);
    list_free(range,NULL);
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
  int err = 0;
  do {
    list **listptr = &state->activities;
    found = 0;
    while (NULL != *listptr) {
      list *l = *listptr;
      df_activity *a = (df_activity*)((*listptr)->data);

      if ((0 == a->remaining) && !a->fired) {
        if (0 != df_fire_activity(state,a))
          err = 1;
        else
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
  } while (found && !err);

  if (err) {
    list *l;
    for (l = state->activities; l; l = l->next) {
      df_activity *a = (df_activity*)l->data;
      free_activity(state,a);
    }
    list_free(state->activities,NULL);
  }
  else if (NULL != state->activities) {
    list *l;
    fprintf(stderr,"The following activities remain outstanding:\n");
    for (l = state->activities; l; l = l->next) {
      df_activity *a = (df_activity*)l->data;
      fprintf(stderr,"[%d] %s.%d - %s\n",
             a->actno,a->instr->fun->ident.name,a->instr->id,df_opstr(a->instr->opcode));
    }
  }

  if (err)
    return -1;
  else
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
  init->start = df_add_instruction(program,init,OP_SPECIAL_PRINT,nosourceloc);
  init->rtype = df_seqtype_new_atomic(program->schema->globals->complex_ur_type);

  state = df_state_new(program,ei);
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
