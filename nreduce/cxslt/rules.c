#include "cxslt.h"
#include <assert.h>
#include <string.h>

int compile_expression(elcgen *gen, int indent, expression *expr)
{
  int r = 1;

  if ((RESTYPE_NUMBER == expr->restype)) {
    gen_iprintf(gen,indent,"(cons (xml::mknumber ");
    r = r && compile_num_expression(gen,indent,expr);
    gen_printf(gen,") nil)");
  }
  else if ((XPATH_NUMERIC_LITERAL == expr->type)) {
    gen_iprintf(gen,indent,"(cons (xml::mknumber ");
    gen_printf(gen,"%f",expr->num);
    gen_printf(gen,") nil)");
  }
  else if ((XPATH_STRING_LITERAL == expr->type)) {
    gen_iprintf(gen,indent,"(cons (xml::mkstring \"");
    char *esc = escape(expr->str);
    gen_printf(gen,"%s",esc);
    free(esc);
    gen_printf(gen,"\") nil)");
  }
  else if ((XPATH_VAR_REF == expr->type)) {
    gen_printf(gen,"%s",expr->target->ident);
  }
  else if ((XPATH_AND == expr->type)) {
    gen_iprintf(gen,indent,"(cons (xml::mkbool ");
    r = r && compile_ebv_expression(gen,indent,expr);
    gen_printf(gen,") nil)");
  }
  else if ((XPATH_OR == expr->type)) {
    gen_iprintf(gen,indent,"(cons (xml::mkbool ");
    r = r && compile_ebv_expression(gen,indent,expr);
    gen_printf(gen,") nil)");
  }
  else if ((XPATH_GENERAL_EQ == expr->type)) {
    gen_iprintf(gen,indent,"(cons (xml::mkbool ");
    r = r && compile_ebv_expression(gen,indent,expr);
    gen_printf(gen,") nil)");
  }
  else if ((XPATH_GENERAL_NE == expr->type)) {
    gen_iprintf(gen,indent,"(cons (xml::mkbool ");
    r = r && compile_ebv_expression(gen,indent,expr);
    gen_printf(gen,") nil)");
  }
  else if ((XPATH_GENERAL_LT == expr->type)) {
    gen_iprintf(gen,indent,"(cons (xml::mkbool ");
    r = r && compile_ebv_expression(gen,indent,expr);
    gen_printf(gen,") nil)");
  }
  else if ((XPATH_GENERAL_LE == expr->type)) {
    gen_iprintf(gen,indent,"(cons (xml::mkbool ");
    r = r && compile_ebv_expression(gen,indent,expr);
    gen_printf(gen,") nil)");
  }
  else if ((XPATH_GENERAL_GT == expr->type)) {
    gen_iprintf(gen,indent,"(cons (xml::mkbool ");
    r = r && compile_ebv_expression(gen,indent,expr);
    gen_printf(gen,") nil)");
  }
  else if ((XPATH_GENERAL_GE == expr->type)) {
    gen_iprintf(gen,indent,"(cons (xml::mkbool ");
    r = r && compile_ebv_expression(gen,indent,expr);
    gen_printf(gen,") nil)");
  }
  else if ((XPATH_VALUE_EQ == expr->type)) {
    gen_iprintf(gen,indent,"(xslt::value_eq ");
    r = r && compile_expression(gen,indent+1,expr->left);
    gen_printf(gen," ");
    r = r && compile_expression(gen,indent+1,expr->right);
    gen_printf(gen,")");
  }
  else if ((XPATH_VALUE_NE == expr->type)) {
    gen_iprintf(gen,indent,"(xslt::value_ne ");
    r = r && compile_expression(gen,indent+1,expr->left);
    gen_printf(gen," ");
    r = r && compile_expression(gen,indent+1,expr->right);
    gen_printf(gen,")");
  }
  else if ((XPATH_VALUE_LT == expr->type)) {
    gen_iprintf(gen,indent,"(xslt::value_lt ");
    r = r && compile_expression(gen,indent+1,expr->left);
    gen_printf(gen," ");
    r = r && compile_expression(gen,indent+1,expr->right);
    gen_printf(gen,")");
  }
  else if ((XPATH_VALUE_LE == expr->type)) {
    gen_iprintf(gen,indent,"(xslt::value_le ");
    r = r && compile_expression(gen,indent+1,expr->left);
    gen_printf(gen," ");
    r = r && compile_expression(gen,indent+1,expr->right);
    gen_printf(gen,")");
  }
  else if ((XPATH_VALUE_GT == expr->type)) {
    gen_iprintf(gen,indent,"(xslt::value_gt ");
    r = r && compile_expression(gen,indent+1,expr->left);
    gen_printf(gen," ");
    r = r && compile_expression(gen,indent+1,expr->right);
    gen_printf(gen,")");
  }
  else if ((XPATH_VALUE_GE == expr->type)) {
    gen_iprintf(gen,indent,"(xslt::value_ge ");
    r = r && compile_expression(gen,indent+1,expr->left);
    gen_printf(gen," ");
    r = r && compile_expression(gen,indent+1,expr->right);
    gen_printf(gen,")");
  }
  else if ((XPATH_ADD == expr->type)) {
    gen_iprintf(gen,indent,"(xslt::add ");
    r = r && compile_expression(gen,indent+1,expr->left);
    gen_printf(gen," ");
    r = r && compile_expression(gen,indent+1,expr->right);
    gen_printf(gen,")");
  }
  else if ((XPATH_SUBTRACT == expr->type)) {
    gen_iprintf(gen,indent,"(xslt::subtract ");
    r = r && compile_expression(gen,indent+1,expr->left);
    gen_printf(gen," ");
    r = r && compile_expression(gen,indent+1,expr->right);
    gen_printf(gen,")");
  }
  else if ((XPATH_MULTIPLY == expr->type)) {
    gen_iprintf(gen,indent,"(xslt::multiply ");
    r = r && compile_expression(gen,indent+1,expr->left);
    gen_printf(gen," ");
    r = r && compile_expression(gen,indent+1,expr->right);
    gen_printf(gen,")");
  }
  else if ((XPATH_DIVIDE == expr->type)) {
    gen_iprintf(gen,indent,"(xslt::divide ");
    r = r && compile_expression(gen,indent+1,expr->left);
    gen_printf(gen," ");
    r = r && compile_expression(gen,indent+1,expr->right);
    gen_printf(gen,")");
  }
  else if ((XPATH_IDIVIDE == expr->type)) {
    gen_iprintf(gen,indent,"(xslt::idivide ");
    r = r && compile_expression(gen,indent+1,expr->left);
    gen_printf(gen," ");
    r = r && compile_expression(gen,indent+1,expr->right);
    gen_printf(gen,")");
  }
  else if ((XPATH_MOD == expr->type)) {
    gen_iprintf(gen,indent,"(xslt::mod ");
    r = r && compile_expression(gen,indent+1,expr->left);
    gen_printf(gen," ");
    r = r && compile_expression(gen,indent+1,expr->right);
    gen_printf(gen,")");
  }
  else if ((XPATH_UNARY_MINUS == expr->type)) {
    gen_iprintf(gen,indent,"(xslt::uminus ");
    r = r && compile_expression(gen,indent+1,expr->left);
    gen_printf(gen,")");
  }
  else if ((XPATH_UNARY_PLUS == expr->type)) {
    gen_iprintf(gen,indent,"(xslt::uplus ");
    r = r && compile_expression(gen,indent+1,expr->left);
    gen_printf(gen,")");
  }
  else if ((XPATH_IF == expr->type)) {
    gen_iprintf(gen,indent,"(if ");
    r = r && compile_ebv_expression(gen,indent+1,expr->test);
    gen_printf(gen," ");
    r = r && compile_expression(gen,indent+1,expr->left);
    gen_printf(gen," ");
    r = r && compile_expression(gen,indent+1,expr->right);
    gen_printf(gen,")");
  }
  else if ((XPATH_TO == expr->type)) {
    gen_iprintf(gen,indent,"(xslt::range ");
    gen_iprintf(gen,indent,"  (xslt::getnumber ");
    r = r && compile_expression(gen,indent+2,expr->left);
    gen_printf(gen,") ");
    gen_iprintf(gen,indent,"  (xslt::getnumber ");
    r = r && compile_expression(gen,indent+2,expr->right);
    gen_printf(gen,"))");
  }
  else if ((XPATH_CONTEXT_ITEM == expr->type)) {
    gen_iprintf(gen,indent,"(cons citem nil)");
  }
  else if ((XPATH_ROOT == expr->type)) {
    gen_printf(gen,"(cons (xml::item_root citem) nil)");
  }
  else if ((XPATH_EMPTY == expr->type)) {
    gen_printf(gen,"nil");
  }
  else if ((XPATH_SEQUENCE == expr->type)) {
    gen_iprintf(gen,indent,"(append ");
    r = r && compile_expression(gen,indent,expr->left);
    gen_printf(gen," ");
    r = r && compile_expression(gen,indent,expr->right);
    gen_printf(gen,")");
  }
  else if ((XPATH_STEP == expr->type)) {
    if (is_expr_doc_order(expr->left) && is_expr_doc_order(expr->right))
      gen_iprintf(gen,indent,"(xslt::path_result");
    else
      gen_iprintf(gen,indent,"(xslt::path_result_sort");
    gen_iprintf(gen,indent,"  (xslt::apmap3 (!citem.!cpos.!csize.");
    r = r && compile_expression(gen,indent+2,expr->right);
    gen_printf(gen,") ");
    r = r && compile_expression(gen,indent+2,expr->left);
    gen_printf(gen,"))");
  }
  else if ((XPATH_NODE_TEST == expr->type) &&
           (XPATH_AXIS == expr->left->type) &&
           is_forward_axis(expr->left->axis)) {
    gen_iprintf(gen,indent,"(filter ");
    r = r && compile_test(gen,indent+1,expr);
    r = r && compile_axis(gen,indent+1,expr->left);
    gen_printf(gen,")");
  }
  else if ((XPATH_NODE_TEST == expr->type) &&
           (XPATH_AXIS == expr->left->type) &&
           is_reverse_axis(expr->left->axis)) {
    gen_iprintf(gen,indent,"(reverse ");
    gen_iprintf(gen,indent,"  (filter ");
    r = r && compile_test(gen,indent+2,expr);
    r = r && compile_axis(gen,indent+2,expr->left);
    gen_printf(gen,")");
    gen_printf(gen,")");
  }
  else if ((XPATH_FILTER == expr->type) &&
           (XPATH_NODE_TEST == expr->left->type) &&
           (XPATH_AXIS == expr->left->left->type) &&
           is_forward_axis(expr->left->left->axis)) {
    gen_iprintf(gen,indent,"(xslt::filter3");
    gen_iprintf(gen,indent,"  (!citem.!cpos.!csize.");
    r = r && compile_predicate(gen,indent+1,expr->right);
    gen_printf(gen,") ");
    gen_iprintf(gen,indent,"  (filter ");
    r = r && compile_test(gen,indent+2,expr->left);
    r = r && compile_axis(gen,indent+2,expr->left->left);
    gen_printf(gen,")");
    gen_printf(gen,")");
  }
  else if ((XPATH_FILTER == expr->type) &&
           (XPATH_NODE_TEST == expr->left->type) &&
           (XPATH_AXIS == expr->left->left->type) &&
           is_reverse_axis(expr->left->left->axis)) {
    gen_iprintf(gen,indent,"(reverse ");
    gen_iprintf(gen,indent,"  (xslt::filter3");
    gen_iprintf(gen,indent,"    (!citem.!cpos.!csize.");
    r = r && compile_predicate(gen,indent+2,expr->right);
    gen_printf(gen,") ");
    gen_iprintf(gen,indent,"    (filter ");
    r = r && compile_test(gen,indent+3,expr->left);
    r = r && compile_axis(gen,indent+3,expr->left->left);
    gen_printf(gen,")");
    gen_printf(gen,")");
    gen_printf(gen,")");
  }
  else if ((XPATH_FUNCTION_CALL == expr->type)) {
    if (!strcmp(expr->qn.prefix,""))
      return compile_builtin_function_call(gen,indent,expr);
    else if (expr->target)
      return compile_user_function_call(gen,indent,expr,0);
    else if (!strncmp(expr->qn.uri,"wsdl-",5))
      return compile_ws_call(gen,indent,expr,expr->qn.uri+5);
    else
      return gen_error(gen,"Call to non-existent function {%s}%s",
                       expr->qn.uri ? expr->qn.uri : "",expr->qn.localpart);
  }
  else if ((XPATH_FILTER == expr->type)) {
    gen_iprintf(gen,indent,"(xslt::filter3");
    gen_iprintf(gen,indent,"  (!citem.!cpos.!csize.");
    r = r && compile_predicate(gen,indent+1,expr->right);
    gen_printf(gen,") ");
    r = r && compile_expression(gen,indent+1,expr->left);
    gen_printf(gen,")");
  }
  else {
    return gen_error(gen,"Unexpected expression type: %s",expr_names[expr->type]);
  }
  return r;
}

