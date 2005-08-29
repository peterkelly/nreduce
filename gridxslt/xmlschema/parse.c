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

#include "parse.h"
#include "xmlschema.h"
#include "util/namespace.h"
#include "util/stringbuf.h"
#include "util/debug.h"
#include "util/macros.h"
#include <libxml/xmlwriter.h>
#include <libxml/xmlIO.h>
#include <libxml/parser.h>
#include <libxml/tree.h>
#include <libxml/uri.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <ctype.h>

int xs_check_attribute_type(xs_schema *s, xs_reference *r)
{
  xs_attribute *a = (xs_attribute*)r->source;
  assert(a->type);
  if (a->type->complex)
    return error(&s->ei,s->uri,r->def.loc.line,NULL,"Attributes can only use simple types");
  return 0;
}

void xs_skip_others(xmlNodePtr *c)
{
  while ((*c) && (XML_ELEMENT_NODE != (*c)->type))
    (*c) = (*c)->next;
}

void xs_next_element(xmlNodePtr *c)
{
  *c = (*c)->next;
  xs_skip_others(c);
}

void xs_skip_annotation(xmlNodePtr *c)
{
  if (*c && check_element(*c,"annotation",XS_NAMESPACE))
    xs_next_element(c);
}

xmlNodePtr xs_first_non_annotation_child(xmlNodePtr n)
{
  xmlNodePtr c =  n->children;
  xs_skip_others(&c);
  xs_skip_annotation(&c);
  return c;
}

int xs_parse_max_occurs(xs_schema *s, xmlNodePtr n, int *val)
{
  /* FIXME: what to do if minOccurs > maxOccurs? should we set maxOccurs to the higher value,
     or signal an error? */
  char *str;
  if ((NULL != (str = xmlGetProp(n,"maxOccurs"))) && !strcmp(str,"unbounded")) {
    *val = -1;
    free(str);
    return 0;
  }
  free(str);
  return parse_optional_int_attr(&s->ei,s->uri,n,"maxOccurs",val,1);
}

int xs_check_sgheadref(xs_schema *s, xs_reference *r)
{
  xs_element *source = (xs_element*)r->source;
  xs_element *head = (xs_element*)r->target;
/*   debugl("resolved sghead for %s: %s\n",source->name,head->name); */
/*   debugl("head->exclude_extension = %d\n",head->exclude_extension); */
/*   debugl("head->exclude_restriction = %d\n",head->exclude_restriction); */
  list_push(&head->sgmembers,source);
  return 0;
}

int xs_check_forbidden_attribute(xs_schema *s, xmlNodePtr n, char *attrname)
{
  if (xmlHasProp(n,attrname))
    return attribute_not_allowed(&s->ei,s->uri,n->line,attrname);
  return 0;
}

int xs_init_toplevel_object(xs_schema *s, xmlNodePtr n, xmlDocPtr doc, char *ns,
                            symbol_space *ss, void *obj, char **obj_name,
                            char **obj_ns, char *typestr)
{
  char *name;
  if (!xmlHasProp(n,"name"))
    return missing_attribute2(&s->ei,s->uri,n->line,NULL,"name");
  name = get_wscollapsed_attr(n,"name",NULL);

  if (NULL != ss_lookup_local(ss,nsname_temp(ns,name))) {
    error(&s->ei,s->uri,n->line,NULL,"%s \"%s\" already declared",typestr,name);
    free(name);
    return -1;
  }
  ss_add(ss,nsname_temp(ns,name),obj);

  *obj_name = name;
  if (ns)
    *obj_ns = strdup(ns);

  return 0;
}

int xs_check_conflicting_attributes(xs_schema *s, xmlNodePtr n, const char *attrname1,
                                    const char *attrname2, const char *errname)
{
  if (xmlHasProp(n,attrname1) && xmlHasProp(n,attrname2))
    return conflicting_attributes(&s->ei,s->uri,n->line,errname,attrname1,attrname2);
  return 0;
}

int xs_parse_value_constraint(xs_schema *s, xmlNodePtr n, xmlDocPtr doc, xs_value_constraint *vc,
                              const char *errname)
{
  vc->type = XS_VALUE_CONSTRAINT_NONE;
  CHECK_CALL(xs_check_conflicting_attributes(s,n,"default","fixed",errname))
  if (xmlHasProp(n,"default")) {
    vc->value = xmlGetProp(n,"default");
    vc->type = XS_VALUE_CONSTRAINT_DEFAULT;
  }
  else if (xmlHasProp(n,"fixed")) {
    vc->value = xmlGetProp(n,"fixed");
    vc->type = XS_VALUE_CONSTRAINT_FIXED;
  }
  return 0;
}

int xs_parse_ref(xs_schema *s, xmlNodePtr n, xmlDocPtr doc, char *attrname, int type,
                 void **obj, xs_reference **refptr)
{
  char *namestr;
  char *name = NULL;
  char *ns = NULL;
  xs_reference *r;
  qname qn;

  *refptr = NULL;

  if (!xmlHasProp(n,attrname))
    return 0;

  namestr = get_wscollapsed_attr(n,attrname,NULL);
  qn = qname_parse(namestr);
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
  if (0 != get_ns_name_from_qname(n,doc,namestr,&ns,&name)) {
    error(&s->ei,s->uri,n->line,"src-qname.1.1",
          "Could not resolve namespace for prefix \"%s\"",qn.prefix);
    qname_free(qn);
    free(namestr);
    return -1;
  }

  if (NULL != ns)
    ns_add_preferred(s->globals->namespaces,ns,qn.prefix);
  qname_free(qn);
  free(namestr);

  *refptr = r = xs_reference_new(s->as);
  r->s = s;
  r->def.ident = nsname_new(ns,name);
  r->def.loc.line = n->line;
  r->type = type;
  r->obj = obj;

  free(ns);
  free(name);
  return 0;
}

int xs_parse_form(xs_schema *s, xmlNodePtr n, int *qualified)
{
  char *form;
  int invalid = 0;
  if (NULL == (form = get_wscollapsed_attr(n,"form",NULL)))
    return 0;

  if (!strcmp(form,"qualified"))
    *qualified = 1;
  else if (!strcmp(form,"unqualified"))
    *qualified = 0;
  else
    invalid = 1;
  free(form);
  if (invalid)
    return invalid_attribute_val(&s->ei,s->uri,n,"form");
  else
    return 0;
}

