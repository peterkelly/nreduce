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

#define _DATAFLOW_DATAFLOW_C
#include "dataflow.h"
#include "funops.h"
#include "util/macros.h"
#include "util/namespace.h"
#include "util/debug.h"
#include <assert.h>
#include <string.h>
#include <errno.h>
#include <ctype.h>
#include <libxml/parser.h>
#include <libxml/tree.h>

const char *df_type_names[TYPE_COUNT] = {
  "",
  "xs:string",
  "xs:boolean",
  "xs:decimal",
  "xs:float",
  "xs:double",
  "xs:duration",
  "xs:dateTime",
  "xs:time",
  "xs:date",
  "xs:gYearMonth",
  "xs:gYear",
  "xs:gMonthDay",
  "xs:gDay",
  "xs:gMonth",
  "xs:hexBinary",
  "xs:base64Binary",
  "xs:anyURI",
  "xs:QName",
  "xs:NOTATION",

  "xs:normalizedString",
  "xs:token",
  "xs:language",
  "xs:NMTOKEN",
  "xs:NMTOKENS",
  "xs:Name",
  "xs:NCName",
  "xs:ID",
  "xs:IDREF",
  "xs:IDREFS",
  "xs:ENTITY",
  "xs:ENTITIES",
  "xs:integer",
  "xs:nonPositiveInteger",
  "xs:negativeInteger",
  "xs:long",
  "xs:int",
  "xs:short",
  "xs:byte",
  "xs:nonNegativeInteger",
  "xs:unsignedLong",
  "xs:unsignedInt",
  "xs:unsignedShort",
  "xs:unsignedByte",
  "xs:positiveInteger",


  "xdt:untyped",
  "xdt:untypedAtomic",
  "xdt:anyAtomicType",
  "xdt:dayTimeDuration",
  "xdt:yearMonthDuration",

  "attribute()",
  "comment()",
  "document-node()",
  "element()",
  "processing-instruction()",
  "text()",
  "node()",
  "item()",
  "none",

  "numeric"};

const char *df_special_op_names[OP_SPECIAL_COUNT] = {
  "const",
  "dup",
  "split",
  "merge",
  "call",
  "map",
  "pass",
  "swallow",
  "return",
  "print",
  "output",
  "sequence",
  "gate",
  "document",
  "element",
  "attribute",
  "pi",
  "comment",
  "value_of",
  "text",
  "namespace",
  "select",
  "selectroot",
  "filter",
  "empty",
  "contains-node",
  "range",
};

const char *df_axis_types[AXIS_COUNT] = {
  NULL,
  "child",
  "descendant",
  "attribute",
  "self",
  "descendant-or-self",
  "following-sibling",
  "following",
  "namespace",
  "parent",
  "ancestor",
  "preceding-sibling",
  "preceding",
  "ancestor-or-self",
};

static df_seqtype *df_get_seqtype_from_optype(df_program *program, const df_optype *optype)
{
  df_seqtype *st = NULL;

  if (TYPE_LAST_XMLSCHEMA_TYPE >= optype->type) {
    char *colon = strchr(df_type_names[optype->type],':');
    nsname nn;
    assert(NULL != colon);

    if (!strncmp(df_type_names[optype->type],"xs:",3))
      nn.ns = XS_NAMESPACE;
    else if (!strncmp(df_type_names[optype->type],"xdt:",4))
      nn.ns = XDT_NAMESPACE;
    else
      assert(0);

    nn.name = colon+1;
    st = df_seqtype_new_atomic(xs_lookup_type(program->schema,nn));
    assert(NULL != st->item->type);
  }
  else {
    switch (optype->type) {
    case TYPE_ATTRIBUTE:
      st = df_seqtype_new_item(ITEM_ATTRIBUTE);
      break;
    case TYPE_COMMENT:
      st = df_seqtype_new_item(ITEM_COMMENT);
      break;
    case TYPE_DOCUMENT_NODE:
      st = df_seqtype_new_item(ITEM_DOCUMENT);
      break;
    case TYPE_ELEMENT:
      st = df_seqtype_new_item(ITEM_ELEMENT);
      break;
    case TYPE_PROCESSING_INSTRUCTION:
      st = df_seqtype_new_item(ITEM_PI);
      break;
    case TYPE_TEXT:
      st = df_seqtype_new_item(ITEM_TEXT);
      break;
    case TYPE_NODE:
      st = df_normalize_itemnode(0);
      break;
    case TYPE_ITEM:
      st = df_normalize_itemnode(1);
      break;
    case TYPE_NONE:
      st = df_seqtype_new(SEQTYPE_EMPTY);
      break;
    case TYPE_NUMERIC: {
      /* FIXME: this should really be decimal */
      st = df_seqtype_new_atomic(xs_lookup_type(program->schema,nsname_temp(XS_NAMESPACE,"int")));
      assert(NULL != st->item->type);
      break;
    }
    default:
      assert(!"invalid optype->type");
      break;
    }
  }

  assert(NULL != st);

  if (OCCURS_ONCE != optype->occurrence) {
    df_seqtype *occ = df_seqtype_new(SEQTYPE_OCCURRENCE);
    occ->left = st;
    occ->occurrence = optype->occurrence;
    return occ;
  }
  else {
    return st;
  }
}

