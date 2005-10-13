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

#define _XMLSCHEMA_XMLSCHEMA_C

#include "xmlschema.h"
#include "parse.h"
#include "derivation.h"
#include "util/namespace.h"
#include "util/stringbuf.h"
#include "util/debug.h"
#include "util/macros.h"
#include <libxml/xmlwriter.h>
#include <libxml/xmlIO.h>
#include <libxml/parser.h>
#include <libxml/tree.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <ctype.h>
#include <errno.h>

const char *xs_facet_names[XS_FACET_NUMFACETS] = {
  "equal",
  "ordered",
  "bounded",
  "cardinality",
  "numeric",
  "length",
  "minLength",
  "maxLength",
  "pattern",
  "enumeration",
  "whiteSpace",
  "maxInclusive",
  "maxExclusive",
  "minExclusive",
  "minInclusive",
  "totalDigits",
  "fractionDigits"};

const char *xs_object_types[7] = {
  "type",
  "attribute",
  "element",
  "attribute group",
  "identity constraint",
  "group",
  "notation"
};

typedef struct xs_type_allows_facet xs_type_allows_facet;

struct xs_type_allows_facet {
  char *typename;
  int facets[XS_FACET_NUMFACETS+1];
};

/* A mapping of primitive type -> allowed facets, as specified by the table in section 4.1.5
   of spec part 2 (datatypes) */
xs_type_allows_facet xs_allowed_facets[21] = {
  {"anySimpleType",{ XS_FACET_LENGTH, XS_FACET_MINLENGTH, XS_FACET_MAXLENGTH, XS_FACET_PATTERN,
                     XS_FACET_ENUMERATION, XS_FACET_WHITESPACE, XS_FACET_MAXINCLUSIVE,
                     XS_FACET_MAXEXCLUSIVE, XS_FACET_MINEXCLUSIVE, XS_FACET_MININCLUSIVE,
                     XS_FACET_TOTALDIGITS, XS_FACET_FRACTIONDIGITS, -1 }},

  {"string",       { XS_FACET_LENGTH, XS_FACET_MINLENGTH, XS_FACET_MAXLENGTH, XS_FACET_PATTERN,
                     XS_FACET_ENUMERATION, XS_FACET_WHITESPACE, -1 }},

  {"boolean",      { XS_FACET_PATTERN, XS_FACET_WHITESPACE, -1 }},

  {"float",        { XS_FACET_PATTERN, XS_FACET_ENUMERATION, XS_FACET_WHITESPACE,
                     XS_FACET_MAXINCLUSIVE, XS_FACET_MAXEXCLUSIVE, XS_FACET_MININCLUSIVE,
                     XS_FACET_MINEXCLUSIVE, -1 }},

  {"double",       { XS_FACET_PATTERN, XS_FACET_ENUMERATION, XS_FACET_WHITESPACE,
                     XS_FACET_MAXINCLUSIVE, XS_FACET_MAXEXCLUSIVE, XS_FACET_MININCLUSIVE,
                     XS_FACET_MINEXCLUSIVE, -1 }},

  {"decimal",      { XS_FACET_TOTALDIGITS, XS_FACET_FRACTIONDIGITS, XS_FACET_PATTERN,
                     XS_FACET_WHITESPACE, XS_FACET_ENUMERATION, XS_FACET_MAXINCLUSIVE,
                     XS_FACET_MAXEXCLUSIVE, XS_FACET_MININCLUSIVE, XS_FACET_MINEXCLUSIVE, -1 }},

  {"duration",     { XS_FACET_PATTERN, XS_FACET_ENUMERATION, XS_FACET_WHITESPACE,
                     XS_FACET_MAXINCLUSIVE, XS_FACET_MAXEXCLUSIVE, XS_FACET_MININCLUSIVE,
                     XS_FACET_MINEXCLUSIVE, -1 }},

  {"dateTime",     { XS_FACET_PATTERN, XS_FACET_ENUMERATION, XS_FACET_WHITESPACE,
                     XS_FACET_MAXINCLUSIVE, XS_FACET_MAXEXCLUSIVE, XS_FACET_MININCLUSIVE,
                     XS_FACET_MINEXCLUSIVE, -1 }},

  {"time",         { XS_FACET_PATTERN, XS_FACET_ENUMERATION, XS_FACET_WHITESPACE,
                     XS_FACET_MAXINCLUSIVE, XS_FACET_MAXEXCLUSIVE, XS_FACET_MININCLUSIVE,
                     XS_FACET_MINEXCLUSIVE, -1 }},

  {"date",         { XS_FACET_PATTERN, XS_FACET_ENUMERATION, XS_FACET_WHITESPACE,
                     XS_FACET_MAXINCLUSIVE, XS_FACET_MAXEXCLUSIVE, XS_FACET_MININCLUSIVE,
                     XS_FACET_MINEXCLUSIVE, -1 }},

  {"gYearMonth",   { XS_FACET_PATTERN, XS_FACET_ENUMERATION, XS_FACET_WHITESPACE,
                     XS_FACET_MAXINCLUSIVE, XS_FACET_MAXEXCLUSIVE, XS_FACET_MININCLUSIVE,
                     XS_FACET_MINEXCLUSIVE, -1 }},

  {"gYear",        { XS_FACET_PATTERN, XS_FACET_ENUMERATION, XS_FACET_WHITESPACE,
                     XS_FACET_MAXINCLUSIVE, XS_FACET_MAXEXCLUSIVE, XS_FACET_MININCLUSIVE,
                     XS_FACET_MINEXCLUSIVE, -1 }},

  {"gMonthDay",    { XS_FACET_PATTERN, XS_FACET_ENUMERATION, XS_FACET_WHITESPACE,
                     XS_FACET_MAXINCLUSIVE, XS_FACET_MAXEXCLUSIVE, XS_FACET_MININCLUSIVE,
                     XS_FACET_MINEXCLUSIVE, -1 }},

  {"gDay",         { XS_FACET_PATTERN, XS_FACET_ENUMERATION, XS_FACET_WHITESPACE,
                     XS_FACET_MAXINCLUSIVE, XS_FACET_MAXEXCLUSIVE, XS_FACET_MININCLUSIVE,
                     XS_FACET_MINEXCLUSIVE, -1 }},

  {"gMonth",       { XS_FACET_PATTERN, XS_FACET_ENUMERATION, XS_FACET_WHITESPACE,
                     XS_FACET_MAXINCLUSIVE, XS_FACET_MAXEXCLUSIVE, XS_FACET_MININCLUSIVE,
                     XS_FACET_MINEXCLUSIVE, -1 }},

  {"hexBinary",    { XS_FACET_LENGTH, XS_FACET_MINLENGTH, XS_FACET_MAXLENGTH, XS_FACET_PATTERN,
                     XS_FACET_ENUMERATION, XS_FACET_WHITESPACE, -1 }},

  {"base64Binary", { XS_FACET_LENGTH, XS_FACET_MINLENGTH, XS_FACET_MAXLENGTH, XS_FACET_PATTERN,
                     XS_FACET_ENUMERATION, XS_FACET_WHITESPACE, -1 }},

  {"anyURI",       { XS_FACET_LENGTH, XS_FACET_MINLENGTH, XS_FACET_MAXLENGTH, XS_FACET_PATTERN,
                     XS_FACET_ENUMERATION, XS_FACET_WHITESPACE, -1 }},

  {"QName",        { XS_FACET_LENGTH, XS_FACET_MINLENGTH, XS_FACET_MAXLENGTH, XS_FACET_PATTERN,
                     XS_FACET_ENUMERATION, XS_FACET_WHITESPACE, -1 }},

  {"NOTATION",     { XS_FACET_LENGTH, XS_FACET_MINLENGTH, XS_FACET_MAXLENGTH, XS_FACET_PATTERN,
                     XS_FACET_ENUMERATION, XS_FACET_WHITESPACE, -1 }},

  { NULL,          { -1 }}
};

int xs_visit_particle(xs_schema *s, xmlDocPtr doc, void *data, xs_particle *p, xs_visitor *v);
int xs_visit_type(xs_schema *s, xmlDocPtr doc, void *data, xs_type *t, xs_visitor *v);
int xs_visit_attribute_group_ref(xs_schema *s, xmlDocPtr doc, void *data,
                                 xs_attribute_group_ref *agr, xs_visitor *v);
int xs_visit_attribute_group(xs_schema *s, xmlDocPtr doc, void *data, xs_attribute_group *a,
                             xs_visitor *v);
int xs_visit_attribute_use(xs_schema *s, xmlDocPtr doc, void *data, xs_attribute_use *au,
                           xs_visitor *v);

char *xs_particle_str(xs_particle *p)
{
  if (XS_PARTICLE_TERM_ELEMENT == p->term_type) {
    stringbuf *buf = stringbuf_new();
    char *s;
    stringbuf_format(buf,"element %#n",p->term.e->def.ident);
    s = strdup(buf->data);
    stringbuf_free(buf);
    return s;
  }
  else if (XS_PARTICLE_TERM_WILDCARD == p->term_type) {
    return strdup("<any>");
  }
  else {
    assert(XS_PARTICLE_TERM_MODEL_GROUP == p->term_type);
    if (XS_MODEL_GROUP_COMPOSITOR_ALL == p->term.mg->compositor) {
      return strdup("<all>");
    }
    else if (XS_MODEL_GROUP_COMPOSITOR_CHOICE == p->term.mg->compositor) {
      return strdup("<choice>");
    }
    else {
      assert(XS_MODEL_GROUP_COMPOSITOR_SEQUENCE == p->term.mg->compositor);
      return strdup("<sequence>");
    }
  }
}

char *xs_range_str(xs_range r)
{
  char *str = (char*)malloc(100);
  if (0 > r.max_occurs)
    sprintf(str,"%d-unbounded",r.min_occurs);
  else
    sprintf(str,"%d-%d",r.min_occurs,r.max_occurs);
  return str;
}

int xs_resolve_ref(xs_schema *s, xs_reference *r)
{
  /* FIXME: Currently, QName references within a schema can only resolve to objects declared in
     that schema itself or other schemas in the imports list. It is not possible for a reference
     in an imported schema document to resolve to an object in the parent schema. I'm not sure
     whether or not we're supposed to support this. */

  /* @implements(xmlschema-1:src-resolve.1) @end
     @implements(xmlschema-1:src-resolve.1.1) @end
     @implements(xmlschema-1:src-resolve.1.2) @end
     @implements(xmlschema-1:src-resolve.1.3) @end
     @implements(xmlschema-1:src-resolve.1.4) @end
     @implements(xmlschema-1:src-resolve.1.5) @end
     @implements(xmlschema-1:src-resolve.1.6) @end
     @implements(xmlschema-1:src-resolve.2) @end
     @implements(xmlschema-1:src-resolve.3) @end
     @implements(xmlschema-1:src-resolve.4) @end
     @implements(xmlschema-1:src-resolve.4.1) @end
     @implements(xmlschema-1:src-resolve.4.1.1) @end
     @implements(xmlschema-1:src-resolve.4.1.2) @end
     @implements(xmlschema-1:src-resolve.4.2) @end
     @implements(xmlschema-1:src-resolve.4.2.1) @end
     @implements(xmlschema-1:src-resolve.4.2.2) @end */
  assert(!r->builtin);
  assert(NULL != r->def.ident.name);
  assert(NULL == r->target); /* shouldn't be resolved yet */
  assert(!r->resolved);

  if (NULL == (r->target = xs_lookup_object(r->s,r->type,r->def.ident)))
    return error(&s->ei,s->uri,r->def.loc.line,NULL,"No such %s %#n",
                 xs_object_types[r->type],r->def.ident);

  if (r->obj)
    *r->obj = r->target;

  if (r->check)
    CHECK_CALL(r->check(r->s,r))

  if (XS_OBJECT_MODEL_GROUP_DEF == r->type) {
    /* FIXME: HACK: for <group ref="..."> we want the *model group* to be set as the object,
       not the def itself */
    xs_model_group_def *mgd = (xs_model_group_def*)(*r->obj);
    if (r->obj)
      *r->obj = mgd->model_group;
     r->target = mgd->model_group;
  }

  r->resolved = 1;

  return 0;
}

int xs_facet_allowed(xs_type *t, int facet)
{
  int f;
  if (NULL == t->primitive_type)
    return 1;
  for (f = 0; 0 <= t->primitive_type->allowed_facets[f]; f++)
    if (t->primitive_type->allowed_facets[f] == facet)
      return 1;
  return 0;
}

xs_type *xs_init_complex_ur_type(xs_globals *g)
{
  /* 3.4.7 Built-in Complex Type Definition */
  xs_type *complex_ur_type = xs_type_new(g->as);
  xs_particle *p = xs_particle_new(g->as);
  p->in_anyType = 1;
  complex_ur_type->builtin = 1;
  complex_ur_type->complex = 1;
  complex_ur_type->def.ident.name = strdup("anyType");
  complex_ur_type->def.ident.ns = strdup(XS_NAMESPACE);
  complex_ur_type->base = complex_ur_type;
  complex_ur_type->derivation_method = XS_TYPE_DERIVATION_RESTRICTION;
  complex_ur_type->mixed = 1;
  complex_ur_type->content_type = xs_particle_new(g->as);
  complex_ur_type->content_type->in_anyType = 1;
  complex_ur_type->content_type->range.min_occurs = 1;
  complex_ur_type->content_type->range.max_occurs = 1;
  complex_ur_type->content_type->term_type = XS_PARTICLE_TERM_MODEL_GROUP;
  complex_ur_type->content_type->term.mg = xs_model_group_new(g->as);
  complex_ur_type->content_type->term.mg->compositor = XS_MODEL_GROUP_COMPOSITOR_SEQUENCE;
  complex_ur_type->content_type->term.mg->typeinfo_known = 1;
  complex_ur_type->content_type->term.mg->size = 0;
  complex_ur_type->content_type->term.mg->ctype = strdup("****anytype****");
  complex_ur_type->complex_content = 1;
  list_push(&complex_ur_type->content_type->term.mg->particles,p);
  p->range.min_occurs = 0;
  p->range.max_occurs = -1;
  p->term_type = XS_PARTICLE_TERM_WILDCARD;
  g->complex_ur_type_element_wildcard = xs_wildcard_new(g->as);
  p->term.w = g->complex_ur_type_element_wildcard;
  p->term.w->type = XS_WILDCARD_TYPE_ANY;
  p->term.w->process_contents = XS_WILDCARD_PROCESS_CONTENTS_LAX;
  p->term.w->defline = 0;
  complex_ur_type->effective_content = complex_ur_type->content_type;
  complex_ur_type->attribute_uses = NULL;
  complex_ur_type->attribute_wildcard = xs_wildcard_new(g->as);
  complex_ur_type->attribute_wildcard->type = XS_WILDCARD_TYPE_ANY;
  complex_ur_type->attribute_wildcard->process_contents = XS_WILDCARD_PROCESS_CONTENTS_LAX;
  complex_ur_type->attribute_wildcard->defline = 0;
  complex_ur_type->final_restriction = 0;
  complex_ur_type->final_extension = 0;
  complex_ur_type->prohibited_extension = 0;
  complex_ur_type->prohibited_restriction = 0;
  complex_ur_type->abstract = 0;
  complex_ur_type->computed_wildcard = 1;
  complex_ur_type->computed_attribute_uses = 1;
  complex_ur_type->computed_content_type = 1;
  /* FIXME: set typeinfo based on contents of anyType */
  complex_ur_type->typeinfo_known = 1;
  complex_ur_type->size = 0;
  complex_ur_type->ctype = strdup("XSanyType");
  return complex_ur_type;
}

xs_type *xs_init_simple_ur_type(xs_globals *g)
{
  /* 3.14.7 Built-in Simple Type Definition */
  xs_type *simple_ur_type = xs_type_new(g->as);
  simple_ur_type->builtin = 1;
  simple_ur_type->complex = 0;
  simple_ur_type->def.ident.name = strdup("anySimpleType");
  simple_ur_type->def.ident.ns = strdup(XS_NAMESPACE);
  assert(g->complex_ur_type);
  simple_ur_type->base = g->complex_ur_type;
  simple_ur_type->final_extension = 0;
  simple_ur_type->final_restriction = 0;
  simple_ur_type->variety = XS_TYPE_VARIETY_ATOMIC; /* absent? */
  simple_ur_type->stype = XS_TYPE_SIMPLE_BUILTIN;
  simple_ur_type->allowed_facets = xs_allowed_facets[0].facets;
  simple_ur_type->typeinfo_known = 1;
  simple_ur_type->size = sizeof(IMPL_POINTER);
  simple_ur_type->ctype = strdup("XSanySimpleType");
  return simple_ur_type;
}

int xs_wildcard_namespace_constraints_equal(xs_schema *s, xs_wildcard *a, xs_wildcard *b)
{
  list *l;

  if (a->type != b->type)
    return 0;

  if (XS_WILDCARD_TYPE_ANY == a->type) {
    return 1;
  }
  else if (XS_WILDCARD_TYPE_NOT == a->type) {
    if ((NULL == a->not_ns) && (NULL == b->not_ns))
      return 1;
    if ((NULL != a->not_ns) && (NULL != b->not_ns) && !strcmp(a->not_ns,b->not_ns))
      return 1;
    return 0;
  }
  else {
    assert(XS_WILDCARD_TYPE_SET == a->type);
    for (l = a->nslist; l; l = l->next)
      if (!list_contains_string(b->nslist,(char*)l->data))
        return 0;
    for (l = b->nslist; l; l = l->next)
      if (!list_contains_string(a->nslist,(char*)l->data))
        return 0;
    return 1;
  }
}

void xs_wildcard_copy_namespace_constraint(xs_wildcard *to, xs_wildcard *from)
{
  list *l;
  to->type = from->type;
  to->not_ns = from->not_ns ? strdup(from->not_ns) : NULL;
  assert(NULL == to->nslist);
  for (l = from->nslist; l; l = l->next) {
    char *ns = (char*)l->data;
    list_append(&to->nslist,ns ? strdup(ns) : NULL);
  }
}

