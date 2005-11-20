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

#include "Parse.h"
#include "XMLSchema.h"
#include "util/Namespace.h"
#include "util/String.h"
#include "util/Debug.h"
#include "util/Macros.h"
#include <libxml/xmlwriter.h>
#include <libxml/xmlIO.h>
#include <libxml/parser.h>
#include <libxml/tree.h>
#include <libxml/uri.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

using namespace GridXSLT;

static NSName ATTR_ABSTRACT                 = NSName("abstract");
static NSName ATTR_ATTRIBUTE_FORM_DEFAULT   = NSName("attributeFormDefault");
static NSName ATTR_BASE                     = NSName("base");
static NSName ATTR_BLOCK                    = NSName("block");
static NSName ATTR_BLOCK_DEFAULT            = NSName("blockDefault");
static NSName ATTR_DEFAULT                  = NSName("default");
static NSName ATTR_ELEMENT_FORM_DEFAULT     = NSName("elementFormDefault");
static NSName ATTR_FINAL                    = NSName("final");
static NSName ATTR_FINAL_DEFAULT            = NSName("finalDefault");
static NSName ATTR_FIXED                    = NSName("fixed");
static NSName ATTR_FORM                     = NSName("form");
static NSName ATTR_ITEM_TYPE                = NSName("itemType");
static NSName ATTR_MAX_OCCURS               = NSName("maxOccurs");
static NSName ATTR_MEMBER_TYPES             = NSName("memberTypes");
static NSName ATTR_MIN_OCCURS               = NSName("minOccurs");
static NSName ATTR_MIXED                    = NSName("mixed");
static NSName ATTR_NAME                     = NSName("name");
static NSName ATTR_NAMESPACE                = NSName("namespace");
static NSName ATTR_NILLABLE                 = NSName("nillable");
static NSName ATTR_PROCESS_CONTENTS         = NSName("processContents");
static NSName ATTR_REF                      = NSName("ref");
static NSName ATTR_SCHEMA_LOCATION          = NSName("schemaLocation");
static NSName ATTR_SUBSTITUTION_GROUP       = NSName("substitutionGroup");
static NSName ATTR_TARGET_NAMESPACE         = NSName("targetNamespace");
static NSName ATTR_TYPE                     = NSName("type");
static NSName ATTR_USE                      = NSName("use");
static NSName ATTR_VALUE                    = NSName("value");

static NSName ELEM_ALL                      = NSName(XS_NAMESPACE,"all");
static NSName ELEM_ANNOTATION               = NSName(XS_NAMESPACE,"annotation");
static NSName ELEM_ANY                      = NSName(XS_NAMESPACE,"any");
static NSName ELEM_ANY_ATTRIBUTE            = NSName(XS_NAMESPACE,"anyAttribute");
static NSName ELEM_ATTRIBUTE                = NSName(XS_NAMESPACE,"attribute");
static NSName ELEM_ATTRIBUTE_GROUP          = NSName(XS_NAMESPACE,"attributeGroup");
static NSName ELEM_CHOICE                   = NSName(XS_NAMESPACE,"choice");
static NSName ELEM_COMPLEX_CONTENT          = NSName(XS_NAMESPACE,"complexContent");
static NSName ELEM_COMPLEX_TYPE             = NSName(XS_NAMESPACE,"complexType");
static NSName ELEM_ELEMENT                  = NSName(XS_NAMESPACE,"element");
static NSName ELEM_ENUMERATION              = NSName(XS_NAMESPACE,"enumeration");
static NSName ELEM_EXTENSION                = NSName(XS_NAMESPACE,"extension");
static NSName ELEM_FRACTION_DIGITS          = NSName(XS_NAMESPACE,"fractionDigits");
static NSName ELEM_GROUP                    = NSName(XS_NAMESPACE,"group");
static NSName ELEM_IMPORT                   = NSName(XS_NAMESPACE,"import");
static NSName ELEM_INCLUDE                  = NSName(XS_NAMESPACE,"include");
static NSName ELEM_KEY                      = NSName(XS_NAMESPACE,"key");
static NSName ELEM_KEYREF                   = NSName(XS_NAMESPACE,"keyref");
static NSName ELEM_LENGTH                   = NSName(XS_NAMESPACE,"length");
static NSName ELEM_LIST                     = NSName(XS_NAMESPACE,"list");
static NSName ELEM_MAX_EXCLUSIVE            = NSName(XS_NAMESPACE,"maxExclusive");
static NSName ELEM_MAX_INCLUSIVE            = NSName(XS_NAMESPACE,"maxInclusive");
static NSName ELEM_MAX_LENGTH               = NSName(XS_NAMESPACE,"maxLength");
static NSName ELEM_MIN_EXCLUSIVE            = NSName(XS_NAMESPACE,"minExclusive");
static NSName ELEM_MIN_INCLUSIVE            = NSName(XS_NAMESPACE,"minInclusive");
static NSName ELEM_MIN_LENGTH               = NSName(XS_NAMESPACE,"minLength");
static NSName ELEM_NOTATION                 = NSName(XS_NAMESPACE,"notation");
static NSName ELEM_PATTERN                  = NSName(XS_NAMESPACE,"pattern");
static NSName ELEM_REDEFINE                 = NSName(XS_NAMESPACE,"redefine");
static NSName ELEM_RESTRICTION              = NSName(XS_NAMESPACE,"restriction");
static NSName ELEM_SCHEMA                   = NSName(XS_NAMESPACE,"schema");
static NSName ELEM_SEQUENCE                 = NSName(XS_NAMESPACE,"sequence");
static NSName ELEM_SIMPLE_CONTENT           = NSName(XS_NAMESPACE,"simpleContent");
static NSName ELEM_SIMPLE_TYPE              = NSName(XS_NAMESPACE,"simpleType");
static NSName ELEM_TOTAL_DIGITS             = NSName(XS_NAMESPACE,"totalDigits");
static NSName ELEM_UNION                    = NSName(XS_NAMESPACE,"union");
static NSName ELEM_UNIQUE                   = NSName(XS_NAMESPACE,"unique");
static NSName ELEM_WHITE_SPACE              = NSName(XS_NAMESPACE,"whiteSpace");

int xs_check_attribute_type(Schema *s, Reference *r)
{
  SchemaAttribute *a = (SchemaAttribute*)r->source;
  ASSERT(a->type);
  if (a->type->complex)
    return error(&s->ei,s->m_uri,r->def.loc.line,String::null(),"Attributes can only use simple types");
  return 0;
}

void xs_skip_others2(Node **c)
{
  while ((*c) && (NODE_ELEMENT != (*c)->m_type))
    (*c) = (*c)->next();
}

void xs_next_element2(Node **c)
{
  *c = (*c)->next();
  xs_skip_others2(c);
}

void xs_skip_annotation2(Node **c)
{
  if (*c && ((*c)->m_ident == ELEM_ANNOTATION))
    xs_next_element2(c);
}

Node *xs_first_non_annotation_child2(Node *n)
{
  Node *c =  n->firstChild();
  xs_skip_others2(&c);
  xs_skip_annotation2(&c);
  return c;
}

int xs_parse_max_occurs(Schema *s, Node *node, int *val)
{
  /* FIXME: what to do if minOccurs > maxOccurs? should we set maxOccurs to the higher value,
     or signal an error? */
  String maxOccurs = node->getAttribute(ATTR_MAX_OCCURS);
  if (maxOccurs == "unbounded") {
    *val = -1;
    return 0;
  }
  return parse_optional_int_attr(&s->ei,s->m_uri,node,ATTR_MAX_OCCURS,val,1);
}

int xs_check_sgheadref(Schema *s, Reference *r)
{
  SchemaElement *source = (SchemaElement*)r->source;
  SchemaElement *head = (SchemaElement*)r->target;
/*   debugl("resolved sghead for %s: %s\n",source->name,head->name); */
/*   debugl("head->exclude_extension = %d\n",head->exclude_extension); */
/*   debugl("head->exclude_restriction = %d\n",head->exclude_restriction); */
  list_push(&head->sgmembers,source);
  return 0;
}

int xs_check_forbidden_attribute(Schema *s, Node *n, const NSName &attrname)
{
  if (n->hasAttribute(attrname))
    return attribute_not_allowed(&s->ei,s->m_uri,n->m_line,attrname.m_name);
  return 0;
}

int xs_init_toplevel_object(Schema *s, Node *node, char *ns,
                            SymbolSpace *ss, void *obj, NSName *ident, char *typestr)
{
  String name = node->getAttribute(ATTR_NAME).collapseWhitespace();
  if (name.isNull())
    return missing_attribute(&s->ei,s->m_uri,node->m_line,String::null(),ATTR_NAME);

  if (NULL != ss->lookup(NSName(ns,name))) {
    error(&s->ei,s->m_uri,node->m_line,String::null(),"%s \"%*\" already declared",typestr,&name);
    return -1;
  }

  *ident = NSName(ns,name);

  ss->add(*ident,obj);

  return 0;
}

int xs_check_conflicting_attributes(Schema *s, Node *n, const NSName &attrname1,
                                    const NSName &attrname2, const char *errname)
{
  if (n->hasAttribute(attrname1) && n->hasAttribute(attrname2))
    return conflicting_attributes(&s->ei,s->m_uri,n->m_line,errname,
                                  attrname1.m_name,attrname2.m_name);
  return 0;
}

int GridXSLT::xs_parse_value_constraint(Schema *s, Node *n, ValueConstraint *vc,
                              const char *errname)
{
  vc->type = VALUECONSTRAINT_NONE;
  CHECK_CALL(xs_check_conflicting_attributes(s,n,ATTR_DEFAULT,ATTR_FIXED,errname))

  String defaul = n->getAttribute(ATTR_DEFAULT);
  String fixed = n->getAttribute(ATTR_FIXED);

  if (!defaul.isNull()) {
    vc->value = defaul;
    vc->type = VALUECONSTRAINT_DEFAULT;
  }
  else if (!fixed.isNull()) {
    vc->value = fixed;
    vc->type = VALUECONSTRAINT_FIXED;
  }

  return 0;
}

