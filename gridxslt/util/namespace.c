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

#include "namespace.h"
#include "debug.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

void ns_def_free(ns_def *def)
{
  free(def->prefix);
  free(def->href);
  free(def);
}

ns_map *ns_map_new()
{
  ns_map *map = (ns_map*)calloc(1,sizeof(ns_map));
  return map;
}

void ns_map_free(ns_map *map, int free_parents)
{
  if (free_parents)
    ns_map_free(map->parent,1);
  list_free(map->defs,(list_d_t)ns_def_free);
  free(map);
}

void ns_add_preferred(ns_map *map, const char *href, const char *preferred_prefix)
{
  ns_def *ns;
  char *prefix;
  int number = 1;

  if (NULL != ns_lookup_href(map,href))
    return;

  if (NULL != preferred_prefix) {
    prefix = (char*)malloc(strlen(preferred_prefix)+100);
    sprintf(prefix,"%s",preferred_prefix);
  }
  else {
    preferred_prefix = "ns";
    prefix = (char*)malloc(100);
    sprintf(prefix,"ns0");
  }
  while (NULL != ns_lookup_prefix(map,prefix))
    sprintf(prefix,"%s%d",preferred_prefix,number++);
  ns = (ns_def*)calloc(1,sizeof(ns_def));
  ns->prefix = prefix;
  ns->href = strdup(href);
  list_append(&map->defs,ns);
}

void ns_add_direct(ns_map *map, const char *href, const char *prefix)
{
  ns_def *ns = (ns_def*)calloc(1,sizeof(ns_def));
  ns->prefix = strdup(prefix);
  ns->href = strdup(href);
  list_append(&map->defs,ns);
}

ns_def *ns_lookup_prefix(ns_map *map, const char *prefix)
{
  list *l;
  for (l = map->defs; l; l = l->next) {
    ns_def *ns = (ns_def*)l->data;
    if (!strcmp(ns->prefix,prefix))
      return ns;
  }
  if (NULL != map->parent)
    return ns_lookup_prefix(map->parent,prefix);
  else
    return NULL;
}

ns_def *ns_lookup_href(ns_map *map, const char *href)
{
  list *l;
  for (l = map->defs; l; l = l->next) {
    ns_def *ns = (ns_def*)l->data;
    if (!strcmp(ns->href,href))
      return ns;
  }
  if (NULL != map->parent)
    return ns_lookup_href(map->parent,href);
  else
    return NULL;
}

nsname qname_to_nsname(ns_map *map, const qname qn)
{
  nsname nn;
  ns_def *def;
  nn.ns = NULL;
  nn.name = NULL;
  if (NULL != qn.prefix) {
    if (NULL == (def = ns_lookup_prefix(map,qn.prefix)))
      return nn;
    nn.ns = strdup(def->href);
  }
  nn.name = strdup(qn.localpart);
  return nn;
}

qname nsname_to_qname(ns_map *map, const nsname nn)
{
  qname qn;
  ns_def *def;
  qn.prefix = NULL;
  qn.localpart = NULL;
  if (NULL != nn.ns) {
    if (NULL == (def = ns_lookup_href(map,nn.ns)))
      return qn;
    qn.prefix = strdup(def->prefix);
  }
  qn.localpart = strdup(nn.name);
  return qn;
}

nsnametest *qnametest_to_nsnametest(ns_map *map, const qnametest *qt)
{
  nsnametest *nt = (nsnametest*)calloc(1,sizeof(nsnametest));
  nt->wcns = qt->wcprefix;
  nt->wcname = qt->wcname;
  if (NULL != qt->qn.prefix) {
    ns_def *def;
    if (NULL == (def = ns_lookup_prefix(map,qt->qn.prefix)))
      return NULL;
    nt->nn.ns = strdup(def->href);
  }
  if (NULL != qt->qn.localpart)
    nt->nn.name = strdup(qt->qn.localpart);
  return nt;
}

