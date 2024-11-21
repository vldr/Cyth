#include "parser.h"
#include "array.h"
#include "expression.h"
#include "main.h"
#include "scanner.h"

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

static Token peek_next()
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

  // if (match(TOKEN_TRUE))
  //   return new LiteralExpr(true);
  // else if (match(TOKEN_FALSE))
  //   return new LiteralExpr(false);
  // else if (match(TOKEN_NIL))
  //   return new LiteralExpr(Nil());
  // else if (match(TOKEN_THIS))
  //   return new ThisExpr(previous());
  // else if (match(TOKEN_SUPER))
  // {
  //   Token keyword = previous();
  //   consume(TOKEN_DOT, "Expected a '.' after 'super' keyword.");
  //   Token method = consume(TOKEN_IDENTIFIER, "Expected a superclass name.");

  //   return new SuperExpr(keyword, method);
  // }
  // else if (match(TOKEN_NUMBER, TOKEN_STRING))
  // {
  //   return new LiteralExpr(previous().literal);
  // }
  // else if (match(TOKEN_LEFT_PAREN))
  // {
  //   auto expr = expression();
  //   consume(TOKEN_RIGHT_PAREN, "Expected ')' after expression.");

  //   return new GroupExpr(expr);
  // }
  // else if (match(TOKEN_IDENTIFIER))
  // {
  //   return new VarExpr(previous());
  // }

  // throw error(peek(), "Expected an expression.");

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