int df_instruction_compute_types(df_instruction *instr, xs_globals *g)
{
  int i;

  instr->computed = 1;
/*   debugl("df_instruction_compute_types %s:%d (%s) (%p)", */
/*          instr->fun->name,instr->id,df_opstr(instr->opcode),instr); */
  switch (instr->opcode) {
  case OP_SPECIAL_CONST:
/*     instr->outports[0].seqtype = df_seqtype_ref(instr->cvalue->seqtype); */
    assert(NULL != instr->outports[0].seqtype);
    break;
  case OP_SPECIAL_DUP:
    assert(NULL == instr->outports[0].seqtype);
    assert(NULL == instr->outports[1].seqtype);
    instr->outports[0].seqtype = df_seqtype_ref(instr->inports[0].seqtype);
    instr->outports[1].seqtype = df_seqtype_ref(instr->inports[0].seqtype);
    break;
  case OP_SPECIAL_SPLIT:
    assert(NULL == instr->outports[0].seqtype);
    assert(NULL == instr->outports[1].seqtype);
    instr->outports[0].seqtype = df_seqtype_ref(instr->inports[1].seqtype);
    instr->outports[1].seqtype = df_seqtype_ref(instr->inports[1].seqtype);
    instr->outports[2].seqtype = df_seqtype_new_atomic(g->boolean_type);
    break;
  case OP_SPECIAL_MERGE:
/*     if (!df_seqtype_compatible(instr->inports[0].seqtype,instr->inports[1].seqtype)) { */
/*       fprintf(stderr,"Merge %d has incompatible input types\n",instr->id); */
/*       return -1; */
/*     } */
    /* FIXME: this should be a choice between the two input types */

    /* FIXME */
/*     assert(NULL == instr->outports[0].seqtype); */
    instr->outports[0].seqtype = df_seqtype_ref(instr->inports[0].seqtype);
    break;
  case OP_SPECIAL_CALL:
    assert(NULL != instr->cfun->rtype);
    assert(NULL == instr->outports[0].seqtype);
    instr->outports[0].seqtype = df_seqtype_ref(instr->cfun->rtype);
    break;
  case OP_SPECIAL_MAP:
    assert(NULL == instr->outports[0].seqtype);
    instr->outports[0].seqtype = df_seqtype_ref(instr->inports[0].seqtype);
    break;
  case OP_SPECIAL_PASS:
    assert(NULL == instr->outports[0].seqtype);
    instr->outports[0].seqtype = df_seqtype_ref(instr->inports[0].seqtype);
    break;
  case OP_SPECIAL_SWALLOW:
    /* FIXME */
    break;
  case OP_SPECIAL_RETURN:
    assert(NULL == instr->outports[0].seqtype);
    instr->outports[0].seqtype = df_seqtype_ref(instr->inports[0].seqtype);
    break;
  case OP_SPECIAL_PRINT:
    break;
  case OP_SPECIAL_OUTPUT:
    instr->outports[0].seqtype = df_seqtype_new(SEQTYPE_EMPTY);
    break;
  case OP_SPECIAL_SEQUENCE:
    assert(NULL == instr->outports[0].seqtype);
    instr->outports[0].seqtype = df_seqtype_new(SEQTYPE_SEQUENCE);
    instr->outports[0].seqtype->left = df_seqtype_ref(instr->inports[0].seqtype);
    instr->outports[0].seqtype->right = df_seqtype_ref(instr->inports[1].seqtype);
    break;
  case OP_SPECIAL_GATE:
    assert(NULL == instr->outports[0].seqtype);
    instr->outports[0].seqtype = df_seqtype_ref(instr->inports[0].seqtype);
    break;
  case OP_SPECIAL_DOCUMENT:
    instr->outports[0].seqtype = df_seqtype_new_item(ITEM_DOCUMENT);
    break;
  case OP_SPECIAL_ELEMENT:
    assert(NULL == instr->outports[0].seqtype);
    instr->outports[0].seqtype = df_seqtype_new_item(ITEM_ELEMENT);
    break;
  case OP_SPECIAL_ATTRIBUTE:
    assert(NULL == instr->outports[0].seqtype);
    instr->outports[0].seqtype = df_seqtype_new_item(ITEM_ATTRIBUTE);
    break;
  case OP_SPECIAL_PI:
    /* FIXME */
    break;
  case OP_SPECIAL_COMMENT:
    /* FIXME */
    break;
  case OP_SPECIAL_VALUE_OF:
    assert(NULL == instr->outports[0].seqtype);
    instr->outports[0].seqtype = df_seqtype_new_item(ITEM_TEXT);
    break;
  case OP_SPECIAL_TEXT:
    assert(NULL == instr->outports[0].seqtype);
    instr->outports[0].seqtype = df_seqtype_new_item(ITEM_TEXT);
    break;
  case OP_SPECIAL_NAMESPACE:
    assert(NULL == instr->outports[0].seqtype);
    instr->outports[0].seqtype = df_seqtype_new_item(ITEM_NAMESPACE);
    break;
  case OP_SPECIAL_SELECT: {
    /* type: node()* */
    df_seqtype *nodetype = df_normalize_itemnode(0);
    instr->outports[0].seqtype = df_seqtype_new(SEQTYPE_OCCURRENCE);
    instr->outports[0].seqtype->left = nodetype;
    instr->outports[0].seqtype->occurrence = OCCURS_ZERO_OR_MORE;
    break;
  }
  case OP_SPECIAL_SELECTROOT: {
    /* type: node()* */
    df_seqtype *nodetype = df_normalize_itemnode(0);
    instr->outports[0].seqtype = df_seqtype_new(SEQTYPE_OCCURRENCE);
    instr->outports[0].seqtype->left = nodetype;
    instr->outports[0].seqtype->occurrence = OCCURS_ZERO_OR_MORE;
    break;
  }
  case OP_SPECIAL_FILTER:
    /* FIXME: actually, we shouldn't copy the output type verbatim. The input type could be
       a sequence with a fixed occurrence range, but the output must have a variable
       occurrence range as not all input items will necessarily be in the output sequence */
    instr->outports[0].seqtype = df_seqtype_ref(instr->inports[0].seqtype);
    break;
  case OP_SPECIAL_EMPTY:
    instr->outports[0].seqtype = df_seqtype_new(SEQTYPE_EMPTY);
    break;
  case OP_SPECIAL_CONTAINS_NODE:
    instr->outports[0].seqtype = df_seqtype_new_atomic(g->boolean_type);
    break;
  case OP_SPECIAL_RANGE: {
    df_seqtype *nodetype = df_seqtype_new_atomic(g->int_type);
    instr->outports[0].seqtype = df_seqtype_new(SEQTYPE_OCCURRENCE);
    instr->outports[0].seqtype->left = nodetype;
    instr->outports[0].seqtype->occurrence = OCCURS_ZERO_OR_MORE;
    break;
  }
  }

  if (OP_SPECIAL_RETURN == instr->opcode) {
/*     debugl("  return instruction; skipping rest"); */
    return 0;
  }

  for (i = 0; i < instr->noutports; i++) {
    df_outport *outport = &instr->outports[i];
    df_inport *inport;
    int isready = 1;
    int j;

    assert(NULL != outport->dest);

    inport = &outport->dest->inports[outport->destp];

    assert(NULL != outport->seqtype);

    if (NULL == inport->seqtype) {
      inport->seqtype = df_seqtype_ref(outport->seqtype);
    }
    /* FIXME: check type compatibility */
/*     else if (!df_seqtype_compatible(outport->seqtype,inport->seqtype)) { */
/*       fprintf(stderr,"Output port %d.%d and input port %d.%d have incompatible types\n", */
/*               instr->id,i,outport->dest->id,outport->destp); */
/*       return -1; */
/*     } */

    if (!outport->dest->computed) {
      for (j = 0; j < outport->dest->ninports; j++)
        if (NULL == outport->dest->inports[j].seqtype)
          isready = 0;
      if (isready)
        CHECK_CALL(df_instruction_compute_types(outport->dest,g))
    }
  }
  return 0;
}

