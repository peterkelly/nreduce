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

#include "syntax_bpel.h"
#include "xslt/parse.h"
#include "util/namespace.h"
#include <libxml/xmlwriter.h>
#include <libxml/xmlIO.h>
#include <libxml/parser.h>
#include <libxml/tree.h>
#include <stdio.h>
#include <string.h>
#include <assert.h>

bp_snode *bp_snode_new(int type)
{
  bp_snode *sn = (bp_snode*)calloc(1,sizeof(bp_snode));
  sn->type = type;
  return sn;
}

void bpel_output_activities(xmlTextWriter *writer, bp_activity *a)
{
  for (; a; a = a->next) {
    switch (a->type) {
    case BPEL_ACTIVITY_INVALID:
/*       assert(0); */
      /* Skip for now. FIXME: enable this when parsing of all activity types is implemented */
      break;
    case BPEL_ACTIVITY_RECEIVE:
      /* FIXME */
      break;
    case BPEL_ACTIVITY_REPLY:
      /* FIXME */
      break;
    case BPEL_ACTIVITY_INVOKE:
      /* FIXME */
      break;
    case BPEL_ACTIVITY_ASSIGN:
      /* FIXME */
      break;
    case BPEL_ACTIVITY_THROW:
      /* FIXME */
      break;
    case BPEL_ACTIVITY_TERMINATE:
      /* FIXME */
      break;
    case BPEL_ACTIVITY_WAIT:
      /* FIXME */
      break;
    case BPEL_ACTIVITY_EMPTY:
      xmlTextWriterStartElement(writer,"empty");
      xmlTextWriterEndElement(writer);
      break;
    case BPEL_ACTIVITY_SEQUENCE:
      /* FIXME */
      break;
    case BPEL_ACTIVITY_SWITCH:
      /* FIXME */
      break;
    case BPEL_ACTIVITY_WHILE:
      /* FIXME */
      break;
    case BPEL_ACTIVITY_PICK:
      /* FIXME */
      break;
    case BPEL_ACTIVITY_FLOW:
      /* FIXME */
      break;
    case BPEL_ACTIVITY_SCOPE:
      /* FIXME */
      break;
    case BPEL_ACTIVITY_COMPENSATE:
      /* FIXME */
      break;
    default:
      assert(0);
      break;
    }
  }
}





