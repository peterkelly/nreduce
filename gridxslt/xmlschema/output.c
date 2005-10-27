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

#include "output.h"
#include "xmlschema.h"
#include "util/stringbuf.h"
#include "util/namespace.h"
#include <libxml/xmlwriter.h>
#include <libxml/xmlIO.h>
#include <libxml/parser.h>
#include <libxml/tree.h>
#include <libxml/uri.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <ctype.h>
#include <errno.h>

using namespace GridXSLT;

void DumpVisitor::incIndent()
{
  indent++;
}

void DumpVisitor::decIndent()
{
  assert(0 < indent);
  indent--;
}

void DumpVisitor::dump_printf(const char *format, ...)
{
  va_list ap;
  StringBuffer buf;

  buf.format("%#i",2*indent);

  va_start(ap,format);
  buf.vformat(format,ap);
  va_end(ap);

  String contents = buf.contents();
  fmessage(f,"%*",&contents);
}

void DumpVisitor::dump_reference(char *type, Reference *r)
{
  if (!r)
    return;
  dump_printf("  %s reference: %*\n",type,&r->def.ident);
}

void DumpVisitor::dump_enum_val(const char **enumvals, char *name, int val)
{
  int i;
  for (i = 0; enumvals[i]; i++) {
    if (i == val) {
      dump_printf("  %s: %s\n",name,enumvals[i]);
      return;
    }
  }
  /* we only get here if the value is invalid */
  assert(0);
}

void output_value_constraint(xmlTextWriter *writer, ValueConstraint *vc)
{
  if (VALUECONSTRAINT_DEFAULT == vc->type)
    XMLWriter::attribute(writer,"default",vc->value);
  else if (VALUECONSTRAINT_FIXED == vc->type)
    XMLWriter::attribute(writer,"fixed",vc->value);
}

void DumpVisitor::dump_value_constraint(ValueConstraint *vc)
{
  if (VALUECONSTRAINT_DEFAULT == vc->type)
    dump_printf("  Value constraint: default \"%s\"\n",vc->value);
  else if (VALUECONSTRAINT_FIXED == vc->type)
    dump_printf("  Value constraint: fixed \"%s\"\n",vc->value);
  else
    dump_printf("  Value constraint: (none)\n");
}

void write_ref_attribute(Schema *s, xmlTextWriter *writer, char *attrname, Reference *r)
{
  if (!r->def.ident.m_ns.isNull()) {
    ns_def *ns = s->globals->namespaces->lookup_href(r->def.ident.m_ns);
    assert(NULL != ns);
    XMLWriter::formatAttribute(writer,attrname,"%s:%*",ns->prefix,&r->def.ident.m_name);
  }
  else {
    XMLWriter::attribute(writer,attrname,r->def.ident.m_name);
  }
}

void xs_output_block_final(xmlTextWriter *writer, char *attrname,
                           int extension, int restriction, int substitution, int list, int union1)
{
  if (extension || restriction || substitution || list || union1) {
    stringbuf *buf = stringbuf_new();
    if (extension)
      stringbuf_format(buf,"extension ");
    if (restriction)
      stringbuf_format(buf,"restriction ");
    if (substitution)
      stringbuf_format(buf,"substitution ");
    if (list)
      stringbuf_format(buf,"list ");
    if (union1)
      stringbuf_format(buf,"union ");
    if (buf->size != 1)
      buf->data[buf->size-2] = '\0';
    XMLWriter::attribute(writer,attrname,buf->data);
    stringbuf_free(buf);
  }
}

void output_wildcard_attrs(Schema *s, xmlTextWriter *writer, Wildcard *w)
{
  if (WILDCARD_TYPE_NOT == w->type) {
    XMLWriter::attribute(writer,"namespace","##other");
  }
  else if (WILDCARD_TYPE_SET == w->type) {
    stringbuf *buf = stringbuf_new();
    list *l;
    for (l = w->nslist; l; l = l->next) {
      char *ns = (char*)l->data;
      if (NULL == ns)
        stringbuf_format(buf,"##local ");
      else if ((NULL != s->ns) && !strcmp(ns,s->ns))
        stringbuf_format(buf,"##targetNamespace ");
      else
        stringbuf_format(buf,"%s ",ns);
    }
    if (buf->size != 1)
      buf->data[buf->size-2] = '\0';
    XMLWriter::attribute(writer,"namespace",buf->data);
    stringbuf_free(buf);
  }
  /* else WILDCARD_TYPE_ANY - default: omit the attribute */

  if (WILDCARD_PROCESS_CONTENTS_SKIP == w->process_contents)
    XMLWriter::attribute(writer,"processContents","skip");
  else if (WILDCARD_PROCESS_CONTENTS_LAX == w->process_contents)
    XMLWriter::attribute(writer,"processContents","lax");
  /* else WILDCARD_PROCESS_CONTENTS_STRICT - default: omit the attribute */
}

