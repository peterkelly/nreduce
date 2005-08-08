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

#include "syntax_wsdl.h"
#include "xslt/parse.h"
#include "util/namespace.h"
#include <libxml/xmlwriter.h>
#include <libxml/xmlIO.h>
#include <libxml/parser.h>
#include <libxml/tree.h>
#include <stdlib.h>
#include <string.h>

void wsdl_skip_others_strict(xmlNodePtr *c)
{
  while ((*c) && (XML_ELEMENT_NODE != (*c)->type))
    (*c) = (*c)->next;
}

void wsdl_skip_others(xmlNodePtr *c)
{
/*   while ((*c) && */
/*          ((XML_ELEMENT_NODE != (*c)->type) || */
/*            (NULL == (*c)->ns) || */
/*            (strcmp((*c)->ns->href,WSDL_NAMESPACE) && */
/*             strcmp((*c)->ns->href,SOAP_NAMESPACE)))) { */
/*     (*c) = (*c)->next; */
/*   } */
  while ((*c) &&
         ((XML_ELEMENT_NODE != (*c)->type) ||
           (NULL == (*c)->ns) ||
           (strcmp((*c)->ns->href,WSDL_NAMESPACE)))) {
    (*c) = (*c)->next;
  }
}

void wsdl_next_element_strict(xmlNodePtr *c)
{
  *c = (*c)->next;
  wsdl_skip_others_strict(c);
}

void wsdl_next_element(xmlNodePtr *c)
{
  *c = (*c)->next;
  wsdl_skip_others(c);
}

int wsdl_get_message(wsdl *w, char *name, xmlNodePtr n, xmlDocPtr doc, ws_message **mout)
{
  /* FIXME: should also check imports here; the nsprefix may reference an imported message */
  ws_message *m;
  char *namespace;
  char *localpart;

  if (0 != get_ns_name_from_qname(n,doc,name,&namespace,&localpart))
    return -1;

  *mout = NULL;
  for (m = w->messages; m && (NULL == *mout); m = m->next) {
    if (((NULL != namespace) && m->ns && !strcmp(m->ns,namespace) &&
                                         !strcmp(m->name,localpart)) ||
        ((NULL == namespace) && !m->ns && !strcmp(m->name,localpart)))
      *mout = m;
  }
  free(namespace);
  free(localpart);

  if (NULL == *mout)
    return parse_error(n,"No such message \"%s\"",name);

  return 0;
}

int wsdl_get_port_type(wsdl *w, char *name, xmlNodePtr n, xmlDocPtr doc, ws_port_type **ptout)
{
  /* FIXME: should also check imports here; the nsprefix may reference an imported message */
  ws_port_type *pt;
  char *namespace;
  char *localpart;

  if (0 != get_ns_name_from_qname(n,doc,name,&namespace,&localpart))
    return -1;

  *ptout = NULL;
  for (pt = w->port_types; pt && (NULL == *ptout); pt = pt->next) {
    if (((NULL != namespace) && pt->ns && !strcmp(pt->ns,namespace) &&
                                         !strcmp(pt->name,localpart)) ||
        ((NULL == namespace) && !pt->ns && !strcmp(pt->name,localpart)))
      *ptout = pt;
  }
  free(namespace);
  free(localpart);

  if (NULL == *ptout)
    return parse_error(n,"No such portType \"%s\"",name);

  return 0;
}

int wsdl_get_binding(wsdl *w, char *name, xmlNodePtr n, xmlDocPtr doc, ws_binding **bout)
{
  /* FIXME: should also check imports here; the nsprefix may reference an imported message */
  ws_binding *b;
  char *namespace;
  char *localpart;

  if (0 != get_ns_name_from_qname(n,doc,name,&namespace,&localpart))
    return -1;

  *bout = NULL;
  for (b = w->bindings; b && (NULL == *bout); b = b->next) {
    if (((NULL != namespace) && b->ns && !strcmp(b->ns,namespace) &&
                                         !strcmp(b->name,localpart)) ||
        ((NULL == namespace) && !b->ns && !strcmp(b->name,localpart)))
      *bout = b;
  }
  free(namespace);
  free(localpart);

  if (NULL == *bout)
    return parse_error(n,"No such binding \"%s\"",name);

  return 0;
}

/* FIXME: also need "get" method for bindings */
/* FIXME: also need "get" method for service */
/* FIXME: also need "get" method for port */

