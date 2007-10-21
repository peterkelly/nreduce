#ifndef _DS_H
#define _DS_H

#define SYNTAX_STRING     0
#define SYNTAX_CALL       1
#define SYNTAX_VAR        2

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
  char *fun;
  struct mapping *next;
} mapping;

char *varname_str(varname *vn);

#endif