void output_facetdata(xmlTextWriter *writer, xs_facetdata *fd)
{
  int i;
  list *l;

  for (i = 0; i < XS_FACET_NUMFACETS; i++) {
    if (NULL != fd->strval[i]) {
      char *ename = (char*)malloc(strlen("xsd:")+strlen(xs_facet_names[i])+1);
      sprintf(ename,"xsd:%s",xs_facet_names[i]);
      xmlTextWriterStartElement(writer,ename);
      XMLWriter::attribute(writer,"value",fd->strval[i]);
      xmlTextWriterEndElement(writer);
      free(ename);
    }
  }

  for (l = fd->patterns; l; l = l->next) {
    xmlTextWriterStartElement(writer,"xsd:pattern");
    XMLWriter::attribute(writer,"value",(char*)l->data);
    xmlTextWriterEndElement(writer);
  }

  for (l = fd->enumerations; l; l = l->next) {
    xmlTextWriterStartElement(writer,"xsd:enumeration");
    XMLWriter::attribute(writer,"value",(char*)l->data);
    xmlTextWriterEndElement(writer);
  }
}

int OutputVisitor::type(Schema *s, xmlDocPtr doc, int post, Type *t)
{
  if (t->builtin)
    return 0;

  if (!t->complex) {
    /* <simpleType> */

    assert((TYPE_VARIETY_ATOMIC == t->variety) ||
           (TYPE_VARIETY_LIST == t->variety) ||
           (TYPE_VARIETY_UNION == t->variety));

    if (!post) {

      xmlTextWriterStartElement(writer,"xsd:simpleType");
      if (!t->def.ident.m_name.isNull())
        XMLWriter::attribute(writer,"name",t->def.ident.m_name);

      xs_output_block_final(writer,"final",t->final_extension,t->final_restriction,0,
                            t->final_list,t->final_union);

      if (TYPE_SIMPLE_RESTRICTION == t->stype) {
        xmlTextWriterStartElement(writer,"xsd:restriction");

        if (NULL != t->baseref)
          write_ref_attribute(s,writer,"base",t->baseref);
        /* otherwise, the anonymous type definition within us will get visited and output
           separately */
      }
      else if (TYPE_SIMPLE_LIST == t->stype) {
        xmlTextWriterStartElement(writer,"xsd:list");

        if (NULL != t->item_typeref)
          write_ref_attribute(s,writer,"itemType",t->item_typeref);
        /* otherwise, the anonymous type definition within us will get visited and output
           separately */

      }
      else {
        stringbuf *member_types = stringbuf_new();
        list *l;
        assert(TYPE_SIMPLE_UNION == t->stype);
        xmlTextWriterStartElement(writer,"xsd:union");

        for (l = t->members; l; l = l->next) {
          MemberType *mt = (MemberType*)l->data;

          if (mt->ref) {
            QName qn = nsname_to_qname(s->globals->namespaces,mt->ref->def.ident);
            stringbuf_format(member_types,"%* ",&qn);
          }
        }

        if (1 != member_types->size) {
            member_types->data[member_types->size-2] = '\0';
            XMLWriter::formatAttribute(writer,"memberTypes","%s",member_types->data);
        }

        stringbuf_free(member_types);
      }
    }
    else { /* post=1 */
      /* note: we output the facets in the post phase so they appear _after_ any anonymous
         type definitions within the <restriction>, <list>, or <union> */
      output_facetdata(writer,&t->facets);

      xmlTextWriterEndElement(writer); /* <restriction>, <list>, or <union> */
      xmlTextWriterEndElement(writer); /* <simpleType> */
    }
  }
  else {
    int shorthand = 0;
    /* <complexType> */
    if (!post) {
      xmlTextWriterStartElement(writer,"xsd:complexType");
      if (!t->def.ident.m_name.isNull())
        XMLWriter::attribute(writer,"name",t->def.ident.m_name);
      if (t->mixed)
        XMLWriter::attribute(writer,"mixed","true");

      xs_output_block_final(writer,"block",t->prohibited_extension,t->prohibited_restriction,0,0,0);
      xs_output_block_final(writer,"final",t->final_extension,t->final_restriction,0,0,0);
      if (t->abstract)
        XMLWriter::attribute(writer,"abstract","true");

      if (!t->complex_content) {
        /* <simpleContent> */
        xmlTextWriterStartElement(writer,"xsd:simpleContent");
      }
      else if (t->base != s->globals->complex_ur_type ||
               TYPE_DERIVATION_RESTRICTION != t->derivation_method) {
        /* <complexContent> */
        xmlTextWriterStartElement(writer,"xsd:complexContent");
      }
      else {
        shorthand = 1;
      }

      if (!shorthand) {
        if (TYPE_DERIVATION_EXTENSION == t->derivation_method) {
          xmlTextWriterStartElement(writer,"xsd:extension");
          write_ref_attribute(s,writer,"base",t->baseref);
        }
        else {
          assert(TYPE_DERIVATION_RESTRICTION == t->derivation_method);
          xmlTextWriterStartElement(writer,"xsd:restriction");
          write_ref_attribute(s,writer,"base",t->baseref);
        }
      }
    }
    else { /* post=1 */
      /* attribute wildcard - we do this when post=1 so it goes after the content */
      /* note that we output the *local* wildcard here, not the computed one */
      if (NULL != t->local_wildcard) {
        xmlTextWriterStartElement(writer,"xsd:anyAttribute");
        output_wildcard_attrs(s,writer,t->local_wildcard);
        xmlTextWriterEndElement(writer);
      }

      if (!t->complex_content) {
        output_facetdata(writer,&t->child_facets);
        xmlTextWriterEndElement(writer); /* </extension> or </restriction> */
        xmlTextWriterEndElement(writer); /* </simpleContent> */
      }
      else if (t->base != s->globals->complex_ur_type ||
               TYPE_DERIVATION_RESTRICTION != t->derivation_method) {
        xmlTextWriterEndElement(writer); /* </extension> or </restriction> */
        xmlTextWriterEndElement(writer); /* </complexContent> */
      }

      xmlTextWriterEndElement(writer);
    }
  }

  return 0;
}