void bpel_output_scope_contents(xmlTextWriter *writer, bp_scope *scope)
{
  if (scope->variables) {
    bp_variable *v;
    xmlTextWriterStartElement(writer,"variables");
    for (v = scope->variables; v; v = v->next) {
      xmlTextWriterStartElement(writer,"variable");
      xmlTextWriterWriteAttribute(writer,"name",v->name);
      if (v->message_type.localpart)
        qname_attr(writer,"messageType",v->message_type);
      if (v->type.localpart)
        qname_attr(writer,"type",v->type);
      if (v->element.localpart)
        qname_attr(writer,"element",v->element);
      xmlTextWriterEndElement(writer);
    }
    xmlTextWriterEndElement(writer);
  }

  if (scope->correlation_sets) {
    bp_correlation_set *cs;
    xmlTextWriterStartElement(writer,"correlationSets");
    for (cs = scope->correlation_sets; cs; cs = cs->next) {
      xmlTextWriterStartElement(writer,"correlationSet");
      xmlTextWriterWriteAttribute(writer,"name",cs->name);
      qname_list_attr(writer,"properties",cs->properties);
      xmlTextWriterEndElement(writer);
    }
    xmlTextWriterEndElement(writer);
  }

  if (scope->fh_catches || scope->fh_catchall) {
    bp_catch *catch;

    xmlTextWriterStartElement(writer,"faultHandlers");

    for (catch = scope->fh_catches; catch; catch = catch->next) {
      xmlTextWriterStartElement(writer,"catch");
      if (catch->fault_name.localpart)
        qname_attr(writer,"faultName",catch->fault_name);
      if (catch->fault_variable)
        xmlTextWriterWriteAttribute(writer,"faultVariable",catch->fault_variable);
      bpel_output_activities(writer,catch->activities);
      xmlTextWriterEndElement(writer);
    }

    if (scope->fh_catchall) {
      xmlTextWriterStartElement(writer,"catch");
      bpel_output_activities(writer,scope->fh_catchall);
      xmlTextWriterEndElement(writer);
    }

    xmlTextWriterEndElement(writer);
  }

  if (scope->compensation_handler) {
    xmlTextWriterStartElement(writer,"compensationHandler");
    bpel_output_activities(writer,scope->compensation_handler);
    xmlTextWriterEndElement(writer);
  }

  if (scope->eh_on_message || scope->eh_on_alarm) {
    bp_on_message *om;
    bp_on_alarm *oa;

    xmlTextWriterStartElement(writer,"eventHandlers");

    for (om = scope->eh_on_message; om; om = om->next) {
      xmlTextWriterStartElement(writer,"onMessage");
      xmlTextWriterWriteAttribute(writer,"partnerLink",om->partner_link);
      qname_attr(writer,"portType",om->port_type);
      xmlTextWriterWriteAttribute(writer,"operation",om->operation);
      if (om->variable)
        xmlTextWriterWriteAttribute(writer,"variable",om->variable);

      if (om->correlations) {
        bp_correlation *cor;
        xmlTextWriterStartElement(writer,"correlations");
        for (cor = om->correlations; cor; cor = cor->next) {
          xmlTextWriterStartElement(writer,"correlation");
          xmlTextWriterWriteAttribute(writer,"set",cor->set);
          if (cor->initiate)
            xmlTextWriterWriteAttribute(writer,"initiate","yes");
          else
            xmlTextWriterWriteAttribute(writer,"initiate","no");
          xmlTextWriterEndElement(writer);
        }
        xmlTextWriterEndElement(writer);
      }

      /* FIXME: write correlations */

      bpel_output_activities(writer,om->activities);
      xmlTextWriterEndElement(writer);
    }

    for (oa = scope->eh_on_alarm; oa; oa = oa->next) {
      xmlTextWriterStartElement(writer,"onAlarm");
      if (oa->dfor)
        xmlTextWriterWriteAttribute(writer,"for",oa->dfor);
      if (oa->duntil)
        xmlTextWriterWriteAttribute(writer,"until",oa->duntil);
      bpel_output_activities(writer,oa->activities);
      xmlTextWriterEndElement(writer);
    }

    xmlTextWriterEndElement(writer);
  }

  bpel_output_activities(writer,scope->activities);
}

void output_bpel(FILE *f, bp_scope *scope)
{
  xmlOutputBuffer *buf = xmlOutputBufferCreateFile(f,NULL);
  xmlTextWriter *writer = xmlNewTextWriter(buf);

  xmlTextWriterSetIndent(writer,1);
  xmlTextWriterSetIndentString(writer,"  ");
  xmlTextWriterStartDocument(writer,NULL,NULL,NULL);
  xmlTextWriterStartElement(writer,"process");
  xmlTextWriterWriteAttribute(writer,"xmlns",BPEL_NAMESPACE);

  if (scope->partner_links) {
    bp_partner_link *pl;
    xmlTextWriterStartElement(writer,"partnerLinks");
    for (pl = scope->partner_links; pl; pl = pl->next) {
      xmlTextWriterStartElement(writer,"partnerLink");
      xmlTextWriterWriteAttribute(writer,"name",pl->name);
      qname_attr(writer,"partnerLinkType",pl->partner_link_type);
      if (pl->my_role)
        xmlTextWriterWriteAttribute(writer,"myRole",pl->my_role);
      if (pl->partner_role)
        xmlTextWriterWriteAttribute(writer,"partnerRole",pl->partner_role);
      xmlTextWriterEndElement(writer);
    }
    xmlTextWriterEndElement(writer);
  }

  if (scope->partners) {
    bp_partner *p;
    xmlTextWriterStartElement(writer,"partners");
    for (p = scope->partners; p; p = p->next) {
      bp_partner_partner_link *pl;
      xmlTextWriterStartElement(writer,"partner");
      for (pl = p->partner; pl; pl = pl ->next) {
        xmlTextWriterStartElement(writer,"partnerLink");
        xmlTextWriterWriteAttribute(writer,"name",pl->name);
        xmlTextWriterEndElement(writer);
      }
      xmlTextWriterEndElement(writer);
    }
    xmlTextWriterEndElement(writer);
  }

  bpel_output_scope_contents(writer,scope);

  xmlTextWriterEndElement(writer);
  xmlTextWriterEndDocument(writer);
  xmlTextWriterFlush(writer);
  xmlFreeTextWriter(writer);
}























