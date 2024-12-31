#include "codegen.h"
#include "array.h"
#include "expression.h"
#include "lexer.h"
#include "main.h"
#include "map.h"
#include "memory.h"
#include "statement.h"

#include <binaryen-c.h>
#include <stdio.h>

array_def(BinaryenExpressionRef, BinaryenExpressionRef);
array_def(BinaryenPackedType, BinaryenPackedType);
array_def(BinaryenType, BinaryenType);

static BinaryenExpressionRef generate_expression(Expr* expression);
static BinaryenExpressionRef generate_statement(Stmt* statement);
static BinaryenExpressionRef generate_statements(ArrayStmt* statements);

static struct
{
  BinaryenModuleRef module;
  BinaryenType class;
  ArrayStmt statements;

  BinaryenHeapType string_heap_type;
  BinaryenType string_type;
  MapSInt string_constants;

  int strings;
  int loops;
} codegen;

static BinaryenType data_type_to_binaryen_type(DataType data_type)
{
  switch (data_type.type)
  {
  case TYPE_VOID:
  case TYPE_FUNCTION:
  case TYPE_FUNCTION_MEMBER:
    return BinaryenTypeNone();
  case TYPE_BOOL:
  case TYPE_INTEGER:
    return BinaryenTypeInt32();
  case TYPE_FLOAT:
    return BinaryenTypeFloat32();
  case TYPE_STRING:
    return codegen.string_type;
  case TYPE_OBJECT:
    return data_type.class->ref;

  default:
    UNREACHABLE("Unhandled data type");
  }
}

static void generate_string_concat_function(void)
{
  const char* name = "string.concat";

  BinaryenExpressionRef left = BinaryenLocalGet(codegen.module, 0, codegen.string_type);
  BinaryenExpressionRef right = BinaryenLocalGet(codegen.module, 1, codegen.string_type);

  BinaryenExpressionRef left_length = BinaryenArrayLen(codegen.module, left);
  BinaryenExpressionRef right_length = BinaryenArrayLen(codegen.module, right);

  BinaryenExpressionRef length =
    BinaryenBinary(codegen.module, BinaryenAddInt32(), left_length, right_length);

  BinaryenExpressionRef index = BinaryenConst(codegen.module, BinaryenLiteralInt32(0));
  BinaryenExpressionRef clone =
    BinaryenArrayNew(codegen.module, codegen.string_heap_type, length, NULL);

  BinaryenExpressionRef body_list[] = {
    BinaryenLocalSet(codegen.module, 2, clone),
    BinaryenArrayCopy(codegen.module, BinaryenLocalGet(codegen.module, 2, codegen.string_type),
                      BinaryenExpressionCopy(index, codegen.module),
                      BinaryenExpressionCopy(left, codegen.module),
                      BinaryenExpressionCopy(index, codegen.module),
                      BinaryenExpressionCopy(left_length, codegen.module)),

    BinaryenArrayCopy(codegen.module, BinaryenLocalGet(codegen.module, 2, codegen.string_type),
                      BinaryenExpressionCopy(left_length, codegen.module),
                      BinaryenExpressionCopy(right, codegen.module),
                      BinaryenExpressionCopy(index, codegen.module),
                      BinaryenExpressionCopy(right_length, codegen.module)),

    BinaryenLocalGet(codegen.module, 2, codegen.string_type)
  };

  BinaryenExpressionRef body =
    BinaryenBlock(codegen.module, NULL, body_list, sizeof(body_list) / sizeof_ptr(body_list),
                  codegen.string_type);

  BinaryenType params_list[] = { codegen.string_type, codegen.string_type };
  BinaryenType params =
    BinaryenTypeCreate(params_list, sizeof(params_list) / sizeof_ptr(params_list));

  BinaryenType results_list[] = { codegen.string_type };
  BinaryenType results =
    BinaryenTypeCreate(results_list, sizeof(results_list) / sizeof_ptr(results_list));

  BinaryenAddFunction(codegen.module, name, params, results, &codegen.string_type, 1, body);
  BinaryenAddFunctionExport(codegen.module, name, name);
}