void print_name(FILE *f, char *name, char *ns)
{
  if (ns)
    fmessage(f,"%s [%s]",name,ns);
  else
    fmessage(f,"%s",name);
}

int DumpVisitor::schema(Schema *s, xmlDocPtr doc, int post, Schema *s2)
{
  if (post) {
    decIndent();
    return 0;
  }

  dump_printf("Schema\n");
  dump_printf("  Target namespace: %s\n",s2->ns ? s2->ns : "(none)");
  dump_printf("  Default attribute form: %s\n",s2->attrformq ? "qualified" : "unqualified");
  dump_printf("  Default element form: %s\n",s2->elemformq ? "qualified" : "unqualified");

/* FIXME: display these:
  int block_extension;
  int block_restriction;
  int block_substitution;

  int final_extension;
  int final_restriction;
  int final_list;
  int final_union;
*/

  incIndent();
  return 0;
}

int DumpVisitor::type(Schema *s, xmlDocPtr doc, int post, Type *t)
{
  if (t->builtin) {
    if (!found_non_builtin)
      return 0; /* skip processing the default typedefs */
    if (!post) {
      if (!in_builtin_type)
        dump_printf("(built-in type %*)\n",&t->def.ident.m_name);
      in_builtin_type++;
    }
    else {
      in_builtin_type--;
    }
    return 0;
  }

  found_non_builtin = 1;

  if (post) {
    decIndent();
    return 0;
  }

  if (!t->complex) {
    int facet;
    list *l;
    /* <simpleType> */

    dump_printf("Simple type %*\n",&t->def.ident);

    if (TYPE_SIMPLE_BUILTIN == t->stype)
      dump_printf("  Type: built-in\n");
    else if (TYPE_SIMPLE_RESTRICTION == t->stype)
      dump_printf("  Type: restriction\n");
    else if (TYPE_SIMPLE_LIST == t->stype)
      dump_printf("  Type: list\n");
    else if (TYPE_SIMPLE_UNION == t->stype)
      dump_printf("  Type: union\n");
    else
      assert(0);

    if (TYPE_VARIETY_ATOMIC == t->variety)
      dump_printf("  Variety: atomic\n");
    else if (TYPE_VARIETY_LIST == t->variety)
      dump_printf("  Variety: list\n");
    else if (TYPE_VARIETY_UNION == t->variety)
      dump_printf("  Variety: union\n");
    else
      assert(0);

    dump_printf("  Locally declared facets:\n");
    for (facet = 0; facet < XS_FACET_NUMFACETS; facet++)
      if (t->facets.strval[facet])
        dump_printf("    %s=%s\n",xs_facet_names[facet],t->facets.strval[facet]);
    for (l = t->facets.patterns; l; l = l->next)
      dump_printf("    pattern=%s\n",(char*)l->data);
    for (l = t->facets.enumerations; l; l = l->next)
      dump_printf("    enumeration=%s\n",(char*)l->data);
  }
  else {
    dump_printf("Complex type %*\n",&t->def.ident);

    if (!t->complex_content) {
      /* <simpleContent> */
      dump_printf("  Simple content\n");
      /* FIXME */
    }
    else {
      /* <complexContent> */
      stringbuf *tmpbuf;

      dump_printf("  Complex content\n");

      if (TYPE_DERIVATION_EXTENSION == t->derivation_method) {
        dump_printf("  Derivation method: extension\n");
      }
      else {
        assert(TYPE_DERIVATION_RESTRICTION == t->derivation_method);
        dump_printf("  Derivation method: restriction\n");
      }

      if (t->baseref) {
        if (!t->baseref->def.ident.m_ns.isNull())
          dump_printf("  Base type: %* [%*]\n",&t->baseref->def.ident.m_name,
                      &t->baseref->def.ident.m_ns);
        else
          dump_printf("  Base type: %*\n",&t->baseref->def.ident.m_name);
      }
/*       else if (t->base == s->globals->simple_ur_type) { */
/*         dump_printf("  Base type: {http://www.w3.org/2001/XMLSchema}anySimpleType\n"); */
/*       } */
      else {
/*         assert(t->base == s->globals->complex_ur_type); */
/*         dump_printf("  Base type: {http://www.w3.org/2001/XMLSchema}anyType\n"); */
        dump_printf("  Base type: (built-in ur-type)\n");
      }

      dump_printf("  Abstract: %s\n",t->abstract ? "true" : "false");
      dump_printf("  Mixed: %s\n",t->mixed ? "true" : "false");

      tmpbuf = stringbuf_new();
      if (t->prohibited_extension)
        stringbuf_format(tmpbuf,"extension ");
      if (t->prohibited_restriction)
        stringbuf_format(tmpbuf,"restriction ");
      dump_printf("  Prohibited substitutions: %s\n",tmpbuf->data);

      tmpbuf->data[0] = '\0';
      if (t->final_extension)
        stringbuf_format(tmpbuf,"extension ");
      if (t->final_restriction)
        stringbuf_format(tmpbuf,"restriction ");
      dump_printf("  Final: %s\n",tmpbuf->data);
      stringbuf_free(tmpbuf);
    }
  }

  incIndent();

  return 0;
}

