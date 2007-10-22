#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <assert.h>
#include <libxml/parser.h>
#include "ds.h"
#include "cxslt/cxslt.h"

extern FILE *dsin;
extern const char *dsfilename;
int dsparse();

rule *parse_rules = NULL;
mapping *parse_mappings = NULL;

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

static int have_dsvar_type(const char *dsvar)
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

static void gen_tests(FILE *f, int first, const char *base, const char *suffix, expression *expr)
{
  if (NULL == expr)
    return;

  if ((XPATH_DSVAR == expr->type) && !is_dsvar_type(expr->str,"n"))
    return;

  if (!first)
    fprintf(f," &&\n           ");

  char *exprname = concat_str(base,suffix);

  if ((XPATH_DSVAR == expr->type) && is_dsvar_type(expr->str,"n")) {
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
    gen_tests(f,0,exprname,"->left",expr->left);
    gen_tests(f,0,exprname,"->right",expr->right);
  }
  else {
    char *def = get_define("XPATH_",expr_names,expr->type);
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

    gen_tests(f,0,exprname,"->test",expr->test);
    gen_tests(f,0,exprname,"->left",expr->left);
    gen_tests(f,0,exprname,"->right",expr->right);
  }

  free(exprname);
}

static expression *get_symexpr(const char *base, const char *suffix,
                               expression *expr, const char *name)
{
  char *exprname;
  expression *res = NULL;

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
            (XPATH_DSVAR_NODETEST == expr->type)) && !strcmp(expr->str,name))
    return expr;

  exprname = concat_str(base,suffix);
  if (NULL == res)
    res = get_symexpr(exprname,"->test",expr->test,name);
  if (NULL == res)
    res = get_symexpr(exprname,"->left",expr->left,name);
  if (NULL == res)
    res = get_symexpr(exprname,"->right",expr->right,name);

  free(exprname);
  return res;
}


static char *get_symref(const char *base, const char *suffix,
                       expression *expr, const char *name)
{
  char *exprname;
  char *ref = NULL;

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
            (XPATH_DSVAR_NODETEST == expr->type)) && !strcmp(expr->str,name))
    ref = strdup(exprname);

  if (NULL == ref)
    ref = get_symref(exprname,"->test",expr->test,name);
  if (NULL == ref)
    ref = get_symref(exprname,"->left",expr->left,name);
  if (NULL == ref)
    ref = get_symref(exprname,"->right",expr->right,name);

  free(exprname);
  return ref;
}

static mapping *get_mapping(mapping *mappings, const char *name)
{
  mapping *m;
  for (m = mappings; m; m = m->next)
    if (!strcmp(m->name,name))
      return m;
  return NULL;
}

static char *flatten_syntax(syntax *a)
{
  array *buf = array_new(1,0);
  char *ret;
  for (; a; a = a->next) {
    switch (a->type) {
    case SYNTAX_STRING:
      array_printf(buf,"%s",a->str);
      break;
    case SYNTAX_VAR:
      array_printf(buf,"#%s",a->str);
      break;
    default:
      fprintf(stderr,"Only STRING and VAR allowed in pattern\n");
      exit(1);
      break;
    }
  }
  ret = buf->data;
  free(buf);
  return ret;
}