int bpel_is_activity(xmlNodePtr c)
{
  return (check_element(c,"receive",BPEL_NAMESPACE) ||
          check_element(c,"reply",BPEL_NAMESPACE) ||
          check_element(c,"invoke",BPEL_NAMESPACE) ||
          check_element(c,"assign",BPEL_NAMESPACE) ||
          check_element(c,"throw",BPEL_NAMESPACE) ||
          check_element(c,"terminate",BPEL_NAMESPACE) ||
          check_element(c,"wait",BPEL_NAMESPACE) ||
          check_element(c,"empty",BPEL_NAMESPACE) ||
          check_element(c,"sequence",BPEL_NAMESPACE) ||
          check_element(c,"switch",BPEL_NAMESPACE) ||
          check_element(c,"while",BPEL_NAMESPACE) ||
          check_element(c,"pick",BPEL_NAMESPACE) ||
          check_element(c,"flow",BPEL_NAMESPACE) ||
          check_element(c,"scope",BPEL_NAMESPACE) ||
          check_element(c,"compensate",BPEL_NAMESPACE));
}

void bpel_skip_others(xmlNodePtr *c)
{
  while ((*c) &&
         ((XML_ELEMENT_NODE != (*c)->type) ||
           (NULL == (*c)->ns) ||
           strcmp((*c)->ns->href,BPEL_NAMESPACE))) {
    (*c) = (*c)->next;
  }
}

int bpel_parse_standard_attributes(bp_activity *a, xmlNodePtr n)
{
  return 0;
}

int bpel_parse_standard_elements(bp_activity *a, xmlNodePtr *c)
{
  return 0;
}

int bpel_parse_activities(bp_activity **list, xmlNodePtr c)
{
  xmlNodePtr c2;
  bp_activity *a;
  bp_activity **aptr = list;

  for (; c; c = c->next) {

    bpel_skip_others(&c);
    if (!c)
      break;

    if (!bpel_is_activity(c))
      return invalid_element(c);

    a = (bp_activity*)calloc(1,sizeof(bp_activity));
    a->type = BPEL_ACTIVITY_INVALID;
    *aptr = a;
    aptr = &a->next;

    if (0 != bpel_parse_standard_attributes(a,c))
      return -1;

    c2 = c->children;

    if (0 != bpel_parse_standard_elements(a,&c2))
      return -1;

    bpel_skip_others(&c2);

    if (check_element(c,"receive",BPEL_NAMESPACE)) {
      /* FIXME */
    }
    else if (check_element(c,"reply",BPEL_NAMESPACE)) {
      /* FIXME */
    }
    else if (check_element(c,"invoke",BPEL_NAMESPACE)) {
      /* FIXME */
    }
    else if (check_element(c,"assign",BPEL_NAMESPACE)) {
      /* FIXME */
    }
    else if (check_element(c,"throw",BPEL_NAMESPACE)) {
      /* FIXME */
    }
    else if (check_element(c,"terminate",BPEL_NAMESPACE)) {
      /* FIXME */
    }
    else if (check_element(c,"wait",BPEL_NAMESPACE)) {
      /* FIXME */
    }
    else if (check_element(c,"empty",BPEL_NAMESPACE)) {
      a->type = BPEL_ACTIVITY_EMPTY;
      if (c2)
        return invalid_element(c2);
    }
    else if (check_element(c,"sequence",BPEL_NAMESPACE)) {
      /* FIXME */
    }
    else if (check_element(c,"switch",BPEL_NAMESPACE)) {
      /* FIXME */
    }
    else if (check_element(c,"while",BPEL_NAMESPACE)) {
      /* FIXME */
    }
    else if (check_element(c,"pick",BPEL_NAMESPACE)) {
      /* FIXME */
    }
    else if (check_element(c,"flow",BPEL_NAMESPACE)) {
      /* FIXME */
    }
    else if (check_element(c,"scope",BPEL_NAMESPACE)) {
      /* FIXME */
    }
    else if (check_element(c,"compensate",BPEL_NAMESPACE)) {
      /* FIXME */
    }

    /* FIXME: allow <compensate> in the case where we are inside a <catchAll> */
  }

  return 0;
}




