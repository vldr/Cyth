#include "codegen.h"
#include "array.h"
#include "checker.h"
#include "expression.h"
#include "lexer.h"
#include "main.h"
#include "map.h"
#include "memory.h"
#include "statement.h"

#include <math.h>
#include <mir-gen.h>
#include <mir.h>

static struct
{
  MIR_context_t ctx;
  MIR_module_t module;
  ArrayStmt statements;
} codegen;

typedef int BinaryenExpressionRef;

static const char* generate_function_internal(DataType data_type)
{
}

static BinaryenExpressionRef generate_function_pointer(DataType data_type)
{
}

static BinaryenExpressionRef generate_group_expression(GroupExpr* expression)
{
}

static BinaryenExpressionRef generate_string_literal_expression(const char* literal, int length)
{
}

static BinaryenExpressionRef generate_literal_expression(LiteralExpr* expression)
{
}

static BinaryenExpressionRef generate_binary_expression(BinaryExpr* expression)
{
}

static BinaryenExpressionRef generate_unary_expression(UnaryExpr* expression)
{
}

static BinaryenExpressionRef generate_cast_expression(CastExpr* expression)
{
}

static BinaryenExpressionRef generate_variable_expression(VarExpr* expression)
{
}

static BinaryenExpressionRef generate_assignment_expression(AssignExpr* expression)
{
}

static BinaryenExpressionRef generate_call_expression(CallExpr* expression)
{
}

static BinaryenExpressionRef generate_access_expression(AccessExpr* expression)
{
}

static BinaryenExpressionRef generate_index_expression(IndexExpr* expression)
{
}

static BinaryenExpressionRef generate_array_expression(LiteralArrayExpr* expression)
{
}

static BinaryenExpressionRef generate_is_expression(IsExpr* expression)
{
}

static BinaryenExpressionRef generate_if_expression(IfExpr* expression)
{
}

static BinaryenExpressionRef generate_expression(Expr* expression)
{
  switch (expression->type)
  {
  case EXPR_LITERAL:
    return generate_literal_expression(&expression->literal);
  case EXPR_BINARY:
    return generate_binary_expression(&expression->binary);
  case EXPR_GROUP:
    return generate_group_expression(&expression->group);
  case EXPR_UNARY:
    return generate_unary_expression(&expression->unary);
  case EXPR_CAST:
    return generate_cast_expression(&expression->cast);
  case EXPR_VAR:
    return generate_variable_expression(&expression->var);
  case EXPR_ASSIGN:
    return generate_assignment_expression(&expression->assign);
  case EXPR_CALL:
    return generate_call_expression(&expression->call);
  case EXPR_ACCESS:
    return generate_access_expression(&expression->access);
  case EXPR_INDEX:
    return generate_index_expression(&expression->index);
  case EXPR_ARRAY:
    return generate_array_expression(&expression->array);
  case EXPR_IS:
    return generate_is_expression(&expression->is);
  case EXPR_IF:
    return generate_if_expression(&expression->cond);

  default:
    UNREACHABLE("Unhandled expression");
  }
}

static BinaryenExpressionRef generate_expression_statement(ExprStmt* statement)
{
}

static BinaryenExpressionRef generate_if_statement(IfStmt* statement)
{
}

static BinaryenExpressionRef generate_while_statement(WhileStmt* statement)
{
}

static BinaryenExpressionRef generate_return_statement(ReturnStmt* statement)
{
}

static BinaryenExpressionRef generate_continue_statement(void)
{
}

static BinaryenExpressionRef generate_break_statement(void)
{
}

static BinaryenExpressionRef generate_variable_declaration(VarStmt* statement)
{
}

static BinaryenExpressionRef generate_function_declaration(FuncStmt* statement)
{
}

static BinaryenExpressionRef generate_function_template_declaration(FuncTemplateStmt* statement)
{
}

static void generate_class_body_declaration(ClassStmt* statement)
{
}

static BinaryenExpressionRef generate_class_template_declaration(ClassTemplateStmt* statement)
{
  return NULL;
}

static BinaryenExpressionRef generate_import_declaration(ImportStmt* statement)
{
  return NULL;
}

static BinaryenExpressionRef generate_statement(Stmt* statement)
{
  switch (statement->type)
  {
  case STMT_EXPR:
    return generate_expression_statement(&statement->expr);
  case STMT_IF:
    return generate_if_statement(&statement->cond);
  case STMT_WHILE:
    return generate_while_statement(&statement->loop);
  case STMT_RETURN:
    return generate_return_statement(&statement->ret);
  case STMT_CONTINUE:
    return generate_continue_statement();
  case STMT_BREAK:
    return generate_break_statement();
  case STMT_VARIABLE_DECL:
    return generate_variable_declaration(&statement->var);
  case STMT_FUNCTION_DECL:
    return generate_function_declaration(&statement->func);
  case STMT_IMPORT_DECL:
    return generate_import_declaration(&statement->import);
  case STMT_CLASS_TEMPLATE_DECL:
    return generate_class_template_declaration(&statement->class_template);
  case STMT_FUNCTION_TEMPLATE_DECL:
    return generate_function_template_declaration(&statement->func_template);
  case STMT_CLASS_DECL: {
  }
  }

  UNREACHABLE("Unhandled statement");
}