static void c_print_rules(FILE *f, rule *r, mapping *mappings)
{
  xmlDocPtr doc;
  elcgen *gen = (elcgen*)calloc(1,sizeof(elcgen));
  char *docstr = "<empty/>";
  int firstif = 1;
  int havedefault = 0;
  gen->buf = array_new(1,0);

  if (NULL == (doc = xmlReadMemory(docstr,strlen(docstr),NULL,NULL,0))) {
    fprintf(stderr,"Error parsing empty doc\n");
    exit(1);
  }
  gen->parse_doc = doc;

  for (; r; r = r->next) {
    char *strpattern = flatten_syntax(r->pattern);
    if ('<' == strpattern[0]) {
    }
    else if (0 < strlen(strpattern)) {
      expression *expr = parse_xpath(gen,NULL,strpattern);
      if (NULL == expr) {
        fprintf(stderr,"%s\n",gen->error);
        exit(1);
      }
      syntax *a;
      if (havedefault) {
        fprintf(stderr,"%s: Default rule is not at end\n",r->name);
        exit(1);
      }


      char *tmp = flatten_syntax(r->pattern);
      printf("%s: %s\n",r->name,tmp);
      free(tmp);



      if ((XPATH_DSVAR == expr->type) &&
          !have_dsvar_type(expr->str)) {
        fprintf(f,"  else {\n");
        havedefault = 1;
      }
      else {
        if (firstif)
          fprintf(f,"  if (");
        else
          fprintf(f,"  else if (");
        firstif = 0;
        gen_tests(f,1,"expr","",expr);

        fprintf(f,") {\n");
      }

      for (a = r->action; a; a = a->next) {
        switch (a->type) {
        case SYNTAX_STRING:
          printf("  action: string %s\n",escape(a->str));
          if ('\n' == a->str[0]) {
            char *esc = escape(a->str+1);
            fprintf(f,"    gen_iprintf(gen,indent,\"%s\");\n",esc);
            free(esc);
          }
          else {
            char *esc = escape(a->str);
            fprintf(f,"    gen_printf(gen,\"%s\");\n",esc);
            free(esc);
          }
          break;
        case SYNTAX_VAR: {
          expression *v = get_symexpr("expr","",expr,a->str);
          char *ref = get_symref("expr","",expr,a->str);
          assert(v);
          printf("  action: var %s (type %s)\n",a->str,expr_names[v->type]);
          switch (v->type) {
          case XPATH_DSVAR_VAR_REF: {
            fprintf(f,"    gen_printf(gen,\"%%s\",%s->target->ident);\n",ref);
            break;
          }
          case XPATH_FUNCTION_CALL:
            fprintf(f,"    r = r && compile_user_function_call(gen,indent,expr,1);\n");
            break;
          case XPATH_DSVAR_NUMERIC: {
            fprintf(f,"    gen_printf(gen,\"%%f\",%s->num);\n",ref);
            break;
          }
          case XPATH_DSVAR_STRING: {
            fprintf(f,"    char *esc = escape(%s->str);\n",ref);
            fprintf(f,"    gen_printf(gen,\"%%s\",esc);\n");
            fprintf(f,"    free(esc);\n");
            break;
          }
          default:
            fprintf(stderr,"unexpected type in pattern: %s\n",expr_names[v->type]);
            abort();
            break;
          }
          free(ref);
          break;
        }
        case SYNTAX_CALL: {
          printf("  action: call %s\n",a->name);
          mapping *m;
          if (NULL == (m = get_mapping(mappings,a->name))) {
            fprintf(stderr,"No mapping for %s\n",a->name);
            exit(1);
          }

          if (0 < a->indent)
            fprintf(f,"    r = r && %s(gen,indent+%d,",m->fun,a->indent);
          else
            fprintf(f,"    r = r && %s(gen,indent,",m->fun);

          if (!strcmp(a->vn->name,"*")) {
            fprintf(f,"expr");
          }
          else {
            char *ref = get_symref("expr","",expr,a->str);
            if (ref) {
              fprintf(f,"%s",ref);
            }
            else {
              printf(" ** could not resolve var: \"%s\"\n",a->str);
              fprintf(f,"NULL"); /* FIXME: temp */
            }
            free(ref);
          }

          fprintf(f,");\n");
          break;
        }
        default:
          abort();
          break;
        }
      }

      fprintf(f,"  }\n");

      free_expression(expr);
    }
    free(strpattern);
  }

  if (!havedefault) {
    fprintf(f,"  else {\n");
    fprintf(f,"    return gen_error(gen,\"Unexpected expression type: %%s\","
            "expr_names[expr->type]);\n");
    fprintf(f,"  }\n");
  }
  fprintf(f,"  return r;\n");

  xmlFreeDoc(doc);
  array_free(gen->buf);
  free(gen);
}

static char *latex_escape(const char *str, int first)
{
  const char *c;
  char *res = (char*)malloc(2*strlen(str)+1);
  char *w = res;
  int skip = 0;
  for (c = str; *c; c++) {
    if (skip) {
      if (':' == *c)
        skip = 0;
      continue;
    }

    if ('#' == *c) {
      skip = 1;
    }
    else if (('\\' == *c) || ('{' == *c) || ('}' == *c) || ('$' == *c) || ('^' == *c) ||
        ('_' == *c) || ('%' == *c) || ('~' == *c) || ('#' == *c) || ('&' == *c)) {
      *(w++) = '\\';
      *(w++) = *c;
    }
    else if (('\n' == *c) && (!first || (c != str))) {
      *(w++) = '\\';
      *(w++) = '\\';
    }
    else if (' ' == *c) {
      *(w++) = '~';
    }
    else if ('\n' != *c) {
      *(w++) = *c;
    }
  }
  *w = '\0';
  return res;
}

static void latex_print_var(FILE *f, varname *vn)
{
  char *esc;

  esc = latex_escape(vn->name,0);
  fprintf(f,"%s",esc);
  free(esc);

  if (vn->suffix || vn->type) {
    fprintf(f,"$");
    if (vn->suffix) {
      esc = latex_escape(vn->suffix,0);
      fprintf(f,"_{%s}",esc);
      free(esc);
    }
    if (vn->type) {
      esc = latex_escape(vn->type,0);
      fprintf(f,"^{%s}",esc);
      free(esc);
    }
    fprintf(f,"$");
  }
}