int xs_parse_block_final(const char *str, int *extension, int *restriction,
                         int *substitution, int *list, int *union1)
{
  char *val = strdup(str);
  char *start = val;
  char *cur = val;

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

  while (1) {
    if ('\0' == *cur || isspace(*cur)) {
      int endc = *cur;
      *cur = '\0';
      if (cur != start) {
        if (!strcmp(start,"extension") && (NULL != extension))
          *extension = 1;
        else if (!strcmp(start,"restriction") && (NULL != restriction))
          *restriction = 1;
        else if (!strcmp(start,"substitution") && (NULL != substitution))
          *substitution = 1;
        else if (!strcmp(start,"list") && (NULL != list))
          *list = 1;
        else if (!strcmp(start,"union") && (NULL != union1))
          *union1 = 1;
        else if (!strcmp(start,"#all")) {
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
      *cur = endc;
      start = cur+1;
    }
    if ('\0' == *cur)
      break;
    cur++;
  }

  free(val);

  return 0;
}

int xs_parse_block_final_attr(xs_schema *s, xmlNodePtr n, const char *attrname,
                              const char *defaultval, int *extension, int *restriction,
                              int *substitution, int *list, int *union1)
{
  char *val;
  int r;

  if (xmlHasProp(n,attrname))
    val = xmlGetProp(n,attrname);
  else if (NULL != defaultval)
    val = strdup(defaultval);
  else
    val = strdup("");

  r = xs_parse_block_final(val,extension,restriction,substitution,list,union1);
  free(val);
  return r;
}

/* possible parents: <schema> <complexType> <group> */

int xs_parse_element(xmlNodePtr n, xmlDocPtr doc, char *ns, xs_schema *s, list **particles_list)
{
  xs_element *e;
  xmlNodePtr c;

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
    e = xs_element_new(s->as);
    e->def.loc.line = n->line;
    CHECK_CALL(xs_init_toplevel_object(s,n,doc,ns,s->symt->ss_elements,e,
               &e->def.ident.name,&e->def.ident.ns,"element"))
    CHECK_CALL(xs_check_forbidden_attribute(s,n,"ref"))
    CHECK_CALL(xs_check_forbidden_attribute(s,n,"form"))
    CHECK_CALL(xs_check_forbidden_attribute(s,n,"minOccurs"))
    CHECK_CALL(xs_check_forbidden_attribute(s,n,"maxOccurs"))
    /* @implements(xmlschema-1:Element Declaration{substitution group affiliation}) @end */
    CHECK_CALL(xs_parse_ref(s,n,doc,"substitutionGroup",XS_OBJECT_ELEMENT,
                            (void**)&e->sghead,&e->sgheadref))
    if (e->sgheadref) {
      e->sgheadref->check = xs_check_sgheadref;
      e->sgheadref->source = e;
    }

    /* @implements(xmlschema-1:Element Declaration{substitution group exclusions}) @end */
    CHECK_CALL(xs_parse_block_final_attr(s,n,"final",s->final_default,
               &e->exclude_extension,&e->exclude_restriction,NULL,NULL,NULL));

    e->toplevel = 1;
  }
  else {
    int max_occurs_val = 1;
    int min_occurs_val = 1;

    CHECK_CALL(xs_check_forbidden_attribute(s,n,"substitutionGroup"))
    CHECK_CALL(xs_check_forbidden_attribute(s,n,"final"))
    CHECK_CALL(xs_parse_max_occurs(s,n,&max_occurs_val))
    CHECK_CALL(parse_optional_int_attr(&s->ei,s->uri,n,"minOccurs",&min_occurs_val,1))

    /* FIXME: if minOccurs=maxOccurs=0 we should not create anything */

    /* case 2: has ancestor of <complexType> or <group> and #ref is absent */
    if (!xmlHasProp(n,"ref")) {
      xs_particle *p;
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
      CHECK_CALL(xs_parse_form(s,n,&qualified))

      /* @implements(xmlschema-1:src-element.2) @end */
      /* @implements(xmlschema-1:src-element.2.1)
         test { element_noname.test }
         @end */
      if (!xmlHasProp(n,"name"))
        return missing_attribute2(&s->ei,s->uri,n->line,"src-element.2.1","name");

      e = xs_element_new(s->as);
      e->def.loc.line = n->line;
      p = xs_particle_new(s->as);
      p->range.min_occurs = min_occurs_val;
      p->range.max_occurs = max_occurs_val;
      p->term.e = e;
      p->term_type = XS_PARTICLE_TERM_ELEMENT;
      p->defline = e->def.loc.line;
      list_append(particles_list,p);

      /* FIXME: testcases for form */
      e->def.ident.name = xmlGetProp(n,"name");
      if (ns && qualified)
        e->def.ident.ns = strdup(ns);
    }

    /* case 3: has ancestor of <complexType> or <group> and #ref is present */
    else {
      xmlNodePtr c;
      xs_particle *p = xs_particle_new(s->as);
      p->range.min_occurs = min_occurs_val;
      p->range.max_occurs = max_occurs_val;
      p->term.e = NULL; /* will be resolved later */
      p->term_type = XS_PARTICLE_TERM_ELEMENT;
      p->defline = n->line;
      list_append(particles_list,p);

      CHECK_CALL(xs_parse_ref(s,n,doc,"ref",XS_OBJECT_ELEMENT,(void**)&p->term.e,&p->ref))

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
      CHECK_CALL(xs_check_conflicting_attributes(s,n,"name","ref","src-element.2.1"))
      CHECK_CALL(xs_check_conflicting_attributes(s,n,"nillable","ref","src-element.2.2"))
      CHECK_CALL(xs_check_conflicting_attributes(s,n,"default","ref","src-element.2.2"))
      CHECK_CALL(xs_check_conflicting_attributes(s,n,"fixed","ref","src-element.2.2"))
      CHECK_CALL(xs_check_conflicting_attributes(s,n,"form","ref","src-element.2.2"))
      CHECK_CALL(xs_check_conflicting_attributes(s,n,"block","ref","src-element.2.2"))
      CHECK_CALL(xs_check_conflicting_attributes(s,n,"type","ref","src-element.2.2"))

      if (NULL != (c = xs_first_non_annotation_child(n)))
        return invalid_element2(&s->ei,s->uri,c);

      /* Note that we have not created the actual element object yet... this is done later, in
         xs_resolve_ref(), since the element we are referencing may not have been parsed
         yet. */

      /* FIXME: should we check that the node does not contain any children (e.g. complexType)? */

      return 0;
    }
  }

  /* common properties - this part is only done for cases 1 and 2 (i.e. not for refs) */

  /* @implements(xmlschema-1:Element Declaration{disallowed substitutions}) @end */
  CHECK_CALL(xs_parse_block_final_attr(s,n,"block",s->block_default,
             &e->disallow_extension,&e->disallow_restriction,&e->disallow_substitution,
             NULL,NULL));

  /* @implements(xmlschema-1:src-element.1)
     description { default and fixed must not both be present }
     test { element_default_fixed.test }
     test { element_local_default_fixed.test }
     @end
     @implements(xmlschema-1:Element Declaration{value constraint}) @end
     */
  CHECK_CALL(xs_parse_value_constraint(s,n,doc,&e->vc,"src-element.1"))

  /* @implements(xmlschema-1:Element Declaration{nillable})
     test { element_local_nillable.test }
     test { element_local_nillable2.test }
     test { element_tl_nillable.test }
     test { element_tl_nillable2.test }
     test { element_tl_nillable3.test }
     test { element_tl_nillable4.test }
     test { element_tl_nillable5.test }
     @end */
  CHECK_CALL(parse_optional_boolean_attr(&s->ei,s->uri,n,"nillable",&e->nillable,0))

  /* @implements(xmlschema-1:Element Declaration{abstract})
     test { element_local_abstract.test }
     test { element_local_abstract2.test }
     test { element_tl_abstract.test }
     test { element_tl_abstract2.test }
     @end */
  CHECK_CALL(parse_optional_boolean_attr(&s->ei,s->uri,n,"abstract",&e->abstract,0))

  e->type = NULL; /* FIXME */
  e->identity_constraint_definitions = NULL; /* FIXME */

  c = xs_first_non_annotation_child(n);

  if (c && (check_element(c,"simpleType",XS_NAMESPACE) ||
            check_element(c,"complexType",XS_NAMESPACE))) {

    /* @implements(xmlschema-1:src-element.3)
       test { element_type_complextype.test }
       test { element_type_simpletype.test }
       @end */
    if (xmlHasProp(n,"type"))
      return error(&s->ei,s->uri,n->line,"src-element.3","\"type\" attribute must not be specified "
                   "if a <simpleType> or <complexType> child is present");

    if (check_element(c,"simpleType",XS_NAMESPACE)) {
      CHECK_CALL(xs_parse_simple_type(s,c,doc,ns,0,&e->type))
      xs_next_element(&c);
    }
    else { /* complexType */
      CHECK_CALL(xs_parse_complex_type(s,c,doc,ns,0,&e->type,e->def.ident.name,e->def.ident.ns))
      assert(e->type);
      xs_next_element(&c);
    }
  }
  else if (xmlHasProp(n,"type")) {
    CHECK_CALL(xs_parse_ref(s,n,doc,"type",XS_OBJECT_TYPE,(void**)&e->type,&e->typeref))

  }

  while (c) {
    if (check_element(c,"unique",XS_NAMESPACE)) {
      /* FIXME */
    }
    else if (check_element(c,"key",XS_NAMESPACE)) {
      /* FIXME */
    }
    else if (check_element(c,"keyref",XS_NAMESPACE)) {
      /* FIXME */
    }
    else  {
      return invalid_element2(&s->ei,s->uri,c);
    }

    xs_next_element(&c);
  }


  return 0;
}

int xs_parse_attribute_use(xs_schema *s, xmlNodePtr n, int *use)
{
  char *str;
  int invalid = 0;

  *use = XS_ATTRIBUTE_USE_OPTIONAL;

  if (NULL == (str = xmlGetProp(n,"use")))
    return 0;

  if (!strcmp(str,"optional"))
    *use = XS_ATTRIBUTE_USE_OPTIONAL;
  else if (!strcmp(str,"prohibited"))
    *use = XS_ATTRIBUTE_USE_PROHIBITED;
  else if (!strcmp(str,"required"))
    *use = XS_ATTRIBUTE_USE_REQUIRED;
  else
    invalid = 1;
  free(str);
  if (invalid)
    return invalid_attribute_val(&s->ei,s->uri,n,"use");
  return 0;
}

int xs_parse_attribute(xs_schema *s, xmlNodePtr n, xmlDocPtr doc, char *ns,
                       int toplevel, list **aulist)
{
  xs_attribute_use *au = NULL;
  xs_attribute *a = NULL;
  xmlNodePtr c;
  int use = XS_ATTRIBUTE_USE_OPTIONAL;
  xs_value_constraint *vcptr;

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
    a = xs_attribute_new(s->as);
    a->def.loc.line = n->line;
    CHECK_CALL(xs_init_toplevel_object(s,n,doc,ns,s->symt->ss_attributes,a,
               &a->def.ident.name,&a->def.ident.ns,"attribute"))
    CHECK_CALL(xs_check_forbidden_attribute(s,n,"ref"))
    CHECK_CALL(xs_check_forbidden_attribute(s,n,"form"))
    CHECK_CALL(xs_check_forbidden_attribute(s,n,"use"))

    vcptr = &a->vc;
    a->toplevel = 1;
  }
  else if (xmlHasProp(n,"ref")) {
    /* local attribute reference */
    CHECK_CALL(xs_parse_attribute_use(s,n,&use))

    /* @implements(xmlschema-1:src-attribute.3.2)
       test { attribute_local_ref_name.test }
       test { attribute_local_ref_form.test }
       test { attribute_local_ref_type.test }
       @end */
    CHECK_CALL(xs_check_conflicting_attributes(s,n,"name","ref","src-attribute.3.2"))
    CHECK_CALL(xs_check_conflicting_attributes(s,n,"form","ref","src-attribute.3.2"))
    CHECK_CALL(xs_check_conflicting_attributes(s,n,"type","ref","src-attribute.3.2"))

    au = xs_attribute_use_new(s->as);
    au->defline = n->line;
    au->required = (XS_ATTRIBUTE_USE_REQUIRED == use);
    au->prohibited = (XS_ATTRIBUTE_USE_PROHIBITED == use);
    CHECK_CALL(xs_parse_ref(s,n,doc,"ref",XS_OBJECT_ATTRIBUTE,(void**)&au->attribute,&au->ref))
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
    CHECK_CALL(xs_parse_form(s,n,&qualified))

    /* local attribute declaration */
    CHECK_CALL(xs_parse_attribute_use(s,n,&use))

    a = xs_attribute_new(s->as);
    a->def.loc.line = n->line;
    au = xs_attribute_use_new(s->as);
    au->defline = n->line;

    /* @implements(xmlschema-1:src-attribute.3.1)
       test { attribute_local_noname.test }
       @end */
    if (!xmlHasProp(n,"name"))
      return missing_attribute2(&s->ei,s->uri,n->line,"src-attribute.3.1","name");
    a->def.ident.name = xmlGetProp(n,"name");
    if (ns && qualified)
      a->def.ident.ns = strdup(ns);
    au->required = (XS_ATTRIBUTE_USE_REQUIRED == use);
    au->prohibited = (XS_ATTRIBUTE_USE_PROHIBITED == use);
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
  if (a && !strcmp(a->def.ident.name,"xmlns"))
    return error(&s->ei,s->uri,n->line,"no-xmlns","\"xmlns\" cannot be used as an attribute name");

  if (au && a) {
    a->vc.type = au->vc.type;
    a->vc.value = au->vc.value ? strdup(au->vc.value) : NULL;
  }

  /* @implements(xmlschema-1:no-xsi)
     test { attribute_tl_xsi.test }
     test { attribute_local_xsi.test }
     @end */
  if (a && a->def.ident.ns && !strcmp(a->def.ident.ns,XSI_NAMESPACE))
    return error(&s->ei,s->uri,n->line,"no-xsi",
                 "attributes cannot be declared with a target namespace of \"%s\"",XSI_NAMESPACE);
  /* FIXME: need to add the 4 built-in types in section 3.2.7 by default */

  /* @implements(xmlschema-1:src-attribute.1)
     test { attribute_tl_default_fixed.test }
     test { attribute_local_default_fixed.test }
     test { attribute_local_ref_default_fixed.test }
     @end
     @implements(xmlschema-1:Attribute Declaration{value constraint}) @end
     @implements(xmlschema-1:Attribute Use{value constraint}) @end */
  CHECK_CALL(xs_parse_value_constraint(s,n,doc,vcptr,"src-attribute.1"))

  /* @implements(xmlschema-1:src-attribute.2)
     description { if default present, use must be optional }
     test { attribute_local_default_use1.test }
     test { attribute_local_default_use2.test }
     test { attribute_local_default_use3.test }
     test { attribute_local_ref_default_use1.test }
     test { attribute_local_ref_default_use2.test }
     test { attribute_local_ref_default_use3.test }
     @end */
  if (xmlHasProp(n,"default") && (XS_ATTRIBUTE_USE_OPTIONAL != use))
    return error(&s->ei,s->uri,n->line,"src-attribute.2",
                 "\"use\" must be \"optional\" when default value specified");

  if (!xmlHasProp(n,"ref")) {
    CHECK_CALL(xs_parse_ref(s,n,doc,"type",XS_OBJECT_TYPE,(void**)&a->type,&a->typeref))
    if (a->typeref) {
      a->typeref->check = xs_check_attribute_type;
      a->typeref->source = a;
    }
  }

  if (NULL != (c = xs_first_non_annotation_child(n))) {
    if (!check_element(c,"simpleType",XS_NAMESPACE))
      return invalid_element2(&s->ei,s->uri,c);

    /* @implements(xmlschema-1:src-attribute.3) @end
       @implements(xmlschema-1:src-attribute.3.1) test { attribute_local_ref_simpletype.test } @end */
    if (xmlHasProp(n,"ref"))
      return error(&s->ei,s->uri,c->line,"src-attribute.3.1",
                   "<simpleType> not allowed here when \"ref\" is set on <attribute>");

    /* @implements(xmlschema-1:src-attribute.4) test { attribute_tl_type_simple_ref.test} @end */
    if (NULL != a->typeref)
      return error(&s->ei,s->uri,c->line,"src-attribute.4",
                   "<simpleType> not allowed here when \"type\" is set on <attribute>");
    CHECK_CALL(xs_parse_simple_type(s,c,doc,ns,0,&a->type))
    assert(NULL != a->type);
  }

  if (!xmlHasProp(n,"ref") && (NULL == a->type) && (NULL == a->typeref))
    a->type = s->globals->simple_ur_type;

  return 0;
}

int xs_parse_attribute_group_def(xs_schema *s, xmlNodePtr n, xmlDocPtr doc, char *ns)
{
  xmlNodePtr c;
  xs_attribute_group *ag = xs_attribute_group_new(s->as);

  /* @implements(xmlschema-1:src-attribute_group.1) @end
     @implements(xmlschema-1:ag-props-correct.1) @end
     @implements(xmlschema-1:Attribute Group Definition{name}) @end
     @implements(xmlschema-1:Attribute Group Definition{target namespace}) @end
     @implements(xmlschema-1:Attribute Group Definition{attribute uses}) @end
     @implements(xmlschema-1:Attribute Group Definition{attribute wildcard}) @end
     @implements(xmlschema-1:Schema{attribute group definitions}) @end
   */

  CHECK_CALL(xs_init_toplevel_object(s,n,doc,ns,s->symt->ss_attribute_groups,ag,
                                     &ag->def.ident.name,&ag->def.ident.ns,"attribute group"))
  ag->def.loc.line = n->line;
  CHECK_CALL(xs_check_forbidden_attribute(s,n,"ref"))

  for (c = xs_first_non_annotation_child(n); c; xs_next_element(&c)) {
    /* @implements(xmlschema-1:Complex Type Definition{attribute uses}) @end */
    if (check_element(c,"attribute",XS_NAMESPACE))
      CHECK_CALL(xs_parse_attribute(s,c,doc,ns,0,&ag->local_attribute_uses))
    else if (check_element(c,"attributeGroup",XS_NAMESPACE))
      CHECK_CALL(xs_parse_attribute_group_ref(s,c,doc,ns,&ag->attribute_group_refs))
    else
      break;
  }

  if (c && check_element(c,"anyAttribute",XS_NAMESPACE)) {
    /* FIXME: need to do all the magic here as described in the spec - the same as for
       <anyAttribute> inside a complex type */
    CHECK_CALL(xs_parse_wildcard(s,c,doc,&ag->local_wildcard))
    xs_next_element(&c);
  }

  if (c)
    return invalid_element2(&s->ei,s->uri,c);

  return 0;
}

int xs_parse_attribute_group_ref(xs_schema *s, xmlNodePtr n, xmlDocPtr doc, char *ns,
                                 list **reflist)
{
  xs_attribute_group_ref *agr = xs_attribute_group_ref_new(s->as);
  CHECK_CALL(xs_check_forbidden_attribute(s,n,"name"))

  if (!xmlHasProp(n,"ref"))
    return missing_attribute2(&s->ei,s->uri,n->line,NULL,"ref");
  CHECK_CALL(xs_parse_ref(s,n,doc,"ref",XS_OBJECT_ATTRIBUTE_GROUP,(void**)&agr->ag,&agr->ref))
  assert(agr->ref);


  list_append(reflist,agr);

  return 0;
}

int xs_facet_num(const char *str)
{
  int i;
  for (i = 0; i < XS_FACET_NUMFACETS; i++)
    if (!strcmp(xs_facet_names[i],str))
      return i;
  assert(0);
}

int xs_parse_facet(xs_schema *s, xmlNodePtr n, xs_facetdata *fd)
{
  int facet = xs_facet_num(n->name);

  /* @implements(xmlschema-1:Simple Type Definition{facets})
     test { simpletype_atomic_facets.test }
     test { simpletype_list_facets.test }
     test { simpletype_list_facet.test }
     test { simpletype_union_facets.test }
     @end */
  if (!xmlHasProp(n,"value"))
    return missing_attribute2(&s->ei,s->uri,n->line,NULL,"value");

  if (XS_FACET_PATTERN == facet) {
    list_append(&fd->patterns,xmlGetProp(n,"value"));
  }
  else if (XS_FACET_ENUMERATION == facet) {
    list_append(&fd->enumerations,xmlGetProp(n,"value"));
  }
  else {
    if (NULL != fd->strval[facet])
      return error(&s->ei,s->uri,n->line,NULL,"facet already defined");
    fd->strval[facet] = get_wscollapsed_attr(n,"value",NULL);
    fd->defline[facet] = n->line;

    if ((XS_FACET_MINLENGTH == facet) ||
        (XS_FACET_MAXLENGTH == facet) ||
        (XS_FACET_LENGTH == facet) ||
        (XS_FACET_TOTALDIGITS == facet) ||
        (XS_FACET_FRACTIONDIGITS == facet)) {
      if (0 != convert_to_nonneg_int(fd->strval[facet],&fd->intval[facet]))
        return error(&s->ei,s->uri,n->line,NULL,
                     "Invalid value for facet %s: must be a non-negative integer",n->name);
    }
    else if (XS_FACET_TOTALDIGITS == facet) {
      if ((0 != convert_to_nonneg_int(fd->strval[facet],&fd->intval[facet])) ||
          (0 == fd->intval[facet]))
        return error(&s->ei,s->uri,n->line,NULL,
                     "Invalid value for facet %s: must be a positive integer",n->name);
    }

  }

  return 0;
}

int xs_is_builtin_type_redeclaration(xs_schema *s, char *ns, xmlNodePtr n)
{
  if ((NULL != ns) && !strcmp(ns,XS_NAMESPACE) && xmlHasProp(n,"name")) {
    xs_type *existing;
    char *name = get_wscollapsed_attr(n,"name",NULL);
    if ((NULL != (existing = xs_symbol_table_lookup_object(s->globals->symt,XS_OBJECT_TYPE,
                                                           nsname_temp(ns,name)))) &&
        existing->builtin) {
      free(name);
      return 1;
    }
    free(name);
  }
  return 0;
}

int xs_parse_simple_type(xs_schema *s, xmlNodePtr n, xmlDocPtr doc, char *ns,
                         int toplevel, xs_type **tout)
{
  xs_type *t;
  xmlNodePtr c;

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
    if (xs_is_builtin_type_redeclaration(s,ns,n))
      return 0;

    t = xs_type_new(s->as);
    t->def.loc.line = n->line;
    CHECK_CALL(xs_init_toplevel_object(s,n,doc,ns,s->symt->ss_types,t,
               &t->def.ident.name,&t->def.ident.ns,"type"))

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
    CHECK_CALL(xs_parse_block_final_attr(s,n,"final",s->final_default,
               &t->final_extension,&t->final_restriction,NULL,
               &t->final_list,&t->final_union));
  }
  else {
    t = xs_type_new(s->as);
    t->def.loc.line = n->line;
    CHECK_CALL(xs_check_forbidden_attribute(s,n,"name"))
    CHECK_CALL(xs_check_forbidden_attribute(s,n,"final"))
  }

  t->variety = XS_TYPE_VARIETY_INVALID;
  if (tout)
    *tout = t;

  c = xs_first_non_annotation_child(n);

  if (!c)
    return error(&s->ei,s->uri,n->line,NULL,
                 "<simpleType> requires a <restriction>, <list>, or <union>");

  if (check_element(c,"restriction",XS_NAMESPACE)) {
    xmlNodePtr c2 = xs_first_non_annotation_child(c);

    t->stype = XS_TYPE_SIMPLE_RESTRICTION;
    CHECK_CALL(xs_parse_ref(s,c,doc,"base",XS_OBJECT_TYPE,(void**)&t->base,&t->baseref))

    /* @implements(xmlschema-1:src-simple-type.2)
       test { simpletype_restriction.test }
       test { simpletype_restriction_basechild.test }
       test { simpletype_restriction_list.test }
       test { simpletype_restriction_list2.test }
       test { simpletype_restriction_nobase.test }
       test { simpletype_restriction_restriction.test }
       test { simpletype_restriction_restriction2.test }
       @end */
    if (c2 && check_element(c2,"simpleType",XS_NAMESPACE)) {
      if (NULL != t->baseref)
        return error(&s->ei,s->uri,c2->line,"src-simple-type.2",
                     "<simpleType> not allowed here when \"base\" is set on <restriction>");
      CHECK_CALL(xs_parse_simple_type(s,c2,doc,ns,0,&t->base));
      t->variety = t->base->variety;
      xs_next_element(&c2);
    }

    if ((NULL == t->base) && (NULL == t->baseref))
      return error(&s->ei,s->uri,c->line,"structures-3.14.4","<restriction> must have either the "
                   "\"base\" attribute set, or a <simpleType> child");

    while (c2) {

      if (check_element(c2,"length",XS_NAMESPACE) ||
          check_element(c2,"minLength",XS_NAMESPACE) ||
          check_element(c2,"maxLength",XS_NAMESPACE) ||
          check_element(c2,"pattern",XS_NAMESPACE) ||
          check_element(c2,"enumeration",XS_NAMESPACE) ||
          check_element(c2,"whiteSpace",XS_NAMESPACE) ||
          check_element(c2,"maxInclusive",XS_NAMESPACE) ||
          check_element(c2,"maxExclusive",XS_NAMESPACE) ||
          check_element(c2,"minExclusive",XS_NAMESPACE) ||
          check_element(c2,"minInclusive",XS_NAMESPACE) ||
          check_element(c2,"totalDigits",XS_NAMESPACE) ||
          check_element(c2,"fractionDigits",XS_NAMESPACE)) {
        CHECK_CALL(xs_parse_facet(s,c2,&t->facets))
      }
      else {
        return invalid_element2(&s->ei,s->uri,c2);
      }

      xs_next_element(&c2);
    }

    t->restriction = 1; /* used to set variety when base type is resolved */

    xs_next_element(&c);
  }
  else if (check_element(c,"list",XS_NAMESPACE)) {
    xmlNodePtr c2 = xs_first_non_annotation_child(c);

    t->base = s->globals->simple_ur_type;

    CHECK_CALL(xs_parse_ref(s,c,doc,"itemType",XS_OBJECT_TYPE,
                            (void**)&t->item_type,&t->item_typeref))

    /* @implements(xmlschema-1:src-simple-type.3)
       test { simpletype_list_itemtypechild.test }
       test { simpletype_list_list.test }
       test { simpletype_list_noitemtype.test }
       test { simpletype_list_restriction2.test }
       test { simpletype_list_restriction.test }
       test { simpletype_list.test }
       @end */
    if (c2 && check_element(c2,"simpleType",XS_NAMESPACE)) {
      if (NULL != t->item_typeref)
        return error(&s->ei,s->uri,c2->line,"src-simple-type.3",
                     "<simpleType> not allowed here when \"itemType\" is set on <list>");
      CHECK_CALL(xs_parse_simple_type(s,c2,doc,ns,0,&t->item_type));
      xs_next_element(&c2);
    }

    if ((NULL == t->item_type) && (NULL == t->item_typeref))
      return error(&s->ei,s->uri,c->line,"src-simple-type.3","<list> must have "
                   "either the \"itemType\" attribute set, or a <simpleType> child");

    if (c2)
      return invalid_element2(&s->ei,s->uri,c2);

    t->variety = XS_TYPE_VARIETY_LIST;
    t->stype = XS_TYPE_SIMPLE_LIST;

    /* FIXME */
    xs_next_element(&c);
  }
  else if (check_element(c,"union",XS_NAMESPACE)) {
    xmlNodePtr c2 = xs_first_non_annotation_child(c);
    char *member_types = xmlGetProp(c,"memberTypes");

    t->base = s->globals->simple_ur_type;

    if (NULL != member_types) {
      char *start = member_types;
      char *cur = member_types;

      while (1) {
        if ('\0' == *cur || isspace(*cur)) {
          int endc = *cur;
          *cur = '\0';
          if (cur != start) {
            xs_member_type *mt;
            char *ref_name;
            char *ref_ns;

            if (0 != get_ns_name_from_qname(c,doc,start,&ref_ns,&ref_name)) {
              free(member_types);
              return -1;
            }

            /* FIXME!: make sure member types are handled property with namespaces */
/*             debugl("line %d: Parsing union member type %s",c->line,start); */
            mt = xs_member_type_new(s->as);
            list_append(&t->members,mt);
            mt->ref = xs_reference_new(s->as);
            mt->ref->s = s;
            mt->ref->def.ident = nsname_temp(ref_ns,ref_name);
            mt->ref->def.loc.line = c->line;
            mt->ref->type = XS_OBJECT_TYPE;
            mt->ref->obj = (void**)&mt->type;

          }
          *cur = endc;
          start = cur+1;
        }
        if ('\0' == *cur)
          break;
        cur++;
      }
      free(member_types);
    }

    while (c2) {
      xs_member_type *mt;
      if (!check_element(c2,"simpleType",XS_NAMESPACE))
        return invalid_element2(&s->ei,s->uri,c2);

      mt = xs_member_type_new(s->as);
      CHECK_CALL(xs_parse_simple_type(s,c2,doc,ns,0,&mt->type));
      assert(NULL != mt->type);
      list_append(&t->members,mt);

      xs_next_element(&c2);
    }

    if (NULL == t->members)
      return error(&s->ei,s->uri,c->line,NULL,"<union> requires one or more member types");

    t->variety = XS_TYPE_VARIETY_UNION;
    t->stype = XS_TYPE_SIMPLE_UNION;
    xs_next_element(&c);
  }

  if (c)
    return invalid_element2(&s->ei,s->uri,c);

  return 0;
}

