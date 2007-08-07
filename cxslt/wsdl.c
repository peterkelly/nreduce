/*
 * This file is part of the GridXSLT project
 * Copyright (C) 2007 Peter Kelly (pmk@cs.adelaide.edu.au)
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
 * $Id: cxslt.c 557 2007-05-15 05:30:03Z pmkelly $
 *
 */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>
#include <assert.h>
#include <libxml/parser.h>
#include "cxslt.h"

static int is_element(xmlNodePtr n, const char *ns, const char *name)
{
  return ((XML_ELEMENT_NODE == n->type) &&
          (NULL != n->ns) &&
          !xmlStrcmp(n->ns->href,ns) &&
          !xmlStrcmp(n->name,name));
}

static int is_wsdl_element(xmlNodePtr n, const char *name)
{
  return is_element(n,WSDL_NAMESPACE,name);
}

static int is_wsdlsoap_element(xmlNodePtr n, const char *name)
{
  return is_element(n,WSDLSOAP_NAMESPACE,name);
}

wsdlfile *process_wsdl(elcgen *gen, const char *filename)
{
  list *l;
  wsdlfile *wf;
  for (l = gen->wsdlfiles; l; l = l->next) {
    wf = (wsdlfile*)l->data;
    if (!strcmp(wf->filename,filename))
      return wf;
  }
  wf = (wsdlfile*)calloc(1,sizeof(wsdlfile));
  wf->filename = strdup(filename);

  if (NULL == (wf->doc = xmlReadFile(wf->filename,NULL,0))) {
    fprintf(stderr,"Error parsing %s\n",wf->filename);
    exit(1);
  }

  wf->root = xmlDocGetRootElement(wf->doc);

  if (!is_wsdl_element(wf->root,"definitions")) {
    fprintf(stderr,"%s: invalid root element\n",wf->filename);
    exit(1);
  }

/*   printf("processed %s\n",wf->filename); */

  return wf;
}

xmlNodePtr wsdl_get_object(wsdlfile *wf, const char *type, const char *name)
{
  xmlNodePtr c;
  for (c = wf->root->children; c; c = c->next) {
    if (is_wsdl_element(c,type)) {
      if (NULL == name) {
        return c;
      }
      else {
        char *nameattr = xmlGetProp(c,"name");
        if (nameattr && !xmlStrcmp(nameattr,name))
          return c;
      }
    }
  }
  return NULL;
}

char *wsdl_get_url(wsdlfile *wf)
{
  xmlNodePtr n = wsdl_get_object(wf,"service",NULL);
  xmlNodePtr port;
  if (NULL == n) {
    fprintf(stderr,"%s: no service found\n",wf->filename);
    exit(1);
  }
  for (port = n->children; port; port = port->next) {
    if (is_wsdl_element(port,"port")) {
      xmlNodePtr address;
      for (address = port->children; address; address = address->next) {
        if (is_wsdlsoap_element(address,"address")) {
          char *location = xmlGetProp(address,"location");
          if (location)
            return location;
        }
      }
    }
  }
  fprintf(stderr,"%s: could not get url\n",wf->filename);
  exit(1);
  return NULL;
}

static qname get_part_element(wsdlfile *wf, xmlNodePtr message, const char *msgname)
{
  xmlNodePtr part;
  qname empty;
  memset(&empty,0,sizeof(empty));
  for (part = message->children; part; part = part->next) {
    if (is_wsdl_element(part,"part")) {
      char *element = xmlGetProp(part,"element");

      if (NULL == element)
        return empty;

      return string_to_qname(element,part);
    }
  }
  return empty;
}

static xmlNodePtr get_element(wsdlfile *wf, const char *elemname)
{
  xmlNodePtr types = wf->root->children;
  xmlNodePtr schema;
  xmlNodePtr element;

  while (types && !is_wsdl_element(types,"types"))
    types = types->next;
  if (NULL == types)
    fatal("%s: no type definitions",wf->filename);

  schema = types->children;
  while (schema && !is_element(schema,XMLSCHEMA_NAMESPACE,"schema"))
    schema = schema->next;
  if (NULL == schema)
    fatal("%s: no schema",wf->filename);

  for (element = schema->children; element; element = element->next) {
    if (is_element(element,XMLSCHEMA_NAMESPACE,"element")) {
      char *name = xmlGetProp(element,"name");
      if (!strcmp(name,elemname))
        return element;
    }
  }

  if (NULL == schema)
    fatal("%s: no such element \"%s\"",wf->filename,elemname);
  return NULL;
}