int df_compute_types(df_function *fun)
{
  int i;
  for (i = 0; i < fun->nparams; i++) {
    if (1 != fun->params[i].start->ninports) {
      fprintf(stderr,"Input %d must have only 1 input port as it is the start instruction for "
              "parameter %d\n",fun->params[i].start->id,i);
      return -1;
    }
    assert(NULL != fun->params[i].start->inports[0].seqtype);
    if (/*(NULL != fun->params[i].start->inports[0].seqtype) &&*/
        df_seqtype_compatible(fun->params[i].seqtype,fun->params[i].start->inports[0].seqtype)) {
      fprintf(stderr,"Input port %d.0 has incompatible type with parameter %d\n",
              fun->params[i].start->id,i);
      return -1;
    }
/*     if (NULL == fun->params[i].start->inports[0].seqtype) */
/*       fun->params[i].start->inports[0].seqtype = fun->params[i].seqtype; */
    CHECK_CALL(df_instruction_compute_types(fun->params[i].start,fun->program->schema->globals))
  }

  if (1 != fun->start->ninports) {
    fprintf(stderr,"Input %d must have only 1 input port as it is the start instruction for "
            "the function\n",fun->start->id);
    return -1;
  }
  assert(NULL != fun->start->inports[0].seqtype);
  CHECK_CALL(df_instruction_compute_types(fun->start,fun->program->schema->globals))
  return 0;
}