int xs_parse_complex_type_attributes(xs_schema *s, xmlNodePtr n, xmlDocPtr doc, char *ns,
                                     xs_type *t, xmlNodePtr c)
{
  for (; c; xs_next_element(&c)) {
    /* FIXME: the attribute_uses of a type should also contain all of the attribute uses contained
       in any referenced groups (recursively), and any attribute uses declared on base types
       (recursively), including those of their attribute group references (recursively) */
    if (check_element(c,"attribute",XS_NAMESPACE))
      CHECK_CALL(xs_parse_attribute(s,c,doc,ns,0,&t->local_attribute_uses))
    else if (check_element(c,"attributeGroup",XS_NAMESPACE)) {
      CHECK_CALL(xs_parse_attribute_group_ref(s,c,doc,ns,&t->attribute_group_refs))
    }
    else
      break;
  }

  if (c && check_element(c,"anyAttribute",XS_NAMESPACE)) {
    /* {attribute wildcard} 1.1 */
    CHECK_CALL(xs_parse_wildcard(s,c,doc,&t->local_wildcard))
    assert(t->local_wildcard);
    xs_next_element(&c);
  }

  if (c)
    return invalid_element2(&s->ei,s->uri,c);

  return 0;
}

int xs_parse_simple_content_children(xs_schema *s, xmlNodePtr n, xmlDocPtr doc, char *ns,
                                     xs_type *t, xmlNodePtr c)
{
  if (XS_TYPE_DERIVATION_RESTRICTION == t->derivation_method) {

    if (c && check_element(c,"simpleType",XS_NAMESPACE)) {
      CHECK_CALL(xs_parse_simple_type(s,c,doc,ns,0,&t->child_type));
      xs_next_element(&c);
    }

    while (c && (check_element(c,"length",XS_NAMESPACE) ||
                 check_element(c,"minLength",XS_NAMESPACE) ||
                 check_element(c,"maxLength",XS_NAMESPACE) ||
                 check_element(c,"pattern",XS_NAMESPACE) ||
                 check_element(c,"enumeration",XS_NAMESPACE) ||
                 check_element(c,"whiteSpace",XS_NAMESPACE) ||
                 check_element(c,"maxInclusive",XS_NAMESPACE) ||
                 check_element(c,"maxExclusive",XS_NAMESPACE) ||
                 check_element(c,"minExclusive",XS_NAMESPACE) ||
                 check_element(c,"minInclusive",XS_NAMESPACE) ||
                 check_element(c,"totalDigits",XS_NAMESPACE) ||
                 check_element(c,"fractionDigits",XS_NAMESPACE))) {
      CHECK_CALL(xs_parse_facet(s,c,&t->child_facets))
      xs_next_element(&c);
    }
  }

  return xs_parse_complex_type_attributes(s,n,doc,ns,t,c);
}

