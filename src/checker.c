#include "checker.h"
#include "array.h"
#include "expression.h"
#include "main.h"
#include "scanner.h"
#include <assert.h>

static DataType check_expression(Expr* expression);
static void check_statement(Stmt* statement);

static struct
{
  bool error;
  ArrayStmt statements;
} checker;

static void error(Token token, const char* message)
{
  if (checker.error)
    return;

  report_error(token.start_line, token.start_column, token.end_line, token.end_column, message);
  checker.error = true;
}

static void error_type_mismatch(Token token)
{
  error(token, "Type mismatch.");
}

static void error_operation_not_defined(Token token, const char* type)
{
  error(token, memory_sprintf(&memory, "Operator '%.*s' only defined for %s.", token.length,
                              token.start, type));
}

static bool upcast(Expr* expression, DataType* left, DataType* right, DataType from, DataType to)
{
  Expr** target;
  DataType* target_type;

  if (*left == from && *right == to)
  {
    target_type = left;
    target = &expression->binary.left;
  }
  else if (*left == to && *right == from)
  {
    target_type = right;
    target = &expression->binary.right;
  }
  else
  {
    return false;
  }

  Expr* cast_expression = EXPR();
  cast_expression->type = EXPR_CAST;
  cast_expression->data_type = to;
  cast_expression->cast.expr = *target;

  *target = cast_expression;
  *target_type = to;

  return true;
}

static DataType check_cast_expression(Expr* expression)
{
  return expression->data_type;
}

static DataType check_literal_expression(Expr* expression)
{
  expression->data_type = expression->literal.data_type;
  return expression->data_type;
}

static DataType check_group_expression(Expr* expression)
{
  expression->data_type = check_expression(expression->group.expr);
  return expression->data_type;
}

static DataType check_unary_expression(Expr* expression)
{
  Token op = expression->unary.op;
  DataType type = check_expression(expression->unary.expr);

  if (op.type == TOKEN_MINUS)
  {
    if (type != TYPE_INTEGER && type != TYPE_FLOAT)
    {
      error_type_mismatch(op);
    }
  }
  else if (op.type == TOKEN_BANG || op.type == TOKEN_NOT)
  {
    if (type != TYPE_BOOL)
    {
      error_type_mismatch(op);
    }
  }

  expression->data_type = type;
  return type;
}

static DataType check_binary_expression(Expr* expression)
{
  Token op = expression->binary.op;
  DataType left = check_expression(expression->binary.left);
  DataType right = check_expression(expression->binary.right);

  if (left != right)
  {
    if (!upcast(expression, &left, &right, TYPE_INTEGER, TYPE_FLOAT))
    {
      error_type_mismatch(expression->binary.op);
    }
  }

  expression->binary.data_type = left;
  expression->data_type = left;

  switch (op.type)
  {
  case TOKEN_AND:
  case TOKEN_OR:
    if (left != TYPE_BOOL)
      error_operation_not_defined(op, "'bool'");

    break;
  case TOKEN_EQUAL_EQUAL:
  case TOKEN_BANG_EQUAL:
  case TOKEN_GREATER:
  case TOKEN_GREATER_EQUAL:
  case TOKEN_LESS:
  case TOKEN_LESS_EQUAL:
    if (left != TYPE_INTEGER && left != TYPE_FLOAT && left != TYPE_BOOL)
      error_operation_not_defined(op, "'int', 'float', 'bool");

    expression->data_type = TYPE_BOOL;
    break;
  case TOKEN_PLUS:
  case TOKEN_MINUS:
  case TOKEN_STAR:
  case TOKEN_SLASH:
    if (left != TYPE_INTEGER && left != TYPE_FLOAT)
      error_operation_not_defined(op, "'int' and 'float'");

    break;

  case TOKEN_PERCENT:
    if (left != TYPE_INTEGER)
      error_operation_not_defined(op, "'int'");

    break;

  default:
    error_operation_not_defined(op, "'unknown'");
  }

  return expression->data_type;
}

static DataType check_expression(Expr* expression)
{
  switch (expression->type)
  {
  case EXPR_CAST:
    return check_cast_expression(expression);
  case EXPR_LITERAL:
    return check_literal_expression(expression);
  case EXPR_GROUP:
    return check_group_expression(expression);
  case EXPR_BINARY:
    return check_binary_expression(expression);
  case EXPR_UNARY:
    return check_unary_expression(expression);

  default:
    assert(!"Unhanled expression");
  }
}

static void check_statement(Stmt* statement)
{
  checker.error = false;

  switch (statement->type)
  {
  case STMT_EXPR:
    check_expression(statement->expr.expr);
    break;

  default:
    assert(!"Unhandled statement");
  }
}

void checker_init(ArrayStmt statements)
{
  checker.error = false;
  checker.statements = statements;
}

void checker_validate(void)
{
  Stmt* statement;
  array_foreach(&checker.statements, statement)
  {
    check_statement(statement);
  }
}
