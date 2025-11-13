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
  MIR_item_t function;
  ArrayStmt statements;
} codegen;

static void generate_expression(MIR_reg_t dest, Expr* expression);

static MIR_type_t data_type_to_mir_type(DataType data_type)
{
  switch (data_type.type)
  {
  case TYPE_VOID:
  case TYPE_ALIAS:
  case TYPE_PROTOTYPE:
  case TYPE_PROTOTYPE_TEMPLATE:
  case TYPE_FUNCTION_TEMPLATE:
  case TYPE_FUNCTION_GROUP:
    return MIR_T_UNDEF;
  case TYPE_FUNCTION:
  case TYPE_FUNCTION_MEMBER:
  case TYPE_FUNCTION_INTERNAL:
  case TYPE_FUNCTION_POINTER: {
    return MIR_T_P;
  }
  case TYPE_NULL:
    return MIR_T_P;
  case TYPE_ANY:
    return MIR_T_P;
  case TYPE_BOOL:
  case TYPE_CHAR:
  case TYPE_INTEGER:
    return MIR_T_I64;
  case TYPE_FLOAT:
    return MIR_T_F;
  case TYPE_STRING:
    return MIR_T_P;
  case TYPE_OBJECT:
    return MIR_T_P;
  case TYPE_ARRAY:
    return MIR_T_P;
  }

  UNREACHABLE("Unhandled data type");
}

static const char* generate_function_internal(DataType data_type)
{
}

static void generate_function_pointer(DataType data_type)
{
}

static void generate_group_expression(MIR_reg_t dest, GroupExpr* expression)
{
  generate_expression(dest, expression->expr);
}

static void generate_string_literal_expression(const char* literal, int length)
{
}

static void generate_literal_expression(MIR_reg_t dest, LiteralExpr* expression)
{
  switch (expression->data_type.type)
  {
  case TYPE_INTEGER:
    MIR_append_insn(codegen.ctx, codegen.function,
                    MIR_new_insn(codegen.ctx, MIR_MOV, MIR_new_reg_op(codegen.ctx, dest),
                                 MIR_new_int_op(codegen.ctx, expression->integer)));
    return;
  case TYPE_FLOAT:
    MIR_append_insn(codegen.ctx, codegen.function,
                    MIR_new_insn(codegen.ctx, MIR_FMOV, MIR_new_reg_op(codegen.ctx, dest),
                                 MIR_new_float_op(codegen.ctx, expression->floating)));
    return;
  case TYPE_BOOL:
    MIR_append_insn(codegen.ctx, codegen.function,
                    MIR_new_insn(codegen.ctx, MIR_MOV, MIR_new_reg_op(codegen.ctx, dest),
                                 MIR_new_int_op(codegen.ctx, expression->boolean)));
    return;
  case TYPE_NULL:
    MIR_append_insn(codegen.ctx, codegen.function,
                    MIR_new_insn(codegen.ctx, MIR_MOV, MIR_new_reg_op(codegen.ctx, dest),
                                 MIR_new_int_op(codegen.ctx, 0)));
    return;
  case TYPE_CHAR:
    MIR_append_insn(codegen.ctx, codegen.function,
                    MIR_new_insn(codegen.ctx, MIR_MOV, MIR_new_reg_op(codegen.ctx, dest),
                                 MIR_new_int_op(codegen.ctx, expression->string.data[0])));
    return;
  // case TYPE_STRING:
  //   return generate_string_literal_expression(expression->string.data,
  //   expression->string.length);
  default:
    UNREACHABLE("Unhandled literal value");
  }
}

