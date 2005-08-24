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

#ifndef _DATAFLOW_DATAFLOW_H
#define _DATAFLOW_DATAFLOW_H

#include "xmlschema/xmlschema.h"
#include "util/stringbuf.h"
#include "util/list.h"
#include "sequencetype.h"
#include <stdio.h>
#include <libxml/tree.h>

#define TYPE_INVALID                  0

#define TYPE_XS_STRING                1
#define TYPE_XS_BOOLEAN               2
#define TYPE_XS_DECIMAL               3
#define TYPE_XS_FLOAT                 4
#define TYPE_XS_DOUBLE                5
#define TYPE_XS_DURATION              6
#define TYPE_XS_DATETIME              7
#define TYPE_XS_TIME                  8
#define TYPE_XS_DATE                  9
#define TYPE_XS_GYEARMONTH            10
#define TYPE_XS_GYEAR                 11
#define TYPE_XS_GMONTHDAY             12
#define TYPE_XS_GDAY                  13
#define TYPE_XS_GMONTH                14
#define TYPE_XS_HEXBINARY             15
#define TYPE_XS_BASE64BINARY          16
#define TYPE_XS_ANYURI                17
#define TYPE_XS_QNAME                 18
#define TYPE_XS_NOTATION              19

#define TYPE_XS_NORMALIZEDSTRING      20
#define TYPE_XS_TOKEN                 21
#define TYPE_XS_LANGUAGE              22
#define TYPE_XS_NMTOKEN               23
#define TYPE_XS_NMTOKENS              24
#define TYPE_XS_NAME                  25
#define TYPE_XS_NCNAME                26
#define TYPE_XS_ID                    27
#define TYPE_XS_IDREF                 28
#define TYPE_XS_IDREFS                29
#define TYPE_XS_ENTITY                30
#define TYPE_XS_ENTITIES              31
#define TYPE_XS_INTEGER               32
#define TYPE_XS_NONPOSITIVEINTEGER    33
#define TYPE_XS_NEGATIVEINTEGER       34
#define TYPE_XS_LONG                  35
#define TYPE_XS_INT                   36
#define TYPE_XS_SHORT                 37
#define TYPE_XS_BYTE                  38
#define TYPE_XS_NONNEGATIVEINTEGER    39
#define TYPE_XS_UNSIGNEDLONG          40
#define TYPE_XS_UNSIGNEDINT           41
#define TYPE_XS_UNSIGNEDSHORT         42
#define TYPE_XS_UNSIGNEDBYTE          43
#define TYPE_XS_POSITIVEINTEGER       44

#define TYPE_XDT_UNTYPED              45
#define TYPE_XDT_UNTYPEDATOMIC        46
#define TYPE_XDT_ANYATOMICTYPE        47
#define TYPE_XDT_DAYTIMEDURATION      48
#define TYPE_XDT_YEARMONTHDURATION    49

#define TYPE_LAST_XMLSCHEMA_TYPE      49

#define TYPE_ATTRIBUTE                50
#define TYPE_COMMENT                  51
#define TYPE_DOCUMENT_NODE            52
#define TYPE_ELEMENT                  53
#define TYPE_PROCESSING_INSTRUCTION   54
#define TYPE_TEXT                     55
#define TYPE_NODE                     56
#define TYPE_ITEM                     57
#define TYPE_NONE                     58

#define TYPE_NUMERIC                  59

#define TYPE_COUNT                    60

#define OP_SPECIAL_BASE               1000

#define OP_SPECIAL_CONST              1000
#define OP_SPECIAL_DUP                1001
#define OP_SPECIAL_SPLIT              1002
#define OP_SPECIAL_MERGE              1003
#define OP_SPECIAL_CALL               1004
#define OP_SPECIAL_MAP                1005
#define OP_SPECIAL_PASS               1006
#define OP_SPECIAL_SWALLOW            1007
#define OP_SPECIAL_RETURN             1008
#define OP_SPECIAL_PRINT              1009
#define OP_SPECIAL_SEQUENCE           1010 /* not reallly special */
#define OP_SPECIAL_GATE               1011

