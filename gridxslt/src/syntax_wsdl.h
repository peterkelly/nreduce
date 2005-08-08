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

#ifndef _SRC_SYNTAX_WSDL_H
#define _SRC_SYNTAX_WSDL_H

#include <stdio.h>

typedef struct ws_part ws_part;
typedef struct ws_message ws_message;
typedef struct ws_param ws_param;
typedef struct ws_operation ws_operation;
typedef struct ws_port_type ws_port_type;
typedef struct ws_bindparam ws_bindparam;
typedef struct ws_bindop ws_bindop;
typedef struct ws_binding ws_binding;
typedef struct ws_port ws_port;
typedef struct ws_service ws_service;
typedef struct wsdl wsdl;

struct ws_part {
  char *name; /* required */
  char *element; /* optional */
  char *type; /* optional */

  ws_part *next;
};

struct ws_message {
  char *ns;
  char *name; /* required */
  ws_part *parts;

  ws_message *next;
};

#define WSDL_PARAM_INVALID  0
#define WSDL_PARAM_INPUT    1
#define WSDL_PARAM_OUTPUT   2
#define WSDL_PARAM_FAULT    3

struct ws_param {
  char *name;
  char *message;
  ws_part *parts;

  ws_param *next;
};

struct ws_operation {
  char *name; /* required */

  ws_param *input;
  ws_param *output;
  ws_param *faults;
  ws_operation *next;
};

struct ws_port_type {
  char *ns;
  char *name;
  ws_operation *operations;
  ws_port_type *next;
};

struct ws_bindparam {
  char *name; /* required for faults */
  ws_bindparam *next;
};

struct ws_bindop {
  char *name; /* required */
  ws_bindparam *input;
  ws_bindparam *output;
  ws_bindparam *faults;

  ws_bindop *next;
};

struct ws_binding {
  char *ns;
  char *name; /* required */
  char *type; /* required */
  ws_port_type *pt;
  ws_bindop *operations;

  ws_binding *next;
};

struct ws_port {
  char *name;
  char *binding;
  ws_binding *b;
  ws_port *next;
};

struct ws_service {
  char *name;
  ws_port *ports;
  ws_service *next;
};

struct wsdl {
  char *name;
  char *target_ns;
  ws_message *messages;
  ws_port_type *port_types;
  ws_binding *bindings;
  ws_service *services;
};

wsdl *parse_wsdl(FILE *f);

#endif /* _SRC_SYNTAX_WSDL_H */