int df_check_derived_atomic_type(df_value *v, xs_type *type)
{
  xs_type *t1;
  if (SEQTYPE_ITEM != v->seqtype->type)
    return 0;

  if (ITEM_ATOMIC != v->seqtype->item->kind)
    return 0;

  for (t1 = v->seqtype->item->type; t1 != t1->base; t1 = t1->base)
    if (t1 == type)
      return 1;

  return 0;
}

int df_get_op_id(char *name)
{
  int i;
  for (i = 0; i < FN_OP_COUNT; i++) {
    if (!strcmp(df_opdefs[i].name,name))
      return i;
  }
  return -1;
}

void df_init_function(df_program *program, df_function *fun, sourceloc sloc)
{
  assert(NULL == fun->ret);
  assert(NULL == fun->start);
  assert(NULL == fun->rtype);

  fun->ret = df_add_instruction(program,fun,OP_SPECIAL_RETURN,sloc);
  fun->start = df_add_instruction(program,fun,OP_SPECIAL_PASS,sloc);
  fun->start->inports[0].seqtype = df_seqtype_new_atomic(program->schema->globals->int_type);
  fun->start->outports[0].dest = fun->ret;
  fun->start->outports[0].destp = 0;

  fun->rtype = df_seqtype_new_atomic(program->schema->globals->complex_ur_type);
}