int wsdl_parse_message(wsdl *w, xmlNodePtr n, xmlDocPtr doc)
{
  xmlNodePtr c;
  ws_message *m;
  ws_part **pptr;
  char *name;

  ws_message **mptr = &w->messages;
  while (*mptr)
    mptr = &((*mptr)->next);

  name = xmlGetProp(n,"name");
  if (NULL == name)
    return missing_attribute(n,"name");

  m = (ws_message*)calloc(1,sizeof(ws_message));
  if (w->target_ns)
    m->ns = strdup(w->target_ns);
  m->name = strdup(name);
  pptr = &m->parts;
  *mptr = m;
  mptr = &m->next;

  c = n->children;
  wsdl_skip_others_strict(&c);
  if (!c)
    return 0;

  if (check_element(c,"documentation",WSDL_NAMESPACE)) {
    /* skip */
    wsdl_next_element_strict(&c);
  }

  while (c) {
    char *name = xmlGetProp(c,"name");
    char *element = xmlGetProp(c,"element");
    char *type = xmlGetProp(c,"type");
    ws_part *p;

    if (!check_element(c,"part",WSDL_NAMESPACE))
      return invalid_element(c);

    if (NULL == name)
      return missing_attribute(c,"name");

    /* We enforce the constraint that either element or type must be specified... this is not
       strictly required by the spec, as it states that extensions can add their own attributes
       for specifying the type of an element. But since we support no such extensions, we need
       one of these attributes to be specified. */
    if ((NULL == element) && (NULL == type))
      return parse_error(c,"\"element\" or \"type\" must be specified for message part");

    /* Similarly, we don't allow specification of _both_ attributes */
    if ((NULL != element) && (NULL != type))
      return parse_error(c,"Only one of \"element\" or \"type\" can be specified "
                            "for message part");

    p = (ws_part*)calloc(1,sizeof(ws_part));
    p->name = strdup(name);
    if (element)
      p->element = strdup(element);
    if (type)
      p->type = strdup(type);
    *pptr = p;
    pptr = &p->next;

    wsdl_next_element_strict(&c);
  }

  return 0;
}

int wsdl_parse_param(wsdl *w, xmlNodePtr c2, xmlDocPtr doc, ws_param **p, int req_name)
{
  char *name = xmlGetProp(c2,"name");
  char *message = xmlGetProp(c2,"message");
  ws_message *m;
  *p = NULL;
  if (NULL == message)
    return missing_attribute(c2,"message");
  if (req_name && (NULL == name))
    return missing_attribute(c2,"name");
  if (0 != wsdl_get_message(w,message,c2,doc,&m))
    return -1;

  *p = (ws_param*)calloc(1,sizeof(ws_param));
  if (NULL != name)
    (*p)->name = strdup(name);
  (*p)->message = strdup(message);
  (*p)->parts = m->parts;
  return 0;
}

int wsdl_parse_port_type(wsdl *w, xmlNodePtr n, xmlDocPtr doc)
{
  xmlNodePtr c;
  ws_port_type *pt;
  ws_operation **optr;
  char *name;

  ws_port_type **ptptr = &w->port_types;
  while (*ptptr)
    ptptr = &((*ptptr)->next);

  name = xmlGetProp(n,"name");
  if (NULL == name)
    return missing_attribute(n,"name");

  pt = (ws_port_type*)calloc(1,sizeof(ws_port_type));
  if (w->target_ns)
    pt->ns = strdup(w->target_ns);
  pt->name = strdup(name);
  optr = &pt->operations;
  *ptptr = pt;
  ptptr = &pt->next;

  c = n->children;
  wsdl_skip_others_strict(&c);
  if (!c)
    return 0;

  if (check_element(c,"documentation",WSDL_NAMESPACE)) {
    /* skip */
    wsdl_next_element_strict(&c);
  }

  while (c) {
    char *name = xmlGetProp(c,"name");
    ws_operation *o;
    ws_param **fptr;
    xmlNodePtr c2;

    if (!check_element(c,"operation",WSDL_NAMESPACE))
      return invalid_element(c);

    if (NULL == name)
      return missing_attribute(c,"name");

    o = (ws_operation*)calloc(1,sizeof(ws_operation));
    o->name = strdup(name);
    fptr = &o->faults;
    *optr = o;
    optr = &o->next;

    c2 = c->children;
    wsdl_skip_others_strict(&c2);

    if (check_element(c2,"documentation",WSDL_NAMESPACE)) {
      /* skip */
      wsdl_next_element_strict(&c2);
    }

    if (check_element(c2,"input",WSDL_NAMESPACE)) {
      if (0 != wsdl_parse_param(w,c2,doc,&o->input,0))
        return -1;
      wsdl_next_element_strict(&c2);
    }

    if (check_element(c2,"output",WSDL_NAMESPACE)) {
      if (0 != wsdl_parse_param(w,c2,doc,&o->output,0))
        return -1;
      wsdl_next_element_strict(&c2);
    }

    while (c2) {

      if (!check_element(c2,"fault",WSDL_NAMESPACE))
        return invalid_element(c2);

      if (0 != wsdl_parse_param(w,c2,doc,fptr,1))
        return -1;
      fptr = &((*fptr)->next);

      wsdl_next_element_strict(&c2);
    }

    wsdl_next_element_strict(&c);
  }

  return 0;
}

