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

#include "namespace.h"
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
  list_free(map->defs,(void*)ns_def_free);
  free(map);
}

void ns_add(ns_map *map, const char *href, const char *preferred_prefix)
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

nsname_t qname_to_nsname(ns_map *map, qname_t qn)
{
  nsname_t nn;
  /* FIXME */
  return nn;
}

qname_t nsname_to_qname(ns_map *map, nsname_t nn)
{
  qname_t qn;
  /* FIXME */
  return qn;
}

