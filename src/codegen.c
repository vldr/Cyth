#include "codegen.h"
#include "array.h"
#include "expression.h"
#include "lexer.h"
#include "main.h"
#include "statement.h"

#include <binaryen-c.h>
#include <stdio.h>

array_def(BinaryenExpressionRef, BinaryenExpressionRef);
array_def(BinaryenType, BinaryenType);

static BinaryenExpressionRef generate_expression(Expr* expression);
static BinaryenExpressionRef generate_statements(ArrayStmt* statements);

static struct
{
  BinaryenModuleRef module;
  ArrayStmt statements;
} codegen;

static BinaryenType data_type_to_binaryen_type(DataType data_type)
{
  switch (data_type)
  {
  case TYPE_VOID:
  case TYPE_NULL:
    return BinaryenTypeNone();
  case TYPE_BOOL:
  case TYPE_INTEGER:
    return BinaryenTypeInt32();
  case TYPE_FLOAT:
    return BinaryenTypeFloat32();

  default:
    ERROR("Unhandled data type");
  }
}

static BinaryenExpressionRef generate_group_expression(Expr* expression)
{
  return generate_expression(expression->group.expr);
}

static BinaryenExpressionRef generate_literal_expression(Expr* expression)
{
  switch (expression->literal.data_type)
  {
  case TYPE_INTEGER:
    return BinaryenConst(codegen.module, BinaryenLiteralInt32(expression->literal.integer));
  case TYPE_FLOAT:
    return BinaryenConst(codegen.module, BinaryenLiteralFloat32(expression->literal.floating));
  case TYPE_BOOL:
    return BinaryenConst(codegen.module, BinaryenLiteralInt32(expression->literal.boolean));

  default:
    ERROR("Unhandled literal value");
  }
}

static BinaryenExpressionRef generate_binary_expression(Expr* expression)
{
  BinaryenExpressionRef left = generate_expression(expression->binary.left);
  BinaryenExpressionRef right = generate_expression(expression->binary.right);
  BinaryenOp op = 0;

  DataType data_type = expression->binary.data_type;

  switch (expression->binary.op.type)
  {
  case TOKEN_PLUS:
    if (data_type == TYPE_INTEGER)
      op = BinaryenAddInt32();
    else if (data_type == TYPE_FLOAT)
      op = BinaryenAddFloat32();
    else
      ERROR("Unsupported binary type for +");

    break;
  case TOKEN_MINUS:
    if (data_type == TYPE_INTEGER)
      op = BinaryenSubInt32();
    else if (data_type == TYPE_FLOAT)
      op = BinaryenSubFloat32();
    else
      ERROR("Unsupported binary type for -");

    break;
  case TOKEN_STAR:
    if (data_type == TYPE_INTEGER)
      op = BinaryenMulInt32();
    else if (data_type == TYPE_FLOAT)
      op = BinaryenMulFloat32();
    else
      ERROR("Unsupported binary type for *");

    break;
  case TOKEN_SLASH:
    if (data_type == TYPE_INTEGER)
      op = BinaryenDivSInt32();
    else if (data_type == TYPE_FLOAT)
      op = BinaryenDivFloat32();
    else
      ERROR("Unsupported binary type for /");

    break;

  case TOKEN_PERCENT:
    if (data_type == TYPE_INTEGER)
      op = BinaryenRemSInt32();
    else
      ERROR("Unsupported binary type for %");

    break;

  case TOKEN_EQUAL_EQUAL:
    if (data_type == TYPE_INTEGER || data_type == TYPE_BOOL)
      op = BinaryenEqInt32();
    else if (data_type == TYPE_FLOAT)
      op = BinaryenEqFloat32();
    else
      ERROR("Unsupported binary type for ==");

    break;

  case TOKEN_BANG_EQUAL:
    if (data_type == TYPE_INTEGER || data_type == TYPE_BOOL)
      op = BinaryenNeInt32();
    else if (data_type == TYPE_FLOAT)
      op = BinaryenNeFloat32();
    else
      ERROR("Unsupported binary type for ==");

    break;

  case TOKEN_LESS_EQUAL:
    if (data_type == TYPE_INTEGER || data_type == TYPE_BOOL)
      op = BinaryenLeSInt32();
    else if (data_type == TYPE_FLOAT)
      op = BinaryenLeFloat32();
    else
      ERROR("Unsupported binary type for <=");

    break;

  case TOKEN_GREATER_EQUAL:
    if (data_type == TYPE_INTEGER || data_type == TYPE_BOOL)
      op = BinaryenGeSInt32();
    else if (data_type == TYPE_FLOAT)
      op = BinaryenGeFloat32();
    else
      ERROR("Unsupported binary type for <=");

    break;

  case TOKEN_LESS:
    if (data_type == TYPE_INTEGER || data_type == TYPE_BOOL)
      op = BinaryenLtSInt32();
    else if (data_type == TYPE_FLOAT)
      op = BinaryenLtFloat32();
    else
      ERROR("Unsupported binary type for <");

    break;

  case TOKEN_GREATER:
    if (data_type == TYPE_INTEGER || data_type == TYPE_BOOL)
      op = BinaryenGtSInt32();
    else if (data_type == TYPE_FLOAT)
      op = BinaryenGtFloat32();
    else
      ERROR("Unsupported binary type for >");

    break;

  case TOKEN_AND:
    if (data_type == TYPE_BOOL)
    {
      BinaryenExpressionRef zero = BinaryenConst(codegen.module, BinaryenLiteralInt32(0));
      BinaryenExpressionRef ifTrue = right;
      BinaryenExpressionRef ifFalse = zero;

      return BinaryenIf(codegen.module, left, ifTrue, ifFalse);
    }
    else
      ERROR("Unsupported binary type for AND");

    break;

  case TOKEN_OR:
    if (data_type == TYPE_BOOL)
    {
      BinaryenExpressionRef one = BinaryenConst(codegen.module, BinaryenLiteralInt32(1));
      BinaryenExpressionRef ifTrue = one;
      BinaryenExpressionRef ifFalse = right;

      return BinaryenIf(codegen.module, left, ifTrue, ifFalse);
    }
    else
      ERROR("Unsupported binary type for OR");

    break;

  default:
    ERROR("Unhandled binary operation");
  }

  return BinaryenBinary(codegen.module, op, left, right);
}