int wsdl_parse_bindparam(wsdl *w, ws_operation *o, xmlNodePtr c2, xmlDocPtr doc,
                         ws_bindparam **p, int fault)
{
  char *name = xmlGetProp(c2,"name");
  *p = NULL;
  if (fault && (NULL == name))
    return missing_attribute(c2,"name");

  if (fault) {
    ws_param *wsp;
    int found = 0;
    for (wsp = o->faults; wsp && !found; wsp = wsp->next)
      if (!strcmp(wsp->name,name))
        found = 1;
    if (!found)
      return parse_error(c2,"No such fault \"%s\" declared on operation \"%s\"\n",name,o->name);
  }

  *p = (ws_bindparam*)calloc(1,sizeof(ws_bindparam));
  if (NULL != name)
    (*p)->name = strdup(name);
  return 0;
}

int wsdl_parse_binding(wsdl *w, xmlNodePtr n, xmlDocPtr doc)
{
  char *name = xmlGetProp(n,"name");
  char *type = xmlGetProp(n,"type");
  ws_port_type *pt;
  ws_binding *b;
  xmlNodePtr c;
  ws_binding **bptr = &w->bindings;
  ws_bindop **boptr;

  while (*bptr)
    bptr = &((*bptr)->next);

  if (NULL == name)
    return missing_attribute(n,"name");
  if (NULL == type)
    return missing_attribute(n,"type");
  if (0 != wsdl_get_port_type(w,type,n,doc,&pt))
    return -1;

  /* FIXME: make sure the name matches an operation in the port type */

  b = (ws_binding*)calloc(1,sizeof(ws_binding));
  if (w->target_ns)
    b->ns = strdup(w->target_ns);
  b->name = strdup(name);
  b->type = strdup(type);
  b->pt = pt;
  boptr = &b->operations;
  *bptr = b;

  c = n->children;
  wsdl_skip_others_strict(&c);

  if (check_element(c,"documentation",WSDL_NAMESPACE)) {
    /* skip */
    wsdl_next_element_strict(&c);
  }

  wsdl_skip_others(&c); /* this allows extensibility elements */

  while (c) {
    char *name = xmlGetProp(c,"name");
    ws_bindop *bo;
    ws_bindparam **fptr;
    xmlNodePtr c2;
    ws_operation *o;
    ws_operation *foundop = NULL;

    if (!check_element(c,"operation",WSDL_NAMESPACE))
      return invalid_element(c);

    if (NULL == name)
      return missing_attribute(c,"name");

    for (o = pt->operations; o && !foundop; o = o->next)
      if (!strcmp(o->name,name))
        foundop = o;

    if (!foundop)
      return parse_error(c,"No such operation \"%s\" on portType \"%s\"",name,b->type);

    bo = (ws_bindop*)calloc(1,sizeof(ws_bindop));
    bo->name = strdup(name);
    fptr = &bo->faults;
    *boptr = bo;
    boptr = &((*boptr)->next);

    c2 = c->children;
    wsdl_skip_others_strict(&c2);

    if (check_element(c2,"documentation",WSDL_NAMESPACE)) {
      /* skip */
      wsdl_next_element_strict(&c2);
    }

    wsdl_skip_others(&c2); /* this allows extensibility elements */

    if (check_element(c2,"input",WSDL_NAMESPACE)) {
      if (0 != wsdl_parse_bindparam(w,foundop,c2,doc,&bo->input,0))
        return -1;
      wsdl_next_element_strict(&c2);
    }

    if (check_element(c2,"output",WSDL_NAMESPACE)) {
      if (0 != wsdl_parse_bindparam(w,foundop,c2,doc,&bo->output,0))
        return -1;
      wsdl_next_element_strict(&c2);
    }

    while (c2) {

      if (!check_element(c2,"fault",WSDL_NAMESPACE))
        return invalid_element(c2);

      if (0 != wsdl_parse_bindparam(w,foundop,c2,doc,fptr,1))
        return -1;
      fptr = &((*fptr)->next);

      wsdl_next_element_strict(&c2);
    }

    wsdl_next_element(&c);
  }

  return 0;
}

