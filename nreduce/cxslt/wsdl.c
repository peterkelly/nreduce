/*
 * This file is part of the nreduce project
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

static int is_wsdl_element(xmlNodePtr n, const char *name)
{
  return is_element(n,WSDL_NS,name);
}

int process_wsdl(elcgen *gen, const char *filename, wsdlfile **wfout)
{
  list *l;
  wsdlfile *wf;
  xmlNodePtr n;

  *wfout = NULL;

  for (l = gen->wsdlfiles; l; l = l->next) {
    wf = (wsdlfile*)l->data;
    if (!strcmp(wf->filename,filename)) {
      *wfout = wf;
      return 1;
    }
  }
  wf = (wsdlfile*)calloc(1,sizeof(wsdlfile));
  wf->filename = strdup(filename);

  for (n = gen->toplevel->children; n; n = n->next) {
    if ((NULL == n->ns) && (!strcmp((char*)n->name,"wsdlfile"))) {
      char *source = xmlGetProp(n,"source");
      if (!strcmp(source,filename)) {
        n = n->children;
        while (n && (XML_ELEMENT_NODE != n->type))
          n = n->next;
        if (n) {
          wf->root = n;
          break;
        }
      }
      free(source);
    }
  }

  if (NULL == wf->root)
    return gen_error(gen,"Could not find WSDL for %s",wf->filename);

  if (!is_wsdl_element(wf->root,"definitions"))
    return gen_error(gen,"%s: invalid root element",wf->filename);

  *wfout = wf;
  return 1;
}

static xmlNodePtr get_child(xmlNodePtr parent, const char *elemns, const char *elemname,
                            const char *attr, const char *value)
{
  xmlNodePtr c;
  for (c = parent->children; c; c = c->next) {
    if (!is_element(c,elemns,elemname))
      continue;
    if ((NULL == attr) || (NULL == value))
      return c;
    if (xmlHasProp(c,attr)) {
      char *str = xmlGetProp(c,attr);
      int cmp = strcmp(str,value);
      free(str);
      if (!cmp)
        return c;
    }
  }
  return NULL;
}

static xmlNodePtr get_object(xmlNodePtr wsdlroot, const char *type, const char *name)
{
  return get_child(wsdlroot,WSDL_NS,type,"name",name);
}

int wsdl_get_style(elcgen *gen, xmlNodePtr wsdlroot, int *styleout)
{
  xmlNodePtr service;
  xmlNodePtr port;
  xmlNodePtr binding;
  xmlNodePtr soapb;

  *styleout = 0;
  if ((NULL != (service = get_child(wsdlroot,WSDL_NS,"service",NULL,NULL))) &&
      (NULL != (port = get_child(service,WSDL_NS,"port",NULL,NULL))) &&
      xmlHasProp(port,"binding")) {

    qname bqn = get_qname_attr(port,"binding");
    if ((NULL != (binding = get_child(wsdlroot,WSDL_NS,"binding","name",bqn.localpart))) &&
        (NULL != (soapb = get_child(binding,WSDLSOAP_NS,"binding",NULL,NULL))) &&
        xmlHasProp(soapb,"style")) {

      char *style = xmlGetProp(soapb,"style");
      if (!strcmp(style,"rpc"))
        *styleout = STYLE_RPC;
      else if (!strcmp(style,"document"))
        *styleout = STYLE_DOCWRAPPED;
      else
        gen_error(gen,"SOAP binding has invalid style \"%s\"",style);
      free(style);
    }
    free_qname(bqn);
  }

  if (0 == *styleout)
    return gen_error(gen,"Could not find SOAP binding style");
  else
    return 1;
}

int wsdl_get_url(elcgen *gen, wsdlfile *wf, char **urlout)
{
  xmlNodePtr service;
  xmlNodePtr port;
  xmlNodePtr address;

  if ((NULL != (service = get_object(wf->root,"service",NULL))) &&
      (NULL != (port = get_child(service,WSDL_NS,"port",NULL,NULL))) &&
      (NULL != (address = get_child(port,WSDLSOAP_NS,"address",NULL,NULL)))) {
    if (xmlHasProp(address,"location")) {
/*       printf("service name = %s\n",xmlGetProp(service,"name")); */
/*       printf("port name = %s\n",xmlGetProp(port,"name")); */
/*       printf("address location = %s\n",xmlGetProp(address,"location")); */
      *urlout = xmlGetProp(address,"location");
      return 1;
    }
  }
  return gen_error(gen,"could not get url");
}