int GridXSLT::xs_parse_ref(Schema *s, Node *n, const NSName &attrname, int type,
                 void **obj, Reference **refptr)
{
//   char *name = NULL;
//   char *ns = NULL;
  Reference *r;
  QName qn;

  *refptr = NULL;

  if (!n->hasAttribute(attrname))
    return 0;

  String namestr = n->getAttribute(attrname).collapseWhitespace().cstring();
  qn = QName::parse(namestr);
/*   debugl("line %d: Parsing %s reference %s",n->line,ss->type,namestr); */

  /* @implements(xmlschema-1:src-qname.1) @end
     @implements(xmlschema-1:src-qname.1.1)
     test { qname_interpretation_nons.test } @end
     @implements(xmlschema-1:src-qname.1.2) @end
     @implements(xmlschema-1:src-qname.1.3) @end
     @implements(xmlschema-1:src-qname.2) @end
     @implements(xmlschema-1:src-qname.2.1) @end
     @implements(xmlschema-1:src-qname.2.2) @end
     @implements(xmlschema-1:src-qname.2.2.1) @end
     @implements(xmlschema-1:src-qname.2.2.2) @end */
  NSName resolved = n->resolveQName(qn);
  if (resolved.isNull()) {
    error(&s->ei,s->m_uri,n->m_line,"src-qname.1.1",
          "Could not resolve namespace for prefix \"%*\"",&qn.m_prefix);
    return -1;
  }

  if (!resolved.m_ns.isNull())
    s->globals->namespaces->add_preferred(resolved.m_ns,qn.m_prefix);

  *refptr = r = new Reference(s->as);
  r->s = s;
  r->def.ident = resolved;
  r->def.loc.line = n->m_line;
  r->type = type;
  r->obj = obj;

  return 0;
}

int GridXSLT::xs_parse_form(Schema *s, Node *n, int *qualified)
{
  int invalid = 0;
  String form = n->getAttribute(ATTR_FORM).collapseWhitespace();
  if (form.isNull())
    return 0;

  if ("qualified" == form)
    *qualified = 1;
  else if ("unqualified" == form)
    *qualified = 0;
  else
    invalid = 1;
  if (invalid)
    return invalid_attribute_val(&s->ei,s->m_uri,n,ATTR_FORM);
  else
    return 0;
}

int GridXSLT::xs_parse_block_final(const String &str, int *extension, int *restriction,
                         int *substitution, int *list, int *union1)
{
  if (NULL != extension)
    *extension = 0;
  if (NULL != restriction)
    *restriction = 0;
  if (NULL != substitution)
    *substitution = 0;
  if (NULL != list)
    *list = 0;
  if (NULL != union1)
    *union1 = 0;

  List<String> values = str.parseList();
  for (Iterator<String> it = values; it.haveCurrent(); it++) {
    String cur = *it;

    if (("extension" == cur) && (NULL != extension))
      *extension = 1;
    else if (("restriction" == cur) && (NULL != restriction))
      *restriction = 1;
    else if (("substitution" == cur) && (NULL != substitution))
      *substitution = 1;
    else if (("list" == cur) && (NULL != list))
      *list = 1;
    else if (("union" == cur) && (NULL != union1))
      *union1 = 1;
    else if ("#all" == cur) {
      if (NULL != extension)
        *extension = 1;
      if (NULL != restriction)
        *restriction = 1;
      if (NULL != substitution)
        *substitution = 1;
      if (NULL != list)
        *list = 1;
      if (NULL != union1)
        *union1 = 1;
    }
  }

  return 0;
}

int xs_parse_block_final_attr(Schema *s, Node *node, const NSName &attrname,
                              const String &defaultval, int *extension, int *restriction,
                              int *substitution, int *list, int *union1)
{
  String val = node->getAttribute(attrname);
  if (val.isNull())
    val = defaultval;
  if (val.isNull())
    val = "";

  return xs_parse_block_final(val,extension,restriction,substitution,list,union1);
}

/* possible parents: <schema> <complexType> <group> */

int GridXSLT::xs_parse_element(Node *node, const String &ns1, Schema *s,
                               list **particles_list)
{
  char *ns = ns1.cstring();
  SchemaElement *e;

  /* @implements(xmlschema-1:src-element.4) @end
     @implements(xmlschema-1:e-props-correct.1) @end
     @implements(xmlschema-1:e-props-correct.2) status { unsupported }
     issue { requires support for simple type validation } @end
     @implements(xmlschema-1:e-props-correct.4) status { unsupported }
     issue { requires support for substitution groups } @end


     @implements(xmlschema-1:cos-valid-default.1)
     status { deferred } issue { requires support for simple type validation } @end
     @implements(xmlschema-1:cos-valid-default.2)
     status { deferred } issue { requires support for simple type validation } @end
     @implements(xmlschema-1:cos-valid-default.2.1)
     status { deferred } issue { requires support for simple type validation } @end
     @implements(xmlschema-1:cos-valid-default.2.2)
     status { deferred } issue { requires support for simple type validation } @end
     @implements(xmlschema-1:cos-valid-default.2.2.1)
     status { deferred } issue { requires support for simple type validation } @end
     @implements(xmlschema-1:cos-valid-default.2.2.2)
     status { deferred } issue { requires support for simple type validation } @end

     @implements(xmlschema-1:Element Declaration{name}) @end
     @implements(xmlschema-1:Element Declaration{scope}) @end
     @implements(xmlschema-1:Schema{element declarations}) @end
     @implements(xmlschema-1:Element Declaration{identity-constraint definitions})
       status { unsupported }
       issue { requires support for xpath! }
     @end
     */

  /* case 1: parent is <schema> */
  if (NULL == particles_list) {
    e = new SchemaElement(s->as);
    e->def.loc.line = node->m_line;
    CHECK_CALL(xs_init_toplevel_object(s,node,ns,s->symt->ss_elements,e,&e->def.ident,"element"))
    CHECK_CALL(xs_check_forbidden_attribute(s,node,ATTR_REF))
    CHECK_CALL(xs_check_forbidden_attribute(s,node,ATTR_FORM))
    CHECK_CALL(xs_check_forbidden_attribute(s,node,ATTR_MIN_OCCURS))
    CHECK_CALL(xs_check_forbidden_attribute(s,node,ATTR_MAX_OCCURS))
    /* @implements(xmlschema-1:Element Declaration{substitution group affiliation}) @end */
    CHECK_CALL(xs_parse_ref(s,node,ATTR_SUBSTITUTION_GROUP,XS_OBJECT_ELEMENT,
                            (void**)&e->sghead,&e->sgheadref))
    if (e->sgheadref) {
      e->sgheadref->check = xs_check_sgheadref;
      e->sgheadref->source = e;
    }

    /* @implements(xmlschema-1:Element Declaration{substitution group exclusions}) @end */
    CHECK_CALL(xs_parse_block_final_attr(s,node,ATTR_FINAL,s->final_default,
               &e->exclude_extension,&e->exclude_restriction,NULL,NULL,NULL));

    e->toplevel = 1;
  }
  else {
    int max_occurs_val = 1;
    int min_occurs_val = 1;

    CHECK_CALL(xs_check_forbidden_attribute(s,node,ATTR_SUBSTITUTION_GROUP))
    CHECK_CALL(xs_check_forbidden_attribute(s,node,ATTR_FINAL))
    CHECK_CALL(xs_parse_max_occurs(s,node,&max_occurs_val))
    CHECK_CALL(parse_optional_int_attr(&s->ei,s->m_uri,node,ATTR_MIN_OCCURS,&min_occurs_val,1))

    /* FIXME: if minOccurs=maxOccurs=0 we should not create anything */

    /* case 2: has ancestor of <complexType> or <group> and #ref is absent */
    if (!node->hasAttribute(ATTR_REF)) {
      Particle *p;
      int qualified = s->elemformq;

      /* @implements(xmlschema-1:Element Declaration{target namespace})
         test { element_local_form.test }
         test { element_local_form2.test }
         test { element_local_form3.test }
         test { element_local_form4.test }
         test { element_d_local_form.test }
         test { element_d_local_form2.test }
         test { element_d_local_form3.test }
         test { element_d_local_form4.test }
         @end */
      CHECK_CALL(xs_parse_form(s,node,&qualified))

      /* @implements(xmlschema-1:src-element.2) @end */
      /* @implements(xmlschema-1:src-element.2.1)
         test { element_noname.test }
         @end */
      if (!node->hasAttribute(ATTR_NAME))
        return missing_attribute(&s->ei,s->m_uri,node->m_line,"src-element.2.1",ATTR_NAME);

      e = new SchemaElement(s->as);
      e->def.loc.line = node->m_line;
      p = new Particle(s->as);
      p->range.min_occurs = min_occurs_val;
      p->range.max_occurs = max_occurs_val;
      p->term.e = e;
      p->term_type = PARTICLE_TERM_ELEMENT;
      p->defline = e->def.loc.line;
      list_append(particles_list,p);

      /* FIXME: testcases for form */
      e->def.ident.m_name = node->getAttribute(ATTR_NAME);
      if (ns && qualified)
        e->def.ident.m_ns = ns;
    }

    /* case 3: has ancestor of <complexType> or <group> and #ref is present */
    else {
      Particle *p = new Particle(s->as);
      p->range.min_occurs = min_occurs_val;
      p->range.max_occurs = max_occurs_val;
      p->term.e = NULL; /* will be resolved later */
      p->term_type = PARTICLE_TERM_ELEMENT;
      p->defline = node->m_line;
      list_append(particles_list,p);

      CHECK_CALL(xs_parse_ref(s,node,ATTR_REF,XS_OBJECT_ELEMENT,(void**)&p->term.e,&p->ref))

      /* @implements(xmlschema-1:src-element.2.2)
         test { element_ref_name.test }
         test { element_ref_complextype.test }
         test { element_ref_simpletype.test }
         test { element_ref_key.test }
         test { element_ref_keyref.test }
         test { element_ref_unique.test }
         test { element_ref_nillable.test }
         test { element_ref_default.test }
         test { element_ref_fixed.test }
         test { element_ref_form.test }
         test { element_ref_block.test }
         test { element_ref_type.test }
         @end */
      CHECK_CALL(xs_check_conflicting_attributes(s,node,ATTR_NAME,ATTR_REF,"src-element.2.1"))
      CHECK_CALL(xs_check_conflicting_attributes(s,node,ATTR_NILLABLE,ATTR_REF,"src-element.2.2"))
      CHECK_CALL(xs_check_conflicting_attributes(s,node,ATTR_DEFAULT,ATTR_REF,"src-element.2.2"))
      CHECK_CALL(xs_check_conflicting_attributes(s,node,ATTR_FIXED,ATTR_REF,"src-element.2.2"))
      CHECK_CALL(xs_check_conflicting_attributes(s,node,ATTR_FORM,ATTR_REF,"src-element.2.2"))
      CHECK_CALL(xs_check_conflicting_attributes(s,node,ATTR_BLOCK,ATTR_REF,"src-element.2.2"))
      CHECK_CALL(xs_check_conflicting_attributes(s,node,ATTR_TYPE,ATTR_REF,"src-element.2.2"))

      Node *child;
      if (NULL != (child = xs_first_non_annotation_child2(node)))
        return invalid_element(&s->ei,s->m_uri,child);

      /* Note that we have not created the actual element object yet... this is done later, in
         xs_resolve_ref(), since the element we are referencing may not have been parsed
         yet. */

      /* FIXME: should we check that the node does not contain any children (e.g. complexType)? */

      return 0;
    }
  }

  /* common properties - this part is only done for cases 1 and 2 (i.e. not for refs) */

  /* @implements(xmlschema-1:Element Declaration{disallowed substitutions}) @end */
  CHECK_CALL(xs_parse_block_final_attr(s,node,ATTR_BLOCK,s->block_default,
             &e->disallow_extension,&e->disallow_restriction,&e->disallow_substitution,
             NULL,NULL));

  /* @implements(xmlschema-1:src-element.1)
     description { default and fixed must not both be present }
     test { element_default_fixed.test }
     test { element_local_default_fixed.test }
     @end
     @implements(xmlschema-1:Element Declaration{value constraint}) @end
     */
  CHECK_CALL(xs_parse_value_constraint(s,node,&e->vc,"src-element.1"))

  /* @implements(xmlschema-1:Element Declaration{nillable})
     test { element_local_nillable.test }
     test { element_local_nillable2.test }
     test { element_tl_nillable.test }
     test { element_tl_nillable2.test }
     test { element_tl_nillable3.test }
     test { element_tl_nillable4.test }
     test { element_tl_nillable5.test }
     @end */
  CHECK_CALL(parse_optional_boolean_attr(&s->ei,s->m_uri,node,ATTR_NILLABLE,&e->nillable,0))

  /* @implements(xmlschema-1:Element Declaration{abstract})
     test { element_local_abstract.test }
     test { element_local_abstract2.test }
     test { element_tl_abstract.test }
     test { element_tl_abstract2.test }
     @end */
  CHECK_CALL(parse_optional_boolean_attr(&s->ei,s->m_uri,node,ATTR_ABSTRACT,&e->abstract,0))

  e->type = NULL; /* FIXME */
  e->identity_constraint_definitions = NULL; /* FIXME */

  Node *child = xs_first_non_annotation_child2(node);

  if (child && ((child->m_ident == ELEM_SIMPLE_TYPE) ||
                (child->m_ident == ELEM_COMPLEX_TYPE))) {

    /* @implements(xmlschema-1:src-element.3)
       test { element_type_complextype.test }
       test { element_type_simpletype.test }
       @end */
    if (node->hasAttribute(ATTR_TYPE))
      return error(&s->ei,s->m_uri,node->m_line,"src-element.3",
                   "\"type\" attribute must not be specified "
                   "if a <simpleType> or <complexType> child is present");

    if (child->m_ident == ELEM_SIMPLE_TYPE) {
      CHECK_CALL(xs_parse_simple_type(s,child,ns,0,&e->type))
      xs_next_element2(&child);
    }
    else { /* complexType */
      CHECK_CALL(xs_parse_complex_type(s,child,ns,0,&e->type))
      ASSERT(e->type);
      xs_next_element2(&child);
    }
  }
  else if (node->hasAttribute(ATTR_TYPE)) {
    CHECK_CALL(xs_parse_ref(s,node,ATTR_TYPE,XS_OBJECT_TYPE,(void**)&e->type,&e->typeref))
  }

  while (child) {
    if (child->m_ident == ELEM_UNIQUE) {
      /* FIXME */
    }
    else if (child->m_ident == ELEM_KEY) {
      /* FIXME */
    }
    else if (child->m_ident == ELEM_KEYREF) {
      /* FIXME */
    }
    else  {
      return invalid_element(&s->ei,s->m_uri,child);
    }

    xs_next_element2(&child);
  }


  return 0;
}