int xs_wildcard_constraint_is_subset(xs_schema *s, xs_wildcard *sub, xs_wildcard *super)
{
  /* @implements(xmlschema-1:cos-ns-subset.1)
     test { wildcard_subset1a.test }
     test { wildcard_subset1b.test }
     @end
  */
  if (XS_WILDCARD_TYPE_ANY == super->type)
    return 1;

  /* @implements(xmlschema-1:cos-ns-subset.2)
     test { wildcard_subset2a.test }
     test { wildcard_subset2b.test }
     test { wildcard_subset2c.test }
     test { wildcard_subset2d.test }
     test { wildcard_subset2e.test }
     test { wildcard_subset2f.test }
     @end
     @implements(xmlschema-1:cos-ns-subset.2.1) @end
     @implements(xmlschema-1:cos-ns-subset.2.2) @end
  */
  if ((XS_WILDCARD_TYPE_NOT == sub->type) && (XS_WILDCARD_TYPE_NOT == super->type) &&
      (((NULL == sub->not_ns) && (NULL == super->not_ns)) ||
       ((NULL != sub->not_ns) && (NULL != super->not_ns) && !strcmp(sub->not_ns,super->not_ns))))
    return 1;

  /* @implements(xmlschema-1:cos-ns-subset.3)
     test { wildcard_subset3a.test }
     test { wildcard_subset3b.test }
     test { wildcard_subset3c.test }
     test { wildcard_subset3d.test }
     test { wildcard_subset3e.test }
     test { wildcard_subset3f.test }
     test { wildcard_subset3g.test }
     test { wildcard_subset3h.test }
     test { wildcard_subset3i.test }
     test { wildcard_subset3j.test }
     test { wildcard_subset3k.test }
     test { wildcard_subset3l.test }
     test { wildcard_subset3m.test }
     test { wildcard_subset3n.test }
     @end
     @implements(xmlschema-1:cos-ns-subset.3.1) @end
     @implements(xmlschema-1:cos-ns-subset.3.2) @end
     @implements(xmlschema-1:cos-ns-subset.3.2.1) @end
     @implements(xmlschema-1:cos-ns-subset.3.2.2) @end
  */
  if (XS_WILDCARD_TYPE_SET == sub->type) {

    /* 3.10.6-2-3-3-2-1 */
    if (XS_WILDCARD_TYPE_SET == super->type) {
      list *l;
      int found_missing = 0;
      for (l = sub->nslist; l; l = l->next)
        if (!list_contains_string(super->nslist,(char*)l->data))
          found_missing = 1;
      if (!found_missing)
        return 1;
    }

    /* 3.10.6-2-3-3-2-2 */
    if (XS_WILDCARD_TYPE_NOT == super->type) {
      if (!list_contains_string(sub->nslist,super->not_ns) &&
          !list_contains_string(sub->nslist,NULL))
        return 1;
    }
  }

  return 0;
}

xs_wildcard *xs_wildcard_constraint_union(xs_schema *s, xs_wildcard *O1, xs_wildcard *O2,
                                          int process_contents)
{
  list *S;
  char *not_ns;
  xs_wildcard *w = xs_wildcard_new(s->as);
  w->process_contents = process_contents;
  w->defline = (0 < O1->defline) ? O1->defline : O2->defline;

  /* @implements(xmlschema-1:cos-aw-union.1)
     test { wildcard_union1a.test }
     test { wildcard_union1b.test }
     test { wildcard_union1c.test }
     test { wildcard_union1d.test }
     test { wildcard_union1e.test }
     @end
  */
  if (xs_wildcard_namespace_constraints_equal(s,O1,O2)) {
    xs_wildcard_copy_namespace_constraint(w,O1);
    return w;
  }

  /* @implements(xmlschema-1:cos-aw-union.2)
     test { wildcard_union2ar.test }
     test { wildcard_union2a.test }
     test { wildcard_union2br.test }
     test { wildcard_union2b.test }
     test { wildcard_union2cr.test }
     test { wildcard_union2c.test }
     @end
  */
  if ((XS_WILDCARD_TYPE_ANY == O1->type) || (XS_WILDCARD_TYPE_ANY == O2->type)) {
    w->type = XS_WILDCARD_TYPE_ANY;
    return w;
  }

  /* @implements(xmlschema-1:cos-aw-union.3)
     test { wildcard_union3ar.test }
     test { wildcard_union3a.test }
     test { wildcard_union3br.test }
     test { wildcard_union3b.test }
     test { wildcard_union3cr.test }
     test { wildcard_union3c.test }
     @end
  */
  if ((XS_WILDCARD_TYPE_SET == O1->type) && (XS_WILDCARD_TYPE_SET == O2->type)) {
    w->type = XS_WILDCARD_TYPE_SET;
    w->nslist = string_list_union(O1->nslist,O2->nslist);
    return w;
  }

  /* @implements(xmlschema-1:cos-aw-union.4) @end */
  if ((XS_WILDCARD_TYPE_NOT == O1->type) && (XS_WILDCARD_TYPE_NOT == O2->type)) {
    /* we know the values are different because otherwise they would have been taken care of
       by rule 3.10.6-3-1 */
    /* FIXME: no test for this currently because there is no way to create "not" wildcards in
       different namespaces... this requires <import> functionality */
    w->type = XS_WILDCARD_TYPE_NOT;
    w->not_ns = NULL;
    return w;
  }

  if ((XS_WILDCARD_TYPE_NOT == O1->type) && (XS_WILDCARD_TYPE_SET == O2->type)) {
    S = O2->nslist;
    not_ns = O1->not_ns;
  }
  else {
    assert((XS_WILDCARD_TYPE_NOT == O2->type) && (XS_WILDCARD_TYPE_SET == O1->type));
    S = O1->nslist;
    not_ns = O2->not_ns;
  }

  if (NULL != not_ns) {
    /* @implements(xmlschema-1:cos-aw-union.5) @end */
    if (list_contains_string(S,not_ns)) {
      if (list_contains_string(S,NULL)) {
        /* @implements(xmlschema-1:cos-aw-union.5.1)
           test { wildcard_union51ar.test }
           test { wildcard_union51a.test }
           test { wildcard_union51br.test }
           test { wildcard_union51b.test }
           @end */
        w->type = XS_WILDCARD_TYPE_ANY;
      }
      else {
        /* @implements(xmlschema-1:cos-aw-union.5.2)
           test { wildcard_union52ar.test }
           test { wildcard_union52a.test }
           test { wildcard_union52br.test }
           test { wildcard_union52b.test }
           @end */
        w->type = XS_WILDCARD_TYPE_NOT;
        w->not_ns = NULL;
      }
    }
    else if (list_contains_string(S,NULL)) {
      /* @implements(xmlschema-1:cos-aw-union.5.3)
         test { wildcard_union53ar.test }
         test { wildcard_union53a.test }
         test { wildcard_union53br.test }
         test { wildcard_union53b.test }
         @end */
      w = NULL; /* union not expressible */
      /* FIXME: is is safe to return NULL here? should check all cases that use this function */
    }
    else {
      /* @implements(xmlschema-1:cos-aw-union.5.4)
         test { wildcard_union54ar.test }
         test { wildcard_union54a.test }
         test { wildcard_union54br.test }
         test { wildcard_union54b.test }
         @end */
      w->type = XS_WILDCARD_TYPE_NOT;
      w->not_ns = strdup(not_ns);
    }
  }
  else {
    /* @implements(xmlschema-1:cos-aw-union.6) @end */
    if (list_contains_string(S,NULL)) {
      /* @implements(xmlschema-1:cos-aw-union.6.1)
         test { wildcard_union61ar.test }
         test { wildcard_union61a.test }
         test { wildcard_union61br.test }
         test { wildcard_union61b.test }
         @end */
      w->type = XS_WILDCARD_TYPE_ANY;
    }
    else {
      /* @implements(xmlschema-1:cos-aw-union.6.2)
         test { wildcard_union62ar.test }
         test { wildcard_union62a.test }
         test { wildcard_union62br.test }
         test { wildcard_union62b.test }
         @end */
      w->type = XS_WILDCARD_TYPE_NOT;
      w->not_ns = NULL;
    }
  }

  return w;
}

xs_wildcard *xs_wildcard_constraint_intersection(xs_schema *s, xs_wildcard *O1, xs_wildcard *O2,
                                                 int process_contents)
{
  list *S = NULL;
  char *not_ns = NULL;
  xs_wildcard *w = xs_wildcard_new(s->as);
  w->process_contents = process_contents;
  w->defline = (0 < O1->defline) ? O1->defline : O2->defline;

  /* @implements(xmlschema-1:cos-aw-intersect.1)
     test { wildcard_intersection1a.test }
     test { wildcard_intersection1b.test }
     test { wildcard_intersection1c.test }
     test { wildcard_intersection1d.test }
     test { wildcard_intersection1e.test }
     @end
  */
  if (xs_wildcard_namespace_constraints_equal(s,O1,O2)) {
    xs_wildcard_copy_namespace_constraint(w,O1);
    return w;
  }

  /* @implements(xmlschema-1:cos-aw-intersect.2)
     test { wildcard_intersection2a.test }
     test { wildcard_intersection2ar.test }
     test { wildcard_intersection2b.test }
     test { wildcard_intersection2br.test }
     test { wildcard_intersection2c.test }
     test { wildcard_intersection2cr.test }
     @end
  */
  if (XS_WILDCARD_TYPE_ANY == O1->type) {
    xs_wildcard_copy_namespace_constraint(w,O1);
    return w;
  }

  if (XS_WILDCARD_TYPE_ANY == O2->type) {
    xs_wildcard_copy_namespace_constraint(w,O2);
    return w;
  }

  /* @implements(xmlschema-1:cos-aw-intersect.3)
     test { wildcard_intersection3a.test }
     test { wildcard_intersection3ar.test }
     test { wildcard_intersection3b.test }
     test { wildcard_intersection3br.test }
     test { wildcard_intersection3c.test }
     test { wildcard_intersection3cr.test }
     test { wildcard_intersection3d.test }
     test { wildcard_intersection3dr.test }
     test { wildcard_intersection3e.test }
     test { wildcard_intersection3er.test }
     @end
  */
  if ((XS_WILDCARD_TYPE_NOT == O1->type) && (XS_WILDCARD_TYPE_SET == O2->type)) {
    S = O2->nslist;
    not_ns = O1->not_ns;
  }
  else if ((XS_WILDCARD_TYPE_SET == O1->type) && (XS_WILDCARD_TYPE_NOT == O2->type)) {
    S = O1->nslist;
    not_ns = O2->not_ns;
  }

  if (NULL != S) {
    list *l;
    w->type = XS_WILDCARD_TYPE_SET;
    for (l = S; l; l = l->next) {
      char *str = (char*)l->data;
      if ((NULL != str) && ((NULL == not_ns) || strcmp(str,not_ns)))
        list_append(&w->nslist,strdup(str));
    }
    return w;
  }

  /* @implements(xmlschema-1:cos-aw-intersect.4)
     test { wildcard_intersection4a.test }
     test { wildcard_intersection4ar.test }
     test { wildcard_intersection4b.test }
     test { wildcard_intersection4br.test }
     test { wildcard_intersection4c.test }
     test { wildcard_intersection4cr.test }
     @end
  */
  if ((XS_WILDCARD_TYPE_SET == O1->type) && (XS_WILDCARD_TYPE_SET == O2->type)) {
    w->type = XS_WILDCARD_TYPE_SET;
    w->nslist = string_list_intersection(O1->nslist,O2->nslist);
    return w;
  }

  assert((XS_WILDCARD_TYPE_NOT == O1->type) && (XS_WILDCARD_TYPE_NOT == O2->type));

  /* @implements(xmlschema-1:cos-aw-intersect.5) @end */
  if ((NULL != O1->not_ns) && (NULL != O2->not_ns)) {
    /* we know the values are different because otherwise they would have been taken care of
       by rule 3.10.6-4-1 */
    /* FIXME: no test for this currently because there is no way to create "not" wildcards in
       different namespaces... this requires <import> functionality */
    w = NULL; /* intersection not expressible */
    /* FIXME: is is safe to return NULL here? should check all cases that use this function */
  }
  /* @implements(xmlschema-1:cos-aw-intersect.6) @end */
  else {
    /* one is absent and the other has a value */
    /* FIXME: no test for this currently because there is no way to create "not" wildcards in
       different namespaces... this requires <import> functionality */
    w->type = XS_WILDCARD_TYPE_NOT;
    w->not_ns = O1->not_ns ? strdup(O1->not_ns) : strdup(O2->not_ns);
  }

  return NULL;
}

int check_attribute_uses(xs_schema *s, list *attribute_uses, char *constraint1, char *constraint2)
{
  list *encountered = NULL;
  list *l;
  xs_attribute_use *id_derivative = NULL;

  /* @implements(xmlschema-1:ct-props-correct.4)
     description {Check for duplicate attributes directly inside the same complex type}
     test { complextype_attribute_dup_ref2.test }
     test { complextype_attribute_dup_ref.test }
     test { complextype_attribute_dup.test }
     test { complextype_attribute_basedup.test }
     @end

     @implements(xmlschema-1:ag-props-correct.2)
     description {Check for duplicate attributes directly inside the same attribute group}
     test { attributegroup_attribute_dup_ref2.test }
     test { attributegroup_attribute_dup_ref.test }
     test { attributegroup_attribute_dup.test }
     test { attributegroup_attribute_dup2.test }
     test { attributegroup_attribute_dup3.test }
     @end
  */
  for (l = attribute_uses; l; l = l->next) {
    xs_attribute_use *au = (xs_attribute_use*)l->data;
    xs_type *t;
    list *l2;
    assert(NULL != au->attribute);
    assert(NULL != au->attribute->type);

    for (l2 = encountered; l2; l2 = l2->next) {
      xs_attribute_use *au2 = (xs_attribute_use*)l2->data;
      assert(NULL != au->attribute);
      if (!strcmp(au->attribute->def.ident.name,au2->attribute->def.ident.name) &&
          (((NULL == au->attribute->def.ident.ns) && (NULL == au2->attribute->def.ident.ns)) ||
           ((NULL != au->attribute->def.ident.ns) && (NULL != au2->attribute->def.ident.ns) &&
            !strcmp(au->attribute->def.ident.ns,au2->attribute->def.ident.ns)))) {
        list_free(encountered,NULL);
        return error(&s->ei,s->uri,au2->defline,constraint1,"duplicate attribute not allowed: %#n",
                     au2->attribute->def.ident);
      }
    }

    /* @implements(xmlschema-1:ct-props-correct.5)
       description { Disallow multiple attributes with type ID or type derived from ID }
       test { complextype_attribute_id_dup2.test }
       test { complextype_attribute_id_dup3.test }
       test { complextype_attribute_id_dup.test }
       @end

       @implements(xmlschema-1:ag-props-correct.3)
       description { Disallow multiple attributes with type ID or type derived from ID }
       test { attributegroup_attribute_id_dup2.test }
       test { attributegroup_attribute_id_dup3.test }
       test { attributegroup_attribute_id_dup.test }
       @end
    */
    for (t = au->attribute->type; t && t != t->base; t = t->base) {
      if (t == s->globals->id_type) {
        if (NULL != id_derivative) {
          list_free(encountered,NULL);
          return error(&s->ei,s->uri,au->defline,constraint2,"only one attribute in a group can be "
                       "of type ID (or a derivative); %s conflicts with %s",
                       au->attribute->def.ident.name,id_derivative->attribute->def.ident.name);
        }
        id_derivative = au;
      }
    }


    list_push(&encountered,au);
  }
  list_free(encountered,NULL);

  return 0;
}

int check_circular_references(xs_schema *s, void *obj, char *type, int defline, char *section,
                              char *constraint,
                              char* (get_obj_name)(void *obj),
                              void (add_references)(xs_schema *s, void *obj, list **tocheck))
{
  list *encountered = NULL;
  list *tocheck = NULL;
  void *cur;

  list_push(&tocheck,obj);
  while (NULL != (cur = list_pop(&tocheck))) {
    char *name = get_obj_name(cur);
    list *l;
    for (l = encountered; l; l = l->next) {
      if (cur == l->data) {
        stringbuf *buf = stringbuf_new();
        list *l2;
        for (l2 = encountered; l2; l2 = l2->next) {
          char *name2 = get_obj_name(l2->data);
          stringbuf_format(buf,"%s -> ",name2);
        }
        stringbuf_format(buf,"%s",name);

        /* FIXME: circular references are allowed inside a <redefine> - see section 3.6.3 rule 3 */
        if (constraint)
          error(&s->ei,s->uri,defline,constraint,"Circular %s reference detected: %s",
                type,buf->data);
        else
          error(&s->ei,s->uri,defline,NULL,"Circular %s reference detected: %s",type,buf->data);
        stringbuf_free(buf);
        list_free(tocheck,NULL);
        list_free(encountered,NULL);
        return -1;
      }
    }
    add_references(s,cur,&tocheck);
    list_append(&encountered,cur);
  }

  list_free(tocheck,NULL);
  list_free(encountered,NULL);

  return 0;
}

char* get_model_group_def_name(void *obj)
{
  return ((xs_model_group_def*)obj)->def.ident.name;
}

void add_model_group_mgdrefs(xs_schema *s, xs_model_group *mg, list **mgds, int indent)
{
  list *l;
  for (l = mg->particles; l; l = l->next) {
    xs_particle *p = (xs_particle*)l->data;
    if (XS_PARTICLE_TERM_MODEL_GROUP == p->term_type) {
      if (NULL != p->term.mg->mgd) {
        list_push(mgds,p->term.mg->mgd);
      }
      else {
        add_model_group_mgdrefs(s,p->term.mg,mgds,indent+1);
      }
    }
  }
}


void add_model_group_def_references(xs_schema *s, void *obj, list **tocheck)
{
  xs_model_group_def *mgd = (xs_model_group_def*)obj;
  add_model_group_mgdrefs(s,mgd->model_group,tocheck,1);
}

int post_process_model_group_def(xs_schema *s, xmlDocPtr doc, xs_model_group_def *mgd)
{
  /* @implements(xmlschema-1:mg-props-correct.2)
     test { groupdef_sequence_circular.test }
     test { groupdef_sequence_circular2.test }
     test { groupdef_sequence_bigcircular.test }
     test { groupdef_choice_circular.test }
     test { groupdef_choice_circular2.test }
     test { groupdef_choice_bigcircular.test }
     @end */
  CHECK_CALL(check_circular_references(s,mgd,"model group",mgd->def.loc.line,NULL,
                                       "mg-props-correct.2",
                                       get_model_group_def_name,add_model_group_def_references));
  return 0;
}


