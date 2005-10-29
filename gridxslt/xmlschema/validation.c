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

#include "validation.h"
#include <stdio.h>
#include <assert.h>
#include <stdlib.h>
#include <math.h>
#include <string.h>
#include <signal.h>
#include <unistd.h>
#include <argp.h>
#include <sys/types.h>
#include <libxml/tree.h>

using namespace GridXSLT;

typedef struct childencinfo childencinfo;
struct childencinfo {
  xmlNodePtr n;
  Type *type;
  int datapos;
};

typedef struct stringencinfo stringencinfo;
class stringencinfo {
public:
  stringencinfo() : datapos(0) { }
  String str;
  int datapos;
};

void init_typeinfo_particle(Schema *s, Particle *p, list **typestack, list **mgstack,
                            int pos, int *size);
void init_typeinfo_model_group(Schema *s, ModelGroup *mg, list **typestack, list **mgstack);

int validate_element_against_particle(xs_validator *v,
                                      xmlNodePtr *n, int *lineno, Particle *p,
                                      stringbuf *encoded, int pos);

static String xs_alloc_cname(BuiltinTypes *g, int useprefix, List<String> &cnames,
                            List<String> &cnames2,
                            const String &name, const String &ns)
{
  String cname;
  String base;
  int num = 1;
  String fixedname;
  String prefix;

  if (useprefix) {
    if (!ns.isNull()) {
      Iterator<ns_def*> nsit;
      for (nsit = g->namespaces->defs; nsit.haveCurrent(); nsit++) {
        ns_def *xns = *nsit;
        if ((xns->href == ns) && (!xns->prefix.isNull())) {
          prefix = xns->prefix.toUpper();
          break;
        }
      }
    }
    if (prefix.isNull())
      prefix = "XML";
  }
  else {
    prefix = "";
  }

  if (!name.isNull()) {
    const Char *chars = name.chars();
    unsigned int len = name.length();

    Char *newchars = new Char[len];
    for (unsigned int i = 0; i < len; i++) {
      if ((255 >= chars[i].c) && !isalnum(chars[i].c))
        newchars[i].c = '_';
      else
        newchars[i].c = chars[i].c;
    }

    fixedname = new StringImpl(newchars,len);

    base = fixedname;
    cname = String::format("%*%*",&prefix,&fixedname);
  }
  else {
    base = "";
    cname = String::format("%*0",&prefix);
  }
  while (cnames.contains(cname) || (cnames2.contains(cname)))
    cname = String::format("%*%*%d",&prefix,&base,num++);

  cnames.append(cname);
  return cname;
}

void init_typeinfo_attributes(list *attribute_uses, list *attribute_group_refs, int *size)
{
  list *l;
  for (l = attribute_uses; l; l = l->next) {
    AttributeUse *au = (AttributeUse*)l->data;
    assert(!au->typeinfo_known);
    assert(au->attribute->type);
    assert(au->attribute->type->typeinfo_known);
    au->size = au->attribute->type->size;
    au->pos = *size;
    *size += au->size;
  }

  for (l = attribute_group_refs; l; l = l->next) {
    AttributeGroupRef *agr = (AttributeGroupRef*)l->data;
    assert(!agr->typeinfo_known);
    assert(agr->ag);
    assert(agr->ag->typeinfo_known);
    agr->size = agr->ag->size;
    agr->pos = *size;
    *size += agr->size;
  }

}

void init_typeinfo_attribute_group(Schema *s, AttributeGroup *ag)
{
  list *l;
  CStruct *cs;
  if (ag->typeinfo_known)
    return;

  for (l = ag->attribute_group_refs; l; l = l->next) {
    AttributeGroupRef *agr = (AttributeGroupRef*)l->data;
    if (!agr->ag->typeinfo_known)
      init_typeinfo_attribute_group(s,agr->ag);
  }

  ag->size = 0;

  init_typeinfo_attributes(ag->local_attribute_uses,ag->attribute_group_refs,&ag->size);

  cs = new CStruct(s->globals->as);
  cs->type = CSTRUCT_ATTRIBUTE_GROUP;
  cs->object.ag = ag;
  list_append(&s->globals->cstructs,cs);

  ag->typeinfo_known = 1;
}

void init_typeinfo_simple_type(Schema *s, Type *t)
{
  assert(!t->complex);
  if (t->typeinfo_known)
    return;

  if (!t->base->typeinfo_known)
    init_typeinfo_simple_type(s,t->base);

  assert(0 != t->base->size);
  t->size = t->base->size;

  t->typeinfo_known = 1;
}

void init_typeinfo_complex_type(Schema *s, Type *t, list **typestack, list **mgstack)
{
  Type *t2;
  CStruct *cs;
  if (t->typeinfo_known || t->builtin)
    return;
  assert(t->complex);

  assert(!list_contains_ptr(*typestack,t));
  list_push(typestack,t);

/*   message("init_typeinfo_complex_type %p %s (%s)\n",t,t->name,t->ctype); */

  t->size = 0;

  /* base type (only for extension of a complex type) */
  /* FIXME: don't really want to do this in the case of simple content */
  if ((t->base != s->globals->complex_ur_type) &&
      t->base->complex) {
    assert(!list_contains_ptr(*typestack,t->base));
    init_typeinfo_complex_type(s,t->base,typestack,mgstack);
    /* if it's a simple type, it should already have been initialized */
    assert(t->base->typeinfo_known);
    t->size += t->base->size;
  }

  /* content model */
  if (NULL != t->effective_content) {
    int psize = 0;
    init_typeinfo_particle(s,t->effective_content,typestack,mgstack,t->size,&psize);
    t->size += psize;
  }

  /* attributes and attribute group references */
  init_typeinfo_attributes(t->local_attribute_uses,t->attribute_group_refs,&t->size);

  t2 = (Type*)list_pop(typestack);
  assert(t2 == t);

  cs = new CStruct(s->globals->as);
  cs->type = CSTRUCT_TYPE;
  cs->object.t = t;
  list_append(&s->globals->cstructs,cs);

  t->typeinfo_known = 1;
}