int bpel_parse_partner_links(bp_scope *scope, xmlNodePtr n)
{
  int count = 0;
  bp_partner_link *pl;
  bp_partner_link **plptr = &scope->partner_links;
  xmlNodePtr c;

  c = n->children;
  while (c) {
    char *name;
    char *partner_link_type;
    char *my_role;
    char *partner_role;

    bpel_skip_others(&c);
    if (!c)
      break;

    if (!check_element(c,"partnerLink",BPEL_NAMESPACE))
      return invalid_element(c);

    name = xmlGetProp(c,"name");
    partner_link_type = xmlGetProp(c,"partnerLinkType");
    my_role = xmlGetProp(c,"myRole");
    partner_role = xmlGetProp(c,"partnerRole");

    if (NULL == name)
      return missing_attribute(c,"name");
    if (NULL == partner_link_type)
      return missing_attribute(c,"partnerLinkType");

    pl = (bp_partner_link*)calloc(1,sizeof(bp_partner_link));
    pl->name = strdup(name);
    pl->partner_link_type = get_qname(partner_link_type);
    if (my_role)
      pl->my_role = strdup(my_role);
    if (partner_role)
      pl->partner_role = strdup(partner_role);
    pl->next = NULL;

    *plptr = pl;
    plptr = &pl->next;
    count++;
    c = c->next;
  }

  if (0 == count)
    return parse_error(n,"At least one <partnerLink> must be present inside a "
                         "<partnerLinks> element");

  return 0;
}

int bpel_parse_partners(bp_scope *scope, xmlNodePtr n)
{
  int pcount = 0;
  bp_partner *p;
  bp_partner_partner_link **pplptr;
  bp_partner **pptr = &scope->partners;
  xmlNodePtr c;

  printf("bpel_parse_partners\n");

  /* iterate over <partner> children */
  c = n->children;
  while (c) {
    int plcount = 0;
    xmlNodePtr c2;

    bpel_skip_others(&c);
    if (!c)
      break;

    if (!check_element(c,"partner",BPEL_NAMESPACE))
      return invalid_element(c);

    /* add a new bp_partner object to the scope */
    p = (bp_partner*)calloc(1,sizeof(bp_partner));
    *pptr = p;
    pptr = &p->next;

    pplptr = &p->partner;

    /* iterate over <partnerLink> children */
    c2 = c->children;
    while (c2) {
      bp_partner_partner_link *ppl;
      char *name;

      bpel_skip_others(&c2);
      if (!c2)
        break;

      if (!check_element(c2,"partnerLink",BPEL_NAMESPACE))
        return invalid_element(c2);

      name = xmlGetProp(c2,"name");
      if (NULL == name)
        return missing_attribute(c2,"name");

      /* add a new bp_partner_partner_link object to the partner */
      ppl = (bp_partner_partner_link*)calloc(1,sizeof(bp_partner_partner_link));
      ppl->name = strdup(name);
      *pplptr = ppl;
      pplptr = &ppl->next;

      plcount++;
      c2 = c2->next;
    }

    if (0 == plcount)
      return parse_error(c,"At least one <partnerLink> must be present inside a "
                            "<partner> element");

    pcount++;
    c = c->next;
  }

  if (0 == pcount)
    return parse_error(n,"At least one <partner> must be present inside a "
                         "<partners> element");

  return 0;
}