int GridXSLT::xs_parse_attribute_use(Schema *s, Node *node, int *use)
{
  int invalid = 0;

  *use = ATTRIBUTEUSE_OPTIONAL;

  String str = node->getAttribute(ATTR_USE);
  if (str.isNull())
    return 0;

  if ("optional" == str)
    *use = ATTRIBUTEUSE_OPTIONAL;
  else if ("prohibited" == str)
    *use = ATTRIBUTEUSE_PROHIBITED;
  else if ("required" == str)
    *use = ATTRIBUTEUSE_REQUIRED;
  else
    invalid = 1;
  if (invalid)
    return invalid_attribute_val(&s->ei,s->m_uri,node,ATTR_USE);
  return 0;
}

int GridXSLT::xs_parse_attribute(Schema *s, Node *node, const String &ns1,
                       int toplevel, list **aulist)
{
  char *ns = ns1.cstring();
  AttributeUse *au = NULL;
  SchemaAttribute *a = NULL;
  int use = ATTRIBUTEUSE_OPTIONAL;
  ValueConstraint *vcptr;

  /* @implements(xmlschema-1:src-attribute.5) @end
     @implements(xmlschema-1:a-props-correct.1) @end
     @implements(xmlschema-1:a-props-correct.2)
     status { unsupported } issue { requires support for simple type validation } @end
     @implements(xmlschema-1:au-props-correct.1) @end
     @implements(xmlschema-1:Attribute Declaration{name}) @end
     @implements(xmlschema-1:Attribute Use{required})
     test { attribute_local_default_use1.test }
     test { attribute_local_default_use2.test }
     test { attribute_local_default_use3.test }
     test { attribute_local_ref_default_use1.test }
     test { attribute_local_ref_default_use2.test }
     test { attribute_local_ref_default_use3.test }
     test { attribute_local_ref_use_invalid.test }
     test { attribute_local_use_invalid.test }
     test { attribute_local_use_optional.test }
     test { attribute_local_use_required.test }
     test { attribute_tl_use.test }
     @end
     @implements(xmlschema-1:Attribute Declaration{type definition}) @end
     @implements(xmlschema-1:Attribute Declaration{scope}) @end
     @implements(xmlschema-1:Attribute Use{attribute declaration}) @end
     @implements(xmlschema-1:Schema{attribute declarations}) @end
     */

  if (toplevel) {
    /* top-level attribute declaration */
    a = new SchemaAttribute(s->as);
    a->def.loc.line = node->m_line;
    CHECK_CALL(xs_init_toplevel_object(s,node,ns,s->symt->ss_attributes,a,
               &a->def.ident,"attribute"))
    CHECK_CALL(xs_check_forbidden_attribute(s,node,ATTR_REF))
    CHECK_CALL(xs_check_forbidden_attribute(s,node,ATTR_FORM))
    CHECK_CALL(xs_check_forbidden_attribute(s,node,ATTR_USE))

    vcptr = &a->vc;
    a->toplevel = 1;
  }
  else if (node->hasAttribute(ATTR_REF)) {
    /* local attribute reference */
    CHECK_CALL(xs_parse_attribute_use(s,node,&use))

    /* @implements(xmlschema-1:src-attribute.3.2)
       test { attribute_local_ref_name.test }
       test { attribute_local_ref_form.test }
       test { attribute_local_ref_type.test }
       @end */
    CHECK_CALL(xs_check_conflicting_attributes(s,node,ATTR_NAME,ATTR_REF,"src-attribute.3.2"))
    CHECK_CALL(xs_check_conflicting_attributes(s,node,ATTR_FORM,ATTR_REF,"src-attribute.3.2"))
    CHECK_CALL(xs_check_conflicting_attributes(s,node,ATTR_TYPE,ATTR_REF,"src-attribute.3.2"))

    au = new AttributeUse(s->as);
    au->defline = node->m_line;
    au->required = (ATTRIBUTEUSE_REQUIRED == use);
    au->prohibited = (ATTRIBUTEUSE_PROHIBITED == use);
    CHECK_CALL(xs_parse_ref(s,node,ATTR_REF,XS_OBJECT_ATTRIBUTE,(void**)&au->attribute,&au->ref))
    vcptr = &au->vc;
    list_append(aulist,au);
  }
  else {
    int qualified = s->attrformq;

    /* @implements(xmlschema-1:Attribute Declaration{target namespace})
       test { attribute_local_form.test }
       test { attribute_local_form2.test }
       test { attribute_local_form3.test }
       test { attribute_local_form4.test }
       test { attribute_d_local_form.test }
       test { attribute_d_local_form2.test }
       test { attribute_d_local_form3.test }
       test { attribute_d_local_form4.test }
       @end */
    CHECK_CALL(xs_parse_form(s,node,&qualified))

    /* local attribute declaration */
    CHECK_CALL(xs_parse_attribute_use(s,node,&use))

    a = new SchemaAttribute(s->as);
    a->def.loc.line = node->m_line;
    au = new AttributeUse(s->as);
    au->defline = node->m_line;

    /* @implements(xmlschema-1:src-attribute.3.1)
       test { attribute_local_noname.test }
       @end */
    if (!node->hasAttribute(ATTR_NAME))
      return missing_attribute(&s->ei,s->m_uri,node->m_line,"src-attribute.3.1",ATTR_NAME);
    a->def.ident.m_name = node->getAttribute(ATTR_NAME);
    if (ns && qualified)
      a->def.ident.m_ns = ns;
    au->required = (ATTRIBUTEUSE_REQUIRED == use);
    au->prohibited = (ATTRIBUTEUSE_PROHIBITED == use);
    au->attribute = a;

    vcptr = &au->vc;
    list_append(aulist,au);
  }

  /* @implements(xmlschema-1:no-xmlns)
     test { attribute_tl_xmlns.test }
     test { attribute_local_xmlns.test }
     @end */
  /* FIXME: when form is implemented, make sure attrs can be declared with the schema having
     a targetNamespace of XSI_NAMESPACE when the attr doesn't inherit this namespace } */
  if (a && (a->def.ident.m_name == "xmlns"))
    return error(&s->ei,s->m_uri,node->m_line,
                 "no-xmlns","\"xmlns\" cannot be used as an attribute name");

  if (au && a) {
    a->vc.type = au->vc.type;
    a->vc.value = au->vc.value;
  }

  /* @implements(xmlschema-1:no-xsi)
     test { attribute_tl_xsi.test }
     test { attribute_local_xsi.test }
     @end */
  if (a && (a->def.ident.m_ns == XSI_NAMESPACE))
    return error(&s->ei,s->m_uri,node->m_line,"no-xsi",
                 "attributes cannot be declared with a target namespace of \"%*\"",&XSI_NAMESPACE);
  /* FIXME: need to add the 4 built-in types in section 3.2.7 by default */

  /* @implements(xmlschema-1:src-attribute.1)
     test { attribute_tl_default_fixed.test }
     test { attribute_local_default_fixed.test }
     test { attribute_local_ref_default_fixed.test }
     @end
     @implements(xmlschema-1:Attribute Declaration{value constraint}) @end
     @implements(xmlschema-1:Attribute Use{value constraint}) @end */
  CHECK_CALL(xs_parse_value_constraint(s,node,vcptr,"src-attribute.1"))

  /* @implements(xmlschema-1:src-attribute.2)
     description { if default present, use must be optional }
     test { attribute_local_default_use1.test }
     test { attribute_local_default_use2.test }
     test { attribute_local_default_use3.test }
     test { attribute_local_ref_default_use1.test }
     test { attribute_local_ref_default_use2.test }
     test { attribute_local_ref_default_use3.test }
     @end */
  if (node->hasAttribute(ATTR_DEFAULT) && (ATTRIBUTEUSE_OPTIONAL != use))
    return error(&s->ei,s->m_uri,node->m_line,"src-attribute.2",
                 "\"use\" must be \"optional\" when default value specified");

  if (!node->hasAttribute(ATTR_REF)) {
    CHECK_CALL(xs_parse_ref(s,node,ATTR_TYPE,XS_OBJECT_TYPE,(void**)&a->type,&a->typeref))
    if (a->typeref) {
      a->typeref->check = xs_check_attribute_type;
      a->typeref->source = a;
    }
  }

  Node *child;
  if (NULL != (child = xs_first_non_annotation_child2(node))) {
    if (child->m_ident != ELEM_SIMPLE_TYPE)
      return invalid_element(&s->ei,s->m_uri,child);

    /* @implements(xmlschema-1:src-attribute.3) @end
       @implements(xmlschema-1:src-attribute.3.1) test { attribute_local_ref_simpletype.test }
       @end */
    if (node->hasAttribute(ATTR_REF))
      return error(&s->ei,s->m_uri,child->m_line,"src-attribute.3.1",
                   "<simpleType> not allowed here when \"ref\" is set on <attribute>");

    /* @implements(xmlschema-1:src-attribute.4) test { attribute_tl_type_simple_ref.test} @end */
    if (NULL != a->typeref)
      return error(&s->ei,s->m_uri,child->m_line,"src-attribute.4",
                   "<simpleType> not allowed here when \"type\" is set on <attribute>");
    CHECK_CALL(xs_parse_simple_type(s,child,ns,0,&a->type))
    ASSERT(NULL != a->type);
  }

  if (!node->hasAttribute(ATTR_REF) && (NULL == a->type) && (NULL == a->typeref))
    a->type = s->globals->simple_ur_type;

  return 0;
}