static void generate_string_equals_function(void)
{
#define LEFT() (BinaryenLocalGet(codegen.module, 0, codegen.string_type))
#define RIGHT() (BinaryenLocalGet(codegen.module, 1, codegen.string_type))
#define COUNTER() (BinaryenLocalGet(codegen.module, 2, BinaryenTypeInt32()))
#define CONSTANT(_v) (BinaryenConst(codegen.module, BinaryenLiteralInt32(_v)))

  const char* name = "string.equals";
  const char* loop_name = "string.equals.loop";

  BinaryenExpressionRef check_value_condition = BinaryenBinary(
    codegen.module, BinaryenNeInt32(),
    BinaryenArrayGet(codegen.module, LEFT(), COUNTER(), BinaryenTypeInt32(), false),
    BinaryenArrayGet(codegen.module, RIGHT(), COUNTER(), BinaryenTypeInt32(), false));
  BinaryenExpressionRef check_value_body = BinaryenReturn(codegen.module, CONSTANT(0));
  BinaryenExpressionRef check_value =
    BinaryenIf(codegen.module, check_value_condition, check_value_body, NULL);

  BinaryenExpressionRef increment_count = BinaryenLocalSet(
    codegen.module, 2, BinaryenBinary(codegen.module, BinaryenAddInt32(), COUNTER(), CONSTANT(1)));

  BinaryenExpressionRef branch = BinaryenBreak(codegen.module, loop_name, NULL, NULL);

  BinaryenExpressionRef counter_body_list[] = { check_value, increment_count, branch };
  BinaryenExpressionRef counter_body =
    BinaryenBlock(codegen.module, NULL, counter_body_list,
                  sizeof(counter_body_list) / sizeof_ptr(counter_body_list), BinaryenTypeNone());
  BinaryenExpressionRef counter_condition = BinaryenBinary(
    codegen.module, BinaryenLtSInt32(), COUNTER(), BinaryenArrayLen(codegen.module, LEFT()));
  BinaryenExpressionRef counter = BinaryenIf(codegen.module, counter_condition, counter_body, NULL);

  BinaryenExpressionRef loop = BinaryenLoop(codegen.module, loop_name, counter);
  BinaryenExpressionRef loop_block_list[] = { loop, BinaryenReturn(codegen.module, CONSTANT(1)) };
  BinaryenExpressionRef loop_block =
    BinaryenBlock(codegen.module, NULL, loop_block_list,
                  sizeof(loop_block_list) / sizeof_ptr(loop_block_list), BinaryenTypeNone());

  BinaryenExpressionRef condition =
    BinaryenBinary(codegen.module, BinaryenEqInt32(), BinaryenArrayLen(codegen.module, LEFT()),
                   BinaryenArrayLen(codegen.module, RIGHT()));
  BinaryenExpressionRef iterate = BinaryenIf(codegen.module, condition, loop_block, NULL);
  BinaryenExpressionRef fast_exit =
    BinaryenIf(codegen.module, BinaryenRefEq(codegen.module, LEFT(), RIGHT()),
               BinaryenReturn(codegen.module, CONSTANT(1)), NULL);

  BinaryenExpressionRef body_list[] = { fast_exit, iterate,
                                        BinaryenReturn(codegen.module, CONSTANT(0)) };
  BinaryenExpressionRef body = BinaryenBlock(
    codegen.module, NULL, body_list, sizeof(body_list) / sizeof_ptr(body_list), BinaryenTypeNone());

  BinaryenType params_list[] = { codegen.string_type, codegen.string_type };
  BinaryenType params =
    BinaryenTypeCreate(params_list, sizeof(params_list) / sizeof_ptr(params_list));

  BinaryenType results_list[] = { BinaryenTypeInt32() };
  BinaryenType results =
    BinaryenTypeCreate(results_list, sizeof(results_list) / sizeof_ptr(results_list));

  BinaryenType vars_list[] = { BinaryenTypeInt32() };

  BinaryenAddFunction(codegen.module, name, params, results, vars_list,
                      sizeof(vars_list) / sizeof_ptr(vars_list), body);
  BinaryenAddFunctionExport(codegen.module, name, name);

#undef LEFT
#undef RIGHT
#undef COUNTER
#undef CONSTANT
}