int bpel_parse_variables(bp_scope *scope, xmlNodePtr n)
{
  int count = 0;
  bp_variable *v;
  bp_variable **vptr = &scope->variables;
  xmlNodePtr c;

  c = n->children;
  while (c) {
    char *name;
    char *message_type;
    char *type;
    char *element;

    bpel_skip_others(&c);
    if (!c)
      break;

    if (!check_element(c,"variable",BPEL_NAMESPACE))
      return invalid_element(c);

    name = xmlGetProp(c,"name");
    message_type = xmlGetProp(c,"messageType");
    type = xmlGetProp(c,"type");
    element = xmlGetProp(c,"element");

    if (NULL == name)
      return missing_attribute(c,"name");

    v = (bp_variable*)calloc(1,sizeof(bp_variable));
    v->name = strdup(name);
    if (message_type)
      v->message_type = get_qname(message_type);
    if (type)
      v->type = get_qname(type);
    if (element)
      v->element = get_qname(element);

    *vptr = v;
    vptr = &v->next;
    count++;
    c = c->next;
  }

  if (0 == count)
    return parse_error(n,"At least one <variable> must be present inside a "
                         "<variables> element");

  return 0;
}

int bpel_parse_correlation_sets(bp_scope *scope, xmlNodePtr n)
{
  int count = 0;
  bp_correlation_set *cs;
  bp_correlation_set **csptr = &scope->correlation_sets;
  xmlNodePtr c;

  c = n->children;
  while (c) {
    char *name;
    char *properties;

    bpel_skip_others(&c);
    if (!c)
      break;

    if (!check_element(c,"correlationSet",BPEL_NAMESPACE))
      return invalid_element(c);

    name = xmlGetProp(c,"name");
    properties = xmlGetProp(c,"properties");

    if (NULL == name)
      return missing_attribute(c,"name");
    if (NULL == properties)
      return missing_attribute(c,"properties");

    cs = (bp_correlation_set*)calloc(1,sizeof(bp_correlation_set));
    cs->name = strdup(name);
    cs->properties = get_qname_list(properties);

    *csptr = cs;
    csptr = &cs->next;
    count++;
    c = c->next;
  }

  if (0 == count)
    return parse_error(n,"At least one <correlationSet> must be present inside a "
                         "<correlationSets> element");

  return 0;
}

int bpel_parse_fault_handlers(bp_scope *scope, xmlNodePtr n)
{
  bp_catch *catch;
  bp_catch **catchptr = &scope->fh_catches;
  xmlNodePtr c;
  int found_catchall = 0;

  c = n->children;
  while (c) {
    char *fault_name;
    char *fault_variable;

    bpel_skip_others(&c);
    if (!c)
      break;

    if (check_element(c,"catchall",BPEL_NAMESPACE)) {
      if (found_catchall)
        return parse_error(c,"only one <catchall> element allowed\n");

      /* FIXME: parse child elements (activities) */

      found_catchall = 1;
    }
    else if (check_element(c,"catch",BPEL_NAMESPACE)) {

      if (found_catchall)
        return parse_error(c,"<catch> elements cannot appear after the <catchall> element\n");

      fault_name = xmlGetProp(c,"faultName");
      fault_variable = xmlGetProp(c,"faultVariable");

      catch = (bp_catch*)calloc(1,sizeof(bp_catch));
      if (fault_name)
        catch->fault_name = get_qname(fault_name);
      if (fault_variable)
        catch->fault_variable = strdup(fault_variable);
      *catchptr = catch;
      catchptr = &catch->next;

      /* FIXME: parse child elements (activities) */

    }
    else {
      return invalid_element(c);
    }

    c = c->next;
  }

  return 0;
}