int xs_parse_complex_content_children(xs_schema *s,
                                   xmlNodePtr n, xmlDocPtr doc, char *ns,
                                   xs_type *t, xmlNodePtr c, int effective_mixed)
{
  /* Section 3.4.2 - Complex type definition with complex content */

  /* ((group | all | choice | sequence)?, ((attribute | attributeGroup)*, anyAttribute?)) */

  /* {content type} rules */
  int test211 = ((NULL == c) ||
                 (!check_element(c,"group",XS_NAMESPACE) &&
                  !check_element(c,"all",XS_NAMESPACE) &&
                  !check_element(c,"choice",XS_NAMESPACE) &&
                  !check_element(c,"sequence",XS_NAMESPACE)));
  int test212 = (c &&
                 (check_element(c,"all",XS_NAMESPACE) ||
                  check_element(c,"sequence",XS_NAMESPACE)) &&
                 (NULL == xs_first_non_annotation_child(c)));
  int min_occurs_val = 1;
  int convr = 0;
  int test213 = (c &&
                 check_element(c,"all",XS_NAMESPACE) &&
                 (NULL == xs_first_non_annotation_child(c)) &&
                 (0 == (convr = parse_optional_int_attr(&s->ei,s->uri,c,"minOccurs",
                                                        &min_occurs_val,1))) &&
                 (0 == min_occurs_val));
  xs_particle *effective_content = NULL;

  if (0 != convr) /* FIXME: set error info here! */
    return -1;

  if (test211 || test212 || test213) {
    /* 2.1.4, 2.1.5 */
    if (effective_mixed) {
      xs_particle *p = xs_particle_new(s->as);
      p->range.min_occurs = 1;
      p->range.max_occurs = 1;
      p->term_type = XS_PARTICLE_TERM_MODEL_GROUP;
      p->term.mg = xs_model_group_new(s->as);
      p->term.mg->compositor = XS_MODEL_GROUP_COMPOSITOR_SEQUENCE;
      p->term.mg->defline = c ? c->line : n->line;
      p->term.mg->content_model_of = t;
      p->defline = n->line;
      p->empty_content_of = t;
      effective_content = p;
    }
    else {
      effective_content = NULL; /* empty */
    }
  }
  else {
    if (check_element(c,"group",XS_NAMESPACE)) {
      CHECK_CALL(xs_parse_group_ref(s,c,doc,NULL,&effective_content))
    }
    else { /* must be <all>, <choice> or <sequence> - otherwise test211 would be true */
      CHECK_CALL(xs_parse_all_choice_sequence(s,c,ns,doc,NULL,&effective_content,
                 t->def.ident.name,t->def.ident.ns))
      assert(effective_content);
      assert(XS_PARTICLE_TERM_MODEL_GROUP == effective_content->term_type);
      assert(effective_content->term.mg);
      effective_content->term.mg->content_model_of = t;
    }
  }

  if (!test211)
    xs_next_element(&c);

  t->effective_content = effective_content;
  t->effective_mixed = effective_mixed;
  if (t->effective_content)
    t->effective_content->effective_content_of = t;

  /* {content type} rule 3 - done after resolution of the base type */

  return xs_parse_complex_type_attributes(s,n,doc,ns,t,c);
}