static BinaryenExpressionRef generate_unary_expression(Expr* expression)
{
  BinaryenExpressionRef value = generate_expression(expression->unary.expr);

  switch (expression->unary.op.type)
  {
  case TOKEN_MINUS:
    if (expression->data_type == TYPE_INTEGER)
      return BinaryenBinary(codegen.module, BinaryenSubInt32(),
                            BinaryenConst(codegen.module, BinaryenLiteralInt32(0)), value);
    else if (expression->data_type == TYPE_FLOAT)
      return BinaryenUnary(codegen.module, BinaryenNegFloat32(), value);
    else
      ERROR("Unsupported unary type for -");

  case TOKEN_BANG:
  case TOKEN_NOT:
    if (expression->data_type == TYPE_BOOL)
      return BinaryenUnary(codegen.module, BinaryenEqZInt32(), value);
    else
      ERROR("Unsupported unary type for !");

  default:
    ERROR("Unhandled unary expression");
  }
}

static BinaryenExpressionRef generate_cast_expression(Expr* expression)
{
  if (expression->data_type == TYPE_FLOAT && expression->cast.expr->data_type == TYPE_INTEGER)
  {
    BinaryenExpressionRef value = generate_expression(expression->cast.expr);
    return BinaryenUnary(codegen.module, BinaryenConvertSInt32ToFloat32(), value);
  }

  ERROR("Unsupported cast type");
}

static BinaryenExpressionRef generate_variable_expression(Expr* expression)
{
  BinaryenType type = data_type_to_binaryen_type(expression->data_type);

  if (expression->var.index == -1)
    return BinaryenGlobalGet(codegen.module, expression->var.name.lexeme, type);
  else
    return BinaryenLocalGet(codegen.module, expression->var.index, type);
}

static BinaryenExpressionRef generate_assignment_expression(Expr* expression)
{
  BinaryenExpressionRef value = generate_expression(expression->assign.value);
  BinaryenType type = data_type_to_binaryen_type(expression->data_type);

  if (expression->assign.index == -1)
  {
    BinaryenExpressionRef list[] = {
      BinaryenGlobalSet(codegen.module, expression->assign.name.lexeme, value),
      BinaryenGlobalGet(codegen.module, expression->assign.name.lexeme, type),
    };

    return BinaryenBlock(codegen.module, NULL, list, sizeof(list) / sizeof(*list), type);
  }
  else
  {
    return BinaryenLocalTee(codegen.module, expression->assign.index, value, type);
  }
}

static BinaryenExpressionRef generate_call_expression(Expr* expression)
{
  BinaryenType type = data_type_to_binaryen_type(expression->data_type);

  ArrayBinaryenExpressionRef arguments;
  array_init(&arguments);

  Expr* argument;
  array_foreach(&expression->call.arguments, argument)
  {
    array_add(&arguments, generate_expression(argument));
  }

  return BinaryenCall(codegen.module, expression->call.name.lexeme, arguments.elems, arguments.size,
                      type);
}

