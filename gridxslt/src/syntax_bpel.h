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

#ifndef _SRC_SYNTAX_BPEL_H
#define _SRC_SYNTAX_BPEL_H

#include "xslt/xslt.h"
#include <stdio.h>

#define BPEL_ACTIVITY_INVALID             0
#define BPEL_ACTIVITY_RECEIVE             1
#define BPEL_ACTIVITY_REPLY               2
#define BPEL_ACTIVITY_INVOKE              3
#define BPEL_ACTIVITY_ASSIGN              4
#define BPEL_ACTIVITY_THROW               5
#define BPEL_ACTIVITY_TERMINATE           6
#define BPEL_ACTIVITY_WAIT                7
#define BPEL_ACTIVITY_EMPTY               8
#define BPEL_ACTIVITY_SEQUENCE            9
#define BPEL_ACTIVITY_SWITCH              10
#define BPEL_ACTIVITY_WHILE               11
#define BPEL_ACTIVITY_PICK                12
#define BPEL_ACTIVITY_FLOW                13
#define BPEL_ACTIVITY_SCOPE               14
#define BPEL_ACTIVITY_COMPENSATE          15

typedef struct bp_snode bp_snode;

typedef struct bp_partner_link bp_partner_link;
typedef struct bp_partner_partner_link bp_partner_partner_link;
typedef struct bp_partner bp_partner;
typedef struct bp_variable bp_variable;
typedef struct bp_correlation_set bp_correlation_set;
typedef struct bp_catch bp_catch;
typedef struct bp_correlation bp_correlation;
typedef struct bp_on_message bp_on_message;
typedef struct bp_on_alarm bp_on_alarm;
typedef struct bp_target bp_target;
typedef struct bp_source bp_source;
typedef struct bp_activity bp_activity;
typedef struct bp_scope bp_scope;

struct bp_partner_link {
  char *name;                /* requried */
  qname_t partner_link_type; /* requried */
  char *my_role;             /* optional */
  char *partner_role;        /* optional */

  bp_partner_link *next;
};

struct bp_partner_partner_link {
  char *name; /* required */

  bp_partner_partner_link *next;
};

struct bp_partner {
  bp_partner_partner_link *partner;

  bp_partner *next;
};

struct bp_variable {
  char *name;           /* required */
  qname_t message_type; /* optional */
  qname_t type;         /* optional */
  qname_t element;      /* optional */

  bp_variable *next;
};

struct bp_correlation_set {
  char *name;           /* required */
  qname_t *properties;  /* required */

  bp_correlation_set *next;
};

struct bp_catch {
  qname_t fault_name;
  char *fault_variable;
  bp_activity *activities;

  bp_catch *next;
};

struct bp_correlation {
  char *set;    /* required */
  int initiate; /* optional; default 0 */
  /* FIXME: pattern (required by invoke) */

  bp_correlation *next;
};

struct bp_on_message {
  char *partner_link; /* required */
  qname_t port_type;  /* required */
  char *operation;   /* required */
  char *variable;    /* optional */

  bp_correlation *correlations;

  bp_activity *activities;

  bp_on_message *next;
};

struct bp_on_alarm {
  char *dfor;
  char *duntil;

  bp_activity *activities;

  bp_on_alarm *next;
};




struct bp_target {
  char *link_name;
  bp_target *next;
};

struct bp_source {
  char *link_name;
  char *transition_condition;
  bp_source *next;
};






struct bp_activity {
  int type;

  /* standard-attributes */
  char *name;
  char *join_condition;
  int supress_join_failure;

  /* standard-elements */
  bp_target *target;
  bp_source *source;

  /* receive */
  struct {
    char *partnerLink;  /* required */
    qname_t portType;   /* required */
    char *operation;    /* required */
    char *variable;     /* optional */
    int createInstance; /* default: no */

    bp_correlation *correlations;
  } receive;

  /* reply */
  struct {
    char *partnerLink; /* required */
    qname_t portType; /* required */
    char *operation; /* required */
    char *variable; /* optional */
    qname_t faultName; /* optional */

    bp_correlation *correlations;
  } reply;

  /* invoke */
  struct {
    char *partnerLink; /* required */
    qname_t portType; /* required */
    char *operation; /* required */
    char *inputVariable; /* optional */
    char *outputVariable; /* optional */

    bp_correlation *correlations;
    bp_catch *fh_catches;
    bp_activity *fh_catchall;
    bp_activity *compensation_handler;
  } invoke;





  bp_activity *next;
};

struct bp_scope {
  /* for <process> only */
  bp_partner_link *partner_links;
  bp_partner *partners;

  /* for <process> and <scope> */
  bp_variable *variables;
  bp_correlation_set *correlation_sets;
  bp_catch *fh_catches;
  bp_activity *fh_catchall;
  bp_activity *compensation_handler;
  bp_on_message *eh_on_message;
  bp_on_alarm *eh_on_alarm;

  bp_activity *activities;
};

struct bp_snode {
  int type;
};

void output_bpel(FILE *f, bp_scope *scope);
bp_scope *parse_bpel(FILE *f);

void bp_scope_free(bp_scope *scope);

#endif /* _SRC_SYNTAX_BPEL_H */
