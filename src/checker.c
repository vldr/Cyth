#include "checker.h"
#include "array.h"
#include "expression.h"
#include "main.h"
#include "scanner.h"

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

static Type check_expression(Expr* expression)
{
  switch (expression->type)
  {
  case EXPR_LITERAL:
    return expression->literal.type;
  case EXPR_GROUP:
    return check_expression(expression->group.expr);
  case EXPR_BINARY: {
    Type left = check_expression(expression->binary.left);
    Type right = check_expression(expression->binary.right);

    if (left != right)
    {
      error(expression->binary.op, memory_sprintf(&memory, "Type mismatch"));
    }

    return left;
  }
  case EXPR_UNARY: {
    Token op = expression->unary.op;
    Type type = check_expression(expression->unary.expr);

    if (op.type == TOKEN_MINUS)
    {
      if (type != TYPE_INTEGER || type != TYPE_FLOAT)
      {
        error(expression->binary.op, memory_sprintf(&memory, "Type mismatch"));
      }
    }

    break;
  }
  case EXPR_VAR:
  case EXPR_ASSIGN:
  case EXPR_CALL:
    break;
  }

  return TYPE_VOID;
}

static void check_statement(Stmt* statement)
{
  checker.error = false;

  switch (statement->type)
  {
  case STMT_EXPR:
    check_expression(statement->expr.expr);
    break;
  }
}

void checker_init(ArrayStmt statements)
{
  checker.error = false;
  checker.statements = statements;
}

void checker_validate()
{
  Stmt* statement;
  array_foreach(&checker.statements, statement)
  {
    check_statement(statement);
  }
}