static void generate_binary_expression(MIR_reg_t dest, BinaryExpr* expression)
{
  MIR_reg_t left = _MIR_new_temp_reg(
    codegen.ctx, data_type_to_mir_type(expression->operand_data_type), codegen.function->u.func);
  MIR_reg_t right = _MIR_new_temp_reg(
    codegen.ctx, data_type_to_mir_type(expression->operand_data_type), codegen.function->u.func);

  generate_expression(left, expression->left);
  generate_expression(right, expression->right);
  int op = 0;

  DataType data_type = expression->operand_data_type;

  switch (expression->op.type)
  {
  case TOKEN_PLUS:
    if (data_type.type == TYPE_INTEGER || data_type.type == TYPE_CHAR)
      op = MIR_ADD;
    else if (data_type.type == TYPE_FLOAT)
      op = MIR_FADD;
    else
      UNREACHABLE("Unsupported binary type for +");

    break;
  case TOKEN_MINUS:
    if (data_type.type == TYPE_INTEGER || data_type.type == TYPE_CHAR)
      op = MIR_SUB;
    else if (data_type.type == TYPE_FLOAT)
      op = MIR_FSUB;
    else
      UNREACHABLE("Unsupported binary type for -");

    break;
  case TOKEN_STAR:
    if (data_type.type == TYPE_INTEGER || data_type.type == TYPE_CHAR)
      op = MIR_MULS;
    else if (data_type.type == TYPE_FLOAT)
      op = MIR_FMUL;
    else
      UNREACHABLE("Unsupported binary type for *");

    break;
  case TOKEN_SLASH:
    if (data_type.type == TYPE_INTEGER || data_type.type == TYPE_CHAR)
      op = MIR_DIVS;
    else if (data_type.type == TYPE_FLOAT)
      op = MIR_FDIV;
    else
      UNREACHABLE("Unsupported binary type for /");

    break;

  case TOKEN_PERCENT:
  case TOKEN_AMPERSAND:
  case TOKEN_PIPE:
  case TOKEN_CARET:
  case TOKEN_LESS_LESS:
  case TOKEN_GREATER_GREATER: {
    switch (expression->op.type)
    {
    case TOKEN_PERCENT:
      op = MIR_MODS;
      break;
    case TOKEN_AMPERSAND:
      op = MIR_ANDS;
      break;
    case TOKEN_PIPE:
      op = MIR_ORS;
      break;
    case TOKEN_CARET:
      op = MIR_XORS;
      break;
    case TOKEN_LESS_LESS:
      op = MIR_LSHS;
      break;
    case TOKEN_GREATER_GREATER:
      op = MIR_URSHS;
      break;
    default:
      UNREACHABLE("Unknown operator");
    }

    if (data_type.type != TYPE_INTEGER && data_type.type != TYPE_CHAR)
      UNREACHABLE("Unsupported binary type for %, &, |, ^, <<, >>");

    break;
  }

  case TOKEN_EQUAL_EQUAL:
    if (data_type.type == TYPE_INTEGER || data_type.type == TYPE_BOOL ||
        data_type.type == TYPE_CHAR)
      op = MIR_EQS;
    else if (data_type.type == TYPE_FLOAT)
      op = MIR_FEQ;

    else
      UNREACHABLE("Unsupported binary type for ==");

    break;

  case TOKEN_BANG_EQUAL:
    if (data_type.type == TYPE_INTEGER || data_type.type == TYPE_BOOL ||
        data_type.type == TYPE_CHAR)
      op = MIR_NES;
    else if (data_type.type == TYPE_FLOAT)
      op = MIR_FNE;
    else
      UNREACHABLE("Unsupported binary type for !=");

    break;

  case TOKEN_LESS_EQUAL:
    if (data_type.type == TYPE_INTEGER || data_type.type == TYPE_BOOL ||
        data_type.type == TYPE_CHAR)
      op = MIR_LES;
    else if (data_type.type == TYPE_FLOAT)
      op = MIR_FLE;
    else
      UNREACHABLE("Unsupported binary type for <=");

    break;

  case TOKEN_GREATER_EQUAL:
    if (data_type.type == TYPE_INTEGER || data_type.type == TYPE_BOOL ||
        data_type.type == TYPE_CHAR)
      op = MIR_GES;
    else if (data_type.type == TYPE_FLOAT)
      op = MIR_FGE;
    else
      UNREACHABLE("Unsupported binary type for <=");

    break;

  case TOKEN_LESS:
    if (data_type.type == TYPE_INTEGER || data_type.type == TYPE_BOOL ||
        data_type.type == TYPE_CHAR)
      op = MIR_LTS;
    else if (data_type.type == TYPE_FLOAT)
      op = MIR_FLT;
    else
      UNREACHABLE("Unsupported binary type for <");

    break;

  case TOKEN_GREATER:
    if (data_type.type == TYPE_INTEGER || data_type.type == TYPE_BOOL ||
        data_type.type == TYPE_CHAR)
      op = MIR_GTS;
    else if (data_type.type == TYPE_FLOAT)
      op = MIR_FGT;
    else
      UNREACHABLE("Unsupported binary type for >");

    break;

  case TOKEN_AND:
    if (data_type.type == TYPE_BOOL)
    {
      MIR_label_t cont = MIR_new_label(codegen.ctx);
      MIR_label_t if_false = MIR_new_label(codegen.ctx);

      MIR_append_insn(codegen.ctx, codegen.function,
                      MIR_new_insn(codegen.ctx, MIR_BNES, MIR_new_label_op(codegen.ctx, if_false),
                                   MIR_new_reg_op(codegen.ctx, left),
                                   MIR_new_int_op(codegen.ctx, 0)));

      MIR_append_insn(codegen.ctx, codegen.function,
                      MIR_new_insn(codegen.ctx, MIR_MOV, MIR_new_reg_op(codegen.ctx, dest),
                                   MIR_new_int_op(codegen.ctx, 0)));

      MIR_append_insn(codegen.ctx, codegen.function,
                      MIR_new_insn(codegen.ctx, MIR_JMP, MIR_new_label_op(codegen.ctx, cont)));

      MIR_append_insn(codegen.ctx, codegen.function, if_false);

      MIR_append_insn(codegen.ctx, codegen.function,
                      MIR_new_insn(codegen.ctx, MIR_MOV, MIR_new_reg_op(codegen.ctx, dest),
                                   MIR_new_reg_op(codegen.ctx, right)));

      MIR_append_insn(codegen.ctx, codegen.function, cont);

      return;
    }
    else
      UNREACHABLE("Unsupported binary type for AND");

    break;

  case TOKEN_OR:
    if (data_type.type == TYPE_BOOL)
    {
      MIR_label_t cont = MIR_new_label(codegen.ctx);
      MIR_label_t if_false = MIR_new_label(codegen.ctx);

      MIR_append_insn(codegen.ctx, codegen.function,
                      MIR_new_insn(codegen.ctx, MIR_BNES, MIR_new_label_op(codegen.ctx, if_false),
                                   MIR_new_reg_op(codegen.ctx, left),
                                   MIR_new_int_op(codegen.ctx, 0)));

      MIR_append_insn(codegen.ctx, codegen.function,
                      MIR_new_insn(codegen.ctx, MIR_MOV, MIR_new_reg_op(codegen.ctx, dest),
                                   MIR_new_reg_op(codegen.ctx, right)));

      MIR_append_insn(codegen.ctx, codegen.function,
                      MIR_new_insn(codegen.ctx, MIR_JMP, MIR_new_label_op(codegen.ctx, cont)));

      MIR_append_insn(codegen.ctx, codegen.function, if_false);

      MIR_append_insn(codegen.ctx, codegen.function,
                      MIR_new_insn(codegen.ctx, MIR_MOV, MIR_new_reg_op(codegen.ctx, dest),
                                   MIR_new_int_op(codegen.ctx, 1)));

      MIR_append_insn(codegen.ctx, codegen.function, cont);

      return;
    }
    else
      UNREACHABLE("Unsupported binary type for OR");

    break;

  default:
    UNREACHABLE("Unhandled binary operation");
  }

  MIR_append_insn(codegen.ctx, codegen.function,
                  MIR_new_insn(codegen.ctx, op, MIR_new_reg_op(codegen.ctx, dest),
                               MIR_new_reg_op(codegen.ctx, left),
                               MIR_new_reg_op(codegen.ctx, right)));
}