int GridXSLT::xs_parse_attribute_group_def(Schema *s, Node *node,
                                           const String &ns1)
{
  char *ns = ns1.cstring();
  AttributeGroup *ag = new AttributeGroup(s->as);

  /* @implements(xmlschema-1:src-attribute_group.1) @end
     @implements(xmlschema-1:ag-props-correct.1) @end
     @implements(xmlschema-1:Attribute Group Definition{name}) @end
     @implements(xmlschema-1:Attribute Group Definition{target namespace}) @end
     @implements(xmlschema-1:Attribute Group Definition{attribute uses}) @end
     @implements(xmlschema-1:Attribute Group Definition{attribute wildcard}) @end
     @implements(xmlschema-1:Schema{attribute group definitions}) @end
   */

  CHECK_CALL(xs_init_toplevel_object(s,node,ns,s->symt->ss_attribute_groups,ag,
                                     &ag->def.ident,"attribute group"))
  ag->def.loc.line = node->m_line;
  CHECK_CALL(xs_check_forbidden_attribute(s,node,ATTR_REF))

  Node *child;
  for (child = xs_first_non_annotation_child2(node); child; xs_next_element2(&child)) {
    /* @implements(xmlschema-1:Complex Type Definition{attribute uses}) @end */
    if (child->m_ident == ELEM_ATTRIBUTE)
      CHECK_CALL(xs_parse_attribute(s,child,ns,0,&ag->local_attribute_uses))
    else if (child->m_ident == ELEM_ATTRIBUTE_GROUP)
      CHECK_CALL(xs_parse_attribute_group_ref(s,child,ns,&ag->attribute_group_refs))
    else
      break;
  }

  if (child && (child->m_ident == ELEM_ANY_ATTRIBUTE)) {
    /* FIXME: need to do all the magic here as described in the spec - the same as for
       <anyAttribute> inside a complex type */
    CHECK_CALL(xs_parse_wildcard(s,child,&ag->local_wildcard))
    xs_next_element2(&child);
  }

  if (child)
    return invalid_element(&s->ei,s->m_uri,child);

  return 0;
}

int GridXSLT::xs_parse_attribute_group_ref(Schema *s, Node *node,
                                           const String &ns1, list **reflist)
{
  AttributeGroupRef *agr = new AttributeGroupRef(s->as);
  CHECK_CALL(xs_check_forbidden_attribute(s,node,ATTR_NAME))

  if (!node->hasAttribute(ATTR_REF))
    return missing_attribute(&s->ei,s->m_uri,node->m_line,String::null(),ATTR_REF);
  CHECK_CALL(xs_parse_ref(s,node,ATTR_REF,XS_OBJECT_ATTRIBUTE_GROUP,(void**)&agr->ag,&agr->ref))
  ASSERT(agr->ref);


  list_append(reflist,agr);

  return 0;
}

int xs_facet_num(const String &str)
{
  int i;
  for (i = 0; i < XS_FACET_NUMFACETS; i++)
    if (xs_facet_names[i] == str)
      return i;
  ASSERT(0);
  return 0;
}

int xs_parse_facet(Schema *s, Node *node, xs_facetdata *fd)
{
  int facet = xs_facet_num(node->m_ident.m_name);

  /* @implements(xmlschema-1:Simple Type Definition{facets})
     test { simpletype_atomic_facets.test }
     test { simpletype_list_facets.test }
     test { simpletype_list_facet.test }
     test { simpletype_union_facets.test }
     @end */
  String value = node->getAttribute(ATTR_VALUE); // FIXME: collapse whitespace in all cases?
  if (value.isNull())
    return missing_attribute(&s->ei,s->m_uri,node->m_line,String::null(),ATTR_VALUE);

  if (XS_FACET_PATTERN == facet) {
    fd->patterns.append(value);
  }
  else if (XS_FACET_ENUMERATION == facet) {
    fd->enumerations.append(value);
  }
  else {
    if (NULL != fd->strval[facet])
      return error(&s->ei,s->m_uri,node->m_line,String::null(),"facet already defined");
    fd->strval[facet] = value.collapseWhitespace().cstring();
    fd->defline[facet] = node->m_line;

    if ((XS_FACET_MINLENGTH == facet) ||
        (XS_FACET_MAXLENGTH == facet) ||
        (XS_FACET_LENGTH == facet) ||
        (XS_FACET_TOTALDIGITS == facet) ||
        (XS_FACET_FRACTIONDIGITS == facet)) {
      if (0 != convert_to_nonneg_int(fd->strval[facet],&fd->intval[facet]))
        return error(&s->ei,s->m_uri,node->m_line,String::null(),
                     "Invalid value for facet %*: must be a non-negative integer",
                     &node->m_ident.m_name);
    }
    else if (XS_FACET_TOTALDIGITS == facet) {
      if ((0 != convert_to_nonneg_int(fd->strval[facet],&fd->intval[facet])) ||
          (0 == fd->intval[facet]))
        return error(&s->ei,s->m_uri,node->m_line,String::null(),
                     "Invalid value for facet %*: must be a positive integer",
                     &node->m_ident.m_name);
    }

  }

  return 0;
}

int xs_is_builtin_type_redeclaration(Schema *s, const String &ns, Node *node)
{
  if (ns == XS_NAMESPACE) {
    Type *existing;
    String name = node->getAttribute(ATTR_NAME).collapseWhitespace();
    if (!name.isNull() &&
        (NULL != (existing = (Type*)s->globals->symt->lookup(XS_OBJECT_TYPE,
                                                             NSName(ns,name)))) &&
          existing->builtin) {
      return 1;
    }
  }
  return 0;
}

