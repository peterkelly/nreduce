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

#ifndef _XMLSCHEMA_XMLSCHEMA_H
#define _XMLSCHEMA_XMLSCHEMA_H

#include "util/xmlutils.h"
#include "util/list.h"
#include "util/namespace.h"
#include <libxml/tree.h>
#include <stdio.h>

#define XS_SPEC_S "XML Schema Part 1: Structures Second Edition"
#define XS_SPEC_D "XML Schema Part 2: Datatypes Second Edition"

/*

Types of schema components


Primary components

    * Simple type definitions           - name optional
    * Complex type definitions          - name optional
    * Attribute declarations            - name required
    * Element declarations              - name required

Seconary components

    * Attribute group definitions       - name required
    * Identity-constraint definitions   - name required
    * Model group definitions           - name required
    * Notation declarations             - name required

Helper components

    * Annotations
    * Model groups
    * Particles
    * Wildcards
    * Attribute Uses

*/

#define IMPL_POINTER int
#define IMPL_ARRAYSIZE int
#define IMPL_BOOLEAN char
#define IMPL_LONG long
#define IMPL_INT int
#define IMPL_SHORT short
#define IMPL_BYTE char
#define IMPL_FLOAT float
#define IMPL_DOUBLE double

#define XS_VALUE_CONSTRAINT_NONE            0
#define XS_VALUE_CONSTRAINT_DEFAULT         1
#define XS_VALUE_CONSTRAINT_FIXED           2

#define XS_TYPE_DERIVATION_EXTENSION        0
#define XS_TYPE_DERIVATION_RESTRICTION      1

#define XS_TYPE_VARIETY_INVALID             0
#define XS_TYPE_VARIETY_ATOMIC              1
#define XS_TYPE_VARIETY_LIST                2
#define XS_TYPE_VARIETY_UNION               3

#define XS_TYPE_SIMPLE_BUILTIN              0
#define XS_TYPE_SIMPLE_RESTRICTION          1
#define XS_TYPE_SIMPLE_LIST                 2
#define XS_TYPE_SIMPLE_UNION                3

#define XS_ATTRIBUTE_USE_OPTIONAL           0
#define XS_ATTRIBUTE_USE_PROHIBITED         1
#define XS_ATTRIBUTE_USE_REQUIRED           2

#define XS_MODEL_GROUP_COMPOSITOR_ALL       0
#define XS_MODEL_GROUP_COMPOSITOR_CHOICE    1
#define XS_MODEL_GROUP_COMPOSITOR_SEQUENCE  2

#define XS_PARTICLE_TERM_INVALID            0
#define XS_PARTICLE_TERM_ELEMENT            1
#define XS_PARTICLE_TERM_MODEL_GROUP        2
#define XS_PARTICLE_TERM_WILDCARD           3

#define XS_WILDCARD_TYPE_ANY                0
#define XS_WILDCARD_TYPE_NOT                1
#define XS_WILDCARD_TYPE_SET                2

#define XS_WILDCARD_PROCESS_CONTENTS_SKIP   0
#define XS_WILDCARD_PROCESS_CONTENTS_LAX    1
#define XS_WILDCARD_PROCESS_CONTENTS_STRICT 2

#define XS_FACET_EQUAL                      0
#define XS_FACET_ORDERED                    1
#define XS_FACET_BOUNDED                    2
#define XS_FACET_CARDINALITY                3
#define XS_FACET_NUMERIC                    4

#define XS_FACET_LENGTH                     5
#define XS_FACET_MINLENGTH                  6
#define XS_FACET_MAXLENGTH                  7
#define XS_FACET_PATTERN                    8
#define XS_FACET_ENUMERATION                9
#define XS_FACET_WHITESPACE                 10
#define XS_FACET_MAXINCLUSIVE               11
#define XS_FACET_MAXEXCLUSIVE               12
#define XS_FACET_MINEXCLUSIVE               13
#define XS_FACET_MININCLUSIVE               14
#define XS_FACET_TOTALDIGITS                15
#define XS_FACET_FRACTIONDIGITS             16

#define XS_FACET_NUMFACETS                  17

#define XS_OBJECT_TYPE                      0
#define XS_OBJECT_ATTRIBUTE                 1
#define XS_OBJECT_ELEMENT                   2
#define XS_OBJECT_ATTRIBUTE_GROUP           3
#define XS_OBJECT_IDENTITY_CONSTRAINT       4
#define XS_OBJECT_MODEL_GROUP_DEF           5
#define XS_OBJECT_NOTATION                  6

