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

#ifndef _XSLT_XSLT_H
#define _XSLT_XSLT_H

#include "xpath.h"
#include "xmlschema/xmlschema.h"
#include "util/namespace.h"
#include "util/xmlutils.h"
#include "util/list.h"
#include "dataflow/sequencetype.h"
#include "dataflow/serialization.h"

#define XSLT_DECLARATION                  0
#define XSLT_IMPORT                       1
#define XSLT_INSTRUCTION                  2
#define XSLT_LITERAL_RESULT_ELEMENT       3
#define XSLT_MATCHING_SUBSTRING           4
#define XSLT_NON_MATCHING_SUBSTRING       5
#define XSLT_OTHERWISE                    6
#define XSLT_OUTPUT_CHARACTER             7
#define XSLT_PARAM                        8
#define XSLT_SORT                         9
#define XSLT_TRANSFORM                    10
#define XSLT_VARIABLE                     11
#define XSLT_WHEN                         12
#define XSLT_WITH_PARAM                   13

#define XSLT_DECL_ATTRIBUTE_SET           14
#define XSLT_DECL_CHARACTER_MAP           15
#define XSLT_DECL_DECIMAL_FORMAT          16
#define XSLT_DECL_FUNCTION                17
#define XSLT_DECL_IMPORT_SCHEMA           18
#define XSLT_DECL_INCLUDE                 19
#define XSLT_DECL_KEY                     20
#define XSLT_DECL_NAMESPACE_ALIAS         21
#define XSLT_DECL_OUTPUT                  22
#define XSLT_DECL_PRESERVE_SPACE          23
#define XSLT_DECL_STRIP_SPACE             24
#define XSLT_DECL_TEMPLATE                25

#define XSLT_INSTR_ANALYZE_STRING         26
#define XSLT_INSTR_APPLY_IMPORTS          27
#define XSLT_INSTR_APPLY_TEMPLATES        28
#define XSLT_INSTR_ATTRIBUTE              29
#define XSLT_INSTR_CALL_TEMPLATE          30
#define XSLT_INSTR_CHOOSE                 31
#define XSLT_INSTR_COMMENT                32
#define XSLT_INSTR_COPY                   33
#define XSLT_INSTR_COPY_OF                34
#define XSLT_INSTR_ELEMENT                35
#define XSLT_INSTR_FALLBACK               36
#define XSLT_INSTR_FOR_EACH               37
#define XSLT_INSTR_FOR_EACH_GROUP         38
#define XSLT_INSTR_IF                     39
#define XSLT_INSTR_MESSAGE                40
#define XSLT_INSTR_NAMESPACE              41
#define XSLT_INSTR_NEXT_MATCH             42
#define XSLT_INSTR_NUMBER                 43
#define XSLT_INSTR_PERFORM_SORT           44
#define XSLT_INSTR_PROCESSING_INSTRUCTION 45
#define XSLT_INSTR_RESULT_DOCUMENT        46
#define XSLT_INSTR_SEQUENCE               47
#define XSLT_INSTR_TEXT                   48
#define XSLT_INSTR_VALUE_OF               49

#define XSLT_GROUPING_BY                  0
#define XSLT_GROUPING_ADJACENT            1
#define XSLT_GROUPING_STARTING_WITH       2
#define XSLT_GROUPING_ENDING_WITH         3

#define FLAG_REQUIRED                     1
#define FLAG_TUNNEL                       2
#define FLAG_OVERRIDE                     4
#define FLAG_DISABLE_OUTPUT_ESCAPING      8
#define FLAG_TERMINATE                    16

typedef struct xslt_source xslt_source;

struct xslt_source {
  xs_schema *schema;
  xl_snode *root;
  list *output_defs;
  list *space_decls;
};

struct xl_snode {
  int type;
  int flags;
  xl_snode *parent;
  xl_snode *prev;

  xl_snode *child;
  xl_snode *next;
  xl_snode *param;
  xl_snode *sort;

  xp_expr *select;
  xp_expr *expr1;
  xp_expr *expr2;
  xp_expr *name_expr;

  ns_map *namespaces;

  qname qn;
  qname mode;
  seqtype *st;
  int gmethod;
  char *strval;
  char **seroptions;
  list *qnametests;
  int importpred;
  int includens;
  int literal;

#if 0
  /* FIXME: turn this into a sourceloc */
  int defline; /* FIXME: set this during parsing */
  char *deffilename; /* FIXME: set this during parsing */
#endif
  sourceloc sloc;

  char *uri;
  nsname ident;

  int templateno;
  struct template *tmpl;
  struct df_outport *outp;
  list *templates;
};

xl_snode *xl_snode_new(int type);
void xl_snode_free(xl_snode *sn);
int xl_snode_resolve(xl_snode *first, xs_schema *s, const char *filename, error_info *ei);
xl_snode *xl_snode_resolve_var(xl_snode *from, qname varname);
void xp_expr_resolve_var(xp_expr *from, qname varname, xp_expr **defexpr, xl_snode **defnode);

int parse_xl_syntax(const char *str, const char *filename, int baseline, error_info *ei,
                    xp_expr **expr, xl_snode **sn, seqtype **st);
xl_snode *xl_snode_parse(const char *str, const char *filename, int baseline, error_info *ei);
void xl_snode_print_tree(xl_snode *sn, int indent);
xl_snode *xl_first_decl(xl_snode *root);
xl_snode *xl_next_decl(xl_snode *sn);

int xslt_parse(error_info *ei, const char *uri, xslt_source **source);
void xslt_source_free(xslt_source *source);
df_seroptions *xslt_get_output_def(xslt_source *source, nsname ident);

#ifndef _XSLT_XSLT_C

extern const char **xslt_element_names;

#endif /* _XSLT_XSLT_C */

#endif /* _XSLT_XSLT_H */