void xs_get_first_elements_and_wildcards(xs_particle *p, list **first_ew)
{
  if (XS_PARTICLE_TERM_ELEMENT == p->term_type) {
    list_push(first_ew,p);
  }
  else if (XS_PARTICLE_TERM_MODEL_GROUP == p->term_type) {
    if (XS_MODEL_GROUP_COMPOSITOR_SEQUENCE == p->term.mg->compositor) {
      if (p->term.mg->particles) {
        xs_particle *p2 = (xs_particle*)(p->term.mg->particles->data);
        xs_get_first_elements_and_wildcards(p2,first_ew);
      }
    }
    else {
      list *l;
      for (l = p->term.mg->particles; l; l = l->next) {
        xs_particle *p2 = (xs_particle*)l->data;
        xs_get_first_elements_and_wildcards(p2,first_ew);
      }
    }
  }
  else {
    assert(XS_PARTICLE_TERM_WILDCARD == p->term_type);
    list_push(first_ew,p);
  }
}

/** Get the element and all elements in it's substitution group member list, recursively */
void get_sg_members_r(xs_element *e, list **elements)
{
  list *l;
  list_push(elements,e);
  for (l = e->sgmembers; l; l = l->next)
    get_sg_members_r((xs_element*)l->data,elements);
}

/** Get a list of all elements in the same substitution group as e */
void get_all_sg_elements(xs_element *e, list **elements)
{
  while (NULL != e->sghead)
    e = e->sghead;
  get_sg_members_r(e,elements);
}

void get_all_elements(xs_particle *p, list **elements)
{
  list *l;
  if (XS_PARTICLE_TERM_ELEMENT == p->term_type) {
    get_all_sg_elements(p->term.e,elements);
  }
  else if (XS_PARTICLE_TERM_MODEL_GROUP == p->term_type) {
    for (l = p->term.mg->particles; l; l = l->next) {
      xs_particle *p2 = (xs_particle*)l->data;
      get_all_elements(p2,elements);
    }
  }
}

int particles_overlap(xs_particle *p1, xs_particle *p2, int indent)
{
/*   char *p1str = xs_particle_str(p1); */
/*   char *p2str = xs_particle_str(p2); */
/*   debug_indent(indent,"particles_overlap(%s,%s)",p1str,p2str); */
/*   free(p1str); */
/*   free(p2str); */

  assert(XS_PARTICLE_TERM_MODEL_GROUP != p1->term_type);
  assert(XS_PARTICLE_TERM_MODEL_GROUP != p2->term_type);

  /* H Analysis of the Unique Particle Attribution Constraint (non-normative) */

  /* Note: this function assumes both particles are elements or wildcards. For groups,
     xs_adjacent_particles_overlap() must be used */

  /* [Definition:]  Two non-group particles overlap if */

  /* They are both element declaration particles whose declarations have the same {name} and
     {target namespace}. */
  if ((XS_PARTICLE_TERM_ELEMENT == p1->term_type) &&
      (XS_PARTICLE_TERM_ELEMENT == p2->term_type) &&
      nsname_equals(p1->term.e->def.ident,p2->term.e->def.ident))
    return 1;

  /* or They are both element declaration particles one of whose {name} and {target namespace} are
     the same as those of an element declaration in the other's substitution group. */
  /* FIXME */

  /* or They are both wildcards, and the intensional intersection of their {namespace constraint}s
     as defined in Attribute Wildcard Intersection (3.10.6) is not the empty set. */
  /* FIXME */

  /* or One is a wildcard and the other an element declaration, and the {target namespace} of any
     member of its substitution group is valid with respect to the {namespace constraint} of the
     wildcard. */
  /* FIXME */

  return 0;
}

int xs_adjacent_particles_overlap(xs_particle *p1, xs_particle *p2, int indent,
                                  xs_particle **overlap1, xs_particle **overlap2)
{
  list *l;

  /* FIXME: this algorithm does not cover all cases. Revisit when doing validation/encoding */

  /* This function checks for the unique particle attribution in the case of two particles
     which can match adjacent information items (and the first has minOccurs < maxOccurs). Unlike
     particles_overlap(), this also takes care of model groups, i.e. <all>, <choice> and <sequence>.

     The function calls itself recursively for each particle in the group as appropriate for the
     type of group:
     - For <sequence>, each particle is checked against the next particle in the sequence
     - For <all> and <choice> each particle is checked against the particle preceding the group and
       the particle following it
     e.g. for the following content model:
      <xsd:sequence>
        <element name="a"/>
        <element name="b"/>
      </xsd:sequence>
      <xsd:choice>
        <element name="c"/>
        <element name="d"/>
      </xsd:choice>
      <element name="e"/>

     the following particle combinations would be checked: ab, bc, bd, ce, de

     When a group has maxOccurs > 1, it is treated as if there is another particle following the
     group with the same content model, i.e. for <sequence>, the last element is checked against
     the first (ba in the above example), and for <all> and <choice> all elements are checked
     against all other elements (cc, cd, dc, dd) */

  if (XS_PARTICLE_TERM_MODEL_GROUP == p1->term_type) {
    if (XS_MODEL_GROUP_COMPOSITOR_SEQUENCE == p1->term.mg->compositor) {
      xs_particle *last = NULL;

      if (p1->term.mg->particles)
        last = (xs_particle*)p1->term.mg->particles->data;

      /* Check each item in the sequence to see if it overlaps with the next */
      for (l = p1->term.mg->particles; l && l->next; l = l->next) {
        xs_particle *cp1 = (xs_particle*)l->data;
        xs_particle *cp2 = (xs_particle*)l->next->data;
        if (xs_adjacent_particles_overlap(cp1,cp2,indent+1,overlap1,overlap2))
          return 1;




        last = cp2;
      }
      if (NULL != last) {

        /* Check if last item in sequence overlaps with whatever particle follows this
           sequence (note: if a choice follows the sequence, this function will be called
           once for each particle in the choice) */
        if (xs_adjacent_particles_overlap(last,p2,indent+1,overlap1,overlap2))
          return 1;

        /* if maxOccurs is > 1 or unbouned, then we could have another copy of this sequence
           following the end; check if last item in sequence conflicts with first */
        if (1 != p1->range.max_occurs) {
          xs_particle *first = (xs_particle*)p1->term.mg->particles->data;
          if (xs_adjacent_particles_overlap(last,first,indent+1,overlap1,overlap2))
            return 1;
        }
      }
    }
    else { /* all or choice */
      /* Check each particle to see if it overlaps with p2 */
      for (l = p1->term.mg->particles; l; l = l->next) {
        xs_particle *cp = (xs_particle*)l->data;
        if (xs_adjacent_particles_overlap(cp,p2,indent+1,overlap1,overlap2))
          return 1;
      }

      /* if maxOccurs is > 1 or unbouned, then we could have any particle of this all/choice again,
         so check that this is ok */
      if (1 != p1->range.max_occurs) {
        for (l = p1->term.mg->particles; l; l = l->next) {
          xs_particle *cp = (xs_particle*)l->data;
          if (xs_adjacent_particles_overlap(cp,cp,indent+1,overlap1,overlap2))
            return 1;
        }
      }


    }
    return 0;
  }
  else if (XS_PARTICLE_TERM_MODEL_GROUP == p2->term_type) {
    if (XS_MODEL_GROUP_COMPOSITOR_SEQUENCE == p2->term.mg->compositor) {
      /* Just test with the first particle */
      if (p2->term.mg->particles)
        return xs_adjacent_particles_overlap(p1,(xs_particle*)p2->term.mg->particles->data,
                                             indent+1,overlap1,overlap2);
    }
    else { /* all or choice */
      /* Test with all particles */
      for (l = p2->term.mg->particles; l; l = l->next)
        if (xs_adjacent_particles_overlap(p1,(xs_particle*)l->data,indent+1,overlap1,overlap2))
          return 1;
    }
    return 0;
  }
  else {
    if (particles_overlap(p1,p2,indent)) {
/*       debug_indent(indent,"potential overlap, range = %d-%d", */
/*                    p1->range.min_occurs,p1->range.max_occurs); */
      if ((0 > p1->range.max_occurs) || (p1->range.min_occurs < p1->range.max_occurs)) {
        *overlap1 = p1;
        *overlap2 = p2;
        return 1;
      }
    }
    return 0;
  }
}



int xs_mg_particles_overlap(xs_model_group *mg, xs_particle **overlap1, xs_particle **overlap2)
{
  if (XS_MODEL_GROUP_COMPOSITOR_SEQUENCE == mg->compositor) {
    list *l;
    for (l = mg->particles; l && l->next; l = l->next) {
      xs_particle *p1 = (xs_particle*)l->data;
      xs_particle *p2 = (xs_particle*)l->next->data;
      if (xs_adjacent_particles_overlap(p1,p2,1,overlap1,overlap2))
        return 1;
    }
  }
  return 0;
}

int post_process_model_group(xs_schema *s, xmlDocPtr doc, xs_model_group *mg)
{
  list *elements = NULL;
  list *l;
  xs_particle *overlap1 = NULL;
  xs_particle *overlap2 = NULL;

  /* @implements(xmlschema-1:cos-all-limited.2)
     test { all_element_maxoccurs0.test }
     test { all_element_maxoccurs1.test }
     test { all_element_maxoccurs2.test }
     @end */
  if (XS_MODEL_GROUP_COMPOSITOR_ALL == mg->compositor) {
    list *l;
    for (l = mg->particles; l; l = l->next) {
      xs_particle *p = (xs_particle*)l->data;
      if ((0 != p->range.max_occurs) && (1 != p->range.max_occurs))
        return error(&s->ei,s->uri,p->defline,"cos-all-limited.2",
                     "items directly within <all> must have maxOccurs equal to 0 or 1");
    }
  }

  /* @implements(xmlschema-1:cos-nonambig)
     status { partial }
     description { check unique particle attribution constraint }
     issue { check wildcards }
     issue { check particles that can validate adjacent items }
     test { all_unique.test }
     test { choice_unique.test }
     test { choice_unique2.test }
     test { choice_unique3.test }
     test { choice_unique4.test }
     test { choice_unique5.test }
     test { choice_unique6.test }
     @end
  */

/*   debugl("post_process_model_group"); */
  if (xs_mg_particles_overlap(mg,&overlap1,&overlap2)) {
    char *overlap1str = xs_particle_str(overlap1);
    char *overlap2str = xs_particle_str(overlap2);
/*     debugl("found overlapping particles that can match adjacent elements"); */

    if (overlap1 == overlap2) {
      error(&s->ei,s->uri,overlap1->defline,"cos-nonambig","unique particle attribution constraint "
            "violated: separate instances of %s may validate adjacent information items, and "
            "minOccurs is less than maxOccurs",overlap1str);
    }
    else {
      error(&s->ei,s->uri,overlap1->defline,"cos-nonambig","unique particle attribution constraint "
            "violated: overlapping particles %s and %s may validate adjacent information items, "
            "and the former has minOccurs less than maxOccurs",overlap1str,overlap2str);
    }
    free(overlap1str);
    free(overlap2str);
    return -1;
  }


/*   if (check_adjacent_mg(NULL,mg,NULL,2)) */
/*     printf("********** found overlapping particles that can match adjacent elements\n"); */

  if ((XS_MODEL_GROUP_COMPOSITOR_CHOICE == mg->compositor) ||
      (XS_MODEL_GROUP_COMPOSITOR_ALL == mg->compositor)) {
    list *l;
    list *first_ew = NULL;

    for (l = mg->particles; l; l = l->next) {
      xs_particle *p2 = (xs_particle*)l->data;
      xs_get_first_elements_and_wildcards(p2,&first_ew);
    }
    for (l = first_ew; l; l = l->next) {
      xs_particle *few1 = (xs_particle*)l->data;
      list *l2;
      for (l2 = l->next; l2; l2 = l2->next) {
        xs_particle *few2 = (xs_particle*)l2->data;
        if (particles_overlap(few1,few2,0)) {
          char *few1str = xs_particle_str(few1);
          char *few2str = xs_particle_str(few2);
          error(&s->ei,s->uri,few1->defline,"cos-nonambig","unique particle attribution constraint "
                "violated: %s and %s overlap, and are both within the same <choice> or <all> group",
                few1str,few2str);
          free(few1str);
          free(few2str);
          list_free(first_ew,NULL);
          return -1;
        }
      }
    }
    list_free(first_ew,NULL);
  }

  /* @implements(xmlschema-1:cos-element-consistent.1) @end
     @implements(xmlschema-1:cos-element-consistent.2) @end
     @implements(xmlschema-1:cos-element-consistent.3)
     test { all_inconsistent_types.test }
     test { choice_inconsistent_types.test }
     test { sequence_consistent_types.test }
     test { sequence_inconsistent_types10.test }
     test { sequence_inconsistent_types2.test }
     test { sequence_inconsistent_types3.test }
     test { sequence_inconsistent_types4.test }
     test { sequence_inconsistent_types5.test }
     test { sequence_inconsistent_types6.test }
     test { sequence_inconsistent_types7.test }
     test { sequence_inconsistent_types8.test }
     test { sequence_inconsistent_types9.test }
     test { sequence_inconsistent_types.test }
     @end */
  for (l = mg->particles; l; l = l->next)
    get_all_elements((xs_particle*)l->data,&elements);

  for (l = elements; l; l = l->next) {
    xs_element *e1 = (xs_element*)l->data;
    list *l2;
    for (l2 = l->next; l2; l2 = l2->next) {
      xs_element *e2 = (xs_element*)l2->data;
      if (!strcmp(e1->def.ident.name,e2->def.ident.name) &&
          (((NULL == e1->def.ident.ns) && (NULL == e2->def.ident.ns)) ||
           ((NULL != e1->def.ident.ns) && (NULL != e2->def.ident.ns) &&
            !strcmp(e1->def.ident.ns,e2->def.ident.ns)))) {
        if (e1->type != e2->type) {
          stringbuf *t1 = stringbuf_new();
          stringbuf *t2 = stringbuf_new();
          if (e1->type && !nsname_isnull(e1->type->def.ident))
            stringbuf_format(t1,"%#n",e1->type->def.ident);
          else
            stringbuf_format(t1,"(none)");

          if (e2->type && !nsname_isnull(e2->type->def.ident))
            stringbuf_format(t2,"%#n",e2->type->def.ident);
          else
            stringbuf_format(t2,"(none)");

          error(&s->ei,s->uri,e1->def.loc.line,"cos-element-consistent","inconsistent element "
                "declarations found for %#n: types %s and %s are not the same top-level type "
                "declaration; both elements must have the same type for them to be used within the "
                "same model group",e1->def.ident,t1->data,t2->data);

          stringbuf_free(t1);
          stringbuf_free(t2);
          list_free(elements,NULL);
          return -1;
        }
      }
    }
  }
  list_free(elements,NULL);

  return 0;
}

char *get_element_name(void *obj)
{
  return ((xs_element*)obj)->def.ident.name;
}

void add_element_sgreferences(xs_schema *s, void *obj, list **tocheck)
{
  xs_element *e = (xs_element*)obj;
  if (NULL != e->sghead)
    list_push(tocheck,e->sghead);
}

void compute_element_type(xs_schema *s, xs_element *e)
{
  /* @implements(xmlschema-1:Element Declaration{type definition})
     test { element_type1.test }
     test { element_type2.test }
     test { element_type3.test }
     test { element_type4.test }
     test { element_type5.test }
     test { element_type6.test }
     test { element_type7.test }
     test { element_type8.test }
     test { element_type9.test }
     test { element_d_type1.test }
     test { element_d_type2.test }
     test { element_d_type3.test }
     test { element_d_type4.test }
     test { element_d_type5.test }
     test { element_d_type6.test }
     test { element_d_type7.test }
     test { element_d_type8.test }
     test { element_d_type9.test }
     @end */

  if (e->computed_type)
    return;

  /* 3.3.2 XML Representation Summary: element Element Information Item */

  /* {type definition}
     Case 1: The type definition corresponding to the <simpleType> or <complexType> element
             information item in the [children], if either is present,
     Case 2: otherwise the type definition resolved to by the actual value of the type [attribute],
     Case 3: otherwise the {type definition} of the element declaration resolved to by the
             actual value of the substitutionGroup [attribute], if present
     Case 4: otherwise the ur-type definition.

     The first two cases are taken care of by xs_parse_element(). Howver, if no type was specified,
     then we need to assign it here (cases 3 and 4). We can only do this after parsing has completed
     because we need to make sure that all references have been resolved and the substitution group
     head has computed its type (hence the recursive call to compute_element_type()). We can
     guarantee that this will not cause infinite recursion because circular substitution group
     references have already been checked for in post_process_element1(). */

  if (NULL != e->sghead) {
    if (!e->sghead->computed_type)
      compute_element_type(s,e->sghead);
    assert(e->sghead->type);
  }

  if (NULL == e->type) {
    if ((NULL != e->sghead) && (NULL != e->sghead->type)) {
      e->type = e->sghead->type;
      e->typeref = e->sghead->typeref;
/*       printf("element %s: setting type to that of substitution group head %s (type %s)\n", */
/*              e->name,e->sghead->name,e->sghead->type->name); */
    }
    else {
      e->type = s->globals->complex_ur_type;
      e->typeref = xs_reference_new(s->as);
      e->typeref->def.ident.ns = strdup(e->type->def.ident.ns);
      e->typeref->def.ident.name = strdup(e->type->def.ident.name);
      e->typeref->def.loc.line = e->def.loc.line;
      e->typeref->type = XS_OBJECT_TYPE;
      e->typeref->obj = (void**)&e->type;
      e->typeref->builtin = 0;
    }
  }

  e->computed_type = 1;
}

int post_process_element1(xs_schema *s, xmlDocPtr doc, xs_element *e)
{
  /* @implements(xmlschema-1:e-props-correct.6)
     test { element_sg_bigcircular.test }
     test { element_sg_circular2.test }
     test { element_sg_circular.test }
     @end */
  CHECK_CALL(check_circular_references(s,e,"substitution group",e->def.loc.line,NULL,
                                       "e-props-correct.6",
                                       get_element_name,add_element_sgreferences));
  return 0;
}

