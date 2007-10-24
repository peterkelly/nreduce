#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <assert.h>
#include <libxml/parser.h>
#include "ds.h"

extern FILE *dsin;
extern const char *dsfilename;
int dsparse();

rule *parse_rules = NULL;
mapping *parse_mappings = NULL;

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
      if (!strncmp(a->str,"Y",1)) /* FIXME: hack */
        array_printf(buf,"_Y");
      else
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

static void c_print_action_string(FILE *f, syntax *a)
{
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
}

static void c_print_action_var(FILE *f, expression *expr, syntax *a)
{
  expression *v = get_symexpr("expr","",expr,a->str);
  char *ref = get_symref("expr","",expr,a->str);
  if (NULL == v)
    fatal("Could not find variable %s",a->str);
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
  case XPATH_DSVAR_STRING:
  case XSLT_DSVAR_TEXT:
  case XSLT_DSVAR_TEXTINSTR:
    fprintf(f,"    char *esc = escape(%s->str);\n",ref);
    fprintf(f,"    gen_printf(gen,\"%%s\",esc);\n");
    fprintf(f,"    free(esc);\n");
    break;
  default:
    fprintf(stderr,"unexpected type in pattern: %s\n",expr_names[v->type]);
    abort();
    break;
  }
  free(ref);
}

static void c_print_action_call(FILE *f, expression *expr, mapping *mappings, syntax *a)
{
  mapping *m;
  char *ref = NULL;
  printf("  action: call %s\n",a->name);
  if (NULL == (m = get_mapping(mappings,a->name))) {
    fprintf(stderr,"No mapping for %s\n",a->name);
    exit(1);
  }

  if (0 < a->indent)
    fprintf(f,"    r = r && %s(gen,indent+%d,",m->fun,a->indent);
  else
    fprintf(f,"    r = r && %s(gen,indent,",m->fun);

  if (!strcmp(a->vn->name,"*"))
    fprintf(f,"expr");
  else if (NULL != (ref = get_symref("expr","",expr,a->str)))
    fprintf(f,"%s",ref);
  else
    fatal("could not resolve var: \"%s\"",a->str);
  fprintf(f,");\n");

  free(ref);
}

static expression *parse_xslt_str(elcgen *gen, const char *str)
{
  expression *res = NULL;
  xmlDocPtr doc;
  char *pre =
    "<stylesheet"
    " xmlns=\"http://www.w3.org/1999/XSL/Transform\""
    " version=\"2.0\">"
    "<template match=\"/\">";
  char *post = "</template></stylesheet>";
  char *source = (char*)malloc(strlen(pre)+strlen(str)+strlen(post)+1);
  xmlNodePtr root;
  sprintf(source,"%s%s%s",pre,str,post);
  if (NULL == (doc = xmlReadMemory(source,strlen(source),NULL,NULL,0))) {
    gen_error(gen,"Error parsing XSLT code");
  }
  else {
    expression *tree;
    gen->parse_doc = doc;
    gen->ispattern = 1;
    root = xmlDocGetRootElement(doc);
    if (build_xslt_tree(gen,root,&tree)) {
      assert(XSLT_TEMPLATE == tree->r.children->type);
      assert(NULL == tree->r.children->next);
      res = tree->r.children->r.children;

/*       expr_print_tree(gen,0,tree); */
/*       printf("\n%s\n\n",gen->buf->data); */
    }
  }
  free(source);
  /* FIXME: free the doc afterwards */
  return res;
}

static void c_print_rules(FILE *f, rule *r, mapping *mappings, char *language)
{
  xmlDocPtr doc;
  elcgen *gen = (elcgen*)calloc(1,sizeof(elcgen));
  char *docstr = "<empty/>";
  int firstif = 1;
  int havedefault = 0;
  gen->buf = array_new(1,0);
  gen->typestack = stack_new();

  if (NULL == (doc = xmlReadMemory(docstr,strlen(docstr),NULL,NULL,0))) {
    fprintf(stderr,"Error parsing empty doc\n");
    exit(1);
  }
  gen->parse_doc = doc;

  for (; r; r = r->next) {
    char *strpattern = flatten_syntax(r->pattern);
    expression *expr;
    syntax *a;
    int nulltests = 0;
    if (!strcmp(language,"xslt")) {
      expr = parse_xslt_str(gen,strpattern);
      nulltests = 1;
    }
    else if (!strcmp(language,"xpath")) {
      expr = parse_xpath(gen,NULL,strpattern);
    }
    else {
      fatal("Unknown source language: %s",language);
    }

    expr_print_tree(gen,0,expr);
    printf("\n%s\n\n",gen->buf->data);

    if (NULL == expr)
      fatal("%s",gen->error);

    if (havedefault)
      fatal("%s: Default rule is not at end",r->name);

    printf("%s: %s\n",r->name,strpattern);

    if ((XPATH_DSVAR == expr->type) && !have_dsvar_type(expr->str)) {
      fprintf(f,"  else {\n");
      havedefault = 1;
    }
    else {
      if (firstif)
        fprintf(f,"  if (");
      else
        fprintf(f,"  else if (");
      firstif = 0;

      gen_tests(f,1,"expr","",expr,nulltests);

      fprintf(f,") {\n");
    }

    for (a = r->action; a; a = a->next) {
      switch (a->type) {
      case SYNTAX_STRING:
        c_print_action_string(f,a);
        break;
      case SYNTAX_VAR:
        c_print_action_var(f,expr,a);
        break;
      case SYNTAX_CALL:
        c_print_action_call(f,expr,mappings,a);
        break;
      case SYNTAX_PRINTORIG:
        fprintf(f,"    gen_printorig(gen,indent,expr);\n");
        break;
      default:
        abort();
        break;
      }
    }

    fprintf(f,"  }\n");

    free_expression(expr);
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
  stack_free(gen->typestack);
  array_free(gen->buf);
  free(gen);
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
      c_print_rules(cf,rs->rules,parse_mappings,m->language);
      fprintf(cf,"}\n");
    }
  }

  fclose(texf);
  fclose(cf);

  return r;
}