void output_particle_minmax(xmlTextWriter *writer, Particle *p)
{
  if (0 > p->range.max_occurs)
    XMLWriter::attribute(writer,"maxOccurs","unbounded");
  else if (1 != p->range.max_occurs) /* omit attribute if default value (1) */
    XMLWriter::formatAttribute(writer,"maxOccurs","%d",p->range.max_occurs);

  if (1 != p->range.min_occurs) /* omit attribute if default value (1) */
    XMLWriter::formatAttribute(writer,"minOccurs","%d",p->range.min_occurs);
}

int output_model_group(Schema *s, xmlDocPtr doc, void *data, int post, ModelGroup *mg)
{
  return 0;
}

int DumpVisitor::modelGroup(Schema *s, xmlDocPtr doc, int post, ModelGroup *mg)
{
  if (post) {
    decIndent();
    return 0;
  }

  dump_printf("Model group\n");
  if (MODELGROUP_COMPOSITOR_ALL == mg->compositor)
    dump_printf("  Compositor: all\n");
  else if (MODELGROUP_COMPOSITOR_CHOICE == mg->compositor)
    dump_printf("  Compositor: choice\n");
  else if (MODELGROUP_COMPOSITOR_SEQUENCE == mg->compositor)
    dump_printf("  Compositor: sequence\n");
  else
    assert(0);

  incIndent();
  return 0;
}