int post_process_element2(xs_schema *s, xmlDocPtr doc, xs_element *e)
{
  /* @implements(xmlschema-1:e-props-correct.3) @end
     Note: this constraint specifies that if {substitution group affiliaton} is non-absent
     (i.e. e->sg_head != NULL), then {scope} must be global. This constraint does not apply
     to our implementation for two reasons: 1. because we do not define an equivalent of the
     {scope} property on elements (scope is determined by the context, i.e. if the element
     was found because it was a top-level type declaration), and 2. because xs_parse_element()
     forbids the "substitutionGroup" attribute when the element declaration is not a top-level
     one.
     Because parsing from XML is currently the only way to create a schema, this constraint
     is implicitly enforced by the parser. If we add other means to create schemas in the future
     (i.e. via APIs) then we would need an explicit check here.
  */


  /* @implements(xmlschema-1:e-props-correct.5),
     status { partial }
     test { element_tl_id_default.test }
     test { element_tl_id_fixed.test }
     test { element_tl_idderiv_default.test }
     test { element_tl_idderiv_fixed.test }
     test { element_tl_idderiv_ct_default.test }
     test { element_tl_idderiv_ct_fixed.test }
     issue { still need to check t.content type - waiting on completed support for <simpleContent> }
     @end */
  if (XS_VALUE_CONSTRAINT_NONE != e->vc.type) {
    xs_type *t;
    int id_derived = 0;
    for (t = e->type; t && t != t->base; t = t->base) {
      if (t == s->globals->id_type)
        id_derived = 1;
      /* FIXME: check t->{content type} */
    }
    if (id_derived)
      return error(&s->ei,s->uri,e->def.loc.line,"e-props-correct.5",
                   "value constraints cannot be specified for ID-derived elements");
    /* If default/fixed value specified, need to check here that it is valid */
  }

  return 0;
}

int post_process_attribute(xs_schema *s, xmlDocPtr doc, xs_attribute *a)
{
  /* @implements(xmlschema-1:a-props-correct.3)
     test { attribute_tl_id_default.test }
     test { attribute_tl_id_fixed.test }
     test { attribute_tl_idderiv_default.test }
     test { attribute_tl_idderiv_fixed.test }
     @end */
  if (XS_VALUE_CONSTRAINT_NONE != a->vc.type) {
    xs_type *t;
    for (t = a->type; t && t != t->base; t = t->base)
      if (t == s->globals->id_type)
        return error(&s->ei,s->uri,a->def.loc.line,"a-props-correct.3",
                     "value constraints cannot be specified for ID-derived attributes");
  }

  return 0;
}

int post_process_attribute_use(xs_schema *s, xmlDocPtr doc, xs_attribute_use *au)
{
  assert(au->attribute);

  /* Note: we have to do this check separately for both attributes and attribute uses, since each
     has an independent value constraint, i.e. it is possible to have a top-level attribute
     declaration with one value for "default", and a local reference with a different value */

  /* @implements(xmlschema-1:a-props-correct.3)
     test { attribute_local_id_default.test }
     test { attribute_local_id_fixed.test }
     test { attribute_local_idderiv_default.test }
     test { attribute_local_idderiv_fixed.test }
     test { attribute_local_ref_id_default.test }
     test { attribute_local_ref_id_fixed.test }
     test { attribute_local_ref_idderiv_default.test }
     test { attribute_local_ref_idderiv_fixed.test }
     @end */
  if (XS_VALUE_CONSTRAINT_NONE != au->vc.type) {
    xs_type *t;
    for (t = au->attribute->type; t && t != t->base; t = t->base)
      if (t == s->globals->id_type)
        return error(&s->ei,s->uri,au->defline,"a-props-correct.3",
                     "value constraints cannot be specified for ID-derived attributes");
  }

  /* @implements(xmlschema-1:au-props-correct.2)
     test { attribute_local_ref_fixed_same.test }
     test { attribute_local_ref_fixed_different.test }
     test { attribute_local_ref_fixed_missing.test }
     test { attribute_local_ref_fixed_default.test }
     @end */
  if (XS_VALUE_CONSTRAINT_FIXED == au->attribute->vc.type) {
    if ((XS_VALUE_CONSTRAINT_FIXED != au->vc.type) ||
        strcmp(au->attribute->vc.value,au->vc.value))
      return error(&s->ei,s->uri,au->defline,"au-props-correct.2","attribute reference must "
                   "contain a \"fixed\" value equal to that of the attribute it references");
  }

  return 0;
}

char *get_attribute_group_name(void *obj)
{
  return ((xs_attribute_group*)obj)->def.ident.name;
}

void add_attribute_group_references(xs_schema *s, void *obj, list **tocheck)
{
  list *l;
  xs_attribute_group *ag = (xs_attribute_group*)obj;
  for (l = ag->attribute_group_refs; l; l = l->next) {
    xs_attribute_group_ref *agr = (xs_attribute_group_ref*)l->data;
    assert(NULL != agr->ag);
    list_push(tocheck,agr->ag);
  }
}

int post_process_attribute_group(xs_schema *s, xmlDocPtr doc, xs_attribute_group *ag)
{
  /* @implements(xmlschema-1:src-attribute_group.3)
     description { Check for circular attribute group references }
     test { attributegroup_bigcircular.test }
     test { attributegroup_circular2.test }
     test { attributegroup_circular.test }
     issue { Test task 1 }
     issue { Test task 2 }
     @end */
  CHECK_CALL(check_circular_references(s,ag,"attribute group",ag->def.loc.line,NULL,
                                       "src-attribute_group.3",
                                       get_attribute_group_name,add_attribute_group_references));

  ag->post_processed = 1;

  return 0;
}

int compute_content_type(xs_schema *s, xs_type *t)
{
  /* @implements(xmlschema-1:Complex Type Definition{content type})
     test { complextype_sc_content1.test }
     test { complextype_sc_content2.test }
     test { complextype_sc_content3.test }
     test { complextype_sc_content4.test }
     test { complextype_sc_content5.test }
     test { complextype_sc_content6.test }
     test { complextype_sc_content7.test }
     test { complextype_sc_content8.test }
     test { complextype_sc_content9.test }
     test { complextype_sc_content10.test }
     test { complextype_sc_content11.test }
     test { complextype_sc_content12.test }
     test { complextype_sc_content13.test }
     test { complextype_sc_d_content6.test }
     test { complextype_sc_d_content7.test }
     test { complextype_sc_d_content8.test }
     test { complextype_sc_d_content11.test }
     test { complextype_sc_d_content12.test }
     test { complextype_sc_d_content13.test }
     @end */

  /* complex type definition with complex content:
     {content type} rule 3 - see xs_parse_complex_type_children() for rules 1 and 2 */

  if (t->computed_content_type || !t->complex || t->builtin)
    return 0;

  if (t->base != t)
    CHECK_CALL(compute_content_type(s,t->base))

  /* complex content */
  if (t->complex_content) {

    if (XS_TYPE_DERIVATION_RESTRICTION == t->derivation_method) {
      t->content_type = t->effective_content;
      t->mixed = t->effective_mixed;
    }
    else { /* XS_TYPE_DERIVATION_EXTENSION */
      if (NULL == t->effective_content) {
        t->content_type = t->base->content_type;
        t->mixed = t->base->mixed;
      }
      else if (NULL == t->base->content_type) {
        t->content_type = t->effective_content;
        t->mixed = t->effective_mixed;
      }
      else {
        xs_particle *p = xs_particle_new(s->as);
        p->range.min_occurs = 1;
        p->range.max_occurs = 1;
        p->term_type = XS_PARTICLE_TERM_MODEL_GROUP;
        p->term.mg = xs_model_group_new(s->as);
        p->term.mg->compositor = XS_MODEL_GROUP_COMPOSITOR_SEQUENCE;
        p->defline = t->def.loc.line;
        p->extension_for = t;
        list_push(&p->term.mg->particles,t->base->content_type);
        list_append(&p->term.mg->particles,t->effective_content);

        t->content_type = p;
        t->mixed = t->effective_mixed;
      }
    }
  }
  /* simple content */
  else {

    assert(t->base);

    /* @implements(xmlschema-1:src-ct.2) @end
       @implements(xmlschema-1:src-ct.2.1) @end
       @implements(xmlschema-1:src-ct.2.1.1) @end
       @implements(xmlschema-1:src-ct.2.1.2) @end
       @implements(xmlschema-1:src-ct.2.1.3) @end
       @implements(xmlschema-1:src-ct.2.2) @end
       This rule essentially essentially duplication; everything in it is covered by 3.4.6-1-2 */

    /* 3.4.2 Complex Type Definition with simple content Schema Component - {content type}  */

    if (XS_TYPE_DERIVATION_RESTRICTION == t->derivation_method) {
      /* @implements(xmlschema-1:ct-props-correct.2)
         test { complextype_sc_restriction_simpletype2.test }
         test { complextype_sc_restriction_simpletype.test }
         @end */
      if (!t->base->complex)
        return error(&s->ei,s->uri,t->def.loc.line,"ct-props-correct.2","base type is a "
                     "simple type; must use <extension> instead of <restriction>");

      /* 1 If the type definition resolved to by the actual value of the base [attribute] is a
           complex type definition whose own {content type} is a simple type definition and the
           <restriction> alternative is chosen, then starting from either */
      if (!t->base->complex_content) {
        xs_type *base;
        int defline;
        assert(NULL != t->base->simple_content_type);

        /* 1.1 the simple type definition corresponding to the <simpleType> among the [children] of
               <restriction> if there is one; */
        if (NULL != t->child_type) {
          base = t->child_type;
          defline = t->child_type->def.loc.line;
        }
        /* 1.2 otherwise (<restriction> has no <simpleType> among its [children]), the simple type
               definition which is the {content type} of the type definition resolved to by the
               actual value of the base [attribute] */
        else {
          base = t->base->simple_content_type;
          defline = t->def.loc.line;
        }
        /* a simple type definition which restricts the simple type definition identified in clause
           1.1 or clause 1.2 with a set of facet components corresponding to the appropriate element
           information items among the <restriction>'s [children] (i.e. those which specify facets,
           if any), as defined in Simple Type Restriction (Facets) (3.14.6); */
        CHECK_CALL(xs_create_simple_type_restriction(s,base,defline,&t->child_facets,
                   &t->simple_content_type))
        assert(NULL != t->simple_content_type);
      }

      /* 2 If the type definition resolved to by the actual value of the base [attribute] is a
           complex type definition whose own {content type} is mixed and a particle which is
           emptiable, as defined in Particle Emptiable (3.9.6) and the <restriction> alternative is
           chosen, then */
      else {
        if (!t->base->mixed)
          return error(&s->ei,s->uri,t->def.loc.line,"structures-3.4.2","%#n cannot be restricted "
                       "with simple content, because it does not have its \"mixed\" attribute "
                       "set to \"true\"",t->base->def.ident);

        if ((NULL != t->base->content_type) && !particle_emptiable(s,t->base->content_type))
          return error(&s->ei,s->uri,t->def.loc.line,"structures-3.4.2","%#n cannot be restricted "
                       "with simple content, because its content model requires one or more "
                       "elements to be present",t->base->def.ident);

        /* starting from the simple type definition corresponding to the <simpleType> among the
           [children] of <restriction> (which must be present) a simple type definition which
           restricts that simple type definition with a set of facet components corresponding to the
           appropriate element information items among the <restriction>'s [children] (i.e. those
           which specify facets, if any), as defined in Simple Type Restriction (Facets)
           (3.14.6); */
        if (NULL == t->child_type)
          return error(&s->ei,s->uri,t->def.loc.line,"structures-3.4.2","A complex type with "
                       "simple content that restricts a complex type with complex content must "
                       "contain a <simpleType> child");

        CHECK_CALL(xs_create_simple_type_restriction(s,t->child_type,t->child_type->def.loc.line,
                                                     &t->child_facets,&t->simple_content_type))
        assert(NULL != t->simple_content_type);
      }
    }
    else if (XS_TYPE_DERIVATION_EXTENSION == t->derivation_method) {

      /* 3 If the type definition resolved to by the actual value of the base [attribute] is a
           complex type definition (whose own {content type} must be a simple type definition, see
           below) and the <extension> alternative is chosen, then the {content type} of that
           complex type definition; */
      if (t->base->complex) {

        if (t->base->complex_content)
          return error(&s->ei,s->uri,t->def.loc.line,"structures-3.4.2","A complex type with "
                       "simple content cannot extend a complex type with complex content");

/*         printf("t = %s\n",t->name); */
/*         printf("t->base = %s\n",t->base->name); */
        assert(t->base->simple_content_type);
        t->simple_content_type = t->base->simple_content_type;
      }

      /* 4 otherwise (the type definition resolved to by the actual value of the base [attribute]
           is a simple type definition and the <extension> alternative is chosen), then that simple
           type definition. */
      else {
        t->simple_content_type = t->base;
      }
    }
  }

  t->computed_content_type = 1;
  return 0;
}

int compute_complete_wildcard(xs_schema *s, xs_wildcard *local_wildcard, list *attribute_group_refs,
                              int defline, xs_wildcard **complete_wildcard)
{
  list *l;
  xs_wildcard *w = local_wildcard;

  for (l = attribute_group_refs; l; l = l->next) {
    xs_attribute_group_ref *agr = (xs_attribute_group_ref*)l->data;
    xs_attribute_group *ag = (xs_attribute_group*)agr->ag;
    assert(NULL != ag);

    /* Make sure the referenced group has its {attribute wildcard} property computed. Note:
       this cannot cause an infinite loop because we've already checked for circular references. */
    if (!ag->computed_wildcard) {
      CHECK_CALL(compute_complete_wildcard(s,ag->local_wildcard,ag->attribute_group_refs,
                                           ag->def.loc.line,&ag->attribute_wildcard))
      ag->computed_wildcard = 1;
    }

    if (NULL != ag->local_wildcard) {
      xs_wildcard *agw = ag->attribute_wildcard;
      if (NULL == w) {
        w = agw;
      }
      else
        w = xs_wildcard_constraint_intersection(s,w,agw,w->process_contents); {

        /* @implements(xmlschema-1:src-ct.4) @end
           @implements(xmlschema-1:src-attribute_group.2) @end */
        /* FIXME: no test for this currently because there is no way to create "not" wildcards in
           different namespaces... this requires <import> functionality */
        if (NULL == w)
          return error(&s->ei,s->uri,defline,"structures-3.10.6","Attribute wildcard intersection "
                       "with the local wildcard and/or one or more attribute groups is not "
                       "expressible, because two wildcards are negations of different namespaces");

      }
      /* FIXME: the spec gives additional instructions on how to handle annotations when merging
         wildcards. When annotations are supported, these should be implemented. */
    }
  }

  *complete_wildcard = w;
  return 0;
}

int compute_attribute_uses(xs_schema *s, list *local_attribute_uses, list *attribute_group_refs,
                           list **attribute_uses)
{
  list *l;
  list *l2;

  assert(NULL == *attribute_uses);

  /* Compute the {attribute uses} property of attribute groups, and for complex types
     (step 1 and 2 only) */

  /* local attribute uses */
  for (l2 = local_attribute_uses; l2; l2 = l2->next) {
    xs_attribute_use *au = (xs_attribute_use*)l2->data;
    if (!au->prohibited)
      list_append(attribute_uses,au);
  }

  /* attribute references in referenced attribute groups */
  for (l = attribute_group_refs; l; l = l->next) {
    xs_attribute_group_ref *agr = (xs_attribute_group_ref*)l->data;
    xs_attribute_group *ag = (xs_attribute_group*)agr->ag;
    assert(NULL != ag);

    /* Make sure the referenced group has its {attribute uses} property computed. Note:
       this cannot cause an infinite loop because we've already checked for circular references. */
    if (!ag->computed_attribute_uses) {
      CHECK_CALL(compute_attribute_uses(s,ag->local_attribute_uses,ag->attribute_group_refs,
                                        &ag->attribute_uses))
      ag->computed_attribute_uses = 1;
    }

    for (l2 = ag->attribute_uses; l2; l2 = l2->next) {
      xs_attribute_use *au = (xs_attribute_use*)l2->data;
      if (!au->prohibited)
        list_append(attribute_uses,au);
    }
  }

  return 0;
}


int compute_attribute_uses_type(xs_schema *s, xs_type *t)
{
  list *l;
  list *l2;

  /* Steps 1 and 2 */
  CHECK_CALL(compute_attribute_uses(s,t->local_attribute_uses,t->attribute_group_refs,
                                    &t->attribute_uses))
  /* Step 3 */

  /* Note: no infinite loop here as circuler base references already checked for */
  if (t->base && !t->base->computed_attribute_uses && (t->base != t))
    CHECK_CALL(compute_attribute_uses_type(s,t->base))

  if (t->complex_content || (t->base && t->base->complex)) {
    for (l = t->base->attribute_uses; l; l = l->next) {
      xs_attribute_use *au = (xs_attribute_use*)l->data;
      int add = 1;

      if (XS_TYPE_DERIVATION_RESTRICTION == t->derivation_method) {
        /* is this attribute already declared in the local attribute uses or the attribute uses of
           a referenced attribute group? */
        for (l2 = t->attribute_uses; l2; l2 = l2->next) {
          xs_attribute_use *au2 = (xs_attribute_use*)l2->data;
          if (nsname_equals(au->attribute->def.ident,au2->attribute->def.ident))
            add = 0;
        }

        /* is this attribute declared in the local attribute uses as "prohibited"? */
        for (l2 = t->local_attribute_uses; l2; l2 = l2->next) {
          xs_attribute_use *au2 = (xs_attribute_use*)l2->data;
          if (nsname_equals(au->attribute->def.ident,au2->attribute->def.ident) &&
              au2->prohibited)
            add = 0;
        }
      }

      if (add)
        list_append(&t->attribute_uses,au);
    }
  }

  t->computed_attribute_uses = 1;

  return 0;
}

