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

#include "derivation.h"
#include "xmlschema.h"
#include "util/debug.h"
#include "util/macros.h"
#include "parse.h"
#include <libxml/xmlwriter.h>
#include <libxml/xmlIO.h>
#include <libxml/parser.h>
#include <libxml/tree.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <ctype.h>
#include <errno.h>

int check_facet_valid_restriction(xs_schema *s, xs_type *D, xs_type *B, int facet)
{
  xs_type *b2;

  if (NULL == D->facets.strval[facet])
    return 0;

  for (b2 = B; b2; b2 = b2->base) {

    if (NULL != b2->facets.strval[facet]) {
      /* FIXME: this is not yet a complete implementation... just enough so we can test this
         from the simple type derivation ok test */

      if (((XS_FACET_LENGTH == facet) && (D->facets.intval[facet] != B->facets.intval[facet])) ||
          ((XS_FACET_MINLENGTH == facet) && (D->facets.intval[facet] < B->facets.intval[facet])) ||
          ((XS_FACET_MAXLENGTH == facet) && (D->facets.intval[facet] > B->facets.intval[facet])))
        return error(&s->ei,s->uri,D->facets.defline[facet],"structures-3.14.6",
                     "\"%s\" is not a valid restriction of \"%s\" for the %s facet",
                     D->facets.strval[facet],b2->facets.strval[facet],xs_facet_names[facet]);
    }

    if (b2->base == b2)
      break;
  }

  return 0;
}


int xs_check_simple_type_derivation_valid_atomic(xs_schema *s, xs_type *t)
{
  int facet;
  /* 3.14.6 Schema Component Constraint: Derivation Valid (Restriction, Simple) */
  /* Note: we return 0 here on success, and -1 on error (and set error info) */

  if (t == s->globals->simple_ur_type)
    return 0;
  assert(t->primitive_type);

  /* @implements(xmlschema-1:cos-st-restricts.1)
     test { simpletype_restriction_derivation_anysimpletype.test }
     @end */
  /* 1 If the {variety} is atomic, then  all of the following must be true: */

  /* @implements(xmlschema-1:cos-st-restricts.1.1) @end */
  /* 1.1 The {base type definition} must be an atomic simple type definition or a built-in
     primitive datatype. */
  assert(XS_TYPE_VARIETY_INVALID != t->base->variety);
  /* note: not sure if we can test this; i think this stuff is already checked previously */
  if (t->base->complex || ((XS_TYPE_VARIETY_ATOMIC != t->base->variety) && !t->base->builtin)) {
    return error(&s->ei,s->uri,t->def.loc.line,"cos-st-restricts.1.1","For atomic simple types, "
                 "the base type definition must be an atomic simple type definition or a "
                 "built-in primitive datatype");
  }

  /* @implements(xmlschema-1:cos-st-restricts.1.2) @end */
  /* 1.2 The {final} of the {base type definition} must not contain restriction. */
  /* Already covered by rule 3.14.6-1-3 */

  /* @implements(xmlschema-1:cos-st-restricts.1.3) @end */
  /* 1.3 For each facet in the {facets} (call this DF) all of the following must be true: */
  for (facet = 0; facet < XS_FACET_NUMFACETS; facet++) {
    if (t->facets.strval[facet]) {
      /* @implements(xmlschema-1:cos-st-restricts.1.3.1)
         test { simpletype_restriction_okfacet.test }
         test { simpletype_restriction_badfacet.test }
         @end */
      /* 1.3.1 DF must be an allowed constraining facet for the {primitive type definition}, as
         specified in the appropriate subsection of 3.2 Primitive datatypes. */
      if (!xs_facet_allowed(t,facet))
        return error(&s->ei,s->uri,t->facets.defline[facet],"cos-st-restricts.1.3.1",
                     "Facet %s not allowed on types derived from %#n",
                     xs_facet_names[facet],t->primitive_type->def.ident);

      /* @implements(xmlschema-1:cos-st-restricts.1.3.2)
         test { simpletype_restriction_okfacetrestriction.test }
         test { simpletype_restriction_badfacetrestriction.test }
         @end */
      /* 1.3.2 If there is a facet of the same kind in the {facets} of the {base type definition}
         (call this BF),then the DF's {value} must be a valid restriction of BF's {value} as defined
         in [XML Schemas: Datatypes]. */
      CHECK_CALL(check_facet_valid_restriction(s,t,t->base,facet))
    }
  }

  return 0;
}

int xs_check_simple_type_derivation_valid_list(xs_schema *s, xs_type *t)
{
  int facet;
  /* 3.14.6 Schema Component Constraint: Derivation Valid (Restriction, Simple) */
  /* Note: we return 0 here on success, and -1 on error (and set error info) */

  /* @implements(xmlschema-1:cos-st-restricts.2) @end */

  /* 2 If the {variety} is list, then  all of the following must be true: */

  /* @implements(xmlschema-1:cos-st-restricts.2.1)
     test { simpletype_list_list.test }
     test { simpletype_list_complex.test }
     test { simpletype_list_badunion.test }
     test { simpletype_list_badunion2.test }
     @end */
  /* 2.1 The {item type definition} must have a {variety} of atomic or union (in which case all
         the {member type definitions} must be atomic). */
  if (t->item_type->complex || (XS_TYPE_VARIETY_LIST == t->item_type->variety))
    return error(&s->ei,s->uri,t->def.loc.line,"cos-st-restricts.2.1",
                 "<list> can only have its item type set to an atomic or union type");
  if (XS_TYPE_VARIETY_UNION == t->item_type->variety) {
    list *l;
    for (l = t->item_type->members; l; l = l->next) {
      xs_member_type *mt = (xs_member_type*)l->data;
      assert(mt->type);
      if (mt->type->complex || (XS_TYPE_VARIETY_ATOMIC != mt->type->variety))
        return error(&s->ei,s->uri,t->def.loc.line,"cos-st-restricts.2.1","If <list> has an item "
                     "type that is a union, then all member types of the union must be atomic");
    }
  }

  /* @implements(xmlschema-1:cos-st-restricts.2.2) @end */
  /* 2.2 (none) */

  /* @implements(xmlschema-1:cos-st-restricts.2.3) @end */
  /* 2.3 The appropriate case among the following must be true: */
  /* @implements(xmlschema-1:cos-st-restricts.2.3.1) @end */
  /* 2.3.1 If the {base type definition} is the simple ur-type definition, then all of the
           following must be true: */
  if (t->base == s->globals->simple_ur_type) {
    /* @implements(xmlschema-1:cos-st-restricts.2.3.1.1)
       test { simpletype_list_itemtype_final_list.test }
       test { simpletype_list_itemtype_final_list2.test }
       @end */
    /* 2.3.1.1 The {final} of the {item type definition} must not contain list. */

    if (t->item_type->final_list)
      return error(&s->ei,s->uri,t->def.loc.line,"cos-st-restricts.2.3.1.1",
                   "Item type %#n disallows use in lists",t->item_type->def.ident);

    /* @implements(xmlschema-1:cos-st-restricts.2.3.1.2) @end */
    /* 2.3.1.2 The {facets} must only contain the whiteSpace facet component. */
    /* Can't really test this - the parsing code for <list> disallows facet children (as required
       by the specification of the XML representation) */
    for (facet = 0; facet < XS_FACET_NUMFACETS; facet++) {
      if ((NULL != t->facets.strval[facet]) && (XS_FACET_WHITESPACE != facet)) {
        return error(&s->ei,s->uri,t->def.loc.line,"cos-st-restricts.2.3.1.2","Simple types "
                     "declared with <list> may not have any facets set other than whiteSpace");
      }
    }
  }

  /* @implements(xmlschema-1:cos-st-restricts.2.3.2) @end */
  /* 2.3.2 otherwise all of the following must be true: */
  else {
    /* @implements(xmlschema-1:cos-st-restricts.2.3.2.1) @end */
    /* 2.3.2.1 The {base type definition} must have a {variety} of list. */
    /* Can't really test this, it's enforced by the parser */
    if (t->base->complex || (XS_TYPE_VARIETY_LIST != t->base->variety))
      return error(&s->ei,s->uri,t->def.loc.line,"cos-st-restricts.2.3.2.1","Simple types with a "
                   "variety of list must have a base type which is also a list");

    /* @implements(xmlschema-1:cos-st-restricts.2.3.2.2) @end */
    /* 2.3.2.2 The {final} of the {base type definition} must not contain restriction. */
    /* Already covered by rule 3.14.6-1-3 */

    /* @implements(xmlschema-1:cos-st-restricts.2.3.2.3) @end */
    /* 2.3.2.3 The {item type definition} must be validly derived from the {base type definition}'s
               {item type definition} given the empty set, as defined in Type Derivation OK (Simple)
               (3.14.6). */
    /* This should be implicitly true, since the parser doesn't provide a way to create union types
       that are derived from other union types except by using <restriction>, which doesn't allow
       the declaration of a different set of member types.
       Probably don't need to check this. */
    assert(t->item_type);
    assert(t->base->item_type);
    CHECK_CALL(xs_check_simple_type_derivation_ok(s,t->item_type,t->base->item_type,0,0,0,0))

    for (facet = 0; facet < XS_FACET_NUMFACETS; facet++) {
      if (NULL != t->facets.strval[facet]) {
        /* @implements(xmlschema-1:cos-st-restricts.2.3.2.4)
           test { simpletype_list_derived_okfacets.test }
           test { simpletype_list_derived_badfacet.test }
           @end */
        /* 2.3.2.4 Only length, minLength, maxLength, whiteSpace, pattern and enumeration facet
                   components are allowed among the {facets}. */
        if ((XS_FACET_LENGTH != facet) &&
            (XS_FACET_MINLENGTH != facet) &&
            (XS_FACET_MAXLENGTH != facet) &&
            (XS_FACET_WHITESPACE != facet) &&
            (XS_FACET_PATTERN != facet) &&
            (XS_FACET_ENUMERATION != facet))
          return error(&s->ei,s->uri,t->def.loc.line,"cos-st-restricts.2.3.2.4","Facet %s is not "
                       "allowed in a list or list-derived type",xs_facet_names[facet]);

        /* @implements(xmlschema-1:cos-st-restricts.2.3.2.5)
           test { simpletype_list_derived_okfacetrestriction.test }
           test { simpletype_list_derived_badfacetrestriction.test }
           @end */
        /* 2.3.2.5 For each facet in the {facets} (call this DF), if there is a facet of the same
                   kind in the {facets} of the {base type definition} (call this BF),then the DF's
                   {value} must be a valid restriction of BF's {value} as defined in [XML Schemas:
                   Datatypes]. */
        CHECK_CALL(check_facet_valid_restriction(s,t,t->base,facet))
      }
    }
  }

  return 0;
}

int xs_check_simple_type_derivation_valid_union(xs_schema *s, xs_type *t)
{
  list *l;
  int facet;
  /* 3.14.6 Schema Component Constraint: Derivation Valid (Restriction, Simple) */
  /* Note: we return 0 here on success, and -1 on error (and set error info) */

  /* @implements(xmlschema-1:cos-st-restricts.3) @end */
  /* 3 If the {variety} is union, then  all of the following must be true: */

  /* @implements(xmlschema-1:cos-st-restricts.3.1)
     test { simpletype_union_badmembers1.test }
     test { simpletype_union_badmembers2.test }
     @end */
  /* 3.1 The {member type definitions} must all have {variety} of atomic or list. */
  for (l = t->members; l; l = l->next) {
    xs_member_type *mt = (xs_member_type*)l->data;
    assert(mt->type);
    assert(mt->type->complex || (XS_TYPE_VARIETY_INVALID != mt->type->variety));
    if (mt->type->complex || (XS_TYPE_VARIETY_UNION == mt->type->variety))
      return error(&s->ei,s->uri,t->def.loc.line,"cos-st-restricts.3.1",
                   "All member types of a union must be atomic or list types");
  }

  /* @implements(xmlschema-1:cos-st-restricts.3.2) @end */
  /* 3.2 (none) */
  /* @implements(xmlschema-1:cos-st-restricts.3.3) @end */
  /* 3.3 The appropriate case among the following must be true: */
  /* @implements(xmlschema-1:cos-st-restricts.3.3.1) @end */
  /* 3.3.1 If the {base type definition} is the simple ur-type definition, then all of the
           following must be true: */
  if (t->base == s->globals->simple_ur_type) {
    /* @implements(xmlschema-1:cos-st-restricts.3.3.1.1)
     test { simpletype_union_member_final_union.test }
     test { simpletype_union_member_final_union2.test }
     test { simpletype_union_member_final_union3.test }
     test { simpletype_union_member_final_union4.test }
     @end */
    /* 3.3.1.1 All of the {member type definitions} must have a {final} which does not contain
               union. */
    for (l = t->members; l; l = l->next) {
      xs_member_type *mt = (xs_member_type*)l->data;
      if (mt->type->final_union)
        return error(&s->ei,s->uri,t->def.loc.line,"cos-st-restricts.3.3.1.1",
                     "Member type %#n disallows use inside <union>",mt->type->def.ident);
    }

    /* @implements(xmlschema-1:cos-st-restricts.3.3.1.2) @end */
    /* 3.3.1.2 The {facets} must be empty. */
    /* No way to test this in the direct case... possibly still needed for particle derivation
       checks? */
    for (facet = 0; facet < XS_FACET_NUMFACETS; facet++)
      if (NULL != t->facets.strval[facet])
        error(&s->ei,s->uri,t->facets.defline[facet],"cos-st-restricts.3.3.1.2",
              "Union type derived from anySimpleType must not have any facets set");
  }
  /* @implements(xmlschema-1:cos-st-restricts.3.3.2) @end */
  /* 3.3.2 otherwise all of the following must be true: */
  else {

    /* @implements(xmlschema-1:cos-st-restricts.3.3.2.1) @end */
    /* 3.3.2.1 The {base type definition} must have a {variety} of union. */
    /* Can't really test this, it's enforced by the parser */
    if (t->base->complex || (XS_TYPE_VARIETY_UNION != t->base->variety))
      return error(&s->ei,s->uri,t->def.loc.line,"cos-st-restricts.3.3.2.1","Simple types with a "
                   "variety of union must have a base type which is also a union");

    /* @implements(xmlschema-1:cos-st-restricts.3.3.2.2) @end */
    /* 3.3.2.2 The {final} of the {base type definition} must not contain restriction. */
    /* Already covered by rule 3.14.6-1-3 */

    /* @implements(xmlschema-1:cos-st-restricts.3.3.2.3) @end */
    /* 3.3.2.3 The {member type definitions}, in order, must be validly derived from the
               corresponding type definitions in the {base type definition}'s {member type
               definitions} given the empty set, as defined in Type Derivation OK (Simple)
               (3.14.6). */
    /* This should be implicitly true, since the parser doesn't provide a way to create union types
       that are derived from other union types except by using <restriction>, which doesn't allow
       the declaration of a different set of member types */

    /* this loop is just for rules 3.3.2.4 and 3.3.2.5; rule 3.3.2.3 is not implemented */
    for (facet = 0; facet < XS_FACET_NUMFACETS; facet++) {
      if (NULL != t->facets.strval[facet]) {
        /* @implements(xmlschema-1:cos-st-restricts.3.3.2.4) @end */
        /* 3.3.2.4 Only pattern and enumeration facet components are allowed among the {facets}. */
        if ((XS_FACET_PATTERN != facet) &&
            (XS_FACET_ENUMERATION != facet))
          return error(&s->ei,s->uri,t->def.loc.line,"cos-st-restricts.3.3.2.4","Facet %s is not "
                       "allowed in a union or union-derived type",xs_facet_names[facet]);

        /* @implements(xmlschema-1:cos-st-restricts.3.3.2.5)
           test { simpletype_union_derived_okfacets.test }
           test { simpletype_union_derived_badfacet.test }
           @end */
        /* 3.3.2.5 For each facet in the {facets} (call this DF), if there is a facet of the same
                   kind in the {facets} of the {base type definition} (call this BF),then the DF's
                   {value} must be a valid restriction of BF's {value} as defined in [XML Schemas:
                   Datatypes]. */
        /* This is implicitly true, because only pattern and enumeration facets can be specified
           in a union, and these add additional items to the pattern_facets or enumeration_facets
           lists respectively, so it is not possible to have an invalid derivation of these. */
      }
    }


  }

  return 0;
}