/* not reallly special */
#define OP_SPECIAL_DOCUMENT           1012
#define OP_SPECIAL_ELEMENT            1013
#define OP_SPECIAL_ATTRIBUTE          1014
#define OP_SPECIAL_PI                 1015
#define OP_SPECIAL_COMMENT            1016
#define OP_SPECIAL_VALUE_OF           1017
#define OP_SPECIAL_TEXT               1018
#define OP_SPECIAL_NAMESPACE          1019
#define OP_SPECIAL_SELECT             1020
#define OP_SPECIAL_FILTER             1021
#define OP_SPECIAL_EMPTY              1022
#define OP_SPECIAL_CONTAINS_NODE      1023
#define OP_SPECIAL_COUNT              24

#define AXIS_INVALID                  0
#define AXIS_CHILD                    1
#define AXIS_DESCENDANT               2
#define AXIS_ATTRIBUTE                3
#define AXIS_SELF                     4
#define AXIS_DESCENDANT_OR_SELF       5
#define AXIS_FOLLOWING_SIBLING        6
#define AXIS_FOLLOWING                7
#define AXIS_NAMESPACE                8
#define AXIS_PARENT                   9
#define AXIS_ANCESTOR                 10
#define AXIS_PRECEDING_SIBLING        11
#define AXIS_PRECEDING                12
#define AXIS_ANCESTOR_OR_SELF         13
#define AXIS_COUNT                    14

typedef struct df_optype df_optype;
typedef struct df_opdef df_opdef;
typedef struct df_inport df_inport;
typedef struct df_outport df_outport;
typedef struct df_instruction df_instruction;
typedef struct df_parameter df_parameter;
typedef struct df_function df_function;
typedef struct df_state df_state;

struct df_optype {
  int type;
  int occurrence;
};

struct df_opdef {
  char *name;
  int nparams;
  df_optype rettype;
  df_optype paramtypes[8];
};

struct df_inport {
  df_seqtype *seqtype;
  df_instruction *source;
  int sourcep;
  int from_special;
};

struct df_outport {
  df_seqtype *seqtype;
  df_instruction *dest;
  int destp;

  df_instruction *owner;
  int portno;
};

struct df_instruction {
  int opcode;
  int id;

  int ninports;
  df_inport *inports;

  int noutports;
  df_outport *outports;

  df_value *cvalue;
/*   df_seqtype *ctype; */
  df_function *cfun;

  struct df_activity *act;
  df_function *fun;
  int internal;

  int computed;

  char *nametest;
  df_seqtype *seqtypetest;
  int axis;
};

struct df_parameter {
  df_seqtype *seqtype;
  df_instruction *start;
  df_outport *outport;
  df_outport *varsource;
};

struct df_function {
  nsname ident;
  list *instructions;
  int nparams;
  df_parameter *params;
  df_seqtype *rtype;
  df_instruction *start;
  df_instruction *ret;
  int nextid;
  df_instruction *mapseq;
  df_state *state;
  int isapply;
  char *mode;
};

int df_compute_types(df_function *fun);
int df_check_is_atomic_type(df_state *state, df_seqtype *st, xs_type *type);
int df_check_derived_atomic_type(df_state *state, df_value *v, xs_type *type);
int df_get_op_id(char *name);
int df_execute_op(df_state *state, df_instruction *instr, int opcode,
                  df_value **values, df_value **result);

void df_init_function(df_state *state, df_function *fun);
df_function *df_new_function(df_state *state, const nsname ident);
df_function *df_lookup_function(df_state *state, const nsname ident);
void df_free_function(df_function *fun);

df_instruction *df_add_instruction(df_state *state, df_function *fun, int opcode);
void df_delete_instruction(df_state *state, df_function *fun, df_instruction *instr);
void df_free_instruction_inports(df_instruction *instr);
void df_free_instruction(df_instruction *instr);

struct df_state {
  list *functions;
  list *activities;
  df_function *init;
  df_function *deftmpl;
  xs_schema *schema;
  int actno;
  int nextanonid;
  df_seqtype *intype;
  int trace;
  error_info ei;
};

const char *df_opstr(int opcode);

df_node *df_node_from_xmlnode(xmlNodePtr xn);

df_state *df_state_new(xs_schema *schema);
void df_state_free(df_state *state);

void df_output_dot(df_state *s, FILE *f);
void df_output_df(df_state *s, FILE *f);

#ifndef _DATAFLOW_DATAFLOW_C
extern const char *df_type_names[TYPE_COUNT];
extern const char *df_special_op_names[OP_SPECIAL_COUNT];
extern const char *df_item_types[ITEM_COUNT];
extern const char *df_axis_types[AXIS_COUNT];
#endif

#endif /* _DATAFLOW_DATAFLOW_H */