int xs_parse_complex_type(xs_schema *s, xmlNodePtr n, xmlDocPtr doc, char *ns,
                          int toplevel, xs_type **tout,
                          char *container_name, char *container_ns)
{
  xs_type *t;
  xmlNodePtr c;
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
    if (xs_is_builtin_type_redeclaration(s,ns,n))
      return 0;

    t = xs_type_new(s->as);
    t->def.loc.line = n->line;
    CHECK_CALL(xs_init_toplevel_object(s,n,doc,ns,s->symt->ss_types,t,
               &t->def.ident.name,&t->def.ident.ns,"type"))

    /* @implements(xmlschema-1:Complex Type Definition{prohibited substitutions})
       test { complextype_cc_extension_block.test }
       test { complextype_cc_extension_block2.test }
       test { complextype_cc_extension_block3.test }
       test { complextype_cc_extension_block4.test }
       test { complextype_cc_extension_block5.test }
       test { complextype_cc_extension_block6.test }
       @end */
    CHECK_CALL(xs_parse_block_final_attr(s,n,"block",s->block_default,
               &t->prohibited_extension,&t->prohibited_restriction,NULL,NULL,NULL));

    /* @implements(xmlschema-1:Complex Type Definition{final})
       test { complextype_cc_extension_final.test }
       test { complextype_cc_extension_final2.test }
       test { complextype_cc_extension_final3.test }
       test { complextype_cc_extension_final4.test }
       test { complextype_cc_extension_final5.test }
       test { complextype_cc_extension_final6.test }
       @end */
    CHECK_CALL(xs_parse_block_final_attr(s,n,"final",s->final_default,
               &t->final_extension,&t->final_restriction,NULL,NULL,NULL));
  }
  else {
    t = xs_type_new(s->as);
    t->def.loc.line = n->line;
    CHECK_CALL(xs_check_forbidden_attribute(s,n,"name"))
    CHECK_CALL(xs_check_forbidden_attribute(s,n,"abstract"))
    CHECK_CALL(xs_check_forbidden_attribute(s,n,"final"))
    CHECK_CALL(xs_check_forbidden_attribute(s,n,"block"))
  }

  t->complex = 1;

  /* @implements(xmlschema-1:Complex Type Definition{abstract})
     test { complextype_abstract.test }
     test { complextype_abstract2.test }
     @end */
  CHECK_CALL(parse_optional_boolean_attr(&s->ei,s->uri,n,"abstract",&t->abstract,0))
  CHECK_CALL(parse_optional_boolean_attr(&s->ei,s->uri,n,"mixed",&effective_mixed,0))
  if (tout)
    *tout = t;

  c = xs_first_non_annotation_child(n);

  if (c && (check_element(c,"simpleContent",XS_NAMESPACE) ||
            check_element(c,"complexContent",XS_NAMESPACE))) {
    xmlNodePtr c2;
    xmlNodePtr rest;

    if (NULL == (c2 = xs_first_non_annotation_child(c)))
      return error(&s->ei,s->uri,c->line,NULL,"expected <restriction> or <extension>");

    /* @implements(xmlschema-1:Complex Type Definition{derivation method}) @end */
    if (check_element(c2,"restriction",XS_NAMESPACE))
      t->derivation_method = XS_TYPE_DERIVATION_RESTRICTION;
    else if (check_element(c2,"extension",XS_NAMESPACE))
      t->derivation_method = XS_TYPE_DERIVATION_EXTENSION;
    else
      return invalid_element2(&s->ei,s->uri,c2);

    /* nothing else allowed after <restriction> or <extension> */
    rest = c2;
    xs_next_element(&rest);
    if (rest)
      return invalid_element2(&s->ei,s->uri,rest);

    /* @implements(xmlschema-1:Complex Type Definition{base type definition}) @end */
    if (!xmlHasProp(c2,"base"))
      return missing_attribute2(&s->ei,s->uri,c2->line,NULL,"base");
    CHECK_CALL(xs_parse_ref(s,c2,doc,"base",XS_OBJECT_TYPE,(void**)&t->base,&t->baseref))

    if (check_element(c,"simpleContent",XS_NAMESPACE)) {
      xmlNodePtr c3;
      if (NULL != (c3 = xs_first_non_annotation_child(c2)))
        CHECK_CALL(xs_parse_simple_content_children(s,n,doc,ns,t,c3))
    }
    else { /* complexContent */
      xmlNodePtr c3;
      CHECK_CALL(parse_optional_boolean_attr(&s->ei,s->uri,c,"mixed",
                                             &effective_mixed,effective_mixed))
      t->complex_content = 1;
      c3 = xs_first_non_annotation_child(c2);
      CHECK_CALL(xs_parse_complex_content_children(s,n,doc,ns,t,c3,effective_mixed))
    }

    /* FIXME: write testcases for extra elements appearing where they are not allowed (for all
       cases where this can occur) */
    xs_next_element(&c2);
    if (c2)
      return invalid_element2(&s->ei,s->uri,c2);

    xs_next_element(&c);
    if (c)
      return invalid_element2(&s->ei,s->uri,c);
  }
  else {
    /* <complexContent> omitted - this is shorthand for complex content restricting ur-type.
       Parse the remaining children of n as if they were inside the <restriction> element */
    t->base = s->globals->complex_ur_type;
    t->derivation_method = XS_TYPE_DERIVATION_RESTRICTION;
    t->complex_content = 1;
    return xs_parse_complex_content_children(s,n,doc,ns,t,c,effective_mixed);
  }

  return 0;
}