static void generate_string_at_function(void)
{
  const char* name = "string.at";

  BinaryenExpressionRef ref = BinaryenLocalGet(codegen.module, 0, codegen.string_type);
  BinaryenExpressionRef index = BinaryenLocalGet(codegen.module, 1, BinaryenTypeInt32());

  BinaryenExpressionRef body = BinaryenLocalTee(
    codegen.module, 2, BinaryenArrayGet(codegen.module, ref, index, BinaryenTypeInt32(), false),
    BinaryenTypeInt32());

  BinaryenType params_list[] = { codegen.string_type, BinaryenTypeInt32() };
  BinaryenType params =
    BinaryenTypeCreate(params_list, sizeof(params_list) / sizeof_ptr(params_list));

  BinaryenType results_list[] = { BinaryenTypeInt32() };
  BinaryenType results =
    BinaryenTypeCreate(results_list, sizeof(results_list) / sizeof_ptr(results_list));

  BinaryenType vars_list[] = { BinaryenTypeInt32() };

  BinaryenAddFunction(codegen.module, name, params, results, vars_list,
                      sizeof(vars_list) / sizeof_ptr(vars_list), body);
  BinaryenAddFunctionExport(codegen.module, name, name);
}

static BinaryenExpressionRef generate_default_initialization(DataType data_type)
{
  switch (data_type.type)
  {
  case TYPE_INTEGER:
  case TYPE_BOOL:
    return BinaryenConst(codegen.module, BinaryenLiteralInt32(0));
  case TYPE_FLOAT:
    return BinaryenConst(codegen.module, BinaryenLiteralFloat32(0));
  case TYPE_OBJECT:
    return BinaryenRefNull(codegen.module, data_type.class->ref);
  case TYPE_STRING:
    return BinaryenArrayNewFixed(codegen.module, codegen.string_heap_type, NULL, 0);
  default:
    UNREACHABLE("Unexpected default initializer");
  }
}

static BinaryenExpressionRef generate_group_expression(GroupExpr* expression)
{
  return generate_expression(expression->expr);
}

static BinaryenExpressionRef generate_string_literal_expression(const char* literal)
{
  int index = map_get_sint(&codegen.string_constants, literal);
  if (!index)
  {
    index = ++codegen.strings;
    map_put_sint(&codegen.string_constants, literal, index);

    ArrayBinaryenExpressionRef values;
    array_init(&values);

    size_t length = strlen(literal);
    for (size_t i = 0; i < length; i++)
      array_add(&values, BinaryenConst(codegen.module, BinaryenLiteralInt32(literal[i])));

    BinaryenExpressionRef initializer =
      BinaryenArrayNewFixed(codegen.module, codegen.string_heap_type, values.elems, values.size);

    const char* name = memory_sprintf(&memory, "string.%d", index);
    BinaryenAddGlobal(codegen.module, name, codegen.string_type, false, initializer);
  }

  const char* name = memory_sprintf(&memory, "string.%d", index);
  return BinaryenGlobalGet(codegen.module, name, codegen.string_type);
}

static BinaryenExpressionRef generate_literal_expression(LiteralExpr* expression)
{
  switch (expression->data_type.type)
  {
  case TYPE_INTEGER:
    return BinaryenConst(codegen.module, BinaryenLiteralInt32(expression->integer));
  case TYPE_FLOAT:
    return BinaryenConst(codegen.module, BinaryenLiteralFloat32(expression->floating));
  case TYPE_BOOL:
    return BinaryenConst(codegen.module, BinaryenLiteralInt32(expression->boolean));
  case TYPE_OBJECT:
    return BinaryenRefNull(codegen.module, BinaryenTypeNullref());
  case TYPE_STRING:
    return generate_string_literal_expression(expression->string);
  default:
    UNREACHABLE("Unhandled literal value");
  }
}