int compile_predicate(elcgen *gen, int indent, expression *expr)
{
  int r = 1;

  if ((XPATH_PREDICATE == expr->type)) {
    gen_iprintf(gen,indent,"(if (xslt::predicate_match cpos ");
    r = r && compile_expression(gen,indent+1,expr->left);
    gen_printf(gen,") ");
    r = r && compile_predicate(gen,indent+1,expr->right);
    gen_printf(gen," nil)");
  }
  else {
    gen_iprintf(gen,indent,"(xslt::predicate_match cpos ");
    r = r && compile_expression(gen,indent+1,expr);
    gen_printf(gen,")");
  }
  return r;
}

int compile_axis(elcgen *gen, int indent, expression *expr)
{
  int r = 1;

  if ((XPATH_AXIS == expr->type) &&
           (AXIS_SELF == expr->axis)) {
    gen_iprintf(gen,indent,"(cons citem nil)");
  }
  else if ((XPATH_AXIS == expr->type) &&
           (AXIS_CHILD == expr->axis)) {
    gen_iprintf(gen,indent,"(xml::item_children citem)");
  }
  else if ((XPATH_AXIS == expr->type) &&
           (AXIS_DESCENDANT == expr->axis)) {
    gen_iprintf(gen,indent,"(xslt::node_descendants citem)");
  }
  else if ((XPATH_AXIS == expr->type) &&
           (AXIS_DESCENDANT_OR_SELF == expr->axis)) {
    gen_iprintf(gen,indent,"(cons citem (xslt::node_descendants citem))");
  }
  else if ((XPATH_AXIS == expr->type) &&
           (AXIS_PARENT == expr->axis)) {
    gen_iprintf(gen,indent,"(xslt::node_parent_list citem)");
  }
  else if ((XPATH_AXIS == expr->type) &&
           (AXIS_ANCESTOR == expr->axis)) {
    gen_iprintf(gen,indent,"(xslt::node_ancestors citem)");
  }
  else if ((XPATH_AXIS == expr->type) &&
           (AXIS_ANCESTOR_OR_SELF == expr->axis)) {
    gen_iprintf(gen,indent,"(xslt::node_ancestors_or_self citem)");
  }
  else if ((XPATH_AXIS == expr->type) &&
           (AXIS_PRECEDING_SIBLING == expr->axis)) {
    gen_iprintf(gen,indent,"(xslt::node_preceding_siblings citem)");
  }
  else if ((XPATH_AXIS == expr->type) &&
           (AXIS_FOLLOWING_SIBLING == expr->axis)) {
    gen_iprintf(gen,indent,"(xslt::node_following_siblings citem)");
  }
  else if ((XPATH_AXIS == expr->type) &&
           (AXIS_PRECEDING == expr->axis)) {
    gen_iprintf(gen,indent,"(xslt::node_preceding citem)");
  }
  else if ((XPATH_AXIS == expr->type) &&
           (AXIS_FOLLOWING == expr->axis)) {
    gen_iprintf(gen,indent,"(xslt::node_following citem)");
  }
  else if ((XPATH_AXIS == expr->type) &&
           (AXIS_ATTRIBUTE == expr->axis)) {
    gen_iprintf(gen,indent,"(xml::item_attributes citem)");
  }
  else if ((XPATH_AXIS == expr->type) &&
           (AXIS_NAMESPACE == expr->axis)) {
    gen_iprintf(gen,indent,"(xml::item_namespaces citem)");
  }
  else {
    return gen_error(gen,"Unexpected expression type: %s",expr_names[expr->type]);
  }
  return r;
}