static void generate_unary_expression(MIR_reg_t dest, UnaryExpr* expression)
{

  MIR_reg_t expr = _MIR_new_temp_reg(codegen.ctx, data_type_to_mir_type(expression->data_type),
                                     codegen.function->u.func);
  generate_expression(expr, expression->expr);

  switch (expression->op.type)
  {
  case TOKEN_TILDE:
    if (expression->data_type.type == TYPE_INTEGER)
    {
      MIR_append_insn(codegen.ctx, codegen.function,
                      MIR_new_insn(codegen.ctx, MIR_XORS, MIR_new_reg_op(codegen.ctx, dest),
                                   MIR_new_reg_op(codegen.ctx, expr),
                                   MIR_new_int_op(codegen.ctx, 0xFFFFFFFF)));
      break;
    }
    else
      UNREACHABLE("Unsupported unary type for ~");

  case TOKEN_MINUS:
    if (expression->data_type.type == TYPE_INTEGER)
    {
      MIR_append_insn(codegen.ctx, codegen.function,
                      MIR_new_insn(codegen.ctx, MIR_NEGS, MIR_new_reg_op(codegen.ctx, dest),
                                   MIR_new_reg_op(codegen.ctx, expr)));
      break;
    }
    else if (expression->data_type.type == TYPE_FLOAT)
    {
      MIR_append_insn(codegen.ctx, codegen.function,
                      MIR_new_insn(codegen.ctx, MIR_FNEG, MIR_new_reg_op(codegen.ctx, dest),
                                   MIR_new_reg_op(codegen.ctx, expr)));
      break;
    }
    else
      UNREACHABLE("Unsupported unary type for -");

  case TOKEN_BANG:
  case TOKEN_NOT:
    if (expression->data_type.type == TYPE_BOOL)
    {
      MIR_label_t cont = MIR_new_label(codegen.ctx);
      MIR_label_t if_false = MIR_new_label(codegen.ctx);

      MIR_append_insn(codegen.ctx, codegen.function,
                      MIR_new_insn(codegen.ctx, MIR_BEQS, MIR_new_label_op(codegen.ctx, if_false),
                                   MIR_new_reg_op(codegen.ctx, expr),
                                   MIR_new_int_op(codegen.ctx, 0)));

      MIR_append_insn(codegen.ctx, codegen.function,
                      MIR_new_insn(codegen.ctx, MIR_MOV, MIR_new_reg_op(codegen.ctx, dest),
                                   MIR_new_int_op(codegen.ctx, 0)));

      MIR_append_insn(codegen.ctx, codegen.function,
                      MIR_new_insn(codegen.ctx, MIR_JMP, MIR_new_label_op(codegen.ctx, cont)));

      MIR_append_insn(codegen.ctx, codegen.function, if_false);

      MIR_append_insn(codegen.ctx, codegen.function,
                      MIR_new_insn(codegen.ctx, MIR_MOV, MIR_new_reg_op(codegen.ctx, dest),
                                   MIR_new_int_op(codegen.ctx, 1)));

      MIR_append_insn(codegen.ctx, codegen.function, cont);

      break;
    }
    else
      UNREACHABLE("Unsupported unary type for !");

  default:
    UNREACHABLE("Unhandled unary expression");
  }
}

