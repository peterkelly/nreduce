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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <libxml/xmlreader.h>

#define HASH_SIZE 16

using namespace GridXSLT;

typedef struct attr_indexes attr_indexes;
typedef struct attr_index attr_index;
typedef struct attrvalue_index attrvalue_index;

struct attr_indexes {
  int count;
  struct {
    int aid;
    int pos; /* *attr_index */
  } *indexes;
};

struct attr_index {
  int count;
  struct {
    int vid;
    int pos; /* *attrvalue_index */
  } *avindexes;
};

struct attrvalue_index {
  int count;
  int *eids;
  char *value;
};


int lookup_attr(char *attr)
{
  /* returns: aid */
  return 0;
}

int lookup_attrvalue(char *attr, char *value, int **result)
{
  /* returns [eid] - all element ids having attr=value */
/*   int aid = lookup_attr(attr); */
  return 0;
}










int hash(const char *ns, const char *name)
{
  int h = 0;
  const char *c = ns;
  if (c)
    for (; *c; c++)
      h += (int)*c;
  c = name;
  if (c)
    for (; *c; c++)
      h += (int)*c;
  return h % HASH_SIZE;
}

typedef struct emapping emapping;
typedef struct xmldb xmldb;
typedef uint eid_t;

struct emapping {
  char *ns;
  char *name;
  eid_t eid;
  emapping *next;
};





struct xmldb {
  emapping **emappings;
  eid_t next_eid;
};

eid_t xmldb_lookup_emapping(xmldb *db, const char *ns, const char *name)
{
  int h = hash(ns,name);
  emapping **emptr = &(db->emappings[h]);
  while (*emptr) {
    if (((!(*emptr)->ns && !ns) || ((*emptr)->ns && ns && !strcmp((*emptr)->ns,ns))) &&
        !strcmp((*emptr)->name,name))
      return (*emptr)->eid;
    emptr = &((*emptr)->next);
  }
  (*emptr) = (emapping*)malloc(sizeof(emapping));
  (*emptr)->ns = ns ? strdup(ns) : NULL;
  (*emptr)->name = strdup(name);
  (*emptr)->eid = db->next_eid++;
  return (*emptr)->eid;
}

xmldb *xmldb_new()
{
  xmldb *db = (xmldb*)malloc(sizeof(xmldb));
  db->emappings = (emapping**)calloc(HASH_SIZE,sizeof(emapping*));
  db->next_eid = 0;
  return db;
}

void xmldb_free(xmldb *db)
{
  int h;
  for (h = 0; h < HASH_SIZE; h++) {
    emapping *em = db->emappings[h];
    while (em) {
      emapping *next = em->next;
      free(em->name);
      free(em);
      em = next;
    }
  }
  free(db);
}



int main(int argc, char **argv)
{
  char *infilename;
  char *outfilename;
/*   FILE *in; */
/*   FILE *out; */
  xmlTextReaderPtr reader;
  int r;
  xmldb *db = xmldb_new();

  if (3 > argc) {
    fmessage(stderr,"Usage: create <infile> <outfile>\n");
    exit(1);
  }

  infilename = argv[1];
  outfilename = argv[2];

  if (NULL == (reader = xmlReaderForFile(infilename,NULL,0))) {
    perror(infilename);
    exit(1);
  }

  while (1 == (r = xmlTextReaderRead(reader))) {
    const xmlChar *name = xmlTextReaderConstName(reader);
    const xmlChar *value = xmlTextReaderConstValue(reader);
    int type = xmlTextReaderNodeType(reader);
    char *val = NULL;
    if (value) {
      char *c = val = strdup(value);
      while ('\0' != *c) {
        if ('\n' == *c)
          *c = '|';
        c++;
      }
    }
    if (XML_READER_TYPE_ELEMENT == type) {
      eid_t eid;
      message("depth %d, type %d, name \"%s\", empty %d, has value %d attrs %d, "
             "nattrs %d, value \"%s\"\n",
             xmlTextReaderDepth(reader),
             xmlTextReaderNodeType(reader),
             name,
             xmlTextReaderIsEmptyElement(reader),
             xmlTextReaderHasValue(reader),
             xmlTextReaderHasAttributes(reader),
             xmlTextReaderAttributeCount(reader),
             val);
      eid = xmldb_lookup_emapping(db,xmlTextReaderNamespaceUri(reader),
                                  xmlTextReaderConstName(reader));
      message("eid = %d\n",eid);
      if (1 == xmlTextReaderMoveToFirstAttribute(reader)) {
        while (1) {
          message("  %s=%s\n",xmlTextReaderConstName(reader),xmlTextReaderConstValue(reader));
          if (1 != xmlTextReaderMoveToNextAttribute(reader))
            break;
        }
      }
    }
    free(val);
  }
  xmlFreeTextReader(reader);
  xmldb_free(db);
  if (0 != r) {
    fmessage(stderr,"Parse error\n");
    exit(1);
  }

  return 0;
}