int DumpVisitor::wildcard(Schema *s, xmlDocPtr doc, int post, Wildcard *w)
{
  list *l;

  if (post)
    return 0;

  dump_printf("Wildcard\n");

  if (WILDCARD_TYPE_ANY == w->type)
    dump_printf("  Namespace constraint type: any\n");
  else if (WILDCARD_TYPE_NOT == w->type)
    dump_printf("  Namespace constraint type: not\n");
  else if (WILDCARD_TYPE_SET == w->type)
    dump_printf("  Namespace constraint type: set\n");
  else if (WILDCARD_TYPE_ANY == w->type)
    assert(!"invalid wildcard type");
  dump_printf("  Namespace not value: %s\n",w->not_ns ? w->not_ns : "(absent)");
  dump_printf("  Namespace list:%s\n",w->nslist ? "" : " (empty)");
  for (l = w->nslist; l; l = l->next)
    dump_printf("    %s\n",l->data ? (char*)l->data : "(absent)");

  if (WILDCARD_PROCESS_CONTENTS_SKIP == w->process_contents)
    dump_printf("  Process contents: skip\n");
  else if (WILDCARD_PROCESS_CONTENTS_LAX == w->process_contents)
    dump_printf("  Process contents: lax\n");
  else if (WILDCARD_PROCESS_CONTENTS_STRICT == w->process_contents)
    dump_printf("  Process contents: strict\n");
  else
    assert(0);

  dump_printf("  Annotation: (none)\n");

  return 0;
}

void maybe_output_element_typeref(Schema *s, SchemaElement *e, xmlTextWriter *writer)
{
  /* only print the type reference if it is different from what would be assigned by default -
     see compute_element_type() */
  if (((NULL == e->sghead) && (e->type != s->globals->complex_ur_type)) ||
      ((NULL != e->sghead) && (e->type != e->sghead->type)))
    write_ref_attribute(s,writer,"type",e->typeref);
}

int OutputVisitor::particle(Schema *s, xmlDocPtr doc, int post, Particle *p)
{
  if (PARTICLE_TERM_ELEMENT == p->term_type) {

    if (!post) {
      xmlTextWriterStartElement(writer,"xsd:element");

      assert(p->term.e);

      if (p->ref)
        write_ref_attribute(s,writer,"ref",p->ref);
      else
        XMLWriter::attribute(writer,"name",p->term.e->def.ident.m_name);

      if (p->term.e->typeref)
        maybe_output_element_typeref(s,p->term.e,writer);
      if (p->term.e->nillable)
        XMLWriter::attribute(writer,"nillable","true");
      if (p->term.e->abstract)
        XMLWriter::attribute(writer,"abstract","true");
      xs_output_block_final(writer,"block",p->term.e->disallow_extension,
                            p->term.e->disallow_restriction,p->term.e->disallow_substitution,0,0);

      output_value_constraint(writer,&p->term.e->vc);
      output_particle_minmax(writer,p);

      if ((!p->term.e->def.ident.m_ns.isNull()) && !s->elemformq)
        XMLWriter::attribute(writer,"form","qualified");
      else if ((p->term.e->def.ident.m_ns.isNull()) && s->elemformq)
        XMLWriter::attribute(writer,"form","unqualified");
    }
    else {
      xmlTextWriterEndElement(writer);
    }
  }
  else if (PARTICLE_TERM_MODEL_GROUP == p->term_type) {

    assert(p->term.mg);

    if (p->ref) {
      if (!post) {
        xmlTextWriterStartElement(writer,"xsd:group");
        write_ref_attribute(s,writer,"ref",p->ref);
        output_particle_minmax(writer,p);
      }
      else {
        xmlTextWriterEndElement(writer);
      }
    }
    else {
      if (!post) {

        if (MODELGROUP_COMPOSITOR_ALL == p->term.mg->compositor)
          xmlTextWriterStartElement(writer,"xsd:all");
        else if (MODELGROUP_COMPOSITOR_CHOICE == p->term.mg->compositor)
          xmlTextWriterStartElement(writer,"xsd:choice");
        else if (MODELGROUP_COMPOSITOR_SEQUENCE == p->term.mg->compositor)
          xmlTextWriterStartElement(writer,"xsd:sequence");
        else
          assert(0);

        output_particle_minmax(writer,p);
      }
      else {
          xmlTextWriterEndElement(writer);
      }
    }
  }
  else {
    assert(PARTICLE_TERM_WILDCARD == p->term_type);

    if (!post) {
      xmlTextWriterStartElement(writer,"xsd:any");
      output_particle_minmax(writer,p);
      assert(p->term.w);

      output_wildcard_attrs(s,writer,p->term.w);
    }
    else {
      xmlTextWriterEndElement(writer);
    }
  }

  return 0;
}

