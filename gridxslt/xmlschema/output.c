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

void dump_inc_indent(dumpinfo *di)
{
  di->indent++;
}

void dump_dec_indent(dumpinfo *di)
{
  assert(0 < di->indent);
  di->indent--;
}

void dump_printf(dumpinfo *di, const char *format, ...)
{
  va_list ap;
  stringbuf *buf = stringbuf_new();

  stringbuf_format(buf,"%#i",2*di->indent);

  va_start(ap,format);
  stringbuf_vformat(buf,format,ap);
  va_end(ap);

  fprintf(di->f,buf->data);
  stringbuf_free(buf);
}

void dump_reference(dumpinfo *di, char *type, xs_reference *r)
{
  if (!r)
    return;
  dump_printf(di,"  %s reference: %#n\n",type,r->def.ident);
}

void dump_enum_val(dumpinfo *di, const char **enumvals, char *name, int val)
{
  int i;
  for (i = 0; enumvals[i]; i++) {
    if (i == val) {
      dump_printf(di,"  %s: %s\n",name,enumvals[i]);
      return;
    }
  }
  /* we only get here if the value is invalid */
  assert(0);
}

void output_value_constraint(xmlTextWriter *writer, xs_value_constraint *vc)
{
  if (XS_VALUE_CONSTRAINT_DEFAULT == vc->type)
    xmlTextWriterWriteAttribute(writer,"default",vc->value);
  else if (XS_VALUE_CONSTRAINT_FIXED == vc->type)
    xmlTextWriterWriteAttribute(writer,"fixed",vc->value);
}

void dump_value_constraint(dumpinfo *di, xs_value_constraint *vc)
{
  if (XS_VALUE_CONSTRAINT_DEFAULT == vc->type)
    dump_printf(di,"  Value constraint: default \"%s\"\n",vc->value);
  else if (XS_VALUE_CONSTRAINT_FIXED == vc->type)
    dump_printf(di,"  Value constraint: fixed \"%s\"\n",vc->value);
  else
    dump_printf(di,"  Value constraint: (none)\n");
}

void write_ref_attribute(xs_schema *s, xmlTextWriter *writer, char *attrname, xs_reference *r)
{
  if (r->def.ident.ns) {
    ns_def *ns = ns_lookup_href(s->globals->namespaces,r->def.ident.ns);
    assert(NULL != ns);
    xmlTextWriterWriteFormatAttribute(writer,attrname,"%s:%s",ns->prefix,r->def.ident.name);
  }
  else {
    xmlTextWriterWriteAttribute(writer,attrname,r->def.ident.name);
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
    xmlTextWriterWriteAttribute(writer,attrname,buf->data);
    stringbuf_free(buf);
  }
}

void output_wildcard_attrs(xs_schema *s, xmlTextWriter *writer, xs_wildcard *w)
{
  if (XS_WILDCARD_TYPE_NOT == w->type) {
    xmlTextWriterWriteAttribute(writer,"namespace","##other");
  }
  else if (XS_WILDCARD_TYPE_SET == w->type) {
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
    xmlTextWriterWriteAttribute(writer,"namespace",buf->data);
    stringbuf_free(buf);
  }
  /* else XS_WILDCARD_TYPE_ANY - default: omit the attribute */

  if (XS_WILDCARD_PROCESS_CONTENTS_SKIP == w->process_contents)
    xmlTextWriterWriteAttribute(writer,"processContents","skip");
  else if (XS_WILDCARD_PROCESS_CONTENTS_LAX == w->process_contents)
    xmlTextWriterWriteAttribute(writer,"processContents","lax");
  /* else XS_WILDCARD_PROCESS_CONTENTS_STRICT - default: omit the attribute */
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
      xmlTextWriterWriteAttribute(writer,"value",fd->strval[i]);
      xmlTextWriterEndElement(writer);
      free(ename);
    }
  }

  for (l = fd->patterns; l; l = l->next) {
    xmlTextWriterStartElement(writer,"xsd:pattern");
    xmlTextWriterWriteAttribute(writer,"value",(char*)l->data);
    xmlTextWriterEndElement(writer);
  }

  for (l = fd->enumerations; l; l = l->next) {
    xmlTextWriterStartElement(writer,"xsd:enumeration");
    xmlTextWriterWriteAttribute(writer,"value",(char*)l->data);
    xmlTextWriterEndElement(writer);
  }
}

