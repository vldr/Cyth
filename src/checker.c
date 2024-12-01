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

// static void error_operation_not_defined(Token token)
// {
//   error(token, memory_sprintf(&memory, "Operator %c not defined for types.", *token.start));
// }

// static void implicit_cast()
// {
// }

static DataType check_cast_expression(Expr* expression)
{
  return expression->data_type;
}

static DataType check_literal_expression(Expr* expression)
{
  return expression->literal.type;
}

static DataType check_group_expression(Expr* expression)
{
  return check_expression(expression->group.expr);
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
  else if (op.type == TOKEN_BANG)
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
  DataType left = check_expression(expression->binary.left);
  DataType right = check_expression(expression->binary.right);
  Token op = expression->binary.op;

  if (left != right)
  {
    error_type_mismatch(expression->binary.op);
  }

  // switch (op.type)
  // {
  // default:
  //   error_operation_not_defined(op);
  //   break;
  // }

  expression->data_type = left;
  return left;
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