int DumpVisitor::particle(Schema *s, xmlDocPtr doc, int post, Particle *p)
{
  if (post) {
    decIndent();
    return 0;
  }

  if (0 > p->range.max_occurs)
    dump_printf("Particle, occurs %d-(unbounded)\n",p->range.min_occurs);
  else
    dump_printf("Particle, occurs %d-%d\n",p->range.min_occurs,p->range.max_occurs);

  if ((PARTICLE_TERM_ELEMENT == p->term_type) && p->ref)
    dump_reference("Element",p->ref);
  else if ((PARTICLE_TERM_MODEL_GROUP == p->term_type) && p->ref)
    dump_reference("Model group",p->ref);

  incIndent();

  return 0;
}

int OutputVisitor::modelGroupDef(Schema *s, xmlDocPtr doc, int post, ModelGroupDef *mgd)
{
  if (!post) {
    xmlTextWriterStartElement(writer,"xsd:group");
    XMLWriter::attribute(writer,"name",mgd->def.ident.m_name);
    assert(mgd->model_group);

    if (MODELGROUP_COMPOSITOR_ALL == mgd->model_group->compositor)
      xmlTextWriterStartElement(writer,"xsd:all");
    else if (MODELGROUP_COMPOSITOR_CHOICE == mgd->model_group->compositor)
      xmlTextWriterStartElement(writer,"xsd:choice");
    else if (MODELGROUP_COMPOSITOR_SEQUENCE == mgd->model_group->compositor)
      xmlTextWriterStartElement(writer,"xsd:sequence");
    else
      assert(0);
  }
  else {
    xmlTextWriterEndElement(writer);
    xmlTextWriterEndElement(writer);
  }
  return 0;
}

int DumpVisitor::modelGroupDef(Schema *s, xmlDocPtr doc, int post, ModelGroupDef *mgd)
{
  if (post) {
    decIndent();
    return 0;
  }

  assert(mgd->model_group);
  dump_printf("Model group %*\n",&mgd->def.ident);
  if (MODELGROUP_COMPOSITOR_ALL == mgd->model_group->compositor)
    dump_printf("  Compositor: all\n");
  else if (MODELGROUP_COMPOSITOR_CHOICE == mgd->model_group->compositor)
    dump_printf("  Compositor: choice\n");
  else if (MODELGROUP_COMPOSITOR_SEQUENCE == mgd->model_group->compositor)
    dump_printf("  Compositor: sequence\n");
  else
    assert(0);

  incIndent();
  return 0;
}

int OutputVisitor::element(Schema *s, xmlDocPtr doc, int post, SchemaElement *e)
{
  if (!e->toplevel) {
    /* we are inside a model group, referenced from a particle - output_particle() will print
       us instead (since it needs to print the minOccurs and maxOccurs values) */
    return 0;
  }
  if (!post) {
    xmlTextWriterStartElement(writer,"xsd:element");
    XMLWriter::attribute(writer,"name",e->def.ident.m_name);

    assert(e->type);
    if (e->typeref)
      maybe_output_element_typeref(s,e,writer);
    if (e->nillable)
      XMLWriter::attribute(writer,"nillable","true");
    if (e->abstract)
      XMLWriter::attribute(writer,"abstract","true");
    xs_output_block_final(writer,"block",e->disallow_extension,e->disallow_restriction,
                          e->disallow_substitution,0,0);
    if (e->sgheadref)
      write_ref_attribute(s,writer,"substitutionGroup",e->sgheadref);
    output_value_constraint(writer,&e->vc);
  }
  else {
    xmlTextWriterEndElement(writer);
  }
  return 0;
}