int xs_parse_group_def(xmlNodePtr n, xmlDocPtr doc, char *ns, xs_schema *s)
{
  /* parent is a <schema> or <redifine> */
  xs_model_group_def *mgd;
  xmlNodePtr c;

  /* @implements(xmlschema-1:src-model_group_defn) @end
     @implements(xmlschema-1:mgd-props-correct) @end
     @implements(xmlschema-1:src-model_group) @end
     @implements(xmlschema-1:mg-props-correct.1) @end

     @implements(xmlschema-1:Model Group Definition{name}) @end
     @implements(xmlschema-1:Model Group Definition{target namespace}) @end
     @implements(xmlschema-1:Model Group Definition{model group}) @end
     @implements(xmlschema-1:Schema{model group definitions}) @end
   */

  mgd = xs_model_group_def_new(s->as);
  mgd->def.loc.line = n->line;
  CHECK_CALL(xs_init_toplevel_object(s,n,doc,ns,s->symt->ss_model_group_defs,mgd,
                                     &mgd->def.ident.name,&mgd->def.ident.ns,"group"))
  CHECK_CALL(xs_check_conflicting_attributes(s,n,"ref","name",NULL))
  CHECK_CALL(xs_check_conflicting_attributes(s,n,"minOccurs","name",NULL))
  CHECK_CALL(xs_check_conflicting_attributes(s,n,"maxOccurs","name",NULL))

  mgd->model_group = NULL; /* FIXME */
  mgd->annotation = NULL;
  /* FIXME: handle the case of <redefine> */

  if (NULL == (c = xs_first_non_annotation_child(n)))
    return error(&s->ei,s->uri,n->line,NULL,
                 "Model group definition requires an <all>, <choice> or <sequence> child element");

  if (!check_element(c,"all",XS_NAMESPACE) &&
      !check_element(c,"choice",XS_NAMESPACE) &&
      !check_element(c,"sequence",XS_NAMESPACE))
    return invalid_element2(&s->ei,s->uri,c);

  CHECK_CALL(xs_parse_model_group(s,c,ns,doc,&mgd->model_group,mgd->def.ident.name,mgd->def.ident.ns))
  assert(mgd->model_group);
  mgd->model_group->mgd = mgd;

  xs_next_element(&c);
  if (c)
    return invalid_element2(&s->ei,s->uri,c);

  return 0;
}