void init_typeinfo_particle(Schema *s, Particle *p, list **typestack, list **mgstack,
                            int pos, int *size)
{
  int force_ptr = 0;
  assert(!p->typeinfo_known);
  p->base_size = 0;
  switch (p->term_type) {
  case PARTICLE_TERM_ELEMENT:
    if (!p->term.e->type->typeinfo_known) {
      if (list_contains_ptr(*typestack,p->term.e->type))
        force_ptr = 1;
      else
        init_typeinfo_complex_type(s,p->term.e->type,typestack,mgstack);
    }

    p->base_size = p->term.e->type->size;
    break;
  case PARTICLE_TERM_MODEL_GROUP:
    if (list_contains_ptr(*mgstack,p->term.mg)) {
      force_ptr = 1;
      p->base_size = 0;
    }
    else {
      init_typeinfo_model_group(s,p->term.mg,typestack,mgstack);
      assert(p->term.mg->typeinfo_known);
      p->base_size = p->term.mg->size;
    }
    break;
  case PARTICLE_TERM_WILDCARD:
    /* FIXME: what to do here? */
    break;
  default:
    assert(!"invalid particle type");
    break;
  }

  if ((0 > p->range.max_occurs) || force_ptr) {
    /* Particle encoding type 2: Array size and pointer to contents */
    p->enctype = XS_ENCODING_ARRAY_PTR;
    p->size = sizeof(IMPL_ARRAYSIZE) + sizeof(IMPL_POINTER);
  }
  else if ((1 == p->range.min_occurs) && (1 == p->range.max_occurs)) {
    /* Particle encoding type 1: Single instance of the particle stored directly in the parent's
       structure */
    p->enctype = XS_ENCODING_SINGLE;
    p->size = p->base_size;
  }
  else if (p->range.min_occurs == p->range.max_occurs) {
    /* Particle encoding type 3: Fixed number of instances of the particle stored directly in the
       parent's structure */
    p->enctype = XS_ENCODING_FIXED_ARRAY;
    p->size = p->range.max_occurs*p->base_size;
  }
  else {
    /* Particle encoding type 4: maxOccurs is a positive integer... store array size and actual
       array in place */
    p->enctype = XS_ENCODING_MAX_ARRAY;
    p->size = sizeof(IMPL_ARRAYSIZE) + p->range.max_occurs*p->base_size;
  }

  p->typeinfo_known = 1;

  p->pos = pos;
  *size = p->size;
}

void init_typeinfo_model_group(Schema *s, ModelGroup *mg, list **typestack, list **mgstack)
{
  list *l;
  ModelGroup *mg2;
  CStruct *cs;
  int largest = 0;
  if (mg->typeinfo_known)
    return;

  assert(!list_contains_ptr(*mgstack,mg));
  list_push(mgstack,mg);

  mg->size = 0;

  if (MODELGROUP_COMPOSITOR_CHOICE == mg->compositor) {
    /* extra int at the start indicating which option is selected */
    mg->size += sizeof(IMPL_INT);
  }

/*   message("init_typeinfo_model_group %p pn %s (ct %s)\n",mg,mg->parent_name,mg->ctype); */
  for (l = mg->particles; l; l = l->next) {
    Particle *p = (Particle*)l->data;
    int psize;
    init_typeinfo_particle(s,p,typestack,mgstack,mg->size,&psize);

    if (MODELGROUP_COMPOSITOR_CHOICE == mg->compositor) {
      if (largest < p->size)
        largest = psize;
    }
    else {
      mg->size += psize;
    }

  }

  if (MODELGROUP_COMPOSITOR_CHOICE == mg->compositor)
    mg->size += largest;

  mg2 = (ModelGroup*)list_pop(mgstack);
  assert(mg2 == mg);

  cs = new CStruct(s->globals->as);
  cs->type = CSTRUCT_MODEL_GROUP;
  cs->object.mg = mg;
  list_append(&s->globals->cstructs,cs);

  mg->typeinfo_known = 1;
}

void xs_init_cnames_mg(Schema *s, ModelGroup *mg, int indent)
{
  list *l;
  List<String> empty;
  assert(!mg->ctype.isNull());
  assert(!mg->parent_name.isNull());
  for (l = mg->particles; l; l = l->next) {
    Particle *p = (Particle*)l->data;
    assert(p->cvar.isNull());
    switch (p->term_type) {
    case PARTICLE_TERM_ELEMENT:
      p->cvar = xs_alloc_cname(s->globals,0,mg->cvars,empty,
                               p->term.e->def.ident.m_name,
                               p->term.e->def.ident.m_ns);
      break;
    case PARTICLE_TERM_WILDCARD:
      p->cvar = xs_alloc_cname(s->globals,0,mg->cvars,empty,"any",String::null());
      break;
    case PARTICLE_TERM_MODEL_GROUP:
      if (p->ref) {
        assert(!p->term.mg->parent_name.isNull());
        p->cvar = xs_alloc_cname(s->globals,0,mg->cvars,empty,
                                   p->term.mg->parent_name,p->term.mg->parent_ns);
      }
      else {
        switch (p->term.mg->compositor) {
        case MODELGROUP_COMPOSITOR_ALL:
          p->cvar = xs_alloc_cname(s->globals,0,mg->cvars,empty,"all",String::null());
          break;
        case MODELGROUP_COMPOSITOR_CHOICE:
          p->cvar = xs_alloc_cname(s->globals,0,mg->cvars,empty,"choice",String::null());
          break;
        case MODELGROUP_COMPOSITOR_SEQUENCE:
          p->cvar = xs_alloc_cname(s->globals,0,mg->cvars,empty,"sequence",String::null());
          break;
        default:
          assert(!"invalid model group compositor");
          break;
        }
        assert(p->term.mg->ctype.isNull());
        assert(p->term.mg->parent_name.isNull());
        p->term.mg->ctype = xs_alloc_cname(s->globals,1,s->globals->ctypes,empty,
                                           mg->parent_name,mg->parent_ns);
/*         message("model group %s\n",p->term.mg->cname); */
        p->term.mg->parent_name = mg->parent_name;
        p->term.mg->parent_ns = mg->parent_ns;
        xs_init_cnames_mg(s,p->term.mg,indent+1);
      }
      break;
    default:
      assert(!"invalid particle term type");
      break;
    }
  }
}

void xs_init_attribute_cnames(Schema *s, list *attribute_uses, list *attribute_group_refs,
                              List<String> &cnames, List<String> &cnames2)
{
  list *l;
  for (l = attribute_uses; l; l = l->next) {
    AttributeUse *au = (AttributeUse*)l->data;
    assert(au->cvar.isNull());
    au->cvar = xs_alloc_cname(s->globals,0,cnames,cnames2,
                              au->attribute->def.ident.m_name,
                              au->attribute->def.ident.m_ns);
  }

  for (l = attribute_group_refs; l; l = l->next) {
    AttributeGroupRef *agr = (AttributeGroupRef*)l->data;
    assert(agr->cvar.isNull());
    agr->cvar = xs_alloc_cname(s->globals,0,cnames,cnames2,
                               agr->ag->def.ident.m_name,
                               agr->ag->def.ident.m_ns);
  }
}