int DumpVisitor::element(Schema *s, xmlDocPtr doc, int post, SchemaElement *e)
{
  list *l;
  stringbuf *tmpbuf;

  if (post) {
    decIndent();
    return 0;
  }

  dump_printf("Element %*\n",&e->def.ident);
  dump_reference("Type",e->typeref);
  if (NULL != e->sgheadref) {
    assert(NULL != e->sghead);
    dump_reference("Substitution group",e->sgheadref);
  }
  if (NULL != e->sgmembers) {
    dump_printf("  Substitution group members:\n");
    for (l = e->sgmembers; l; l = l->next) {
      SchemaElement *member = (SchemaElement*)l->data;
      dump_printf("    %*\n",&member->def.ident);
    }
  }

  dump_printf("  Abstract: %s\n",e->abstract ? "true" : "false");
  dump_printf("  Nillable: %s\n",e->nillable ? "true" : "false");

  tmpbuf = stringbuf_new();
  if (e->disallow_substitution)
    stringbuf_format(tmpbuf,"substitution ");
  if (e->disallow_extension)
    stringbuf_format(tmpbuf,"extension ");
  if (e->disallow_restriction)
    stringbuf_format(tmpbuf,"restriction ");
  dump_printf("  Disallowed substitutions: %s\n",tmpbuf->data);

  tmpbuf->data[0] = '\0';
  if (e->exclude_extension)
    stringbuf_format(tmpbuf,"extension ");
  if (e->exclude_restriction)
    stringbuf_format(tmpbuf,"restriction ");
  dump_printf("  Substitution group exclusions: %s\n",tmpbuf->data);
  stringbuf_free(tmpbuf);

  incIndent();
  return 0;
}

int OutputVisitor::attribute(Schema *s, xmlDocPtr doc, int post, SchemaAttribute *a)
{
  if (!a->toplevel) {
    /* we are referenced from an attribute use... that will print the relevant info instead */
    return 0;
  }
  if (!post) {
    xmlTextWriterStartElement(writer,"xsd:attribute");
    XMLWriter::attribute(writer,"name",a->def.ident.m_name);

    if (a->typeref)
      write_ref_attribute(s,writer,"type",a->typeref);

    output_value_constraint(writer,&a->vc);
  }
  else {
    xmlTextWriterEndElement(writer);
  }
  return 0;
}

int OutputVisitor::attributeUse(Schema *s, xmlDocPtr doc, int post, AttributeUse *au)
{
  if (!post) {
    xmlTextWriterStartElement(writer,"xsd:attribute");

    if (au->ref) {
      write_ref_attribute(s,writer,"ref",au->ref);
    }
    else {
      XMLWriter::attribute(writer,"name",au->attribute->def.ident.m_name);

      if (au->attribute->typeref)
        write_ref_attribute(s,writer,"type",au->attribute->typeref);

      if ((!au->attribute->def.ident.m_ns.isNull()) && !s->attrformq)
        XMLWriter::attribute(writer,"form","qualified");
      else if ((au->attribute->def.ident.m_ns.isNull()) && s->attrformq)
        XMLWriter::attribute(writer,"form","unqualified");
    }

    if (au->required)
      XMLWriter::attribute(writer,"use","required");
    else if (au->prohibited)
      XMLWriter::attribute(writer,"use","prohibited");

    output_value_constraint(writer,&au->vc);
  }
  else {
    xmlTextWriterEndElement(writer);
  }
  return 0;
}

int DumpVisitor::attributeGroup(Schema *s, xmlDocPtr doc, int post, AttributeGroup *ag)
{
  if (post) {
    decIndent();
    return 0;
  }

  dump_printf("Attribute group %*\n",&ag->def.ident);

  incIndent();
  return 0;
}

int DumpVisitor::attributeGroupRef(Schema *s, xmlDocPtr doc, int post, AttributeGroupRef *agr)
{
  if (post) {
    decIndent();
    return 0;
  }

  dump_printf("Attribute group reference: %*\n",&agr->ref->def.ident);

  incIndent();
  return 0;
}

