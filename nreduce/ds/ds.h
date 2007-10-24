#ifndef _DS_H
#define _DS_H

#include "cxslt/cxslt.h"

#define SYNTAX_STRING     0
#define SYNTAX_CALL       1
#define SYNTAX_VAR        2
#define SYNTAX_PRINTORIG  3

typedef struct varname {
  char *name;
  char *suffix;
  char *type;
} varname;

typedef struct syntax {
  int type;
  char *name;
  char *str;
  varname *vn;
  int indent;
  struct syntax *next;
} syntax;

typedef struct rule {
  char *name;
  syntax *pattern;
  syntax *action;
  struct rule *next;
} rule;

typedef struct ruleset {
  char *name;
  rule *rules;
  struct ruleset *next;
} ruleset;

typedef struct mapping {
  char *name;
  char *language;
  char *fun;
  struct mapping *next;
} mapping;

char *varname_str(varname *vn);

/* latex */

void latex_print_rules(FILE *f, rule *r);

/* support */

expression *get_symexpr(const char *base, const char *suffix, expression *expr, const char *name);
char *get_symref(const char *base, const char *suffix, expression *expr, const char *name);
mapping *get_mapping(mapping *mappings, const char *name);
int have_dsvar_type(const char *dsvar);
void gen_tests(FILE *f, int first, const char *base, const char *suffix,
               expression *expr, int nulltests);

#endif