int wsdl_parse_service(wsdl *w, xmlNodePtr n, xmlDocPtr doc)
{
  char *name = xmlGetProp(n,"name");
  ws_service *s;
  ws_service **sptr = &w->services;
  ws_port **pptr;
  xmlNodePtr c;

  while (*sptr)
    sptr = &((*sptr)->next);

  if (NULL == name)
    return missing_attribute(n,"name");

  s = (ws_service*)calloc(1,sizeof(ws_service));
  s->name = strdup(name);
  pptr = &s->ports;
  *sptr = s;

  c = n->children;
  wsdl_skip_others_strict(&c);

  if (check_element(c,"documentation",WSDL_NAMESPACE)) {
    /* skip */
    wsdl_next_element_strict(&c);
  }

  while (c && check_element(c,"port",WSDL_NAMESPACE)) {
    char *name = xmlGetProp(c,"name");
    char *binding = xmlGetProp(c,"binding");
    ws_port *p;
    xmlNodePtr c2;
    ws_binding *b;

    if (NULL == name)
      return missing_attribute(c,"name");
    if (NULL == binding)
      return missing_attribute(c,"binding");
    if (0 != wsdl_get_binding(w,binding,n,doc,&b))
      return -1;

    p = (ws_port*)calloc(1,sizeof(ws_port));
    p->name = strdup(name);
    p->binding = strdup(binding);
    p->b = b;
    *pptr = p;
    pptr = &((*pptr)->next);

    c2 = c->children;

    /* FIXME: process soap binding info here */

    wsdl_skip_others(&c2); /* this allows extensibility elements */
    if (c2)
      return invalid_element(c);

    wsdl_next_element_strict(&c);
  }

  wsdl_skip_others(&c); /* this allows extensibility elements */

  if (c)
    return invalid_element(c);

  return 0;
}

int wsdl_parse_definitions(wsdl *w, xmlNodePtr n, xmlDocPtr doc)
{
  xmlNodePtr c;
  char *name = xmlGetProp(n,"name");
  char *target_ns = xmlGetProp(n,"targetNamespace");

  if (NULL != name)
    w->name = strdup(name);
  if (NULL != target_ns)
    w->target_ns = strdup(target_ns);

  c = n->children;

  wsdl_skip_others_strict(&c);

  /* <import>* */
  while (c && check_element(c,"import",WSDL_NAMESPACE)) {
    /* FIXME */
    wsdl_next_element_strict(&c);
  }

  /* <documentation>? */
  if (c && check_element(c,"documentation",WSDL_NAMESPACE)) {
    /* skip */
    wsdl_next_element_strict(&c);
  }

  /* <types>? */
  if (c && check_element(c,"types",WSDL_NAMESPACE)) {
    /* FIXME */
    wsdl_next_element_strict(&c);
  }

  /* <message>* */
  while (c && check_element(c,"message",WSDL_NAMESPACE)) {
    if (0 != wsdl_parse_message(w,c,doc))
      return -1;
    wsdl_next_element_strict(&c);
  }

  /* FIXME: check that message names are unique */

  /* <portType>* */
  while (c && check_element(c,"portType",WSDL_NAMESPACE)) {
    if (0 != wsdl_parse_port_type(w,c,doc))
      return -1;
    wsdl_next_element_strict(&c);
  }

  /* FIXME: check that port type names are unique */

  /* <binding>* */
  while (c && check_element(c,"binding",WSDL_NAMESPACE)) {
    if (0 != wsdl_parse_binding(w,c,doc))
      return -1;
    wsdl_next_element_strict(&c);
  }

  /* FIXME: check that binding names are unique */

  /* <service>* */
  while (c && check_element(c,"service",WSDL_NAMESPACE)) {
    if (0 != wsdl_parse_service(w,c,doc))
      return -1;
    wsdl_next_element_strict(&c);
  }

  /* FIXME: check that service names are unique */

  wsdl_skip_others(&c); /* this allows extensibility elements */

  if (c)
    return invalid_element(c);

  return 0;
}

wsdl *parse_wsdl(FILE *f)
{
  xmlDocPtr doc;
  xmlNodePtr root;
  wsdl *w;

  if (NULL == (doc = xmlReadFd(fileno(f),NULL,NULL,0))) {
    fprintf(stderr,"XML parse error.\n");
    return NULL;
  }

  if (NULL == (root = xmlDocGetRootElement(doc))) {
    fprintf(stderr,"No root element.\n");
    xmlFreeDoc(doc);
    return NULL;
  }

  if (!check_element(root,"definitions",WSDL_NAMESPACE)) {
    printf("Invalid element at root: %s\n",root->name);
    xmlFreeDoc(doc);
    return NULL;
  }

  w = (wsdl*)calloc(1,sizeof(wsdl));

  if (0 != wsdl_parse_definitions(w,root,doc)) {
    xmlFreeDoc(doc);
    return NULL;
  }

  xmlFreeDoc(doc);

  return w;
}