int xs_check_simple_type_derivation_valid(xs_schema *s, xs_type *t)
{
  /* 3.14.6 Schema Component Constraint: Derivation Valid (Restriction, Simple) */
  /* Note: we return 0 here on success, and -1 on error (and set error info) */
  assert(!t->complex);
  assert(XS_TYPE_VARIETY_INVALID != t->variety);

  if (XS_TYPE_VARIETY_ATOMIC == t->variety) {
    CHECK_CALL(xs_check_simple_type_derivation_valid_atomic(s,t))
  }
  else if (XS_TYPE_VARIETY_LIST == t->variety) {
    CHECK_CALL(xs_check_simple_type_derivation_valid_list(s,t))
  }
  else {
    assert(XS_TYPE_VARIETY_UNION == t->variety);
    CHECK_CALL(xs_check_simple_type_derivation_valid_union(s,t))
  }

  return 0; /* success */
}

int xs_check_simple_type_derivation_ok(xs_schema *s, xs_type *D, xs_type *B,
                                    int final_extension, int final_restriction,
                                    int final_list, int final_union)
{
  /* 3.14.6 Schema Component Constraint: Type Derivation OK (Simple) */
  /* Note: we return 0 here on success, and -1 on error (and set error info) */
  assert(!D->complex);
  assert(XS_TYPE_VARIETY_INVALID != D->variety);

  /* For a simple type definition (call it D, for derived) to be validly derived from a type
     definition (call this B, for base) given a subset of {extension, restriction, list, union}
     (of which only restriction is actually relevant) one of the following must be true: */

  /* @implements(xmlschema-1:cos-st-derived-ok.1)
     test { simpletype_derivationok1.test }
     @end */
  /* 1 They are the same type definition. */
  if (D == B)
    return 0;

  /* @implements(xmlschema-1:cos-st-derived-ok.2) @end */
  /* 2 All of the following must be true: */
  /* @implements(xmlschema-1:cos-st-derived-ok.2.1)
     test { simpletype_derivationnotok3.test }
     @end */
  /* 2.1 restriction is not in the subset, or in the {final} of its own {base type definition}; */
  /* Note: base type case is already taken care of by 3.14.6-1-3 */
  if (final_restriction)
    return error(&s->ei,s->uri,D->def.loc.line,"cos-st-derived-ok.2.1",
                 "Restriction disallowed when deriving from %#n",B->def.ident);

  /* @implements(xmlschema-1:cos-st-derived-ok.2.2) @end */
  /* 2.2 One of the following must be true: */
  /* @implements(xmlschema-1:cos-st-derived-ok.2.2.1)
     test { simpletype_derivationok2.test }
     @end */
  /* 2.2.1 D's base type definition is B. */
  if (D->base == B)
    return 0;

  /* @implements(xmlschema-1:cos-st-derived-ok.2.2.3)
     test { simpletype_derivationok3.test }
     test { simpletype_derivationok4.test }
     @end */
  /* 2.2.3 D's {variety} is list or union and B is the simple ur-type definition. */
  if (((XS_TYPE_VARIETY_LIST == D->variety) || (XS_TYPE_VARIETY_UNION == D->variety)) &&
      (B == s->globals->simple_ur_type))
    return 0;

  /* @implements(xmlschema-1:cos-st-derived-ok.2.2.4)
     test { simpletype_derivationok5.test }
     @end */
  /* 2.2.4 B's {variety} is union and D is validly derived from a type definition in B's {member
           type definitions} given the subset, as defined by this constraint. */
  if (!B->complex && (XS_TYPE_VARIETY_UNION == B->variety)) {
    list *l;
    for (l = B->members; l; l = l->next) {
      xs_member_type *mt = (xs_member_type*)l->data;
      assert(mt->type);
      if (0 == xs_check_simple_type_derivation_ok(s,D,mt->type,final_extension,final_restriction,
                                                  final_list,final_union))
        return 0;
    }
  }

  /* @implements(xmlschema-1:cos-st-derived-ok.2.2.2)
     test { simpletype_derivationnotok1.test }
     test { simpletype_derivationnotok2.test }
     test { simpletype_derivationok6.test }
     test { simpletype_derivationok7.test }
     @end */
  /* 2.2.2 D's base type definition is not the ur-type definition and is validly derived from B
           given the subset, as defined by this constraint. */
  if ((D->base == s->globals->simple_ur_type) ||
      (0 != xs_check_simple_type_derivation_ok(s,D->base,B,final_extension,final_restriction,
                                                final_list,final_union)))
    return error(&s->ei,s->uri,D->def.loc.line,"cos-st-derived-ok.2.2.2",
                 "%#n cannot be derived from %#n",D->def.ident,B->def.ident);

  return 0;
}

int xs_create_simple_type_restriction(xs_schema *s, xs_type *base, int defline,
                                      xs_facetdata *facets, xs_type **restriction)
{
  /* @implements(xmlschema-1:st-restrict-facets.1) @end
     @implements(xmlschema-1:st-restrict-facets.2) @end
     @implements(xmlschema-1:st-restrict-facets.3)
     test { complextype_sc_content10.test }
     test { complextype_sc_content11.test }
     test { complextype_sc_content6.test }
     test { complextype_sc_content7.test }
     test { complextype_sc_content8.test }
     test { complextype_sc_content9.test }
     test { complextype_sc_d_content11.test }
     test { complextype_sc_d_content6.test }
     test { complextype_sc_d_content7.test }
     test { complextype_sc_d_content8.test }
     @end */
  int facet;
  xs_type *t = xs_type_new(s->as);
  list *l;
  t->def.loc.line = defline;
  t->stype = XS_TYPE_SIMPLE_RESTRICTION;
  t->base = base;
  t->variety = t->base->variety; /* FIXME: make sure this is set properly! */

  for (facet = 0; facet < XS_FACET_NUMFACETS; facet++) {
    if (facets->strval[facet])
      t->facets.strval[facet] = strdup(facets->strval[facet]);
    t->facets.intval[facet] = facets->intval[facet];
    t->facets.defline[facet] = facets->defline[facet];
  }

  for (l = facets->patterns; l; l = l->next)
    list_append(&t->facets.patterns,strdup((char*)l->data));

  for (l = facets->enumerations; l; l = l->next)
    list_append(&t->facets.enumerations,strdup((char*)l->data));

  *restriction = t;
  return 0;
}

xs_range xs_particle_effective_total_range(xs_particle *p)
{
  list *l;
  xs_range r;

  /* FIXME: check that this can only be called _after_ recursive group references have been
     checked */

  if ((XS_PARTICLE_TERM_ELEMENT == p->term_type) ||
      (XS_PARTICLE_TERM_WILDCARD == p->term_type)) {
    r.min_occurs = p->range.min_occurs;
    r.max_occurs = p->range.max_occurs;
  }
  else {
    assert(XS_PARTICLE_TERM_MODEL_GROUP == p->term_type);

    if ((XS_MODEL_GROUP_COMPOSITOR_ALL == p->term.mg->compositor) ||
        (XS_MODEL_GROUP_COMPOSITOR_SEQUENCE == p->term.mg->compositor)) {
      /* @implements(xmlschema-1:cos-seq-range)
         test { particle_range_all1.test }
         test { particle_range_all2.test }
         test { particle_range_sequence1.test }
         test { particle_range_sequence2.test }
         test { particle_range_sequence3.test }
         test { particle_range_sequence4.test }
         test { particle_range_sequence5.test }
         test { particle_range_sequence6.test }
         test { particle_range_sequence7.test }
         test { particle_range_sequence8.test }
         test { particle_range_sequence9.test }
         test { particle_range_sequence10.test }
         test { particle_range_sequence11.test }
         test { particle_range_sequence12.test }
         test { particle_range_sequence13.test }
         test { particle_range_sequence14.test }
         test { particle_range_sequence15.test }
         @end */
      r.min_occurs = 0;
      r.max_occurs = 0;
      for (l = p->term.mg->particles; l; l = l->next) {
        xs_particle *p2 = (xs_particle*)l->data;
        xs_range r2 = xs_particle_effective_total_range(p2);
        r.min_occurs += r2.min_occurs;
        if ((0 > r2.max_occurs) || (0 > p->range.max_occurs))
          r.max_occurs = -1;
        else if (0 <= r.max_occurs)
          r.max_occurs += r2.max_occurs;
      }
    }
    else {
      /* @implements(xmlschema-1:cos-choice-range)
         test { particle_range_choice1.test }
         test { particle_range_choice2.test }
         test { particle_range_choice3.test }
         test { particle_range_choice4.test }
         test { particle_range_choice5.test }
         test { particle_range_choice6.test }
         test { particle_range_choice7.test }
         test { particle_range_choice8.test }
         @end */
      r.min_occurs = -1;
      r.max_occurs = 0;
      assert(XS_MODEL_GROUP_COMPOSITOR_CHOICE == p->term.mg->compositor);
      for (l = p->term.mg->particles; l; l = l->next) {
        xs_particle *p2 = (xs_particle*)l->data;
        xs_range r2 = xs_particle_effective_total_range(p2);

        if ((0 > r.min_occurs) || (r.min_occurs > r2.min_occurs))
          r.min_occurs = r2.min_occurs;
        if ((0 > r2.max_occurs) || (0 > p->range.max_occurs))
          r.max_occurs = -1;
        else if ((0 <= r.max_occurs) && (r.max_occurs < r2.max_occurs))
          r.max_occurs = r2.max_occurs;
      }
    }
    if (0 > r.min_occurs)
      r.min_occurs = 0;
    r.min_occurs *= p->range.min_occurs;
    if (0 <= r.max_occurs)
      r.max_occurs *= p->range.max_occurs;
  }
  return r;
}

void particle_debug(char *str)
{
/*   debugl("%s",str); */
}

int xs_check_particle_valid_extension(xs_schema *s, xs_particle *E, xs_particle *B)
{
  /* 3.9.6 Schema Component Constraint: Particle Valid (Extension) */

  /* Note: There are no tests for this, because as far as I'm aware there's no way to construct
     a situation in which this would actually be violated. This is called from within
     xs_check_complex_extension(), which checks that a type claiming that another type is its
     base is a valid extension of that type. compute_content_type() handles the case of one
     complex type extending another (when both are non-empty), and creates the sequence which
     satisfies the constraints tested here. */

  assert(NULL != E);
  assert(NULL != B);

  /* [Definition:]  For a particle (call it E, for extension) to be a valid extension of another
     particle (call it B, for base)  one of the following must be true: */

  /* @implements(xmlschema-1:cos-particle-extend.1) @end */
  /* 1 They are the same particle. */
  if (E == B)
    return 0; /* success */

  /* @implements(xmlschema-1:cos-particle-extend.2) @end */
  /* 2 E's {min occurs}={max occurs}=1 and its {term} is a sequence group whose {particles}' first 
       member is a particle all of whose properties, recursively, are identical to those of B, with
       the exception of {annotation} properties. */

  if ((1 != E->range.min_occurs) || (1 != E->range.max_occurs))
    return error(&s->ei,s->uri,E->defline,"cos-particle-extend.2","Particle range is not 1-1");

  if ((XS_PARTICLE_TERM_MODEL_GROUP != E->term_type) ||
      (XS_MODEL_GROUP_COMPOSITOR_SEQUENCE != E->term.mg->compositor))
    return error(&s->ei,s->uri,E->defline,"cos-particle-extend.2",
                 "Particle is not a sequence group");

  if (NULL == E->term.mg->particles)
    return error(&s->ei,s->uri,E->defline,"cos-particle-extend.2",
                 "Sequence group has no particles");

  /* Note: The wording in the spec suggests that a deep comparison is necessary here, i.e.
     looking at all the properties recursively and comparing them. But since this is only
     used for checking if a complex type is a valid extension of another, and rule 2 is
     exactly what is done when constructing a derived type, comparing to see if they are
     the same object is sufficient. */
  if ((xs_particle*)E->term.mg->particles->data != B)
    return error(&s->ei,s->uri,E->defline,"cos-particle-extend.2",
                 "First particle in the sequence is not equal to base");

  return 0; /* success */
}