int compute_attribute_wildcard(xs_schema *s, xs_type *t)
{
  /* @implements(xmlschema-1:Complex Type Definition{attribute wildcard})
     test { complextype_cc_wildcard1.test }
     test { complextype_cc_wildcard2.test }
     test { complextype_cc_wildcard3.test }
     test { complextype_cc_wildcard4.test }
     test { complextype_cc_wildcard5.test }
     test { complextype_cc_wildcard6.test }
     test { complextype_cc_wildcard7.test }
     test { complextype_cc_wildcard8.test }
     test { complextype_cc_wildcard9.test }
     test { complextype_cc_wildcard10.test }
     test { complextype_cc_wildcard11.test }
     test { complextype_sc_wildcard1.test }
     @end
  */

  /* Note: no infinite loop here as circuler base references already checked for */
  if (t->base && !t->base->computed_wildcard && (t->base != t))
    CHECK_CALL(compute_attribute_wildcard(s,t->base))

  CHECK_CALL(compute_complete_wildcard(s,t->local_wildcard,t->attribute_group_refs,
                                       t->def.loc.line,&t->attribute_wildcard))

  if ((XS_TYPE_DERIVATION_EXTENSION == t->derivation_method) &&
      t->base && (t->base != t) && t->base->complex && t->base->attribute_wildcard) {
    if (t->attribute_wildcard) {
        t->attribute_wildcard =
          xs_wildcard_constraint_union(s,t->attribute_wildcard,t->base->attribute_wildcard,
                                       t->attribute_wildcard->process_contents);

      if (NULL == t->attribute_wildcard)
        return error(&s->ei,s->uri,t->def.loc.line,"structures-3.10.6","Union of local attribute "
                     "wildcard and base type's attribute wildcard is not expressible, because one "
                     "is a negation and the other is a set of namespaces including ##local but not "
                     "##targetNamespace");
    }
    else
       t->attribute_wildcard = t->base->attribute_wildcard;
  }

  t->computed_wildcard = 1;
  return 0;
}

char* get_type_name(void *obj)
{
  return ((xs_type*)obj)->def.ident.name;
}

void add_type_references(xs_schema *s, void *obj, list **tocheck)
{
  xs_type *t = (xs_type*)obj;
  if (t != s->globals->complex_ur_type)
    list_push(tocheck,t->base);
}

void add_type_union_members(xs_schema *s, void *obj, list **tocheck)
{
  xs_type *t = (xs_type*)obj;
  list *l;
  for (l = t->members; l; l = l->next) {
    xs_member_type *mt = (xs_member_type*)l->data;
    assert(mt->type);
    assert(mt->type->complex || (XS_TYPE_VARIETY_INVALID != mt->type->variety));
    if (!mt->type->complex && (XS_TYPE_VARIETY_UNION == mt->type->variety))
      list_push(tocheck,mt->type);
  }
}

int post_process_particle(xs_schema *s, xmlDocPtr doc, xs_particle *p)
{
  /* @implements(xmlschema-1:p-props-correct.1) @end */

  if (0 <= p->range.max_occurs) {
    /* @implements(xmlschema-1:p-props-correct.2) @end */

    /* @implements(xmlschema-1:p-props-correct.2.1)
       test { all_element_maxoccurs0.test }
       test { any_maxoccurs0.test }
       test { choice_maxoccurs0.test }
       test { element_maxoccurs0.test }
       test { groupref_maxoccurs0.test }
       test { sequence_maxoccurs0.test }
       @end */
    if (0 == p->range.max_occurs)
      return error(&s->ei,s->uri,p->defline,"p-props-correct.2.1",
                   "if maxOccurs has a finite value, it must be greater than or equal to 1");
    /* @implements(xmlschema-1:p-props-correct.2.2)
       test { any_minoccursgtmaxoccurs.test }
       test { choice_minoccursgtmaxoccurs.test }
       test { element_minoccursgtmaxoccurs.test }
       test { groupref_minoccursgtmaxoccurs.test }
       test { sequence_minoccursgtmaxoccurs.test }
       @end */
    if (p->range.min_occurs > p->range.max_occurs)
      return error(&s->ei,s->uri,p->defline,"p-props-correct.2.2",
                   "minOccurs must not be greater than maxOccurs");
  }

  return 0;
}

int post_process_type1(xs_schema *s, xmlDocPtr doc, xs_type *t)
{

  assert(t->base);

  /* @implements(xmlschema-1:src-ct.1)
     test { complextype_cc_extension_simpletype.test }
     test { complextype_cc_extension_simpletype2.test }
     test { complextype_cc_restriction_simpletype.test }
     test { complextype_cc_restriction_simpletype2.test }
     @end */
  if (t->complex && t->complex_content && !t->base->complex)
    return error(&s->ei,s->uri,t->def.loc.line,"src-ct.1",
                 "complex type with complex content cannot extend or restrict a simple type");

  /* @implements(xmlschema-1:ct-props-correct.3)
     description { Check for circular base type references }
     test { complextype_cc_extension_all_circular2.test }
     test { complextype_cc_extension_all_circular.test }
     test { complextype_cc_extension_bigcircular.test }
     test { complextype_cc_extension_choice_circular2.test }
     test { complextype_cc_extension_choice_circular.test }
     test { complextype_cc_extension_sequence_circular2.test }
     test { complextype_cc_extension_sequence_circular.test }
     @end */
  CHECK_CALL(check_circular_references(s,t,"type",t->def.loc.line,NULL,NULL,
                                       get_type_name,add_type_references));

  if (t->restriction) {
    xs_type *b = t->base;
    while (b->restriction)
      b = b->base;
    t->variety = b->variety;
    if (XS_TYPE_VARIETY_LIST == t->variety) {
      assert(b->item_type);
      t->item_type = b->item_type;
      /* FIXME: do the same for member types? */
    }
  }

  if (!t->complex) {
    xs_type *b;
    int found = 0;
    if (t->primitive)
      t->primitive_type = t;
    for (b = t->base; b && b->base != b; b = b->base) {
      if (b == s->globals->simple_ur_type)
        found = 1;
      if (b->primitive && (NULL == t->primitive_type))
        t->primitive_type = b;
    }

    /* @implements(xmlschema-1:st-props-correct.2)
       test { simpletype_union_bigcircular.test }
       test { simpletype_union_circular1.test }
       test { simpletype_union_circular2.test }
       test { simpletype_restriction_derivation_anytype.test }
       test { simpletype_restriction_derivation_complex.test }
       @end */
    if (!found && (t != s->globals->simple_ur_type))
      return error(&s->ei,s->uri,t->def.loc.line,"st-props-correct.2",
                   "Simple type must ultimately be derived from %#n",
                   s->globals->simple_ur_type->def.ident);
  }

  return 0;
}

int post_process_type2(xs_schema *s, xmlDocPtr doc, xs_type *t)
{
  CHECK_CALL(compute_content_type(s,t))

  if (!t->complex) {
    /* Make sure that an atomic type is derived from a primitive type... section 3.14.1 states:

         The simple ur-type definition must not be named as the base type definition of any
         user-defined atomic simple type definitions: as it has no constraining facets, this would
         be incoherent.

       This is also necessary so that we can ensure a non-NULL value for the
       {primitive type definition} property.

       Note that we make an exception for the case where we are parsing the schema for XML Schema
       itself (defined in the spec). This is necessary because some of the definitions violate the
       above rule (by necessity) when defining the built-in simple types. */

    if ((t == s->globals->simple_ur_type) ||
        ((NULL != t->def.ident.ns) && !strcmp(t->def.ident.ns,XS_NAMESPACE) &&
        (t->base == s->globals->simple_ur_type)))
      t->primitive_type = s->globals->simple_ur_type;

    if ((XS_TYPE_VARIETY_ATOMIC == t->variety) && (NULL == t->primitive_type)) {
      return error(&s->ei,s->uri,t->def.loc.line,"structures-3.14.6",
                   "An atomic type must be ultimately derived from a built-in primitive type");
    }

    /* @implements(xmlschema-1:st-props-correct.3)
       test { simpletype_restriction_final.test }
       test { simpletype_restriction_final2.test }
       test { simpletype_restriction_final3.test }
       test { simpletype_restriction_final4.test }
       test { simpletype_list_base_final_restriction.test }
       test { simpletype_list_base_final_restriction2.test }
       test { simpletype_list_base_final_restriction3.test }
       test { simpletype_list_base_final_restriction4.test }
       test { simpletype_union_base_final_restriction.test }
       test { simpletype_union_base_final_restriction2.test }
       test { simpletype_union_base_final_restriction3.test }
       test { simpletype_union_base_final_restriction4.test }
       @end */
    if (t->base->final_restriction)
      return error(&s->ei,s->uri,t->def.loc.line,"st-props-correct.3",
                   "Base type disallows restriction");

    /* rule 4 - actually marked as "deleted" in the spec file, not sure how to hide it in table
       @implements(xmlschema-1:st-props-correct.4) @end
       @implements(xmlschema-1:st-props-correct.4.1) @end
       @implements(xmlschema-1:st-props-correct.4.2) @end
       @implements(xmlschema-1:st-props-correct.4.2.1) @end
       @implements(xmlschema-1:st-props-correct.4.2.2) @end */

    /* @implements(xmlschema-1:src-simple-type.4)
       test { simpletype_union_circular1.test }
       test { simpletype_union_circular2.test }
       test { simpletype_union_bigcircular.test }
       @end */
    CHECK_CALL(check_circular_references(s,t,"union",t->def.loc.line,NULL,"src-simple-type.4",
                                         get_type_name,add_type_union_members));

    CHECK_CALL(xs_check_simple_type_derivation_valid(s,t))
  }

  return 0;
}

int xs_post_process_schema(xs_schema *s, xmlDocPtr doc)
{
  list *l;
  /* resolve references */
  for (l = s->as->alloc_reference; l; l = l->next) {
    xs_reference *r = (xs_reference*)l->data;
    if (!r->builtin)
      CHECK_CALL(xs_resolve_ref(s,r))
  }
  /* perform post-resolution processing */
  /* note: post_process_type1() and post_process_element2() are done first for all types and
     elements, because these check for circular references. The other post-processing functions
     rely on these checks being done to avoid infinite recursion. */
  for (l = s->as->alloc_type; l; l = l->next)
    CHECK_CALL(post_process_type1(s,doc,(xs_type*)l->data))
  for (l = s->as->alloc_element; l; l = l->next)
    CHECK_CALL(post_process_element1(s,doc,(xs_element*)l->data))

  /* now we compute the types for all elements; this must also be done before other post-processing
     occurs */

  for (l = s->as->alloc_element; l; l = l->next) {
    xs_element *e = (xs_element*)l->data;
    if (!e->computed_type)
      compute_element_type(s,e);
  }


  for (l = s->as->alloc_type; l; l = l->next)
    CHECK_CALL(post_process_type2(s,doc,(xs_type*)l->data))
  for (l = s->as->alloc_particle; l; l = l->next)
    CHECK_CALL(post_process_particle(s,doc,(xs_particle*)l->data))
  for (l = s->as->alloc_model_group_def; l; l = l->next)
    CHECK_CALL(post_process_model_group_def(s,doc,(xs_model_group_def*)l->data))
  for (l = s->as->alloc_model_group; l; l = l->next)
    CHECK_CALL(post_process_model_group(s,doc,(xs_model_group*)l->data))
  for (l = s->as->alloc_element; l; l = l->next)
    CHECK_CALL(post_process_element2(s,doc,(xs_element*)l->data))
  for (l = s->as->alloc_attribute; l; l = l->next)
    CHECK_CALL(post_process_attribute(s,doc,(xs_attribute*)l->data))
  for (l = s->as->alloc_attribute_use; l; l = l->next)
    CHECK_CALL(post_process_attribute_use(s,doc,(xs_attribute_use*)l->data))
  for (l = s->as->alloc_attribute_group; l; l = l->next)
    CHECK_CALL(post_process_attribute_group(s,doc,(xs_attribute_group*)l->data))

  /* compute attribute wildcards - note this must be done after post processing to ensure
     that circular references have been checked for */
  for (l = s->as->alloc_attribute_group; l; l = l->next) {
    xs_attribute_group *ag = (xs_attribute_group*)l->data;
    if (!ag->computed_wildcard) {
      CHECK_CALL(compute_complete_wildcard(s,ag->local_wildcard,ag->attribute_group_refs,
                                           ag->def.loc.line,&ag->attribute_wildcard))
      ag->computed_wildcard = 1;
    }
    if (!ag->computed_attribute_uses) {
      CHECK_CALL(compute_attribute_uses(s,ag->local_attribute_uses,ag->attribute_group_refs,
                                        &ag->attribute_uses))
      ag->computed_attribute_uses = 1;
    }
  }
  for (l = s->as->alloc_type; l; l = l->next) {
    xs_type *t = (xs_type*)l->data;
    if (!t->computed_wildcard)
      CHECK_CALL(compute_attribute_wildcard(s,t))
    if (!t->computed_attribute_uses)
      CHECK_CALL(compute_attribute_uses_type(s,t))
  }

  /* Check the validity of {attribute uses} for complex types and attribute groups. This must
     be done here because the value of this property was only just computed in the code above. */
  for (l = s->as->alloc_attribute_group; l; l = l->next)
    CHECK_CALL(check_attribute_uses(s,((xs_attribute_group*)l->data)->attribute_uses,
                                    "ag-props-correct.2","ag-props-correct.3"))
  for (l = s->as->alloc_type; l; l = l->next)
    CHECK_CALL(check_attribute_uses(s,((xs_type*)l->data)->attribute_uses,
                                    "ct-props-correct.4","ct-props-correct.5"))

  /* Check that derivation or extension is valid for all types. Note that we do this here, because
     it depends on the attribute uses having been computed above. */
  for (l = s->as->alloc_type; l; l = l->next) {
    xs_type *t = (xs_type*)l->data;
    if (XS_TYPE_DERIVATION_EXTENSION == t->derivation_method) {
      CHECK_CALL(xs_check_complex_extension(s,t))
    }
    else {
      /* XS_TYPE_DERIVATION_RESTRICTION == t->derivation_method */
      CHECK_CALL(xs_check_complex_restriction(s,t))
    }
  }

  return 0;
}

int parse_xmlschema_element(xmlNodePtr n, xmlDocPtr doc, const char *uri, const char *sourceloc,
                            xs_schema **sout, error_info *ei, xs_globals *g)
{
  xs_schema *s = xs_schema_new(g);
  s->uri = strdup(uri);

  if ((0 == xs_parse_schema(s,n,doc)) &&
      (0 == xs_post_process_schema(s,doc))) {
    *sout = s;
    return 0;
  }
  else {
    error_info_copy(ei,&s->ei);
    xs_schema_free(s);
    return -1;
  }
}

xs_schema *parse_xmlschema_file(char *filename, xs_globals *g)
{
  xmlDocPtr doc;
  xmlNodePtr root;
  xs_schema *s = NULL;
  error_info ei;
  FILE *f;
  char *uri;

  memset(&ei,0,sizeof(error_info));

  if (NULL == (f = fopen(filename,"r"))) {
    fprintf(stderr,"Can't open %s: %s\n",filename,strerror(errno));
    return NULL;
  }

  if (NULL == (doc = xmlReadFd(fileno(f),NULL,NULL,0))) {
    fclose(f);
    fprintf(stderr,"XML parse error.\n");
    return NULL;
  }
  fclose(f);

  if (NULL == (root = xmlDocGetRootElement(doc))) {
    fprintf(stderr,"No root element.\n");
    xmlFreeDoc(doc);
    return NULL;
  }

  if (!check_element(root,"schema",XS_NAMESPACE)) {
    if (root->ns)
      fprintf(stderr,"Invalid element at root: {%s}%s\n",root->ns->href,root->name);
    else
      fprintf(stderr,"Invalid element at root: %s\n",root->name);
    xmlFreeDoc(doc);
    return NULL;
  }

  uri = get_real_uri(filename);
  if (0 != parse_xmlschema_element(root,doc,uri,filename,&s,&ei,g)) {
    error_info_print(&ei,stderr);
    error_info_free_vals(&ei);
    xmlFreeDoc(doc);
    free(uri);
    return NULL;
  }
  free(uri);

  xmlFreeDoc(doc);
  return s;
}

int xs_visit_element(xs_schema *s, xmlDocPtr doc, void *data, xs_element *e, xs_visitor *v)
{
  if (e->type &&
      !(e->typeref && v->once_each)) {

    /* If we're in "once each" mode, we only want to visit the type if it is not a type that
       has been assigned to the element by default (i.e. because no type was explicitly specified
       by the element). See compute_element_type(), maybe_output_element_typeref() and
       element_type4.test */
    if (!v->once_each ||
        ((NULL == e->sghead) && (e->type != s->globals->complex_ur_type)) ||
        ((NULL != e->sghead) && (e->type != e->sghead->type))) {

      if (v->visit_type)
        CHECK_CALL(v->visit_type(s,doc,data,0,e->type))
      CHECK_CALL(xs_visit_type(s,doc,data,e->type,v))
      if (v->visit_type)
        CHECK_CALL(v->visit_type(s,doc,data,1,e->type))
    }
  }

  return 0;
}

int xs_visit_wildcard(xs_schema *s, xmlDocPtr doc, void *data, xs_wildcard *e, xs_visitor *v)
{
  return 0;
}

int xs_visit_model_group(xs_schema *s, xmlDocPtr doc, void *data, xs_model_group *mg, xs_visitor *v)
{
  list *l;
  for (l = mg->particles; l; l = l->next) {
    xs_particle *p2 = (xs_particle*)l->data;
    if (v->visit_particle)
      CHECK_CALL(v->visit_particle(s,doc,data,0,p2))
    CHECK_CALL(xs_visit_particle(s,doc,data,p2,v))
    if (v->visit_particle)
      CHECK_CALL(v->visit_particle(s,doc,data,1,p2))
  }
  return 0;
}

int xs_visit_particle(xs_schema *s, xmlDocPtr doc, void *data, xs_particle *p, xs_visitor *v)
{
  if (XS_PARTICLE_TERM_ELEMENT == p->term_type) {
    if (NULL == p->term.e) { /* may not be resolved yet */
      assert(p->ref && !p->ref->resolved);
      return 0;
    }
    if (p->ref && v->once_each)
      return 0;
    if (v->visit_element)
      CHECK_CALL(v->visit_element(s,doc,data,0,p->term.e))
    CHECK_CALL(xs_visit_element(s,doc,data,p->term.e,v))
    if (v->visit_element)
      CHECK_CALL(v->visit_element(s,doc,data,1,p->term.e))
  }
  else if (XS_PARTICLE_TERM_MODEL_GROUP == p->term_type) {
    if (NULL == p->term.mg) /* may not be resolved yet */
      return 0;
    if (p->ref && v->once_each)
      return 0;
    if (v->visit_model_group)
      CHECK_CALL(v->visit_model_group(s,doc,data,0,p->term.mg))
    CHECK_CALL(xs_visit_model_group(s,doc,data,p->term.mg,v))
    if (v->visit_model_group)
      CHECK_CALL(v->visit_model_group(s,doc,data,1,p->term.mg))

  }
  else {
    assert(XS_PARTICLE_TERM_WILDCARD == p->term_type);
    if (v->visit_wildcard)
      CHECK_CALL(v->visit_wildcard(s,doc,data,0,p->term.w))
    CHECK_CALL(xs_visit_wildcard(s,doc,data,p->term.w,v))
    if (v->visit_wildcard)
      CHECK_CALL(v->visit_wildcard(s,doc,data,1,p->term.w))
  }

  return 0;
}

