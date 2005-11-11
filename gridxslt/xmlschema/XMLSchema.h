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

#include "util/XMLUtils.h"
#include "util/List.h"
#include "util/Namespace.h"
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
#define IMPL_INTEGER int
#define IMPL_SHORT short
#define IMPL_BYTE char
#define IMPL_FLOAT float
#define IMPL_DOUBLE double

#define VALUECONSTRAINT_NONE            0
#define VALUECONSTRAINT_DEFAULT         1
#define VALUECONSTRAINT_FIXED           2

#define TYPE_DERIVATION_EXTENSION        0
#define TYPE_DERIVATION_RESTRICTION      1

#define TYPE_VARIETY_INVALID             0
#define TYPE_VARIETY_ATOMIC              1
#define TYPE_VARIETY_LIST                2
#define TYPE_VARIETY_UNION               3

#define TYPE_SIMPLE_BUILTIN              0
#define TYPE_SIMPLE_RESTRICTION          1
#define TYPE_SIMPLE_LIST                 2
#define TYPE_SIMPLE_UNION                3

#define ATTRIBUTEUSE_OPTIONAL           0
#define ATTRIBUTEUSE_PROHIBITED         1
#define ATTRIBUTEUSE_REQUIRED           2

#define MODELGROUP_COMPOSITOR_ALL       0
#define MODELGROUP_COMPOSITOR_CHOICE    1
#define MODELGROUP_COMPOSITOR_SEQUENCE  2

#define PARTICLE_TERM_INVALID            0
#define PARTICLE_TERM_ELEMENT            1
#define PARTICLE_TERM_MODEL_GROUP        2
#define PARTICLE_TERM_WILDCARD           3

#define WILDCARD_TYPE_ANY                0
#define WILDCARD_TYPE_NOT                1
#define WILDCARD_TYPE_SET                2

#define WILDCARD_PROCESS_CONTENTS_SKIP   0
#define WILDCARD_PROCESS_CONTENTS_LAX    1
#define WILDCARD_PROCESS_CONTENTS_STRICT 2

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

#define CSTRUCT_INVALID                  0
#define CSTRUCT_TYPE                     1
#define CSTRUCT_MODEL_GROUP              2
#define CSTRUCT_ATTRIBUTE_GROUP          3

namespace GridXSLT {

class Type;
class SchemaAttribute;
class SchemaElement;
class AttributeGroupRef;
class AttributeGroup;
class ModelGroupDef;
class ModelGroup;
class Particle;
class Wildcard;
class AttributeUse;
class BuiltinTypes;
class Schema;

typedef struct Range Range;
typedef struct ValueConstraint ValueConstraint;
typedef struct Reference Reference;
typedef struct MemberType MemberType;
typedef struct xs_facetdata xs_facetdata;
typedef struct IdentityConstraint IdentityConstraint;
typedef struct Notation Notation;


typedef struct CStruct CStruct;
typedef struct xs_allocset xs_allocset;
typedef struct xs_symbol_table xs_symbol_table;

class SchemaVisitor {
  DISABLE_COPY(SchemaVisitor)
public:
  SchemaVisitor() : once_each(0) { }
  virtual ~SchemaVisitor() { }

  virtual int type(Schema *s, xmlDocPtr doc, int post, Type *t) { return 0; }
  virtual int attribute(Schema *s, xmlDocPtr doc, int post, SchemaAttribute *a) { return 0; }
  virtual int element(Schema *s, xmlDocPtr doc, int post, SchemaElement *e) { return 0; }
  virtual int attributeGroup(Schema *s, xmlDocPtr doc, int post, AttributeGroup *ag) { return 0; }
  virtual int attributeGroupRef(Schema *s, xmlDocPtr doc, int post,
                                AttributeGroupRef *agr) { return 0; }
  virtual int identityConstraint(Schema *s, xmlDocPtr doc, int post,
                         IdentityConstraint *ic) { return 0; }
  virtual int modelGroupDef(Schema *s, xmlDocPtr doc, int post, ModelGroupDef *mgd) { return 0; }
  virtual int modelGroup(Schema *s, xmlDocPtr doc, int post, ModelGroup *mg) { return 0; }
  virtual int notation(Schema *s, xmlDocPtr doc, int post, Notation *n) { return 0; }
  virtual int particle(Schema *s, xmlDocPtr doc, int post, Particle *p) { return 0; }
  virtual int wildcard(Schema *s, xmlDocPtr doc, int post, Wildcard *w) { return 0; }
  virtual int attributeUse(Schema *s, xmlDocPtr doc, int post, AttributeUse *au) { return 0; }
  virtual int schema(Schema *s, xmlDocPtr doc, int post, Schema *s2) { return 0; }