int occurrence_range_restriction_ok(xs_range R, xs_range B)
{
  /* @implements(xmlschema-1:range-ok.1) @end */
  /* @implements(xmlschema-1:range-ok.2) @end */
  /* @implements(xmlschema-1:range-ok.2.1) @end */
  /* @implements(xmlschema-1:range-ok.2.2) @end */

  /* 3.9.6 Schema Component Constraint: Occurrence Range OK

     For a particle's occurrence range to be a valid restriction of another's occurrence range all
     of the following must be true: */

  /* 1 Its {min occurs} is greater than or equal to the other's {min occurs}.
     2 one of the following must be true:
     2.1 The other's {max occurs} is unbounded.
     2.2 Both {max occurs} are numbers, and the particle's is less than or equal to the other's. */

  if ((R.min_occurs >= B.min_occurs) &&
      ((0 > B.max_occurs) ||
       ((0 <= R.max_occurs) && (0 <= B.max_occurs) && (R.max_occurs <= B.max_occurs))))
    return 1;
  else
    return 0;
}

int xs_check_range_restriction_ok(xs_schema *s, xs_particle *R, xs_particle *B, char *constraint)
{
  if (!occurrence_range_restriction_ok(R->range,B->range)) {
    char *rrangestr = xs_range_str(R->range);
    char *brangestr = xs_range_str(B->range);
    char *bstr = xs_particle_str(B);
    error(&s->ei,s->uri,R->defline,constraint,"Occurrence range %s is not a valid restriction of "
          "the range %s, declared on the corresponding %s of the base type",
          rrangestr,brangestr,bstr);
    free(rrangestr);
    free(brangestr);
    free(bstr);
    return -1;
  }
  return 0;
}

int ns_valid_wrt_wildcard(xs_schema *s, char *ns, xs_wildcard *w)
{
  /* 3.10.4: Validation Rule: Wildcard allows Namespace Name */
  return ((XS_WILDCARD_TYPE_ANY == w->type) ||               /* 1 */
          ((XS_WILDCARD_TYPE_NOT == w->type) &&
           (NULL != ns) &&
           ((NULL == w->not_ns) || strcmp(ns,w->not_ns))) || /* 2 */
          ((XS_WILDCARD_TYPE_SET == w->type) &&
           list_contains_string(w->nslist,ns)));             /* 3 */
}

int particle_restriction_ok_elt_elt_nameandtypeok(xs_schema *s, xs_element *R, xs_range Rrange,
                                                  int Rdefline,
                                                  xs_element *B, xs_range Brange,
                                                  int Bdefline)
{
  /* 3.9.6 Schema Component Constraint: Particle Restriction OK (Elt:Elt -- NameAndTypeOK) */

  particle_debug("particle_restriction_ok_elt_elt_nameandtypeok");

  /* For an element declaration particle to be a valid restriction of another element declaration
     particle all of the following must be true: */

  /* @implements(xmlschema-1:rcase-NameAndTypeOK.1)
     test { particle_restriction_ee_name.test }
     @end */
  /* 1 The declarations' {name}s and {target namespace}s are the same. */
  if (!nsname_equals(R->def.ident,B->def.ident))
    return error(&s->ei,s->uri,Rdefline,"rcase-NameAndTypeOK.1","Element %#n does not match "
                 "corresponding element %#n on base type",R->def.ident,B->def.ident);

  /* @implements(xmlschema-1:rcase-NameAndTypeOK.2)
     test { particle_restriction_ee_range1.test }
     test { particle_restriction_ee_range2.test }
     @end */
  /* 2 R's occurrence range is a valid restriction of B's occurrence range as defined by
     Occurrence Range OK (3.9.6). */
  if (!occurrence_range_restriction_ok(Rrange,Brange)) {
    char *rrangestr = xs_range_str(Rrange);
    char *brangestr = xs_range_str(Brange);
    error(&s->ei,s->uri,Rdefline,"rcase-NameAndTypeOK.2","Occurrence range %s is not a valid "
          "restriction of the range %s, declared on the corresponding %#n element on the base type",
          rrangestr,brangestr,B->def.ident);
    free(rrangestr);
    free(brangestr);
    return -1;
  }

  /* @implements(xmlschema-1:rcase-NameAndTypeOK.3) @end */
  /* @implements(xmlschema-1:rcase-NameAndTypeOK.3.1)
     test { particle_restriction_ee_toplevel.test }
     @end */
  /* 3 One of the following must be true: */
  /* 3.1 Both B's declaration's {scope} and R's declaration's {scope} are global. */
  if (R->toplevel && B->toplevel)
    return 0;

  /* @implements(xmlschema-1:rcase-NameAndTypeOK.3.2) @end */
  /* 3.2 All of the following must be true: */

  /* @implements(xmlschema-1:rcase-NameAndTypeOK.3.2.1)
     test { particle_restriction_ee_nillable1.test }
     test { particle_restriction_ee_nillable2.test }
     test { particle_restriction_ee_nillable3.test }
     test { particle_restriction_ee_nillable4.test }
     @end */
  /* 3.2.1 Either B's {nillable} is true or R's {nillable} is false. */
  if (!B->nillable && R->nillable)
    return error(&s->ei,s->uri,Rdefline,"rcase-NameAndTypeOK.3.2.1","This element cannot be "
                 "declared nillable, because the corresponding element declaration on the base "
                 "type does not specify nillable");

  /* @implements(xmlschema-1:rcase-NameAndTypeOK.3.2.2)
     test { particle_restriction_ee_fixed1.test }
     test { particle_restriction_ee_fixed2.test }
     test { particle_restriction_ee_fixed3.test }
     test { particle_restriction_ee_fixed4.test }
     test { particle_restriction_ee_fixed5.test }
     test { particle_restriction_ee_fixed6.test }
     @end */
  /* 3.2.2 either B's declaration's {value constraint} is absent, or is not fixed, or R's
           declaration's {value constraint} is fixed with the same value. */
  if ((XS_VALUE_CONSTRAINT_FIXED == B->vc.type) &&
      ((XS_VALUE_CONSTRAINT_FIXED != R->vc.type) ||
       strcmp(B->vc.value,R->vc.value)))
    return error(&s->ei,s->uri,Rdefline,"rcase-NameAndTypeOK.3.2.2","This element must be declared "
                 "with a fixed value of \"%s\", as is required by the corresponding element "
                 "declaration on the base type",B->vc.value);

  /* @implements(xmlschema-1:rcase-NameAndTypeOK.3.2.3)
     status { deferred } issue { requires support for identity-constraint definitions } @end */
  /* 3.2.3 R's declaration's {identity-constraint definitions} is a subset of B's declaration's
           {identity-constraint definitions}, if any. */

  /* @implements(xmlschema-1:rcase-NameAndTypeOK.3.2.4)
     test { particle_restriction_ee_block1.test }
     test { particle_restriction_ee_block2.test }
     test { particle_restriction_ee_block3.test }
     test { particle_restriction_ee_block4.test }
     test { particle_restriction_ee_block5.test }
     test { particle_restriction_ee_block6.test }
     test { particle_restriction_ee_block7.test }
     @end */
  /* 3.2.4 R's declaration's {disallowed substitutions} is a superset of B's declaration's
           {disallowed substitutions}. */
  if ((B->disallow_substitution && !R->disallow_substitution) ||
      (B->disallow_extension && !R->disallow_extension) ||
      (B->disallow_restriction && !R->disallow_restriction)) {
    return error(&s->ei,s->uri,Rdefline,"rcase-NameAndTypeOK.3.2.4","This element must declare in "
                 "its \"block\" attribute a superset of the items blocked by the corresponding "
                 "element declaration on the base type");
  }

  /* @implements(xmlschema-1:rcase-NameAndTypeOK.3.2.5)
     test { particle_restriction_ee_type1.test }
     test { particle_restriction_ee_type2.test }
     test { particle_restriction_ee_type3.test }
     test { particle_restriction_ee_type4.test }
     test { particle_restriction_ee_type5.test }
     test { particle_restriction_ee_type6.test }
     test { particle_restriction_ee_type7.test }
     test { particle_restriction_ee_type8.test }
     test { particle_restriction_ee_type9.test }
     @end */
  /* 3.2.5 R's {type definition} is validly derived given {extension, list, union} from B's
           {type definition} as defined by Type Derivation OK (Complex) (3.4.6) or Type Derivation 
           OK (Simple) (3.14.6), as appropriate. */
  assert(R->type);
  assert(B->type);
  if ((R->type->complex && (0 != xs_check_complex_type_derivation_ok(s,R->type,B->type,1,0))) ||
      (!R->type->complex && (0 != xs_check_simple_type_derivation_ok(s,R->type,B->type,1,0,1,1)))) {

    if (B->type->def.ident.name)
      error(&s->ei,s->uri,Rdefline,"rcase-NameAndTypeOK.3.2.5",
            "Element type must be a valid derivation of %#n",B->type->def.ident);
    else
      error(&s->ei,s->uri,Rdefline,"rcase-NameAndTypeOK.3.2.5","Element type must be a valid "
            "derivation of the type declared for the corresponding element on the base type");
    return -1;
  }

  /* Note: The above constraint on {type definition} means that in deriving a type by
     restriction, any contained type definitions must themselves be explicitly derived by
     restriction from the corresponding type definitions in the base definition, or be one of
     the member types of a corresponding union. */

  return 0;
}

int particle_derivation_ok_elt_any_nscompat(xs_schema *s, xs_particle *R, xs_particle *B)
{
  assert(XS_PARTICLE_TERM_ELEMENT == R->term_type);
  assert(XS_PARTICLE_TERM_WILDCARD == B->term_type);

  particle_debug("particle_derivation_ok_elt_any_nscompat");


  /* Schema Component Constraint: Particle Derivation OK (Elt:Any -- NSCompat)

     For an element declaration particle to be a valid restriction of a wildcard particle all of
     the following must be true: */

  /* @implements(xmlschema-1:rcase-NSCompat.1)
     test { particle_restriction_element_nscompat1.test }
     test { particle_restriction_element_nscompat2.test }
     test { particle_restriction_element_nscompat3.test }
     test { particle_restriction_element_nscompat4.test }
     test { particle_restriction_element_nscompat5.test }
     @end */
  /* 1 The element declaration's {target namespace} is valid with respect to the wildcard's
       {namespace constraint} as defined by Wildcard allows Namespace Name (3.10.4). */
  if (!ns_valid_wrt_wildcard(s,R->term.e->def.ident.ns,B->term.w)) {
    char *rstr = xs_particle_str(R);
    char *bstr = xs_particle_str(B);
    error(&s->ei,s->uri,R->defline,"rcase-NSCompat.1","Element %s is not a valid restriction of "
          "the corresponding %s in the base type, because it is not valid with respect to the "
          "namespace constraint",rstr,bstr);
    free(rstr);
    free(bstr);
    return -1;
  }

  /* @implements(xmlschema-1:rcase-NSCompat.2)
     test { particle_restriction_element_nscompat_range1.test }
     test { particle_restriction_element_nscompat_range2.test }
     test { particle_restriction_element_nscompat_range3.test }
     @end */
  /* 2 R's occurrence range is a valid restriction of B's occurrence range as defined by
       Occurrence Range OK (3.9.6). */
  CHECK_CALL(xs_check_range_restriction_ok(s,R,B,"rcase-NSCompat.2"))

  return 0;
}

int particle_derivation_ok_all_choice_sequence_recurseasifgroup(xs_schema *s,
                                                                xs_particle *R, xs_particle *B)
{
  xs_particle tp;
  xs_model_group tmg;
  list tl;
  assert(XS_PARTICLE_TERM_ELEMENT == R->term_type);
  assert(XS_PARTICLE_TERM_MODEL_GROUP == B->term_type);
  particle_debug("particle_derivation_ok_all_choice_sequence_recurseasifgroup");

/*   debugl("B is %s",xs_particle_str(B)); */

  /* @implements(xmlschema-1:rcase-RecurseAsIfGroup)
     test { particle_restriction_recurseasifgroup1.test }
     test { particle_restriction_recurseasifgroup2.test }
     test { particle_restriction_recurseasifgroup3.test }
     test { particle_restriction_recurseasifgroup4.test }
     test { particle_restriction_recurseasifgroup5.test }
     test { particle_restriction_recurseasifgroup6.test }
     test { particle_restriction_recurseasifgroup7.test }
     test { particle_restriction_recurseasifgroup8.test }
     test { particle_restriction_recurseasifgroup9.test }
     test { particle_restriction_recurseasifgroup10.test }
     @end */
  /* Schema Component Constraint: Particle Derivation OK
     (Elt:All/Choice/Sequence -- RecurseAsIfGroup)

     For an element declaration particle to be a valid restriction of a group particle (all,
     choice or sequence) a group particle of the variety corresponding to B's, with {min occurs}
     and {max occurs} of 1 and with {particles} consisting of a single particle the same as the
     element declaration must be a valid restriction of the group as defined by Particle Derivation
     OK (All:All,Sequence:Sequence -- Recurse) (3.9.6), Particle Derivation OK (Choice:Choice --
     RecurseLax) (3.9.6) or Particle Derivation OK (All:All,Sequence:Sequence -- Recurse) (3.9.6),
     depending on whether the group is all, choice or sequence.
  */

  memset(&tp,0,sizeof(xs_particle));
  memset(&tmg,0,sizeof(xs_model_group));
  memset(&tl,0,sizeof(list));
  tp.range.min_occurs = 1;
  tp.range.max_occurs = 1;
  tp.term_type = XS_PARTICLE_TERM_MODEL_GROUP;
  tp.term.mg = &tmg;
  tp.defline = R->defline;
  tmg.compositor = B->term.mg->compositor;
  tmg.particles = &tl;
  tmg.defline = R->defline;
  tl.data = R;

  return xs_check_particle_valid_restriction(s,&tp,B);
}

