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
#include "util/macros.h"
#include "util/namespace.h"
#include "util/debug.h"
#include <assert.h>
#include <string.h>
#include <errno.h>
#include <ctype.h>
#include <libxml/parser.h>
#include <libxml/tree.h>

const char *df_special_op_names[OP_COUNT] = {
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
  "gate",
  "builtin",
};

void df_instruction_compute_types(df_instruction *instr)
{
  int i;

  instr->computed = 1;

  debug("df_instruction_compute_types %#n %d (%s)\n",
        instr->fun->ident,instr->id,df_opstr(instr->opcode));

  switch (instr->opcode) {
  case OP_CONST:
    break;
  case OP_DUP:
    instr->outports[0].st = seqtype_ref(instr->inports[0].st);
    instr->outports[1].st = seqtype_ref(instr->inports[0].st);
    break;
  case OP_SPLIT:
    instr->outports[0].st = seqtype_ref(instr->inports[1].st);
    instr->outports[1].st = seqtype_ref(instr->inports[1].st);
    instr->outports[2].st = seqtype_ref(instr->inports[0].st);
    break;
  case OP_MERGE:
    /* FIXME: merge the two input types; if you know they're both going to be ints, then the
       result of the merge can be int. */
    instr->outports[0].st = seqtype_new(SEQTYPE_OCCURRENCE);
    instr->outports[0].st->occurrence = OCCURS_ZERO_OR_MORE;
    instr->outports[0].st->left = df_normalize_itemnode(1);
    break;
  case OP_CALL:
    instr->outports[0].st = seqtype_ref(instr->cfun->rtype);
    break;
  case OP_MAP:
    /* FIXME: we can possibly be more specific here if we know the occurrence range of the
       input type, e.g. OCCURS_ONE_OR_MORE */
    /* FIXME: what if the return type of the function is a sequence type with an occurrence
       indicator, e.g. item()*? The result will be a nested occurrence indicator... is this ok? */
    instr->outports[0].st = seqtype_new(SEQTYPE_OCCURRENCE);
    instr->outports[0].st->occurrence = OCCURS_ZERO_OR_MORE;
    instr->outports[0].st->left = seqtype_ref(instr->cfun->rtype);
    break;
  case OP_PASS:
    instr->outports[0].st = seqtype_ref(instr->inports[0].st);
    break;
  case OP_SWALLOW:
    break;
  case OP_RETURN:
    return;
  case OP_PRINT:
    break;
  case OP_GATE:
    instr->outports[0].st = seqtype_ref(instr->inports[0].st);
    break;
  case OP_BUILTIN:
    break;
  default:
    assert(!"invalid opcode");
    break;
  }

  for (i = 0; i < instr->noutports; i++) {
    df_outport *op = &instr->outports[i];
    df_inport *ip = &op->dest->inports[op->destp];
    int remaining = 0;

    assert(NULL != op->st);

    if ((OP_BUILTIN == op->dest->opcode) || (OP_CALL == op->dest->opcode)) {
      if (NULL == ip->st) {
        printf("null sequence type!\n");
      }
      assert(NULL != ip->st);
    }
    else {
      int i;

      if ((OP_MERGE != op->dest->opcode) || (NULL == ip->st)) {
        if (NULL != ip->st) {
          debug("%#n instr %d inport %d [opcode %d] already has a type\n",
                instr->fun->ident,op->dest->id,op->destp,op->dest->opcode);
        }
        assert(NULL == ip->st);
        ip->st = seqtype_ref(op->st);

        for (i = 0; i < op->dest->ninports; i++)
          if (NULL == op->dest->inports[i].st)
            remaining++;

      }
    }

    if ((0 == remaining) && !op->dest->computed)
      df_instruction_compute_types(op->dest);
  }
}