int xs_visit_type(xs_schema *s, xmlDocPtr doc, void *data, xs_type *t, xs_visitor *v)
{
  xs_particle *p = t->content_type;
  list *l;

  if (t->builtin)
    return 0;

  /* first visit the base type if necessary */
  if (t->base &&
      (t->base != t) && /* ur-type */
      !(t->baseref && v->once_each)) {
    if (v->visit_type)
      CHECK_CALL(v->visit_type(s,doc,data,0,t->base))
    CHECK_CALL(xs_visit_type(s,doc,data,t->base,v))
    if (v->visit_type)
      CHECK_CALL(v->visit_type(s,doc,data,1,t->base))
  }

  /* now the item type (if we're a list simple type) */
  if (t->item_type && ((NULL == t->item_typeref) || !v->once_each)) {
    if (v->visit_type)
      CHECK_CALL(v->visit_type(s,doc,data,0,t->item_type))
    CHECK_CALL(xs_visit_type(s,doc,data,t->item_type,v))
    if (v->visit_type)
      CHECK_CALL(v->visit_type(s,doc,data,1,t->item_type))
  }

  /* now member types (if we're a union simple type) */
  for (l = t->members; l; l = l->next) {
    xs_member_type *mt = (xs_member_type*)l->data;
    if (mt->type && ((NULL == mt->ref) || !v->once_each)) {
      if (v->visit_type)
        CHECK_CALL(v->visit_type(s,doc,data,0,mt->type))
      CHECK_CALL(xs_visit_type(s,doc,data,mt->type,v))
      if (v->visit_type)
        CHECK_CALL(v->visit_type(s,doc,data,1,mt->type))
    }
  }

  /* now the content */

  /* if the "just effective content" flag is set, we do not recurse through the type's _actual_
     content, since it may reference types we've already seen */
  if (v->once_each || !p)
    p = t->effective_content;

  if (p) {
    if (v->visit_particle)
      CHECK_CALL(v->visit_particle(s,doc,data,0,p))
    CHECK_CALL(xs_visit_particle(s,doc,data,p,v))
    if (v->visit_particle)
      CHECK_CALL(v->visit_particle(s,doc,data,1,p))
  }

  if (t->child_type) {
    if (v->visit_type)
      CHECK_CALL(v->visit_type(s,doc,data,0,t->child_type))
    CHECK_CALL(xs_visit_type(s,doc,data,t->child_type,v))
    if (v->visit_type)
      CHECK_CALL(v->visit_type(s,doc,data,1,t->child_type))
  }

  if (t->simple_content_type && !v->once_each) {
    if (v->visit_type)
      CHECK_CALL(v->visit_type(s,doc,data,0,t->simple_content_type))
    CHECK_CALL(xs_visit_type(s,doc,data,t->simple_content_type,v))
    if (v->visit_type)
      CHECK_CALL(v->visit_type(s,doc,data,1,t->simple_content_type))
  }

  /* FIXME: should we also visit simple_content_type? (probably only relevant for dumping) */

  /* attribute uses */
  if (v->once_each)
    l = t->local_attribute_uses;
  else
    l = t->attribute_uses;
  for (; l; l = l->next) {
    xs_attribute_use *au = (xs_attribute_use*)l->data;
    if (v->visit_attribute_use)
      CHECK_CALL(v->visit_attribute_use(s,doc,data,0,au))
    CHECK_CALL(xs_visit_attribute_use(s,doc,data,au,v))
    if (v->visit_attribute_use)
      CHECK_CALL(v->visit_attribute_use(s,doc,data,1,au))
  }

  /* attribute group references */
  for (l = t->attribute_group_refs; l; l = l->next) {
    xs_attribute_group_ref *agr = (xs_attribute_group_ref*)l->data;
    if (v->visit_attribute_group_ref)
      CHECK_CALL(v->visit_attribute_group_ref(s,doc,data,0,agr))
    CHECK_CALL(xs_visit_attribute_group_ref(s,doc,data,agr,v))
    if (v->visit_attribute_group_ref)
      CHECK_CALL(v->visit_attribute_group_ref(s,doc,data,1,agr))
  }

  return 0;
}

int xs_visit_model_group_def(xs_schema *s, xmlDocPtr doc, void *data, xs_model_group_def *mgd,
                             xs_visitor *v)
{
  list *l;

  for (l = mgd->model_group->particles; l; l = l->next) {
    xs_particle *p = (xs_particle*)l->data;
    if (v->visit_particle)
      CHECK_CALL(v->visit_particle(s,doc,data,0,p))
    CHECK_CALL(xs_visit_particle(s,doc,data,p,v))
    if (v->visit_particle)
      CHECK_CALL(v->visit_particle(s,doc,data,1,p))
  }

  return 0;
}

int xs_visit_attribute_group_ref(xs_schema *s, xmlDocPtr doc, void *data,
                                 xs_attribute_group_ref *agr, xs_visitor *v)
{
  if (!v->once_each) {
    xs_attribute_group *ag = (xs_attribute_group*)agr->ag;
    if (v->visit_attribute_group)
      CHECK_CALL(v->visit_attribute_group(s,doc,data,0,ag))
    CHECK_CALL(xs_visit_attribute_group(s,doc,data,ag,v))
    if (v->visit_attribute_group)
      CHECK_CALL(v->visit_attribute_group(s,doc,data,1,ag))
  }
  return 0;
}

int xs_visit_attribute_group(xs_schema *s, xmlDocPtr doc, void *data, xs_attribute_group *ag,
                             xs_visitor *v)
{
  list *l;

  /* attribute uses */
  if (v->once_each)
    l = ag->local_attribute_uses;
  else
    l = ag->attribute_uses;
  for (; l; l = l->next) {
    xs_attribute_use *au = (xs_attribute_use*)l->data;
    if (v->visit_attribute_use)
      CHECK_CALL(v->visit_attribute_use(s,doc,data,0,au))
    CHECK_CALL(xs_visit_attribute_use(s,doc,data,au,v))
    if (v->visit_attribute_use)
      CHECK_CALL(v->visit_attribute_use(s,doc,data,1,au))
  }

  /* attribute group references */
  for (l = ag->attribute_group_refs; l; l = l->next) {
    xs_attribute_group_ref *agr = (xs_attribute_group_ref*)l->data;
    if (v->visit_attribute_group_ref)
      CHECK_CALL(v->visit_attribute_group_ref(s,doc,data,0,agr))
    CHECK_CALL(xs_visit_attribute_group_ref(s,doc,data,agr,v))
    if (v->visit_attribute_group_ref)
      CHECK_CALL(v->visit_attribute_group_ref(s,doc,data,1,agr))
  }
  return 0;
}

int xs_visit_attribute(xs_schema *s, xmlDocPtr doc, void *data, xs_attribute *a, xs_visitor *v)
{
  if (a->type &&
      !(a->typeref && v->once_each)) {
    if (v->visit_type)
      CHECK_CALL(v->visit_type(s,doc,data,0,a->type))
    CHECK_CALL(xs_visit_type(s,doc,data,a->type,v))
    if (v->visit_type)
      CHECK_CALL(v->visit_type(s,doc,data,1,a->type))
  }

  return 0;
}

int xs_visit_attribute_use(xs_schema *s, xmlDocPtr doc, void *data, xs_attribute_use *au,
                           xs_visitor *v)
{
  if (au->attribute &&
      !(au->ref && v->once_each)) {
    if (v->visit_attribute)
      CHECK_CALL(v->visit_attribute(s,doc,data,0,au->attribute))
    CHECK_CALL(xs_visit_attribute(s,doc,data,au->attribute,v))
    if (v->visit_attribute)
      CHECK_CALL(v->visit_attribute(s,doc,data,1,au->attribute))
  }

  return 0;
}

int xs_visit_schema(xs_schema *s, xmlDocPtr doc, void *data, xs_visitor *v)
{
  symbol_space_entry *sse;

  if (v->visit_schema)
    CHECK_CALL(v->visit_schema(s,doc,data,0,s))

  for (sse = s->symt->ss_types->entries; sse; sse = sse->next) {
    xs_type *t = (xs_type*)sse->object;
    if (v->visit_type)
      CHECK_CALL(v->visit_type(s,doc,data,0,t))
    CHECK_CALL(xs_visit_type(s,doc,data,t,v))
    if (v->visit_type)
      CHECK_CALL(v->visit_type(s,doc,data,1,t))
  }

  for (sse = s->symt->ss_elements->entries; sse; sse = sse->next) {
    if (v->visit_element)
      CHECK_CALL(v->visit_element(s,doc,data,0,(xs_element*)sse->object))
    CHECK_CALL(xs_visit_element(s,doc,data,(xs_element*)sse->object,v))
    if (v->visit_element)
      CHECK_CALL(v->visit_element(s,doc,data,1,(xs_element*)sse->object))
  }

  for (sse = s->symt->ss_attributes->entries; sse; sse = sse->next) {
    if (v->visit_attribute)
      CHECK_CALL(v->visit_attribute(s,doc,data,0,(xs_attribute*)sse->object))
    CHECK_CALL(xs_visit_attribute(s,doc,data,(xs_attribute*)sse->object,v))
    if (v->visit_attribute)
      CHECK_CALL(v->visit_attribute(s,doc,data,1,(xs_attribute*)sse->object))
  }

  for (sse = s->symt->ss_model_group_defs->entries; sse; sse = sse->next) {
    if (v->visit_model_group_def)
      CHECK_CALL(v->visit_model_group_def(s,doc,data,0,(xs_model_group_def*)sse->object))
    CHECK_CALL(xs_visit_model_group_def(s,doc,data,(xs_model_group_def*)sse->object,v))
    if (v->visit_model_group_def)
      CHECK_CALL(v->visit_model_group_def(s,doc,data,1,(xs_model_group_def*)sse->object))
  }

  for (sse = s->symt->ss_attribute_groups->entries; sse; sse = sse->next) {
    if (v->visit_attribute_group)
      CHECK_CALL(v->visit_attribute_group(s,doc,data,0,(xs_attribute_group*)sse->object))
    CHECK_CALL(xs_visit_attribute_group(s,doc,data,(xs_attribute_group*)sse->object,v))
    if (v->visit_attribute_group)
      CHECK_CALL(v->visit_attribute_group(s,doc,data,1,(xs_attribute_group*)sse->object))
  }

  if (v->visit_schema)
    CHECK_CALL(v->visit_schema(s,doc,data,1,s))

  return 0;
}

xs_type *new_primitive_type(xs_globals *g, char *name, int size, char *ctype)
{
  int i;
  int found_allowed_facets = 0;
  xs_type *t = xs_type_new(g->as);
  t->def.ident.name = strdup(name);
  t->def.ident.ns = strdup(XS_NAMESPACE);
  t->primitive = 1;
  t->builtin = 1;
  t->variety = XS_TYPE_VARIETY_ATOMIC;
  t->stype = XS_TYPE_SIMPLE_BUILTIN;
  assert(g->any_atomic_type);
  t->base = g->any_atomic_type;

  t->typeinfo_known = 1;
  t->size = size;
  if (NULL != ctype) {
    t->ctype = strdup(ctype);
    t->custom_ctype = 1;
  }
  else if (t->base->custom_ctype) {
    t->ctype = strdup(t->base->ctype);
    t->custom_ctype = 1;
  }
  else {
    t->ctype = (char*)malloc(strlen("XS")+strlen(name)+1);
    sprintf(t->ctype,"XS%s",name);
  }

  for (i = 0; xs_allowed_facets[i].typename; i++) {
    if (!strcmp(t->def.ident.name,xs_allowed_facets[i].typename)) {
      t->allowed_facets = xs_allowed_facets[i].facets;
      found_allowed_facets = 1;
      break;
    }
  }
  assert(found_allowed_facets);

  ss_add(g->symt->ss_types,nsname_temp(XS_NAMESPACE,name),t);
  return t;
}

xs_type *new_derived_type(xs_globals *g, char *name, char *base, int size, char *ctype)
{
  xs_type *t = xs_type_new(g->as);
  t->def.ident.name = strdup(name);
  t->def.ident.ns = strdup(XS_NAMESPACE);
  t->builtin = 1;
  t->variety = XS_TYPE_VARIETY_ATOMIC;
  t->stype = XS_TYPE_SIMPLE_RESTRICTION;
  t->base = ss_lookup_local(g->symt->ss_types,nsname_temp(XS_NAMESPACE,base));
  if (!t->base) {
    printf("no such base type %s for %s\n",base,name);
    assert(0);
  }

  t->typeinfo_known = 1;
  assert(t->base->typeinfo_known);
  t->size = (0 <= size) ? size : t->base->size;

  if (NULL != ctype) {
    t->ctype = strdup(ctype);
    t->custom_ctype = 1;
  }
  else if (t->base->custom_ctype) {
    t->ctype = strdup(t->base->ctype);
    t->custom_ctype = 1;
  }
  else {
    t->ctype = (char*)malloc(strlen("XS")+strlen(name)+1);
    sprintf(t->ctype,"XS%s",name);
  }

  t->baseref = xs_reference_new(g->as);
  t->baseref->def.ident.ns = strdup(name);
  t->baseref->def.ident.name = strdup(XS_NAMESPACE);
  t->baseref->def.loc.line = -1;
  t->baseref->type = XS_OBJECT_TYPE;
  t->baseref->obj = (void**)&t->base;
  t->baseref->builtin = 1;
  
  assert(t->base);
  ss_add(g->symt->ss_types,nsname_temp(XS_NAMESPACE,name),t);
  return t;
}

xs_type *new_list_type(xs_globals *g, char *name, char *item_type)
{
  xs_type *t = xs_type_new(g->as);
  t->def.ident.name = strdup(name);
  t->def.ident.ns = strdup(XS_NAMESPACE);
  t->builtin = 1;
  t->variety = XS_TYPE_VARIETY_LIST;
  t->stype = XS_TYPE_SIMPLE_LIST;
  t->base = g->simple_ur_type;
  t->item_type = ss_lookup_local(g->symt->ss_types,nsname_temp(XS_NAMESPACE,item_type));
  if (!t->item_type) {
    printf("no such item type %s for %s\n",item_type,name);
    assert(0);
  }

  /* FIXME: find a better way to handle encoding of list (and union?) simpleTypes... for now
     we just encode them as a string */
  t->typeinfo_known = 1;
  t->size = sizeof(IMPL_POINTER);
  t->ctype = (char*)malloc(strlen("XS")+strlen(name)+1);
  sprintf(t->ctype,"XS%s",name);

  t->baseref = xs_reference_new(g->as);
  t->baseref->def.ident.ns = strdup(t->base->def.ident.ns);
  t->baseref->def.ident.name = strdup(t->base->def.ident.name);
  t->baseref->def.loc.line = -1;
  t->baseref->type = XS_OBJECT_TYPE;
  t->baseref->obj = (void**)&t->base;
  t->baseref->builtin = 1;

  t->item_typeref = xs_reference_new(g->as);
  t->item_typeref->def.ident.ns = strdup(t->item_type->def.ident.ns);
  t->item_typeref->def.ident.name = strdup(t->item_type->def.ident.name);
  t->item_typeref->def.loc.line = -1;
  t->item_typeref->type = XS_OBJECT_TYPE;
  t->item_typeref->obj = (void**)&t->base;
  t->item_typeref->builtin = 1;
  
  assert(t->base);
  ss_add(g->symt->ss_types,nsname_temp(XS_NAMESPACE,name),t);
  return t;
}

void *xs_symbol_table_lookup_object(xs_symbol_table *symt, int type, const nsname ident)
{
  symbol_space *ss = NULL;
  switch (type) {
  case XS_OBJECT_TYPE:
    ss = symt->ss_types;
    break;
  case XS_OBJECT_ATTRIBUTE:
    ss = symt->ss_attributes;
    break;
  case XS_OBJECT_ELEMENT:
    ss = symt->ss_elements;
    break;
  case XS_OBJECT_ATTRIBUTE_GROUP:
    ss = symt->ss_attribute_groups;
    break;
  case XS_OBJECT_IDENTITY_CONSTRAINT:
    ss = symt->ss_identity_constraints;
    break;
  case XS_OBJECT_MODEL_GROUP_DEF:
    ss = symt->ss_model_group_defs;
    break;
  case XS_OBJECT_NOTATION:
    ss = symt->ss_notations;
    break;
  default:
    assert(!"invalid object type");
    break;
  }
  return ss_lookup(ss,ident);
}

void *xs_lookup_object(xs_schema *s, int type, const nsname ident)
{
  list *l;
  void *obj;
  if (NULL != (obj = xs_symbol_table_lookup_object(s->globals->symt,type,ident)))
    return obj;
  if (NULL != (obj = xs_symbol_table_lookup_object(s->symt,type,ident)))
    return obj;
  for (l = s->imports; l; l = l->next) {
    xs_schema *is = (xs_schema*)l->data;
    if (NULL != (obj = xs_lookup_object(is,type,ident)))
      return obj;
  }
  return NULL;
}

xs_type *xs_lookup_type(xs_schema *s, const nsname ident)
{
  return (xs_type*)xs_lookup_object(s,XS_OBJECT_TYPE,ident);
}

xs_attribute *xs_lookup_attribute(xs_schema *s, const nsname ident)
{
  return (xs_attribute*)xs_lookup_object(s,XS_OBJECT_ATTRIBUTE,ident);
}

xs_element *xs_lookup_element(xs_schema *s, const nsname ident)
{
  return (xs_element*)xs_lookup_object(s,XS_OBJECT_ELEMENT,ident);
}

xs_attribute_group *xs_lookup_attribute_group(xs_schema *s, const nsname ident)
{
  return (xs_attribute_group*)xs_lookup_object(s,XS_OBJECT_ATTRIBUTE_GROUP,ident);
}

xs_identity_constraint *xs_lookup_identity_constraint(xs_schema *s, const nsname ident)
{
  return (xs_identity_constraint*)xs_lookup_object(s,XS_OBJECT_IDENTITY_CONSTRAINT,ident);
}

xs_model_group_def *xs_lookup_model_group_def(xs_schema *s, const nsname ident)
{
  return (xs_model_group_def*)xs_lookup_object(s,XS_OBJECT_MODEL_GROUP_DEF,ident);
}