static xmlNodePtr get_element_r(xmlNodePtr schema, const char *elemname)
{
  xmlNodePtr n;
  for (n = schema->children; n; n = n->next) {
    if (is_element(n,XMLSCHEMA_NS,"element")) {
      char *name = xmlGetProp(n,"name");
      if (!strcmp(name,elemname))
        return n;
    }
    else if (is_element(n,XMLSCHEMA_NS,"schema")) {
      xmlNodePtr element;
      if (NULL != (element = get_element_r(n,elemname)))
        return element;
    }
  }
  return NULL;
}

static xmlNodePtr get_wsdl_types(xmlNodePtr wsdlroot)
{
  xmlNodePtr n;
  for (n = wsdlroot->children; n; n = n->next) {
    if (is_wsdl_element(n,"types"))
      return n;
  }
  return NULL;
}

static xmlNodePtr get_element(wsdlfile *wf, const char *elemname)
{
  xmlNodePtr types;
  xmlNodePtr n;
  xmlNodePtr element;

  if (NULL == (types = get_wsdl_types(wf->root)))
    return NULL;

  for (n = types->children; n; n = n->next) {
    if (is_element(n,XMLSCHEMA_NS,"schema")) {
      if (NULL != (element = get_element_r(n,elemname)))
        return element;
    }
  }

  return NULL;
}

static xmlNodePtr get_complex_type(xmlNodePtr root, qname name)
{
  xmlNodePtr sn;
  xmlNodePtr type = NULL;
  for (sn = root->children; sn && (NULL == type); sn = sn->next) {
    if (is_element(sn,XMLSCHEMA_NS,"schema")) {

      char *targetNamespace = xmlGetProp(sn,"targetNamespace");
      if (!nullstrcmp(targetNamespace,name.uri)) {
        xmlNodePtr tn;
        for (tn = sn->children; tn && (NULL == type); tn = tn->next) {
          if (is_element(tn,XMLSCHEMA_NS,"complexType")) {
            char *typename = xmlGetProp(tn,"name");
            if (typename && !strcmp(typename,name.localpart))
              type = tn;
            free(typename);
          }
        }
      }
      free(targetNamespace);

      if (NULL == type)
        type = get_complex_type(sn,name);
    }
    else if (XML_ELEMENT_NODE == sn->type) {
      type = get_complex_type(sn,name);
    }
  }
  return type;
}

static char *element_namespace(xmlNodePtr elem)
{
  int qualified = 0;
  xmlNodePtr n;
  if (attr_equals(elem,"form","qualified"))
    qualified = 1;
  n = elem;
  while (n && !is_element(n,XMLSCHEMA_NS,"schema"))
    n = n->parent;
  if (n) {
    if (attr_equals(n,"elementFormDefault","qualified"))
      qualified = 1;
    if (qualified && xmlHasProp(n,"targetNamespace"))
      return xmlGetProp(n,"targetNamespace");
  }
  return strdup("");
}

static int has_simple_type(xmlNodePtr n)
{
  int simple = 0;
  if (xmlHasProp(n,"type")) {
    qname qn = get_qname_attr(n,"type");
    if (!strcmp(qn.uri,XMLSCHEMA_NS))
      simple = 1;
    free_qname(qn);
  }
  return simple;
}