int compile_test(elcgen *gen, int indent, expression *expr)
{
  assert(XPATH_NODE_TEST == expr->type);
  assert(expr->right);
  assert(expr->left);
  assert(XPATH_AXIS == expr->left->type);
  assert(AXIS_INVALID != expr->left->axis);
  expression *axis = expr->left;
  expr = expr->right;
  assert((XPATH_KIND_TEST == expr->type) || (XPATH_NAME_TEST == expr->type));

  int r = 1;

  if ((XPATH_KIND_TEST == expr->type) &&
           (KIND_DOCUMENT == expr->kind)) {
    gen_printf(gen,"(xslt::type_test xml::TYPE_DOCUMENT)");
  }
  else if ((XPATH_KIND_TEST == expr->type) &&
           (KIND_ELEMENT == expr->kind)) {
    gen_printf(gen,"(xslt::type_test xml::TYPE_ELEMENT)");
  }
  else if ((XPATH_KIND_TEST == expr->type) &&
           (KIND_ATTRIBUTE == expr->kind)) {
    gen_printf(gen,"(xslt::type_test xml::TYPE_ATTRIBUTE)");
  }
  else if ((XPATH_KIND_TEST == expr->type) &&
           (KIND_COMMENT == expr->kind)) {
    gen_printf(gen,"(xslt::type_test xml::TYPE_COMMENT)");
  }
  else if ((XPATH_KIND_TEST == expr->type) &&
           (KIND_TEXT == expr->kind)) {
    gen_printf(gen,"(xslt::type_test xml::TYPE_TEXT)");
  }
  else if ((XPATH_KIND_TEST == expr->type) &&
           (KIND_ANY == expr->kind)) {
    gen_printf(gen,"xslt::any_test");
  }
  else if ((XPATH_NAME_TEST == expr->type)) {
    char *nsuri = expr->qn.uri ? escape(expr->qn.uri) : NULL;
    char *localname = expr->qn.localpart ? escape(expr->qn.localpart) : NULL;
    char *type = (AXIS_ATTRIBUTE == axis->axis) ? "xml::TYPE_ATTRIBUTE" : "xml::TYPE_ELEMENT";

    if (nsuri && localname)
      gen_printf(gen,"(xslt::name_test %s \"%s\" \"%s\")",type,nsuri,localname);
    else if (localname)
      gen_printf(gen,"(xslt::wildcard_uri_test %s \"%s\")",type,localname);
    else if (nsuri)
      gen_printf(gen,"(xslt::wildcard_localname_test %s \"%s\")",type,nsuri);
    else
      gen_printf(gen,"(xslt::type_test %s)",type);
    free(nsuri);
    free(localname);
  }
  else {
    return gen_error(gen,"Unexpected expression type: %s",expr_names[expr->type]);
  }
  return r;
}