void df_compute_types(df_function *fun)
{
  int i;

  df_instruction_compute_types(fun->start);
  for (i = 0; i < fun->nparams; i++)
    df_instruction_compute_types(fun->params[i].start);


/*
  list *l;

  for (l = fun->instructions; l; l = l->next) {
    df_instruction *instr = (df_instruction*)l->data;
    if (OP_BUILTIN != instr->opcode) {
      int i;
      for (i = 0; i < instr->ninports; i++) {
        seqtype *st = seqtype_new(SEQTYPE_OCCURRENCE);
        st->occurrence = OCCURS_ZERO_OR_MORE;
        st->left = df_normalize_itemnode(1);
        instr->inports[i].st = st;
      }
      for (i = 0; i < instr->noutports; i++) {
        seqtype *st = seqtype_new(SEQTYPE_OCCURRENCE);
        st->occurrence = OCCURS_ZERO_OR_MORE;
        st->left = df_normalize_itemnode(1);
        instr->outports[i].st = st;
      }
    }
  }
*/
}

int df_check_is_atomic_type(seqtype *st, xs_type *type)
{
  return ((SEQTYPE_ITEM == st->type) &&
          (ITEM_ATOMIC == st->item->kind) &&
          (st->item->type == type));
}

int df_check_derived_atomic_type(value *v, xs_type *type)
{
  xs_type *t1;
  if (SEQTYPE_ITEM != v->st->type)
    return 0;

  if (ITEM_ATOMIC != v->st->item->kind)
    return 0;

  for (t1 = v->st->item->type; t1 != t1->base; t1 = t1->base)
    if (t1 == type)
      return 1;

  return 0;
}

void df_init_function(df_program *program, df_function *fun, sourceloc sloc)
{
  assert(NULL == fun->ret);
  assert(NULL == fun->start);
  assert(NULL == fun->rtype);

  fun->ret = df_add_instruction(program,fun,OP_RETURN,sloc);
  fun->start = df_add_instruction(program,fun,OP_PASS,sloc);
  fun->start->inports[0].st = df_normalize_itemnode(1);
  fun->start->outports[0].dest = fun->ret;
  fun->start->outports[0].destp = 0;

  fun->rtype = seqtype_new_atomic(xs_g->complex_ur_type);
}