static BinaryenExpressionRef generate_binary_expression(BinaryExpr* expression)
{
  BinaryenExpressionRef left = generate_expression(expression->left);
  BinaryenExpressionRef right = generate_expression(expression->right);
  BinaryenOp op = 0;

  DataType data_type = expression->operand_data_type;

  switch (expression->op.type)
  {
  case TOKEN_PLUS:
    if (data_type.type == TYPE_INTEGER)
      op = BinaryenAddInt32();
    else if (data_type.type == TYPE_FLOAT)
      op = BinaryenAddFloat32();
    else if (data_type.type == TYPE_STRING)
    {
      BinaryenExpressionRef operands[] = { left, right };
      return BinaryenCall(codegen.module, "string.concat", operands,
                          sizeof(operands) / sizeof_ptr(operands), codegen.string_type);
    }
    else
      UNREACHABLE("Unsupported binary type for +");

    break;
  case TOKEN_MINUS:
    if (data_type.type == TYPE_INTEGER)
      op = BinaryenSubInt32();
    else if (data_type.type == TYPE_FLOAT)
      op = BinaryenSubFloat32();
    else
      UNREACHABLE("Unsupported binary type for -");

    break;
  case TOKEN_STAR:
    if (data_type.type == TYPE_INTEGER)
      op = BinaryenMulInt32();
    else if (data_type.type == TYPE_FLOAT)
      op = BinaryenMulFloat32();
    else
      UNREACHABLE("Unsupported binary type for *");

    break;
  case TOKEN_SLASH:
    if (data_type.type == TYPE_INTEGER)
      op = BinaryenDivSInt32();
    else if (data_type.type == TYPE_FLOAT)
      op = BinaryenDivFloat32();
    else
      UNREACHABLE("Unsupported binary type for /");

    break;

  case TOKEN_PERCENT:
    if (data_type.type == TYPE_INTEGER)
      op = BinaryenRemSInt32();
    else
      UNREACHABLE("Unsupported binary type for %");

    break;

  case TOKEN_EQUAL_EQUAL:
    if (data_type.type == TYPE_INTEGER || data_type.type == TYPE_BOOL)
      op = BinaryenEqInt32();
    else if (data_type.type == TYPE_FLOAT)
      op = BinaryenEqFloat32();
    else if (data_type.type == TYPE_OBJECT)
      return BinaryenRefEq(codegen.module, left, right);
    else if (data_type.type == TYPE_STRING)
    {
      BinaryenExpressionRef operands[] = { left, right };
      return BinaryenCall(codegen.module, "string.equals", operands,
                          sizeof(operands) / sizeof_ptr(operands), BinaryenTypeInt32());
    }
    else
      UNREACHABLE("Unsupported binary type for ==");

    break;

  case TOKEN_BANG_EQUAL:
    if (data_type.type == TYPE_INTEGER || data_type.type == TYPE_BOOL)
      op = BinaryenNeInt32();
    else if (data_type.type == TYPE_FLOAT)
      op = BinaryenNeFloat32();
    else if (data_type.type == TYPE_OBJECT)
      return BinaryenUnary(codegen.module, BinaryenEqZInt32(),
                           BinaryenRefEq(codegen.module, left, right));
    else
      UNREACHABLE("Unsupported binary type for ==");

    break;

  case TOKEN_LESS_EQUAL:
    if (data_type.type == TYPE_INTEGER || data_type.type == TYPE_BOOL)
      op = BinaryenLeSInt32();
    else if (data_type.type == TYPE_FLOAT)
      op = BinaryenLeFloat32();
    else
      UNREACHABLE("Unsupported binary type for <=");

    break;

  case TOKEN_GREATER_EQUAL:
    if (data_type.type == TYPE_INTEGER || data_type.type == TYPE_BOOL)
      op = BinaryenGeSInt32();
    else if (data_type.type == TYPE_FLOAT)
      op = BinaryenGeFloat32();
    else
      UNREACHABLE("Unsupported binary type for <=");

    break;

  case TOKEN_LESS:
    if (data_type.type == TYPE_INTEGER || data_type.type == TYPE_BOOL)
      op = BinaryenLtSInt32();
    else if (data_type.type == TYPE_FLOAT)
      op = BinaryenLtFloat32();
    else
      UNREACHABLE("Unsupported binary type for <");

    break;

  case TOKEN_GREATER:
    if (data_type.type == TYPE_INTEGER || data_type.type == TYPE_BOOL)
      op = BinaryenGtSInt32();
    else if (data_type.type == TYPE_FLOAT)
      op = BinaryenGtFloat32();
    else
      UNREACHABLE("Unsupported binary type for >");

    break;

  case TOKEN_AND:
    if (data_type.type == TYPE_BOOL)
    {
      BinaryenExpressionRef if_true = right;
      BinaryenExpressionRef if_false = BinaryenConst(codegen.module, BinaryenLiteralInt32(0));

      return BinaryenIf(codegen.module, left, if_true, if_false);
    }
    else
      UNREACHABLE("Unsupported binary type for AND");

    break;

  case TOKEN_OR:
    if (data_type.type == TYPE_BOOL)
    {
      BinaryenExpressionRef if_true = BinaryenConst(codegen.module, BinaryenLiteralInt32(1));
      BinaryenExpressionRef if_false = right;

      return BinaryenIf(codegen.module, left, if_true, if_false);
    }
    else
      UNREACHABLE("Unsupported binary type for OR");

    break;

  default:
    UNREACHABLE("Unhandled binary operation");
  }

  return BinaryenBinary(codegen.module, op, left, right);
}

