#ifndef _CXSLT_H
#define _CXSLT_H

#include <libxml/tree.h>

#define XSLT_NAMESPACE "http://www.w3.org/1999/XSL/Transform"
#define WSDL_NAMESPACE "http://schemas.xmlsoap.org/wsdl/"
#define WSDLSOAP_NAMESPACE "http://schemas.xmlsoap.org/wsdl/soap/"
#define XMLSCHEMA_NAMESPACE "http://www.w3.org/2001/XMLSchema"
#define SOAPENV_NAMESPACE "http://schemas.xmlsoap.org/soap/envelope/"

#define xmlStrcmp(a,b) xmlStrcmp((xmlChar*)(a),(xmlChar*)(b))
#define xmlStrdup(a) ((char*)xmlStrdup((xmlChar*)(a)))
#define xmlGetProp(a,b) ((char*)xmlGetProp(a,(xmlChar*)(b)))
#define xmlNodeListGetString(doc,list,inLine) (char*)xmlNodeListGetString(doc,list,inLine)

/* util */

typedef struct list list;
struct list {
  void *data;
  list *next;
};

typedef void (*list_d_t)(void *a);
typedef void* (*list_copy_t)(void *a);

list *list_new(void *data, list *next);
list *list_copy(list *orig, list_copy_t copy);
void list_append(list **l, void *data);
void list_push(list **l, void *data);
void *list_pop(list **l);
int list_count(list *l);
void *list_item(list *l, int item);
void list_free(list *l, list_d_t d);

int list_contains_string(list *l, const char *str);
int list_contains_ptr(list *l, const void *data);
void list_remove_ptr(list **l, void *ptr);

void fatal(const char *format, ...);

/* cxslt */

#define XPATH_INVALID                     0
#define XPATH_FOR                         1
#define XPATH_SOME                        2
#define XPATH_EVERY                       3
#define XPATH_IF                          4
#define XPATH_VAR_IN                      5
#define XPATH_OR                          6
#define XPATH_AND                         7

#define XPATH_GENERAL_EQ                  8
#define XPATH_GENERAL_NE                  9
#define XPATH_GENERAL_LT                  10
#define XPATH_GENERAL_LE                  11
#define XPATH_GENERAL_GT                  12
#define XPATH_GENERAL_GE                  13

#define XPATH_VALUE_EQ                    14
#define XPATH_VALUE_NE                    15
#define XPATH_VALUE_LT                    16
#define XPATH_VALUE_LE                    17
#define XPATH_VALUE_GT                    18
#define XPATH_VALUE_GE                    19

#define XPATH_NODE_IS                     20
#define XPATH_NODE_PRECEDES               21
#define XPATH_NODE_FOLLOWS                22

#define XPATH_TO                          23
#define XPATH_ADD                         24
#define XPATH_SUBTRACT                    25
#define XPATH_MULTIPLY                    26
#define XPATH_DIVIDE                      27
#define XPATH_IDIVIDE                     28
#define XPATH_MOD                         29
#define XPATH_UNION                       30
#define XPATH_UNION2                      31
#define XPATH_INTERSECT                   32
#define XPATH_EXCEPT                      33
#define XPATH_INSTANCE_OF                 34
#define XPATH_TREAT                       35
#define XPATH_CASTABLE                    36
#define XPATH_CAST                        37
#define XPATH_UNARY_MINUS                 38
#define XPATH_UNARY_PLUS                  39
#define XPATH_ROOT                        40
#define XPATH_STRING_LITERAL              41
#define XPATH_INTEGER_LITERAL             42
#define XPATH_DECIMAL_LITERAL             43
#define XPATH_DOUBLE_LITERAL              44
#define XPATH_VAR_REF                     45
#define XPATH_EMPTY                       46
#define XPATH_CONTEXT_ITEM                47
#define XPATH_KIND_TEST                   48
#define XPATH_ACTUAL_PARAM                49
#define XPATH_FUNCTION_CALL               50
#define XPATH_PAREN                       51
#define XPATH_ATOMIC_TYPE                 52
#define XPATH_ITEM_TYPE                   53
#define XPATH_SEQUENCE                    54
#define XPATH_STEP                        55
#define XPATH_VARINLIST                   56
#define XPATH_PARAMLIST                   57
#define XPATH_FILTER                      58
#define XPATH_NAME_TEST                   59

#define XPATH_AVT_COMPONENT               60

#define AXIS_INVALID                      0
#define AXIS_CHILD                        1
#define AXIS_DESCENDANT                   2
#define AXIS_ATTRIBUTE                    3
#define AXIS_SELF                         4
#define AXIS_DESCENDANT_OR_SELF           5
#define AXIS_FOLLOWING_SIBLING            6
#define AXIS_FOLLOWING                    7
#define AXIS_NAMESPACE                    8
#define AXIS_PARENT                       9
#define AXIS_ANCESTOR                     10
#define AXIS_PRECEDING_SIBLING            11
#define AXIS_PRECEDING                    12
#define AXIS_ANCESTOR_OR_SELF             13
#define AXIS_COUNT                        14

#define SEQTYPE_INVALID               0
#define SEQTYPE_ITEM                  1
#define SEQTYPE_OCCURRENCE            2
#define SEQTYPE_ALL                   3
#define SEQTYPE_SEQUENCE              4
#define SEQTYPE_CHOICE                5
#define SEQTYPE_EMPTY                 6

#define KIND_DOCUMENT                 1
#define KIND_ELEMENT                  2
#define KIND_ATTRIBUTE                3
#define KIND_SCHEMA_ELEMENT           4
#define KIND_SCHEMA_ATTRIBUTE         5
#define KIND_PI                       6
#define KIND_COMMENT                  7
#define KIND_TEXT                     8
#define KIND_ANY                      9

#define OCCURS_ONCE                   0
#define OCCURS_OPTIONAL               1
#define OCCURS_ZERO_OR_MORE           2
#define OCCURS_ONE_OR_MORE            3

typedef struct qname {
  char *uri;
  char *prefix;
  char *localpart;
} qname;

typedef struct expression {
  int type;
  struct expression *left;
  struct expression *right;
  qname qn;
  int axis;
  double num;
  char *str;
  int kind;
} expression;

typedef struct seqtype {
} seqtype;

expression *new_expression(int type);
expression *new_expression2(int type, expression *left, expression *right);
void free_expression(expression *expr);
const char *lookup_nsuri(xmlNodePtr n, const char *prefix);

expression *new_TypeExpr(int type, seqtype *st, expression *right);
expression *new_ForExpr(expression *left, expression *right);
expression *new_VarInExpr(expression *left);
expression *new_VarInListExpr(expression *left, expression *right);
expression *new_QuantifiedExpr(int type, expression *left, expression *right);
expression *new_XPathIfExpr(expression *cond, expression *tbranch, expression *fbranch);
expression *new_RootExpr(expression *left);

typedef struct {
  char *filename;
  xmlDocPtr doc;
  xmlNodePtr root;
} wsdlfile;

typedef struct {
  list *wsdlfiles;
  xmlNodePtr toplevel;
} elcgen;

qname string_to_qname(const char *str, xmlNodePtr n);
void free_qname(qname qn);

/* wsdl */

wsdlfile *process_wsdl(elcgen *gen, const char *filename);
xmlNodePtr wsdl_get_object(wsdlfile *wf, const char *type, const char *name);
char *wsdl_get_url(wsdlfile *wf);
void wsdl_get_operation_messages(wsdlfile *wf, const char *opname,
				 qname *inqn, qname *outqn,
				 list **inargs, list **outargs);

#endif
