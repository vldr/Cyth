#include "parser.h"
#include "array.h"
#include "expression.h"
#include "lexer.h"
#include "main.h"
#include "statement.h"
#include <stdio.h>

static Expr* expression(void);
static Stmt* declaration(void);
static Stmt* statement(void);

static struct
{
  bool error;
  int current;
  ArrayToken tokens;
} parser;

static void error(Token token, const char* message)
{
  if (!parser.error)
  {
    report_error(token.start_line, token.start_column, token.end_line, token.end_column, message);
  }

  parser.error = true;
}

static Token peek(void)
{
  return array_at(&parser.tokens, parser.current);
}

static Token previous(void)
{
  return array_at(&parser.tokens, parser.current - 1);
}

static bool check(TokenType type)
{
  return peek().type == type;
}

static bool eof(void)
{
  return peek().type == TOKEN_EOF;
}

static Token advance(void)
{
  if (!eof())
    parser.current++;

  return previous();
}

static bool match(TokenType type)
{
  if (check(type))
  {
    advance();
    return true;
  }

  return false;
}
static Token consume(TokenType type, const char* message)
{
  if (match(type))
    return previous();

  error(peek(), message);

  return peek();
}

static bool match_type(void)
{
  return match(TOKEN_IDENTIFIER) || match(TOKEN_IDENTIFIER_VOID) || match(TOKEN_IDENTIFIER_INT) ||
         match(TOKEN_IDENTIFIER_FLOAT) || match(TOKEN_IDENTIFIER_STRING) ||
         match(TOKEN_IDENTIFIER_BOOL);
}

static Token consume_type(const char* message)
{
  if (match_type())
    return previous();

  error(peek(), message);

  return peek();
}

static void synchronize(void)
{
  advance();

  while (!eof())
  {
    if (previous().type == TOKEN_NEWLINE)
      return;

    switch (peek().type)
    {
    case TOKEN_CLASS:
    case TOKEN_FOR:
    case TOKEN_IF:
    case TOKEN_WHILE:
    case TOKEN_RETURN:
      return;
    default:
      advance();
      break;
    }
  }
}

static Expr* primary(void)
{
  Expr* expr = EXPR();
  Token token = peek();

  switch (token.type)
  {
  case TOKEN_TRUE:
    advance();

    expr->type = EXPR_LITERAL;
    expr->literal.data_type = TYPE_BOOL;
    expr->literal.boolean = true;

    break;
  case TOKEN_FALSE:
    advance();

    expr->type = EXPR_LITERAL;
    expr->literal.data_type = TYPE_BOOL;
    expr->literal.boolean = false;

    break;
  case TOKEN_NULL:
    advance();

    expr->type = EXPR_LITERAL;
    expr->literal.data_type = TYPE_NULL;

    break;
  case TOKEN_INTEGER:
    advance();

    expr->type = EXPR_LITERAL;
    expr->literal.data_type = TYPE_INTEGER;
    expr->literal.integer = strtol(token.lexeme, NULL, 10);

    break;
  case TOKEN_FLOAT:
    advance();

    expr->type = EXPR_LITERAL;
    expr->literal.data_type = TYPE_FLOAT;
    expr->literal.floating = (float)strtod(token.lexeme, NULL);

    break;
  case TOKEN_STRING:
    advance();

    expr->type = EXPR_LITERAL;
    expr->literal.data_type = TYPE_STRING;
    expr->literal.string = token.lexeme;

    break;
  case TOKEN_LEFT_PAREN:
    advance();

    expr->type = EXPR_GROUP;
    expr->group.expr = expression();

    consume(TOKEN_RIGHT_PAREN, "Expected ')' after expression.");

    break;
  case TOKEN_IDENTIFIER:
    advance();

    expr->type = EXPR_VAR;
    expr->var.name = token;

    break;
  default:
    error(token, "Expected an expression.");
    break;
  }

  return expr;
}

static Expr* prefix_unary(void)
{
  if (match(TOKEN_BANG) || match(TOKEN_NOT) || match(TOKEN_MINUS))
  {
    Token op = previous();
    Expr* expr = prefix_unary();

    Expr* unary;
    UNARY_EXPR(unary, op, expr);

    return unary;
  }

  return primary();
}

static Expr* factor(void)
{
  Expr* expr = prefix_unary();

  while (match(TOKEN_SLASH) || match(TOKEN_STAR) || match(TOKEN_PERCENT))
  {
    Token op = previous();
    Expr* right = prefix_unary();

    BINARY_EXPR(expr, op, expr, right);
  }

  return expr;
}

static Expr* term(void)
{
  Expr* expr = factor();

  while (match(TOKEN_MINUS) || match(TOKEN_PLUS))
  {
    Token op = previous();
    Expr* right = factor();

    BINARY_EXPR(expr, op, expr, right);
  }

  return expr;
}

static Expr* comparison(void)
{
  Expr* expr = term();

  while (match(TOKEN_GREATER) || match(TOKEN_GREATER_EQUAL) || match(TOKEN_LESS) ||
         match(TOKEN_LESS_EQUAL))
  {
    Token op = previous();
    Expr* right = term();

    BINARY_EXPR(expr, op, expr, right);
  }

  return expr;
}

