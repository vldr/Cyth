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
  MIR_label_t continue_label;
  MIR_label_t break_label;
} codegen;

static void generate_expression(MIR_reg_t dest, Expr* expression);
static void generate_statement(Stmt* statement);
static void generate_statements(ArrayStmt* statements);

static void print_num(int n)
{
  printf("%d\n", n);
}

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
  case TYPE_FUNCTION:
  case TYPE_FUNCTION_MEMBER:
  case TYPE_FUNCTION_INTERNAL:
  case TYPE_FUNCTION_POINTER:
  case TYPE_NULL:
  case TYPE_ANY:
  case TYPE_BOOL:
  case TYPE_CHAR:
  case TYPE_INTEGER:
  case TYPE_STRING:
  case TYPE_OBJECT:
  case TYPE_ARRAY:
    return MIR_T_I64;
  case TYPE_FLOAT:
    return MIR_T_F;
  }

  UNREACHABLE("Unhandled data type");
}

static void generate_default_initialization(MIR_reg_t dest, DataType data_type)
{
  switch (data_type.type)
  {
  case TYPE_INTEGER:
  case TYPE_CHAR:
  case TYPE_BOOL:
    MIR_append_insn(codegen.ctx, codegen.function,
                    MIR_new_insn(codegen.ctx, data_type_to_mov_type(data_type),
                                 MIR_new_reg_op(codegen.ctx, dest),
                                 MIR_new_int_op(codegen.ctx, 0)));
    return;
  case TYPE_FLOAT:
    MIR_append_insn(codegen.ctx, codegen.function,
                    MIR_new_insn(codegen.ctx, data_type_to_mov_type(data_type),
                                 MIR_new_reg_op(codegen.ctx, dest),
                                 MIR_new_float_op(codegen.ctx, 0.0)));
    return;
  default:
    UNREACHABLE("Unexpected default initializer");
  }
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
      MIR_label_t cont_label = MIR_new_label(codegen.ctx);
      MIR_label_t if_false_label = MIR_new_label(codegen.ctx);

      MIR_append_insn(
        codegen.ctx, codegen.function,
        MIR_new_insn(codegen.ctx, MIR_BNES, MIR_new_label_op(codegen.ctx, if_false_label),
                     MIR_new_reg_op(codegen.ctx, left), MIR_new_int_op(codegen.ctx, 0)));

      MIR_append_insn(codegen.ctx, codegen.function,
                      MIR_new_insn(codegen.ctx, data_type_to_mov_type(data_type),
                                   MIR_new_reg_op(codegen.ctx, dest),
                                   MIR_new_int_op(codegen.ctx, 0)));

      MIR_append_insn(
        codegen.ctx, codegen.function,
        MIR_new_insn(codegen.ctx, MIR_JMP, MIR_new_label_op(codegen.ctx, cont_label)));

      MIR_append_insn(codegen.ctx, codegen.function, if_false_label);

      MIR_append_insn(codegen.ctx, codegen.function,
                      MIR_new_insn(codegen.ctx, data_type_to_mov_type(data_type),
                                   MIR_new_reg_op(codegen.ctx, dest),
                                   MIR_new_reg_op(codegen.ctx, right)));

      MIR_append_insn(codegen.ctx, codegen.function, cont_label);

      return;
    }
    else
      UNREACHABLE("Unsupported binary type for AND");

    break;

  case TOKEN_OR:
    if (data_type.type == TYPE_BOOL)
    {
      MIR_label_t cont_label = MIR_new_label(codegen.ctx);
      MIR_label_t if_false_label = MIR_new_label(codegen.ctx);

      MIR_append_insn(
        codegen.ctx, codegen.function,
        MIR_new_insn(codegen.ctx, MIR_BNES, MIR_new_label_op(codegen.ctx, if_false_label),
                     MIR_new_reg_op(codegen.ctx, left), MIR_new_int_op(codegen.ctx, 0)));

      MIR_append_insn(codegen.ctx, codegen.function,
                      MIR_new_insn(codegen.ctx, data_type_to_mov_type(data_type),
                                   MIR_new_reg_op(codegen.ctx, dest),
                                   MIR_new_reg_op(codegen.ctx, right)));

      MIR_append_insn(
        codegen.ctx, codegen.function,
        MIR_new_insn(codegen.ctx, MIR_JMP, MIR_new_label_op(codegen.ctx, cont_label)));

      MIR_append_insn(codegen.ctx, codegen.function, if_false_label);

      MIR_append_insn(codegen.ctx, codegen.function,
                      MIR_new_insn(codegen.ctx, data_type_to_mov_type(data_type),
                                   MIR_new_reg_op(codegen.ctx, dest),
                                   MIR_new_int_op(codegen.ctx, 1)));

      MIR_append_insn(codegen.ctx, codegen.function, cont_label);

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
      MIR_label_t cont_label = MIR_new_label(codegen.ctx);
      MIR_label_t if_false_label = MIR_new_label(codegen.ctx);

      MIR_append_insn(
        codegen.ctx, codegen.function,
        MIR_new_insn(codegen.ctx, MIR_BEQS, MIR_new_label_op(codegen.ctx, if_false_label),
                     MIR_new_reg_op(codegen.ctx, expr), MIR_new_int_op(codegen.ctx, 0)));

      MIR_append_insn(codegen.ctx, codegen.function,
                      MIR_new_insn(codegen.ctx, data_type_to_mov_type(expression->data_type),
                                   MIR_new_reg_op(codegen.ctx, dest),
                                   MIR_new_int_op(codegen.ctx, 0)));

      MIR_append_insn(
        codegen.ctx, codegen.function,
        MIR_new_insn(codegen.ctx, MIR_JMP, MIR_new_label_op(codegen.ctx, cont_label)));

      MIR_append_insn(codegen.ctx, codegen.function, if_false_label);

      MIR_append_insn(codegen.ctx, codegen.function,
                      MIR_new_insn(codegen.ctx, data_type_to_mov_type(expression->data_type),
                                   MIR_new_reg_op(codegen.ctx, dest),
                                   MIR_new_int_op(codegen.ctx, 1)));

      MIR_append_insn(codegen.ctx, codegen.function, cont_label);

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
    MIR_reg_t ptr = _MIR_new_temp_reg(codegen.ctx, MIR_T_I64, codegen.function->u.func);
    MIR_append_insn(codegen.ctx, codegen.function,
                    MIR_new_insn(codegen.ctx, MIR_MOV, MIR_new_reg_op(codegen.ctx, ptr),
                                 MIR_new_ref_op(codegen.ctx, expression->variable->item)));
    MIR_append_insn(
      codegen.ctx, codegen.function,
      MIR_new_insn(
        codegen.ctx, data_type_to_mov_type(expression->data_type),
        MIR_new_reg_op(codegen.ctx, dest),
        MIR_new_mem_op(codegen.ctx, data_type_to_mir_type(expression->data_type), 0, ptr, 0, 1)));

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

static void generate_assignment_expression(MIR_reg_t dest, AssignExpr* expression)
{
  MIR_reg_t value = _MIR_new_temp_reg(codegen.ctx, data_type_to_mir_type(expression->data_type),
                                      codegen.function->u.func);
  generate_expression(value, expression->value);

  VarStmt* variable = expression->variable;
  if (variable)
  {
    switch (variable->scope)
    {
    case SCOPE_LOCAL: {
      MIR_append_insn(codegen.ctx, codegen.function,
                      MIR_new_insn(codegen.ctx, data_type_to_mov_type(expression->data_type),
                                   MIR_new_reg_op(codegen.ctx, variable->reg),
                                   MIR_new_reg_op(codegen.ctx, value)));

      MIR_append_insn(codegen.ctx, codegen.function,
                      MIR_new_insn(codegen.ctx, data_type_to_mov_type(expression->data_type),
                                   MIR_new_reg_op(codegen.ctx, dest),
                                   MIR_new_reg_op(codegen.ctx, value)));

      return;
    }

    case SCOPE_GLOBAL: {
      MIR_reg_t ptr = _MIR_new_temp_reg(codegen.ctx, MIR_T_I64, codegen.function->u.func);
      MIR_append_insn(codegen.ctx, codegen.function,
                      MIR_new_insn(codegen.ctx, MIR_MOV, MIR_new_reg_op(codegen.ctx, ptr),
                                   MIR_new_ref_op(codegen.ctx, expression->variable->item)));
      MIR_append_insn(
        codegen.ctx, codegen.function,
        MIR_new_insn(
          codegen.ctx, data_type_to_mov_type(expression->data_type),
          MIR_new_mem_op(codegen.ctx, data_type_to_mir_type(expression->data_type), 0, ptr, 0, 1),
          MIR_new_reg_op(codegen.ctx, value)));

      MIR_append_insn(codegen.ctx, codegen.function,
                      MIR_new_insn(codegen.ctx, data_type_to_mov_type(expression->data_type),
                                   MIR_new_reg_op(codegen.ctx, dest),
                                   MIR_new_reg_op(codegen.ctx, value)));

      return;
    }

      // case SCOPE_CLASS: {
      //   BinaryenExpressionRef ref;
      //   if (expression->target->type == EXPR_ACCESS)
      //     ref = generate_expression(expression->target->access.expr);
      //   else
      //     ref = BinaryenLocalGet(codegen.module, 0, codegen.class);

      //   BinaryenExpressionRef list[] = {
      //     BinaryenStructSet(codegen.module, variable->index, ref, value),
      //     BinaryenStructGet(codegen.module, variable->index,
      //                       BinaryenExpressionCopy(ref, codegen.module), codegen.class, false),
      //   };

      //   generate_debug_info(expression->op, list[0], codegen.function);

      //   return BinaryenBlock(codegen.module, NULL, list, sizeof(list) / sizeof_ptr(list),
      //   type);
      // }

    default:
      UNREACHABLE("Unhandled scope type");
    }
  }
  else
  {
    UNREACHABLE("Unhandled expression type");
  }
  // else
  // {
  //   if (expression->target->type != EXPR_INDEX)
  //   {
  //     UNREACHABLE("Unhandled expression type");
  //   }

  //   BinaryenExpressionRef ref = generate_expression(expression->target->index.expr);
  //   BinaryenExpressionRef index = generate_expression(expression->target->index.index);

  //   if (expression->target->index.expr_data_type.type == TYPE_OBJECT)
  //   {
  //     BinaryenExpressionRef operands[] = { ref, index, value };
  //     BinaryenExpressionRef call = BinaryenCall(
  //       codegen.module, expression->function, operands, sizeof(operands) /
  //       sizeof_ptr(operands), data_type_to_binaryen_type(expression->target->index.data_type));

  //     generate_debug_info(expression->target->index.index_token, call, codegen.function);
  //     return call;
  //   }
  //   else
  //   {
  //     BinaryenExpressionRef array =
  //       BinaryenStructGet(codegen.module, 0, ref, BinaryenTypeAuto(), false);
  //     BinaryenExpressionRef length = BinaryenStructGet(
  //       codegen.module, 1, BinaryenExpressionCopy(ref, codegen.module), BinaryenTypeInt32(),
  //       false);
  //     BinaryenExpressionRef bounded_index = BinaryenSelect(
  //       codegen.module, BinaryenBinary(codegen.module, BinaryenLtSInt32(), index, length),
  //       BinaryenExpressionCopy(index, codegen.module),
  //       BinaryenConst(codegen.module, BinaryenLiteralInt32(-1)));

  //     BinaryenExpressionRef list[] = {
  //       BinaryenArraySet(codegen.module, array, bounded_index, value),
  //       BinaryenArrayGet(codegen.module, BinaryenExpressionCopy(array, codegen.module),
  //                        BinaryenExpressionCopy(index, codegen.module), type, false),
  //     };

  //     generate_debug_info(expression->target->index.index_token, list[0], codegen.function);
  //     return BinaryenBlock(codegen.module, NULL, list, sizeof(list) / sizeof_ptr(list), type);
  //   }
  // }
}

static void generate_call_expression(MIR_reg_t dest, CallExpr* expression)
{
  if (!expression->function->proto)
  {
    ArrayMIR_var_t vars;
    array_init(&vars);

    VarStmt* parameter;
    array_foreach(&expression->function->parameters, parameter)
    {
      MIR_var_t var;
      var.name = memory_sprintf("%s.%d", parameter->name.lexeme, parameter->index);
      var.type = data_type_to_mir_type(parameter->data_type);

      array_add(&vars, var);
    }

    expression->function->proto =
      MIR_new_proto_arr(codegen.ctx, memory_sprintf("%s.proto", expression->function->name.lexeme),
                        expression->function->data_type.type != TYPE_VOID,
                        (MIR_type_t[]){ data_type_to_mir_type(expression->function->data_type) },
                        vars.size, vars.elems);

    if (!expression->function->item)
    {
      MIR_func_t previous_func = MIR_get_curr_func(codegen.ctx);
      MIR_set_curr_func(codegen.ctx, NULL);

      expression->function->item =
        MIR_new_func_arr(codegen.ctx, expression->function->name.lexeme,
                         expression->function->data_type.type != TYPE_VOID,
                         (MIR_type_t[]){ data_type_to_mir_type(expression->function->data_type) },
                         vars.size, vars.elems);

      MIR_set_curr_func(codegen.ctx, previous_func);
    }
  }

  ArrayMIR_op_t arguments;
  array_init(&arguments);

  array_add(&arguments, MIR_new_ref_op(codegen.ctx, expression->function->proto));
  array_add(&arguments, MIR_new_ref_op(codegen.ctx, expression->function->item));

  if (expression->return_data_type.type != TYPE_VOID)
    array_add(&arguments, MIR_new_reg_op(codegen.ctx, dest));

  Expr* argument;
  array_foreach(&expression->arguments, argument)
  {
    MIR_reg_t temp =
      _MIR_new_temp_reg(codegen.ctx, expression->function->proto->u.proto->args->varr[_i].type,
                        codegen.function->u.func);
    generate_expression(temp, argument);

    array_add(&arguments, MIR_new_reg_op(codegen.ctx, temp));
  }

  MIR_append_insn(codegen.ctx, codegen.function,
                  MIR_new_insn_arr(codegen.ctx, MIR_CALL, arguments.size, arguments.elems));

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
  case EXPR_ASSIGN:
    generate_assignment_expression(dest, &expression->assign);
    return;
  case EXPR_CALL:
    generate_call_expression(dest, &expression->call);
    return;
  }

  UNREACHABLE("Unhandled expression");
}

