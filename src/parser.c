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

    return expr;
  case TOKEN_FALSE:
    advance();

    expr->type = EXPR_LITERAL;
    expr->literal.data_type = TYPE_BOOL;
    expr->literal.boolean = false;

    return expr;
  case TOKEN_NULL:
    advance();

    expr->type = EXPR_LITERAL;
    expr->literal.data_type = TYPE_NULL;

    return expr;
  case TOKEN_INTEGER:
    advance();

    expr->type = EXPR_LITERAL;
    expr->literal.data_type = TYPE_INTEGER;
    expr->literal.integer = strtol(token.start, NULL, 10);

    return expr;
  case TOKEN_FLOAT:
    advance();

    expr->type = EXPR_LITERAL;
    expr->literal.data_type = TYPE_FLOAT;
    expr->literal.floating = (float)strtod(token.start, NULL);

    return expr;
  case TOKEN_STRING:
    advance();

    expr->type = EXPR_LITERAL;
    expr->literal.data_type = TYPE_STRING;
    expr->literal.string.value = token.start;
    expr->literal.string.length = token.length;

    return expr;
  case TOKEN_LEFT_PAREN:
    advance();

    expr->type = EXPR_GROUP;
    expr->group.expr = expression();

    consume(TOKEN_RIGHT_PAREN, "Expected ')' after expression.");

    return expr;
  case TOKEN_IDENTIFIER:
    advance();

    expr->type = EXPR_VAR;
    expr->var.name = token;

    return expr;
  default:
    break;
  }

  error(token, "Expected an expression.");
  return NULL;
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

static Expr* expression(void)
{
  return logic_or();
}

static Stmt* block_statement(void)
{
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
  default:
    return expression_statement();
  }
}

static Stmt* function_declaration(void)
{
  Token type = previous();
  Token name = consume(TOKEN_IDENTIFIER, "Expected a function name after type name.");

  Stmt* stmt = STMT();
  stmt->type = STMT_FUNCTION_DECL;
  stmt->func.type = type;
  stmt->func.name = name;

  array_init(&stmt->func.param_types);
  array_init(&stmt->func.params);
  array_init(&stmt->func.body);

  consume(TOKEN_LEFT_PAREN, "Expected '(' after function name.");

  if (!check(TOKEN_RIGHT_PAREN))
  {
    do
    {
      Token type = consume_type("Expected a type after '('");
      Token name = consume(TOKEN_IDENTIFIER, "Expected a parameter name after type.");

      array_add(&stmt->func.param_types, type);
      array_add(&stmt->func.params, name);
    } while (match(TOKEN_COMMA));
  }

  consume(TOKEN_RIGHT_PAREN, "Expected ')' after parameters.");
  consume(TOKEN_NEWLINE, "Expected newline after ')'.");

  stmt->func.body = block_statement();
  return
}

static Stmt* declaration(void)
{
  if (match_type())
  {
    return function_declaration();
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
