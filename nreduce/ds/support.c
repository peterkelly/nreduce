#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <assert.h>
#include <libxml/parser.h>
#include "ds.h"

static char *concat_str(const char *a, const char *b)
{
  char *res = (char*)malloc(strlen(a)+strlen(b)+1);
  sprintf(res,"%s%s",a,b);
  return res;
}

char *varname_str(varname *vn)
{
  int len =
    strlen(vn->name) + 1 +
    (vn->suffix ? strlen(vn->suffix) : 0) + 1 +
    (vn->type ? strlen(vn->type) : 0) + 1;
  char *str = (char*)malloc(len);
  sprintf(str,"%s:%s:%s",
          vn->name,
          vn->suffix ? vn->suffix : "",
          vn->type ? vn->type : "");
  return str;
}

static char *get_define(const char *prefix, const char **names, int type)
{
  char *str = (char*)malloc(strlen(prefix)+strlen(names[type])+1);
  char *c;
  sprintf(str,"%s%s",prefix,names[type]);
  for (c = str; *c; c++) {
    *c = toupper(*c);
    if ('-' == *c)
      *c = '_';
  }
  return str;
}

static char *get_expr_define(int type)
{
  if (type >= XSLT_FIRST)
    return get_define("XSLT_",expr_names,type);
  else
    return get_define("XPATH_",expr_names,type);
}

static int is_dsvar_type(const char *dsvar, const char *type)
{
  char *c1;
  char *c2;
  if ((NULL != (c1 = strchr(dsvar,':'))) &&
      (NULL != (c2 = strchr(c1+1,':'))))
    return !strcmp(c2+1,type);
  else
    return 0;
}

int have_dsvar_type(const char *dsvar)
{
  char *c1;
  char *c2;
  if ((NULL != (c1 = strchr(dsvar,':'))) &&
      (NULL != (c2 = strchr(c1+1,':'))) &&
      (0 < strlen(c2+1)))
    return 1;
  else
    return 0;
}

void gen_tests(FILE *f, int first, const char *base, const char *suffix,
               expression *expr, int nulltests)
{
  if (NULL == expr)
    return;

  if (((XPATH_DSVAR == expr->type) || (XSLT_DSVAR_INSTRUCTION == expr->type)) &&
      !is_dsvar_type(expr->str,"n")) {
    if (nulltests && (XPATH_DSVAR == expr->type)) {
      char *exprname = concat_str(base,suffix);
      if (!first)
        fprintf(f," &&\n           ");
      fprintf(f,"(NULL != %s)",exprname);
      free(exprname);
    }
    return;
  }

  if (!first)
    fprintf(f," &&\n           ");

  char *exprname = concat_str(base,suffix);

  if (XSLT_DSVAR_TEXT == expr->type) {
    fprintf(f,"(XSLT_LITERAL_TEXT_NODE == %s->type)",exprname);
  }
  else if (XSLT_DSVAR_TEXTINSTR == expr->type) {
    fprintf(f,"(XSLT_TEXT == %s->type)",exprname);
  }
  else if (XSLT_DSVAR_LITELEM == expr->type) {
    fprintf(f,"(XSLT_LITERAL_RESULT_ELEMENT == %s->type)",exprname);
  }
  else if (((XPATH_DSVAR == expr->type) || (XSLT_DSVAR_INSTRUCTION == expr->type)) &&
      is_dsvar_type(expr->str,"n")) {
    fprintf(f,"(RESTYPE_NUMBER == %s->restype)",exprname);
  }
  else if (XPATH_DSVAR_NUMERIC == expr->type) {
    fprintf(f,"(XPATH_NUMERIC_LITERAL == %s->type)",exprname);
  }
  else if (XPATH_DSVAR_STRING == expr->type) {
    fprintf(f,"(XPATH_STRING_LITERAL == %s->type)",exprname);
  }
  else if (XPATH_DSVAR_VAR_REF == expr->type) {
    fprintf(f,"(XPATH_VAR_REF == %s->type)",exprname);
  }
  else if (XPATH_DSVAR_NODETEST == expr->type) {
    fprintf(f,"(XPATH_NODE_TEST == %s->type)",exprname);
    gen_tests(f,0,exprname,"->r.left",expr->r.left,nulltests);
    gen_tests(f,0,exprname,"->r.right",expr->r.right,nulltests);
  }
  else {
    int i;
    char *def = get_expr_define(expr->type);
    fprintf(f,"(%s == %s->type)",def,exprname);
    free(def);

    if (XPATH_AXIS == expr->type) {
      if (AXIS_DSVAR_FORWARD == expr->axis) {
        fprintf(f," &&\n           is_forward_axis(%s->axis)",exprname);
      }
      else if (AXIS_DSVAR_REVERSE == expr->axis) {
        fprintf(f," &&\n           is_reverse_axis(%s->axis)",exprname);
      }
      else {
        assert(0 <= expr->axis);
        assert(AXIS_COUNT > expr->axis);
        char *def = get_define("AXIS_",axis_names,expr->axis);
        fprintf(f," &&\n           (%s == %s->axis)",def,exprname);
        free(def);
      }
    }

    if (XPATH_KIND_TEST == expr->type) {
      def = get_define("KIND_",kind_names,expr->kind);
      fprintf(f," &&\n           (%s == %s->kind)",def,exprname);
      free(def);
    }

    for (i = 0; i < EXPR_REFCOUNT; i++)
      gen_tests(f,0,exprname,expr_refnames[i],expr->refs[i],nulltests);

    gen_tests(f,0,exprname,"->next",expr->next,nulltests);
  }

  free(exprname);
}