static void generate_expression_statement(ExprStmt* statement)
{
  MIR_reg_t temp = _MIR_new_temp_reg(codegen.ctx, data_type_to_mir_type(statement->data_type),
                                     codegen.function->u.func);
  generate_expression(temp, statement->expr);
}

static void generate_if_statement(IfStmt* statement)
{
  MIR_reg_t condition = _MIR_new_temp_reg(codegen.ctx, data_type_to_mir_type(DATA_TYPE(TYPE_BOOL)),
                                          codegen.function->u.func);
  generate_expression(condition, statement->condition);

  MIR_label_t cont_label = MIR_new_label(codegen.ctx);
  MIR_label_t if_false_label = MIR_new_label(codegen.ctx);

  MIR_append_insn(codegen.ctx, codegen.function,
                  MIR_new_insn(codegen.ctx, MIR_BEQS, MIR_new_label_op(codegen.ctx, if_false_label),
                               MIR_new_reg_op(codegen.ctx, condition),
                               MIR_new_int_op(codegen.ctx, 0)));

  generate_statements(&statement->then_branch);

  MIR_append_insn(codegen.ctx, codegen.function,
                  MIR_new_insn(codegen.ctx, MIR_JMP, MIR_new_label_op(codegen.ctx, cont_label)));

  MIR_append_insn(codegen.ctx, codegen.function, if_false_label);

  if (statement->else_branch.elems)
    generate_statements(&statement->else_branch);

  MIR_append_insn(codegen.ctx, codegen.function, cont_label);
}