int particle_derivation_ok_any_any_nssubset(xs_schema *s, xs_particle *R, xs_particle *B)
{
  assert(XS_PARTICLE_TERM_WILDCARD == R->term_type);
  assert(XS_PARTICLE_TERM_WILDCARD == B->term_type);
  particle_debug("particle_derivation_ok_any_any_nssubset");
  /* Schema Component Constraint: Particle Derivation OK (Any:Any -- NSSubset)

     For a wildcard particle to be a valid restriction of another wildcard particle all of the
     following must be true: */

  /* @implements(xmlschema-1:rcase-NSSubset.1)
     test { particle_restriction_any_nssubset_range1.test }
     test { particle_restriction_any_nssubset_range2.test }
     test { particle_restriction_any_nssubset_range3.test }
     @end */
  /* 1 R's occurrence range must be a valid restriction of B's occurrence range as defined by
       Occurrence Range OK (@3.9.6). */
  CHECK_CALL(xs_check_range_restriction_ok(s,R,B,"rcase-NSSubset.1"))

  /* @implements(xmlschema-1:rcase-NSSubset.2)
     test { particle_restriction_any_nssubset1.test }
     test { particle_restriction_any_nssubset2.test }
     test { particle_restriction_any_nssubset3.test }
     test { particle_restriction_any_nssubset4.test }
     test { particle_restriction_any_nssubset5.test }
     @end */
  /* 2 R's {namespace constraint} must be an intensional subset of B's {namespace constraint} as
       defined by Wildcard Subset (3.10.6). */
  if (!xs_wildcard_constraint_is_subset(s,R->term.w,B->term.w)) {
    return error(&s->ei,s->uri,R->defline,"rcase-NSSubset.2","Element wildcard is not a valid "
                 "restriction of the corresponding wildcard in the base type, because its "
                 "namespace constraint is not an intensional subset of the base wildcard's");
  }

  /* @implements(xmlschema-1:rcase-NSSubset.3)
     test { particle_restriction_any_nssubset_processcontents1.test }
     test { particle_restriction_any_nssubset_processcontents2.test }
     test { particle_restriction_any_nssubset_processcontents3.test }
     test { particle_restriction_any_nssubset_processcontents4.test }
     test { particle_restriction_any_nssubset_processcontents5.test }
     test { particle_restriction_any_nssubset_processcontents6.test }
     @end */
  /* 3 Unless B is the content model wildcard of the ur-type definition, R's {process contents}
       must be identical to or stronger than B's {process contents}, where strict is stronger than
       lax is stronger than skip. */
  if ((B->term.w != s->globals->complex_ur_type_element_wildcard) &&
      (R->term.w->process_contents < B->term.w->process_contents)) {
    return error(&s->ei,s->uri,R->defline,"rcase-NSSubset.3","Element wildcard is not a valid "
                 "restriction of the corresponding wildcard in the base type, because it's "
                 "processContents is weaker, in the ordering of \"skip\" (weakest), \"lax\", and "
                 "\"strict\" (strongest)");
  }
  return 0;
}

int particle_derivation_ok_all_choice_sequence_any_nsrecursecheckcardinality(xs_schema *s,
                                                                             xs_particle *R,
                                                                             xs_particle *B)
{
  list *l;
  xs_range etr;
  assert(XS_PARTICLE_TERM_MODEL_GROUP == R->term_type);
  assert(XS_PARTICLE_TERM_WILDCARD == B->term_type);
  particle_debug("particle_derivation_ok_all_choice_sequence_any_nsrecursecheckcardinality");

  /* Schema Component Constraint: Particle Derivation OK
     (All/Choice/Sequence:Any -- NSRecurseCheckCardinality)

     For a group particle to be a valid restriction of a wildcard particle all of the following
     must be true: */

  /* @implements(xmlschema-1:rcase-NSRecurseCheckCardinality.1)
     test { particle_restriction_nsrecursecheckcardinality6.test }
     @end */
  /* 1 Every member of the {particles} of the group is a valid restriction of the wildcard as
       defined by Particle Valid (Restriction) (3.9.6). */
  for (l = R->term.mg->particles; l; l = l->next) {
    xs_particle *p = (xs_particle*)l->data;
    CHECK_CALL(xs_check_particle_valid_restriction(s,p,B))
  }

  /* @implements(xmlschema-1:rcase-NSRecurseCheckCardinality.2)
     test { particle_restriction_nsrecursecheckcardinality1.test }
     test { particle_restriction_nsrecursecheckcardinality2.test }
     test { particle_restriction_nsrecursecheckcardinality3.test }
     test { particle_restriction_nsrecursecheckcardinality4.test }
     test { particle_restriction_nsrecursecheckcardinality5.test }
     @end */
  /* 2 The effective total range of the group, as defined by Effective Total Range (all and
       sequence) (3.8.6) (if the group is all or sequence) or Effective Total Range (choice)
       (3.8.6) (if it is choice) is a valid restriction of B's occurrence range as defined by
       Occurrence Range OK (3.9.6). */

  etr = xs_particle_effective_total_range(R);
  if (!occurrence_range_restriction_ok(etr,B->range)) {
    char *rstr = xs_particle_str(R);
    char *etrstr = xs_range_str(etr);
    char *brangestr = xs_range_str(B->range);
    error(&s->ei,s->uri,R->defline,"rcase-NSRecurseCheckCardinality.2","%s effective total range "
          "(%s) is not a valid restriction of the range %s declared for the corresponding <any> "
          "within the base type",rstr,etrstr,brangestr);
    free(rstr);
    free(etrstr);
    free(brangestr);
    return -1;
  }

  return 0;
}

int xs_compute_particle_mapping(xs_schema *s, xs_particle *from, xs_particle *to,
                                int order_preserving,
                                int require_nonemptiable_unique,
                                int require_nonemptiable_mapped,
                                char *constraint)
{
  int fromcount = list_count(from->term.mg->particles);
  int tocount = list_count(to->term.mg->particles);
  int frompos;
  int topos;
  list *l;
  int i;
  error_info first_error;

  xs_particle **fromparticles = (xs_particle**)alloca(fromcount*sizeof(xs_particle*));
  xs_particle **toparticles = (xs_particle**)alloca(tocount*sizeof(xs_particle*));
  int *mapping = (int*)alloca(fromcount*sizeof(int));
  int *tomapped = (int*)alloca(tocount*sizeof(int));
  memset(tomapped,0,tocount*sizeof(int));
  memset(&first_error,0,sizeof(error_info));

  /* See calling functions for the testcases and constraint ids relevant to this function */

  if ((0 == tocount) && (0 == fromcount))
    return 0;

  /* FIXME: write tests for this */
  if (0 == tocount)
    return error(&s->ei,s->uri,from->defline,"constraint","Base type does not allow content here");

  frompos = 0;
  for (l = from->term.mg->particles; l; l = l->next)
    fromparticles[frompos++] = (xs_particle*)l->data;
  assert(frompos == fromcount);

  topos = 0;
  for (l = to->term.mg->particles; l; l = l->next)
    toparticles[topos++] = (xs_particle*)l->data;
  assert(topos == tocount);

/*   debugl("mapping: fromcount=%d, tocount=%d",fromcount,tocount); */
  frompos = 0;
  mapping[0] = 0;
  while (frompos < fromcount) {
    /* find the first element in B that r can validly map to */
    int found = 0;
    int retry = 0;

    for (topos = mapping[frompos]; topos < tocount; topos++) {
      int try = 1;
      if (require_nonemptiable_unique) {
        int f;
        for (f = 0; f < frompos; f++)
          if (mapping[f] == topos)
            try = 0;
      }

/*       if (try) */
/*         debugl("mapping: trying %d -> %d",frompos,topos); */
/*       else */
/*         debugl("mapping: NOT trying %d -> %d",frompos,topos); */

      if (try &&
          (0 == xs_check_particle_valid_restriction(s,fromparticles[frompos],toparticles[topos]))) {

/*         debugl("mapping: candidate %d -> %d",frompos,topos); */
        found = 1;
        mapping[frompos] = topos;
        if (frompos+1 < fromcount) {
          if (order_preserving)
            mapping[frompos+1] = topos+1;
          else
            mapping[frompos+1] = 0;
        }
        frompos++;
        break;
      }
      else if (!particle_emptiable(s,toparticles[topos]) && order_preserving) {
        if (NULL == first_error.message)
          error_info_copy(&first_error,&s->ei);
        if (require_nonemptiable_mapped) {
          if (0 < frompos) {
            frompos--;
            mapping[frompos]++;
/*             debugl("mapping: item %d in to list in the way; retrying from frompos=%d, topos %d", */
/*                   topos,frompos,mapping[frompos]); */
            retry = 1;
          }
          break;
        }
      }
      else {
/*         debugl("mapping: non-candidate %d -> %d",frompos,topos); */
      }
    }
    if (!found && !retry) {
/*       debugl("mapping: failed"); */
      if (!order_preserving && !require_nonemptiable_unique) {
        char *fpstr = xs_particle_str(fromparticles[frompos]);
        error(&s->ei,s->uri,fromparticles[frompos]->defline,constraint,
              "A match for %s does not exist in the base type",fpstr);
        free(fpstr);
      }
      else if (NULL != first_error.message) {
        error_info_free_vals(&s->ei);
        error_info_copy(&s->ei,&first_error);
      }
      else if (NULL == s->ei.message) {
        /* No error yet; this means that xs_check_particle_valid_restriction() has not found
           any problems, from which we can deduce that the reason for the failure was that the
           particle we are looking for actually does not exist in the base at all. */
        char *fpstr = xs_particle_str(fromparticles[frompos]);
        error(&s->ei,s->uri,fromparticles[frompos]->defline,constraint,"%s does not exist in the "
              "base type, or does not reside at the same position in the base type as it does here",
              fpstr);
        free(fpstr);
      }
      error_info_free_vals(&first_error);
      return -1;
    }
  }

/*   debugl("mapping: succeeded"); */

  assert(frompos == fromcount);
  for (i = 0; i < fromcount; i++) {
/*     debugl("mapping: final %d = %d",i,mapping[i]); */
    tomapped[mapping[i]]++;
  }
/*   for (i = 0; i < tocount; i++) */
/*     debugl("mapping: tomapped[%d] = %d",i,tomapped[i]); */

  if (require_nonemptiable_mapped) {
    for (i = 0; i < tocount; i++)
      if (!particle_emptiable(s,toparticles[i]) && (0 == tomapped[i])) {
        char *tpstr = xs_particle_str(toparticles[i]);
        int previous;
        xs_particle *prev = NULL;
        for (previous = i-1; (0 <= previous) && (NULL == prev); previous--) {
          for (frompos = 0; (frompos < fromcount) && (NULL == prev); frompos++)
            if (mapping[frompos] == previous)
              prev = fromparticles[frompos];
        }

        if ((NULL != prev) && order_preserving) {
          char *prevstr = xs_particle_str(prev);
          error(&s->ei,s->uri,prev->defline,constraint,"Expected %s to appear after %s, as it is "
                "present in the base type with a minOccurs value greater than 0",tpstr,prevstr);
          free(prevstr);
        }
        else {
          char *fromstr = xs_particle_str(from);
          error(&s->ei,s->uri,from->defline,constraint,"Expected %s to appear within this %s, as "
                "it is present in the base type with a minOccurs value greater than 0",
                tpstr,fromstr);
          free(fromstr);
        }

        free(tpstr);
        error_info_free_vals(&first_error);
        return -1;
      }
  }

  error_info_free_vals(&first_error);
  return 0;
}

int particle_derivation_ok_all_all_sequence_sequence_recurse(xs_schema *s, xs_particle *R,
                                                             xs_particle *B)
{

  assert(XS_PARTICLE_TERM_MODEL_GROUP == R->term_type);
  assert(XS_PARTICLE_TERM_MODEL_GROUP == B->term_type);
  particle_debug("particle_derivation_ok_all_all_sequence_sequence_recurse");

  /* Schema Component Constraint: Particle Derivation OK (All:All,Sequence:Sequence -- Recurse)

     For an all or sequence group particle to be a valid restriction of another group particle
     with the same {compositor} all of the following must be true: */

  /* @implements(xmlschema-1:rcase-Recurse.1)
     test { particle_restriction_sequence_recurse_range1.test }
     test { particle_restriction_sequence_recurse_range2.test }
     test { particle_restriction_sequence_recurse_range3.test }
     test { particle_restriction_all_recurse_range1.test }
     test { particle_restriction_all_recurse_range2.test }
     @end */
  /* 1 R's occurrence range is a valid restriction of B's occurrence range as defined by
       Occurrence Range OK (3.9.6). */

  CHECK_CALL(xs_check_range_restriction_ok(s,R,B,"rcase-Recurse.1"))

  /* @implements(xmlschema-1:rcase-Recurse.2)
     test { particle_restriction_sequence_recurse_mapping1.test }
     test { particle_restriction_sequence_recurse_mapping2.test }
     test { particle_restriction_sequence_recurse_mapping3.test }
     test { particle_restriction_sequence_recurse_mapping4.test }
     test { particle_restriction_sequence_recurse_mapping5.test }
     test { particle_restriction_sequence_recurse_mapping6.test }
     test { particle_restriction_sequence_recurse_mapping7.test }
     test { particle_restriction_sequence_recurse_mapping8.test }
     test { particle_restriction_sequence_recurse_mapping9.test }
     test { particle_restriction_sequence_recurse_mapping10.test }
     test { particle_restriction_sequence_recurse_mapping11.test }
     test { particle_restriction_sequence_recurse_mapping12.test }
     test { particle_restriction_sequence_recurse_mapping13.test }
     test { particle_restriction_sequence_recurse_mapping14.test }
     test { particle_restriction_all_recurse_mapping1.test }
     test { particle_restriction_all_recurse_mapping2.test }
     test { particle_restriction_all_recurse_mapping3.test }
     test { particle_restriction_all_recurse_mapping4.test }
     test { particle_restriction_all_recurse_mapping9.test }
     test { particle_restriction_all_recurse_mapping14.test }
     @end */
  /* @implements(xmlschema-1:rcase-Recurse.2.1) @end */
  /* @implements(xmlschema-1:rcase-Recurse.2.2) @end */
/*   2 There is a complete order-preserving functional mapping from the particles in the
       {particles} of R to the particles in the {particles} of B such that all of the following
       must be true:
     2.1 Each particle in the {particles} of R is a valid restriction of the particle in the
         {particles} of B it maps to as defined by Particle Valid (Restriction) (3.9.6).
     2.2 All particles in the {particles} of B which are not mapped to by any particle in the
         {particles} of R are emptiable as defined by Particle Emptiable (3.9.6).
         Note: Although the validation semantics of an all group does not depend on the order of
         its particles, derived all groups are required to match the order of their base in order
         to simplify checking that the derivation is OK. */

  CHECK_CALL(xs_compute_particle_mapping(s,R,B,1,0,1,"rcase-Recurse.2"))

  /*  [Definition:]  A complete functional mapping is order-preserving if each particle r in the
     domain R maps to a particle b in the range B which follows (not necessarily immediately) the
     particle in the range B mapped to by the predecessor of r, if any, where "predecessor" and
     "follows" are defined with respect to the order of the lists which constitute R and B.
  */

  /* FIXME */

  return 0;
}