void xs_init_cnames(Schema *s)
{
  list *l;
  symbol_space_entry *sse;
  List<String> empty;

  for (l = s->imports; l; l = l->next)
    xs_init_cnames((Schema*)l->data);

  /* Objects that must have cnames assigned: model groups, types, particles */

  /* Inbuilt types: These all have the prefix "XSD" and then the type name */
  /* FIXME: set these when we create the actual builtin types */
  Iterator<Type*> tit;
  for (tit = s->as->alloc_type; tit.haveCurrent(); tit++) {
    Type *t = *tit;
    /* should already be allocated... */
    if (t->builtin) {
      assert(!t->ctype.isNull());
    }
  }

  /* Attribute groups (these are always top-level */
  for (sse = s->symt->ss_attribute_groups->entries; sse; sse = sse->next) {
    AttributeGroup *ag = (AttributeGroup*)sse->object;
    ag->ctype = xs_alloc_cname(s->globals,1,s->globals->ctypes,empty,
                               ag->def.ident.m_name,ag->def.ident.m_ns);
  }

  /* Top-level model groups: use the name of the model group definition */
  for (sse = s->symt->ss_model_group_defs->entries; sse; sse = sse->next) {
    ModelGroupDef *mgd = (ModelGroupDef*)sse->object;
    mgd->model_group->ctype = xs_alloc_cname(s->globals,1,s->globals->ctypes,empty,
                                             mgd->def.ident.m_name,
                                             mgd->def.ident.m_ns);
    mgd->model_group->parent_name = mgd->def.ident.m_name;
    mgd->model_group->parent_ns = mgd->def.ident.m_ns;
  }

  /* Top-level type declarations: use the name of the type, or the base custom cname for simple
     types (if there is one) */
  for (sse = s->symt->ss_types->entries; sse; sse = sse->next) {
    Type *t = (Type*)sse->object;
    if (!t->complex && t->base->custom_ctype)
      t->ctype = t->base->ctype;
    else if (t->builtin)
      t->ctype = xs_alloc_cname(s->globals,1,s->globals->ctypes,empty,
                                t->def.ident.m_name,t->def.ident.m_ns);
    else
      t->ctype = xs_alloc_cname(s->globals,1,s->globals->ctypes,empty,
                                t->def.ident.m_name,t->def.ident.m_ns);
    t->parent_name = t->def.ident.m_name;
    t->parent_ns = t->def.ident.m_ns;
  }

  /* Anonymous type declarations inside <element>: use the name of the element */
  Iterator<SchemaElement*> eit;
  for (eit = s->as->alloc_element; eit.haveCurrent(); eit++) {
    SchemaElement *e = *eit;
    if ((NULL == e->typeref) && ((NULL == e->sghead) || (e->type != e->sghead->type)))  {
      if (!e->type->ctype.isNull()) {
        message("type cname already set! %* (name=%*,element=%*)\n",
               &e->type->ctype,&e->type->def.ident.m_name,&e->def.ident.m_name);
      }
      assert(e->type->ctype.isNull());
      assert(e->type->parent_name.isNull());
      e->type->ctype = xs_alloc_cname(s->globals,1,s->globals->ctypes,empty,
                                      e->def.ident.m_name,e->def.ident.m_ns);
      e->type->parent_name = e->def.ident.m_name;
      e->type->parent_ns = e->def.ident.m_ns;
    }
  }

  /* Simple type declarations within a <simpleType> (i.e. all types which have not yet been assigned
     a name) - generate numerically */
  for (tit = s->as->alloc_type; tit.haveCurrent(); tit++) {
    Type *t = *tit;
    if (t->ctype.isNull()) {
      assert(!t->complex);
      t->ctype = xs_alloc_cname(s->globals,1,s->globals->ctypes,empty,String::null(),String::null());
    }
  }

  /* Model group definitions that are the content model of a type declaration - these take the
     name of the type */
  for (tit = s->as->alloc_type; tit.haveCurrent(); tit++) {
    Type *t = *tit;
    if (t->complex && t->content_type) {
      assert(!t->ctype.isNull());
      assert(!t->parent_name.isNull()); /* FIXME: this is not set for anon simpletypes */
      assert(PARTICLE_TERM_MODEL_GROUP == t->content_type->term_type);

      t->content_type->cvar = "(content model)";
      if (NULL == t->content_type->ref) {
        /* content model is an anonymous group; assign it a cname */
        t->content_type->term.mg->ctype = String::format("CM_%*",&t->ctype);

        t->content_type->term.mg->parent_name = t->parent_name;
        t->content_type->term.mg->parent_ns = t->parent_ns;
      }

      if ((NULL != t->effective_content) && 
          (t->effective_content != t->content_type)) {
        assert(PARTICLE_TERM_MODEL_GROUP == t->effective_content->term_type);
        t->effective_content->cvar = "(content model)";
        if (NULL == t->effective_content->ref) {
          /* effective content model is an anonymous group; assign it a cname */
          t->effective_content->term.mg->ctype = String::format("ECM_%*",&t->ctype);
          t->effective_content->term.mg->parent_name = t->parent_name;
          t->effective_content->term.mg->parent_ns = t->parent_ns;
        }
      }
    }
    else {
      assert(NULL == t->content_type);
      assert(NULL == t->effective_content);
    }
  }

  /* Particles - must be done recursively from model groups and type declarations. Anonymous groups
     may reside within these; these inherit their names from the parent model group or type */
  for (sse = s->symt->ss_model_group_defs->entries; sse; sse = sse->next) {
    ModelGroupDef *mgd = (ModelGroupDef*)sse->object;
    xs_init_cnames_mg(s,mgd->model_group,1);
  }

  for (tit = s->as->alloc_type; tit.haveCurrent(); tit++) {
    Type *t = *tit;
    if (t->complex && t->effective_content) {
      assert(PARTICLE_TERM_MODEL_GROUP == t->effective_content->term_type);
      if (NULL == t->effective_content->ref)
        xs_init_cnames_mg(s,t->effective_content->term.mg,1);
    }
    if (t->complex) {
      List<String> cnames2;
      if (NULL != t->effective_content) {
        assert(PARTICLE_TERM_MODEL_GROUP == t->effective_content->term_type);
        cnames2 = t->effective_content->term.mg->cvars;
      }

      xs_init_attribute_cnames(s,t->local_attribute_uses,t->attribute_group_refs,
                               t->attribute_cvars,cnames2);
    }
  }

  /* Attribute groups */
  Iterator<AttributeGroup*> agit;
  for (agit = s->as->alloc_attribute_group; agit.haveCurrent(); agit++) {
    AttributeGroup *ag = *agit;
    xs_init_attribute_cnames(s,ag->local_attribute_uses,ag->attribute_group_refs,ag->cvars,empty);
  }

  for (tit = s->as->alloc_type; tit.haveCurrent(); tit++)
    assert(!(*tit)->ctype.isNull());
  Iterator<ModelGroup*> mgit;
  for (mgit = s->as->alloc_model_group; mgit.haveCurrent(); mgit++)
    assert(!(*mgit)->ctype.isNull());
  Iterator<Particle*> pit;
  for (pit = s->as->alloc_particle; pit.haveCurrent(); pit++)
    assert(!(*pit)->cvar.isNull());
}