xs_notation *xs_lookup_notation(xs_schema *s, const nsname ident)
{
  return (xs_notation*)xs_lookup_object(s,XS_OBJECT_NOTATION,ident);
}

void xs_globals_init_simple_types(xs_globals *g)
{
  /* FIXME: set the default facets (whitespace, pattern etc.) as defined in the XML Schema schema */
  new_primitive_type(g,"duration",sizeof(IMPL_POINTER),NULL);
  new_primitive_type(g,"dateTime",sizeof(IMPL_POINTER),NULL);
  new_primitive_type(g,"time",sizeof(IMPL_POINTER),NULL);
  new_primitive_type(g,"date",sizeof(IMPL_POINTER),NULL);
  new_primitive_type(g,"gYearMonth",sizeof(IMPL_POINTER),NULL);
  new_primitive_type(g,"gYear",sizeof(IMPL_POINTER),NULL);
  new_primitive_type(g,"gMonthDay",sizeof(IMPL_POINTER),NULL);
  new_primitive_type(g,"gDay",sizeof(IMPL_POINTER),NULL);
  new_primitive_type(g,"gMonth",sizeof(IMPL_POINTER),NULL);
  g->string_type = new_primitive_type(g,"string",sizeof(IMPL_POINTER),"char*");
  g->boolean_type = new_primitive_type(g,"boolean",sizeof(IMPL_BOOLEAN),NULL);
  new_primitive_type(g,"base64Binary",sizeof(IMPL_POINTER),NULL);
  new_primitive_type(g,"hexBinary",sizeof(IMPL_POINTER),NULL);
  g->float_type = new_primitive_type(g,"float",sizeof(IMPL_FLOAT),"float");
  g->decimal_type = new_primitive_type(g,"decimal",sizeof(IMPL_DOUBLE),NULL);
  g->double_type = new_primitive_type(g,"double",sizeof(IMPL_DOUBLE),"double");
  new_primitive_type(g,"anyURI",sizeof(IMPL_POINTER),NULL);
  new_primitive_type(g,"QName",sizeof(IMPL_POINTER),NULL);
  new_primitive_type(g,"NOTATION",sizeof(IMPL_POINTER),NULL);

  new_derived_type(g,"normalizedString","string",-1,NULL);
  new_derived_type(g,"token","normalizedString",-1,NULL);
  new_derived_type(g,"language","token",-1,NULL);
  new_derived_type(g,"Name","token",-1,NULL);
  new_derived_type(g,"NMTOKEN","token",-1,NULL);
  new_derived_type(g,"NCName","Name",-1,NULL);
  g->id_type = new_derived_type(g,"ID","NCName",-1,NULL);
  new_derived_type(g,"IDREF","NCName",-1,NULL);
  new_derived_type(g,"ENTITY","NCName",-1,NULL);

  new_list_type(g,"IDREFS","IDREF");
  new_list_type(g,"ENTITIES","ENTITY");
  new_list_type(g,"NMTOKENS","NMTOKEN");

  new_derived_type(g,"integer","decimal",-1,NULL);
  new_derived_type(g,"nonPositiveInteger","integer",-1,NULL);
  g->long_type = new_derived_type(g,"long","integer",sizeof(IMPL_LONG),"long");
  new_derived_type(g,"nonNegativeInteger","integer",-1,NULL);
  new_derived_type(g,"negativeInteger","nonPositiveInteger",-1,NULL);
  g->int_type = new_derived_type(g,"int","long",sizeof(IMPL_INT),"int");
  g->short_type = new_derived_type(g,"short","int",sizeof(IMPL_SHORT),"short");
  g->byte_type = new_derived_type(g,"byte","short",sizeof(IMPL_BYTE),"char");
  new_derived_type(g,"unsignedLong","nonNegativeInteger",-1,NULL);
  new_derived_type(g,"postivieInteger","nonNegativeInteger",-1,NULL);
  new_derived_type(g,"unsignedInt","unsignedLong",-1,NULL);
  new_derived_type(g,"unsignedShort","unsignedInt",-1,NULL);
  new_derived_type(g,"unsignedByte","unsignedShort",-1,NULL);
}

xs_symbol_table *xs_symbol_table_new()
{
  xs_symbol_table *symt = (xs_symbol_table*)calloc(1,sizeof(xs_symbol_table));
  symt->ss_types = ss_new(NULL,"type");
  symt->ss_attributes = ss_new(NULL,"attribute");
  symt->ss_elements = ss_new(NULL,"element");
  symt->ss_attribute_groups = ss_new(NULL,"attribute group");
  symt->ss_identity_constraints = ss_new(NULL,"identity constraint");
  symt->ss_model_group_defs = ss_new(NULL,"group");
  symt->ss_notations = ss_new(NULL,"notation");
  return symt;
}

void xs_symbol_table_free(xs_symbol_table *symt)
{
  ss_free(symt->ss_types);
  ss_free(symt->ss_attributes);
  ss_free(symt->ss_elements);
  ss_free(symt->ss_attribute_groups);
  ss_free(symt->ss_identity_constraints);
  ss_free(symt->ss_model_group_defs);
  ss_free(symt->ss_notations);
  free(symt);
}

xs_allocset *xs_allocset_new()
{
  xs_allocset *as = (xs_allocset*)calloc(1,sizeof(xs_allocset));
  return as;
}

void xs_allocset_free(xs_allocset *as)
{
  list_free(as->alloc_cstruct,(void*)xs_cstruct_free);
  list_free(as->alloc_reference,(void*)xs_reference_free);
  list_free(as->alloc_member_type,(void*)xs_member_type_free);
  list_free(as->alloc_type,(void*)xs_type_free);
  list_free(as->alloc_attribute,(void*)xs_attribute_free);
  list_free(as->alloc_element,(void*)xs_element_free);

  list_free(as->alloc_attribute_group_ref,(void*)xs_attribute_group_ref_free);
  list_free(as->alloc_attribute_group,(void*)xs_attribute_group_free);
  list_free(as->alloc_identity_constraint,(void*)xs_identity_constraint_free);
  list_free(as->alloc_model_group_def,(void*)xs_model_group_def_free);
  list_free(as->alloc_model_group,(void*)xs_model_group_free);
  list_free(as->alloc_notation,(void*)xs_notation_free);

  list_free(as->alloc_particle,(void*)xs_particle_free);
  list_free(as->alloc_wildcard,(void*)xs_wildcard_free);
  list_free(as->alloc_attribute_use,(void*)xs_attribute_use_free);
  free(as);
}

xs_type *xs_new_simple_builtin(xs_globals *g, char *name, char *ns, xs_type *base)
{
  xs_type *t;
  assert(NULL != base);
  t = xs_type_new(g->as);
  t->builtin = 1;
  t->complex = 0;
  t->def.ident.name = strdup(name);
  t->def.ident.ns = strdup(ns);
  t->base = g->simple_ur_type;
  t->variety = XS_TYPE_VARIETY_ATOMIC;
  t->stype = XS_TYPE_SIMPLE_BUILTIN;
  t->allowed_facets = g->simple_ur_type->allowed_facets;
  t->typeinfo_known = 1;
  t->size = sizeof(IMPL_POINTER);
  t->ctype = (char*)malloc(strlen("XS")+strlen(name)+1);
  sprintf(t->ctype,"XS%s",name);
  ss_add(g->symt->ss_types,t->def.ident,t);
  return t;
}

void xs_globals_init_xdt_types(xs_globals *g)
{
  g->any_atomic_type = xs_new_simple_builtin(g,"anyAtomicType",XDT_NAMESPACE,g->simple_ur_type);
  g->untyped_atomic = xs_new_simple_builtin(g,"untypedAtomic",XDT_NAMESPACE,g->any_atomic_type);
  g->untyped = xs_new_simple_builtin(g,"untyped",XDT_NAMESPACE,g->complex_ur_type);
  g->untyped = xs_new_simple_builtin(g,"dayTimeDuration",XDT_NAMESPACE,g->complex_ur_type);
  g->untyped = xs_new_simple_builtin(g,"yearMonthDuration",XDT_NAMESPACE,g->complex_ur_type);
  /* FIXME: dayTimeDuration and yearMonthDuration are just placeholders for now; they need
     to be filled in with the correct type definitions. */
}

xs_globals *xs_globals_new()
{
  xs_globals *g = (xs_globals*)calloc(1,sizeof(xs_globals));
  g->as = xs_allocset_new();
  g->symt = xs_symbol_table_new();
  g->complex_ur_type = xs_init_complex_ur_type(g);
  g->simple_ur_type = xs_init_simple_ur_type(g);
  ss_add(g->symt->ss_types,nsname_temp(XS_NAMESPACE,"anyType"),g->complex_ur_type);
  ss_add(g->symt->ss_types,nsname_temp(XS_NAMESPACE,"anySimpleType"),g->simple_ur_type);

  /* FIXME: init builtin attributes (section 3.2.7) */
  xs_globals_init_xdt_types(g);
  xs_globals_init_simple_types(g);

  g->namespaces = ns_map_new();
  ns_add_direct(g->namespaces,XS_NAMESPACE,"xsd");
  ns_add_direct(g->namespaces,XDT_NAMESPACE,"xdt");

  return g;
}

void xs_globals_free(xs_globals *g)
{
  xs_symbol_table_free(g->symt);
  xs_allocset_free(g->as);
  list_free(g->ctypes,(void*)free);
  list_free(g->cstructs,NULL);
  ns_map_free(g->namespaces,0);
  free(g);
}

xs_schema *xs_schema_new(xs_globals *g)
{
  xs_schema *s = (xs_schema*)calloc(1,sizeof(xs_schema));
  s->globals = g;
  s->as = xs_allocset_new();
  s->symt = xs_symbol_table_new();

  return s;
}

void xs_schema_free(xs_schema *s)
{
  free(s->uri);
  free(s->ns);

  error_info_free_vals(&s->ei);

  free(s->block_default);
  free(s->final_default);

  list_free(s->imports,(void*)xs_schema_free);

  xs_allocset_free(s->as);
  xs_symbol_table_free(s->symt);
  free(s);
}

xs_cstruct *xs_cstruct_new(xs_allocset *as)
{
  xs_cstruct *cs = (xs_cstruct*)calloc(1,sizeof(xs_cstruct));
  list_push(&as->alloc_cstruct,cs);
  return cs;
}

void xs_cstruct_free(xs_cstruct *cs)
{
  free(cs);
}

xs_reference *xs_reference_new(xs_allocset *as)
{
  xs_reference *r = (xs_reference*)calloc(1,sizeof(xs_reference));
  list_push(&as->alloc_reference,r);
  return r;
}

void xs_reference_free(xs_reference *r)
{
  nsname_free(r->def.ident);
  free(r);
}

xs_member_type *xs_member_type_new(xs_allocset *as)
{
  xs_member_type *mt = (xs_member_type*)calloc(1,sizeof(xs_member_type));
  list_push(&as->alloc_member_type,mt);
  return mt;
}

void xs_member_type_free(xs_member_type *mt)
{
  free(mt);
}

xs_type *xs_type_new(xs_allocset *as)
{
  xs_type *t = (xs_type*)calloc(1,sizeof(xs_type));
  list_push(&as->alloc_type,t);
  return t;
}

void xs_facetdata_freevals(xs_facetdata *fd)
{
  int facet;
  for (facet = 0; facet < XS_FACET_NUMFACETS; facet++)
    free(fd->strval[facet]);
  list_free(fd->patterns,free);
  list_free(fd->enumerations,free);
}

void xs_type_free(xs_type *t)
{
  nsname_free(t->def.ident);
  list_free(t->attribute_uses,NULL);
  list_free(t->local_attribute_uses,NULL);
  list_free(t->attribute_group_refs,NULL);
  list_free(t->members,NULL);
  xs_facetdata_freevals(&t->facets);
  xs_facetdata_freevals(&t->child_facets);
  free(t->parent_name);
  free(t->parent_ns);
  list_free(t->attribute_cvars,free);
  free(t->ctype);
  free(t);
}

xs_attribute *xs_attribute_new(xs_allocset *as)
{
  xs_attribute *a = (xs_attribute*)calloc(1,sizeof(xs_attribute));
  list_push(&as->alloc_attribute,a);
  return a;
}

void xs_attribute_free(xs_attribute *a)
{
  nsname_free(a->def.ident);
  free(a->vc.value);
  free(a);
}

xs_element *xs_element_new(xs_allocset *as)
{
  xs_element *e = (xs_element*)calloc(1,sizeof(xs_element));
  list_push(&as->alloc_element,e);
  return e;
}

void xs_element_free(xs_element *e)
{
  nsname_free(e->def.ident);
  free(e->vc.value);
  list_free(e->sgmembers,NULL);
  free(e);
}

xs_attribute_group_ref *xs_attribute_group_ref_new(xs_allocset *as)
{
  xs_attribute_group_ref *agr = (xs_attribute_group_ref*)calloc(1,sizeof(xs_attribute_group_ref));
  list_push(&as->alloc_attribute_group_ref,agr);
  return agr;
}

void xs_attribute_group_ref_free(xs_attribute_group_ref *agr)
{
  free(agr->cvar);
  free(agr);
}

xs_attribute_group *xs_attribute_group_new(xs_allocset *as)
{
  xs_attribute_group *ag = (xs_attribute_group*)calloc(1,sizeof(xs_attribute_group));
  list_push(&as->alloc_attribute_group,ag);
  return ag;
}

void xs_attribute_group_free(xs_attribute_group *ag)
{
  nsname_free(ag->def.ident);
  list_free(ag->attribute_uses,NULL);
  list_free(ag->local_attribute_uses,NULL);
  list_free(ag->attribute_group_refs,NULL);
  list_free(ag->cvars,free);
  free(ag->ctype);
  free(ag);
}

xs_identity_constraint *xs_identity_constraint_new(xs_allocset *as)
{
  xs_identity_constraint *ic = (xs_identity_constraint*)calloc(1,sizeof(xs_identity_constraint));
  list_push(&as->alloc_identity_constraint,ic);
  return ic;
}

void xs_identity_constraint_free(xs_identity_constraint *ic)
{
  /* FIXME */
}

xs_model_group_def *xs_model_group_def_new(xs_allocset *as)
{
  xs_model_group_def *mgd = (xs_model_group_def*)calloc(1,sizeof(xs_model_group_def));
  list_push(&as->alloc_model_group_def,mgd);
  return mgd;
}

void xs_model_group_def_free(xs_model_group_def *mgd)
{
  nsname_free(mgd->def.ident);
  free(mgd);
}

xs_model_group *xs_model_group_new(xs_allocset *as)
{
  xs_model_group *mg = (xs_model_group*)calloc(1,sizeof(xs_model_group));
  list_push(&as->alloc_model_group,mg);
  return mg;
}

void xs_model_group_free(xs_model_group *mg)
{
  list_free(mg->particles,NULL);
  free(mg->ctype);
  list_free(mg->cvars,free);
  free(mg->parent_name);
  free(mg->parent_ns);
  free(mg);
}

xs_notation *xs_notation_new(xs_allocset *as)
{
  xs_notation *n = (xs_notation*)calloc(1,sizeof(xs_notation));
  list_push(&as->alloc_notation,n);
  return n;
}

void xs_notation_free(xs_notation *n)
{
  /* FIXME */
}

xs_particle *xs_particle_new(xs_allocset *as)
{
  xs_particle *p = (xs_particle*)calloc(1,sizeof(xs_particle));
  list_push(&as->alloc_particle,p);
  return p;
}

void xs_particle_free(xs_particle *p)
{
  free(p->cvar);
  free(p);
}

xs_wildcard *xs_wildcard_new(xs_allocset *as)
{
  xs_wildcard *w = (xs_wildcard*)calloc(1,sizeof(xs_wildcard));
  list_push(&as->alloc_wildcard,w);
  return w;
}

void xs_wildcard_free(xs_wildcard *w)
{
  free(w->not_ns);
  list_free(w->nslist,free);
  free(w);
}

xs_attribute_use *xs_attribute_use_new(xs_allocset *as)
{
  xs_attribute_use *au = (xs_attribute_use*)calloc(1,sizeof(xs_attribute_use));
  list_push(&as->alloc_attribute_use,au);
  return au;
}