int particle_derivation_ok_choice_choice_recurselax(xs_schema *s, xs_particle *R, xs_particle *B)
{
  particle_debug("particle_derivation_ok_choice_choice_recurselax");

  /* Schema Component Constraint: Particle Derivation OK (Choice:Choice -- RecurseLax)

     For a choice group particle to be a valid restriction of another choice group particle all of
     the following must be true: */

  /* @implements(xmlschema-1:rcase-RecurseLax.1)
     test { particle_restriction_choice_recurselax_range1.test }
     test { particle_restriction_choice_recurselax_range2.test }
     test { particle_restriction_choice_recurselax_range3.test }
     @end */
  /* 1 R's occurrence range is a valid restriction of B's occurrence range as defined by
       Occurrence Range OK (3.9.6); */
  CHECK_CALL(xs_check_range_restriction_ok(s,R,B,"rcase-RecurseLax.1"))

  /* @implements(xmlschema-1:rcase-RecurseLax.2)
     test { particle_restriction_choice_recurselax_mapping1.test }
     test { particle_restriction_choice_recurselax_mapping2.test }
     test { particle_restriction_choice_recurselax_mapping3.test }
     test { particle_restriction_choice_recurselax_mapping4.test }
     test { particle_restriction_choice_recurselax_mapping5.test }
     test { particle_restriction_choice_recurselax_mapping6.test }
     test { particle_restriction_choice_recurselax_mapping7.test }
     test { particle_restriction_choice_recurselax_mapping8.test }
     test { particle_restriction_choice_recurselax_mapping9.test }
     test { particle_restriction_choice_recurselax_mapping10.test }
     test { particle_restriction_choice_recurselax_mapping11.test }
     test { particle_restriction_choice_recurselax_mapping12.test }
     test { particle_restriction_choice_recurselax_mapping13.test }
     @end */
  /* 2 There is a complete order-preserving functional mapping from the particles in the
       {particles} of R to the particles in the {particles} of B such that each particle in the
       {particles} of R is a valid restriction of the particle in the {particles} of B it maps to
       as defined by Particle Valid (Restriction) (3.9.6).
       Note: Although the validation semantics of a choice group does not depend on the order of
       its particles, derived choice groups are required to match the order of their base in order
       to simplify checking that the derivation is OK.
  */
  CHECK_CALL(xs_compute_particle_mapping(s,R,B,1,0,0,"rcase-RecurseLax.2"))

  /* FIXME */
  return 0;
}

int particle_derivation_ok_sequence_all_recurseunordered(xs_schema *s, xs_particle *R,
                                                         xs_particle *B)
{

  particle_debug("particle_derivation_ok_sequence_all_recurseunordered");

  /* Schema Component Constraint: Particle Derivation OK (Sequence:All -- RecurseUnordered)

     For a sequence group particle to be a valid restriction of an all group particle all of the
     following must be true: */

  /* @implements(xmlschema-1:rcase-RecurseUnordered.1)
     test { particle_restriction_recurseunordered_range1.test }
     test { particle_restriction_recurseunordered_range2.test }
     test { particle_restriction_recurseunordered_range3.test }
     @end */
  /* 1 R's occurrence range is a valid restriction of B's occurrence range as defined by
       Occurrence Range OK (3.9.6). */
  CHECK_CALL(xs_check_range_restriction_ok(s,R,B,"rcase-RecurseUnordered.1"))

  /* @implements(xmlschema-1:rcase-RecurseUnordered.2)
     test { particle_restriction_recurseunordered1.test }
     test { particle_restriction_recurseunordered2.test }
     test { particle_restriction_recurseunordered3.test }
     test { particle_restriction_recurseunordered4.test }
     test { particle_restriction_recurseunordered5.test }
     test { particle_restriction_recurseunordered6.test }
     test { particle_restriction_recurseunordered7.test }
     test { particle_restriction_recurseunordered8.test }
     test { particle_restriction_recurseunordered9.test }
     test { particle_restriction_recurseunordered10.test }
     @end */
  /* @implements(xmlschema-1:rcase-RecurseUnordered.2.1) @end */
  /* @implements(xmlschema-1:rcase-RecurseUnordered.2.2) @end */
  /* @implements(xmlschema-1:rcase-RecurseUnordered.2.3) @end */
  /* 2 There is a complete functional mapping from the particles in the {particles} of R to the
       particles in the {particles} of B such that all of the following must be true:
     2.1 No particle in the {particles} of B is mapped to by more than one of the particles in the
         {particles} of R;
     2.2 Each particle in the {particles} of R is a valid restriction of the particle in the
         {particles} of B it maps to as defined by Particle Valid (Restriction) (3.9.6);
     2.3 All particles in the {particles} of B which are not mapped to by any particle in the
         {particles} of R are emptiable as defined by Particle Emptiable (3.9.6).
         Note: Although this clause allows reordering, because of the limits on the contents of
         all groups the checking process can still be deterministic.
  */

  CHECK_CALL(xs_compute_particle_mapping(s,R,B,0,1,1,"rcase-RecurseUnordered.2"))

  return 0;
}

int particle_derivation_ok_sequence_choice_mapandsum(xs_schema *s, xs_particle *R, xs_particle *B)
{
  xs_range Rrange2;
  particle_debug("particle_derivation_ok_sequence_choice_mapandsum");
  assert(XS_PARTICLE_TERM_MODEL_GROUP == R->term_type);
  assert(XS_PARTICLE_TERM_MODEL_GROUP == B->term_type);
  assert(XS_MODEL_GROUP_COMPOSITOR_SEQUENCE == R->term.mg->compositor);
  assert(XS_MODEL_GROUP_COMPOSITOR_CHOICE == B->term.mg->compositor);
  /* Schema Component Constraint: Particle Derivation OK (Sequence:Choice -- MapAndSum)

     For a sequence group particle to be a valid restriction of a choice group particle all of the
     following must be true: */

  /* @implements(xmlschema-1:rcase-MapAndSum.1)
     test { particle_restriction_mapandsum1.test }
     test { particle_restriction_mapandsum2.test }
     test { particle_restriction_mapandsum3.test }
     test { particle_restriction_mapandsum4.test }
     test { particle_restriction_mapandsum5.test }
     test { particle_restriction_mapandsum6.test }
     test { particle_restriction_mapandsum7.test }
     @end */
  /* 1 There is a complete functional mapping from the particles in the {particles} of R to the
       particles in the {particles} of B such that each particle in the {particles} of R is a
       valid restriction of the particle in the {particles} of B it maps to as defined by Particle
       Valid (Restriction) (3.9.6). */

  CHECK_CALL(xs_compute_particle_mapping(s,R,B,0,0,0,"rcase-MapAndSum.1"))

  /* @implements(xmlschema-1:rcase-MapAndSum.2)
     test { particle_restriction_mapandsum_range1.test }
     test { particle_restriction_mapandsum_range2.test }
     test { particle_restriction_mapandsum_range3.test }
     test { particle_restriction_mapandsum_range4.test }
     test { particle_restriction_mapandsum_range5.test }
     test { particle_restriction_mapandsum_range6.test }
     @end */
  /* 2 The pair consisting of the product of the {min occurs} of R and the length of its
       {particles} and unbounded if {max occurs} is unbounded otherwise the product of the
       {max occurs} of R and the length of its {particles} is a valid restriction of B's occurrence
       range as defined by Occurrence Range OK (3.9.6).
       Note: This clause is in principle more restrictive than absolutely necessary, but in
       practice will cover all the likely cases, and is much easier to specify than the fully
       general version.
       Note: This case allows the "unfolding" of iterated disjunctions into sequences. It may be
       particularly useful when the disjunction is an implicit one arising from the use of
       substitution groups. */
  Rrange2.min_occurs = R->range.min_occurs*list_count(R->term.mg->particles);
  if (0 > R->range.min_occurs)
    Rrange2.max_occurs = -1;
  else
    Rrange2.max_occurs = R->range.max_occurs*list_count(R->term.mg->particles);

  if (!occurrence_range_restriction_ok(Rrange2,B->range)) {
    char *rrange2str = xs_range_str(Rrange2);
    char *brangestr = xs_range_str(B->range);
    char *bstr = xs_particle_str(B);
    error(&s->ei,s->uri,R->defline,"rcase-MapAndSum.2","Total occurrence range %s is not a valid "
          "restriction of the range %s, declared on the corresponding %s of the base type",
          rrange2str,brangestr,bstr);
    free(rrange2str);
    free(brangestr);
    free(bstr);
    return -1;
  }

  return 0;
}

int particle_emptiable(xs_schema *s, xs_particle *p)
{
  /* @implements(xmlschema-1:cos-group-emptiable.1) @end */
  /* @implements(xmlschema-1:cos-group-emptiable.2) @end */
  /* Schema Component Constraint: Particle Emptiable

     [Definition:]  For a particle to be emptiable one of the following must be true:
     1 Its {min occurs} is 0.
     2 Its {term} is a group and the minimum part of the effective total range of that group, as
       defined by Effective Total Range (all and sequence) (3.8.6) (if the group is all or
       sequence) or Effective Total Range (choice) (3.8.6) (if it is choice), is 0.
  */
  return (0 == xs_particle_effective_total_range(p).min_occurs);
}





xs_particle *new_empty_group_particle(xs_particle *orig, list **allocp, list **allocmg)
{
  xs_particle *newp = (xs_particle*)calloc(1,sizeof(xs_particle));
  list_append(allocp,newp);
  assert(XS_PARTICLE_TERM_MODEL_GROUP == orig->term_type);
  newp->range = orig->range;
  newp->term_type = XS_PARTICLE_TERM_MODEL_GROUP;
  newp->term.mg = (xs_model_group*)calloc(1,sizeof(xs_model_group));
  list_append(allocmg,newp->term.mg);
  newp->defline = orig->defline;

  newp->term.mg->compositor = orig->term.mg->compositor;
  newp->term.mg->defline = orig->term.mg->defline;
  return newp;
}

int add_nonpointless_particles(xs_particle *in, xs_particle *parent, list **allocp, list **allocmg)
{
  assert(XS_PARTICLE_TERM_MODEL_GROUP == parent->term_type);

  if (XS_PARTICLE_TERM_MODEL_GROUP != in->term_type) {
    list_append(&parent->term.mg->particles,in);
  }
  else if (XS_MODEL_GROUP_COMPOSITOR_SEQUENCE == in->term.mg->compositor) {
    /* <sequence>
       One of the following must be true:
       2.2.1 {particles} is empty.
       2.2.2 All of the following must be true:
       2.2.2.1 The particle within which this <sequence> appears has {max occurs} and {min occurs}
               of 1.
       2.2.2.2 One of the following must be true:
       2.2.2.2.1 The <sequence>'s {particles} has only one member.
       2.2.2.2.2 The particle within which this <sequence> appears is itself among the {particles}
                 of a <sequence>. */
    if (NULL == in->term.mg->particles) {
      /* pointless; empty sequence - add nothing */
      return 0;
    }
    else if (((1 == in->range.min_occurs) && (1 == in->range.max_occurs)) &&
        ((NULL == in->term.mg->particles->next) ||
         (XS_MODEL_GROUP_COMPOSITOR_SEQUENCE == parent->term.mg->compositor))) {
      /* pointless; just add the child particles directly into the parent sequence's list */
      list *l;
      for (l = in->term.mg->particles; l; l = l->next)
        add_nonpointless_particles((xs_particle*)l->data,parent,allocp,allocmg);
    }
    else {
      /* non-pointless; create a new sequence and add non-pointless particles recursively */
      xs_particle *newp = new_empty_group_particle(in,allocp,allocmg);
      list *l;
      for (l = in->term.mg->particles; l; l = l->next)
        add_nonpointless_particles((xs_particle*)l->data,newp,allocp,allocmg);
      list_append(&parent->term.mg->particles,newp);
    }
  }
  else if (XS_MODEL_GROUP_COMPOSITOR_CHOICE == in->term.mg->compositor) {
    /* <choice>
       One of the following must be true:
       2.2.1 {particles} is empty and the particle within which this <choice> appears has
             {min occurs} of 0.
       2.2.2 All of the following must be true:
       2.2.2.1 The particle within which this <choice> appears has {max occurs} and {min occurs}
               of 1.
       2.2.2.2 One of the following must be true:
       2.2.2.2.1 The <choice>'s {particles} has only one member.
       2.2.2.2.2 The particle within which this <choice> appears is itself among the {particles}
                 of a <choice>. */
    if ((NULL == in->term.mg->particles) && (0 == parent->range.min_occurs)) {
      /* pointless; empty choice - add nothing */
      return 0;
    }
    else if (((1 == in->range.min_occurs) && (1 == in->range.max_occurs)) &&
        (((NULL != in->term.mg->particles) && (NULL == in->term.mg->particles->next)) ||
         (XS_MODEL_GROUP_COMPOSITOR_CHOICE == parent->term.mg->compositor))) {
      /* pointless; just add the child particles directly into the parent choice's list */
      list *l;
      for (l = in->term.mg->particles; l; l = l->next)
        add_nonpointless_particles((xs_particle*)l->data,parent,allocp,allocmg);
    }
    else {
      /* non-pointless; create a new choice and add non-pointless particles recursively */
      xs_particle *newp = new_empty_group_particle(in,allocp,allocmg);
      list *l;
      for (l = in->term.mg->particles; l; l = l->next)
        add_nonpointless_particles((xs_particle*)l->data,newp,allocp,allocmg);
      list_append(&parent->term.mg->particles,newp);
    }
  }
  else {
    /* <all>

       One of the following must be true:
       2.2.1 {particles} is empty.
       2.2.2 {particles} has only one member. */

    if (NULL == in->term.mg->particles) {
      /* pointless; empty all - add nothing */
      return 0;
    }
    else if (((1 == in->range.min_occurs) && (1 == in->range.max_occurs)) &&
             (NULL == in->term.mg->particles->next)) {
      /* pointless; only one member - just add it */
      add_nonpointless_particles((xs_particle*)in->term.mg->particles->data,parent,allocp,allocmg);
    }
    else {
      /* non-pointless; create a new all and add non-pointless particles recursively */
      xs_particle *newp = new_empty_group_particle(in,allocp,allocmg);
      list *l;
      for (l = in->term.mg->particles; l; l = l->next)
        add_nonpointless_particles((xs_particle*)l->data,newp,allocp,allocmg);
      list_append(&parent->term.mg->particles,newp);
    }
  }

  return 0;
}