void xs_init_typeinfo_simpletypes(Schema *s)
{
  list *l;

  for (l = s->imports; l; l = l->next)
    xs_init_typeinfo_simpletypes((Schema*)l->data);

  Iterator<Type*> tit;
  for (tit = s->as->alloc_type; tit.haveCurrent(); tit++) {
    Type *t = *tit;
    if (!t->complex)
      init_typeinfo_simple_type(s,t);
  }
}


void xs_init_typeinfo_attribute_groups(Schema *s)
{
  list *l;

  for (l = s->imports; l; l = l->next)
    xs_init_typeinfo_attribute_groups((Schema*)l->data);

  Iterator<AttributeGroup*> agit;
  for (agit = s->as->alloc_attribute_group; agit.haveCurrent(); agit++) {
    AttributeGroup *ag = *agit;
    init_typeinfo_attribute_group(s,ag);
  }
}



void xs_init_typeinfo_others(Schema *s)
{
  list *l;
  symbol_space_entry *sse;

  for (l = s->imports; l; l = l->next)
    xs_init_typeinfo_others((Schema*)l->data);

  /* top-level model groups */
  for (sse = s->symt->ss_model_group_defs->entries; sse; sse = sse->next) {
    ModelGroupDef *mgd = (ModelGroupDef*)sse->object;
    list *typestack = NULL;
    list *mgstack = NULL;
    if (!mgd->model_group->typeinfo_known)
      init_typeinfo_model_group(s,mgd->model_group,&typestack,&mgstack);
  }

  /* complex types */
  Iterator<Type*> tit;
  for (tit = s->as->alloc_type; tit.haveCurrent(); tit++) {
    Type *t = *tit;
    list *typestack = NULL;
    list *mgstack = NULL;
    if (t->complex && !t->typeinfo_known)
      init_typeinfo_complex_type(s,t,&typestack,&mgstack);
  }


}

void GridXSLT::xs_init_typeinfo(Schema *s)
{
  xs_init_typeinfo_simpletypes(s);
  xs_init_typeinfo_attribute_groups(s);
  xs_init_typeinfo_others(s);
}

xs_validator *GridXSLT::xs_validator_new(Schema *s)
{
  xs_validator *v = (xs_validator*)calloc(1,sizeof(xs_validator));
  v->s = s;

  xs_init_cnames(s);
  xs_init_typeinfo(s);

  return v;
}

void xs_print_model_group_defs(ModelGroup *mg)
{
  int choice = (MODELGROUP_COMPOSITOR_CHOICE == mg->compositor);
  list *l;

  assert(mg->typeinfo_known);
  if (choice) {
    message("  int valtype;\n");
    message("  union {\n");
  }

/*   message("model group pn %s (ct %s)\n",mg->parent_name,mg->ctype); */
  for (l = mg->particles; l; l = l->next) {
    Particle *p = (Particle*)l->data;
    const char *indent = choice ? "  " : "";
    String typenam = "(type)";
    String name = "(name)";
    assert(p->typeinfo_known);
    switch (p->term_type) {
    case PARTICLE_TERM_ELEMENT:
      assert(p->term.e->type);
      assert(!p->term.e->type->ctype.isNull());
      assert(!p->cvar.isNull());
      typenam = p->term.e->type->ctype;
      name = p->cvar;
      break;
    case PARTICLE_TERM_MODEL_GROUP:
      name = p->cvar;
      typenam = p->term.mg->ctype;
      break;
    case PARTICLE_TERM_WILDCARD:
      name = p->cvar;
      typenam = "wildcard"; /* FIXME: what to do here? */
      break;
    default:
      assert(!"invalid particle type");
      break;
    }

    switch (p->enctype) {
    case XS_ENCODING_SINGLE:
      message("%s  %* %*;\n",indent,&typenam,&name);
      break;
    case XS_ENCODING_ARRAY_PTR:
      message("%s  int n%*;\n",indent,&name);
      message("%s  %* *%*;\n",indent,&typenam,&name);
      break;
    case XS_ENCODING_FIXED_ARRAY:
      message("%s  %* %*[%d];\n",indent,&typenam,&name,p->range.max_occurs);
      break;
    case XS_ENCODING_MAX_ARRAY:
      message("%s  int n%*;\n",indent,&name);
      message("%s  %* %*[%d];\n",indent,&typenam,&name,p->range.max_occurs);
      break;
    default:
      assert(!"invalid particle encoding type");
      break;
    }
  }
  if (choice) {
    message("  } val;\n");
  }
}

void xs_print_attribute_defs(list *attribute_uses, list *attribute_group_refs)
{
  list *l;
  for (l = attribute_uses; l; l = l->next) {
    AttributeUse *au = (AttributeUse*)l->data;
    assert(!au->cvar.isNull());
    assert(!au->attribute->type->ctype.isNull());
    message("  %* %*;\n",&au->attribute->type->ctype,&au->cvar);
  }

  for (l = attribute_group_refs; l; l = l->next) {
    AttributeGroupRef *agr = (AttributeGroupRef*)l->data;
    assert(!agr->cvar.isNull());
    message("  %* %*;\n",&agr->ag->ctype,&agr->cvar);
  }
}

void xs_print_mg_choices(ModelGroup *mg, const String &ctype)
{
  if (MODELGROUP_COMPOSITOR_CHOICE == mg->compositor) {
    list *first_ew = NULL;
    list *l;
    int choice = 1;
    for (l = mg->particles; l; l = l->next) {
      Particle *p = (Particle*)l->data;
      xs_get_first_elements_and_wildcards(p,&first_ew);
      assert(!p->cvar.isNull());
      message("#define %*_%* %d\n",&ctype,&p->cvar,choice++);
    }
    list_free(first_ew,NULL);
    message("\n");
  }
}