static list *get_element_args(wsdlfile *wf, xmlNodePtr elem)
{
  list *args = NULL;
  xmlNodePtr complexType = elem->children;
  xmlNodePtr sequence;
  xmlNodePtr arg;
  while (complexType && !is_element(complexType,XMLSCHEMA_NAMESPACE,"complexType"))
    complexType = complexType->next;
  if (NULL == complexType)
    return NULL;

  sequence = complexType->children;
  while (sequence && !is_element(sequence,XMLSCHEMA_NAMESPACE,"sequence"))
    sequence = sequence->next;
  if (NULL == sequence)
    return NULL;

  for (arg = sequence->children; arg; arg = arg->next) {
    if (is_element(arg,XMLSCHEMA_NAMESPACE,"element")) {
      char *name = xmlGetProp(arg,"name");
      if (name)
        list_append(&args,strdup(name));
    }
  }

  return args;
}

void wsdl_get_operation_messages(wsdlfile *wf, const char *opname,
                                 qname *inqn, qname *outqn,
                                 list **inargs, list **outargs)
{
  xmlNodePtr portType = wsdl_get_object(wf,"portType",NULL);
  xmlNodePtr operation;

  memset(inqn,0,sizeof(*inqn));
  memset(outqn,0,sizeof(*outqn));
  *inargs = NULL;
  *outargs = NULL;

  if (NULL == portType)
    fatal("%s: no port types found",wf->filename);

  for (operation = portType->children; operation; operation = operation->next) {
    if (is_wsdl_element(operation,"operation")) {
      char *name = xmlGetProp(operation,"name");
      char *inmsgname = NULL;
      char *outmsgname = NULL;
      xmlNodePtr message;
      xmlNodePtr inpmessage;
      xmlNodePtr outpmessage;
      qname inpelemname;
      qname outpelemname;
      xmlNodePtr inpelem;
      xmlNodePtr outpelem;

      if ((NULL == name) || strcmp(name,opname))
        continue;

      for (message = operation->children; message; message = message->next) {
        if (is_wsdl_element(message,"input"))
          inmsgname = xmlGetProp(message,"name");
        if (is_wsdl_element(message,"output"))
          outmsgname = xmlGetProp(message,"name");
      }
      if (NULL == inmsgname)
        fatal("%s: operation \"%s\" has no input message",wf->filename,opname);
      if (NULL == outmsgname)
        fatal("%s: operation \"%s\" has no output message",wf->filename,opname);

      inmsgname = strdup(inmsgname);
      outmsgname = strdup(outmsgname);

      if (NULL == (inpmessage = wsdl_get_object(wf,"message",inmsgname)))
        fatal("%s: no such message \"%s\"",wf->filename,inmsgname);
      if (NULL == (outpmessage = wsdl_get_object(wf,"message",outmsgname)))
        fatal("%s: no such message \"%s\"",wf->filename,outmsgname);

      inpelemname = get_part_element(wf,inpmessage,inmsgname);
      outpelemname = get_part_element(wf,outpmessage,outmsgname);

      if (NULL == inpelemname.localpart)
        fatal("%s: no element specified in message \"%s\"",wf->filename,inmsgname);
      if (NULL == outpelemname.localpart)
        fatal("%s: no element specified in message \"%s\"",wf->filename,outmsgname);

      if (NULL == (inpelem = get_element(wf,inpelemname.localpart)))
        fatal("%s: no such element \"%s\"",wf->filename,inpelemname.localpart);
      if (NULL == (outpelem = get_element(wf,outpelemname.localpart)))
        fatal("%s: no such element \"%s\"",wf->filename,outpelemname.localpart);

      *inqn = inpelemname;
      *outqn = outpelemname;
      *inargs = get_element_args(wf,inpelem);
      *outargs = get_element_args(wf,outpelem);

      return;
    }
  }
  fatal("%s: no operation named \"%s\"\n",wf->filename,opname);
}