int GridXSLT::xs_parse_simple_type(Schema *s, Node *node, const String &ns1,
                         int toplevel, Type **tout)
{
  char *ns = ns1.cstring();
  Type *t;

  /* @implements(xmlschema-1:Simple Type Definition{name}) @end
     @implements(xmlschema-1:Simple Type Definition{target namespace}) @end
     @implements(xmlschema-1:Simple Type Definition{base type definition}) @end
     @implements(xmlschema-1:Simple Type Definition{variety}) @end
     @implements(xmlschema-1:src-simple-type.1) @end
     @implements(xmlschema-1:st-props-correct.1) @end
  */

  /* FIXME: check this: The simple ur-type definition must not be named as the base type definition
     of any user-defined atomic simple type definitions: as it has no constraining facets, this
     would be incoherent. */

  if (toplevel) {
    /* Special case: Redefining a builtin type in the XML Schema namespace. In this case we
       simply ignore the type definition. This is necessary for parsing the Schema for Schemas
       included with the spec, and relied upon by other specs such as XSLT which use the XML
       Schema types */
    if (xs_is_builtin_type_redeclaration(s,ns1,node))
      return 0;

    t = new Type(s->as);
    t->def.loc.line = node->m_line;
    CHECK_CALL(xs_init_toplevel_object(s,node,ns,s->symt->ss_types,t,&t->def.ident,"type"))

    /* @implements(xmlschema-1:Simple Type Definition{final})
       test { simpletype_final1.test }
       test { simpletype_final2.test }
       test { simpletype_final3.test }
       test { simpletype_final4.test }
       test { simpletype_final5.test }
       test { simpletype_final6.test }
       test { simpletype_final7.test }
       @end */
    /* FIXME: spec part 1 says to include restriction here, but part 2 (normative?) says just
       restriction, list, and union. Remove extension if it is not needed by the derivation
       validity rules. */
    CHECK_CALL(xs_parse_block_final_attr(s,node,ATTR_FINAL,s->final_default,
               &t->final_extension,&t->final_restriction,NULL,
               &t->final_list,&t->final_union));
  }
  else {
    t = new Type(s->as);
    t->def.loc.line = node->m_line;
    CHECK_CALL(xs_check_forbidden_attribute(s,node,ATTR_NAME))
    CHECK_CALL(xs_check_forbidden_attribute(s,node,ATTR_FINAL))
  }

  t->variety = TYPE_VARIETY_INVALID;
  if (tout)
    *tout = t;

  Node *child = xs_first_non_annotation_child2(node);

  if (!child)
    return error(&s->ei,s->m_uri,node->m_line,String::null(),
                 "<simpleType> requires a <restriction>, <list>, or <union>");

  if (child->m_ident == ELEM_RESTRICTION) {
    Node *child2 = xs_first_non_annotation_child2(child);

    t->stype = TYPE_SIMPLE_RESTRICTION;
    CHECK_CALL(xs_parse_ref(s,child,ATTR_BASE,XS_OBJECT_TYPE,(void**)&t->base,&t->baseref))

    /* @implements(xmlschema-1:src-simple-type.2)
       test { simpletype_restriction.test }
       test { simpletype_restriction_basechild.test }
       test { simpletype_restriction_list.test }
       test { simpletype_restriction_list2.test }
       test { simpletype_restriction_nobase.test }
       test { simpletype_restriction_restriction.test }
       test { simpletype_restriction_restriction2.test }
       @end */
    if (child2 && (child2->m_ident == ELEM_SIMPLE_TYPE)) {
      if (NULL != t->baseref)
        return error(&s->ei,s->m_uri,child2->m_line,"src-simple-type.2",
                     "<simpleType> not allowed here when \"base\" is set on <restriction>");
      CHECK_CALL(xs_parse_simple_type(s,child2,ns,0,&t->base));
      t->variety = t->base->variety;
      xs_next_element2(&child2);
    }

    if ((NULL == t->base) && (NULL == t->baseref))
      return error(&s->ei,s->m_uri,child->m_line,"structures-3.14.4",
                   "<restriction> must have either the "
                   "\"base\" attribute set, or a <simpleType> child");

    while (child2) {

      if ((child2->m_ident == ELEM_LENGTH) ||
          (child2->m_ident == ELEM_MIN_LENGTH) ||
          (child2->m_ident == ELEM_MAX_LENGTH) ||
          (child2->m_ident == ELEM_PATTERN) ||
          (child2->m_ident == ELEM_ENUMERATION) ||
          (child2->m_ident == ELEM_WHITE_SPACE) ||
          (child2->m_ident == ELEM_MAX_INCLUSIVE) ||
          (child2->m_ident == ELEM_MAX_EXCLUSIVE) ||
          (child2->m_ident == ELEM_MIN_EXCLUSIVE) ||
          (child2->m_ident == ELEM_MIN_INCLUSIVE) ||
          (child2->m_ident == ELEM_TOTAL_DIGITS) ||
          (child2->m_ident == ELEM_FRACTION_DIGITS)) {
        CHECK_CALL(xs_parse_facet(s,child2,&t->facets))
      }
      else {
        return invalid_element(&s->ei,s->m_uri,child2);
      }

      xs_next_element2(&child2);
    }

    t->restriction = 1; /* used to set variety when base type is resolved */

    xs_next_element2(&child);
  }
  else if (child->m_ident == ELEM_LIST) {
    Node *child2 = xs_first_non_annotation_child2(child);

    t->base = s->globals->simple_ur_type;

    CHECK_CALL(xs_parse_ref(s,child,ATTR_ITEM_TYPE,XS_OBJECT_TYPE,
                            (void**)&t->item_type,&t->item_typeref))

    /* @implements(xmlschema-1:src-simple-type.3)
       test { simpletype_list_itemtypechild.test }
       test { simpletype_list_list.test }
       test { simpletype_list_noitemtype.test }
       test { simpletype_list_restriction2.test }
       test { simpletype_list_restriction.test }
       test { simpletype_list.test }
       @end */
    if (child2 && (child2->m_ident == ELEM_SIMPLE_TYPE)) {
      if (NULL != t->item_typeref)
        return error(&s->ei,s->m_uri,child2->m_line,"src-simple-type.3",
                     "<simpleType> not allowed here when \"itemType\" is set on <list>");
      CHECK_CALL(xs_parse_simple_type(s,child2,ns,0,&t->item_type));
      xs_next_element2(&child2);
    }

    if ((NULL == t->item_type) && (NULL == t->item_typeref))
      return error(&s->ei,s->m_uri,child->m_line,"src-simple-type.3","<list> must have "
                   "either the \"itemType\" attribute set, or a <simpleType> child");

    if (child2)
      return invalid_element(&s->ei,s->m_uri,child2);

    t->variety = TYPE_VARIETY_LIST;
    t->stype = TYPE_SIMPLE_LIST;

    /* FIXME */
    xs_next_element2(&child);
  }
  else if (child->m_ident == ELEM_UNION) {
//     char *member_types = child->getAttribute(ATTR_MEMBER_TYPES).cstring();
    String mt = child->getAttribute(ATTR_MEMBER_TYPES);


    t->base = s->globals->simple_ur_type;

    if (!mt.isNull()) {
      List<String> memberTypes = child->getAttribute(ATTR_MEMBER_TYPES).parseList();
      for (Iterator<String> it = memberTypes; it.haveCurrent(); it++) {
        MemberType *mt;

        QName qn = QName::parse(*it);
        NSName resolved = child->resolveQName(qn);
        if (resolved.isNull()) {
          error(&s->ei,s->m_uri,child->m_line,"src-qname.1.1",
                "Could not resolve namespace for prefix \"%*\"",&qn.m_prefix);
          return -1;
        }

        /* FIXME!: make sure member types are handled property with namespaces */
/*         debugl("line %d: Parsing union member type %s",c->line,start); */
        mt = new MemberType(s->as);
        list_append(&t->members,mt);
        mt->ref = new Reference(s->as);
        mt->ref->s = s;
        mt->ref->def.ident = resolved;
        mt->ref->def.loc.line = child->m_line;
        mt->ref->type = XS_OBJECT_TYPE;
        mt->ref->obj = (void**)&mt->type;
      }
    }

    Node *child2 = xs_first_non_annotation_child2(child);
    while (child2) {
      MemberType *mt;
      if (child2->m_ident != ELEM_SIMPLE_TYPE)
        return invalid_element(&s->ei,s->m_uri,child2);

      mt = new MemberType(s->as);
      CHECK_CALL(xs_parse_simple_type(s,child2,ns,0,&mt->type));
      ASSERT(NULL != mt->type);
      list_append(&t->members,mt);

      xs_next_element2(&child2);
    }

    if (NULL == t->members)
      return error(&s->ei,s->m_uri,child->m_line,String::null(),
                   "<union> requires one or more member types");

    t->variety = TYPE_VARIETY_UNION;
    t->stype = TYPE_SIMPLE_UNION;
    xs_next_element2(&child);
  }

  if (child)
    return invalid_element(&s->ei,s->m_uri,child);

  return 0;
}

int GridXSLT::xs_parse_complex_type_attributes(Schema *s, Node *node,
                                               const String &ns1, Type *t, Node *child)
{
  char *ns = ns1.cstring();
  for (; child; xs_next_element2(&child)) {
    /* FIXME: the attribute_uses of a type should also contain all of the attribute uses contained
       in any referenced groups (recursively), and any attribute uses declared on base types
       (recursively), including those of their attribute group references (recursively) */
    if (child->m_ident == ELEM_ATTRIBUTE)
      CHECK_CALL(xs_parse_attribute(s,child,ns,0,&t->local_attribute_uses))
    else if (child->m_ident == ELEM_ATTRIBUTE_GROUP)
      CHECK_CALL(xs_parse_attribute_group_ref(s,child,ns,&t->attribute_group_refs))
    else
      break;
  }

  if (child && (child->m_ident == ELEM_ANY_ATTRIBUTE)) {
    /* {attribute wildcard} 1.1 */
    CHECK_CALL(xs_parse_wildcard(s,child,&t->local_wildcard))
    ASSERT(t->local_wildcard);
    xs_next_element2(&child);
  }

  if (child)
    return invalid_element(&s->ei,s->m_uri,child);

  return 0;
}

int GridXSLT::xs_parse_simple_content_children(Schema *s, Node *node,
                                               const String &ns1, Type *t, Node *child)
{
  char *ns = ns1.cstring();
  if (TYPE_DERIVATION_RESTRICTION == t->derivation_method) {

    if (child && (child->m_ident == ELEM_SIMPLE_TYPE)) {
      CHECK_CALL(xs_parse_simple_type(s,child,ns,0,&t->child_type));
      xs_next_element2(&child);
    }

    while (child && ((child->m_ident == ELEM_LENGTH) ||
                     (child->m_ident == ELEM_MIN_LENGTH) ||
                     (child->m_ident == ELEM_MAX_LENGTH) ||
                     (child->m_ident == ELEM_PATTERN) ||
                     (child->m_ident == ELEM_ENUMERATION) ||
                     (child->m_ident == ELEM_WHITE_SPACE) ||
                     (child->m_ident == ELEM_MAX_INCLUSIVE) ||
                     (child->m_ident == ELEM_MAX_EXCLUSIVE) ||
                     (child->m_ident == ELEM_MIN_EXCLUSIVE) ||
                     (child->m_ident == ELEM_MIN_INCLUSIVE) ||
                     (child->m_ident == ELEM_TOTAL_DIGITS) ||
                     (child->m_ident == ELEM_FRACTION_DIGITS))) {
      CHECK_CALL(xs_parse_facet(s,child,&t->child_facets))
      xs_next_element2(&child);
    }
  }

  return xs_parse_complex_type_attributes(s,node,ns,t,child);
}