static void generate_while_statement(WhileStmt* statement)
{
  MIR_label_t previous_continue_label = codegen.continue_label;
  MIR_label_t previous_break_label = codegen.break_label;

  codegen.continue_label = MIR_new_label(codegen.ctx);
  codegen.break_label = MIR_new_label(codegen.ctx);

  MIR_label_t loop_label = MIR_new_label(codegen.ctx);

  generate_statements(&statement->initializer);

  MIR_append_insn(codegen.ctx, codegen.function, loop_label);

  MIR_reg_t condition = _MIR_new_temp_reg(codegen.ctx, data_type_to_mir_type(DATA_TYPE(TYPE_BOOL)),
                                          codegen.function->u.func);
  generate_expression(condition, statement->condition);
  MIR_append_insn(
    codegen.ctx, codegen.function,
    MIR_new_insn(codegen.ctx, MIR_BEQS, MIR_new_label_op(codegen.ctx, codegen.break_label),
                 MIR_new_reg_op(codegen.ctx, condition), MIR_new_int_op(codegen.ctx, 0)));

  generate_statements(&statement->body);

  MIR_append_insn(codegen.ctx, codegen.function, codegen.continue_label);

  generate_statements(&statement->incrementer);

  MIR_append_insn(codegen.ctx, codegen.function,
                  MIR_new_insn(codegen.ctx, MIR_JMP, MIR_new_label_op(codegen.ctx, loop_label)));

  MIR_append_insn(codegen.ctx, codegen.function, codegen.break_label);

  codegen.continue_label = previous_continue_label;
  codegen.break_label = previous_break_label;
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
  MIR_append_insn(
    codegen.ctx, codegen.function,
    MIR_new_insn(codegen.ctx, MIR_JMP, MIR_new_label_op(codegen.ctx, codegen.continue_label)));
}