static void generate_cast_expression(CastExpr* expression)
{
}

static void generate_variable_expression(VarExpr* expression)
{
  // BinaryenType type = data_type_to_binaryen_type(expression->data_type);
  // if (type == BinaryenTypeNone())
  // {
  //   return BinaryenRefNull(codegen.module, BinaryenTypeAnyref());
  // }

  // if (expression->data_type.type == TYPE_FUNCTION ||
  //     expression->data_type.type == TYPE_FUNCTION_MEMBER ||
  //     expression->data_type.type == TYPE_FUNCTION_INTERNAL)
  // {
  //   return generate_function_pointer(expression->data_type);
  // }

  Scope scope = expression->variable->scope;
  switch (scope)
  {
  // case SCOPE_LOCAL:
  //   return BinaryenLocalGet(codegen.module, expression->variable->index, type);
  case SCOPE_GLOBAL:

    return;
  // case SCOPE_CLASS: {
  //   BinaryenExpressionRef get =
  //     BinaryenStructGet(codegen.module, expression->variable->index,
  //                       BinaryenLocalGet(codegen.module, 0, codegen.class), type, false);
  //   generate_debug_info(expression->name, get, codegen.function);

  //   return get;
  // }
  default:
    UNREACHABLE("Unhandled scope type");
  }
}

static void generate_assignment_expression(AssignExpr* expression)
{
}

static void generate_call_expression(CallExpr* expression)
{
}

static void generate_access_expression(AccessExpr* expression)
{
}

static void generate_index_expression(IndexExpr* expression)
{
}

static void generate_array_expression(LiteralArrayExpr* expression)
{
}

static void generate_is_expression(IsExpr* expression)
{
}

static void generate_if_expression(IfExpr* expression)
{
}