df_function *df_new_function(df_program *program, const nsname ident)
{
  df_function *fun = (df_function*)calloc(1,sizeof(df_function));
  list_append(&program->functions,fun);
  fun->ident = nsname_copy(ident);
  fun->program = program;

  /* Create sequence instruction (not actually part of the program, but used by the map operator
     to collate results of inner function calls) */
  fun->mapseq = df_add_instruction(program,fun,OP_SPECIAL_SEQUENCE,nosourceloc);
  fun->mapseq->internal = 1;
  fun->mapseq->inports[0].seqtype =
    df_seqtype_new_atomic(program->schema->globals->complex_ur_type);
  fun->mapseq->inports[1].seqtype =
    df_seqtype_new_atomic(program->schema->globals->complex_ur_type);
  fun->mapseq->outports[0].seqtype = df_seqtype_new(SEQTYPE_SEQUENCE);
  fun->mapseq->outports[0].seqtype->left = df_seqtype_ref(fun->mapseq->inports[0].seqtype);
  fun->mapseq->outports[0].seqtype->right = df_seqtype_ref(fun->mapseq->inports[1].seqtype);

  return fun;
}

df_function *df_lookup_function(df_program *program, const nsname ident)
{
  list *l;
  for (l = program->functions; l; l = l->next) {
    df_function *fun = (df_function*)l->data;
    if (nsname_equals(fun->ident,ident))
      return fun;
  }
  return NULL;
}

void df_free_function(df_function *fun)
{
  int i;
  list_free(fun->instructions,(void*)df_free_instruction);

  nsname_free(fun->ident);
  if (NULL != fun->rtype)
    df_seqtype_deref(fun->rtype);

  for (i = 0; i < fun->nparams; i++) {
    assert(NULL != fun->params[i].seqtype);
    df_seqtype_deref(fun->params[i].seqtype);
  }

  free(fun->params);
  free(fun->mode);
  free(fun);
}

df_instruction *df_add_instruction(df_program *program, df_function *fun, int opcode,
                                   sourceloc sloc)
{
  df_instruction *instr = (df_instruction*)calloc(1,sizeof(df_instruction));
  int i;
  list_append(&fun->instructions,instr);
  instr->id = fun->nextid++;
  instr->opcode = opcode;
  instr->fun = fun;
  instr->sloc = sourceloc_copy(sloc);

  if (FN_OP_COUNT > opcode) {
    instr->ninports = df_opdefs[opcode].nparams;
    instr->inports = (df_inport*)calloc(instr->ninports,sizeof(df_inport));
    for (i = 0; i < instr->ninports; i++)
      instr->inports[i].seqtype =
        df_get_seqtype_from_optype(program,&df_opdefs[opcode].paramtypes[i]);
    instr->noutports = 1;
    instr->outports = (df_outport*)calloc(1,sizeof(df_outport));
    instr->outports[0].seqtype = df_get_seqtype_from_optype(program,&df_opdefs[opcode].rettype);
  }
  else {

    switch (opcode) {
    case OP_SPECIAL_CONST:
      instr->ninports = 1;
      instr->noutports = 1;
      break;
    case OP_SPECIAL_DUP:
      instr->ninports = 1;
      instr->noutports = 2;
      break;
    case OP_SPECIAL_SPLIT:
      instr->ninports = 2;
      instr->noutports = 3;
      break;
    case OP_SPECIAL_MERGE:
      instr->ninports = 2;
      instr->noutports = 1;
      break;
    case OP_SPECIAL_CALL:
      instr->ninports = 1;
      instr->noutports = 1;
      break;
    case OP_SPECIAL_MAP:
      instr->ninports = 1;
      instr->noutports = 1;
      break;
    case OP_SPECIAL_PASS:
      instr->ninports = 1;
      instr->noutports = 1;
      break;
    case OP_SPECIAL_SWALLOW:
      instr->ninports = 1;
      instr->noutports = 0;
      break;
    case OP_SPECIAL_RETURN:
      instr->ninports = 1;
      instr->noutports = 1;
      break;
    case OP_SPECIAL_PRINT:
      instr->ninports = 1;
      instr->noutports = 0;
      break;
    case OP_SPECIAL_OUTPUT:
      instr->ninports = 1;
      instr->noutports = 1;
      break;
    case OP_SPECIAL_SEQUENCE:
      instr->ninports = 2;
      instr->noutports = 1;
      break;
    case OP_SPECIAL_GATE:
      instr->ninports = 2;
      instr->noutports = 1;
      break;
    case OP_SPECIAL_DOCUMENT:
      instr->ninports = 1;
      instr->noutports = 1;
      break;
    case OP_SPECIAL_ELEMENT:
      instr->ninports = 2;
      instr->noutports = 1;
      break;
    case OP_SPECIAL_ATTRIBUTE:
      instr->ninports = 2;
      instr->noutports = 1;
      break;
    case OP_SPECIAL_PI:
      /* FIXME */
      break;
    case OP_SPECIAL_COMMENT:
      /* FIXME */
      break;
    case OP_SPECIAL_VALUE_OF:
      instr->ninports = 2;
      instr->noutports = 1;
      break;
    case OP_SPECIAL_TEXT:
      instr->ninports = 1;
      instr->noutports = 1;
      break;
    case OP_SPECIAL_NAMESPACE:
      instr->ninports = 2;
      instr->noutports = 1;
      break;
    case OP_SPECIAL_SELECT:
      instr->ninports = 1;
      instr->noutports = 1;
      break;
    case OP_SPECIAL_SELECTROOT:
      instr->ninports = 1;
      instr->noutports = 1;
      break;
    case OP_SPECIAL_FILTER:
      instr->ninports = 2;
      instr->noutports = 1;
      break;
    case OP_SPECIAL_EMPTY:
      instr->ninports = 1;
      instr->noutports = 1;
      break;
    case OP_SPECIAL_CONTAINS_NODE:
      instr->ninports = 2;
      instr->noutports = 1;
      break;
    case OP_SPECIAL_RANGE:
      instr->ninports = 2;
      instr->noutports = 1;
      break;
    default:
      assert(!"invalid opcode");
      break;
    }

    if (0 != instr->ninports)
      instr->inports = (df_inport*)calloc(instr->ninports,sizeof(df_inport));
    if (0 != instr->noutports)
      instr->outports = (df_outport*)calloc(instr->noutports,sizeof(df_outport));
  }

  for (i = 0; i < instr->noutports; i++) {
    instr->outports[i].owner = instr;
    instr->outports[i].portno = i;
  }

  return instr;
}