#define XS_ENCODING_INVALID                 0
#define XS_ENCODING_SINGLE                  1
#define XS_ENCODING_ARRAY_PTR               2
#define XS_ENCODING_FIXED_ARRAY             3
#define XS_ENCODING_MAX_ARRAY               4

#define XS_CSTRUCT_INVALID                  0
#define XS_CSTRUCT_TYPE                     1
#define XS_CSTRUCT_MODEL_GROUP              2
#define XS_CSTRUCT_ATTRIBUTE_GROUP          3

typedef struct xs_range xs_range;
typedef struct xs_value_constraint xs_value_constraint;
typedef struct xs_reference xs_reference;
typedef struct xs_member_type xs_member_type;
typedef struct xs_facetdata xs_facetdata;
typedef struct xs_type xs_type;
typedef struct xs_attribute xs_attribute;
typedef struct xs_element xs_element;

typedef struct xs_attribute_group_ref xs_attribute_group_ref;
typedef struct xs_attribute_group xs_attribute_group;
typedef struct xs_identity_constraint xs_identity_constraint;
typedef struct xs_model_group_def xs_model_group_def;
typedef struct xs_model_group xs_model_group;
typedef struct xs_notation xs_notation;

typedef struct xs_particle xs_particle;
typedef struct xs_wildcard xs_wildcard;
typedef struct xs_attribute_use xs_attribute_use;

typedef struct xs_cstruct xs_cstruct;
typedef struct xs_allocset xs_allocset;
typedef struct xs_symbol_table xs_symbol_table;
typedef struct xs_globals xs_globals;
typedef struct xs_schema xs_schema;

typedef struct xs_visitor xs_visitor;


typedef int (*type_visitor)                (xs_schema *s, xmlDocPtr doc, void *data, int post,
                                            xs_type *t);
typedef int (*attribute_visitor)           (xs_schema *s, xmlDocPtr doc, void *data, int post,
                                            xs_attribute *a);
typedef int (*element_visitor)             (xs_schema *s, xmlDocPtr doc, void *data, int post,
                                            xs_element *e);

typedef int (*attribute_group_visitor)     (xs_schema *s, xmlDocPtr doc, void *data, int post,
                                            xs_attribute_group *ag);
typedef int (*attribute_group_ref_visitor) (xs_schema *s, xmlDocPtr doc, void *data, int post,
                                            xs_attribute_group_ref *agr);
typedef int (*identity_constraint_visitor) (xs_schema *s, xmlDocPtr doc, void *data, int post,
                                            xs_identity_constraint *ic);
typedef int (*model_group_def_visitor)     (xs_schema *s, xmlDocPtr doc, void *data, int post,
                                            xs_model_group_def *mgd);
typedef int (*model_group_visitor)         (xs_schema *s, xmlDocPtr doc, void *data, int post,
                                            xs_model_group *mg);
typedef int (*notation_visitor)            (xs_schema *s, xmlDocPtr doc, void *data, int post,
                                            xs_notation *n);

typedef int (*particle_visitor)            (xs_schema *s, xmlDocPtr doc, void *data, int post,
                                            xs_particle *p);
typedef int (*wildcard_visitor)            (xs_schema *s, xmlDocPtr doc, void *data, int post,
                                            xs_wildcard *w);
typedef int (*attribute_use_visitor)       (xs_schema *s, xmlDocPtr doc, void *data, int post,
                                            xs_attribute_use *au);

typedef int (*schema_visitor)              (xs_schema *s, xmlDocPtr doc, void *data, int post,
                                            xs_schema *s2);

struct xs_visitor {
  type_visitor visit_type;
  attribute_visitor visit_attribute;
  element_visitor visit_element;
  attribute_group_visitor visit_attribute_group;
  attribute_group_ref_visitor visit_attribute_group_ref;
  identity_constraint_visitor visit_identity_constraint;
  model_group_def_visitor visit_model_group_def;
  model_group_visitor visit_model_group;
  notation_visitor visit_notation;
  particle_visitor visit_particle;
  wildcard_visitor visit_wildcard;
  attribute_use_visitor visit_attribute_use;
  schema_visitor visit_schema;
  int once_each;
};

struct xs_range {
  int min_occurs;
  int max_occurs;
};

struct xs_value_constraint {
  char *value;
  int type;
};