static void generate_expression(MIR_reg_t dest, Expr* expression)
{
  switch (expression->type)
  {
  case EXPR_LITERAL:
    generate_literal_expression(dest, &expression->literal);
    return;
  case EXPR_BINARY:
    generate_binary_expression(dest, &expression->binary);
    return;
  case EXPR_GROUP:
    generate_group_expression(dest, &expression->group);
    return;
  case EXPR_UNARY:
    generate_unary_expression(dest, &expression->unary);
    return;
  }

  UNREACHABLE("Unhandled expression");
}

static void generate_expression_statement(ExprStmt* statement)
{
  MIR_reg_t temp = MIR_new_func_reg(codegen.ctx, codegen.function->u.func,
                                    data_type_to_mir_type(statement->data_type), "$temp");
  generate_expression(temp, statement->expr);

  MIR_append_insn(codegen.ctx, codegen.function,
                  MIR_new_ret_insn(codegen.ctx, 1, MIR_new_reg_op(codegen.ctx, temp)));
}

static void generate_if_statement(IfStmt* statement)
{
}

static void generate_while_statement(WhileStmt* statement)
{
}

static void generate_return_statement(ReturnStmt* statement)
{
}

static void generate_continue_statement(void)
{
}

static void generate_break_statement(void)
{
}

static void generate_variable_declaration(VarStmt* statement)
{

  if (statement->scope == SCOPE_GLOBAL)
  {
    const char* name = statement->name.lexeme;
    int a = 10;
    MIR_item_t gv = MIR_new_data(codegen.ctx, "greetings", MIR_T_U32, 1, &a);
    // MIR_append_insn(codegen.ctx, codegen.function,
    //                 MIR_new_insn(codegen.ctx, MIR_MOV, MIR_new(codegen.ctx, gv,),
    //                              MIR_new_int_op(codegen.ctx, 0)));

    // BinaryenType type = data_type_to_binaryen_type(statement->data_type);
    // BinaryenAddGlobal(codegen.module, name, type, true, initializer);

    // if (statement->initializer)
    // {
    //   return BinaryenGlobalSet(codegen.module, statement->name.lexeme,
    //                            generate_expression(statement->initializer));
    // }
    // else
    // {
    //   return NULL;
    // }
  }
  else if (statement->scope == SCOPE_LOCAL)
  {
    // if (statement->initializer)
    // {
    //   initializer = generate_expression(statement->initializer);
    // }

    // return BinaryenLocalSet(codegen.module, statement->index, initializer);
  }
  else
  {
    UNREACHABLE("Unexpected scope type");
  }
}

static void generate_function_declaration(FuncStmt* statement)
{
}

static void generate_function_template_declaration(FuncTemplateStmt* statement)
{
}

static void generate_class_body_declaration(ClassStmt* statement)
{
}

static void generate_class_template_declaration(ClassTemplateStmt* statement)
{
}

static void generate_import_declaration(ImportStmt* statement)
{
}

static void generate_statement(Stmt* statement)
{
  switch (statement->type)
  {
  case STMT_EXPR:
    generate_expression_statement(&statement->expr);
    return;
  case STMT_IF:
    generate_if_statement(&statement->cond);
    return;
  case STMT_WHILE:
    generate_while_statement(&statement->loop);
    return;
  case STMT_RETURN:
    generate_return_statement(&statement->ret);
    return;
  case STMT_CONTINUE:
    generate_continue_statement();
    return;
  case STMT_BREAK:
    generate_break_statement();
    return;
  case STMT_VARIABLE_DECL:
    generate_variable_declaration(&statement->var);
    return;
  case STMT_FUNCTION_DECL:
    generate_function_declaration(&statement->func);
    return;
  case STMT_IMPORT_DECL:
    generate_import_declaration(&statement->import);
    return;
  case STMT_CLASS_TEMPLATE_DECL:
    generate_class_template_declaration(&statement->class_template);
    return;
  case STMT_FUNCTION_TEMPLATE_DECL:
    generate_function_template_declaration(&statement->func_template);
    return;
  case STMT_CLASS_DECL: {
  }
  }

  UNREACHABLE("Unhandled statement");
}

static void generate_statements(ArrayStmt* statements)
{
  Stmt* statement;
  array_foreach(statements, statement)
  {
    generate_statement(statement);
  }
}