xs_particle *ignore_pointless(xs_schema *s, xs_particle *p, ignore_pointless_data *ipd)
{
  xs_particle *newp;
  list *l;
  ipd->allocp = NULL;
  ipd->allocmg = NULL;
  if (XS_PARTICLE_TERM_MODEL_GROUP != p->term_type)
    return p;

  newp = new_empty_group_particle(p,&ipd->allocp,&ipd->allocmg);

  for (l = p->term.mg->particles; l; l = l->next)
    add_nonpointless_particles((xs_particle*)l->data,newp,&ipd->allocp,&ipd->allocmg);


  return newp;
}

void ignore_pointless_free(ignore_pointless_data *ipd)
{
  list_free(ipd->allocp,(void*)xs_particle_free);
  list_free(ipd->allocmg,(void*)xs_model_group_free);
}








int xs_check_particle_valid_restriction2(xs_schema *s, xs_particle *R, xs_particle *B)
{
  /* 3.9.6 Schema Component Constraint: Particle Valid (Restriction) */
  /* [Definition:]  For a particle (call it R, for restriction) to be a valid restriction of
     another particle (call it B, for base) one of the following must be true: */
/*   int pointless = 0; */
  xs_particle *p = R;
  char *btypestr = NULL;
  char *rtypestr = NULL;

  if (XS_PARTICLE_TERM_ELEMENT == R->term_type) {
    rtypestr = "element";
  }
  else if (XS_PARTICLE_TERM_WILDCARD == R->term_type) {
    rtypestr = "any";
  }
  else if (XS_PARTICLE_TERM_MODEL_GROUP == R->term_type) {
    if (XS_MODEL_GROUP_COMPOSITOR_ALL == R->term.mg->compositor)
      rtypestr = "all";
    else if (XS_MODEL_GROUP_COMPOSITOR_CHOICE == R->term.mg->compositor)
      rtypestr = "choice";
    else if (XS_MODEL_GROUP_COMPOSITOR_SEQUENCE == R->term.mg->compositor)
      rtypestr = "sequence";
  }

  if (XS_PARTICLE_TERM_ELEMENT == B->term_type) {
    btypestr = "element";
  }
  else if (XS_PARTICLE_TERM_WILDCARD == B->term_type) {
    btypestr = "any";
  }
  else if (XS_PARTICLE_TERM_MODEL_GROUP == B->term_type) {
    if (XS_MODEL_GROUP_COMPOSITOR_ALL == B->term.mg->compositor)
      btypestr = "all";
    else if (XS_MODEL_GROUP_COMPOSITOR_CHOICE == B->term.mg->compositor)
      btypestr = "choice";
    else if (XS_MODEL_GROUP_COMPOSITOR_SEQUENCE == B->term.mg->compositor)
      btypestr = "sequence";
  }

  assert(btypestr);
  assert(rtypestr);
/*   debugl("checking particle restriction: R=%s, B=%s",rtypestr,btypestr); */





  /* @implements(xmlschema-1:cos-particle-restrict.1) @end */
  /* 1 They are the same particle. */
  if (R == B)
    return 0;

  /* @implements(xmlschema-1:cos-particle-restrict.2) @end */
  /* 2 depending on the kind of particle, per the table below, with the qualifications that all of
     the following must be true: */

  /* @implements(xmlschema-1:cos-particle-restrict.2.1) status { unsupported } @end */
  /* 2.1 Any top-level element declaration particle (in R or B) which is the {substitution group
     affiliation} of one or more other element declarations and whose substitution group contains
     at least one element declaration other than itself is treated as if it were a choice group
     whose {min occurs} and {max occurs} are those of the particle, and whose {particles} consists
     of one particle with {min occurs} and {max occurs} of 1 for each of the declarations in its
     substitution group */

  /* @implements(xmlschema-1:cos-particle-restrict.2.2)
     test { particle_pointless_all1.test }
     test { particle_pointless_all2.test }
     test { particle_pointless_all3.test }
     test { particle_pointless_all4.test }
     test { particle_pointless_choice1.test }
     test { particle_pointless_choice2.test }
     test { particle_pointless_choice3.test }
     test { particle_pointless_choice4.test }
     test { particle_pointless_choice5.test }
     test { particle_pointless_choice6.test }
     test { particle_pointless_choice7.test }
     test { particle_pointless_choice8.test }
     test { particle_pointless_sequence1.test }
     test { particle_pointless_sequence2.test }
     test { particle_pointless_sequence3.test }
     test { particle_pointless_sequence4.test }
     test { particle_pointless_sequence5.test }
     test { particle_pointless_sequence6.test }
     test { particle_restriction_ignorepointless1.test }
     test { particle_restriction_ignorepointless2.test }
     @end

     yeah, these get duplicated in the constraint table... the stylesheet doesn't deal with the
     fact that there are multiple lists properly

     @implements(xmlschema-1:cos-particle-restrict.2.2.1) @end
     @implements(xmlschema-1:cos-particle-restrict.2.2.2) @end
     @implements(xmlschema-1:cos-particle-restrict.2.2.2.1) @end
     @implements(xmlschema-1:cos-particle-restrict.2.2.2.2) @end
     @implements(xmlschema-1:cos-particle-restrict.2.2.2.2.1) @end
     @implements(xmlschema-1:cos-particle-restrict.2.2.2.2.2) @end
     */

  /* 2.2 Any pointless occurrences of <sequence>, <choice> or <all> are ignored, where
     pointlessness is understood as follows: */

  if (XS_PARTICLE_TERM_MODEL_GROUP == p->term_type) {
    if (XS_MODEL_GROUP_COMPOSITOR_SEQUENCE == p->term.mg->compositor) {


    }
    else if (XS_MODEL_GROUP_COMPOSITOR_ALL == p->term.mg->compositor) {

    }
    else {
      assert(XS_MODEL_GROUP_COMPOSITOR_CHOICE == p->term.mg->compositor);

    }
  }

  /* Base particle: element */
  if (XS_PARTICLE_TERM_ELEMENT == B->term_type) {

    btypestr = "element";

    if (XS_PARTICLE_TERM_ELEMENT == R->term_type)
      return particle_restriction_ok_elt_elt_nameandtypeok(s,R->term.e,R->range,R->defline,
                                                           B->term.e,B->range,B->defline);

  }
  /* Base particle: any */
  else if (XS_PARTICLE_TERM_WILDCARD == B->term_type) {

    btypestr = "any";

    if (XS_PARTICLE_TERM_ELEMENT == R->term_type)
      return particle_derivation_ok_elt_any_nscompat(s,R,B);
    else if (XS_PARTICLE_TERM_WILDCARD == R->term_type)
      return particle_derivation_ok_any_any_nssubset(s,R,B);
    else if (XS_PARTICLE_TERM_MODEL_GROUP == R->term_type)
      return particle_derivation_ok_all_choice_sequence_any_nsrecursecheckcardinality(s,R,B);

  }
  /* Base particle: all, choice, or sequence */
  else if (XS_PARTICLE_TERM_MODEL_GROUP == B->term_type) {

    if (XS_PARTICLE_TERM_ELEMENT == R->term_type)
      return particle_derivation_ok_all_choice_sequence_recurseasifgroup(s,R,B);

    /* Base particle: all */
    if (XS_MODEL_GROUP_COMPOSITOR_ALL == B->term.mg->compositor) {

      btypestr = "all";

      if ((XS_PARTICLE_TERM_MODEL_GROUP == R->term_type) &&
          (XS_MODEL_GROUP_COMPOSITOR_ALL == R->term.mg->compositor))
        return particle_derivation_ok_all_all_sequence_sequence_recurse(s,R,B);
      else if ((XS_PARTICLE_TERM_MODEL_GROUP == R->term_type) &&
               (XS_MODEL_GROUP_COMPOSITOR_SEQUENCE == R->term.mg->compositor))
        return particle_derivation_ok_sequence_all_recurseunordered(s,R,B);

    }
    /* Base particle: choice */
    else if (XS_MODEL_GROUP_COMPOSITOR_CHOICE == B->term.mg->compositor) {

      btypestr = "choice";

      if ((XS_PARTICLE_TERM_MODEL_GROUP == R->term_type) &&
          (XS_MODEL_GROUP_COMPOSITOR_CHOICE == R->term.mg->compositor))
        return particle_derivation_ok_choice_choice_recurselax(s,R,B);
      else if ((XS_PARTICLE_TERM_MODEL_GROUP == R->term_type) &&
               (XS_MODEL_GROUP_COMPOSITOR_SEQUENCE == R->term.mg->compositor))
        return particle_derivation_ok_sequence_choice_mapandsum(s,R,B);

    }
    /* Base particle: sequence */
    else if (XS_MODEL_GROUP_COMPOSITOR_SEQUENCE == B->term.mg->compositor) {

      btypestr = "sequence";

      if ((XS_PARTICLE_TERM_MODEL_GROUP == R->term_type) &&
          (XS_MODEL_GROUP_COMPOSITOR_SEQUENCE == R->term.mg->compositor))
        return particle_derivation_ok_all_all_sequence_sequence_recurse(s,R,B);

    }
  }

  return error(&s->ei,s->uri,R->defline,"cos-particle-restrict.2",
               "Invalid particle restriction: %s cannot restrict %s",rtypestr,btypestr);
}

int xs_check_particle_valid_restriction(xs_schema *s, xs_particle *R, xs_particle *B)
{
  /* First we try it without removing pointless particles. If and only if this fails, we then
     try again with pointless particles removed. This is necessary because it is possible for
     a derivation to be valid in the first case and not the second, for example if a pointless
     particle in the base type corresponds to the same type of particle in the derived type
     which due to some change (such as the addition of the maxOccurs > 1) is not pointless in
     the derived type.
     If an error occurs in both cases, we return the first error, not the second. */
  if (0 != xs_check_particle_valid_restriction2(s,R,B)) {
    error_info first_error;
    int r;
    ignore_pointless_data ipd1;
    ignore_pointless_data ipd2;
    memset(&first_error,0,sizeof(error_info));
    error_info_copy(&first_error,&s->ei);
    R = ignore_pointless(s,R,&ipd1);
    B = ignore_pointless(s,B,&ipd2);

    r = xs_check_particle_valid_restriction2(s,R,B);

    ignore_pointless_free(&ipd1);
    ignore_pointless_free(&ipd2);

    if (0 != r) {
      error_info_copy(&s->ei,&first_error);
      error_info_free_vals(&first_error);
      return -1;
    }
    error_info_free_vals(&first_error);
    return 0;
  }
  return 0;
}

int xs_check_complex_restriction_rule1(xs_schema *s, xs_type *t)
{
  /* 3.4.6 Schema Component Constraint: Derivation Valid (Restriction, Complex) */

  /* Note: we return 0 here on success, and -1 on error (and set error info) */

  /* @implements(xmlschema-1:derivation-ok-restriction.1)
     test { complextype_cc_restriction_final1.test }
     test { complextype_cc_restriction_final2.test }
     test { complextype_cc_restriction_final3.test }
     test { complextype_cc_restriction_final4.test }
     test { complextype_cc_restriction_final5.test }
     test { complextype_cc_restriction_final6.test }
     @end */
  /* 1 The {base type definition} must be a complex type definition whose {final} does not contain
       restriction. */
  /* Note: the case of the base type being a simple type is already handled by src-ct.1 in
     post_process_type1() */
  if (!t->base->complex || t->base->final_restriction) {
    return error(&s->ei,s->uri,t->def.loc.line,"derivation-ok-restriction.1",
                 "Base type disallows restriction");
  }

  return 0;
}