struct xs_reference {
  definition def;
  xs_schema *s;
  int type;
  void **obj;
  int resolved;
  int builtin;
  int (*check)(xs_schema *s, xs_reference *r);
  void *source;
  void *target;
};

struct xs_member_type {
  xs_type *type;
  xs_reference *ref;
};

struct xs_facetdata {
  char *strval[XS_FACET_NUMFACETS];
  int intval[XS_FACET_NUMFACETS];
  int defline[XS_FACET_NUMFACETS];
  list *patterns;
  list *enumerations;
};

struct xs_type {
  definition def;                  /* {name} and {target namespace} */

  int complex;

  xs_type *base;                   /* {base type definition} */
  xs_reference *baseref;

  int final_extension;             /* {final} */
  int final_restriction;           /* {final} */
  int final_list;                  /* {final} (simple types only) */
  int final_union;                 /* {final} (simple types only) */

  void *annotations;               /* {annotations} */


  /* properties specific to complex types */
  int derivation_method;           /* {derivation method} */
  int abstract;                    /* {abstract} */
  list *attribute_uses;            /* {attribute uses} */
  list *local_attribute_uses;
  xs_wildcard *local_wildcard;     /* {attribute wildcard} */
  xs_wildcard *attribute_wildcard; /* {attribute wildcard} */
  xs_particle *content_type;       /* {content type} - content model */
  xs_type *simple_content_type;    /* {content type} - simple type definition */
  int mixed;                       /* {content type} - mixed or element-only */
  int prohibited_extension;        /* {prohibited substitutions} */
  int prohibited_restriction;      /* {prohibited substitutions} */

  list *attribute_group_refs;      /* list of xs_reference */

  /* properties specific to simple types */
  xs_facetdata facets;             /* {facets} */
  void *fundamental_facets;        /* {fundamental facets} */
  int variety;                     /* {variety} */
  xs_type *primitive_type;         /* {primitive type definition} - atomic types only */
  xs_type *item_type;              /* {item type definition} - list types only */
  xs_reference *item_typeref;      /* {item type definition} - list types only */
  list *members;                   /* {member type definitions} - union types only */
  int stype;                       /* restriction, list or union */
  int *allowed_facets; /* primitive types only */
  xs_facetdata child_facets;
  xs_type *child_type;

  int complex_content;

  xs_particle *effective_content;
  int effective_mixed;

  int primitive;
  int builtin;
  int restriction;
  int computed_wildcard;
  int computed_attribute_uses;
  int computed_content_type;

  int typeinfo_known;
  int size;
  char *ctype;
  int custom_ctype;
  char *parent_name;
  char *parent_ns;
  list *attribute_cvars;
};

struct xs_attribute {
  definition def;                        /* {name} and {target namespace} */

  xs_type *type;                         /* {type definition} */
  xs_reference *typeref;
                                         /* {scope} (nothing corresponds to this) */
  xs_value_constraint vc;                /* {value constraint} */
  void *annotation;                      /* {annotation} */
  int toplevel;
};

struct xs_element {
  definition def;                        /* {name} and {target namespace} */

  xs_type *type;                         /* {type definition} */
  xs_reference *typeref;
                                         /* {scope} (nothing corresponds to this) */
  xs_value_constraint vc;                /* {value constraint} */
  int nillable;                          /* {nillable} */
  void *identity_constraint_definitions; /* {identity-constraint definitions} */

  xs_element *sghead;                    /* {substitution group affiliation} */
  xs_reference *sgheadref;
  list *sgmembers;

  int exclude_extension;                 /* {substitution group exclusions} */
  int exclude_restriction;               /* {substitution group exclusions} */

  int disallow_substitution;             /* {disallowed substitutions} */
  int disallow_extension;                /* {disallowed substitutions} */
  int disallow_restriction;              /* {disallowed substitutions} */

  int abstract;                          /* {abstract} */
  void *annotation;                      /* {annotation} */
  int toplevel;
  int computed_type;
};

struct xs_attribute_group_ref {
  xs_attribute_group *ag;
  xs_reference *ref;

  int typeinfo_known;
  int size;
  int pos;
  char *cvar;
};

struct xs_attribute_group {
  definition def;                        /* {name} and {target namespace} */

  list *attribute_uses;                  /* {attribute uses} */
  list *local_attribute_uses;
  xs_wildcard *attribute_wildcard;       /* {attribute wildcard} */
  xs_wildcard *local_wildcard;
  void *annotation;                      /* {annotation} */
  list *attribute_group_refs;            /* list of xs_reference */

