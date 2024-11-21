#include "parser.h"
#include "array.h"
#include "expression.h"
#include "main.h"
#include "scanner.h"

Expr* expression();

struct
{
  int current;
  ArrayToken tokens;
} parser;

static void error(Token token, const char* message)
{
  report_error(token.start_line, token.start_column, token.end_line, token.end_column, message);
}

static Token peek()
{
  return array_at(&parser.tokens, parser.current);
}

static Token previous()
{
  return array_at(&parser.tokens, parser.current - 1);
}

static bool check(TokenType type)
{
  return peek().type == type;
}

static bool eof()
{
  return peek().type == TOKEN_EOF;
}

static Token advance()
{
  if (!eof())
    parser.current++;

  return previous();
}

static Token consume(TokenType type, const char* message)
{
  if (check(type))
    return advance();

  error(peek(), message);
  return peek();
}

bool match(TokenType type)
{
  if (check(type))
  {
    advance();
    return true;
  }

  return false;
}

static Expr* primary()
{
  Expr* expr = EXPR();
  Token token = peek();

  switch (token.type)
  {
  case TOKEN_TRUE:
    expr->type = EXPR_LITERAL;
    expr->literal.type = LITERAL_BOOL;
    expr->literal.boolean = true;

    advance();
    return expr;
  case TOKEN_FALSE:
    expr->type = EXPR_LITERAL;
    expr->literal.type = LITERAL_BOOL;
    expr->literal.boolean = false;

    advance();
    return expr;
  case TOKEN_NULL:
    expr->type = EXPR_LITERAL;
    expr->literal.type = LITERAL_NULL;

    advance();
    return expr;
  case TOKEN_INTEGER:
    expr->type = EXPR_LITERAL;
    expr->literal.type = LITERAL_INTEGER;
    expr->literal.integer = strtol(token.start, NULL, 10);

    advance();
    return expr;
  case TOKEN_FLOAT:
    expr->type = EXPR_LITERAL;
    expr->literal.type = LITERAL_FLOAT;
    expr->literal.floating = strtod(token.start, NULL);

    advance();
    return expr;
  case TOKEN_STRING:
    expr->type = EXPR_LITERAL;
    expr->literal.type = LITERAL_STRING;
    expr->literal.string.value = token.start;
    expr->literal.string.length = token.length;

    advance();
    return expr;
  case TOKEN_LEFT_PAREN:
    advance();

    expr->type = EXPR_GROUP;
    expr->group.expr = expression();

    consume(TOKEN_RIGHT_PAREN, "Expected ')' after expression.");
    return expr;
  case TOKEN_IDENTIFIER:
    expr->type = EXPR_VAR;
    expr->var.name = token;

    advance();
    return expr;
  default:
    advance();
    break;
  }

  error(token, "Expected an expression.");
  return NULL;
}

static Expr* prefix_unary()
{
  if (match(TOKEN_BANG) || match(TOKEN_MINUS))
  {
    Token op = previous();
    Expr* expr = prefix_unary();

    Expr* unary;
    UNARY_EXPR(unary, op, expr);

    return unary;
  }

  return primary();
}

static Expr* factor()
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

static Expr* term()
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

static Expr* comparison()
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

static Expr* equality()
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

Expr* expression()
{
  return equality();
}

void parser_init(ArrayToken tokens)
{
  parser.current = 0;
  parser.tokens = tokens;
}

Expr* parser_parse()
{
  return expression();
}