int xs_check_complex_restriction_rule2(xs_schema *s, xs_type *t)
{
  list *l;

  /* Note: we return 0 here on success, and -1 on error (and set error info) */

  /* 3.4.6 Schema Component Constraint: Derivation Valid (Restriction, Complex) */

  /* @implements(xmlschema-1:derivation-ok-restriction.2) @end */
  /* 2 For each attribute use (call this R) in the {attribute uses}  the appropriate case among the
       following must be true: */
  for (l = t->attribute_uses; l; l = l->next) {
    xs_attribute_use *R = (xs_attribute_use*)l->data;
    xs_attribute_use *B = NULL;
    list *bl;

    for (bl = t->base->attribute_uses; bl && !B; bl = bl->next) {
      xs_attribute_use *au2 = (xs_attribute_use*)bl->data;
      if (nsname_equals(R->attribute->def.ident,au2->attribute->def.ident))
        B = au2;
    }

    /* @implements(xmlschema-1:derivation-ok-restriction.2.1) @end */
    /* 2.1 If there is an attribute use in the {attribute uses} of the {base type definition}
           (call this B) whose {attribute declaration} has the same {name} and {target namespace},
           then all of the following must be true: */
    if (NULL != B) {
      xs_value_constraint Revc;
      xs_value_constraint Bevc;

      /* @implements(xmlschema-1:derivation-ok-restriction.2.1.1)
         test { complextype_cc_restriction_attribute_required1.test }
         test { complextype_cc_restriction_attribute_required2.test }
         test { complextype_cc_restriction_attribute_required3.test }
         test { complextype_cc_restriction_attribute_required4.test }
         @end */
      /* @implements(xmlschema-1:derivation-ok-restriction.2.1.1.1) @end */
      /* @implements(xmlschema-1:derivation-ok-restriction.2.1.1.2) @end */
      /* 2.1.1 one of the following must be true:
         2.1.1.1 B's {required} is false.
         2.1.1.2 R's {required} is true. */
      if (B->required && !R->required)
        return error(&s->ei,s->uri,R->defline,"derivation-ok-restriction.2.1.1","Attribute %#n "
                     "must be marked as required, because it is required by base type %#n",
                     R->attribute->def.ident,t->base->def.ident);

      /* @implements(xmlschema-1:derivation-ok-restriction.2.1.2)
         test { complextype_cc_restriction_attribute_typederiv1.test }
         test { complextype_cc_restriction_attribute_typederiv2.test }
         @end */
      /* 2.1.2 R's {attribute declaration}'s {type definition} must be validly derived from B's
               {type definition} given the empty set as defined in Type Derivation OK (Simple)
               (3.14.6). */
      assert(R->attribute->type);
      assert(B->attribute->type);
      if (0 != xs_check_simple_type_derivation_ok(s,R->attribute->type,B->attribute->type,
                                                  0,0,0,0))
        return error(&s->ei,s->uri,R->defline,"derivation-ok-restriction.2.1.2","Attribute %#n "
                     "must have a type that is a valid derivation of type %#n, used on the "
                     "corresponding attribute in base type %#n",
                     R->attribute->def.ident,B->attribute->type->def.ident,t->base->def.ident);

      /* @implements(xmlschema-1:derivation-ok-restriction.2.1.3)
         test { complextype_cc_restriction_valueconstraint1.test }
         test { complextype_cc_restriction_valueconstraint2.test }
         test { complextype_cc_restriction_valueconstraint3.test }
         test { complextype_cc_restriction_valueconstraint4.test }
         test { complextype_cc_restriction_valueconstraint5.test }
         test { complextype_cc_restriction_valueconstraint6.test }
         test { complextype_cc_restriction_valueconstraint7.test }
         @end */
      /* 2.1.3 [Definition:]  Let the effective value constraint of an attribute use be its {value
               constraint}, if present, otherwise its {attribute declaration}'s {value
               constraint}. */
      if (XS_VALUE_CONSTRAINT_NONE != R->vc.type)
        Revc = R->vc;
      else
        Revc = R->attribute->vc;

      if (XS_VALUE_CONSTRAINT_NONE != B->vc.type)
        Bevc = B->vc;
      else
        Bevc = B->attribute->vc;

      /* @implements(xmlschema-1:derivation-ok-restriction.2.1.3.1) @end */
      /* @implements(xmlschema-1:derivation-ok-restriction.2.1.3.2) @end */
      /* Then one of the following must be true:
         2.1.3.1 B's effective value constraint is absent or default.
         2.1.3.2 R's effective value constraint is fixed with the same string as B's. */
      if ((XS_VALUE_CONSTRAINT_FIXED == Bevc.type) &&
          ((XS_VALUE_CONSTRAINT_FIXED != Revc.type) ||
           strcmp(Revc.value,Bevc.value)))
        return error(&s->ei,s->uri,R->defline,"derivation-ok-restriction.2.1.3","Attribute %#n "
                     "must have a fixed value of \"%s\", because this fixed value is set on the "
                     "corresponding attribute declaration in base type %#n",
                     R->attribute->def.ident,Bevc.value,t->base->def.ident);
    }
#if 0
    /* Wait until we have all the other restriction stuff implemented... this causes problems
       with some of the tests */

    /* @implements(xmlschema-1:derivation-ok-restriction.2.2) status { unsupported } @end */
    /* 2.2 otherwise the {base type definition} must have an {attribute wildcard} and the {target
       namespace} of the R's {attribute declaration} must be valid with respect to that wildcard,
       as defined in Wildcard allows Namespace Name (3.10.4). */
    else {
      if ((NULL == t->base->attribute_wildcard) ||
          !ns_valid_wrt_wildcard(s,R->attribute->def.ident.ns,t->base->attribute_wildcard)) {
        debugl("t->name = %#n\n",t->def.ident);
        debugl("t->base->name = %#n\n",t->base->def.ident);
        debugl("R->attribute->name = %#n\n",R->attribute->def.ident);
        return error(&s->ei,s->uri,R->defline,"structures-3.4.6","Declaration of attribute %#n is "
                     "invalid because base type %#n does not have an attribute wildcard that "
                     "permits attributes from this namespace",
                     R->attribute->def.ident,t->base->def.ident);
      }
    }
#endif
  }

  return 0; /* success */
}

int xs_check_complex_restriction_rule3(xs_schema *s, xs_type *t)
{
  list *l;
  /* 3.4.6 Schema Component Constraint: Derivation Valid (Restriction, Complex) */

  /* Note: we return 0 here on success, and -1 on error (and set error info) */

  /* @implements(xmlschema-1:derivation-ok-restriction.3)
     test { complextype_cc_restriction_attribute_required5.test }
     @end */
  /* 3 For each attribute use in the {attribute uses} of the {base type definition} whose {required}
       is true, there must be an attribute use with an {attribute declaration} with the same {name}
       and {target namespace} as its {attribute declaration} in the {attribute uses} of the complex
       type definition itself whose {required} is true. */
  for (l = t->base->attribute_uses; l; l = l->next) {
    xs_attribute_use *B = (xs_attribute_use*)l->data;
    if (B->required) {
      int found = 0;
      list *l2;

      for (l2 = t->attribute_uses; l2 && !found; l2 = l2->next) {
        xs_attribute_use *R = (xs_attribute_use*)l->data;
        if (nsname_equals(R->attribute->def.ident,B->attribute->def.ident)) {
          found = 1;
          /* Note: we don't need to check R->required here, because this has already been taken
             care of by constraint derivation-ok-restriction.2.1.1 above */
        }
      }

      if (!found)
        return error(&s->ei,s->uri,t->def.loc.line,"derivation-ok-restriction.3","Attribute %#n "
                     "cannot be declared as prohibited, because it is declared as required by base "
                     "type %#n",B->attribute->def.ident,t->base->def.ident);
    }
  }

  return 0; /* success */
}

int xs_check_complex_restriction_rule4(xs_schema *s, xs_type *t)
{
  /* 3.4.6 Schema Component Constraint: Derivation Valid (Restriction, Complex) */

  /* Note: we return 0 here on success, and -1 on error (and set error info) */

  /* @implements(xmlschema-1:derivation-ok-restriction.4) @end */
  /* 4 If there is an {attribute wildcard}, all of the following must be true: */
  if (NULL == t->attribute_wildcard)
    return 0; /* success */

  /* @implements(xmlschema-1:derivation-ok-restriction.4.1) 
     test { complextype_cc_restriction_attribute_wildcard1.test }
     @end */
  /* 4.1 The {base type definition} must also have one. */
  if (NULL == t->base->attribute_wildcard) {
    return error(&s->ei,s->uri,t->local_wildcard->defline,"derivation-ok-restriction.4.1",
                 "Attribute wildcard is only allowed if the base type also has one");
  }

  /* @implements(xmlschema-1:derivation-ok-restriction.4.2)
     test { complextype_cc_restriction_attribute_wildcard2.test }
     @end */
  /* 4.2 The complex type definition's {attribute wildcard}'s {namespace constraint} must be a 
         subset of the {base type definition}'s {attribute wildcard}'s {namespace constraint}, as
         defined by Wildcard Subset (3.10.6). */
  if (!xs_wildcard_constraint_is_subset(s,t->attribute_wildcard,t->base->attribute_wildcard)) {
    return error(&s->ei,s->uri,t->local_wildcard->defline,"derivation-ok-restriction.4.2",
                 "Attribute wildcard must be a valid subset of the base type's");
  }

  /* @implements(xmlschema-1:derivation-ok-restriction.4.3)
     test { complextype_cc_restriction_attribute_wildcard3.test }
     test { complextype_cc_restriction_attribute_wildcard4.test }
     test { complextype_cc_restriction_attribute_wildcard5.test }
     test { complextype_cc_restriction_attribute_wildcard6.test }
     test { complextype_cc_restriction_attribute_wildcard7.test }
     test { complextype_cc_restriction_attribute_wildcard8.test }
     test { complextype_cc_restriction_attribute_wildcard9.test }
     test { complextype_cc_restriction_attribute_wildcard10.test }
     @end */
  /* 4.3 Unless the {base type definition} is the ur-type definition, the complex type definition's
         {attribute wildcard}'s {process contents} must be identical to or stronger than the {base
         type definition}'s {attribute wildcard}'s {process contents}, where strict is stronger than
         lax is stronger than skip. */
  if ((t->base != s->globals->complex_ur_type) &&
      (t->attribute_wildcard->process_contents < t->base->attribute_wildcard->process_contents)) {
    return error(&s->ei,s->uri,t->local_wildcard->defline,"derivation-ok-restriction.4.3",
                 "Attribute wildcard on derived type must be at least as strong as that of the "
                 "base type, in the ordering of \"skip\" (weakest), \"lax\", and \"strict\" "
                 "(strongest)");
  }

  return 0; /* success */
}

int xs_check_complex_restriction_rule5(xs_schema *s, xs_type *t)
{
  /* 3.4.6 Schema Component Constraint: Derivation Valid (Restriction, Complex) */

  /* Note: we return 0 here on success, and -1 on error (and set error info) */

  /* @implements(xmlschema-1:derivation-ok-restriction.5) @end */
  /* 5 One of the following must be true: */

  /* @implements(xmlschema-1:derivation-ok-restriction.5.1) @end */
  /* 5.1 The {base type definition} must be the ur-type definition. */
  if (t->base == s->globals->complex_ur_type) {
    return 0; /* success */
  }

  /* @implements(xmlschema-1:derivation-ok-restriction.5.2) @end */
  /* @implements(xmlschema-1:derivation-ok-restriction.5.2.1) @end */
  /* @implements(xmlschema-1:derivation-ok-restriction.5.2.2) @end */
  /* 5.2 All of the following must be true:
     5.2.1 The {content type} of the complex type definition must be a simple type definition
     5.2.2 One of the following must be true: */
  else if (!t->complex_content) {
    assert(t->simple_content_type);

    /* @implements(xmlschema-1:derivation-ok-restriction.5.2.2.1)
       test { complextype_sc_content9.test }
       test { complextype_sc_content10.test }
       @end */
    /* 5.2.2.1 The {content type} of the {base type definition} must be a simple type definition
               from which the {content type} is validly derived given the empty set as defined in
               Type Derivation OK (Simple) (3.14.6). */
    if (!t->base->complex_content) {
      if (0 == xs_check_simple_type_derivation_ok(s,t->simple_content_type,
                                              t->base->simple_content_type,0,0,0,0)) {
        return 0; /* success */
      }
      else {
        return error(&s->ei,s->uri,t->def.loc.line,"derivation-ok-restriction.5.2.2.1",
                     "Invalid derivation for complex type with simple content; the simple type "
                     "that is declared within %#n is not a valid derivation of the simple type "
                     "declared within %#n",t->def.ident,t->base->def.ident);
      }
    }
    /* @implements(xmlschema-1:derivation-ok-restriction.5.2.2.2) @end */
    /* 5.2.2.2 The {base type definition} must be mixed and have a particle which is emptiable as
               defined in Particle Emptiable (3.9.6). */
    /* This is actually enforced already in compute_content_type() */
    else {
      if (t->base->mixed &&
             ((NULL == t->base->content_type) || particle_emptiable(s,t->base->content_type))) {
        return 0; /* success */
      }
      else {
        return error(&s->ei,s->uri,t->def.loc.line,"derivation-ok-restriction.5.2.2.2","A complex "
                     "type with simple content must either have a content type that is validly "
                     "derived from that of the base, or the base must be mixed and have an "
                     "emptiable particle");
      }
    }
  }

  /* @implements(xmlschema-1:derivation-ok-restriction.5.3) @end */
  /* @implements(xmlschema-1:derivation-ok-restriction.5.3.1) @end */
  /* @implements(xmlschema-1:derivation-ok-restriction.5.3.2)
     test { complextype_cc_restriction_empty1.test }
     test { complextype_cc_restriction_empty2.test }
     test { complextype_cc_restriction_empty3.test }
     test { complextype_cc_restriction_empty4.test }
     test { complextype_cc_restriction_empty5.test }
     test { complextype_cc_restriction_empty6.test }
     test { complextype_cc_restriction_empty7.test }
     test { complextype_cc_restriction_empty8.test }
     test { complextype_cc_restriction_empty9.test }
     test { complextype_cc_restriction_empty10.test }
     test { complextype_cc_restriction_empty11.test }
     @end */
  /* 5.3 All of the following must be true:
     5.3.1 The {content type} of the complex type itself must be empty
     5.3.2 One of the following must be true: */
  else if (NULL == t->content_type) {
    /* @implements(xmlschema-1:derivation-ok-restriction.5.3.2.1) @end */
    /* @implements(xmlschema-1:derivation-ok-restriction.5.3.2.2) @end */
    /* 5.3.2.1 The {content type} of the {base type definition} must also be empty. */
    /* 5.3.2.2 The {content type} of the {base type definition} must be elementOnly or mixed and
               have a particle which is emptiable as defined in Particle Emptiable (3.9.6). */
    if ((NULL != t->base->content_type) &&
        (!t->base->mixed || !particle_emptiable(s,t->base->content_type))) {
      return error(&s->ei,s->uri,t->def.loc.line,"derivation-ok-restriction.5.3.2","A complex type "
                   "can only have empty content if it's base also has empty content, or allows "
                   "mixed content and has an emptiable particle as its content type");
    }
  }

  /* @implements(xmlschema-1:derivation-ok-restriction.5.4) @end */
  /* 5.4 All of the following must be true: */
  else {
    /* @implements(xmlschema-1:derivation-ok-restriction.5.4.1)
       test { complextype_cc_restriction_mixed1.test }
       test { complextype_cc_restriction_mixed2.test }
       test { complextype_cc_restriction_mixed3.test }
       test { complextype_cc_restriction_mixed4.test }
       @end */
    /* @implements(xmlschema-1:derivation-ok-restriction.5.4.1.1) @end */
    /* @implements(xmlschema-1:derivation-ok-restriction.5.4.1.2) @end */
    /* 5.4.1 One of the following must be true:
       5.4.1.1 The {content type} of the complex type definition itself must be element-only
       5.4.1.2 The {content type} of the complex type definition itself and of the {base type
               definition} must be mixed */
    if (t->mixed && !t->base->mixed) {
      return error(&s->ei,s->uri,t->def.loc.line,"derivation-ok-restriction.5.4.1","A complex type "
                   "can only allow mixed content if its base type also allows mixed content");
    }

    /* @implements(xmlschema-1:derivation-ok-restriction.5.4.2) @end */
    /* 5.4.2 The particle of the complex type definition itself must be a valid restriction of the
             particle of the {content type} of the {base type definition} as defined in Particle
             Valid (Restriction) (3.9.6). */
    if (NULL == t->base->content_type) { /* not sure about this - see above */
      return error(&s->ei,s->uri,t->def.loc.line,"structures-3.4.6","Base type does not allow any "
                   "content; extension must be specified if content to be allowed in this type");
    }
    else {
      CHECK_CALL(xs_check_particle_valid_restriction(s,t->content_type,t->base->content_type))
    }

    /* Note: Attempts to derive complex type definitions whose {content type} is element-only by
       restricting a {base type definition} whose {content type} is empty are not ruled out by this
       clause. However if the complex type definition itself has a non-pointless particle it will
       fail to satisfy Particle Valid (Restriction) (3.9.6). On the other hand some type
       definitions with pointless element-only content, for example an empty <sequence>, will
       satisfy Particle Valid (Restriction) (3.9.6) with respect to an empty {base type definition},
       and so be valid restrictions. */

  }

  return 0; /* success */
}