  int once_each;
};











class Range {
public:
  Range() : min_occurs(0), max_occurs(0) {}
  GridXSLT::String toString();
  int min_occurs;
  int max_occurs;
};

class ValueConstraint {
  DISABLE_COPY(ValueConstraint)
public:
  ValueConstraint();
  ~ValueConstraint();

  char *value;
  int type;
};

class Reference {
  DISABLE_COPY(Reference)
public:
  Reference(xs_allocset *as);
  ~Reference();

  definition def;
  Schema *s;
  int type;
  void **obj;
  int resolved;
  int builtin;
  int (*check)(Schema *s, Reference *r);
  void *source;
  void *target;
};

class MemberType {
  DISABLE_COPY(MemberType)
public:
  MemberType(xs_allocset *as);
  ~MemberType();

  Type *type;
  Reference *ref;
};

struct xs_facetdata {
  char *strval[XS_FACET_NUMFACETS];
  int intval[XS_FACET_NUMFACETS];
  int defline[XS_FACET_NUMFACETS];
  list *patterns;
  list *enumerations;
};

class Type {
  DISABLE_COPY(Type)
public:
  Type(xs_allocset *as);
  ~Type();

  int isDerived(Type *from);
  int facetAllowed(int facet);

  definition def;                  /* {name} and {target namespace} */

  int complex;

  Type *base;                   /* {base type definition} */
  Reference *baseref;

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
  Wildcard *local_wildcard;     /* {attribute wildcard} */
  Wildcard *attribute_wildcard; /* {attribute wildcard} */
  Particle *content_type;       /* {content type} - content model */
  Type *simple_content_type;    /* {content type} - simple type definition */
  int mixed;                       /* {content type} - mixed or element-only */
  int prohibited_extension;        /* {prohibited substitutions} */
  int prohibited_restriction;      /* {prohibited substitutions} */

  list *attribute_group_refs;      /* list of Reference */

  /* properties specific to simple types */
  xs_facetdata facets;             /* {facets} */
  void *fundamental_facets;        /* {fundamental facets} */
  int variety;                     /* {variety} */
  Type *primitive_type;         /* {primitive type definition} - atomic types only */
  Type *item_type;              /* {item type definition} - list types only */
  Reference *item_typeref;      /* {item type definition} - list types only */
  list *members;                   /* {member type definitions} - union types only */
  int stype;                       /* restriction, list or union */
  int *allowed_facets; /* primitive types only */
  xs_facetdata child_facets;
  Type *child_type;

  int complex_content;

  Particle *effective_content;
  int effective_mixed;

  int primitive;
  int builtin;
  int restriction;
  int computed_wildcard;
  int computed_attribute_uses;
  int computed_content_type;

  int typeinfo_known;
  int size;
  String ctype;
  int custom_ctype;
  String parent_name;
  String parent_ns;
  List<String> attribute_cvars;
};

class SchemaAttribute {
  DISABLE_COPY(SchemaAttribute)
public:
  SchemaAttribute(xs_allocset *as);
  ~SchemaAttribute();

  definition def;                        /* {name} and {target namespace} */

  Type *type;                         /* {type definition} */
  Reference *typeref;
                                         /* {scope} (nothing corresponds to this) */
  ValueConstraint vc;                /* {value constraint} */
  void *annotation;                      /* {annotation} */
  int toplevel;
};

struct SchemaElement {
  DISABLE_COPY(SchemaElement)
public:
  SchemaElement(xs_allocset *as);
  ~SchemaElement();

  definition def;                        /* {name} and {target namespace} */

  Type *type;                         /* {type definition} */
  Reference *typeref;
                                         /* {scope} (nothing corresponds to this) */
  ValueConstraint vc;                /* {value constraint} */
  int nillable;                          /* {nillable} */
  void *identity_constraint_definitions; /* {identity-constraint definitions} */