int bpel_parse_compensation_handler(bp_scope *scope, xmlNodePtr n)
{
  if (0 != bpel_parse_activities(&scope->compensation_handler,n->children))
    return -1;

  return 0;
}

int bpel_parse_event_handlers(bp_scope *scope, xmlNodePtr n)
{
  int count = 0;
  bp_on_message *om;
  bp_on_alarm *oa;
  bp_on_message **omptr = &scope->eh_on_message;
  bp_on_alarm **oaptr = &scope->eh_on_alarm;
  xmlNodePtr c;
  int found_on_alarm = 0;

  c = n->children;
  while (c) {

    bpel_skip_others(&c);
    if (!c)
      break;

    if (check_element(c,"onMessage",BPEL_NAMESPACE)) {
      char *partner_link = xmlGetProp(c,"partnerLink");
      char *port_type = xmlGetProp(c,"portType");
      char *operation = xmlGetProp(c,"operation");
      char *variable = xmlGetProp(c,"variable");
      bp_correlation **corptr;
      xmlNodePtr c2;

      if (found_on_alarm)
        return parse_error(c,"<onMessage> elements cannot appear after <onAlarm> elements\n");

      if (NULL == partner_link)
        return missing_attribute(c,"partnerLink");
      if (NULL == port_type)
        return missing_attribute(c,"portType");
      if (NULL == operation)
        return missing_attribute(c,"operation");

      om = (bp_on_message*)calloc(1,sizeof(bp_on_message));
      om->partner_link = strdup(partner_link);
      om->port_type = get_qname(port_type);
      om->operation = strdup(operation);
      if (variable)
        om->variable = strdup(variable);
      *omptr = om;
      omptr = &om->next;
      corptr = &om->correlations;

      /* <correlations> child */
      c2 = c->children;
      bpel_skip_others(&c2);
      if (c2 && check_element(c2,"correlations",BPEL_NAMESPACE)) {
        xmlNodePtr c3 = c2->children;
        bp_correlation *cor;
        char *set;
        char *initiate;

        bpel_skip_others(&c3);
        if (!c3)
          break;

        if (!check_element(c3,"correlation",BPEL_NAMESPACE))
          return invalid_element(c3);

        set = xmlGetProp(c3,"set");
        initiate = xmlGetProp(c3,"initiate");

        if (NULL == set)
          return missing_attribute(c3,"set");
        if (initiate && strcmp(initiate,"yes") && strcmp(initiate,"no"))
          return parse_error(c3,"\"initiate\" attribute, if specified, must be \"yes\" or \"no\"");

        cor = (bp_correlation*)calloc(1,sizeof(bp_correlation));
        cor->set = strdup(set);
        if (initiate && !strcmp(initiate,"yes"))
          cor->initiate = 1;
        *corptr = cor;
        corptr = &cor->next;

        c2 = c2->next;
      }

      /* activity children */
      if (0 != bpel_parse_activities(&om->activities,c2))
        return -1;

    }
    else if (check_element(c,"onAlarm",BPEL_NAMESPACE)) {
      char *dfor = xmlGetProp(c,"for");
      char *duntil = xmlGetProp(c,"until");

      oa = (bp_on_alarm*)calloc(1,sizeof(bp_on_alarm));
      if (dfor)
        oa->dfor = strdup(dfor);
      if (duntil)
        oa->duntil = strdup(duntil);

      /* activity children */
      if (0 != bpel_parse_activities(&oa->activities,c->children))
        return -1;

      *oaptr = oa;
      oaptr = &oa->next;
      found_on_alarm = 1;
    }
    else {
      return invalid_element(c);
    }

    count++;
    c = c->next;
  }

  if (0 == count)
    return parse_error(n,"At least one <onMessage> or <onAlarm> must be present inside "
                         "<eventHandlers>");

  return 0;
}





























