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

typedef struct childencinfo childencinfo;
struct childencinfo {
  xmlNodePtr n;
  xs_type *type;
  int datapos;
};

typedef struct stringencinfo stringencinfo;
struct stringencinfo {
  char *str;
  int datapos;
};

void init_typeinfo_particle(xs_schema *s, xs_particle *p, list **typestack, list **mgstack,
                            int pos, int *size);
void init_typeinfo_model_group(xs_schema *s, xs_model_group *mg, list **typestack, list **mgstack);

int validate_element_against_particle(xs_validator *v,
                                      xmlNodePtr *n, int *lineno, xs_particle *p,
                                      stringbuf *encoded, int pos);

char *xs_alloc_cname(xs_globals *g, int useprefix, list **cnames, list **cnames2,
                     const char *name, const char *ns)
{
  char *cname;
  char *base;
  int num = 1;
  char *fixedname = NULL;
  char *prefix = NULL;
  list *l;

  if (useprefix) {
    if (NULL != ns) {
      for (l = g->namespaces->defs; l; l = l->next) {
        ns_def *xns = (ns_def*)l->data;
        char *c;
        if (!strcmp(xns->href,ns) && (NULL != xns->prefix)) {
          prefix = strdup(xns->prefix);
          for (c = prefix; '\0' != *c; c++)
            *c = toupper(*c);
          break;
        }
      }
    }
    if (NULL == prefix)
      prefix = strdup("XML");
  }
  else {
    prefix = strdup("");
  }

  if (name) {
    fixedname = strdup(name);
    char *c;
    for (c = fixedname; '\0' != *c; c++)
      if (!isalnum(*c))
        *c = '_';
    base = fixedname;
    cname = (char*)malloc(strlen(prefix)+strlen(fixedname)+100);
    sprintf(cname,"%s%s",prefix,fixedname);
  }
  else {
    base = "";
    cname = (char*)malloc(strlen(prefix)+100);
    sprintf(cname,"%s0",prefix);
  }
  while (list_contains_string(*cnames,cname) ||
         ((NULL != cnames2) && list_contains_string(*cnames2,cname)))
    sprintf(cname,"%s%s%d",prefix,base,num++);

  list_push(cnames,strdup(cname));
  free(fixedname);
  free(prefix);
  return cname;
}

void init_typeinfo_attributes(list *attribute_uses, list *attribute_group_refs, int *size)
{
  list *l;
  for (l = attribute_uses; l; l = l->next) {
    xs_attribute_use *au = (xs_attribute_use*)l->data;
    assert(!au->typeinfo_known);
    assert(au->attribute->type);
    assert(au->attribute->type->typeinfo_known);
    au->size = au->attribute->type->size;
    au->pos = *size;
    *size += au->size;
  }

  for (l = attribute_group_refs; l; l = l->next) {
    xs_attribute_group_ref *agr = (xs_attribute_group_ref*)l->data;
    assert(!agr->typeinfo_known);
    assert(agr->ag);
    assert(agr->ag->typeinfo_known);
    agr->size = agr->ag->size;
    agr->pos = *size;
    *size += agr->size;
  }

}

void init_typeinfo_attribute_group(xs_schema *s, xs_attribute_group *ag)
{
  list *l;
  xs_cstruct *cs;
  if (ag->typeinfo_known)
    return;

  for (l = ag->attribute_group_refs; l; l = l->next) {
    xs_attribute_group_ref *agr = (xs_attribute_group_ref*)l->data;
    if (!agr->ag->typeinfo_known)
      init_typeinfo_attribute_group(s,agr->ag);
  }

  ag->size = 0;

  init_typeinfo_attributes(ag->local_attribute_uses,ag->attribute_group_refs,&ag->size);

  cs = xs_cstruct_new(s->globals->as);
  cs->type = XS_CSTRUCT_ATTRIBUTE_GROUP;
  cs->object.ag = ag;
  list_append(&s->globals->cstructs,cs);

  ag->typeinfo_known = 1;
}

void init_typeinfo_simple_type(xs_schema *s, xs_type *t)
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

void init_typeinfo_complex_type(xs_schema *s, xs_type *t, list **typestack, list **mgstack)
{
  xs_type *t2;
  xs_cstruct *cs;
  if (t->typeinfo_known || t->builtin)
    return;
  assert(t->complex);

  assert(!list_contains_ptr(*typestack,t));
  list_push(typestack,t);

/*   printf("init_typeinfo_complex_type %p %s (%s)\n",t,t->name,t->ctype); */

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

  t2 = list_pop(typestack);
  assert(t2 == t);

  cs = xs_cstruct_new(s->globals->as);
  cs->type = XS_CSTRUCT_TYPE;
  cs->object.t = t;
  list_append(&s->globals->cstructs,cs);

  t->typeinfo_known = 1;
}