int compile_ebv_expression(elcgen *gen, int indent, expression *expr)
{
  int r = 1;

  if ((XPATH_AND == expr->type)) {
    gen_printf(gen,"(&& ");
    r = r && compile_ebv_expression(gen,indent+1,expr->left);
    gen_printf(gen," ");
    r = r && compile_ebv_expression(gen,indent+1,expr->right);
    gen_printf(gen,")");
  }
  else if ((XPATH_OR == expr->type)) {
    gen_printf(gen,"(|| ");
    r = r && compile_ebv_expression(gen,indent+1,expr->left);
    gen_printf(gen," ");
    r = r && compile_ebv_expression(gen,indent+1,expr->right);
    gen_printf(gen,")");
  }
  else if ((XPATH_GENERAL_EQ == expr->type) &&
           (RESTYPE_NUMBER == expr->left->restype) &&
           (RESTYPE_NUMBER == expr->right->restype)) {
    gen_iprintf(gen,indent,"(== ");
    r = r && compile_num_expression(gen,indent+1,expr->left);
    gen_printf(gen," ");
    r = r && compile_num_expression(gen,indent+1,expr->right);
    gen_printf(gen,")");
  }
  else if ((XPATH_GENERAL_NE == expr->type) &&
           (RESTYPE_NUMBER == expr->left->restype) &&
           (RESTYPE_NUMBER == expr->right->restype)) {
    gen_iprintf(gen,indent,"(!= ");
    r = r && compile_num_expression(gen,indent+1,expr->left);
    gen_printf(gen," ");
    r = r && compile_num_expression(gen,indent+1,expr->right);
    gen_printf(gen,")");
  }
  else if ((XPATH_GENERAL_LT == expr->type) &&
           (RESTYPE_NUMBER == expr->left->restype) &&
           (RESTYPE_NUMBER == expr->right->restype)) {
    gen_iprintf(gen,indent,"(< ");
    r = r && compile_num_expression(gen,indent+1,expr->left);
    gen_printf(gen," ");
    r = r && compile_num_expression(gen,indent+1,expr->right);
    gen_printf(gen,")");
  }
  else if ((XPATH_GENERAL_LE == expr->type) &&
           (RESTYPE_NUMBER == expr->left->restype) &&
           (RESTYPE_NUMBER == expr->right->restype)) {
    gen_iprintf(gen,indent,"(<= ");
    r = r && compile_num_expression(gen,indent+1,expr->left);
    gen_printf(gen," ");
    r = r && compile_num_expression(gen,indent+1,expr->right);
    gen_printf(gen,")");
  }
  else if ((XPATH_GENERAL_GT == expr->type) &&
           (RESTYPE_NUMBER == expr->left->restype) &&
           (RESTYPE_NUMBER == expr->right->restype)) {
    gen_iprintf(gen,indent,"(> ");
    r = r && compile_num_expression(gen,indent+1,expr->left);
    gen_printf(gen," ");
    r = r && compile_num_expression(gen,indent+1,expr->right);
    gen_printf(gen,")");
  }
  else if ((XPATH_GENERAL_GE == expr->type) &&
           (RESTYPE_NUMBER == expr->left->restype) &&
           (RESTYPE_NUMBER == expr->right->restype)) {
    gen_iprintf(gen,indent,"(>= ");
    r = r && compile_num_expression(gen,indent+1,expr->left);
    gen_printf(gen," ");
    r = r && compile_num_expression(gen,indent+1,expr->right);
    gen_printf(gen,")");
  }
  else if ((XPATH_VALUE_EQ == expr->type) &&
           (RESTYPE_NUMBER == expr->left->restype) &&
           (RESTYPE_NUMBER == expr->right->restype)) {
    gen_iprintf(gen,indent,"(== ");
    r = r && compile_num_expression(gen,indent+1,expr->left);
    gen_printf(gen," ");
    r = r && compile_num_expression(gen,indent+1,expr->right);
    gen_printf(gen,")");
  }
  else if ((XPATH_VALUE_NE == expr->type) &&
           (RESTYPE_NUMBER == expr->left->restype) &&
           (RESTYPE_NUMBER == expr->right->restype)) {
    gen_iprintf(gen,indent,"(!= ");
    r = r && compile_num_expression(gen,indent+1,expr->left);
    gen_printf(gen," ");
    r = r && compile_num_expression(gen,indent+1,expr->right);
    gen_printf(gen,")");
  }
  else if ((XPATH_VALUE_LT == expr->type) &&
           (RESTYPE_NUMBER == expr->left->restype) &&
           (RESTYPE_NUMBER == expr->right->restype)) {
    gen_iprintf(gen,indent,"(< ");
    r = r && compile_num_expression(gen,indent+1,expr->left);
    gen_printf(gen," ");
    r = r && compile_num_expression(gen,indent+1,expr->right);
    gen_printf(gen,")");
  }
  else if ((XPATH_VALUE_LE == expr->type) &&
           (RESTYPE_NUMBER == expr->left->restype) &&
           (RESTYPE_NUMBER == expr->right->restype)) {
    gen_iprintf(gen,indent,"(<= ");
    r = r && compile_num_expression(gen,indent+1,expr->left);
    gen_printf(gen," ");
    r = r && compile_num_expression(gen,indent+1,expr->right);
    gen_printf(gen,")");
  }
  else if ((XPATH_VALUE_GT == expr->type) &&
           (RESTYPE_NUMBER == expr->left->restype) &&
           (RESTYPE_NUMBER == expr->right->restype)) {
    gen_iprintf(gen,indent,"(> ");
    r = r && compile_num_expression(gen,indent+1,expr->left);
    gen_printf(gen," ");
    r = r && compile_num_expression(gen,indent+1,expr->right);
    gen_printf(gen,")");
  }
  else if ((XPATH_VALUE_GE == expr->type) &&
           (RESTYPE_NUMBER == expr->left->restype) &&
           (RESTYPE_NUMBER == expr->right->restype)) {
    gen_iprintf(gen,indent,"(>= ");
    r = r && compile_num_expression(gen,indent+1,expr->left);
    gen_printf(gen," ");
    r = r && compile_num_expression(gen,indent+1,expr->right);
    gen_printf(gen,")");
  }
  else if ((XPATH_GENERAL_EQ == expr->type)) {
    gen_iprintf(gen,indent,"(xslt::general_eq ");
    r = r && compile_expression(gen,indent+1,expr->left);
    gen_printf(gen," ");
    r = r && compile_expression(gen,indent+1,expr->right);
    gen_printf(gen,")");
  }
  else if ((XPATH_GENERAL_NE == expr->type)) {
    gen_iprintf(gen,indent,"(xslt::general_ne ");
    r = r && compile_expression(gen,indent+1,expr->left);
    gen_printf(gen," ");
    r = r && compile_expression(gen,indent+1,expr->right);
    gen_printf(gen,")");
  }
  else if ((XPATH_GENERAL_LT == expr->type)) {
    gen_iprintf(gen,indent,"(xslt::general_lt ");
    r = r && compile_expression(gen,indent+1,expr->left);
    gen_printf(gen," ");
    r = r && compile_expression(gen,indent+1,expr->right);
    gen_printf(gen,")");
  }
  else if ((XPATH_GENERAL_LE == expr->type)) {
    gen_iprintf(gen,indent,"(xslt::general_le ");
    r = r && compile_expression(gen,indent+1,expr->left);
    gen_printf(gen," ");
    r = r && compile_expression(gen,indent+1,expr->right);
    gen_printf(gen,")");
  }
  else if ((XPATH_GENERAL_GT == expr->type)) {
    gen_iprintf(gen,indent,"(xslt::general_gt ");
    r = r && compile_expression(gen,indent+1,expr->left);
    gen_printf(gen," ");
    r = r && compile_expression(gen,indent+1,expr->right);
    gen_printf(gen,")");
  }
  else if ((XPATH_GENERAL_GE == expr->type)) {
    gen_iprintf(gen,indent,"(xslt::general_ge ");
    r = r && compile_expression(gen,indent+1,expr->left);
    gen_printf(gen," ");
    r = r && compile_expression(gen,indent+1,expr->right);
    gen_printf(gen,")");
  }
  else if ((XPATH_VALUE_EQ == expr->type)) {
    gen_iprintf(gen,indent,"(xslt::value_eq_ebv ");
    r = r && compile_expression(gen,indent+1,expr->left);
    gen_printf(gen," ");
    r = r && compile_expression(gen,indent+1,expr->right);
    gen_printf(gen,")");
  }
  else if ((XPATH_VALUE_NE == expr->type)) {
    gen_iprintf(gen,indent,"(xslt::value_ne_ebv ");
    r = r && compile_expression(gen,indent+1,expr->left);
    gen_printf(gen," ");
    r = r && compile_expression(gen,indent+1,expr->right);
    gen_printf(gen,")");
  }
  else if ((XPATH_VALUE_LT == expr->type)) {
    gen_iprintf(gen,indent,"(xslt::value_lt_ebv ");
    r = r && compile_expression(gen,indent+1,expr->left);
    gen_printf(gen," ");
    r = r && compile_expression(gen,indent+1,expr->right);
    gen_printf(gen,")");
  }
  else if ((XPATH_VALUE_LE == expr->type)) {
    gen_iprintf(gen,indent,"(xslt::value_le_ebv ");
    r = r && compile_expression(gen,indent+1,expr->left);
    gen_printf(gen," ");
    r = r && compile_expression(gen,indent+1,expr->right);
    gen_printf(gen,")");
  }
  else if ((XPATH_VALUE_GT == expr->type)) {
    gen_iprintf(gen,indent,"(xslt::value_gt_ebv ");
    r = r && compile_expression(gen,indent+1,expr->left);
    gen_printf(gen," ");
    r = r && compile_expression(gen,indent+1,expr->right);
    gen_printf(gen,")");
  }
  else if ((XPATH_VALUE_GE == expr->type)) {
    gen_iprintf(gen,indent,"(xslt::value_ge_ebv ");
    r = r && compile_expression(gen,indent+1,expr->left);
    gen_printf(gen," ");
    r = r && compile_expression(gen,indent+1,expr->right);
    gen_printf(gen,")");
  }
  else {
    gen_iprintf(gen,indent,"(xslt::ebv ");
    r = r && compile_expression(gen,indent,expr);
    gen_printf(gen,")");
  }
  return r;
}