static int get_element_args(elcgen *gen, xmlNodePtr types, xmlNodePtr elem, list **args)
{
  xmlNodePtr complexType;
  xmlNodePtr sequence;
  xmlNodePtr arg;

  *args = NULL;

  if (xmlHasProp(elem,"type")) {
    char *typename = xmlGetProp(elem,"type");
    qname tqn = string_to_qname(typename,elem);
    complexType = get_complex_type(types,tqn);
    free_qname(tqn);
    free(typename);
    if (NULL == complexType)
      return gen_error(gen,"no complex type %s",typename);
  }
  else {
    complexType = elem->children;
    while (complexType && !is_element(complexType,XMLSCHEMA_NS,"complexType"))
      complexType = complexType->next;
    if (NULL == complexType)
      return gen_error(gen,"no complex type");
  }

  sequence = complexType->children;
  while (sequence && !is_element(sequence,XMLSCHEMA_NS,"sequence"))
    sequence = sequence->next;
  if (NULL == sequence)
    return 1;

  for (arg = sequence->children; arg; arg = arg->next) {
    if (is_element(arg,XMLSCHEMA_NS,"element") && xmlHasProp(arg,"name")) {
      wsarg *wa = (wsarg*)calloc(1,sizeof(wsarg));
      wa->simple = has_simple_type(arg);
      wa->list = 0;
      wa->uri = element_namespace(arg);
      wa->localpart = xmlGetProp(arg,"name");
      list_append(args,wa);

      if (xmlHasProp(arg,"maxOccurs")) {
        char *max = xmlGetProp(arg,"maxOccurs");
        if (strcmp(max,"1"))
          wa->list = 1;
        free(max);
      }
    }
  }

  return 1;
}

static void get_message_parts(elcgen *gen, xmlNodePtr message, list **parts)
{
  xmlNodePtr n;
  for (n = message->children; n; n = n->next) {
    if (is_wsdl_element(n,"part") && xmlHasProp(n,"name")) {
      wsarg *wa = (wsarg*)calloc(1,sizeof(wsarg));
      wa->simple = has_simple_type(n);
      wa->list = 0;
      wa->uri = strdup("");
      wa->localpart = xmlGetProp(n,"name");
      list_append(parts,wa);
    }
  }
}

static int get_message_names(elcgen *gen, wsdlfile *wf, xmlNodePtr operation,
                             const char *opname,
                             qname *in, qname *out)
{
  xmlNodePtr n;

  memset(in,0,sizeof(qname));
  memset(out,0,sizeof(qname));

  for (n = operation->children; n; n = n->next) {
    if (is_wsdl_element(n,"input") && xmlHasProp(n,"message"))
      *in = get_qname_attr(n,"message");
    if (is_wsdl_element(n,"output") && xmlHasProp(n,"message"))
      *out = get_qname_attr(n,"message");
  }

  if (NULL == in->localpart)
    gen_error(gen,"operation \"%s\" has no input message",opname);
  else if (NULL == out->localpart)
    gen_error(gen,"operation \"%s\" has no output message",opname);
  else
    return 1;

  free_qname(*in);
  free_qname(*out);
  return 0;
}

static xmlNodePtr get_operation(wsdlfile *wf, const char *opname)
{
  xmlNodePtr n;
  xmlNodePtr found = NULL;
  xmlNodePtr portType = get_object(wf->root,"portType",NULL);

  if (NULL == portType)
    return NULL;

  for (n = portType->children; n && (NULL == found); n = n->next) {
    if (is_wsdl_element(n,"operation") && xmlHasProp(n,"name")) {
      char *name = xmlGetProp(n,"name");
      if (name && !strcmp(name,opname))
        found = n;
      free(name);
    }
  }
  return found;
}

static qname get_message_elemname(xmlNodePtr message)
{
  xmlNodePtr n;
  qname empty;
  memset(&empty,0,sizeof(empty));
  for (n = message->children; n; n = n->next) {
    if (is_wsdl_element(n,"part") && xmlHasProp(n,"element"))
      return get_qname_attr(n,"element");
  }
  return empty;
}