static BinaryenExpressionRef generate_expression(Expr* expression)
{
  switch (expression->type)
  {
  case EXPR_LITERAL:
    return generate_literal_expression(expression);
  case EXPR_BINARY:
    return generate_binary_expression(expression);
  case EXPR_GROUP:
    return generate_group_expression(expression);
  case EXPR_UNARY:
    return generate_unary_expression(expression);
  case EXPR_CAST:
    return generate_cast_expression(expression);
  case EXPR_VAR:
    return generate_variable_expression(expression);
  case EXPR_ASSIGN:
    return generate_assignment_expression(expression);
  case EXPR_CALL:
    return generate_call_expression(expression);

  default:
    ERROR("Unhandled expression");
  }
}

static BinaryenExpressionRef generate_expression_statement(Stmt* statement)
{
  BinaryenExpressionRef expression = generate_expression(statement->expr.expr);
  if (statement->expr.expr->data_type != TYPE_VOID)
  {
    expression = BinaryenDrop(codegen.module, expression);
  }

  return expression;
}

static BinaryenExpressionRef generate_return_statement(Stmt* statement)
{
  BinaryenExpressionRef expression = NULL;
  if (statement->ret.expr)
    expression = generate_expression(statement->ret.expr);

  return BinaryenReturn(codegen.module, expression);
}

static BinaryenExpressionRef generate_variable_declaration(Stmt* declaration)
{
  BinaryenExpressionRef initializer;
  switch (declaration->var.data_type)
  {
  case TYPE_INTEGER:
  case TYPE_BOOL:
    initializer = BinaryenConst(codegen.module, BinaryenLiteralInt32(0));
    break;
  case TYPE_FLOAT:
    initializer = BinaryenConst(codegen.module, BinaryenLiteralFloat32(0));
    break;
  default:
    ERROR("Unexpected default initializer");
  }

  if (declaration->var.index == -1)
  {
    const char* name = declaration->var.name.lexeme;
    BinaryenType type = data_type_to_binaryen_type(declaration->var.data_type);
    BinaryenAddGlobal(codegen.module, name, type, true, initializer);

    if (declaration->var.initializer)
    {
      return BinaryenGlobalSet(codegen.module, declaration->var.name.lexeme,
                               generate_expression(declaration->var.initializer));
    }
    else
    {
      return NULL;
    }
  }
  else
  {
    if (declaration->var.initializer)
    {
      initializer = generate_expression(declaration->var.initializer);
    }

    return BinaryenLocalSet(codegen.module, declaration->var.index, initializer);
  }
}

static BinaryenExpressionRef generate_function_declaration(Stmt* declaration)
{
  ArrayBinaryenType parameter_types;
  array_init(&parameter_types);

  Stmt* parameter;
  array_foreach(&declaration->func.parameters, parameter)
  {
    BinaryenType parameter_data_type = data_type_to_binaryen_type(parameter->var.data_type);
    array_add(&parameter_types, parameter_data_type);
  }

  ArrayBinaryenType variable_types;
  array_init(&variable_types);

  Stmt* variable;
  array_foreach(&declaration->func.variables, variable)
  {
    BinaryenType variable_data_type = data_type_to_binaryen_type(variable->var.data_type);
    array_add(&variable_types, variable_data_type);
  }

  const char* name = declaration->func.name.lexeme;
  BinaryenExpressionRef body = generate_statements(&declaration->func.body);
  BinaryenType results = data_type_to_binaryen_type(declaration->func.data_type);
  BinaryenType params = BinaryenTypeCreate(parameter_types.elems, parameter_types.size);

  BinaryenAddFunction(codegen.module, name, params, results, variable_types.elems,
                      variable_types.size, body);
  BinaryenAddFunctionExport(codegen.module, name, name);

  return NULL;
}

static BinaryenExpressionRef generate_statement(Stmt* statement)
{
  switch (statement->type)
  {
  case STMT_EXPR:
    return generate_expression_statement(statement);
  case STMT_RETURN:
    return generate_return_statement(statement);
  case STMT_VARIABLE_DECL:
    return generate_variable_declaration(statement);
  case STMT_FUNCTION_DECL:
    return generate_function_declaration(statement);

  default:
    ERROR("Unhandled statement");
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
  codegen.statements = statements;
}

void codegen_generate(void)
{
  BinaryenFunctionRef start =
    BinaryenAddFunction(codegen.module, "~start", BinaryenTypeNone(), BinaryenTypeNone(), NULL, 0,
                        generate_statements(&codegen.statements));

  BinaryenSetStart(codegen.module, start);
  BinaryenModuleValidate(codegen.module);
  BinaryenModuleOptimize(codegen.module);
  BinaryenModulePrint(codegen.module);
  BinaryenModuleDispose(codegen.module);
}