int xs_check_complex_restriction(xs_schema *s, xs_type *t)
{
/*   printf("*** xs_check_complex_restriction %s\n",t->name); */

  /* 3.4.6 Schema Component Constraint: Derivation Valid (Restriction, Complex) */

  CHECK_CALL(xs_check_complex_restriction_rule1(s,t))
  CHECK_CALL(xs_check_complex_restriction_rule2(s,t))
  CHECK_CALL(xs_check_complex_restriction_rule3(s,t))
  CHECK_CALL(xs_check_complex_restriction_rule4(s,t))
  CHECK_CALL(xs_check_complex_restriction_rule5(s,t))

  /* FIXME!!!: make sure callers know this returns 0 on success! it should generally be wrapped
     inside a CHECK_CALL() */
  return 0;
}





int xs_check_complex_extension_rule14(xs_schema *s, xs_type *t)
{
  /* @implements(xmlschema-1:cos-ct-extends.1.4) @end */
  /* 1.4 One of the following must be true: */

  /* @implements(xmlschema-1:cos-ct-extends.1.4.1) @end */
  /* 1.4.1 The {content type} of the {base type definition} and the {content type} of the complex
           type definition itself must be the same simple type definition. */
  if (!t->complex_content) {
    /* This rule is already enforced by compute_content_type() */
    return 0;
  }

  /* @implements(xmlschema-1:cos-ct-extends.1.4.2) @end */
  /* 1.4.2 The {content type} of both the {base type definition} and the complex type definition
           itself must be empty. */
  if ((NULL == t->content_type) && (NULL == t->base->content_type))
    return 0;

  /* @implements(xmlschema-1:cos-ct-extends.1.4.3) @end */
  /* 1.4.3 All of the following must be true: */

  /* @implements(xmlschema-1:cos-ct-extends.1.4.3.1) @end */
  /* 1.4.3.1 The {content type} of the complex type definition itself must specify a particle. */
  /* This is implicitly true, because t->content_type != NULL */

  /* @implements(xmlschema-1:cos-ct-extends.1.4.3.2) @end */
  /* 1.4.3.2 One of the following must be true: */

  /* @implements(xmlschema-1:cos-ct-extends.1.4.3.2.1) @end */
  /* 1.4.3.2.1 The {content type} of the {base type definition} must be empty. */
  if (NULL == t->base->content_type)
    return 0;

  /* @implements(xmlschema-1:cos-ct-extends.1.4.3.2.2) @end */
  /* 1.4.3.2.2 All of the following must be true: */

  /* @implements(xmlschema-1:cos-ct-extends.1.4.3.2.2.1)
     test { complextype_cc_extension_mixed_elementonly.test }
     test { complextype_cc_extension_elementonly_mixed.test }
     @end */
  /* 1.4.3.2.2.1 Both {content type}s must be mixed or both must be element-only. */

  if (t->mixed && !t->base->mixed) {
    return error(&s->ei,s->uri,t->def.loc.line,"cos-ct-extends.1.4.3.2.2.1","Mixed content cannot "
                 "be specified for this type, because both the type itself and the base type are "
                 "non-empty, and the base type specifies element-only content");
  }
  else if (!t->mixed && t->base->mixed) {
    return error(&s->ei,s->uri,t->def.loc.line,"cos-ct-extends.1.4.3.2.2.1","Element-only content "
                 "cannot be specified for this type, because both the type itself and the base "
                 "type are non-empty, and the base type specifies mixed content");
  }

  /* @implements(xmlschema-1:cos-ct-extends.1.4.3.2.2.2) @end */
  /* 1.4.3.2.2.2 The particle of the complex type definition must be a valid extension of the
                 {base type definition}'s particle, as defined in Particle Valid (Extension)
                 (3.9.6). */
  CHECK_CALL(xs_check_particle_valid_extension(s,t->content_type,t->base->content_type))

  return 0;
}


int xs_check_complex_extension(xs_schema *s, xs_type *t)
{
  /* 3.4.6 Schema Component Constraint: Derivation Valid (Extension) */

  /* Schema Component Constraint: Derivation Valid (Extension)
     If the {derivation method} is extension, the appropriate case among the following must be true:
     */

  /* @implements(xmlschema-1:cos-ct-extends.1)
     test { complextype_cc_extension_valid.test }
     @end */
  /*  1 If the {base type definition} is a complex type definition, then all of the following must
       be true: */
  if (t->base->complex) {

    /* @implements(xmlschema-1:cos-ct-extends.1.1)
       test { complextype_cc_extension_deriv_final2.test }
       test { complextype_cc_extension_deriv_final3.test }
       test { complextype_cc_extension_deriv_final4.test }
       test { complextype_cc_extension_deriv_final5.test }
       test { complextype_cc_extension_deriv_final6.test }
       test { complextype_cc_extension_deriv_final.test }
       @end */

    /* 1.1 The {final} of the {base type definition} must not contain extension. */
    if (t->base->final_extension)
      return error(&s->ei,s->uri,t->def.loc.line,"cos-ct-extends.1.1",
                   "Invalid derivation; base type is declared final for extensions");


    /* @implements(xmlschema-1:cos-ct-extends.1.2) @end */
    /* 1.2 Its {attribute uses} must be a subset of the {attribute uses} of the complex type
           definition itself, that is, for every attribute use in the {attribute uses} of the {base
           type definition}, there must be an attribute use in the {attribute uses} of the complex
           type definition itself whose {attribute declaration} has the same {name}, {target
           namespace} and {type definition} as its attribute declaration. */

    /* This is implicitly satisfied by the way we compute {attribute uses} */

    /* @implements(xmlschema-1:cos-ct-extends.1.3) @end */
    /* 1.3 If it has an {attribute wildcard}, the complex type definition must also have one, and
           the base type definition's {attribute wildcard}'s {namespace constraint} must be a subset
           of the complex type definition's {attribute wildcard}'s {namespace constraint}, as
           defined by Wildcard Subset (3.10.6). */
    /* This is implicitly true due to the way we compute the attribute wildcard */

    CHECK_CALL(xs_check_complex_extension_rule14(s,t))

  /* 1.5 It must in principle be possible to derive the complex type definition in two steps, the
         first an extension and the second a restriction (possibly vacuous), from that type
         definition among its ancestors whose {base type definition} is the ur-type definition.
     Note: This requirement ensures that nothing removed by a restriction is subsequently added
           back by an extension. It is trivial to check if the extension in question is the only
           extension in its derivation, or if there are no restrictions bar the first from the
         ur-type definition.

     Constructing the intermediate type definition to check this constraint is straightforward:
     simply re-order the derivation to put all the extension steps first, then collapse them into
     a single extension. If the resulting definition can be the basis for a valid restriction to
     the desired definition, the constraint is satisfied. */


  }

  /* @implements(xmlschema-1:cos-ct-extends.2) @end */
  /* 2 If the {base type definition} is a simple type definition, then all of the following must be
       true: */
  else {
    /* @implements(xmlschema-1:cos-ct-extends.2.1) @end */
    /* 2.1 The {content type} must be the same simple type definition. */
    /* Implicitly satisfied by the way we compute simple_content_type for this case in
       compute_content_type() */

    /* @implements(xmlschema-1:cos-ct-extends.2.2)
       test { complextype_sc_deriv_final2.test }
       test { complextype_sc_deriv_final3.test }
       test { complextype_sc_deriv_final4.test }
       test { complextype_sc_deriv_final5.test }
       test { complextype_sc_deriv_final6.test }
       test { complextype_sc_deriv_final.test }
       @end */
    /* 2.2 The {final} of the {base type definition} must not contain extension. */
    if (t->base->final_extension)
      return error(&s->ei,s->uri,t->def.loc.line,"cos-ct-extends.2.2",
                   "Invalid derivation; base type is declared final for extensions");
  }

  return 0;
}

int xs_check_complex_type_derivation_ok(xs_schema *s, xs_type *D, xs_type *B,
                                        int final_extension, int final_restriction)
{
  /* 3.4.6 Schema Component Constraint: Type Derivation OK (Complex) */


  /* For a complex type definition (call it D, for derived) to be validly derived from a type
     definition (call this B, for base) given a subset of {extension, restriction} all of the
     following must be true: */

  /* @implements(xmlschema-1:cos-ct-derived-ok.1)
     test { complextype_cc_extension_deriv_ok.test }
     test { complextype_cc_restriction_deriv_ok.test }
     @end */
  /* 1 If B and D are not the same type definition, then the {derivation method} of D must not be
     in the subset. */
  if (B != D) {
    if ((XS_TYPE_DERIVATION_EXTENSION == D->derivation_method) && final_extension)
      return error(&s->ei,s->uri,D->def.loc.line,"cos-ct-derived-ok.1","%#n is not a valid "
                   "extension of %#n; extension is disallowed for the latter",
                   D->def.ident,B->def.ident);

    if ((XS_TYPE_DERIVATION_RESTRICTION == D->derivation_method) && final_restriction)
      return error(&s->ei,s->uri,D->def.loc.line,"cos-ct-derived-ok.1","%#n is not a valid "
                   "restriction of %#n; restriction is disallowed for the latter",
                   D->def.ident,B->def.ident);
  }

  /* @implements(xmlschema-1:cos-ct-derived-ok.2) @end */
  /* 2 One of the following must be true: */

  /* @implements(xmlschema-1:cos-ct-derived-ok.2.1) @end */
  /* 2.1 B and D must be the same type definition. */
  if (B == D)
    return 0;

  /* @implements(xmlschema-1:cos-ct-derived-ok.2.2) @end */
  /* 2.2 B must be D's {base type definition}. */
  if (B == D->base)
    return 0;

  /* @implements(xmlschema-1:cos-ct-derived-ok.2.3)
     test { complextype_deriv_ok1.test }
     test { complextype_deriv_ok2.test }
     test { complextype_deriv_ok3.test }
     test { complextype_deriv_ok4.test }
     test { complextype_deriv_ok5.test }
     test { complextype_deriv_ok6.test }
     test { complextype_deriv_ok7.test }
     @end
     @implements(xmlschema-1:cos-ct-derived-ok.2.3.1) @end
     @implements(xmlschema-1:cos-ct-derived-ok.2.3.2) @end
     @implements(xmlschema-1:cos-ct-derived-ok.2.3.2.1) @end
     @implements(xmlschema-1:cos-ct-derived-ok.2.3.2.2) @end

     2.3 All of the following must be true:
     2.3.1 D's {base type definition} must not be the ur-type definition.
     2.3.2 The appropriate case among the following must be true:
     2.3.2.1 If D's {base type definition} is complex, then it must be validly derived from B given
     the subset as defined by this constraint.
     2.3.2.2 If D's {base type definition} is simple, then it must be validly derived from B given
     the subset as defined in Type Derivation OK (Simple) (3.14.6). */
  if (((D->base == s->globals->complex_ur_type) || (D->base == s->globals->simple_ur_type)) ||
      (D->complex && (0 != xs_check_complex_type_derivation_ok(s,D->base,B,final_extension,
                                                               final_restriction))) ||
      (!D->complex && (0 != xs_check_simple_type_derivation_ok(s,D->base,B,final_extension,
                                                               final_restriction,0,0))))
    return error(&s->ei,s->uri,D->def.loc.line,"cos-ct-derived-ok.2.3",
                 "%#n cannot be derived from %#n",D->def.ident,B->def.ident);

  /* Note: This constraint is used to check that when someone uses a type in a context where
     another type was expected (either via xsi:type or substitution groups), that the type used is
     actually derived from the expected type, and that that derivation does not involve a form of
     derivation which was ruled out by the expected type. */

  return 0;
}