int xs_parse_group_ref(xs_schema *s, xmlNodePtr n, xmlDocPtr doc, list **particles_list,
                       xs_particle **pout)
{
  int max_occurs_val = 1;
  int min_occurs_val = 1;
  xs_particle *p;

  if (!xmlHasProp(n,"ref"))
    return error(&s->ei,s->uri,n->line,NULL,
                 "<group> can only be used as a group reference here");

  CHECK_CALL(xs_check_conflicting_attributes(s,n,"name","ref",NULL))
  CHECK_CALL(xs_parse_max_occurs(s,n,&max_occurs_val))
  CHECK_CALL(parse_optional_int_attr(&s->ei,s->uri,n,"minOccurs",&min_occurs_val,1))

  if ((0 == min_occurs_val) && (0 == max_occurs_val))
    return 0; /* do not create a schema component */

  p = xs_particle_new(s->as);
  p->range.min_occurs = min_occurs_val;
  p->range.max_occurs = max_occurs_val;
  p->term.mg = NULL; /* will be resolved later */
  p->term_type = XS_PARTICLE_TERM_MODEL_GROUP;
  p->defline = n->line;
  if (particles_list)
    list_append(particles_list,p);
  if (pout)
    *pout = p;

  CHECK_CALL(xs_parse_ref(s,n,doc,"ref",XS_OBJECT_MODEL_GROUP_DEF,(void**)&p->term.mg,&p->ref))

  /* We can't do the resolution here because it may depend on a model group definition that
     we haven't encountered yet. Instead, the resolution is done in xs_resolve_ref() */

  return 0;
}

int xs_parse_model_group(xs_schema *s, xmlNodePtr n, char *ns, xmlDocPtr doc,
                         xs_model_group **mgout, char *container_name, char *container_ns)
{
  /* <all>, <choice>, or <sequence> */
  xs_model_group *mg;
  xmlNodePtr c;
  int allow_gcsa = 0;

  /* @implements(xmlschema-1:Model Group{compositor}) @end
     @implements(xmlschema-1:Model Group{particles}) @end
     */

  /* Create the particle and model group schema components. Because the particle is added
     to the supplied list, and the model is referenced from the particle, these will both
     be freed up eventually even if we return with this function with an error. */
  mg = xs_model_group_new(s->as);
  mg->defline = n->line;

  if (check_element(n,"all",XS_NAMESPACE))
    mg->compositor = XS_MODEL_GROUP_COMPOSITOR_ALL;
  else if (check_element(n,"choice",XS_NAMESPACE))
    mg->compositor = XS_MODEL_GROUP_COMPOSITOR_CHOICE;
  else if (check_element(n,"sequence",XS_NAMESPACE))
    mg->compositor = XS_MODEL_GROUP_COMPOSITOR_SEQUENCE;
  else
    assert(0);

  mg->particles = NULL;
  mg->annotation = NULL;
  assert(mgout);
  *mgout = mg;

  /* Iterate through the child elements of the model group.
     <choice> and <sequence> can have the following children: element, group, choice, sequence, any
     <all> can only have children of type element
     In all cases we allow an annotation element as the first child. */
  allow_gcsa = (XS_MODEL_GROUP_COMPOSITOR_CHOICE == mg->compositor) ||
               (XS_MODEL_GROUP_COMPOSITOR_SEQUENCE == mg->compositor);

  c = xs_first_non_annotation_child(n);

  while (c) {

    if (check_element(c,"element",XS_NAMESPACE))
      CHECK_CALL(xs_parse_element(c,doc,ns,s,&mg->particles))
    else if (allow_gcsa && check_element(c,"group",XS_NAMESPACE))
      CHECK_CALL(xs_parse_group_ref(s,c,doc,&mg->particles,NULL))
    else if (allow_gcsa && check_element(c,"choice",XS_NAMESPACE))
      CHECK_CALL(xs_parse_all_choice_sequence(s,c,ns,doc,&mg->particles,NULL,
                 container_name,container_ns))
    else if (allow_gcsa && check_element(c,"sequence",XS_NAMESPACE))
      CHECK_CALL(xs_parse_all_choice_sequence(s,c,ns,doc,&mg->particles,NULL,
                 container_name,container_ns))
    else if (allow_gcsa && check_element(c,"any",XS_NAMESPACE))
      CHECK_CALL(xs_parse_any(s,c,doc,&mg->particles))
    else
      return invalid_element2(&s->ei,s->uri,c);

    xs_next_element(&c);
  }

  return 0;
}

int xs_parse_all_choice_sequence(xs_schema *s, xmlNodePtr n, char *ns, xmlDocPtr doc,
                                 list **particles_list, xs_particle **pout,
                                 char *container_name, char *container_ns)
{
  xs_particle *p;
  xs_model_group *mg;

  int max_occurs_val = 1;
  int min_occurs_val = 1;

  /* For <choice> and <sequence>, maxOccurs can be "unbounded" or a positive integer */
  /* For <all>, maxOccurs must be equal to 1 */
  CHECK_CALL(xs_parse_max_occurs(s,n,&max_occurs_val))
  CHECK_CALL(parse_optional_int_attr(&s->ei,s->uri,n,"minOccurs",&min_occurs_val,1))

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
  if (check_element(n,"all",XS_NAMESPACE) && (max_occurs_val != 1))
    return invalid_attribute_val(&s->ei,s->uri,n,"maxOccurs");
  if (check_element(n,"all",XS_NAMESPACE) && (min_occurs_val != 0) && (min_occurs_val != 1))
    return invalid_attribute_val(&s->ei,s->uri,n,"minOccurs");

  CHECK_CALL(xs_parse_model_group(s,n,ns,doc,&mg,container_name,container_ns))
  assert(mg);

  p = xs_particle_new(s->as);
  p->term.mg = mg;
  p->term_type = XS_PARTICLE_TERM_MODEL_GROUP;
  p->range.max_occurs = max_occurs_val;
  p->range.min_occurs = min_occurs_val;
  p->defline = n->line;
  if (particles_list)
    list_append(particles_list,p);
  if (pout)
    *pout = p;

  return 0;
}