int compile_num_expression(elcgen *gen, int indent, expression *expr)
{
  int r = 1;

  if ((XPATH_ADD == expr->type)) {
    gen_iprintf(gen,indent,"(+ ");
    r = r && compile_num_expression(gen,indent+1,expr->left);
    gen_printf(gen," ");
    r = r && compile_num_expression(gen,indent+1,expr->right);
    gen_printf(gen,")");
  }
  else if ((XPATH_SUBTRACT == expr->type)) {
    gen_iprintf(gen,indent,"(- ");
    r = r && compile_num_expression(gen,indent+1,expr->left);
    gen_printf(gen," ");
    r = r && compile_num_expression(gen,indent+1,expr->right);
    gen_printf(gen,")");
  }
  else if ((XPATH_MULTIPLY == expr->type)) {
    gen_iprintf(gen,indent,"(* ");
    r = r && compile_num_expression(gen,indent+1,expr->left);
    gen_printf(gen," ");
    r = r && compile_num_expression(gen,indent+1,expr->right);
    gen_printf(gen,")");
  }
  else if ((XPATH_DIVIDE == expr->type)) {
    gen_iprintf(gen,indent,"(/ ");
    r = r && compile_num_expression(gen,indent+1,expr->left);
    gen_printf(gen," ");
    r = r && compile_num_expression(gen,indent+1,expr->right);
    gen_printf(gen,")");
  }
  else if ((XPATH_IDIVIDE == expr->type)) {
    gen_iprintf(gen,indent,"(idiv ");
    r = r && compile_num_expression(gen,indent+1,expr->left);
    gen_printf(gen," ");
    r = r && compile_num_expression(gen,indent+1,expr->right);
    gen_printf(gen,")");
  }
  else if ((XPATH_MOD == expr->type)) {
    gen_iprintf(gen,indent,"(%% ");
    r = r && compile_num_expression(gen,indent+1,expr->left);
    gen_printf(gen," ");
    r = r && compile_num_expression(gen,indent+1,expr->right);
    gen_printf(gen,")");
  }
  else if ((XPATH_IF == expr->type)) {
    gen_iprintf(gen,indent,"(if ");
    r = r && compile_ebv_expression(gen,indent+1,expr->test);
    gen_printf(gen," ");
    r = r && compile_num_expression(gen,indent+1,expr->left);
    gen_printf(gen," ");
    r = r && compile_num_expression(gen,indent+1,expr->right);
    gen_printf(gen,")");
  }
  else if ((XPATH_CONTEXT_ITEM == expr->type)) {
    gen_iprintf(gen,indent,"(xml::item_value citem)");
  }
  else if ((XPATH_NUMERIC_LITERAL == expr->type)) {
    gen_iprintf(gen,indent,"");
    gen_printf(gen,"%f",expr->num);
  }
  else if ((XPATH_VAR_REF == expr->type)) {
    gen_printf(gen,"%s",expr->target->ident);
  }
  else if ((XPATH_FUNCTION_CALL == expr->type)) {
    r = r && compile_user_function_call(gen,indent,expr,1);
  }
  else {
    return gen_error(gen,"Unexpected expression type: %s",expr_names[expr->type]);
  }
  return r;
}