int GridXSLT::xs_parse_complex_content_children(Schema *s,
                                   Node *node, const String &ns1,
                                   Type *t, Node *child, int effective_mixed)
{
  char *ns = ns1.cstring();
  /* Section 3.4.2 - Complex type definition with complex content */

  /* ((group | all | choice | sequence)?, ((attribute | attributeGroup)*, anyAttribute?)) */

  /* {content type} rules */
  int test211 = ((NULL == child) ||
                 ((child->m_ident != ELEM_GROUP) &&
                  (child->m_ident != ELEM_ALL) &&
                  (child->m_ident != ELEM_CHOICE) &&
                  (child->m_ident != ELEM_SEQUENCE)));
  int test212 = (child &&
                 ((child->m_ident == ELEM_ALL) ||
                  (child->m_ident == ELEM_SEQUENCE)) &&
                 (NULL == xs_first_non_annotation_child2(child)));
  int min_occurs_val = 1;
  int convr = 0;
  int test213 = (child &&
                 (child->m_ident == ELEM_ALL) &&
                 (NULL == xs_first_non_annotation_child2(child)) &&
                 (0 == (convr = parse_optional_int_attr(&s->ei,s->m_uri,child,ATTR_MIN_OCCURS,
                                                        &min_occurs_val,1))) &&
                 (0 == min_occurs_val));
  Particle *effective_content = NULL;

  if (0 != convr) /* FIXME: set error info here! */
    return -1;

  if (test211 || test212 || test213) {
    /* 2.1.4, 2.1.5 */
    if (effective_mixed) {
      Particle *p = new Particle(s->as);
      p->range.min_occurs = 1;
      p->range.max_occurs = 1;
      p->term_type = PARTICLE_TERM_MODEL_GROUP;
      p->term.mg = new ModelGroup(s->as);
      p->term.mg->compositor = MODELGROUP_COMPOSITOR_SEQUENCE;
      p->term.mg->defline = child ? child->m_line : node->m_line;
      p->term.mg->content_model_of = t;
      p->defline = node->m_line;
      p->empty_content_of = t;
      effective_content = p;
    }
    else {
      effective_content = NULL; /* empty */
    }
  }
  else {
    if (child->m_ident == ELEM_GROUP) {
      CHECK_CALL(xs_parse_group_ref(s,child,NULL,&effective_content))
    }
    else { /* must be <all>, <choice> or <sequence> - otherwise test211 would be true */
      CHECK_CALL(xs_parse_all_choice_sequence(s,child,ns,NULL,&effective_content))
      ASSERT(effective_content);
      ASSERT(PARTICLE_TERM_MODEL_GROUP == effective_content->term_type);
      ASSERT(effective_content->term.mg);
      effective_content->term.mg->content_model_of = t;
    }
  }

  if (!test211) {
    xs_next_element2(&child);
  }

  t->effective_content = effective_content;
  t->effective_mixed = effective_mixed;
  if (t->effective_content)
    t->effective_content->effective_content_of = t;

  /* {content type} rule 3 - done after resolution of the base type */

  return xs_parse_complex_type_attributes(s,node,ns,t,child);
}

int GridXSLT::xs_parse_complex_type(Schema *s, Node *node, const String &ns1,
                          int toplevel, Type **tout)
{
  char *ns = ns1.cstring();
  Type *t;
  int effective_mixed = 0;

  /* @implements(xmlschema-1:src-ct.3) @end
     @implements(xmlschema-1:ct-props-correct.1) @end
     @implements(xmlschema-1:Complex Type Definition{name}) @end
     @implements(xmlschema-1:Complex Type Definition{target namespace}) @end
     @implements(xmlschema-1:Schema{type definitions}) @end
   */

  if (toplevel) {
    /* Special case: Redefining a builtin type in the XML Schema namespace. In this case we
       simply ignore the type definition. This is necessary for parsing the Schema for Schemas
       included with the spec, and relied upon by other specs such as XSLT which use the XML
       Schema types */
    if (xs_is_builtin_type_redeclaration(s,ns1,node))
      return 0;

    t = new Type(s->as);
    t->def.loc.line = node->m_line;
    CHECK_CALL(xs_init_toplevel_object(s,node,ns,s->symt->ss_types,t,&t->def.ident,"type"))

    /* @implements(xmlschema-1:Complex Type Definition{prohibited substitutions})
       test { complextype_cc_extension_block.test }
       test { complextype_cc_extension_block2.test }
       test { complextype_cc_extension_block3.test }
       test { complextype_cc_extension_block4.test }
       test { complextype_cc_extension_block5.test }
       test { complextype_cc_extension_block6.test }
       @end */
    CHECK_CALL(xs_parse_block_final_attr(s,node,ATTR_BLOCK,s->block_default,
               &t->prohibited_extension,&t->prohibited_restriction,NULL,NULL,NULL));

    /* @implements(xmlschema-1:Complex Type Definition{final})
       test { complextype_cc_extension_final.test }
       test { complextype_cc_extension_final2.test }
       test { complextype_cc_extension_final3.test }
       test { complextype_cc_extension_final4.test }
       test { complextype_cc_extension_final5.test }
       test { complextype_cc_extension_final6.test }
       @end */
    CHECK_CALL(xs_parse_block_final_attr(s,node,ATTR_FINAL,s->final_default,
               &t->final_extension,&t->final_restriction,NULL,NULL,NULL));
  }
  else {
    t = new Type(s->as);
    t->def.loc.line = node->m_line;
    CHECK_CALL(xs_check_forbidden_attribute(s,node,ATTR_NAME))
    CHECK_CALL(xs_check_forbidden_attribute(s,node,ATTR_ABSTRACT))
    CHECK_CALL(xs_check_forbidden_attribute(s,node,ATTR_FINAL))
    CHECK_CALL(xs_check_forbidden_attribute(s,node,ATTR_BLOCK))
  }

  t->complex = 1;

  /* @implements(xmlschema-1:Complex Type Definition{abstract})
     test { complextype_abstract.test }
     test { complextype_abstract2.test }
     @end */
  CHECK_CALL(parse_optional_boolean_attr(&s->ei,s->m_uri,node,ATTR_ABSTRACT,&t->abstract,0))
  CHECK_CALL(parse_optional_boolean_attr(&s->ei,s->m_uri,node,ATTR_MIXED,&effective_mixed,0))
  if (tout)
    *tout = t;

  Node *child = xs_first_non_annotation_child2(node);

  if (child && ((child->m_ident == ELEM_SIMPLE_CONTENT) ||
                (child->m_ident == ELEM_COMPLEX_CONTENT))) {

    Node *child2;
    if (NULL == (child2 = xs_first_non_annotation_child2(child)))
      return error(&s->ei,s->m_uri,child->m_line,String::null(),
                   "expected <restriction> or <extension>");

    /* @implements(xmlschema-1:Complex Type Definition{derivation method}) @end */
    if (child2->m_ident == ELEM_RESTRICTION)
      t->derivation_method = TYPE_DERIVATION_RESTRICTION;
    else if (child2->m_ident == ELEM_EXTENSION)
      t->derivation_method = TYPE_DERIVATION_EXTENSION;
    else
      return invalid_element(&s->ei,s->m_uri,child2);

    /* nothing else allowed after <restriction> or <extension> */
    Node *rest = child2;
    xs_next_element2(&rest);
    if (rest)
      return invalid_element(&s->ei,s->m_uri,rest);

    /* @implements(xmlschema-1:Complex Type Definition{base type definition}) @end */
    if (!child2->hasAttribute(ATTR_BASE))
      return missing_attribute(&s->ei,s->m_uri,child2->m_line,String::null(),ATTR_BASE);
    CHECK_CALL(xs_parse_ref(s,child2,ATTR_BASE,XS_OBJECT_TYPE,(void**)&t->base,&t->baseref))

    if (child->m_ident == ELEM_SIMPLE_CONTENT) {
      Node *child3;
      if (NULL != (child3 = xs_first_non_annotation_child2(child2)))
        CHECK_CALL(xs_parse_simple_content_children(s,node,ns,t,child3))
    }
    else { /* complexContent */
      CHECK_CALL(parse_optional_boolean_attr(&s->ei,s->m_uri,child,ATTR_MIXED,
                                             &effective_mixed,effective_mixed))
      t->complex_content = 1;
      Node *child3 = xs_first_non_annotation_child2(child2);
      CHECK_CALL(xs_parse_complex_content_children(s,node,ns,t,child3,effective_mixed))
    }

    /* FIXME: write testcases for extra elements appearing where they are not allowed (for all
       cases where this can occur) */
    xs_next_element2(&child2);
    if (child2)
      return invalid_element(&s->ei,s->m_uri,child2);

    xs_next_element2(&child);
    if (child)
      return invalid_element(&s->ei,s->m_uri,child);
  }
  else {
    /* <complexContent> omitted - this is shorthand for complex content restricting ur-type.
       Parse the remaining children of n as if they were inside the <restriction> element */
    t->base = s->globals->complex_ur_type;
    t->derivation_method = TYPE_DERIVATION_RESTRICTION;
    t->complex_content = 1;
    return xs_parse_complex_content_children(s,node,ns,t,child,effective_mixed);
  }

  return 0;
}

int GridXSLT::xs_parse_group_def(Node *node, const String &ns1, Schema *s)
{
  char *ns = ns1.cstring();
  /* parent is a <schema> or <redifine> */
  ModelGroupDef *mgd;

  /* @implements(xmlschema-1:src-model_group_defn) @end
     @implements(xmlschema-1:mgd-props-correct) @end
     @implements(xmlschema-1:src-model_group) @end
     @implements(xmlschema-1:mg-props-correct.1) @end

     @implements(xmlschema-1:Model Group Definition{name}) @end
     @implements(xmlschema-1:Model Group Definition{target namespace}) @end
     @implements(xmlschema-1:Model Group Definition{model group}) @end
     @implements(xmlschema-1:Schema{model group definitions}) @end
   */

  mgd = new ModelGroupDef(s->as);
  mgd->def.loc.line = node->m_line;
  CHECK_CALL(xs_init_toplevel_object(s,node,ns,s->symt->ss_model_group_defs,mgd,
                                     &mgd->def.ident,"group"))
  CHECK_CALL(xs_check_conflicting_attributes(s,node,ATTR_REF,ATTR_NAME,NULL))
  CHECK_CALL(xs_check_conflicting_attributes(s,node,ATTR_MIN_OCCURS,ATTR_NAME,NULL))
  CHECK_CALL(xs_check_conflicting_attributes(s,node,ATTR_MAX_OCCURS,ATTR_NAME,NULL))

  mgd->model_group = NULL; /* FIXME */
  mgd->annotation = NULL;
  /* FIXME: handle the case of <redefine> */

  Node *child;
  if (NULL == (child = xs_first_non_annotation_child2(node)))
    return error(&s->ei,s->m_uri,node->m_line,String::null(),
                 "Model group definition requires an <all>, <choice> or <sequence> child element");

  if ((child->m_ident != ELEM_ALL) &&
      (child->m_ident != ELEM_CHOICE) &&
      (child->m_ident != ELEM_SEQUENCE))
    return invalid_element(&s->ei,s->m_uri,child);

  CHECK_CALL(xs_parse_model_group(s,child,ns,&mgd->model_group))
  ASSERT(mgd->model_group);
  mgd->model_group->mgd = mgd;

  xs_next_element2(&child);
  if (child)
    return invalid_element(&s->ei,s->m_uri,child);

  return 0;
}