void GridXSLT::xs_print_defs(Schema *s)
{
  list *l;

  for (l = s->globals->cstructs; l; l = l->next) {
    CStruct *cs = (CStruct*)l->data;
    switch (cs->type) {
    case CSTRUCT_TYPE:
      message("typedef struct %* %*;\n",&cs->object.t->ctype,&cs->object.t->ctype);
      break;
    case CSTRUCT_MODEL_GROUP:
      if (NULL == cs->object.mg->content_model_of)
        message("typedef struct %* %*;\n",&cs->object.mg->ctype,&cs->object.mg->ctype);
      break;
    case CSTRUCT_ATTRIBUTE_GROUP:
      message("typedef struct %* %*;\n",&cs->object.ag->ctype,&cs->object.ag->ctype);
      break;
    default:
      assert(!"invalid cstruct type");
      break;
    }
  }

  message("\n");

  for (l = s->globals->cstructs; l; l = l->next) {
    CStruct *cs = (CStruct*)l->data;
    switch (cs->type) {
    case CSTRUCT_TYPE:
      if (cs->object.t->complex) {
        Type *t = cs->object.t;
        if (NULL != t->effective_content) {
          assert(PARTICLE_TERM_MODEL_GROUP == t->effective_content->term_type);
          xs_print_mg_choices(t->effective_content->term.mg,t->ctype);
        }

        message("struct %* {\n",&t->ctype);
        if ((t->base != s->globals->complex_ur_type) &&
            t->base->complex) {
          message("  %* base;\n",&t->base->ctype);
        }

        if (NULL != t->effective_content)
          xs_print_model_group_defs(t->effective_content->term.mg);

        xs_print_attribute_defs(t->local_attribute_uses,t->attribute_group_refs);
        message("} __attribute__((__packed__));\n");
        message("/* complex type - %d bytes */\n",t->size);
        message("\n");
      }
      break;
    case CSTRUCT_MODEL_GROUP:
      if (NULL == cs->object.mg->content_model_of) {
        ModelGroup *mg = cs->object.mg;
        xs_print_mg_choices(mg,mg->ctype);
        message("struct %* {\n",&mg->ctype);
        xs_print_model_group_defs(mg);
        message("} __attribute__((__packed__));\n");
        message("/* model group - %d bytes */\n",mg->size);
        message("\n");
      }
      break;
    case CSTRUCT_ATTRIBUTE_GROUP: {
      AttributeGroup *ag = cs->object.ag;
      message("struct %* {\n",&ag->ctype);
      xs_print_attribute_defs(ag->local_attribute_uses,ag->attribute_group_refs);
      message("} __attribute__((__packed__));\n");
      message("/* attribute group - %d bytes */\n",ag->size);
      message("\n");
      break;
    }
    default:
      assert(!"invalid cstruct type");
      break;
    }
  }
}

void GridXSLT::xs_validator_free(xs_validator *v)
{
  v->ei.clear();
  free(v);
}





























void skip_text(xmlNodePtr *n)
{
  while (*n && (XML_ELEMENT_NODE != (*n)->type)) /* FIXME: don't just skip text! */
    (*n) = (*n)->next;
}

void next_element(xmlNodePtr *n)
{
  (*n) = (*n)->next;
  skip_text(n);
}



void get_particle_pattern(stringbuf *buf, Particle *p)
{
  int inbrackets = 0;
  if (((1 != p->range.min_occurs) || (1 != p->range.max_occurs)) &&
      (PARTICLE_TERM_MODEL_GROUP == p->term_type)) {
    stringbuf_format(buf,"(");
    inbrackets = 1;
  }

  switch (p->term_type) {
  case PARTICLE_TERM_ELEMENT: {
    stringbuf_format(buf,"%*",&p->term.e->def.ident.m_name);
    break;
  }
  case PARTICLE_TERM_MODEL_GROUP:
    switch (p->term.mg->compositor) {
    case MODELGROUP_COMPOSITOR_ALL:
      assert(!"not implemented");
      break;
    case MODELGROUP_COMPOSITOR_CHOICE: {
      list *l;
      if (!inbrackets)
        stringbuf_format(buf,"(");
      for (l = p->term.mg->particles; l; l = l ->next) {
        Particle *p2 = (Particle*)l->data;
        get_particle_pattern(buf,p2);
        if (l->next)
          stringbuf_format(buf,"|");
      }
      if (!inbrackets)
        stringbuf_format(buf,")");
      break;
    }
    case MODELGROUP_COMPOSITOR_SEQUENCE: {
      list *l;
      for (l = p->term.mg->particles; l; l = l ->next) {
        Particle *p2 = (Particle*)l->data;
        get_particle_pattern(buf,p2);
        if (l->next)
          stringbuf_format(buf," ");
      }
      break;
    }
    default:
      assert(!"not implemented");
      break;
    }
    break;
  case PARTICLE_TERM_WILDCARD:
    assert(!"not yet implemented");
    break;
  default:
    assert(!"invalid particle type");
    break;
  }
  if ((1 != p->range.min_occurs) || (1 != p->range.max_occurs)) {
    if (PARTICLE_TERM_MODEL_GROUP == p->term_type)
      stringbuf_format(buf,")");

    if (p->range.min_occurs == p->range.max_occurs)
      stringbuf_format(buf,"{%d}",p->range.min_occurs,p->range.max_occurs);
    else if ((0 == p->range.min_occurs) && (1 == p->range.max_occurs))
      stringbuf_format(buf,"?",p->range.min_occurs,p->range.max_occurs);
    else if (0 <= p->range.max_occurs)
      stringbuf_format(buf,"{%d,%d}",p->range.min_occurs,p->range.max_occurs);
    else if (0 == p->range.min_occurs)
      stringbuf_format(buf,"*",p->range.min_occurs,p->range.max_occurs);
    else if (1 == p->range.min_occurs)
      stringbuf_format(buf,"+",p->range.min_occurs,p->range.max_occurs);
    else
      stringbuf_format(buf,"{%d,}",p->range.min_occurs,p->range.max_occurs);
  }
}

/**
 * Validate and (optionally) encode the contents of an element using the given type. For complex
 * content, this recursively processes all of the children according to the particles of the model
 * group corresponding to the type's content type.
 *
 * @param v           Validator
 *
 * @param n           The element to validate
 *
 * @param encoded     If non-null, the content of the element will be encoded in binary format
 *                    according to the schema
 *
 * @param t           The type to use when validating/encoding th element
 *
 * @param lineno      Pointer to the current line number. Upon return from this function, this will
 *                    be set to the line number of the last valid descendant element found
 *
 * @param posinparent If encoded is non-null, this must be set to the position in the encoded
 *                    buffer at which this element's in-place data should be written. The buffer
 *                    may be expected for out-of-place data such as strings and arrays.
 *
 * @return            0 If the element was completely valid, or -1 if some or all of the elements
 *                    contents were invalid, in which case the inv* fields in v will be set
 *                    according to the validation error
 */