void df_delete_instruction(df_program *program, df_function *fun, df_instruction *instr)
{
  list **lptr = &fun->instructions;
/*   debugl("deleting instruction %s:%d (%s), actual function is %s", */
/*          instr->fun->name,instr->id,df_opstr(instr->opcode),fun->name); */
/*   if (instr->fun != fun) { */
/*     debugl("function does not match!"); */
/*   } */
  assert(instr->fun == fun);
  while (NULL != *lptr) {
    if ((*lptr)->data == instr) {
      list *old = *lptr;
      *lptr = (*lptr)->next;
      free(old);
      df_free_instruction(instr);
/*       debugl("done"); */
      return;
    }
    lptr = &((*lptr)->next);
  }
/*   debugl("not found"); */
  assert(!"instruction not found");
}

void df_free_instruction_inports(df_instruction *instr)
{
  int i;
  for (i = 0; i < instr->ninports; i++)
    if (NULL != instr->inports[i].seqtype)
      df_seqtype_deref(instr->inports[i].seqtype);
  free(instr->inports);
}

void df_free_instruction(df_instruction *instr)
{
  int i;
  df_free_instruction_inports(instr);
  for (i = 0; i < instr->noutports; i++)
    if (NULL != instr->outports[i].seqtype)
      df_seqtype_deref(instr->outports[i].seqtype);
  free(instr->outports);

  if (OP_SPECIAL_CONST == instr->opcode) {
/*     debugl("df_free_instruction CONT: instr->cvalue = %p, with %d refs (before my deref)", */
/*           instr->cvalue,instr->cvalue->refcount); */
    df_value_deref(instr->fun->program->schema->globals,instr->cvalue);
  }

  free(instr->nametest);
  if (NULL != instr->seqtypetest)
    df_seqtype_deref(instr->seqtypetest);
  if (NULL != instr->seroptions)
    df_seroptions_free(instr->seroptions);
  list_free(instr->nsdefs,(void*)ns_def_free);
  free(instr);
}

const char *df_opstr(int opcode)
{
  if (FN_OP_COUNT >= opcode)
    return df_opdefs[opcode].name;
  else
    return df_special_op_names[opcode-OP_SPECIAL_BASE];
}