int GridXSLT::xs_parse_group_ref(Schema *s, Node *node, list **particles_list,
                       Particle **pout)
{
  int max_occurs_val = 1;
  int min_occurs_val = 1;
  Particle *p;

  if (!node->hasAttribute(ATTR_REF))
    return error(&s->ei,s->m_uri,node->m_line,String::null(),
                 "<group> can only be used as a group reference here");

  CHECK_CALL(xs_check_conflicting_attributes(s,node,ATTR_NAME,ATTR_REF,NULL))
  CHECK_CALL(xs_parse_max_occurs(s,node,&max_occurs_val))
  CHECK_CALL(parse_optional_int_attr(&s->ei,s->m_uri,node,ATTR_MIN_OCCURS,&min_occurs_val,1))

  if ((0 == min_occurs_val) && (0 == max_occurs_val))
    return 0; /* do not create a schema component */

  p = new Particle(s->as);
  p->range.min_occurs = min_occurs_val;
  p->range.max_occurs = max_occurs_val;
  p->term.mg = NULL; /* will be resolved later */
  p->term_type = PARTICLE_TERM_MODEL_GROUP;
  p->defline = node->m_line;
  if (particles_list)
    list_append(particles_list,p);
  if (pout)
    *pout = p;

  CHECK_CALL(xs_parse_ref(s,node,ATTR_REF,XS_OBJECT_MODEL_GROUP_DEF,(void**)&p->term.mg,&p->ref))

  /* We can't do the resolution here because it may depend on a model group definition that
     we haven't encountered yet. Instead, the resolution is done in xs_resolve_ref() */

  return 0;
}

int GridXSLT::xs_parse_model_group(Schema *s, Node *node, const String &ns1,
                                   ModelGroup **mgout)
{
  char *ns = ns1.cstring();
  /* <all>, <choice>, or <sequence> */
  ModelGroup *mg;
  int allow_gcsa = 0;

  /* @implements(xmlschema-1:Model Group{compositor}) @end
     @implements(xmlschema-1:Model Group{particles}) @end
     */

  /* Create the particle and model group schema components. Because the particle is added
     to the supplied list, and the model is referenced from the particle, these will both
     be freed up eventually even if we return with this function with an error. */
  mg = new ModelGroup(s->as);
  mg->defline = node->m_line;

  if (node->m_ident == ELEM_ALL)
    mg->compositor = MODELGROUP_COMPOSITOR_ALL;
  else if (node->m_ident == ELEM_CHOICE)
    mg->compositor = MODELGROUP_COMPOSITOR_CHOICE;
  else if (node->m_ident == ELEM_SEQUENCE)
    mg->compositor = MODELGROUP_COMPOSITOR_SEQUENCE;
  else
    ASSERT(0);

  mg->particles = NULL;
  mg->annotation = NULL;
  ASSERT(mgout);
  *mgout = mg;

  /* Iterate through the child elements of the model group.
     <choice> and <sequence> can have the following children: element, group, choice, sequence, any
     <all> can only have children of type element
     In all cases we allow an annotation element as the first child. */
  allow_gcsa = (MODELGROUP_COMPOSITOR_CHOICE == mg->compositor) ||
               (MODELGROUP_COMPOSITOR_SEQUENCE == mg->compositor);

  Node *child = xs_first_non_annotation_child2(node);

  while (child) {

    if (child->m_ident == ELEM_ELEMENT)
      CHECK_CALL(xs_parse_element(child,ns,s,&mg->particles))
    else if (allow_gcsa && (child->m_ident == ELEM_GROUP))
      CHECK_CALL(xs_parse_group_ref(s,child,&mg->particles,NULL))
    else if (allow_gcsa && (child->m_ident == ELEM_CHOICE))
      CHECK_CALL(xs_parse_all_choice_sequence(s,child,ns,&mg->particles,NULL))
    else if (allow_gcsa && (child->m_ident == ELEM_SEQUENCE))
      CHECK_CALL(xs_parse_all_choice_sequence(s,child,ns,&mg->particles,NULL))
    else if (allow_gcsa && (child->m_ident == ELEM_ANY))
      CHECK_CALL(xs_parse_any(s,child,&mg->particles))
    else
      return invalid_element(&s->ei,s->m_uri,child);

    xs_next_element2(&child);
  }

  return 0;
}

int GridXSLT::xs_parse_all_choice_sequence(Schema *s, Node *node, const String &ns1,
                                           list **particles_list, Particle **pout)
{
  char *ns = ns1.cstring();
  Particle *p;
  ModelGroup *mg;

  int max_occurs_val = 1;
  int min_occurs_val = 1;

  /* For <choice> and <sequence>, maxOccurs can be "unbounded" or a positive integer */
  /* For <all>, maxOccurs must be equal to 1 */
  CHECK_CALL(xs_parse_max_occurs(s,node,&max_occurs_val))
  CHECK_CALL(parse_optional_int_attr(&s->ei,s->m_uri,node,ATTR_MIN_OCCURS,&min_occurs_val,1))

  /* @implements(xmlschema-1:cos-all-limited.1)
     test { all_maxoccurs1.test }
     test { all_maxoccurs2.test }
     test { all_maxoccurs_invalid.test }
     test { all_maxoccursunbounded.test }
     test { all_minoccurs0.test }
     test { all_minoccurs1.test }
     test { all_minoccurs2.test }
     test { all_minoccurs_invalid.test }
     @end
     @implements(xmlschema-1:cos-all-limited.1.1) @end
     @implements(xmlschema-1:cos-all-limited.1.2) @end */
  /* Note: although not specified by 3.8.6-2-1, the value of maxOccurs is fixed at 1 even when
     the <all> element is used within a model group definition, as the definition of the
     <all> element attributes in 3.8.2 constraints maxOccurs = 1 */
  if ((node->m_ident == ELEM_ALL) && (max_occurs_val != 1))
    return invalid_attribute_val(&s->ei,s->m_uri,node,ATTR_MAX_OCCURS);
  if ((node->m_ident == ELEM_ALL) &&
      (min_occurs_val != 0) && (min_occurs_val != 1))
    return invalid_attribute_val(&s->ei,s->m_uri,node,ATTR_MIN_OCCURS);

  CHECK_CALL(xs_parse_model_group(s,node,ns,&mg))
  ASSERT(mg);

  p = new Particle(s->as);
  p->term.mg = mg;
  p->term_type = PARTICLE_TERM_MODEL_GROUP;
  p->range.max_occurs = max_occurs_val;
  p->range.min_occurs = min_occurs_val;
  p->defline = node->m_line;
  if (particles_list)
    list_append(particles_list,p);
  if (pout)
    *pout = p;

  return 0;
}

int GridXSLT::xs_parse_wildcard(Schema *s, Node *node, Wildcard **wout)
{
  Wildcard *w;

  *wout = w = new Wildcard(s->as);
  w->defline = node->m_line;

  /* @implements(xmlschema-1:src-wildcard) @end
     @implements(xmlschema-1:w-props-correct) @end
  */

  /* @implements(xmlschema-1:Wildcard{namespace constraint})
     test { any_d_namespace1.test }
     test { any_d_namespace2.test }
     test { any_d_namespace3.test }
     test { any_d_namespace4.test }
     test { any_d_namespace5.test }
     test { any_d_namespace6.test }
     test { any_d_namespace7.test }
     test { any_d_namespace8.test }
     test { any_d_namespace9.test }
     test { any_d_namespace10.test }
     test { any_d_namespace11.test }
     test { any_d_no_namespace.test }
     test { anyattribute_d_namespace1.test }
     test { anyattribute_d_namespace2.test }
     test { anyattribute_d_namespace3.test }
     test { anyattribute_d_namespace4.test }
     test { anyattribute_d_namespace5.test }
     test { anyattribute_d_namespace6.test }
     test { anyattribute_d_namespace7.test }
     test { anyattribute_d_namespace8.test }
     test { anyattribute_d_namespace9.test }
     test { anyattribute_d_no_namespace.test }
     test { anyattribute_d_namespace10.test }
     test { anyattribute_d_namespace11.test }
     test { attributegroup_d_wildcard.test }
     test { any_namespace1.test }
     test { any_namespace2.test }
     test { any_namespace3.test }
     test { any_namespace4.test }
     test { any_namespace5.test }
     test { any_namespace6.test }
     test { any_namespace7.test }
     test { any_namespace8.test }
     test { any_namespace9.test }
     test { any_namespace10.test }
     test { any_namespace11.test }
     test { any_namespace12.test }
     test { any_no_namespace.test }
     test { anyattribute_namespace1.test }
     test { anyattribute_namespace2.test }
     test { anyattribute_namespace3.test }
     test { anyattribute_namespace4.test }
     test { anyattribute_namespace5.test }
     test { anyattribute_namespace6.test }
     test { anyattribute_namespace7.test }
     test { anyattribute_namespace8.test }
     test { anyattribute_namespace9.test }
     test { anyattribute_namespace10.test }
     test { anyattribute_namespace11.test }
     test { anyattribute_namespace12.test }
     test { anyattribute_no_namespace.test }
     @end
   */
  String namesp = node->getAttribute(ATTR_NAMESPACE).collapseWhitespace();
  if (namesp.isNull()) {
    w->type = WILDCARD_TYPE_ANY;
  }
  else {
    if ("##any" == namesp) {
      w->type = WILDCARD_TYPE_ANY;
    }
    else if ("##other" == namesp) {
      w->type = WILDCARD_TYPE_NOT;
      w->not_ns = s->m_targetNamespace;
    }
    else {
      w->type = WILDCARD_TYPE_SET;
      List<String> values = namesp.parseList();
      for (Iterator<String> it = values; it.haveCurrent(); it++) {
        String cur = *it;
        if ("##targetNamespace" == cur)
          w->nslist.append(s->m_targetNamespace);
        else if ("##local" == cur)
          w->nslist.append(String::null());
        else
          w->nslist.append(cur);
      }
    }
  }

  /* @implements(xmlschema-1:Wildcard{process contents})
     test { anyattribute_processcontents_invalid.test }
     test { anyattribute_processcontents_lax.test }
     test { anyattribute_processcontents_skip.test }
     test { anyattribute_processcontents_strict.test }
     test { any_processcontents_invalid.test }
     test { any_processcontents_lax.test }
     test { any_processcontents_skip.test }
     test { any_processcontents_strict.test }
     @end
  */
  if (!node->hasAttribute(ATTR_PROCESS_CONTENTS)) {
    w->process_contents = WILDCARD_PROCESS_CONTENTS_STRICT;
  }
  else {
    String processContents = node->getAttribute(ATTR_PROCESS_CONTENTS);
    int invalid = 0;
    if ("strict" == processContents)
      w->process_contents = WILDCARD_PROCESS_CONTENTS_STRICT;
    else if ("lax" == processContents)
      w->process_contents = WILDCARD_PROCESS_CONTENTS_LAX;
    else if ("skip" == processContents)
      w->process_contents = WILDCARD_PROCESS_CONTENTS_SKIP;
    else
      invalid = 1;

    if (invalid)
      return invalid_attribute_val(&s->ei,s->m_uri,node,ATTR_PROCESS_CONTENTS);
  }

  Node *child;
  if (NULL != (child = xs_first_non_annotation_child2(node)))
    return invalid_element(&s->ei,s->m_uri,child);

  return 0;
}