int validate_element_children(xs_validator *v, xmlNodePtr n, stringbuf *encoded,
                              Type *t, int *lineno, int pos)
{
  xmlNodePtr c;
  int r;

  c = n->children;
  skip_text(&c);
  if (NULL != c)
    *lineno = c->line;

  r = validate_element_against_particle(v,&c,lineno,t->content_type,encoded,pos);
/*   message("validating children of %s: r = %d\n",n->name,r); */
  if (1 != r) {
    return -1;
  }

  if (NULL != c) {
    v->inv_type = XS_VALIDATOR_INV_NOTHING_MORE_ALLOWED;
    v->inv_lineno = *lineno;
    return -1;
  }

  return 0;
}

/**
 * Given an element and a particle, check that the contents of the element matches the type
 * information corresponding to the particle, including occurance constraints. Validation stops
 * after p->range.max_occurs occurrances are found.
 *
 * @param v           Validator
 *
 * @param n           Pointer to the current node. Upon return from this function, this will point
 *                    to the first node *after* the last valid occurrance.
 *
 * @param lineno      Pointer to the current line number. Upon return from this function, this will
 *                    be set to the line number of the last valid element found (possibly a
 *                    descendent of the current element)
 *
 * @param p           Particle against which the element is to be checked
 *
 * @param encoded     If non-null, the content of the element will be encoded in binary format
 *                    according to the schema
 *
 * @param posinparent If encoded is non-null, this must be set to the position in the encoded
 *                    buffer at which this element's in-place data should be written. The buffer
 *                    may be expected for out-of-place data such as strings and arrays.
 *
 * @return            The number of valid occurrances of the element found. If this is less than
 *                    p->range.min_occurs, it means that the element (overall) was not valid. The return
 *                    value will be no more than p->range.max_occurs.
 */