expression *get_symexpr(const char *base, const char *suffix, expression *expr, const char *name)
{
  char *exprname;
  expression *res = NULL;
  int i;

  assert(name);

  if (NULL == expr)
    return NULL;

  if ((XPATH_DSVAR_VAR_REF == expr->type) && (!strcmp(expr->str,name)))
    return expr;
  else if ((XPATH_DSVAR_NUMERIC == expr->type) && (!strcmp(expr->str,name)))
    return expr;
  else if ((XPATH_DSVAR_STRING == expr->type) && (!strcmp(expr->str,name)))
    return expr;
  else if (((XPATH_DSVAR == expr->type) ||
            (XPATH_DSVAR_NODETEST == expr->type) ||
            (XSLT_DSVAR_INSTRUCTION == expr->type) ||
            (XSLT_DSVAR_TEXT == expr->type) ||
            (XSLT_DSVAR_LITELEM == expr->type) ||
            (XSLT_DSVAR_TEXTINSTR == expr->type)) && !strcmp(expr->str,name))
    return expr;

  exprname = concat_str(base,suffix);

  for (i = 0; (NULL == res) && (i < EXPR_REFCOUNT); i++)
    res = get_symexpr(exprname,expr_refnames[i],expr->refs[i],name);
  if (NULL == res)
    res = get_symexpr(exprname,"->next",expr->next,name);

  free(exprname);
  return res;
}

char *get_symref(const char *base, const char *suffix, expression *expr, const char *name)
{
  char *exprname;
  char *ref = NULL;
  int i;

  assert(name);

  if (NULL == expr)
    return NULL;

  exprname = concat_str(base,suffix);
  if (!strcmp(name,"R::"))
    printf("get_symref: %s %s\n",exprname,expr_names[expr->type]);

  if ((XPATH_DSVAR_VAR_REF == expr->type) && (!strcmp(expr->str,name)))
    ref = strdup(exprname);
  else if ((XPATH_DSVAR_NUMERIC == expr->type) && (!strcmp(expr->str,name)))
    ref = strdup(exprname);
  else if ((XPATH_DSVAR_STRING == expr->type) && (!strcmp(expr->str,name)))
    ref = strdup(exprname);
  else if ((XPATH_AXIS == expr->type) &&
           ((AXIS_DSVAR_FORWARD == expr->axis) || (AXIS_DSVAR_REVERSE == expr->axis)) &&
           !strcmp(expr->str,name))
    ref = strdup(exprname);
  else if (((XPATH_DSVAR == expr->type) ||
            (XPATH_DSVAR_NODETEST == expr->type) ||
            (XSLT_DSVAR_INSTRUCTION == expr->type) ||
            (XSLT_DSVAR_TEXT == expr->type) ||
            (XSLT_DSVAR_LITELEM == expr->type) ||
            (XSLT_DSVAR_TEXTINSTR == expr->type)) && !strcmp(expr->str,name))
    ref = strdup(exprname);

  for (i = 0; (NULL == ref) && (i < EXPR_REFCOUNT); i++)
    ref = get_symref(exprname,expr_refnames[i],expr->refs[i],name);
  if (NULL == ref)
    ref = get_symref(exprname,"->next",expr->next,name);

  free(exprname);
  return ref;
}

mapping *get_mapping(mapping *mappings, const char *name)
{
  mapping *m;
  for (m = mappings; m; m = m->next)
    if (!strcmp(m->name,name))
      return m;
  return NULL;
}