int GridXSLT::xs_parse_any(Schema *s, Node *node, list **particles_list)
{
  int max_occurs_val = 1;
  int min_occurs_val = 1;
  Particle *p;

  if ((0 == min_occurs_val) && (0 == max_occurs_val))
    return 0; /* do not create a schema component */

  CHECK_CALL(xs_parse_max_occurs(s,node,&max_occurs_val))
  CHECK_CALL(parse_optional_int_attr(&s->ei,s->m_uri,node,ATTR_MIN_OCCURS,&min_occurs_val,1))

  p = new Particle(s->as);
  list_append(particles_list,p);
  p->term_type = PARTICLE_TERM_WILDCARD;
  p->range.max_occurs = max_occurs_val;
  p->range.min_occurs = min_occurs_val;
  p->defline = node->m_line;

  CHECK_CALL(xs_parse_wildcard(s,node,&p->term.w))

  return 0;
}

int xs_parse_import(Schema *s, Node *node)
{
/*   stringbuf *src = stringbuf_new(); */
  Schema *import_schema = NULL;
  String full_uri;
  if (!node->hasAttribute(ATTR_SCHEMA_LOCATION))
    return error(&s->ei,s->m_uri,node->m_line,String::null(),
                 "No schemaLocation specified; don't know what to import here");

  /* FIXME: test with bad and nonexistant uris, like with xsl:import */

  /* @implements(xmlschema-1:src-import.1) @end */

  /* @implements(xmlschema-1:src-import.1.1)
     test { xmlschema/import/nsconflict1.test }
     @end */

  String namesp = node->getAttribute(ATTR_NAMESPACE);
  if (!namesp.isNull() && !s->m_targetNamespace.isNull() && (namesp == s->m_targetNamespace))
    return error(&s->ei,s->m_uri,node->m_line,"src-import.1.1",
                 "The \"namespace\" attribute for this import must either be absent, or "
                 "different to the target namespace of the enclosing schema");

  /* @implements(xmlschema-1:src-import.1.2)
     test { xmlschema/import/nsconflict2.test }
     @end */
  if (namesp.isNull() && s->m_targetNamespace.isNull())
    return error(&s->ei,s->m_uri,node->m_line,"src-import.1.1",
                 "A \"namespace\" attribute is required on "
                 "this import element since the enclosing schema has no target namespace defined");

  String schemaloc = node->getAttribute(ATTR_SCHEMA_LOCATION).collapseWhitespace();

  full_uri = buildURI(schemaloc,s->m_uri);
  if (full_uri.isNull())
    return error(&s->ei,s->m_uri,node->m_line,String::null(),
                 "\"%*\" is not a valid relative or absolute URI",&schemaloc);

  Node *importNode = NULL;
  Node *importRoot = NULL;
  if (0 != retrieve_uri_element(&s->ei,s->m_uri,node->m_line,String::null(),full_uri,
                                &importNode,&importRoot,s->m_uri)) {
    /* FIXME: not sure if we're really supposed to signal an error here... see note
       in import2.test */
    return -1;
  }

  /* @implements(xmlschema-1:src-import.2)
     test { xmlschema/import/id.test }
     test { xmlschema/import/idnonexistant.test }
     test { xmlschema/import/idnotschema.test }
     test { xmlschema/import/nonexistant.test }
     test { xmlschema/import/notschema.test }
     @end
     @implements(xmlschema-1:src-import.2.1) @end
     @implements(xmlschema-1:src-import.2.2) @end */

  if (importNode->m_ident != ELEM_SCHEMA) {
    char *justdoc = full_uri.cstring();
    char *hash;
    if (NULL != (hash = strchr(justdoc,'#')))
      *hash = '\0';
    error(&s->ei,justdoc,importNode->m_line,String::null(),"Expected element {%*}%s",
          &XS_NAMESPACE,"schema");
    free(justdoc);
    return -1;
  }

  if (0 != parse_xmlschema_element(importNode,full_uri,schemaloc.cstring(),
                                   &import_schema,&s->ei,s->globals)) {
    return -1;
  }
  delete importRoot;

  list_append(&s->imports,import_schema);


  /* @implements(xmlschema-1:src-import.3) @end
     @implements(xmlschema-1:src-import.3.1)
     test { xmlschema/import/nsmismatch1.test }
     test { xmlschema/import/nsmismatch3.test }
     @end
     @implements(xmlschema-1:src-import.3.2)
     test { xmlschema/import/nsmismatch2.test }
     @end */
  if (import_schema->m_targetNamespace.isNull()) {
    if (!namesp.isNull()) {
      return error(&s->ei,s->m_uri,node->m_line,"src-import.3.1",
                   "This import element must not have a \"namespace\" attribute, since the "
                   "imported schema does not define a target namespace");
    }
  }
  else {
    if (namesp.isNull() || (namesp != import_schema->m_targetNamespace)) {
      return error(&s->ei,s->m_uri,node->m_line,"src-import.3.1","This import element must its "
                   "\"namespace\" attribute set to \"%*\", which is the target namespace declared "
                   "by the imported schema",&import_schema->m_targetNamespace);
    }
  }
  return 0;
}

int GridXSLT::xs_parse_schema(Schema *s, Node *node)
{
  /* 
     @implements(xmlschema-1:Particle{min occurs}) @end
     @implements(xmlschema-1:Particle{max occurs}) @end
     @implements(xmlschema-1:Particle{term}) @end

     FIXME: check that _everywhere_ we parse an attribute, we respect the whitespace facet defined
     on that attribute's type (i.e. whether to allow leading/trailing whitespace)
  */

  /* FIXME: parse the other attributes */
  s->m_targetNamespace = node->getAttribute(ATTR_TARGET_NAMESPACE);

  /* parse the "attributeFormDefault" attribute */
  String afd = node->getAttribute(ATTR_ATTRIBUTE_FORM_DEFAULT).collapseWhitespace();
  if (!afd.isNull()) {
    int invalid = 0;
    if ("qualified" == afd)
      s->attrformq = 1;
    else if ("unqualified" == afd)
      s->attrformq = 0;
    else
      invalid = 1;
    if (invalid)
      return invalid_attribute_val(&s->ei,s->m_uri,node,ATTR_ATTRIBUTE_FORM_DEFAULT);
  }

  /* parse the "elementFormDefault" attribute */
  String efd = node->getAttribute(ATTR_ELEMENT_FORM_DEFAULT);
  if (!efd.isNull()) {
    int invalid = 0;
    if ("qualified" == efd)
      s->elemformq = 1;
    else if ("unqualified" == efd)
      s->elemformq = 0;
    else
      invalid = 1;
    if (invalid)
      return invalid_attribute_val(&s->ei,s->m_uri,node,ATTR_ELEMENT_FORM_DEFAULT);
  }

  s->block_default = node->getAttribute(ATTR_BLOCK_DEFAULT);
  s->final_default = node->getAttribute(ATTR_FINAL_DEFAULT);

  Node *child = node->firstChild();
  xs_skip_others2(&child);

  /* FIXME: check if this properly enforces the ordering defined in the spec */

  /* first section */
  while (child) {
    if (child->m_ident == ELEM_INCLUDE) {
    }
    else if (child->m_ident == ELEM_IMPORT) {
      CHECK_CALL(xs_parse_import(s,child))
    }
    else if (child->m_ident == ELEM_REDEFINE) {
    }
    else if (child->m_ident == ELEM_ANNOTATION) {
    }
    else {
      break;
    }

    xs_next_element2(&child);
  }

  /* second section */
  while (child) {
    if (child->m_ident == ELEM_SIMPLE_TYPE) {
      CHECK_CALL(xs_parse_simple_type(s,child,s->m_targetNamespace,1,NULL))
    }
    else if (child->m_ident == ELEM_COMPLEX_TYPE) {
      CHECK_CALL(xs_parse_complex_type(s,child,s->m_targetNamespace,1,NULL))
    }
    else if (child->m_ident == ELEM_GROUP) {
      CHECK_CALL(xs_parse_group_def(child,s->m_targetNamespace,s))
    }
    else if (child->m_ident == ELEM_ATTRIBUTE_GROUP) {
      CHECK_CALL(xs_parse_attribute_group_def(s,child,s->m_targetNamespace))
    }
    else if (child->m_ident == ELEM_ELEMENT) {
      CHECK_CALL(xs_parse_element(child,s->m_targetNamespace,s,NULL))
    }
    else if (child->m_ident == ELEM_ATTRIBUTE) {
      CHECK_CALL(xs_parse_attribute(s,child,s->m_targetNamespace,1,NULL))
    }
    else if (child->m_ident == ELEM_NOTATION) {
    }
    else if (child->m_ident == ELEM_ANNOTATION) {
    }
    else {
      return invalid_element(&s->ei,s->m_uri,child);
    }

    xs_next_element2(&child);
  }

  return 0;
}