int compile_choose(elcgen *gen, int indent, expression *xchild)
{
  int r = 1;

  if ((NULL == xchild)) {
    gen_printf(gen,"nil");
  }
  else if ((XSLT_WHEN == xchild->type)) {
    gen_printorig(gen,indent,"when",xchild,"test");
    gen_iprintf(gen,indent,"(if ");
    r = r && compile_ebv_expression(gen,indent+1,xchild->left);
    gen_printf(gen," ");
    r = r && compile_sequence(gen,indent+1,xchild->children);
    gen_printf(gen," ");
    r = r && compile_choose(gen,indent,xchild->next);
    gen_printf(gen,")");
  }
  else if ((XSLT_OTHERWISE == xchild->type)) {
    gen_printorig(gen,indent,"otherwise",NULL,NULL);
    r = r && compile_sequence(gen,indent,xchild->children);
  }
  else {
    r = gen_error(gen,"Invalid element in choose: %s\n",expr_names[xchild->type]);
  }

  return r;
}

int compile_instruction(elcgen *gen, int indent, expression *expr)
{
  int r = 1;

  if ((XSLT_SEQUENCE == expr->type)) {
    gen_printorig(gen,indent,"sequence",expr,"select");
    r = r && compile_expression(gen,indent,expr->left);
  }
  else if ((XSLT_VALUE_OF == expr->type)) {
    gen_iprintf(gen,indent,"(xslt::construct_value_of ");
    if (expr->left)
      r = r && compile_expression(gen,indent,expr->left);
    else
      r = r && compile_sequence(gen,indent,expr->children);
    gen_printf(gen,")");
  }
  else if ((XSLT_TEXT == expr->type)) {
    char *str = xmlNodeListGetString(gen->parse_doc,expr->n->children,1);
    char *esc = escape(str);
    gen_iprintf(gen,indent,"(xslt::construct_text \"%s\")",esc);
    free(esc);
    free(str);
  }
  else if ((XSLT_FOR_EACH == expr->type)) {
    gen_printorig(gen,indent,"for-each",expr,"select");
    gen_iprintf(gen,indent,"(xslt::foreach3 ");
    r = r && compile_expression(gen,indent+1,expr->left);
    gen_iprintf(gen,indent,"  (!citem.!cpos.!csize.");
    r = r && compile_sequence(gen,indent+2,expr->children);
    gen_printf(gen,"))");
  }
  else if ((XSLT_IF == expr->type)) {
    gen_printorig(gen,indent,"if",expr,"test");
    gen_iprintf(gen,indent,"(if ");
    r = r && compile_ebv_expression(gen,indent+1,expr->left);
    gen_printf(gen," ");
    r = r && compile_sequence(gen,indent+1,expr->children);
    gen_iprintf(gen,indent,"  nil)");
  }
  else if ((XSLT_CHOOSE == expr->type)) {
    gen_printorig(gen,indent,"choose",NULL,NULL);
    r = r && compile_choose(gen,indent,expr->children);
  }
  else if ((XSLT_ELEMENT == expr->type)) {
    /* FIXME: complete this, and handle namespaces properly */
    gen_printorig(gen,indent,"element",expr,"name");
    gen_iprintf(gen,indent,"(xslt::construct_elem1 ");

    if (expr->namespace_avt)
      r = r && compile_avt(gen,indent,expr->namespace_avt);
    else
      gen_iprintf(gen,indent,"nil ");

    r = r && compile_avt(gen,indent,expr->name_avt);

    gen_printf(gen," nil nil ");
    r = r && compile_sequence(gen,indent+1,expr->children);
    gen_printf(gen,")");
  }
  else if ((XSLT_ATTRIBUTE == expr->type)) {
    /* FIXME: handle namespaces properly */
    gen_printorig(gen,indent,"attribute",expr,"name");
    gen_iprintf(gen,indent,"(cons (xml::mkattr nil nil nil nil ");
    gen_printf(gen,"nil "); // nsuri
    gen_printf(gen,"nil "); // nsprefix

    // localname
    r = r && compile_avt(gen,indent+1,expr->name_avt);
    // value
    gen_printf(gen,"(xslt::consimple ");
    if (expr->left)
      r = r && compile_expression(gen,indent+1,expr->left);
    else
      r = r && compile_sequence(gen,indent+1,expr->children);
    gen_printf(gen,")");
    gen_printf(gen,") nil)");
  }
  else if ((XSLT_INAMESPACE == expr->type)) {
    gen_printorig(gen,indent,"namespace",expr,"name");
    gen_iprintf(gen,indent,"(cons (xml::mknamespace (xslt::consimple ");

    if (expr->left)
      r = r && compile_expression(gen,indent,expr->left);
    else
      r = r && compile_sequence(gen,indent,expr->children);
    gen_printf(gen,") ");

    r = r && compile_avt(gen,indent,expr->name_avt);
    gen_printf(gen,") nil)");
  }
  else if ((XSLT_APPLY_TEMPLATES == expr->type)) {
    gen_printorig(gen,indent,"apply-templates",expr,"select");
    gen_iprintf(gen,indent,"(apply_templates ");
    if (expr->left)
      r = r && compile_expression(gen,indent+1,expr->left);
    else
      gen_printf(gen,"(xml::item_children citem)");
    gen_printf(gen,")");
  }
  else if ((XSLT_LITERAL_RESULT_ELEMENT == expr->type)) {
    /* literal result element */
    gen_iprintf(gen,indent,"(xslt::construct_elem2 ");
    if (expr->n->ns && expr->n->ns->prefix)
      gen_printf(gen," \"%s\" \"%s\" \"%s\" ",expr->n->ns->href,expr->n->ns->prefix,expr->n->name);
    else if (expr->n->ns)
      gen_printf(gen," \"%s\" \"\" \"%s\" ",expr->n->ns->href,expr->n->name);
    else
      gen_printf(gen," nil nil \"%s\" ",expr->n->name);
    r = r && compile_attributes(gen,indent+1,expr);
    gen_printf(gen," ");
    r = r && compile_namespaces(gen,indent+1,expr);
    gen_printf(gen," ");
    r = r && compile_sequence(gen,indent+1,expr->children);
    gen_printf(gen,")");
  }
  else if ((XSLT_LITERAL_TEXT_NODE == expr->type)) {
    char *esc = escape((const char*)expr->n->content);
    gen_iprintf(gen,indent,"(xslt::construct_text \"%s\")",esc);
    free(esc);
  }
  else {
    return gen_error(gen,"Invalid XSLT node: %d\n",expr->type);
  }

  return r;
}