void init_typeinfo_particle(xs_schema *s, xs_particle *p, list **typestack, list **mgstack,
                            int pos, int *size)
{
  int force_ptr = 0;
  assert(!p->typeinfo_known);
  p->base_size = 0;
  switch (p->term_type) {
  case XS_PARTICLE_TERM_ELEMENT:
    if (!p->term.e->type->typeinfo_known) {
      if (list_contains_ptr(*typestack,p->term.e->type))
        force_ptr = 1;
      else
        init_typeinfo_complex_type(s,p->term.e->type,typestack,mgstack);
    }

    p->base_size = p->term.e->type->size;
    break;
  case XS_PARTICLE_TERM_MODEL_GROUP:
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
  case XS_PARTICLE_TERM_WILDCARD:
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

void init_typeinfo_model_group(xs_schema *s, xs_model_group *mg, list **typestack, list **mgstack)
{
  list *l;
  xs_model_group *mg2;
  xs_cstruct *cs;
  int largest = 0;
  if (mg->typeinfo_known)
    return;

  assert(!list_contains_ptr(*mgstack,mg));
  list_push(mgstack,mg);

  mg->size = 0;

  if (XS_MODEL_GROUP_COMPOSITOR_CHOICE == mg->compositor) {
    /* extra int at the start indicating which option is selected */
    mg->size += sizeof(IMPL_INT);
  }

/*   printf("init_typeinfo_model_group %p pn %s (ct %s)\n",mg,mg->parent_name,mg->ctype); */
  for (l = mg->particles; l; l = l->next) {
    xs_particle *p = (xs_particle*)l->data;
    int psize;
    init_typeinfo_particle(s,p,typestack,mgstack,mg->size,&psize);

    if (XS_MODEL_GROUP_COMPOSITOR_CHOICE == mg->compositor) {
      if (largest < p->size)
        largest = psize;
    }
    else {
      mg->size += psize;
    }

  }

  if (XS_MODEL_GROUP_COMPOSITOR_CHOICE == mg->compositor)
    mg->size += largest;

  mg2 = list_pop(mgstack);
  assert(mg2 == mg);

  cs = xs_cstruct_new(s->globals->as);
  cs->type = XS_CSTRUCT_MODEL_GROUP;
  cs->object.mg = mg;
  list_append(&s->globals->cstructs,cs);

  mg->typeinfo_known = 1;
}

void xs_init_cnames_mg(xs_schema *s, xs_model_group *mg, int indent)
{
  list *l;
  assert(NULL != mg->ctype);
  assert(NULL != mg->parent_name);
  for (l = mg->particles; l; l = l->next) {
    xs_particle *p = (xs_particle*)l->data;
    assert(NULL == p->cvar);
    switch (p->term_type) {
    case XS_PARTICLE_TERM_ELEMENT:
      p->cvar = xs_alloc_cname(s->globals,0,&mg->cvars,NULL,p->term.e->name,p->term.e->ns);
      break;
    case XS_PARTICLE_TERM_WILDCARD:
      p->cvar = xs_alloc_cname(s->globals,0,&mg->cvars,NULL,"any",NULL);
      break;
    case XS_PARTICLE_TERM_MODEL_GROUP:
      if (p->ref) {
        assert(NULL != p->term.mg->parent_name);
        p->cvar = xs_alloc_cname(s->globals,0,&mg->cvars,NULL,
                                   p->term.mg->parent_name,p->term.mg->parent_ns);
      }
      else {
        switch (p->term.mg->compositor) {
        case XS_MODEL_GROUP_COMPOSITOR_ALL:
          p->cvar = xs_alloc_cname(s->globals,0,&mg->cvars,NULL,"all",NULL);
          break;
        case XS_MODEL_GROUP_COMPOSITOR_CHOICE:
          p->cvar = xs_alloc_cname(s->globals,0,&mg->cvars,NULL,"choice",NULL);
          break;
        case XS_MODEL_GROUP_COMPOSITOR_SEQUENCE:
          p->cvar = xs_alloc_cname(s->globals,0,&mg->cvars,NULL,"sequence",NULL);
          break;
        default:
          assert(!"invalid model group compositor");
          break;
        }
        assert(NULL == p->term.mg->ctype);
        assert(NULL == p->term.mg->parent_name);
        p->term.mg->ctype = xs_alloc_cname(s->globals,1,&s->globals->ctypes,NULL,
                                           mg->parent_name,mg->parent_ns);
/*         printf("model group %s\n",p->term.mg->cname); */
        p->term.mg->parent_name = strdup(mg->parent_name);
        p->term.mg->parent_ns = mg->parent_ns ? strdup(mg->parent_ns) : NULL;
        xs_init_cnames_mg(s,p->term.mg,indent+1);
      }
      break;
    default:
      assert(!"invalid particle term type");
      break;
    }
  }
}

void xs_init_attribute_cnames(xs_schema *s, list *attribute_uses, list *attribute_group_refs,
                              list **cnames, list **cnames2)
{
  list *l;
  for (l = attribute_uses; l; l = l->next) {
    xs_attribute_use *au = (xs_attribute_use*)l->data;
    assert(NULL == au->cvar);
    au->cvar = xs_alloc_cname(s->globals,0,cnames,cnames2,
                               au->attribute->name,au->attribute->ns);
  }

  for (l = attribute_group_refs; l; l = l->next) {
    xs_attribute_group_ref *agr = (xs_attribute_group_ref*)l->data;
    assert(NULL == agr->cvar);
    agr->cvar = xs_alloc_cname(s->globals,0,cnames,cnames2,agr->ag->name,agr->ag->ns);
  }
}

void xs_init_cnames(xs_schema *s)
{
  list *l;
  symbol_space_entry *sse;

  for (l = s->imports; l; l = l->next)
    xs_init_cnames((xs_schema*)l->data);

  /* Objects that must have cnames assigned: model groups, types, particles */

  /* Inbuilt types: These all have the prefix "XSD" and then the type name */
  /* FIXME: set these when we create the actual builtin types */
  for (l = s->as->alloc_type; l; l = l->next) {
    xs_type *t = (xs_type*)l->data;
    /* should already be allocated... */
    if (t->builtin) {
      assert(t->ctype);
    }
  }

  /* Attribute groups (these are always top-level */
  for (sse = s->symt->ss_attribute_groups->entries; sse; sse = sse->next) {
    xs_attribute_group *ag = (xs_attribute_group*)sse->object;
    ag->ctype = xs_alloc_cname(s->globals,1,&s->globals->ctypes,NULL,ag->name,ag->ns);
  }

  /* Top-level model groups: use the name of the model group definition */
  for (sse = s->symt->ss_model_group_defs->entries; sse; sse = sse->next) {
    xs_model_group_def *mgd = (xs_model_group_def*)sse->object;
    mgd->model_group->ctype = xs_alloc_cname(s->globals,1,&s->globals->ctypes,NULL,
                                             mgd->name,mgd->ns);
    mgd->model_group->parent_name = strdup(mgd->name);
    mgd->model_group->parent_ns = mgd->ns ? strdup(mgd->ns) : NULL;
  }

  /* Top-level type declarations: use the name of the type, or the base custom cname for simple
     types (if there is one) */
  for (sse = s->symt->ss_types->entries; sse; sse = sse->next) {
    xs_type *t = (xs_type*)sse->object;
    if (!t->complex && t->base->custom_ctype)
      t->ctype = strdup(t->base->ctype);
    else if (t->builtin)
      t->ctype = xs_alloc_cname(s->globals,1,&s->globals->ctypes,NULL,t->name,t->ns);
    else
      t->ctype = xs_alloc_cname(s->globals,1,&s->globals->ctypes,NULL,t->name,t->ns);
    t->parent_name = strdup(t->name);
    t->parent_ns = t->ns ? strdup(t->ns) : NULL;
  }

  /* Anonymous type declarations inside <element>: use the name of the element */
  for (l = s->as->alloc_element; l; l = l->next) {
    xs_element *e = (xs_element*)l->data;
    if ((NULL == e->typeref) && ((NULL == e->sghead) || (e->type != e->sghead->type)))  {
      if (NULL != e->type->ctype) {
        printf("type cname already set! %s (name=%s,element=%s)\n",
               e->type->ctype,e->type->name,e->name);
      }
      assert(NULL == e->type->ctype);
      assert(NULL == e->type->parent_name);
      e->type->ctype = xs_alloc_cname(s->globals,1,&s->globals->ctypes,NULL,e->name,e->ns);
      e->type->parent_name = strdup(e->name);
      e->type->parent_ns = e->ns ? strdup(e->ns) : NULL;
    }
  }

  /* Simple type declarations within a <simpleType> (i.e. all types which have not yet been assigned
     a name) - generate numerically */
  for (l = s->as->alloc_type; l; l = l->next) {
    xs_type *t = (xs_type*)l->data;
    if (NULL == t->ctype) {
      assert(!t->complex);
      t->ctype = xs_alloc_cname(s->globals,1,&s->globals->ctypes,NULL,NULL,NULL);
    }
  }

  /* Model group definitions that are the content model of a type declaration - these take the
     name of the type */
  for (l = s->as->alloc_type; l; l = l->next) {
    xs_type *t = (xs_type*)l->data;
    if (t->complex && t->content_type) {
      assert(NULL != t->ctype);
      assert(NULL != t->parent_name); /* FIXME: this is not set for anon simpletypes */
      assert(XS_PARTICLE_TERM_MODEL_GROUP == t->content_type->term_type);

      t->content_type->cvar = strdup("(content model)");
      if (NULL == t->content_type->ref) {
        /* content model is an anonymous group; assign it a cname */
        t->content_type->term.mg->ctype = (char*)malloc(strlen("CM_")+strlen(t->ctype)+1);
        sprintf(t->content_type->term.mg->ctype,"CM_%s",t->ctype);

        t->content_type->term.mg->parent_name = strdup(t->parent_name);
        t->content_type->term.mg->parent_ns = t->parent_ns ? strdup(t->parent_ns) : NULL;
      }

      if ((NULL != t->effective_content) && 
          (t->effective_content != t->content_type)) {
        assert(XS_PARTICLE_TERM_MODEL_GROUP == t->effective_content->term_type);
        t->effective_content->cvar = strdup("(content model)");
        if (NULL == t->effective_content->ref) {
          /* effective content model is an anonymous group; assign it a cname */
          t->effective_content->term.mg->ctype = (char*)malloc(strlen("ECM_")+strlen(t->ctype)+1);
          sprintf(t->effective_content->term.mg->ctype,"ECM_%s",t->ctype);
          t->effective_content->term.mg->parent_name = strdup(t->parent_name);
          t->effective_content->term.mg->parent_ns = t->parent_ns ? strdup(t->parent_ns) : NULL;
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
    xs_model_group_def *mgd = (xs_model_group_def*)sse->object;
    xs_init_cnames_mg(s,mgd->model_group,1);
  }

  for (l = s->as->alloc_type; l; l = l->next) {
    xs_type *t = (xs_type*)l->data;
    if (t->complex && t->effective_content) {
      assert(XS_PARTICLE_TERM_MODEL_GROUP == t->effective_content->term_type);
      if (NULL == t->effective_content->ref)
        xs_init_cnames_mg(s,t->effective_content->term.mg,1);
    }
    if (t->complex) {
      list **cnames2 = NULL;
      if (NULL != t->effective_content) {
        assert(XS_PARTICLE_TERM_MODEL_GROUP == t->effective_content->term_type);
        cnames2 = &t->effective_content->term.mg->cvars;
      }

      xs_init_attribute_cnames(s,t->local_attribute_uses,t->attribute_group_refs,
                               &t->attribute_cvars,cnames2);
    }
  }

  /* Attribute groups */
  for (l = s->as->alloc_attribute_group; l; l = l->next) {
    xs_attribute_group *ag = (xs_attribute_group*)l->data;
    xs_init_attribute_cnames(s,ag->local_attribute_uses,ag->attribute_group_refs,&ag->cvars,NULL);
  }

  for (l = s->as->alloc_type; l; l = l->next)
    assert(NULL != ((xs_type*)l->data)->ctype);
  for (l = s->as->alloc_model_group; l; l = l->next)
    assert(NULL != ((xs_model_group*)l->data)->ctype);
  for (l = s->as->alloc_particle; l; l = l->next)
    assert(NULL != ((xs_particle*)l->data)->cvar);
}





void xs_init_typeinfo_simpletypes(xs_schema *s)
{
  list *l;

  for (l = s->imports; l; l = l->next)
    xs_init_typeinfo_simpletypes((xs_schema*)l->data);

  for (l = s->as->alloc_type; l; l = l->next) {
    xs_type *t = (xs_type*)l->data;
    if (!t->complex)
      init_typeinfo_simple_type(s,t);
  }
}


void xs_init_typeinfo_attribute_groups(xs_schema *s)
{
  list *l;

  for (l = s->imports; l; l = l->next)
    xs_init_typeinfo_attribute_groups((xs_schema*)l->data);

  for (l = s->as->alloc_attribute_group; l; l = l->next) {
    xs_attribute_group *ag = (xs_attribute_group*)l->data;
    init_typeinfo_attribute_group(s,ag);
  }
}



void xs_init_typeinfo_others(xs_schema *s)
{
  list *l;
  symbol_space_entry *sse;

  for (l = s->imports; l; l = l->next)
    xs_init_typeinfo_others((xs_schema*)l->data);

  /* top-level model groups */
  for (sse = s->symt->ss_model_group_defs->entries; sse; sse = sse->next) {
    xs_model_group_def *mgd = (xs_model_group_def*)sse->object;
    list *typestack = NULL;
    list *mgstack = NULL;
    if (!mgd->model_group->typeinfo_known)
      init_typeinfo_model_group(s,mgd->model_group,&typestack,&mgstack);
  }

  /* complex types */
  for (l = s->as->alloc_type; l; l = l->next) {
    xs_type *t = (xs_type*)l->data;
    list *typestack = NULL;
    list *mgstack = NULL;
    if (t->complex && !t->typeinfo_known)
      init_typeinfo_complex_type(s,t,&typestack,&mgstack);
  }


}

void xs_init_typeinfo(xs_schema *s)
{
  xs_init_typeinfo_simpletypes(s);
  xs_init_typeinfo_attribute_groups(s);
  xs_init_typeinfo_others(s);
}

xs_validator *xs_validator_new(xs_schema *s)
{
  xs_validator *v = (xs_validator*)calloc(1,sizeof(xs_validator));
  v->s = s;

  xs_init_cnames(s);
  xs_init_typeinfo(s);

  return v;
}

void xs_print_model_group_defs(xs_model_group *mg)
{
  int choice = (XS_MODEL_GROUP_COMPOSITOR_CHOICE == mg->compositor);
  list *l;

  assert(mg->typeinfo_known);
  if (choice) {
    printf("  int valtype;\n");
    printf("  union {\n");
  }

/*   printf("model group pn %s (ct %s)\n",mg->parent_name,mg->ctype); */
  for (l = mg->particles; l; l = l->next) {
    xs_particle *p = (xs_particle*)l->data;
    char *indent = choice ? "  " : "";
    char *typename = "(type)";
    char *name = "(name)";
    assert(p->typeinfo_known);
    switch (p->term_type) {
    case XS_PARTICLE_TERM_ELEMENT:
      assert(p->term.e->type);
      assert(p->term.e->type->ctype);
      assert(p->cvar);
      typename = p->term.e->type->ctype;
      name = p->cvar;
      break;
    case XS_PARTICLE_TERM_MODEL_GROUP:
      name = p->cvar;
      typename = p->term.mg->ctype;
      break;
    case XS_PARTICLE_TERM_WILDCARD:
      name = p->cvar;
      typename = "wildcard"; /* FIXME: what to do here? */
      break;
    default:
      assert(!"invalid particle type");
      break;
    }

    switch (p->enctype) {
    case XS_ENCODING_SINGLE:
      printf("%s  %s %s;\n",indent,typename,name);
      break;
    case XS_ENCODING_ARRAY_PTR:
      printf("%s  int n%s;\n",indent,name);
      printf("%s  %s *%s;\n",indent,typename,name);
      break;
    case XS_ENCODING_FIXED_ARRAY:
      printf("%s  %s %s[%d];\n",indent,typename,name,p->range.max_occurs);
      break;
    case XS_ENCODING_MAX_ARRAY:
      printf("%s  int n%s;\n",indent,name);
      printf("%s  %s %s[%d];\n",indent,typename,name,p->range.max_occurs);
      break;
    default:
      assert(!"invalid particle encoding type");
      break;
    }
  }
  if (choice) {
    printf("  } val;\n");
  }
}

void xs_print_attribute_defs(list *attribute_uses, list *attribute_group_refs)
{
  list *l;
  for (l = attribute_uses; l; l = l->next) {
    xs_attribute_use *au = (xs_attribute_use*)l->data;
    assert(au->cvar);
    assert(au->attribute->type->ctype);
    printf("  %s %s;\n",au->attribute->type->ctype,au->cvar);
  }

  for (l = attribute_group_refs; l; l = l->next) {
    xs_attribute_group_ref *agr = (xs_attribute_group_ref*)l->data;
    assert(agr->cvar);
    printf("  %s %s;\n",agr->ag->ctype,agr->cvar);
  }
}

void xs_print_mg_choices(xs_model_group *mg, char *ctype)
{
  if (XS_MODEL_GROUP_COMPOSITOR_CHOICE == mg->compositor) {
    list *first_ew = NULL;
    list *l;
    int choice = 1;
    for (l = mg->particles; l; l = l->next) {
      xs_particle *p = (xs_particle*)l->data;
      xs_get_first_elements_and_wildcards(p,&first_ew);
      assert(p->cvar);
      printf("#define %s_%s %d\n",ctype,p->cvar,choice++);
    }
    list_free(first_ew,NULL);
    printf("\n");
  }
}

void xs_print_defs(xs_schema *s)
{
  list *l;

  for (l = s->globals->cstructs; l; l = l->next) {
    xs_cstruct *cs = (xs_cstruct*)l->data;
    switch (cs->type) {
    case XS_CSTRUCT_TYPE:
      printf("typedef struct %s %s;\n",cs->object.t->ctype,cs->object.t->ctype);
      break;
    case XS_CSTRUCT_MODEL_GROUP:
      if (NULL == cs->object.mg->content_model_of)
        printf("typedef struct %s %s;\n",cs->object.mg->ctype,cs->object.mg->ctype);
      break;
    case XS_CSTRUCT_ATTRIBUTE_GROUP:
      printf("typedef struct %s %s;\n",cs->object.ag->ctype,cs->object.ag->ctype);
      break;
    default:
      assert(!"invalid cstruct type");
      break;
    }
  }

  printf("\n");

  for (l = s->globals->cstructs; l; l = l->next) {
    xs_cstruct *cs = (xs_cstruct*)l->data;
    switch (cs->type) {
    case XS_CSTRUCT_TYPE:
      if (cs->object.t->complex) {
        xs_type *t = cs->object.t;
        if (NULL != t->effective_content) {
          assert(XS_PARTICLE_TERM_MODEL_GROUP == t->effective_content->term_type);
          xs_print_mg_choices(t->effective_content->term.mg,t->ctype);
        }

        printf("struct %s {\n",t->ctype);
        if ((t->base != s->globals->complex_ur_type) &&
            t->base->complex) {
          printf("  %s base;\n",t->base->ctype);
        }

        if (NULL != t->effective_content)
          xs_print_model_group_defs(t->effective_content->term.mg);

        xs_print_attribute_defs(t->local_attribute_uses,t->attribute_group_refs);
        printf("} __attribute__((__packed__));\n");
        printf("/* complex type - %d bytes */\n",t->size);
        printf("\n");
      }
      break;
    case XS_CSTRUCT_MODEL_GROUP:
      if (NULL == cs->object.mg->content_model_of) {
        xs_model_group *mg = cs->object.mg;
        xs_print_mg_choices(mg,mg->ctype);
        printf("struct %s {\n",mg->ctype);
        xs_print_model_group_defs(mg);
        printf("} __attribute__((__packed__));\n");
        printf("/* model group - %d bytes */\n",mg->size);
        printf("\n");
      }
      break;
    case XS_CSTRUCT_ATTRIBUTE_GROUP: {
      xs_attribute_group *ag = cs->object.ag;
      printf("struct %s {\n",ag->ctype);
      xs_print_attribute_defs(ag->local_attribute_uses,ag->attribute_group_refs);
      printf("} __attribute__((__packed__));\n");
      printf("/* attribute group - %d bytes */\n",ag->size);
      printf("\n");
      break;
    }
    default:
      assert(!"invalid cstruct type");
      break;
    }
  }
}

void xs_validator_free(xs_validator *v)
{
  error_info_free_vals(&v->ei);
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



void get_particle_pattern(stringbuf *buf, xs_particle *p)
{
  int inbrackets = 0;
  if (((1 != p->range.min_occurs) || (1 != p->range.max_occurs)) &&
      (XS_PARTICLE_TERM_MODEL_GROUP == p->term_type)) {
    stringbuf_printf(buf,"(");
    inbrackets = 1;
  }

  switch (p->term_type) {
  case XS_PARTICLE_TERM_ELEMENT: {
    stringbuf_printf(buf,"%s",p->term.e->name);
    break;
  }
  case XS_PARTICLE_TERM_MODEL_GROUP:
    switch (p->term.mg->compositor) {
    case XS_MODEL_GROUP_COMPOSITOR_ALL:
      assert(!"not implemented");
      break;
    case XS_MODEL_GROUP_COMPOSITOR_CHOICE: {
      list *l;
      if (!inbrackets)
        stringbuf_printf(buf,"(");
      for (l = p->term.mg->particles; l; l = l ->next) {
        xs_particle *p2 = (xs_particle*)l->data;
        get_particle_pattern(buf,p2);
        if (l->next)
          stringbuf_printf(buf,"|");
      }
      if (!inbrackets)
        stringbuf_printf(buf,")");
      break;
    }
    case XS_MODEL_GROUP_COMPOSITOR_SEQUENCE: {
      list *l;
      for (l = p->term.mg->particles; l; l = l ->next) {
        xs_particle *p2 = (xs_particle*)l->data;
        get_particle_pattern(buf,p2);
        if (l->next)
          stringbuf_printf(buf," ");
      }
      break;
    }
    default:
      assert(!"not implemented");
      break;
    }
    break;
  case XS_PARTICLE_TERM_WILDCARD:
    assert(!"not yet implemented");
    break;
  default:
    assert(!"invalid particle type");
    break;
  }
  if ((1 != p->range.min_occurs) || (1 != p->range.max_occurs)) {
    if (XS_PARTICLE_TERM_MODEL_GROUP == p->term_type)
      stringbuf_printf(buf,")");

    if (p->range.min_occurs == p->range.max_occurs)
      stringbuf_printf(buf,"{%d}",p->range.min_occurs,p->range.max_occurs);
    else if ((0 == p->range.min_occurs) && (1 == p->range.max_occurs))
      stringbuf_printf(buf,"?",p->range.min_occurs,p->range.max_occurs);
    else if (0 <= p->range.max_occurs)
      stringbuf_printf(buf,"{%d,%d}",p->range.min_occurs,p->range.max_occurs);
    else if (0 == p->range.min_occurs)
      stringbuf_printf(buf,"*",p->range.min_occurs,p->range.max_occurs);
    else if (1 == p->range.min_occurs)
      stringbuf_printf(buf,"+",p->range.min_occurs,p->range.max_occurs);
    else
      stringbuf_printf(buf,"{%d,}",p->range.min_occurs,p->range.max_occurs);
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
                              xs_type *t, int *lineno, int pos)
{
  xmlNodePtr c;
  int r;

  c = n->children;
  skip_text(&c);
  if (NULL != c)
    *lineno = c->line;

  r = validate_element_against_particle(v,&c,lineno,t->content_type,encoded,pos);
/*   printf("validating children of %s: r = %d\n",n->name,r); */
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
                                      xmlNodePtr *n, int *lineno, xs_particle *p,
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
    printf("typeinfo of particle %p is not known\n",p);
    printf("p->extension_for = %p\n",p->extension_for);
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
    case XS_PARTICLE_TERM_ELEMENT: {
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
      if (!check_element(*n,p->term.e->name,p->term.e->ns)) {
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
/*         printf("allocating data 0x%02x-0x%02x for item %d\n", */
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
        char *text = xmlNodeGetContent(*n);
        if (NULL == text)
          text = strdup("");

        if (v->s->globals->boolean_type == p->term.e->type) {
          IMPL_BOOLEAN val = 0;
          if (!strcmp(text,"true") || !strcmp(text,"1"))
            val = 1;
          memcpy(encoded->data+datapos,&val,sizeof(IMPL_BOOLEAN));
        }
        else if (v->s->globals->long_type == p->term.e->type) {
          IMPL_LONG val = atoi(text);
          memcpy(encoded->data+datapos,&val,sizeof(IMPL_LONG));
        }
        else if (v->s->globals->int_type == p->term.e->type) {
          IMPL_INT val = atoi(text);
          memcpy(encoded->data+datapos,&val,sizeof(IMPL_INT));
        }
        else if (v->s->globals->short_type == p->term.e->type) {
          IMPL_SHORT val = atoi(text);
          memcpy(encoded->data+datapos,&val,sizeof(IMPL_SHORT));
        }
        else if (v->s->globals->byte_type == p->term.e->type) {
          IMPL_BYTE val = atoi(text);
          memcpy(encoded->data+datapos,&val,sizeof(IMPL_BYTE));
        }
        else if (v->s->globals->float_type == p->term.e->type) {
          IMPL_FLOAT val = atof(text);
          memcpy(encoded->data+datapos,&val,sizeof(IMPL_FLOAT));
        }
        else if (v->s->globals->double_type == p->term.e->type) {
          IMPL_DOUBLE val = atof(text);
          memcpy(encoded->data+datapos,&val,sizeof(IMPL_DOUBLE));
        }
        /* Strings are a special case; we have to write these after we have finished allocating
           all memory for element instances, since all instances of an element must be adjacent
           to each other in memory - strings go afterwards. */
        else if (v->s->globals->string_type == p->term.e->type) {
          stringencinfo *s = (stringencinfo*)calloc(1,sizeof(stringencinfo));
          s->str = strdup(text);
          s->datapos = datapos;
          list_append(&strings,s);
        }
        free(text);
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
    case XS_PARTICLE_TERM_MODEL_GROUP:
      switch (p->term.mg->compositor) {
      case XS_MODEL_GROUP_COMPOSITOR_ALL:
        assert(!"not yet implemented");
        break;
      case XS_MODEL_GROUP_COMPOSITOR_CHOICE: {
        xmlNodePtr nstart = *n;
        int foundvalid = 0;
        if (NULL != *n)
           *lineno = (*n)->line;
        for (l = p->term.mg->particles; l && !foundvalid; l = l->next) {
          xs_particle *p2 = (xs_particle*)l->data;
          *n = nstart;
          if (validate_element_against_particle(v,n,lineno,
                                                p2,encoded,datapos) >= p2->range.min_occurs)
            foundvalid = 1;
        }
        if (!foundvalid)
          finished = 1;
        break;
      }
      case XS_MODEL_GROUP_COMPOSITOR_SEQUENCE: {
        for (l = p->term.mg->particles; l; l = l->next) {
          xs_particle *p2 = (xs_particle*)l->data;
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
    case XS_PARTICLE_TERM_WILDCARD:
      assert(!"not yet implemented");
      break;
    default:
      assert(!"invalid term type");
      break;
    }

    if (finished)
      break;

    count++;
/*     printf("set count = %d\n",count); */
  }

  for (l = strings; l; l = l->next) {
    stringencinfo *s = (stringencinfo*)l->data;
    IMPL_INT spos = encoded->size-1-s->datapos;
    s = (stringencinfo*)l->data;
/*     printf("string value: %s, datapos=%d, spos=%d\n",s->str,s->datapos,spos); */
    memcpy(encoded->data+s->datapos,&spos,sizeof(IMPL_INT));
/*     printf("string relpos 0x%02x (abspos 0x%02x) value \"%s\"\n",spos,encoded->size-1,s->str); */
    stringbuf_append(encoded,s->str,strlen(s->str)+1);
  }
  list_free(strings,(void*)free);

  /* Now process all of the children. */
  for (l = children; l; l = l->next) {
    c = (childencinfo*)l->data;
    if (0 != validate_element_children(v,c->n,encoded,
                                       c->type,lineno,c->datapos)) {
      count = 0;
      break;
    }
  }
  list_free(children,(void*)free);

/*   if (XS_PARTICLE_TERM_ELEMENT == p->term_type) */
/*     printf("validate_element_against_particle[%s]: returning %d\n",p->term.e->name,count); */
/*   else */
/*     printf("validate_element_against_particle[MG %p]: returning %d\n",p->term.mg,count); */

  return count;
}

void set_inv_error(xs_validator *v, char *filename)
{
  switch (v->inv_type) {
  case XS_VALIDATOR_INV_NONE:
    break;
  case XS_VALIDATOR_INV_MISSING_ELEMENT: {
    char *fn1 = get_full_name(v->inv_p->term.e->ns,v->inv_p->term.e->name);
    if (0 < v->inv_count)
      set_error_info(&v->ei,filename,v->inv_lineno,NULL,NULL,
                     "Expected another %s element; found %d instance(s) but require at least %d",
                     fn1,v->inv_count,v->inv_p->range.min_occurs);
    else
      set_error_info(&v->ei,filename,v->inv_lineno,NULL,NULL,"Expected element %s",fn1);
    free(fn1);
    break;
  }
  case XS_VALIDATOR_INV_WRONG_ELEMENT: {
    char *fn1 = get_full_name(v->inv_p->term.e->ns,v->inv_p->term.e->name);
    char *fn2 = get_full_name(v->inv_n->ns ? v->inv_n->ns->href : NULL,v->inv_n->name);
    set_error_info(&v->ei,filename,v->inv_lineno,NULL,NULL,
                   "Expected element %s, but found %s",fn1,fn2);
    free(fn1);
    free(fn2);
    break;
  }
  case XS_VALIDATOR_INV_NOTHING_MORE_ALLOWED:
    set_error_info(&v->ei,filename,v->inv_lineno,NULL,NULL,"No more elements allowed here");
    break;
  default:
    assert(!"invalid inv_type");
    break;
  }
}

int validate_root(xs_validator *v, char *filename, xmlNodePtr n, stringbuf *encoded)
{
  xs_type *t;
  int lineno = n->line;
  int pos = encoded->size-1;

  if (XML_ELEMENT_NODE != n->type)
    return set_error_info(&v->ei,filename,n->line,NULL,NULL,"Element expected\n");
  if (n->ns) {
    if (NULL == (t = xs_lookup_type(v->s,n->name,n->ns->href)))
      return set_error_info(&v->ei,filename,n->line,NULL,NULL,
                            "No type definition for element {%s}%s",n->ns->href,n->name);
  }
  else {
    if (NULL == (t = xs_lookup_type(v->s,n->name,NULL)))
      return set_error_info(&v->ei,filename,n->line,NULL,NULL,
                            "No type definition for element %s",n->name);
  }

  /* FIXME: what if root is a simple type (i.e. not a model group)? */
  assert(XS_PARTICLE_TERM_MODEL_GROUP == t->content_type->term_type);
  stringbuf_append(encoded,NULL,t->content_type->term.mg->size);
/*   printf("root element: allocated %d bytes\n",encoded->size-1); */

  if (0 != validate_element_children(v,n,encoded,t,&lineno,pos)) {
    set_inv_error(v,filename);
    return -1;
  }
  return 0;
}

int xs_decode_particle(xs_validator *v, xmlTextWriter *writer, xs_particle *p, stringbuf *encoded, int pos)
{
/*   printf("xs_decode_particle pos 0x%02x particle %s (enctype %d)\n",pos,p->cvar,p->enctype); */
  if (XS_PARTICLE_TERM_ELEMENT == p->term_type) {
    xs_element *e = p->term.e;
    xmlTextWriterStartElement(writer,e->name);
    if (!e->type->complex) {
      if (v->s->globals->boolean_type == e->type) {
        IMPL_BOOLEAN val = *(IMPL_BOOLEAN*)(encoded->data+pos);
        xmlTextWriterWriteFormatString(writer,"%s",val ? "true" : "false");
      }
      else if (v->s->globals->long_type == p->term.e->type) {
        IMPL_LONG val = *(IMPL_LONG*)(encoded->data+pos);
        xmlTextWriterWriteFormatString(writer,"%ld",val);
      }
      else if (v->s->globals->int_type == p->term.e->type) {
        IMPL_INT val = *(IMPL_INT*)(encoded->data+pos);
        xmlTextWriterWriteFormatString(writer,"%d",val);
      }
      else if (v->s->globals->short_type == p->term.e->type) {
        IMPL_SHORT val = *(IMPL_SHORT*)(encoded->data+pos);
        xmlTextWriterWriteFormatString(writer,"%hd",val);
      }
      else if (v->s->globals->byte_type == p->term.e->type) {
        IMPL_BYTE val = *(IMPL_BYTE*)(encoded->data+pos);
        int val2 = val;
        xmlTextWriterWriteFormatString(writer,"%d",val2);
      }
      else if (v->s->globals->float_type == p->term.e->type) {
        IMPL_FLOAT val = *(IMPL_FLOAT*)(encoded->data+pos);
        double val2 = val;
        xmlTextWriterWriteFormatString(writer,"%f",val2);
      }
      else if (v->s->globals->double_type == p->term.e->type) {
        IMPL_DOUBLE val = *(IMPL_DOUBLE*)(encoded->data+pos);
        xmlTextWriterWriteFormatString(writer,"%f",val);
      }
      else {
        IMPL_POINTER val = *(IMPL_POINTER*)(encoded->data+pos);
        val += pos;
        if (val >= encoded->size-1) {
          fprintf(stderr,"Pointer goes beyond end of data (pos: %d, target: %d)\n",pos,val);
          return -1;
        }
        xmlTextWriterWriteFormatString(writer,"%s",(char*)(encoded->data+val));
      }
    }
    else if (e->type->content_type) {
      if (0 != xs_decode_particle(v,writer,e->type->content_type,encoded,pos))
        return -1;
    }
    xmlTextWriterEndElement(writer);
  }
  else if (XS_PARTICLE_TERM_MODEL_GROUP == p->term_type) {
    xs_model_group *mg = p->term.mg;
    list *l;
    int basepos = pos;

    for (l = mg->particles; l; l = l->next) {
      xs_particle *p2 = (xs_particle*)l->data;
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

int xs_decode(xs_validator *v, xmlTextWriter *writer, xs_type *t, stringbuf *encoded, int pos)
{
  int r;
  assert(t->content_type);
  assert(t->name);
  xmlTextWriterStartElement(writer,t->name);
  r = xs_decode_particle(v,writer,t->content_type,encoded,pos);
  xmlTextWriterEndElement(writer);
  return r;
}
