#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <assert.h>
#include <libxml/parser.h>
#include "ds.h"

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
/*     else if (' ' == *c) { */
/*       *(w++) = '~'; */
/*     } */
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
    case SYNTAX_PRINTORIG:
      /* invisible */
      break;
    default:
      abort();
      break;
    }
    first = 0;
  }
}

void latex_print_rules(FILE *f, rule *r)
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