int compile_num_sequence(elcgen *gen, int indent, expression *xfirst)
{
  expression *xchild;
  int count = 0;
  int r = 1;

  for (xchild = xfirst; xchild && (XSLT_VARIABLE == xchild->type); xchild = xchild->next) {
    if (!compile_variable(gen,indent,xchild))
      return 0;
    count++;
  }

  assert(xchild);
  assert(NULL == xchild->next);

  r = compile_num_instruction(gen,indent,xchild);

  while (0 < count--)
    gen_printf(gen,")");
  return r;
}

int compile_sequence(elcgen *gen, int indent, expression *xfirst)
{
  expression *xchild;
  int count = 0;
  int r = 1;

  if (NULL == xfirst) {
    gen_printf(gen,"nil");
    return 1;
  }

  if (NULL == xfirst->next)
    return compile_instruction(gen,indent,xfirst);

  for (xchild = xfirst; xchild && r; xchild = xchild->next) {
    if (XSLT_VARIABLE == xchild->type) {
      /* FIXME: support top-level variables as well */
      r = compile_variable(gen,indent,xchild);
    }
    else {
      gen_printf(gen,"(append ");
      r = compile_instruction(gen,indent,xchild);
      gen_printf(gen," ");
    }
    count++;
  }
  gen_printf(gen,"nil");
  while (0 < count--)
    gen_printf(gen,")");
  return r;
}