static BinaryenExpressionRef generate_unary_expression(UnaryExpr* expression)
{
  BinaryenExpressionRef value = generate_expression(expression->expr);

  switch (expression->op.type)
  {
  case TOKEN_MINUS:
    if (expression->data_type.type == TYPE_INTEGER)
      return BinaryenBinary(codegen.module, BinaryenSubInt32(),
                            BinaryenConst(codegen.module, BinaryenLiteralInt32(0)), value);
    else if (expression->data_type.type == TYPE_FLOAT)
      return BinaryenUnary(codegen.module, BinaryenNegFloat32(), value);
    else
      UNREACHABLE("Unsupported unary type for -");

  case TOKEN_BANG:
  case TOKEN_NOT:
    if (expression->data_type.type == TYPE_BOOL)
      return BinaryenUnary(codegen.module, BinaryenEqZInt32(), value);
    else
      UNREACHABLE("Unsupported unary type for !");

  default:
    UNREACHABLE("Unhandled unary expression");
  }
}

static BinaryenExpressionRef generate_cast_expression(CastExpr* expression)
{
  BinaryenExpressionRef value = generate_expression(expression->expr);

  if (expression->to_data_type.type == TYPE_FLOAT &&
      expression->from_data_type.type == TYPE_INTEGER)
  {
    return BinaryenUnary(codegen.module, BinaryenConvertSInt32ToFloat32(), value);
  }
  else if (expression->to_data_type.type == TYPE_BOOL)
  {
    switch (expression->from_data_type.type)
    {
    case TYPE_INTEGER:
      return value;
    case TYPE_OBJECT:
      return BinaryenUnary(codegen.module, BinaryenEqZInt32(),
                           BinaryenRefIsNull(codegen.module, value));
    default:
      break;
    }
  }

  UNREACHABLE("Unsupported cast type");
}

static BinaryenExpressionRef generate_variable_expression(VarExpr* expression)
{
  BinaryenType type = data_type_to_binaryen_type(expression->data_type);

  switch (expression->variable->scope)
  {
  case SCOPE_LOCAL:
    return BinaryenLocalGet(codegen.module, expression->variable->index, type);
  case SCOPE_GLOBAL:
    return BinaryenGlobalGet(codegen.module, expression->name.lexeme, type);
  case SCOPE_CLASS:
    return BinaryenStructGet(codegen.module, expression->variable->index,
                             BinaryenLocalGet(codegen.module, 0, codegen.class), type, false);
  default:
    UNREACHABLE("Unhandled scope type");
  }
}

static BinaryenExpressionRef generate_assignment_expression(AssignExpr* expression)
{
  BinaryenExpressionRef value = generate_expression(expression->value);
  BinaryenType type = data_type_to_binaryen_type(expression->data_type);
  VarStmt* variable = expression->variable;

  switch (variable->scope)
  {
  case SCOPE_LOCAL:
    return BinaryenLocalTee(codegen.module, variable->index, value, type);

  case SCOPE_GLOBAL: {
    BinaryenExpressionRef list[] = {
      BinaryenGlobalSet(codegen.module, variable->name.lexeme, value),
      BinaryenGlobalGet(codegen.module, variable->name.lexeme, type),
    };

    return BinaryenBlock(codegen.module, NULL, list, sizeof(list) / sizeof_ptr(list), type);
  }

  case SCOPE_CLASS: {
    BinaryenExpressionRef ref;
    if (expression->target->type == EXPR_ACCESS)
      ref = generate_expression(expression->target->access.expr);
    else
      ref = BinaryenLocalGet(codegen.module, 0, codegen.class);

    BinaryenExpressionRef list[] = {
      BinaryenStructSet(codegen.module, variable->index, ref, value),
      BinaryenStructGet(codegen.module, variable->index,
                        BinaryenExpressionCopy(ref, codegen.module), codegen.class, false),
    };

    return BinaryenBlock(codegen.module, NULL, list, sizeof(list) / sizeof_ptr(list), type);
  }

  default:
    UNREACHABLE("Unhandled scope type");
  }
}