df_program *df_program_new(xs_schema *schema)
{
  df_program *program = (df_program*)calloc(1,sizeof(df_program));
  program->schema = schema;
  return program;
}

void df_program_free(df_program *program)
{
  list_free(program->functions,(void*)df_free_function);
  list_free(program->space_decls,(void*)space_decl_free);
  free(program);
}

void df_output_dot(df_program *program, FILE *f)
{
  xs_globals *g = program->schema->globals;
  list *l;
  fprintf(f,"digraph {\n");
/*   fprintf(f,"  rankdir=LR;\n"); */
  fprintf(f,"  nodesep=0.5;\n");
  fprintf(f,"  ranksep=0.3;\n");
  fprintf(f,"  node[shape=record,fontsize=12,fontname=\"Arial\",style=filled];\n");
  for (l = program->functions; l; l = l->next) {
    df_function *fun = (df_function*)l->data;
    list *il;
    int i;

    fprintf(f,"  subgraph cluster%p {\n",fun);
    fprintf(f,"    label=\"%s\";\n",fun->ident.name);

    fprintf(f,"  subgraph clusterz%p {\n",fun);
    fprintf(f,"    label=\"\";\n");
    fprintf(f,"    color=white;\n");

    fprintf(f,"    %sstart [label=\"[context]\",color=white];\n",fun->ident.name);
    if (NULL != fun->start)
      fprintf(f,"    %sstart -> %s%d;\n",fun->ident.name,fun->ident.name,fun->start->id);
    for (i = 0; i < fun->nparams; i++) {
      fprintf(f,"    %sp%d [label=\"[param %d]\",color=white];\n",fun->ident.name,i,i);
      if (NULL != fun->params[i].start)
        fprintf(f,"    %sp%d -> %s%d;\n",
                fun->ident.name,i,fun->ident.name,fun->params[i].start->id);
    }
    fprintf(f,"  }\n");

    fprintf(f,"  subgraph clusterx%p {\n",fun);
    fprintf(f,"    label=\"\";\n");
    fprintf(f,"    color=white;\n");

    for (il = fun->instructions; il; il = il->next) {
      df_instruction *instr = (df_instruction*)il->data;
      int i;

      if (fun->mapseq == instr)
        continue;

      if (OP_SPECIAL_DUP == instr->opcode) {
        fprintf(f,"    %s%d [label=\"\",style=filled,height=0.2,width=0.2,"
                "fixedsize=true,color=\"#808080\",shape=circle];\n",fun->ident.name,instr->id);
      }
      else if (OP_SPECIAL_GATE == instr->opcode) {
        fprintf(f,"    %s%d [label=\"\",height=0.4,width=0.4,style=solid,"
                "fixedsize=true,color=\"#808080\",shape=triangle];\n",fun->ident.name,instr->id);
      }
      else {

        if (OP_SPECIAL_CONST == instr->opcode) {
          fprintf(f,"    %s%d [label=\"%d|",fun->ident.name,instr->id,instr->id);
          df_value_print(g,f,instr->cvalue);
          fprintf(f,"\",fillcolor=\"#FF8080\"];\n");
        }
        else if (OP_SPECIAL_CALL == instr->opcode) {
          fprintf(f,"    %s%d [label=\"%d|",fun->ident.name,instr->id,instr->id);
          fprintf(f,"call(%s)",instr->cfun->ident.name);
          fprintf(f,"\",fillcolor=\"#C0FFC0\"];\n");
        }
        else if (OP_SPECIAL_MAP == instr->opcode) {
          fprintf(f,"    %s%d [label=\"%d|",fun->ident.name,instr->id,instr->id);
          fprintf(f,"map(%s)",instr->cfun->ident.name);
          fprintf(f,"\",fillcolor=\"#FFFFC0\"];\n");
        }
        else {
          fprintf(f,"    %s%d [label=\"%d|",fun->ident.name,instr->id,instr->id);

          if (OP_SPECIAL_SELECT == instr->opcode) {
            fprintf(f,"select: ");
            if (NULL != instr->nametest) {
              fprintf(f,"%s",instr->nametest);
            }
            else {
              stringbuf *buf = stringbuf_new();
              /* FIXME: need to use the namespace map of the syntax tree node */
              df_seqtype_print_xpath(buf,instr->seqtypetest,program->schema->globals->namespaces);
              fprintf(f,"%s",buf->data);
              stringbuf_free(buf);
            }
            fprintf(f,"\\n[%s]",df_axis_types[instr->axis]);
          }
          else {
            fprintf(f,"%s",df_opstr(instr->opcode));
          }
          fprintf(f,"\",fillcolor=\"#80E0FF\"];\n");
        }


      }

      for (i = 0; i < instr->noutports; i++) {
        if (NULL != instr->outports[i].dest) {
          df_instruction *dest = instr->outports[i].dest;
          fprintf(f,"    %s%d -> %s%d [label=\"",
                  fun->ident.name,instr->id,dest->fun->ident.name,dest->id);
/*           if ((OP_SPLIT == instr->opcode) && (0 == i)) */
/*             fprintf(f,"F"); */
/*           else if ((OP_SPLIT == instr->opcode) && (1 == i)) */
/*             fprintf(f,"T"); */

          fprintf(f,"\"");

          if (OP_SPECIAL_DUP == instr->outports[i].dest->opcode)
            fprintf(f,",color=\"#808080\",arrowhead=none");
          else if (OP_SPECIAL_DUP == instr->opcode)
            fprintf(f,",color=\"#808080\"");
          else if ((2 == i) &&
                   (OP_SPECIAL_SPLIT == instr->opcode) &&
                   (OP_SPECIAL_MERGE == instr->outports[i].dest->opcode))
            fprintf(f,",style=dashed");

          fprintf(f,"];\n");
        }
      }
    }

    fprintf(f,"  }\n");
    fprintf(f,"  }\n");
  }
  fprintf(f,"}\n");
}