  SchemaElement *sghead;                    /* {substitution group affiliation} */
  Reference *sgheadref;
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

class AttributeGroupRef {
  DISABLE_COPY(AttributeGroupRef)
public:
  AttributeGroupRef(xs_allocset *as);
  ~AttributeGroupRef();

  AttributeGroup *ag;
  Reference *ref;

  int typeinfo_known;
  int size;
  int pos;
  String cvar;
};

class AttributeGroup {
  DISABLE_COPY(AttributeGroup)
public:
  AttributeGroup(xs_allocset *as);
  ~AttributeGroup();

  definition def;                        /* {name} and {target namespace} */

  list *attribute_uses;                  /* {attribute uses} */
  list *local_attribute_uses;
  Wildcard *attribute_wildcard;       /* {attribute wildcard} */
  Wildcard *local_wildcard;
  void *annotation;                      /* {annotation} */
  list *attribute_group_refs;            /* list of Reference */

  int computed_wildcard;
  int computed_attribute_uses;
  int post_processed;

  int typeinfo_known;
  int size;
  String ctype;
  List<String> cvars;
};

class IdentityConstraint {
  DISABLE_COPY(IdentityConstraint)
public:
  IdentityConstraint(xs_allocset *as);
  ~IdentityConstraint();

};

class ModelGroupDef {
  DISABLE_COPY(ModelGroupDef)
public:
  ModelGroupDef(xs_allocset *as);
  ~ModelGroupDef();

  definition def;
  ModelGroup *model_group;
  void *annotation;
};

class ModelGroup {
  DISABLE_COPY(ModelGroup)
public:
  ModelGroup(xs_allocset *as);
  ~ModelGroup();

  int compositor;
  list *particles;
  void *annotation;
  ModelGroupDef *mgd;
  void *vdata;
  int defline;

  Type *content_model_of;

  int typeinfo_known;
  int size;
  String ctype;
  SchemaElement *defe;
  String parent_name;
  String parent_ns;
  List<String> cvars;
};

class Notation {
  DISABLE_COPY(Notation)
public:
  Notation(xs_allocset *as);
  ~Notation();

};

class Particle {
  DISABLE_COPY(Particle)
public:
  Particle(xs_allocset *as);
  ~Particle();

  GridXSLT::String toString();

  Range range;

  union {
    SchemaElement *e;
    ModelGroup *mg;
    Wildcard *w;
  } term;
  int term_type;

  Reference *ref;
  int defline;
  void *vdata;
  void *seqdata;

  int typeinfo_known;
  String cvar;
  int pos;
  int base_size;
  int size;
  int enctype;

  /* FIXME: temp */
  Type *effective_content_of;
  Type *empty_content_of;
  Type *extension_for;
  int in_anyType;
  int from_mgd;
};


class Wildcard {
  DISABLE_COPY(Wildcard)
public:
  Wildcard(xs_allocset *as);
  ~Wildcard();

  int type;                                /* {namespace constraint} */
  String not_ns;                            /* {namespace constraint} */
  List<String> nslist;                            /* {namespace constraint} */
  int process_contents;                    /* {process contents} */
  void *annotation;                        /* {annotation} */
  int defline;
};

class AttributeUse {
  DISABLE_COPY(AttributeUse)
public:
  AttributeUse(xs_allocset *as);
  ~AttributeUse();

  int required;                            /* {required} */
  SchemaAttribute *attribute;                 /* {attribute declaration} */
  ValueConstraint vc;                  /* {value constraint} */
  Reference *ref;
  int defline;
  int prohibited;

  int typeinfo_known;
  int pos;
  int size;
  String cvar;
};

class CStruct {
  DISABLE_COPY(CStruct)
public:
  CStruct(xs_allocset *as);
  ~CStruct();

  int type;
  union {
    Type *t;
    ModelGroup *mg;
    AttributeGroup *ag;
  } object;
};

class xs_allocset {
  DISABLE_COPY(xs_allocset)
public:
  xs_allocset();
  ~xs_allocset();

  GridXSLT::ManagedPtrList<CStruct*> alloc_cstruct;
  GridXSLT::ManagedPtrList<Reference*> alloc_reference;
  GridXSLT::ManagedPtrList<MemberType*> alloc_member_type;
  GridXSLT::ManagedPtrList<Type*> alloc_type;
  GridXSLT::ManagedPtrList<SchemaAttribute*> alloc_attribute;
  GridXSLT::ManagedPtrList<SchemaElement*> alloc_element;