static BinaryenExpressionRef generate_statements(ArrayStmt* statements)
{
}

void codegen_init(ArrayStmt statements)
{
  codegen.ctx = MIR_init();
  codegen.module = MIR_new_module(codegen.ctx, "hello");
  codegen.statements = statements;
}

Codegen codegen_generate(bool logging)
{
  Codegen result = { 0 };
  BinaryenExpressionRef body = generate_statements(&codegen.statements);

  MIR_item_t gv = MIR_new_data(codegen.ctx, "greetings", MIR_T_U8, 12, "world\0all\0\0");
  MIR_type_t i = MIR_T_I32;
  MIR_item_t callback = MIR_new_proto(codegen.ctx, "cb", 1, &i, 1, MIR_T_P, "string");
  MIR_item_t func = MIR_new_func(codegen.ctx, "hello", 1, &i, 3, MIR_T_P, "string", MIR_T_P,
                                 "callback", MIR_T_I32, "id");
  MIR_reg_t temp = MIR_new_func_reg(codegen.ctx, func->u.func, MIR_T_I64, "$temp");
  MIR_reg_t ret = MIR_new_func_reg(codegen.ctx, func->u.func, MIR_T_I64, "$ret");
  MIR_reg_t string = MIR_reg(codegen.ctx, "string", func->u.func),
            cb = MIR_reg(codegen.ctx, "callback", func->u.func),
            id = MIR_reg(codegen.ctx, "id", func->u.func);
  MIR_append_insn(codegen.ctx, func,
                  MIR_new_insn(codegen.ctx, MIR_MOV, MIR_new_reg_op(codegen.ctx, temp),
                               MIR_new_ref_op(codegen.ctx, gv)));
  MIR_append_insn(codegen.ctx, func,
                  MIR_new_insn(codegen.ctx, MIR_MUL, MIR_new_reg_op(codegen.ctx, id),
                               MIR_new_reg_op(codegen.ctx, id), MIR_new_int_op(codegen.ctx, 6)));
  MIR_append_insn(codegen.ctx, func,
                  MIR_new_insn(codegen.ctx, MIR_ADD, MIR_new_reg_op(codegen.ctx, id),
                               MIR_new_reg_op(codegen.ctx, id), MIR_new_reg_op(codegen.ctx, temp)));
  MIR_append_insn(codegen.ctx, func,
                  MIR_new_call_insn(codegen.ctx, 4, MIR_new_ref_op(codegen.ctx, callback),
                                    MIR_new_reg_op(codegen.ctx, cb),
                                    MIR_new_reg_op(codegen.ctx, ret),
                                    MIR_new_reg_op(codegen.ctx, string)));
  MIR_append_insn(codegen.ctx, func,
                  MIR_new_call_insn(codegen.ctx, 4, MIR_new_ref_op(codegen.ctx, callback),
                                    MIR_new_reg_op(codegen.ctx, cb),
                                    MIR_new_reg_op(codegen.ctx, temp),
                                    MIR_new_reg_op(codegen.ctx, id)));
  MIR_append_insn(codegen.ctx, func,
                  MIR_new_insn(codegen.ctx, MIR_ADD, MIR_new_reg_op(codegen.ctx, ret),
                               MIR_new_reg_op(codegen.ctx, ret),
                               MIR_new_reg_op(codegen.ctx, temp)));
  MIR_append_insn(codegen.ctx, func,
                  MIR_new_ret_insn(codegen.ctx, 1, MIR_new_reg_op(codegen.ctx, ret)));
  MIR_finish_func(codegen.ctx);
  MIR_finish_module(codegen.ctx);
  MIR_output(codegen.ctx, stderr);
  MIR_load_module(codegen.ctx, codegen.module);
  MIR_gen_init(codegen.ctx);
  typedef int (*Callback)(const char*);
  int (*boo)(const char*, Callback, unsigned) = NULL;
  MIR_link(codegen.ctx, MIR_set_gen_interface, NULL);
  boo = MIR_gen(codegen.ctx, func);
  printf("%d\n", boo("hello", puts, 0) + boo("goodbye", puts, 1));
cleanup:
  MIR_gen_finish(codegen.ctx);
  MIR_finish(codegen.ctx);

  return result;
}