void codegen_init(ArrayStmt statements)
{
  codegen.statements = statements;
  codegen.ctx = MIR_init();
  codegen.module = MIR_new_module(codegen.ctx, "main");
  codegen.function = MIR_new_func(codegen.ctx, "<start>", 1, (MIR_type_t[]){ MIR_T_I32 }, 0);
}

Codegen codegen_generate(bool logging)
{
  Codegen result = { 0 };
  generate_statements(&codegen.statements);

  // MIR_item_t gv = MIR_new_data(codegen.ctx, "greetings", MIR_T_U8, 12, "world\0all\0\0");
  // MIR_item_t callback = MIR_new_proto(codegen.ctx, "cb", 1, &i, 1, MIR_T_P, "string");
  // MIR_item_t func = MIR_new_func(codegen.ctx, "hello", 1, &i, 3, MIR_T_P, "string", MIR_T_P,
  //                                "callback", MIR_T_I32, "id");
  // MIR_reg_t temp = MIR_new_func_reg(codegen.ctx, func->u.func, MIR_T_I64, "$temp");
  // MIR_reg_t ret = MIR_new_func_reg(codegen.ctx, func->u.func, MIR_T_I64, "$ret");
  // MIR_reg_t string = MIR_reg(codegen.ctx, "string", func->u.func),
  //           cb = MIR_reg(codegen.ctx, "callback", func->u.func),
  //           id = MIR_reg(codegen.ctx, "id", func->u.func);
  // MIR_append_insn(codegen.ctx, func,
  //                 MIR_new_insn(codegen.ctx, MIR_MOV, MIR_new_reg_op(codegen.ctx, temp),
  //                              MIR_new_ref_op(codegen.ctx, gv)));
  // MIR_append_insn(codegen.ctx, func,
  //                 MIR_new_insn(codegen.ctx, MIR_MUL, MIR_new_reg_op(codegen.ctx, id),
  //                              MIR_new_reg_op(codegen.ctx, id), MIR_new_int_op(codegen.ctx, 6)));
  // MIR_append_insn(codegen.ctx, func,
  //                 MIR_new_insn(codegen.ctx, MIR_ADD, MIR_new_reg_op(codegen.ctx, id),
  //                              MIR_new_reg_op(codegen.ctx, id), MIR_new_reg_op(codegen.ctx,
  //                              temp)));
  // MIR_append_insn(codegen.ctx, func,
  //                 MIR_new_call_insn(codegen.ctx, 4, MIR_new_ref_op(codegen.ctx, callback),
  //                                   MIR_new_reg_op(codegen.ctx, cb),
  //                                   MIR_new_reg_op(codegen.ctx, ret),
  //                                   MIR_new_reg_op(codegen.ctx, string)));
  // MIR_append_insn(codegen.ctx, func,
  //                 MIR_new_call_insn(codegen.ctx, 4, MIR_new_ref_op(codegen.ctx, callback),
  //                                   MIR_new_reg_op(codegen.ctx, cb),
  //                                   MIR_new_reg_op(codegen.ctx, temp),
  //                                   MIR_new_reg_op(codegen.ctx, id)));
  // MIR_append_insn(codegen.ctx, func,
  //                 MIR_new_insn(codegen.ctx, MIR_ADD, MIR_new_reg_op(codegen.ctx, ret),
  //                              MIR_new_reg_op(codegen.ctx, ret),
  //                              MIR_new_reg_op(codegen.ctx, temp)));
  // MIR_append_insn(codegen.ctx, func,
  //                 MIR_new_ret_insn(codegen.ctx, 1, MIR_new_reg_op(codegen.ctx, ret)));

  MIR_finish_func(codegen.ctx);
  MIR_finish_module(codegen.ctx);
  MIR_output(codegen.ctx, stderr);
  MIR_load_module(codegen.ctx, codegen.module);
  MIR_gen_init(codegen.ctx);

  int (*boo)(void) = NULL;
  MIR_link(codegen.ctx, MIR_set_gen_interface, NULL);
  boo = MIR_gen(codegen.ctx, codegen.function);

  int out = boo();
  printf("%d\n", out);

  MIR_gen_finish(codegen.ctx);
  MIR_finish(codegen.ctx);

  return result;
}