  GridXSLT::ManagedPtrList<AttributeGroupRef*> alloc_attribute_group_ref;
  GridXSLT::ManagedPtrList<AttributeGroup*> alloc_attribute_group;
  GridXSLT::ManagedPtrList<IdentityConstraint*> alloc_identity_constraint;
  GridXSLT::ManagedPtrList<ModelGroupDef*> alloc_model_group_def;
  GridXSLT::ManagedPtrList<ModelGroup*> alloc_model_group;
  GridXSLT::ManagedPtrList<Notation*> alloc_notation;

  GridXSLT::ManagedPtrList<Particle*> alloc_particle;
  GridXSLT::ManagedPtrList<Wildcard*> alloc_wildcard;
  GridXSLT::ManagedPtrList<AttributeUse*> alloc_attribute_use;
};

class xs_symbol_table {
  DISABLE_COPY(xs_symbol_table)
public:
  xs_symbol_table();
  ~xs_symbol_table();

  void *lookup(int type, const NSName &ident);

  symbol_space *ss_types;                  /* {type definitions} */
  symbol_space *ss_attributes;             /* {attribute declarations} */
  symbol_space *ss_elements;               /* {element declarations} */
  symbol_space *ss_attribute_groups;       /* {attribute group definitions} */
  symbol_space *ss_identity_constraints;
  symbol_space *ss_model_group_defs;       /* {model group declarations} */
  symbol_space *ss_notations;              /* {notation declarations} */
};

class BuiltinTypes {
  DISABLE_COPY(BuiltinTypes)
public:
  BuiltinTypes();
  ~BuiltinTypes();

  Type *complex_ur_type;
  Wildcard *complex_ur_type_element_wildcard;
  Type *simple_ur_type;

  Type *untyped;
  Type *untyped_atomic;
  Type *any_atomic_type;

  Type *id_type;
  Type *boolean_type;
  Type *string_type;
  Type *long_type;
  Type *integer_type;
  Type *short_type;
  Type *byte_type;
  Type *float_type;
  Type *double_type;
  Type *decimal_type;

  Type *base64_binary_type;
  Type *hex_binary_type;
  Type *any_uri_type;
  Type *qname_type;

  Type *context_type;

  xs_symbol_table *symt;
  xs_allocset *as;

  List<String> ctypes;
  list *cstructs;
  NamespaceMap *namespaces;
};

class Schema {
  DISABLE_COPY(Schema)
public:
  Schema(BuiltinTypes *g);
  ~Schema();

  Type *getType(const NSName &ident);
  SchemaAttribute *getAttribute(const NSName &ident);
  SchemaElement *getElement(const NSName &ident);
  AttributeGroup *getAttributeGroup(const NSName &ident);
  IdentityConstraint *getIdentityConstraint(const NSName &ident);
  ModelGroupDef *getModelGroupDef(const NSName &ident);
  Notation *getNotation(const NSName &ident);
  void *getObject(int type, const NSName &ident);

  int visit(xmlDocPtr doc, SchemaVisitor *v);

  Type *type_definitions;
  void *attribute_declarations;
  void *attribute_grop_definitions;
  void *notation_declarations;

  char *uri;
  char *ns;
  GridXSLT::Error ei;

  BuiltinTypes *globals;
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

void xs_get_first_elements_and_wildcards(Particle *p, list **first_ew);

int Wildcard_constraint_is_subset(Schema *s, Wildcard *sub, Wildcard *super);
Wildcard *Wildcard_constraint_union(Schema *s, Wildcard *O1, Wildcard *O2,
                                          int process_contents);
Wildcard *Wildcard_constraint_intersection(Schema *s, Wildcard *a, Wildcard *b,
                                                 int process_contents);

int parse_xmlschema_element(xmlNodePtr n, xmlDocPtr doc, const String &uri, const char *sourceloc,
                            Schema **sout, GridXSLT::Error *ei, BuiltinTypes *g);

Schema *parse_xmlschema_file(char *filename, BuiltinTypes *g);

};

#ifndef _XMLSCHEMA_XMLSCHEMA_C
extern const char *xs_facet_names[XS_FACET_NUMFACETS];
extern const char *xs_object_types[7];
extern GridXSLT::BuiltinTypes *xs_g;
#endif

#endif /* _XMLSCHEMA_XMLSCHEMA_H */
