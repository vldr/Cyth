#include "scanner.h"

#include <ctype.h>
#include <stdbool.h>
#include <stdio.h>

struct
{
  ArrayToken tokens;
  const char* start;
  const char* current;
  int line;
} scanner;

static void add_token(TokenType type)
{
  Token token;
  token.type = type;
  token.start = scanner.start;
  token.length = (int)(scanner.current - scanner.start);

  array_add(&scanner.tokens, token);
}

static bool eof()
{
  return *scanner.current == '\0';
}

static char advance()
{
  return *scanner.current++;
}

static char peek()
{
  return *scanner.current;
}

static char peek_next()
{
  if (eof())
    return '\0';

  return *(scanner.current + 1);
}

static bool match(char c)
{
  if (peek() == c)
  {
    advance();
    return true;
  }

  return false;
}

static void string()
{
  while (peek() != '"')
  {
    if (peek() == '\0')
    {
      printf("Error: unterminated string\n");
      return;
    }

    advance();
  }

  advance();
  add_token(TOKEN_STRING);
}

static void number()
{
  while (isdigit(peek()))
    advance();

  if (peek() == '.' && isdigit(peek_next()))
  {
    advance();

    while (isdigit(peek()))
      advance();
  }

  add_token(TOKEN_NUMBER);
}

static void literal()
{
  while (isalnum(peek()) || peek() == '_')
    advance();

  TokenType type = TOKEN_IDENTIFIER;

  add_token(type);
}

static void scan_token()
{
  char c = advance();

  switch (c)
  {
  case '(':
    add_token(TOKEN_LEFT_PAREN);
    break;
  case ')':
    add_token(TOKEN_RIGHT_PAREN);
    break;
  case '{':
    add_token(TOKEN_LEFT_BRACE);
    break;
  case '}':
    add_token(TOKEN_RIGHT_BRACE);
    break;
  case '[':
    add_token(TOKEN_LEFT_BRACKET);
    break;
  case ']':
    add_token(TOKEN_RIGHT_BRACKET);
    break;
  case ',':
    add_token(TOKEN_COMMA);
    break;
  case '.':
    add_token(TOKEN_DOT);
    break;
  case ':':
    add_token(TOKEN_COLON);
    break;
  case ';':
    add_token(TOKEN_SEMICOLON);
    break;

  case '+':
    if (match('+'))
      add_token(TOKEN_PLUS_PLUS);
    else if (match('='))
      add_token(TOKEN_PLUS_EQUAL);
    else
      add_token(TOKEN_PLUS);

    break;
  case '-':
    if (match('-'))
      add_token(TOKEN_MINUS_MINUS);
    else if (match('='))
      add_token(TOKEN_MINUS_EQUAL);
    else
      add_token(TOKEN_MINUS);

    break;
  case '/':
    if (match('/'))
      while (peek() != '\n' && peek() != '\0')
        advance();
    else if (match('='))
      add_token(TOKEN_SLASH_EQUAL);
    else
      add_token(TOKEN_SLASH);

    break;
  case '%':
    add_token(match('=') ? TOKEN_PERCENT_EQUAL : TOKEN_PERCENT);
    break;
  case '*':
    add_token(match('=') ? TOKEN_STAR_EQUAL : TOKEN_STAR);
    break;
  case '!':
    add_token(match('=') ? TOKEN_BANG_EQUAL : TOKEN_BANG);
    break;
  case '=':
    add_token(match('=') ? TOKEN_EQUAL_EQUAL : TOKEN_EQUAL);
    break;
  case '<':
    add_token(match('=') ? TOKEN_LESS_EQUAL : TOKEN_LESS);
    break;
  case '>':
    add_token(match('=') ? TOKEN_GREATER_EQUAL : TOKEN_GREATER);
    break;

  case '"':
    string();
    break;

  case ' ':
  case '\r':
  case '\n':
  case '\t':
    break;

  default:
    if (isdigit(c))
    {
      number();
      break;
    }
    else if (isalpha(c) || c == '_' || c == '#')
    {
      literal();
      break;
    }

    printf("Unexpected character: %c", c);
    break;
  }
}

void scanner_init(const char* source)
{
  array_init(&scanner.tokens);

  scanner.start = source;
  scanner.current = source;
}

ArrayToken scanner_scan()
{
  while (!eof())
  {
    scanner.start = scanner.current;
    scan_token();
  }

  add_token(TOKEN_EOF);

  return scanner.tokens;
}