static void latex_print_pattern(FILE *f, syntax *a, rule *r)
{
  syntax *firsta = a;
  int first = 1;
  for (; a; a = a->next) {
    switch (a->type) {
    case SYNTAX_STRING: {
      char *esc = latex_escape(a->str,first);
      fprintf(f,"\\texttt{\\small{%s}}",esc);
      free(esc);
      break;
    }
    case SYNTAX_CALL: {
      char *escname = latex_escape(a->name,first);
      fprintf(f,"\\textbf{%s}{[}\\!{[}",escname);

      if (!strcmp(a->vn->name,"*") && (firsta != r->pattern))
        latex_print_pattern(f,r->pattern,r);
      else
        latex_print_var(f,a->vn);

      fprintf(f,"{]}\\!{]}");
      free(escname);
      break;
    }
    case SYNTAX_VAR:
      latex_print_var(f,a->vn);
      break;
    default:
      abort();
      break;
    }
    first = 0;
  }
}

static void latex_print_rules(FILE *f, rule *r)
{
  double divider = 0.4;
  fprintf(f,"\\begin{tabular}[t]{p{%.1f\\columnwidth}@{ = }p{%.1f\\columnwidth}}\n",
          divider,1.0-divider);
  for (; r; r = r->next) {
    char *escrname = latex_escape(r->name,1);

    fprintf(f,"\\begin{minipage}[s]{%.1f\\columnwidth}\n",divider);
    fprintf(f,"\\begin{flushleft}\n");
    fprintf(f,"\\textbf{%s}{[}\\!{[}",escrname);

    latex_print_pattern(f,r->pattern,r);

    fprintf(f,"{]}\\!{]}");
    fprintf(f,"\\end{flushleft}\n");
    fprintf(f,"\\end{minipage}\n");

    fprintf(f," & ");
    free(escrname);

    fprintf(f,"\\begin{minipage}[s]{%.1f\\columnwidth}\n",1.0-divider);
    fprintf(f,"\\begin{flushleft}\n");

    latex_print_pattern(f,r->action,r);
    fprintf(f,"\\end{flushleft}\n");
    fprintf(f,"\\end{minipage}\n");
    fprintf(f,"\\\\\n");
  }
  fprintf(f,"\\end{tabular}\n");
}

static ruleset *find_ruleset(ruleset **sets, const char *name)
{
  ruleset *rs;
  for (rs = *sets; rs; rs = rs->next)
    if (!strcmp(rs->name,name))
      return rs;
  return NULL;
}

static ruleset *add_ruleset(ruleset **sets, const char *name)
{
  ruleset *rs = (ruleset*)calloc(1,sizeof(ruleset));
  ruleset **ptr = sets;
  rs->name = strdup(name);
  rs->next = NULL;

  while (*ptr)
    ptr = &((*ptr)->next);
  *ptr = rs;

  return rs;
}

static ruleset *make_ruleset(rule *rules)
{
  ruleset *sets = NULL;
  rule *r = rules;
  while (r) {
    rule *next = r->next;
    ruleset *rs = find_ruleset(&sets,r->name);
    rule **rptr;
    if (NULL == rs)
      rs = add_ruleset(&sets,r->name);

    rptr = &rs->rules;
    while (*rptr)
      rptr = &((*rptr)->next);
    *rptr = r;
    r->next = NULL;

    r = next;
  }
  return sets;
}

int main(int argc, char **argv)
{
  int r;
  char *specfilename;
  char *texfilename;
  char *cfilename;
  FILE *texf;
  FILE *cf;

  setbuf(stdout,NULL);

  if (4 > argc) {
    fprintf(stderr,"Usage: process <specfile> <texfile> <cfile>\n");
    return 1;
  }

  specfilename = argv[1];
  texfilename = argv[2];
  cfilename = argv[3];
  dsfilename = specfilename;

  if (NULL == (dsin = fopen(specfilename,"r"))) {
    perror(specfilename);
    return 1;
  }

  if (NULL == (texf = fopen(texfilename,"w"))) {
    perror(texfilename);
    return 1;
  }

  if (NULL == (cf = fopen(cfilename,"w"))) {
    perror(cfilename);
    return 1;
  }

  r = dsparse();
  fclose(dsin);

  if (0 == r) {
    ruleset *sets = make_ruleset(parse_rules);
    ruleset *rs;

    fprintf(cf,"#include \"cxslt.h\"\n");
    fprintf(cf,"#include <assert.h>\n");
    fprintf(cf,"#include <string.h>\n");

    for (rs = sets; rs; rs = rs->next) {
      fprintf(texf,"\\section*{%s rule}\n",rs->name);
      latex_print_rules(texf,rs->rules);

      mapping *m;
      if (NULL == (m = get_mapping(parse_mappings,rs->name))) {
        fprintf(stderr,"No mapping for %s\n",rs->name);
        exit(1);
      }

      fprintf(cf,"\nint %s(elcgen *gen, int indent, expression *expr)\n",m->fun);
      fprintf(cf,"{\n");
      fprintf(cf,"  int r = 1;\n");
      fprintf(cf,"\n");
      c_print_rules(cf,rs->rules,parse_mappings);
      fprintf(cf,"}\n");
    }
  }

  fclose(texf);
  fclose(cf);

  return r;
}