static void generate_break_statement(void)
{
  MIR_append_insn(
    codegen.ctx, codegen.function,
    MIR_new_insn(codegen.ctx, MIR_JMP, MIR_new_label_op(codegen.ctx, codegen.break_label)));
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

    MIR_reg_t ptr = _MIR_new_temp_reg(codegen.ctx, MIR_T_I64, codegen.function->u.func);
    MIR_append_insn(codegen.ctx, codegen.function,
                    MIR_new_insn(codegen.ctx, MIR_MOV, MIR_new_reg_op(codegen.ctx, ptr),
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
      generate_default_initialization(statement->reg, statement->data_type);
    }
  }
  else if (statement->scope == SCOPE_LOCAL)
  {
    if (statement->initializer)
    {
      generate_expression(statement->reg, statement->initializer);
    }
    else
    {
      generate_default_initialization(statement->reg, statement->data_type);
    }
  }
  else
  {
    UNREACHABLE("Unexpected scope type");
  }
}

static void generate_function_declaration(FuncStmt* statement)
{
  if (statement->import)
  {
    statement->item = MIR_new_import(codegen.ctx, statement->name.lexeme);
    return;
  }

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

  if (statement->item)
  {
    codegen.function = statement->item;
    MIR_set_curr_func(codegen.ctx, statement->item->u.func);
  }
  else
  {
    codegen.function = MIR_new_func_arr(
      codegen.ctx, statement->name.lexeme, statement->data_type.type != TYPE_VOID,
      (MIR_type_t[]){ data_type_to_mir_type(statement->data_type) }, vars.size, vars.elems);
  }

  statement->item = codegen.function;

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

  generate_statements(&statement->body);

  MIR_new_export(codegen.ctx, statement->name.lexeme);
  MIR_finish_func(codegen.ctx);

  MIR_set_curr_func(codegen.ctx, previous_func);
  codegen.function = previous_function;

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
  Stmt* body_statement;
  array_foreach(&statement->body, body_statement)
  {
    generate_statement(body_statement);
  }
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
  codegen.function = MIR_new_func(codegen.ctx, "<start>", 0, 0, 0);
  codegen.continue_label = NULL;
  codegen.break_label = NULL;

  VarStmt* global_local;
  ArrayVarStmt global_local_statements = global_locals();
  array_foreach(&global_local_statements, global_local)
  {
    global_local->reg = MIR_new_func_reg(
      codegen.ctx, codegen.function->u.func, data_type_to_mir_type(global_local->data_type),
      memory_sprintf("%s.%d", global_local->name.lexeme, global_local->index));
  }
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
  MIR_gen_set_optimize_level(codegen.ctx, 4);

  void (*boo)(void) = NULL;
  MIR_load_external(codegen.ctx, "log", print_num);

  MIR_link(codegen.ctx, MIR_set_gen_interface, NULL);
  boo = MIR_gen(codegen.ctx, codegen.function);

  boo();

  MIR_gen_finish(codegen.ctx);
  MIR_finish(codegen.ctx);

  return result;
}