int OutputVisitor::attributeGroup(Schema *s, xmlDocPtr doc, int post, AttributeGroup *ag)
{
  if (!post) {
    xmlTextWriterStartElement(writer,"xsd:attributeGroup");
    XMLWriter::attribute(writer,"name",ag->def.ident.m_name);
  }
  else {
    /* attribute wildcard - we do this when post=1 so it goes after the content */
    /* note we print the *local* wildcard here, not the computed one */
    if (NULL != ag->local_wildcard) {
      xmlTextWriterStartElement(writer,"xsd:anyAttribute");
      output_wildcard_attrs(s,writer,ag->local_wildcard);
      xmlTextWriterEndElement(writer);
    }

    xmlTextWriterEndElement(writer);
  }
  return 0;
}

int OutputVisitor::attributeGroupRef(Schema *s, xmlDocPtr doc, int post,
                                AttributeGroupRef *agr)
{
  if (post)
    return 0;

  /* should be nothing inside of this; write it all on the first visit */
  xmlTextWriterStartElement(writer,"xsd:attributeGroup");
  write_ref_attribute(s,writer,"ref",agr->ref);
  xmlTextWriterEndElement(writer);
  return 0;
}

int DumpVisitor::attribute(Schema *s, xmlDocPtr doc, int post, SchemaAttribute *a)
{
  if (post) {
    decIndent();
    return 0;
  }

  assert(a->type);

  dump_printf("Attribute %*\n",&a->def.ident);

  if (a->toplevel)
    dump_value_constraint(&a->vc);

  dump_reference("Type",a->typeref);

  /* FIXME */

  incIndent();
  return 0;
}

int DumpVisitor::attributeUse(Schema *s, xmlDocPtr doc, int post, AttributeUse *au)
{
  if (post) {
    decIndent();
    return 0;
  }

  assert(au->attribute);

  dump_printf("Attribute use\n");
  dump_printf("  Required: %s\n",au->required ? "true" : "false");

  dump_value_constraint(&au->vc);

  dump_reference("Attribute",au->ref);

  incIndent();
  return 0;
}

void output_xmlschema(FILE *f, Schema *s)
{
  xmlOutputBuffer *buf = xmlOutputBufferCreateFile(f,NULL);
  xmlTextWriter *writer = xmlNewTextWriter(buf);
  OutputVisitor v(writer);
  list *l;

  xmlTextWriterSetIndent(writer,1);
  xmlTextWriterSetIndentString(writer,"  ");
  xmlTextWriterStartDocument(writer,NULL,NULL,NULL);
  xmlTextWriterStartElement(writer,"xsd:schema");
  for (l = s->globals->namespaces->defs; l; l = l->next) {
    ns_def *ns = (ns_def*)l->data;
    /* Hide the xdt namespace since it is only applicable for xpath */
    if (ns->href != XDT_NAMESPACE) {
      char *attrname = (char*)malloc(strlen("xmlns:")+strlen(ns->prefix)+1);
      sprintf(attrname,"xmlns:%s",ns->prefix);
      XMLWriter::attribute(writer,attrname,ns->href);
      free(attrname);
    }
  }
  if (s->ns)
    XMLWriter::attribute(writer,"targetNamespace",s->ns);
  if (s->attrformq)
    XMLWriter::attribute(writer,"attributeFormDefault","qualified");
  if (s->elemformq)
    XMLWriter::attribute(writer,"elementFormDefault","qualified");

  for (l = s->imports; l; l = l->next) {
    Schema *import = (Schema*)l->data;
    char *rel = get_relative_uri(import->uri,s->uri);
    xmlTextWriterStartElement(writer,"xsd:import");
    if (NULL != import->ns)
      XMLWriter::attribute(writer,"namespace",import->ns);
    XMLWriter::attribute(writer,"schemaLocation",rel);
    xmlTextWriterEndElement(writer);
    free(rel);
  }

  v.once_each = 1;

  s->visit(NULL,&v);

  xmlTextWriterEndElement(writer);
  xmlTextWriterEndDocument(writer);
  xmlTextWriterFlush(writer);
  xmlFreeTextWriter(writer);
}

void dump_xmlschema(FILE *f, Schema *s)
{
  DumpVisitor v;

  v.f = f;
  v.indent = 0;
  v.in_builtin_type = 0;
  v.found_non_builtin = 0;

  s->visit(NULL,&v);
}