int output_type(xs_schema *s, xmlDocPtr doc, void *data, int post, xs_type *t)
{
  xmlTextWriter *writer = (xmlTextWriter*)data;

  if (t->builtin)
    return 0;

  if (!t->complex) {
    /* <simpleType> */

    assert((XS_TYPE_VARIETY_ATOMIC == t->variety) ||
           (XS_TYPE_VARIETY_LIST == t->variety) ||
           (XS_TYPE_VARIETY_UNION == t->variety));

    if (!post) {

      xmlTextWriterStartElement(writer,"xsd:simpleType");
      if (t->def.ident.name)
        xmlTextWriterWriteAttribute(writer,"name",t->def.ident.name);

      xs_output_block_final(writer,"final",t->final_extension,t->final_restriction,0,
                            t->final_list,t->final_union);

      if (XS_TYPE_SIMPLE_RESTRICTION == t->stype) {
        xmlTextWriterStartElement(writer,"xsd:restriction");

        if (NULL != t->baseref)
          write_ref_attribute(s,writer,"base",t->baseref);
        /* otherwise, the anonymous type definition within us will get visited and output
           separately */
      }
      else if (XS_TYPE_SIMPLE_LIST == t->stype) {
        xmlTextWriterStartElement(writer,"xsd:list");

        if (NULL != t->item_typeref)
          write_ref_attribute(s,writer,"itemType",t->item_typeref);
        /* otherwise, the anonymous type definition within us will get visited and output
           separately */

      }
      else {
        stringbuf *member_types = stringbuf_new();
        list *l;
        assert(XS_TYPE_SIMPLE_UNION == t->stype);
        xmlTextWriterStartElement(writer,"xsd:union");

        for (l = t->members; l; l = l->next) {
          xs_member_type *mt = (xs_member_type*)l->data;

          if (mt->ref) {
            qname qn = nsname_to_qname(s->globals->namespaces,mt->ref->def.ident);
            stringbuf_format(member_types,"%#q ",qn);
            qname_free(qn);
          }
        }

        if (1 != member_types->size) {
            member_types->data[member_types->size-2] = '\0';
            xmlTextWriterWriteFormatAttribute(writer,"memberTypes","%s",member_types->data);
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
      if (t->def.ident.name)
        xmlTextWriterWriteAttribute(writer,"name",t->def.ident.name);
      if (t->mixed)
        xmlTextWriterWriteAttribute(writer,"mixed","true");

      xs_output_block_final(writer,"block",t->prohibited_extension,t->prohibited_restriction,0,0,0);
      xs_output_block_final(writer,"final",t->final_extension,t->final_restriction,0,0,0);
      if (t->abstract)
        xmlTextWriterWriteAttribute(writer,"abstract","true");

      if (!t->complex_content) {
        /* <simpleContent> */
        xmlTextWriterStartElement(writer,"xsd:simpleContent");
      }
      else if (t->base != s->globals->complex_ur_type ||
               XS_TYPE_DERIVATION_RESTRICTION != t->derivation_method) {
        /* <complexContent> */
        xmlTextWriterStartElement(writer,"xsd:complexContent");
      }
      else {
        shorthand = 1;
      }

      if (!shorthand) {
        if (XS_TYPE_DERIVATION_EXTENSION == t->derivation_method) {
          xmlTextWriterStartElement(writer,"xsd:extension");
          write_ref_attribute(s,writer,"base",t->baseref);
        }
        else {
          assert(XS_TYPE_DERIVATION_RESTRICTION == t->derivation_method);
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
               XS_TYPE_DERIVATION_RESTRICTION != t->derivation_method) {
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
    fprintf(f,"%s [%s]",name,ns);
  else
    fprintf(f,"%s",name);
}

int dump_schema(xs_schema *s, xmlDocPtr doc, void *data, int post, xs_schema *s2)
{
  dumpinfo *di = (dumpinfo*)data;

  if (post) {
    dump_dec_indent(di);
    return 0;
  }

  dump_printf(di,"Schema\n");
  dump_printf(di,"  Target namespace: %s\n",s2->ns ? s2->ns : "(none)");
  dump_printf(di,"  Default attribute form: %s\n",s2->attrformq ? "qualified" : "unqualified");
  dump_printf(di,"  Default element form: %s\n",s2->elemformq ? "qualified" : "unqualified");

/* FIXME: display these:
  int block_extension;
  int block_restriction;
  int block_substitution;

  int final_extension;
  int final_restriction;
  int final_list;
  int final_union;
*/

  dump_inc_indent(di);
  return 0;
}

int dump_type(xs_schema *s, xmlDocPtr doc, void *data, int post, xs_type *t)
{
  dumpinfo *di = (dumpinfo*)data;

  if (t->builtin) {
    if (!di->found_non_builtin)
      return 0; /* skip processing the default typedefs */
    if (!post) {
      if (!di->in_builtin_type)
        dump_printf(di,"(built-in type %s)\n",t->def.ident.name);
      di->in_builtin_type++;
    }
    else {
      di->in_builtin_type--;
    }
    return 0;
  }

  di->found_non_builtin = 1;

  if (post) {
    dump_dec_indent(di);
    return 0;
  }

  if (!t->complex) {
    int facet;
    list *l;
    /* <simpleType> */

    dump_printf(di,"Simple type %#n\n",t->def.ident);

    if (XS_TYPE_SIMPLE_BUILTIN == t->stype)
      dump_printf(di,"  Type: built-in\n");
    else if (XS_TYPE_SIMPLE_RESTRICTION == t->stype)
      dump_printf(di,"  Type: restriction\n");
    else if (XS_TYPE_SIMPLE_LIST == t->stype)
      dump_printf(di,"  Type: list\n");
    else if (XS_TYPE_SIMPLE_UNION == t->stype)
      dump_printf(di,"  Type: union\n");
    else
      assert(0);

    if (XS_TYPE_VARIETY_ATOMIC == t->variety)
      dump_printf(di,"  Variety: atomic\n");
    else if (XS_TYPE_VARIETY_LIST == t->variety)
      dump_printf(di,"  Variety: list\n");
    else if (XS_TYPE_VARIETY_UNION == t->variety)
      dump_printf(di,"  Variety: union\n");
    else
      assert(0);

    dump_printf(di,"  Locally declared facets:\n");
    for (facet = 0; facet < XS_FACET_NUMFACETS; facet++)
      if (t->facets.strval[facet])
        dump_printf(di,"    %s=%s\n",xs_facet_names[facet],t->facets.strval[facet]);
    for (l = t->facets.patterns; l; l = l->next)
      dump_printf(di,"    pattern=%s\n",(char*)l->data);
    for (l = t->facets.enumerations; l; l = l->next)
      dump_printf(di,"    enumeration=%s\n",(char*)l->data);
  }
  else {
    dump_printf(di,"Complex type %#n\n",t->def.ident);

    if (!t->complex_content) {
      /* <simpleContent> */
      dump_printf(di,"  Simple content\n");
      /* FIXME */
    }
    else {
      /* <complexContent> */
      stringbuf *tmpbuf;

      dump_printf(di,"  Complex content\n");

      if (XS_TYPE_DERIVATION_EXTENSION == t->derivation_method) {
        dump_printf(di,"  Derivation method: extension\n");
      }
      else {
        assert(XS_TYPE_DERIVATION_RESTRICTION == t->derivation_method);
        dump_printf(di,"  Derivation method: restriction\n");
      }

      if (t->baseref) {
        if (t->baseref->def.ident.ns)
          dump_printf(di,"  Base type: %s [%s]\n",t->baseref->def.ident.name,
                      t->baseref->def.ident.ns);
        else
          dump_printf(di,"  Base type: %s\n",t->baseref->def.ident.name);
      }
/*       else if (t->base == s->globals->simple_ur_type) { */
/*         dump_printf(di,"  Base type: {http://www.w3.org/2001/XMLSchema}anySimpleType\n"); */
/*       } */
      else {
/*         assert(t->base == s->globals->complex_ur_type); */
/*         dump_printf(di,"  Base type: {http://www.w3.org/2001/XMLSchema}anyType\n"); */
        dump_printf(di,"  Base type: (built-in ur-type)\n");
      }

      dump_printf(di,"  Abstract: %s\n",t->abstract ? "true" : "false");
      dump_printf(di,"  Mixed: %s\n",t->mixed ? "true" : "false");

      tmpbuf = stringbuf_new();
      if (t->prohibited_extension)
        stringbuf_format(tmpbuf,"extension ");
      if (t->prohibited_restriction)
        stringbuf_format(tmpbuf,"restriction ");
      dump_printf(di,"  Prohibited substitutions: %s\n",tmpbuf->data);

      tmpbuf->data[0] = '\0';
      if (t->final_extension)
        stringbuf_format(tmpbuf,"extension ");
      if (t->final_restriction)
        stringbuf_format(tmpbuf,"restriction ");
      dump_printf(di,"  Final: %s\n",tmpbuf->data);
      stringbuf_free(tmpbuf);
    }
  }

  dump_inc_indent(di);

  return 0;
}

void output_particle_minmax(xmlTextWriter *writer, xs_particle *p)
{
  if (0 > p->range.max_occurs)
    xmlTextWriterWriteAttribute(writer,"maxOccurs","unbounded");
  else if (1 != p->range.max_occurs) /* omit attribute if default value (1) */
    xmlTextWriterWriteFormatAttribute(writer,"maxOccurs","%d",p->range.max_occurs);

  if (1 != p->range.min_occurs) /* omit attribute if default value (1) */
    xmlTextWriterWriteFormatAttribute(writer,"minOccurs","%d",p->range.min_occurs);
}

int output_model_group(xs_schema *s, xmlDocPtr doc, void *data, int post, xs_model_group *mg)
{
  return 0;
}

int dump_model_group(xs_schema *s, xmlDocPtr doc, void *data, int post, xs_model_group *mg)
{
  dumpinfo *di = (dumpinfo*)data;

  if (post) {
    dump_dec_indent(di);
    return 0;
  }

  dump_printf(di,"Model group\n");
  if (XS_MODEL_GROUP_COMPOSITOR_ALL == mg->compositor)
    dump_printf(di,"  Compositor: all\n");
  else if (XS_MODEL_GROUP_COMPOSITOR_CHOICE == mg->compositor)
    dump_printf(di,"  Compositor: choice\n");
  else if (XS_MODEL_GROUP_COMPOSITOR_SEQUENCE == mg->compositor)
    dump_printf(di,"  Compositor: sequence\n");
  else
    assert(0);

  dump_inc_indent(di);
  return 0;
}

int dump_wildcard(xs_schema *s, xmlDocPtr doc, void *data, int post, xs_wildcard *w)
{
  dumpinfo *di = (dumpinfo*)data;
  list *l;

  if (post)
    return 0;

  dump_printf(di,"Wildcard\n");

  if (XS_WILDCARD_TYPE_ANY == w->type)
    dump_printf(di,"  Namespace constraint type: any\n");
  else if (XS_WILDCARD_TYPE_NOT == w->type)
    dump_printf(di,"  Namespace constraint type: not\n");
  else if (XS_WILDCARD_TYPE_SET == w->type)
    dump_printf(di,"  Namespace constraint type: set\n");
  else if (XS_WILDCARD_TYPE_ANY == w->type)
    assert(!"invalid wildcard type");
  dump_printf(di,"  Namespace not value: %s\n",w->not_ns ? w->not_ns : "(absent)");
  dump_printf(di,"  Namespace list:%s\n",w->nslist ? "" : " (empty)");
  for (l = w->nslist; l; l = l->next)
    dump_printf(di,"    %s\n",l->data ? (char*)l->data : "(absent)");

  if (XS_WILDCARD_PROCESS_CONTENTS_SKIP == w->process_contents)
    dump_printf(di,"  Process contents: skip\n");
  else if (XS_WILDCARD_PROCESS_CONTENTS_LAX == w->process_contents)
    dump_printf(di,"  Process contents: lax\n");
  else if (XS_WILDCARD_PROCESS_CONTENTS_STRICT == w->process_contents)
    dump_printf(di,"  Process contents: strict\n");
  else
    assert(0);

  dump_printf(di,"  Annotation: (none)\n");

  return 0;
}

void maybe_output_element_typeref(xs_schema *s, xs_element *e, xmlTextWriter *writer)
{
  /* only print the type reference if it is different from what would be assigned by default -
     see compute_element_type() */
  if (((NULL == e->sghead) && (e->type != s->globals->complex_ur_type)) ||
      ((NULL != e->sghead) && (e->type != e->sghead->type)))
    write_ref_attribute(s,writer,"type",e->typeref);
}

int output_particle(xs_schema *s, xmlDocPtr doc, void *data, int post, xs_particle *p)
{
  xmlTextWriter *writer = (xmlTextWriter*)data;

  if (XS_PARTICLE_TERM_ELEMENT == p->term_type) {

    if (!post) {
      xmlTextWriterStartElement(writer,"xsd:element");

      assert(p->term.e);

      if (p->ref)
        write_ref_attribute(s,writer,"ref",p->ref);
      else
        xmlTextWriterWriteAttribute(writer,"name",p->term.e->def.ident.name);

      if (p->term.e->typeref)
        maybe_output_element_typeref(s,p->term.e,writer);
      if (p->term.e->nillable)
        xmlTextWriterWriteAttribute(writer,"nillable","true");
      if (p->term.e->abstract)
        xmlTextWriterWriteAttribute(writer,"abstract","true");
      xs_output_block_final(writer,"block",p->term.e->disallow_extension,
                            p->term.e->disallow_restriction,p->term.e->disallow_substitution,0,0);

      output_value_constraint(writer,&p->term.e->vc);
      output_particle_minmax(writer,p);

      if ((NULL != p->term.e->def.ident.ns) && !s->elemformq)
        xmlTextWriterWriteAttribute(writer,"form","qualified");
      else if ((NULL == p->term.e->def.ident.ns) && s->elemformq)
        xmlTextWriterWriteAttribute(writer,"form","unqualified");
    }
    else {
      xmlTextWriterEndElement(writer);
    }
  }
  else if (XS_PARTICLE_TERM_MODEL_GROUP == p->term_type) {

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

        if (XS_MODEL_GROUP_COMPOSITOR_ALL == p->term.mg->compositor)
          xmlTextWriterStartElement(writer,"xsd:all");
        else if (XS_MODEL_GROUP_COMPOSITOR_CHOICE == p->term.mg->compositor)
          xmlTextWriterStartElement(writer,"xsd:choice");
        else if (XS_MODEL_GROUP_COMPOSITOR_SEQUENCE == p->term.mg->compositor)
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
    assert(XS_PARTICLE_TERM_WILDCARD == p->term_type);

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

int dump_particle(xs_schema *s, xmlDocPtr doc, void *data, int post, xs_particle *p)
{
  dumpinfo *di = (dumpinfo*)data;

  if (post) {
    dump_dec_indent(di);
    return 0;
  }

  if (0 > p->range.max_occurs)
    dump_printf(di,"Particle, occurs %d-(unbounded)\n",p->range.min_occurs);
  else
    dump_printf(di,"Particle, occurs %d-%d\n",p->range.min_occurs,p->range.max_occurs);

  if ((XS_PARTICLE_TERM_ELEMENT == p->term_type) && p->ref)
    dump_reference(di,"Element",p->ref);
  else if ((XS_PARTICLE_TERM_MODEL_GROUP == p->term_type) && p->ref)
    dump_reference(di,"Model group",p->ref);

  dump_inc_indent(di);

  return 0;
}

int output_model_group_def(xs_schema *s, xmlDocPtr doc, void *data, int post,
                           xs_model_group_def *mgd)
{
  xmlTextWriter *writer = (xmlTextWriter*)data;

  if (!post) {
    xmlTextWriterStartElement(writer,"xsd:group");
    xmlTextWriterWriteAttribute(writer,"name",mgd->def.ident.name);
    assert(mgd->model_group);

    if (XS_MODEL_GROUP_COMPOSITOR_ALL == mgd->model_group->compositor)
      xmlTextWriterStartElement(writer,"xsd:all");
    else if (XS_MODEL_GROUP_COMPOSITOR_CHOICE == mgd->model_group->compositor)
      xmlTextWriterStartElement(writer,"xsd:choice");
    else if (XS_MODEL_GROUP_COMPOSITOR_SEQUENCE == mgd->model_group->compositor)
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

int dump_model_group_def(xs_schema *s, xmlDocPtr doc, void *data, int post, xs_model_group_def *mgd)
{
  dumpinfo *di = (dumpinfo*)data;

  if (post) {
    dump_dec_indent(di);
    return 0;
  }

  assert(mgd->model_group);
  dump_printf(di,"Model group %#n\n",mgd->def.ident);
  if (XS_MODEL_GROUP_COMPOSITOR_ALL == mgd->model_group->compositor)
    dump_printf(di,"  Compositor: all\n");
  else if (XS_MODEL_GROUP_COMPOSITOR_CHOICE == mgd->model_group->compositor)
    dump_printf(di,"  Compositor: choice\n");
  else if (XS_MODEL_GROUP_COMPOSITOR_SEQUENCE == mgd->model_group->compositor)
    dump_printf(di,"  Compositor: sequence\n");
  else
    assert(0);

  dump_inc_indent(di);
  return 0;
}

int output_element(xs_schema *s, xmlDocPtr doc, void *data, int post, xs_element *e)
{
  xmlTextWriter *writer = (xmlTextWriter*)data;
  if (!e->toplevel) {
    /* we are inside a model group, referenced from a particle - output_particle() will print
       us instead (since it needs to print the minOccurs and maxOccurs values) */
    return 0;
  }
  if (!post) {
    xmlTextWriterStartElement(writer,"xsd:element");
    xmlTextWriterWriteAttribute(writer,"name",e->def.ident.name);

    assert(e->type);
    if (e->typeref)
      maybe_output_element_typeref(s,e,writer);
    if (e->nillable)
      xmlTextWriterWriteAttribute(writer,"nillable","true");
    if (e->abstract)
      xmlTextWriterWriteAttribute(writer,"abstract","true");
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

int dump_element(xs_schema *s, xmlDocPtr doc, void *data, int post, xs_element *e)
{
  dumpinfo *di = (dumpinfo*)data;
  list *l;
  stringbuf *tmpbuf;

  if (post) {
    dump_dec_indent(di);
    return 0;
  }

  dump_printf(di,"Element %#n\n",e->def.ident);
  dump_reference(di,"Type",e->typeref);
  if (NULL != e->sgheadref) {
    assert(NULL != e->sghead);
    dump_reference(di,"Substitution group",e->sgheadref);
  }
  if (NULL != e->sgmembers) {
    dump_printf(di,"  Substitution group members:\n");
    for (l = e->sgmembers; l; l = l->next) {
      xs_element *member = (xs_element*)l->data;
      dump_printf(di,"    %#n\n",member->def.ident);
    }
  }

  dump_printf(di,"  Abstract: %s\n",e->abstract ? "true" : "false");
  dump_printf(di,"  Nillable: %s\n",e->nillable ? "true" : "false");

  tmpbuf = stringbuf_new();
  if (e->disallow_substitution)
    stringbuf_format(tmpbuf,"substitution ");
  if (e->disallow_extension)
    stringbuf_format(tmpbuf,"extension ");
  if (e->disallow_restriction)
    stringbuf_format(tmpbuf,"restriction ");
  dump_printf(di,"  Disallowed substitutions: %s\n",tmpbuf->data);

  tmpbuf->data[0] = '\0';
  if (e->exclude_extension)
    stringbuf_format(tmpbuf,"extension ");
  if (e->exclude_restriction)
    stringbuf_format(tmpbuf,"restriction ");
  dump_printf(di,"  Substitution group exclusions: %s\n",tmpbuf->data);
  stringbuf_free(tmpbuf);

  dump_inc_indent(di);
  return 0;
}

int output_attribute(xs_schema *s, xmlDocPtr doc, void *data, int post, xs_attribute *a)
{
  xmlTextWriter *writer = (xmlTextWriter*)data;
  if (!a->toplevel) {
    /* we are referenced from an attribute use... that will print the relevant info instead */
    return 0;
  }
  if (!post) {
    xmlTextWriterStartElement(writer,"xsd:attribute");
    xmlTextWriterWriteAttribute(writer,"name",a->def.ident.name);

    if (a->typeref)
      write_ref_attribute(s,writer,"type",a->typeref);

    output_value_constraint(writer,&a->vc);
  }
  else {
    xmlTextWriterEndElement(writer);
  }
  return 0;
}

int output_attribute_use(xs_schema *s, xmlDocPtr doc, void *data, int post, xs_attribute_use *au)
{
  xmlTextWriter *writer = (xmlTextWriter*)data;
  if (!post) {
    xmlTextWriterStartElement(writer,"xsd:attribute");

    if (au->ref) {
      write_ref_attribute(s,writer,"ref",au->ref);
    }
    else {
      xmlTextWriterWriteAttribute(writer,"name",au->attribute->def.ident.name);

      if (au->attribute->typeref)
        write_ref_attribute(s,writer,"type",au->attribute->typeref);

      if ((NULL != au->attribute->def.ident.ns) && !s->attrformq)
        xmlTextWriterWriteAttribute(writer,"form","qualified");
      else if ((NULL == au->attribute->def.ident.ns) && s->attrformq)
        xmlTextWriterWriteAttribute(writer,"form","unqualified");
    }

    if (au->required)
      xmlTextWriterWriteAttribute(writer,"use","required");
    else if (au->prohibited)
      xmlTextWriterWriteAttribute(writer,"use","prohibited");

    output_value_constraint(writer,&au->vc);
  }
  else {
    xmlTextWriterEndElement(writer);
  }
  return 0;
}

int dump_attribute_group(xs_schema *s, xmlDocPtr doc, void *data, int post,
                         xs_attribute_group *ag)
{
  dumpinfo *di = (dumpinfo*)data;
  if (post) {
    dump_dec_indent(di);
    return 0;
  }

  dump_printf(di,"Attribute group %#n\n",ag->def.ident);

  dump_inc_indent(di);
  return 0;
}

int dump_attribute_group_ref(xs_schema *s, xmlDocPtr doc, void *data, int post,
                             xs_attribute_group_ref *agr)
{
  dumpinfo *di = (dumpinfo*)data;
  if (post) {
    dump_dec_indent(di);
    return 0;
  }

  dump_printf(di,"Attribute group reference: %#n\n",agr->ref->def.ident);

  dump_inc_indent(di);
  return 0;
}

int output_attribute_group(xs_schema *s, xmlDocPtr doc, void *data, int post,
                           xs_attribute_group *ag)
{
  xmlTextWriter *writer = (xmlTextWriter*)data;
  if (!post) {
    xmlTextWriterStartElement(writer,"xsd:attributeGroup");
    xmlTextWriterWriteAttribute(writer,"name",ag->def.ident.name);
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

int output_attribute_group_ref(xs_schema *s, xmlDocPtr doc, void *data, int post,
                               xs_attribute_group_ref *agr)
{
  xmlTextWriter *writer = (xmlTextWriter*)data;
  if (post)
    return 0;

  /* should be nothing inside of this; write it all on the first visit */
  xmlTextWriterStartElement(writer,"xsd:attributeGroup");
  write_ref_attribute(s,writer,"ref",agr->ref);
  xmlTextWriterEndElement(writer);
  return 0;
}

int dump_attribute(xs_schema *s, xmlDocPtr doc, void *data, int post, xs_attribute *a)
{
  dumpinfo *di = (dumpinfo*)data;
  if (post) {
    dump_dec_indent(di);
    return 0;
  }

  assert(a->type);

  dump_printf(di,"Attribute %#n\n",a->def.ident);

  if (a->toplevel)
    dump_value_constraint(di,&a->vc);

  dump_reference(di,"Type",a->typeref);

  /* FIXME */

  dump_inc_indent(di);
  return 0;
}

int dump_attribute_use(xs_schema *s, xmlDocPtr doc, void *data, int post, xs_attribute_use *au)
{
  dumpinfo *di = (dumpinfo*)data;
  if (post) {
    dump_dec_indent(di);
    return 0;
  }

  assert(au->attribute);

  dump_printf(di,"Attribute use\n");
  dump_printf(di,"  Required: %s\n",au->required ? "true" : "false");

  dump_value_constraint(di,&au->vc);

  dump_reference(di,"Attribute",au->ref);

  dump_inc_indent(di);
  return 0;
}

void output_xmlschema(FILE *f, xs_schema *s)
{
  xmlOutputBuffer *buf = xmlOutputBufferCreateFile(f,NULL);
  xmlTextWriter *writer = xmlNewTextWriter(buf);
  xs_visitor v;
  list *l;
  memset(&v,0,sizeof(xs_visitor));

  xmlTextWriterSetIndent(writer,1);
  xmlTextWriterSetIndentString(writer,"  ");
  xmlTextWriterStartDocument(writer,NULL,NULL,NULL);
  xmlTextWriterStartElement(writer,"xsd:schema");
  for (l = s->globals->namespaces->defs; l; l = l->next) {
    ns_def *ns = (ns_def*)l->data;
    /* Hide the xdt namespace since it is only applicable for xpath */
    if (strcmp(ns->href,XDT_NAMESPACE)) {
      char *attrname = (char*)malloc(strlen("xmlns:")+strlen(ns->prefix)+1);
      sprintf(attrname,"xmlns:%s",ns->prefix);
      xmlTextWriterWriteAttribute(writer,attrname,ns->href);
      free(attrname);
    }
  }
  if (s->ns)
    xmlTextWriterWriteAttribute(writer,"targetNamespace",s->ns);
  if (s->attrformq)
    xmlTextWriterWriteAttribute(writer,"attributeFormDefault","qualified");
  if (s->elemformq)
    xmlTextWriterWriteAttribute(writer,"elementFormDefault","qualified");

  for (l = s->imports; l; l = l->next) {
    xs_schema *import = (xs_schema*)l->data;
    char *rel = get_relative_uri(import->uri,s->uri);
    xmlTextWriterStartElement(writer,"xsd:import");
    if (NULL != import->ns)
      xmlTextWriterWriteAttribute(writer,"namespace",import->ns);
    xmlTextWriterWriteAttribute(writer,"schemaLocation",rel);
    xmlTextWriterEndElement(writer);
    free(rel);
  }

  v.visit_type = output_type;
  v.visit_element = output_element;
  v.visit_particle = output_particle;
  v.visit_model_group_def = output_model_group_def;
  v.visit_attribute = output_attribute;
  v.visit_attribute_use = output_attribute_use;
  v.visit_attribute_group = output_attribute_group;
  v.visit_attribute_group_ref = output_attribute_group_ref;
  v.once_each = 1;

  xs_visit_schema(s,NULL,writer,&v);

  xmlTextWriterEndElement(writer);
  xmlTextWriterEndDocument(writer);
  xmlTextWriterFlush(writer);
  xmlFreeTextWriter(writer);
}

void dump_xmlschema(FILE *f, xs_schema *s)
{
  dumpinfo di;
  xs_visitor v;
  memset(&v,0,sizeof(xs_visitor));

  di.f = f;
  di.indent = 0;
  di.in_builtin_type = 0;
  di.found_non_builtin = 0;

  v.visit_schema = dump_schema;
  v.visit_type = dump_type;
  v.visit_element = dump_element;
  v.visit_model_group = dump_model_group;
  v.visit_wildcard = dump_wildcard;
  v.visit_particle = dump_particle;
  v.visit_model_group_def = dump_model_group_def;
  v.visit_attribute = dump_attribute;
  v.visit_attribute_use = dump_attribute_use;
  v.visit_attribute_group = dump_attribute_group;
  v.visit_attribute_group_ref = dump_attribute_group_ref;

  xs_visit_schema(s,NULL,&di,&v);
}