int wsdl_get_operation_messages(elcgen *gen,
                                wsdlfile *wf, const char *opname,
                                qname *inqn, qname *outqn,
                                list **inargs, list **outargs)
{
  xmlNodePtr types;
  xmlNodePtr operation;
  qname inmsgqname;
  qname outmsgqname;
  xmlNodePtr inpmessage;
  xmlNodePtr outpmessage;
  int style;
  int r = 0;
  char *targetns;

  memset(inqn,0,sizeof(*inqn));
  memset(outqn,0,sizeof(*outqn));
  *inargs = NULL;
  *outargs = NULL;

  if (!wsdl_get_style(gen,wf->root,&style))
    return 0;

/*   printf("wsdl style = %d\n",style); */

  if (NULL == (targetns = xmlGetProp(wf->root,"targetNamespace")))
    return gen_error(gen,"no target namespace");

  if (NULL == (types = get_wsdl_types(wf->root)))
    return gen_error(gen,"no <types> element");

  if (NULL == (operation = get_operation(wf,opname)))
    return gen_error(gen,"no operation named \"%s\"",opname);

  if (!get_message_names(gen,wf,operation,opname,&inmsgqname,&outmsgqname))
    return 0;

  if (NULL == (inpmessage = get_object(wf->root,"message",inmsgqname.localpart)))
    gen_error(gen,"no such message \"%s\"",inmsgqname.localpart);
  else if (NULL == (outpmessage = get_object(wf->root,"message",outmsgqname.localpart)))
    gen_error(gen,"no such message \"%s\"",outmsgqname.localpart);
  else if (STYLE_RPC == style) {
    inqn->uri = strdup(targetns);
    inqn->localpart = strdup(opname);
    outqn->uri = strdup(targetns);
    outqn->localpart = (char*)malloc(strlen(opname)+strlen("Response")+1);
    sprintf(outqn->localpart,"%sResponse",opname);
    get_message_parts(gen,inpmessage,inargs);
    get_message_parts(gen,outpmessage,outargs);
    r = 1;
  }
  else { /* STYLE_DOCWRAPPED */
    qname inpelemname = QNAME_NULL;
    qname outpelemname = QNAME_NULL;
    xmlNodePtr inpelem = NULL;
    xmlNodePtr outpelem = NULL;

    inpelemname = get_message_elemname(inpmessage);
    outpelemname = get_message_elemname(outpmessage);

    if (NULL == inpelemname.localpart)
      gen_error(gen,"no element specified in message \"%s\"",inmsgqname.localpart);
    else if (NULL == outpelemname.localpart)
      gen_error(gen,"no element specified in message \"%s\"",outmsgqname.localpart);
    else if (NULL == (inpelem = get_element(wf,inpelemname.localpart)))
      gen_error(gen,"no such element \"%s\"",inpelemname.localpart);
    else if (NULL == (outpelem = get_element(wf,outpelemname.localpart)))
      gen_error(gen,"no such element \"%s\"",outpelemname.localpart);
    else {
/*       printf("opname = %s\n",opname); */
/*       printf("input message name = {%s}%s\n",inmsgqname.uri,inmsgqname.localpart); */
/*       printf("output message name = {%s}%s\n",outmsgqname.uri,outmsgqname.localpart); */
/*       printf("input element name = {%s}%s\n",inpelemname.uri,inpelemname.localpart); */
/*       printf("output element name = {%s}%s\n",outpelemname.uri,outpelemname.localpart); */

      *inqn = inpelemname;
      *outqn = outpelemname;
      if (get_element_args(gen,types,inpelem,inargs) &&
          get_element_args(gen,types,outpelem,outargs))
        r = 1;
    }

    if (0 == r) {
      list_free(*inargs,free_wsarg_ptr);
      list_free(*outargs,free_wsarg_ptr);
      free_qname(inpelemname);
      free_qname(outpelemname);
    }
  }

  free_qname(inmsgqname);
  free_qname(outmsgqname);
  free(targetns);

  return r;
}