  int computed_wildcard;
  int computed_attribute_uses;
  int post_processed;

  int typeinfo_known;
  int size;
  char *ctype;
  list *cvars;
};

struct xs_identity_constraint {
};

struct xs_model_group_def {
  definition def;
  xs_model_group *model_group;
  void *annotation;
};

struct xs_model_group {
  int compositor;
  list *particles;
  void *annotation;
  xs_model_group_def *mgd;
  void *vdata;
  int defline;
/*   void *typeinfo; */

  xs_type *content_model_of;

  int typeinfo_known;
  int size;
  char *ctype;
  xs_element *defe;
  char *parent_name;
  char *parent_ns;
  list *cvars;
};

struct xs_notation {
};

struct xs_particle {
  xs_range range;

  union {
    xs_element *e;
    xs_model_group *mg;
    xs_wildcard *w;
  } term;
  int term_type;

  xs_reference *ref;
  int defline;
  void *vdata;
  void *seqdata;

  int typeinfo_known;
  char *cvar;
  int pos;
  int base_size;
  int size;
  int enctype;

  /* FIXME: temp */
  xs_type *effective_content_of;
  xs_type *empty_content_of;
  xs_type *extension_for;
  int in_anyType;
  int from_mgd;


};


struct xs_wildcard {
  int type;                                /* {namespace constraint} */
  char *not_ns;                            /* {namespace constraint} */
  list *nslist;                            /* {namespace constraint} */
  int process_contents;                    /* {process contents} */
  void *annotation;                        /* {annotation} */
  int defline;
};

struct xs_attribute_use {
  int required;                            /* {required} */
  xs_attribute *attribute;                 /* {attribute declaration} */
  xs_value_constraint vc;                  /* {value constraint} */
  xs_reference *ref;
  int defline;
  int prohibited;

  int typeinfo_known;
  int pos;
  int size;
  char *cvar;
};

void *xs_symbol_table_lookup_object(xs_symbol_table *symt, int type, const nsname ident);
void *xs_lookup_object(xs_schema *s, int type, const nsname ident);
xs_type *xs_lookup_type(xs_schema *s, const nsname ident);
xs_attribute *xs_lookup_attribute(xs_schema *s, const nsname ident);
xs_element *xs_lookup_element(xs_schema *s, const nsname ident);
xs_attribute_group *xs_lookup_attribute_group(xs_schema *s, const nsname ident);
xs_identity_constraint *xs_lookup_identity_constraint(xs_schema *s, const nsname ident);
xs_model_group_def *xs_lookup_model_group_def(xs_schema *s, const nsname ident);
xs_notation *xs_lookup_notation(xs_schema *s, const nsname ident);

struct xs_cstruct {
  int type;
  union {
    xs_type *t;
    xs_model_group *mg;
    xs_attribute_group *ag;
  } object;
};

struct xs_allocset {
  list *alloc_cstruct;
  list *alloc_reference;
  list *alloc_member_type;
  list *alloc_type;
  list *alloc_attribute;
  list *alloc_element;

  list *alloc_attribute_group_ref;
  list *alloc_attribute_group;
  list *alloc_identity_constraint;
  list *alloc_model_group_def;
  list *alloc_model_group;
  list *alloc_notation;

  list *alloc_particle;
  list *alloc_wildcard;
  list *alloc_attribute_use;
};

xs_allocset *xs_allocset_new();
void xs_allocset_free(xs_allocset *as);

struct xs_symbol_table {
  symbol_space *ss_types;                  /* {type definitions} */
  symbol_space *ss_attributes;             /* {attribute declarations} */
  symbol_space *ss_elements;               /* {element declarations} */
  symbol_space *ss_attribute_groups;       /* {attribute group definitions} */
  symbol_space *ss_identity_constraints;
  symbol_space *ss_model_group_defs;       /* {model group declarations} */
  symbol_space *ss_notations;              /* {notation declarations} */
};

xs_symbol_table *xs_symbol_table_new();
void xs_symbol_table_free(xs_symbol_table *symt);

struct xs_globals {
  xs_type *complex_ur_type;
  xs_wildcard *complex_ur_type_element_wildcard;
  xs_type *simple_ur_type;

  xs_type *untyped;
  xs_type *untyped_atomic;
  xs_type *any_atomic_type;