df_function *df_new_function(df_program *program, const nsname ident)
{
  df_function *fun = (df_function*)calloc(1,sizeof(df_function));
  list_append(&program->functions,fun);
  fun->ident = nsname_copy(ident);
  fun->program = program;

  /* Create sequence instruction (not actually part of the program, but used by the map operator
     to collate results of inner function calls) */


  fun->mapseq = df_add_builtin_instruction(program,fun,nsname_temp(SPECIAL_NAMESPACE,"sequence"),2,
                                           nosourceloc);
  fun->mapseq->internal = 1;

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
  list_free(fun->instructions,(list_d_t)df_free_instruction);

  nsname_free(fun->ident);
  if (NULL != fun->rtype)
    seqtype_deref(fun->rtype);

  for (i = 0; i < fun->nparams; i++) {
    assert(NULL != fun->params[i].st);
    seqtype_deref(fun->params[i].st);
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

  switch (opcode) {
  case OP_CONST:
    instr->ninports = 1;
    instr->noutports = 1;
    break;
  case OP_DUP:
    instr->ninports = 1;
    instr->noutports = 2;
    break;
  case OP_SPLIT:
    instr->ninports = 2;
    instr->noutports = 3;
    break;
  case OP_MERGE:
    instr->ninports = 2;
    instr->noutports = 1;
    break;
  case OP_CALL:
    instr->ninports = 1;
    instr->noutports = 1;
    break;
  case OP_MAP:
    instr->ninports = 1;
    instr->noutports = 1;
    break;
  case OP_PASS:
    instr->ninports = 1;
    instr->noutports = 1;
    break;
  case OP_SWALLOW:
    instr->ninports = 1;
    instr->noutports = 0;
    break;
  case OP_RETURN:
    instr->ninports = 1;
    instr->noutports = 1;
    break;
  case OP_PRINT:
    instr->ninports = 1;
    instr->noutports = 0;
    break;
  case OP_GATE:
    instr->ninports = 2;
    instr->noutports = 1;
    break;
  case OP_BUILTIN:
    instr->ninports = 0;
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

  for (i = 0; i < instr->noutports; i++) {
    instr->outports[i].owner = instr;
    instr->outports[i].portno = i;
  }

  return instr;
}

df_instruction *df_add_builtin_instruction(df_program *program, df_function *fun,
                                           const nsname ident, int nargs, sourceloc sloc)
{
  df_builtin_function *bif = NULL;
  df_instruction *instr;
  int i;
  list *l;
  for (l = program->builtin_functions; l; l = l->next) {
    df_builtin_function *b = (df_builtin_function*)l->data;
    if (nsname_equals(b->ident,ident) && (b->nargs == nargs)) {
      bif = b;
      break;
    }
  }

  if (NULL == bif)
    return NULL;
  instr = df_add_instruction(program,fun,OP_BUILTIN,sloc);

  instr->bif = bif;

  df_free_instruction_inports(instr);
  instr->ninports = nargs;
  instr->inports = (df_inport*)calloc(nargs,sizeof(df_inport));
  for (i = 0; i < nargs; i++)
    instr->inports[i].st = seqtype_ref(bif->argtypes[i]);

  assert(1 == instr->noutports);
  instr->outports[0].st = seqtype_ref(bif->rettype);

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
    if (NULL != instr->inports[i].st)
      seqtype_deref(instr->inports[i].st);
  free(instr->inports);
}

void df_free_instruction(df_instruction *instr)
{
  int i;
  df_free_instruction_inports(instr);
  for (i = 0; i < instr->noutports; i++)
    if (NULL != instr->outports[i].st)
      seqtype_deref(instr->outports[i].st);
  free(instr->outports);

  if (OP_CONST == instr->opcode) {
/*     debugl("df_free_instruction CONT: instr->cvalue = %p, with %d refs (before my deref)", */
/*           instr->cvalue,instr->cvalue->refcount); */
    value_deref(instr->cvalue);
  }

  free(instr->nametest);
  if (NULL != instr->seqtypetest)
    seqtype_deref(instr->seqtypetest);
  if (NULL != instr->seroptions)
    df_seroptions_free(instr->seroptions);
  list_free(instr->nsdefs,(list_d_t)ns_def_free);
  free(instr->str);
  sourceloc_free(instr->sloc);
  free(instr);
}

gxcontext *df_create_context(value *item, int position, int size, int havefocus,
                              gxcontext *prev)
{
  gxcontext *ctxt = (gxcontext*)calloc(1,sizeof(gxcontext));
  ctxt->item = item ? value_ref(item) : NULL;
  ctxt->position = position;
  ctxt->size = size;
  ctxt->havefocus = havefocus;
  if (NULL != prev) {
    ctxt->tmpl = prev->tmpl;
    ctxt->mode = prev->mode ? strdup(prev->mode) : NULL;
    ctxt->group = prev->group ? strdup(prev->group) : NULL;
    ctxt->groupkey = prev->groupkey ? strdup(prev->groupkey) : NULL;
  }

  return ctxt;
}

void df_free_context(gxcontext *ctxt)
{
  if (ctxt->item)
    value_deref(ctxt->item);
  free(ctxt->mode);
  free(ctxt->group);
  free(ctxt->groupkey);
  free(ctxt);
}

const char *df_opstr(int opcode)
{
  return df_special_op_names[opcode];
}

void df_free_builtin_function(df_builtin_function *bif)
{
  int i;
  nsname_free(bif->ident);
  for (i = 0; i < bif->nargs; i++)
    seqtype_deref(bif->argtypes[i]);
  free(bif->argtypes);
  seqtype_deref(bif->rettype);
  free(bif);
}

df_program *df_program_new(xs_schema *schema)
{
  df_program *program = (df_program*)calloc(1,sizeof(df_program));
  program->schema = schema;
  return program;
}

void df_program_free(df_program *program)
{
  list_free(program->functions,(list_d_t)df_free_function);
  list_free(program->space_decls,(list_d_t)space_decl_free);
  list_free(program->builtin_functions,(list_d_t)df_free_builtin_function);
  free(program);
}

static int has_portlabels(df_instruction *instr)
{
  return ((OP_DUP != instr->opcode) && (OP_GATE != instr->opcode));
}

void df_output_dot(df_program *program, FILE *f, int types)
{
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

      if (OP_DUP == instr->opcode) {
        fprintf(f,"    %s%d [label=\"\",style=filled,height=0.2,width=0.2,"
                "fixedsize=true,color=\"#808080\",shape=circle];\n",fun->ident.name,instr->id);
      }
      else if (OP_GATE == instr->opcode) {
        fprintf(f,"    %s%d [label=\"\",height=0.4,width=0.4,style=solid,"
                "fixedsize=true,color=\"#808080\",shape=triangle];\n",fun->ident.name,instr->id);
      }
      else {

        const char *extra = "";

        fprintf(f,"    %s%d [label=\"",fun->ident.name,instr->id);

        if (types) {
          fprintf(f,"{{");

          for (i = 0; i < instr->ninports; i++) {
            if (NULL != instr->inports[i].st) {
              stringbuf *buf = stringbuf_new();
              seqtype_print_fs(buf,instr->inports[i].st,xs_g->namespaces);
              if (0 < i)
                fprintf(f,"|<i%d>%s",i,buf->data);
              else
                fprintf(f,"<i%d>%s",i,buf->data);
              stringbuf_free(buf);
            }
          }

          fprintf(f,"}|{");
        }

        fprintf(f,"%d|",instr->id);

        if (OP_CONST == instr->opcode) {
          value_print(f,instr->cvalue);
          extra = ",fillcolor=\"#FF8080\"";
        }
        else if (OP_CALL == instr->opcode) {
          fprintf(f,"call(%s)",instr->cfun->ident.name);
          extra = ",fillcolor=\"#C0FFC0\"";
        }
        else if (OP_MAP == instr->opcode) {
          fprintf(f,"map(%s)",instr->cfun->ident.name);
          extra = ",fillcolor=\"#FFFFC0\"";
        }
        else {

          if ((OP_BUILTIN == instr->opcode) &&
              (nsname_equals(instr->bif->ident,nsname_temp(SPECIAL_NAMESPACE,"select")))) {
            fprintf(f,"select: ");
            if (NULL != instr->nametest) {
              fprintf(f,"%s",instr->nametest);
            }
            else {
              stringbuf *buf = stringbuf_new();
              /* FIXME: need to use the namespace map of the syntax tree node */
              seqtype_print_xpath(buf,instr->seqtypetest,xs_g->namespaces);
              fprintf(f,"%s",buf->data);
              stringbuf_free(buf);
            }
/*             fprintf(f,"\\n[%s]",df_axis_types[instr->axis]); */
          }
          else if (OP_BUILTIN == instr->opcode) {
            fprintf(f,"%s",instr->bif->ident.name);
          }
          else {
            fprintf(f,"%s",df_opstr(instr->opcode));
          }
          extra = ",fillcolor=\"#80E0FF\"";
        }

        if (types) {
          fprintf(f,"}|{");

          for (i = 0; i < instr->noutports; i++) {
            if (NULL != instr->outports[i].st) {
              stringbuf *buf = stringbuf_new();
              seqtype_print_fs(buf,instr->outports[i].st,xs_g->namespaces);
              if (0 < i)
                fprintf(f,"|<o%d>%s",i,buf->data);
              else
                fprintf(f,"<o%d>%s",i,buf->data);
              stringbuf_free(buf);
            }
          }

          fprintf(f,"}}");
        }

        fprintf(f,"\"%s];\n",extra);
      }

      for (i = 0; i < instr->noutports; i++) {
        if (NULL != instr->outports[i].dest) {
          df_instruction *dest = instr->outports[i].dest;
          int destp = instr->outports[i].destp;

          if (types && has_portlabels(instr))
            fprintf(f,"    %s%d:o%d",fun->ident.name,instr->id,i);
          else
            fprintf(f,"    %s%d",fun->ident.name,instr->id);

          fprintf(f," -> ");

          if (types && has_portlabels(dest))
            fprintf(f," %s%d:i%d [label=\"",dest->fun->ident.name,dest->id,destp);
          else
            fprintf(f," %s%d [label=\"",dest->fun->ident.name,dest->id);

/*           if ((OP_SPLIT == instr->opcode) && (0 == i)) */
/*             fprintf(f,"F"); */
/*           else if ((OP_SPLIT == instr->opcode) && (1 == i)) */
/*             fprintf(f,"T"); */

          fprintf(f,"\"");

          if (OP_DUP == instr->outports[i].dest->opcode)
            fprintf(f,",color=\"#808080\",arrowhead=none");
          else if (OP_DUP == instr->opcode)
            fprintf(f,",color=\"#808080\"");
          else if ((2 == i) &&
                   (OP_SPLIT == instr->opcode) &&
                   (OP_MERGE == instr->outports[i].dest->opcode))
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
    seqtype_print_fs(buf,fun->rtype,xs_g->namespaces);
    fprintf(f,"  return %s\n",buf->data);

    for (i = 0; i < fun->nparams; i++) {
      stringbuf_clear(buf);
      seqtype_print_fs(buf,fun->params[i].st,xs_g->namespaces);
      fprintf(f,"  param %d start %d type %s\n",i,fun->params[i].start->id,buf->data);
    }

    fprintf(f,"  start %d\n",fun->start->id);

    for (il = fun->instructions; il; il = il->next) {
      df_instruction *instr = (df_instruction*)il->data;
      int i;

      if (fun->mapseq == instr)
        continue;

      fprintf(f,"  %-5d %-20s",instr->id,df_opstr(instr->opcode));
      if (OP_RETURN != instr->opcode) {
        for (i = 0; i < instr->noutports; i++) {
          if (0 < i)
            fprintf(f," ");
          assert(NULL != instr->outports[i].dest);
          fprintf(f,"%d:%d",instr->outports[i].dest->id,instr->outports[i].destp);
        }
      }

      if (OP_CONST == instr->opcode) {
        assert(SEQTYPE_ITEM == instr->cvalue->st->type);
        assert(ITEM_ATOMIC == instr->cvalue->st->item->kind);

        if (instr->cvalue->st->item->type == xs_g->string_type) {
          char *escaped = escape_str(instr->cvalue->value.s);
          fprintf(f," = \"%s\"",escaped);
          free(escaped);
        }
        else if (instr->cvalue->st->item->type == xs_g->int_type) {
          fprintf(f," = %d",instr->cvalue->value.i);
        }
        else {
          assert(!"unknown constant type");
        }
      }

      fprintf(f,"\n");

      for (i = 0; i < instr->ninports; i++) {
        if (NULL == instr->inports[i].st) {
          fprintf(f,"    in %d: none\n",i);
        }
        else {
          stringbuf *buf = stringbuf_new();
          seqtype_print_fs(buf,instr->inports[i].st,xs_g->namespaces);
          fprintf(f,"    in %d: %s\n",i,buf->data);
          stringbuf_free(buf);
        }
      }

      for (i = 0; i < instr->noutports; i++) {
        if (NULL == instr->outports[i].st) {
          fprintf(f,"    out %d: none\n",i);
        }
        else {
          stringbuf *buf = stringbuf_new();
          seqtype_print_fs(buf,instr->outports[i].st,xs_g->namespaces);
          fprintf(f,"    out %d: %s\n",i,buf->data);
          stringbuf_free(buf);
        }
      }

    }
    fprintf(f,"}\n");
    if (fl->next)
      fprintf(f,"\n");
  }
  stringbuf_free(buf);
}