static BinaryenExpressionRef generate_call_expression(CallExpr* expression)
{
  const char* name;
  DataType callee_data_type = expression->callee_data_type;

  switch (callee_data_type.type)
  {
  case TYPE_FUNCTION:
    name = callee_data_type.function->name.lexeme;
    break;
  case TYPE_FUNCTION_MEMBER:
    name = callee_data_type.function_member.function->name.lexeme;
    break;
  case TYPE_PROTOTYPE:
    name = callee_data_type.class->name.lexeme;
    break;
  default:
    UNREACHABLE("Unhandled data type");
  }

  BinaryenType return_type = data_type_to_binaryen_type(expression->return_data_type);

  ArrayBinaryenExpressionRef arguments;
  array_init(&arguments);

  Expr* argument;
  array_foreach(&expression->arguments, argument)
  {
    array_add(&arguments, generate_expression(argument));
  }

  return BinaryenCall(codegen.module, name, arguments.elems, arguments.size, return_type);
}

static BinaryenExpressionRef generate_access_expression(AccessExpr* expression)
{
  BinaryenExpressionRef ref = generate_expression(expression->expr);

  if (expression->data_type.type == TYPE_STRING)
  {
    if (strcmp(expression->name.lexeme, "length") == 0)
    {
      return BinaryenArrayLen(codegen.module, ref);
    }

    UNREACHABLE("Unhandled string access name");
  }
  else
  {
    BinaryenType type = data_type_to_binaryen_type(expression->data_type);
    return BinaryenStructGet(codegen.module, expression->variable->index, ref, type, false);
  }
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

  default:
    UNREACHABLE("Unhandled expression");
  }
}

static BinaryenExpressionRef generate_expression_statement(ExprStmt* statement)
{
  BinaryenExpressionRef expression = generate_expression(statement->expr);
  if (statement->data_type.type != TYPE_VOID)
  {
    expression = BinaryenDrop(codegen.module, expression);
  }

  return expression;
}

static BinaryenExpressionRef generate_if_statement(IfStmt* statement)
{
  BinaryenExpressionRef condition = generate_expression(statement->condition);
  BinaryenExpressionRef ifTrue = generate_statements(&statement->then_branch);
  BinaryenExpressionRef ifFalse = NULL;

  if (statement->else_branch.elems)
  {
    ifFalse = generate_statements(&statement->else_branch);
  }

  return BinaryenIf(codegen.module, condition, ifTrue, ifFalse);
}

static BinaryenExpressionRef generate_while_statement(WhileStmt* statement)
{
  codegen.loops++;

  const char* continue_name = memory_sprintf(&memory, "continue|%d", codegen.loops);
  const char* break_name = memory_sprintf(&memory, "break|%d", codegen.loops);
  const char* loop_name = memory_sprintf(&memory, "loop|%d", codegen.loops);

  BinaryenExpressionRef continue_block = generate_statements(&statement->body);
  BinaryenBlockSetName(continue_block, continue_name);

  ArrayBinaryenExpressionRef loop_block_list;
  array_init(&loop_block_list);
  array_add(&loop_block_list, continue_block);

  if (statement->incrementer)
  {
    array_add(&loop_block_list, generate_statement(statement->incrementer));
  }

  array_add(&loop_block_list, BinaryenBreak(codegen.module, loop_name, NULL, NULL));

  BinaryenExpressionRef loop_block = BinaryenBlock(codegen.module, NULL, loop_block_list.elems,
                                                   loop_block_list.size, BinaryenTypeNone());

  BinaryenExpressionRef loop_body =
    BinaryenIf(codegen.module, generate_expression(statement->condition), loop_block, NULL);
  BinaryenExpressionRef loop = BinaryenLoop(codegen.module, loop_name, loop_body);

  ArrayBinaryenExpressionRef block_list;
  array_init(&block_list);

  if (statement->initializer)
  {
    array_add(&block_list, generate_statement(statement->initializer));
  }

  array_add(&block_list, loop);

  return BinaryenBlock(codegen.module, break_name, block_list.elems, block_list.size,
                       BinaryenTypeNone());
}

static BinaryenExpressionRef generate_return_statement(ReturnStmt* statement)
{
  BinaryenExpressionRef expression = NULL;
  if (statement->expr)
    expression = generate_expression(statement->expr);

  return BinaryenReturn(codegen.module, expression);
}

static BinaryenExpressionRef generate_continue_statement(void)
{
  const char* name = memory_sprintf(&memory, "continue|%d", codegen.loops);
  return BinaryenBreak(codegen.module, name, NULL, NULL);
}