static Expr* equality(void)
{
  Expr* expr = comparison();

  while (match(TOKEN_BANG_EQUAL) || match(TOKEN_EQUAL_EQUAL))
  {
    Token op = previous();
    Expr* right = comparison();

    BINARY_EXPR(expr, op, expr, right);
  }

  return expr;
}

static Expr* logic_and(void)
{
  Expr* expr = equality();

  while (match(TOKEN_AND))
  {
    Token op = previous();
    Expr* right = equality();

    BINARY_EXPR(expr, op, expr, right);
  }

  return expr;
}

static Expr* logic_or(void)
{
  Expr* expr = logic_and();

  while (match(TOKEN_OR))
  {
    Token op = previous();
    Expr* right = logic_and();

    BINARY_EXPR(expr, op, expr, right);
  }

  return expr;
}

static Expr* assignment(void)
{
  Expr* expr = logic_or();

  if (match(TOKEN_EQUAL))
  {
    Token op = previous();

    if (expr->type == EXPR_VAR)
    {
      expr->type = EXPR_ASSIGN;
      expr->assign.name = expr->var.name;
      expr->assign.value = assignment();

      return expr;
    }

    error(op, "Invalid assignment target.");
  }

  return expr;
}

static Expr* expression(void)
{
  return assignment();
}

static Stmt* return_statement(void)
{
  Token keyword = advance();

  Expr* expr = expression();
  consume(TOKEN_NEWLINE, "Expected a newline after an expression.");

  Stmt* stmt = STMT();
  stmt->type = STMT_RETURN;
  stmt->ret.expr = expr;
  stmt->ret.keyword = keyword;

  return stmt;
}

static Stmt* expression_statement(void)
{
  Expr* expr = expression();
  consume(TOKEN_NEWLINE, "Expected a newline after an expression.");

  Stmt* stmt = STMT();
  stmt->type = STMT_EXPR;
  stmt->expr.expr = expr;

  return stmt;
}

static Stmt* statement(void)
{
  Token token = peek();

  switch (token.type)
  {
  case TOKEN_RETURN:
    return return_statement();

  default:
    return expression_statement();
  }
}

static ArrayStmt statements(void)
{
  ArrayStmt statements;
  array_init(&statements);

  consume(TOKEN_INDENT, "Expected an indent.");

  while (!eof() && !check(TOKEN_DEDENT))
  {
    array_add(&statements, declaration());
  }

  consume(TOKEN_DEDENT, "Expected a dedent.");

  return statements;
}

static Stmt* function_declaration(Token type, Token name)
{
  Stmt* stmt = STMT();
  stmt->type = STMT_FUNCTION_DECL;
  stmt->func.type = type;
  stmt->func.name = name;

  array_init(&stmt->func.parameters);
  array_init(&stmt->func.body);
  array_init(&stmt->func.variables);

  consume(TOKEN_LEFT_PAREN, "Expected '(' after function name.");

  if (!check(TOKEN_RIGHT_PAREN))
  {
    do
    {
      Token type = consume_type("Expected a type after '('");
      Token name = consume(TOKEN_IDENTIFIER, "Expected a parameter name after type.");

      Stmt* parameter = STMT();
      parameter->type = STMT_VARIABLE_DECL;
      parameter->var.type = type;
      parameter->var.name = name;
      parameter->var.initializer = NULL;

      array_add(&stmt->func.parameters, parameter);
    } while (match(TOKEN_COMMA));
  }

  consume(TOKEN_RIGHT_PAREN, "Expected ')' after parameters.");
  consume(TOKEN_NEWLINE, "Expected newline after ')'.");

  stmt->func.body = statements();
  return stmt;
}

static Stmt* variable_declaration(Token type, Token name)
{
  Stmt* stmt = STMT();
  stmt->type = STMT_VARIABLE_DECL;
  stmt->var.type = type;
  stmt->var.name = name;

  if (match(TOKEN_EQUAL))
    stmt->var.initializer = expression();
  else
    stmt->var.initializer = NULL;

  consume(TOKEN_NEWLINE, "Expected newline after variable declaration.");

  return stmt;
}

static Stmt* declaration(void)
{
  if (match_type())
  {
    Token type = previous();
    Token name = consume(TOKEN_IDENTIFIER, "Expected a name after type.");

    if (check(TOKEN_LEFT_PAREN))
      return function_declaration(type, name);
    else
      return variable_declaration(type, name);
  }
  else
  {
    return statement();
  }
}

void parser_init(ArrayToken tokens)
{
  parser.error = false;
  parser.current = 0;
  parser.tokens = tokens;
}

ArrayStmt parser_parse(void)
{
  ArrayStmt statements;
  array_init(&statements);

  while (!eof())
  {
    array_add(&statements, declaration());

    if (parser.error)
    {
      synchronize();

      parser.error = false;
    }
  }

  return statements;
}