int validate_element_against_particle(xs_validator *v,
                                      xmlNodePtr *n, int *lineno, Particle *p,
                                      stringbuf *encoded, int posinparent)
{
  list *l;
  int count = 0;
  list *children = NULL;
  list *strings = NULL;
  childencinfo *c;
  int finished = 0;
  int datapos = 0;

  if (!p->typeinfo_known) {
    message("typeinfo of particle %p is not known\n",p);
    message("p->extension_for = %p\n",p->extension_for);
  }

  assert(p->typeinfo_known);

  switch (p->enctype) {
  case XS_ENCODING_SINGLE:
    /* Single occurance of this element. Store data within the memory allocated by the parent,
       starting at posinparent. */
    datapos = posinparent;
    break;
  case XS_ENCODING_ARRAY_PTR: {
    /* Unbounded number of occurrances of this element. In the memory allocated by the parent, we
       store the number of occurrances and a pointer to the array. The actual data gets written
       to newly allocated memory starting at arraypos. Note that we don't actually allocate any
       new memory here; this is done as we find new occurrances (at which time count also gets
       updated). */
    IMPL_POINTER arraypos = encoded->size-1-posinparent;
    IMPL_INT count2 = 0;
    memcpy(encoded->data+posinparent,&count2,sizeof(IMPL_INT));
    memcpy(encoded->data+posinparent+sizeof(IMPL_INT),&arraypos,sizeof(IMPL_POINTER));
    datapos = arraypos;
    break;
  }
  case XS_ENCODING_FIXED_ARRAY:
    /* Fixed number of occurrances; write data directly in parent */
    datapos = posinparent;
    break;
  case XS_ENCODING_MAX_ARRAY: {
    /* Maximum number of occurrances; write count in parent, and then the actual data directly
       after that. */
    IMPL_INT count2 = 0;
    memcpy(encoded->data+posinparent,&count2,sizeof(IMPL_INT));
    datapos = posinparent + sizeof(IMPL_INT);
    break;
  }
  default:
    assert(!"invalid particle encoding type");
    break;
  }

  /* First validate and encode the list of elements according to particle p. The children are not
     procsessed yet; this only happens after we have procssed all the elements at this level.
     This is necessary because we need to know how much memory to allocate for this set of
     elements before we go onto the children. */
  while (!finished && ((0 > p->range.max_occurs) || (count < p->range.max_occurs))) {

    switch (p->term_type) {
    case PARTICLE_TERM_ELEMENT: {
      /* Do we have an element here? */
      if (NULL == *n) {
        v->inv_type = XS_VALIDATOR_INV_MISSING_ELEMENT;
        v->inv_p = p;
        v->inv_e = p->term.e;
        v->inv_lineno = *lineno;
        v->inv_count = count;
        finished = 1;
        break;
      }

      /* Is it the right element? */
      if (!check_element(*n,p->term.e->def.ident.m_name,
                         p->term.e->def.ident.m_ns)) {
        v->inv_type = XS_VALIDATOR_INV_WRONG_ELEMENT;
        v->inv_p = p;
        v->inv_e = p->term.e;
        v->inv_n = *n;
        v->inv_lineno = *lineno;
        finished = 1;
        continue;
      }

      /* Element validated. Now handle encoding. */


      switch (p->enctype) {
      case XS_ENCODING_SINGLE:
        /* Particle encoding type 1: Single instance of the particle stored directly in the parent's
           structure */
        /* FIXME */
        break;
      case XS_ENCODING_ARRAY_PTR: {
        /* Particle encoding type 2: Array size and pointer to contents */
        /* We need to allocate some more memory for this instance here */
        IMPL_ARRAYSIZE count2 = count+1;
        memcpy(encoded->data+posinparent,&count2,sizeof(IMPL_ARRAYSIZE));
/*         message("allocating data 0x%02x-0x%02x for item %d\n", */
/*                encoded->size-1,encoded->size-1+p->term.e->type->size,count); */
        stringbuf_append(encoded,NULL,p->term.e->type->size);
        break;
      }
      case XS_ENCODING_FIXED_ARRAY:
        /* Particle encoding type 3: Fixed number of instances of the particle stored directly in
           the parent's structure */
        /* FIXME */
        break;
      case XS_ENCODING_MAX_ARRAY: {
        /* Particle encoding type 4: maxOccurs is a positive integer... store array size and actual
           array in place */
        IMPL_ARRAYSIZE count2 = count+1;
        memcpy(encoded->data+posinparent,&count2,sizeof(IMPL_ARRAYSIZE));
        break;
      }
      default:
        assert(!"invalid particle encoding type");
        break;
      }

      /* Simple type - write data to memory.  */
      if (!p->term.e->type->complex) {
        /* FIXME: should collapse whitespace on this text where appropriate */
        char *text1 = xmlNodeGetContent(*n);
        String text = text1;
        free(text1);

        if (text.isNull())
          text = "";

        if (v->s->globals->boolean_type == p->term.e->type) {
          IMPL_BOOLEAN val = 0;
          if ((text == "true") || (text == "1"))
            val = 1;
          memcpy(encoded->data+datapos,&val,sizeof(IMPL_BOOLEAN));
        }
        else if (v->s->globals->long_type == p->term.e->type) {
          IMPL_LONG val = text.toInt();
          memcpy(encoded->data+datapos,&val,sizeof(IMPL_LONG));
        }
        else if (v->s->globals->int_type == p->term.e->type) {
          IMPL_INT val = text.toInt();
          memcpy(encoded->data+datapos,&val,sizeof(IMPL_INT));
        }
        else if (v->s->globals->short_type == p->term.e->type) {
          IMPL_SHORT val = text.toInt();
          memcpy(encoded->data+datapos,&val,sizeof(IMPL_SHORT));
        }
        else if (v->s->globals->byte_type == p->term.e->type) {
          IMPL_BYTE val = text.toInt();
          memcpy(encoded->data+datapos,&val,sizeof(IMPL_BYTE));
        }
        else if (v->s->globals->float_type == p->term.e->type) {
          IMPL_FLOAT val = text.toDouble();
          memcpy(encoded->data+datapos,&val,sizeof(IMPL_FLOAT));
        }
        else if (v->s->globals->double_type == p->term.e->type) {
          IMPL_DOUBLE val = text.toDouble();
          memcpy(encoded->data+datapos,&val,sizeof(IMPL_DOUBLE));
        }
        /* Strings are a special case; we have to write these after we have finished allocating
           all memory for element instances, since all instances of an element must be adjacent
           to each other in memory - strings go afterwards. */
        else if (v->s->globals->string_type == p->term.e->type) {
          stringencinfo *s = new stringencinfo();
          s->str = text;
          s->datapos = datapos;
          list_append(&strings,s);
        }
      }

      if (p->term.e->type->complex) {
        c = (childencinfo*)calloc(1,sizeof(childencinfo));
        c->n = *n;
        c->type = p->term.e->type;
        c->datapos = datapos;
        list_append(&children,c);
      }

      next_element(n);
      datapos += p->term.e->type->size;
      break;
    }
    case PARTICLE_TERM_MODEL_GROUP:
      switch (p->term.mg->compositor) {
      case MODELGROUP_COMPOSITOR_ALL:
        assert(!"not yet implemented");
        break;
      case MODELGROUP_COMPOSITOR_CHOICE: {
        xmlNodePtr nstart = *n;
        int foundvalid = 0;
        if (NULL != *n)
           *lineno = (*n)->line;
        for (l = p->term.mg->particles; l && !foundvalid; l = l->next) {
          Particle *p2 = (Particle*)l->data;
          *n = nstart;
          if (validate_element_against_particle(v,n,lineno,
                                                p2,encoded,datapos) >= p2->range.min_occurs)
            foundvalid = 1;
        }
        if (!foundvalid)
          finished = 1;
        break;
      }
      case MODELGROUP_COMPOSITOR_SEQUENCE: {
        for (l = p->term.mg->particles; l; l = l->next) {
          Particle *p2 = (Particle*)l->data;
          if (NULL != *n)
             *lineno = (*n)->line;
          if (validate_element_against_particle(v,n,lineno,
                                                p2,encoded,datapos+p2->pos) < p2->range.min_occurs) {
            finished = 1;
            break;
          }
        }
        break;
      }
      default:
        assert(!"invalid term type");
        break;
      }
      break;
    case PARTICLE_TERM_WILDCARD:
      assert(!"not yet implemented");
      break;
    default:
      assert(!"invalid term type");
      break;
    }

    if (finished)
      break;

    count++;
/*     message("set count = %d\n",count); */
  }

  for (l = strings; l; l = l->next) {
    stringencinfo *s = (stringencinfo*)l->data;
    IMPL_INT spos = encoded->size-1-s->datapos;
    s = (stringencinfo*)l->data;
/*     message("string value: %s, datapos=%d, spos=%d\n",s->str,s->datapos,spos); */
    memcpy(encoded->data+s->datapos,&spos,sizeof(IMPL_INT));
/*     message("string relpos 0x%02x (abspos 0x%02x) value \"%s\"\n",spos,encoded->size-1,s->str); */
    char *temp = s->str.cstring();
    stringbuf_append(encoded,temp,strlen(temp)+1);
    free(temp);
  }
  for (l = strings; l; l = l->next)
    delete static_cast<stringencinfo*>(l->data);
  list_free(strings,NULL);

  /* Now process all of the children. */
  for (l = children; l; l = l->next) {
    c = (childencinfo*)l->data;
    if (0 != validate_element_children(v,c->n,encoded,
                                       c->type,lineno,c->datapos)) {
      count = 0;
      break;
    }
  }
  list_free(children,(list_d_t)free);

/*   if (PARTICLE_TERM_ELEMENT == p->term_type) */
/*     message("validate_element_against_particle[%s]: returning %d\n",p->term.e->name,count); */
/*   else */
/*     message("validate_element_against_particle[MG %p]: returning %d\n",p->term.mg,count); */

  return count;
}

void set_inv_error(xs_validator *v, char *filename)
{
  switch (v->inv_type) {
  case XS_VALIDATOR_INV_NONE:
    break;
  case XS_VALIDATOR_INV_MISSING_ELEMENT: {
    if (0 < v->inv_count)
      error(&v->ei,filename,v->inv_lineno,String::null(),
            "Expected another %* element; found %d instance(s) but require at least %d",
            &v->inv_p->term.e->def.ident,v->inv_count,v->inv_p->range.min_occurs);
    else
      error(&v->ei,filename,v->inv_lineno,String::null(),
            "Expected element %*",&v->inv_p->term.e->def.ident);
    break;
  }
  case XS_VALIDATOR_INV_WRONG_ELEMENT: {
    NSName fn2;
    fn2.m_ns = (v->inv_n->ns ? v->inv_n->ns->href : NULL);
    fn2.m_name = v->inv_n->name;
    error(&v->ei,filename,v->inv_lineno,String::null(),"Expected element %*, but found %*",
          &v->inv_p->term.e->def.ident,&fn2);
    break;
  }
  case XS_VALIDATOR_INV_NOTHING_MORE_ALLOWED:
    error(&v->ei,filename,v->inv_lineno,String::null(),"No more elements allowed here");
    break;
  default:
    assert(!"invalid inv_type");
    break;
  }
}