static BinaryenExpressionRef generate_break_statement(void)
{
  const char* name = memory_sprintf(&memory, "break|%d", codegen.loops);
  return BinaryenBreak(codegen.module, name, NULL, NULL);
}

static BinaryenExpressionRef generate_variable_declaration(VarStmt* statement)
{
  BinaryenExpressionRef initializer = generate_default_initialization(statement->data_type);

  if (statement->scope == SCOPE_GLOBAL)
  {
    const char* name = statement->name.lexeme;
    BinaryenType type = data_type_to_binaryen_type(statement->data_type);
    BinaryenAddGlobal(codegen.module, name, type, true, initializer);

    if (statement->initializer)
    {
      return BinaryenGlobalSet(codegen.module, statement->name.lexeme,
                               generate_expression(statement->initializer));
    }
    else
    {
      return NULL;
    }
  }
  else if (statement->scope == SCOPE_LOCAL)
  {
    if (statement->initializer)
    {
      initializer = generate_expression(statement->initializer);
    }

    return BinaryenLocalSet(codegen.module, statement->index, initializer);
  }
  else
  {
    UNREACHABLE("Unexpected scope type");
  }
}

static BinaryenExpressionRef generate_function_declaration(FuncStmt* statement)
{
  ArrayBinaryenType parameter_types;
  array_init(&parameter_types);

  VarStmt* parameter;
  array_foreach(&statement->parameters, parameter)
  {
    BinaryenType parameter_data_type = data_type_to_binaryen_type(parameter->data_type);
    array_add(&parameter_types, parameter_data_type);
  }

  ArrayBinaryenType variable_types;
  array_init(&variable_types);

  VarStmt* variable;
  array_foreach(&statement->variables, variable)
  {
    BinaryenType variable_data_type = data_type_to_binaryen_type(variable->data_type);
    array_add(&variable_types, variable_data_type);
  }

  const char* name = statement->name.lexeme;
  BinaryenType params = BinaryenTypeCreate(parameter_types.elems, parameter_types.size);
  BinaryenType results = data_type_to_binaryen_type(statement->data_type);

  if (statement->body.elems)
  {
    BinaryenExpressionRef body = generate_statements(&statement->body);
    BinaryenAddFunction(codegen.module, name, params, results, variable_types.elems,
                        variable_types.size, body);
    BinaryenAddFunctionExport(codegen.module, name, name);
  }
  else
  {
    BinaryenAddFunctionImport(codegen.module, name, "env", name, params, results);
  }

  return NULL;
}