  xs_type *id_type;
  xs_type *boolean_type;
  xs_type *string_type;
  xs_type *long_type;
  xs_type *int_type;
  xs_type *short_type;
  xs_type *byte_type;
  xs_type *float_type;
  xs_type *double_type;
  xs_type *decimal_type;

  xs_type *context_type;

  xs_symbol_table *symt;
  xs_allocset *as;

  list *ctypes;
  list *cstructs;
  ns_map *namespaces;
};

xs_globals *xs_globals_new();
void xs_globals_free(xs_globals *g);

struct xs_schema {
  xs_type *type_definitions;
  void *attribute_declarations;
  void *attribute_grop_definitions;
  void *notation_declarations;

  char *uri;
  char *ns;
  error_info ei;

  xs_globals *globals;
  xs_allocset *as;
  xs_symbol_table *symt;


  int attrformq;  /* attributeFormDefault: qualified=1, unqualified=0 */
  int elemformq;  /* elementFormDefault: qualified=1, unqualified=0 */

  char *block_default;
  char *final_default;

  void *annotations;                       /* {annotations} */

  list *imports;
};

void xs_init();
void xs_cleanup();

char *xs_particle_str(xs_particle *p);
char *xs_range_str(xs_range r);

void xs_get_first_elements_and_wildcards(xs_particle *p, list **first_ew);

int xs_type_is_derived(xs_type *t, xs_type *from);
int xs_facet_allowed(xs_type *t, int facet);
int xs_wildcard_constraint_is_subset(xs_schema *s, xs_wildcard *sub, xs_wildcard *super);
xs_wildcard *xs_wildcard_constraint_union(xs_schema *s, xs_wildcard *O1, xs_wildcard *O2,
                                          int process_contents);
xs_wildcard *xs_wildcard_constraint_intersection(xs_schema *s, xs_wildcard *a, xs_wildcard *b,
                                                 int process_contents);

int parse_xmlschema_element(xmlNodePtr n, xmlDocPtr doc, const char *uri, const char *sourceloc,
                            xs_schema **sout, error_info *ei, xs_globals *g);

xs_schema *parse_xmlschema_file(char *filename, xs_globals *g);
int xs_visit_schema(xs_schema *s, xmlDocPtr doc, void *data, xs_visitor *v);
xs_schema *xs_schema_new(xs_globals *g);
void xs_schema_free(xs_schema *s);

xs_cstruct *xs_cstruct_new(xs_allocset *as);
void xs_cstruct_free(xs_cstruct *cs);

xs_reference *xs_reference_new(xs_allocset *as);
void xs_reference_free(xs_reference *r);

xs_member_type *xs_member_type_new(xs_allocset *as);
void xs_member_type_free(xs_member_type *mt);

xs_type *xs_type_new(xs_allocset *as);
void xs_type_free(xs_type *t);

xs_attribute *xs_attribute_new(xs_allocset *as);
void xs_attribute_free(xs_attribute *a);

xs_element *xs_element_new(xs_allocset *as);
void xs_element_free(xs_element *e);

xs_attribute_group_ref *xs_attribute_group_ref_new(xs_allocset *as);
void xs_attribute_group_ref_free(xs_attribute_group_ref *agr);

xs_attribute_group *xs_attribute_group_new(xs_allocset *as);
void xs_attribute_group_free(xs_attribute_group *ag);

xs_identity_constraint *xs_identity_constraint_new(xs_allocset *as);
void xs_identity_constraint_free(xs_identity_constraint *ic);

xs_model_group_def *xs_model_group_def_new(xs_allocset *as);
void xs_model_group_def_free(xs_model_group_def *mgd);

xs_model_group *xs_model_group_new(xs_allocset *as);
void xs_model_group_free(xs_model_group *mg);

xs_notation *xs_notation_new(xs_allocset *as);
void xs_notation_free(xs_notation *n);

xs_particle *xs_particle_new(xs_allocset *as);
void xs_particle_free(xs_particle *p);

xs_wildcard *xs_wildcard_new(xs_allocset *as);
void xs_wildcard_free(xs_wildcard *w);

xs_attribute_use *xs_attribute_use_new(xs_allocset *as);
void xs_attribute_use_free(xs_attribute_use *au);

#ifndef _XMLSCHEMA_XMLSCHEMA_C
extern const char *xs_facet_names[XS_FACET_NUMFACETS];
extern const char *xs_object_types[7];
extern xs_globals *xs_g;
#endif

#endif /* _XMLSCHEMA_XMLSCHEMA_H */
