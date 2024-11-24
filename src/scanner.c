#include "scanner.h"
#include "array.h"
#include "main.h"

#include <ctype.h>
#include <stdbool.h>
#include <stdio.h>

#define KEYWORD_GROUP(c)                                                                           \
  break;                                                                                           \
  case c:

#define KEYWORD(keyword, token)                                                                    \
  {                                                                                                \
    size_t input_length = scanner.current - scanner.start;                                         \
    size_t keyword_length = sizeof(keyword) - 1;                                                   \
    if (input_length == keyword_length && memcmp(keyword, scanner.start, input_length) == 0)       \
    {                                                                                              \
      type = token;                                                                                \
      break;                                                                                       \
    }                                                                                              \
  }

static struct
{
  int start_line;
  int start_column;
  int current_line;
  int current_column;

  int multi_line;

  const char* start;
  const char* current;

  enum
  {
    INDENTATION_NONE,
    INDENTATION_TAB,
    INDENTATION_SPACE
  } indentation_type;
  ArrayInt indentation;
  ArrayToken tokens;
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
  add_custom_token(type, scanner.start, (int)(scanner.current - scanner.start));
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
      report_error(scanner.start_line, scanner.start_column, scanner.current_line,
                   scanner.current_column, "unterminated string");
      return;
    }

    advance();
  }

  add_custom_token(TOKEN_STRING, scanner.start + 1, (int)(scanner.current - scanner.start - 1));
  advance();
}

static void number()
{
  TokenType type = TOKEN_INTEGER;

  while (isdigit(peek()))
    advance();

  if (peek() == '.' && isdigit(peek_next()))
  {
    advance();

    while (isdigit(peek()))
      advance();

    type = TOKEN_FLOAT;
  }

  add_token(type);
}

static void literal()
{
  while (isalnum(peek()) || peek() == '_')
    advance();

  TokenType type = TOKEN_IDENTIFIER;

  switch (*scanner.start)
  {
  default:
    KEYWORD_GROUP('a')
    KEYWORD("and", TOKEN_AND)
    KEYWORD_GROUP('c')
    KEYWORD("class", TOKEN_CLASS)
    KEYWORD_GROUP('e')
    KEYWORD("else", TOKEN_ELSE)
    KEYWORD_GROUP('f')
    KEYWORD("false", TOKEN_FALSE)
    KEYWORD("for", TOKEN_FOR)
    KEYWORD_GROUP('i')
    KEYWORD("if", TOKEN_IF)
    KEYWORD_GROUP('n')
    KEYWORD("null", TOKEN_NULL)
    KEYWORD("not", TOKEN_NOT)
    KEYWORD_GROUP('o')
    KEYWORD("or", TOKEN_OR)
    KEYWORD_GROUP('r')
    KEYWORD("return", TOKEN_RETURN)
    KEYWORD_GROUP('s')
    KEYWORD("super", TOKEN_SUPER)
    KEYWORD_GROUP('t')
    KEYWORD("this", TOKEN_THIS)
    KEYWORD("true", TOKEN_TRUE)
    KEYWORD_GROUP('w')
    KEYWORD("while", TOKEN_WHILE)
  }

  add_token(type);
}

static void comment()
{
  while (peek() != '\n' && peek() != '\0')
    advance();
}

static void scan_token()
{
  char c = advance();

  switch (c)
  {
  case '(':
    scanner.multi_line++;

    add_token(TOKEN_LEFT_PAREN);
    break;
  case ')':
    scanner.multi_line--;

    add_token(TOKEN_RIGHT_PAREN);
    break;
  case '{':
    scanner.multi_line++;

    add_token(TOKEN_LEFT_BRACE);
    break;
  case '}':
    scanner.multi_line--;

    add_token(TOKEN_RIGHT_BRACE);
    break;
  case '[':
    scanner.multi_line++;

    add_token(TOKEN_LEFT_BRACKET);
    break;
  case ']':
    scanner.multi_line--;

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
    if (match('='))
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

  case '#':
    comment();
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

    report_error(scanner.start_line, scanner.start_column, scanner.current_line,
                 scanner.current_column, "unexpected character");
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
      indentation += 1;
      scanner.indentation_type |= INDENTATION_SPACE;

      advance();
      break;
    case '\t':
      indentation += 4;
      scanner.indentation_type |= INDENTATION_TAB;

      advance();
      break;
    default:
      advance();
      break;
    }
  }

  if (eof() || peek() == '#')
  {
    return;
  }

  if ((scanner.indentation_type & INDENTATION_SPACE) &&
      (scanner.indentation_type & INDENTATION_TAB))
  {
    report_error(scanner.start_line, scanner.start_column, scanner.current_line,
                 scanner.current_column, "mixing of tabs and spaces");
    scanner.indentation_type = INDENTATION_NONE;
  }

  if (indentation > array_last(&scanner.indentation))
  {
    array_add(&scanner.indentation, indentation);
    add_custom_token(TOKEN_INDENT, scanner.start + 1, (int)(scanner.current - scanner.start - 1));
  }
  else if (indentation < array_last(&scanner.indentation))
  {
    while (array_last(&scanner.indentation) > indentation)
    {
      add_custom_token(TOKEN_DEDENT, NULL, 0);
      array_del_last(&scanner.indentation);
    }

    if (indentation != array_last(&scanner.indentation))
    {
      report_error(scanner.start_line, scanner.start_column, scanner.current_line,
                   scanner.current_column, "unexpected deindent");
    }
  }
}

void scanner_init(const char* source)
{
  scanner.start = source;
  scanner.current = source;
  scanner.start_line = 1;
  scanner.start_column = 1;
  scanner.current_line = scanner.start_line;
  scanner.current_column = scanner.start_column;
  scanner.multi_line = 0;
  scanner.indentation_type = INDENTATION_NONE;

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

  if (scanner.multi_line)
  {
    report_error(scanner.start_line, scanner.start_column, scanner.current_line,
                 scanner.current_column, "reached end-of-file in multi-line mode");
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