static BinaryenExpressionRef generate_class_declaration(ClassStmt* statement)
{
  TypeBuilderRef type_builder = TypeBuilderCreate(1);
  BinaryenHeapType temporary_heap_type = TypeBuilderGetTempHeapType(type_builder, 0);
  BinaryenType temporary_type = TypeBuilderGetTempRefType(type_builder, temporary_heap_type, true);

  statement->ref = temporary_type;

  ArrayBinaryenType types;
  ArrayBinaryenPackedType packed_types;
  ArrayBool mutables;

  array_init(&types);
  array_init(&packed_types);
  array_init(&mutables);

  VarStmt* variable;
  array_foreach(&statement->variables, variable)
  {
    BinaryenType type = data_type_to_binaryen_type(variable->data_type);
    BinaryenPackedType packed_type = BinaryenPackedTypeNotPacked();
    bool mutable = true;

    array_add(&types, type);
    array_add(&packed_types, packed_type);
    array_add(&mutables, mutable);
  }

  TypeBuilderSetStructType(type_builder, 0, types.elems, packed_types.elems, mutables.elems,
                           types.size);

  BinaryenHeapType heap_type;
  TypeBuilderBuildAndDispose(type_builder, &heap_type, 0, 0);

  BinaryenType type = BinaryenTypeFromHeapType(heap_type, true);
  statement->ref = type;

  BinaryenType previous_class = codegen.class;
  codegen.class = type;

  const char* initalizer_name = statement->name.lexeme;
  const char* initalizer_function_name = memory_sprintf(&memory, "%s.__init__", initalizer_name);

  FuncStmt* function;
  FuncStmt* initializer_function = NULL;

  array_foreach(&statement->functions, function)
  {
    if (strcmp(function->name.lexeme, initalizer_function_name) == 0)
      initializer_function = function;

    generate_function_declaration(function);
  }

  ArrayBinaryenExpressionRef initializer_body;
  array_init(&initializer_body);
  array_add(
    &initializer_body,
    BinaryenLocalSet(codegen.module, 0, BinaryenStructNew(codegen.module, NULL, 0, heap_type)));

  array_foreach(&statement->variables, variable)
  {
    if (variable->initializer)
    {
      BinaryenExpressionRef ref = BinaryenLocalGet(codegen.module, 0, type);
      BinaryenExpressionRef value = generate_expression(variable->initializer);
      array_add(&initializer_body, BinaryenStructSet(codegen.module, variable->index, ref, value));
    }
  }

  ArrayBinaryenType parameter_types;
  array_init(&parameter_types);
  array_add(&parameter_types, type);

  if (initializer_function)
  {
    ArrayBinaryenExpressionRef parameters;
    array_init(&parameters);
    array_add(&parameters, BinaryenLocalGet(codegen.module, 0, type));

    for (unsigned int i = 1; i < initializer_function->parameters.size; i++)
    {
      VarStmt* parameter = array_at(&initializer_function->parameters, i);
      BinaryenType parameter_type = data_type_to_binaryen_type(parameter->data_type);

      array_add(&parameters, BinaryenLocalGet(codegen.module, i, parameter_type));
      array_add(&parameter_types, parameter_type);
    }

    array_add(&initializer_body,
              BinaryenCall(codegen.module, initalizer_function_name, parameters.elems,
                           parameters.size, BinaryenTypeNone()));
  }

  array_add(&initializer_body, BinaryenLocalGet(codegen.module, 0, type));

  BinaryenType initializer_params = BinaryenTypeCreate(parameter_types.elems, parameter_types.size);
  BinaryenExpressionRef initializer =
    BinaryenBlock(codegen.module, NULL, initializer_body.elems, initializer_body.size, type);

  BinaryenAddFunction(codegen.module, initalizer_name, initializer_params, type, NULL, 0,
                      initializer);
  BinaryenAddFunctionExport(codegen.module, initalizer_name, initalizer_name);

  codegen.class = previous_class;
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
  case STMT_CLASS_DECL:
    return generate_class_declaration(&statement->class);

  default:
    UNREACHABLE("Unhandled statement");
  }
}

static BinaryenExpressionRef generate_statements(ArrayStmt* statements)
{
  ArrayBinaryenExpressionRef list;
  array_init(&list);

  Stmt* statement;
  array_foreach(statements, statement)
  {
    BinaryenExpressionRef ref = generate_statement(statement);
    if (ref)
    {
      array_add(&list, ref);
    }
  }

  BinaryenExpressionRef block =
    BinaryenBlock(codegen.module, NULL, list.elems, list.size, BinaryenTypeAuto());

  return block;
}

void codegen_init(ArrayStmt statements)
{
  codegen.module = BinaryenModuleCreate();
  codegen.class = BinaryenTypeNone();
  codegen.statements = statements;
  codegen.loops = -1;
  codegen.strings = 0;

  TypeBuilderRef type_builder = TypeBuilderCreate(1);
  TypeBuilderSetArrayType(type_builder, 0, BinaryenTypeInt32(), BinaryenPackedTypeInt8(), true);
  TypeBuilderBuildAndDispose(type_builder, &codegen.string_heap_type, 0, 0);

  codegen.string_type = BinaryenTypeFromHeapType(codegen.string_heap_type, false);
  map_init_sint(&codegen.string_constants, 0, 0);

  generate_string_concat_function();
  generate_string_equals_function();
  generate_string_at_function();
}

Codegen codegen_generate(void)
{
  Codegen result = { 0 };
  BinaryenFunctionRef start =
    BinaryenAddFunction(codegen.module, "~start", BinaryenTypeNone(), BinaryenTypeNone(), NULL, 0,
                        generate_statements(&codegen.statements));

  BinaryenSetStart(codegen.module, start);
  BinaryenModuleSetFeatures(codegen.module, BinaryenFeatureReferenceTypes() | BinaryenFeatureGC());

  if (BinaryenModuleValidate(codegen.module))
  {
    BinaryenModuleOptimize(codegen.module);

    BinaryenModuleAllocateAndWriteResult binaryen_result =
      BinaryenModuleAllocateAndWrite(codegen.module, NULL);

    result.data = binaryen_result.binary;
    result.size = binaryen_result.binaryBytes;
  }

  BinaryenModulePrint(codegen.module);
  BinaryenModuleDispose(codegen.module);
  return result;
}
