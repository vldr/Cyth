#include "scanner.h"
#include "array.h"

#include <ctype.h>
#include <stdbool.h>
#include <stdio.h>

struct
{
  int start_line;
  int start_column;
  int current_line;
  int current_column;

  bool multi_line;

  const char* start;
  const char* current;

  ArrayToken tokens;
  ArrayInt indentation;
} scanner;

static void add_custom_token(TokenType type, const char* start, int length)
{
  Token token;
  token.type = type;
  token.start_line = scanner.start_line;
  token.start_column = scanner.start_column;
  token.end_line = scanner.current_line;
  token.end_column = scanner.current_column;
  token.length = length;
  token.start = start;

  array_add(&scanner.tokens, token);
}

static void add_token(TokenType type)
{
  add_custom_token(type, scanner.start, scanner.current - scanner.start);
}

static bool eof()
{
  return *scanner.current == '\0';
}

static void newline()
{
  scanner.current_column = 1;
  scanner.current_line++;
}

static char advance()
{
  scanner.current_column++;
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
    if (peek() == '\n')
    {
      advance();
      newline();
      continue;
    }

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
    scanner.multi_line = true;

    add_token(TOKEN_LEFT_PAREN);
    break;
  case ')':
    scanner.multi_line = false;

    add_token(TOKEN_RIGHT_PAREN);
    break;
  case '{':
    scanner.multi_line = true;

    add_token(TOKEN_LEFT_BRACE);
    break;
  case '}':
    scanner.multi_line = false;

    add_token(TOKEN_RIGHT_BRACE);
    break;
  case '[':
    scanner.multi_line = true;

    add_token(TOKEN_LEFT_BRACKET);
    break;
  case ']':
    scanner.multi_line = false;

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
  case '\t':
  case '\r':
    break;

  case '\n':
    if (!scanner.multi_line)
      add_custom_token(TOKEN_NEWLINE, "\\n", sizeof("\\n") - 1);

    newline();
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

static void scan_indentation()
{
  if (scanner.multi_line || scanner.current_column != 1)
    return;

  int indentation = 0;

  while (peek() == ' ' || peek() == '\t' || peek() == '\r' || peek() == '\n')
  {
    switch (peek())
    {
    case '\n':
      indentation = 0;

      advance();
      newline();
      break;
    case ' ':
    case '\t':
      indentation++;
    default:
      advance();
      break;
    }
  }

  if (eof())
  {
    return;
  }

  if (indentation > array_last(&scanner.indentation))
  {
    array_add(&scanner.indentation, indentation);
    add_custom_token(TOKEN_INDENT, scanner.start + 1, scanner.current - scanner.start - 1);
  }
  else if (indentation < array_last(&scanner.indentation))
  {
    while (array_last(&scanner.indentation) > indentation)
    {
      add_custom_token(TOKEN_DEDENT, NULL, 0);
      array_del_last(&scanner.indentation);
    }
  }
}

void scanner_init(const char* source)
{
  scanner.start_line = 1;
  scanner.start_column = 1;
  scanner.current_line = scanner.start_line;
  scanner.current_column = scanner.start_column;
  scanner.multi_line = false;
  scanner.start = source;
  scanner.current = source;

  array_init(&scanner.tokens);
  array_init(&scanner.indentation);
  array_add(&scanner.indentation, 0);
}

ArrayToken scanner_scan()
{
  for (;;)
  {
    scan_indentation();

    if (eof())
      break;

    scanner.start = scanner.current;
    scanner.start_line = scanner.current_line;
    scanner.start_column = scanner.current_column;

    scan_token();
  }

  if (array_size(&scanner.tokens) && array_last(&scanner.tokens).type != TOKEN_NEWLINE)
  {
    add_custom_token(TOKEN_NEWLINE, "\\n", sizeof("\\n") - 1);
  }

  while (array_last(&scanner.indentation))
  {
    add_custom_token(TOKEN_DEDENT, NULL, 0);
    array_del_last(&scanner.indentation);
  }

  add_custom_token(TOKEN_EOF, NULL, 0);

  return scanner.tokens;
}

void scanner_print()
{
  Token token;
  array_foreach(&scanner.tokens, token)
  {
    static const char* types[] = {"INDENT",        "DEDENT",       "NEWLINE",
                                  "LEFT_PAREN",    "RIGHT_PAREN",  "LEFT_BRACE",
                                  "RIGHT_BRACE",   "LEFT_BRACKET", "RIGHT_BRACKET",
                                  "SEMICOLON",     "COLON",        "COMMA",
                                  "DOT",           "MINUS",        "MINUS_MINUS",
                                  "MINUS_EQUAL",   "PLUS",         "PLUS_PLUS",
                                  "PLUS_EQUAL",    "SLASH",        "SLASH_EQUAL",
                                  "STAR",          "STAR_EQUAL",   "PERCENT",
                                  "PERCENT_EQUAL", "BANG",         "BANG_EQUAL",
                                  "EQUAL",         "EQUAL_EQUAL",  "GREATER",
                                  "GREATER_EQUAL", "LESS",         "LESS_EQUAL",
                                  "IDENTIFIER",    "STRING",       "NUMBER",
                                  "AND",           "CLASS",        "ELSE",
                                  "FALSE",         "FOR",          "IF",
                                  "NULL",          "OR",           "NOT",
                                  "RETURN",        "SUPER",        "THIS",
                                  "TRUE",          "WHILE",        "EOF"};

    printf("%d,%d-%d,%d \t%s    \t'%.*s'  \n", token.start_line, token.start_column, token.end_line, token.end_column,
           types[token.type], token.length, token.start);
  }
}