int xs_parse_wildcard(xs_schema *s, xmlNodePtr n, xmlDocPtr doc, xs_wildcard **wout)
{
  xs_wildcard *w;
  xmlNodePtr c;

  *wout = w = xs_wildcard_new(s->as);
  w->defline = n->line;

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
  if (!xmlHasProp(n,"namespace")) {
    w->type = XS_WILDCARD_TYPE_ANY;
  }
  else {
    char *namespace = get_wscollapsed_attr(n,"namespace",NULL);

    if (!strcmp(namespace,"##any")) {
      w->type = XS_WILDCARD_TYPE_ANY;
    }
    else if (!strcmp(namespace,"##other")) {
      w->type = XS_WILDCARD_TYPE_NOT;
      w->not_ns = s->ns ? strdup(s->ns) : NULL;
    }
    else {
      char *start = namespace;
      char *cur = namespace;

      w->type = XS_WILDCARD_TYPE_SET;

      while (1) {
        if ('\0' == *cur || isspace(*cur)) {
          int endc = *cur;
          *cur = '\0';
          if (cur != start) {
            char *ns;
            if (!strcmp(start,"##targetNamespace"))
              ns = s->ns ? strdup(s->ns) : NULL;
            else if (!strcmp(start,"##local"))
              ns = NULL;
            else
              ns = strdup(start);
            list_append(&w->nslist,ns);
          }
          *cur = endc;
          start = cur+1;
        }
        if ('\0' == *cur)
          break;
        cur++;
      }
    }

    free(namespace);
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
  if (!xmlHasProp(n,"processContents")) {
    w->process_contents = XS_WILDCARD_PROCESS_CONTENTS_STRICT;
  }
  else {
    char *process_contents = xmlGetProp(n,"processContents");
    int invalid = 0;
    if (!strcmp(process_contents,"strict"))
      w->process_contents = XS_WILDCARD_PROCESS_CONTENTS_STRICT;
    else if (!strcmp(process_contents,"lax"))
      w->process_contents = XS_WILDCARD_PROCESS_CONTENTS_LAX;
    else if (!strcmp(process_contents,"skip"))
      w->process_contents = XS_WILDCARD_PROCESS_CONTENTS_SKIP;
    else
      invalid = 1;
    free(process_contents);

    if (invalid)
      return invalid_attribute_val(&s->ei,s->uri,n,"processContents");
  }

  if (NULL != (c = xs_first_non_annotation_child(n)))
    return invalid_element2(&s->ei,s->uri,c);

  return 0;
}

int xs_parse_any(xs_schema *s, xmlNodePtr n, xmlDocPtr doc, list **particles_list)
{
  int max_occurs_val = 1;
  int min_occurs_val = 1;
  xs_particle *p;

  if ((0 == min_occurs_val) && (0 == max_occurs_val))
    return 0; /* do not create a schema component */

  CHECK_CALL(xs_parse_max_occurs(s,n,&max_occurs_val))
  CHECK_CALL(parse_optional_int_attr(&s->ei,s->uri,n,"minOccurs",&min_occurs_val,1))

  p = xs_particle_new(s->as);
  list_append(particles_list,p);
  p->term_type = XS_PARTICLE_TERM_WILDCARD;
  p->range.max_occurs = max_occurs_val;
  p->range.min_occurs = min_occurs_val;
  p->defline = n->line;

  CHECK_CALL(xs_parse_wildcard(s,n,doc,&p->term.w))

  return 0;
}

int xs_parse_import(xs_schema *s, xmlNodePtr n, xmlDocPtr doc)
{
  char *schemaloc;
/*   stringbuf *src = stringbuf_new(); */
  xmlDocPtr import_doc;
  xmlNodePtr import_elem;
  xs_schema *import_schema = NULL;
  char *namespace;
  char *full_uri;
  if (!xmlHasProp(n,"schemaLocation"))
    return error(&s->ei,s->uri,n->line,NULL,
                 "No schemaLocation specified; don't know what to import here");

  /* FIXME: test with bad and nonexistant uris, like with xsl:import */

  /* @implements(xmlschema-1:src-import.1) @end */

  /* @implements(xmlschema-1:src-import.1.1)
     test { xmlschema/import/nsconflict1.test }
     @end */
  if (xmlHasProp(n,"namespace") && (NULL != s->ns)) {
    namespace = xmlGetProp(n,"namespace");
    if (!strcmp(namespace,s->ns)) {
      free(namespace);
      return error(&s->ei,s->uri,n->line,"src-import.1.1","The \"namespace\" attribute for this "
                   "import must either be absent, or different to the target namespace of the "
                   "enclosing schema");
    }
    free(namespace);
  }

  /* @implements(xmlschema-1:src-import.1.2)
     test { xmlschema/import/nsconflict2.test }
     @end */
  if (!xmlHasProp(n,"namespace") && (NULL == s->ns))
    return error(&s->ei,s->uri,n->line,"src-import.1.1","A \"namespace\" attribute is required on "
                 "this import element since the enclosing schema has no target namespace defined");

  schemaloc = get_wscollapsed_attr(n,"schemaLocation",NULL);

  if (NULL == (full_uri = xmlBuildURI(schemaloc,s->uri))) {
    return error(&s->ei,s->uri,n->line,NULL,
                 "\"%s\" is not a valid relative or absolute URI",schemaloc);
  }

  if (0 != retrieve_uri_element(&s->ei,s->uri,n->line,NULL,full_uri,
                                &import_doc,&import_elem,s->uri)) {
    /* FIXME: not sure if we're really supposed to signal an error here... see note
       in import2.test */
    free(schemaloc);
    free(full_uri);
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

  if (!check_element(import_elem,"schema",XS_NAMESPACE)) {
    char *justdoc = strdup(full_uri);
    char *hash;
    if (NULL != (hash = strchr(justdoc,'#')))
      *hash = '\0';
    error(&s->ei,justdoc,import_elem->line,NULL,"Expected element {%s}%s",XS_NAMESPACE,"schema");
    free(justdoc);
    free(schemaloc);
    free(full_uri);
    xmlFreeDoc(import_doc);
    return -1;
  }

  if (0 != parse_xmlschema_element(import_elem,import_doc,full_uri,schemaloc,
                                   &import_schema,&s->ei,s->globals)) {
    free(schemaloc);
    xmlFreeDoc(import_doc);
    free(full_uri);
    return -1;
  }
  free(full_uri);

  list_append(&s->imports,import_schema);
  xmlFreeDoc(import_doc);


  /* @implements(xmlschema-1:src-import.3) @end
     @implements(xmlschema-1:src-import.3.1)
     test { xmlschema/import/nsmismatch1.test }
     test { xmlschema/import/nsmismatch3.test }
     @end
     @implements(xmlschema-1:src-import.3.2)
     test { xmlschema/import/nsmismatch2.test }
     @end */
  namespace = xmlGetProp(n,"namespace");
  if (NULL == import_schema->ns) {
    if (NULL != namespace) {
      free(schemaloc);
      free(namespace);
      return error(&s->ei,s->uri,n->line,"src-import.3.1","This import element must not have a "
                   "\"namespace\" attribute, since the imported schema does not define a target "
                   "namespace");
    }
  }
  else {
    if ((NULL == namespace) || strcmp(namespace,import_schema->ns)) {
      free(namespace);
      free(schemaloc);
      return error(&s->ei,s->uri,n->line,"src-import.3.1","This import element must its "
                   "\"namespace\" attribute set to \"%s\", which is the target namespace declared "
                   "by the imported schema",import_schema->ns);
    }
  }
  free(namespace);
  free(schemaloc);
  return 0;
}

int xs_parse_schema(xs_schema *s, xmlNodePtr n, xmlDocPtr doc)
{
  xmlNodePtr c;

  /* 
     @implements(xmlschema-1:Particle{min occurs}) @end
     @implements(xmlschema-1:Particle{max occurs}) @end
     @implements(xmlschema-1:Particle{term}) @end

     FIXME: check that _everywhere_ we parse an attribute, we respect the whitespace facet defined
     on that attribute's type (i.e. whether to allow leading/trailing whitespace)
  */

  /* FIXME: parse the other attributes */
  s->ns = xmlGetProp(n,"targetNamespace");

  /* parse the "attributeFormDefault" attribute */
  if (xmlHasProp(n,"attributeFormDefault")) {
    int invalid = 0;
    char *afd = get_wscollapsed_attr(n,"attributeFormDefault",NULL);
    if (!strcmp(afd,"qualified"))
      s->attrformq = 1;
    else if (!strcmp(afd,"unqualified"))
      s->attrformq = 0;
    else
      invalid = 1;
    free(afd);
    if (invalid)
      return invalid_attribute_val(&s->ei,s->uri,n,"attributeFormDefault");
  }

  /* parse the "elementFormDefault" attribute */
  if (xmlHasProp(n,"elementFormDefault")) {
    int invalid = 0;
    char *efd = get_wscollapsed_attr(n,"elementFormDefault",NULL);
    if (!strcmp(efd,"qualified"))
      s->elemformq = 1;
    else if (!strcmp(efd,"unqualified"))
      s->elemformq = 0;
    else
      invalid = 1;
    free(efd);
    if (invalid)
      return invalid_attribute_val(&s->ei,s->uri,n,"elementFormDefault");
  }

  s->block_default = xmlGetProp(n,"blockDefault");
  s->final_default = xmlGetProp(n,"finalDefault");

  c = n->children;
  xs_skip_others(&c);

  /* FIXME: check if this properly enforces the ordering defined in the spec */

  /* first section */
  while (c) {
    if (check_element(c,"include",XS_NAMESPACE)) {
    }
    else if (check_element(c,"import",XS_NAMESPACE)) {
      CHECK_CALL(xs_parse_import(s,c,doc))
    }
    else if (check_element(c,"redefine",XS_NAMESPACE)) {
    }
    else if (check_element(c,"annotation",XS_NAMESPACE)) {
    }
    else {
      break;
    }

    xs_next_element(&c);
  }

  /* second section */
  while (c) {
    if (check_element(c,"simpleType",XS_NAMESPACE)) {
      CHECK_CALL(xs_parse_simple_type(s,c,doc,s->ns,1,NULL))
    }
    else if (check_element(c,"complexType",XS_NAMESPACE)) {
      CHECK_CALL(xs_parse_complex_type(s,c,doc,s->ns,1,NULL,NULL,NULL))
    }
    else if (check_element(c,"group",XS_NAMESPACE)) {
      CHECK_CALL(xs_parse_group_def(c,doc,s->ns,s))
    }
    else if (check_element(c,"attributeGroup",XS_NAMESPACE)) {
      CHECK_CALL(xs_parse_attribute_group_def(s,c,doc,s->ns))
    }
    else if (check_element(c,"element",XS_NAMESPACE)) {
      CHECK_CALL(xs_parse_element(c,doc,s->ns,s,NULL))
    }
    else if (check_element(c,"attribute",XS_NAMESPACE)) {
      CHECK_CALL(xs_parse_attribute(s,c,doc,s->ns,1,NULL))
    }
    else if (check_element(c,"notation",XS_NAMESPACE)) {
    }
    else if (check_element(c,"annotation",XS_NAMESPACE)) {
    }
    else {
      return invalid_element2(&s->ei,s->uri,c);
    }

    xs_next_element(&c);
  }

  return 0;
}