void df_output_df(df_program *program, FILE *f)
{
  list *fl;
  stringbuf *buf = stringbuf_new();
  for (fl = program->functions; fl; fl = fl->next) {
    df_function *fun = (df_function*)fl->data;
    list *il;
    int i;

    fprintf(f,"function %s {\n",fun->ident.name);
    assert(NULL != fun->rtype);
    stringbuf_clear(buf);
    df_seqtype_print_fs(buf,fun->rtype,program->schema->globals->namespaces->defs);
    fprintf(f,"  return %s\n",buf->data);

    for (i = 0; i < fun->nparams; i++) {
      stringbuf_clear(buf);
      df_seqtype_print_fs(buf,fun->params[i].seqtype,program->schema->globals->namespaces->defs);
      fprintf(f,"  param %d start %d type %s\n",i,fun->params[i].start->id,buf->data);
    }

    fprintf(f,"  start %d\n",fun->start->id);

    for (il = fun->instructions; il; il = il->next) {
      df_instruction *instr = (df_instruction*)il->data;

      if (fun->mapseq == instr)
        continue;

      fprintf(f,"  %-5d %-20s",instr->id,df_opstr(instr->opcode));
      if (OP_SPECIAL_RETURN != instr->opcode) {
        for (i = 0; i < instr->noutports; i++) {
          if (0 < i)
            fprintf(f," ");
          assert(NULL != instr->outports[i].dest);
          fprintf(f,"%d:%d",instr->outports[i].dest->id,instr->outports[i].destp);
        }
      }

      if (OP_SPECIAL_CONST == instr->opcode) {
        assert(SEQTYPE_ITEM == instr->cvalue->seqtype->type);
        assert(ITEM_ATOMIC == instr->cvalue->seqtype->item->kind);

        if (instr->cvalue->seqtype->item->type ==
            program->schema->globals->string_type) {
          char *escaped = escape_str(instr->cvalue->value.s);
          fprintf(f," = \"%s\"",escaped);
          free(escaped);
        }
        else if (instr->cvalue->seqtype->item->type ==
                 program->schema->globals->int_type) {
          fprintf(f," = %d",instr->cvalue->value.i);
        }
        else {
          assert(!"unknown constant type");
        }
      }

      fprintf(f,"\n");
    }
    fprintf(f,"}\n");
    if (fl->next)
      fprintf(f,"\n");
  }
  stringbuf_free(buf);
}