int bpel_parse_scope(bp_scope *scope, xmlNodePtr n)
{
  xmlNodePtr c;
  int state = 0;

  /* state 0: expecting <partnerLinks> or later - *process* elements only */
  /* state 1: expecting <partners> or later - *process* elements only  */

  /* state 2: expecting <variables> or later */
  /* state 3: expecting <correlationSets> or later */
  /* state 4: expecting <faultHandlers> or later */
  /* state 5: expecting <compensationHandlers> or later */
  /* state 6: expecting <eventHandlers> or later */
  /* state 7: expecting activities */

  c = n->children;
  while (c) {

    bpel_skip_others(&c);
    if (!c)
      break;

    if ((0 == state) && check_element(c,"partnerLinks",BPEL_NAMESPACE)) {
      if (0 != bpel_parse_partner_links(scope,c))
        return -1;
      state = 1;
      c = c->next;
    }
    else if ((1 >= state) && check_element(c,"partners",BPEL_NAMESPACE)) {
      if (0 != bpel_parse_partners(scope,c))
        return -1;
      state = 2;
      c = c->next;
    }
    else if ((2 >= state) && check_element(c,"variables",BPEL_NAMESPACE)) {
      if (0 != bpel_parse_variables(scope,c))
        return -1;
      state = 3;
      c = c->next;
    }
    else if ((3 >= state) && check_element(c,"correlationSets",BPEL_NAMESPACE)) {
      if (0 != bpel_parse_correlation_sets(scope,c))
        return -1;
      state = 4;
      c = c->next;
    }
    else if ((4 >= state) && check_element(c,"faultHandlers",BPEL_NAMESPACE)) {
      if (0 != bpel_parse_fault_handlers(scope,c))
        return -1;
      state = 5;
      c = c->next;
    }
    else if ((5 >= state) && check_element(c,"compensationHandler",BPEL_NAMESPACE)) {
      if (0 != bpel_parse_compensation_handler(scope,c))
        return -1;
      state = 6;
      c = c->next;
    }
    else if ((6 >= state) && check_element(c,"eventHandlers",BPEL_NAMESPACE)) {
      if (0 != bpel_parse_event_handlers(scope,c))
        return -1;
      state = 7;
      c = c->next;
    }
    else if ((7 >= state) && bpel_is_activity(c)) {
      return bpel_parse_activities(&scope->activities,c);
    }
    else {
      return invalid_element(c);
    }
    c = c->next;
  }

  return 0;
}

bp_scope *parse_bpel(FILE *f)
{
  xmlDocPtr doc;
  xmlNodePtr root;
  bp_scope *broot;

  if (NULL == (doc = xmlReadFd(fileno(f),NULL,NULL,0))) {
    fprintf(stderr,"XML parse error.\n");
    return NULL;
  }

  if (NULL == (root = xmlDocGetRootElement(doc))) {
    fprintf(stderr,"No root element.\n");
    xmlFreeDoc(doc);
    return NULL;
  }

  if (!check_element(root,"process",BPEL_NAMESPACE)) {
    printf("Invalid element at root: %s\n",root->name);
    xmlFreeDoc(doc);
    return NULL;
  }

  broot = (bp_scope*)calloc(1,sizeof(bp_scope));

  if (0 != bpel_parse_scope(broot,root)) {
    xmlFreeDoc(doc);
    return NULL;
  }

  xmlFreeDoc(doc);

  return broot;
}

void bp_scope_free(bp_scope *scope)
{
  /* FIXME */
}