int GridXSLT::validate_root(xs_validator *v, char *filename, xmlNodePtr n, stringbuf *encoded)
{
  Type *t;
  int lineno = n->line;
  int pos = encoded->size-1;

  if (XML_ELEMENT_NODE != n->type)
    return error(&v->ei,filename,n->line,String::null(),"Element expected\n");
  if (n->ns) {
    NSName nn = NSName(n->ns->href,n->name);
    if (NULL == (t = v->s->getType(nn))) {
      return error(&v->ei,filename,n->line,String::null(),"No type definition for element {%s}%s",
                   n->ns->href,n->name);
    }
  }
  else {
    NSName nn = NSName(String::null(),n->name);
    if (NULL == (t = v->s->getType(nn))) {
      return error(&v->ei,filename,n->line,String::null(),"No type definition for element %s",n->name);
    }
  }

  /* FIXME: what if root is a simple type (i.e. not a model group)? */
  assert(PARTICLE_TERM_MODEL_GROUP == t->content_type->term_type);
  stringbuf_append(encoded,NULL,t->content_type->term.mg->size);
/*   message("root element: allocated %d bytes\n",encoded->size-1); */

  if (0 != validate_element_children(v,n,encoded,t,&lineno,pos)) {
    set_inv_error(v,filename);
    return -1;
  }
  return 0;
}

int xs_decode_particle(xs_validator *v, xmlTextWriter *writer, Particle *p, stringbuf *encoded, int pos)
{
/*   message("xs_decode_particle pos 0x%02x particle %s (enctype %d)\n",pos,p->cvar,p->enctype); */
  if (PARTICLE_TERM_ELEMENT == p->term_type) {
    SchemaElement *e = p->term.e;
    XMLWriter::startElement(writer,e->def.ident.m_name);
    if (!e->type->complex) {
      if (v->s->globals->boolean_type == e->type) {
        IMPL_BOOLEAN val = *(IMPL_BOOLEAN*)(encoded->data+pos);
        XMLWriter::formatString(writer,"%s",val ? "true" : "false");
      }
      else if (v->s->globals->long_type == p->term.e->type) {
        IMPL_LONG val = *(IMPL_LONG*)(encoded->data+pos);
        XMLWriter::formatString(writer,"%ld",val);
      }
      else if (v->s->globals->int_type == p->term.e->type) {
        IMPL_INT val = *(IMPL_INT*)(encoded->data+pos);
        XMLWriter::formatString(writer,"%d",val);
      }
      else if (v->s->globals->short_type == p->term.e->type) {
        IMPL_SHORT val = *(IMPL_SHORT*)(encoded->data+pos);
        XMLWriter::formatString(writer,"%hd",val);
      }
      else if (v->s->globals->byte_type == p->term.e->type) {
        IMPL_BYTE val = *(IMPL_BYTE*)(encoded->data+pos);
        int val2 = val;
        XMLWriter::formatString(writer,"%d",val2);
      }
      else if (v->s->globals->float_type == p->term.e->type) {
        IMPL_FLOAT val = *(IMPL_FLOAT*)(encoded->data+pos);
        double val2 = val;
        XMLWriter::formatString(writer,"%f",val2);
      }
      else if (v->s->globals->double_type == p->term.e->type) {
        IMPL_DOUBLE val = *(IMPL_DOUBLE*)(encoded->data+pos);
        XMLWriter::formatString(writer,"%f",val);
      }
      else {
        IMPL_POINTER val = *(IMPL_POINTER*)(encoded->data+pos);
        val += pos;
        if (val >= encoded->size-1) {
          fmessage(stderr,"Pointer goes beyond end of data (pos: %d, target: %d)\n",pos,val);
          return -1;
        }
        XMLWriter::formatString(writer,"%s",(char*)(encoded->data+val));
      }
    }
    else if (e->type->content_type) {
      if (0 != xs_decode_particle(v,writer,e->type->content_type,encoded,pos))
        return -1;
    }
    XMLWriter::endElement(writer);
  }
  else if (PARTICLE_TERM_MODEL_GROUP == p->term_type) {
    ModelGroup *mg = p->term.mg;
    list *l;
    int basepos = pos;

    for (l = mg->particles; l; l = l->next) {
      Particle *p2 = (Particle*)l->data;
      pos = basepos + p2->pos;

      switch (p2->enctype) {
      case XS_ENCODING_SINGLE:
        /* Single instance of the particle stored directly in the parent's structure */
          if (0 != xs_decode_particle(v,writer,p2,encoded,pos))
            return -1;
        break;
      case XS_ENCODING_ARRAY_PTR: {
        /* Array size and pointer to contents */
        IMPL_ARRAYSIZE count = *(IMPL_ARRAYSIZE*)(encoded->data+pos);
        IMPL_POINTER ptr = *(IMPL_POINTER*)(encoded->data+pos+sizeof(IMPL_ARRAYSIZE));
        int i;
        pos += ptr;
        for (i = 0; i < count; i++) {
          if (0 != xs_decode_particle(v,writer,p2,encoded,pos))
            return -1;
          pos += p2->base_size;
        }
        break;
      }
      case XS_ENCODING_FIXED_ARRAY: {
        /* Fixed number of instances of the particle stored directly in the parent's structure */
        int i;
        for (i = 0; i < p2->range.max_occurs; i++) {
          if (0 != xs_decode_particle(v,writer,p2,encoded,pos))
            return -1;
          pos += p2->base_size;
        }
        break;
      }
      case XS_ENCODING_MAX_ARRAY: {
        /* maxOccurs is a positive integer... store array size and actual array in place */
        IMPL_ARRAYSIZE count = *(IMPL_ARRAYSIZE*)(encoded->data+pos);
        int i;
        pos += sizeof(IMPL_ARRAYSIZE);
        for (i = 0; i < count; i++) {
          if (0 != xs_decode_particle(v,writer,p2,encoded,pos))
            return -1;
          pos += p2->base_size;
        }
        break;
      }
      default:
        break;
      }
    }
  }
  return 0;
}

int GridXSLT::xs_decode(xs_validator *v, xmlTextWriter *writer, Type *t, stringbuf *encoded, int pos)
{
  int r;
  assert(t->content_type);
  assert(!t->def.ident.isNull());
  XMLWriter::startElement(writer,t->def.ident.m_name);
  r = xs_decode_particle(v,writer,t->content_type,encoded,pos);
  XMLWriter::endElement(writer);
  return r;
}