void xs_attribute_use_free(xs_attribute_use *au)
{
  free(au->vc.value);
  free(au->cvar);
  free(au);
}

  /*
     FIXME: when parsing, we currently ignore attributes we're not interested in. I think we're
     supposed to disallow any attributes from the XML Schema namespace that are not explicitly
     allowed on the elements.

     Deferred: Simple type validation
     @implements(xmlschema-1:cvc-simple-type.1) status { deferred } @end
     @implements(xmlschema-1:cvc-simple-type.2) status { deferred } @end
     @implements(xmlschema-1:cvc-simple-type.2.1) status { deferred } @end
     @implements(xmlschema-1:cvc-simple-type.2.2) status { deferred } @end
     @implements(xmlschema-1:cvc-simple-type.2.3) status { deferred } @end

     Deferred: Annotations
     @implements(xmlschema-1:src-annotation) status { deferred } @end
     @implements(xmlschema-1:an-props-correct) status { deferred } @end

     Deferred: Notation declarations
     @implements(xmlschema-1:src-notation) status { deferred } @end
     @implements(xmlschema-1:sic-notation-used) status { deferred } @end
     @implements(xmlschema-1:n-props-correct) status { deferred } @end
     @implements(xmlschema-1:Notation Declaration{name}) status { deferred } @end
     @implements(xmlschema-1:Notation Declaration{target namespace}) status { deferred } @end
     @implements(xmlschema-1:Notation Declaration{system identifier}) status { deferred } @end
     @implements(xmlschema-1:Notation Declaration{public identifier}) status { deferred } @end
     @implements(xmlschema-1:Notation Declaration{annotation}) status { deferred } @end

     Deferred: Identity-constraint definitions
     @implements(xmlschema-1:src-identity-constraint) status { deferred } @end
     @implements(xmlschema-1:cvc-identity-constraint.1) status { deferred } @end
     @implements(xmlschema-1:cvc-identity-constraint.2) status { deferred } @end
     @implements(xmlschema-1:cvc-identity-constraint.3) status { deferred } @end
     @implements(xmlschema-1:cvc-identity-constraint.4) status { deferred } @end
     @implements(xmlschema-1:cvc-identity-constraint.4.1) status { deferred } @end
     @implements(xmlschema-1:cvc-identity-constraint.4.2) status { deferred } @end
     @implements(xmlschema-1:cvc-identity-constraint.4.2.1) status { deferred } @end
     @implements(xmlschema-1:cvc-identity-constraint.4.2.2) status { deferred } @end
     @implements(xmlschema-1:cvc-identity-constraint.4.2.3) status { deferred } @end
     @implements(xmlschema-1:cvc-identity-constraint.4.3) status { deferred } @end
     @implements(xmlschema-1:sic-key.1) status { deferred } @end
     @implements(xmlschema-1:sic-key.2) status { deferred } @end
     @implements(xmlschema-1:c-props-correct.1) status { deferred } @end
     @implements(xmlschema-1:c-props-correct.2) status { deferred } @end
     @implements(xmlschema-1:c-selector-xpath.1) status { deferred } @end
     @implements(xmlschema-1:c-selector-xpath.2) status { deferred } @end
     @implements(xmlschema-1:c-selector-xpath.2.1) status { deferred } @end
     @implements(xmlschema-1:c-selector-xpath.2.2) status { deferred } @end
     @implements(xmlschema-1:c-fields-xpaths.1) status { deferred } @end
     @implements(xmlschema-1:c-fields-xpaths.2) status { deferred } @end
     @implements(xmlschema-1:c-fields-xpaths.2.1) status { deferred } @end
     @implements(xmlschema-1:c-fields-xpaths.2.2) status { deferred } @end
     @implements(xmlschema-1:Identity-constraint Definition{name}) status { deferred } @end
     @implements(xmlschema-1:Identity-constraint Definition{target namespace}) status { deferred } @end
     @implements(xmlschema-1:Identity-constraint Definition{identity-constraint category})
     status { deferred } @end
     @implements(xmlschema-1:Identity-constraint Definition{selector}) status { deferred } @end
     @implements(xmlschema-1:Identity-constraint Definition{fields}) status { deferred } @end
     @implements(xmlschema-1:Identity-constraint Definition{referenced key}) status { deferred } @end
     @implements(xmlschema-1:Identity-constraint Definition{annotation}) status { deferred } @end

     Deferred: Wildcard validation

     @implements(xmlschema-1:cvc-wildcard.1) status { deferred } @end
     @implements(xmlschema-1:cvc-wildcard.2) status { deferred } @end
     @implements(xmlschema-1:cvc-wildcard.3) status { deferred } @end
     @implements(xmlschema-1:cvc-wildcard-namespace.1) status { deferred } @end
     @implements(xmlschema-1:cvc-wildcard-namespace.2) status { deferred } @end
     @implements(xmlschema-1:cvc-wildcard-namespace.2.1) status { deferred } @end
     @implements(xmlschema-1:cvc-wildcard-namespace.2.2) status { deferred } @end
     @implements(xmlschema-1:cvc-wildcard-namespace.2.3) status { deferred } @end
     @implements(xmlschema-1:cvc-wildcard-namespace.3) status { deferred } @end

     Deferred: Particle validation

     @implements(xmlschema-1:cvc-particle.1) status { deferred } @end
     @implements(xmlschema-1:cvc-particle.1.1) status { deferred } @end
     @implements(xmlschema-1:cvc-particle.1.2) status { deferred } @end
     @implements(xmlschema-1:cvc-particle.1.3) status { deferred } @end
     @implements(xmlschema-1:cvc-particle.2) status { deferred } @end
     @implements(xmlschema-1:cvc-particle.2.1) status { deferred } @end
     @implements(xmlschema-1:cvc-particle.2.2) status { deferred } @end
     @implements(xmlschema-1:cvc-particle.2.3) status { deferred } @end
     @implements(xmlschema-1:cvc-particle.2.3.1) status { deferred } @end
     @implements(xmlschema-1:cvc-particle.2.3.2) status { deferred } @end
     @implements(xmlschema-1:cvc-particle.2.3.3) status { deferred } @end
     @implements(xmlschema-1:cvc-particle.3) status { deferred } @end
     @implements(xmlschema-1:cvc-particle.3.1) status { deferred } @end
     @implements(xmlschema-1:cvc-particle.3.2) status { deferred } @end
     @implements(xmlschema-1:cvc-particle.3.3) status { deferred } @end

     Deferred: Model group validation

     @implements(xmlschema-1:cvc-model-group.1) status { deferred } @end
     @implements(xmlschema-1:cvc-model-group.2) status { deferred } @end
     @implements(xmlschema-1:cvc-model-group.3) status { deferred } @end

     Deferred: Attribute use validation
     @implements(xmlschema-1:cvc-au) status { deferred } @end

     Deferred: Redefinition

     @implements(xmlschema-1:src-redefine.1) status { deferred } @end
     @implements(xmlschema-1:src-redefine.2) status { deferred } @end
     @implements(xmlschema-1:src-redefine.2.1) status { deferred } @end
     @implements(xmlschema-1:src-redefine.2.2) status { deferred } @end
     @implements(xmlschema-1:src-redefine.3) status { deferred } @end
     @implements(xmlschema-1:src-redefine.3.1) status { deferred } @end
     @implements(xmlschema-1:src-redefine.3.2) status { deferred } @end
     @implements(xmlschema-1:src-redefine.3.3) status { deferred } @end
     @implements(xmlschema-1:src-redefine.4) status { deferred } @end
     @implements(xmlschema-1:src-redefine.4.1) status { deferred } @end
     @implements(xmlschema-1:src-redefine.4.2) status { deferred } @end
     @implements(xmlschema-1:src-redefine.5) status { deferred } @end
     @implements(xmlschema-1:src-redefine.6) status { deferred } @end
     @implements(xmlschema-1:src-redefine.6.1) status { deferred } @end
     @implements(xmlschema-1:src-redefine.6.1.1) status { deferred } @end
     @implements(xmlschema-1:src-redefine.6.1.2) status { deferred } @end
     @implements(xmlschema-1:src-redefine.6.2) status { deferred } @end
     @implements(xmlschema-1:src-redefine.6.2.1) status { deferred } @end
     @implements(xmlschema-1:src-redefine.6.2.2) status { deferred } @end
     @implements(xmlschema-1:src-redefine.7) status { deferred } @end
     @implements(xmlschema-1:src-redefine.7.1) status { deferred } @end
     @implements(xmlschema-1:src-redefine.7.2) status { deferred } @end
     @implements(xmlschema-1:src-redefine.7.2.1) status { deferred } @end
     @implements(xmlschema-1:src-redefine.7.2.2) status { deferred } @end

     @implements(xmlschema-1:src-expredef.1) status { deferred } @end
     @implements(xmlschema-1:src-expredef.1.1) status { deferred } @end
     @implements(xmlschema-1:src-expredef.1.2) status { deferred } @end
     @implements(xmlschema-1:src-expredef.2) status { deferred } @end

     Deferred: Complex type validation

     @implements(xmlschema-1:cvc-complex-type.1) status { deferred } @end
     @implements(xmlschema-1:cvc-complex-type.2) status { deferred } @end
     @implements(xmlschema-1:cvc-complex-type.2.1) status { deferred } @end
     @implements(xmlschema-1:cvc-complex-type.2.2) status { deferred } @end
     @implements(xmlschema-1:cvc-complex-type.2.3) status { deferred } @end
     @implements(xmlschema-1:cvc-complex-type.2.4) status { deferred } @end
     @implements(xmlschema-1:cvc-complex-type.3) status { deferred } @end
     @implements(xmlschema-1:cvc-complex-type.3.1) status { deferred } @end
     @implements(xmlschema-1:cvc-complex-type.3.2) status { deferred } @end
     @implements(xmlschema-1:cvc-complex-type.3.2.1) status { deferred } @end
     @implements(xmlschema-1:cvc-complex-type.3.2.2) status { deferred } @end
     @implements(xmlschema-1:cvc-complex-type.4) status { deferred } @end
     @implements(xmlschema-1:cvc-complex-type.5) status { deferred } @end
     @implements(xmlschema-1:cvc-complex-type.5.1) status { deferred } @end
     @implements(xmlschema-1:cvc-complex-type.5.2) status { deferred } @end

     Deferred: Complex type information set contributions

     @implements(xmlschema-1:sic-attrDefault) status { deferred } @end

     Deferred: Element validation

     @implements(xmlschema-1:cvc-elt.1) status { deferred } @end
     @implements(xmlschema-1:cvc-elt.2) status { deferred } @end
     @implements(xmlschema-1:cvc-elt.3) status { deferred } @end
     @implements(xmlschema-1:cvc-elt.3.1) status { deferred } @end
     @implements(xmlschema-1:cvc-elt.3.2) status { deferred } @end
     @implements(xmlschema-1:cvc-elt.3.2.1) status { deferred } @end
     @implements(xmlschema-1:cvc-elt.3.2.2) status { deferred } @end
     @implements(xmlschema-1:cvc-elt.4) status { deferred } @end
     @implements(xmlschema-1:cvc-elt.4.1) status { deferred } @end
     @implements(xmlschema-1:cvc-elt.4.2) status { deferred } @end
     @implements(xmlschema-1:cvc-elt.4.3) status { deferred } @end
     @implements(xmlschema-1:cvc-elt.5) status { deferred } @end
     @implements(xmlschema-1:cvc-elt.5.1) status { deferred } @end
     @implements(xmlschema-1:cvc-elt.5.1.1) status { deferred } @end
     @implements(xmlschema-1:cvc-elt.5.1.2) status { deferred } @end
     @implements(xmlschema-1:cvc-elt.5.2) status { deferred } @end
     @implements(xmlschema-1:cvc-elt.5.2.1) status { deferred } @end
     @implements(xmlschema-1:cvc-elt.5.2.2) status { deferred } @end
     @implements(xmlschema-1:cvc-elt.5.2.2.1) status { deferred } @end
     @implements(xmlschema-1:cvc-elt.5.2.2.2) status { deferred } @end
     @implements(xmlschema-1:cvc-elt.5.2.2.2.1) status { deferred } @end
     @implements(xmlschema-1:cvc-elt.5.2.2.2.2) status { deferred } @end
     @implements(xmlschema-1:cvc-elt.6) status { deferred } @end
     @implements(xmlschema-1:cvc-elt.7) status { deferred } @end
     @implements(xmlschema-1:cvc-type.1) status { deferred } @end
     @implements(xmlschema-1:cvc-type.2) status { deferred } @end
     @implements(xmlschema-1:cvc-type.3) status { deferred } @end
     @implements(xmlschema-1:cvc-type.3.1) status { deferred } @end
     @implements(xmlschema-1:cvc-type.3.1.1) status { deferred } @end
     @implements(xmlschema-1:cvc-type.3.1.2) status { deferred } @end
     @implements(xmlschema-1:cvc-type.3.1.3) status { deferred } @end
     @implements(xmlschema-1:cvc-type.3.2) status { deferred } @end
     @implements(xmlschema-1:cvc-id.1) status { deferred } @end
     @implements(xmlschema-1:cvc-id.2) status { deferred } @end
     @implements(xmlschema-1:cvc-assess-elt.1) status { deferred } @end
     @implements(xmlschema-1:cvc-assess-elt.1.1) status { deferred } @end
     @implements(xmlschema-1:cvc-assess-elt.1.1.1) status { deferred } @end
     @implements(xmlschema-1:cvc-assess-elt.1.1.1.1) status { deferred } @end
     @implements(xmlschema-1:cvc-assess-elt.1.1.1.2) status { deferred } @end
     @implements(xmlschema-1:cvc-assess-elt.1.1.1.3) status { deferred } @end
     @implements(xmlschema-1:cvc-assess-elt.1.1.1.3.1) status { deferred } @end
     @implements(xmlschema-1:cvc-assess-elt.1.1.1.3.2) status { deferred } @end
     @implements(xmlschema-1:cvc-assess-elt.1.1.2) status { deferred } @end
     @implements(xmlschema-1:cvc-assess-elt.1.1.3) status { deferred } @end
     @implements(xmlschema-1:cvc-assess-elt.1.2) status { deferred } @end
     @implements(xmlschema-1:cvc-assess-elt.1.2.1) status { deferred } @end
     @implements(xmlschema-1:cvc-assess-elt.1.2.1.1) status { deferred } @end
     @implements(xmlschema-1:cvc-assess-elt.1.2.1.2) status { deferred } @end
     @implements(xmlschema-1:cvc-assess-elt.1.2.1.2.1) status { deferred } @end
     @implements(xmlschema-1:cvc-assess-elt.1.2.1.2.2) status { deferred } @end
     @implements(xmlschema-1:cvc-assess-elt.1.2.1.2.3) status { deferred } @end
     @implements(xmlschema-1:cvc-assess-elt.1.2.1.2.4) status { deferred } @end
     @implements(xmlschema-1:cvc-assess-elt.1.2.2) status { deferred } @end
     @implements(xmlschema-1:cvc-assess-elt.2) status { deferred } @end

     @implements(xmlschema-1:cos-equiv-derived-ok-rec.1)
     status { deferred } issue { only relevant for particle validation? } @end
     @implements(xmlschema-1:cos-equiv-derived-ok-rec.2)
     status { deferred } issue { only relevant for particle validation? } @end
     @implements(xmlschema-1:cos-equiv-derived-ok-rec.2.1)
     status { deferred } issue { only relevant for particle validation? } @end
     @implements(xmlschema-1:cos-equiv-derived-ok-rec.2.2)
     status { deferred } issue { only relevant for particle validation? } @end
     @implements(xmlschema-1:cos-equiv-derived-ok-rec.2.3)
     status { deferred } issue { only relevant for particle validation? } @end

     Deferred: Element information set contributions

     @implements(xmlschema-1:sic-e-outcome.1) status { deferred } @end
     @implements(xmlschema-1:sic-e-outcome.1.1) status { deferred } @end
     @implements(xmlschema-1:sic-e-outcome.1.1.1) status { deferred } @end
     @implements(xmlschema-1:sic-e-outcome.1.1.1.1) status { deferred } @end
     @implements(xmlschema-1:sic-e-outcome.1.1.1.2) status { deferred } @end
     @implements(xmlschema-1:sic-e-outcome.1.1.2) status { deferred } @end
     @implements(xmlschema-1:sic-e-outcome.1.1.3) status { deferred } @end
     @implements(xmlschema-1:sic-e-outcome.1.2) status { deferred } @end
     @implements(xmlschema-1:sic-e-outcome.2) status { deferred } @end
     @implements(xmlschema-1:sic-e-outcome.1) status { deferred } @end
     @implements(xmlschema-1:sic-e-outcome.2) status { deferred } @end
     @implements(xmlschema-1:sic-e-outcome.3) status { deferred } @end
     @implements(xmlschema-1:sic-elt-error-code.1) status { deferred } @end
     @implements(xmlschema-1:sic-elt-error-code.2) status { deferred } @end
     @implements(xmlschema-1:sic-elt-decl) status { deferred } @end
     @implements(xmlschema-1:sic-eltType.1) status { deferred } @end
     @implements(xmlschema-1:sic-eltType.2) status { deferred } @end
     @implements(xmlschema-1:sic-eltDefault.1) status { deferred } @end
     @implements(xmlschema-1:sic-eltDefault.2) status { deferred } @end

     Deferred: Attribute validation

     @implements(xmlschema-1:cvc-attribute.1) status { deferred } @end
     @implements(xmlschema-1:cvc-attribute.2) status { deferred } @end
     @implements(xmlschema-1:cvc-attribute.3) status { deferred } @end
     @implements(xmlschema-1:cvc-attribute.4) status { deferred } @end
     @implements(xmlschema-1:cvc-assess-attr.1) status { deferred } @end
     @implements(xmlschema-1:cvc-assess-attr.1.1) status { deferred } @end
     @implements(xmlschema-1:cvc-assess-attr.1.2) status { deferred } @end
     @implements(xmlschema-1:cvc-assess-attr.2) status { deferred } @end
     @implements(xmlschema-1:cvc-assess-attr.3) status { deferred } @end

     Deferred: Attribute information set contributions

     @implements(xmlschema-1:sic-a-outcome.1) status { deferred } @end
     @implements(xmlschema-1:sic-a-outcome.1.1) status { deferred } @end
     @implements(xmlschema-1:sic-a-outcome.1.2) status { deferred } @end
     @implements(xmlschema-1:sic-a-outcome.2) status { deferred } @end
     @implements(xmlschema-1:sic-a-outcome.1) status { deferred } @end
     @implements(xmlschema-1:sic-a-outcome.2) status { deferred } @end
     @implements(xmlschema-1:sic-attr-error-code.1) status { deferred } @end
     @implements(xmlschema-1:sic-attr-error-code.2) status { deferred } @end
     @implements(xmlschema-1:sic-attr-decl) status { deferred } @end
     @implements(xmlschema-1:sic-attrType.1) status { deferred } @end
     @implements(xmlschema-1:sic-attrType.2) status { deferred } @end

     Deferred: Annotations

     @implements(xmlschema-1:Element Declaration{annotation}) status { deferred } @end
     @implements(xmlschema-1:Attribute Declaration{annotation}) status { deferred } @end
     @implements(xmlschema-1:Attribute Group Definition{annotation}) status { deferred } @end
     @implements(xmlschema-1:Simple Type Definition{annotation}) status { deferred } @end
     @implements(xmlschema-1:Complex Type Definition{annotations}) status { deferred } @end
     @implements(xmlschema-1:Model Group Definition{annotation}) status { deferred } @end
     @implements(xmlschema-1:Model Group{annotation}) status { deferred } @end
     @implements(xmlschema-1:Wildcard{annotation}) status { deferred } @end
     @implements(xmlschema-1:Schema{annotations}) status { deferred } @end
     @implements(xmlschema-1:Annotation{application information}) status { deferred } @end
     @implements(xmlschema-1:Annotation{user information}) status { deferred } @end
     @implements(xmlschema-1:Annotation{attributes}) status { deferred } @end

     Deferred: Qname resolution (instance)

     @implements(xmlschema-1:cvc-resolve-instance.1) status { deferred } @end
     @implements(xmlschema-1:cvc-resolve-instance.2) status { deferred } @end
     @implements(xmlschema-1:cvc-resolve-instance.3) status { deferred } @end
     @implements(xmlschema-1:cvc-resolve-instance.4) status { deferred } @end
     @implements(xmlschema-1:cvc-resolve-instance.5) status { deferred } @end
     @implements(xmlschema-1:cvc-resolve-instance.6) status { deferred } @end
  */
