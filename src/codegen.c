#include "codegen.h"
#include "array.h"
#include "checker.h"
#include "expression.h"
#include "lexer.h"
#include "main.h"
#include "map.h"
#include "memory.h"
#include "mir.h"
#include "statement.h"

#include <math.h>
#include <mir-gen.h>

array_def(MIR_type_t, MIR_type_t);
array_def(MIR_var_t, MIR_var_t);
array_def(MIR_op_t, MIR_op_t);

static struct
{
  MIR_context_t ctx;
  MIR_module_t module;
  MIR_item_t function;
  ArrayStmt statements;
} codegen;

static void generate_expression(MIR_reg_t dest, Expr* expression);
static void generate_statements(ArrayStmt* statements);

static MIR_insn_code_t data_type_to_mov_type(DataType data_type)
{
  switch (data_type.type)
  {
  case TYPE_FLOAT:
    return MIR_FMOV;
  default:
    return MIR_MOV;
  }
}

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
                    MIR_new_insn(codegen.ctx, data_type_to_mov_type(expression->data_type),
                                 MIR_new_reg_op(codegen.ctx, dest),
                                 MIR_new_int_op(codegen.ctx, expression->integer)));
    return;
  case TYPE_FLOAT:
    MIR_append_insn(codegen.ctx, codegen.function,
                    MIR_new_insn(codegen.ctx, data_type_to_mov_type(expression->data_type),
                                 MIR_new_reg_op(codegen.ctx, dest),
                                 MIR_new_float_op(codegen.ctx, expression->floating)));
    return;
  case TYPE_BOOL:
    MIR_append_insn(codegen.ctx, codegen.function,
                    MIR_new_insn(codegen.ctx, data_type_to_mov_type(expression->data_type),
                                 MIR_new_reg_op(codegen.ctx, dest),
                                 MIR_new_int_op(codegen.ctx, expression->boolean)));
    return;
  case TYPE_NULL:
    MIR_append_insn(codegen.ctx, codegen.function,
                    MIR_new_insn(codegen.ctx, data_type_to_mov_type(expression->data_type),
                                 MIR_new_reg_op(codegen.ctx, dest),
                                 MIR_new_int_op(codegen.ctx, 0)));
    return;
  case TYPE_CHAR:
    MIR_append_insn(codegen.ctx, codegen.function,
                    MIR_new_insn(codegen.ctx, data_type_to_mov_type(expression->data_type),
                                 MIR_new_reg_op(codegen.ctx, dest),
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
                      MIR_new_insn(codegen.ctx, data_type_to_mov_type(data_type),
                                   MIR_new_reg_op(codegen.ctx, dest),
                                   MIR_new_int_op(codegen.ctx, 0)));

      MIR_append_insn(codegen.ctx, codegen.function,
                      MIR_new_insn(codegen.ctx, MIR_JMP, MIR_new_label_op(codegen.ctx, cont)));

      MIR_append_insn(codegen.ctx, codegen.function, if_false);

      MIR_append_insn(codegen.ctx, codegen.function,
                      MIR_new_insn(codegen.ctx, data_type_to_mov_type(data_type),
                                   MIR_new_reg_op(codegen.ctx, dest),
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
                      MIR_new_insn(codegen.ctx, data_type_to_mov_type(data_type),
                                   MIR_new_reg_op(codegen.ctx, dest),
                                   MIR_new_reg_op(codegen.ctx, right)));

      MIR_append_insn(codegen.ctx, codegen.function,
                      MIR_new_insn(codegen.ctx, MIR_JMP, MIR_new_label_op(codegen.ctx, cont)));

      MIR_append_insn(codegen.ctx, codegen.function, if_false);

      MIR_append_insn(codegen.ctx, codegen.function,
                      MIR_new_insn(codegen.ctx, data_type_to_mov_type(data_type),
                                   MIR_new_reg_op(codegen.ctx, dest),
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
                      MIR_new_insn(codegen.ctx, data_type_to_mov_type(expression->data_type),
                                   MIR_new_reg_op(codegen.ctx, dest),
                                   MIR_new_int_op(codegen.ctx, 0)));

      MIR_append_insn(codegen.ctx, codegen.function,
                      MIR_new_insn(codegen.ctx, MIR_JMP, MIR_new_label_op(codegen.ctx, cont)));

      MIR_append_insn(codegen.ctx, codegen.function, if_false);

      MIR_append_insn(codegen.ctx, codegen.function,
                      MIR_new_insn(codegen.ctx, data_type_to_mov_type(expression->data_type),
                                   MIR_new_reg_op(codegen.ctx, dest),
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

static void generate_variable_expression(MIR_reg_t dest, VarExpr* expression)
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
  case SCOPE_LOCAL: {
    MIR_append_insn(codegen.ctx, codegen.function,
                    MIR_new_insn(codegen.ctx, data_type_to_mov_type(expression->data_type),
                                 MIR_new_reg_op(codegen.ctx, dest),
                                 MIR_new_reg_op(codegen.ctx, expression->variable->reg)));

    return;
  }
  case SCOPE_GLOBAL: {
    MIR_reg_t temp = _MIR_new_temp_reg(codegen.ctx, data_type_to_mir_type(expression->data_type),
                                       codegen.function->u.func);
    MIR_append_insn(codegen.ctx, codegen.function,
                    MIR_new_insn(codegen.ctx, data_type_to_mov_type(expression->data_type),
                                 MIR_new_reg_op(codegen.ctx, temp),
                                 MIR_new_ref_op(codegen.ctx, expression->variable->item)));
    MIR_append_insn(
      codegen.ctx, codegen.function,
      MIR_new_insn(
        codegen.ctx, data_type_to_mov_type(expression->data_type),
        MIR_new_reg_op(codegen.ctx, dest),
        MIR_new_mem_op(codegen.ctx, data_type_to_mir_type(expression->data_type), 0, temp, 0, 1)));

    return;
  }
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

static void generate_call_expression(MIR_reg_t dest, CallExpr* expression)
{
  MIR_type_t return_type = data_type_to_mir_type(expression->return_data_type);

  // MIR_new_insn_arr(codegen.module, MIR_CALL, expression->arguments.size, MIR_op_t *ops)?

  ArrayMIR_op_t arguments;
  array_init(&arguments);

  Expr* argument;
  array_foreach(&expression->arguments, argument)
  {
    MIR_reg_t temp = _MIR_new_temp_reg(codegen.ctx, MIR_T_I64, codegen.function->u.func);
    generate_expression(temp, argument);

    array_add(&arguments, MIR_new_reg_op(codegen.ctx, temp));
  }

  MIR_reg_t result = _MIR_new_temp_reg(codegen.ctx, return_type, codegen.function->u.func);
  array_add(&arguments, MIR_new_reg_op(codegen.ctx, result));

  MIR_append_insn(codegen.ctx, codegen.function,
                  MIR_new_insn_arr(codegen.ctx, MIR_CALL, arguments.size, arguments.elems));

  MIR_append_insn(codegen.ctx, codegen.function,
                  MIR_new_insn(codegen.ctx, data_type_to_mov_type(expression->return_data_type),
                               MIR_new_reg_op(codegen.ctx, dest),
                               MIR_new_reg_op(codegen.ctx, result)));

  // BinaryenExpressionRef call;

  // if (expression->callee_data_type.type == TYPE_ALIAS)
  // {
  //   return generate_default_initialization(*expression->callee_data_type.alias.data_type);
  // }
  // else if (expression->callee_data_type.type == TYPE_FUNCTION_POINTER)
  // {
  //   call = BinaryenCallRef(codegen.module, generate_expression(expression->callee),
  //   arguments.elems,
  //                          arguments.size, return_type, false);
  // }
  // else
  // {
  //   const char* name;
  //   if (expression->callee_data_type.type == TYPE_FUNCTION_INTERNAL)
  //     name = generate_function_internal(expression->callee_data_type);
  //   else
  //     name = expression->function;

  //   call = BinaryenCall(codegen.module, name, arguments.elems, arguments.size, return_type);
  // }

  // generate_debug_info(expression->callee_token, call, codegen.function);
  // return call;
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
  case EXPR_VAR:
    generate_variable_expression(dest, &expression->var);
    return;
  case EXPR_CALL:
    generate_call_expression(dest, &expression->call);
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
  if (statement->expr)
  {
    MIR_reg_t temp = _MIR_new_temp_reg(codegen.ctx, *codegen.function->u.func->res_types,
                                       codegen.function->u.func);
    generate_expression(temp, statement->expr);

    MIR_append_insn(codegen.ctx, codegen.function,
                    MIR_new_ret_insn(codegen.ctx, 1, MIR_new_reg_op(codegen.ctx, temp)));
  }
  else
  {
    MIR_append_insn(codegen.ctx, codegen.function, MIR_new_ret_insn(codegen.ctx, 0));
  }
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
    uint64_t init = 0;

    MIR_item_t data =
      MIR_new_data(codegen.ctx, name, data_type_to_mir_type(statement->data_type), 1, &init);
    statement->item = data;

    MIR_reg_t ptr = _MIR_new_temp_reg(codegen.ctx, data_type_to_mir_type(statement->data_type),
                                      codegen.function->u.func);
    MIR_append_insn(codegen.ctx, codegen.function,
                    MIR_new_insn(codegen.ctx, data_type_to_mov_type(statement->data_type),
                                 MIR_new_reg_op(codegen.ctx, ptr),
                                 MIR_new_ref_op(codegen.ctx, data)));

    if (statement->initializer)
    {
      MIR_reg_t initializer = _MIR_new_temp_reg(
        codegen.ctx, data_type_to_mir_type(statement->data_type), codegen.function->u.func);
      generate_expression(initializer, statement->initializer);

      MIR_append_insn(
        codegen.ctx, codegen.function,
        MIR_new_insn(
          codegen.ctx, data_type_to_mov_type(statement->data_type),
          MIR_new_mem_op(codegen.ctx, data_type_to_mir_type(statement->data_type), 0, ptr, 0, 1),
          MIR_new_reg_op(codegen.ctx, initializer)));
    }
    else
    {
      MIR_append_insn(
        codegen.ctx, codegen.function,
        MIR_new_insn(
          codegen.ctx, data_type_to_mov_type(statement->data_type),
          MIR_new_mem_op(codegen.ctx, data_type_to_mir_type(statement->data_type), 0, ptr, 0, 1),
          MIR_new_int_op(codegen.ctx, 0)));
    }
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
  ArrayMIR_var_t vars;
  array_init(&vars);

  VarStmt* parameter;
  array_foreach(&statement->parameters, parameter)
  {
    MIR_var_t var;
    var.name = memory_sprintf("%s.%d", parameter->name.lexeme, parameter->index);
    var.type = data_type_to_mir_type(parameter->data_type);

    array_add(&vars, var);
  }

  MIR_item_t previous_function = codegen.function;
  MIR_func_t previous_func = MIR_get_curr_func(codegen.ctx);
  MIR_set_curr_func(codegen.ctx, NULL);

  codegen.function = MIR_new_func_arr(
    codegen.ctx, statement->name.lexeme, statement->data_type.type != TYPE_VOID,
    (MIR_type_t[]){ data_type_to_mir_type(statement->data_type) }, vars.size, vars.elems);

  array_foreach(&statement->parameters, parameter)
  {
    parameter->reg = MIR_reg(codegen.ctx, vars.elems[_i].name, codegen.function->u.func);
  }

  VarStmt* variable;
  array_foreach(&statement->variables, variable)
  {
    variable->reg = MIR_new_func_reg(
      codegen.ctx, codegen.function->u.func, data_type_to_mir_type(variable->data_type),
      memory_sprintf("%s.%d", variable->name.lexeme, variable->index));
  }

  if (statement->import)
  {
    MIR_load_external(codegen.ctx, statement->name.lexeme, NULL);
  }
  else
  {
    generate_statements(&statement->body);

    MIR_new_export(codegen.ctx, statement->name.lexeme);
  }

  MIR_finish_func(codegen.ctx);

  // const char* name = statement->name.lexeme;

  // BinaryenType params = BinaryenTypeCreate(parameter_types.elems, parameter_types.size);
  // BinaryenType results = data_type_to_binaryen_type(statement->data_type);

  // if (statement->import)
  // {
  //   BinaryenAddFunctionImport(codegen.module, name, statement->import, name, params, results);

  //   if (parameters_contain_string)
  //     generate_string_export_functions();

  //   ArrayTypeBuilderSubtype subtypes;
  //   array_init(&subtypes);

  //   generate_function_heap_binaryen_type(NULL, &subtypes, statement->function_data_type);
  // }
  // else
  // {
  //   BinaryenExpressionRef body = generate_statements(&statement->body);
  //   BinaryenFunctionRef func = BinaryenAddFunction(codegen.module, name, params, results,
  //                                                  variable_types.elems, variable_types.size,
  //                                                  body);
  //   BinaryenAddFunctionExport(codegen.module, name, name);

  //   ArrayTypeBuilderSubtype subtypes;
  //   array_init(&subtypes);

  //   BinaryenHeapType heap_type =
  //     generate_function_heap_binaryen_type(NULL, &subtypes, statement->function_data_type);

  //   BinaryenFunctionSetType(func, heap_type);
  // }

  MIR_set_curr_func(codegen.ctx, previous_func);
  codegen.function = previous_function;
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
